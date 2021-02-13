/*
 * IDL Type Tree
 *
 * Copyright 2008 Robert Shearman
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "widl.h"
#include "utils.h"
#include "parser.h"
#include "typetree.h"
#include "header.h"

type_t *duptype(type_t *t, int dupname)
{
  type_t *d = alloc_type();

  *d = *t;
  if (dupname && t->name)
    d->name = xstrdup(t->name);

  return d;
}

type_t *make_type(enum type_type type)
{
    type_t *t = alloc_type();
    t->name = NULL;
    t->namespace = NULL;
    t->type_type = type;
    t->attrs = NULL;
    t->c_name = NULL;
    memset(&t->details, 0, sizeof(t->details));
    t->typestring_offset = 0;
    t->ptrdesc = 0;
    t->ignore = (parse_only != 0);
    t->defined = FALSE;
    t->written = FALSE;
    t->user_types_registered = FALSE;
    t->tfswrite = FALSE;
    t->checked = FALSE;
    t->typelib_idx = -1;
    init_loc_info(&t->loc_info);
    return t;
}

static const var_t *find_arg(const var_list_t *args, const char *name)
{
    const var_t *arg;

    if (args) LIST_FOR_EACH_ENTRY(arg, args, const var_t, entry)
    {
        if (arg->name && !strcmp(name, arg->name))
            return arg;
    }

    return NULL;
}

const char *type_get_name(const type_t *type, enum name_type name_type)
{
    switch(name_type) {
    case NAME_DEFAULT:
        return type->name;
    case NAME_C:
        return type->c_name ? type->c_name : type->name;
    }

    assert(0);
    return NULL;
}

static size_t append_namespace(char **buf, size_t *len, size_t pos, struct namespace *namespace, const char *separator, const char *abi_prefix)
{
    int nested = namespace && !is_global_namespace(namespace);
    const char *name = nested ? namespace->name : abi_prefix;
    size_t n = 0;
    if (!name) return 0;
    if (nested) n += append_namespace(buf, len, pos + n, namespace->parent, separator, abi_prefix);
    n += strappend(buf, len, pos + n, "%s%s", name, separator);
    return n;
}

char *format_namespace(struct namespace *namespace, const char *prefix, const char *separator, const char *suffix, const char *abi_prefix)
{
    size_t len = 0, pos = 0;
    char *buf = NULL;
    pos += strappend(&buf, &len, pos, "%s", prefix);
    pos += append_namespace(&buf, &len, pos, namespace, separator, abi_prefix);
    pos += strappend(&buf, &len, pos, "%s", suffix);
    return buf;
}

type_t *type_new_function(var_list_t *args)
{
    var_t *arg;
    type_t *t;
    unsigned int i = 0;

    if (args)
    {
        arg = LIST_ENTRY(list_head(args), var_t, entry);
        if (list_count(args) == 1 && !arg->name && arg->declspec.type && type_get_type(arg->declspec.type) == TYPE_VOID)
        {
            list_remove(&arg->entry);
            free(arg);
            free(args);
            args = NULL;
        }
    }
    if (args) LIST_FOR_EACH_ENTRY(arg, args, var_t, entry)
    {
        if (arg->declspec.type && type_get_type(arg->declspec.type) == TYPE_VOID)
            error_loc("argument '%s' has void type\n", arg->name);
        if (!arg->name)
        {
            if (i > 26 * 26)
                error_loc("too many unnamed arguments\n");
            else
            {
                int unique;
                do
                {
                    char name[3];
                    name[0] = i > 26 ? 'a' + i / 26 : 'a' + i;
                    name[1] = i > 26 ? 'a' + i % 26 : 0;
                    name[2] = 0;
                    unique = !find_arg(args, name);
                    if (unique)
                        arg->name = xstrdup(name);
                    i++;
                } while (!unique);
            }
        }
    }

    t = make_type(TYPE_FUNCTION);
    t->details.function = xmalloc(sizeof(*t->details.function));
    t->details.function->args = args;
    t->details.function->retval = make_var(xstrdup("_RetVal"));
    return t;
}

type_t *type_new_pointer(type_t *ref)
{
    type_t *t = make_type(TYPE_POINTER);
    t->details.pointer.ref.type = ref;
    return t;
}

type_t *type_new_alias(const decl_spec_t *t, const char *name)
{
    type_t *a = make_type(TYPE_ALIAS);

    a->name = xstrdup(name);
    a->attrs = NULL;
    a->details.alias.aliasee = *t;
    init_loc_info(&a->loc_info);

    return a;
}

type_t *type_new_array(const char *name, const decl_spec_t *element, int declptr,
                       unsigned int dim, expr_t *size_is, expr_t *length_is)
{
    type_t *t = make_type(TYPE_ARRAY);
    if (name) t->name = xstrdup(name);
    t->details.array.declptr = declptr;
    t->details.array.length_is = length_is;
    if (size_is)
        t->details.array.size_is = size_is;
    else
        t->details.array.dim = dim;
    if (element)
        t->details.array.elem = *element;
    return t;
}

type_t *type_new_basic(enum type_basic_type basic_type)
{
    type_t *t = make_type(TYPE_BASIC);
    t->details.basic.type = basic_type;
    t->details.basic.sign = 0;
    return t;
}

type_t *type_new_int(enum type_basic_type basic_type, int sign)
{
    static type_t *int_types[TYPE_BASIC_INT_MAX+1][3];

    assert(basic_type <= TYPE_BASIC_INT_MAX);

    /* map sign { -1, 0, 1 } -> { 0, 1, 2 } */
    if (!int_types[basic_type][sign + 1])
    {
        int_types[basic_type][sign + 1] = type_new_basic(basic_type);
        int_types[basic_type][sign + 1]->details.basic.sign = sign;
    }
    return int_types[basic_type][sign + 1];
}

