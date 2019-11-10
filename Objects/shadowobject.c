/* shadow object; a dummy type object for use in type hinting. 
  typing.py should promote it to an actual type when encountered */

#include "Python.h"

// Dummy Type to use in typing.py
/*[clinic input]
class shadowobject "shadowobject *" "&PyShadow_Type"
[clinic start generated code]*/
/*[clinic end generated code: output=da39a3ee5e6b4b0d input=08165972310008df]*/
typedef struct
{
    PyObject_HEAD
    int s_tp;  // the type
    PyTupleObject *s_params; /* tuple of params */
} shadowobject;

#include "clinic/shadowobject.c.h"

#define PyShadow_Base 0
#define PyShadow_UnionTp 1
#define PyShadow_ForwardRefTp 2
#define PyShadow_TypeVarTp 3
#define PyShadow_MAXTp 3

PyObject *_get_promoted(PyObject *shadow);

Py_ssize_t
_is_same_type(PyObject *lhs, PyObject *rhs)
{
    if (lhs == Py_None && rhs == Py_None)
    {
        return 1;
    }
    if ((lhs) == (rhs))
    { // TODO: this isn't correct
        //printf("is same type\n");
        return 1;
    }

    return -1;
}

Py_ssize_t
union_check_in(shadowobject *s, PyObject *needle)
{
    // return 1 if needle is in un.un_params, else -1
    Py_ssize_t len = PyTuple_Size((PyObject *)s->s_params);
    if (len == 0)
    {
        return -1;
    }
    for (Py_ssize_t i = 0; i < len; i++)
    {
        PyObject *other = PyTuple_GetItem((PyObject *)s->s_params, i);
        if (other == NULL)
        {
            return -1;
        }
        if (_is_same_type(other, needle) > 0)
        {
            return 1;
        }
    }
    return -1;
}

Py_ssize_t
typ_check(PyObject *thing)
{
    // return 1 if thing is None, callable, other shadow type, or type
    // return 2 for shadow union
    // return 3 for string
    // returns -1 for anything else

    //returns -1 if thing is not a type, None, or union
    if (thing == NULL)
    {
        return -1;
    }
    if (thing == Py_None)
    {
        return 1;
    }
    // forward ref, should it be explicit?
    if (PyObject_IsInstance(thing, (PyObject *)&PyUnicode_Type) > 0)
    {
        return 3;
    }
    if (PyObject_IsInstance(thing, (PyObject *)&PyType_Type) > 0)
    {
        return 1;
    }
    if (PyObject_IsInstance(thing, (PyObject *)&PyShadow_Type) > 0)
    {
        // todo: might need to change this, or specialize based upon type
        if (((shadowobject *)thing)->s_tp == PyShadow_UnionTp)
        {
            return 2;
        }
        return 1; // maybe also 2?
    }
    if (!PyCallable_Check(thing))
    {
        //print("not callable")
        // equiv to the `if not callable(...)` line
        return -1;
    }
    // TODO: anything else?
    return 1;
}

PyObject *
_copy_union_into(PyObject *tuple, PyObject *alias, Py_ssize_t alias_size, Py_ssize_t offset)
{
    for (Py_ssize_t i = 0; i < alias_size; i++)
    {
        PyObject *other = PyTuple_GetItem(alias, i);
        if (other == NULL)
        {
            return NULL;
        }
        if (PyTuple_SetItem(tuple, i + offset, other) < 0)
        {
            return NULL;
        }
        Py_INCREF(other); // i think
    }
    return tuple;
}

