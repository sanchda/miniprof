#include <stdlib.h>
#include <string.h>

#define Py_BUILD_CORE
#include <Python.h>

#include "internal/pycore_pystate.h"
#include "interface.hpp" // from ddup


const char *get_env_or_default(const char *name, const char *default_value) {
    char *value = getenv(name);
    if (value == NULL || strlen(value) == 0)
        return default_value;
    return value;
}

const double g_period= 1.0/100.0; // TODO make more firm
static PyObject* check_threads(PyObject* self) {
  static bool initialized = false;
  static double counter = 0;

  if (!initialized) {
    ddup_config_env(get_env_or_default("DD_ENV", "prod"));
    ddup_config_version(get_env_or_default("DD_VERSION", "miniprof_service"));
    ddup_config_url(get_env_or_default("DD_TRACE_AGENT_URL", "http://localhost:8126"));
    ddup_config_runtime("python");
    ddup_config_runtime_version(Py_GetVersion());
    ddup_config_profiler_version("ddup_v0.2");
    ddup_config_max_nframes(256);

    initialized = true;
    ddup_init();
  }
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


        // Push to profiler
        ddup_start_sample(256);
        ddup_push_walltime(g_period, 1); // could be firmer
        ddup_push_threadinfo(thread->thread_id, 0, "miniprofiled_thread");
        while (frame) {
            PyCodeObject *code = PyFrame_GetCode(frame);
            Py_XDECREF(code);

            // Push frameinfo
            ddup_push_frame(PyUnicode_AsUTF8(PyObject_GetAttrString((PyObject *)code, "co_name")),
                            PyUnicode_AsUTF8(PyObject_GetAttrString((PyObject *)code, "co_filename")),
                            0,
                            PyFrame_GetLineNumber(frame));

            // Iterate
            PyFrameObject *prev = PyFrame_GetBack(frame);
            Py_XDECREF(frame); // give back one strong reference
            frame = prev;
        }
        ddup_flush_sample();
        thread = PyThreadState_Next(thread);
      }
      rt_state = PyInterpreterState_Next(rt_state);
    }
    PyThread_release_lock(lmutex);

    // Check to see if we need to upload
    if (1 < (counter += g_period)) {
      counter = 0;
      ddup_upload();
    }

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
