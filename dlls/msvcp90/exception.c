/*
 * Copyright 2010 Piotr Caban for CodeWeavers
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

#include <stdarg.h>

#include "msvcp90.h"
#include "windef.h"
#include "winbase.h"
#include "winternl.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(msvcp);

#define CLASS_IS_SIMPLE_TYPE          1
#define CLASS_HAS_VIRTUAL_BASE_CLASS  4

void WINAPI _CxxThrowException(exception*,const cxx_exception_type*);
int* __cdecl __processing_throw(void);

#if _MSVCP_VER >= 70 || defined(_MSVCIRT)
typedef const char **exception_name;
#define EXCEPTION_STR(name) (*name)
#define EXCEPTION_NAME(str) ((exception_name)&str)
#else
typedef const char *exception_name;
#define EXCEPTION_STR(name) (name)
#define EXCEPTION_NAME(str) (str)
#endif

void (CDECL *_Raise_handler)(const exception*) = NULL;

/* vtables */
extern const vtable_ptr MSVCP_exception_vtable;
/* ??_7bad_alloc@std@@6B@ */
extern const vtable_ptr MSVCP_bad_alloc_vtable;
/* ??_7logic_error@std@@6B@ */
extern const vtable_ptr MSVCP_logic_error_vtable;
/* ??_7length_error@std@@6B@ */
extern const vtable_ptr MSVCP_length_error_vtable;
/* ??_7out_of_range@std@@6B@ */
extern const vtable_ptr MSVCP_out_of_range_vtable;
extern const vtable_ptr MSVCP_invalid_argument_vtable;
/* ??_7runtime_error@std@@6B@ */
extern const vtable_ptr MSVCP_runtime_error_vtable;
extern const vtable_ptr MSVCP__System_error_vtable;
extern const vtable_ptr MSVCP_system_error_vtable;
extern const vtable_ptr MSVCP_failure_vtable;
/* ??_7bad_cast@std@@6B@ */
extern const vtable_ptr MSVCP_bad_cast_vtable;
/* ??_7range_error@std@@6B@ */
extern const vtable_ptr MSVCP_range_error_vtable;

static void MSVCP_type_info_dtor(type_info * _this)
{
    free(_this->name);
}

/* Unexported */
DEFINE_THISCALL_WRAPPER(MSVCP_type_info_vector_dtor,8)
void * __thiscall MSVCP_type_info_vector_dtor(type_info * _this, unsigned int flags)
{
    TRACE("(%p %x)\n", _this, flags);
    if (flags & 2)
    {
        /* we have an array, with the number of elements stored before the first object */
        INT_PTR i, *ptr = (INT_PTR *)_this - 1;

        for (i = *ptr - 1; i >= 0; i--) MSVCP_type_info_dtor(_this + i);
        MSVCRT_operator_delete(ptr);
    }
    else
    {
        MSVCP_type_info_dtor(_this);
        if (flags & 1) MSVCRT_operator_delete(_this);
    }
    return _this;
}

DEFINE_RTTI_DATA0( type_info, 0, ".?AVtype_info@@" )

/* ??0exception@@QAE@ABQBD@Z */
/* ??0exception@@QEAA@AEBQEBD@Z */
DEFINE_THISCALL_WRAPPER(MSVCP_exception_ctor,8)
exception* __thiscall MSVCP_exception_ctor(exception *this, exception_name name)
{
    TRACE("(%p %s)\n", this, EXCEPTION_STR(name));

    this->vtable = &MSVCP_exception_vtable;
    if(EXCEPTION_STR(name)) {
        unsigned int name_len = strlen(EXCEPTION_STR(name)) + 1;
        this->name = malloc(name_len);
        memcpy(this->name, EXCEPTION_STR(name), name_len);
        this->do_free = TRUE;
    } else {
        this->name = NULL;
        this->do_free = FALSE;
    }
    return this;
}

DEFINE_THISCALL_WRAPPER(MSVCP_exception_copy_ctor,8)
exception* __thiscall MSVCP_exception_copy_ctor(exception *this, const exception *rhs)
{
    TRACE("(%p,%p)\n", this, rhs);

    if(!rhs->do_free) {
        this->vtable = &MSVCP_exception_vtable;
        this->name = rhs->name;
        this->do_free = FALSE;
    } else
        MSVCP_exception_ctor(this, EXCEPTION_NAME(rhs->name));
    TRACE("name = %s\n", this->name);
    return this;
}

/* ??0exception@@QAE@XZ */
/* ??0exception@@QEAA@XZ */
DEFINE_THISCALL_WRAPPER(MSVCP_exception_default_ctor,4)
exception* __thiscall MSVCP_exception_default_ctor(exception *this)
{
    TRACE("(%p)\n", this);
    this->vtable = &MSVCP_exception_vtable;
    this->name = NULL;
    this->do_free = FALSE;
    return this;
}

DEFINE_THISCALL_WRAPPER(MSVCP_exception_dtor,4)
void __thiscall MSVCP_exception_dtor(exception *this)
{
    TRACE("(%p)\n", this);
    this->vtable = &MSVCP_exception_vtable;
    if(this->do_free)
        free(this->name);
}

DEFINE_THISCALL_WRAPPER(MSVCP_exception_vector_dtor, 8)
void * __thiscall MSVCP_exception_vector_dtor(exception *this, unsigned int flags)
{
    TRACE("%p %x\n", this, flags);
    if(flags & 2) {
        /* we have an array, with the number of elements stored before the first object */
        INT_PTR i, *ptr = (INT_PTR *)this-1;

        for(i=*ptr-1; i>=0; i--)
            MSVCP_exception_dtor(this+i);
        MSVCRT_operator_delete(ptr);
    } else {
        MSVCP_exception_dtor(this);
        if(flags & 1)
            MSVCRT_operator_delete(this);
    }

    return this;
}

/* ??_Gexception@@UAEPAXI@Z */
DEFINE_THISCALL_WRAPPER(MSVCP_exception_scalar_dtor, 8)
void * __thiscall MSVCP_exception_scalar_dtor(exception *this, unsigned int flags)
{
    TRACE("(%p %x)\n", this, flags);
    MSVCP_exception_dtor(this);
    if (flags & 1) MSVCRT_operator_delete(this);
    return this;
}

