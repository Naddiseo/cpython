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

PyDoc_STRVAR(shadow_setstate__doc__,
"__setstate__($self, state, /)\n"
"--\n"
"\n");

#define SHADOW_SETSTATE_METHODDEF    \
    {"__setstate__", (PyCFunction)shadow_setstate, METH_O, shadow_setstate__doc__},
/*[clinic end generated code: output=218ef05077878cd4 input=a9049054013a1b77]*/