/* Creates a new shadow object with type==union */
PyObject *
PyShadow_Union_(PyTypeObject *type, PyObject *target, PyObject *alias)
{
    shadowobject *shadow;
    PyObject *params;

    if (target == NULL || alias == NULL)
    {
        PyErr_SetString(PyExc_TypeError, "both 'target' and 'alias' expected");
        return NULL;
    }

    shadow = (shadowobject *)type->tp_alloc(type, 0);
    if (shadow == NULL)
    {
        return NULL;
    }
    shadow->s_tp = PyShadow_UnionTp;
    Py_ssize_t target_tp = typ_check(target);
    Py_ssize_t alias_tp = typ_check(alias);

    if (target_tp < 0)
    {
        //printf("target_tp=%s\n", Py_TYPE(target)->tp_name);
        PyErr_SetString(PyExc_TypeError, "'target' should be a type, None, or union");
        Py_DECREF(shadow);
        return NULL;
    }
    if (alias_tp < 0)
    {
        PyErr_SetString((PyExc_TypeError), "'alias' should be a type, None, or union");
        Py_DECREF(shadow);
        return NULL;
    }

    if (target == Py_None)
    {
        target = (PyObject *)Py_TYPE(Py_None);
    }
    else if (target_tp == 3)
    { // str
        target = PyShadow_ForwardRef(target);
        if (target == NULL)
        {
            Py_DECREF(shadow);
            return NULL;
        }
        target_tp = 1;
    }
    if (alias == Py_None)
    {
        alias = (PyObject *)Py_TYPE(Py_None);
    }
    else if (alias_tp == 3)
    {
        alias = PyShadow_ForwardRef(alias);
        if (alias == NULL)
        {
            //TODO: I need to decref target here, but only if it's a forward ref..
            Py_DECREF(shadow);
            return NULL;
        }
        alias_tp = 1;
    }

    /* in the simple case, both target and alias are simple types */
    if (target_tp == 1 && alias_tp == 1)
    {
        // sanity check both the same

        if (target == alias)
        { // is this valid?
            //printf("target==alias\n");
            Py_DECREF(shadow);
            Py_INCREF(target);
            return target;
        }

        params = PyTuple_Pack(2, target, alias);
        if (params == NULL)
        {
            // TODO: should I just return NULL?
            PyErr_SetString(PyExc_TypeError, "unknown error occured");
            Py_DECREF(shadow);
            Py_DECREF(params);
            return NULL;
        }
        Py_INCREF(params); // needed?
        shadow->s_params = (PyTupleObject *)params;
    }
    // if "type | union", then check if type in union, and merge
    else if (target_tp == 1 && alias_tp == 2)
    {
        shadowobject *s_alias = (shadowobject *)alias;
        Py_ssize_t res = union_check_in(s_alias, target);
        if (res > 0)
        {
            // target is in alias, so we just use alias
            Py_INCREF(s_alias->s_params);
            // re-use
            shadow->s_params = s_alias->s_params;
        }
        else
        {
            // append tuple after type
            Py_ssize_t alias_size = PyTuple_Size((PyObject *)s_alias->s_params);
            if (alias_size < 0)
            {
                // error
                Py_DECREF(shadow);
                return NULL;
            }
            params = PyTuple_New(alias_size + 1);
            if (params == NULL)
            {
                // TOOD: NEW will set the error?
                Py_DECREF(shadow);
                return NULL;
            }
            if (PyTuple_SetItem(params, 0, target) < 0)
            {
                Py_DECREF(params);
                Py_DECREF(shadow);
                return NULL;
            }
            Py_INCREF(target); // I think?

            params = _copy_union_into(params, (PyObject *)s_alias->s_params, alias_size, 1);
            if (params == NULL)
            {
                Py_DECREF(params);
                Py_DECREF(shadow);
                return NULL;
            }

            shadow->s_params = (PyTupleObject *)params;
        }
    }
    // if "union | type", then check if type in union, and merge
    else if (target_tp == 2 && alias_tp == 1)
    {
        //printf("union | type\n");
        shadowobject *s_target = (shadowobject *)target;
        Py_ssize_t res = union_check_in(s_target, alias);
        if (res > 0)
        {
            //printf("-- type is in union, return union\n");
            // alias is in target, so we just use taret
            Py_INCREF(s_target->s_params);
            // re-use
            shadow->s_params = s_target->s_params;
        }
        else
        {
            //printf("-- type is not in union, need to append\n");
            // append type after tuple
            Py_ssize_t target_size = PyTuple_Size((PyObject *)s_target->s_params);
            if (target_size < 0)
            {
                // error
                Py_DECREF(shadow);
                return NULL;
            }
            params = PyTuple_New(target_size + 1);
            if (params == NULL)
            {
                // TOOD: NEW will set the error?
                Py_DECREF(shadow);
                return NULL;
            }

            params = _copy_union_into(params, (PyObject *)s_target->s_params, target_size, 0);
            if (params == NULL)
            {
                Py_DECREF(params);
                Py_DECREF(shadow);
                return NULL;
            }

            if (PyTuple_SetItem(params, target_size, alias) < 0)
            {
                Py_DECREF(params);
                Py_DECREF(shadow);
                return NULL;
            }
            Py_INCREF(alias); // I think?

            shadow->s_params = (PyTupleObject *)params;
        }
    }
    // if "union | union", check if each element of the smaller union is
    // in the other union, then merge
    else if (target_tp == 2 && alias_tp == 2)
    {
        // size of new alias list is going to be at least the size of the lhs
        // tmp = tuple of length(rhs)
        // for each item in rhs
        //  if item in lhs: continue
        //  else: tmp.append(item)
        // params = tuple_concat(target, tmp)
        Py_ssize_t rhs_size = PyTuple_Size((PyObject *)((shadowobject *)alias)->s_params);
        if (rhs_size < 0)
        {
            Py_DECREF(shadow);
            return NULL;
        }
        // special case
        if (rhs_size == 0)
        { // shouldn't happend
            Py_INCREF(((shadowobject *)target)->s_params);
            shadow->s_params = ((shadowobject *)target)->s_params;
        }
        else
        {
            PyObject *tmp = PyTuple_New(rhs_size);
            if (tmp == NULL)
            {
                Py_DECREF(shadow);
                return NULL;
            }
            Py_ssize_t offset = 0;
            for (Py_ssize_t i = 0; i < rhs_size; i++)
            {
                PyObject *other = PyTuple_GetItem(alias, i);
                if (other == NULL)
                {
                    Py_DECREF(shadow);
                    Py_DECREF(tmp);
                    return NULL;
                }
                if (union_check_in(((shadowobject *)target), other) > 0)
                {
                    // this item is in the other
                    continue;
                }
                if (PyTuple_SetItem(tmp, offset, other) < 0)
                {
                    Py_DECREF(shadow);
                    Py_DECREF(tmp);
                    return NULL;
                }
                Py_INCREF(other); // i think
                offset++;
            }
            if (offset < rhs_size)
            {
                // tmp contains potentials NULLs
                PyObject *tmp2 = PyTuple_New(offset);
                tmp2 = _copy_union_into(tmp2, tmp, offset, 0);
                Py_DECREF(tmp); // dealloc tmp if needed
                tmp = tmp2;
            }

            // now tmp contains all the elements from rhs not in lhs
            // so we can concat
            params = PySequence_Concat(
                (PyObject *)((shadowobject *)target)->s_params,
                (PyObject *)((shadowobject *)alias)->s_params);

            Py_DECREF(tmp); // not needed now

            if (params == NULL)
            {
                Py_DECREF(shadow);
                return NULL;
            }
            shadow->s_params = (PyTupleObject *)params;
        }
    }
    return (PyObject *)shadow;
}

