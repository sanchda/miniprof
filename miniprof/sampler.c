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

static const char *get_class_name(PyFrameObject *frame) {
  PyObject *varnames, *value, *name, *clsname;
  PyCodeObject *code = PyFrame_GetCode(frame);

  if (!code)
    return "";

  varnames = PyCode_GetVarnames(code);
  if (!varnames || !PyTuple_Check(varnames) || PyTuple_Size(varnames) <= 0)
    return "";

  name = PyTuple_GetItem(varnames, 0);  // Borrowed reference, no need to DECREF.
  if (!name)
    return "";

  PyObject *locals = PyFrame_GetLocals(frame);
  if (!locals)
      return "";
  value = PyDict_GetItem(locals, name);  // Borrowed reference. No need to DECREF.
  if (!value)
    return "";

  if (PyUnicode_CompareWithASCIIString(name, "self") == 0) {
    value = PyObject_Type(value);  // Gets the type of 'self'
    if (!value)
      return "";
  } else if (PyUnicode_CompareWithASCIIString(name, "cls") != 0) {
    return "";
  }

  clsname = PyObject_GetAttrString(value, "__name__");
  Py_XDECREF(value);  // Safe to DECREF now.

  if (!clsname)
    return "";

  const char *result = PyUnicode_AsUTF8(clsname);
  Py_DECREF(clsname);  // DECREF after using.

  return result ? result : "";
}

const double g_period= 1.0/100.0; // TODO make more firm
static PyObject* check_threads(PyObject* self) {
  static bool initialized = false;
  static double counter = 0;

  if (!initialized) {
    ddup_config_env(get_env_or_default("DD_ENV", "prod"));
    ddup_config_version(get_env_or_default("DD_VERSION", ""));
    ddup_config_service(get_env_or_default("DD_SERVICE", "miniprof_service"));
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

  // TODO should probably check that these could be allocated
  PyObject *thread_tb = PyDict_New();
  PyObject* running_threads = PyDict_New();
  PyObject* current_exceptions = PyDict_New();

  PyThread_type_lock lmutex = _PyRuntime.interpreters.mutex;

  if (PyThread_acquire_lock(lmutex, WAIT_LOCK) == PY_LOCK_ACQUIRED) {
    rt_state = PyInterpreterState_Head();

    while (rt_state) {
      thread = PyInterpreterState_ThreadHead(rt_state);

      while (thread) {
        PyFrameObject *frame = PyThreadState_GetFrame(thread);
        if (!frame)
          break;

        PyObject *thread_id = PyLong_FromUnsignedLong(thread->thread_id);
        if (thread_id)
          PyDict_SetItem(running_threads, thread_id, (PyObject *)frame); // unhandled
        Py_DECREF(thread_id);
        Py_XDECREF(frame); // give back one strong reference

        // Now harvest exceptions
        _PyErr_StackItem* exc_info = _PyErr_GetTopmostException(thread);
        if (exc_info && exc_info->exc_value && exc_info->exc_value != Py_None) {
          PyObject *exc_type = (PyObject *)Py_TYPE(exc_info->exc_value);
          PyObject *exc_tb = PyException_GetTraceback(exc_info->exc_value);

          if (exc_tb) {
            PyObject *key = PyLong_FromUnsignedLong(thread->thread_id);
            PyObject *value = PyTuple_Pack(2, exc_type, exc_tb);

            if (key && value)
              PyDict_SetItem(current_exceptions, key, value);

            Py_XDECREF(key);
            Py_XDECREF(value);
          }
          Py_XDECREF(exc_tb);
        }
        thread = PyThreadState_Next(thread);
      }
      rt_state = PyInterpreterState_Next(rt_state);
    }
    PyThread_release_lock(lmutex);

    // Stash the wall/cpu data we harvested
    {
      PyObject *key, *value;
      Py_ssize_t pos = 0;
      while (PyDict_Next(running_threads, &pos, &key, &value)) {
        ddup_start_sample(256);
        ddup_push_walltime(g_period * 1e9, 1); // could be firmer
        ddup_push_cputime(g_period * 1e9, 1); // outright lie
        ddup_push_threadinfo(PyLong_AsLong(key), 0, "miniprofiled_thread");

        PyFrameObject *frame = (PyFrameObject *)value;
        while (frame) {
          PyCodeObject *code = PyFrame_GetCode(frame);

          // Push frameinfo
          ddup_push_class_name(get_class_name(frame));
          ddup_push_frame(PyUnicode_AsUTF8(PyObject_GetAttrString((PyObject *)code, "co_name")),
                          PyUnicode_AsUTF8(PyObject_GetAttrString((PyObject *)code, "co_filename")),
                          0,
                          PyFrame_GetLineNumber(frame));
          Py_XDECREF(code);

          // Iterate
          PyFrameObject *prev = PyFrame_GetBack(frame);
          frame = prev;
        }
        ddup_flush_sample();
      }
    }

    // Stash the exception data
    {
      PyObject *key, *value;
      Py_ssize_t pos = 0;
      while (PyDict_Next(current_exceptions, &pos, &key, &value)) {
        ddup_start_sample(256);
        ddup_push_exceptioninfo("idk lol", 1);
        ddup_push_threadinfo(PyLong_AsLong(key), 0, "miniprofiled_thread");

        PyObject *tb = PyTuple_GetItem(value, 1);
        while (tb && PyTraceBack_Check(tb)) {
          PyFrameObject *frame = (PyFrameObject *)PyObject_GetAttrString(tb, "tb_frame");
          if (!frame || !PyFrame_Check((PyObject*)frame))
            continue;
          PyCodeObject *code = PyFrame_GetCode(frame);
          if (!code || !PyCode_Check(code))
            continue;

          // Push frameinfo
          ddup_push_frame(PyUnicode_AsUTF8(PyObject_GetAttrString((PyObject *)code, "co_name")),
                          PyUnicode_AsUTF8(PyObject_GetAttrString((PyObject *)code, "co_filename")),
                          0,
                          PyFrame_GetLineNumber(frame));
          Py_XDECREF(frame);
          Py_XDECREF(code);

          // Iterate
          tb = PyObject_GetAttrString(tb, "tb_next");
        }
        ddup_flush_sample();
      }
    }

    // Check to see if we need to upload
    if (60 < (counter += g_period)) {
      printf("Uploading!\n");
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
