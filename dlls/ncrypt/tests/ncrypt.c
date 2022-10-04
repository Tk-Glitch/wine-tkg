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

static UCHAR signature_pubkey_blob[] = {
  0x52, 0x53, 0x41, 0x31, 0x00, 0x08, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00,
  0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x01, 0x00, 0x01, 0xb4, 0xdb, 0x15, 0x24, 0xd0, 0x9a, 0xf4, 0xa3, 0x22,
  0x04, 0xbe, 0xbd, 0xda, 0xb4, 0x3b, 0x1f, 0x9d, 0x5f, 0x2a, 0x3e, 0xe7,
  0x42, 0xd7, 0xfa, 0xbc, 0xeb, 0x4e, 0xba, 0xcf, 0x92, 0x5d, 0x42, 0x0e,
  0xc0, 0xb0, 0x0f, 0xcc, 0xc3, 0x03, 0x69, 0x23, 0xb9, 0xc2, 0x07, 0xf7,
  0xf5, 0x19, 0xef, 0x9f, 0xb6, 0xd4, 0x6d, 0x35, 0x0d, 0x4c, 0x3a, 0x2b,
  0x37, 0x9d, 0x4d, 0x61, 0xce, 0x30, 0x33, 0x27, 0xe4, 0x9a, 0xca, 0x23,
  0xd6, 0x8f, 0x90, 0x33, 0xba, 0x5b, 0x16, 0xab, 0xa6, 0xda, 0x7a, 0x16,
  0x8e, 0xae, 0xf9, 0x6f, 0x29, 0xe2, 0xf1, 0x3d, 0xf0, 0xb6, 0xb4, 0x70,
  0xc0, 0x6d, 0xfc, 0x73, 0xf1, 0x3e, 0xdf, 0x02, 0x2c, 0xab, 0x24, 0x7a,
  0xd8, 0x8b, 0xcf, 0x63, 0xd0, 0x1a, 0x76, 0xb3, 0x11, 0x22, 0x5d, 0xf6,
  0x72, 0xb5, 0xd1, 0x6b, 0x83, 0xcd, 0x63, 0x5d, 0x3d, 0x03, 0xa2, 0xe0,
  0x36, 0xe8, 0x54, 0xe9, 0x6f, 0x04, 0xb3, 0x1a, 0xdf, 0x35, 0xd7, 0x01,
  0x57, 0xc7, 0x8d, 0x81, 0x4b, 0x39, 0x0c, 0x92, 0x71, 0x0b, 0x7d, 0x05,
  0xb4, 0xa4, 0xb4, 0xd4, 0x73, 0x55, 0x85, 0x19, 0xd5, 0x37, 0x7f, 0xcd,
  0x81, 0xd3, 0xf2, 0x62, 0x60, 0xfb, 0x6a, 0x1a, 0xda, 0xb4, 0x83, 0xfa,
  0x87, 0xac, 0x51, 0x3a, 0xb4, 0xc2, 0x03, 0xd3, 0xc6, 0xf0, 0x20, 0xca,
  0x10, 0xfd, 0xeb, 0xd8, 0x85, 0xb9, 0x74, 0x29, 0x83, 0xda, 0xa2, 0xee,
  0xcc, 0xf4, 0xf6, 0x18, 0xbc, 0x84, 0x92, 0xf5, 0xdb, 0x71, 0x14, 0xb9,
  0x62, 0x54, 0x4b, 0x64, 0x58, 0x95, 0xbc, 0x69, 0xc1, 0xcd, 0xac, 0x4b,
  0x7c, 0x1d, 0xc4, 0xe7, 0x6c, 0x44, 0x80, 0x5e, 0x88, 0x3a, 0x9c, 0xbd,
  0xd5, 0xa5, 0x2f, 0xa6, 0x73, 0x65, 0x48, 0x00, 0x5e, 0xfd, 0xe9, 0x4e,
  0xd7, 0x6c, 0x59, 0x47, 0x9b, 0xd1, 0x9d};

