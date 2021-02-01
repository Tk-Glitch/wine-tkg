/*
 * Copyright 2008 Maarten Lankhorst
 * Copyright 2020 Hans Leidekker for CodeWeavers
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

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winbase.h"
#include "wincrypt.h"
#include "winreg.h"
#include "bcrypt.h"
#include "objbase.h"
#include "rpcproxy.h"
#include "ntsecapi.h"

#include "wine/debug.h"
#include "wine/heap.h"

static HINSTANCE instance;

WINE_DEFAULT_DEBUG_CHANNEL(dssenh);

#define MAGIC_KEY (('K' << 24) | ('E' << 16) | ('Y' << 8) | '0')
struct key
{
    DWORD             magic;
    DWORD             algid;
    DWORD             flags;
    BCRYPT_ALG_HANDLE alg_handle;
    BCRYPT_KEY_HANDLE handle;
};

#define MAGIC_CONTAINER (('C' << 24) | ('O' << 16) | ('N' << 8) | 'T')
struct container
{
    DWORD       magic;
    DWORD       flags;
    struct key *exch_key;
    struct key *sign_key;
    char        name[MAX_PATH];
};

#define MAGIC_HASH (('H' << 24) | ('A' << 16) | ('S' << 8) | 'H')
struct hash
{
    DWORD              magic;
    BCRYPT_HASH_HANDLE handle;
    DWORD              len;
    UCHAR              value[64];
    BOOL               finished;
};

static const char dss_path_fmt[] = "Software\\Wine\\Crypto\\DSS\\%s";

static BOOL create_container_regkey( struct container *container, REGSAM sam, HKEY *hkey )
{
    char path[sizeof(dss_path_fmt) + MAX_PATH];
    HKEY rootkey;

    sprintf( path, dss_path_fmt, container->name );

    if (container->flags & CRYPT_MACHINE_KEYSET)
        rootkey = HKEY_LOCAL_MACHINE;
    else
        rootkey = HKEY_CURRENT_USER;

    /* @@ Wine registry key: HKLM\Software\Wine\Crypto\DSS */
    /* @@ Wine registry key: HKCU\Software\Wine\Crypto\DSS */
    return !RegCreateKeyExA( rootkey, path, 0, NULL, REG_OPTION_NON_VOLATILE, sam, NULL, hkey, NULL );
}

static struct container *create_key_container( const char *name, DWORD flags )
{
    struct container *ret;

    if (!(ret = heap_alloc_zero( sizeof(*ret) ))) return NULL;
    ret->magic = MAGIC_CONTAINER;
    ret->flags = flags;
    if (name) strcpy( ret->name, name );

    if (!(flags & CRYPT_VERIFYCONTEXT))
    {
        HKEY hkey;
        if (create_container_regkey( ret, KEY_WRITE, &hkey )) RegCloseKey( hkey );
    }
    return ret;
}

static BOOL open_container_regkey( const char *name, DWORD flags, REGSAM access, HKEY *hkey )
{
    char path[sizeof(dss_path_fmt) + MAX_PATH];
    HKEY rootkey;

    sprintf( path, dss_path_fmt, name );

    if (flags & CRYPT_MACHINE_KEYSET)
        rootkey = HKEY_LOCAL_MACHINE;
    else
        rootkey = HKEY_CURRENT_USER;

    /* @@ Wine registry key: HKLM\Software\Wine\Crypto\DSS */
    /* @@ Wine registry key: HKCU\Software\Wine\Crypto\DSS */
    return !RegOpenKeyExA( rootkey, path, 0, access, hkey );
}

static const WCHAR *map_keyspec_to_keypair_name( DWORD keyspec )
{
    const WCHAR *name;

    switch (keyspec)
    {
    case AT_KEYEXCHANGE:
        name = L"KeyExchangeKeyPair";
        break;
    case AT_SIGNATURE:
        name = L"SignatureKeyPair";
        break;
    default:
        ERR( "invalid key spec %u\n", keyspec );
        return NULL;
    }
    return name;
}

static struct key *create_key( ALG_ID algid, DWORD flags )
{
    struct key *ret;
    const WCHAR *alg;

    switch (algid)
    {
    case AT_SIGNATURE:
    case CALG_DSS_SIGN:
        alg = BCRYPT_DSA_ALGORITHM;
        break;

    default:
        FIXME( "unhandled algorithm %08x\n", algid );
        return NULL;
    }

