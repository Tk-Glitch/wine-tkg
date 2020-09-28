/*
 * Copyright 2009 Henri Verbeet for CodeWeavers
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
 */

#include "config.h"
#include "wine/port.h"

#include <stdarg.h>
#ifdef HAVE_COMMONCRYPTO_COMMONCRYPTOR_H
#include <AvailabilityMacros.h>
#include <CommonCrypto/CommonCryptor.h>
#endif

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winbase.h"
#include "ntsecapi.h"
#include "wincrypt.h"
#include "bcrypt.h"

#include "bcrypt_internal.h"

#include "wine/debug.h"
#include "wine/heap.h"
#include "wine/unicode.h"

WINE_DEFAULT_DEBUG_CHANNEL(bcrypt);

static HINSTANCE instance;

NTSTATUS WINAPI BCryptAddContextFunction(ULONG table, LPCWSTR context, ULONG iface, LPCWSTR function, ULONG pos)
{
    FIXME("%08x, %s, %08x, %s, %u: stub\n", table, debugstr_w(context), iface, debugstr_w(function), pos);
    return STATUS_SUCCESS;
}

NTSTATUS WINAPI BCryptAddContextFunctionProvider(ULONG table, LPCWSTR context, ULONG iface, LPCWSTR function, LPCWSTR provider, ULONG pos)
{
    FIXME("%08x, %s, %08x, %s, %s, %u: stub\n", table, debugstr_w(context), iface, debugstr_w(function), debugstr_w(provider), pos);
    return STATUS_SUCCESS;
}

