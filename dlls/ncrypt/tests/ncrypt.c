/* Unit test suite for ncrypt.dll
 *
 * Copyright 2021 Santino Mazza
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
#include <windef.h>
#include <winbase.h>
#include <ncrypt.h>
#include <bcrypt.h>

#include "wine/test.h"

static UCHAR rsa_key_blob[] = {
    0x52, 0x53, 0x41, 0x31, 0x00, 0x04, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00,
    0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x01, 0x00, 0x01, 0xc7, 0x8f, 0xac, 0x2a, 0xce, 0xbf, 0xc9, 0x6c, 0x7b,
    0x85, 0x74, 0x71, 0xbb, 0xff, 0xbb, 0x9b, 0x20, 0x03, 0x79, 0x17, 0x34,
    0xe7, 0x26, 0x91, 0x5c, 0x1f, 0x1b, 0x03, 0x3d, 0x46, 0xdf, 0xb6, 0xf2,
    0x10, 0x55, 0xf0, 0x39, 0x55, 0x0a, 0xe3, 0x9c, 0x0c, 0x63, 0xc2, 0x14,
    0x03, 0x94, 0x51, 0x0d, 0xb4, 0x22, 0x09, 0xf2, 0x5c, 0xb2, 0xd1, 0xc3,
    0xac, 0x6f, 0xa8, 0xc4, 0xac, 0xb8, 0xbc, 0x59, 0xe7, 0xed, 0x77, 0x6e,
    0xb1, 0x80, 0x58, 0x7d, 0xb2, 0x94, 0x46, 0xe5, 0x00, 0xe2, 0xb7, 0x33,
    0x48, 0x7a, 0xd3, 0x78, 0xe9, 0x26, 0x01, 0xc7, 0x00, 0x7b, 0x41, 0x6d,
    0x94, 0x3a, 0xe1, 0x50, 0x2b, 0x9f, 0x6b, 0x1c, 0x08, 0xa3, 0xfc, 0x0a,
    0x44, 0x81, 0x09, 0x41, 0x80, 0x23, 0x7b, 0xf6, 0x3f, 0xaf, 0x91, 0xa1,
    0x87, 0x75, 0x33, 0x15, 0xb8, 0xde, 0x32, 0x30, 0xb4, 0x5e, 0xfd};

static UCHAR rsa_key_blob_with_invalid_bit_length[] = {
    0x52, 0x53, 0x41, 0x31, 0x00, 0x03, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00,
    0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x01, 0x00, 0x01, 0xc7, 0x8f, 0xac, 0x2a, 0xce, 0xbf, 0xc9, 0x6c, 0x7b,
    0x85, 0x74, 0x71, 0xbb, 0xff, 0xbb, 0x9b, 0x20, 0x03, 0x79, 0x17, 0x34,
    0xe7, 0x26, 0x91, 0x5c, 0x1f, 0x1b, 0x03, 0x3d, 0x46, 0xdf, 0xb6, 0xf2,
    0x10, 0x55, 0xf0, 0x39, 0x55, 0x0a, 0xe3, 0x9c, 0x0c, 0x63, 0xc2, 0x14,
    0x03, 0x94, 0x51, 0x0d, 0xb4, 0x22, 0x09, 0xf2, 0x5c, 0xb2, 0xd1, 0xc3,
    0xac, 0x6f, 0xa8, 0xc4, 0xac, 0xb8, 0xbc, 0x59, 0xe7, 0xed, 0x77, 0x6e,
    0xb1, 0x80, 0x58, 0x7d, 0xb2, 0x94, 0x46, 0xe5, 0x00, 0xe2, 0xb7, 0x33,
    0x48, 0x7a, 0xd3, 0x78, 0xe9, 0x26, 0x01, 0xc7, 0x00, 0x7b, 0x41, 0x6d,
    0x94, 0x3a, 0xe1, 0x50, 0x2b, 0x9f, 0x6b, 0x1c, 0x08, 0xa3, 0xfc, 0x0a,
    0x44, 0x81, 0x09, 0x41, 0x80, 0x23, 0x7b, 0xf6, 0x3f, 0xaf, 0x91, 0xa1,
    0x87, 0x75, 0x33, 0x15, 0xb8, 0xde, 0x32, 0x30, 0xb4, 0x5e, 0xfd};

static UCHAR rsa_key_blob_with_invalid_publicexp_size[] = {
    0x52, 0x53, 0x41, 0x31, 0x00, 0x04, 0x00, 0x00, 0x09, 0x00, 0x00, 0x00,
    0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x01, 0x00, 0x01, 0xc7, 0x8f, 0xac, 0x2a, 0xce, 0xbf, 0xc9, 0x6c, 0x7b,
    0x85, 0x74, 0x71, 0xbb, 0xff, 0xbb, 0x9b, 0x20, 0x03, 0x79, 0x17, 0x34,
    0xe7, 0x26, 0x91, 0x5c, 0x1f, 0x1b, 0x03, 0x3d, 0x46, 0xdf, 0xb6, 0xf2,
    0x10, 0x55, 0xf0, 0x39, 0x55, 0x0a, 0xe3, 0x9c, 0x0c, 0x63, 0xc2, 0x14,
    0x03, 0x94, 0x51, 0x0d, 0xb4, 0x22, 0x09, 0xf2, 0x5c, 0xb2, 0xd1, 0xc3,
    0xac, 0x6f, 0xa8, 0xc4, 0xac, 0xb8, 0xbc, 0x59, 0xe7, 0xed, 0x77, 0x6e,
    0xb1, 0x80, 0x58, 0x7d, 0xb2, 0x94, 0x46, 0xe5, 0x00, 0xe2, 0xb7, 0x33,
    0x48, 0x7a, 0xd3, 0x78, 0xe9, 0x26, 0x01, 0xc7, 0x00, 0x7b, 0x41, 0x6d,
    0x94, 0x3a, 0xe1, 0x50, 0x2b, 0x9f, 0x6b, 0x1c, 0x08, 0xa3, 0xfc, 0x0a,
    0x44, 0x81, 0x09, 0x41, 0x80, 0x23, 0x7b, 0xf6, 0x3f, 0xaf, 0x91, 0xa1,
    0x87, 0x75, 0x33, 0x15, 0xb8, 0xde, 0x32, 0x30, 0xb4, 0x5e, 0xfd};

/* RSA public key with invalid magic value. */
static UCHAR invalid_rsa_key_blob[] = {
    0x75, 0x59, 0x41, 0x31, 0x00, 0x04, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00,
    0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x01, 0x00, 0x01, 0xc7, 0x8f, 0xac, 0x2a, 0xce, 0xbf, 0xc9, 0x6c, 0x7b,
    0x85, 0x74, 0x71, 0xbb, 0xff, 0xbb, 0x9b, 0x20, 0x03, 0x79, 0x17, 0x34,
    0xe7, 0x26, 0x91, 0x5c, 0x1f, 0x1b, 0x03, 0x3d, 0x46, 0xdf, 0xb6, 0xf2,
    0x10, 0x55, 0xf0, 0x39, 0x55, 0x0a, 0xe3, 0x9c, 0x0c, 0x63, 0xc2, 0x14,
    0x03, 0x94, 0x51, 0x0d, 0xb4, 0x22, 0x09, 0xf2, 0x5c, 0xb2, 0xd1, 0xc3,
    0xac, 0x6f, 0xa8, 0xc4, 0xac, 0xb8, 0xbc, 0x59, 0xe7, 0xed, 0x77, 0x6e,
    0xb1, 0x80, 0x58, 0x7d, 0xb2, 0x94, 0x46, 0xe5, 0x00, 0xe2, 0xb7, 0x33,
    0x48, 0x7a, 0xd3, 0x78, 0xe9, 0x26, 0x01, 0xc7, 0x00, 0x7b, 0x41, 0x6d,
    0x94, 0x3a, 0xe1, 0x50, 0x2b, 0x9f, 0x6b, 0x1c, 0x08, 0xa3, 0xfc, 0x0a,
    0x44, 0x81, 0x09, 0x41, 0x80, 0x23, 0x7b, 0xf6, 0x3f, 0xaf, 0x91, 0xa1,
    0x87, 0x75, 0x33, 0x15, 0xb8, 0xde, 0x32, 0x30, 0xb4, 0x5e, 0xfd};