// The public interface for creating a union in code
PyObject *
PyShadow_Union(PyObject *target, PyObject *alias)
{
    return PyShadow_Union_(&PyShadow_Type, target, alias);
}

PyObject *
PyShadow_ForwardRef(PyObject *ref_str)
{
    shadowobject *shadow;
    PyTypeObject *type = &PyShadow_Type;
    PyObject *params;

    if (ref_str == NULL || !PyUnicode_Check(ref_str))
    {
        PyErr_SetString(PyExc_TypeError, "Cannot create shadow forward ref from non-string");
        return NULL;
    }
    shadow = (shadowobject *)type->tp_alloc(type, 0);
    if (shadow == NULL)
    {
        return NULL;
    }
    shadow->s_tp = PyShadow_ForwardRefTp;

    params = PyTuple_Pack(1, ref_str);
    if (params == NULL)
    {
        Py_DECREF(shadow);
        return NULL;
    }

    Py_INCREF(params);
    shadow->s_params = params;

    return (PyObject *)shadow;
}

/*[clinic input]
@classmethod
shadowobject.__new__ as shadow_new
    

return a shadow object

[clinic start generated code]*/

static PyObject *
shadow_new_impl(PyTypeObject *type)
/*[clinic end generated code: output=9737e60b2ff87da9 input=8b326cc43aace39e]*/
{

    shadowobject *shadow = (shadowobject *)type->tp_alloc(type, 0);
    if (shadow == NULL)
    {
        return NULL;
    }
    shadow->s_tp = PyShadow_Base;
    shadow->s_params = (PyTupleObject *)PyTuple_New(0); // ensure this is always a valid tuple
    if (shadow->s_params == NULL)
    {
        Py_DECREF(shadow);
        return NULL;
    }
    //printf("shadow__new__\n");
    return (PyObject *)shadow;
}