static UCHAR signature_pkcs1_sha256[] = {
  0xad, 0x3d, 0x2f, 0x31, 0x87, 0xba, 0x5b, 0xdc, 0x84, 0x95, 0x77, 0x68,
  0xf9, 0x9c, 0xd2, 0x38, 0x5b, 0x9f, 0x09, 0xc3, 0x8e, 0x9e, 0x8e, 0x8d,
  0x49, 0x56, 0x31, 0x09, 0x55, 0xc6, 0xd2, 0x1a, 0xb3, 0x00, 0x71, 0x60,
  0x84, 0x38, 0xf7, 0x1b, 0xc0, 0x43, 0x87, 0x31, 0x2d, 0xcc, 0xf2, 0x49,
  0x5f, 0x21, 0x8e, 0x77, 0x16, 0x57, 0xd7, 0xe9, 0x37, 0x3a, 0xe9, 0x91,
  0x5b, 0xc5, 0xea, 0x34, 0xb5, 0xf7, 0x4d, 0xaa, 0x65, 0x06, 0xef, 0xc6,
  0x14, 0xfa, 0xa7, 0x1e, 0xe5, 0xc6, 0x18, 0xfe, 0x06, 0x64, 0xe7, 0x6e,
  0x11, 0x83, 0x03, 0x4c, 0x91, 0x47, 0x60, 0x88, 0x12, 0x0c, 0x5e, 0xcc,
  0x3a, 0x93, 0x2f, 0x5c, 0x57, 0x5b, 0x66, 0x55, 0xa3, 0xe5, 0xf4, 0x3c,
  0x6f, 0xe9, 0xd8, 0xfd, 0xcc, 0x19, 0xc8, 0xf8, 0x13, 0x21, 0x3d, 0x29,
  0xee, 0x3c, 0xb2, 0xb4, 0x3f, 0x36, 0xd8, 0x82, 0xb1, 0xa9, 0x34, 0x89,
  0x2a, 0x7a, 0x42, 0x4a, 0x13, 0x0a, 0x0b, 0x50, 0xbe, 0x43, 0xa8, 0x2c,
  0x5a, 0x86, 0x65, 0x9f, 0xab, 0x86, 0x25, 0x44, 0xbb, 0x6b, 0xca, 0x04,
  0xca, 0xa5, 0xe7, 0x8e, 0xe2, 0x1d, 0xa8, 0x31, 0x90, 0x85, 0x6a, 0xd9,
  0xaf, 0x67, 0x8c, 0x1a, 0x13, 0xea, 0xa8, 0xfd, 0x3d, 0xc0, 0xb0, 0x3f,
  0xde, 0x6a, 0xc9, 0x65, 0xeb, 0x0a, 0xa6, 0x9b, 0xfc, 0x97, 0x0a, 0x72,
  0xaf, 0x04, 0xde, 0x3f, 0xcb, 0xac, 0x97, 0x6e, 0xfa, 0xbb, 0xd8, 0x6e,
  0xad, 0x0d, 0x10, 0xc6, 0x34, 0x18, 0x50, 0xb9, 0x12, 0xd4, 0x6b, 0xd5,
  0x3d, 0x9d, 0xfb, 0x31, 0x8d, 0x96, 0x39, 0x4d, 0x20, 0x91, 0x40, 0x06,
  0xca, 0xe6, 0x33, 0x73, 0x76, 0x2a, 0xe3, 0xab, 0xaf, 0x9d, 0xe9, 0x4e,
  0xb7, 0xe0, 0x4b, 0x45, 0x23, 0x07, 0xe9, 0xe2, 0xab, 0xdc, 0x9f, 0x5a,
  0xed, 0xbe, 0x76, 0x27};