/* ??4exception@@QAEAAV0@ABV0@@Z */
/* ??4exception@@QEAAAEAV0@AEBV0@@Z */
DEFINE_THISCALL_WRAPPER(MSVCP_exception_assign, 8)
exception* __thiscall MSVCP_exception_assign(exception *this, const exception *assign)
{
    MSVCP_exception_dtor(this);
    return MSVCP_exception_copy_ctor(this, assign);
}

/* ?_Doraise@bad_alloc@std@@MBEXXZ */
/* ?_Doraise@bad_alloc@std@@MEBAXXZ */
/* ?_Doraise@logic_error@std@@MBEXXZ */
/* ?_Doraise@logic_error@std@@MEBAXXZ */
/* ?_Doraise@length_error@std@@MBEXXZ */
/* ?_Doraise@length_error@std@@MEBAXXZ */
/* ?_Doraise@out_of_range@std@@MBEXXZ */
/* ?_Doraise@out_of_range@std@@MEBAXXZ */
/* ?_Doraise@runtime_error@std@@MBEXXZ */
/* ?_Doraise@runtime_error@std@@MEBAXXZ */
/* ?_Doraise@bad_cast@std@@MBEXXZ */
/* ?_Doraise@bad_cast@std@@MEBAXXZ */
/* ?_Doraise@range_error@std@@MBEXXZ */
/* ?_Doraise@range_error@std@@MEBAXXZ */
DEFINE_THISCALL_WRAPPER(MSVCP_exception__Doraise, 4)
void __thiscall MSVCP_exception__Doraise(exception *this)
{
    FIXME("(%p) stub\n", this);
}

DEFINE_THISCALL_WRAPPER(MSVCP_exception_what,4)
const char* __thiscall MSVCP_exception_what(exception * this)
{
    TRACE("(%p) returning %s\n", this, this->name);
    return this->name ? this->name : "Unknown exception";
}

#if _MSVCP_VER >= 80
DEFINE_RTTI_DATA0(exception, 0, ".?AVexception@std@@")
#else
DEFINE_RTTI_DATA0(exception, 0, ".?AVexception@@")
#endif
DEFINE_CXX_DATA0(exception, MSVCP_exception_dtor)

/* bad_alloc class data */
typedef exception bad_alloc;

/* ??0bad_alloc@std@@QAE@PBD@Z */
/* ??0bad_alloc@std@@QEAA@PEBD@Z */
DEFINE_THISCALL_WRAPPER(MSVCP_bad_alloc_ctor, 8)
bad_alloc* __thiscall MSVCP_bad_alloc_ctor(bad_alloc *this, exception_name name)
{
    TRACE("%p %s\n", this, EXCEPTION_STR(name));
    MSVCP_exception_ctor(this, name);
    this->vtable = &MSVCP_bad_alloc_vtable;
    return this;
}

/* ??0bad_alloc@std@@QAE@XZ */
/* ??0bad_alloc@std@@QEAA@XZ */
DEFINE_THISCALL_WRAPPER(MSVCP_bad_alloc_default_ctor, 4)
bad_alloc* __thiscall MSVCP_bad_alloc_default_ctor(bad_alloc *this)
{
    static const char name[] = "bad allocation";
    return MSVCP_bad_alloc_ctor(this, EXCEPTION_NAME(name));
}

/* ??0bad_alloc@std@@QAE@ABV01@@Z */
/* ??0bad_alloc@std@@QEAA@AEBV01@@Z */
DEFINE_THISCALL_WRAPPER(MSVCP_bad_alloc_copy_ctor, 8)
bad_alloc* __thiscall MSVCP_bad_alloc_copy_ctor(bad_alloc *this, const bad_alloc *rhs)
{
    TRACE("%p %p\n", this, rhs);
    MSVCP_exception_copy_ctor(this, rhs);
    this->vtable = &MSVCP_bad_alloc_vtable;
    return this;
}

/* ??1bad_alloc@std@@UAE@XZ */
/* ??1bad_alloc@std@@UEAA@XZ */
DEFINE_THISCALL_WRAPPER(MSVCP_bad_alloc_dtor, 4)
void __thiscall MSVCP_bad_alloc_dtor(bad_alloc *this)
{
    TRACE("%p\n", this);
    MSVCP_exception_dtor(this);
}

DEFINE_THISCALL_WRAPPER(MSVCP_bad_alloc_vector_dtor, 8)
void * __thiscall MSVCP_bad_alloc_vector_dtor(bad_alloc *this, unsigned int flags)
{
    TRACE("%p %x\n", this, flags);
    if(flags & 2) {
        /* we have an array, with the number of elements stored before the first object */
        INT_PTR i, *ptr = (INT_PTR *)this-1;

        for(i=*ptr-1; i>=0; i--)
            MSVCP_bad_alloc_dtor(this+i);
        MSVCRT_operator_delete(ptr);
    } else {
        MSVCP_bad_alloc_dtor(this);
        if(flags & 1)
            MSVCRT_operator_delete(this);
    }

    return this;
}

/* ??4bad_alloc@std@@QAEAAV01@ABV01@@Z */
/* ??4bad_alloc@std@@QEAAAEAV01@AEBV01@@Z */
DEFINE_THISCALL_WRAPPER(MSVCP_bad_alloc_assign, 8)
bad_alloc* __thiscall MSVCP_bad_alloc_assign(bad_alloc *this, const bad_alloc *assign)
{
    MSVCP_bad_alloc_dtor(this);
    return MSVCP_bad_alloc_copy_ctor(this, assign);
}

DEFINE_RTTI_DATA1(bad_alloc, 0, &exception_rtti_base_descriptor, ".?AVbad_alloc@std@@")
DEFINE_CXX_DATA1(bad_alloc, &exception_cxx_type_info, MSVCP_bad_alloc_dtor)

/* logic_error class data */
typedef struct {
    exception e;
#if _MSVCP_VER <= 90 && !defined _MSVCIRT
    basic_string_char str;
#endif
} logic_error;