    if (!(ret = heap_alloc_zero( sizeof(*ret) ))) return NULL;

    ret->magic = MAGIC_KEY;
    ret->algid = algid;
    ret->flags = flags;
    if (BCryptOpenAlgorithmProvider( &ret->alg_handle, alg, MS_PRIMITIVE_PROVIDER, 0 ))
    {
        heap_free( ret );
        return NULL;
    }
    return ret;
}

static void destroy_key( struct key *key )
{
    if (!key) return;
    BCryptDestroyKey( key->handle );
    BCryptCloseAlgorithmProvider( key->alg_handle, 0 );
    key->magic = 0;
    heap_free( key );
}

static struct key *import_key( DWORD keyspec, BYTE *data, DWORD len )
{
    struct key *ret;

    if (!(ret = create_key( keyspec, 0 ))) return NULL;

    if (BCryptImportKeyPair( ret->alg_handle, NULL, LEGACY_DSA_V2_PRIVATE_BLOB, &ret->handle, data, len, 0 ))
    {
        WARN( "failed to import key\n" );
        destroy_key( ret );
        return NULL;
    }
    return ret;
}

static struct key *read_key( HKEY hkey, DWORD keyspec, DWORD flags )
{
    const WCHAR *value;
    DWORD type, len;
    BYTE *data;
    DATA_BLOB blob_in, blob_out;
    struct key *ret = NULL;

    if (!(value = map_keyspec_to_keypair_name( keyspec ))) return NULL;
    if (RegQueryValueExW( hkey, value, 0, &type, NULL, &len )) return NULL;
    if (!(data = heap_alloc( len ))) return NULL;

    if (!RegQueryValueExW( hkey, value, 0, &type, data, &len ))
    {
        blob_in.pbData = data;
        blob_in.cbData = len;
        if (CryptUnprotectData( &blob_in, NULL, NULL, NULL, NULL, flags, &blob_out ))
        {
            ret = import_key( keyspec, blob_out.pbData, blob_out.cbData );
            LocalFree( blob_out.pbData );
        }
    }

    heap_free( data );
    return ret;
}

static void destroy_container( struct container *container )
{
    if (!container) return;
    destroy_key( container->exch_key );
    destroy_key( container->sign_key );
    container->magic = 0;
    heap_free( container );
}

static struct container *read_key_container( const char *name, DWORD flags )
{
    DWORD protect_flags = (flags & CRYPT_MACHINE_KEYSET) ? CRYPTPROTECT_LOCAL_MACHINE : 0;
    struct container *ret;
    HKEY hkey;

    if (!open_container_regkey( name, flags, KEY_READ, &hkey )) return NULL;

    if ((ret = create_key_container( name, flags )))
    {
        ret->exch_key = read_key( hkey, AT_KEYEXCHANGE, protect_flags );
        ret->sign_key = read_key( hkey, AT_SIGNATURE, protect_flags );
    }

    RegCloseKey( hkey );
    return ret;
}

static void delete_key_container( const char *name, DWORD flags )
{
    char path[sizeof(dss_path_fmt) + MAX_PATH];
    HKEY rootkey;

    sprintf( path, dss_path_fmt, name );

    if (flags & CRYPT_MACHINE_KEYSET)
        rootkey = HKEY_LOCAL_MACHINE;
    else
        rootkey = HKEY_CURRENT_USER;

    /* @@ Wine registry key: HKLM\Software\Wine\Crypto\DSS */
    /* @@ Wine registry key: HKCU\Software\Wine\Crypto\DSS */
    RegDeleteKeyExA( rootkey, path, 0, 0 );
}