static UCHAR invalid_signature[] = {
  0x4f, 0x8f, 0x88, 0x43, 0xed, 0x9e, 0xe8, 0x75, 0x0f, 0xe7, 0x32, 0xab,
  0x07, 0xcc, 0x6c, 0x8d, 0x56, 0xaa, 0x78, 0xf8, 0x43, 0x62, 0x4c, 0x0c,
  0xd5, 0x45, 0x2c, 0x3d, 0x28, 0x6f, 0xbc, 0x8b, 0xc1, 0x9f, 0xfe, 0x33,
  0x50, 0x4e, 0xd0, 0x6a, 0x83, 0xf8, 0x0e, 0xc8, 0x19, 0x59, 0xde, 0x25,
  0xf0, 0x7b, 0x4e, 0x3d, 0x9d, 0xc7, 0xbf, 0x72, 0xc3, 0x26, 0x48, 0xb6,
  0x19, 0x08, 0x3f, 0x05, 0xe8, 0x76, 0x2c, 0x11, 0x49, 0xfb, 0xa1, 0xc0,
  0xae, 0x6e, 0x30, 0xf8, 0x10, 0x61, 0x9f, 0x53, 0xc6, 0xeb, 0x7c, 0x7c,
  0x94, 0x5f, 0xca, 0xe5, 0x41, 0x40, 0xd6, 0x83, 0xa8, 0x4a, 0x45, 0x5c,
  0x77, 0x69, 0xf2, 0x89, 0xe9, 0xf7, 0x81, 0xa1, 0xae, 0x77, 0xf5, 0xec,
  0x41, 0x4b, 0xfc, 0x82, 0x7f, 0xee, 0x4e, 0x5b, 0x43, 0xdd, 0xcc, 0xe8,
  0x90, 0x12, 0xf0, 0x9a, 0xd0, 0xbc, 0x56, 0xd8, 0xeb, 0xd6, 0xfc, 0x80,
  0x48, 0x26, 0x8c, 0x63, 0x5c, 0x55, 0xd9, 0x6c, 0xb5, 0x10, 0xc1, 0xab,
  0x98, 0x3c, 0xe1, 0x25, 0x13, 0x4e, 0xc2, 0xdc, 0xd5, 0x4a, 0x25, 0xe5,
  0xc7, 0x9f, 0xfc, 0xf8, 0x53, 0x53, 0x12, 0x4c, 0xc1, 0x6c, 0x67, 0xa9,
  0x9e, 0x6e, 0x88, 0x2e, 0x93, 0xa3, 0x3f, 0x2d, 0xd3, 0xc5, 0xf4, 0x25,
  0xe2, 0x0f, 0x8f, 0xc0, 0x2b, 0x30, 0x94, 0x9d, 0xc0, 0x09, 0x31, 0x6c,
  0x19, 0xf7, 0x2e, 0x72, 0x6b, 0xb3, 0xf6, 0x78, 0x91, 0xca, 0x95, 0x28,
  0x9f, 0xf6, 0xd0, 0x77, 0x1c, 0x73, 0xff, 0x30, 0xe7, 0x02, 0xc3, 0xc8,
  0x0e, 0x95, 0x4f, 0x02, 0x96, 0xc5, 0xa9, 0x02, 0xb8, 0xaa, 0x97, 0xac,
  0x37, 0x47, 0xa9, 0x6d, 0xcb, 0x25, 0xb3, 0x8d, 0x20, 0xc5, 0xb6, 0x49,
  0xd8, 0xaf, 0x1a, 0xad, 0x62, 0x8c, 0x79, 0x60, 0x73, 0x8d, 0x46, 0x1e,
  0xe5, 0x23, 0x39, 0x44};

static UCHAR sha256_hash[] = {
  0x27, 0x51, 0x8b, 0xa9, 0x68, 0x30, 0x11, 0xf6, 0xb3, 0x96, 0x07, 0x2c,
  0x05, 0xf6, 0x65, 0x6d, 0x04, 0xf5, 0xfb, 0xc3, 0x78, 0x7c, 0xf9, 0x24,
  0x90, 0xec, 0x60, 0x6e, 0x50, 0x92, 0xe3, 0x26};