NTSTATUS WINAPI BCryptRemoveContextFunction(ULONG table, LPCWSTR context, ULONG iface, LPCWSTR function)
{
    FIXME("%08x, %s, %08x, %s: stub\n", table, debugstr_w(context), iface, debugstr_w(function));
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS WINAPI BCryptRemoveContextFunctionProvider(ULONG table, LPCWSTR context, ULONG iface, LPCWSTR function, LPCWSTR provider)
{
    FIXME("%08x, %s, %08x, %s, %s: stub\n", table, debugstr_w(context), iface, debugstr_w(function), debugstr_w(provider));
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS WINAPI BCryptEnumContextFunctions( ULONG table, const WCHAR *ctx, ULONG iface, ULONG *buflen,
                                            CRYPT_CONTEXT_FUNCTIONS **buffer )
{
    FIXME( "%u, %s, %u, %p, %p\n", table, debugstr_w(ctx), iface, buflen, buffer );
    return STATUS_NOT_IMPLEMENTED;
}

void WINAPI BCryptFreeBuffer( void *buffer )
{
    FIXME( "%p\n", buffer );
}

NTSTATUS WINAPI BCryptRegisterProvider(LPCWSTR provider, ULONG flags, PCRYPT_PROVIDER_REG reg)
{
    FIXME("%s, %08x, %p: stub\n", debugstr_w(provider), flags, reg);
    return STATUS_SUCCESS;
}

NTSTATUS WINAPI BCryptUnregisterProvider(LPCWSTR provider)
{
    FIXME("%s: stub\n", debugstr_w(provider));
    return STATUS_NOT_IMPLEMENTED;
}

#define MAX_HASH_OUTPUT_BYTES 64
#define MAX_HASH_BLOCK_BITS 1024

/* ordered by class, keep in sync with enum alg_id */
static const struct
{
    const WCHAR *name;
    ULONG        class;
    ULONG        object_length;
    ULONG        hash_length;
    ULONG        block_bits;
}
builtin_algorithms[] =
{
    {  BCRYPT_AES_ALGORITHM,        BCRYPT_CIPHER_INTERFACE,                654,    0,    0 },
    {  BCRYPT_SHA256_ALGORITHM,     BCRYPT_HASH_INTERFACE,                  286,   32,  512 },
    {  BCRYPT_SHA384_ALGORITHM,     BCRYPT_HASH_INTERFACE,                  382,   48, 1024 },
    {  BCRYPT_SHA512_ALGORITHM,     BCRYPT_HASH_INTERFACE,                  382,   64, 1024 },
    {  BCRYPT_SHA1_ALGORITHM,       BCRYPT_HASH_INTERFACE,                  278,   20,  512 },
    {  BCRYPT_MD5_ALGORITHM,        BCRYPT_HASH_INTERFACE,                  274,   16,  512 },
    {  BCRYPT_MD4_ALGORITHM,        BCRYPT_HASH_INTERFACE,                  270,   16,  512 },
    {  BCRYPT_MD2_ALGORITHM,        BCRYPT_HASH_INTERFACE,                  270,   16,  128 },
    {  BCRYPT_RSA_ALGORITHM,        BCRYPT_ASYMMETRIC_ENCRYPTION_INTERFACE, 0,      0,    0 },
    {  BCRYPT_ECDH_P256_ALGORITHM,  BCRYPT_SECRET_AGREEMENT_INTERFACE,      0,      0,    0 },
    {  BCRYPT_RSA_SIGN_ALGORITHM,   BCRYPT_SIGNATURE_INTERFACE,             0,      0,    0 },
    {  BCRYPT_ECDSA_P256_ALGORITHM, BCRYPT_SIGNATURE_INTERFACE,             0,      0,    0 },
    {  BCRYPT_ECDSA_P384_ALGORITHM, BCRYPT_SIGNATURE_INTERFACE,             0,      0,    0 },
    {  BCRYPT_DSA_ALGORITHM,        BCRYPT_SIGNATURE_INTERFACE,             0,      0,    0 },
    {  BCRYPT_RNG_ALGORITHM,        BCRYPT_RNG_INTERFACE,                   0,      0,    0 },
};

static BOOL match_operation_type( ULONG type, ULONG class )
{
    if (!type) return TRUE;
    switch (class)
    {
    case BCRYPT_CIPHER_INTERFACE:                return type & BCRYPT_CIPHER_OPERATION;
    case BCRYPT_HASH_INTERFACE:                  return type & BCRYPT_HASH_OPERATION;
    case BCRYPT_ASYMMETRIC_ENCRYPTION_INTERFACE: return type & BCRYPT_ASYMMETRIC_ENCRYPTION_OPERATION;
    case BCRYPT_SECRET_AGREEMENT_INTERFACE:      return type & BCRYPT_SECRET_AGREEMENT_OPERATION;
    case BCRYPT_SIGNATURE_INTERFACE:             return type & BCRYPT_SIGNATURE_OPERATION;
    case BCRYPT_RNG_INTERFACE:                   return type & BCRYPT_RNG_OPERATION;
    default: break;
    }
    return FALSE;
}

NTSTATUS WINAPI BCryptEnumAlgorithms( ULONG type, ULONG *ret_count, BCRYPT_ALGORITHM_IDENTIFIER **ret_list, ULONG flags )
{
    static const ULONG supported = BCRYPT_CIPHER_OPERATION |\
                                   BCRYPT_HASH_OPERATION |\
                                   BCRYPT_ASYMMETRIC_ENCRYPTION_OPERATION |\
                                   BCRYPT_SECRET_AGREEMENT_OPERATION |\
                                   BCRYPT_SIGNATURE_OPERATION |\
                                   BCRYPT_RNG_OPERATION;
    BCRYPT_ALGORITHM_IDENTIFIER *list;
    ULONG i, count = 0;

    TRACE( "%08x, %p, %p, %08x\n", type, ret_count, ret_list, flags );

    if (!ret_count || !ret_list || (type & ~supported)) return STATUS_INVALID_PARAMETER;

    for (i = 0; i < ARRAY_SIZE( builtin_algorithms ); i++)
    {
        if (match_operation_type( type, builtin_algorithms[i].class )) count++;
    }

    if (!(list = heap_alloc( count * sizeof(*list) ))) return STATUS_NO_MEMORY;

    for (i = 0; i < ARRAY_SIZE( builtin_algorithms ); i++)
    {
        if (!match_operation_type( type, builtin_algorithms[i].class )) continue;
        list[i].pszName = (WCHAR *)builtin_algorithms[i].name;
        list[i].dwClass = builtin_algorithms[i].class;
        list[i].dwFlags = 0;
    }

    *ret_count = count;
    *ret_list = list;
    return STATUS_SUCCESS;
}

NTSTATUS WINAPI BCryptGenRandom(BCRYPT_ALG_HANDLE handle, UCHAR *buffer, ULONG count, ULONG flags)
{
    const DWORD supported_flags = BCRYPT_USE_SYSTEM_PREFERRED_RNG;
    struct algorithm *algorithm = handle;

    TRACE("%p, %p, %u, %08x - semi-stub\n", handle, buffer, count, flags);

    if (!algorithm)
    {
        /* It's valid to call without an algorithm if BCRYPT_USE_SYSTEM_PREFERRED_RNG
         * is set. In this case the preferred system RNG is used.
         */
        if (!(flags & BCRYPT_USE_SYSTEM_PREFERRED_RNG))
            return STATUS_INVALID_HANDLE;
    }
    else if (algorithm->hdr.magic != MAGIC_ALG || algorithm->id != ALG_ID_RNG)
        return STATUS_INVALID_HANDLE;

    if (!buffer)
        return STATUS_INVALID_PARAMETER;

    if (flags & ~supported_flags)
        FIXME("unsupported flags %08x\n", flags & ~supported_flags);

    if (algorithm)
        FIXME("ignoring selected algorithm\n");

    /* When zero bytes are requested the function returns success too. */
    if (!count)
        return STATUS_SUCCESS;

    if (algorithm || (flags & BCRYPT_USE_SYSTEM_PREFERRED_RNG))
    {
        if (RtlGenRandom(buffer, count))
            return STATUS_SUCCESS;
    }

    FIXME("called with unsupported parameters, returning error\n");
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS WINAPI BCryptOpenAlgorithmProvider( BCRYPT_ALG_HANDLE *handle, LPCWSTR id, LPCWSTR implementation, DWORD flags )
{
    const DWORD supported_flags = BCRYPT_ALG_HANDLE_HMAC_FLAG | BCRYPT_HASH_REUSABLE_FLAG;
    struct algorithm *alg;
    enum alg_id alg_id;
    ULONG i;

    TRACE( "%p, %s, %s, %08x\n", handle, wine_dbgstr_w(id), wine_dbgstr_w(implementation), flags );

    if (!handle || !id) return STATUS_INVALID_PARAMETER;
    if (flags & ~supported_flags)
    {
        FIXME( "unsupported flags %08x\n", flags & ~supported_flags);
        return STATUS_NOT_IMPLEMENTED;
    }

    for (i = 0; i < ARRAY_SIZE( builtin_algorithms ); i++)
    {
        if (!strcmpW( id, builtin_algorithms[i].name))
        {
            alg_id = i;
            break;
        }
    }
    if (i == ARRAY_SIZE( builtin_algorithms ))
    {
        FIXME( "algorithm %s not supported\n", debugstr_w(id) );
        return STATUS_NOT_IMPLEMENTED;
    }

    if (implementation && strcmpW( implementation, MS_PRIMITIVE_PROVIDER ))
    {
        FIXME( "implementation %s not supported\n", debugstr_w(implementation) );
        return STATUS_NOT_IMPLEMENTED;
    }

    if (!(alg = heap_alloc( sizeof(*alg) ))) return STATUS_NO_MEMORY;
    alg->hdr.magic = MAGIC_ALG;
    alg->id        = alg_id;
    alg->mode      = MODE_ID_CBC;
    alg->flags     = flags;

    *handle = alg;
    return STATUS_SUCCESS;
}

NTSTATUS WINAPI BCryptCloseAlgorithmProvider( BCRYPT_ALG_HANDLE handle, DWORD flags )
{
    struct algorithm *alg = handle;

    TRACE( "%p, %08x\n", handle, flags );

    if (!alg || alg->hdr.magic != MAGIC_ALG) return STATUS_INVALID_HANDLE;
    alg->hdr.magic = 0;
    heap_free( alg );
    return STATUS_SUCCESS;
}

NTSTATUS WINAPI BCryptGetFipsAlgorithmMode(BOOLEAN *enabled)
{
    FIXME("%p - semi-stub\n", enabled);

    if (!enabled)
        return STATUS_INVALID_PARAMETER;

    *enabled = FALSE;
    return STATUS_SUCCESS;
}

struct hash_impl
{
    union
    {
        MD2_CTX md2;
        MD4_CTX md4;
        MD5_CTX md5;
        SHA_CTX sha1;
        SHA256_CTX sha256;
        SHA512_CTX sha512;
    } u;
};

static NTSTATUS hash_init( struct hash_impl *hash, enum alg_id alg_id )
{
    switch (alg_id)
    {
    case ALG_ID_MD2:
        md2_init( &hash->u.md2 );
        break;

    case ALG_ID_MD4:
        MD4Init( &hash->u.md4 );
        break;

    case ALG_ID_MD5:
        MD5Init( &hash->u.md5 );
        break;

    case ALG_ID_SHA1:
        A_SHAInit( &hash->u.sha1 );
        break;

    case ALG_ID_SHA256:
        sha256_init( &hash->u.sha256 );
        break;

    case ALG_ID_SHA384:
        sha384_init( &hash->u.sha512 );
        break;

    case ALG_ID_SHA512:
        sha512_init( &hash->u.sha512 );
        break;

    default:
        ERR( "unhandled id %u\n", alg_id );
        return STATUS_NOT_IMPLEMENTED;
    }
    return STATUS_SUCCESS;
}

static NTSTATUS hash_update( struct hash_impl *hash, enum alg_id alg_id,
                             UCHAR *input, ULONG size )
{
    switch (alg_id)
    {
    case ALG_ID_MD2:
        md2_update( &hash->u.md2, input, size );
        break;

    case ALG_ID_MD4:
        MD4Update( &hash->u.md4, input, size );
        break;

    case ALG_ID_MD5:
        MD5Update( &hash->u.md5, input, size );
        break;

    case ALG_ID_SHA1:
        A_SHAUpdate( &hash->u.sha1, input, size );
        break;

    case ALG_ID_SHA256:
        sha256_update( &hash->u.sha256, input, size );
        break;

    case ALG_ID_SHA384:
        sha384_update( &hash->u.sha512, input, size );
        break;

    case ALG_ID_SHA512:
        sha512_update( &hash->u.sha512, input, size );
        break;

    default:
        ERR( "unhandled id %u\n", alg_id );
        return STATUS_NOT_IMPLEMENTED;
    }
    return STATUS_SUCCESS;
}

static NTSTATUS hash_finish( struct hash_impl *hash, enum alg_id alg_id,
                             UCHAR *output, ULONG size )
{
    switch (alg_id)
    {
    case ALG_ID_MD2:
        md2_finalize( &hash->u.md2, output );
        break;

    case ALG_ID_MD4:
        MD4Final( &hash->u.md4 );
        memcpy( output, hash->u.md4.digest, 16 );
        break;

    case ALG_ID_MD5:
        MD5Final( &hash->u.md5 );
        memcpy( output, hash->u.md5.digest, 16 );
        break;

    case ALG_ID_SHA1:
        A_SHAFinal( &hash->u.sha1, (ULONG *)output );
        break;

    case ALG_ID_SHA256:
        sha256_finalize( &hash->u.sha256, output );
        break;

    case ALG_ID_SHA384:
        sha384_finalize( &hash->u.sha512, output );
        break;

    case ALG_ID_SHA512:
        sha512_finalize( &hash->u.sha512, output );
        break;

    default:
        ERR( "unhandled id %u\n", alg_id );
        return STATUS_NOT_IMPLEMENTED;
    }
    return STATUS_SUCCESS;
}

#define HASH_FLAG_HMAC      0x01
#define HASH_FLAG_REUSABLE  0x02
struct hash
{
    struct object     hdr;
    enum alg_id       alg_id;
    ULONG             flags;
    UCHAR            *secret;
    ULONG             secret_len;
    struct hash_impl  outer;
    struct hash_impl  inner;
};

#define BLOCK_LENGTH_AES        16

static NTSTATUS generic_alg_property( enum alg_id id, const WCHAR *prop, UCHAR *buf, ULONG size, ULONG *ret_size )
{
    if (!strcmpW( prop, BCRYPT_OBJECT_LENGTH ))
    {
        if (!builtin_algorithms[id].object_length)
            return STATUS_NOT_SUPPORTED;
        *ret_size = sizeof(ULONG);
        if (size < sizeof(ULONG))
            return STATUS_BUFFER_TOO_SMALL;
        if (buf)
            *(ULONG *)buf = builtin_algorithms[id].object_length;
        return STATUS_SUCCESS;
    }

    if (!strcmpW( prop, BCRYPT_HASH_LENGTH ))
    {
        if (!builtin_algorithms[id].hash_length)
            return STATUS_NOT_SUPPORTED;
        *ret_size = sizeof(ULONG);
        if (size < sizeof(ULONG))
            return STATUS_BUFFER_TOO_SMALL;
        if(buf)
            *(ULONG*)buf = builtin_algorithms[id].hash_length;
        return STATUS_SUCCESS;
    }

    if (!strcmpW( prop, BCRYPT_ALGORITHM_NAME ))
    {
        *ret_size = (strlenW(builtin_algorithms[id].name) + 1) * sizeof(WCHAR);
        if (size < *ret_size)
            return STATUS_BUFFER_TOO_SMALL;
        if(buf)
            memcpy(buf, builtin_algorithms[id].name, *ret_size);
        return STATUS_SUCCESS;
    }

    return STATUS_NOT_IMPLEMENTED;
}

static NTSTATUS get_aes_property( enum mode_id mode, const WCHAR *prop, UCHAR *buf, ULONG size, ULONG *ret_size )
{
    if (!strcmpW( prop, BCRYPT_BLOCK_LENGTH ))
    {
        *ret_size = sizeof(ULONG);
        if (size < sizeof(ULONG)) return STATUS_BUFFER_TOO_SMALL;
        if (buf) *(ULONG *)buf = BLOCK_LENGTH_AES;
        return STATUS_SUCCESS;
    }
    if (!strcmpW( prop, BCRYPT_CHAINING_MODE ))
    {
        const WCHAR *str;
        switch (mode)
        {
        case MODE_ID_ECB: str = BCRYPT_CHAIN_MODE_ECB; break;
        case MODE_ID_CBC: str = BCRYPT_CHAIN_MODE_CBC; break;
        case MODE_ID_GCM: str = BCRYPT_CHAIN_MODE_GCM; break;
        default: return STATUS_NOT_IMPLEMENTED;
        }

        *ret_size = 64;
        if (size < *ret_size) return STATUS_BUFFER_TOO_SMALL;
        memcpy( buf, str, (strlenW(str) + 1) * sizeof(WCHAR) );
        return STATUS_SUCCESS;
    }
    if (!strcmpW( prop, BCRYPT_KEY_LENGTHS ))
    {
        BCRYPT_KEY_LENGTHS_STRUCT *key_lengths = (void *)buf;
        *ret_size = sizeof(*key_lengths);
        if (key_lengths && size < *ret_size) return STATUS_BUFFER_TOO_SMALL;
        if (key_lengths)
        {
            key_lengths->dwMinLength = 128;
            key_lengths->dwMaxLength = 256;
            key_lengths->dwIncrement = 64;
        }
        return STATUS_SUCCESS;
    }
    if (!strcmpW( prop, BCRYPT_AUTH_TAG_LENGTH ))
    {
        BCRYPT_AUTH_TAG_LENGTHS_STRUCT *tag_length = (void *)buf;
        if (mode != MODE_ID_GCM) return STATUS_NOT_SUPPORTED;
        *ret_size = sizeof(*tag_length);
        if (tag_length && size < *ret_size) return STATUS_BUFFER_TOO_SMALL;
        if (tag_length)
        {
            tag_length->dwMinLength = 12;
            tag_length->dwMaxLength = 16;
            tag_length->dwIncrement =  1;
        }
        return STATUS_SUCCESS;
    }

    FIXME( "unsupported property %s\n", debugstr_w(prop) );
    return STATUS_NOT_IMPLEMENTED;
}

static NTSTATUS get_rsa_property( enum mode_id mode, const WCHAR *prop, UCHAR *buf, ULONG size, ULONG *ret_size )
{
    if (!strcmpW( prop, BCRYPT_PADDING_SCHEMES ))
    {
        *ret_size = sizeof(ULONG);
        if (size < sizeof(ULONG)) return STATUS_BUFFER_TOO_SMALL;
        if (buf) *(ULONG *)buf = BCRYPT_SUPPORTED_PAD_PKCS1_SIG;
        return STATUS_SUCCESS;
    }

    FIXME( "unsupported property %s\n", debugstr_w(prop) );
    return STATUS_NOT_IMPLEMENTED;
}

static NTSTATUS get_dsa_property( enum mode_id mode, const WCHAR *prop, UCHAR *buf, ULONG size, ULONG *ret_size )
{
    if (!strcmpW( prop, BCRYPT_PADDING_SCHEMES )) return STATUS_NOT_SUPPORTED;
    FIXME( "unsupported property %s\n", debugstr_w(prop) );
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS get_alg_property( const struct algorithm *alg, const WCHAR *prop, UCHAR *buf, ULONG size, ULONG *ret_size )
{
    NTSTATUS status;

    status = generic_alg_property( alg->id, prop, buf, size, ret_size );
    if (status != STATUS_NOT_IMPLEMENTED)
        return status;

    switch (alg->id)
    {
    case ALG_ID_AES:
        return get_aes_property( alg->mode, prop, buf, size, ret_size );

    case ALG_ID_RSA:
        return get_rsa_property( alg->mode, prop, buf, size, ret_size );

    case ALG_ID_DSA:
        return get_dsa_property( alg->mode, prop, buf, size, ret_size );

    default:
        break;
    }

    FIXME( "unsupported property %s algorithm %u\n", debugstr_w(prop), alg->id );
    return STATUS_NOT_IMPLEMENTED;
}

static NTSTATUS set_alg_property( struct algorithm *alg, const WCHAR *prop, UCHAR *value, ULONG size, ULONG flags )
{
    switch (alg->id)
    {
    case ALG_ID_AES:
        if (!strcmpW( prop, BCRYPT_CHAINING_MODE ))
        {
            if (!strcmpW( (WCHAR *)value, BCRYPT_CHAIN_MODE_ECB ))
            {
                alg->mode = MODE_ID_ECB;
                return STATUS_SUCCESS;
            }
            else if (!strcmpW( (WCHAR *)value, BCRYPT_CHAIN_MODE_CBC ))
            {
                alg->mode = MODE_ID_CBC;
                return STATUS_SUCCESS;
            }
            else if (!strcmpW( (WCHAR *)value, BCRYPT_CHAIN_MODE_GCM ))
            {
                alg->mode = MODE_ID_GCM;
                return STATUS_SUCCESS;
            }
            else
            {
                FIXME( "unsupported mode %s\n", debugstr_w((WCHAR *)value) );
                return STATUS_NOT_IMPLEMENTED;
            }
        }
        FIXME( "unsupported aes algorithm property %s\n", debugstr_w(prop) );
        return STATUS_NOT_IMPLEMENTED;

    default:
        FIXME( "unsupported algorithm %u\n", alg->id );
        return STATUS_NOT_IMPLEMENTED;
    }
}

static NTSTATUS get_hash_property( const struct hash *hash, const WCHAR *prop, UCHAR *buf, ULONG size, ULONG *ret_size )
{
    NTSTATUS status;

    status = generic_alg_property( hash->alg_id, prop, buf, size, ret_size );
    if (status == STATUS_NOT_IMPLEMENTED)
        FIXME( "unsupported property %s\n", debugstr_w(prop) );
    return status;
}

static NTSTATUS get_key_property( const struct key *key, const WCHAR *prop, UCHAR *buf, ULONG size, ULONG *ret_size )
{
    switch (key->alg_id)
    {
    case ALG_ID_AES:
        if (!strcmpW( prop, BCRYPT_AUTH_TAG_LENGTH )) return STATUS_NOT_SUPPORTED;
        return get_aes_property( key->u.s.mode, prop, buf, size, ret_size );

    default:
        FIXME( "unsupported algorithm %u\n", key->alg_id );
        return STATUS_NOT_IMPLEMENTED;
    }
}

NTSTATUS WINAPI BCryptGetProperty( BCRYPT_HANDLE handle, LPCWSTR prop, UCHAR *buffer, ULONG count, ULONG *res, ULONG flags )
{
    struct object *object = handle;

    TRACE( "%p, %s, %p, %u, %p, %08x\n", handle, wine_dbgstr_w(prop), buffer, count, res, flags );

    if (!object) return STATUS_INVALID_HANDLE;
    if (!prop || !res) return STATUS_INVALID_PARAMETER;

    switch (object->magic)
    {
    case MAGIC_ALG:
    {
        const struct algorithm *alg = (const struct algorithm *)object;
        return get_alg_property( alg, prop, buffer, count, res );
    }
    case MAGIC_KEY:
    {
        const struct key *key = (const struct key *)object;
        return get_key_property( key, prop, buffer, count, res );
    }
    case MAGIC_HASH:
    {
        const struct hash *hash = (const struct hash *)object;
        return get_hash_property( hash, prop, buffer, count, res );
    }
    default:
        WARN( "unknown magic %08x\n", object->magic );
        return STATUS_INVALID_HANDLE;
    }
}

static NTSTATUS prepare_hash( struct hash *hash )
{
    UCHAR buffer[MAX_HASH_BLOCK_BITS / 8] = {0};
    int block_bytes, i;
    NTSTATUS status;

    /* initialize hash */
    if ((status = hash_init( &hash->inner, hash->alg_id ))) return status;
    if (!(hash->flags & HASH_FLAG_HMAC)) return STATUS_SUCCESS;

    /* initialize hmac */
    if ((status = hash_init( &hash->outer, hash->alg_id ))) return status;
    block_bytes = builtin_algorithms[hash->alg_id].block_bits / 8;
    if (hash->secret_len > block_bytes)
    {
        struct hash_impl temp;
        if ((status = hash_init( &temp, hash->alg_id ))) return status;
        if ((status = hash_update( &temp, hash->alg_id, hash->secret, hash->secret_len ))) return status;
        if ((status = hash_finish( &temp, hash->alg_id, buffer,
                                   builtin_algorithms[hash->alg_id].hash_length ))) return status;
    }
    else memcpy( buffer, hash->secret, hash->secret_len );

    for (i = 0; i < block_bytes; i++) buffer[i] ^= 0x5c;
    if ((status = hash_update( &hash->outer, hash->alg_id, buffer, block_bytes ))) return status;
    for (i = 0; i < block_bytes; i++) buffer[i] ^= (0x5c ^ 0x36);
    return hash_update( &hash->inner, hash->alg_id, buffer, block_bytes );
}

NTSTATUS WINAPI BCryptCreateHash( BCRYPT_ALG_HANDLE algorithm, BCRYPT_HASH_HANDLE *handle, UCHAR *object, ULONG objectlen,
                                  UCHAR *secret, ULONG secretlen, ULONG flags )
{
    struct algorithm *alg = algorithm;
    struct hash *hash;
    NTSTATUS status;

    TRACE( "%p, %p, %p, %u, %p, %u, %08x - stub\n", algorithm, handle, object, objectlen,
           secret, secretlen, flags );
    if (flags & ~BCRYPT_HASH_REUSABLE_FLAG)
    {
        FIXME( "unimplemented flags %08x\n", flags );
        return STATUS_NOT_IMPLEMENTED;
    }

    if (!alg || alg->hdr.magic != MAGIC_ALG) return STATUS_INVALID_HANDLE;
    if (object) FIXME( "ignoring object buffer\n" );

    if (!(hash = heap_alloc_zero( sizeof(*hash) ))) return STATUS_NO_MEMORY;
    hash->hdr.magic = MAGIC_HASH;
    hash->alg_id    = alg->id;
    if (alg->flags & BCRYPT_ALG_HANDLE_HMAC_FLAG) hash->flags = HASH_FLAG_HMAC;
    if ((alg->flags & BCRYPT_HASH_REUSABLE_FLAG) || (flags & BCRYPT_HASH_REUSABLE_FLAG))
        hash->flags |= HASH_FLAG_REUSABLE;

    if (secretlen && !(hash->secret = heap_alloc( secretlen )))
    {
        heap_free( hash );
        return STATUS_NO_MEMORY;
    }
    memcpy( hash->secret, secret, secretlen );
    hash->secret_len = secretlen;

    if ((status = prepare_hash( hash )) != STATUS_SUCCESS)
    {
        heap_free( hash->secret );
        heap_free( hash );
        return status;
    }

    *handle = hash;
    return STATUS_SUCCESS;
}

NTSTATUS WINAPI BCryptDuplicateHash( BCRYPT_HASH_HANDLE handle, BCRYPT_HASH_HANDLE *handle_copy,
                                     UCHAR *object, ULONG objectlen, ULONG flags )
{
    struct hash *hash_orig = handle;
    struct hash *hash_copy;

    TRACE( "%p, %p, %p, %u, %u\n", handle, handle_copy, object, objectlen, flags );

    if (!hash_orig || hash_orig->hdr.magic != MAGIC_HASH) return STATUS_INVALID_HANDLE;
    if (!handle_copy) return STATUS_INVALID_PARAMETER;
    if (object) FIXME( "ignoring object buffer\n" );

    if (!(hash_copy = heap_alloc( sizeof(*hash_copy) )))
        return STATUS_NO_MEMORY;

    memcpy( hash_copy, hash_orig, sizeof(*hash_orig) );
    if (hash_orig->secret && !(hash_copy->secret = heap_alloc( hash_orig->secret_len )))
    {
        heap_free( hash_copy );
        return STATUS_NO_MEMORY;
    }
    memcpy( hash_copy->secret, hash_orig->secret, hash_orig->secret_len );

    *handle_copy = hash_copy;
    return STATUS_SUCCESS;
}

NTSTATUS WINAPI BCryptDestroyHash( BCRYPT_HASH_HANDLE handle )
{
    struct hash *hash = handle;

    TRACE( "%p\n", handle );

    if (!hash || hash->hdr.magic != MAGIC_HASH) return STATUS_INVALID_PARAMETER;
    hash->hdr.magic = 0;
    heap_free( hash->secret );
    heap_free( hash );
    return STATUS_SUCCESS;
}

NTSTATUS WINAPI BCryptHashData( BCRYPT_HASH_HANDLE handle, UCHAR *input, ULONG size, ULONG flags )
{
    struct hash *hash = handle;

    TRACE( "%p, %p, %u, %08x\n", handle, input, size, flags );

    if (!hash || hash->hdr.magic != MAGIC_HASH) return STATUS_INVALID_HANDLE;
    if (!input) return STATUS_SUCCESS;

    return hash_update( &hash->inner, hash->alg_id, input, size );
}

NTSTATUS WINAPI BCryptFinishHash( BCRYPT_HASH_HANDLE handle, UCHAR *output, ULONG size, ULONG flags )
{
    UCHAR buffer[MAX_HASH_OUTPUT_BYTES];
    struct hash *hash = handle;
    NTSTATUS status;
    int hash_length;

    TRACE( "%p, %p, %u, %08x\n", handle, output, size, flags );

    if (!hash || hash->hdr.magic != MAGIC_HASH) return STATUS_INVALID_HANDLE;
    if (!output) return STATUS_INVALID_PARAMETER;

    if (!(hash->flags & HASH_FLAG_HMAC))
    {
        if ((status = hash_finish( &hash->inner, hash->alg_id, output, size ))) return status;
        if (hash->flags & HASH_FLAG_REUSABLE) return prepare_hash( hash );
        return STATUS_SUCCESS;
    }

    hash_length = builtin_algorithms[hash->alg_id].hash_length;
    if ((status = hash_finish( &hash->inner, hash->alg_id, buffer, hash_length ))) return status;
    if ((status = hash_update( &hash->outer, hash->alg_id, buffer, hash_length ))) return status;
    if ((status = hash_finish( &hash->outer, hash->alg_id, output, size ))) return status;
    if (hash->flags & HASH_FLAG_REUSABLE) return prepare_hash( hash );
    return STATUS_SUCCESS;
}

NTSTATUS WINAPI BCryptHash( BCRYPT_ALG_HANDLE algorithm, UCHAR *secret, ULONG secretlen,
                            UCHAR *input, ULONG inputlen, UCHAR *output, ULONG outputlen )
{
    NTSTATUS status;
    BCRYPT_HASH_HANDLE handle;

    TRACE( "%p, %p, %u, %p, %u, %p, %u\n", algorithm, secret, secretlen,
           input, inputlen, output, outputlen );

    status = BCryptCreateHash( algorithm, &handle, NULL, 0, secret, secretlen, 0);
    if (status != STATUS_SUCCESS)
    {
        return status;
    }

    status = BCryptHashData( handle, input, inputlen, 0 );
    if (status != STATUS_SUCCESS)
    {
        BCryptDestroyHash( handle );
        return status;
    }

    status = BCryptFinishHash( handle, output, outputlen, 0 );
    if (status != STATUS_SUCCESS)
    {
        BCryptDestroyHash( handle );
        return status;
    }

    return BCryptDestroyHash( handle );
}

#if defined(HAVE_GNUTLS_CIPHER_INIT) || defined(HAVE_COMMONCRYPTO_COMMONCRYPTOR_H) && MAC_OS_X_VERSION_MAX_ALLOWED >= 1080
BOOL key_is_symmetric( struct key *key )
{
    return builtin_algorithms[key->alg_id].class == BCRYPT_CIPHER_INTERFACE;
}

BOOL is_zero_vector( const UCHAR *vector, ULONG len )
{
    ULONG i;
    if (!vector) return FALSE;
    for (i = 0; i < len; i++) if (vector[i]) return FALSE;
    return TRUE;
}

BOOL is_equal_vector( const UCHAR *vector, ULONG len, const UCHAR *vector2, ULONG len2 )
{
    if (!vector && !vector2) return TRUE;
    if (len != len2) return FALSE;
    return !memcmp( vector, vector2, len );
}

static NTSTATUS key_import( BCRYPT_ALG_HANDLE algorithm, const WCHAR *type, BCRYPT_KEY_HANDLE *key, UCHAR *object,
                            ULONG object_len, UCHAR *input, ULONG input_len )
{
    ULONG len;

    if (!strcmpW( type, BCRYPT_KEY_DATA_BLOB ))
    {
        BCRYPT_KEY_DATA_BLOB_HEADER *header = (BCRYPT_KEY_DATA_BLOB_HEADER *)input;

        if (input_len < sizeof(BCRYPT_KEY_DATA_BLOB_HEADER)) return STATUS_BUFFER_TOO_SMALL;
        if (header->dwMagic != BCRYPT_KEY_DATA_BLOB_MAGIC) return STATUS_INVALID_PARAMETER;
        if (header->dwVersion != BCRYPT_KEY_DATA_BLOB_VERSION1)
        {
            FIXME( "unknown key data blob version %u\n", header->dwVersion );
            return STATUS_INVALID_PARAMETER;
        }
        len = header->cbKeyData;
        if (len + sizeof(BCRYPT_KEY_DATA_BLOB_HEADER) > input_len) return STATUS_INVALID_PARAMETER;

        return BCryptGenerateSymmetricKey( algorithm, key, object, object_len, (UCHAR *)&header[1], len, 0 );
    }
    else if (!strcmpW( type, BCRYPT_OPAQUE_KEY_BLOB ))
    {
        if (input_len < sizeof(len)) return STATUS_BUFFER_TOO_SMALL;
        len = *(ULONG *)input;
        if (len + sizeof(len) > input_len) return STATUS_INVALID_PARAMETER;

        return BCryptGenerateSymmetricKey( algorithm, key, object, object_len, input + sizeof(len), len, 0 );
    }

    FIXME( "unsupported key type %s\n", debugstr_w(type) );
    return STATUS_NOT_IMPLEMENTED;
}

static NTSTATUS key_export( struct key *key, const WCHAR *type, UCHAR *output, ULONG output_len, ULONG *size )
{
    if (!strcmpW( type, BCRYPT_KEY_DATA_BLOB ))
    {
        BCRYPT_KEY_DATA_BLOB_HEADER *header = (BCRYPT_KEY_DATA_BLOB_HEADER *)output;
        ULONG req_size = sizeof(BCRYPT_KEY_DATA_BLOB_HEADER) + key->u.s.secret_len;

        *size = req_size;
        if (output_len < req_size) return STATUS_BUFFER_TOO_SMALL;

        header->dwMagic   = BCRYPT_KEY_DATA_BLOB_MAGIC;
        header->dwVersion = BCRYPT_KEY_DATA_BLOB_VERSION1;
        header->cbKeyData = key->u.s.secret_len;
        memcpy( &header[1], key->u.s.secret, key->u.s.secret_len );
        return STATUS_SUCCESS;
    }
    else if (!strcmpW( type, BCRYPT_OPAQUE_KEY_BLOB ))
    {
        ULONG len, req_size = sizeof(len) + key->u.s.secret_len;

        *size = req_size;
        if (output_len < req_size) return STATUS_BUFFER_TOO_SMALL;

        *(ULONG *)output = key->u.s.secret_len;
        memcpy( output + sizeof(len), key->u.s.secret, key->u.s.secret_len );
        return STATUS_SUCCESS;
    }
    else if (!strcmpW( type, BCRYPT_RSAPUBLIC_BLOB ) || !strcmpW( type, BCRYPT_DSA_PUBLIC_BLOB ) ||
             !strcmpW( type, BCRYPT_ECCPUBLIC_BLOB ))
    {
        *size = key->u.a.pubkey_len;
        if (output_len < key->u.a.pubkey_len) return STATUS_SUCCESS;

        memcpy( output, key->u.a.pubkey, key->u.a.pubkey_len );
        return STATUS_SUCCESS;
    }
    else if (!strcmpW( type, BCRYPT_ECCPRIVATE_BLOB ))
    {
        return key_export_ecc( key, output, output_len, size );
    }
    else if (!strcmpW( type, LEGACY_DSA_V2_PRIVATE_BLOB ))
    {
        return key_export_dsa_capi( key, output, output_len, size );
    }

    FIXME( "unsupported key type %s\n", debugstr_w(type) );
    return STATUS_NOT_IMPLEMENTED;
}

static NTSTATUS key_duplicate( struct key *key_orig, struct key *key_copy )
{
    UCHAR *buffer;

    memset( key_copy, 0, sizeof(*key_copy) );
    key_copy->hdr    = key_orig->hdr;
    key_copy->alg_id = key_orig->alg_id;

    if (key_is_symmetric( key_orig ))
    {
        if (!(buffer = heap_alloc( key_orig->u.s.secret_len ))) return STATUS_NO_MEMORY;
        memcpy( buffer, key_orig->u.s.secret, key_orig->u.s.secret_len );

        key_copy->u.s.mode       = key_orig->u.s.mode;
        key_copy->u.s.block_size = key_orig->u.s.block_size;
        key_copy->u.s.secret     = buffer;
        key_copy->u.s.secret_len = key_orig->u.s.secret_len;
    }
    else
    {
        if (!(buffer = heap_alloc( key_orig->u.a.pubkey_len ))) return STATUS_NO_MEMORY;
        memcpy( buffer, key_orig->u.a.pubkey, key_orig->u.a.pubkey_len );

        key_copy->u.a.pubkey     = buffer;
        key_copy->u.a.pubkey_len = key_orig->u.a.pubkey_len;
    }

    return STATUS_SUCCESS;
}

static NTSTATUS key_encrypt( struct key *key,  UCHAR *input, ULONG input_len, void *padding, UCHAR *iv,
                             ULONG iv_len, UCHAR *output, ULONG output_len, ULONG *ret_len, ULONG flags )
{
    ULONG bytes_left = input_len;
    UCHAR *buf, *src, *dst;
    NTSTATUS status;

    if (key->u.s.mode == MODE_ID_GCM)
    {
        BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO *auth_info = padding;

        if (!auth_info) return STATUS_INVALID_PARAMETER;
        if (!auth_info->pbNonce) return STATUS_INVALID_PARAMETER;
        if (!auth_info->pbTag) return STATUS_INVALID_PARAMETER;
        if (auth_info->cbTag < 12 || auth_info->cbTag > 16) return STATUS_INVALID_PARAMETER;
        if (auth_info->dwFlags & BCRYPT_AUTH_MODE_CHAIN_CALLS_FLAG)
            FIXME( "call chaining not implemented\n" );

        if ((status = key_symmetric_set_vector( key, auth_info->pbNonce, auth_info->cbNonce )))
            return status;

        *ret_len = input_len;
        if (flags & BCRYPT_BLOCK_PADDING) return STATUS_INVALID_PARAMETER;
        if (input && !output) return STATUS_SUCCESS;
        if (output_len < *ret_len) return STATUS_BUFFER_TOO_SMALL;

        if ((status = key_symmetric_set_auth_data( key, auth_info->pbAuthData, auth_info->cbAuthData )))
            return status;
        if ((status = key_symmetric_encrypt( key, input, input_len, output, output_len ))) return status;

        return key_symmetric_get_tag( key, auth_info->pbTag, auth_info->cbTag );
    }

    *ret_len = input_len;

    if (flags & BCRYPT_BLOCK_PADDING)
        *ret_len = (input_len + key->u.s.block_size) & ~(key->u.s.block_size - 1);
    else if (input_len & (key->u.s.block_size - 1))
        return STATUS_INVALID_BUFFER_SIZE;

    if (!output) return STATUS_SUCCESS;
    if (output_len < *ret_len) return STATUS_BUFFER_TOO_SMALL;
    if (key->u.s.mode == MODE_ID_ECB && iv) return STATUS_INVALID_PARAMETER;
    if ((status = key_symmetric_set_vector( key, iv, iv_len ))) return status;

    src = input;
    dst = output;
    while (bytes_left >= key->u.s.block_size)
    {
        if ((status = key_symmetric_encrypt( key, src, key->u.s.block_size, dst, key->u.s.block_size )))
            return status;
        if (key->u.s.mode == MODE_ID_ECB && (status = key_symmetric_set_vector( key, NULL, 0 ))) return status;
        bytes_left -= key->u.s.block_size;
        src += key->u.s.block_size;
        dst += key->u.s.block_size;
    }

    if (flags & BCRYPT_BLOCK_PADDING)
    {
        if (!(buf = heap_alloc( key->u.s.block_size ))) return STATUS_NO_MEMORY;
        memcpy( buf, src, bytes_left );
        memset( buf + bytes_left, key->u.s.block_size - bytes_left, key->u.s.block_size - bytes_left );
        status = key_symmetric_encrypt( key, buf, key->u.s.block_size, dst, key->u.s.block_size );
        heap_free( buf );
    }

    return status;
}

static NTSTATUS key_decrypt( struct key *key, UCHAR *input, ULONG input_len, void *padding, UCHAR *iv,
                             ULONG iv_len, UCHAR *output, ULONG output_len, ULONG *ret_len, ULONG flags )
{
    ULONG bytes_left = input_len;
    UCHAR *buf, *src, *dst;
    NTSTATUS status;

    if (key->u.s.mode == MODE_ID_GCM)
    {
        BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO *auth_info = padding;
        UCHAR tag[16];

        if (!auth_info) return STATUS_INVALID_PARAMETER;
        if (!auth_info->pbNonce) return STATUS_INVALID_PARAMETER;
        if (!auth_info->pbTag) return STATUS_INVALID_PARAMETER;
        if (auth_info->cbTag < 12 || auth_info->cbTag > 16) return STATUS_INVALID_PARAMETER;

        if ((status = key_symmetric_set_vector( key, auth_info->pbNonce, auth_info->cbNonce )))
            return status;

        *ret_len = input_len;
        if (flags & BCRYPT_BLOCK_PADDING) return STATUS_INVALID_PARAMETER;
        if (!output) return STATUS_SUCCESS;
        if (output_len < *ret_len) return STATUS_BUFFER_TOO_SMALL;

        if ((status = key_symmetric_set_auth_data( key, auth_info->pbAuthData, auth_info->cbAuthData )))
            return status;
        if ((status = key_symmetric_decrypt( key, input, input_len, output, output_len ))) return status;

        if ((status = key_symmetric_get_tag( key, tag, sizeof(tag) ))) return status;
        if (memcmp( tag, auth_info->pbTag, auth_info->cbTag )) return STATUS_AUTH_TAG_MISMATCH;

        return STATUS_SUCCESS;
    }

    *ret_len = input_len;

    if (input_len & (key->u.s.block_size - 1)) return STATUS_INVALID_BUFFER_SIZE;
    if (!output) return STATUS_SUCCESS;
    if (flags & BCRYPT_BLOCK_PADDING)
    {
        if (output_len + key->u.s.block_size < *ret_len) return STATUS_BUFFER_TOO_SMALL;
        if (input_len < key->u.s.block_size) return STATUS_BUFFER_TOO_SMALL;
        bytes_left -= key->u.s.block_size;
    }
    else if (output_len < *ret_len) return STATUS_BUFFER_TOO_SMALL;

    if (key->u.s.mode == MODE_ID_ECB && iv) return STATUS_INVALID_PARAMETER;
    if ((status = key_symmetric_set_vector( key, iv, iv_len ))) return status;

    src = input;
    dst = output;
    while (bytes_left >= key->u.s.block_size)
    {
        if ((status = key_symmetric_decrypt( key, src, key->u.s.block_size, dst, key->u.s.block_size )))
            return status;
        if (key->u.s.mode == MODE_ID_ECB && (status = key_symmetric_set_vector( key, NULL, 0 ))) return status;
        bytes_left -= key->u.s.block_size;
        src += key->u.s.block_size;
        dst += key->u.s.block_size;
    }

    if (flags & BCRYPT_BLOCK_PADDING)
    {
        if (!(buf = heap_alloc( key->u.s.block_size ))) return STATUS_NO_MEMORY;
        status = key_symmetric_decrypt( key, src, key->u.s.block_size, buf, key->u.s.block_size );
        if (!status && buf[ key->u.s.block_size - 1 ] <= key->u.s.block_size)
        {
            *ret_len -= buf[ key->u.s.block_size - 1 ];
            if (output_len < *ret_len) status = STATUS_BUFFER_TOO_SMALL;
            else memcpy( dst, buf, key->u.s.block_size - buf[ key->u.s.block_size - 1 ] );
        }
        else status = STATUS_UNSUCCESSFUL; /* FIXME: invalid padding */
        heap_free( buf );
    }

    return status;
}

static NTSTATUS key_import_pair( struct algorithm *alg, const WCHAR *type, BCRYPT_KEY_HANDLE *ret_key, UCHAR *input,
                                 ULONG input_len )
{
    struct key *key;
    NTSTATUS status;

    if (!strcmpW( type, BCRYPT_ECCPUBLIC_BLOB ))
    {
        BCRYPT_ECCKEY_BLOB *ecc_blob = (BCRYPT_ECCKEY_BLOB *)input;
        DWORD key_size, magic, size;

        if (input_len < sizeof(*ecc_blob)) return STATUS_INVALID_PARAMETER;

        switch (alg->id)
        {
        case ALG_ID_ECDH_P256:
            key_size = 32;
            magic = BCRYPT_ECDH_PUBLIC_P256_MAGIC;
            break;

        case ALG_ID_ECDSA_P256:
            key_size = 32;
            magic = BCRYPT_ECDSA_PUBLIC_P256_MAGIC;
            break;

        case ALG_ID_ECDSA_P384:
            key_size = 48;
            magic = BCRYPT_ECDSA_PUBLIC_P384_MAGIC;
            break;

        default:
            FIXME( "algorithm %u does not yet support importing blob of type %s\n", alg->id, debugstr_w(type) );
            return STATUS_NOT_SUPPORTED;
        }

        if (ecc_blob->dwMagic != magic) return STATUS_INVALID_PARAMETER;
        if (ecc_blob->cbKey != key_size || input_len < sizeof(*ecc_blob) + ecc_blob->cbKey * 2)
            return STATUS_INVALID_PARAMETER;

        if (!(key = heap_alloc_zero( sizeof(*key) ))) return STATUS_NO_MEMORY;
        key->hdr.magic = MAGIC_KEY;

        size = sizeof(*ecc_blob) + ecc_blob->cbKey * 2;
        if ((status = key_asymmetric_init( key, alg, key_size * 8, (BYTE *)ecc_blob, size )))
        {
            heap_free( key );
            return status;
        }

        *ret_key = key;
        return STATUS_SUCCESS;
    }
    else if (!strcmpW( type, BCRYPT_ECCPRIVATE_BLOB ))
    {
        BCRYPT_ECCKEY_BLOB *ecc_blob = (BCRYPT_ECCKEY_BLOB *)input;
        DWORD key_size, magic;

        if (input_len < sizeof(*ecc_blob)) return STATUS_INVALID_PARAMETER;

        switch (alg->id)
        {
        case ALG_ID_ECDH_P256:
            key_size = 32;
            magic = BCRYPT_ECDH_PRIVATE_P256_MAGIC;
            break;
        case ALG_ID_ECDSA_P256:
            key_size = 32;
            magic = BCRYPT_ECDSA_PRIVATE_P256_MAGIC;
            break;

        default:
            FIXME( "algorithm %u does not yet support importing blob of type %s\n", alg->id, debugstr_w(type) );
            return STATUS_NOT_SUPPORTED;
        }

        if (ecc_blob->dwMagic != magic) return STATUS_INVALID_PARAMETER;
        if (ecc_blob->cbKey != key_size || input_len < sizeof(*ecc_blob) + ecc_blob->cbKey * 3)
            return STATUS_INVALID_PARAMETER;

        if (!(key = heap_alloc_zero( sizeof(*key) ))) return STATUS_NO_MEMORY;
        key->hdr.magic = MAGIC_KEY;

        if ((status = key_asymmetric_init( key, alg, key_size * 8, NULL, 0 )))
        {
            heap_free( key );
            return status;
        }
        if ((status = key_import_ecc( key, input, input_len )))
        {
            heap_free( key );
            return status;
        }

        *ret_key = key;
        return STATUS_SUCCESS;
    }
    else if (!strcmpW( type, BCRYPT_RSAPUBLIC_BLOB ))
    {
        BCRYPT_RSAKEY_BLOB *rsa_blob = (BCRYPT_RSAKEY_BLOB *)input;
        ULONG size;

        if (input_len < sizeof(*rsa_blob)) return STATUS_INVALID_PARAMETER;
        if ((alg->id != ALG_ID_RSA && alg->id != ALG_ID_RSA_SIGN) || rsa_blob->Magic != BCRYPT_RSAPUBLIC_MAGIC)
            return STATUS_NOT_SUPPORTED;

        if (!(key = heap_alloc_zero( sizeof(*key) ))) return STATUS_NO_MEMORY;
        key->hdr.magic = MAGIC_KEY;

        size = sizeof(*rsa_blob) + rsa_blob->cbPublicExp + rsa_blob->cbModulus;
        if ((status = key_asymmetric_init( key, alg, rsa_blob->BitLength, (BYTE *)rsa_blob, size )))
        {
            heap_free( key );
            return status;
        }

        *ret_key = key;
        return STATUS_SUCCESS;
    }
    else if (!strcmpW( type, BCRYPT_DSA_PUBLIC_BLOB ))
    {
        BCRYPT_DSA_KEY_BLOB *dsa_blob = (BCRYPT_DSA_KEY_BLOB *)input;
        ULONG size;

        if (input_len < sizeof(*dsa_blob)) return STATUS_INVALID_PARAMETER;
        if ((alg->id != ALG_ID_DSA) || dsa_blob->dwMagic != BCRYPT_DSA_PUBLIC_MAGIC)
            return STATUS_NOT_SUPPORTED;

        if (!(key = heap_alloc_zero( sizeof(*key) ))) return STATUS_NO_MEMORY;
        key->hdr.magic = MAGIC_KEY;

        size = sizeof(*dsa_blob) + dsa_blob->cbKey * 3;
        if ((status = key_asymmetric_init( key, alg, dsa_blob->cbKey * 8, (BYTE *)dsa_blob, size )))
        {
            heap_free( key );
            return status;
        }

        *ret_key = key;
        return STATUS_SUCCESS;
    }
    else if (!strcmpW( type, LEGACY_DSA_V2_PRIVATE_BLOB ))
    {
        BLOBHEADER *hdr = (BLOBHEADER *)input;
        DSSPUBKEY *pubkey;

        if (input_len < sizeof(*hdr)) return STATUS_INVALID_PARAMETER;

        if (hdr->bType != PRIVATEKEYBLOB && hdr->bVersion != 2 && hdr->aiKeyAlg != CALG_DSS_SIGN)
        {
            FIXME( "blob type %u version %u alg id %u not supported\n", hdr->bType, hdr->bVersion, hdr->aiKeyAlg );
            return STATUS_NOT_SUPPORTED;
        }
        if (alg->id != ALG_ID_DSA)
        {
            FIXME( "algorithm %u does not support importing blob of type %s\n", alg->id, debugstr_w(type) );
            return STATUS_NOT_SUPPORTED;
        }

        if (input_len < sizeof(*hdr) + sizeof(*pubkey)) return STATUS_INVALID_PARAMETER;
        pubkey = (DSSPUBKEY *)(hdr + 1);
        if (pubkey->magic != MAGIC_DSS2) return STATUS_NOT_SUPPORTED;

        if (input_len < sizeof(*hdr) + sizeof(*pubkey) + (pubkey->bitlen / 8) * 2 + 40 + sizeof(DSSSEED))
            return STATUS_INVALID_PARAMETER;

        if (!(key = heap_alloc_zero( sizeof(*key) ))) return STATUS_NO_MEMORY;
        key->hdr.magic = MAGIC_KEY;

        if ((status = key_asymmetric_init( key, alg, pubkey->bitlen, NULL, 0 )))
        {
            heap_free( key );
            return status;
        }
        if ((status = key_import_dsa_capi( key, input, input_len )))
        {
            heap_free( key );
            return status;
        }

        *ret_key = key;
        return STATUS_SUCCESS;
    }

    FIXME( "unsupported key type %s\n", debugstr_w(type) );
    return STATUS_NOT_SUPPORTED;
}
#else
NTSTATUS key_symmetric_init( struct key *key, struct algorithm *alg, const UCHAR *secret, ULONG secret_len )
{
    ERR( "support for keys not available at build time\n" );
    return STATUS_NOT_IMPLEMENTED;
}

BOOL key_is_symmetric( struct key *key )
{
    ERR( "support for keys not available at build time\n" );
    return FALSE;
}

NTSTATUS key_set_property( struct key *key, const WCHAR *prop, UCHAR *value, ULONG size, ULONG flags )
{
    ERR( "support for keys not available at build time\n" );
    return STATUS_NOT_IMPLEMENTED;
}

static NTSTATUS key_duplicate( struct key *key_orig, struct key *key_copy )
{
    ERR( "support for keys not available at build time\n" );
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS key_asymmetric_init( struct key *key, struct algorithm *alg, ULONG bitlen, const UCHAR *pubkey,
                              ULONG pubkey_len )
{
    ERR( "support for keys not available at build time\n" );
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS key_asymmetric_generate( struct key *key )
{
    ERR( "support for keys not available at build time\n" );
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS key_asymmetric_sign( struct key *key, void *padding, UCHAR *input, ULONG input_len, UCHAR *output,
                              ULONG output_len, ULONG *ret_len, ULONG flags )
{
    ERR( "support for keys not available at build time\n" );
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS key_asymmetric_verify( struct key *key, void *padding, UCHAR *hash, ULONG hash_len, UCHAR *signature,
                                ULONG signature_len, DWORD flags )
{
    ERR( "support for keys not available at build time\n" );
    return STATUS_NOT_IMPLEMENTED;
}

static NTSTATUS key_import( BCRYPT_ALG_HANDLE algorithm, const WCHAR *type, BCRYPT_KEY_HANDLE *key, UCHAR *object,
                            ULONG object_len, UCHAR *input, ULONG input_len )
{
    ERR( "support for keys not available at build time\n" );
    return STATUS_NOT_IMPLEMENTED;
}

static NTSTATUS key_export( struct key *key, const WCHAR *type, UCHAR *output, ULONG output_len, ULONG *size )
{
    ERR( "support for keys not available at build time\n" );
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS key_destroy( struct key *key )
{
    ERR( "support for keys not available at build time\n" );
    return STATUS_NOT_IMPLEMENTED;
}

static NTSTATUS key_encrypt( struct key *key,  UCHAR *input, ULONG input_len, void *padding, UCHAR *iv,
                             ULONG iv_len, UCHAR *output, ULONG output_len, ULONG *ret_len, ULONG flags )
{
    ERR( "support for keys not available at build time\n" );
    return STATUS_NOT_IMPLEMENTED;
}

static NTSTATUS key_decrypt( struct key *key, UCHAR *input, ULONG input_len, void *padding, UCHAR *iv,
                             ULONG iv_len, UCHAR *output, ULONG output_len, ULONG *ret_len, ULONG flags )
{
    ERR( "support for keys not available at build time\n" );
    return STATUS_NOT_IMPLEMENTED;
}

static NTSTATUS key_import_pair( struct algorithm *alg, const WCHAR *type, BCRYPT_KEY_HANDLE *ret_key, UCHAR *input,
                                 ULONG input_len )
{
    ERR( "support for keys not available at build time\n" );
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS key_export_ecc( struct key *key, UCHAR *output, ULONG len, ULONG *ret_len )
{
    ERR( "support for keys not available at build time\n" );
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS key_import_ecc( struct key *key, UCHAR *input, ULONG len )
{
    ERR( "support for keys not available at build time\n" );
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS compute_secret_ecc (struct key *pubkey_in, struct key *privkey_in, struct secret *secret)
{
    ERR( "support for secrets not available at build time\n" );
    return STATUS_NOT_IMPLEMENTED;
}
#endif

NTSTATUS WINAPI BCryptGenerateSymmetricKey( BCRYPT_ALG_HANDLE algorithm, BCRYPT_KEY_HANDLE *handle,
                                            UCHAR *object, ULONG object_len, UCHAR *secret, ULONG secret_len,
                                            ULONG flags )
{
    struct algorithm *alg = algorithm;
    struct key *key;
    NTSTATUS status;

    TRACE( "%p, %p, %p, %u, %p, %u, %08x\n", algorithm, handle, object, object_len, secret, secret_len, flags );

    if (!alg || alg->hdr.magic != MAGIC_ALG) return STATUS_INVALID_HANDLE;
    if (object) FIXME( "ignoring object buffer\n" );

    if (!(key = heap_alloc( sizeof(*key) ))) return STATUS_NO_MEMORY;
    key->hdr.magic = MAGIC_KEY;

    if ((status = key_symmetric_init( key, alg, secret, secret_len )))
    {
        heap_free( key );
        return status;
    }

    *handle = key;
    return STATUS_SUCCESS;
}

NTSTATUS WINAPI BCryptGenerateKeyPair( BCRYPT_ALG_HANDLE algorithm, BCRYPT_KEY_HANDLE *handle, ULONG key_len,
                                       ULONG flags )
{
    struct algorithm *alg = algorithm;
    struct key *key;
    NTSTATUS status;

    TRACE( "%p, %p, %u, %08x\n", algorithm, handle, key_len, flags );

    if (!alg || alg->hdr.magic != MAGIC_ALG) return STATUS_INVALID_HANDLE;
    if (!handle) return STATUS_INVALID_PARAMETER;

    if (!(key = heap_alloc_zero( sizeof(*key) ))) return STATUS_NO_MEMORY;
    key->hdr.magic = MAGIC_KEY;

    if ((status = key_asymmetric_init( key, alg, key_len, NULL, 0 )))
    {
        heap_free( key );
        return status;
    }

    *handle = key;
    return STATUS_SUCCESS;
}

NTSTATUS WINAPI BCryptFinalizeKeyPair( BCRYPT_KEY_HANDLE handle, ULONG flags )
{
    struct key *key = handle;

    TRACE( "%p, %08x\n", key, flags );
    if (!key || key->hdr.magic != MAGIC_KEY) return STATUS_INVALID_HANDLE;

    return key_asymmetric_generate( key );
}

NTSTATUS WINAPI BCryptImportKey( BCRYPT_ALG_HANDLE algorithm, BCRYPT_KEY_HANDLE decrypt_key, LPCWSTR type,
                                 BCRYPT_KEY_HANDLE *key, PUCHAR object, ULONG object_len, PUCHAR input,
                                 ULONG input_len, ULONG flags )
{
    struct algorithm *alg = algorithm;

    TRACE("%p, %p, %s, %p, %p, %u, %p, %u, %u\n", algorithm, decrypt_key, debugstr_w(type), key, object,
          object_len, input, input_len, flags);

    if (!alg || alg->hdr.magic != MAGIC_ALG) return STATUS_INVALID_HANDLE;
    if (!key || !type || !input) return STATUS_INVALID_PARAMETER;

    if (decrypt_key)
    {
        FIXME( "decryption of key not yet supported\n" );
        return STATUS_NOT_IMPLEMENTED;
    }

    return key_import( algorithm, type, key, object, object_len, input, input_len );
}

NTSTATUS WINAPI BCryptExportKey( BCRYPT_KEY_HANDLE export_key, BCRYPT_KEY_HANDLE encrypt_key, LPCWSTR type,
                                 PUCHAR output, ULONG output_len, ULONG *size, ULONG flags )
{
    struct key *key = export_key;

    TRACE("%p, %p, %s, %p, %u, %p, %u\n", key, encrypt_key, debugstr_w(type), output, output_len, size, flags);

    if (!key || key->hdr.magic != MAGIC_KEY) return STATUS_INVALID_HANDLE;
    if (!type || !size) return STATUS_INVALID_PARAMETER;

    if (encrypt_key)
    {
        FIXME( "encryption of key not yet supported\n" );
        return STATUS_NOT_IMPLEMENTED;
    }

    return key_export( key, type, output, output_len, size );
}

NTSTATUS WINAPI BCryptDuplicateKey( BCRYPT_KEY_HANDLE handle, BCRYPT_KEY_HANDLE *handle_copy,
                                    UCHAR *object, ULONG object_len, ULONG flags )
{
    struct key *key_orig = handle;
    struct key *key_copy;
    NTSTATUS status;

    TRACE( "%p, %p, %p, %u, %08x\n", handle, handle_copy, object, object_len, flags );
    if (object) FIXME( "ignoring object buffer\n" );

    if (!key_orig || key_orig->hdr.magic != MAGIC_KEY) return STATUS_INVALID_HANDLE;
    if (!handle_copy) return STATUS_INVALID_PARAMETER;
    if (!(key_copy = heap_alloc( sizeof(*key_copy) ))) return STATUS_NO_MEMORY;

    if ((status = key_duplicate( key_orig, key_copy )))
    {
        heap_free( key_copy );
        return status;
    }

    *handle_copy = key_copy;
    return STATUS_SUCCESS;
}

NTSTATUS WINAPI BCryptImportKeyPair( BCRYPT_ALG_HANDLE algorithm, BCRYPT_KEY_HANDLE decrypt_key, const WCHAR *type,
                                     BCRYPT_KEY_HANDLE *ret_key, UCHAR *input, ULONG input_len, ULONG flags )
{
    struct algorithm *alg = algorithm;

    TRACE( "%p, %p, %s, %p, %p, %u, %08x\n", algorithm, decrypt_key, debugstr_w(type), ret_key, input,
           input_len, flags );

    if (!alg || alg->hdr.magic != MAGIC_ALG) return STATUS_INVALID_HANDLE;
    if (!ret_key || !type || !input) return STATUS_INVALID_PARAMETER;
    if (decrypt_key)
    {
        FIXME( "decryption of key not yet supported\n" );
        return STATUS_NOT_IMPLEMENTED;
    }

    return key_import_pair( alg, type, ret_key, input, input_len );
}

NTSTATUS WINAPI BCryptSignHash( BCRYPT_KEY_HANDLE handle, void *padding, UCHAR *input, ULONG input_len,
                                UCHAR *output, ULONG output_len, ULONG *ret_len, ULONG flags )
{
    struct key *key = handle;

    TRACE( "%p, %p, %p, %u, %p, %u, %p, %08x\n", handle, padding, input, input_len, output, output_len,
           ret_len, flags );

    if (!key || key->hdr.magic != MAGIC_KEY) return STATUS_INVALID_HANDLE;
    if (key_is_symmetric( key ))
    {
        FIXME( "signing with symmetric keys not yet supported\n" );
        return STATUS_NOT_IMPLEMENTED;
    }

    return key_asymmetric_sign( key, padding, input, input_len, output, output_len, ret_len, flags );
}

NTSTATUS WINAPI BCryptVerifySignature( BCRYPT_KEY_HANDLE handle, void *padding, UCHAR *hash, ULONG hash_len,
                                       UCHAR *signature, ULONG signature_len, ULONG flags )
{
    struct key *key = handle;

    TRACE( "%p, %p, %p, %u, %p, %u, %08x\n", handle, padding, hash, hash_len, signature, signature_len, flags );

    if (!key || key->hdr.magic != MAGIC_KEY) return STATUS_INVALID_HANDLE;
    if (!hash || !hash_len || !signature || !signature_len) return STATUS_INVALID_PARAMETER;
    if (key_is_symmetric( key )) return STATUS_NOT_SUPPORTED;

    return key_asymmetric_verify( key, padding, hash, hash_len, signature, signature_len, flags );
}

NTSTATUS WINAPI BCryptDestroyKey( BCRYPT_KEY_HANDLE handle )
{
    struct key *key = handle;

    TRACE( "%p\n", handle );

    if (!key || key->hdr.magic != MAGIC_KEY) return STATUS_INVALID_HANDLE;
    key->hdr.magic = 0;
    return key_destroy( key );
}

NTSTATUS WINAPI BCryptEncrypt( BCRYPT_KEY_HANDLE handle, UCHAR *input, ULONG input_len, void *padding, UCHAR *iv,
                               ULONG iv_len, UCHAR *output, ULONG output_len, ULONG *ret_len, ULONG flags )
{
    struct key *key = handle;

    TRACE( "%p, %p, %u, %p, %p, %u, %p, %u, %p, %08x\n", handle, input, input_len, padding, iv, iv_len, output,
           output_len, ret_len, flags );

    if (!key || key->hdr.magic != MAGIC_KEY) return STATUS_INVALID_HANDLE;
    if (!key_is_symmetric( key ))
    {
        FIXME( "encryption with asymmetric keys not yet supported\n" );
        return STATUS_NOT_IMPLEMENTED;
    }
    if (flags & ~BCRYPT_BLOCK_PADDING)
    {
        FIXME( "flags %08x not implemented\n", flags );
        return STATUS_NOT_IMPLEMENTED;
    }

    return key_encrypt( key, input, input_len, padding, iv, iv_len, output, output_len, ret_len, flags );
}

NTSTATUS WINAPI BCryptDecrypt( BCRYPT_KEY_HANDLE handle, UCHAR *input, ULONG input_len, void *padding, UCHAR *iv,
                               ULONG iv_len, UCHAR *output, ULONG output_len, ULONG *ret_len, ULONG flags )
{
    struct key *key = handle;

    TRACE( "%p, %p, %u, %p, %p, %u, %p, %u, %p, %08x\n", handle, input, input_len, padding, iv, iv_len, output,
           output_len, ret_len, flags );

    if (!key || key->hdr.magic != MAGIC_KEY) return STATUS_INVALID_HANDLE;
    if (!key_is_symmetric( key ))
    {
        FIXME( "decryption with asymmetric keys not yet supported\n" );
        return STATUS_NOT_IMPLEMENTED;
    }
    if (flags & ~BCRYPT_BLOCK_PADDING)
    {
        FIXME( "flags %08x not supported\n", flags );
        return STATUS_NOT_IMPLEMENTED;
    }

    return key_decrypt( key, input, input_len, padding, iv, iv_len, output, output_len, ret_len, flags );
}

NTSTATUS WINAPI BCryptSetProperty( BCRYPT_HANDLE handle, const WCHAR *prop, UCHAR *value, ULONG size, ULONG flags )
{
    struct object *object = handle;

    TRACE( "%p, %s, %p, %u, %08x\n", handle, debugstr_w(prop), value, size, flags );

    if (!object) return STATUS_INVALID_HANDLE;

    switch (object->magic)
    {
    case MAGIC_ALG:
    {
        struct algorithm *alg = (struct algorithm *)object;
        return set_alg_property( alg, prop, value, size, flags );
    }
    case MAGIC_KEY:
    {
        struct key *key = (struct key *)object;
        return key_set_property( key, prop, value, size, flags );
    }
    default:
        WARN( "unknown magic %08x\n", object->magic );
        return STATUS_INVALID_HANDLE;
    }
}

#define HMAC_PAD_LEN 64
NTSTATUS WINAPI BCryptDeriveKeyCapi( BCRYPT_HASH_HANDLE handle, BCRYPT_ALG_HANDLE halg, UCHAR *key, ULONG keylen, ULONG flags )
{
    struct hash *hash = handle;
    UCHAR buf[MAX_HASH_OUTPUT_BYTES * 2];
    NTSTATUS status;
    ULONG len;

    TRACE( "%p, %p, %p, %u, %08x\n", handle, halg, key, keylen, flags );

    if (!key || !keylen) return STATUS_INVALID_PARAMETER;
    if (!hash || hash->hdr.magic != MAGIC_HASH) return STATUS_INVALID_HANDLE;
    if (keylen > builtin_algorithms[hash->alg_id].hash_length * 2) return STATUS_INVALID_PARAMETER;

    if (halg)
    {
        FIXME( "algorithm handle not supported\n" );
        return STATUS_NOT_IMPLEMENTED;
    }

    len = builtin_algorithms[hash->alg_id].hash_length;
    if ((status = BCryptFinishHash( handle, buf, len, 0 ))) return status;

    if (len < keylen)
    {
        UCHAR pad1[HMAC_PAD_LEN], pad2[HMAC_PAD_LEN];
        ULONG i;

        for (i = 0; i < sizeof(pad1); i++)
        {
            pad1[i] = 0x36 ^ (i < len ? buf[i] : 0);
            pad2[i] = 0x5c ^ (i < len ? buf[i] : 0);
        }

        if ((status = prepare_hash( hash )) ||
            (status = BCryptHashData( handle, pad1, sizeof(pad1), 0 )) ||
            (status = BCryptFinishHash( handle, buf, len, 0 ))) return status;

        if ((status = prepare_hash( hash )) ||
            (status = BCryptHashData( handle, pad2, sizeof(pad2), 0 )) ||
            (status = BCryptFinishHash( handle, buf + len, len, 0 ))) return status;
    }

    memcpy( key, buf, keylen );
    return STATUS_SUCCESS;
}

static NTSTATUS pbkdf2( BCRYPT_HASH_HANDLE handle, UCHAR *pwd, ULONG pwd_len, UCHAR *salt, ULONG salt_len,
                        ULONGLONG iterations, ULONG i, UCHAR *dst, ULONG hash_len )
{
    NTSTATUS status = STATUS_INVALID_PARAMETER;
    UCHAR bytes[4], *buf;
    ULONG j, k;

    if (!(buf = heap_alloc( hash_len ))) return STATUS_NO_MEMORY;

    for (j = 0; j < iterations; j++)
    {
        if (j == 0)
        {
            /* use salt || INT(i) */
            status = BCryptHashData( handle, salt, salt_len, 0 );
            if (status != STATUS_SUCCESS)
            {
                heap_free( buf );
                return status;
            }
            bytes[0] = (i >> 24) & 0xff;
            bytes[1] = (i >> 16) & 0xff;
            bytes[2] = (i >> 8) & 0xff;
            bytes[3] = i & 0xff;
            status = BCryptHashData( handle, bytes, 4, 0 );
        }
        else status = BCryptHashData( handle, buf, hash_len, 0 ); /* use U_j */
        if (status != STATUS_SUCCESS)
        {
            heap_free( buf );
            return status;
        }

        status = BCryptFinishHash( handle, buf, hash_len, 0 );
        if (status != STATUS_SUCCESS)
        {
            heap_free( buf );
            return status;
        }

        if (j == 0) memcpy( dst, buf, hash_len );
        else for (k = 0; k < hash_len; k++) dst[k] ^= buf[k];
    }

    heap_free( buf );
    return status;
}

NTSTATUS WINAPI BCryptDeriveKeyPBKDF2( BCRYPT_ALG_HANDLE handle, UCHAR *pwd, ULONG pwd_len, UCHAR *salt, ULONG salt_len,
                                       ULONGLONG iterations, UCHAR *dk, ULONG dk_len, ULONG flags )
{
    struct algorithm *alg = handle;
    ULONG hash_len, block_count, bytes_left, i;
    BCRYPT_HASH_HANDLE hash;
    UCHAR *partial;
    NTSTATUS status;

    TRACE( "%p, %p, %u, %p, %u, %s, %p, %u, %08x\n", handle, pwd, pwd_len, salt, salt_len,
           wine_dbgstr_longlong(iterations), dk, dk_len, flags );

    if (!alg || alg->hdr.magic != MAGIC_ALG) return STATUS_INVALID_HANDLE;

    hash_len = builtin_algorithms[alg->id].hash_length;
    if (dk_len <= 0 || dk_len > ((((ULONGLONG)1) << 32) - 1) * hash_len) return STATUS_INVALID_PARAMETER;

    block_count = 1 + ((dk_len - 1) / hash_len); /* ceil(dk_len / hash_len) */
    bytes_left = dk_len - (block_count - 1) * hash_len;

    status = BCryptCreateHash( handle, &hash, NULL, 0, pwd, pwd_len, BCRYPT_HASH_REUSABLE_FLAG );
    if (status != STATUS_SUCCESS)
        return status;

    /* full blocks */
    for (i = 1; i < block_count; i++)
    {
        status = pbkdf2( hash, pwd, pwd_len, salt, salt_len, iterations, i, dk + ((i - 1) * hash_len), hash_len );
        if (status != STATUS_SUCCESS)
        {
            BCryptDestroyHash( hash );
            return status;
        }
    }

    /* final partial block */
    if (!(partial = heap_alloc( hash_len )))
    {
        BCryptDestroyHash( hash );
        return STATUS_NO_MEMORY;
    }

    status = pbkdf2( hash, pwd, pwd_len, salt, salt_len, iterations, block_count, partial, hash_len );
    if (status != STATUS_SUCCESS)
    {
        BCryptDestroyHash( hash );
        heap_free( partial );
        return status;
    }
    memcpy( dk + ((block_count - 1) * hash_len), partial, bytes_left );

    BCryptDestroyHash( hash );
    heap_free( partial );
    return STATUS_SUCCESS;
}

NTSTATUS WINAPI BCryptSecretAgreement(BCRYPT_KEY_HANDLE privatekey, BCRYPT_KEY_HANDLE publickey, BCRYPT_SECRET_HANDLE *handle, ULONG flags)
{
    struct key *privkey = privatekey;
    struct key *pubkey = publickey;
    struct secret *secret;
    NTSTATUS status;

    TRACE( "%p, %p, %p, %08x\n", privatekey, publickey, handle, flags );

    if (!privkey || privkey->hdr.magic != MAGIC_KEY) return STATUS_INVALID_HANDLE;
    if (!pubkey || pubkey->hdr.magic != MAGIC_KEY) return STATUS_INVALID_HANDLE;
    if (!handle) return STATUS_INVALID_PARAMETER;

    if (!(secret = heap_alloc_zero( sizeof(*secret) ))) return STATUS_NO_MEMORY;
    secret->hdr.magic = MAGIC_SECRET;

    if ((status = compute_secret_ecc( privkey, pubkey, secret )))
    {
        heap_free( secret );
        *handle = NULL;
    }
    else
    {
        *handle = secret;
    }

    return STATUS_SUCCESS;
}

NTSTATUS WINAPI BCryptDestroySecret(BCRYPT_SECRET_HANDLE handle)
{
    struct secret *secret = handle;

    TRACE( "%p\n", handle );

    if (!secret || secret->hdr.magic != MAGIC_SECRET) return STATUS_INVALID_HANDLE;
    secret->hdr.magic = 0;
    heap_free( secret->data );
    heap_free( secret );
    return STATUS_SUCCESS;
}

NTSTATUS WINAPI BCryptDeriveKey(BCRYPT_SECRET_HANDLE handle, LPCWSTR kdf, BCryptBufferDesc *parameter,
        PUCHAR derived, ULONG derived_size, ULONG *result, ULONG flags)
{
    struct secret *secret = handle;

    TRACE( "%p, %s, %p, %p, %d, %p, %08x\n", secret, debugstr_w(kdf), parameter, derived, derived_size, result, flags );

    if (!secret || secret->hdr.magic != MAGIC_SECRET) return STATUS_INVALID_HANDLE;
    if (!kdf) return STATUS_INVALID_PARAMETER;

    if (flags) FIXME("flags ignored: %08x\n", flags);

    if (!(strcmpW( kdf, BCRYPT_KDF_HASH )))
    {
        unsigned int i;
        BCryptBuffer *hash_algorithm = NULL;
        BCryptBuffer *secret_prepend = NULL;
        BCryptBuffer *secret_append = NULL;
        enum alg_id hash_alg_id;
        ULONG hash_length;
        struct hash_impl hash;
        NTSTATUS status;

        if (parameter)
        {
            for (i = 0; i < parameter->cBuffers; i++)
            {
                BCryptBuffer *cur_buffer = &parameter->pBuffers[i];
                switch(cur_buffer->BufferType)
                {
                case KDF_HASH_ALGORITHM:
                    if (hash_algorithm)
                        FIXME("Duplicate KDF_HASH_ALGORITHM, untested\n");
                    hash_algorithm = cur_buffer;
                    break;
                case KDF_SECRET_PREPEND:
                    if (secret_prepend)
                        FIXME("Multiple prefixes unsupported\n");
                    secret_prepend = cur_buffer;
                    break;
                case KDF_SECRET_APPEND:
                    if (secret_append)
                        FIXME("Multiple suffixes unsupported\n");
                    secret_append = cur_buffer;
                    break;
                default:
                    FIXME("Unsupported BCRYPT_KDF_HASH parameter type %x\n", cur_buffer->BufferType);
                    break;
                }
            }
        }

        if (!(hash_algorithm))
            hash_alg_id = ALG_ID_SHA1;
        else
        {
            for (i = 0; i < ARRAY_SIZE( builtin_algorithms ); i++)
            {
                if (!strcmpW( hash_algorithm->pvBuffer, builtin_algorithms[i].name))
                {
                    hash_alg_id = i;
                    break;
                }
            }
            if (i == ARRAY_SIZE(builtin_algorithms))
            {
                WARN("Algorithm %s not found\n", debugstr_w(hash_algorithm->pvBuffer));
                return STATUS_NOT_SUPPORTED;
            }
            if (builtin_algorithms[hash_alg_id].class != BCRYPT_HASH_INTERFACE)
            {
                return STATUS_NOT_SUPPORTED;
            }
        }

        hash_length = builtin_algorithms[hash_alg_id].hash_length;

        if (!derived)
        {
            *result = hash_length;
            return STATUS_SUCCESS;
        }

        if ((status = hash_init(&hash, hash_alg_id)))
        {
            return status;
        }

        if (secret_prepend)
        {
            hash_update(&hash, hash_alg_id, secret_prepend->pvBuffer, secret_prepend->cbBuffer);
        }

        hash_update(&hash, hash_alg_id, secret->data, secret->len);

        if (secret_append)
        {
            hash_update(&hash, hash_alg_id, secret_append->pvBuffer, secret_append->cbBuffer);
        }

        if (derived_size >= hash_length)
        {
            hash_finish(&hash, hash_alg_id, derived, derived_size);
            *result = hash_length;
        }
        else
        {
            UCHAR *output = heap_alloc(hash_length);
            hash_finish(&hash, hash_alg_id, output, hash_length);
            memcpy(derived, output, derived_size);
            heap_free(output);
            *result = derived_size;
        }

        return STATUS_SUCCESS;
    }
    else if (!(strcmpW( kdf, BCRYPT_KDF_RAW_SECRET )))
    {
        ULONG n;
        ULONG secret_length = secret->len;

        if (!derived)
        {
            *result = secret_length;
            return STATUS_SUCCESS;
        }

        /* outputs in little endian for some reason */
        for (n = 0; n < secret_length && n < derived_size; n++)
        {
            derived[n] = secret->data[secret_length - n - 1];
        }

        *result = n;
        return STATUS_SUCCESS;
    }
    FIXME( "Derivation function %s not supported.\n", debugstr_w(kdf) );
    return STATUS_NOT_IMPLEMENTED;
}

BOOL WINAPI DllMain( HINSTANCE hinst, DWORD reason, LPVOID reserved )
{
    switch (reason)
    {
    case DLL_PROCESS_ATTACH:
        instance = hinst;
        DisableThreadLibraryCalls( hinst );
#ifdef HAVE_GNUTLS_CIPHER_INIT
        gnutls_initialize();
#ifdef SONAME_LIBGCRYPT
        gcrypt_initialize();
#endif
#endif
        break;

    case DLL_PROCESS_DETACH:
        if (reserved) break;
#ifdef HAVE_GNUTLS_CIPHER_INIT
        gnutls_uninitialize();
#ifdef SONAME_LIBGCRYPT
    gcrypt_uninitialize();
#endif
#endif
        break;
    }
    return TRUE;
}