BOOL WINAPI CPAcquireContext( HCRYPTPROV *ret_prov, LPSTR container, DWORD flags, PVTableProvStruc vtable )
{
    struct container *ret;
    char name[MAX_PATH];

    TRACE( "%p, %s, %08x, %p\n", ret_prov, debugstr_a(container), flags, vtable );

    if (container && *container)
    {
        if (lstrlenA( container ) >= sizeof(name)) return FALSE;
        lstrcpyA( name, container );
    }
    else
    {
        DWORD len = sizeof(name);
        if (!GetUserNameA( name, &len )) return FALSE;
    }

    switch (flags)
    {
    case 0:
    case 0 | CRYPT_MACHINE_KEYSET:
        ret = read_key_container( name, flags );
        break;

    case CRYPT_NEWKEYSET:
    case CRYPT_NEWKEYSET | CRYPT_MACHINE_KEYSET:
        if ((ret = read_key_container( name, flags )))
        {
            heap_free( ret );
            SetLastError( NTE_EXISTS );
            return FALSE;
        }
        ret = create_key_container( name, flags );
        break;

    case CRYPT_VERIFYCONTEXT:
    case CRYPT_VERIFYCONTEXT | CRYPT_MACHINE_KEYSET:
        ret = create_key_container( "", flags );
        break;

    case CRYPT_DELETEKEYSET:
    case CRYPT_DELETEKEYSET | CRYPT_MACHINE_KEYSET:
        delete_key_container( name, flags );
        *ret_prov = 0;
        return TRUE;

    default:
        FIXME( "unsupported flags %08x\n", flags );
        return FALSE;
    }

    if (!ret) return FALSE;
    *ret_prov = (HCRYPTPROV)ret;
    return TRUE;
}

BOOL WINAPI CPReleaseContext( HCRYPTPROV hprov, DWORD flags )
{
    struct container *container = (struct container *)hprov;

    TRACE( "%p, %08x\n", (void *)hprov, flags );

    if (container->magic != MAGIC_CONTAINER) return FALSE;
    destroy_container( container );
    return TRUE;
}

BOOL WINAPI CPGetProvParam( HCRYPTPROV hprov, DWORD param, BYTE *data, DWORD *len, DWORD flags )
{
    return FALSE;
}

static BOOL store_key_pair( struct key *key, HKEY hkey, DWORD keyspec, DWORD flags )
{
    const WCHAR *value;
    DATA_BLOB blob_in, blob_out;
    DWORD len;
    BYTE *data;
    BOOL ret = TRUE;

    if (!key) return TRUE;
    if (!(value = map_keyspec_to_keypair_name( keyspec ))) return FALSE;

    if (BCryptExportKey( key->handle, NULL, LEGACY_DSA_V2_PRIVATE_BLOB, NULL, 0, &len, 0 )) return FALSE;
    if (!(data = heap_alloc( len ))) return FALSE;

    if (!BCryptExportKey( key->handle, NULL, LEGACY_DSA_V2_PRIVATE_BLOB, data, len, &len, 0 ))
    {
        blob_in.pbData = data;
        blob_in.cbData = len;
        if ((ret = CryptProtectData( &blob_in, NULL, NULL, NULL, NULL, flags, &blob_out )))
        {
            ret = !RegSetValueExW( hkey, value, 0, REG_BINARY, blob_out.pbData, blob_out.cbData );
            LocalFree( blob_out.pbData );
        }
    }

    heap_free( data );
    return ret;
}

static BOOL store_key_container_keys( struct container *container )
{
    HKEY hkey;
    DWORD flags;
    BOOL ret;

    if (container->flags & CRYPT_MACHINE_KEYSET)
        flags = CRYPTPROTECT_LOCAL_MACHINE;
    else
        flags = 0;

    if (!create_container_regkey( container, KEY_WRITE, &hkey )) return FALSE;

    ret = store_key_pair( container->exch_key, hkey, AT_KEYEXCHANGE, flags );
    if (ret) store_key_pair( container->sign_key, hkey, AT_SIGNATURE, flags );
    RegCloseKey( hkey );
    return ret;
}

static struct key *duplicate_key( const struct key *key )
{
    struct key *ret;

    if (!(ret = create_key( key->algid, key->flags ))) return NULL;

    if (BCryptDuplicateKey( key->handle, &ret->handle, NULL, 0, 0 ))
    {
        heap_free( ret );
        return NULL;
    }
    return ret;
}

