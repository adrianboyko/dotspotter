/*
 * imgsum.c
 * 
 * Copyright 2014 Adrian Boyko
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
	FILE* file;         // File where sums are written
	uint32_t* rowSums;  // Row sum accumulators
	uint32_t* colSums;  // Col sum accumulators
	uint16_t width;     // Width of image, in pixels
	uint16_t height;    // Height of image, in pixels
} modulestate;

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

static PyObject* imgsum_start(PyObject* self, PyObject* args) {
	modulestate* ms = PyModule_GetState(self);
	
	char* filename = NULL;
	PyObject* wantRowSumsObj = NULL;
	PyObject* wantColSumsObj = NULL;
	
	// TODO: Make range ends/begins default
	if (!PyArg_ParseTuple(args, "iisOO", 
	  &ms->width, &ms->height, &filename, &wantRowSumsObj, &wantColSumsObj)) {
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
	
	ms->file = fopen(filename, "wb");
	if (ms->file == NULL) {
		PyErr_SetString(PyExc_EnvironmentError, "Couldn't open output file.");
		return NULL;
	}
		
	size_t rowSumsByteCount = ms->height * sizeof(*ms->rowSums);
	size_t colSumsByteCount = ms->width * sizeof(*ms->colSums);
	ms->rowSums = wantRowSums ? malloc(rowSumsByteCount) : NULL;
	ms->colSums = wantColSums ? malloc(colSumsByteCount) : NULL;
		
	Py_RETURN_NONE;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

static PyObject* imgsum_stop(PyObject* self, PyObject* args) {
	modulestate* ms = PyModule_GetState(self);
	
	if (ms->rowSums) free(ms->rowSums);
	if (ms->colSums) free(ms->colSums);
	fclose(ms->file);
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
		
	// Calculate the row sums for the given image:
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
	if (ms->rowSums) fwrite(ms->rowSums, rowSumsByteCount, 1, ms->file);
	if (ms->colSums) fwrite(ms->colSums, colSumsByteCount, 1, ms->file);
	Py_RETURN_NONE;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

static PyMethodDef imgsum_methods[] = {
	{"start", (PyCFunction)imgsum_start, METH_VARARGS, NULL},
	{"sum",   (PyCFunction)imgsum_sum,   METH_VARARGS, NULL},
	{"stop",  (PyCFunction)imgsum_stop,  METH_NOARGS,  NULL},
	{NULL,    NULL,                      0,            NULL}
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
