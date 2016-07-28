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

// From http://stackoverflow.com/questions/3437404/min-and-max-in-c
#define min(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a < _b ? _a : _b; })
     
static void addArrayToArray_uint32(uint32_t* target, uint32_t* toAdd, size_t len) {
	for (int i=0; i < len; i++) {
		target[i] += toAdd[i]; // Overflow not expected.
	}
}

static void subtractArrayFromArray_uint32(uint32_t* target, uint32_t* toSub, size_t len) {
	for (int i=0; i < len; i++) {
		uint32_t s = min(toSub[i], target[i]); // Prevent expected underflow.
		target[i] -= s;
	}
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

static PyObject* imgsum_start(PyObject* self, PyObject* args) {
	modulestate* ms = PyModule_GetState(self);
	
	ms->imgCount = 0;
	
	PyObject* wantRowSumsObj     = NULL;
	PyObject* wantColSumsObj     = NULL;
	PyObject* wantGrandTotalsObj = NULL;
	
	if (!PyArg_ParseTuple(args, "iiOOO", 
	  &ms->width, &ms->height, &wantRowSumsObj, &wantColSumsObj, &wantGrandTotalsObj)) {
		return NULL;
	}
	
	if (ms->width % 32 != 0) {
		// This module's code currently only works with images that have
		// widths that are a multiple of 32. Other widths have padding
		// in the YUV420 format but code doesn't yet deal with padding.
		PyErr_SetString(PyExc_ValueError, "Width of images must be a multiple of 32.");
		return NULL;
	}
	
	int wantRowSums = PyObject_IsTrue(wantRowSumsObj);
	int wantColSums = PyObject_IsTrue(wantColSumsObj);
	int wantGrandTotals = PyObject_IsTrue(wantGrandTotalsObj);
		
	size_t rowSumsByteCount = ms->height * sizeof(*ms->rowSums);
	size_t colSumsByteCount = ms->width * sizeof(*ms->colSums);
	size_t gtRowSumsByteCount = ms->height * sizeof(*ms->gtRowSums);		
	size_t gtColSumsByteCount = ms->width * sizeof(*ms->gtColSums);
		
	ms->rowSums = wantRowSums ? malloc(rowSumsByteCount) : NULL;
	ms->colSums = wantColSums ? malloc(colSumsByteCount) : NULL;
	ms->gtRowSums = wantRowSums && wantGrandTotals ? malloc(gtRowSumsByteCount) : NULL;
	ms->gtColSums = wantColSums && wantGrandTotals ? malloc(gtColSumsByteCount) : NULL;
	if (ms->gtRowSums) memset(ms->rowSums, 0, gtRowSumsByteCount);
	if (ms->gtColSums) memset(ms->colSums, 0, gtColSumsByteCount);
		
	Py_RETURN_NONE;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

static PyObject* imgsum_setFile(PyObject* self, PyObject* args) {
	modulestate* ms = PyModule_GetState(self);

	char* filename = NULL;

	if (!PyArg_ParseTuple(args, "s", &filename)) {
		return NULL;
	}

	ms->file = fopen(filename, "wb");
	if (ms->file == NULL) {
		PyErr_SetString(PyExc_EnvironmentError, "Couldn't open output file.");
		return NULL;
	}
	
	Py_RETURN_NONE;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

static PyObject* imgsum_sum(PyObject* self, PyObject* args) {
	modulestate* ms = PyModule_GetState(self);
	size_t rowSumsByteCount = ms->height * sizeof(*ms->rowSums);		
	size_t colSumsByteCount = ms->width * sizeof(*ms->colSums);

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
	if (ms->rowSums) memset(ms->rowSums, 0, rowSumsByteCount);
	if (ms->colSums) memset(ms->colSums, 0, colSumsByteCount);
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
		if (ms->rowBGs) subtractArrayFromArray_uint32(ms->gtRowSums, ms->rowBGs, ms->height);
		if (ms->file) fwrite(ms->rowSums, rowSumsByteCount, 1, ms->file);
		if (ms->gtRowSums) addArrayToArray_uint32(ms->gtRowSums, ms->rowSums, ms->height);
	}
	if (ms->colSums) {
		if (ms->colBGs) subtractArrayFromArray_uint32(ms->gtColSums, ms->colBGs, ms->width);
		if (ms->file) fwrite(ms->colSums, colSumsByteCount, 1, ms->file);
		if (ms->gtColSums) addArrayToArray_uint32(ms->gtColSums, ms->colSums, ms->width);
	}
	
	ms->imgCount += 1;
	Py_RETURN_NONE;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

static PyObject* imgsum_setBG(PyObject* self, PyObject* args) {
	modulestate* ms = PyModule_GetState(self);
	size_t rowSumsByteCount = ms->height * sizeof(*ms->rowSums);		
	size_t colSumsByteCount = ms->width * sizeof(*ms->colSums);
	if (ms->gtRowSums) {
		if (ms->rowBGs == NULL) ms->rowBGs = malloc(rowSumsByteCount);
		for (int r=0; r < ms->height; r++) {
			ms->rowBGs[r] = ms->gtRowSums[r] / ms->imgCount;
		}
	}
	if (ms->gtColSums) {
		if (ms->colBGs == NULL) ms->colBGs = malloc(colSumsByteCount);
		for (int c=0; c < ms->width; c++) {
			ms->colBGs[c] = ms->gtColSums[c] / ms->imgCount;
		}
	}

	Py_RETURN_NONE;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

static PyObject* imgsum_stop(PyObject* self, PyObject* args) {
	modulestate* ms = PyModule_GetState(self);	

	if (ms->gtRowSums) {
		size_t gtRowSumsByteCount = ms->height * sizeof(*ms->gtRowSums);		
		if (ms->file) fwrite(ms->gtRowSums, gtRowSumsByteCount, 1, ms->file);
		free(ms->gtRowSums);
		ms->gtRowSums = NULL;
	}
	if (ms->gtColSums) {
		size_t gtColSumsByteCount = ms->width * sizeof(*ms->gtColSums);
		if (ms->file) fwrite(ms->gtColSums, gtColSumsByteCount, 1, ms->file);
		free(ms->gtColSums);
		ms->gtColSums = NULL;
	}
	if (ms->file) {
		fclose(ms->file);
		ms->file = NULL;
	}
	if (ms->rowSums) {
		free(ms->rowSums);
		ms->rowSums = NULL;
	}
	if (ms->colSums) {
		free(ms->colSums);
		ms->colSums = NULL;
	}
	
	Py_RETURN_NONE;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

static PyMethodDef imgsum_methods[] = {
	{"start",    (PyCFunction)imgsum_start,   METH_VARARGS, NULL},
	{"set_file", (PyCFunction)imgsum_setFile, METH_VARARGS, NULL},
	{"sum",      (PyCFunction)imgsum_sum,     METH_VARARGS, NULL},
	{"set_bg",   (PyCFunction)imgsum_setBG,   METH_NOARGS,  NULL},
	{"stop",     (PyCFunction)imgsum_stop,    METH_NOARGS,  NULL},
	{NULL,       NULL,                        0,            NULL}
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