static BOOL generate_key( struct container *container, ALG_ID algid, DWORD bitlen, DWORD flags, HCRYPTKEY *ret_key )
{
    struct key *key, *sign_key;
    NTSTATUS status;

    if (!(key = create_key( algid, flags ))) return FALSE;

    if ((status = BCryptGenerateKeyPair( key->alg_handle, &key->handle, bitlen, 0 )))
    {
        ERR( "failed to generate key %08x\n", status );
        destroy_key( key );
        return FALSE;
    }
    if ((status = BCryptFinalizeKeyPair( key->handle, 0 )))
    {
        ERR( "failed to finalize key %08x\n", status );
        destroy_key( key );
        return FALSE;
    }

    switch (algid)
    {
    case AT_SIGNATURE:
    case CALG_DSS_SIGN:
        if (!(sign_key = duplicate_key( key )))
        {
            destroy_key( key );
            return FALSE;
        }
        destroy_key( container->sign_key );
        container->sign_key = sign_key;
        break;

    default:
        FIXME( "unhandled algorithm %08x\n", algid );
        return FALSE;
    }

    if (!store_key_container_keys( container )) return FALSE;

    *ret_key = (HCRYPTKEY)key;
    return TRUE;
}

BOOL WINAPI CPGenKey( HCRYPTPROV hprov, ALG_ID algid, DWORD flags, HCRYPTKEY *ret_key )
{
    static const unsigned int supported_key_lengths[] = { 512, 768, 1024 };
    struct container *container = (struct container *)hprov;
    ULONG i, bitlen = HIWORD(flags) ? HIWORD(flags) : 1024;

    TRACE( "%p, %08x, %08x, %p\n", (void *)hprov, algid, flags, ret_key );

    if (container->magic != MAGIC_CONTAINER) return FALSE;

    if (bitlen % 2)
    {
        SetLastError( STATUS_INVALID_PARAMETER );
        return FALSE;
    }
    for (i = 0; i < ARRAY_SIZE(supported_key_lengths); i++)
    {
        if (bitlen == supported_key_lengths[i]) break;
    }
    if (i >= ARRAY_SIZE(supported_key_lengths))
    {
        SetLastError( NTE_BAD_FLAGS );
        return FALSE;
    }

    return generate_key( container, algid, bitlen, LOWORD(flags), ret_key );
}

BOOL WINAPI CPDestroyKey( HCRYPTPROV hprov, HCRYPTKEY hkey )
{
    struct key *key = (struct key *)hkey;

    TRACE( "%p, %p\n", (void *)hprov, (void *)hkey );

    if (key->magic != MAGIC_KEY)
    {
        SetLastError( NTE_BAD_KEY );
        return FALSE;
    }

    destroy_key( key );
    return TRUE;
}

#define MAGIC_DSS1 ('D' | ('S' << 8) | ('S' << 16) | ('1' << 24))
#define MAGIC_DSS2 ('D' | ('S' << 8) | ('S' << 16) | ('2' << 24))
#define MAGIC_DSS3 ('D' | ('S' << 8) | ('S' << 16) | ('3' << 24))

static BOOL import_key_dss2( struct container *container, ALG_ID algid, const BYTE *data, DWORD len, DWORD flags,
                             HCRYPTKEY *ret_key )
{
    const BLOBHEADER *hdr = (const BLOBHEADER *)data;
    const DSSPUBKEY *pubkey = (const DSSPUBKEY *)(hdr + 1);
    const WCHAR *type;
    struct key *key, *exch_key, *sign_key;
    NTSTATUS status;

    if (len < sizeof(*hdr) + sizeof(*pubkey)) return FALSE;

    switch (pubkey->magic)
    {
    case MAGIC_DSS1:
        type = LEGACY_DSA_V2_PUBLIC_BLOB;
        break;

    case MAGIC_DSS2:
        type = LEGACY_DSA_V2_PRIVATE_BLOB;
        break;

    default:
        FIXME( "unsupported key magic %08x\n", pubkey->magic );
        return FALSE;
    }

    if (!(key = create_key( CALG_DSS_SIGN, flags ))) return FALSE;

    if ((status = BCryptImportKeyPair( key->alg_handle, NULL, type, &key->handle, (UCHAR *)data, len, 0 )))
    {
        TRACE( "failed to import key %08x\n", status );
        destroy_key( key );
        return FALSE;
    }

    if (!wcscmp(type, LEGACY_DSA_V2_PRIVATE_BLOB))
    {
        switch (algid)
        {
        case AT_KEYEXCHANGE:
        case CALG_DH_SF:
            if (!(exch_key = duplicate_key( key )))
            {
                destroy_key( key );
                return FALSE;
            }
            destroy_key( container->exch_key );
            container->exch_key = exch_key;
            break;

        case AT_SIGNATURE:
        case CALG_DSS_SIGN:
            if (!(sign_key = duplicate_key( key )))
            {
                destroy_key( key );
                return FALSE;
            }
            destroy_key( container->sign_key );
            container->sign_key = sign_key;
            break;

        default:
            FIXME( "unhandled key algorithm %u\n", algid );
            destroy_key( key );
            return FALSE;
        }

        if (!store_key_container_keys( container )) return FALSE;
    }

    *ret_key = (HCRYPTKEY)key;
    return TRUE;
}