type_t *type_new_void(void)
{
    static type_t *void_type = NULL;
    if (!void_type)
        void_type = make_type(TYPE_VOID);
    return void_type;
}

type_t *type_new_enum(const char *name, struct namespace *namespace, int defined, var_list_t *enums)
{
    type_t *t = NULL;

    if (name)
        t = find_type(name, namespace,tsENUM);

    if (!t)
    {
        t = make_type(TYPE_ENUM);
        t->name = name;
        t->namespace = namespace;
        if (name)
            reg_type(t, name, namespace, tsENUM);
    }

    if (!t->defined && defined)
    {
        t->details.enumeration = xmalloc(sizeof(*t->details.enumeration));
        t->details.enumeration->enums = enums;
        t->defined = TRUE;
    }
    else if (defined)
        error_loc("redefinition of enum %s\n", name);

    return t;
}

type_t *type_new_struct(char *name, struct namespace *namespace, int defined, var_list_t *fields)
{
    type_t *t = NULL;

    if (name)
        t = find_type(name, namespace, tsSTRUCT);

    if (!t)
    {
        t = make_type(TYPE_STRUCT);
        t->name = name;
        t->namespace = namespace;
        if (name)
            reg_type(t, name, namespace, tsSTRUCT);
    }

    if (!t->defined && defined)
    {
        t->details.structure = xmalloc(sizeof(*t->details.structure));
        t->details.structure->fields = fields;
        t->defined = TRUE;
    }
    else if (defined)
        error_loc("redefinition of struct %s\n", name);

    return t;
}

type_t *type_new_nonencapsulated_union(const char *name, int defined, var_list_t *fields)
{
    type_t *t = NULL;

    if (name)
        t = find_type(name, NULL, tsUNION);

    if (!t)
    {
        t = make_type(TYPE_UNION);
        t->name = name;
        if (name)
            reg_type(t, name, NULL, tsUNION);
    }

    if (!t->defined && defined)
    {
        t->details.structure = xmalloc(sizeof(*t->details.structure));
        t->details.structure->fields = fields;
        t->defined = TRUE;
    }
    else if (defined)
        error_loc("redefinition of union %s\n", name);

    return t;
}

type_t *type_new_encapsulated_union(char *name, var_t *switch_field, var_t *union_field, var_list_t *cases)
{
    type_t *t = NULL;

    if (name)
        t = find_type(name, NULL, tsUNION);

    if (!t)
    {
        t = make_type(TYPE_ENCAPSULATED_UNION);
        t->name = name;
        if (name)
            reg_type(t, name, NULL, tsUNION);
    }
    t->type_type = TYPE_ENCAPSULATED_UNION;

    if (!t->defined)
    {
        if (!union_field)
            union_field = make_var(xstrdup("tagged_union"));
        union_field->declspec.type = type_new_nonencapsulated_union(gen_name(), TRUE, cases);

        t->details.structure = xmalloc(sizeof(*t->details.structure));
        t->details.structure->fields = append_var(NULL, switch_field);
        t->details.structure->fields = append_var(t->details.structure->fields, union_field);
        t->defined = TRUE;
    }
    else
        error_loc("redefinition of union %s\n", name);

    return t;
}

