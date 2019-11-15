/* shadow object; a dummy type object for use in type hinting. 
  typing.py should promote it to an actual type when encountered */

#include "Python.h"
#include "structmember.h"

// Dummy Type to use in typing.py
/*[clinic input]
class shadowobject "shadowobject *" "&PyShadow_Type"
[clinic start generated code]*/
/*[clinic end generated code: output=da39a3ee5e6b4b0d input=08165972310008df]*/
typedef struct
{
    PyObject_HEAD
    int s_flags; // the type
    PyTupleObject *s_params;   /* tuple of params */
} shadowobject;

#include "clinic/shadowobject.c.h"

/*
The flags field is used to indicate where the object came from (eg promoted),
and what type it is.
32 .....            1 0
+-----------+-------+-+
| undefined |7654321|0|
+-----------+-------+-+
      ^        ^     ^
      |        |     +-> 1 bit for created in C vs python flag
      |        +-------> 7 bits for the type
      +----------------> all other bits are undefined for now

The lowest bit tells us if the object was created in the interpreter, or from python
The next 4 lowest bits are the "type", eg Union vs ForwardRef vs TypeVar
All other bits are undefined for now

This layout/flags allows for flags == 0 to be the default for 
unions created in typing.py (eg, typing.Union has flags == 0)
*/
#define PyShadow_FlagPy 0 // created in python land
#define PyShadow_FlagC  1  // created in C land
#define PyShadow_FlagUnion      (0 << 1)
#define PyShadow_FlagForwardRef (1 << 1)
#define PyShadow_FlagTypeVar    (2 << 1)

// the following might be moved into another bit
#define PyShadow_FlagBasicType (126 << 1) // aka any other type that we're not interested in (used internally)
#define PyShadow_FlagInvalid   (127 << 1) // used internally

// the positions
#define PyShadow_MaskOrigin 0x01 // just the lowest bit // TODO: another name?
#define PyShadow_MaskType   0xfe // the 7 bits after the first

#define SHADOW_FLAGS(shadow) ( ((shadowobject *)shadow)->s_flags )

#define _IS_PY(flags) ( (flags & PyShadow_MaskOrigin) == PyShadow_FlagPy )
#define _IS_C(flags)  ( (flags & PyShadow_MaskOrigin) == PyShadow_FlagC )
#define _IS_TYPE_UNION(flags)      ( (flags & PyShadow_MaskType) == PyShadow_FlagUnion )
#define _IS_TYPE_FORWARDREF(flags) ( (flags & PyShadow_MaskType) == PyShadow_FlagForwardRef )
#define _IS_TYPE_TYPEVAR(flags)    ( (flags & PyShadow_MaskType) == PyShadow_FlagTypeVar )
#define _IS_TYPE_BASIC(flags)      ( (flags & PyShadow_MaskType) == PyShadow_FlagBasicType )
#define _IS_TYPE_INVALID(flags)    ( (flags & PyShadow_MaskType) == PyShadow_FlagInvalid )

#define IS_PY(shadow) (_IS_PY(SHADOW_FLAGS(shadow)))
#define IS_C(shadow)  (_IS_C(SHADOW_FLAGS(shadow)))
#define IS_TYPE_UNION(shadow)      (_IS_TYPE_UNION(SHADOW_FLAGS(shadow)))
#define IS_TYPE_FORWARDREF(shadow) (_IS_TYPE_FORWARDREF(SHADOW_FLAGS(shadow)))
#define IS_TYPE_TYPEVAR(shadow)    (_IS_TYPE_TYPEVAR(SHADOW_FLAGS(shadow)))

