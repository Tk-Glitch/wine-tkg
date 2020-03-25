/*
 * File types.c - datatype handling stuff for internal debugger.
 *
 * Copyright (C) 1997, Eric Youngdale.
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
 *
 * Note: This really doesn't do much at the moment, but it forms the framework
 * upon which full support for datatype handling will eventually be built.
 */

#include "config.h"
#include <stdlib.h>

#include "debugger.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(winedbg);

/******************************************************************
 *		types_get_real_type
 *
 * Get rid of any potential typedef in the lvalue's type to get
 * to the 'real' type (the one we can work upon).
 */
BOOL types_get_real_type(struct dbg_type* type, DWORD* tag)
{
    if (type->id == dbg_itype_none) return FALSE;
    do
    {
        if (!types_get_info(type, TI_GET_SYMTAG, tag))
            return FALSE;
        if (*tag != SymTagTypedef) return TRUE;
    } while (types_get_info(type, TI_GET_TYPE, &type->id));
    return FALSE;
}

/******************************************************************
 *		types_extract_as_longlong
 *
 * Given a lvalue, try to get an integral (or pointer/address) value
 * out of it
 */
LONGLONG types_extract_as_longlong(const struct dbg_lvalue* lvalue,
                                   unsigned* psize, BOOL *issigned)
{
    LONGLONG            rtn = 0;
    DWORD               tag, bt;
    DWORD64             size;
    struct dbg_type     type = lvalue->type;
    BOOL                s = FALSE;

    if (!types_get_real_type(&type, &tag))
        RaiseException(DEBUG_STATUS_NOT_AN_INTEGER, 0, 0, NULL);

    if (type.id == dbg_itype_segptr)
    {
        return (LONG_PTR)memory_to_linear_addr(&lvalue->addr);
    }

    if (psize) *psize = 0;
    if (issigned) *issigned = FALSE;
    switch (tag)
    {
    case SymTagBaseType:
        if (!types_get_info(&type, TI_GET_LENGTH, &size) ||
            !types_get_info(&type, TI_GET_BASETYPE, &bt))
        {
            WINE_ERR("Couldn't get information\n");
            RaiseException(DEBUG_STATUS_INTERNAL_ERROR, 0, 0, NULL);
            return rtn;
        }
        if (size > sizeof(rtn))
        {
            WINE_ERR("Size too large (%s)\n", wine_dbgstr_longlong(size));
            RaiseException(DEBUG_STATUS_NOT_AN_INTEGER, 0, 0, NULL);
            return rtn;
        }
        switch (bt)
        {
        case btChar:
        case btInt:
            if (!dbg_curr_process->be_cpu->fetch_integer(lvalue, (unsigned)size, s = TRUE, &rtn))
                RaiseException(DEBUG_STATUS_INTERNAL_ERROR, 0, 0, NULL);
            break;
        case btUInt:
            if (!dbg_curr_process->be_cpu->fetch_integer(lvalue, (unsigned)size, s = FALSE, &rtn))
                RaiseException(DEBUG_STATUS_INTERNAL_ERROR, 0, 0, NULL);
            break;
        case btFloat:
            RaiseException(DEBUG_STATUS_NOT_AN_INTEGER, 0, 0, NULL);
        }
        if (psize) *psize = (unsigned)size;
        if (issigned) *issigned = s;
        break;
    case SymTagPointerType:
        if (!dbg_curr_process->be_cpu->fetch_integer(lvalue, sizeof(void*), s = FALSE, &rtn))
            RaiseException(DEBUG_STATUS_INTERNAL_ERROR, 0, 0, NULL);
        break;
    case SymTagArrayType:
    case SymTagUDT:
        if (!dbg_curr_process->be_cpu->fetch_integer(lvalue, sizeof(unsigned), s = FALSE, &rtn))
            RaiseException(DEBUG_STATUS_INTERNAL_ERROR, 0, 0, NULL);
        break;
    case SymTagEnum:
        /* FIXME: we don't handle enum size */
        if (!dbg_curr_process->be_cpu->fetch_integer(lvalue, sizeof(unsigned), s = FALSE, &rtn))
            RaiseException(DEBUG_STATUS_INTERNAL_ERROR, 0, 0, NULL);
        break;
    case SymTagFunctionType:
        rtn = (ULONG_PTR)memory_to_linear_addr(&lvalue->addr);
        break;
    default:
        WINE_FIXME("Unsupported tag %u\n", tag);
        RaiseException(DEBUG_STATUS_NOT_AN_INTEGER, 0, 0, NULL);
    }

    return rtn;
}

