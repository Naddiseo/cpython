/*[clinic input]
preserve
[clinic start generated code]*/

PyDoc_STRVAR(shadow_new__doc__,
"shadowobject()\n"
"--\n"
"\n"
"return a shadow object");

static PyObject *
shadow_new_impl(PyTypeObject *type);

static PyObject *
shadow_new(PyTypeObject *type, PyObject *args, PyObject *kwargs)
{
    PyObject *return_value = NULL;

    if ((type == &PyShadow_Type) &&
        !_PyArg_NoPositional("shadowobject", args)) {
        goto exit;
    }
    if ((type == &PyShadow_Type) &&
        !_PyArg_NoKeywords("shadowobject", kwargs)) {
        goto exit;
    }
    return_value = shadow_new_impl(type);

exit:
    return return_value;
}

static int
shadow_init_impl(shadowobject *self, int tp);

static int
shadow_init(PyObject *self, PyObject *args, PyObject *kwargs)
{
    int return_value = -1;
    int tp = 0;

    if ((Py_TYPE(self) == &PyShadow_Type) &&
        !_PyArg_NoKeywords("shadowobject", kwargs)) {
        goto exit;
    }
    if (!_PyArg_CheckPositional("shadowobject", PyTuple_GET_SIZE(args), 0, 1)) {
        goto exit;
    }
    if (PyTuple_GET_SIZE(args) < 1) {
        goto skip_optional;
    }
    if (PyFloat_Check(PyTuple_GET_ITEM(args, 0))) {
        PyErr_SetString(PyExc_TypeError,
                        "integer argument expected, got float" );
        goto exit;
    }
    tp = _PyLong_AsInt(PyTuple_GET_ITEM(args, 0));
    if (tp == -1 && PyErr_Occurred()) {
        goto exit;
    }
skip_optional:
    return_value = shadow_init_impl((shadowobject *)self, tp);

exit:
    return return_value;
}

PyDoc_STRVAR(shadow_setstate__doc__,
"__setstate__($self, state, /)\n"
"--\n"
"\n");

#define SHADOW_SETSTATE_METHODDEF    \
    {"__setstate__", (PyCFunction)shadow_setstate, METH_O, shadow_setstate__doc__},
/*[clinic end generated code: output=7b999ac39c236fdf input=a9049054013a1b77]*/