static int is_valid_bitfield_type(const type_t *type)
{
    switch (type_get_type(type))
    {
    case TYPE_ENUM:
        return TRUE;
    case TYPE_BASIC:
        switch (type_basic_get_type(type))
        {
        case TYPE_BASIC_INT8:
        case TYPE_BASIC_INT16:
        case TYPE_BASIC_INT32:
        case TYPE_BASIC_INT64:
        case TYPE_BASIC_INT:
        case TYPE_BASIC_INT3264:
        case TYPE_BASIC_LONG:
        case TYPE_BASIC_CHAR:
        case TYPE_BASIC_HYPER:
        case TYPE_BASIC_BYTE:
        case TYPE_BASIC_WCHAR:
        case TYPE_BASIC_ERROR_STATUS_T:
            return TRUE;
        case TYPE_BASIC_FLOAT:
        case TYPE_BASIC_DOUBLE:
        case TYPE_BASIC_HANDLE:
            return FALSE;
        }
        return FALSE;
    default:
        return FALSE;
    }
}

type_t *type_new_bitfield(type_t *field, const expr_t *bits)
{
    type_t *t;

    if (!is_valid_bitfield_type(field))
        error_loc("bit-field has invalid type\n");

    if (bits->cval < 0)
        error_loc("negative width for bit-field\n");

    /* FIXME: validate bits->cval <= memsize(field) * 8 */

    t = make_type(TYPE_BITFIELD);
    t->details.bitfield.field = field;
    t->details.bitfield.bits = bits;
    return t;
}

static unsigned int compute_method_indexes(type_t *iface)
{
    unsigned int idx;
    statement_t *stmt;

    if (!iface->details.iface)
        return 0;

    if (type_iface_get_inherit(iface))
        idx = compute_method_indexes(type_iface_get_inherit(iface));
    else
        idx = 0;

    STATEMENTS_FOR_EACH_FUNC( stmt, type_iface_get_stmts(iface) )
    {
        var_t *func = stmt->u.var;
        if (!is_callas(func->attrs))
            func->func_idx = idx++;
    }

    return idx;
}

type_t *type_interface_declare(char *name, struct namespace *namespace)
{
    type_t *type = get_type(TYPE_INTERFACE, name, namespace, 0);
    if (type_get_type_detect_alias(type) != TYPE_INTERFACE)
        error_loc("interface %s previously not declared an interface at %s:%d\n",
                  type->name, type->loc_info.input_name, type->loc_info.line_number);
    return type;
}

type_t *type_interface_define(type_t *iface, attr_list_t *attrs, type_t *inherit, statement_list_t *stmts, ifref_list_t *requires)
{
    if (iface->defined)
        error_loc("interface %s already defined at %s:%d\n",
                  iface->name, iface->loc_info.input_name, iface->loc_info.line_number);
    if (iface == inherit)
        error_loc("interface %s can't inherit from itself\n",
                  iface->name);
    iface->attrs = check_interface_attrs(iface->name, attrs);
    iface->details.iface = xmalloc(sizeof(*iface->details.iface));
    iface->details.iface->disp_props = NULL;
    iface->details.iface->disp_methods = NULL;
    iface->details.iface->stmts = stmts;
    iface->details.iface->inherit = inherit;
    iface->details.iface->disp_inherit = NULL;
    iface->details.iface->async_iface = NULL;
    iface->details.iface->requires = requires;
    iface->defined = TRUE;
    compute_method_indexes(iface);
    return iface;
}

type_t *type_dispinterface_declare(char *name)
{
    type_t *type = get_type(TYPE_INTERFACE, name, NULL, 0);
    if (type_get_type_detect_alias(type) != TYPE_INTERFACE)
        error_loc("dispinterface %s previously not declared a dispinterface at %s:%d\n",
                  type->name, type->loc_info.input_name, type->loc_info.line_number);
    return type;
}