/******************************************************************
 *		types_extract_as_integer
 *
 * Given a lvalue, try to get an integral (or pointer/address) value
 * out of it
 */
long int types_extract_as_integer(const struct dbg_lvalue* lvalue)
{
    return types_extract_as_longlong(lvalue, NULL, NULL);
}

/******************************************************************
 *		types_extract_as_address
 *
 *
 */
void types_extract_as_address(const struct dbg_lvalue* lvalue, ADDRESS64* addr)
{
    if (lvalue->type.id == dbg_itype_segptr && lvalue->type.module == 0)
    {
        *addr = lvalue->addr;
    }
    else
    {
        addr->Mode = AddrModeFlat;
        addr->Offset = types_extract_as_longlong(lvalue, NULL, NULL);
    }
}

BOOL types_store_value(struct dbg_lvalue* lvalue_to, const struct dbg_lvalue* lvalue_from)
{
    LONGLONG    val;
    DWORD64     size;
    BOOL        is_signed;

    if (!types_get_info(&lvalue_to->type, TI_GET_LENGTH, &size)) return FALSE;
    if (sizeof(val) < size)
    {
        dbg_printf("Insufficient size\n");
        return FALSE;
    }
    /* FIXME: should support floats as well */
    val = types_extract_as_longlong(lvalue_from, NULL, &is_signed);
    return dbg_curr_process->be_cpu->store_integer(lvalue_to, size, is_signed, val);
}

/******************************************************************
 *		types_get_udt_element_lvalue
 *
 * Implement a structure derefencement
 */
static BOOL types_get_udt_element_lvalue(struct dbg_lvalue* lvalue, 
                                         const struct dbg_type* type, long int* tmpbuf)
{
    DWORD       offset, bitoffset;
    DWORD       bt;
    DWORD64     length;

    unsigned    mask;

    types_get_info(type, TI_GET_TYPE, &lvalue->type.id);
    lvalue->type.module = type->module;
    if (!types_get_info(type, TI_GET_OFFSET, &offset)) return FALSE;
    lvalue->addr.Offset += offset;

    if (types_get_info(type, TI_GET_BITPOSITION, &bitoffset))
    {
        types_get_info(type, TI_GET_LENGTH, &length);
        /* FIXME: this test isn't sufficient, depending on start of bitfield
         * (ie a 32 bit field can spread across 5 bytes)
         */
        if (length > 8 * sizeof(*tmpbuf)) return FALSE;
        lvalue->addr.Offset += bitoffset >> 3;
        /*
         * Bitfield operation.  We have to extract the field and store
         * it in a temporary buffer so that we get it all right.
         */
        if (!memory_read_value(lvalue, sizeof(*tmpbuf), tmpbuf)) return FALSE;
        mask = 0xffffffff << (DWORD)length;
        *tmpbuf >>= bitoffset & 7;
        *tmpbuf &= ~mask;

        lvalue->cookie      = DLV_HOST;
        lvalue->addr.Offset = (ULONG_PTR)tmpbuf;

        /*
         * OK, now we have the correct part of the number.
         * Check to see whether the basic type is signed or not, and if so,
         * we need to sign extend the number.
         */
        if (types_get_info(&lvalue->type, TI_GET_BASETYPE, &bt) && 
            bt == btInt && (*tmpbuf & (1 << ((DWORD)length - 1))))
        {
            *tmpbuf |= mask;
        }
    }
    else
    {
        if (!memory_read_value(lvalue, sizeof(*tmpbuf), tmpbuf)) return FALSE;
    }
    return TRUE;
}

/******************************************************************
 *		types_udt_find_element
 *
 */
