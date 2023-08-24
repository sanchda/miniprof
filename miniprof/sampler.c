#include <stdlib.h>

#define Py_BUILD_CORE
#include <Python.h>
#include "internal/pycore_pystate.h"


static PyObject* check_threads(PyObject* self) {
  PyInterpreterState *rt_state;
  PyThreadState *thread;
  PyObject *thread_tb = PyDict_New();
  PyThread_type_lock lmutex = _PyRuntime.interpreters.mutex;

  if (PyThread_acquire_lock(lmutex, WAIT_LOCK) == PY_LOCK_ACQUIRED) {
    rt_state = PyInterpreterState_Head();

    while (rt_state) {
      thread = PyInterpreterState_ThreadHead(rt_state);

      while (thread) {
        // PyThreadState_GetFrame and PyFrame_GetBack both return strong
        // references, so can manage reference count later the same way
        PyFrameObject *frame = PyThreadState_GetFrame(thread);
        if (!frame)
            break;
        while (frame) {
            PyCodeObject *code = PyFrame_GetCode(frame);
            Py_XDECREF(code);

            // Iterate
            PyFrameObject *prev = PyFrame_GetBack(frame);
            Py_XDECREF(frame); // give back one strong reference
            frame = prev;
        }
        thread = PyThreadState_Next(thread);
      }
      rt_state = PyInterpreterState_Next(rt_state);
    }
    PyThread_release_lock(lmutex);

  }

  return thread_tb;
}

static PyMethodDef mod_methods[] = {
    {"check_threads",  (PyCFunction)check_threads, METH_NOARGS, "Returns some runtime state"},
    // Sentinel value always goes at end
    {NULL, NULL, 0, NULL}
};

static struct PyModuleDef sampler_mod = {
    PyModuleDef_HEAD_INIT,
    "sampler",
    NULL,
    -1,
    mod_methods,
};

PyMODINIT_FUNC PyInit_sampler(void) {
    return PyModule_Create(&sampler_mod);
}