/*[clinic input]
shadowobject.__init__ as shadow_init

    tp: 'i' = 0
    /

[clinic start generated code]*/

static int
shadow_init_impl(shadowobject *self, int tp)
/*[clinic end generated code: output=bf5aecee1a0ad583 input=e3d724cf2fb6500a]*/
{
    //printf("shadow.init(%i)\n", tp);
    self->s_tp = tp;
    self->s_params = (PyTupleObject *)PyTuple_New(0); // ensure this is always a valid tuple
    if (self->s_params == NULL)
    {
        return -1;
    }
    return 0;
}

static void
shadowobject_dealloc(shadowobject *shadow)
{
    PyObject_GC_UnTrack(shadow);
    Py_XDECREF(shadow->s_params);
    Py_TYPE(shadow)->tp_free(shadow);
}

static int
shadowobject_traverse(shadowobject *shadow, visitproc visit, void *arg)
{
    Py_VISIT(shadow->s_params);
    return 0;
}

static PyObject *
shadow_repr(shadowobject *shadow)
{
    /* since repr is "debug" function, and out of scope for isinstance/issubclass
       we can import the typing module here, and do a promotion to typing.Union
    */
    PyObject *promoted, *s;
    //printf("called shadow_repr %p\n", shadow);
    // TODO: this is causing an infinite recursion from pickling
    promoted = _get_promoted((PyObject *)shadow);
    if (promoted == NULL)
    {
        return NULL;
    }
    /*int x = PyObject_IsInstance(promoted, &PyShadow_Type);
    if (x > 0) {
        printf("after promotion %p shadow is still shadow type with tp=%d\n", shadow, shadow->s_tp);
    }*/

    s = PyObject_Repr(promoted);
    Py_DECREF(promoted);

    return s;
}

PyObject *
PyShadow_UnionAsTuple(PyObject *self)
{
    shadowobject *ob = (shadowobject *)self;
    // todo: type check?
    return (PyObject *)ob->s_params;
}

static PyObject *
shadow_iter(PyObject *self)
{
    // todo: type check?
    return PyObject_GetIter(PyShadow_UnionAsTuple(self));
}

static PyObject *
shadow_richcompare(PyObject *v, PyObject *w, int op)
{
    shadowobject *vu, *wu;
    Py_ssize_t i;
    int cmp;
    Py_ssize_t vlen, wlen;

    // TODO: typecheck

    if (!PyShadow_CheckExact(v) || !PyShadow_CheckExact(w))
    {
        Py_RETURN_NOTIMPLEMENTED;
    }
    if (op != Py_EQ && op != Py_NE)
    {
        Py_RETURN_NOTIMPLEMENTED;
    }
    vu = (shadowobject *)v;
    wu = (shadowobject *)w;
    
    if (vu->s_tp != wu->s_tp) {
        // can't compare types that aren't equal
        Py_RETURN_NOTIMPLEMENTED;
    }

    vlen = PyTuple_Size((PyObject *)vu->s_params);
    wlen = PyTuple_Size((PyObject *)wu->s_params);
    // TODO: what if vlen or wlen is -1

    if (vlen != wlen)
    {
        if (op == Py_EQ)
        {
            Py_RETURN_FALSE;
        }
        if (op == Py_NE)
        {
            Py_RETURN_TRUE;
        }
    }
    // vlen and wlen are equal if we reach here

    // for each item in vlen, check if it's in wlen
    for (i = 0; i < vlen; i++)
    {
        PyObject *item = PyTuple_GET_ITEM((PyObject *)vu->s_params, i);
        if (item == NULL)
        {
            // error is set in GET_ITEM
            return NULL;
        }
        cmp = PySequence_Contains((PyObject *)wu->s_params, item);
        if (cmp == -1)
        {
            PyErr_SetString(PyExc_RuntimeError, "unknown error in compare"); //??
            return NULL;
        }
        if (cmp == 0)
        {
            // can't find item in w, bail early
            if (op == Py_EQ)
            {
                Py_RETURN_FALSE;
            }
            if (op == Py_NE)
            {
                Py_RETURN_TRUE;
            }
        }
    }

    // if we get here, the lists are equal
    if (op == Py_EQ)
    {
        Py_RETURN_TRUE;
    }
    if (op == Py_NE)
    {
        Py_RETURN_FALSE;
    }
    Py_RETURN_FALSE; // unreachable?
}