BOOL types_udt_find_element(struct dbg_lvalue* lvalue, const char* name, long int* tmpbuf)
{
    DWORD                       tag, count;
    char                        buffer[sizeof(TI_FINDCHILDREN_PARAMS) + 256 * sizeof(DWORD)];
    TI_FINDCHILDREN_PARAMS*     fcp = (TI_FINDCHILDREN_PARAMS*)buffer;
    WCHAR*                      ptr;
    char                        tmp[256];
    struct dbg_type             type;

    if (!types_get_real_type(&lvalue->type, &tag) || tag != SymTagUDT)
        return FALSE;

    if (types_get_info(&lvalue->type, TI_GET_CHILDRENCOUNT, &count))
    {
        fcp->Start = 0;
        while (count)
        {
            fcp->Count = min(count, 256);
            if (types_get_info(&lvalue->type, TI_FINDCHILDREN, fcp))
            {
                unsigned i;
                type.module = lvalue->type.module;
                for (i = 0; i < min(fcp->Count, count); i++)
                {
                    ptr = NULL;
                    type.id = fcp->ChildId[i];
                    types_get_info(&type, TI_GET_SYMNAME, &ptr);
                    if (!ptr) continue;
                    WideCharToMultiByte(CP_ACP, 0, ptr, -1, tmp, sizeof(tmp), NULL, NULL);
                    HeapFree(GetProcessHeap(), 0, ptr);
                    if (strcmp(tmp, name)) continue;

                    return types_get_udt_element_lvalue(lvalue, &type, tmpbuf);
                }
            }
            count -= min(count, 256);
            fcp->Start += 256;
        }
    }
    return FALSE;
}

/******************************************************************
 *		types_array_index
 *
 * Grab an element from an array
 */
BOOL types_array_index(const struct dbg_lvalue* lvalue, int index, struct dbg_lvalue* result)
{
    struct dbg_type     type = lvalue->type;
    DWORD               tag, count;

    memset(result, 0, sizeof(*result));
    result->type.id = dbg_itype_none;
    result->type.module = 0;

    if (!types_get_real_type(&type, &tag)) return FALSE;
    switch (tag)
    {
    case SymTagArrayType:
        if (!types_get_info(&type, TI_GET_COUNT, &count)) return FALSE;
        if (index < 0 || index >= count) return FALSE;
        result->addr = lvalue->addr;
        break;
    case SymTagPointerType:
        if (!memory_read_value(lvalue, dbg_curr_process->be_cpu->pointer_size, &result->addr.Offset))
            return FALSE;
        result->addr.Mode = AddrModeFlat;
        switch (dbg_curr_process->be_cpu->pointer_size)
        {
        case 4: result->addr.Offset = (DWORD)result->addr.Offset; break;
        case 8: break;
        default: assert(0);
        }
        break;
    default:
        assert(FALSE);
    }
    /*
     * Get the base type, so we know how much to index by.
     */
    if (!types_get_info(&type, TI_GET_TYPE, &result->type.id)) return FALSE;
    result->type.module = type.module;
    if (index)
    {
        DWORD64             length;
        if (!types_get_info(&result->type, TI_GET_LENGTH, &length)) return FALSE;
        result->addr.Offset += index * (DWORD)length;
    }
    /* FIXME: the following statement is not always true (and can lead to buggy behavior).
     * There is no way to tell where the deref:ed value is...
     * For example:
     *	x is a pointer to struct s, x being on the stack
     *		=> lvalue is in debuggee, result is in debugger
     *	x is a pointer to struct s, x being optimized into a reg
     *		=> lvalue is debugger, result is debuggee
     *	x is a pointer to internal variable x
     *	       	=> lvalue is debugger, result is debuggee
     * So we always force debuggee address space, because dereferencing pointers to
     * internal variables is very unlikely. A correct fix would be
     * rather large.
     */
    result->cookie = DLV_TARGET;
    return TRUE;
}

struct type_find_t
{
    unsigned long       result; /* out: the found type */
    enum SymTagEnum     tag;    /* in: the tag to look for */
    union
    {
        unsigned long           typeid; /* when tag is SymTagUDT */
        const char*             name;   /* when tag is SymTagPointerType */
    } u;
};

static BOOL CALLBACK types_cb(PSYMBOL_INFO sym, ULONG size, void* _user)
{
    struct type_find_t* user = _user;
    BOOL                ret = TRUE;
    struct dbg_type     type;
    DWORD               type_id;

    if (sym->Tag == user->tag)
    {
        switch (user->tag)
        {
        case SymTagUDT:
            if (!strcmp(user->u.name, sym->Name))
            {
                user->result = sym->TypeIndex;
                ret = FALSE;
            }
            break;
        case SymTagPointerType:
            type.module = sym->ModBase;
            type.id = sym->TypeIndex;
            if (types_get_info(&type, TI_GET_TYPE, &type_id) && type_id == user->u.typeid)
            {
                user->result = sym->TypeIndex;
                ret = FALSE;
            }
            break;
        default: break;
        }
    }
    return ret;
}