static void test_verify_signature(void)
{
    NCRYPT_PROV_HANDLE prov;
    NCRYPT_KEY_HANDLE key;
    BCRYPT_PKCS1_PADDING_INFO padinfo, invalid_padinfo;
    SECURITY_STATUS ret;

    ret = NCryptOpenStorageProvider(&prov, NULL, 0);
    ok(ret == ERROR_SUCCESS, "got %#lx\n", ret);

    ret = NCryptImportKey(prov, 0, BCRYPT_PUBLIC_KEY_BLOB, NULL, &key, signature_pubkey_blob,
                          sizeof(signature_pubkey_blob), 0);
    ok(ret == ERROR_SUCCESS, "got %#lx\n", ret);

    padinfo.pszAlgId = BCRYPT_SHA256_ALGORITHM;
    ret = NCryptVerifySignature(key, &padinfo, sha256_hash, sizeof(sha256_hash), signature_pkcs1_sha256,
                                sizeof(signature_pkcs1_sha256), NCRYPT_PAD_PKCS1_FLAG);
    ok(ret == ERROR_SUCCESS, "got %#lx\n", ret);

    ret = NCryptVerifySignature(key, &padinfo, sha256_hash, sizeof(sha256_hash), invalid_signature,
                                sizeof(invalid_signature), NCRYPT_PAD_PKCS1_FLAG);
    ok(ret == NTE_BAD_SIGNATURE, "got %#lx\n", ret);

    ret = NCryptVerifySignature(key, NULL, sha256_hash, sizeof(sha256_hash), signature_pkcs1_sha256,
                                sizeof(signature_pkcs1_sha256), 0);
    ok(ret == NTE_INVALID_PARAMETER, "got %#lx\n", ret);

    ret = NCryptVerifySignature(0, &padinfo, sha256_hash, sizeof(sha256_hash), signature_pkcs1_sha256,
                                sizeof(signature_pkcs1_sha256), NCRYPT_PAD_PKCS1_FLAG);
    ok(ret == NTE_INVALID_HANDLE, "got %#lx\n", ret);

    ret = NCryptVerifySignature(key, &padinfo, NULL, sizeof(sha256_hash), signature_pkcs1_sha256,
                                sizeof(signature_pkcs1_sha256), NCRYPT_PAD_PKCS1_FLAG);
    ok(ret == HRESULT_FROM_WIN32(RPC_X_NULL_REF_POINTER) || broken(ret == NTE_FAIL) /* win7 */, "got %#lx\n", ret);

    ret = NCryptVerifySignature(key, &padinfo, sha256_hash, 4, signature_pkcs1_sha256,
                                sizeof(signature_pkcs1_sha256), NCRYPT_PAD_PKCS1_FLAG);
    todo_wine ok(ret == NTE_INVALID_PARAMETER, "got %#lx\n", ret);

    ret = NCryptVerifySignature(key, &padinfo, sha256_hash, sizeof(sha256_hash), NULL,
                                sizeof(signature_pkcs1_sha256), NCRYPT_PAD_PKCS1_FLAG);
    ok(ret == HRESULT_FROM_WIN32(RPC_X_NULL_REF_POINTER) || broken(ret == NTE_FAIL) /* win7 */, "got %#lx\n", ret);

    ret = NCryptVerifySignature(key, &padinfo, sha256_hash, sizeof(sha256_hash), signature_pkcs1_sha256, 4,
                                NCRYPT_PAD_PKCS1_FLAG);
    todo_wine ok(ret == NTE_INVALID_PARAMETER, "got %#lx\n", ret);

    invalid_padinfo.pszAlgId = BCRYPT_MD5_ALGORITHM;
    ret = NCryptVerifySignature(key, &invalid_padinfo, sha256_hash, sizeof(sha256_hash), signature_pkcs1_sha256,
                                sizeof(signature_pkcs1_sha256), NCRYPT_PAD_PKCS1_FLAG);
    todo_wine ok(ret == NTE_INVALID_PARAMETER, "got %#lx\n", ret);

    NCryptFreeObject(key);
    NCryptFreeObject(prov);
}

