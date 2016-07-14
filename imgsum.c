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

static FILE* file = NULL;

static uint32_t* rowSums = NULL; // Row sum accumulators.
static size_t rowSumsByteCount = 0; // Size of rowSums buffer, in bytes.

static uint16_t width = 0; // Width of image, in pixels.
static uint16_t height = 0; // Height of image, in pixels.

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

static PyObject* imgsum_start(PyObject* self, PyObject* args) {

	char* filename;
	if (!PyArg_ParseTuple(args,"iis", &width, &height, &filename)) {
		return NULL;
	}

	file = fopen(filename,"ab");	
	rowSumsByteCount = height * sizeof(*rowSums);	 
	rowSums = malloc(rowSumsByteCount);
	memset(rowSums, 0, rowSumsByteCount); 
	Py_RETURN_NONE;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

static PyObject* imgsum_stop(PyObject* self, PyObject* args) {
	free(rowSums);
	fclose(file);
	Py_RETURN_NONE;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

static PyObject* imgsum_sum(PyObject* self, PyObject* args) {

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
	memset(rowSums, 0, rowSumsByteCount);
	while (1) {
		rowSums[currRow] += *val;
		val += 1;
		currCol += 1;
        if (currCol == width) {
			// Consider enhancement: Skip val ptr over padding for case when width is not multiple of 32.
            currRow += 1;
            currCol = 0;
        }
        if (currRow == height) break;
    }

	fwrite(rowSums, rowSumsByteCount, 1, file);
	
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
		"Sums rows of a series of YUV420 images and writes sums to a file.",
		-1,
		imgsum_methods,
		NULL,NULL,NULL,NULL
	};
	m = PyModule_Create(&moduledef);
	return m;
}