/* ??0logic_error@@QAE@ABQBD@Z */
/* ??0logic_error@@QEAA@AEBQEBD@Z */
DEFINE_THISCALL_WRAPPER(MSVCP_logic_error_ctor, 8)
logic_error* __thiscall MSVCP_logic_error_ctor( logic_error *this, exception_name name )
{
    TRACE("%p %s\n", this, EXCEPTION_STR(name));
#if _MSVCP_VER <= 90 && !defined _MSVCIRT
#if _MSVCP_VER == 60
    MSVCP_exception_ctor(&this->e, "");
#else
    MSVCP_exception_default_ctor(&this->e);
#endif
    MSVCP_basic_string_char_ctor_cstr(&this->str, EXCEPTION_STR(name));
#else
    MSVCP_exception_ctor(&this->e, name);
#endif
    this->e.vtable = &MSVCP_logic_error_vtable;
    return this;
}

/* ??0logic_error@std@@QAE@ABV01@@Z */
/* ??0logic_error@std@@QEAA@AEBV01@@Z */
DEFINE_THISCALL_WRAPPER(MSVCP_logic_error_copy_ctor, 8)
logic_error* __thiscall MSVCP_logic_error_copy_ctor(
        logic_error *this, const logic_error *rhs)
{
    TRACE("%p %p\n", this, rhs);
    MSVCP_exception_copy_ctor(&this->e, &rhs->e);
#if _MSVCP_VER <= 90 && !defined _MSVCIRT
    MSVCP_basic_string_char_copy_ctor(&this->str, &rhs->str);
#endif
    this->e.vtable = &MSVCP_logic_error_vtable;
    return this;
}

/* ??0logic_error@std@@QAE@ABV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@1@@Z */
/* ??0logic_error@std@@QEAA@AEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@1@@Z */
#ifndef _MSVCIRT
DEFINE_THISCALL_WRAPPER(MSVCP_logic_error_ctor_bstr, 8)
logic_error* __thiscall MSVCP_logic_error_ctor_bstr(logic_error *this, const basic_string_char *str)
{
    const char *name = MSVCP_basic_string_char_c_str(str);
    TRACE("(%p %p %s)\n", this, str, name);
    return MSVCP_logic_error_ctor(this, EXCEPTION_NAME(name));
}
#endif

/* ??1logic_error@std@@UAE@XZ */
/* ??1logic_error@std@@UEAA@XZ */
/* ??1length_error@std@@UAE@XZ */
/* ??1length_error@std@@UEAA@XZ */
/* ??1out_of_range@std@@UAE@XZ */
/* ??1out_of_range@std@@UEAA@XZ */
DEFINE_THISCALL_WRAPPER(MSVCP_logic_error_dtor, 4)
void __thiscall MSVCP_logic_error_dtor(logic_error *this)
{
    TRACE("%p\n", this);
    MSVCP_exception_dtor(&this->e);
#if _MSVCP_VER <= 90 && !defined _MSVCIRT
    MSVCP_basic_string_char_dtor(&this->str);
#endif
}

DEFINE_THISCALL_WRAPPER(MSVCP_logic_error_vector_dtor, 8)
void* __thiscall MSVCP_logic_error_vector_dtor(
        logic_error *this, unsigned int flags)
{
    TRACE("%p %x\n", this, flags);
    if(flags & 2) {
        /* we have an array, with the number of elements stored before the first object */
        INT_PTR i, *ptr = (INT_PTR *)this-1;

        for(i=*ptr-1; i>=0; i--)
            MSVCP_logic_error_dtor(this+i);
        MSVCRT_operator_delete(ptr);
    } else {
        MSVCP_logic_error_dtor(this);
        if(flags & 1)
            MSVCRT_operator_delete(this);
    }

    return this;
}

/* ??_Glogic_error@@UAEPAXI@Z */
DEFINE_THISCALL_WRAPPER(MSVCP_logic_error_scalar_dtor, 8)
void * __thiscall MSVCP_logic_error_scalar_dtor(logic_error *this, unsigned int flags)
{
    TRACE("(%p %x)\n", this, flags);
    MSVCP_logic_error_dtor(this);
    if (flags & 1) MSVCRT_operator_delete(this);
    return this;
}

/* ??4logic_error@std@@QAEAAV01@ABV01@@Z */
/* ??4logic_error@std@@QEAAAEAV01@AEBV01@@Z */
DEFINE_THISCALL_WRAPPER(MSVCP_logic_error_assign, 8)
logic_error* __thiscall MSVCP_logic_error_assign(logic_error *this, const logic_error *assign)
{
    MSVCP_logic_error_dtor(this);
    return MSVCP_logic_error_copy_ctor(this, assign);
}

/* ?what@logic_error@std@@UBEPBDXZ */
/* ?what@logic_error@std@@UEBAPEBDXZ */
DEFINE_THISCALL_WRAPPER(MSVCP_logic_error_what, 4)
const char* __thiscall MSVCP_logic_error_what(logic_error *this)
{
    TRACE("%p\n", this);
#if _MSVCP_VER > 90 || defined _MSVCIRT
    return MSVCP_exception_what( &this->e );
#else
    return MSVCP_basic_string_char_c_str(&this->str);
#endif
}

#if _MSVCP_VER >= 80
DEFINE_RTTI_DATA1(logic_error, 0, &exception_rtti_base_descriptor, ".?AVlogic_error@std@@")
#else
DEFINE_RTTI_DATA1(logic_error, 0, &exception_rtti_base_descriptor, ".?AVlogic_error@@")
#endif
DEFINE_CXX_DATA1(logic_error, &exception_cxx_type_info, MSVCP_logic_error_dtor)

/* length_error class data */
typedef logic_error length_error;

static length_error* MSVCP_length_error_ctor( length_error *this, exception_name name )
{
    TRACE("%p %s\n", this, EXCEPTION_STR(name));
    MSVCP_logic_error_ctor(this, name);
    this->e.vtable = &MSVCP_length_error_vtable;
    return this;
}

/* ??0length_error@std@@QAE@ABV01@@Z */
/* ??0length_error@std@@QEAA@AEBV01@@Z */
DEFINE_THISCALL_WRAPPER(MSVCP_length_error_copy_ctor, 8)
length_error* __thiscall MSVCP_length_error_copy_ctor(
        length_error *this, const length_error *rhs)
{
    TRACE("%p %p\n", this, rhs);
    MSVCP_logic_error_copy_ctor(this, rhs);
    this->e.vtable = &MSVCP_length_error_vtable;
    return this;
}