static PyObject *
PyShadow_gettp(shadowobject *self, void *closure)
{
    return PyLong_FromLong(self->s_tp);
}

static PyObject *
PyShadow__getstate__(shadowobject *self, PyObject *Py_UNUSED(ignored))
{
    PyObject *ret, *tp;

    tp = PyLong_FromLong(self->s_tp);
    if (tp == NULL)
    {
        return NULL;
    }

    ret = PyTuple_Pack(2, tp, (PyObject *)self->s_params); // will incref tp?
    if (ret == NULL)
    {
        Py_DECREF(tp);
        return NULL;
    }

    return ret;
}

/*[clinic input]
shadowobject.__setstate__ as shadow_setstate

    state: 'O'
    /
 
[clinic start generated code]*/

static PyObject *
shadow_setstate(shadowobject *self, PyObject *state)
/*[clinic end generated code: output=b1b85de8af0c68a6 input=f9bb611a6b4bc293]*/
{
    //printf("setstate\n");
    PyObject *params, *tpitem;
    int tp = 0;
    if (!PyTuple_CheckExact(state))
    {
        return PyErr_Format(PyExc_TypeError, "ShadowType.__setstate__ received non-tuple state");
    }

    if (PyTuple_Size(state) != 2)
    {
        return PyErr_Format(PyExc_ValueError, "ShadowType.__setstate__ received tuple of wrong length");
    }

    tpitem = PyTuple_GetItem(state, 0); // do I need to incref if I don't use it?
    if (tpitem == NULL || !PyLong_CheckExact(tpitem))
    {
        return PyErr_Format(PyExc_TypeError, "tp argument of Shadow.__setstate__ is not int");
    }
    tp = PyLong_AsLong(tpitem);

    if (tp < 0 || tp > PyShadow_MAXTp)
    {
        return PyErr_Format(PyExc_ValueError, "__setstate__ tp is not in correct range");
    }
    //printf("setstate, tp=%d\n", tp);
    params = PyTuple_GetItem(state, 1);
    if (tpitem == NULL || !PyTuple_CheckExact(params))
    {
        return PyErr_Format(PyExc_TypeError, "could not get params from state");
    }

    shadowobject *ret = (shadowobject *)shadow_new_impl(&PyShadow_Type);
    if (ret == NULL)
    {
        return NULL;
    }
    ret->s_tp = tp;
    Py_INCREF(params);
    Py_DECREF(ret->s_params);
    ret->s_params = (PyTupleObject *)params;
    //printf("returning ret.tp=%d\n", ret->s_tp);
    return (PyObject *)ret;
}

static PyObject *
shadow_getitem(shadowobject *self, PyObject *key)
{
    /*
    Since this isn't related to isinstance/issubclass, we do a promotion,
    and handle self[key] in the python land
    */
    PyObject *promoted, *ret;
    promoted = _get_promoted((PyObject *)self);
    if (promoted == NULL)
    {
        return NULL;
    }

    ret = PyObject_GetItem(promoted, key);
    Py_DECREF(promoted);
    if (ret == NULL)
    {
        return NULL;
    }
    Py_INCREF(ret); // needed?

    return ret;
}

static Py_hash_t
shadow_hash(shadowobject *self)
{
    //printf("called shadow_hash\n");
    PyObject *tuple, *tp;
    Py_hash_t ret;
    tp = PyLong_FromLong(self->s_tp);
    if (tp == NULL)
    {
        return -1;
    }

    tuple = PyTuple_Pack(2, tp, self->s_params);
    if (tuple == NULL)
    {
        Py_DECREF(tp);
        return -1;
    }

    ret = PyObject_Hash(tuple);
    Py_DECREF(tp);
    Py_DECREF(tuple);
    return ret;
}

static PyObject *
shadow_reduce(shadowobject *shadow, PyObject *Py_UNUSED(ignore))
{
    PyObject *args, *state, *ret;
    args = PyTuple_New(0);
    if (args == NULL)
    {
        PyErr_SetString(PyExc_RuntimeError, "Could not create args tuple");
        return NULL;
    }
    state = PyShadow__getstate__(shadow, NULL);
    if (state == NULL)
    {
        PyErr_SetString(PyExc_RuntimeError, "Could not create state");
        //Py_DECREF(args);
        return NULL;
    }

    /*ret = Py_BuildValue("N(i)O",
    (PyObject *)&PyShadow_Type,
    shadow->s_tp,
    state
    );*/
    ret = PyTuple_Pack(3, (PyObject *)&PyShadow_Type, args, state);

    // no longer need these
    //Py_DECREF(args);
    Py_DECREF(state);

    if (ret == NULL)
    {
        PyErr_SetString(PyExc_RuntimeError, "Could not pack tuple");
        return NULL;
    }
    return (PyObject *)ret;
}
PyDoc_STRVAR(reduce_doc, "Return state information for pickling");