type_t *type_dispinterface_define(type_t *iface, attr_list_t *attrs, var_list_t *props, var_list_t *methods)
{
    if (iface->defined)
        error_loc("dispinterface %s already defined at %s:%d\n",
                  iface->name, iface->loc_info.input_name, iface->loc_info.line_number);
    iface->attrs = check_dispiface_attrs(iface->name, attrs);
    iface->details.iface = xmalloc(sizeof(*iface->details.iface));
    iface->details.iface->disp_props = props;
    iface->details.iface->disp_methods = methods;
    iface->details.iface->stmts = NULL;
    iface->details.iface->inherit = find_type("IDispatch", NULL, 0);
    if (!iface->details.iface->inherit) error_loc("IDispatch is undefined\n");
    iface->details.iface->disp_inherit = NULL;
    iface->details.iface->async_iface = NULL;
    iface->details.iface->requires = NULL;
    iface->defined = TRUE;
    compute_method_indexes(iface);
    return iface;
}

type_t *type_dispinterface_define_from_iface(type_t *dispiface, attr_list_t *attrs, type_t *iface)
{
    if (dispiface->defined)
        error_loc("dispinterface %s already defined at %s:%d\n",
                  dispiface->name, dispiface->loc_info.input_name, dispiface->loc_info.line_number);
    dispiface->attrs = check_dispiface_attrs(dispiface->name, attrs);
    dispiface->details.iface = xmalloc(sizeof(*dispiface->details.iface));
    dispiface->details.iface->disp_props = NULL;
    dispiface->details.iface->disp_methods = NULL;
    dispiface->details.iface->stmts = NULL;
    dispiface->details.iface->inherit = find_type("IDispatch", NULL, 0);
    if (!dispiface->details.iface->inherit) error_loc("IDispatch is undefined\n");
    dispiface->details.iface->disp_inherit = iface;
    dispiface->details.iface->async_iface = NULL;
    dispiface->details.iface->requires = NULL;
    dispiface->defined = TRUE;
    compute_method_indexes(dispiface);
    return dispiface;
}

type_t *type_module_declare(char *name)
{
    type_t *type = get_type(TYPE_MODULE, name, NULL, 0);
    if (type_get_type_detect_alias(type) != TYPE_MODULE)
        error_loc("module %s previously not declared a module at %s:%d\n",
                  type->name, type->loc_info.input_name, type->loc_info.line_number);
    return type;
}

type_t *type_module_define(type_t* module, attr_list_t *attrs, statement_list_t *stmts)
{
    if (module->defined)
        error_loc("module %s already defined at %s:%d\n",
                  module->name, module->loc_info.input_name, module->loc_info.line_number);
    module->attrs = check_module_attrs(module->name, attrs);
    module->details.module = xmalloc(sizeof(*module->details.module));
    module->details.module->stmts = stmts;
    module->defined = TRUE;
    return module;
}

type_t *type_coclass_declare(char *name)
{
    type_t *type = get_type(TYPE_COCLASS, name, NULL, 0);
    if (type_get_type_detect_alias(type) != TYPE_COCLASS)
        error_loc("coclass %s previously not declared a coclass at %s:%d\n",
                  type->name, type->loc_info.input_name, type->loc_info.line_number);
    return type;
}

type_t *type_coclass_define(type_t *coclass, attr_list_t *attrs, ifref_list_t *ifaces)
{
    if (coclass->defined)
        error_loc("coclass %s already defined at %s:%d\n",
                  coclass->name, coclass->loc_info.input_name, coclass->loc_info.line_number);
    coclass->attrs = check_coclass_attrs(coclass->name, attrs);
    coclass->details.coclass.ifaces = ifaces;
    coclass->defined = TRUE;
    return coclass;
}

type_t *type_runtimeclass_declare(char *name, struct namespace *namespace)
{
    type_t *type = get_type(TYPE_RUNTIMECLASS, name, namespace, 0);
    if (type_get_type_detect_alias(type) != TYPE_RUNTIMECLASS)
        error_loc("runtimeclass %s previously not declared a runtimeclass at %s:%d\n",
                  type->name, type->loc_info.input_name, type->loc_info.line_number);
    return type;
}

