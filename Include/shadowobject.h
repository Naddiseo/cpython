#ifndef Py_SHADOWOBJECT_H
#define Py_SHADOWOBJECT_H

/* Shadow Object */
#ifdef __cplusplus
extern "C" {
#endif

PyAPI_DATA(PyTypeObject) PyShadow_Type;
PyAPI_FUNC(PyObject *) PyShadow_Union(PyObject *target, PyObject *alias);
PyAPI_FUNC(PyObject *) PyShadow_FordwardRef(PyObject *ref);
PyAPI_FUNC(PyObject *) PyShadow_UnionAsTuple(PyObject * self);
#define PyShadow_CheckExact(op) (Py_TYPE(op) == &PyShadow_Type)
#ifdef __cplusplus
}
#endif

#endif /* !Py_SHADOWOBJECT_H */