static void test_NCryptIsAlgSupported(void)
{
    NCRYPT_PROV_HANDLE prov;
    SECURITY_STATUS ret;

    NCryptOpenStorageProvider(&prov, NULL, 0);
    ret = NCryptIsAlgSupported(0, BCRYPT_RSA_ALGORITHM, 0);
    ok(ret == NTE_INVALID_HANDLE, "expected NTE_INVALID_HANDLE, got %#lx\n", ret);

    ret = NCryptIsAlgSupported(prov, NULL, 0);
    ok(ret == HRESULT_FROM_WIN32(RPC_X_NULL_REF_POINTER) || broken(ret == NTE_FAIL) /* win7 */, "got %#lx\n", ret);

    ret = NCryptIsAlgSupported(prov, BCRYPT_RSA_ALGORITHM, 20);
    ok(ret == NTE_BAD_FLAGS, "expected NTE_BAD_FLAGS, got %#lx\n", ret);

    ret = NCryptIsAlgSupported(prov, BCRYPT_RSA_ALGORITHM, 0);
    ok(ret == ERROR_SUCCESS, "expected BCRYPT_RSA_ALGORITHM to be supported, got %#lx\n", ret);

    ret = NCryptIsAlgSupported(prov, BCRYPT_RSA_ALGORITHM, NCRYPT_SILENT_FLAG);
    ok(ret == ERROR_SUCCESS, "expected BCRYPT_RSA_ALGORITHM to be supported, got %#lx\n", ret);

    ret = NCryptIsAlgSupported(prov, BCRYPT_3DES_ALGORITHM, 0);
    ok(ret == ERROR_SUCCESS || broken(ret == NTE_NOT_SUPPORTED) /* win7 */,
       "expected BCRYPT_3DES_ALGORITHM to be supported, got %#lx\n", ret);

    ret = NCryptIsAlgSupported(prov, BCRYPT_AES_ALGORITHM, 0);
    ok(ret == ERROR_SUCCESS || broken(ret == NTE_NOT_SUPPORTED) /* win7 */,
       "expected BCRYPT_AES_ALGORITHM to be supported, got %#lx\n", ret);

    ret = NCryptIsAlgSupported(prov, BCRYPT_ECDH_P256_ALGORITHM, 0);
    ok(ret == ERROR_SUCCESS, "expected BCRYPT_ECDH_P256_ALGORITHM to be supported, got %#lx\n", ret);

    ret = NCryptIsAlgSupported(prov, BCRYPT_ECDSA_P256_ALGORITHM, 0);
    ok(ret == ERROR_SUCCESS, "expected BCRYPT_ECDSA_P256_ALGORITHM to be supported, got %#lx\n", ret);

    ret = NCryptIsAlgSupported(prov, BCRYPT_ECDSA_P384_ALGORITHM, 0);
    ok(ret == ERROR_SUCCESS, "expected BCRYPT_ECDSA_P384_ALGORITHM to be supported, got %#lx\n", ret);

    ret = NCryptIsAlgSupported(prov, BCRYPT_DSA_ALGORITHM, 0);
    ok(ret == ERROR_SUCCESS, "expected BCRYPT_DSA_ALGORITHM to be supported, got %#lx\n", ret);

    /* Not supported */
    ret = NCryptIsAlgSupported(prov, BCRYPT_SHA256_ALGORITHM, 0);
    ok(ret == NTE_NOT_SUPPORTED, "expected BCRYPT_SHA256_ALGORITHM to not be supported, got %#lx\n", ret);

    ret = NCryptIsAlgSupported(prov, BCRYPT_SHA384_ALGORITHM, 0);
    ok(ret == NTE_NOT_SUPPORTED, "expected BCRYPT_SHA384_ALGORITHM to not be supported, got %#lx\n", ret);

    ret = NCryptIsAlgSupported(prov, BCRYPT_SHA512_ALGORITHM, 0);
    ok(ret == NTE_NOT_SUPPORTED, "expected BCRYPT_SHA512_ALGORITHM to not be supported, got %#lx\n", ret);

    ret = NCryptIsAlgSupported(prov, BCRYPT_SHA1_ALGORITHM, 0);
    ok(ret == NTE_NOT_SUPPORTED, "expected BCRYPT_SHA1_ALGORITHM to not be supported, got %#lx\n", ret);

    ret = NCryptIsAlgSupported(prov, BCRYPT_MD5_ALGORITHM, 0);
    ok(ret == NTE_NOT_SUPPORTED, "expected BCRYPT_MD5_ALGORITHM to not be supported, got %#lx\n", ret);

    ret = NCryptIsAlgSupported(prov, BCRYPT_MD4_ALGORITHM, 0);
    ok(ret == NTE_NOT_SUPPORTED, "expected BCRYPT_MD4_ALGORITHM to not be supported, got %#lx\n", ret);

    ret = NCryptIsAlgSupported(prov, BCRYPT_MD2_ALGORITHM, 0);
    ok(ret == NTE_NOT_SUPPORTED, "expected BCRYPT_MD2_ALGORITHM to not be supported, got %#lx\n", ret);

    ret = NCryptIsAlgSupported(prov, BCRYPT_RSA_SIGN_ALGORITHM, 0);
    ok(ret == NTE_NOT_SUPPORTED, "expected BCRYPT_RSA_SIGN_ALGORITHM to not be supported, got %#lx\n", ret);

    ret = NCryptIsAlgSupported(prov, BCRYPT_RSA_SIGN_ALGORITHM, 20);
    ok(ret == NTE_BAD_FLAGS, "expected NTE_BAD_FLAGS, got %#lx\n", ret);

    ret = NCryptIsAlgSupported(prov, BCRYPT_RNG_ALGORITHM, 0);
    ok(ret == NTE_NOT_SUPPORTED, "expected BCRYPT_RNG_ALGORITHM to not be supported, got %#lx\n", ret);
    NCryptFreeObject(prov);
}

