/*
 * imgsum.c
 *
 * Copyright 2016 Adrian Boyko
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 *
 *
 */

#include <Python.h>
#include <stdio.h>

typedef struct {
	FILE* file;          // File where sums are written
	uint32_t* rowSums;   // Row sum accumulators
	uint32_t* colSums;   // Col sum accumulators
	uint32_t* gtRowSums; // Grand total accumulator
	uint32_t* gtColSums; // Grand total accumulator
	uint32_t* rowBGs;    // Background to subtract from row sums
	uint32_t* colBGs;    // Background to subtract from col sums
	uint16_t  width;     // Width of image, in pixels
	uint16_t  height;    // Height of image, in pixels
	uint32_t  imgCount;  // How many imgs have been summed
} modulestate;

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

static void addArrayToArray_uint32(uint32_t* target, uint32_t* toAdd, size_t len) {
	for (int i=0; i < len; i++) {
		target[i] += toAdd[i]; // Overflow not expected.
	}
}

static void subtractArrayFromArray_uint32(uint32_t* target, uint32_t* toSub, size_t len) {
	for (int i=0; i < len; i++) {
		// Prevent expected underflow.
		if (toSub[i] > target[i]) target[i] = 0;
		else target[i] -= toSub[i];
	}
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

static void clear_file(modulestate* ms) {
	if (!ms->file) return;
	fclose(ms->file);
	ms->file = NULL;
}

static void clear_gtRowSums(modulestate* ms) {
	if (!ms->gtRowSums) return;
	free(ms->gtRowSums);
	ms->gtRowSums = NULL;
}

static void clear_gtColSums(modulestate* ms) {
	if (!ms->gtColSums) return;
	free(ms->gtColSums);
	ms->gtColSums = NULL;
}

static void clear_rowSums(modulestate* ms) {
	if (!ms->rowSums) return;
	free(ms->rowSums);
	ms->rowSums = NULL;
}

static void clear_colSums(modulestate* ms) {
	if (!ms->colSums) return;
	free(ms->colSums);
	ms->colSums = NULL;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

static PyObject* imgsum_beginBatch(PyObject* self, PyObject* args) {
	modulestate* ms = PyModule_GetState(self);

	// Initialize module state:
	ms->rowSums   = NULL;
	ms->colSums   = NULL;
	ms->gtRowSums = NULL;
	ms->gtColSums = NULL;
	ms->rowBGs    = NULL;
	ms->colBGs    = NULL;
	ms->file      = NULL;

	// Parse arguments:
	PyObject* wantRowSumsObj = NULL;
	PyObject* wantColSumsObj = NULL;
	if (!PyArg_ParseTuple(args, "iiOO",
	  &ms->width, &ms->height, &wantRowSumsObj, &wantColSumsObj)) {
		return NULL;
	}

	if (ms->width % 32 != 0) {
		// This module's code currently only works with images that have
		// widths that are a multiple of 32. Other widths have padding
		// in the YUV420 format but code doesn't yet deal with padding.
		PyErr_SetString(PyExc_ValueError, "Width of images must be a multiple of 32.");
		return NULL;
	}

	if (PyObject_IsTrue(wantRowSumsObj))
		ms->rowSums = calloc(ms->height, sizeof(*ms->rowSums));

	if (PyObject_IsTrue(wantColSumsObj))
		ms->colSums = calloc(ms->width, sizeof(*ms->colSums));

	Py_RETURN_NONE;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

static PyObject* imgsum_grandTotals(PyObject* self, PyObject* args) {
	modulestate* ms = PyModule_GetState(self);

	PyObject* wantGrandTotalsObj = NULL;
	if (!PyArg_ParseTuple(args, "O", &wantGrandTotalsObj)) {
		return NULL;
	}

	// No matter if caller is asking to start or stop gt, clear any existing gt data.
	clear_gtRowSums(ms);
	clear_gtColSums(ms);

	if (PyObject_IsTrue(wantGrandTotalsObj)) {
		ms->imgCount = 0;
		// Create appropriate gt sums for current state
		if (ms->rowSums) ms->gtRowSums = calloc(ms->height, sizeof(*ms->gtRowSums));
		if (ms->colSums) ms->gtColSums = calloc(ms->width, sizeof(*ms->gtColSums));
	}

	Py_RETURN_NONE;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

static PyObject* imgsum_saveSumsTo(PyObject* self, PyObject* args) {
	modulestate* ms = PyModule_GetState(self);

	PyObject* filenameObj = NULL;
	if (!PyArg_ParseTuple(args, "O", &filenameObj)) {
		return NULL;
	}

	if (PyObject_IsTrue(filenameObj)) {
		// Caller passed a "true" value, presumably a string.
		char* filename = PyUnicode_AsUTF8(filenameObj);
		if (!filename) {
			PyErr_SetString(PyExc_ValueError, "A string is required.");
			return NULL;
		}

		ms->file = fopen(filename, "wb");
		if (!ms->file) {
			PyErr_SetString(PyExc_EnvironmentError, "Couldn't open output file.");
			return NULL;
		}
	}
	else {
		// Caller passed None, False, or some other Python false value.
		clear_file(ms);
	}

	Py_RETURN_NONE;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

static PyObject* imgsum_processImg(PyObject* self, PyObject* args) {
	modulestate* ms = PyModule_GetState(self);

	// Parse arguments:
	char* yuv420; // The Image in YUV420 format.
	int yuv420Len; // Not used b/c we already know img width & height.
	if (!PyArg_ParseTuple(args,"s#", &yuv420, &yuv420Len)) {
		return NULL;
	}

	// Calculate the sums for the given image:
	char* val = yuv420;
	uint16_t currRow = 0;
	uint16_t currCol = 0;
	if (ms->rowSums) memset(ms->rowSums, 0, ms->height * sizeof(*ms->rowSums));
	if (ms->colSums) memset(ms->colSums, 0, ms->width * sizeof(*ms->colSums));
	while (currRow < ms->height) {
		if (ms->rowSums) ms->rowSums[currRow] += *val;
		if (ms->colSums) ms->colSums[currCol] += *val;
		val += 1;
		currCol += 1;
        if (currCol == ms->width) {
            currRow += 1;
            currCol = 0;
        }
    }

    // Subtract background, write sums to file, add sums to grand totals.
	if (ms->rowSums) {
		if (ms->rowBGs) subtractArrayFromArray_uint32(ms->rowSums, ms->rowBGs, ms->height);
		if (ms->file) fwrite(ms->rowSums, sizeof(*ms->rowSums), ms->height, ms->file);
		if (ms->gtRowSums) addArrayToArray_uint32(ms->gtRowSums, ms->rowSums, ms->height);
	}
	if (ms->colSums) {
		if (ms->colBGs) subtractArrayFromArray_uint32(ms->colSums, ms->colBGs, ms->width);
		if (ms->file) fwrite(ms->colSums, sizeof(*ms->colSums), ms->width, ms->file);
		if (ms->gtColSums) addArrayToArray_uint32(ms->gtColSums, ms->colSums, ms->width);
	}

	ms->imgCount += 1;
	Py_RETURN_NONE;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

static PyObject* imgsum_setBG(PyObject* self, PyObject* args) {
	modulestate* ms = PyModule_GetState(self);

	if (ms->gtRowSums) {
		if (!ms->rowBGs) ms->rowBGs = calloc(ms->height, sizeof(*ms->rowBGs));
		for (int r=0; r < ms->height; r++) {
			ms->rowBGs[r] = ms->gtRowSums[r] / ms->imgCount;
		}
	}
	if (ms->gtColSums) {
		if (!ms->colBGs) ms->colBGs = calloc(ms->width, sizeof(*ms->colBGs));
		for (int c=0; c < ms->width; c++) {
			ms->colBGs[c] = ms->gtColSums[c] / ms->imgCount;
		}
	}

	Py_RETURN_NONE;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

static PyObject* imgsum_endBatch(PyObject* self, PyObject* args) {
	modulestate* ms = PyModule_GetState(self);

	if (ms->gtRowSums && ms->file)
		fwrite(ms->gtRowSums, sizeof(*ms->gtRowSums), ms->height, ms->file);

	if (ms->gtColSums && ms->file)
		fwrite(ms->gtColSums, sizeof(*ms->gtColSums), ms->width, ms->file);

	clear_gtRowSums(ms);
	clear_gtColSums(ms);
	clear_file(ms);
	clear_rowSums(ms);
	clear_colSums(ms);

	Py_RETURN_NONE;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

static PyMethodDef imgsum_methods[] = {
	{"begin_batch",  (PyCFunction)imgsum_beginBatch,  METH_VARARGS, NULL},
	{"save_sums_to", (PyCFunction)imgsum_saveSumsTo,  METH_VARARGS, NULL},
	{"grand_totals", (PyCFunction)imgsum_grandTotals, METH_VARARGS, NULL},
	{"process_img",  (PyCFunction)imgsum_processImg,  METH_VARARGS, NULL},
	{"set_bg",       (PyCFunction)imgsum_setBG,       METH_NOARGS,  NULL},
	{"end_batch",    (PyCFunction)imgsum_endBatch,    METH_NOARGS,  NULL},
	{NULL,           NULL,                            0,            NULL}
};

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

PyMODINIT_FUNC PyInit_imgsum(void) {
	PyObject* m;

	static struct PyModuleDef moduledef = {
		PyModuleDef_HEAD_INIT,
		"imgsum",
		"Calculates ro and/or col sums for each img in a series and writes sums to a file.",
		sizeof(modulestate),
		imgsum_methods,
		NULL,NULL,NULL,NULL
	};
	m = PyModule_Create(&moduledef);
	return m;
}