/* ??0length_error@std@@QAE@ABV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@1@@Z */
/* ??0length_error@std@@QEAA@AEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@1@@Z */
#ifndef _MSVCIRT
DEFINE_THISCALL_WRAPPER(MSVCP_length_error_ctor_bstr, 8)
length_error* __thiscall MSVCP_length_error_ctor_bstr(length_error *this, const basic_string_char *str)
{
    const char *name = MSVCP_basic_string_char_c_str(str);
    TRACE("(%p %p %s)\n", this, str, name);
    return MSVCP_length_error_ctor(this, EXCEPTION_NAME(name));
}
#endif

/* ??4length_error@std@@QAEAAV01@ABV01@@Z */
/* ??4length_error@std@@QEAAAEAV01@AEBV01@@Z */
DEFINE_THISCALL_WRAPPER(MSVCP_length_error_assign, 8)
length_error* __thiscall MSVCP_length_error_assign(length_error *this, const length_error *assign)
{
    MSVCP_logic_error_dtor(this);
    return MSVCP_length_error_copy_ctor(this, assign);
}

DEFINE_RTTI_DATA2(length_error, 0, &logic_error_rtti_base_descriptor, &exception_rtti_base_descriptor, ".?AVlength_error@std@@")
DEFINE_CXX_DATA2(length_error, &logic_error_cxx_type_info, &exception_cxx_type_info, MSVCP_logic_error_dtor)

/* out_of_range class data */
typedef logic_error out_of_range;

static out_of_range* MSVCP_out_of_range_ctor( out_of_range *this, exception_name name )
{
    TRACE("%p %s\n", this, EXCEPTION_STR(name));
    MSVCP_logic_error_ctor(this, name);
    this->e.vtable = &MSVCP_out_of_range_vtable;
    return this;
}

/* ??0out_of_range@std@@QAE@ABV01@@Z */
/* ??0out_of_range@std@@QEAA@AEBV01@@Z */
DEFINE_THISCALL_WRAPPER(MSVCP_out_of_range_copy_ctor, 8)
out_of_range* __thiscall MSVCP_out_of_range_copy_ctor(
        out_of_range *this, const out_of_range *rhs)
{
    TRACE("%p %p\n", this, rhs);
    MSVCP_logic_error_copy_ctor(this, rhs);
    this->e.vtable = &MSVCP_out_of_range_vtable;
    return this;
}

/* ??0out_of_range@std@@QAE@ABV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@1@@Z */
/* ??0out_of_range@std@@QEAA@AEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@1@@Z */
#ifndef _MSVCIRT
DEFINE_THISCALL_WRAPPER(MSVCP_out_of_range_ctor_bstr, 8)
out_of_range* __thiscall MSVCP_out_of_range_ctor_bstr(out_of_range *this, const basic_string_char *str)
{
    const char *name = MSVCP_basic_string_char_c_str(str);
    TRACE("(%p %p %s)\n", this, str, name);
    return MSVCP_out_of_range_ctor(this, EXCEPTION_NAME(name));
}
#endif

/* ??4out_of_range@std@@QAEAAV01@ABV01@@Z */
/* ??4out_of_range@std@@QEAAAEAV01@AEBV01@@Z */
DEFINE_THISCALL_WRAPPER(MSVCP_out_of_range_assign, 8)
out_of_range* __thiscall MSVCP_out_of_range_assign(out_of_range *this, const out_of_range *assign)
{
    MSVCP_logic_error_dtor(this);
    return MSVCP_out_of_range_copy_ctor(this, assign);
}

DEFINE_RTTI_DATA2(out_of_range, 0, &logic_error_rtti_base_descriptor, &exception_rtti_base_descriptor, ".?AVout_of_range@std@@")
DEFINE_CXX_DATA2(out_of_range, &logic_error_cxx_type_info, &exception_cxx_type_info, MSVCP_logic_error_dtor)

/* invalid_argument class data */
typedef logic_error invalid_argument;

static invalid_argument* MSVCP_invalid_argument_ctor( invalid_argument *this, exception_name name )
{
    TRACE("%p %s\n", this, EXCEPTION_STR(name));
    MSVCP_logic_error_ctor(this, name);
    this->e.vtable = &MSVCP_invalid_argument_vtable;
    return this;
}

DEFINE_THISCALL_WRAPPER(MSVCP_invalid_argument_copy_ctor, 8)
invalid_argument* __thiscall MSVCP_invalid_argument_copy_ctor(
        invalid_argument *this, invalid_argument *rhs)
{
    TRACE("%p %p\n", this, rhs);
    MSVCP_logic_error_copy_ctor(this, rhs);
    this->e.vtable = &MSVCP_invalid_argument_vtable;
    return this;
}

DEFINE_RTTI_DATA2(invalid_argument, 0, &logic_error_rtti_base_descriptor, &exception_rtti_base_descriptor, ".?AVinvalid_argument@std@@")
DEFINE_CXX_DATA2(invalid_argument, &logic_error_cxx_type_info,  &exception_cxx_type_info, MSVCP_logic_error_dtor)

/* runtime_error class data */
typedef struct {
    exception e;
#if _MSVCP_VER <= 90 && !defined _MSVCIRT
    basic_string_char str;
#endif
} runtime_error;

static runtime_error* MSVCP_runtime_error_ctor( runtime_error *this, exception_name name )
{
    TRACE("%p %s\n", this, EXCEPTION_STR(name));
#if _MSVCP_VER <= 90 && !defined _MSVCIRT
#if _MSVCP_VER == 60
    MSVCP_exception_ctor(&this->e, "");
#else
    MSVCP_exception_default_ctor(&this->e);
#endif
    MSVCP_basic_string_char_ctor_cstr(&this->str, EXCEPTION_STR(name));
#else
    MSVCP_exception_ctor(&this->e, name);
#endif
    this->e.vtable = &MSVCP_runtime_error_vtable;
    return this;
}

/* ??0runtime_error@std@@QAE@ABV01@@Z */
/* ??0runtime_error@std@@QEAA@AEBV01@@Z */
DEFINE_THISCALL_WRAPPER(MSVCP_runtime_error_copy_ctor, 8)
runtime_error* __thiscall MSVCP_runtime_error_copy_ctor(
        runtime_error *this, const runtime_error *rhs)
{
    TRACE("%p %p\n", this, rhs);
    MSVCP_exception_copy_ctor(&this->e, &rhs->e);
#if _MSVCP_VER <= 90 && !defined _MSVCIRT
    MSVCP_basic_string_char_copy_ctor(&this->str, &rhs->str);
#endif
    this->e.vtable = &MSVCP_runtime_error_vtable;
    return this;
}