type_t *type_runtimeclass_define(type_t *runtimeclass, attr_list_t *attrs, ifref_list_t *ifaces)
{
    ifref_t *ifref, *required, *tmp;
    ifref_list_t *requires;

    if (runtimeclass->defined)
        error_loc("runtimeclass %s already defined at %s:%d\n",
                  runtimeclass->name, runtimeclass->loc_info.input_name, runtimeclass->loc_info.line_number);
    runtimeclass->attrs = check_runtimeclass_attrs(runtimeclass->name, attrs);
    runtimeclass->details.runtimeclass.ifaces = ifaces;
    runtimeclass->defined = TRUE;
    if (!type_runtimeclass_get_default_iface(runtimeclass))
        error_loc("missing default interface on runtimeclass %s\n", runtimeclass->name);

    LIST_FOR_EACH_ENTRY(ifref, ifaces, ifref_t, entry)
    {
        /* FIXME: this should probably not be allowed, here or in coclass, */
        /* but for now there's too many places in Wine IDL where it is to */
        /* even print a warning. */
        if (!(ifref->iface->defined)) continue;
        if (!(requires = type_iface_get_requires(ifref->iface))) continue;
        LIST_FOR_EACH_ENTRY(required, requires, ifref_t, entry)
        {
            int found = 0;

            LIST_FOR_EACH_ENTRY(tmp, ifaces, ifref_t, entry)
                if ((found = type_is_equal(tmp->iface, required->iface))) break;

            if (!found)
                error_loc("interface '%s' also requires interface '%s', "
                          "but runtimeclass '%s' does not implement it.\n",
                          ifref->iface->name, required->iface->name, runtimeclass->name);
        }
    }

    return runtimeclass;
}

type_t *type_apicontract_declare(char *name, struct namespace *namespace)
{
    type_t *type = get_type(TYPE_APICONTRACT, name, namespace, 0);
    if (type_get_type_detect_alias(type) != TYPE_APICONTRACT)
        error_loc("apicontract %s previously not declared a apicontract at %s:%d\n",
                  type->name, type->loc_info.input_name, type->loc_info.line_number);
    return type;
}

type_t *type_apicontract_define(type_t *apicontract, attr_list_t *attrs)
{
    if (apicontract->defined)
        error_loc("apicontract %s already defined at %s:%d\n",
                  apicontract->name, apicontract->loc_info.input_name, apicontract->loc_info.line_number);
    apicontract->attrs = check_apicontract_attrs(apicontract->name, attrs);
    apicontract->defined = TRUE;
    return apicontract;
}

type_t *type_parameterized_interface_declare(char *name, struct namespace *namespace, type_list_t *params)
{
    type_t *type = get_type(TYPE_PARAMETERIZED_TYPE, name, namespace, 0);
    if (type_get_type_detect_alias(type) != TYPE_PARAMETERIZED_TYPE)
        error_loc("pinterface %s previously not declared a pinterface at %s:%d\n",
                  type->name, type->loc_info.input_name, type->loc_info.line_number);
    type->details.parameterized.type = make_type(TYPE_INTERFACE);
    type->details.parameterized.params = params;
    return type;
}

type_t *type_parameterized_interface_define(type_t *type, attr_list_t *attrs, type_t *inherit, statement_list_t *stmts, ifref_list_t *requires)
{
    type_t *iface;
    if (type->defined)
        error_loc("pinterface %s already defined at %s:%d\n",
                  type->name, type->loc_info.input_name, type->loc_info.line_number);

    /* The parameterized type UUID is actually a PIID that is then used as a seed to generate
     * a new type GUID with the rules described in:
     *   https://docs.microsoft.com/en-us/uwp/winrt-cref/winrt-type-system#parameterized-types
     * TODO: store type signatures for generated interfaces, and generate their GUIDs
     */
    type->attrs = check_interface_attrs(type->name, attrs);

    iface = type->details.parameterized.type;
    iface->details.iface = xmalloc(sizeof(*iface->details.iface));
    iface->details.iface->disp_props = NULL;
    iface->details.iface->disp_methods = NULL;
    iface->details.iface->stmts = stmts;
    iface->details.iface->inherit = inherit;
    iface->details.iface->disp_inherit = NULL;
    iface->details.iface->async_iface = NULL;
    iface->details.iface->requires = requires;

    type->defined = TRUE;
    return type;
}

int type_is_equal(const type_t *type1, const type_t *type2)
{
    if (type1 == type2)
        return TRUE;
    if (type_get_type_detect_alias(type1) != type_get_type_detect_alias(type2))
        return FALSE;
    if (type1->namespace != type2->namespace)
        return FALSE;

    if (type1->name && type2->name)
        return !strcmp(type1->name, type2->name);
    else if ((!type1->name && type2->name) || (type1->name && !type2->name))
        return FALSE;

    /* FIXME: do deep inspection of types to determine if they are equal */

    return FALSE;
}