/******************************************************************
 *		types_find_pointer
 *
 * Should look up in module based at linear whether (typeid*) exists
 * Otherwise, we could create it locally
 */
struct dbg_type types_find_pointer(const struct dbg_type* type)
{
    struct type_find_t  f;
    struct dbg_type     ret;

    f.result = dbg_itype_none;
    f.tag = SymTagPointerType;
    f.u.typeid = type->id;
    SymEnumTypes(dbg_curr_process->handle, type->module, types_cb, &f);
    ret.module = type->module;
    ret.id = f.result;
    return ret;
}

/******************************************************************
 *		types_find_type
 *
 * Should look up in the module based at linear address whether a type
 * named 'name' and with the correct tag exists
 */
struct dbg_type types_find_type(unsigned long linear, const char* name, enum SymTagEnum tag)

{
    struct type_find_t  f;
    struct dbg_type     ret;

    f.result = dbg_itype_none;
    f.tag = tag;
    f.u.name = name;
    SymEnumTypes(dbg_curr_process->handle, linear, types_cb, &f);
    ret.module = linear;
    ret.id = f.result;
    return ret;
}

/***********************************************************************
 *           print_value
 *
 * Implementation of the 'print' command.
 */
void print_value(const struct dbg_lvalue* lvalue, char format, int level)
{
    struct dbg_type     type = lvalue->type;
    struct dbg_lvalue   lvalue_field;
    int		        i;
    DWORD               tag;
    DWORD               count;
    DWORD64             size;

    if (!types_get_real_type(&type, &tag))
    {
        WINE_FIXME("---error\n");
        return;
    }

    if (type.id == dbg_itype_none)
    {
        /* No type, just print the addr value */
        print_bare_address(&lvalue->addr);
        goto leave;
    }

    if (format == 'i' || format == 's' || format == 'w' || format == 'b' || format == 'g')
    {
        dbg_printf("Format specifier '%c' is meaningless in 'print' command\n", format);
        format = '\0';
    }

    switch (tag)
    {
    case SymTagBaseType:
    case SymTagEnum:
    case SymTagPointerType:
        /* FIXME: this in not 100% optimal (as we're going through the typedef handling
         * stuff again
         */
        print_basic(lvalue, format);
        break;
    case SymTagUDT:
        if (types_get_info(&type, TI_GET_CHILDRENCOUNT, &count))
        {
            char                        buffer[sizeof(TI_FINDCHILDREN_PARAMS) + 256 * sizeof(DWORD)];
            TI_FINDCHILDREN_PARAMS*     fcp = (TI_FINDCHILDREN_PARAMS*)buffer;
            WCHAR*                      ptr;
            char                        tmp[256];
            long int                    tmpbuf;
            struct dbg_type             sub_type;

            dbg_printf("{");
            fcp->Start = 0;
            while (count)
            {
                fcp->Count = min(count, 256);
                if (types_get_info(&type, TI_FINDCHILDREN, fcp))
                {
                    for (i = 0; i < min(fcp->Count, count); i++)
                    {
                        ptr = NULL;
                        sub_type.module = type.module;
                        sub_type.id = fcp->ChildId[i];
                        types_get_info(&sub_type, TI_GET_SYMNAME, &ptr);
                        if (!ptr) continue;
                        WideCharToMultiByte(CP_ACP, 0, ptr, -1, tmp, sizeof(tmp), NULL, NULL);
                        dbg_printf("%s=", tmp);
                        HeapFree(GetProcessHeap(), 0, ptr);
                        lvalue_field = *lvalue;
                        if (types_get_udt_element_lvalue(&lvalue_field, &sub_type, &tmpbuf))
                        {
                            print_value(&lvalue_field, format, level + 1);
                        }
                        if (i < min(fcp->Count, count) - 1 || count > 256) dbg_printf(", ");
                    }
                }
                count -= min(count, 256);
                fcp->Start += 256;
            }
            dbg_printf("}");
        }
        break;
    case SymTagArrayType:
        /*
         * Loop over all of the entries, printing stuff as we go.
         */
        count = 1; size = 1;
        types_get_info(&type, TI_GET_COUNT, &count);
        types_get_info(&type, TI_GET_LENGTH, &size);

        if (size == count)
	{
            unsigned    len;
            char        buffer[256];
            /*
             * Special handling for character arrays.
             */
            /* FIXME should check basic type here (should be a char!!!!)... */
            len = min(count, sizeof(buffer));
            memory_get_string(dbg_curr_process,
                              memory_to_linear_addr(&lvalue->addr),
                              lvalue->cookie == DLV_TARGET, TRUE, buffer, len);
            dbg_printf("\"%s%s\"", buffer, (len < count) ? "..." : "");
            break;
        }
        lvalue_field = *lvalue;
        types_get_info(&type, TI_GET_TYPE, &lvalue_field.type.id);
        dbg_printf("{");
        for (i = 0; i < count; i++)
	{
            print_value(&lvalue_field, format, level + 1);
            lvalue_field.addr.Offset += size / count;
            dbg_printf((i == count - 1) ? "}" : ", ");
	}
        break;
    case SymTagFunctionType:
        dbg_printf("Function ");
        print_bare_address(&lvalue->addr);
        dbg_printf(": ");
        types_print_type(&type, FALSE);
        break;
    case SymTagTypedef:
        lvalue_field = *lvalue;
        types_get_info(&lvalue->type, TI_GET_TYPE, &lvalue_field.type.id);
        print_value(&lvalue_field, format, level);
        break;
    default:
        WINE_FIXME("Unknown tag (%u)\n", tag);
        RaiseException(DEBUG_STATUS_INTERNAL_ERROR, 0, 0, NULL);
        break;
    }

leave:

    if (level == 0) dbg_printf("\n");
}