static void test_key_import_rsa(void)
{
    NCRYPT_PROV_HANDLE prov;
    NCRYPT_KEY_HANDLE key;
    SECURITY_STATUS ret;

    prov = 0;
    ret = NCryptOpenStorageProvider(&prov, NULL, 0);
    ok(ret == ERROR_SUCCESS, "got %#lx\n", ret);
    ok(prov, "got null handle\n");

    key = 0;
    ret = NCryptImportKey(prov, 0, BCRYPT_RSAPUBLIC_BLOB, NULL, &key, rsa_key_blob, sizeof(rsa_key_blob), 0);
    ok(ret == ERROR_SUCCESS, "got %#lx\n", ret);
    ok(key, "got null handle\n");
    NCryptFreeObject(key);

    key = 0;
    ret = NCryptImportKey(prov, 0, BCRYPT_PUBLIC_KEY_BLOB, NULL, &key, rsa_key_blob, sizeof(rsa_key_blob), 0);
    ok(ret == ERROR_SUCCESS, "got %#lx\n", ret);
    ok(key, "got null handle\n");
    NCryptFreeObject(key);

    ret = NCryptImportKey(prov, 0, BCRYPT_PUBLIC_KEY_BLOB, NULL, &key, rsa_key_blob, sizeof(rsa_key_blob), 49);
    ok(ret == NTE_BAD_FLAGS, "got %#lx\n", ret);

    ret = NCryptImportKey(prov, 0, BCRYPT_PUBLIC_KEY_BLOB, NULL, &key, invalid_rsa_key_blob,
                          sizeof(invalid_rsa_key_blob), 0);
    ok(ret == NTE_INVALID_PARAMETER, "got %#lx\n", ret);

    key = 0;
    ret = NCryptImportKey(prov, 0, BCRYPT_PUBLIC_KEY_BLOB, NULL, &key, rsa_key_blob_with_invalid_bit_length,
                          sizeof(rsa_key_blob_with_invalid_bit_length), 0);
    ok(ret == ERROR_SUCCESS, "got %#lx\n", ret); /* I'm not sure why, but this returns success */
    ok(key, "got null handle\n");
    NCryptFreeObject(key);

    ret = NCryptImportKey(prov, 0, BCRYPT_PUBLIC_KEY_BLOB, NULL, &key, rsa_key_blob_with_invalid_publicexp_size,
                          sizeof(rsa_key_blob_with_invalid_publicexp_size), 0);
    ok(ret == NTE_BAD_DATA, "got %#lx\n", ret);

    ret = NCryptImportKey(prov, 0, BCRYPT_PUBLIC_KEY_BLOB, NULL, &key, rsa_key_blob, sizeof(rsa_key_blob) - 1, 0);
    ok(ret == NTE_BAD_DATA, "got %#lx\n", ret);

    ret = NCryptImportKey(prov, 0, BCRYPT_PUBLIC_KEY_BLOB, NULL, &key, rsa_key_blob, sizeof(rsa_key_blob) + 1, 0);
    ok(ret == NTE_BAD_DATA, "got %#lx\n", ret);

    NCryptFreeObject(prov);
}