static BOOL import_key_dss3( struct container *container, ALG_ID algid, const BYTE *data, DWORD len, DWORD flags,
                             HCRYPTKEY *ret_key )
{
    const BLOBHEADER *hdr = (const BLOBHEADER *)data;
    const DSSPUBKEY_VER3 *pubkey = (const DSSPUBKEY_VER3 *)(hdr + 1);
    BCRYPT_DSA_KEY_BLOB *blob;
    struct key *key;
    BYTE *src, *dst;
    ULONG i, size, size_q;
    NTSTATUS status;

    if (len < sizeof(*hdr) + sizeof(*pubkey)) return FALSE;

    switch (pubkey->magic)
    {
    case MAGIC_DSS3:
        break;

    default:
        FIXME( "unsupported key magic %08x\n", pubkey->magic );
        return FALSE;
    }

    if ((size_q = pubkey->bitlenQ / 8) > sizeof(blob->q))
    {
        FIXME( "q too large\n" );
        return FALSE;
    }

    if (!(key = create_key( CALG_DSS_SIGN, flags ))) return FALSE;

    size = sizeof(*blob) + (pubkey->bitlenP / 8) * 3;
    if (!(blob = heap_alloc_zero( size )))
    {
        destroy_key( key );
        return FALSE;
    }
    blob->dwMagic = BCRYPT_DSA_PUBLIC_MAGIC;
    blob->cbKey   = pubkey->bitlenP / 8;
    memcpy( blob->Count, &pubkey->DSSSeed.counter, sizeof(blob->Count) );
    memcpy( blob->Seed, pubkey->DSSSeed.seed, sizeof(blob->Seed) );

    /* q */
    src = (BYTE *)(pubkey + 1) + blob->cbKey;
    for (i = 0; i < size_q; i++) blob->q[i] = src[size_q - i - 1];

    /* p */
    src -= blob->cbKey;
    dst = (BYTE *)(blob + 1);
    for (i = 0; i < blob->cbKey; i++) dst[i] = src[blob->cbKey - i - 1];

    /* g */
    src += blob->cbKey + size_q;
    dst += blob->cbKey;
    for (i = 0; i < blob->cbKey; i++) dst[i] = src[blob->cbKey - i - 1];

    /* y */
    src += blob->cbKey + pubkey->bitlenJ / 8;
    dst += blob->cbKey;
    for (i = 0; i < blob->cbKey; i++) dst[i] = src[blob->cbKey - i - 1];

    if ((status = BCryptImportKeyPair( key->alg_handle, NULL, BCRYPT_DSA_PUBLIC_BLOB, &key->handle, (UCHAR *)blob,
                                       size, 0 )))
    {
        WARN( "failed to import key %08x\n", status );
        destroy_key( key );
        heap_free( blob );
        return FALSE;
    }

    heap_free( blob );
    *ret_key = (HCRYPTKEY)key;
    return TRUE;
}

BOOL WINAPI CPImportKey( HCRYPTPROV hprov, const BYTE *data, DWORD len, HCRYPTKEY hpubkey, DWORD flags,
                         HCRYPTKEY *ret_key )
{
    struct container *container = (struct container *)hprov;
    const BLOBHEADER *hdr;
    BOOL ret;

    TRACE( "%p, %p, %u, %p, %08x, %p\n", (void *)hprov, data, len, (void *)hpubkey, flags, ret_key );

    if (container->magic != MAGIC_CONTAINER) return FALSE;
    if (len < sizeof(*hdr)) return FALSE;

    hdr = (const BLOBHEADER *)data;
    if ((hdr->bType != PRIVATEKEYBLOB && hdr->bType != PUBLICKEYBLOB) || hdr->aiKeyAlg != CALG_DSS_SIGN)
    {
        FIXME( "bType %u aiKeyAlg %08x not supported\n", hdr->bType, hdr->aiKeyAlg );
        return FALSE;
    }

    switch (hdr->bVersion)
    {
    case 2:
        ret = import_key_dss2( container, hdr->aiKeyAlg, data, len, flags, ret_key );
        break;

    case 3:
        ret = import_key_dss3( container, hdr->aiKeyAlg, data, len, flags, ret_key );
        break;

    default:
        FIXME( "version %u not supported\n", hdr->bVersion );
        return FALSE;
    }

    return ret;
}