static UCHAR data_to_encrypt[12] = "Hello world";

static void test_NCryptEncrypt(void)
{
    NCRYPT_PROV_HANDLE prov;
    NCRYPT_KEY_HANDLE key;
    SECURITY_STATUS ret;
    BYTE *output_a;
    BYTE *output_b;
    DWORD output_size;

    NCryptOpenStorageProvider(&prov, NULL, 0);
    NCryptCreatePersistedKey(prov, &key, BCRYPT_RSA_ALGORITHM, NULL, 0, 0);

    /* Test encrypt with invalid key handle */
    ret = NCryptEncrypt(prov, data_to_encrypt, sizeof(data_to_encrypt), NULL, NULL, 0,
                        &output_size, NCRYPT_PAD_PKCS1_FLAG);
    ok(ret == NTE_INVALID_HANDLE, "got %lx\n", ret);

    /* Test encrypt with a non finalized key */
    ret = NCryptEncrypt(key, data_to_encrypt, sizeof(data_to_encrypt), NULL, NULL, 0,
                        &output_size, NCRYPT_PAD_PKCS1_FLAG);
    todo_wine
    ok(ret == NTE_BAD_KEY_STATE, "got %lx\n", ret);

    NCryptFinalizeKey(key, 0);

    /* Test encrypt with invalid flags */
    ret = NCryptEncrypt(key, data_to_encrypt, sizeof(data_to_encrypt), NULL, NULL, 0,
                        &output_size, 51342);
    ok(ret == NTE_BAD_FLAGS, "got %lx\n", ret);

    /* Test no padding with RSA */
    todo_wine
    {
    ret = NCryptEncrypt(key, data_to_encrypt, sizeof(data_to_encrypt), NULL, NULL, 0, &output_size,
                        NCRYPT_NO_PADDING_FLAG);
    ok(ret == ERROR_SUCCESS, "got %lx\n", ret);
    ok(output_size == 128, "got %ld\n", output_size);

    output_a = malloc(output_size);
    ret = NCryptEncrypt(key, data_to_encrypt, sizeof(data_to_encrypt), NULL, output_a, output_size,
                        &output_size, NCRYPT_NO_PADDING_FLAG);
    ok(ret == NTE_INVALID_PARAMETER, "got %lx\n", ret);
    free(output_a);
    }

    /* Test output RSA with PKCS1. PKCS1 should append a random padding to the data, so the output should be different
     * with each call. */
    ret = NCryptEncrypt(key, data_to_encrypt, sizeof(data_to_encrypt), NULL, NULL, 0,
                        &output_size, NCRYPT_PAD_PKCS1_FLAG);
    ok(ret == ERROR_SUCCESS, "got %lx\n", ret);
    ok(output_size == 128, "got %ld\n", output_size);

    output_a = malloc(output_size);
    output_b = malloc(output_size);

    ret = NCryptEncrypt(key, data_to_encrypt, sizeof(data_to_encrypt), NULL, output_a, 12,
                        &output_size, NCRYPT_PAD_PKCS1_FLAG);
    ok(ret == NTE_BUFFER_TOO_SMALL, "got %lx\n", ret);

    ret = NCryptEncrypt(key, data_to_encrypt, sizeof(data_to_encrypt), NULL, output_a, output_size,
                        &output_size, NCRYPT_PAD_PKCS1_FLAG);
    ok(ret == ERROR_SUCCESS, "got %lx\n", ret);

    ret = NCryptEncrypt(key, data_to_encrypt, sizeof(data_to_encrypt), NULL, output_b, output_size,
                        &output_size, NCRYPT_PAD_PKCS1_FLAG);
    ok(ret == ERROR_SUCCESS, "got %lx\n", ret);
    ok(memcmp(output_a, output_b, 128), "expected to have different outputs.\n");

    NCryptFreeObject(key);
    free(output_a);
    free(output_b);

    NCryptFreeObject(prov);
}

START_TEST(ncrypt)
{
    test_key_import_rsa();
    test_free_object();
    test_get_property();
    test_set_property();
    test_create_persisted_key();
    test_finalize_key();
    test_verify_signature();
    test_NCryptIsAlgSupported();
    test_NCryptEncrypt();
}