static void test_free_object(void)
{
    NCRYPT_PROV_HANDLE prov;
    NCRYPT_KEY_HANDLE key;
    SECURITY_STATUS ret;
    char *buf;

    ret = NCryptOpenStorageProvider(&prov, NULL, 0);
    ok(ret == ERROR_SUCCESS, "got %#lx\n", ret);

    ret = NCryptImportKey(prov, 0, BCRYPT_RSAPUBLIC_BLOB, NULL, &key, rsa_key_blob, sizeof(rsa_key_blob), 0);
    ok(ret == ERROR_SUCCESS, "got %#lx\n", ret);
    ret = NCryptFreeObject(key);
    ok(ret == ERROR_SUCCESS, "got %#lx\n", ret);

    key = 0;
    ret = NCryptFreeObject(key);
    ok(ret == NTE_INVALID_HANDLE, "got %#lx\n", ret);

    buf = calloc(1, 50);
    ret = NCryptFreeObject((NCRYPT_KEY_HANDLE)buf);
    ok(ret == NTE_INVALID_HANDLE, "got %#lx\n", ret);
    free(buf);

    NCryptFreeObject(prov);
}

static void test_get_property(void)
{
    NCRYPT_PROV_HANDLE prov;
    NCRYPT_KEY_HANDLE key;
    SECURITY_STATUS ret;
    WCHAR value[4];
    DWORD keylength, size;

    ret = NCryptOpenStorageProvider(&prov, NULL, 0);
    ok(ret == ERROR_SUCCESS, "got %#lx\n", ret);

    ret = NCryptImportKey(prov, 0, BCRYPT_RSAPUBLIC_BLOB, NULL, &key, rsa_key_blob, sizeof(rsa_key_blob), 0);
    ok(ret == ERROR_SUCCESS, "got %#lx\n", ret);

    size = 0;
    ret = NCryptGetProperty(key, NCRYPT_ALGORITHM_GROUP_PROPERTY, NULL, 0, &size, 0);
    ok(ret == ERROR_SUCCESS, "got %#lx\n", ret);
    ok(size == 8, "got %lu\n", size);

    size = 0;
    value[0] = 0;
    ret = NCryptGetProperty(key, NCRYPT_ALGORITHM_GROUP_PROPERTY, (BYTE *)value, sizeof(value), &size, 0);
    ok(ret == ERROR_SUCCESS, "got %#lx\n", ret);
    ok(size == 8, "got %lu\n", size);
    ok(!lstrcmpW(value, L"RSA"), "The string doesn't match with 'RSA'\n");

    size = 0;
    ret = NCryptGetProperty(key, NCRYPT_LENGTH_PROPERTY, NULL, 0, &size, 0);
    ok(ret == ERROR_SUCCESS, "got %#lx\n", ret);
    ok(size == sizeof(DWORD), "got %lu\n", size);

    keylength = 0;
    ret = NCryptGetProperty(key, NCRYPT_LENGTH_PROPERTY, (BYTE *)&keylength, size, &size, 0);
    ok(ret == ERROR_SUCCESS, "got %#lx\n", ret);
    ok(keylength == 1024, "got %lu\n", keylength);

    ret = NCryptGetProperty(0, NCRYPT_LENGTH_PROPERTY, (BYTE *)&keylength, size, &size, 0);
    ok(ret == NTE_INVALID_HANDLE, "got %#lx\n", ret);

    NCryptFreeObject(prov);
}