BOOL WINAPI CPExportKey( HCRYPTPROV hprov, HCRYPTKEY hkey, HCRYPTKEY hexpkey, DWORD blobtype, DWORD flags,
                         BYTE *data, DWORD *len )
{
    struct key *key = (struct key *)hkey;
    const WCHAR *type;

    TRACE( "%p, %p, %p, %08x, %08x, %p, %p\n", (void *)hprov, (void *)hkey, (void *)hexpkey, blobtype, flags,
           data, len );

    if (key->magic != MAGIC_KEY) return FALSE;
    if (hexpkey)
    {
        FIXME( "export key not supported\n" );
        return FALSE;
    }
    if (flags)
    {
        FIXME( "flags %08x not supported\n", flags );
        return FALSE;
    }

    switch (blobtype)
    {
    case PUBLICKEYBLOB:
        type = LEGACY_DSA_V2_PUBLIC_BLOB;
        break;

    case PRIVATEKEYBLOB:
        type = LEGACY_DSA_V2_PRIVATE_BLOB;
        break;

    default:
        FIXME( "blob type %u not supported\n", blobtype );
        return FALSE;
    }

    return !BCryptExportKey( key->handle, NULL, type, data, *len, len, 0 );
}

BOOL WINAPI CPDuplicateKey( HCRYPTPROV hprov, HCRYPTKEY hkey, DWORD *reserved, DWORD flags, HCRYPTKEY *ret_key )
{
    struct key *key = (struct key *)hkey, *ret;

    TRACE( "%p, %p, %p, %08x, %p\n", (void *)hprov, (void *)hkey, reserved, flags, ret_key );

    if (key->magic != MAGIC_KEY) return FALSE;

    if (!(ret = duplicate_key( key ))) return FALSE;
    *ret_key = (HCRYPTKEY)ret;
    return TRUE;
}

BOOL WINAPI CPGetUserKey( HCRYPTPROV hprov, DWORD keyspec, HCRYPTKEY *ret_key )
{
    struct container *container = (struct container *)hprov;
    BOOL ret = FALSE;

    TRACE( "%p, %08x, %p\n", (void *)hprov, keyspec, ret_key );

    if (container->magic != MAGIC_CONTAINER) return FALSE;

    switch (keyspec)
    {
    case AT_KEYEXCHANGE:
        if (!container->exch_key) SetLastError( NTE_NO_KEY );
        else if ((*ret_key = (HCRYPTKEY)duplicate_key( container->exch_key ))) ret = TRUE;
        break;

    case AT_SIGNATURE:
        if (!container->sign_key) SetLastError( NTE_NO_KEY );
        else if ((*ret_key = (HCRYPTKEY)duplicate_key( container->sign_key ))) ret = TRUE;
        break;

    default:
        SetLastError( NTE_NO_KEY );
        return FALSE;
    }

    return ret;
}

BOOL WINAPI CPGenRandom( HCRYPTPROV hprov, DWORD len, BYTE *buffer )
{
    struct container *container = (struct container *)hprov;

    TRACE( "%p, %u, %p\n", (void *)hprov, len, buffer );

    if (container->magic != MAGIC_CONTAINER) return FALSE;

    return RtlGenRandom( buffer, len );
}

static struct hash *create_hash( ALG_ID algid )
{
    struct hash *ret;
    BCRYPT_ALG_HANDLE alg_handle;
    const WCHAR *alg;
    DWORD len;

    switch (algid)
    {
    case CALG_MD5:
        alg = BCRYPT_MD5_ALGORITHM;
        len = 16;
        break;

    case CALG_SHA1:
        alg = BCRYPT_SHA1_ALGORITHM;
        len = 20;
        break;

    default:
        FIXME( "unhandled algorithm %u\n", algid );
        return NULL;
    }

    if (!(ret = heap_alloc_zero( sizeof(*ret) ))) return NULL;