/* ??0runtime_error@std@@QAE@ABV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@1@@Z */
/* ??0runtime_error@std@@QEAA@AEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@1@@Z */
#ifndef _MSVCIRT
DEFINE_THISCALL_WRAPPER(MSVCP_runtime_error_ctor_bstr, 8)
runtime_error* __thiscall MSVCP_runtime_error_ctor_bstr(runtime_error *this, const basic_string_char *str)
{
    const char *name = MSVCP_basic_string_char_c_str(str);
    TRACE("(%p %p %s)\n", this, str, name);
    return MSVCP_runtime_error_ctor(this, EXCEPTION_NAME(name));
}
#endif

/* ??1runtime_error@std@@UAE@XZ */
/* ??1runtime_error@std@@UEAA@XZ */
/* ??1range_error@std@@UAE@XZ */
/* ??1range_error@std@@UEAA@XZ */
DEFINE_THISCALL_WRAPPER(MSVCP_runtime_error_dtor, 4)
void __thiscall MSVCP_runtime_error_dtor(runtime_error *this)
{
    TRACE("%p\n", this);
    MSVCP_exception_dtor(&this->e);
#if _MSVCP_VER <= 90 && !defined _MSVCIRT
    MSVCP_basic_string_char_dtor(&this->str);
#endif
}

DEFINE_THISCALL_WRAPPER(MSVCP_runtime_error_vector_dtor, 8)
void* __thiscall MSVCP_runtime_error_vector_dtor(
        runtime_error *this, unsigned int flags)
{
    TRACE("%p %x\n", this, flags);
    if(flags & 2) {
        /* we have an array, with the number of elements stored before the first object */
        INT_PTR i, *ptr = (INT_PTR *)this-1;

        for(i=*ptr-1; i>=0; i--)
            MSVCP_runtime_error_dtor(this+i);
        MSVCRT_operator_delete(ptr);
    } else {
        MSVCP_runtime_error_dtor(this);
        if(flags & 1)
            MSVCRT_operator_delete(this);
    }

    return this;
}

/* ??4runtime_error@std@@QAEAAV01@ABV01@@Z */
/* ??4runtime_error@std@@QEAAAEAV01@AEBV01@@Z */
DEFINE_THISCALL_WRAPPER(MSVCP_runtime_error_assign, 8)
runtime_error* __thiscall MSVCP_runtime_error_assign(runtime_error *this, const runtime_error *assign)
{
    MSVCP_runtime_error_dtor(this);
    return MSVCP_runtime_error_copy_ctor(this, assign);
}

/* ?what@runtime_error@std@@UBEPBDXZ */
/* ?what@runtime_error@std@@UEBAPEBDXZ */
DEFINE_THISCALL_WRAPPER(MSVCP_runtime_error_what, 4)
const char* __thiscall MSVCP_runtime_error_what(runtime_error *this)
{
    TRACE("%p\n", this);
#if _MSVCP_VER > 90 || defined _MSVCIRT
    return MSVCP_exception_what( &this->e );
#else
    return MSVCP_basic_string_char_c_str(&this->str);
#endif
}

DEFINE_RTTI_DATA1(runtime_error, 0, &exception_rtti_base_descriptor, ".?AVruntime_error@std@@")
DEFINE_CXX_DATA1(runtime_error, &exception_cxx_type_info, MSVCP_runtime_error_dtor)

/* failure class data */
typedef struct {
    runtime_error base;
#if _MSVCP_VER > 90
    int err;
#endif
} system_error;
typedef system_error _System_error;
typedef system_error failure;

static failure* MSVCP_failure_ctor( failure *this, exception_name name )
{
    TRACE("%p %s\n", this, EXCEPTION_STR(name));
    MSVCP_runtime_error_ctor(&this->base, name);
#if _MSVCP_VER > 90
    /* FIXME: set err correctly */
    this->err = 0;
#endif
    this->base.e.vtable = &MSVCP_failure_vtable;
    return this;
}

DEFINE_THISCALL_WRAPPER(MSVCP_failure_copy_ctor, 8)
failure* __thiscall MSVCP_failure_copy_ctor(
        failure *this, failure *rhs)
{
    TRACE("%p %p\n", this, rhs);
    MSVCP_runtime_error_copy_ctor(&this->base, &rhs->base);
#if _MSVCP_VER > 90
    this->err = rhs->err;
#endif
    this->base.e.vtable = &MSVCP_failure_vtable;
    return this;
}

DEFINE_THISCALL_WRAPPER(MSVCP_failure_dtor, 4)
void __thiscall MSVCP_failure_dtor(failure *this)
{
    TRACE("%p\n", this);
    MSVCP_runtime_error_dtor(&this->base);
}

DEFINE_THISCALL_WRAPPER(MSVCP_failure_vector_dtor, 8)
void* __thiscall MSVCP_failure_vector_dtor(
        failure *this, unsigned int flags)
{
    TRACE("%p %x\n", this, flags);
    return MSVCP_runtime_error_vector_dtor(&this->base, flags);
}

DEFINE_THISCALL_WRAPPER(MSVCP_failure_what, 4)
const char* __thiscall MSVCP_failure_what(failure *this)
{
    TRACE("%p\n", this);
    return MSVCP_runtime_error_what(&this->base);
}

#if _MSVCP_VER > 90
DEFINE_THISCALL_WRAPPER(MSVCP_system_error_copy_ctor, 8)
system_error* __thiscall MSVCP_system_error_copy_ctor(
        system_error *this, system_error *rhs)
{
    MSVCP_failure_copy_ctor(this, rhs);
    this->base.e.vtable = &MSVCP_system_error_vtable;
    return this;
}
#endif

#if _MSVCP_VER > 110
DEFINE_THISCALL_WRAPPER(MSVCP__System_error_copy_ctor, 8)
_System_error* __thiscall MSVCP__System_error_copy_ctor(
        _System_error *this, _System_error *rhs)
{
    MSVCP_failure_copy_ctor(this, rhs);
    this->base.e.vtable = &MSVCP__System_error_vtable;
    return this;
}
#endif