static void test_set_property(void)
{
    NCRYPT_PROV_HANDLE prov;
    SECURITY_STATUS ret;
    NCRYPT_KEY_HANDLE key;
    DWORD keylength;

    ret = NCryptOpenStorageProvider(&prov, NULL, 0);
    ok(ret == ERROR_SUCCESS, "got %#lx\n", ret);

    ret = NCryptImportKey(prov, 0, BCRYPT_RSAPUBLIC_BLOB, NULL, &key, rsa_key_blob, sizeof(rsa_key_blob), 0);
    ok(ret == ERROR_SUCCESS, "got %#lx\n", ret);

    keylength = 2048;
    ret = NCryptSetProperty(key, NCRYPT_LENGTH_PROPERTY, (BYTE *)&keylength, sizeof(keylength), 0);
    ok(ret == ERROR_SUCCESS || broken(ret == NTE_INVALID_HANDLE), "got %#lx\n", ret);

    ret = NCryptSetProperty(0, NCRYPT_LENGTH_PROPERTY, (BYTE *)&keylength, sizeof(keylength), 0);
    ok(ret == NTE_INVALID_HANDLE, "got %#lx\n", ret);

    todo_wine
    {
    ret = NCryptSetProperty(key, NCRYPT_NAME_PROPERTY, (BYTE *)L"Key name", sizeof(L"Key name"), 0);
    ok(ret == NTE_NOT_SUPPORTED, "got %#lx\n", ret);
    }
    NCryptFreeObject(key);

    key = 0;
    ret = NCryptCreatePersistedKey(prov, &key, BCRYPT_RSA_ALGORITHM, NULL, 0, 0);
    ok(ret == ERROR_SUCCESS, "got %#lx\n", ret);
    ok(key, "got null handle\n");

    keylength = 2048;
    ret = NCryptSetProperty(key, NCRYPT_LENGTH_PROPERTY, (BYTE *)&keylength, sizeof(keylength), 0);
    ok(ret == ERROR_SUCCESS, "got %#lx\n", ret);

    todo_wine
    {
    ret = NCryptSetProperty(key, NCRYPT_NAME_PROPERTY, (BYTE *)L"Key name", sizeof(L"Key name"), 0);
    ok(ret == NTE_NOT_SUPPORTED, "got %#lx\n", ret);

    ret = NCryptSetProperty(key, L"My Custom Property", (BYTE *)L"value", sizeof(L"value"), 0);
    ok(ret == NTE_NOT_SUPPORTED, "got %#lx\n", ret);
    }
    NCryptFreeObject(key);
    NCryptFreeObject(prov);
}