    ret->magic = MAGIC_HASH;
    ret->len   = len;
    if (BCryptOpenAlgorithmProvider( &alg_handle, alg, MS_PRIMITIVE_PROVIDER, 0 ))
    {
        heap_free( ret );
        return NULL;
    }
    if (BCryptCreateHash( alg_handle, &ret->handle, NULL, 0, NULL, 0, 0 ))
    {
        BCryptCloseAlgorithmProvider( alg_handle, 0 );
        heap_free( ret );
        return NULL;
    }

    BCryptCloseAlgorithmProvider( alg_handle, 0 );
    return ret;
}

BOOL WINAPI CPCreateHash( HCRYPTPROV hprov, ALG_ID algid, HCRYPTKEY hkey, DWORD flags, HCRYPTHASH *ret_hash )
{
    struct hash *hash;

    TRACE( "%p, %08x, %p, %08x, %p\n", (void *)hprov, algid, (void *)hkey, flags, ret_hash );

    switch (algid)
    {
    case CALG_MD5:
    case CALG_SHA1:
        break;

    default:
        FIXME( "algorithm %u not supported\n", algid );
        SetLastError( NTE_BAD_ALGID );
        return FALSE;
    }

    if (!(hash = create_hash( algid ))) return FALSE;

    *ret_hash = (HCRYPTHASH)hash;
    return TRUE;
}

static void destroy_hash( struct hash *hash )
{
    if (!hash) return;
    BCryptDestroyHash( hash->handle );
    hash->magic = 0;
    heap_free( hash );
}

BOOL WINAPI CPDestroyHash( HCRYPTPROV hprov, HCRYPTHASH hhash )
{
    struct hash *hash = (struct hash *)hhash;

    TRACE( "%p, %p\n", (void *)hprov, (void *)hhash);

    if (hash->magic != MAGIC_HASH)
    {
        SetLastError( NTE_BAD_HASH );
        return FALSE;
    }

    destroy_hash( hash );
    return TRUE;
}

static struct hash *duplicate_hash( const struct hash *hash )
{
    struct hash *ret;

    if (!(ret = heap_alloc( sizeof(*ret) ))) return NULL;

    ret->magic = hash->magic;
    ret->len   = hash->len;
    if (BCryptDuplicateHash( hash->handle, &ret->handle, NULL, 0, 0 ))
    {
        heap_free( ret );
        return NULL;
    }
    memcpy( ret->value, hash->value, sizeof(hash->value) );
    ret->finished = hash->finished;
    return ret;
}

BOOL WINAPI CPDuplicateHash( HCRYPTPROV hprov, HCRYPTHASH hhash, DWORD *reserved, DWORD flags, HCRYPTHASH *ret_hash )
{
    struct hash *hash = (struct hash *)hhash, *ret;

    TRACE( "%p, %p, %p, %08x, %p\n", (void *)hprov, (void *)hhash, reserved, flags, ret_hash );

    if (hash->magic != MAGIC_HASH) return FALSE;

    if (!(ret = duplicate_hash( hash ))) return FALSE;
    *ret_hash = (HCRYPTHASH)ret;
    return TRUE;
}

BOOL WINAPI CPHashData( HCRYPTPROV hprov, HCRYPTHASH hhash, const BYTE *data, DWORD len, DWORD flags )
{
    struct hash *hash = (struct hash *)hhash;

    TRACE("%p, %p, %p, %u, %08x\n", (void *)hprov, (void *)hhash, data, len, flags );

    if (hash->magic != MAGIC_HASH) return FALSE;

    if (hash->finished)
    {
        SetLastError( NTE_BAD_HASH_STATE );
        return FALSE;
    }
    return !BCryptHashData( hash->handle, (UCHAR *)data, len, 0 );
}

BOOL WINAPI CPGetHashParam( HCRYPTPROV hprov, HCRYPTHASH hhash, DWORD param, BYTE *data, DWORD *len, DWORD flags )
{
    struct hash *hash = (struct hash *)hhash;

    TRACE( "%p, %p, %08x, %p, %p, %08x\n", (void *)hprov, (void *)hhash, param, data, len, flags );

    if (hash->magic != MAGIC_HASH) return FALSE;

    switch (param)
    {
    case HP_HASHSIZE:
        if (sizeof(hash->len) > *len)
        {
            *len = sizeof(hash->len);
            SetLastError( ERROR_MORE_DATA );
            return FALSE;
        }
        *(DWORD *)data = hash->len;
        *len = sizeof(hash->len);
        return TRUE;

    case HP_HASHVAL:
        if (!hash->finished)
        {
            if (BCryptFinishHash( hash->handle, hash->value, hash->len, 0 )) return FALSE;
            hash->finished = TRUE;
        }
        if (hash->len > *len)
        {
            *len = hash->len;
            SetLastError( ERROR_MORE_DATA );
            return FALSE;
        }
        memcpy( data, hash->value, hash->len );
        *len = hash->len;
        return TRUE;

    default:
        SetLastError( NTE_BAD_TYPE );
        return FALSE;
    }
}