#if _MSVCP_VER > 110
DEFINE_RTTI_DATA2(_System_error, 0, &runtime_error_rtti_base_descriptor,
        &exception_rtti_base_descriptor, ".?AV_System_error@std@@")
DEFINE_RTTI_DATA3(system_error, 0, &_System_error_rtti_base_descriptor,
        &runtime_error_rtti_base_descriptor, &exception_rtti_base_descriptor,
        ".?AVsystem_error@std@@")
DEFINE_RTTI_DATA4(failure, 0, &system_error_rtti_base_descriptor,
        &_System_error_rtti_base_descriptor, &runtime_error_rtti_base_descriptor,
        &exception_rtti_base_descriptor, ".?AVfailure@ios_base@std@@")
DEFINE_CXX_TYPE_INFO(_System_error)
DEFINE_CXX_TYPE_INFO(system_error);
DEFINE_CXX_DATA4(failure, &system_error_cxx_type_info,
        &_System_error_cxx_type_info, &runtime_error_cxx_type_info,
        &exception_cxx_type_info, MSVCP_runtime_error_dtor)
#elif _MSVCP_VER > 90
DEFINE_RTTI_DATA2(system_error, 0, &runtime_error_rtti_base_descriptor,
        &exception_rtti_base_descriptor, ".?AVsystem_error@std@@")
DEFINE_RTTI_DATA3(failure, 0, &system_error_rtti_base_descriptor,
        &runtime_error_rtti_base_descriptor, &exception_rtti_base_descriptor,
        ".?AVfailure@ios_base@std@@")
DEFINE_CXX_TYPE_INFO(system_error);
DEFINE_CXX_DATA3(failure, &system_error_cxx_type_info, &runtime_error_cxx_type_info,
        &exception_cxx_type_info, MSVCP_runtime_error_dtor)
#else
DEFINE_RTTI_DATA2(failure, 0, &runtime_error_rtti_base_descriptor,
        &exception_rtti_base_descriptor, ".?AVfailure@ios_base@std@@")
DEFINE_CXX_DATA2(failure, &runtime_error_cxx_type_info,
        &exception_cxx_type_info, MSVCP_runtime_error_dtor)
#endif

/* bad_cast class data */
typedef exception bad_cast;

/* ??0bad_cast@std@@QAE@PBD@Z */
/* ??0bad_cast@std@@QEAA@PEBD@Z */
DEFINE_THISCALL_WRAPPER(MSVCP_bad_cast_ctor, 8)
bad_cast* __thiscall MSVCP_bad_cast_ctor(bad_cast *this, const char *name)
{
    TRACE("%p %s\n", this, name);
    MSVCP_exception_ctor(this, EXCEPTION_NAME(name));
    this->vtable = &MSVCP_bad_cast_vtable;
    return this;
}

/* ??_Fbad_cast@@QAEXXZ */
/* ??_Fbad_cast@std@@QEAAXXZ */
DEFINE_THISCALL_WRAPPER(MSVCP_bad_cast_default_ctor,4)
bad_cast* __thiscall MSVCP_bad_cast_default_ctor(bad_cast *this)
{
    return MSVCP_bad_cast_ctor(this, "bad cast");
}

/* ??0bad_cast@std@@QAE@ABV01@@Z */
/* ??0bad_cast@std@@QEAA@AEBV01@@Z */
DEFINE_THISCALL_WRAPPER(MSVCP_bad_cast_copy_ctor, 8)
bad_cast* __thiscall MSVCP_bad_cast_copy_ctor(bad_cast *this, const bad_cast *rhs)
{
    TRACE("%p %p\n", this, rhs);
    MSVCP_exception_copy_ctor(this, rhs);
    this->vtable = &MSVCP_bad_cast_vtable;
    return this;
}

/* ??1bad_cast@@UAE@XZ */
/* ??1bad_cast@std@@UEAA@XZ */
DEFINE_THISCALL_WRAPPER(MSVCP_bad_cast_dtor, 4)
void __thiscall MSVCP_bad_cast_dtor(bad_cast *this)
{
    TRACE("%p\n", this);
    MSVCP_exception_dtor(this);
}

DEFINE_THISCALL_WRAPPER(MSVCP_bad_cast_vector_dtor, 8)
void * __thiscall MSVCP_bad_cast_vector_dtor(bad_cast *this, unsigned int flags)
{
    TRACE("%p %x\n", this, flags);
    if(flags & 2) {
        /* we have an array, with the number of elements stored before the first object */
        INT_PTR i, *ptr = (INT_PTR *)this-1;

        for(i=*ptr-1; i>=0; i--)
            MSVCP_bad_cast_dtor(this+i);
        MSVCRT_operator_delete(ptr);
    } else {
        MSVCP_bad_cast_dtor(this);
        if(flags & 1)
            MSVCRT_operator_delete(this);
    }

    return this;
}

/* ??4bad_cast@std@@QAEAAV01@ABV01@@Z */
/* ??4bad_cast@std@@QEAAAEAV01@AEBV01@@Z */
DEFINE_THISCALL_WRAPPER(MSVCP_bad_cast_opequals, 8)
bad_cast* __thiscall MSVCP_bad_cast_opequals(bad_cast *this, const bad_cast *rhs)
{
    TRACE("(%p %p)\n", this, rhs);

    if(this != rhs) {
        MSVCP_exception_dtor(this);
        MSVCP_exception_copy_ctor(this, rhs);
    }
    return this;
}

DEFINE_RTTI_DATA1(bad_cast, 0, &exception_rtti_base_descriptor, ".?AVbad_cast@std@@")
DEFINE_CXX_DATA1(bad_cast, &exception_cxx_type_info, MSVCP_bad_cast_dtor)

/* range_error class data */
typedef runtime_error range_error;

static range_error* MSVCP_range_error_ctor( range_error *this, exception_name name )
{
    TRACE("%p %s\n", this, EXCEPTION_STR(name));
    MSVCP_runtime_error_ctor(this, name);
    this->e.vtable = &MSVCP_range_error_vtable;
    return this;
}

/* ??0range_error@std@@QAE@ABV01@@Z */
/* ??0range_error@std@@QEAA@AEBV01@@Z */
DEFINE_THISCALL_WRAPPER(MSVCP_range_error_copy_ctor, 8)
range_error* __thiscall MSVCP_range_error_copy_ctor(
        range_error *this, const range_error *rhs)
{
    TRACE("%p %p\n", this, rhs);
    MSVCP_runtime_error_copy_ctor(this, rhs);
    this->e.vtable = &MSVCP_range_error_vtable;
    return this;
}