static BOOL CALLBACK print_types_cb(PSYMBOL_INFO sym, ULONG size, void* ctx)
{
    struct dbg_type     type;
    type.module = sym->ModBase;
    type.id = sym->TypeIndex;
    dbg_printf("Mod: %08lx ID: %08lx\n", type.module, type.id);
    types_print_type(&type, TRUE);
    dbg_printf("\n");
    return TRUE;
}

static BOOL CALLBACK print_types_mod_cb(PCSTR mod_name, DWORD64 base, PVOID ctx)
{
    return SymEnumTypes(dbg_curr_process->handle, base, print_types_cb, ctx);
}

BOOL print_types(void)
{
    if (!dbg_curr_process)
    {
        dbg_printf("No known process, cannot print types\n");
        return FALSE;
    }
    SymEnumerateModules64(dbg_curr_process->handle, print_types_mod_cb, NULL);
    return FALSE;
}

BOOL types_print_type(const struct dbg_type* type, BOOL details)
{
    WCHAR*              ptr;
    char                tmp[256];
    const char*         name;
    DWORD               tag, udt, count;
    struct dbg_type     subtype;

    if (type->id == dbg_itype_none || !types_get_info(type, TI_GET_SYMTAG, &tag))
    {
        dbg_printf("--invalid--<%lxh>--", type->id);
        return FALSE;
    }

    if (types_get_info(type, TI_GET_SYMNAME, &ptr) && ptr)
    {
        WideCharToMultiByte(CP_ACP, 0, ptr, -1, tmp, sizeof(tmp), NULL, NULL);
        name = tmp;
        HeapFree(GetProcessHeap(), 0, ptr);
    }
    else name = "--none--";

    switch (tag)
    {
    case SymTagBaseType:
        if (details) dbg_printf("Basic<%s>", name); else dbg_printf("%s", name);
        break;
    case SymTagPointerType:
        types_get_info(type, TI_GET_TYPE, &subtype.id);
        subtype.module = type->module;
        types_print_type(&subtype, FALSE);
        dbg_printf("*");
        break;
    case SymTagUDT:
        types_get_info(type, TI_GET_UDTKIND, &udt);
        switch (udt)
        {
        case UdtStruct: dbg_printf("struct %s", name); break;
        case UdtUnion:  dbg_printf("union %s", name); break;
        case UdtClass:  dbg_printf("class %s", name); break;
        default:        WINE_ERR("Unsupported UDT type (%d) for %s\n", udt, name); break;
        }
        if (details &&
            types_get_info(type, TI_GET_CHILDRENCOUNT, &count))
        {
            char                        buffer[sizeof(TI_FINDCHILDREN_PARAMS) + 256 * sizeof(DWORD)];
            TI_FINDCHILDREN_PARAMS*     fcp = (TI_FINDCHILDREN_PARAMS*)buffer;
            WCHAR*                      ptr;
            char                        tmp[256];
            int                         i;
            struct dbg_type             type_elt;
            dbg_printf(" {");

            fcp->Start = 0;
            while (count)
            {
                fcp->Count = min(count, 256);
                if (types_get_info(type, TI_FINDCHILDREN, fcp))
                {
                    for (i = 0; i < min(fcp->Count, count); i++)
                    {
                        ptr = NULL;
                        type_elt.module = type->module;
                        type_elt.id = fcp->ChildId[i];
                        types_get_info(&type_elt, TI_GET_SYMNAME, &ptr);
                        if (!ptr) continue;
                        WideCharToMultiByte(CP_ACP, 0, ptr, -1, tmp, sizeof(tmp), NULL, NULL);
                        HeapFree(GetProcessHeap(), 0, ptr);
                        dbg_printf("%s", tmp);
                        if (types_get_info(&type_elt, TI_GET_TYPE, &type_elt.id))
                        {
                            dbg_printf(":");
                            types_print_type(&type_elt, details);
                        }
                        if (i < min(fcp->Count, count) - 1 || count > 256) dbg_printf(", ");
                    }
                }
                count -= min(count, 256);
                fcp->Start += 256;
            }
            dbg_printf("}");
        }
        break;
    case SymTagArrayType:
        types_get_info(type, TI_GET_TYPE, &subtype.id);
        subtype.module = type->module;
        types_print_type(&subtype, details);
        if (types_get_info(type, TI_GET_COUNT, &count))
            dbg_printf(" %s[%d]", name, count);
        else
            dbg_printf(" %s[]", name);
        break;
    case SymTagEnum:
        dbg_printf("enum %s", name);
        break;
    case SymTagFunctionType:
        types_get_info(type, TI_GET_TYPE, &subtype.id);
        /* is the returned type the same object as function sig itself ? */
        if (subtype.id != type->id)
        {
            subtype.module = type->module;
            types_print_type(&subtype, FALSE);
        }
        else
        {
            subtype.module = 0;
            dbg_printf("<ret_type=self>");
        }
        dbg_printf(" (*%s)(", name);
        if (types_get_info(type, TI_GET_CHILDRENCOUNT, &count))
        {
            char                        buffer[sizeof(TI_FINDCHILDREN_PARAMS) + 256 * sizeof(DWORD)];
            TI_FINDCHILDREN_PARAMS*     fcp = (TI_FINDCHILDREN_PARAMS*)buffer;
            int                         i;

            fcp->Start = 0;
            if (!count) dbg_printf("void");
            else while (count)
            {
                fcp->Count = min(count, 256);
                if (types_get_info(type, TI_FINDCHILDREN, fcp))
                {
                    for (i = 0; i < min(fcp->Count, count); i++)
                    {
                        subtype.id = fcp->ChildId[i];
                        types_get_info(&subtype, TI_GET_TYPE, &subtype.id);
                        types_print_type(&subtype, FALSE);
                        if (i < min(fcp->Count, count) - 1 || count > 256) dbg_printf(", ");
                    }
                }
                count -= min(count, 256);
                fcp->Start += 256;
            }
        }
        dbg_printf(")");
        break;
    case SymTagTypedef:
        dbg_printf("%s", name);
        break;
    default:
        WINE_ERR("Unknown type %u for %s\n", tag, name);
        break;
    }
    
    return TRUE;
}