static void test_create_persisted_key(void)
{
    NCRYPT_PROV_HANDLE prov;
    NCRYPT_KEY_HANDLE key;
    SECURITY_STATUS ret;
    DWORD size, keylength;
    WCHAR alggroup[4];

    ret = NCryptOpenStorageProvider(&prov, NULL, 0);
    ok(ret == ERROR_SUCCESS, "got %#lx\n", ret);

    key = 0;
    ret = NCryptCreatePersistedKey(0, &key, BCRYPT_RSA_ALGORITHM, NULL, 0, 0);
    ok(ret == NTE_INVALID_HANDLE, "got %#lx\n", ret);

    ret = NCryptCreatePersistedKey(prov, &key, NULL, NULL, 0, 0);
    ok(ret == HRESULT_FROM_WIN32(RPC_X_NULL_REF_POINTER) || broken(ret == NTE_FAIL), "got %#lx\n", ret);

    ret = NCryptCreatePersistedKey(prov, &key, BCRYPT_RSA_ALGORITHM, NULL, 0, 0);
    ok(ret == ERROR_SUCCESS, "got %#lx\n", ret);
    ok(key, "got null handle\n");

    ret = NCryptGetProperty(key, NCRYPT_ALGORITHM_GROUP_PROPERTY, NULL, 0, &size, 0);
    ok(ret == ERROR_SUCCESS, "got %#lx\n", ret);
    ok(size == 8, "got %lu\n", size);

    size = 0;
    alggroup[0] = 0;
    ret = NCryptGetProperty(key, NCRYPT_ALGORITHM_GROUP_PROPERTY, (BYTE *)alggroup, sizeof(alggroup), &size, 0);
    ok(ret == ERROR_SUCCESS, "got %#lx\n", ret);
    ok(size == 8, "got %lu\n", size);
    ok(!lstrcmpW(alggroup, L"RSA"), "The string doesn't match with 'RSA'\n");

    ret = NCryptGetProperty(key, NCRYPT_LENGTH_PROPERTY, (BYTE *)&keylength, sizeof(keylength), &size, 0);
    ok(ret == ERROR_SUCCESS, "got %#lx\n", ret);
    ok(size == 4, "got %lu\n", size);
    ok(keylength == 1024, "got %lu\n", keylength);

    NCryptFinalizeKey(key, 0);
    NCryptFreeObject(key);

    todo_wine
    {
    key = 0;
    ret = NCryptCreatePersistedKey(prov, &key, BCRYPT_AES_ALGORITHM, NULL, 0, 0);
    ok(ret == ERROR_SUCCESS || broken(ret == NTE_NOT_SUPPORTED) /* win 7 */, "got %#lx\n", ret);
    if (ret == NTE_NOT_SUPPORTED) win_skip("broken, symmetric keys not supported.\n");
    else ok(key, "got null handle\n");
    }

    NCryptFreeObject(prov);
}

static void test_finalize_key(void)
{
    NCRYPT_PROV_HANDLE prov;
    NCRYPT_KEY_HANDLE key;
    SECURITY_STATUS ret;

    ret = NCryptOpenStorageProvider(&prov, NULL, 0);
    ok(ret == ERROR_SUCCESS, "got %#lx\n", ret);

    ret = NCryptCreatePersistedKey(prov, &key, BCRYPT_RSA_ALGORITHM, NULL, 0, 0);
    ok(ret == ERROR_SUCCESS, "got %#lx\n", ret);

    ret = NCryptFinalizeKey(key, 0);
    ok(ret == ERROR_SUCCESS, "got %#lx\n", ret);

    ret = NCryptFinalizeKey(key, 0);
    ok(ret == NTE_INVALID_HANDLE, "got %#lx\n", ret);

    ret = NCryptFinalizeKey(0, 0);
    ok(ret == NTE_INVALID_HANDLE, "got %#lx\n", ret);

    NCryptFreeObject(key);
}

START_TEST(ncrypt)
{
    test_key_import_rsa();
    test_free_object();
    test_get_property();
    test_set_property();
    test_create_persisted_key();
    test_finalize_key();
}