/* ??0range_error@std@@QAE@ABV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@1@@Z */
/* ??0range_error@std@@QEAA@AEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@1@@Z */
#ifndef _MSVCIRT
DEFINE_THISCALL_WRAPPER(MSVCP_range_error_ctor_bstr, 8)
range_error* __thiscall MSVCP_range_error_ctor_bstr(range_error *this, const basic_string_char *str)
{
    const char *name = MSVCP_basic_string_char_c_str(str);
    TRACE("(%p %p %s)\n", this, str, name);
    return MSVCP_range_error_ctor(this, EXCEPTION_NAME(name));
}
#endif

/* ??4range_error@std@@QAEAAV01@ABV01@@Z */
/* ??4range_error@std@@QEAAAEAV01@AEBV01@@Z */
DEFINE_THISCALL_WRAPPER(MSVCP_range_error_assign, 8)
range_error* __thiscall MSVCP_range_error_assign(range_error *this, const range_error *assign)
{
    MSVCP_runtime_error_dtor(this);
    return MSVCP_range_error_copy_ctor(this, assign);
}

DEFINE_RTTI_DATA2(range_error, 0, &runtime_error_rtti_base_descriptor, &exception_rtti_base_descriptor, ".?AVrange_error@std@@")
DEFINE_CXX_DATA2(range_error, &runtime_error_cxx_type_info, &exception_cxx_type_info, MSVCP_runtime_error_dtor)

/* ?_Nomemory@std@@YAXXZ */
void __cdecl _Nomemory(void)
{
    TRACE("()\n");
    throw_exception(EXCEPTION_BAD_ALLOC, NULL);
}

/* ?_Xmem@tr1@std@@YAXXZ */
void __cdecl _Xmem(void)
{
    TRACE("()\n");
    throw_exception(EXCEPTION_BAD_ALLOC, NULL);
}

/* ?_Xinvalid_argument@std@@YAXPBD@Z */
/* ?_Xinvalid_argument@std@@YAXPEBD@Z */
void __cdecl _Xinvalid_argument(const char *str)
{
    TRACE("(%s)\n", debugstr_a(str));
    throw_exception(EXCEPTION_INVALID_ARGUMENT, str);
}

/* ?_Xlength_error@std@@YAXPBD@Z */
/* ?_Xlength_error@std@@YAXPEBD@Z */
void __cdecl _Xlength_error(const char *str)
{
    TRACE("(%s)\n", debugstr_a(str));
    throw_exception(EXCEPTION_LENGTH_ERROR, str);
}

/* ?_Xout_of_range@std@@YAXPBD@Z */
/* ?_Xout_of_range@std@@YAXPEBD@Z */
void __cdecl _Xout_of_range(const char *str)
{
    TRACE("(%s)\n", debugstr_a(str));
    throw_exception(EXCEPTION_OUT_OF_RANGE, str);
}

/* ?_Xruntime_error@std@@YAXPBD@Z */
/* ?_Xruntime_error@std@@YAXPEBD@Z */
void __cdecl _Xruntime_error(const char *str)
{
    TRACE("(%s)\n", debugstr_a(str));
    throw_exception(EXCEPTION_RUNTIME_ERROR, str);
}

/* ?uncaught_exception@std@@YA_NXZ */
bool __cdecl MSVCP__uncaught_exception(void)
{
    return __uncaught_exception();
}

#if _MSVCP_VER >= 140
int __cdecl __uncaught_exceptions(void)
{
    return *__processing_throw();
}

typedef struct
{
    EXCEPTION_RECORD *rec;
    int *ref; /* not binary compatible with native */
} exception_ptr;

/*********************************************************************
 * ?__ExceptionPtrCreate@@YAXPAX@Z
 * ?__ExceptionPtrCreate@@YAXPEAX@Z
 */
void __cdecl __ExceptionPtrCreate(exception_ptr *ep)
{
    TRACE("(%p)\n", ep);

    ep->rec = NULL;
    ep->ref = NULL;
}

#ifdef __ASM_USE_THISCALL_WRAPPER
extern void call_dtor(const cxx_exception_type *type, void *func, void *object);

__ASM_GLOBAL_FUNC( call_dtor,
                   "movl 12(%esp),%ecx\n\t"
                   "call *8(%esp)\n\t"
                   "ret" );
#elif __x86_64__
static inline void call_dtor(const cxx_exception_type *type, unsigned int dtor, void *object)
{
    char *base = RtlPcToFileHeader((void*)type, (void**)&base);
    void (__cdecl *func)(void*) = (void*)(base + dtor);
    func(object);
}
#else
#define call_dtor(type, func, object) ((void (__thiscall*)(void*))(func))(object)
#endif

/*********************************************************************
 * ?__ExceptionPtrDestroy@@YAXPAX@Z
 * ?__ExceptionPtrDestroy@@YAXPEAX@Z
 */
void __cdecl __ExceptionPtrDestroy(exception_ptr *ep)
{
    TRACE("(%p)\n", ep);

    if (!ep->rec)
        return;

    if (!InterlockedDecrement(ep->ref))
    {
        if (ep->rec->ExceptionCode == CXX_EXCEPTION)
        {
            const cxx_exception_type *type = (void*)ep->rec->ExceptionInformation[2];
            void *obj = (void*)ep->rec->ExceptionInformation[1];

            if (type && type->destructor) call_dtor(type, type->destructor, obj);
            HeapFree(GetProcessHeap(), 0, obj);
        }

        HeapFree(GetProcessHeap(), 0, ep->rec);
        HeapFree(GetProcessHeap(), 0, ep->ref);
    }
}
#endif

#if _MSVCP_VER >= 70 || defined(_MSVCIRT)
#define EXCEPTION_VTABLE(name,funcs) __ASM_VTABLE(name,funcs)
#else
#define EXCEPTION_VTABLE(name,funcs) __ASM_VTABLE(name,funcs VTABLE_ADD_FUNC(MSVCP_exception__Doraise))
#endif