PyObject *_get_promoted(PyObject *shadow);
PyObject *_get_union_args(PyObject *shadow);

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
union_check_in(PyObject *tuple, PyObject *needle)
{
    // return 1 if needle is in un.un_params, else -1
    Py_ssize_t len = PyTuple_Size(tuple);
    if (len == 0)
    {
        return -1;
    }
    for (Py_ssize_t i = 0; i < len; i++)
    {
        PyObject *other = PyTuple_GetItem(tuple, i);
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

PyObject *
normalize_arg(PyObject *arg, int *flags)
{
    PyObject *tmp = NULL;
    if (arg == NULL) {
        *flags = PyShadow_FlagC | PyShadow_FlagInvalid;
        return NULL;
    }
    if (arg == Py_None) {
        Py_DECREF(arg);
        arg = (PyObject *)Py_TYPE(Py_None);
        Py_INCREF(arg);
        *flags = PyShadow_FlagC | PyShadow_FlagBasicType;
        return arg;
    }
    if (PyObject_IsInstance(arg, (PyObject *)&PyUnicode_Type) > 0)
    {
        tmp = arg;
        arg = PyShadow_ForwardRef(tmp);
        if (arg == NULL) {
            *flags = PyShadow_FlagC | PyShadow_FlagInvalid;
            return NULL;
        }
        Py_DECREF(tmp);
        *flags = PyShadow_FlagC | PyShadow_FlagForwardRef;
        // TODO: incref arg?
        return arg;
    }
    if (PyObject_IsInstance(arg, (PyObject *)&PyType_Type) > 0)
    {
        *flags = PyShadow_FlagC | PyShadow_FlagBasicType;
        // TODO: incref arg?
        return arg;
    }
    if (PyObject_IsInstance(arg, (PyObject *)&PyShadow_Type) > 0)
    {
        // this branch covers 
        // - isinstance(arg, (...,TypeVar, ForwardRef))
        // - return arg
        // Hmm, so we have an object that inherits from shadow, or may be a shadow
        // but is there a better way to access the flags that using getattr?
        tmp = PyObject_GetAttrString(arg, "_shadow_flags");
        if (tmp == NULL) {
            return NULL;
        }
        *flags = PyLong_AsLong(tmp);
        Py_DECREF(tmp);
        // TODO: incref arg?
        return arg;
    }
    if (!PyCallable_Check(arg)) {
        // equiv to the `if not callable(...)` line in type_check
        *flags  = PyShadow_FlagPy | PyShadow_FlagInvalid;
        
        return arg;
    }
    
    // otherwise, we don't really care about the type, it's "valid"
    *flags = PyShadow_FlagPy | PyShadow_FlagBasicType;
    return arg;
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
#define _IS_SIMPLE(x) (_IS_TYPE_BASIC(x) || !_IS_TYPE_UNION(x)) // TODO: this might need to change
    shadowobject *shadow;
    PyObject *params, *_target = target, *_alias = alias;
    int target_flags = 0, alias_flags = 0;

    if (target == NULL || alias == NULL)
    {
        PyErr_SetString(PyExc_TypeError, "both 'target' and 'alias' expected");
        return NULL;
    }
    
    target = normalize_arg(target, &target_flags);
    if (target == NULL) {
        return NULL;
    }
    if (_IS_TYPE_INVALID(target_flags)) {
        PyErr_Format(PyExc_TypeError, "invalid type 'target', should be type, None, or union, got %.100R", _target);
        return NULL;
    }
    
    alias = normalize_arg(alias, &alias_flags);
    if (alias == NULL) {
        return NULL;
    }
    if (_IS_TYPE_INVALID(alias_flags)) 
    {
        PyErr_Format(PyExc_TypeError, "invalid type 'alias', should be type, None, or union, got %.100R", _alias);
        return NULL;
    }

    shadow = (shadowobject *)type->tp_alloc(type, 0);
    if (shadow == NULL)
    {
        return NULL;
    }
    shadow->s_flags = PyShadow_FlagC | PyShadow_FlagUnion;

    /* in the simple case, both target and alias are simple types */
    if (_IS_SIMPLE(target_flags) && _IS_SIMPLE(alias_flags))
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
            Py_DECREF(shadow);
            Py_DECREF(params);
            return NULL;
        }
        Py_INCREF(params); // needed?
        shadow->s_params = (PyTupleObject *)params;
    }
    // if "simpletype | union", then check if type in union, and merge
    else if (_IS_SIMPLE(target_flags) && _IS_TYPE_UNION(alias_flags))
    {
        PyObject *alias_args = _get_union_args(alias);
        if (alias_args == NULL) {
            //error
            Py_DECREF(shadow);
            return NULL;
        }
        Py_ssize_t res = union_check_in(alias_args, target);
        if (res > 0)
        {
            // target is in alias, so we just use alias
            Py_INCREF(alias_args);
            // re-use
            shadow->s_params = (PyTupleObject *)alias_args;
        }
        else
        {
            // append tuple after type
            Py_ssize_t alias_size = PyTuple_Size(alias_args);
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

            params = _copy_union_into(params, alias_args, alias_size, 1);
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
    else if (_IS_TYPE_UNION(target_flags) && _IS_SIMPLE(alias_flags))
    {
        //printf("union | type\n");
        PyObject *target_args = _get_union_args(target);
        if (target_args == NULL) {
            Py_DECREF(shadow);
            return NULL;
        }
        Py_ssize_t res = union_check_in(target_args, alias);
        if (res > 0)
        {
            //printf("-- type is in union, return union\n");
            // alias is in target, so we just use taret
            Py_INCREF(target_args);
            // re-use
            shadow->s_params = (PyTupleObject *)target_args;
        }
        else
        {
            //printf("-- type is not in union, need to append\n");
            // append type after tuple
            Py_ssize_t target_size = PyTuple_Size(target_args);
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

            params = _copy_union_into(params, target_args, target_size, 0);
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
    else if (_IS_TYPE_UNION(target_flags) && _IS_TYPE_UNION(alias_flags))
    {
        PyObject *target_args = _get_union_args(target);
        PyObject *alias_args = _get_union_args(alias);
        if (target_args == NULL) {
            Py_DECREF(shadow);
            return NULL;
        }
        if (alias_args == NULL) {
            Py_DECREF(shadow);
            return NULL;
        }
        
        // size of new alias list is going to be at least the size of the lhs
        // tmp = tuple of length(rhs)
        // for each item in rhs
        //  if item in lhs: continue
        //  else: tmp.append(item)
        // params = tuple_concat(target, tmp)
        Py_ssize_t rhs_size = PyTuple_Size(alias_args);
        if (rhs_size < 0)
        {
            Py_DECREF(shadow);
            return NULL;
        }
        // special case
        if (rhs_size == 0)
        { // shouldn't happend
            Py_INCREF(target_args);
            shadow->s_params = (PyTupleObject *)target_args;
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
                PyObject *other = PyTuple_GetItem(alias_args, i);
                if (other == NULL)
                {
                    Py_DECREF(shadow);
                    Py_DECREF(tmp);
                    return NULL;
                }
                if (union_check_in(target_args, other) > 0)
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
                target_args,
                alias_args);

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
#undef _IS_SIMPLE
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
    shadow->s_flags = PyShadow_FlagC | PyShadow_FlagForwardRef;

    params = PyTuple_Pack(1, ref_str);
    if (params == NULL)
    {
        Py_DECREF(shadow);
        return NULL;
    }

    Py_INCREF(params);
    shadow->s_params = (PyTupleObject *)params;

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
    shadow->s_flags = PyShadow_FlagC | PyShadow_FlagInvalid; // must be set by caller
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

    tp: 'i' = 0xff
    /

[clinic start generated code]*/

static int
shadow_init_impl(shadowobject *self, int tp)
/*[clinic end generated code: output=bf5aecee1a0ad583 input=2eadbe23bccc880d]*/
{
    //printf("shadow.init(%i)\n", tp);
    self->s_flags = tp;
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
    Py_ssize_t i;
    _PyUnicodeWriter writer;

    i = Py_ReprEnter((PyObject *)shadow);
    if (i != 0)
    {
        return i > 0 ? PyUnicode_FromString("...") : NULL;
    }
    _PyUnicodeWriter_Init(&writer);
    writer.overallocate = 1; // TODO
    /* "<ShadowType>" */
    writer.min_length = 12;

    if (IS_PY(shadow))
    {
        PyObject *s = PyUnicode_FromFormat("<ShadowType flags=%02x at %p>",
                                           shadow->s_flags, shadow);
        // SHOULD NOT GET HERE (because a typing.<TYPE> should implement __repr__)?
        if (_PyUnicodeWriter_WriteStr(&writer, s) < 0)
        {
            goto error;
        }
        goto end;
    }
    else
    { // else created from shadowobject.c or type.__or__
        if (IS_TYPE_UNION(shadow))
        {
            if (_PyUnicodeWriter_WriteASCIIString(&writer, "ShadowUnion[", 12) < 0)
            {
                goto error;
            }
        }
        else if (IS_TYPE_FORWARDREF(shadow))
        {
            if (_PyUnicodeWriter_WriteASCIIString(&writer, "ShadowForwardRef[", 17) < 0)
            {
                goto error;
            }
        }
        else if (IS_TYPE_TYPEVAR(shadow))
        {
            if (_PyUnicodeWriter_WriteASCIIString(&writer, "ShadowTypeVar[", 14) < 0)
            {
                goto error;
            }
        }
        else
        {
            PyErr_Format(PyExc_NotImplementedError, "cannot repr shadow with flags %04x", shadow->s_flags);
            goto error;
        }

        PyObject *s;
        s = PyObject_Repr((PyObject *)shadow->s_params);
        if (s == NULL)
        {
            goto error;
        }
        if (_PyUnicodeWriter_WriteStr(&writer, s) < 0)
        {
            Py_DECREF(s);
            goto error;
        }
        Py_DECREF(s);

        if (_PyUnicodeWriter_WriteChar(&writer, ']') < 0)
        {
            goto error;
        }
    }
end:
    Py_ReprLeave((PyObject *)shadow);
    return _PyUnicodeWriter_Finish(&writer);
error:
    _PyUnicodeWriter_Dealloc(&writer);
    Py_ReprLeave((PyObject *)shadow);
    return NULL;
}

PyObject *
PyShadow_UnionAsTuple(PyObject *self)
{
    shadowobject *ob = (shadowobject *)self;
    if (!IS_TYPE_UNION(ob)) {
        return NULL;
    }
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
    
    if (vu->s_flags != wu->s_flags)
    {
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
PyShadow_getorigin(shadowobject *self, void *closure)
{
    return PyLong_FromLong(self->s_flags & PyShadow_MaskOrigin);
}
static PyObject *
PyShadow_gettype(shadowobject *self, void *closure)
{
    return PyLong_FromLong(self->s_flags & PyShadow_MaskType);
}
static PyObject *
PyShadow_getflags(shadowobject *self, void *closure)
{
    return PyLong_FromLong(self->s_flags);
}
/*
static PyObject *
PyShadow__args__(shadowobject *self, void *Py_UNUSED(closure))
{
    Py_INCREF(self->s_params);
    return (PyObject *)self->s_params;
}
*/

// TODO: there must be a better way to have class attributes ?
static PyObject *
PyShadow_flagpy(PyTypeObject *type, void *Py_UNUSED(closure))
{
    return PyLong_FromLong(PyShadow_FlagPy);
}
static PyObject *
PyShadow_flagc(PyTypeObject *type, void *Py_UNUSED(closure))
{
    return PyLong_FromLong(PyShadow_FlagC);
}
static PyObject *
PyShadow_flagunion(PyTypeObject *type, void *Py_UNUSED(closure))
{
    return PyLong_FromLong(PyShadow_FlagUnion);
}
static PyObject *
PyShadow_flagforwardref(PyTypeObject *type, void *Py_UNUSED(closure))
{
    return PyLong_FromLong(PyShadow_FlagForwardRef);
}
static PyObject *
PyShadow_flagtypevar(PyTypeObject *type, void *Py_UNUSED(closure))
{
    return PyLong_FromLong(PyShadow_FlagTypeVar);
}
static PyObject *
PyShadow_flagbasic(PyTypeObject *type, void *Py_UNUSED(closure))
{
    return PyLong_FromLong(PyShadow_FlagBasicType);
}
static PyObject *
PyShadow_flaginvalid(PyTypeObject *type, void *Py_UNUSED(closure))
{
    return PyLong_FromLong(PyShadow_FlagInvalid);
}
static PyObject *
PyShadow_maskorigin(PyTypeObject *type, void *Py_UNUSED(closure))
{
    return PyLong_FromLong(PyShadow_MaskOrigin);
}
static PyObject *
PyShadow_masktype(PyTypeObject *type, void *Py_UNUSED(closure))
{
    return PyLong_FromLong(PyShadow_MaskType);
}

static PyGetSetDef PyShadow_gettersetters[] = {
    {"_shadow_origin", (getter)PyShadow_getorigin, NULL, "1 if created in C", NULL},
    {"_shadow_type", (getter)PyShadow_gettype, NULL, "shadow type", NULL},
    {"_shadow_flags", (getter)PyShadow_getflags, NULL, "shadow flags", NULL},
    //{"__args__", (getter)PyShadow__args__, NULL, "shadow parameters", NULL},
    
    {"FLAG_PY", (getter)PyShadow_flagpy, NULL, "created in python", NULL},
    {"FLAG_C", (getter)PyShadow_flagc, NULL, "created in c", NULL},
    {"FLAG_UNION", (getter)PyShadow_flagunion, NULL, "union type flag", NULL},
    {"FLAG_FORWARDREF", (getter)PyShadow_flagforwardref, NULL, "forward ref type flat", NULL},
    {"FLAG_TYPEVAR", (getter)PyShadow_flagtypevar, NULL, "typevar type flag", NULL},
    {"FLAG_BASIC", (getter)PyShadow_flagbasic, NULL, "basic type flag", NULL},
    {"FLAG_INVALID", (getter)PyShadow_flaginvalid, NULL, "invalid type flag", NULL},
    {"MASK_ORIGIN", (getter)PyShadow_maskorigin, NULL, "origin mask", NULL},
    {"MASK_TYPE", (getter)PyShadow_masktype, NULL, "type mask", NULL},
    {NULL} /* sentinal */
};

static PyObject *
PyShadow__getstate__(shadowobject *self, PyObject *Py_UNUSED(ignored))
{
    PyObject *ret, *flags;
    flags = PyLong_FromLong(self->s_flags);
    if (flags == NULL)
    {
        return NULL;
    }
    ret = Py_BuildValue("(OO)", flags, (PyObject *)self->s_params);
    if (ret == NULL)
    {
        Py_DECREF(flags);
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
    PyObject *params, *flagitem;
    int flags = PyShadow_FlagC;

    if (self == NULL)
    {
        return NULL;
    }
    if (!PyTuple_CheckExact(state))
    {
        return PyErr_Format(PyExc_TypeError, "ShadowType.__setstate__ received non-tuple state");
    }

    if (PyTuple_Size(state) != 2)
    {
        return PyErr_Format(PyExc_ValueError, "ShadowType.__setstate__ received tuple of wrong length");
    }

    flagitem = PyTuple_GetItem(state, 0); // do I need to incref if I don't use it?
    if (flagitem == NULL || !PyLong_CheckExact(flagitem))
    {
        Py_XDECREF(flagitem);
        return PyErr_Format(PyExc_TypeError, "flags argument of Shadow.__setstate__ is not int");
    }
    flags = PyLong_AsLong(flagitem);
    Py_DECREF(flagitem);

    if (_IS_PY(flags))
    {
        return PyErr_Format(PyExc_ValueError, "__setstate__ flag is invalid");
    }
    //printf("setstate, tp=%d\n", tp);
    params = PyTuple_GetItem(state, 1);
    if (params == NULL || !PyTuple_CheckExact(params))
    {
        Py_XDECREF(params);
        return PyErr_Format(PyExc_TypeError, "could not get params from state");
    }
    /*
    shadowobject *ret = (shadowobject *)shadow_new_impl(&PyShadow_Type);
    if (ret == NULL)
    {
        return NULL;
    }
    */
    self->s_flags = flags;
    Py_INCREF(params);
    Py_DECREF(self->s_params);
    self->s_params = (PyTupleObject *)params;
    //printf("returning ret.tp=%d\n", ret->s_tp);
    return (PyObject *)self;
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
    PyObject *tuple, *flags;
    Py_hash_t ret;
    flags = PyLong_FromLong(self->s_flags);
    if (flags == NULL)
    {
        return -1;
    }

    tuple = PyTuple_Pack(2, flags, self->s_params);
    if (tuple == NULL)
    {
        Py_DECREF(flags);
        return -1;
    }

    ret = PyObject_Hash(tuple);
    Py_DECREF(flags);
    Py_DECREF(tuple);
    return ret;
}

static PyObject *
shadow_reduce(shadowobject *shadow, PyObject *Py_UNUSED(ignore))
{
    PyObject *fn, *args, *state, *ret;
    fn = PyObject_GetAttrString((PyObject *)&PyShadow_Type, "_unpickle");
    if (fn == NULL)
    {
        //PyErr_SetString(PyExc_RuntimeError, "Could not find ShadowType._unpickle");
        return NULL;
    }

    args = PyTuple_New(0);
    if (args == NULL)
    {
        PyErr_SetString(PyExc_RuntimeError, "Could not create args tuple");
        Py_DECREF(fn);
        return NULL;
    }
    state = PyShadow__getstate__(shadow, NULL);
    if (state == NULL)
    {
        PyErr_SetString(PyExc_RuntimeError, "Could not create state");
        Py_DECREF(fn);
        Py_DECREF(args);
        return NULL;
    }

    //ret = PyTuple_Pack(3, fn, args, state);
    ret = Py_BuildValue("(O(OO))", fn, args, state);

    // no longer need these
    Py_DECREF(fn);
    Py_DECREF(args);
    Py_DECREF(state);

    if (ret == NULL)
    {
        PyErr_SetString(PyExc_RuntimeError, "Could not pack tuple");
        return NULL;
    }
    return (PyObject *)ret;
}
PyDoc_STRVAR(reduce_doc, "Return state information for pickling");

/*[clinic input]
@classmethod
shadowobject._unpickle as shadow_unpickle

    tup: 'O'
    state: 'O'

[clinic start generated code]*/

static PyObject *
shadow_unpickle_impl(PyTypeObject *type, PyObject *tup, PyObject *state)
/*[clinic end generated code: output=f493457202450a90 input=e69aab13f29db555]*/
{
    shadowobject *shadow = (shadowobject *)shadow_new_impl(type);
    if (shadow == NULL)
    {
        return NULL;
    }
    shadow = (shadowobject *)shadow_setstate(shadow, state);
    return (PyObject *)shadow;
}

static PyObject *
shadow_or(shadowobject *self, PyObject *other)
{
    return PyShadow_Union((PyObject *)self, other);
}

static PyMethodDef shadow_methods[] = {
   // {"__getstate__", (PyCFunction)PyShadow__getstate__, METH_NOARGS, "__getstate__"},
    SHADOW_UNPICKLE_METHODDEF
    //SHADOW_SETSTATE_METHODDEF
    {"__reduce__", (PyCFunction)shadow_reduce, METH_NOARGS, reduce_doc},
    {NULL, NULL} /* sentinal */
};

static PyMappingMethods PyShadow_mapping = {
    .mp_subscript = (binaryfunc)shadow_getitem,
};

static PyMemberDef members[] = {
    { "_shadow_args", T_OBJECT_EX, offsetof(shadowobject, s_params), READONLY, NULL},
    { NULL }
};

static PyNumberMethods PyShadow_numbermethods = {
    .nb_or = (binaryfunc)shadow_or,
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
    .tp_as_number = &PyShadow_numbermethods,
    .tp_members = members,
};

PyObject *
_get_promoted(PyObject *o)
{
    PyObject *typing, *Union, *ForwardRef, *promoted = NULL;
    shadowobject *shadow = (shadowobject *)o;

    if (IS_PY(shadow))
    {
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

    if (IS_TYPE_UNION(shadow))
    {
        Union = PyObject_GetAttrString(typing, "Union");
        if (Union == NULL)
        {
            PyErr_SetString(PyExc_RuntimeError, "couldn't find typing.Union");
        }
        else
        {
            // incref s_params?
            promoted = PyObject_GetItem(Union, (PyObject *)shadow->s_params);
            Py_DECREF(Union);
        }
    }
    else if (IS_TYPE_FORWARDREF(shadow))
    {
        if (PyTuple_Size((PyObject *)shadow->s_params) != 1)
        {
            PyErr_SetString(PyExc_RuntimeError, "shadow forwardref length != 1");
        }
        else 
        {
            ForwardRef = PyObject_GetAttrString(typing, "ForwardRef");
            if (ForwardRef == NULL)
            {
                PyErr_SetString(PyExc_RuntimeError, "couldn't find typing.ForwardRef");
            }
            else
            {
                promoted = PyObject_CallFunction(ForwardRef, "O", PyTuple_GetItem((PyObject *)shadow->s_params, 0));
                Py_DECREF(ForwardRef);
            }
        }
    }
    else 
    {
        PyErr_Format(PyExc_NotImplementedError, "shadow promotion for type %i not implemented", shadow->s_flags);
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

/* assumption of this function is that it's called where we know shadow is 
   either a shadow object, or inherits from it. We just don't know which.
   
   returns NULL on error, or a tuple object
  */
PyObject *
_get_union_args(PyObject *shadow)
{
    PyObject *flagsitem, *args;
    int flags = PyShadow_FlagInvalid;
    if (shadow == NULL)
    {
        return NULL;
    }
    if (PyShadow_CheckExact(shadow)) {
        if (!IS_TYPE_UNION(shadow)) {
            PyErr_SetString(PyExc_TypeError, "expected union shadow type");
            return NULL;
        }
        //Py_INCREF(((shadowobject *)shadow)->s_params);
        return (PyObject *)((shadowobject *)shadow)->s_params;
    }
    // must inherit from shadowtype
    flagsitem = PyObject_GetAttrString(shadow, "_shadow_flags");
    if (flagsitem == NULL) {
        return NULL;
    }
    flags = PyLong_AsLong(flagsitem);
    Py_DECREF(flagsitem);
    if (!_IS_TYPE_UNION(flags)) {
        PyErr_SetString(PyExc_TypeError, "expected union shadow type");
        return NULL;
    }
    
    if (_IS_PY(flags)) {
        // This means it's most likely a typing.Union
        args = PyObject_GetAttrString(shadow, "__args__");
        if (args == NULL)
        {
            return NULL;
        }
        //Py_INCREF(args);
    }
    else {
        // probably wont get in this branch
        // is C, should have _shadow_args filled out
        args = PyObject_GetAttrString(shadow, "_shadow_args");
        if (args == NULL)
        {
            return NULL;
        }
        //Py_INCREF(args);
    }
    return args;
}