/* helper to typecast pInfo to its expected type (_t) */
#define X(_t) (*((_t*)pInfo))

BOOL types_get_info(const struct dbg_type* type, IMAGEHLP_SYMBOL_TYPE_INFO ti, void* pInfo)
{
    if (type->id == dbg_itype_none) return FALSE;
    if (type->module != 0)
    {
        DWORD ret, tag, bt;
        ret = SymGetTypeInfo(dbg_curr_process->handle, type->module, type->id, ti, pInfo);
        if (!ret &&
            SymGetTypeInfo(dbg_curr_process->handle, type->module, type->id, TI_GET_SYMTAG, &tag) &&
            tag == SymTagBaseType &&
            SymGetTypeInfo(dbg_curr_process->handle, type->module, type->id, TI_GET_BASETYPE, &bt))
        {
            static const WCHAR voidW[] = {'v','o','i','d','\0'};
            static const WCHAR charW[] = {'c','h','a','r','\0'};
            static const WCHAR wcharW[] = {'W','C','H','A','R','\0'};
            static const WCHAR intW[] = {'i','n','t','\0'};
            static const WCHAR uintW[] = {'u','n','s','i','g','n','e','d',' ','i','n','t','\0'};
            static const WCHAR floatW[] = {'f','l','o','a','t','\0'};
            static const WCHAR boolW[] = {'b','o','o','l','\0'};
            static const WCHAR longW[] = {'l','o','n','g',' ','i','n','t','\0'};
            static const WCHAR ulongW[] = {'u','n','s','i','g','n','e','d',' ','l','o','n','g',' ','i','n','t','\0'};
            static const WCHAR complexW[] = {'c','o','m','p','l','e','x','\0'};
            const WCHAR* name = NULL;

            switch (bt)
            {
            case btVoid:        name = voidW; break;
            case btChar:        name = charW; break;
            case btWChar:       name = wcharW; break;
            case btInt:         name = intW; break;
            case btUInt:        name = uintW; break;
            case btFloat:       name = floatW; break;
            case btBool:        name = boolW; break;
            case btLong:        name = longW; break;
            case btULong:       name = ulongW; break;
            case btComplex:     name = complexW; break;
            default:            WINE_FIXME("Unsupported basic type %u\n", bt); return FALSE;
            }
            X(WCHAR*) = HeapAlloc(GetProcessHeap(), 0, (lstrlenW(name) + 1) * sizeof(WCHAR));
            if (X(WCHAR*))
            {
                lstrcpyW(X(WCHAR*), name);
                ret = TRUE;
            }
        }
        return ret;
    }

    assert(type->id >= dbg_itype_first);

    switch (type->id)
    {
    case dbg_itype_unsigned_long_int:
        switch (ti)
        {
        case TI_GET_SYMTAG:     X(DWORD)   = SymTagBaseType; break;
        case TI_GET_LENGTH:     X(DWORD64) = ADDRSIZE; break;
        case TI_GET_BASETYPE:   X(DWORD)   = btUInt; break;
        default: WINE_FIXME("unsupported %u for u-long int\n", ti); return FALSE;
        }
        break;
    case dbg_itype_signed_long_int:
        switch (ti)
        {
        case TI_GET_SYMTAG:     X(DWORD)   = SymTagBaseType; break;
        case TI_GET_LENGTH:     X(DWORD64) = ADDRSIZE; break;
        case TI_GET_BASETYPE:   X(DWORD)   = btInt; break;
        default: WINE_FIXME("unsupported %u for s-long int\n", ti); return FALSE;
        }
        break;
    case dbg_itype_unsigned_int:
        switch (ti)
        {
        case TI_GET_SYMTAG:     X(DWORD)   = SymTagBaseType; break;
        case TI_GET_LENGTH:     X(DWORD64) = 4; break;
        case TI_GET_BASETYPE:   X(DWORD)   = btUInt; break;
        default: WINE_FIXME("unsupported %u for u-int\n", ti); return FALSE;
        }
        break;
    case dbg_itype_signed_int:
        switch (ti)
        {
        case TI_GET_SYMTAG:     X(DWORD)   = SymTagBaseType; break;
        case TI_GET_LENGTH:     X(DWORD64) = 4; break;
        case TI_GET_BASETYPE:   X(DWORD)   = btInt; break;
        default: WINE_FIXME("unsupported %u for s-int\n", ti); return FALSE;
        }
        break;
    case dbg_itype_unsigned_short_int:
        switch (ti)
        {
        case TI_GET_SYMTAG:     X(DWORD)   = SymTagBaseType; break;
        case TI_GET_LENGTH:     X(DWORD64) = 2; break;
        case TI_GET_BASETYPE:   X(DWORD)   = btUInt; break;
        default: WINE_FIXME("unsupported %u for u-short int\n", ti); return FALSE;
        }
        break;
    case dbg_itype_signed_short_int:
        switch (ti)
        {
        case TI_GET_SYMTAG:     X(DWORD)   = SymTagBaseType; break;
        case TI_GET_LENGTH:     X(DWORD64) = 2; break;
        case TI_GET_BASETYPE:   X(DWORD)   = btInt; break;
        default: WINE_FIXME("unsupported %u for s-short int\n", ti); return FALSE;
        }
        break;
    case dbg_itype_unsigned_char_int:
        switch (ti)
        {
        case TI_GET_SYMTAG:     X(DWORD)   = SymTagBaseType; break;
        case TI_GET_LENGTH:     X(DWORD64) = 1; break;
        case TI_GET_BASETYPE:   X(DWORD)   = btUInt; break;
        default: WINE_FIXME("unsupported %u for u-char int\n", ti); return FALSE;
        }
        break;
    case dbg_itype_signed_char_int:
        switch (ti)
        {
        case TI_GET_SYMTAG:     X(DWORD)   = SymTagBaseType; break;
        case TI_GET_LENGTH:     X(DWORD64) = 1; break;
        case TI_GET_BASETYPE:   X(DWORD)   = btInt; break;
        default: WINE_FIXME("unsupported %u for s-char int\n", ti); return FALSE;
        }
        break;
    case dbg_itype_char:
        switch (ti)
        {
        case TI_GET_SYMTAG:     X(DWORD)   = SymTagBaseType; break;
        case TI_GET_LENGTH:     X(DWORD64) = 1; break;
        case TI_GET_BASETYPE:   X(DWORD)   = btChar; break;
        default: WINE_FIXME("unsupported %u for char int\n", ti); return FALSE;
        }
        break;
    case dbg_itype_astring:
        switch (ti)
        {
        case TI_GET_SYMTAG:     X(DWORD)   = SymTagPointerType; break;
        case TI_GET_LENGTH:     X(DWORD64) = ADDRSIZE; break;
        case TI_GET_TYPE:       X(DWORD)   = dbg_itype_char; break;
        default: WINE_FIXME("unsupported %u for a string\n", ti); return FALSE;
        }
        break;
    case dbg_itype_segptr:
        switch (ti)
        {
        case TI_GET_SYMTAG:     X(DWORD)   = SymTagBaseType; break;
        case TI_GET_LENGTH:     X(DWORD64) = 4; break;
        case TI_GET_BASETYPE:   X(DWORD)   = btInt; break;
        default: WINE_FIXME("unsupported %u for seg-ptr\n", ti); return FALSE;
        }
        break;
    case dbg_itype_short_real:
        switch (ti)
        {
        case TI_GET_SYMTAG:     X(DWORD)   = SymTagBaseType; break;
        case TI_GET_LENGTH:     X(DWORD64) = 4; break;
        case TI_GET_BASETYPE:   X(DWORD)   = btFloat; break;
        default: WINE_FIXME("unsupported %u for short real\n", ti); return FALSE;
        }
        break;
    case dbg_itype_real:
        switch (ti)
        {
        case TI_GET_SYMTAG:     X(DWORD)   = SymTagBaseType; break;
        case TI_GET_LENGTH:     X(DWORD64) = 8; break;
        case TI_GET_BASETYPE:   X(DWORD)   = btFloat; break;
        default: WINE_FIXME("unsupported %u for real\n", ti); return FALSE;
        }
        break;
    case dbg_itype_long_real:
        switch (ti)
        {
        case TI_GET_SYMTAG:     X(DWORD)   = SymTagBaseType; break;
        case TI_GET_LENGTH:     X(DWORD64) = 10; break;
        case TI_GET_BASETYPE:   X(DWORD)   = btFloat; break;
        default: WINE_FIXME("unsupported %u for long real\n", ti); return FALSE;
        }
        break;
    case dbg_itype_m128a:
        switch (ti)
        {
        case TI_GET_SYMTAG:     X(DWORD)   = SymTagBaseType; break;
        case TI_GET_LENGTH:     X(DWORD64) = 16; break;
        case TI_GET_BASETYPE:   X(DWORD)   = btUInt; break;
        default: WINE_FIXME("unsupported %u for XMM register\n", ti); return FALSE;
        }
        break;
    default: WINE_FIXME("unsupported type id 0x%lx\n", type->id);
    }

#undef X
    return TRUE;
}