__ASM_BLOCK_BEGIN(exception_vtables)
    __ASM_VTABLE(type_info,
            VTABLE_ADD_FUNC(MSVCP_type_info_vector_dtor));
    EXCEPTION_VTABLE(exception,
            VTABLE_ADD_FUNC(MSVCP_exception_vector_dtor)
            VTABLE_ADD_FUNC(MSVCP_exception_what));
    EXCEPTION_VTABLE(bad_alloc,
            VTABLE_ADD_FUNC(MSVCP_bad_alloc_vector_dtor)
            VTABLE_ADD_FUNC(MSVCP_exception_what));
    EXCEPTION_VTABLE(logic_error,
            VTABLE_ADD_FUNC(MSVCP_logic_error_vector_dtor)
            VTABLE_ADD_FUNC(MSVCP_logic_error_what));
    EXCEPTION_VTABLE(length_error,
            VTABLE_ADD_FUNC(MSVCP_logic_error_vector_dtor)
            VTABLE_ADD_FUNC(MSVCP_logic_error_what));
    EXCEPTION_VTABLE(out_of_range,
            VTABLE_ADD_FUNC(MSVCP_logic_error_vector_dtor)
            VTABLE_ADD_FUNC(MSVCP_logic_error_what));
    EXCEPTION_VTABLE(invalid_argument,
            VTABLE_ADD_FUNC(MSVCP_logic_error_vector_dtor)
            VTABLE_ADD_FUNC(MSVCP_logic_error_what));
    EXCEPTION_VTABLE(runtime_error,
            VTABLE_ADD_FUNC(MSVCP_runtime_error_vector_dtor)
            VTABLE_ADD_FUNC(MSVCP_runtime_error_what));
#if _MSVCP_VER > 110
    EXCEPTION_VTABLE(_System_error,
            VTABLE_ADD_FUNC(MSVCP_failure_vector_dtor)
            VTABLE_ADD_FUNC(MSVCP_failure_what));
#endif
#if _MSVCP_VER > 90
    EXCEPTION_VTABLE(system_error,
            VTABLE_ADD_FUNC(MSVCP_failure_vector_dtor)
            VTABLE_ADD_FUNC(MSVCP_failure_what));
#endif
    EXCEPTION_VTABLE(failure,
            VTABLE_ADD_FUNC(MSVCP_failure_vector_dtor)
            VTABLE_ADD_FUNC(MSVCP_failure_what));
    EXCEPTION_VTABLE(bad_cast,
            VTABLE_ADD_FUNC(MSVCP_bad_cast_vector_dtor)
            VTABLE_ADD_FUNC(MSVCP_exception_what));
    EXCEPTION_VTABLE(range_error,
            VTABLE_ADD_FUNC(MSVCP_runtime_error_vector_dtor)
            VTABLE_ADD_FUNC(MSVCP_runtime_error_what));
__ASM_BLOCK_END

/* Internal: throws selected exception */
void throw_exception(exception_type et, const char *str)
{
    exception_name name = EXCEPTION_NAME(str);

    switch(et) {
    case EXCEPTION_RERAISE:
        _CxxThrowException(NULL, NULL);
    case EXCEPTION: {
        exception e;
        MSVCP_exception_ctor(&e, name);
        _CxxThrowException(&e, &exception_cxx_type);
    }
    case EXCEPTION_BAD_ALLOC: {
        bad_alloc e;
        MSVCP_bad_alloc_ctor(&e, name);
        _CxxThrowException(&e, &bad_alloc_cxx_type);
    }
    case EXCEPTION_BAD_CAST: {
        bad_cast e;
        MSVCP_bad_cast_ctor(&e, str);
        _CxxThrowException(&e, &bad_cast_cxx_type);
    }
    case EXCEPTION_LOGIC_ERROR: {
        logic_error e;
        MSVCP_logic_error_ctor(&e, name);
        _CxxThrowException((exception*)&e, &logic_error_cxx_type);
    }
    case EXCEPTION_LENGTH_ERROR: {
        length_error e;
        MSVCP_length_error_ctor(&e, name);
        _CxxThrowException((exception*)&e, &length_error_cxx_type);
    }
    case EXCEPTION_OUT_OF_RANGE: {
        out_of_range e;
        MSVCP_out_of_range_ctor(&e, name);
        _CxxThrowException((exception*)&e, &out_of_range_cxx_type);
    }
    case EXCEPTION_INVALID_ARGUMENT: {
        invalid_argument e;
        MSVCP_invalid_argument_ctor(&e, name);
        _CxxThrowException((exception*)&e, &invalid_argument_cxx_type);
    }
    case EXCEPTION_RUNTIME_ERROR: {
        runtime_error e;
        MSVCP_runtime_error_ctor(&e, name);
        _CxxThrowException((exception*)&e, &runtime_error_cxx_type);
    }
    case EXCEPTION_FAILURE: {
        failure e;
        MSVCP_failure_ctor(&e, name);
        _CxxThrowException((exception*)&e, &failure_cxx_type);
    }
    case EXCEPTION_RANGE_ERROR: {
        range_error e;
        MSVCP_range_error_ctor(&e, name);
        _CxxThrowException((exception*)&e, &range_error_cxx_type);
    }
    }
}

void init_exception(void *base)
{
#ifdef __x86_64__
    init_type_info_rtti(base);
    init_exception_rtti(base);
    init_bad_alloc_rtti(base);
    init_logic_error_rtti(base);
    init_length_error_rtti(base);
    init_out_of_range_rtti(base);
    init_invalid_argument_rtti(base);
    init_runtime_error_rtti(base);
#if _MSVCP_VER > 110
    init__System_error_rtti(base);
#endif
#if _MSVCP_VER > 90
    init_system_error_rtti(base);
#endif
    init_failure_rtti(base);
    init_bad_cast_rtti(base);
    init_range_error_rtti(base);

    init_exception_cxx(base);
    init_bad_alloc_cxx(base);
    init_logic_error_cxx(base);
    init_length_error_cxx(base);
    init_out_of_range_cxx(base);
    init_invalid_argument_cxx(base);
    init_runtime_error_cxx(base);
#if _MSVCP_VER > 110
    init__System_error_cxx_type_info(base);
#endif
#if _MSVCP_VER > 90
    init_system_error_cxx_type_info(base);
#endif
    init_failure_cxx(base);
    init_bad_cast_cxx(base);
    init_range_error_cxx(base);
#endif
}