BOOL WINAPI CPSetHashParam( HCRYPTPROV hprov, HCRYPTHASH hhash, DWORD param, const BYTE *data, DWORD flags )
{
    struct hash *hash = (struct hash *)hhash;

    TRACE( "%p, %p, %08x, %p, %08x\n", (void *)hprov, (void *)hhash, param, data, flags );

    if (hash->magic != MAGIC_HASH) return FALSE;

    switch (param)
    {
    case HP_HASHVAL:
        memcpy( hash->value, data, hash->len );
        return TRUE;

    default:
        FIXME( "param %u not supported\n", param );
        SetLastError( NTE_BAD_TYPE );
        return FALSE;
    }
}

BOOL WINAPI CPDeriveKey( HCRYPTPROV hprov, ALG_ID algid, HCRYPTHASH hhash, DWORD flags, HCRYPTKEY *ret_key )
{
    return FALSE;
}

static DWORD get_signature_length( DWORD algid )
{
    switch (algid)
    {
    case AT_SIGNATURE:
    case CALG_DSS_SIGN: return 40;
    default:
        FIXME( "unhandled algorithm %u\n", algid );
        return 0;
    }
}

#define MAX_HASH_LEN 20
BOOL WINAPI CPSignHash( HCRYPTPROV hprov, HCRYPTHASH hhash, DWORD keyspec, const WCHAR *desc, DWORD flags, BYTE *sig,
                        DWORD *siglen )
{
    struct container *container = (struct container *)hprov;
    struct hash *hash = (struct hash *)hhash;
    ULONG len;

    TRACE( "%p, %p, %u, %s, %08x, %p, %p\n", (void *)hprov, (void *)hhash, keyspec, debugstr_w(desc), flags, sig,
           siglen );

    if (container->magic != MAGIC_CONTAINER || !container->sign_key) return FALSE;
    if (hash->magic != MAGIC_HASH) return FALSE;

    if (!(len = get_signature_length( container->sign_key->algid ))) return FALSE;
    if (*siglen < len)
    {
        *siglen = len;
        return TRUE;
    }

    return !BCryptSignHash( container->sign_key->handle, NULL, hash->value, hash->len, sig, *siglen, siglen, 0 );
}

BOOL WINAPI CPVerifySignature( HCRYPTPROV hprov, HCRYPTHASH hhash, const BYTE *sig, DWORD siglen, HCRYPTKEY hpubkey,
                               const WCHAR *desc, DWORD flags )
{
    struct hash *hash = (struct hash *)hhash;
    struct key *key = (struct key *)hpubkey;

    TRACE( "%p, %p, %p, %u %p, %s, %08x\n", (void *)hprov, (void *)hhash, sig, siglen, (void *)hpubkey,
           debugstr_w(desc), flags );

    if (hash->magic != MAGIC_HASH || key->magic != MAGIC_KEY) return FALSE;
    if (flags)
    {
        FIXME( "flags %08x not supported\n", flags );
        return FALSE;
    }

    return !BCryptVerifySignature( key->handle, NULL, hash->value, hash->len, (UCHAR *)sig, siglen, 0 );
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    TRACE("(0x%p, %d, %p)\n", hinstDLL, fdwReason, lpvReserved);

    switch (fdwReason)
    {
    case DLL_PROCESS_ATTACH:
        instance = hinstDLL;
        DisableThreadLibraryCalls(hinstDLL);
        break;
    }
    return TRUE;
}

/*****************************************************
 *    DllRegisterServer (DSSENH.@)
 */
HRESULT WINAPI DllRegisterServer(void)
{
    return __wine_register_resources( instance );
}

/*****************************************************
 *    DllUnregisterServer (DSSENH.@)
 */
HRESULT WINAPI DllUnregisterServer(void)
{
    return __wine_unregister_resources( instance );
}