static PyGetSetDef PyShadow_gettersetters[] = {
    {"tp", (getter)PyShadow_gettp, NULL, "shadow type", NULL},
    {NULL} /* sentinal */
};

static PyMethodDef shadow_methods[] = {
    {"__getstate__", (PyCFunction)PyShadow__getstate__, METH_NOARGS, "__getstate__"},
    SHADOW_SETSTATE_METHODDEF
    //{"__reduce__", (PyCFunction)shadow_reduce, METH_NOARGS, reduce_doc},
    {NULL, NULL} /* sentinal */
};

static PyMappingMethods PyShadow_mapping = {
    .mp_subscript = (binaryfunc)shadow_getitem,
};

PyTypeObject PyShadow_Type = {
    PyVarObject_HEAD_INIT(&PyType_Type, 0)
    .tp_name = "typing._ShadowType",
    .tp_doc = shadow_new__doc__,
    .tp_basicsize = sizeof(shadowobject),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC | Py_TPFLAGS_BASETYPE | Py_TPFLAGS_TYPE_SUBCLASS,
    .tp_new = shadow_new,
    .tp_init = (initproc)shadow_init,

    .tp_alloc = PyType_GenericAlloc,
    .tp_dealloc = (destructor)shadowobject_dealloc,
    .tp_free = PyObject_GC_Del,
    .tp_repr = (reprfunc)shadow_repr,
    .tp_traverse = (traverseproc)shadowobject_traverse,
    .tp_richcompare = shadow_richcompare,
    .tp_iter = shadow_iter,
    .tp_hash = (hashfunc)shadow_hash,
    .tp_methods = shadow_methods,
    .tp_getset = PyShadow_gettersetters,
    .tp_as_mapping = &PyShadow_mapping,
};

PyObject *
_get_promoted(PyObject *o)
{
    PyObject *typing, *Union, *ForwardRef, *promoted = NULL;
    shadowobject *shadow = (shadowobject *)o;
    int tp = shadow->s_tp;
    
    if (tp == PyShadow_Base) {
        // it was created in python land, so just return it
        Py_INCREF(o);
        return o;
    }

    typing = PyImport_ImportModule("typing");
    if (typing == NULL)
    {
        PyErr_SetString(PyExc_RuntimeError, "couldn't import typing");
        return NULL;
    }
    
    switch (tp) {
        case PyShadow_UnionTp: {
            Union = PyObject_GetAttrString(typing, "Union");
            if (Union == NULL)
            {
                PyErr_SetString(PyExc_RuntimeError, "couldn't find typing.Union");
                break;
            }
            // incref s_params?
            promoted = PyObject_GetItem(Union, (PyObject *)shadow->s_params);
            Py_DECREF(Union);
        };
        break;
        case PyShadow_ForwardRefTp: {
            if (PyTuple_Size((PyObject *)shadow->s_params) != 1) {
                PyErr_SetString(PyExc_RuntimeError, "shadow forwardref length != 1");
                break;
            }
            ForwardRef = PyObject_GetAttrString(typing, "ForwardRef");
            if (ForwardRef == NULL) 
            {
                PyErr_SetString(PyExc_RuntimeError, "couldn't find typing.ForwardRef");
                break;
            }
            
            promoted = PyObject_CallFunction(ForwardRef, "O", PyTuple_GetItem((PyObject *)shadow->s_params, 0));
            Py_DECREF(ForwardRef);
        }
        break;
        default:
            PyErr_Format(PyExc_NotImplementedError, "shadow promotion for type %i not implemented", shadow->s_tp);
    }
    
    Py_DECREF(typing);

    if (promoted == NULL)
    {
        //PyErr_SetString(PyExc_RuntimeError, "couldn't call typing._promote_shadow(shadow)");
        return NULL;
    }
    Py_INCREF(promoted);

    // TODO: needs incref?
    return promoted;
}
