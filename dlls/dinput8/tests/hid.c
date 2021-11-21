/*
 * Copyright 2021 Rémi Bernon for CodeWeavers
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

#define DIRECTINPUT_VERSION 0x0800

#include <stdarg.h>
#include <stddef.h>

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winbase.h"
#include "winioctl.h"
#include "winternl.h"
#include "wincrypt.h"
#include "winreg.h"
#include "winsvc.h"
#include "winuser.h"
#include "winnls.h"

#include "mscat.h"
#include "mssip.h"
#include "ntsecapi.h"
#include "setupapi.h"
#include "cfgmgr32.h"
#include "newdev.h"

#include "objbase.h"

#define COBJMACROS
#include "wingdi.h"
#include "dinput.h"

#include "initguid.h"
#include "ddk/wdm.h"
#include "ddk/hidclass.h"
#include "ddk/hidsdi.h"
#include "ddk/hidpi.h"
#include "ddk/hidport.h"
#include "hidusage.h"
#include "devguid.h"

#include "wine/test.h"
#include "wine/mssign.h"

#include "driver_hid.h"

static HINSTANCE instance;
static BOOL localized; /* object names get translated */

#define EXPECT_VIDPID MAKELONG( 0x1209, 0x0001 )
static const WCHAR expect_vidpid_str[] = L"VID_1209&PID_0001";
static const GUID expect_guid_product = {EXPECT_VIDPID,0x0000,0x0000,{0x00,0x00,'P','I','D','V','I','D'}};
static const WCHAR expect_path[] = L"\\\\?\\hid#winetest#1&2fafeb0&";
static const WCHAR expect_path_end[] = L"&0000#{4d1e55b2-f16f-11cf-88cb-001111000030}";

static struct winetest_shared_data *test_data;
static HANDLE okfile;

static HRESULT (WINAPI *pSignerSign)( SIGNER_SUBJECT_INFO *subject, SIGNER_CERT *cert,
                                      SIGNER_SIGNATURE_INFO *signature, SIGNER_PROVIDER_INFO *provider,
                                      const WCHAR *timestamp, CRYPT_ATTRIBUTES *attr, void *sip_data );

static const WCHAR container_name[] = L"wine_testsign";

static const CERT_CONTEXT *testsign_sign( const WCHAR *filename )
{
    BYTE encoded_name[100], encoded_key_id[200], public_key_info_buffer[1000];
    BYTE hash_buffer[16], cert_buffer[1000], provider_nameA[100], serial[16];
    CERT_PUBLIC_KEY_INFO *public_key_info = (CERT_PUBLIC_KEY_INFO *)public_key_info_buffer;
    SIGNER_SIGNATURE_INFO signature = {sizeof(SIGNER_SIGNATURE_INFO)};
    SIGNER_CERT_STORE_INFO store = {sizeof(SIGNER_CERT_STORE_INFO)};
    SIGNER_ATTR_AUTHCODE authcode = {sizeof(SIGNER_ATTR_AUTHCODE)};
    SIGNER_SUBJECT_INFO subject = {sizeof(SIGNER_SUBJECT_INFO)};
    SIGNER_FILE_INFO file = {sizeof(SIGNER_FILE_INFO)};
    SIGNER_CERT signer = {sizeof(SIGNER_CERT)};
    CRYPT_KEY_PROV_INFO provider_info = {0};
    CRYPT_ALGORITHM_IDENTIFIER algid = {0};
    CERT_AUTHORITY_KEY_ID_INFO key_info;
    HCERTSTORE root_store, pub_store;
    CERT_INFO cert_info = {0};
    WCHAR provider_nameW[100];
    const CERT_CONTEXT *cert;
    CERT_EXTENSION extension;
    DWORD size, index = 0;
    HCRYPTPROV provider;
    HCRYPTKEY key;
    HRESULT hr;
    BOOL ret;

    ret = CryptAcquireContextW( &provider, container_name, NULL, PROV_RSA_FULL, CRYPT_NEWKEYSET );
    if (!ret && GetLastError() == NTE_EXISTS)
    {
        ret = CryptAcquireContextW( &provider, container_name, NULL, PROV_RSA_FULL, CRYPT_DELETEKEYSET );
        ok( ret, "Failed to delete container, error %#x\n", GetLastError() );
        ret = CryptAcquireContextW( &provider, container_name, NULL, PROV_RSA_FULL, CRYPT_NEWKEYSET );
    }
    ok( ret, "Failed to create container, error %#x\n", GetLastError() );

    ret = CryptGenKey( provider, AT_SIGNATURE, CRYPT_EXPORTABLE, &key );
    ok( ret, "Failed to create key, error %#x\n", GetLastError() );
    ret = CryptDestroyKey( key );
    ok( ret, "Failed to destroy key, error %#x\n", GetLastError() );
    ret = CryptGetUserKey( provider, AT_SIGNATURE, &key );
    ok( ret, "Failed to get user key, error %#x\n", GetLastError() );
    ret = CryptDestroyKey( key );
    ok( ret, "Failed to destroy key, error %#x\n", GetLastError() );

    size = sizeof(encoded_name);
    ret = CertStrToNameW( X509_ASN_ENCODING, L"CN=winetest_cert", CERT_X500_NAME_STR, NULL,
                          encoded_name, &size, NULL );
    ok( ret, "Failed to convert name, error %#x\n", GetLastError() );
    key_info.CertIssuer.cbData = size;
    key_info.CertIssuer.pbData = encoded_name;

    size = sizeof(public_key_info_buffer);
    ret = CryptExportPublicKeyInfo( provider, AT_SIGNATURE, X509_ASN_ENCODING, public_key_info, &size );
    ok( ret, "Failed to export public key, error %#x\n", GetLastError() );
    cert_info.SubjectPublicKeyInfo = *public_key_info;

    size = sizeof(hash_buffer);
    ret = CryptHashPublicKeyInfo( provider, CALG_MD5, 0, X509_ASN_ENCODING, public_key_info, hash_buffer, &size );
    ok( ret, "Failed to hash public key, error %#x\n", GetLastError() );

    key_info.KeyId.cbData = size;
    key_info.KeyId.pbData = hash_buffer;

    RtlGenRandom( serial, sizeof(serial) );
    key_info.CertSerialNumber.cbData = sizeof(serial);
    key_info.CertSerialNumber.pbData = serial;

    size = sizeof(encoded_key_id);
    ret = CryptEncodeObject( X509_ASN_ENCODING, X509_AUTHORITY_KEY_ID, &key_info, encoded_key_id, &size );
    ok( ret, "Failed to convert name, error %#x\n", GetLastError() );

    extension.pszObjId = (char *)szOID_AUTHORITY_KEY_IDENTIFIER;
    extension.fCritical = TRUE;
    extension.Value.cbData = size;
    extension.Value.pbData = encoded_key_id;

    cert_info.dwVersion = CERT_V3;
    cert_info.SerialNumber = key_info.CertSerialNumber;
    cert_info.SignatureAlgorithm.pszObjId = (char *)szOID_RSA_SHA1RSA;
    cert_info.Issuer = key_info.CertIssuer;
    GetSystemTimeAsFileTime( &cert_info.NotBefore );
    GetSystemTimeAsFileTime( &cert_info.NotAfter );
    cert_info.NotAfter.dwHighDateTime += 1;
    cert_info.Subject = key_info.CertIssuer;
    cert_info.cExtension = 1;
    cert_info.rgExtension = &extension;
    algid.pszObjId = (char *)szOID_RSA_SHA1RSA;
    size = sizeof(cert_buffer);
    ret = CryptSignAndEncodeCertificate( provider, AT_SIGNATURE, X509_ASN_ENCODING, X509_CERT_TO_BE_SIGNED,
                                         &cert_info, &algid, NULL, cert_buffer, &size );
    ok( ret, "Failed to create certificate, error %#x\n", GetLastError() );

    cert = CertCreateCertificateContext( X509_ASN_ENCODING, cert_buffer, size );
    ok( !!cert, "Failed to create context, error %#x\n", GetLastError() );

    size = sizeof(provider_nameA);
    ret = CryptGetProvParam( provider, PP_NAME, provider_nameA, &size, 0 );
    ok( ret, "Failed to get prov param, error %#x\n", GetLastError() );
    MultiByteToWideChar( CP_ACP, 0, (char *)provider_nameA, -1, provider_nameW, ARRAY_SIZE(provider_nameW) );

    provider_info.pwszContainerName = (WCHAR *)container_name;
    provider_info.pwszProvName = provider_nameW;
    provider_info.dwProvType = PROV_RSA_FULL;
    provider_info.dwKeySpec = AT_SIGNATURE;
    ret = CertSetCertificateContextProperty( cert, CERT_KEY_PROV_INFO_PROP_ID, 0, &provider_info );
    ok( ret, "Failed to set provider info, error %#x\n", GetLastError() );

    ret = CryptReleaseContext( provider, 0 );
    ok( ret, "failed to release context, error %u\n", GetLastError() );

    root_store = CertOpenStore( CERT_STORE_PROV_SYSTEM_REGISTRY_W, 0, 0, CERT_SYSTEM_STORE_LOCAL_MACHINE, L"root" );
    if (!root_store && GetLastError() == ERROR_ACCESS_DENIED)
    {
        win_skip( "Failed to open root store.\n" );
        ret = CertFreeCertificateContext( cert );
        ok( ret, "Failed to free certificate, error %u\n", GetLastError() );
        return NULL;
    }
    ok( !!root_store, "Failed to open store, error %u\n", GetLastError() );
    ret = CertAddCertificateContextToStore( root_store, cert, CERT_STORE_ADD_ALWAYS, NULL );
    if (!ret && GetLastError() == ERROR_ACCESS_DENIED)
    {
        win_skip( "Failed to add self-signed certificate to store.\n" );
        ret = CertFreeCertificateContext( cert );
        ok( ret, "Failed to free certificate, error %u\n", GetLastError() );
        ret = CertCloseStore( root_store, CERT_CLOSE_STORE_CHECK_FLAG );
        ok( ret, "Failed to close store, error %u\n", GetLastError() );
        return NULL;
    }
    ok( ret, "Failed to add certificate, error %u\n", GetLastError() );
    ret = CertCloseStore( root_store, CERT_CLOSE_STORE_CHECK_FLAG );
    ok( ret, "Failed to close store, error %u\n", GetLastError() );

    pub_store = CertOpenStore( CERT_STORE_PROV_SYSTEM_REGISTRY_W, 0, 0,
                               CERT_SYSTEM_STORE_LOCAL_MACHINE, L"trustedpublisher" );
    ok( !!pub_store, "Failed to open store, error %u\n", GetLastError() );
    ret = CertAddCertificateContextToStore( pub_store, cert, CERT_STORE_ADD_ALWAYS, NULL );
    ok( ret, "Failed to add certificate, error %u\n", GetLastError() );
    ret = CertCloseStore( pub_store, CERT_CLOSE_STORE_CHECK_FLAG );
    ok( ret, "Failed to close store, error %u\n", GetLastError() );

    subject.dwSubjectChoice = 1;
    subject.pdwIndex = &index;
    subject.pSignerFileInfo = &file;
    file.pwszFileName = (WCHAR *)filename;
    signer.dwCertChoice = 2;
    signer.pCertStoreInfo = &store;
    store.pSigningCert = cert;
    store.dwCertPolicy = 0;
    signature.algidHash = CALG_SHA_256;
    signature.dwAttrChoice = SIGNER_AUTHCODE_ATTR;
    signature.pAttrAuthcode = &authcode;
    authcode.pwszName = L"";
    authcode.pwszInfo = L"";
    hr = pSignerSign( &subject, &signer, &signature, NULL, NULL, NULL, NULL );
    todo_wine
    ok( hr == S_OK || broken( hr == NTE_BAD_ALGID ) /* < 7 */, "Failed to sign, hr %#x\n", hr );

    return cert;
}

static void testsign_cleanup( const CERT_CONTEXT *cert )
{
    HCERTSTORE root_store, pub_store;
    const CERT_CONTEXT *store_cert;
    HCRYPTPROV provider;
    BOOL ret;

    root_store = CertOpenStore( CERT_STORE_PROV_SYSTEM_REGISTRY_W, 0, 0, CERT_SYSTEM_STORE_LOCAL_MACHINE, L"root" );
    ok( !!root_store, "Failed to open store, error %u\n", GetLastError() );
    store_cert = CertFindCertificateInStore( root_store, X509_ASN_ENCODING, 0, CERT_FIND_EXISTING, cert, NULL );
    ok( !!store_cert, "Failed to find root certificate, error %u\n", GetLastError() );
    ret = CertDeleteCertificateFromStore( store_cert );
    ok( ret, "Failed to remove certificate, error %u\n", GetLastError() );
    ret = CertCloseStore( root_store, CERT_CLOSE_STORE_CHECK_FLAG );
    ok( ret, "Failed to close store, error %u\n", GetLastError() );

    pub_store = CertOpenStore( CERT_STORE_PROV_SYSTEM_REGISTRY_W, 0, 0,
                               CERT_SYSTEM_STORE_LOCAL_MACHINE, L"trustedpublisher" );
    ok( !!pub_store, "Failed to open store, error %u\n", GetLastError() );
    store_cert = CertFindCertificateInStore( pub_store, X509_ASN_ENCODING, 0, CERT_FIND_EXISTING, cert, NULL );
    ok( !!store_cert, "Failed to find publisher certificate, error %u\n", GetLastError() );
    ret = CertDeleteCertificateFromStore( store_cert );
    ok( ret, "Failed to remove certificate, error %u\n", GetLastError() );
    ret = CertCloseStore( pub_store, CERT_CLOSE_STORE_CHECK_FLAG );
    ok( ret, "Failed to close store, error %u\n", GetLastError() );

    ret = CertFreeCertificateContext( cert );
    ok( ret, "Failed to free certificate, error %u\n", GetLastError() );

    ret = CryptAcquireContextW( &provider, container_name, NULL, PROV_RSA_FULL, CRYPT_DELETEKEYSET );
    ok( ret, "Failed to delete container, error %#x\n", GetLastError() );
}

static void load_resource( const WCHAR *name, WCHAR *filename )
{
    static WCHAR path[MAX_PATH];
    DWORD written;
    HANDLE file;
    HRSRC res;
    void *ptr;

    GetTempPathW( ARRAY_SIZE(path), path );
    GetTempFileNameW( path, name, 0, filename );

    file = CreateFileW( filename, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, 0 );
    ok( file != INVALID_HANDLE_VALUE, "failed to create %s, error %u\n", debugstr_w(filename), GetLastError() );

    res = FindResourceW( NULL, name, L"TESTDLL" );
    ok( res != 0, "couldn't find resource\n" );
    ptr = LockResource( LoadResource( GetModuleHandleW( NULL ), res ) );
    WriteFile( file, ptr, SizeofResource( GetModuleHandleW( NULL ), res ), &written, NULL );
    ok( written == SizeofResource( GetModuleHandleW( NULL ), res ), "couldn't write resource\n" );
    CloseHandle( file );
}

#ifdef __i386__
#define EXT "x86"
#elif defined(__x86_64__)
#define EXT "amd64"
#elif defined(__arm__)
#define EXT "arm"
#elif defined(__aarch64__)
#define EXT "arm64"
#else
#define EXT
#endif

static const char inf_text[] =
    "[Version]\n"
    "Signature=$Chicago$\n"
    "ClassGuid={4d36e97d-e325-11ce-bfc1-08002be10318}\n"
    "CatalogFile=winetest.cat\n"
    "DriverVer=09/21/2006,6.0.5736.1\n"

    "[Manufacturer]\n"
    "Wine=mfg_section,NT" EXT "\n"

    "[mfg_section.NT" EXT "]\n"
    "Wine test root driver=device_section,test_hardware_id\n"

    "[device_section.NT" EXT "]\n"
    "CopyFiles=file_section\n"

    "[device_section.NT" EXT ".Services]\n"
    "AddService=winetest,0x2,svc_section\n"

    "[file_section]\n"
    "winetest.sys\n"

    "[SourceDisksFiles]\n"
    "winetest.sys=1\n"

    "[SourceDisksNames]\n"
    "1=,winetest.sys\n"

    "[DestinationDirs]\n"
    "DefaultDestDir=12\n"

    "[svc_section]\n"
    "ServiceBinary=%12%\\winetest.sys\n"
    "ServiceType=1\n"
    "StartType=3\n"
    "ErrorControl=1\n"
    "LoadOrderGroup=Extended Base\n"
    "DisplayName=\"winetest bus driver\"\n"
    "; they don't sleep anymore, on the beach\n";

static void add_file_to_catalog( HANDLE catalog, const WCHAR *file )
{
    SIP_SUBJECTINFO subject_info = {sizeof(SIP_SUBJECTINFO)};
    SIP_INDIRECT_DATA *indirect_data;
    const WCHAR *filepart = file;
    CRYPTCATMEMBER *member;
    WCHAR hash_buffer[100];
    GUID subject_guid;
    unsigned int i;
    DWORD size;
    BOOL ret;

    ret = CryptSIPRetrieveSubjectGuidForCatalogFile( file, NULL, &subject_guid );
    todo_wine
    ok( ret, "Failed to get subject guid, error %u\n", GetLastError() );

    size = 0;
    subject_info.pgSubjectType = &subject_guid;
    subject_info.pwsFileName = file;
    subject_info.DigestAlgorithm.pszObjId = (char *)szOID_OIWSEC_sha1;
    subject_info.dwFlags = SPC_INC_PE_RESOURCES_FLAG | SPC_INC_PE_IMPORT_ADDR_TABLE_FLAG |
                           SPC_EXC_PE_PAGE_HASHES_FLAG | 0x10000;
    ret = CryptSIPCreateIndirectData( &subject_info, &size, NULL );
    todo_wine
    ok( ret, "Failed to get indirect data size, error %u\n", GetLastError() );

    indirect_data = malloc( size );
    ret = CryptSIPCreateIndirectData( &subject_info, &size, indirect_data );
    todo_wine
    ok( ret, "Failed to get indirect data, error %u\n", GetLastError() );
    if (ret)
    {
        memset( hash_buffer, 0, sizeof(hash_buffer) );
        for (i = 0; i < indirect_data->Digest.cbData; ++i)
            swprintf( &hash_buffer[i * 2], 2, L"%02X", indirect_data->Digest.pbData[i] );

        member = CryptCATPutMemberInfo( catalog, (WCHAR *)file, hash_buffer, &subject_guid,
                                        0, size, (BYTE *)indirect_data );
        ok( !!member, "Failed to write member, error %u\n", GetLastError() );

        if (wcsrchr( file, '\\' )) filepart = wcsrchr( file, '\\' ) + 1;

        ret = !!CryptCATPutAttrInfo( catalog, member, (WCHAR *)L"File",
                                     CRYPTCAT_ATTR_NAMEASCII | CRYPTCAT_ATTR_DATAASCII | CRYPTCAT_ATTR_AUTHENTICATED,
                                     (wcslen( filepart ) + 1) * 2, (BYTE *)filepart );
        ok( ret, "Failed to write attr, error %u\n", GetLastError() );

        ret = !!CryptCATPutAttrInfo( catalog, member, (WCHAR *)L"OSAttr",
                                     CRYPTCAT_ATTR_NAMEASCII | CRYPTCAT_ATTR_DATAASCII | CRYPTCAT_ATTR_AUTHENTICATED,
                                     sizeof(L"2:6.0"), (BYTE *)L"2:6.0" );
        ok( ret, "Failed to write attr, error %u\n", GetLastError() );
    }
}

static void unload_driver( SC_HANDLE service )
{
    SERVICE_STATUS status;

    ControlService( service, SERVICE_CONTROL_STOP, &status );
    while (status.dwCurrentState == SERVICE_STOP_PENDING)
    {
        BOOL ret;
        Sleep( 100 );
        ret = QueryServiceStatus( service, &status );
        ok( ret, "QueryServiceStatus failed: %u\n", GetLastError() );
    }
    ok( status.dwCurrentState == SERVICE_STOPPED, "expected SERVICE_STOPPED, got %d\n", status.dwCurrentState );

    DeleteService( service );
    CloseServiceHandle( service );
}

static void pnp_driver_stop(void)
{
    SP_DEVINFO_DATA device = {sizeof(SP_DEVINFO_DATA)};
    WCHAR path[MAX_PATH], dest[MAX_PATH], *filepart;
    SC_HANDLE manager, service;
    char buffer[512];
    HDEVINFO set;
    HANDLE file;
    DWORD size;
    BOOL ret;

    set = SetupDiCreateDeviceInfoList( NULL, NULL );
    ok( set != INVALID_HANDLE_VALUE, "failed to create device list, error %u\n", GetLastError() );

    ret = SetupDiOpenDeviceInfoW( set, L"root\\winetest\\0", NULL, 0, &device );
    if (!ret && GetLastError() == ERROR_NO_SUCH_DEVINST)
    {
        ret = SetupDiDestroyDeviceInfoList( set );
        ok( ret, "failed to destroy set, error %u\n", GetLastError() );
        return;
    }
    ok( ret, "failed to open device, error %u\n", GetLastError() );

    ret = SetupDiCallClassInstaller( DIF_REMOVE, set, &device );
    ok( ret, "failed to remove device, error %u\n", GetLastError() );

    file = CreateFileW( L"\\\\?\\root#winetest#0#{deadbeef-29ef-4538-a5fd-b69573a362c0}", 0, 0,
                        NULL, OPEN_EXISTING, 0, NULL );
    ok( file == INVALID_HANDLE_VALUE, "expected failure\n" );
    ok( GetLastError() == ERROR_FILE_NOT_FOUND, "got error %u\n", GetLastError() );

    ret = SetupDiDestroyDeviceInfoList( set );
    ok( ret, "failed to destroy set, error %u\n", GetLastError() );

    /* Windows stops the service but does not delete it. */
    manager = OpenSCManagerW( NULL, NULL, SC_MANAGER_CONNECT );
    ok( !!manager, "failed to open service manager, error %u\n", GetLastError() );

    service = OpenServiceW( manager, L"winetest", SERVICE_STOP | DELETE );
    if (service) unload_driver( service );
    else ok( GetLastError() == ERROR_SERVICE_DOES_NOT_EXIST, "got error %u\n", GetLastError() );

    CloseServiceHandle( manager );

    SetFilePointer( okfile, 0, NULL, FILE_BEGIN );
    do
    {
        ReadFile( okfile, buffer, sizeof(buffer), &size, NULL );
        printf( "%.*s", size, buffer );
    } while (size == sizeof(buffer));
    SetFilePointer( okfile, 0, NULL, FILE_BEGIN );
    SetEndOfFile( okfile );

    winetest_add_failures( InterlockedExchange( &test_data->failures, 0 ) );
    winetest_add_failures( InterlockedExchange( &test_data->todo_failures, 0 ) );

    GetFullPathNameW( L"winetest.inf", ARRAY_SIZE(path), path, NULL );
    ret = SetupCopyOEMInfW( path, NULL, 0, 0, dest, ARRAY_SIZE(dest), NULL, &filepart );
    ok( ret, "Failed to copy INF, error %u\n", GetLastError() );
    ret = SetupUninstallOEMInfW( filepart, 0, NULL );
    ok( ret, "Failed to uninstall INF, error %u\n", GetLastError() );

    ret = DeleteFileW( L"winetest.cat" );
    ok( ret, "Failed to delete file, error %u\n", GetLastError() );
    ret = DeleteFileW( L"winetest.inf" );
    ok( ret, "Failed to delete file, error %u\n", GetLastError() );
    ret = DeleteFileW( L"winetest.sys" );
    ok( ret, "Failed to delete file, error %u\n", GetLastError() );
    /* Windows 10 apparently deletes the image in SetupUninstallOEMInf(). */
    ret = DeleteFileW( L"C:/windows/system32/drivers/winetest.sys" );
    ok( ret || GetLastError() == ERROR_FILE_NOT_FOUND, "Failed to delete file, error %u\n", GetLastError() );
}

static BOOL pnp_driver_start( const WCHAR *resource )
{
    static const WCHAR hardware_id[] = L"test_hardware_id\0";
    SP_DEVINFO_DATA device = {sizeof(SP_DEVINFO_DATA)};
    WCHAR path[MAX_PATH], filename[MAX_PATH];
    SC_HANDLE manager, service;
    const CERT_CONTEXT *cert;
    BOOL ret, need_reboot;
    HANDLE catalog;
    HDEVINFO set;
    FILE *f;

    load_resource( resource, filename );
    ret = MoveFileExW( filename, L"winetest.sys", MOVEFILE_COPY_ALLOWED | MOVEFILE_REPLACE_EXISTING );
    ok( ret, "failed to move file, error %u\n", GetLastError() );

    f = fopen( "winetest.inf", "w" );
    ok( !!f, "failed to open winetest.inf: %s\n", strerror( errno ) );
    fputs( inf_text, f );
    fclose( f );

    /* Create the catalog file. */

    catalog = CryptCATOpen( (WCHAR *)L"winetest.cat", CRYPTCAT_OPEN_CREATENEW, 0, CRYPTCAT_VERSION_1, 0 );
    ok( catalog != INVALID_HANDLE_VALUE, "Failed to create catalog, error %u\n", GetLastError() );

    add_file_to_catalog( catalog, L"winetest.sys" );
    add_file_to_catalog( catalog, L"winetest.inf" );

    ret = CryptCATPersistStore( catalog );
    todo_wine
    ok( ret, "Failed to write catalog, error %u\n", GetLastError() );

    ret = CryptCATClose( catalog );
    ok( ret, "Failed to close catalog, error %u\n", GetLastError() );

    if (!(cert = testsign_sign( L"winetest.cat" )))
    {
        ret = DeleteFileW( L"winetest.cat" );
        ok( ret, "Failed to delete file, error %u\n", GetLastError() );
        ret = DeleteFileW( L"winetest.inf" );
        ok( ret, "Failed to delete file, error %u\n", GetLastError() );
        ret = DeleteFileW( L"winetest.sys" );
        ok( ret, "Failed to delete file, error %u\n", GetLastError() );
        return FALSE;
    }

    /* Install the driver. */

    set = SetupDiCreateDeviceInfoList( NULL, NULL );
    ok( set != INVALID_HANDLE_VALUE, "failed to create device list, error %u\n", GetLastError() );

    ret = SetupDiCreateDeviceInfoW( set, L"root\\winetest\\0", &GUID_NULL, NULL, NULL, 0, &device );
    ok( ret, "failed to create device, error %#x\n", GetLastError() );

    ret = SetupDiSetDeviceRegistryPropertyW( set, &device, SPDRP_HARDWAREID, (const BYTE *)hardware_id,
                                             sizeof(hardware_id) );
    ok( ret, "failed to create set hardware ID, error %u\n", GetLastError() );

    ret = SetupDiCallClassInstaller( DIF_REGISTERDEVICE, set, &device );
    ok( ret, "failed to register device, error %u\n", GetLastError() );

    ret = SetupDiDestroyDeviceInfoList( set );
    ok( ret, "failed to destroy set, error %u\n", GetLastError() );

    GetFullPathNameW( L"winetest.inf", ARRAY_SIZE(path), path, NULL );

    ret = UpdateDriverForPlugAndPlayDevicesW( NULL, hardware_id, path, INSTALLFLAG_FORCE, &need_reboot );
    ok( ret, "failed to install device, error %u\n", GetLastError() );
    ok( !need_reboot, "expected no reboot necessary\n" );

    testsign_cleanup( cert );

    /* Check that the service is created and started. */
    manager = OpenSCManagerW( NULL, NULL, SC_MANAGER_CONNECT );
    ok( !!manager, "failed to open service manager, error %u\n", GetLastError() );

    service = OpenServiceW( manager, L"winetest", SERVICE_START );
    ok( !!service, "failed to open service, error %u\n", GetLastError() );

    ret = StartServiceW( service, 0, NULL );
    ok( !ret, "service didn't start automatically\n" );
    if (!ret && GetLastError() != ERROR_SERVICE_ALREADY_RUNNING)
    {
        /* If Secure Boot is enabled or the machine is 64-bit, it will reject an unsigned driver. */
        ok( GetLastError() == ERROR_DRIVER_BLOCKED || GetLastError() == ERROR_INVALID_IMAGE_HASH,
            "unexpected error %u\n", GetLastError() );
        win_skip( "Failed to start service; probably your machine doesn't accept unsigned drivers.\n" );
    }

    CloseServiceHandle( service );
    CloseServiceHandle( manager );
    return ret || GetLastError() == ERROR_SERVICE_ALREADY_RUNNING;
}

#define check_member_( file, line, val, exp, fmt, member )                                         \
    ok_( file, line )((val).member == (exp).member, "got " #member " " fmt ", expected " fmt "\n", \
                      (val).member, (exp).member)
#define check_member( val, exp, fmt, member )                                                      \
    check_member_( __FILE__, __LINE__, val, exp, fmt, member )

#define check_member_guid_( file, line, val, exp, member )                                              \
    ok_( file, line )(IsEqualGUID( &(val).member, &(exp).member ), "got " #member " %s, expected %s\n", \
                      debugstr_guid( &(val).member ), debugstr_guid( &(exp).member ))
#define check_member_guid( val, exp, member )                                                      \
    check_member_guid_( __FILE__, __LINE__, val, exp, member )

#define check_member_wstr_( file, line, val, exp, member )                                         \
    ok_( file, line )(!wcscmp( (val).member, (exp).member ), "got " #member " %s, expected %s\n",  \
                      debugstr_w((val).member), debugstr_w((exp).member))
#define check_member_wstr( val, exp, member )                                                      \
    check_member_wstr_( __FILE__, __LINE__, val, exp, member )

#define check_hidp_caps( a, b ) check_hidp_caps_( __LINE__, a, b )
static inline void check_hidp_caps_( int line, HIDP_CAPS *caps, const HIDP_CAPS *exp )
{
    check_member_( __FILE__, line, *caps, *exp, "%04x", Usage );
    check_member_( __FILE__, line, *caps, *exp, "%04x", UsagePage );
    check_member_( __FILE__, line, *caps, *exp, "%d", InputReportByteLength );
    check_member_( __FILE__, line, *caps, *exp, "%d", OutputReportByteLength );
    check_member_( __FILE__, line, *caps, *exp, "%d", FeatureReportByteLength );
    check_member_( __FILE__, line, *caps, *exp, "%d", NumberLinkCollectionNodes );
    check_member_( __FILE__, line, *caps, *exp, "%d", NumberInputButtonCaps );
    check_member_( __FILE__, line, *caps, *exp, "%d", NumberInputValueCaps );
    check_member_( __FILE__, line, *caps, *exp, "%d", NumberInputDataIndices );
    check_member_( __FILE__, line, *caps, *exp, "%d", NumberOutputButtonCaps );
    check_member_( __FILE__, line, *caps, *exp, "%d", NumberOutputValueCaps );
    check_member_( __FILE__, line, *caps, *exp, "%d", NumberOutputDataIndices );
    check_member_( __FILE__, line, *caps, *exp, "%d", NumberFeatureButtonCaps );
    check_member_( __FILE__, line, *caps, *exp, "%d", NumberFeatureValueCaps );
    check_member_( __FILE__, line, *caps, *exp, "%d", NumberFeatureDataIndices );
}

#define check_hidp_link_collection_node( a, b ) check_hidp_link_collection_node_( __LINE__, a, b )
static inline void check_hidp_link_collection_node_( int line, HIDP_LINK_COLLECTION_NODE *node,
                                                     const HIDP_LINK_COLLECTION_NODE *exp )
{
    check_member_( __FILE__, line, *node, *exp, "%04x", LinkUsage );
    check_member_( __FILE__, line, *node, *exp, "%04x", LinkUsagePage );
    check_member_( __FILE__, line, *node, *exp, "%d", Parent );
    check_member_( __FILE__, line, *node, *exp, "%d", NumberOfChildren );
    check_member_( __FILE__, line, *node, *exp, "%d", NextSibling );
    check_member_( __FILE__, line, *node, *exp, "%d", FirstChild );
    check_member_( __FILE__, line, *node, *exp, "%d", CollectionType );
    check_member_( __FILE__, line, *node, *exp, "%d", IsAlias );
}

#define check_hidp_button_caps( a, b ) check_hidp_button_caps_( __LINE__, a, b )
static inline void check_hidp_button_caps_( int line, HIDP_BUTTON_CAPS *caps, const HIDP_BUTTON_CAPS *exp )
{
    check_member_( __FILE__, line, *caps, *exp, "%04x", UsagePage );
    check_member_( __FILE__, line, *caps, *exp, "%d", ReportID );
    check_member_( __FILE__, line, *caps, *exp, "%d", IsAlias );
    check_member_( __FILE__, line, *caps, *exp, "%d", BitField );
    check_member_( __FILE__, line, *caps, *exp, "%d", LinkCollection );
    check_member_( __FILE__, line, *caps, *exp, "%04x", LinkUsage );
    check_member_( __FILE__, line, *caps, *exp, "%04x", LinkUsagePage );
    check_member_( __FILE__, line, *caps, *exp, "%d", IsRange );
    check_member_( __FILE__, line, *caps, *exp, "%d", IsStringRange );
    check_member_( __FILE__, line, *caps, *exp, "%d", IsDesignatorRange );
    check_member_( __FILE__, line, *caps, *exp, "%d", IsAbsolute );

    if (!caps->IsRange && !exp->IsRange)
    {
        check_member_( __FILE__, line, *caps, *exp, "%04x", NotRange.Usage );
        check_member_( __FILE__, line, *caps, *exp, "%d", NotRange.DataIndex );
    }
    else if (caps->IsRange && exp->IsRange)
    {
        check_member_( __FILE__, line, *caps, *exp, "%04x", Range.UsageMin );
        check_member_( __FILE__, line, *caps, *exp, "%04x", Range.UsageMax );
        check_member_( __FILE__, line, *caps, *exp, "%d", Range.DataIndexMin );
        check_member_( __FILE__, line, *caps, *exp, "%d", Range.DataIndexMax );
    }

    if (!caps->IsRange && !exp->IsRange)
        check_member_( __FILE__, line, *caps, *exp, "%d", NotRange.StringIndex );
    else if (caps->IsStringRange && exp->IsStringRange)
    {
        check_member_( __FILE__, line, *caps, *exp, "%d", Range.StringMin );
        check_member_( __FILE__, line, *caps, *exp, "%d", Range.StringMax );
    }

    if (!caps->IsDesignatorRange && !exp->IsDesignatorRange)
        check_member_( __FILE__, line, *caps, *exp, "%d", NotRange.DesignatorIndex );
    else if (caps->IsDesignatorRange && exp->IsDesignatorRange)
    {
        check_member_( __FILE__, line, *caps, *exp, "%d", Range.DesignatorMin );
        check_member_( __FILE__, line, *caps, *exp, "%d", Range.DesignatorMax );
    }
}

#define check_hidp_value_caps( a, b ) check_hidp_value_caps_( __LINE__, a, b )
static inline void check_hidp_value_caps_( int line, HIDP_VALUE_CAPS *caps, const HIDP_VALUE_CAPS *exp )
{
    check_member_( __FILE__, line, *caps, *exp, "%04x", UsagePage );
    check_member_( __FILE__, line, *caps, *exp, "%d", ReportID );
    check_member_( __FILE__, line, *caps, *exp, "%d", IsAlias );
    check_member_( __FILE__, line, *caps, *exp, "%d", BitField );
    check_member_( __FILE__, line, *caps, *exp, "%d", LinkCollection );
    check_member_( __FILE__, line, *caps, *exp, "%d", LinkUsage );
    check_member_( __FILE__, line, *caps, *exp, "%d", LinkUsagePage );
    check_member_( __FILE__, line, *caps, *exp, "%d", IsRange );
    check_member_( __FILE__, line, *caps, *exp, "%d", IsStringRange );
    check_member_( __FILE__, line, *caps, *exp, "%d", IsDesignatorRange );
    check_member_( __FILE__, line, *caps, *exp, "%d", IsAbsolute );

    check_member_( __FILE__, line, *caps, *exp, "%d", HasNull );
    check_member_( __FILE__, line, *caps, *exp, "%d", BitSize );
    check_member_( __FILE__, line, *caps, *exp, "%d", ReportCount );
    check_member_( __FILE__, line, *caps, *exp, "%d", UnitsExp );
    check_member_( __FILE__, line, *caps, *exp, "%d", Units );
    check_member_( __FILE__, line, *caps, *exp, "%d", LogicalMin );
    check_member_( __FILE__, line, *caps, *exp, "%d", LogicalMax );
    check_member_( __FILE__, line, *caps, *exp, "%d", PhysicalMin );
    check_member_( __FILE__, line, *caps, *exp, "%d", PhysicalMax );

    if (!caps->IsRange && !exp->IsRange)
    {
        check_member_( __FILE__, line, *caps, *exp, "%04x", NotRange.Usage );
        check_member_( __FILE__, line, *caps, *exp, "%d", NotRange.DataIndex );
    }
    else if (caps->IsRange && exp->IsRange)
    {
        check_member_( __FILE__, line, *caps, *exp, "%04x", Range.UsageMin );
        check_member_( __FILE__, line, *caps, *exp, "%04x", Range.UsageMax );
        check_member_( __FILE__, line, *caps, *exp, "%d", Range.DataIndexMin );
        check_member_( __FILE__, line, *caps, *exp, "%d", Range.DataIndexMax );
    }

    if (!caps->IsRange && !exp->IsRange)
        check_member_( __FILE__, line, *caps, *exp, "%d", NotRange.StringIndex );
    else if (caps->IsStringRange && exp->IsStringRange)
    {
        check_member_( __FILE__, line, *caps, *exp, "%d", Range.StringMin );
        check_member_( __FILE__, line, *caps, *exp, "%d", Range.StringMax );
    }

    if (!caps->IsDesignatorRange && !exp->IsDesignatorRange)
        check_member_( __FILE__, line, *caps, *exp, "%d", NotRange.DesignatorIndex );
    else if (caps->IsDesignatorRange && exp->IsDesignatorRange)
    {
        check_member_( __FILE__, line, *caps, *exp, "%d", Range.DesignatorMin );
        check_member_( __FILE__, line, *caps, *exp, "%d", Range.DesignatorMax );
    }
}

static BOOL sync_ioctl( HANDLE file, DWORD code, void *in_buf, DWORD in_len, void *out_buf, DWORD *ret_len )
{
    OVERLAPPED ovl = {0};
    DWORD out_len = ret_len ? *ret_len : 0;
    BOOL ret;

    ovl.hEvent = CreateEventW( NULL, TRUE, FALSE, NULL );
    ret = DeviceIoControl( file, code, in_buf, in_len, out_buf, out_len, &out_len, &ovl );
    if (!ret && GetLastError() == ERROR_IO_PENDING)
        ret = GetOverlappedResult( file, &ovl, &out_len, TRUE );
    CloseHandle( ovl.hEvent );

    if (ret_len) *ret_len = out_len;
    return ret;
}

#define set_hid_expect( a, b, c ) set_hid_expect_( __LINE__, a, b, c )
static void set_hid_expect_( int line, HANDLE file, struct hid_expect *expect, DWORD expect_size )
{
    const char *source_file;
    BOOL ret;
    int i;

    source_file = strrchr( __FILE__, '/' );
    if (!source_file) source_file = strrchr( __FILE__, '\\' );
    if (!source_file) source_file = __FILE__;
    else source_file++;

    for (i = 0; i < expect_size / sizeof(struct hid_expect); ++i)
        snprintf( expect[i].context, ARRAY_SIZE(expect[i].context), "%s:%d", source_file, line );

    ret = sync_ioctl( file, IOCTL_WINETEST_HID_SET_EXPECT, expect, expect_size, NULL, 0 );
    ok( ret, "IOCTL_WINETEST_HID_SET_EXPECT failed, last error %u\n", GetLastError() );
}

#define send_hid_input( a, b, c ) send_hid_input_( __LINE__, a, b, c )
static void send_hid_input_( int line, HANDLE file, struct hid_expect *expect, DWORD expect_size )
{
    const char *source_file;
    BOOL ret;
    int i;

    source_file = strrchr( __FILE__, '/' );
    if (!source_file) source_file = strrchr( __FILE__, '\\' );
    if (!source_file) source_file = __FILE__;
    else source_file++;

    for (i = 0; i < expect_size / sizeof(struct hid_expect); ++i)
        snprintf( expect[i].context, ARRAY_SIZE(expect[i].context), "%s:%d", source_file, line );

    ret = sync_ioctl( file, IOCTL_WINETEST_HID_SEND_INPUT, expect, expect_size, NULL, 0 );
    ok( ret, "IOCTL_WINETEST_HID_SEND_INPUT failed, last error %u\n", GetLastError() );
}

static void test_hidp_get_input( HANDLE file, int report_id, ULONG report_len, PHIDP_PREPARSED_DATA preparsed )
{
    struct hid_expect expect[] =
    {
        {
            .code = IOCTL_HID_GET_INPUT_REPORT,
            .report_id = report_id,
            .report_len = report_len - (report_id ? 0 : 1),
            .report_buf = {report_id ? report_id : 0xa5,0xa5,2},
            .ret_length = 3,
            .ret_status = STATUS_SUCCESS,
        },
        {
            .code = IOCTL_HID_GET_INPUT_REPORT,
            .report_id = report_id,
            .report_len = 2 * report_len - (report_id ? 0 : 1),
            .report_buf = {report_id ? report_id : 0xa5,0xa5,1},
            .ret_length = 3,
            .ret_status = STATUS_SUCCESS,
        },
    };

    char buffer[200], report[200];
    NTSTATUS status;
    ULONG length;
    BOOL ret;

    memset( report, 0xcd, sizeof(report) );
    status = HidP_InitializeReportForID( HidP_Input, report_id, preparsed, report, report_len );
    ok( status == HIDP_STATUS_SUCCESS, "HidP_InitializeReportForID returned %#x\n", status );

    SetLastError( 0xdeadbeef );
    ret = HidD_GetInputReport( file, report, 0 );
    ok( !ret, "HidD_GetInputReport succeeded\n" );
    ok( GetLastError() == ERROR_INVALID_USER_BUFFER, "HidD_GetInputReport returned error %u\n", GetLastError() );

    SetLastError( 0xdeadbeef );
    ret = HidD_GetInputReport( file, report, report_len - 1 );
    ok( !ret, "HidD_GetInputReport succeeded\n" );
    ok( GetLastError() == ERROR_INVALID_PARAMETER || broken( GetLastError() == ERROR_CRC ),
        "HidD_GetInputReport returned error %u\n", GetLastError() );

    if (!report_id)
    {
        struct hid_expect broken_expect =
        {
            .code = IOCTL_HID_GET_INPUT_REPORT,
            .broken = TRUE,
            .report_len = report_len - 1,
            .report_buf =
            {
                0x5a,0x5a,0x5a,0x5a,0x5a,0x5a,0x5a,0x5a,
                0x5a,0x5a,0x5a,0x5a,0x5a,0x5a,0x5a,0x5a,
                0x5a,0x5a,0x5a,0x5a,0x5a,
            },
            .ret_length = 3,
            .ret_status = STATUS_SUCCESS,
        };

        set_hid_expect( file, &broken_expect, sizeof(broken_expect) );
    }

    SetLastError( 0xdeadbeef );
    memset( buffer, 0x5a, sizeof(buffer) );
    ret = HidD_GetInputReport( file, buffer, report_len );
    if (report_id || broken( !ret ) /* w7u */)
    {
        ok( !ret, "HidD_GetInputReport succeeded, last error %u\n", GetLastError() );
        ok( GetLastError() == ERROR_INVALID_PARAMETER || broken( GetLastError() == ERROR_CRC ),
            "HidD_GetInputReport returned error %u\n", GetLastError() );
    }
    else
    {
        ok( ret, "HidD_GetInputReport failed, last error %u\n", GetLastError() );
        ok( buffer[0] == 0x5a, "got buffer[0] %x, expected 0x5a\n", (BYTE)buffer[0] );
    }

    set_hid_expect( file, expect, sizeof(expect) );

    SetLastError( 0xdeadbeef );
    ret = HidD_GetInputReport( file, report, report_len );
    ok( ret, "HidD_GetInputReport failed, last error %u\n", GetLastError() );
    ok( report[0] == report_id, "got report[0] %02x, expected %02x\n", report[0], report_id );

    SetLastError( 0xdeadbeef );
    length = report_len * 2;
    ret = sync_ioctl( file, IOCTL_HID_GET_INPUT_REPORT, NULL, 0, report, &length );
    ok( ret, "IOCTL_HID_GET_INPUT_REPORT failed, last error %u\n", GetLastError() );
    ok( length == 3, "got length %u, expected 3\n", length );
    ok( report[0] == report_id, "got report[0] %02x, expected %02x\n", report[0], report_id );

    set_hid_expect( file, NULL, 0 );
}

static void test_hidp_get_feature( HANDLE file, int report_id, ULONG report_len, PHIDP_PREPARSED_DATA preparsed )
{
    struct hid_expect expect[] =
    {
        {
            .code = IOCTL_HID_GET_FEATURE,
            .report_id = report_id,
            .report_len = report_len - (report_id ? 0 : 1),
            .report_buf = {report_id ? report_id : 0xa5,0xa5,0xa5},
            .ret_length = 3,
            .ret_status = STATUS_SUCCESS,
        },
        {
            .code = IOCTL_HID_GET_FEATURE,
            .report_id = report_id,
            .report_len = 2 * report_len - (report_id ? 0 : 1),
            .report_buf = {report_id ? report_id : 0xa5,0xa5,0xa5},
            .ret_length = 3,
            .ret_status = STATUS_SUCCESS,
        },
    };

    char buffer[200], report[200];
    NTSTATUS status;
    ULONG length;
    BOOL ret;

    memset( report, 0xcd, sizeof(report) );
    status = HidP_InitializeReportForID( HidP_Feature, report_id, preparsed, report, report_len );
    ok( status == HIDP_STATUS_SUCCESS, "HidP_InitializeReportForID returned %#x\n", status );

    SetLastError( 0xdeadbeef );
    ret = HidD_GetFeature( file, report, 0 );
    ok( !ret, "HidD_GetFeature succeeded\n" );
    ok( GetLastError() == ERROR_INVALID_USER_BUFFER, "HidD_GetFeature returned error %u\n", GetLastError() );

    SetLastError( 0xdeadbeef );
    ret = HidD_GetFeature( file, report, report_len - 1 );
    ok( !ret, "HidD_GetFeature succeeded\n" );
    ok( GetLastError() == ERROR_INVALID_PARAMETER || broken( GetLastError() == ERROR_CRC ),
        "HidD_GetFeature returned error %u\n", GetLastError() );

    if (!report_id)
    {
        struct hid_expect broken_expect =
        {
            .code = IOCTL_HID_GET_FEATURE,
            .broken = TRUE,
            .report_len = report_len - 1,
            .report_buf =
            {
                0x5a,0x5a,0x5a,0x5a,0x5a,0x5a,0x5a,0x5a,
                0x5a,0x5a,0x5a,0x5a,0x5a,0x5a,0x5a,0x5a,
                0x5a,0x5a,0x5a,0x5a,0x5a,
            },
            .ret_length = 3,
            .ret_status = STATUS_SUCCESS,
        };

        set_hid_expect( file, &broken_expect, sizeof(broken_expect) );
    }

    SetLastError( 0xdeadbeef );
    memset( buffer, 0x5a, sizeof(buffer) );
    ret = HidD_GetFeature( file, buffer, report_len );
    if (report_id || broken( !ret ))
    {
        ok( !ret, "HidD_GetFeature succeeded, last error %u\n", GetLastError() );
        ok( GetLastError() == ERROR_INVALID_PARAMETER || broken( GetLastError() == ERROR_CRC ),
            "HidD_GetFeature returned error %u\n", GetLastError() );
    }
    else
    {
        ok( ret, "HidD_GetFeature failed, last error %u\n", GetLastError() );
        ok( buffer[0] == 0x5a, "got buffer[0] %x, expected 0x5a\n", (BYTE)buffer[0] );
    }

    set_hid_expect( file, expect, sizeof(expect) );

    SetLastError( 0xdeadbeef );
    ret = HidD_GetFeature( file, report, report_len );
    ok( ret, "HidD_GetFeature failed, last error %u\n", GetLastError() );
    ok( report[0] == report_id, "got report[0] %02x, expected %02x\n", report[0], report_id );

    length = report_len * 2;
    SetLastError( 0xdeadbeef );
    ret = sync_ioctl( file, IOCTL_HID_GET_FEATURE, NULL, 0, report, &length );
    ok( ret, "IOCTL_HID_GET_FEATURE failed, last error %u\n", GetLastError() );
    ok( length == 3, "got length %u, expected 3\n", length );
    ok( report[0] == report_id, "got report[0] %02x, expected %02x\n", report[0], report_id );

    set_hid_expect( file, NULL, 0 );
}

static void test_hidp_set_feature( HANDLE file, int report_id, ULONG report_len, PHIDP_PREPARSED_DATA preparsed )
{
    struct hid_expect expect[] =
    {
        {
            .code = IOCTL_HID_SET_FEATURE,
            .report_id = report_id,
            .report_len = report_len - (report_id ? 0 : 1),
            .report_buf = {report_id},
            .ret_length = 3,
            .ret_status = STATUS_SUCCESS,
        },
        {
            .code = IOCTL_HID_SET_FEATURE,
            .report_id = report_id,
            .report_len = report_len - (report_id ? 0 : 1),
            .report_buf =
            {
                report_id,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
                0,0,0,0,0,0xcd,0xcd,0xcd,0xcd,0xcd,0xcd,0xcd,0xcd,0xcd,0xcd,0xcd,
                0xcd,0xcd,0xcd,0xcd,0xcd,0xcd,0xcd,0xcd,0xcd,0xcd,0xcd,
            },
            .ret_length = 3,
            .ret_status = STATUS_SUCCESS,
        },
    };
    char buffer[200], report[200];
    NTSTATUS status;
    ULONG length;
    BOOL ret;

    memset( report, 0xcd, sizeof(report) );
    status = HidP_InitializeReportForID( HidP_Feature, report_id, preparsed, report, report_len );
    ok( status == HIDP_STATUS_SUCCESS, "HidP_InitializeReportForID returned %#x\n", status );

    SetLastError( 0xdeadbeef );
    ret = HidD_SetFeature( file, report, 0 );
    ok( !ret, "HidD_SetFeature succeeded\n" );
    ok( GetLastError() == ERROR_INVALID_USER_BUFFER, "HidD_SetFeature returned error %u\n", GetLastError() );

    SetLastError( 0xdeadbeef );
    ret = HidD_SetFeature( file, report, report_len - 1 );
    ok( !ret, "HidD_SetFeature succeeded\n" );
    ok( GetLastError() == ERROR_INVALID_PARAMETER || broken( GetLastError() == ERROR_CRC ),
        "HidD_SetFeature returned error %u\n", GetLastError() );

    if (!report_id)
    {
        struct hid_expect broken_expect =
        {
            .code = IOCTL_HID_SET_FEATURE,
            .broken = TRUE,
            .report_len = report_len - 1,
            .report_buf =
            {
                0x5a,0x5a,0x5a,0x5a,0x5a,0x5a,0x5a,0x5a,
                0x5a,0x5a,0x5a,0x5a,0x5a,0x5a,0x5a,0x5a,
                0x5a,0x5a,0x5a,0x5a,0x5a,
            },
            .ret_length = 3,
            .ret_status = STATUS_SUCCESS,
        };

        set_hid_expect( file, &broken_expect, sizeof(broken_expect) );
    }

    SetLastError( 0xdeadbeef );
    memset( buffer, 0x5a, sizeof(buffer) );
    ret = HidD_SetFeature( file, buffer, report_len );
    if (report_id || broken( !ret ))
    {
        ok( !ret, "HidD_SetFeature succeeded, last error %u\n", GetLastError() );
        ok( GetLastError() == ERROR_INVALID_PARAMETER || broken( GetLastError() == ERROR_CRC ),
            "HidD_SetFeature returned error %u\n", GetLastError() );
    }
    else
    {
        ok( ret, "HidD_SetFeature failed, last error %u\n", GetLastError() );
    }

    set_hid_expect( file, expect, sizeof(expect) );

    SetLastError( 0xdeadbeef );
    ret = HidD_SetFeature( file, report, report_len );
    ok( ret, "HidD_SetFeature failed, last error %u\n", GetLastError() );

    length = report_len * 2;
    SetLastError( 0xdeadbeef );
    ret = sync_ioctl( file, IOCTL_HID_SET_FEATURE, NULL, 0, report, &length );
    ok( !ret, "IOCTL_HID_SET_FEATURE succeeded\n" );
    ok( GetLastError() == ERROR_INVALID_USER_BUFFER, "IOCTL_HID_SET_FEATURE returned error %u\n",
        GetLastError() );
    length = 0;
    SetLastError( 0xdeadbeef );
    ret = sync_ioctl( file, IOCTL_HID_SET_FEATURE, report, report_len * 2, NULL, &length );
    ok( ret, "IOCTL_HID_SET_FEATURE failed, last error %u\n", GetLastError() );
    ok( length == 3, "got length %u, expected 3\n", length );

    set_hid_expect( file, NULL, 0 );
}

static void test_hidp_set_output( HANDLE file, int report_id, ULONG report_len, PHIDP_PREPARSED_DATA preparsed )
{
    struct hid_expect expect[] =
    {
        {
            .code = IOCTL_HID_SET_OUTPUT_REPORT,
            .report_id = report_id,
            .report_len = report_len - (report_id ? 0 : 1),
            .report_buf = {report_id},
            .ret_length = 3,
            .ret_status = STATUS_SUCCESS,
        },
        {
            .code = IOCTL_HID_SET_OUTPUT_REPORT,
            .report_id = report_id,
            .report_len = report_len - (report_id ? 0 : 1),
            .report_buf = {report_id,0,0xcd,0xcd,0xcd},
            .ret_length = 3,
            .ret_status = STATUS_SUCCESS,
        },
    };

    char buffer[200], report[200];
    NTSTATUS status;
    ULONG length;
    BOOL ret;

    memset( report, 0xcd, sizeof(report) );
    status = HidP_InitializeReportForID( HidP_Output, report_id, preparsed, report, report_len );
    ok( status == HIDP_STATUS_REPORT_DOES_NOT_EXIST, "HidP_InitializeReportForID returned %#x\n", status );
    memset( report, 0, report_len );
    report[0] = report_id;

    SetLastError( 0xdeadbeef );
    ret = HidD_SetOutputReport( file, report, 0 );
    ok( !ret, "HidD_SetOutputReport succeeded\n" );
    ok( GetLastError() == ERROR_INVALID_USER_BUFFER, "HidD_SetOutputReport returned error %u\n",
        GetLastError() );

    SetLastError( 0xdeadbeef );
    ret = HidD_SetOutputReport( file, report, report_len - 1 );
    ok( !ret, "HidD_SetOutputReport succeeded\n" );
    ok( GetLastError() == ERROR_INVALID_PARAMETER || broken( GetLastError() == ERROR_CRC ),
        "HidD_SetOutputReport returned error %u\n", GetLastError() );

    if (!report_id)
    {
        struct hid_expect broken_expect =
        {
            .code = IOCTL_HID_SET_OUTPUT_REPORT,
            .broken = TRUE,
            .report_len = report_len - 1,
            .report_buf = {0x5a,0x5a},
            .ret_length = 3,
            .ret_status = STATUS_SUCCESS,
        };

        set_hid_expect( file, &broken_expect, sizeof(broken_expect) );
    }

    SetLastError( 0xdeadbeef );
    memset( buffer, 0x5a, sizeof(buffer) );
    ret = HidD_SetOutputReport( file, buffer, report_len );
    if (report_id || broken( !ret ))
    {
        ok( !ret, "HidD_SetOutputReport succeeded, last error %u\n", GetLastError() );
        ok( GetLastError() == ERROR_INVALID_PARAMETER || broken( GetLastError() == ERROR_CRC ),
            "HidD_SetOutputReport returned error %u\n", GetLastError() );
    }
    else
    {
        ok( ret, "HidD_SetOutputReport failed, last error %u\n", GetLastError() );
    }

    set_hid_expect( file, expect, sizeof(expect) );

    SetLastError( 0xdeadbeef );
    ret = HidD_SetOutputReport( file, report, report_len );
    ok( ret, "HidD_SetOutputReport failed, last error %u\n", GetLastError() );

    length = report_len * 2;
    SetLastError( 0xdeadbeef );
    ret = sync_ioctl( file, IOCTL_HID_SET_OUTPUT_REPORT, NULL, 0, report, &length );
    ok( !ret, "IOCTL_HID_SET_OUTPUT_REPORT succeeded\n" );
    ok( GetLastError() == ERROR_INVALID_USER_BUFFER,
        "IOCTL_HID_SET_OUTPUT_REPORT returned error %u\n", GetLastError() );
    length = 0;
    SetLastError( 0xdeadbeef );
    ret = sync_ioctl( file, IOCTL_HID_SET_OUTPUT_REPORT, report, report_len * 2, NULL, &length );
    ok( ret, "IOCTL_HID_SET_OUTPUT_REPORT failed, last error %u\n", GetLastError() );
    ok( length == 3, "got length %u, expected 3\n", length );

    set_hid_expect( file, NULL, 0 );
}

static void test_write_file( HANDLE file, int report_id, ULONG report_len )
{
    struct hid_expect expect =
    {
        .code = IOCTL_HID_WRITE_REPORT,
        .report_id = report_id,
        .report_len = report_len - (report_id ? 0 : 1),
        .report_buf = {report_id ? report_id : 0xcd,0xcd,0xcd,0xcd,0xcd},
        .ret_length = 3,
        .ret_status = STATUS_SUCCESS,
    };

    char report[200];
    ULONG length;
    BOOL ret;

    SetLastError( 0xdeadbeef );
    ret = WriteFile( file, report, 0, &length, NULL );
    ok( !ret, "WriteFile succeeded\n" );
    ok( GetLastError() == ERROR_INVALID_USER_BUFFER, "WriteFile returned error %u\n", GetLastError() );
    ok( length == 0, "WriteFile returned %x\n", length );
    SetLastError( 0xdeadbeef );
    ret = WriteFile( file, report, report_len - 1, &length, NULL );
    ok( !ret, "WriteFile succeeded\n" );
    ok( GetLastError() == ERROR_INVALID_PARAMETER || GetLastError() == ERROR_INVALID_USER_BUFFER,
        "WriteFile returned error %u\n", GetLastError() );
    ok( length == 0, "WriteFile returned %x\n", length );

    set_hid_expect( file, &expect, sizeof(expect) );

    memset( report, 0xcd, sizeof(report) );
    report[0] = 0xa5;
    SetLastError( 0xdeadbeef );
    ret = WriteFile( file, report, report_len * 2, &length, NULL );
    if (report_id || broken( !ret ) /* w7u */)
    {
        ok( !ret, "WriteFile succeeded\n" );
        ok( GetLastError() == ERROR_INVALID_PARAMETER, "WriteFile returned error %u\n", GetLastError() );
        ok( length == 0, "WriteFile wrote %u\n", length );
        SetLastError( 0xdeadbeef );
        report[0] = report_id;
        ret = WriteFile( file, report, report_len, &length, NULL );
    }

    if (report_id)
    {
        ok( ret, "WriteFile failed, last error %u\n", GetLastError() );
        ok( length == 2, "WriteFile wrote %u\n", length );
    }
    else
    {
        ok( ret, "WriteFile failed, last error %u\n", GetLastError() );
        ok( length == 3, "WriteFile wrote %u\n", length );
    }

    set_hid_expect( file, NULL, 0 );
}

static void test_hidp( HANDLE file, HANDLE async_file, int report_id, BOOL polled, const HIDP_CAPS *expect_caps )
{
    const HIDP_BUTTON_CAPS expect_button_caps[] =
    {
        {
            .UsagePage = HID_USAGE_PAGE_BUTTON,
            .ReportID = report_id,
            .BitField = 2,
            .LinkUsage = HID_USAGE_GENERIC_JOYSTICK,
            .LinkUsagePage = HID_USAGE_PAGE_GENERIC,
            .LinkCollection = 1,
            .IsRange = TRUE,
            .IsAbsolute = TRUE,
            .Range.UsageMin = 1,
            .Range.UsageMax = 8,
            .Range.DataIndexMin = 2,
            .Range.DataIndexMax = 9,
        },
        {
            .UsagePage = HID_USAGE_PAGE_BUTTON,
            .ReportID = report_id,
            .BitField = 3,
            .LinkCollection = 1,
            .LinkUsage = HID_USAGE_GENERIC_JOYSTICK,
            .LinkUsagePage = HID_USAGE_PAGE_GENERIC,
            .IsRange = TRUE,
            .IsAbsolute = TRUE,
            .Range.UsageMin = 0x18,
            .Range.UsageMax = 0x1f,
            .Range.DataIndexMin = 10,
            .Range.DataIndexMax = 17,
        },
        {
            .UsagePage = HID_USAGE_PAGE_KEYBOARD,
            .ReportID = report_id,
            .BitField = 0x1fc,
            .LinkCollection = 1,
            .LinkUsage = HID_USAGE_GENERIC_JOYSTICK,
            .LinkUsagePage = HID_USAGE_PAGE_GENERIC,
            .IsRange = TRUE,
            .IsAbsolute = FALSE,
            .Range.UsageMin = 0x8,
            .Range.UsageMax = 0xf,
            .Range.DataIndexMin = 18,
            .Range.DataIndexMax = 25,
        },
        {
            .UsagePage = HID_USAGE_PAGE_BUTTON,
            .ReportID = report_id,
            .BitField = 2,
            .LinkCollection = 1,
            .LinkUsage = HID_USAGE_GENERIC_JOYSTICK,
            .LinkUsagePage = HID_USAGE_PAGE_GENERIC,
            .IsRange = FALSE,
            .IsAbsolute = TRUE,
            .NotRange.Usage = 0x20,
            .NotRange.Reserved1 = 0x20,
            .NotRange.DataIndex = 26,
            .NotRange.Reserved4 = 26,
        },
    };
    const HIDP_VALUE_CAPS expect_value_caps[] =
    {
        {
            .UsagePage = HID_USAGE_PAGE_GENERIC,
            .ReportID = report_id,
            .BitField = 2,
            .LinkUsage = HID_USAGE_GENERIC_JOYSTICK,
            .LinkUsagePage = HID_USAGE_PAGE_GENERIC,
            .LinkCollection = 1,
            .IsAbsolute = TRUE,
            .BitSize = 8,
            .ReportCount = 1,
            .LogicalMin = -128,
            .LogicalMax = 127,
            .NotRange.Usage = HID_USAGE_GENERIC_Y,
            .NotRange.Reserved1 = HID_USAGE_GENERIC_Y,
        },
        {
            .UsagePage = HID_USAGE_PAGE_GENERIC,
            .ReportID = report_id,
            .BitField = 2,
            .LinkUsage = HID_USAGE_GENERIC_JOYSTICK,
            .LinkUsagePage = HID_USAGE_PAGE_GENERIC,
            .LinkCollection = 1,
            .IsAbsolute = TRUE,
            .BitSize = 8,
            .ReportCount = 1,
            .LogicalMin = -128,
            .LogicalMax = 127,
            .NotRange.Usage = HID_USAGE_GENERIC_X,
            .NotRange.Reserved1 = HID_USAGE_GENERIC_X,
            .NotRange.DataIndex = 1,
            .NotRange.Reserved4 = 1,
        },
        {
            .UsagePage = HID_USAGE_PAGE_BUTTON,
            .ReportID = report_id,
            .BitField = 2,
            .LinkUsage = HID_USAGE_GENERIC_JOYSTICK,
            .LinkUsagePage = HID_USAGE_PAGE_GENERIC,
            .LinkCollection = 1,
            .IsAbsolute = TRUE,
            .ReportCount = 1,
            .LogicalMax = 1,
            .IsRange = TRUE,
            .Range.UsageMin = 0x21,
            .Range.UsageMax = 0x22,
            .Range.DataIndexMin = 27,
            .Range.DataIndexMax = 28,
        },
        {
            .UsagePage = HID_USAGE_PAGE_GENERIC,
            .ReportID = report_id,
            .BitField = 2,
            .LinkUsage = HID_USAGE_GENERIC_JOYSTICK,
            .LinkUsagePage = HID_USAGE_PAGE_GENERIC,
            .LinkCollection = 1,
            .IsAbsolute = TRUE,
            .BitSize = 4,
            .ReportCount = 2,
            .LogicalMin = 1,
            .LogicalMax = 8,
            .NotRange.Usage = HID_USAGE_GENERIC_HATSWITCH,
            .NotRange.Reserved1 = HID_USAGE_GENERIC_HATSWITCH,
            .NotRange.DataIndex = 29,
            .NotRange.Reserved4 = 29,
        },
    };
    static const HIDP_LINK_COLLECTION_NODE expect_collections[] =
    {
        {
            .LinkUsage = HID_USAGE_GENERIC_JOYSTICK,
            .LinkUsagePage = HID_USAGE_PAGE_GENERIC,
            .CollectionType = 1,
            .NumberOfChildren = 7,
            .FirstChild = 9,
        },
        {
            .LinkUsage = HID_USAGE_GENERIC_JOYSTICK,
            .LinkUsagePage = HID_USAGE_PAGE_GENERIC,
            .CollectionType = 2,
        },
    };
    static const HIDP_DATA expect_data[] =
    {
        { .DataIndex = 0, },
        { .DataIndex = 1, },
        { .DataIndex = 5, .RawValue = 1, },
        { .DataIndex = 7, .RawValue = 1, },
        { .DataIndex = 19, .RawValue = 1, },
        { .DataIndex = 21, .RawValue = 1, },
        { .DataIndex = 30, },
        { .DataIndex = 31, },
        { .DataIndex = 32, .RawValue = 0xfeedcafe, },
        { .DataIndex = 37, .RawValue = 1, },
        { .DataIndex = 39, .RawValue = 1, },
    };

    OVERLAPPED overlapped = {0}, overlapped2 = {0};
    HIDP_LINK_COLLECTION_NODE collections[16];
    PHIDP_PREPARSED_DATA preparsed_data;
    USAGE_AND_PAGE usage_and_pages[16];
    HIDP_BUTTON_CAPS button_caps[32];
    HIDP_VALUE_CAPS value_caps[16];
    char buffer[200], report[200];
    DWORD collection_count;
    DWORD waveform_list;
    HIDP_DATA data[64];
    USAGE usages[16];
    ULONG off, value;
    NTSTATUS status;
    HIDP_CAPS caps;
    unsigned int i;
    USHORT count;
    BOOL ret;

    ret = HidD_GetPreparsedData( file, &preparsed_data );
    ok( ret, "HidD_GetPreparsedData failed with error %u\n", GetLastError() );

    memset( buffer, 0, sizeof(buffer) );
    status = HidP_GetCaps( (PHIDP_PREPARSED_DATA)buffer, &caps );
    ok( status == HIDP_STATUS_INVALID_PREPARSED_DATA, "HidP_GetCaps returned %#x\n", status );
    status = HidP_GetCaps( preparsed_data, &caps );
    ok( status == HIDP_STATUS_SUCCESS, "HidP_GetCaps returned %#x\n", status );
    check_hidp_caps( &caps, expect_caps );

    collection_count = 0;
    status = HidP_GetLinkCollectionNodes( collections, &collection_count, preparsed_data );
    ok( status == HIDP_STATUS_BUFFER_TOO_SMALL, "HidP_GetLinkCollectionNodes returned %#x\n", status );
    ok( collection_count == caps.NumberLinkCollectionNodes,
        "got %d collection nodes, expected %d\n", collection_count, caps.NumberLinkCollectionNodes );
    collection_count = ARRAY_SIZE(collections);
    status = HidP_GetLinkCollectionNodes( collections, &collection_count, (PHIDP_PREPARSED_DATA)buffer );
    ok( status == HIDP_STATUS_INVALID_PREPARSED_DATA, "HidP_GetLinkCollectionNodes returned %#x\n", status );
    status = HidP_GetLinkCollectionNodes( collections, &collection_count, preparsed_data );
    ok( status == HIDP_STATUS_SUCCESS, "HidP_GetLinkCollectionNodes returned %#x\n", status );
    ok( collection_count == caps.NumberLinkCollectionNodes,
        "got %d collection nodes, expected %d\n", collection_count, caps.NumberLinkCollectionNodes );

    for (i = 0; i < ARRAY_SIZE(expect_collections); ++i)
    {
        winetest_push_context( "collections[%d]", i );
        check_hidp_link_collection_node( &collections[i], &expect_collections[i] );
        winetest_pop_context();
    }

    count = ARRAY_SIZE(button_caps);
    status = HidP_GetButtonCaps( HidP_Output, button_caps, &count, preparsed_data );
    ok( status == HIDP_STATUS_USAGE_NOT_FOUND, "HidP_GetButtonCaps returned %#x\n", status );
    status = HidP_GetButtonCaps( HidP_Feature + 1, button_caps, &count, preparsed_data );
    ok( status == HIDP_STATUS_INVALID_REPORT_TYPE, "HidP_GetButtonCaps returned %#x\n", status );
    count = 0;
    status = HidP_GetButtonCaps( HidP_Input, button_caps, &count, preparsed_data );
    ok( status == HIDP_STATUS_BUFFER_TOO_SMALL, "HidP_GetButtonCaps returned %#x\n", status );
    ok( count == caps.NumberInputButtonCaps, "HidP_GetButtonCaps returned count %d, expected %d\n",
        count, caps.NumberInputButtonCaps );
    count = ARRAY_SIZE(button_caps);
    status = HidP_GetButtonCaps( HidP_Input, button_caps, &count, (PHIDP_PREPARSED_DATA)buffer );
    ok( status == HIDP_STATUS_INVALID_PREPARSED_DATA, "HidP_GetButtonCaps returned %#x\n", status );
    memset( button_caps, 0, sizeof(button_caps) );
    status = HidP_GetButtonCaps( HidP_Input, button_caps, &count, preparsed_data );
    ok( status == HIDP_STATUS_SUCCESS, "HidP_GetButtonCaps returned %#x\n", status );
    ok( count == caps.NumberInputButtonCaps, "HidP_GetButtonCaps returned count %d, expected %d\n",
        count, caps.NumberInputButtonCaps );

    for (i = 0; i < ARRAY_SIZE(expect_button_caps); ++i)
    {
        winetest_push_context( "button_caps[%d]", i );
        check_hidp_button_caps( &button_caps[i], &expect_button_caps[i] );
        winetest_pop_context();
    }

    count = ARRAY_SIZE(button_caps) - 1;
    status = HidP_GetSpecificButtonCaps( HidP_Output, 0, 0, 0, button_caps, &count, preparsed_data );
    ok( status == HIDP_STATUS_USAGE_NOT_FOUND, "HidP_GetSpecificButtonCaps returned %#x\n", status );
    status = HidP_GetSpecificButtonCaps( HidP_Feature + 1, 0, 0, 0, button_caps, &count, preparsed_data );
    ok( status == HIDP_STATUS_INVALID_REPORT_TYPE, "HidP_GetSpecificButtonCaps returned %#x\n", status );
    count = 0;
    status = HidP_GetSpecificButtonCaps( HidP_Input, 0, 0, 0, button_caps, &count, preparsed_data );
    ok( status == HIDP_STATUS_BUFFER_TOO_SMALL, "HidP_GetSpecificButtonCaps returned %#x\n", status );
    ok( count == caps.NumberInputButtonCaps, "HidP_GetSpecificButtonCaps returned count %d, expected %d\n",
        count, caps.NumberInputButtonCaps );
    count = ARRAY_SIZE(button_caps) - 1;
    status = HidP_GetSpecificButtonCaps( HidP_Input, 0, 0, 0, button_caps, &count, (PHIDP_PREPARSED_DATA)buffer );
    ok( status == HIDP_STATUS_INVALID_PREPARSED_DATA, "HidP_GetSpecificButtonCaps returned %#x\n", status );

    status = HidP_GetSpecificButtonCaps( HidP_Input, 0, 0, 0, button_caps + 1, &count, preparsed_data );
    ok( status == HIDP_STATUS_SUCCESS, "HidP_GetSpecificButtonCaps returned %#x\n", status );
    ok( count == caps.NumberInputButtonCaps, "HidP_GetSpecificButtonCaps returned count %d, expected %d\n",
        count, caps.NumberInputButtonCaps );
    check_hidp_button_caps( &button_caps[1], &button_caps[0] );

    status = HidP_GetSpecificButtonCaps( HidP_Input, HID_USAGE_PAGE_BUTTON, 0, 5, button_caps + 1,
                                         &count, preparsed_data );
    ok( status == HIDP_STATUS_SUCCESS, "HidP_GetSpecificButtonCaps returned %#x\n", status );
    ok( count == 1, "HidP_GetSpecificButtonCaps returned count %d, expected %d\n", count, 1 );
    check_hidp_button_caps( &button_caps[1], &button_caps[0] );

    count = 0xbeef;
    status = HidP_GetSpecificButtonCaps( HidP_Input, 0xfffe, 0, 0, button_caps, &count, preparsed_data );
    ok( status == HIDP_STATUS_USAGE_NOT_FOUND, "HidP_GetSpecificButtonCaps returned %#x\n", status );
    ok( count == 0, "HidP_GetSpecificButtonCaps returned count %d, expected %d\n", count, 0 );
    count = 0xbeef;
    status = HidP_GetSpecificButtonCaps( HidP_Input, 0, 0xfffe, 0, button_caps, &count, preparsed_data );
    ok( status == HIDP_STATUS_USAGE_NOT_FOUND, "HidP_GetSpecificButtonCaps returned %#x\n", status );
    ok( count == 0, "HidP_GetSpecificButtonCaps returned count %d, expected %d\n", count, 0 );
    count = 0xbeef;
    status = HidP_GetSpecificButtonCaps( HidP_Input, 0, 0, 0xfffe, button_caps, &count, preparsed_data );
    ok( status == HIDP_STATUS_USAGE_NOT_FOUND, "HidP_GetSpecificButtonCaps returned %#x\n", status );
    ok( count == 0, "HidP_GetSpecificButtonCaps returned count %d, expected %d\n", count, 0 );

    count = ARRAY_SIZE(value_caps);
    status = HidP_GetValueCaps( HidP_Output, value_caps, &count, preparsed_data );
    ok( status == HIDP_STATUS_USAGE_NOT_FOUND, "HidP_GetValueCaps returned %#x\n", status );
    status = HidP_GetValueCaps( HidP_Feature + 1, value_caps, &count, preparsed_data );
    ok( status == HIDP_STATUS_INVALID_REPORT_TYPE, "HidP_GetValueCaps returned %#x\n", status );
    count = 0;
    status = HidP_GetValueCaps( HidP_Input, value_caps, &count, preparsed_data );
    ok( status == HIDP_STATUS_BUFFER_TOO_SMALL, "HidP_GetValueCaps returned %#x\n", status );
    ok( count == caps.NumberInputValueCaps, "HidP_GetValueCaps returned count %d, expected %d\n",
        count, caps.NumberInputValueCaps );
    count = ARRAY_SIZE(value_caps);
    status = HidP_GetValueCaps( HidP_Input, value_caps, &count, (PHIDP_PREPARSED_DATA)buffer );
    ok( status == HIDP_STATUS_INVALID_PREPARSED_DATA, "HidP_GetValueCaps returned %#x\n", status );
    status = HidP_GetValueCaps( HidP_Input, value_caps, &count, preparsed_data );
    ok( status == HIDP_STATUS_SUCCESS, "HidP_GetValueCaps returned %#x\n", status );
    ok( count == caps.NumberInputValueCaps, "HidP_GetValueCaps returned count %d, expected %d\n",
        count, caps.NumberInputValueCaps );

    for (i = 0; i < ARRAY_SIZE(expect_value_caps); ++i)
    {
        winetest_push_context( "value_caps[%d]", i );
        check_hidp_value_caps( &value_caps[i], &expect_value_caps[i] );
        winetest_pop_context();
    }

    count = ARRAY_SIZE(value_caps) - 4;
    status = HidP_GetSpecificValueCaps( HidP_Output, 0, 0, 0, value_caps, &count, preparsed_data );
    ok( status == HIDP_STATUS_USAGE_NOT_FOUND, "HidP_GetSpecificValueCaps returned %#x\n", status );
    status = HidP_GetSpecificValueCaps( HidP_Feature + 1, 0, 0, 0, value_caps, &count, preparsed_data );
    ok( status == HIDP_STATUS_INVALID_REPORT_TYPE, "HidP_GetSpecificValueCaps returned %#x\n", status );
    count = 0;
    status = HidP_GetSpecificValueCaps( HidP_Input, 0, 0, 0, value_caps, &count, preparsed_data );
    ok( status == HIDP_STATUS_BUFFER_TOO_SMALL, "HidP_GetSpecificValueCaps returned %#x\n", status );
    ok( count == caps.NumberInputValueCaps, "HidP_GetSpecificValueCaps returned count %d, expected %d\n",
        count, caps.NumberInputValueCaps );
    count = ARRAY_SIZE(value_caps) - 4;
    status = HidP_GetSpecificValueCaps( HidP_Input, 0, 0, 0, value_caps + 4, &count, (PHIDP_PREPARSED_DATA)buffer );
    ok( status == HIDP_STATUS_INVALID_PREPARSED_DATA, "HidP_GetSpecificValueCaps returned %#x\n", status );

    status = HidP_GetSpecificValueCaps( HidP_Input, 0, 0, 0, value_caps + 4, &count, preparsed_data );
    ok( status == HIDP_STATUS_SUCCESS, "HidP_GetSpecificValueCaps returned %#x\n", status );
    ok( count == caps.NumberInputValueCaps, "HidP_GetSpecificValueCaps returned count %d, expected %d\n",
        count, caps.NumberInputValueCaps );
    check_hidp_value_caps( &value_caps[4], &value_caps[0] );
    check_hidp_value_caps( &value_caps[5], &value_caps[1] );
    check_hidp_value_caps( &value_caps[6], &value_caps[2] );
    check_hidp_value_caps( &value_caps[7], &value_caps[3] );

    count = 1;
    status = HidP_GetSpecificValueCaps( HidP_Input, HID_USAGE_PAGE_GENERIC, 0, HID_USAGE_GENERIC_HATSWITCH,
                                        value_caps + 4, &count, preparsed_data );
    ok( status == HIDP_STATUS_SUCCESS, "HidP_GetSpecificValueCaps returned %#x\n", status );
    ok( count == 1, "HidP_GetSpecificValueCaps returned count %d, expected %d\n", count, 1 );
    check_hidp_value_caps( &value_caps[4], &value_caps[3] );

    count = 0xdead;
    status = HidP_GetSpecificValueCaps( HidP_Input, 0xfffe, 0, 0, value_caps, &count, preparsed_data );
    ok( status == HIDP_STATUS_USAGE_NOT_FOUND, "HidP_GetSpecificValueCaps returned %#x\n", status );
    ok( count == 0, "HidP_GetSpecificValueCaps returned count %d, expected %d\n", count, 0 );
    count = 0xdead;
    status = HidP_GetSpecificValueCaps( HidP_Input, 0, 0xfffe, 0, value_caps, &count, preparsed_data );
    ok( status == HIDP_STATUS_USAGE_NOT_FOUND, "HidP_GetSpecificValueCaps returned %#x\n", status );
    ok( count == 0, "HidP_GetSpecificValueCaps returned count %d, expected %d\n", count, 0 );
    count = 0xdead;
    status = HidP_GetSpecificValueCaps( HidP_Input, 0, 0, 0xfffe, value_caps, &count, preparsed_data );
    ok( status == HIDP_STATUS_USAGE_NOT_FOUND, "HidP_GetSpecificValueCaps returned %#x\n", status );
    ok( count == 0, "HidP_GetSpecificValueCaps returned count %d, expected %d\n", count, 0 );

    status = HidP_InitializeReportForID( HidP_Input, 0, (PHIDP_PREPARSED_DATA)buffer, report, sizeof(report) );
    ok( status == HIDP_STATUS_INVALID_PREPARSED_DATA, "HidP_InitializeReportForID returned %#x\n", status );
    status = HidP_InitializeReportForID( HidP_Feature + 1, 0, preparsed_data, report, sizeof(report) );
    ok( status == HIDP_STATUS_INVALID_REPORT_TYPE, "HidP_InitializeReportForID returned %#x\n", status );
    status = HidP_InitializeReportForID( HidP_Input, 0, preparsed_data, report, sizeof(report) );
    ok( status == HIDP_STATUS_INVALID_REPORT_LENGTH, "HidP_InitializeReportForID returned %#x\n", status );
    status = HidP_InitializeReportForID( HidP_Input, 0, preparsed_data, report, caps.InputReportByteLength + 1 );
    ok( status == HIDP_STATUS_INVALID_REPORT_LENGTH, "HidP_InitializeReportForID returned %#x\n", status );
    status = HidP_InitializeReportForID( HidP_Input, 1 - report_id, preparsed_data, report,
                                         caps.InputReportByteLength );
    ok( status == HIDP_STATUS_REPORT_DOES_NOT_EXIST, "HidP_InitializeReportForID returned %#x\n", status );

    memset( report, 0xcd, sizeof(report) );
    status = HidP_InitializeReportForID( HidP_Input, report_id, preparsed_data, report, caps.InputReportByteLength );
    ok( status == HIDP_STATUS_SUCCESS, "HidP_InitializeReportForID returned %#x\n", status );

    memset( buffer, 0xcd, sizeof(buffer) );
    memset( buffer, 0, caps.InputReportByteLength );
    buffer[0] = report_id;
    ok( !memcmp( buffer, report, sizeof(buffer) ), "unexpected report data\n" );

    status = HidP_SetUsageValueArray( HidP_Input, HID_USAGE_PAGE_GENERIC, 0, HID_USAGE_GENERIC_X, buffer,
                                      sizeof(buffer), preparsed_data, report, caps.InputReportByteLength );
    ok( status == HIDP_STATUS_NOT_VALUE_ARRAY, "HidP_SetUsageValueArray returned %#x\n", status );
    memset( buffer, 0xcd, sizeof(buffer) );
    status = HidP_SetUsageValueArray( HidP_Input, HID_USAGE_PAGE_GENERIC, 0, HID_USAGE_GENERIC_HATSWITCH,
                                      buffer, 0, preparsed_data, report, caps.InputReportByteLength );
    ok( status == HIDP_STATUS_BUFFER_TOO_SMALL, "HidP_SetUsageValueArray returned %#x\n", status );
    status = HidP_SetUsageValueArray( HidP_Input, HID_USAGE_PAGE_GENERIC, 0, HID_USAGE_GENERIC_HATSWITCH,
                                      buffer, 8, preparsed_data, report, caps.InputReportByteLength );
    todo_wine
    ok( status == HIDP_STATUS_NOT_IMPLEMENTED, "HidP_SetUsageValueArray returned %#x\n", status );

    status = HidP_GetUsageValueArray( HidP_Input, HID_USAGE_PAGE_GENERIC, 0, HID_USAGE_GENERIC_X, buffer,
                                      sizeof(buffer), preparsed_data, report, caps.InputReportByteLength );
    ok( status == HIDP_STATUS_NOT_VALUE_ARRAY, "HidP_GetUsageValueArray returned %#x\n", status );
    memset( buffer, 0xcd, sizeof(buffer) );
    status = HidP_GetUsageValueArray( HidP_Input, HID_USAGE_PAGE_GENERIC, 0, HID_USAGE_GENERIC_HATSWITCH,
                                      buffer, 0, preparsed_data, report, caps.InputReportByteLength );
    ok( status == HIDP_STATUS_BUFFER_TOO_SMALL, "HidP_GetUsageValueArray returned %#x\n", status );
    status = HidP_GetUsageValueArray( HidP_Input, HID_USAGE_PAGE_GENERIC, 0, HID_USAGE_GENERIC_HATSWITCH,
                                      buffer, 8, preparsed_data, report, caps.InputReportByteLength );
    todo_wine
    ok( status == HIDP_STATUS_NOT_IMPLEMENTED, "HidP_GetUsageValueArray returned %#x\n", status );

    value = -128;
    status = HidP_SetUsageValue( HidP_Input, HID_USAGE_PAGE_GENERIC, 0, HID_USAGE_GENERIC_X, value,
                                 preparsed_data, report, caps.InputReportByteLength );
    ok( status == HIDP_STATUS_SUCCESS, "HidP_SetUsageValue returned %#x\n", status );
    value = 0xdeadbeef;
    status = HidP_GetUsageValue( HidP_Input, HID_USAGE_PAGE_GENERIC, 0, HID_USAGE_GENERIC_X, &value,
                                 preparsed_data, report, caps.InputReportByteLength );
    ok( status == HIDP_STATUS_SUCCESS, "HidP_GetUsageValue returned %#x\n", status );
    ok( value == 0x80, "got value %x, expected %#x\n", value, 0x80 );
    value = 0xdeadbeef;
    status = HidP_GetScaledUsageValue( HidP_Input, HID_USAGE_PAGE_GENERIC, 0, HID_USAGE_GENERIC_X,
                                       (LONG *)&value, preparsed_data, report, caps.InputReportByteLength );
    ok( status == HIDP_STATUS_SUCCESS, "HidP_GetScaledUsageValue returned %#x\n", status );
    ok( value == -128, "got value %x, expected %#x\n", value, -128 );

    value = 127;
    status = HidP_SetUsageValue( HidP_Input, HID_USAGE_PAGE_GENERIC, 0, HID_USAGE_GENERIC_X, value,
                                 preparsed_data, report, caps.InputReportByteLength );
    ok( status == HIDP_STATUS_SUCCESS, "HidP_SetUsageValue returned %#x\n", status );
    value = 0xdeadbeef;
    status = HidP_GetScaledUsageValue( HidP_Input, HID_USAGE_PAGE_GENERIC, 0, HID_USAGE_GENERIC_X,
                                       (LONG *)&value, preparsed_data, report, caps.InputReportByteLength );
    ok( status == HIDP_STATUS_SUCCESS, "HidP_GetScaledUsageValue returned %#x\n", status );
    ok( value == 127, "got value %x, expected %#x\n", value, 127 );

    value = 0;
    status = HidP_SetUsageValue( HidP_Input, HID_USAGE_PAGE_GENERIC, 0, HID_USAGE_GENERIC_X, value,
                                 preparsed_data, report, caps.InputReportByteLength );
    ok( status == HIDP_STATUS_SUCCESS, "HidP_SetUsageValue returned %#x\n", status );
    value = 0xdeadbeef;
    status = HidP_GetScaledUsageValue( HidP_Input, HID_USAGE_PAGE_GENERIC, 0, HID_USAGE_GENERIC_X,
                                       (LONG *)&value, preparsed_data, report, caps.InputReportByteLength );
    ok( status == HIDP_STATUS_SUCCESS, "HidP_GetScaledUsageValue returned %#x\n", status );
    ok( value == 0, "got value %x, expected %#x\n", value, 0 );

    value = 0x7fffffff;
    status = HidP_SetUsageValue( HidP_Input, HID_USAGE_PAGE_GENERIC, 0, HID_USAGE_GENERIC_Z, value,
                                 preparsed_data, report, caps.InputReportByteLength );
    ok( status == HIDP_STATUS_SUCCESS, "HidP_SetUsageValue returned %#x\n", status );
    value = 0xdeadbeef;
    status = HidP_GetScaledUsageValue( HidP_Input, HID_USAGE_PAGE_GENERIC, 0, HID_USAGE_GENERIC_Z,
                                       (LONG *)&value, preparsed_data, report, caps.InputReportByteLength );
    ok( status == HIDP_STATUS_VALUE_OUT_OF_RANGE, "HidP_GetScaledUsageValue returned %#x\n", status );
    ok( value == 0, "got value %x, expected %#x\n", value, 0 );
    value = 0xdeadbeef;
    status = HidP_GetUsageValue( HidP_Input, HID_USAGE_PAGE_GENERIC, 0, HID_USAGE_GENERIC_Z, &value,
                                 preparsed_data, report, caps.InputReportByteLength );
    ok( status == HIDP_STATUS_SUCCESS, "HidP_GetUsageValue returned %#x\n", status );
    ok( value == 0x7fffffff, "got value %x, expected %#x\n", value, 0x7fffffff );

    value = 0x3fffffff;
    status = HidP_SetUsageValue( HidP_Input, HID_USAGE_PAGE_GENERIC, 0, HID_USAGE_GENERIC_Z, value,
                                 preparsed_data, report, caps.InputReportByteLength );
    ok( status == HIDP_STATUS_SUCCESS, "HidP_SetUsageValue returned %#x\n", status );
    value = 0xdeadbeef;
    status = HidP_GetScaledUsageValue( HidP_Input, HID_USAGE_PAGE_GENERIC, 0, HID_USAGE_GENERIC_Z,
                                       (LONG *)&value, preparsed_data, report, caps.InputReportByteLength );
    ok( status == HIDP_STATUS_SUCCESS, "HidP_GetScaledUsageValue returned %#x\n", status );
    ok( value == 0x7fffffff, "got value %x, expected %#x\n", value, 0x7fffffff );

    value = 0;
    status = HidP_SetUsageValue( HidP_Input, HID_USAGE_PAGE_GENERIC, 0, HID_USAGE_GENERIC_Z, value,
                                 preparsed_data, report, caps.InputReportByteLength );
    ok( status == HIDP_STATUS_SUCCESS, "HidP_SetUsageValue returned %#x\n", status );
    value = 0xdeadbeef;
    status = HidP_GetScaledUsageValue( HidP_Input, HID_USAGE_PAGE_GENERIC, 0, HID_USAGE_GENERIC_Z,
                                       (LONG *)&value, preparsed_data, report, caps.InputReportByteLength );
    ok( status == HIDP_STATUS_SUCCESS, "HidP_GetScaledUsageValue returned %#x\n", status );
    ok( value == 0x80000000, "got value %x, expected %#x\n", value, 0x80000000 );

    value = 0;
    status = HidP_SetUsageValue( HidP_Input, HID_USAGE_PAGE_GENERIC, 0, HID_USAGE_GENERIC_RX, value,
                                 preparsed_data, report, caps.InputReportByteLength );
    ok( status == HIDP_STATUS_SUCCESS, "HidP_SetUsageValue returned %#x\n", status );
    value = 0xdeadbeef;
    status = HidP_GetScaledUsageValue( HidP_Input, HID_USAGE_PAGE_GENERIC, 0, HID_USAGE_GENERIC_RX,
                                       (LONG *)&value, preparsed_data, report, caps.InputReportByteLength );
    ok( status == HIDP_STATUS_SUCCESS, "HidP_GetScaledUsageValue returned %#x\n", status );
    ok( value == 0, "got value %x, expected %#x\n", value, 0 );

    value = 0xfeedcafe;
    status = HidP_SetUsageValue( HidP_Input, HID_USAGE_PAGE_GENERIC, 0, HID_USAGE_GENERIC_RY, value,
                                 preparsed_data, report, caps.InputReportByteLength );
    ok( status == HIDP_STATUS_SUCCESS, "HidP_SetUsageValue returned %#x\n", status );
    value = 0xdeadbeef;
    status = HidP_GetScaledUsageValue( HidP_Input, HID_USAGE_PAGE_GENERIC, 0, HID_USAGE_GENERIC_RY,
                                       (LONG *)&value, preparsed_data, report, caps.InputReportByteLength );
    ok( status == HIDP_STATUS_BAD_LOG_PHY_VALUES, "HidP_GetScaledUsageValue returned %#x\n", status );
    ok( value == 0, "got value %x, expected %#x\n", value, 0 );
    status = HidP_SetScaledUsageValue( HidP_Input, HID_USAGE_PAGE_GENERIC, 0, HID_USAGE_GENERIC_RY,
                                       0, preparsed_data, report, caps.InputReportByteLength );
    ok( status == HIDP_STATUS_BAD_LOG_PHY_VALUES, "HidP_GetScaledUsageValue returned %#x\n", status );
    ok( value == 0, "got value %x, expected %#x\n", value, 0 );

    value = HidP_MaxUsageListLength( HidP_Feature + 1, 0, preparsed_data );
    ok( value == 0, "HidP_MaxUsageListLength(HidP_Feature + 1, 0) returned %d, expected %d\n", value, 0 );
    value = HidP_MaxUsageListLength( HidP_Input, 0, preparsed_data );
    ok( value == 50, "HidP_MaxUsageListLength(HidP_Input, 0) returned %d, expected %d\n", value, 50 );
    value = HidP_MaxUsageListLength( HidP_Input, HID_USAGE_PAGE_BUTTON, preparsed_data );
    ok( value == 32, "HidP_MaxUsageListLength(HidP_Input, HID_USAGE_PAGE_BUTTON) returned %d, expected %d\n",
        value, 32 );
    value = HidP_MaxUsageListLength( HidP_Input, HID_USAGE_PAGE_LED, preparsed_data );
    ok( value == 8, "HidP_MaxUsageListLength(HidP_Input, HID_USAGE_PAGE_LED) returned %d, expected %d\n",
        value, 8 );
    value = HidP_MaxUsageListLength( HidP_Feature, HID_USAGE_PAGE_BUTTON, preparsed_data );
    ok( value == 8, "HidP_MaxUsageListLength(HidP_Feature, HID_USAGE_PAGE_BUTTON) returned %d, expected %d\n",
        value, 8 );
    value = HidP_MaxUsageListLength( HidP_Feature, HID_USAGE_PAGE_LED, preparsed_data );
    ok( value == 0, "HidP_MaxUsageListLength(HidP_Feature, HID_USAGE_PAGE_LED) returned %d, expected %d\n",
        value, 0 );

    usages[0] = 0xff;
    value = 1;
    status = HidP_SetUsages( HidP_Input, HID_USAGE_PAGE_BUTTON, 0, usages, &value, preparsed_data,
                             report, caps.InputReportByteLength );
    ok( status == HIDP_STATUS_USAGE_NOT_FOUND, "HidP_SetUsages returned %#x\n", status );
    usages[1] = 2;
    usages[2] = 0xff;
    value = 3;
    status = HidP_SetUsages( HidP_Input, HID_USAGE_PAGE_BUTTON, 0, usages, &value, preparsed_data,
                             report, caps.InputReportByteLength );
    ok( status == HIDP_STATUS_USAGE_NOT_FOUND, "HidP_SetUsages returned %#x\n", status );
    usages[0] = 4;
    usages[1] = 6;
    value = 2;
    status = HidP_SetUsages( HidP_Input, HID_USAGE_PAGE_BUTTON, 0, usages, &value, preparsed_data,
                             report, caps.InputReportByteLength );
    ok( status == HIDP_STATUS_SUCCESS, "HidP_SetUsages returned %#x\n", status );
    usages[0] = 4;
    usages[1] = 6;
    value = 2;
    status = HidP_SetUsages( HidP_Input, HID_USAGE_PAGE_LED, 0, usages, &value, preparsed_data,
                             report, caps.InputReportByteLength );
    ok( status == HIDP_STATUS_SUCCESS, "HidP_SetUsages returned %#x\n", status );

    value = ARRAY_SIZE(usages);
    status = HidP_GetUsages( HidP_Input, HID_USAGE_PAGE_KEYBOARD, 0, usages, &value, preparsed_data,
                             report, caps.InputReportByteLength );
    ok( status == HIDP_STATUS_SUCCESS, "HidP_GetUsages returned %#x\n", status );
    ok( value == 0, "got usage count %d, expected %d\n", value, 2 );

    usages[0] = 0x9;
    usages[1] = 0xb;
    usages[2] = 0xa;
    value = 3;
    ok( report[6] == 0, "got report[6] %x expected 0\n", report[6] );
    ok( report[7] == 0, "got report[7] %x expected 0\n", report[7] );
    memcpy( buffer, report, caps.InputReportByteLength );
    status = HidP_SetUsages( HidP_Input, HID_USAGE_PAGE_KEYBOARD, 0, usages, &value, preparsed_data,
                             report, caps.InputReportByteLength );
    ok( status == HIDP_STATUS_BUFFER_TOO_SMALL, "HidP_SetUsages returned %#x\n", status );
    buffer[6] = 2;
    buffer[7] = 4;
    ok( !memcmp( buffer, report, caps.InputReportByteLength ), "unexpected report data\n" );

    status = HidP_SetUsageValue( HidP_Input, HID_USAGE_PAGE_LED, 0, 6, 1, preparsed_data, report,
                                 caps.InputReportByteLength );
    ok( status == HIDP_STATUS_USAGE_NOT_FOUND, "HidP_SetUsageValue returned %#x\n", status );

    value = 0xdeadbeef;
    status = HidP_GetUsageValue( HidP_Input, HID_USAGE_PAGE_LED, 0, 6, &value, preparsed_data,
                                 report, caps.InputReportByteLength );
    ok( status == HIDP_STATUS_USAGE_NOT_FOUND, "HidP_SetUsageValue returned %#x\n", status );
    ok( value == 0xdeadbeef, "got value %x, expected %#x\n", value, 0xdeadbeef );

    value = 1;
    status = HidP_GetUsages( HidP_Input, HID_USAGE_PAGE_BUTTON, 0, usages, &value, preparsed_data,
                             report, caps.InputReportByteLength );
    ok( status == HIDP_STATUS_BUFFER_TOO_SMALL, "HidP_GetUsages returned %#x\n", status );
    ok( value == 2, "got usage count %d, expected %d\n", value, 2 );
    value = ARRAY_SIZE(usages);
    memset( usages, 0xcd, sizeof(usages) );
    status = HidP_GetUsages( HidP_Input, HID_USAGE_PAGE_BUTTON, 0, usages, &value, preparsed_data,
                             report, caps.InputReportByteLength );
    ok( status == HIDP_STATUS_SUCCESS, "HidP_GetUsages returned %#x\n", status );
    ok( value == 2, "got usage count %d, expected %d\n", value, 2 );
    ok( usages[0] == 4, "got usages[0] %x, expected %x\n", usages[0], 4 );
    ok( usages[1] == 6, "got usages[1] %x, expected %x\n", usages[1], 6 );

    value = ARRAY_SIZE(usages);
    memset( usages, 0xcd, sizeof(usages) );
    status = HidP_GetUsages( HidP_Input, HID_USAGE_PAGE_LED, 0, usages, &value, preparsed_data,
                             report, caps.InputReportByteLength );
    ok( status == HIDP_STATUS_SUCCESS, "HidP_GetUsages returned %#x\n", status );
    ok( value == 2, "got usage count %d, expected %d\n", value, 2 );
    ok( usages[0] == 6, "got usages[0] %x, expected %x\n", usages[0], 6 );
    ok( usages[1] == 4, "got usages[1] %x, expected %x\n", usages[1], 4 );

    value = ARRAY_SIZE(usage_and_pages);
    memset( usage_and_pages, 0xcd, sizeof(usage_and_pages) );
    status = HidP_GetUsagesEx( HidP_Input, 0, usage_and_pages, &value, preparsed_data, report,
                               caps.InputReportByteLength );
    ok( status == HIDP_STATUS_SUCCESS, "HidP_GetUsagesEx returned %#x\n", status );
    ok( value == 6, "got usage count %d, expected %d\n", value, 4 );
    ok( usage_and_pages[0].UsagePage == HID_USAGE_PAGE_BUTTON, "got usage_and_pages[0] UsagePage %x, expected %x\n",
        usage_and_pages[0].UsagePage, HID_USAGE_PAGE_BUTTON );
    ok( usage_and_pages[1].UsagePage == HID_USAGE_PAGE_BUTTON, "got usage_and_pages[1] UsagePage %x, expected %x\n",
        usage_and_pages[1].UsagePage, HID_USAGE_PAGE_BUTTON );
    ok( usage_and_pages[2].UsagePage == HID_USAGE_PAGE_KEYBOARD, "got usage_and_pages[2] UsagePage %x, expected %x\n",
        usage_and_pages[2].UsagePage, HID_USAGE_PAGE_KEYBOARD );
    ok( usage_and_pages[3].UsagePage == HID_USAGE_PAGE_KEYBOARD, "got usage_and_pages[3] UsagePage %x, expected %x\n",
        usage_and_pages[3].UsagePage, HID_USAGE_PAGE_KEYBOARD );
    ok( usage_and_pages[4].UsagePage == HID_USAGE_PAGE_LED, "got usage_and_pages[4] UsagePage %x, expected %x\n",
        usage_and_pages[4].UsagePage, HID_USAGE_PAGE_LED );
    ok( usage_and_pages[5].UsagePage == HID_USAGE_PAGE_LED, "got usage_and_pages[5] UsagePage %x, expected %x\n",
        usage_and_pages[5].UsagePage, HID_USAGE_PAGE_LED );
    ok( usage_and_pages[0].Usage == 4, "got usage_and_pages[0] Usage %x, expected %x\n",
        usage_and_pages[0].Usage, 4 );
    ok( usage_and_pages[1].Usage == 6, "got usage_and_pages[1] Usage %x, expected %x\n",
        usage_and_pages[1].Usage, 6 );
    ok( usage_and_pages[2].Usage == 9, "got usage_and_pages[2] Usage %x, expected %x\n",
        usage_and_pages[2].Usage, 9 );
    ok( usage_and_pages[3].Usage == 11, "got usage_and_pages[3] Usage %x, expected %x\n",
        usage_and_pages[3].Usage, 11 );
    ok( usage_and_pages[4].Usage == 6, "got usage_and_pages[4] Usage %x, expected %x\n",
        usage_and_pages[4].Usage, 6 );
    ok( usage_and_pages[5].Usage == 4, "got usage_and_pages[5] Usage %x, expected %x\n",
        usage_and_pages[5].Usage, 4 );

    value = HidP_MaxDataListLength( HidP_Feature + 1, preparsed_data );
    ok( value == 0, "HidP_MaxDataListLength(HidP_Feature + 1) returned %d, expected %d\n", value, 0 );
    value = HidP_MaxDataListLength( HidP_Input, preparsed_data );
    ok( value == 58, "HidP_MaxDataListLength(HidP_Input) returned %d, expected %d\n", value, 58 );
    value = HidP_MaxDataListLength( HidP_Output, preparsed_data );
    ok( value == 0, "HidP_MaxDataListLength(HidP_Output) returned %d, expected %d\n", value, 0 );
    value = HidP_MaxDataListLength( HidP_Feature, preparsed_data );
    ok( value == 14, "HidP_MaxDataListLength(HidP_Feature) returned %d, expected %d\n", value, 14 );

    value = 1;
    status = HidP_GetData( HidP_Input, data, &value, preparsed_data, report, caps.InputReportByteLength );
    ok( status == HIDP_STATUS_BUFFER_TOO_SMALL, "HidP_GetData returned %#x\n", status );
    ok( value == 11, "got data count %d, expected %d\n", value, 11 );
    memset( data, 0, sizeof(data) );
    status = HidP_GetData( HidP_Input, data, &value, preparsed_data, report, caps.InputReportByteLength );
    ok( status == HIDP_STATUS_SUCCESS, "HidP_GetData returned %#x\n", status );
    for (i = 0; i < ARRAY_SIZE(expect_data); ++i)
    {
        winetest_push_context( "data[%d]", i );
        check_member( data[i], expect_data[i], "%d", DataIndex );
        check_member( data[i], expect_data[i], "%d", RawValue );
        winetest_pop_context();
    }

    /* HID nary usage collections are set with 1-based usage index in their declaration order */

    memset( report, 0, caps.InputReportByteLength );
    status = HidP_InitializeReportForID( HidP_Input, report_id, preparsed_data, report, caps.InputReportByteLength );
    ok( status == HIDP_STATUS_SUCCESS, "HidP_InitializeReportForID returned %#x\n", status );
    value = 2;
    usages[0] = 0x8e;
    usages[1] = 0x8f;
    status = HidP_SetUsages( HidP_Input, HID_USAGE_PAGE_KEYBOARD, 0, usages, &value, preparsed_data,
                             report, caps.InputReportByteLength );
    ok( status == HIDP_STATUS_SUCCESS, "HidP_SetUsages returned %#x\n", status );
    ok( report[caps.InputReportByteLength - 2] == 3, "unexpected usage index %d, expected 3\n",
        report[caps.InputReportByteLength - 2] );
    ok( report[caps.InputReportByteLength - 1] == 4, "unexpected usage index %d, expected 4\n",
        report[caps.InputReportByteLength - 1] );
    status = HidP_UnsetUsages( HidP_Input, HID_USAGE_PAGE_KEYBOARD, 0, usages, &value,
                               preparsed_data, report, caps.InputReportByteLength );
    ok( status == HIDP_STATUS_SUCCESS, "HidP_UnsetUsages returned %#x\n", status );
    ok( report[caps.InputReportByteLength - 2] == 0, "unexpected usage index %d, expected 0\n",
        report[caps.InputReportByteLength - 2] );
    ok( report[caps.InputReportByteLength - 1] == 0, "unexpected usage index %d, expected 0\n",
        report[caps.InputReportByteLength - 1] );
    status = HidP_UnsetUsages( HidP_Input, HID_USAGE_PAGE_KEYBOARD, 0, usages, &value,
                               preparsed_data, report, caps.InputReportByteLength );
    ok( status == HIDP_STATUS_BUTTON_NOT_PRESSED, "HidP_UnsetUsages returned %#x\n", status );
    value = 1;
    usages[0] = 0x8c;
    status = HidP_SetUsages( HidP_Input, HID_USAGE_PAGE_KEYBOARD, 0, usages, &value, preparsed_data,
                             report, caps.InputReportByteLength );
    ok( status == HIDP_STATUS_SUCCESS, "HidP_SetUsages returned %#x\n", status );
    ok( report[caps.InputReportByteLength - 2] == 1, "unexpected usage index %d, expected 1\n",
        report[caps.InputReportByteLength - 2] );

    memset( report, 0xcd, sizeof(report) );
    status = HidP_InitializeReportForID( HidP_Feature, 3, preparsed_data, report, caps.FeatureReportByteLength );
    ok( status == HIDP_STATUS_REPORT_DOES_NOT_EXIST, "HidP_InitializeReportForID returned %#x\n", status );

    memset( report, 0xcd, sizeof(report) );
    status = HidP_InitializeReportForID( HidP_Feature, report_id, preparsed_data, report,
                                         caps.FeatureReportByteLength );
    ok( status == HIDP_STATUS_SUCCESS, "HidP_InitializeReportForID returned %#x\n", status );

    memset( buffer, 0xcd, sizeof(buffer) );
    memset( buffer, 0, caps.FeatureReportByteLength );
    buffer[0] = report_id;
    ok( !memcmp( buffer, report, sizeof(buffer) ), "unexpected report data\n" );

    for (i = 0; i < caps.NumberLinkCollectionNodes; ++i)
    {
        if (collections[i].LinkUsagePage != HID_USAGE_PAGE_HAPTICS) continue;
        if (collections[i].LinkUsage == HID_USAGE_HAPTICS_WAVEFORM_LIST) break;
    }
    ok( i < caps.NumberLinkCollectionNodes,
        "HID_USAGE_HAPTICS_WAVEFORM_LIST collection not found\n" );
    waveform_list = i;

    status = HidP_SetUsageValue( HidP_Feature, HID_USAGE_PAGE_ORDINAL, waveform_list, 3,
                                 HID_USAGE_HAPTICS_WAVEFORM_RUMBLE, (PHIDP_PREPARSED_DATA)buffer,
                                 report, caps.FeatureReportByteLength );
    ok( status == HIDP_STATUS_INVALID_PREPARSED_DATA, "HidP_SetUsageValue returned %#x\n", status );
    status = HidP_SetUsageValue( HidP_Feature + 1, HID_USAGE_PAGE_ORDINAL, waveform_list, 3,
                                 HID_USAGE_HAPTICS_WAVEFORM_RUMBLE, preparsed_data, report,
                                 caps.FeatureReportByteLength );
    ok( status == HIDP_STATUS_INVALID_REPORT_TYPE, "HidP_SetUsageValue returned %#x\n", status );
    status = HidP_SetUsageValue( HidP_Feature, HID_USAGE_PAGE_ORDINAL, waveform_list, 3,
                                 HID_USAGE_HAPTICS_WAVEFORM_RUMBLE, preparsed_data, report,
                                 caps.FeatureReportByteLength + 1 );
    ok( status == HIDP_STATUS_INVALID_REPORT_LENGTH, "HidP_SetUsageValue returned %#x\n", status );
    report[0] = 1 - report_id;
    status = HidP_SetUsageValue( HidP_Feature, HID_USAGE_PAGE_ORDINAL, waveform_list, 3,
                                 HID_USAGE_HAPTICS_WAVEFORM_RUMBLE, preparsed_data, report,
                                 caps.FeatureReportByteLength );
    ok( status == (report_id ? HIDP_STATUS_SUCCESS : HIDP_STATUS_INCOMPATIBLE_REPORT_ID),
        "HidP_SetUsageValue returned %#x\n", status );
    report[0] = 2;
    status = HidP_SetUsageValue( HidP_Feature, HID_USAGE_PAGE_ORDINAL, waveform_list, 3,
                                 HID_USAGE_HAPTICS_WAVEFORM_RUMBLE, preparsed_data, report,
                                 caps.FeatureReportByteLength );
    ok( status == HIDP_STATUS_INCOMPATIBLE_REPORT_ID, "HidP_SetUsageValue returned %#x\n", status );
    report[0] = report_id;
    status = HidP_SetUsageValue( HidP_Feature, HID_USAGE_PAGE_ORDINAL, 0xdead, 3, HID_USAGE_HAPTICS_WAVEFORM_RUMBLE,
                                 preparsed_data, report, caps.FeatureReportByteLength );
    ok( status == HIDP_STATUS_USAGE_NOT_FOUND, "HidP_SetUsageValue returned %#x\n", status );

    status = HidP_SetUsageValue( HidP_Feature, HID_USAGE_PAGE_ORDINAL, waveform_list, 3,
                                 HID_USAGE_HAPTICS_WAVEFORM_RUMBLE, preparsed_data, report,
                                 caps.FeatureReportByteLength );
    ok( status == HIDP_STATUS_SUCCESS, "HidP_SetUsageValue returned %#x\n", status );

    memset( buffer, 0xcd, sizeof(buffer) );
    memset( buffer, 0, caps.FeatureReportByteLength );
    buffer[0] = report_id;
    value = HID_USAGE_HAPTICS_WAVEFORM_RUMBLE;
    memcpy( buffer + 1, &value, 2 );
    ok( !memcmp( buffer, report, sizeof(buffer) ), "unexpected report data\n" );

    status = HidP_GetUsageValue( HidP_Feature, HID_USAGE_PAGE_ORDINAL, waveform_list, 3, &value,
                                 (PHIDP_PREPARSED_DATA)buffer, report, caps.FeatureReportByteLength );
    ok( status == HIDP_STATUS_INVALID_PREPARSED_DATA, "HidP_GetUsageValue returned %#x\n", status );
    status = HidP_GetUsageValue( HidP_Feature + 1, HID_USAGE_PAGE_ORDINAL, waveform_list, 3, &value,
                                 preparsed_data, report, caps.FeatureReportByteLength );
    ok( status == HIDP_STATUS_INVALID_REPORT_TYPE, "HidP_GetUsageValue returned %#x\n", status );
    status = HidP_GetUsageValue( HidP_Feature, HID_USAGE_PAGE_ORDINAL, waveform_list, 3, &value,
                                 preparsed_data, report, caps.FeatureReportByteLength + 1 );
    ok( status == HIDP_STATUS_INVALID_REPORT_LENGTH, "HidP_GetUsageValue returned %#x\n", status );
    report[0] = 1 - report_id;
    status = HidP_GetUsageValue( HidP_Feature, HID_USAGE_PAGE_ORDINAL, waveform_list, 3, &value,
                                 preparsed_data, report, caps.FeatureReportByteLength );
    ok( status == (report_id ? HIDP_STATUS_SUCCESS : HIDP_STATUS_INCOMPATIBLE_REPORT_ID),
        "HidP_GetUsageValue returned %#x\n", status );
    report[0] = 2;
    status = HidP_GetUsageValue( HidP_Feature, HID_USAGE_PAGE_ORDINAL, waveform_list, 3, &value,
                                 preparsed_data, report, caps.FeatureReportByteLength );
    ok( status == HIDP_STATUS_INCOMPATIBLE_REPORT_ID, "HidP_GetUsageValue returned %#x\n", status );
    report[0] = report_id;
    status = HidP_GetUsageValue( HidP_Feature, HID_USAGE_PAGE_ORDINAL, 0xdead, 3, &value,
                                 preparsed_data, report, caps.FeatureReportByteLength );
    ok( status == HIDP_STATUS_USAGE_NOT_FOUND, "HidP_GetUsageValue returned %#x\n", status );

    value = 0xdeadbeef;
    status = HidP_GetUsageValue( HidP_Feature, HID_USAGE_PAGE_ORDINAL, waveform_list, 3, &value,
                                 preparsed_data, report, caps.FeatureReportByteLength );
    ok( status == HIDP_STATUS_SUCCESS, "HidP_GetUsageValue returned %#x\n", status );
    ok( value == HID_USAGE_HAPTICS_WAVEFORM_RUMBLE, "got value %x, expected %#x\n", value,
        HID_USAGE_HAPTICS_WAVEFORM_RUMBLE );

    memset( buffer, 0xff, sizeof(buffer) );
    status = HidP_SetUsageValueArray( HidP_Feature, HID_USAGE_PAGE_HAPTICS, 0,
                                      HID_USAGE_HAPTICS_WAVEFORM_CUTOFF_TIME, buffer, 0,
                                      preparsed_data, report, caps.FeatureReportByteLength );
    ok( status == HIDP_STATUS_BUFFER_TOO_SMALL, "HidP_SetUsageValueArray returned %#x\n", status );
    status = HidP_SetUsageValueArray( HidP_Feature, HID_USAGE_PAGE_HAPTICS, 0,
                                      HID_USAGE_HAPTICS_WAVEFORM_CUTOFF_TIME, buffer, 64,
                                      preparsed_data, report, caps.FeatureReportByteLength );
    ok( status == HIDP_STATUS_SUCCESS, "HidP_SetUsageValueArray returned %#x\n", status );
    ok( !memcmp( report + 9, buffer, 8 ), "unexpected report data\n" );

    memset( buffer, 0, sizeof(buffer) );
    status = HidP_GetUsageValueArray( HidP_Feature, HID_USAGE_PAGE_HAPTICS, 0,
                                      HID_USAGE_HAPTICS_WAVEFORM_CUTOFF_TIME, buffer, 0,
                                      preparsed_data, report, caps.FeatureReportByteLength );
    ok( status == HIDP_STATUS_BUFFER_TOO_SMALL, "HidP_GetUsageValueArray returned %#x\n", status );
    status = HidP_GetUsageValueArray( HidP_Feature, HID_USAGE_PAGE_HAPTICS, 0,
                                      HID_USAGE_HAPTICS_WAVEFORM_CUTOFF_TIME, buffer, 64,
                                      preparsed_data, report, caps.FeatureReportByteLength );
    ok( status == HIDP_STATUS_SUCCESS, "HidP_GetUsageValueArray returned %#x\n", status );
    memset( buffer + 16, 0xff, 8 );
    ok( !memcmp( buffer, buffer + 16, 16 ), "unexpected report value\n" );

    value = 0x7fffffff;
    status = HidP_SetUsageValue( HidP_Feature, HID_USAGE_PAGE_GENERIC, 0, HID_USAGE_GENERIC_Z,
                                 value, preparsed_data, report, caps.FeatureReportByteLength );
    ok( status == HIDP_STATUS_SUCCESS, "HidP_SetUsageValue returned %#x\n", status );
    value = 0xdeadbeef;
    status = HidP_GetScaledUsageValue( HidP_Feature, HID_USAGE_PAGE_GENERIC, 0, HID_USAGE_GENERIC_Z,
                                       (LONG *)&value, preparsed_data, report, caps.FeatureReportByteLength );
    ok( status == HIDP_STATUS_VALUE_OUT_OF_RANGE, "HidP_GetScaledUsageValue returned %#x\n", status );
    ok( value == 0, "got value %x, expected %#x\n", value, 0 );
    value = 0xdeadbeef;
    status = HidP_GetUsageValue( HidP_Feature, HID_USAGE_PAGE_GENERIC, 0, HID_USAGE_GENERIC_Z,
                                 &value, preparsed_data, report, caps.FeatureReportByteLength );
    ok( status == HIDP_STATUS_SUCCESS, "HidP_GetUsageValue returned %#x\n", status );
    ok( value == 0x7fffffff, "got value %x, expected %#x\n", value, 0x7fffffff );

    value = 0x7fff;
    status = HidP_SetUsageValue( HidP_Feature, HID_USAGE_PAGE_GENERIC, 0, HID_USAGE_GENERIC_Z,
                                 value, preparsed_data, report, caps.FeatureReportByteLength );
    ok( status == HIDP_STATUS_SUCCESS, "HidP_SetUsageValue returned %#x\n", status );
    value = 0xdeadbeef;
    status = HidP_GetScaledUsageValue( HidP_Feature, HID_USAGE_PAGE_GENERIC, 0, HID_USAGE_GENERIC_Z,
                                       (LONG *)&value, preparsed_data, report, caps.FeatureReportByteLength );
    ok( status == HIDP_STATUS_SUCCESS, "HidP_GetScaledUsageValue returned %#x\n", status );
    ok( value == 0x0003ffff, "got value %x, expected %#x\n", value, 0x0003ffff );

    value = 0;
    status = HidP_SetUsageValue( HidP_Feature, HID_USAGE_PAGE_GENERIC, 0, HID_USAGE_GENERIC_Z,
                                 value, preparsed_data, report, caps.FeatureReportByteLength );
    ok( status == HIDP_STATUS_SUCCESS, "HidP_SetUsageValue returned %#x\n", status );
    value = 0xdeadbeef;
    status = HidP_GetScaledUsageValue( HidP_Feature, HID_USAGE_PAGE_GENERIC, 0, HID_USAGE_GENERIC_Z,
                                       (LONG *)&value, preparsed_data, report, caps.FeatureReportByteLength );
    ok( status == HIDP_STATUS_SUCCESS, "HidP_GetScaledUsageValue returned %#x\n", status );
    ok( value == 0xfff90000, "got value %x, expected %#x\n", value, 0xfff90000 );
    status = HidP_SetScaledUsageValue( HidP_Feature, HID_USAGE_PAGE_GENERIC, 0, HID_USAGE_GENERIC_Z,
                                       0x1000, preparsed_data, report, caps.FeatureReportByteLength );
    ok( status == HIDP_STATUS_SUCCESS, "HidP_SetScaledUsageValue returned %#x\n", status );
    value = 0;
    status = HidP_GetUsageValue( HidP_Feature, HID_USAGE_PAGE_GENERIC, 0, HID_USAGE_GENERIC_Z,
                                 &value, preparsed_data, report, caps.FeatureReportByteLength );
    ok( status == HIDP_STATUS_SUCCESS, "HidP_GetUsageValue returned %#x\n", status );
    ok( value == 0xfffff518, "got value %x, expected %#x\n", value, 0xfffff518 );
    status = HidP_SetScaledUsageValue( HidP_Feature, HID_USAGE_PAGE_GENERIC, 0, HID_USAGE_GENERIC_Z,
                                       0, preparsed_data, report, caps.FeatureReportByteLength );
    ok( status == HIDP_STATUS_SUCCESS, "HidP_SetScaledUsageValue returned %#x\n", status );
    value = 0;
    status = HidP_GetUsageValue( HidP_Feature, HID_USAGE_PAGE_GENERIC, 0, HID_USAGE_GENERIC_Z,
                                 &value, preparsed_data, report, caps.FeatureReportByteLength );
    ok( status == HIDP_STATUS_SUCCESS, "HidP_GetUsageValue returned %#x\n", status );
    ok( value == 0xfffff45e, "got value %x, expected %#x\n", value, 0xfffff45e );
    status = HidP_SetScaledUsageValue( HidP_Feature, HID_USAGE_PAGE_GENERIC, 0, HID_USAGE_GENERIC_Z,
                                       0xdead, preparsed_data, report, caps.FeatureReportByteLength );
    ok( status == HIDP_STATUS_SUCCESS, "HidP_SetScaledUsageValue returned %#x\n", status );
    value = 0;
    status = HidP_GetUsageValue( HidP_Feature, HID_USAGE_PAGE_GENERIC, 0, HID_USAGE_GENERIC_Z,
                                 &value, preparsed_data, report, caps.FeatureReportByteLength );
    ok( status == HIDP_STATUS_SUCCESS, "HidP_GetUsageValue returned %#x\n", status );
    ok( value == 0xfffffe7d, "got value %x, expected %#x\n", value, 0xfffffe7d );
    status = HidP_SetScaledUsageValue( HidP_Feature, HID_USAGE_PAGE_GENERIC, 0, HID_USAGE_GENERIC_Z,
                                       0xbeef, preparsed_data, report, caps.FeatureReportByteLength );
    ok( status == HIDP_STATUS_SUCCESS, "HidP_SetScaledUsageValue returned %#x\n", status );
    value = 0;
    status = HidP_GetUsageValue( HidP_Feature, HID_USAGE_PAGE_GENERIC, 0, HID_USAGE_GENERIC_Z,
                                 &value, preparsed_data, report, caps.FeatureReportByteLength );
    ok( status == HIDP_STATUS_SUCCESS, "HidP_GetUsageValue returned %#x\n", status );
    ok( value == 0xfffffd0b, "got value %x, expected %#x\n", value, 0xfffffd0b );

    test_hidp_get_input( file, report_id, caps.InputReportByteLength, preparsed_data );
    test_hidp_get_feature( file, report_id, caps.FeatureReportByteLength, preparsed_data );
    test_hidp_set_feature( file, report_id, caps.FeatureReportByteLength, preparsed_data );
    test_hidp_set_output( file, report_id, caps.OutputReportByteLength, preparsed_data );
    test_write_file( file, report_id, caps.OutputReportByteLength );

    memset( report, 0xcd, sizeof(report) );
    SetLastError( 0xdeadbeef );
    ret = ReadFile( file, report, 0, &value, NULL );
    ok( !ret && GetLastError() == ERROR_INVALID_USER_BUFFER, "ReadFile failed, last error %u\n",
        GetLastError() );
    ok( value == 0, "ReadFile returned %x\n", value );
    SetLastError( 0xdeadbeef );
    ret = ReadFile( file, report, caps.InputReportByteLength - 1, &value, NULL );
    ok( !ret && GetLastError() == ERROR_INVALID_USER_BUFFER, "ReadFile failed, last error %u\n",
        GetLastError() );
    ok( value == 0, "ReadFile returned %x\n", value );

    if (polled)
    {
        struct hid_expect expect[] =
        {
            {
                .code = IOCTL_HID_READ_REPORT,
                .report_len = caps.InputReportByteLength - (report_id ? 0 : 1),
                .report_buf = {report_id ? report_id : 0x5a,0x5a,0},
                .ret_length = 3,
                .ret_status = STATUS_SUCCESS,
            },
            {
                .code = IOCTL_HID_READ_REPORT,
                .report_len = caps.InputReportByteLength - (report_id ? 0 : 1),
                .report_buf = {report_id ? report_id : 0x5a,0x5a,1},
                .ret_length = 3,
                .ret_status = STATUS_SUCCESS,
            },
        };

        send_hid_input( file, expect, sizeof(expect) );

        memset( report, 0xcd, sizeof(report) );
        SetLastError( 0xdeadbeef );
        ret = ReadFile( file, report, caps.InputReportByteLength, &value, NULL );
        ok( ret, "ReadFile failed, last error %u\n", GetLastError() );
        ok( value == (report_id ? 3 : 4), "ReadFile returned %x\n", value );
        ok( report[0] == report_id, "unexpected report data\n" );

        overlapped.hEvent = CreateEventW( NULL, FALSE, FALSE, NULL );
        overlapped2.hEvent = CreateEventW( NULL, FALSE, FALSE, NULL );

        /* drain available input reports */
        SetLastError( 0xdeadbeef );
        while (ReadFile( async_file, report, caps.InputReportByteLength, NULL, &overlapped ))
            ResetEvent( overlapped.hEvent );
        ok( GetLastError() == ERROR_IO_PENDING, "ReadFile returned error %u\n", GetLastError() );
        ret = GetOverlappedResult( async_file, &overlapped, &value, TRUE );
        ok( ret, "GetOverlappedResult failed, last error %u\n", GetLastError() );
        ok( value == (report_id ? 3 : 4), "GetOverlappedResult returned length %u, expected %u\n",
            value, (report_id ? 3 : 4) );
        ResetEvent( overlapped.hEvent );

        memcpy( buffer, report, caps.InputReportByteLength );
        memcpy( buffer + caps.InputReportByteLength, report, caps.InputReportByteLength );

        SetLastError( 0xdeadbeef );
        ret = ReadFile( async_file, report, caps.InputReportByteLength, NULL, &overlapped );
        ok( !ret, "ReadFile succeeded\n" );
        ok( GetLastError() == ERROR_IO_PENDING, "ReadFile returned error %u\n", GetLastError() );

        SetLastError( 0xdeadbeef );
        ret = ReadFile( async_file, buffer, caps.InputReportByteLength, NULL, &overlapped2 );
        ok( !ret, "ReadFile succeeded\n" );
        ok( GetLastError() == ERROR_IO_PENDING, "ReadFile returned error %u\n", GetLastError() );

        /* wait for second report to be ready */
        ret = GetOverlappedResult( async_file, &overlapped2, &value, TRUE );
        ok( ret, "GetOverlappedResult failed, last error %u\n", GetLastError() );
        ok( value == (report_id ? 3 : 4), "GetOverlappedResult returned length %u, expected %u\n",
            value, (report_id ? 3 : 4) );
        /* first report should be ready and the same */
        ret = GetOverlappedResult( async_file, &overlapped, &value, FALSE );
        ok( ret, "GetOverlappedResult failed, last error %u\n", GetLastError() );
        ok( value == (report_id ? 3 : 4), "GetOverlappedResult returned length %u, expected %u\n",
            value, (report_id ? 3 : 4) );
        ok( memcmp( report, buffer + caps.InputReportByteLength, caps.InputReportByteLength ),
            "expected different report\n" );
        ok( !memcmp( report, buffer, caps.InputReportByteLength ), "expected identical reports\n" );

        CloseHandle( overlapped.hEvent );
        CloseHandle( overlapped2.hEvent );
    }
    else
    {
        struct hid_expect expect[] =
        {
            {
                .code = IOCTL_HID_READ_REPORT,
                .report_len = caps.InputReportByteLength - (report_id ? 0 : 1),
                .report_buf = {report_id ? report_id : 0x5a,0x5a,0x5a},
                .ret_length = 3,
                .ret_status = STATUS_SUCCESS,
            },
            {
                .code = IOCTL_HID_READ_REPORT,
                .report_len = caps.InputReportByteLength - (report_id ? 0 : 1),
                .report_buf = {report_id ? report_id : 0xa5,0xa5,0xa5,0xa5,0xa5},
                .ret_length = caps.InputReportByteLength - (report_id ? 0 : 1),
                .ret_status = STATUS_SUCCESS,
            },
        };

        overlapped.hEvent = CreateEventW( NULL, FALSE, FALSE, NULL );
        overlapped2.hEvent = CreateEventW( NULL, FALSE, FALSE, NULL );

        SetLastError( 0xdeadbeef );
        memset( report, 0, sizeof(report) );
        ret = ReadFile( async_file, report, caps.InputReportByteLength, NULL, &overlapped );
        ok( !ret, "ReadFile succeeded\n" );
        ok( GetLastError() == ERROR_IO_PENDING, "ReadFile returned error %u\n", GetLastError() );
        Sleep( 50 );
        ret = GetOverlappedResult( async_file, &overlapped, &value, FALSE );
        ok( !ret, "GetOverlappedResult succeeded\n" );
        ok( GetLastError() == ERROR_IO_INCOMPLETE, "GetOverlappedResult returned error %u\n", GetLastError() );

        SetLastError( 0xdeadbeef );
        memset( buffer, 0, sizeof(buffer) );
        ret = ReadFile( async_file, buffer, caps.InputReportByteLength, NULL, &overlapped2 );
        ok( !ret, "ReadFile succeeded\n" );
        ok( GetLastError() == ERROR_IO_PENDING, "ReadFile returned error %u\n", GetLastError() );
        Sleep( 50 );
        ret = GetOverlappedResult( async_file, &overlapped2, &value, FALSE );
        ok( !ret, "GetOverlappedResult succeeded\n" );
        ok( GetLastError() == ERROR_IO_INCOMPLETE, "GetOverlappedResult returned error %u\n", GetLastError() );

        memset( report + caps.InputReportByteLength, 0xa5, 5 );
        if (report_id) report[caps.InputReportByteLength] = report_id;

        send_hid_input( file, expect, sizeof(expect) );

        /* first read should be completed */
        ret = GetOverlappedResult( async_file, &overlapped, &value, TRUE );
        ok( ret, "GetOverlappedResult failed, last error %u\n", GetLastError() );
        ok( value == caps.InputReportByteLength, "got length %u, expected %u\n", value, caps.InputReportByteLength );
        /* second read should still be pending */
        Sleep( 50 );
        ret = GetOverlappedResult( async_file, &overlapped2, &value, FALSE );
        ok( !ret, "GetOverlappedResult succeeded\n" );
        ok( GetLastError() == ERROR_IO_INCOMPLETE, "GetOverlappedResult returned error %u\n", GetLastError() );

        memset( buffer + caps.InputReportByteLength, 0x3b, 5 );
        if (report_id) buffer[caps.InputReportByteLength] = report_id;
        memset( expect[1].report_buf, 0x3b, 5 );
        if (report_id) expect[1].report_buf[0] = report_id;

        send_hid_input( file, expect, sizeof(expect) );

        ret = GetOverlappedResult( async_file, &overlapped2, &value, TRUE );
        ok( ret, "GetOverlappedResult failed, last error %u\n", GetLastError() );
        ok( value == caps.InputReportByteLength, "got length %u, expected %u\n", value, caps.InputReportByteLength );

        off = report_id ? 0 : 1;
        ok( memcmp( report, buffer, caps.InputReportByteLength ), "expected different report\n" );
        ok( !memcmp( report + off, report + caps.InputReportByteLength, caps.InputReportByteLength - off ),
            "expected identical reports\n" );
        ok( !memcmp( buffer + off, buffer + caps.InputReportByteLength, caps.InputReportByteLength - off ),
            "expected identical reports\n" );

        CloseHandle( overlapped.hEvent );
        CloseHandle( overlapped2.hEvent );
    }

    HidD_FreePreparsedData( preparsed_data );
}

static void test_hid_device( DWORD report_id, DWORD polled, const HIDP_CAPS *expect_caps )
{
    char buffer[FIELD_OFFSET( SP_DEVICE_INTERFACE_DETAIL_DATA_W, DevicePath[MAX_PATH] )];
    SP_DEVICE_INTERFACE_DATA iface = {sizeof(SP_DEVICE_INTERFACE_DATA)};
    SP_DEVICE_INTERFACE_DETAIL_DATA_W *iface_detail = (void *)buffer;
    SP_DEVINFO_DATA device = {sizeof(SP_DEVINFO_DATA)};
    ULONG count, poll_freq, out_len;
    HANDLE file, async_file;
    BOOL ret, found = FALSE;
    OBJECT_ATTRIBUTES attr;
    UNICODE_STRING string;
    IO_STATUS_BLOCK io;
    NTSTATUS status;
    unsigned int i;
    HDEVINFO set;

    winetest_push_context( "id %d%s", report_id, polled ? " poll" : "" );

    set = SetupDiGetClassDevsW( &GUID_DEVINTERFACE_HID, NULL, NULL, DIGCF_DEVICEINTERFACE | DIGCF_PRESENT );
    ok( set != INVALID_HANDLE_VALUE, "failed to get device list, error %#x\n", GetLastError() );

    for (i = 0; SetupDiEnumDeviceInfo( set, i, &device ); ++i)
    {
        ret = SetupDiEnumDeviceInterfaces( set, &device, &GUID_DEVINTERFACE_HID, 0, &iface );
        ok( ret, "failed to get interface, error %#x\n", GetLastError() );
        ok( IsEqualGUID( &iface.InterfaceClassGuid, &GUID_DEVINTERFACE_HID ), "wrong class %s\n",
            debugstr_guid( &iface.InterfaceClassGuid ) );
        ok( iface.Flags == SPINT_ACTIVE, "got flags %#x\n", iface.Flags );

        iface_detail->cbSize = sizeof(*iface_detail);
        ret = SetupDiGetDeviceInterfaceDetailW( set, &iface, iface_detail, sizeof(buffer), NULL, NULL );
        ok( ret, "failed to get interface path, error %#x\n", GetLastError() );

        if (wcsstr( iface_detail->DevicePath, L"\\\\?\\hid#winetest#1" ))
        {
            found = TRUE;
            break;
        }
    }

    SetupDiDestroyDeviceInfoList( set );

    todo_wine
    ok( found, "didn't find device\n" );

    file = CreateFileW( iface_detail->DevicePath, FILE_READ_ACCESS | FILE_WRITE_ACCESS,
                        FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL );
    ok( file != INVALID_HANDLE_VALUE, "got error %u\n", GetLastError() );

    count = 0xdeadbeef;
    SetLastError( 0xdeadbeef );
    ret = HidD_GetNumInputBuffers( file, &count );
    ok( ret, "HidD_GetNumInputBuffers failed last error %u\n", GetLastError() );
    ok( count == 32, "HidD_GetNumInputBuffers returned %u\n", count );

    SetLastError( 0xdeadbeef );
    ret = HidD_SetNumInputBuffers( file, 1 );
    ok( !ret, "HidD_SetNumInputBuffers succeeded\n" );
    ok( GetLastError() == ERROR_INVALID_PARAMETER, "HidD_SetNumInputBuffers returned error %u\n",
        GetLastError() );
    SetLastError( 0xdeadbeef );
    ret = HidD_SetNumInputBuffers( file, 513 );
    ok( !ret, "HidD_SetNumInputBuffers succeeded\n" );
    ok( GetLastError() == ERROR_INVALID_PARAMETER, "HidD_SetNumInputBuffers returned error %u\n",
        GetLastError() );

    SetLastError( 0xdeadbeef );
    ret = HidD_SetNumInputBuffers( file, 16 );
    ok( ret, "HidD_SetNumInputBuffers failed last error %u\n", GetLastError() );

    count = 0xdeadbeef;
    SetLastError( 0xdeadbeef );
    ret = HidD_GetNumInputBuffers( file, &count );
    ok( ret, "HidD_GetNumInputBuffers failed last error %u\n", GetLastError() );
    ok( count == 16, "HidD_GetNumInputBuffers returned %u\n", count );

    async_file = CreateFileW( iface_detail->DevicePath, FILE_READ_ACCESS | FILE_WRITE_ACCESS,
                              FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
                              FILE_FLAG_OVERLAPPED | FILE_FLAG_NO_BUFFERING, NULL );
    ok( async_file != INVALID_HANDLE_VALUE, "got error %u\n", GetLastError() );

    count = 0xdeadbeef;
    SetLastError( 0xdeadbeef );
    ret = HidD_GetNumInputBuffers( async_file, &count );
    ok( ret, "HidD_GetNumInputBuffers failed last error %u\n", GetLastError() );
    ok( count == 32, "HidD_GetNumInputBuffers returned %u\n", count );

    SetLastError( 0xdeadbeef );
    ret = HidD_SetNumInputBuffers( async_file, 2 );
    ok( ret, "HidD_SetNumInputBuffers failed last error %u\n", GetLastError() );

    count = 0xdeadbeef;
    SetLastError( 0xdeadbeef );
    ret = HidD_GetNumInputBuffers( async_file, &count );
    ok( ret, "HidD_GetNumInputBuffers failed last error %u\n", GetLastError() );
    ok( count == 2, "HidD_GetNumInputBuffers returned %u\n", count );
    count = 0xdeadbeef;
    SetLastError( 0xdeadbeef );
    ret = HidD_GetNumInputBuffers( file, &count );
    ok( ret, "HidD_GetNumInputBuffers failed last error %u\n", GetLastError() );
    ok( count == 16, "HidD_GetNumInputBuffers returned %u\n", count );

    if (polled)
    {
        out_len = sizeof(ULONG);
        SetLastError( 0xdeadbeef );
        ret = sync_ioctl( file, IOCTL_HID_GET_POLL_FREQUENCY_MSEC, NULL, 0, &poll_freq, &out_len );
        ok( ret, "IOCTL_HID_GET_POLL_FREQUENCY_MSEC failed last error %u\n", GetLastError() );
        ok( out_len == sizeof(ULONG), "got out_len %u, expected sizeof(ULONG)\n", out_len );
        todo_wine
        ok( poll_freq == 5, "got poll_freq %u, expected 5\n", poll_freq );

        out_len = 0;
        poll_freq = 500;
        SetLastError( 0xdeadbeef );
        ret = sync_ioctl( file, IOCTL_HID_SET_POLL_FREQUENCY_MSEC, &poll_freq, sizeof(ULONG), NULL, &out_len );
        ok( ret, "IOCTL_HID_SET_POLL_FREQUENCY_MSEC failed last error %u\n", GetLastError() );
        ok( out_len == 0, "got out_len %u, expected 0\n", out_len );

        out_len = 0;
        poll_freq = 10001;
        SetLastError( 0xdeadbeef );
        ret = sync_ioctl( file, IOCTL_HID_SET_POLL_FREQUENCY_MSEC, &poll_freq, sizeof(ULONG), NULL, &out_len );
        ok( ret, "IOCTL_HID_SET_POLL_FREQUENCY_MSEC failed last error %u\n", GetLastError() );
        ok( out_len == 0, "got out_len %u, expected 0\n", out_len );

        out_len = 0;
        poll_freq = 0;
        SetLastError( 0xdeadbeef );
        ret = sync_ioctl( file, IOCTL_HID_SET_POLL_FREQUENCY_MSEC, &poll_freq, sizeof(ULONG), NULL, &out_len );
        ok( ret, "IOCTL_HID_SET_POLL_FREQUENCY_MSEC failed last error %u\n", GetLastError() );
        ok( out_len == 0, "got out_len %u, expected 0\n", out_len );

        out_len = sizeof(ULONG);
        SetLastError( 0xdeadbeef );
        ret = sync_ioctl( file, IOCTL_HID_GET_POLL_FREQUENCY_MSEC, NULL, 0, &poll_freq, &out_len );
        ok( ret, "IOCTL_HID_GET_POLL_FREQUENCY_MSEC failed last error %u\n", GetLastError() );
        ok( out_len == sizeof(ULONG), "got out_len %u, expected sizeof(ULONG)\n", out_len );
        ok( poll_freq == 10000, "got poll_freq %u, expected 10000\n", poll_freq );

        out_len = 0;
        poll_freq = 500;
        SetLastError( 0xdeadbeef );
        ret = sync_ioctl( file, IOCTL_HID_SET_POLL_FREQUENCY_MSEC, &poll_freq, sizeof(ULONG), NULL, &out_len );
        ok( ret, "IOCTL_HID_SET_POLL_FREQUENCY_MSEC failed last error %u\n", GetLastError() );
        ok( out_len == 0, "got out_len %u, expected 0\n", out_len );

        out_len = sizeof(ULONG);
        SetLastError( 0xdeadbeef );
        ret = sync_ioctl( async_file, IOCTL_HID_GET_POLL_FREQUENCY_MSEC, NULL, 0, &poll_freq, &out_len );
        ok( ret, "IOCTL_HID_GET_POLL_FREQUENCY_MSEC failed last error %u\n", GetLastError() );
        ok( out_len == sizeof(ULONG), "got out_len %u, expected sizeof(ULONG)\n", out_len );
        ok( poll_freq == 500, "got poll_freq %u, expected 500\n", poll_freq );
    }

    test_hidp( file, async_file, report_id, polled, expect_caps );

    CloseHandle( async_file );
    CloseHandle( file );

    RtlInitUnicodeString( &string, L"\\??\\root#winetest#0#{deadbeef-29ef-4538-a5fd-b69573a362c0}" );
    InitializeObjectAttributes( &attr, &string, OBJ_CASE_INSENSITIVE, NULL, NULL );
    status = NtOpenFile( &file, SYNCHRONIZE, &attr, &io, 0, FILE_SYNCHRONOUS_IO_NONALERT );
    todo_wine
    ok( status == STATUS_UNSUCCESSFUL, "got %#x\n", status );

    winetest_pop_context();
}

static void test_hid_driver( DWORD report_id, DWORD polled )
{
#include "psh_hid_macros.h"
/* Replace REPORT_ID with USAGE_PAGE when id is 0 */
#define REPORT_ID_OR_USAGE_PAGE(size, id, off) SHORT_ITEM_1((id ? 8 : 0), 1, (id + off))
    const unsigned char report_desc[] =
    {
        USAGE_PAGE(1, HID_USAGE_PAGE_GENERIC),
        USAGE(1, HID_USAGE_GENERIC_JOYSTICK),
        COLLECTION(1, Application),
            USAGE(1, HID_USAGE_GENERIC_JOYSTICK),
            COLLECTION(1, Logical),
                REPORT_ID_OR_USAGE_PAGE(1, report_id, 0),
                USAGE_PAGE(1, HID_USAGE_PAGE_GENERIC),
                USAGE(1, HID_USAGE_GENERIC_X),
                USAGE(1, HID_USAGE_GENERIC_Y),
                LOGICAL_MINIMUM(1, -128),
                LOGICAL_MAXIMUM(1, 127),
                REPORT_SIZE(1, 8),
                REPORT_COUNT(1, 2),
                INPUT(1, Data|Var|Abs),

                USAGE_PAGE(1, HID_USAGE_PAGE_BUTTON),
                USAGE_MINIMUM(1, 1),
                USAGE_MAXIMUM(1, 8),
                LOGICAL_MINIMUM(1, 0),
                LOGICAL_MAXIMUM(1, 1),
                REPORT_COUNT(1, 8),
                REPORT_SIZE(1, 1),
                INPUT(1, Data|Var|Abs),

                USAGE_MINIMUM(1, 0x18),
                USAGE_MAXIMUM(1, 0x1f),
                LOGICAL_MINIMUM(1, 0),
                LOGICAL_MAXIMUM(1, 1),
                REPORT_COUNT(1, 8),
                REPORT_SIZE(1, 1),
                INPUT(1, Cnst|Var|Abs),
                REPORT_COUNT(1, 8),
                REPORT_SIZE(1, 1),
                INPUT(1, Cnst|Var|Abs),
                /* needs to be 8 bit aligned as next has Buff */

                USAGE_MINIMUM(4, (HID_USAGE_PAGE_KEYBOARD<<16)|0x8),
                USAGE_MAXIMUM(4, (HID_USAGE_PAGE_KEYBOARD<<16)|0xf),
                LOGICAL_MINIMUM(1, 0),
                LOGICAL_MAXIMUM(1, 8),
                REPORT_COUNT(1, 2),
                REPORT_SIZE(1, 8),
                INPUT(2, Data|Ary|Rel|Wrap|Lin|Pref|Null|Vol|Buff),

                /* needs to be 8 bit aligned as previous has Buff */
                USAGE(1, 0x20),
                LOGICAL_MINIMUM(1, 0),
                LOGICAL_MAXIMUM(1, 1),
                REPORT_COUNT(1, 8),
                REPORT_SIZE(1, 1),
                INPUT(1, Data|Var|Abs),
                USAGE_MINIMUM(1, 0x21),
                USAGE_MAXIMUM(1, 0x22),
                REPORT_COUNT(1, 2),
                REPORT_SIZE(1, 0),
                INPUT(1, Data|Var|Abs),
                USAGE(1, 0x23),
                REPORT_COUNT(1, 0),
                REPORT_SIZE(1, 1),
                INPUT(1, Data|Var|Abs),

                USAGE_PAGE(1, HID_USAGE_PAGE_GENERIC),
                USAGE(1, HID_USAGE_GENERIC_HATSWITCH),
                LOGICAL_MINIMUM(1, 1),
                LOGICAL_MAXIMUM(1, 8),
                REPORT_SIZE(1, 4),
                REPORT_COUNT(1, 2),
                INPUT(1, Data|Var|Abs),

                USAGE_PAGE(1, HID_USAGE_PAGE_GENERIC),
                USAGE(1, HID_USAGE_GENERIC_Z),
                LOGICAL_MINIMUM(4, 0x00000000),
                LOGICAL_MAXIMUM(4, 0x3fffffff),
                PHYSICAL_MINIMUM(4, 0x80000000),
                PHYSICAL_MAXIMUM(4, 0x7fffffff),
                REPORT_SIZE(1, 32),
                REPORT_COUNT(1, 1),
                INPUT(1, Data|Var|Abs),

                /* reset physical range to its default interpretation */
                USAGE_PAGE(1, HID_USAGE_PAGE_GENERIC),
                USAGE(1, HID_USAGE_GENERIC_RX),
                PHYSICAL_MINIMUM(4, 0),
                PHYSICAL_MAXIMUM(4, 0),
                REPORT_SIZE(1, 32),
                REPORT_COUNT(1, 1),
                INPUT(1, Data|Var|Abs),

                USAGE_PAGE(1, HID_USAGE_PAGE_GENERIC),
                USAGE(1, HID_USAGE_GENERIC_RY),
                LOGICAL_MINIMUM(4, 0x7fff),
                LOGICAL_MAXIMUM(4, 0x0000),
                PHYSICAL_MINIMUM(4, 0x0000),
                PHYSICAL_MAXIMUM(4, 0x7fff),
                REPORT_SIZE(1, 32),
                REPORT_COUNT(1, 1),
                INPUT(1, Data|Var|Abs),
            END_COLLECTION,

            USAGE_PAGE(1, HID_USAGE_PAGE_GENERIC),
            USAGE(1, HID_USAGE_GENERIC_JOYSTICK),
            COLLECTION(1, Report),
                REPORT_ID_OR_USAGE_PAGE(1, report_id, 1),
                USAGE_PAGE(1, HID_USAGE_PAGE_BUTTON),
                USAGE_MINIMUM(1, 9),
                USAGE_MAXIMUM(1, 10),
                LOGICAL_MINIMUM(1, 0),
                LOGICAL_MAXIMUM(1, 1),
                REPORT_COUNT(1, 8),
                REPORT_SIZE(1, 1),
                INPUT(1, Data|Var|Abs),
            END_COLLECTION,

            USAGE_PAGE(1, HID_USAGE_PAGE_LED),
            USAGE(1, HID_USAGE_LED_GREEN),
            COLLECTION(1, Report),
                REPORT_ID_OR_USAGE_PAGE(1, report_id, 0),
                USAGE_PAGE(1, HID_USAGE_PAGE_LED),
                USAGE(1, 1),
                USAGE(1, 2),
                USAGE(1, 3),
                USAGE(1, 4),
                USAGE(1, 5),
                USAGE(1, 6),
                USAGE(1, 7),
                USAGE(1, 8),
                LOGICAL_MINIMUM(1, 0),
                LOGICAL_MAXIMUM(1, 1),
                PHYSICAL_MINIMUM(1, 0),
                PHYSICAL_MAXIMUM(1, 1),
                REPORT_COUNT(1, 8),
                REPORT_SIZE(1, 1),
                INPUT(1, Data|Var|Abs),

                USAGE(4, (HID_USAGE_PAGE_KEYBOARD<<16)|0x8c),
                USAGE(4, (HID_USAGE_PAGE_KEYBOARD<<16)|0x8d),
                USAGE(4, (HID_USAGE_PAGE_KEYBOARD<<16)|0x8e),
                USAGE(4, (HID_USAGE_PAGE_KEYBOARD<<16)|0x8f),
                LOGICAL_MINIMUM(1, 1),
                LOGICAL_MAXIMUM(1, 16),
                REPORT_COUNT(1, 2),
                REPORT_SIZE(1, 8),
                INPUT(1, Data|Ary|Abs),
            END_COLLECTION,

            USAGE_PAGE(2, HID_USAGE_PAGE_HAPTICS),
            USAGE(1, HID_USAGE_HAPTICS_SIMPLE_CONTROLLER),
            COLLECTION(1, Logical),
                REPORT_ID_OR_USAGE_PAGE(1, report_id, 0),
                USAGE_PAGE(2, HID_USAGE_PAGE_HAPTICS),

                USAGE(1, HID_USAGE_HAPTICS_WAVEFORM_LIST),
                COLLECTION(1, NamedArray),
                    USAGE_PAGE(1, HID_USAGE_PAGE_ORDINAL),
                    USAGE(1, 3), /* HID_USAGE_HAPTICS_WAVEFORM_RUMBLE */
                    USAGE(1, 4), /* HID_USAGE_HAPTICS_WAVEFORM_BUZZ */
                    LOGICAL_MINIMUM(2, 0x0000),
                    LOGICAL_MAXIMUM(2, 0xffff),
                    REPORT_COUNT(1, 2),
                    REPORT_SIZE(1, 16),
                    FEATURE(1, Data|Var|Abs|Null),
                END_COLLECTION,

                USAGE_PAGE(2, HID_USAGE_PAGE_HAPTICS),
                USAGE(1, HID_USAGE_HAPTICS_DURATION_LIST),
                COLLECTION(1, NamedArray),
                    USAGE_PAGE(1, HID_USAGE_PAGE_ORDINAL),
                    USAGE(1, 3), /* 0 (HID_USAGE_HAPTICS_WAVEFORM_RUMBLE) */
                    USAGE(1, 4), /* 0 (HID_USAGE_HAPTICS_WAVEFORM_BUZZ) */
                    LOGICAL_MINIMUM(2, 0x0000),
                    LOGICAL_MAXIMUM(2, 0xffff),
                    REPORT_COUNT(1, 2),
                    REPORT_SIZE(1, 16),
                    FEATURE(1, Data|Var|Abs|Null),
                END_COLLECTION,

                USAGE_PAGE(2, HID_USAGE_PAGE_HAPTICS),
                USAGE(1, HID_USAGE_HAPTICS_WAVEFORM_CUTOFF_TIME),
                UNIT(2, 0x1001), /* seconds */
                UNIT_EXPONENT(1, -3), /* 10^-3 */
                LOGICAL_MINIMUM(2, 0x8000),
                LOGICAL_MAXIMUM(2, 0x7fff),
                PHYSICAL_MINIMUM(4, 0x00000000),
                PHYSICAL_MAXIMUM(4, 0xffffffff),
                REPORT_SIZE(1, 32),
                REPORT_COUNT(1, 2),
                FEATURE(1, Data|Var|Abs),
                /* reset global items */
                UNIT(1, 0), /* None */
                UNIT_EXPONENT(1, 0),

                USAGE_PAGE(1, HID_USAGE_PAGE_GENERIC),
                USAGE(1, HID_USAGE_GENERIC_Z),
                LOGICAL_MINIMUM(4, 0x0000),
                LOGICAL_MAXIMUM(4, 0x7fff),
                PHYSICAL_MINIMUM(4, 0xfff90000),
                PHYSICAL_MAXIMUM(4, 0x0003ffff),
                REPORT_SIZE(1, 32),
                REPORT_COUNT(1, 1),
                FEATURE(1, Data|Var|Abs),
            END_COLLECTION,

            USAGE_PAGE(1, HID_USAGE_PAGE_GENERIC),
            USAGE(1, HID_USAGE_GENERIC_JOYSTICK),
            COLLECTION(1, Report),
                REPORT_ID_OR_USAGE_PAGE(1, report_id, 1),
                USAGE_PAGE(1, HID_USAGE_PAGE_BUTTON),
                USAGE_MINIMUM(1, 9),
                USAGE_MAXIMUM(1, 10),
                LOGICAL_MINIMUM(1, 0),
                LOGICAL_MAXIMUM(1, 1),
                PHYSICAL_MINIMUM(1, 0),
                PHYSICAL_MAXIMUM(1, 1),
                REPORT_COUNT(1, 8),
                REPORT_SIZE(1, 1),
                FEATURE(1, Data|Var|Abs),
            END_COLLECTION,

            USAGE_PAGE(1, HID_USAGE_PAGE_LED),
            USAGE(1, HID_USAGE_LED_GREEN),
            COLLECTION(1, Report),
                REPORT_ID_OR_USAGE_PAGE(1, report_id, 0),
                USAGE_PAGE(1, HID_USAGE_PAGE_LED),
                REPORT_COUNT(1, 8),
                REPORT_SIZE(1, 1),
                OUTPUT(1, Cnst|Var|Abs),
            END_COLLECTION,

            USAGE_PAGE(1, HID_USAGE_PAGE_LED),
            USAGE(1, HID_USAGE_LED_RED),
            COLLECTION(1, Report),
                REPORT_ID_OR_USAGE_PAGE(1, report_id, 1),
                USAGE_PAGE(1, HID_USAGE_PAGE_LED),
                REPORT_COUNT(1, 8),
                REPORT_SIZE(1, 1),
                OUTPUT(1, Cnst|Var|Abs),
            END_COLLECTION,
        END_COLLECTION,
    };
#undef REPORT_ID_OR_USAGE_PAGE
#include "pop_hid_macros.h"

    static const HID_DEVICE_ATTRIBUTES attributes =
    {
        .Size = sizeof(HID_DEVICE_ATTRIBUTES),
        .VendorID = 0x1209,
        .ProductID = 0x0001,
        .VersionNumber = 0x0100,
    };
    const HIDP_CAPS caps =
    {
        .Usage = HID_USAGE_GENERIC_JOYSTICK,
        .UsagePage = HID_USAGE_PAGE_GENERIC,
        .InputReportByteLength = report_id ? 25 : 26,
        .OutputReportByteLength = report_id ? 2 : 3,
        .FeatureReportByteLength = report_id ? 21 : 22,
        .NumberLinkCollectionNodes = 10,
        .NumberInputButtonCaps = 17,
        .NumberInputValueCaps = 7,
        .NumberInputDataIndices = 47,
        .NumberFeatureButtonCaps = 1,
        .NumberFeatureValueCaps = 6,
        .NumberFeatureDataIndices = 8,
    };
    const struct hid_expect expect_in =
    {
        .code = IOCTL_HID_READ_REPORT,
        .report_len = caps.InputReportByteLength - (report_id ? 0 : 1),
        .report_buf = {report_id ? report_id : 0x5a,0x5a,0x5a},
        .ret_length = 3,
        .ret_status = STATUS_SUCCESS,
    };

    WCHAR cwd[MAX_PATH], tempdir[MAX_PATH];
    LSTATUS status;
    HKEY hkey;

    GetCurrentDirectoryW( ARRAY_SIZE(cwd), cwd );
    GetTempPathW( ARRAY_SIZE(tempdir), tempdir );
    SetCurrentDirectoryW( tempdir );

    status = RegCreateKeyExW( HKEY_LOCAL_MACHINE, L"System\\CurrentControlSet\\Services\\winetest",
                              0, NULL, REG_OPTION_VOLATILE, KEY_ALL_ACCESS, NULL, &hkey, NULL );
    ok( !status, "RegCreateKeyExW returned %#x\n", status );

    status = RegSetValueExW( hkey, L"ReportID", 0, REG_DWORD, (void *)&report_id, sizeof(report_id) );
    ok( !status, "RegSetValueExW returned %#x\n", status );

    status = RegSetValueExW( hkey, L"PolledMode", 0, REG_DWORD, (void *)&polled, sizeof(polled) );
    ok( !status, "RegSetValueExW returned %#x\n", status );

    status = RegSetValueExW( hkey, L"Descriptor", 0, REG_BINARY, (void *)report_desc, sizeof(report_desc) );
    ok( !status, "RegSetValueExW returned %#x\n", status );

    status = RegSetValueExW( hkey, L"Attributes", 0, REG_BINARY, (void *)&attributes, sizeof(attributes) );
    ok( !status, "RegSetValueExW returned %#x\n", status );

    status = RegSetValueExW( hkey, L"Caps", 0, REG_BINARY, (void *)&caps, sizeof(caps) );
    ok( !status, "RegSetValueExW returned %#x\n", status );

    status = RegSetValueExW( hkey, L"Expect", 0, REG_BINARY, NULL, 0 );
    ok( !status, "RegSetValueExW returned %#x\n", status );

    status = RegSetValueExW( hkey, L"Input", 0, REG_BINARY, (void *)&expect_in, polled ? sizeof(expect_in) : 0 );
    ok( !status, "RegSetValueExW returned %#x\n", status );

    if (pnp_driver_start( L"driver_hid.dll" )) test_hid_device( report_id, polled, &caps );

    pnp_driver_stop();
    SetCurrentDirectoryW( cwd );
}

/* undocumented HID internal preparsed data structure */

struct hidp_kdr_caps
{
    USHORT usage_page;
    UCHAR report_id;
    UCHAR start_bit;
    USHORT bit_size;
    USHORT report_count;
    USHORT start_byte;
    USHORT total_bits;
    ULONG bit_field;
    USHORT end_byte;
    USHORT link_collection;
    USAGE link_usage_page;
    USAGE link_usage;
    ULONG flags;
    ULONG padding[8];
    USAGE usage_min;
    USAGE usage_max;
    USHORT string_min;
    USHORT string_max;
    USHORT designator_min;
    USHORT designator_max;
    USHORT data_index_min;
    USHORT data_index_max;
    USHORT null_value;
    USHORT unknown;
    LONG logical_min;
    LONG logical_max;
    LONG physical_min;
    LONG physical_max;
    LONG units;
    LONG units_exp;
};

/* named array continues on next caps */
#define HIDP_KDR_CAPS_ARRAY_HAS_MORE       0x01
#define HIDP_KDR_CAPS_IS_CONSTANT          0x02
#define HIDP_KDR_CAPS_IS_BUTTON            0x04
#define HIDP_KDR_CAPS_IS_ABSOLUTE          0x08
#define HIDP_KDR_CAPS_IS_RANGE             0x10
#define HIDP_KDR_CAPS_IS_STRING_RANGE      0x40
#define HIDP_KDR_CAPS_IS_DESIGNATOR_RANGE  0x80

struct hidp_kdr_node
{
    USAGE usage;
    USAGE usage_page;
    USHORT parent;
    USHORT number_of_children;
    USHORT next_sibling;
    USHORT first_child;
    ULONG collection_type;
};

struct hidp_kdr
{
    char magic[8];
    USAGE usage;
    USAGE usage_page;
    USHORT unknown[2];
    USHORT input_caps_start;
    USHORT input_caps_count;
    USHORT input_caps_end;
    USHORT input_report_byte_length;
    USHORT output_caps_start;
    USHORT output_caps_count;
    USHORT output_caps_end;
    USHORT output_report_byte_length;
    USHORT feature_caps_start;
    USHORT feature_caps_count;
    USHORT feature_caps_end;
    USHORT feature_report_byte_length;
    USHORT caps_size;
    USHORT number_link_collection_nodes;
    struct hidp_kdr_caps caps[1];
    /* struct hidp_kdr_node nodes[1] */
};

static void test_hidp_kdr(void)
{
#include "psh_hid_macros.h"
    const unsigned char report_desc[] =
    {
        USAGE_PAGE(1, HID_USAGE_PAGE_GENERIC),
        USAGE(1, HID_USAGE_GENERIC_JOYSTICK),
        COLLECTION(1, Application),
            USAGE_PAGE(1, HID_USAGE_PAGE_GENERIC),
            LOGICAL_MINIMUM(1, 1),
            LOGICAL_MAXIMUM(1, 127),
            PHYSICAL_MINIMUM(1, -128),
            PHYSICAL_MAXIMUM(1, 127),

            USAGE(1, HID_USAGE_GENERIC_RZ),
            REPORT_SIZE(1, 16),
            REPORT_COUNT(1, 0),
            FEATURE(1, Data|Var|Abs),
            USAGE(1, HID_USAGE_GENERIC_SLIDER),
            REPORT_SIZE(1, 16),
            REPORT_COUNT(1, 1),
            FEATURE(1, Data|Var|Abs),

            USAGE(1, HID_USAGE_GENERIC_X),
            REPORT_SIZE(1, 8),
            REPORT_COUNT(1, 1),
            UNIT(1, 0x100e),
            UNIT_EXPONENT(1, -3),
            INPUT(1, Data|Var|Abs),
            UNIT_EXPONENT(1, 0),
            UNIT(1, 0),
            USAGE(1, HID_USAGE_GENERIC_Y),
            DESIGNATOR_MINIMUM(1, 1),
            DESIGNATOR_MAXIMUM(1, 4),
            REPORT_SIZE(1, 8),
            REPORT_COUNT(1, 1),
            INPUT(1, Cnst|Var|Abs),
            USAGE(1, HID_USAGE_GENERIC_Z),
            REPORT_SIZE(1, 8),
            REPORT_COUNT(1, 1),
            INPUT(1, Data|Var|Rel),
            USAGE(1, HID_USAGE_GENERIC_RX),
            USAGE(1, HID_USAGE_GENERIC_RY),
            REPORT_SIZE(1, 16),
            REPORT_COUNT(1, 2),
            LOGICAL_MINIMUM(1, 7),
            INPUT(1, Data|Var|Abs|Null),

            COLLECTION(1, Application),
                USAGE(4, (HID_USAGE_PAGE_BUTTON << 16)|1),
                USAGE(4, (HID_USAGE_PAGE_BUTTON << 16)|2),
                REPORT_SIZE(1, 1),
                REPORT_COUNT(1, 8),
                LOGICAL_MINIMUM(1, 0),
                LOGICAL_MAXIMUM(1, 1),
                INPUT(1, Data|Var|Abs),

                USAGE_MINIMUM(4, (HID_USAGE_PAGE_BUTTON << 16)|3),
                USAGE_MAXIMUM(4, (HID_USAGE_PAGE_BUTTON << 16)|8),
                REPORT_SIZE(1, 8),
                REPORT_COUNT(1, 1),
                LOGICAL_MINIMUM(1, 3),
                LOGICAL_MAXIMUM(1, 8),
                INPUT(1, Data|Ary|Abs),

                USAGE_MINIMUM(4, (HID_USAGE_PAGE_BUTTON << 16)|9),
                USAGE_MAXIMUM(4, (HID_USAGE_PAGE_BUTTON << 16)|12),
                REPORT_SIZE(1, 8),
                REPORT_COUNT(1, 4),
                LOGICAL_MINIMUM(1, 9),
                LOGICAL_MAXIMUM(1, 12),
                INPUT(2, Data|Ary|Abs|Buff),

                USAGE(4, (HID_USAGE_PAGE_BUTTON << 16)|13),
                USAGE(4, (HID_USAGE_PAGE_BUTTON << 16)|14),
                USAGE(4, (HID_USAGE_PAGE_BUTTON << 16)|15),
                USAGE(4, (HID_USAGE_PAGE_BUTTON << 16)|16),
                REPORT_SIZE(1, 8),
                REPORT_COUNT(1, 1),
                LOGICAL_MINIMUM(1, 13),
                LOGICAL_MAXIMUM(1, 16),
                INPUT(1, Data|Ary|Abs),
            END_COLLECTION,
        END_COLLECTION,
    };
#include "pop_hid_macros.h"

    static const HIDP_CAPS expect_hidp_caps =
    {
        .Usage = HID_USAGE_GENERIC_JOYSTICK,
        .UsagePage = HID_USAGE_PAGE_GENERIC,
        .InputReportByteLength = 15,
    };
    static const HID_DEVICE_ATTRIBUTES attributes =
    {
        .Size = sizeof(HID_DEVICE_ATTRIBUTES),
        .VendorID = 0x1209,
        .ProductID = 0x0001,
        .VersionNumber = 0x0100,
    };
    static const struct hidp_kdr expect_kdr =
    {
        .magic = "HidP KDR",
        .usage = 0x04,
        .usage_page = 0x01,
        .input_caps_count = 13,
        .input_caps_end = 13,
        .input_report_byte_length = 15,
        .output_caps_start = 13,
        .output_caps_end = 13,
        .feature_caps_start = 13,
        .feature_caps_count = 2,
        .feature_caps_end = 14,
        .feature_report_byte_length = 3,
        .caps_size = 1560,
        .number_link_collection_nodes = 2,
    };
    static const struct hidp_kdr_caps expect_caps[] =
    {
        {
            .usage_page = 0x01, .bit_size = 0x08, .report_count = 0x1, .start_byte = 0x1, .total_bits = 0x08,
            .bit_field = 0x002, .end_byte = 0x2, .link_usage_page = 0x01, .link_usage = 0x04, .flags = 0x08,
            .usage_min = 0x30, .usage_max = 0x30, .logical_min = 1, .logical_max = 127, .physical_min = -128,
            .physical_max = 127, .units = 0xe, .units_exp = -3
        },
        {
            .usage_page = 0x01, .bit_size = 0x08, .report_count = 0x1, .start_byte = 0x2, .total_bits = 0x08,
            .bit_field = 0x003, .end_byte = 0x3, .link_usage_page = 0x01, .link_usage = 0x04, .flags = 0x8a,
            .usage_min = 0x31, .usage_max = 0x31, .designator_min = 1, .designator_max = 4, .data_index_min = 0x01,
            .data_index_max = 0x01, .logical_min = 1, .logical_max = 127, .physical_min = -128, .physical_max = 127
        },
        {
            .usage_page = 0x01, .bit_size = 0x08, .report_count = 0x1, .start_byte = 0x3, .total_bits = 0x08,
            .bit_field = 0x006, .end_byte = 0x4, .link_usage_page = 0x01, .link_usage = 0x04, .usage_min = 0x32,
            .usage_max = 0x32, .data_index_min = 0x02, .data_index_max = 0x02, .logical_min = 1, .logical_max = 127,
            .physical_min = -128, .physical_max = 127
        },
        {
            .usage_page = 0x01, .bit_size = 0x10, .report_count = 0x1, .start_byte = 0x6, .total_bits = 0x10,
            .bit_field = 0x042, .end_byte = 0x8, .link_usage_page = 0x01, .link_usage = 0x04, .flags = 0x08,
            .usage_min = 0x34, .usage_max = 0x34, .data_index_min = 0x03, .data_index_max = 0x03, .null_value = 1,
            .logical_min = 7, .logical_max = 127, .physical_min = -128, .physical_max = 127
        },
        {
            .usage_page = 0x01, .bit_size = 0x10, .report_count = 0x1, .start_byte = 0x4, .total_bits = 0x10,
            .bit_field = 0x042, .end_byte = 0x6, .link_usage_page = 0x01, .link_usage = 0x04, .flags = 0x08,
            .usage_min = 0x33, .usage_max = 0x33, .data_index_min = 0x04, .data_index_max = 0x04, .null_value = 1,
            .logical_min = 7, .logical_max = 127, .physical_min = -128, .physical_max = 127
        },
        {
            .usage_page = 0x09, .start_bit = 1, .bit_size = 0x01, .report_count = 0x7, .start_byte = 0x8, .total_bits = 0x07,
            .bit_field = 0x002, .end_byte = 0x9, .link_collection = 1, .link_usage_page = 0x01, .flags = 0x0c,
            .usage_min = 0x02, .usage_max = 0x02, .data_index_min = 0x05, .data_index_max = 0x05,
        },
        {
            .usage_page = 0x09, .bit_size = 0x01, .report_count = 0x1, .start_byte = 0x8, .total_bits = 0x01,
            .bit_field = 0x002, .end_byte = 0x9, .link_collection = 1, .link_usage_page = 0x01, .flags = 0x0c,
            .usage_min = 0x01, .usage_max = 0x01, .data_index_min = 0x06, .data_index_max = 0x06,
        },
        {
            .usage_page = 0x09, .bit_size = 0x08, .report_count = 0x1, .start_byte = 0x9, .total_bits = 0x08,
            .bit_field = 0x000, .end_byte = 0xa, .link_collection = 1, .link_usage_page = 0x01, .flags = 0x1c,
            .usage_min = 0x03, .usage_max = 0x08, .data_index_min = 0x07, .data_index_max = 0x0c, .null_value = 3,
            .logical_min = 8
        },
        {
            .usage_page = 0x09, .bit_size = 0x08, .report_count = 0x4, .start_byte = 0xa, .total_bits = 0x20,
            .bit_field = 0x100, .end_byte = 0xe, .link_collection = 1, .link_usage_page = 0x01, .flags = 0x1c,
            .usage_min = 0x09, .usage_max = 0x0c, .data_index_min = 0x0d, .data_index_max = 0x10, .null_value = 9,
            .logical_min = 12
        },
        {
            .usage_page = 0x09, .bit_size = 0x08, .report_count = 0x1, .start_byte = 0xe, .total_bits = 0x08,
            .bit_field = 0x000, .end_byte = 0xf, .link_collection = 1, .link_usage_page = 0x01, .flags = 0x0d,
            .usage_min = 0x10, .usage_max = 0x10, .data_index_min = 0x14, .data_index_max = 0x14, .null_value = 13,
            .logical_min = 16
        },
        {
            .usage_page = 0x09, .bit_size = 0x08, .report_count = 0x1, .start_byte = 0xe, .total_bits = 0x08,
            .bit_field = 0x000, .end_byte = 0xf, .link_collection = 1, .link_usage_page = 0x01, .flags = 0x0d,
            .usage_min = 0x0f, .usage_max = 0x0f, .data_index_min = 0x13, .data_index_max = 0x13, .null_value = 13,
            .logical_min = 16
        },
        {
            .usage_page = 0x09, .bit_size = 0x08, .report_count = 0x1, .start_byte = 0xe, .total_bits = 0x08,
            .bit_field = 0x000, .end_byte = 0xf, .link_collection = 1, .link_usage_page = 0x01, .flags = 0x0d,
            .usage_min = 0x0e, .usage_max = 0x0e, .data_index_min = 0x12, .data_index_max = 0x12, .null_value = 13,
            .logical_min = 16
        },
        {
            .usage_page = 0x09, .bit_size = 0x08, .report_count = 0x1, .start_byte = 0xe, .total_bits = 0x08,
            .bit_field = 0x000, .end_byte = 0xf, .link_collection = 1, .link_usage_page = 0x01, .flags = 0x0c,
            .usage_min = 0x0d, .usage_max = 0x0d, .data_index_min = 0x11, .data_index_max = 0x11, .null_value = 13,
            .logical_min = 16
        },
        {
            .usage_page = 0x01, .bit_size = 0x10, .report_count = 0x1, .start_byte = 0x1, .total_bits = 0x10,
            .bit_field = 0x002, .end_byte = 0x3, .link_usage_page = 0x01, .link_usage = 0x04, .flags = 0x08,
            .usage_min = 0x36, .usage_max = 0x36, .logical_min = 1, .logical_max = 127, .physical_min = -128,
            .physical_max = 127
        },
        {
        },
    };
    static const struct hidp_kdr_node expect_nodes[] =
    {
        {
            .usage = 0x04,
            .usage_page = 0x01,
            .parent = 0,
            .number_of_children = 0x1,
            .next_sibling = 0,
            .first_child = 0x1,
            .collection_type = 0x1,
        },
        {
            .usage = 0x00,
            .usage_page = 0x01,
            .parent = 0,
            .number_of_children = 0,
            .next_sibling = 0,
            .first_child = 0,
            .collection_type = 0x1,
        },
    };

    char buffer[FIELD_OFFSET( SP_DEVICE_INTERFACE_DETAIL_DATA_W, DevicePath[MAX_PATH] )];
    SP_DEVICE_INTERFACE_DATA iface = {sizeof(SP_DEVICE_INTERFACE_DATA)};
    SP_DEVICE_INTERFACE_DETAIL_DATA_W *iface_detail = (void *)buffer;
    SP_DEVINFO_DATA device = {sizeof(SP_DEVINFO_DATA)};
    WCHAR cwd[MAX_PATH], tempdir[MAX_PATH];
    PHIDP_PREPARSED_DATA preparsed_data;
    DWORD i, report_id = 0, polled = 0;
    struct hidp_kdr *kdr;
    LSTATUS status;
    HDEVINFO set;
    HANDLE file;
    HKEY hkey;
    BOOL ret;

    GetCurrentDirectoryW( ARRAY_SIZE(cwd), cwd );
    GetTempPathW( ARRAY_SIZE(tempdir), tempdir );
    SetCurrentDirectoryW( tempdir );

    status = RegCreateKeyExW( HKEY_LOCAL_MACHINE, L"System\\CurrentControlSet\\Services\\winetest",
                              0, NULL, REG_OPTION_VOLATILE, KEY_ALL_ACCESS, NULL, &hkey, NULL );
    ok( !status, "RegCreateKeyExW returned %#x\n", status );

    status = RegSetValueExW( hkey, L"ReportID", 0, REG_DWORD, (void *)&report_id, sizeof(report_id) );
    ok( !status, "RegSetValueExW returned %#x\n", status );

    status = RegSetValueExW( hkey, L"PolledMode", 0, REG_DWORD, (void *)&polled, sizeof(polled) );
    ok( !status, "RegSetValueExW returned %#x\n", status );

    status = RegSetValueExW( hkey, L"Descriptor", 0, REG_BINARY, (void *)report_desc, sizeof(report_desc) );
    ok( !status, "RegSetValueExW returned %#x\n", status );

    status = RegSetValueExW( hkey, L"Attributes", 0, REG_BINARY, (void *)&attributes, sizeof(attributes) );
    ok( !status, "RegSetValueExW returned %#x\n", status );

    status = RegSetValueExW( hkey, L"Caps", 0, REG_BINARY, (void *)&expect_hidp_caps, sizeof(expect_hidp_caps) );
    ok( !status, "RegSetValueExW returned %#x\n", status );

    status = RegSetValueExW( hkey, L"Expect", 0, REG_BINARY, NULL, 0 );
    ok( !status, "RegSetValueExW returned %#x\n", status );

    status = RegSetValueExW( hkey, L"Input", 0, REG_BINARY, NULL, 0 );
    ok( !status, "RegSetValueExW returned %#x\n", status );

    if (!pnp_driver_start( L"driver_hid.dll" )) goto done;

    set = SetupDiGetClassDevsW( &GUID_DEVINTERFACE_HID, NULL, NULL, DIGCF_DEVICEINTERFACE | DIGCF_PRESENT );
    ok( set != INVALID_HANDLE_VALUE, "failed to get device list, error %#x\n", GetLastError() );
    for (i = 0; SetupDiEnumDeviceInfo( set, i, &device ); ++i)
    {
        ret = SetupDiEnumDeviceInterfaces( set, &device, &GUID_DEVINTERFACE_HID, 0, &iface );
        ok( ret, "failed to get interface, error %#x\n", GetLastError() );
        ok( IsEqualGUID( &iface.InterfaceClassGuid, &GUID_DEVINTERFACE_HID ), "wrong class %s\n",
            debugstr_guid( &iface.InterfaceClassGuid ) );
        ok( iface.Flags == SPINT_ACTIVE, "got flags %#x\n", iface.Flags );

        iface_detail->cbSize = sizeof(*iface_detail);
        ret = SetupDiGetDeviceInterfaceDetailW( set, &iface, iface_detail, sizeof(buffer), NULL, NULL );
        ok( ret, "failed to get interface path, error %#x\n", GetLastError() );

        if (wcsstr( iface_detail->DevicePath, L"\\\\?\\hid#winetest#1" )) break;
    }
    SetupDiDestroyDeviceInfoList( set );

    file = CreateFileW( iface_detail->DevicePath, FILE_READ_ACCESS | FILE_WRITE_ACCESS,
                        FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL );
    ok( file != INVALID_HANDLE_VALUE, "got error %u\n", GetLastError() );

    ret = HidD_GetPreparsedData( file, &preparsed_data );
    ok( ret, "HidD_GetPreparsedData failed with error %u\n", GetLastError() );

    kdr = (struct hidp_kdr *)preparsed_data;
    ok( !strncmp( kdr->magic, expect_kdr.magic, 8 ), "got %s expected %s\n",
        debugstr_an(kdr->magic, 8), debugstr_an(expect_kdr.magic, 8) );

    if (!strncmp( kdr->magic, expect_kdr.magic, 8 ))
    {
        check_member( *kdr, expect_kdr, "%04x", usage );
        check_member( *kdr, expect_kdr, "%04x", usage_page );
        check_member( *kdr, expect_kdr, "%#x", unknown[0] );
        check_member( *kdr, expect_kdr, "%#x", unknown[1] );
        check_member( *kdr, expect_kdr, "%d", input_caps_start );
        check_member( *kdr, expect_kdr, "%d", input_caps_count );
        check_member( *kdr, expect_kdr, "%d", input_caps_end );
        check_member( *kdr, expect_kdr, "%d", input_report_byte_length );
        check_member( *kdr, expect_kdr, "%d", output_caps_start );
        check_member( *kdr, expect_kdr, "%d", output_caps_count );
        check_member( *kdr, expect_kdr, "%d", output_caps_end );
        check_member( *kdr, expect_kdr, "%d", output_report_byte_length );
        check_member( *kdr, expect_kdr, "%d", feature_caps_start );
        todo_wine
        check_member( *kdr, expect_kdr, "%d", feature_caps_count );
        check_member( *kdr, expect_kdr, "%d", feature_caps_end );
        check_member( *kdr, expect_kdr, "%d", feature_report_byte_length );
        todo_wine
        check_member( *kdr, expect_kdr, "%d", caps_size );
        check_member( *kdr, expect_kdr, "%d", number_link_collection_nodes );

        for (i = 0; i < min( ARRAY_SIZE(expect_caps), kdr->caps_size / sizeof(struct hidp_kdr_caps) ); ++i)
        {
            winetest_push_context( "caps[%d]", i );
            check_member( kdr->caps[i], expect_caps[i], "%04x", usage_page );
            check_member( kdr->caps[i], expect_caps[i], "%d", report_id );
            check_member( kdr->caps[i], expect_caps[i], "%d", start_bit );
            check_member( kdr->caps[i], expect_caps[i], "%d", bit_size );
            check_member( kdr->caps[i], expect_caps[i], "%d", report_count );
            check_member( kdr->caps[i], expect_caps[i], "%d", start_byte );
            check_member( kdr->caps[i], expect_caps[i], "%d", total_bits );
            check_member( kdr->caps[i], expect_caps[i], "%#x", bit_field );
            check_member( kdr->caps[i], expect_caps[i], "%d", end_byte );
            check_member( kdr->caps[i], expect_caps[i], "%d", link_collection );
            check_member( kdr->caps[i], expect_caps[i], "%04x", link_usage_page );
            check_member( kdr->caps[i], expect_caps[i], "%04x", link_usage );
            check_member( kdr->caps[i], expect_caps[i], "%#x", flags );
            check_member( kdr->caps[i], expect_caps[i], "%#x", padding[0] );
            check_member( kdr->caps[i], expect_caps[i], "%#x", padding[1] );
            check_member( kdr->caps[i], expect_caps[i], "%#x", padding[2] );
            check_member( kdr->caps[i], expect_caps[i], "%#x", padding[3] );
            check_member( kdr->caps[i], expect_caps[i], "%#x", padding[4] );
            check_member( kdr->caps[i], expect_caps[i], "%#x", padding[5] );
            check_member( kdr->caps[i], expect_caps[i], "%#x", padding[6] );
            check_member( kdr->caps[i], expect_caps[i], "%#x", padding[7] );
            check_member( kdr->caps[i], expect_caps[i], "%04x", usage_min );
            check_member( kdr->caps[i], expect_caps[i], "%04x", usage_max );
            check_member( kdr->caps[i], expect_caps[i], "%d", string_min );
            check_member( kdr->caps[i], expect_caps[i], "%d", string_max );
            check_member( kdr->caps[i], expect_caps[i], "%d", designator_min );
            check_member( kdr->caps[i], expect_caps[i], "%d", designator_max );
            check_member( kdr->caps[i], expect_caps[i], "%#x", data_index_min );
            check_member( kdr->caps[i], expect_caps[i], "%#x", data_index_max );
            check_member( kdr->caps[i], expect_caps[i], "%d", null_value );
            check_member( kdr->caps[i], expect_caps[i], "%d", unknown );
            check_member( kdr->caps[i], expect_caps[i], "%d", logical_min );
            check_member( kdr->caps[i], expect_caps[i], "%d", logical_max );
            check_member( kdr->caps[i], expect_caps[i], "%d", physical_min );
            check_member( kdr->caps[i], expect_caps[i], "%d", physical_max );
            check_member( kdr->caps[i], expect_caps[i], "%#x", units );
            check_member( kdr->caps[i], expect_caps[i], "%#x", units_exp );
            winetest_pop_context();
        }

        for (i = 0; i < ARRAY_SIZE(expect_nodes); ++i)
        {
            struct hidp_kdr_node *nodes = (struct hidp_kdr_node *)((char *)kdr->caps + kdr->caps_size);
            winetest_push_context( "nodes[%d]", i );
            check_member( nodes[i], expect_nodes[i], "%04x", usage );
            check_member( nodes[i], expect_nodes[i], "%04x", usage_page );
            check_member( nodes[i], expect_nodes[i], "%d", parent );
            check_member( nodes[i], expect_nodes[i], "%d", number_of_children );
            check_member( nodes[i], expect_nodes[i], "%d", next_sibling );
            check_member( nodes[i], expect_nodes[i], "%d", first_child );
            check_member( nodes[i], expect_nodes[i], "%#x", collection_type );
            winetest_pop_context();
        }
    }

    HidD_FreePreparsedData( preparsed_data );

    CloseHandle( file );

done:
    pnp_driver_stop();
    SetCurrentDirectoryW( cwd );
}

static void cleanup_registry_keys(void)
{
    static const WCHAR joystick_oem_path[] = L"System\\CurrentControlSet\\Control\\MediaProperties\\"
                                              "PrivateProperties\\Joystick\\OEM";
    static const WCHAR dinput_path[] = L"System\\CurrentControlSet\\Control\\MediaProperties\\"
                                       "PrivateProperties\\DirectInput";
    HKEY root_key;

    /* These keys are automatically created by DInput and they store the
       list of supported force-feedback effects. OEM drivers are supposed
       to provide a list in HKLM for the vendor-specific force-feedback
       support.

       We need to clean them up, or DInput will not refresh the list of
       effects from the PID report changes.
    */
    RegCreateKeyExW( HKEY_CURRENT_USER, joystick_oem_path, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &root_key, NULL );
    RegDeleteTreeW( root_key, expect_vidpid_str );
    RegCloseKey( root_key );

    RegCreateKeyExW( HKEY_CURRENT_USER, dinput_path, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &root_key, NULL );
    RegDeleteTreeW( root_key, expect_vidpid_str );
    RegCloseKey( root_key );

    RegCreateKeyExW( HKEY_LOCAL_MACHINE, joystick_oem_path, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &root_key, NULL );
    RegDeleteTreeW( root_key, expect_vidpid_str );
    RegCloseKey( root_key );

    RegCreateKeyExW( HKEY_LOCAL_MACHINE, dinput_path, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &root_key, NULL );
    RegDeleteTreeW( root_key, expect_vidpid_str );
    RegCloseKey( root_key );
}

static BOOL dinput_driver_start( const BYTE *desc_buf, ULONG desc_len, const HIDP_CAPS *caps )
{
    static const HID_DEVICE_ATTRIBUTES attributes =
    {
        .Size = sizeof(HID_DEVICE_ATTRIBUTES),
        .VendorID = LOWORD( EXPECT_VIDPID ),
        .ProductID = HIWORD( EXPECT_VIDPID ),
        .VersionNumber = 0x0100,
    };
    DWORD report_id = 1;
    DWORD polled = 0;
    LSTATUS status;
    HKEY hkey;

    status = RegCreateKeyExW( HKEY_LOCAL_MACHINE, L"System\\CurrentControlSet\\Services\\winetest",
                              0, NULL, REG_OPTION_VOLATILE, KEY_ALL_ACCESS, NULL, &hkey, NULL );
    ok( !status, "RegCreateKeyExW returned %#x\n", status );
    status = RegSetValueExW( hkey, L"ReportID", 0, REG_DWORD, (void *)&report_id, sizeof(report_id) );
    ok( !status, "RegSetValueExW returned %#x\n", status );
    status = RegSetValueExW( hkey, L"PolledMode", 0, REG_DWORD, (void *)&polled, sizeof(polled) );
    ok( !status, "RegSetValueExW returned %#x\n", status );
    status = RegSetValueExW( hkey, L"Descriptor", 0, REG_BINARY, (void *)desc_buf, desc_len );
    ok( !status, "RegSetValueExW returned %#x\n", status );
    status = RegSetValueExW( hkey, L"Attributes", 0, REG_BINARY, (void *)&attributes, sizeof(attributes) );
    ok( !status, "RegSetValueExW returned %#x\n", status );
    status = RegSetValueExW( hkey, L"Caps", 0, REG_BINARY, (void *)caps, sizeof(*caps) );
    ok( !status, "RegSetValueExW returned %#x\n", status );
    status = RegSetValueExW( hkey, L"Expect", 0, REG_BINARY, NULL, 0 );
    ok( !status, "RegSetValueExW returned %#x\n", status );
    status = RegSetValueExW( hkey, L"Input", 0, REG_BINARY, NULL, 0 );
    ok( !status, "RegSetValueExW returned %#x\n", status );

    return pnp_driver_start( L"driver_hid.dll" );
}

static BOOL CALLBACK find_test_device( const DIDEVICEINSTANCEW *devinst, void *context )
{
    if (IsEqualGUID( &devinst->guidProduct, &expect_guid_product ))
        *(DIDEVICEINSTANCEW *)context = *devinst;
    return DIENUM_CONTINUE;
}

struct check_objects_todos
{
    BOOL ofs;
    BOOL type;
    BOOL collection_number;
};

struct check_objects_params
{
    UINT index;
    UINT expect_count;
    const DIDEVICEOBJECTINSTANCEW *expect_objs;
    const struct check_objects_todos *todo_objs;
};

static BOOL CALLBACK check_objects( const DIDEVICEOBJECTINSTANCEW *obj, void *args )
{
    static const DIDEVICEOBJECTINSTANCEW unexpected_obj = {0};
    struct check_objects_params *params = args;
    const DIDEVICEOBJECTINSTANCEW *exp = params->expect_objs + params->index;
    const struct check_objects_todos *todo = params->todo_objs + params->index;

    winetest_push_context( "obj[%d]", params->index );

    ok( params->index < params->expect_count, "unexpected extra object\n" );
    if (params->index >= params->expect_count) exp = &unexpected_obj;

    check_member( *obj, *exp, "%u", dwSize );
    check_member_guid( *obj, *exp, guidType );
    todo_wine_if( todo->ofs )
    check_member( *obj, *exp, "%#x", dwOfs );
    todo_wine_if( todo->type )
    check_member( *obj, *exp, "%#x", dwType );
    check_member( *obj, *exp, "%#x", dwFlags );
    if (!localized) todo_wine check_member_wstr( *obj, *exp, tszName );
    check_member( *obj, *exp, "%u", dwFFMaxForce );
    check_member( *obj, *exp, "%u", dwFFForceResolution );
    todo_wine_if( todo->collection_number )
    check_member( *obj, *exp, "%u", wCollectionNumber );
    check_member( *obj, *exp, "%u", wDesignatorIndex );
    check_member( *obj, *exp, "%#04x", wUsagePage );
    check_member( *obj, *exp, "%#04x", wUsage );
    check_member( *obj, *exp, "%#04x", dwDimension );
    check_member( *obj, *exp, "%#04x", wExponent );
    check_member( *obj, *exp, "%u", wReportId );

    winetest_pop_context();

    params->index++;
    return DIENUM_CONTINUE;
}

static BOOL CALLBACK check_object_count( const DIDEVICEOBJECTINSTANCEW *obj, void *args )
{
    DWORD *count = args;
    *count = *count + 1;
    return DIENUM_CONTINUE;
}

static void test_simple_joystick(void)
{
#include "psh_hid_macros.h"
    static const unsigned char report_desc[] =
    {
        USAGE_PAGE(1, HID_USAGE_PAGE_GENERIC),
        USAGE(1, HID_USAGE_GENERIC_JOYSTICK),
        COLLECTION(1, Application),
            USAGE(1, HID_USAGE_GENERIC_JOYSTICK),
            COLLECTION(1, Report),
                REPORT_ID(1, 1),

                USAGE(1, HID_USAGE_GENERIC_WHEEL),
                USAGE(4, (0xff01u<<16)|(0x1234)),
                USAGE(1, HID_USAGE_GENERIC_X),
                USAGE(1, HID_USAGE_GENERIC_Y),
                LOGICAL_MINIMUM(1, 0xe7),
                LOGICAL_MAXIMUM(1, 0x38),
                PHYSICAL_MINIMUM(1, 0xe7),
                PHYSICAL_MAXIMUM(1, 0x38),
                REPORT_SIZE(1, 8),
                REPORT_COUNT(1, 4),
                INPUT(1, Data|Var|Abs),

                USAGE(1, HID_USAGE_GENERIC_HATSWITCH),
                LOGICAL_MINIMUM(1, 1),
                LOGICAL_MAXIMUM(1, 8),
                PHYSICAL_MINIMUM(1, 0),
                PHYSICAL_MAXIMUM(1, 8),
                REPORT_SIZE(1, 4),
                REPORT_COUNT(1, 1),
                INPUT(1, Data|Var|Abs|Null),

                USAGE_PAGE(1, HID_USAGE_PAGE_BUTTON),
                USAGE_MINIMUM(1, 1),
                USAGE_MAXIMUM(1, 2),
                LOGICAL_MINIMUM(1, 0),
                LOGICAL_MAXIMUM(1, 1),
                PHYSICAL_MINIMUM(1, 0),
                PHYSICAL_MAXIMUM(1, 1),
                REPORT_SIZE(1, 1),
                REPORT_COUNT(1, 4),
                INPUT(1, Data|Var|Abs),
            END_COLLECTION,
        END_COLLECTION,
    };
#undef REPORT_ID_OR_USAGE_PAGE
#include "pop_hid_macros.h"

    static const HIDP_CAPS hid_caps =
    {
        .InputReportByteLength = 6,
    };
    static const DIDEVCAPS expect_caps =
    {
        .dwSize = sizeof(DIDEVCAPS),
        .dwFlags = DIDC_ATTACHED | DIDC_EMULATED,
        .dwDevType = DIDEVTYPE_HID | (DI8DEVTYPEJOYSTICK_LIMITED << 8) | DI8DEVTYPE_JOYSTICK,
        .dwAxes = 3,
        .dwPOVs = 1,
        .dwButtons = 2,
    };
    struct hid_expect injected_input[] =
    {
        {
            .code = IOCTL_HID_READ_REPORT,
            .report_buf = {1,0x10,0x10,0x10,0x10,0},
        },
        {
            .code = IOCTL_HID_READ_REPORT,
            .report_buf = {1,0x10,0x10,0x38,0x38,0xf8},
        },
        {
            .code = IOCTL_HID_READ_REPORT,
            .report_buf = {1,0x10,0x10,0x01,0x01,0x00},
        },
        {
            .code = IOCTL_HID_READ_REPORT,
            .report_buf = {1,0x10,0x10,0x01,0x01,0x00},
        },
        {
            .code = IOCTL_HID_READ_REPORT,
            .report_buf = {1,0x10,0x10,0x80,0x80,0xff},
        },
        {
            .code = IOCTL_HID_READ_REPORT,
            .report_buf = {1,0x10,0x10,0x10,0xee,0x54},
        },
    };
    static const struct DIJOYSTATE2 expect_state[] =
    {
        {.lX = 32767, .lY = 32767, .lZ = 32767, .rgdwPOV = {-1, -1, -1, -1}, .rgbButtons = {0, 0}},
        {.lX = 32767, .lY = 32767, .lZ = 32767, .rgdwPOV = {-1, -1, -1, -1}, .rgbButtons = {0, 0}},
        {.lX = 65535, .lY = 65535, .lZ = 32767, .rgdwPOV = {31500, -1, -1, -1}, .rgbButtons = {0x80, 0x80}},
        {.lX = 20779, .lY = 20779, .lZ = 32767, .rgdwPOV = {-1, -1, -1, -1}, .rgbButtons = {0, 0}},
        {.lX = 20779, .lY = 20779, .lZ = 32767, .rgdwPOV = {-1, -1, -1, -1}, .rgbButtons = {0, 0}},
        {.lX = 0, .lY = 0, .lZ = 32767, .rgdwPOV = {-1, -1, -1, -1}, .rgbButtons = {0x80, 0x80}},
        {.lX = 32767, .lY = 5594, .lZ = 32767, .rgdwPOV = {13500, -1, -1, -1}, .rgbButtons = {0x80}},
    };
    static const struct DIJOYSTATE2 expect_state_abs[] =
    {
        {.lX = -9000, .lY = 26000, .lZ = 26000, .rgdwPOV = {-1, -1, -1, -1}, .rgbButtons = {0, 0}},
        {.lX = -9000, .lY = 26000, .lZ = 26000, .rgdwPOV = {-1, -1, -1, -1}, .rgbButtons = {0, 0}},
        {.lX = -4000, .lY = 51000, .lZ = 26000, .rgdwPOV = {31500, -1, -1, -1}, .rgbButtons = {0x80, 0x80}},
        {.lX = -10667, .lY = 12905, .lZ = 26000, .rgdwPOV = {-1, -1, -1, -1}, .rgbButtons = {0, 0}},
        {.lX = -10667, .lY = 12905, .lZ = 26000, .rgdwPOV = {-1, -1, -1, -1}, .rgbButtons = {0, 0}},
        {.lX = -14000, .lY = 1000, .lZ = 26000, .rgdwPOV = {-1, -1, -1, -1}, .rgbButtons = {0x80, 0x80}},
        {.lX = -9000, .lY = 1000, .lZ = 26000, .rgdwPOV = {13500, -1, -1, -1}, .rgbButtons = {0x80}},
    };
    static const struct DIJOYSTATE2 expect_state_rel[] =
    {
        {.lX = 0, .lY = 0, .rgdwPOV = {13500, -1, -1, -1}, .rgbButtons = {0x80, 0}},
        {.lX = 9016, .lY = -984, .lZ = -25984, .rgdwPOV = {-1, -1, -1, -1}, .rgbButtons = {0, 0}},
        {.lX = 40, .lY = 40, .rgdwPOV = {31500, -1, -1, -1}, .rgbButtons = {0x80, 0x80}},
        {.lX = -55, .lY = -55, .rgdwPOV = {-1, -1, -1, -1}, .rgbButtons = {0, 0}},
        {.lX = 0, .lY = 0, .rgdwPOV = {-1, -1, -1, -1}, .rgbButtons = {0, 0}},
        {.lX = -129, .lY = -129, .rgdwPOV = {-1, -1, -1, -1}, .rgbButtons = {0x80, 0x80}},
        {.lX = 144, .lY = 110, .rgdwPOV = {13500, -1, -1, -1}, .rgbButtons = {0x80}},
    };
    static const DIDEVICEOBJECTDATA expect_objdata[] =
    {
        {.dwOfs = 0x4, .dwData = 0xffff, .dwSequence = 0xa},
        {.dwOfs = 0x4, .dwData = 0xffff, .dwSequence = 0xa},
        {.dwOfs = 0, .dwData = 0xffff, .dwSequence = 0xa},
        {.dwOfs = 0x20, .dwData = 31500, .dwSequence = 0xa},
        {.dwOfs = 0x30, .dwData = 0x80, .dwSequence = 0xa},
        {.dwOfs = 0x4, .dwData = 0x512b, .dwSequence = 0xd},
        {.dwOfs = 0, .dwData = 0x512b, .dwSequence = 0xd},
        {.dwOfs = 0x20, .dwData = -1, .dwSequence = 0xd},
        {.dwOfs = 0x30, .dwData = 0, .dwSequence = 0xd},
        {.dwOfs = 0x31, .dwData = 0, .dwSequence = 0xd},
        {.dwOfs = 0x4, .dwData = 0, .dwSequence = 0xf},
        {.dwOfs = 0, .dwData = 0, .dwSequence = 0xf},
        {.dwOfs = 0x30, .dwData = 0x80, .dwSequence = 0xf},
        {.dwOfs = 0x31, .dwData = 0x80, .dwSequence = 0xf},
    };

    const DIDEVICEINSTANCEW expect_devinst =
    {
        .dwSize = sizeof(DIDEVICEINSTANCEW),
        .guidInstance = expect_guid_product,
        .guidProduct = expect_guid_product,
        .dwDevType = DIDEVTYPE_HID | (DI8DEVTYPEJOYSTICK_LIMITED << 8) | DI8DEVTYPE_JOYSTICK,
        .tszInstanceName = L"Wine test root driver",
        .tszProductName = L"Wine test root driver",
        .guidFFDriver = GUID_NULL,
        .wUsagePage = HID_USAGE_PAGE_GENERIC,
        .wUsage = HID_USAGE_GENERIC_JOYSTICK,
    };
    const DIDEVICEOBJECTINSTANCEW expect_objects[] =
    {
        {
            .dwSize = sizeof(DIDEVICEOBJECTINSTANCEW),
            .guidType = GUID_YAxis,
            .dwType = DIDFT_ABSAXIS|DIDFT_MAKEINSTANCE(1),
            .dwFlags = DIDOI_ASPECTPOSITION,
            .tszName = L"Y Axis",
            .wCollectionNumber = 1,
            .wUsagePage = HID_USAGE_PAGE_GENERIC,
            .wUsage = HID_USAGE_GENERIC_Y,
            .wReportId = 1,
        },
        {
            .dwSize = sizeof(DIDEVICEOBJECTINSTANCEW),
            .guidType = GUID_XAxis,
            .dwOfs = 0x4,
            .dwType = DIDFT_ABSAXIS|DIDFT_MAKEINSTANCE(0),
            .dwFlags = DIDOI_ASPECTPOSITION,
            .tszName = L"X Axis",
            .wCollectionNumber = 1,
            .wUsagePage = HID_USAGE_PAGE_GENERIC,
            .wUsage = HID_USAGE_GENERIC_X,
            .wReportId = 1,
        },
        {
            .dwSize = sizeof(DIDEVICEOBJECTINSTANCEW),
            .guidType = GUID_ZAxis,
            .dwOfs = 0xc,
            .dwType = DIDFT_ABSAXIS|DIDFT_MAKEINSTANCE(2),
            .dwFlags = DIDOI_ASPECTPOSITION,
            .tszName = L"Wheel",
            .wCollectionNumber = 1,
            .wUsagePage = HID_USAGE_PAGE_GENERIC,
            .wUsage = HID_USAGE_GENERIC_WHEEL,
            .wReportId = 1,
        },
        {
            .dwSize = sizeof(DIDEVICEOBJECTINSTANCEW),
            .guidType = GUID_POV,
            .dwOfs = 0x10,
            .dwType = DIDFT_POV|DIDFT_MAKEINSTANCE(0),
            .tszName = L"Hat Switch",
            .wCollectionNumber = 1,
            .wUsagePage = HID_USAGE_PAGE_GENERIC,
            .wUsage = HID_USAGE_GENERIC_HATSWITCH,
            .wReportId = 1,
        },
        {
            .dwSize = sizeof(DIDEVICEOBJECTINSTANCEW),
            .guidType = GUID_Button,
            .dwOfs = 0x14,
            .dwType = DIDFT_PSHBUTTON|DIDFT_MAKEINSTANCE(0),
            .tszName = L"Button 0",
            .wCollectionNumber = 1,
            .wUsagePage = HID_USAGE_PAGE_BUTTON,
            .wUsage = 0x1,
            .wReportId = 1,
        },
        {
            .dwSize = sizeof(DIDEVICEOBJECTINSTANCEW),
            .guidType = GUID_Button,
            .dwOfs = 0x15,
            .dwType = DIDFT_PSHBUTTON|DIDFT_MAKEINSTANCE(1),
            .tszName = L"Button 1",
            .wCollectionNumber = 1,
            .wUsagePage = HID_USAGE_PAGE_BUTTON,
            .wUsage = 0x2,
            .wReportId = 1,
        },
        {
            .dwSize = sizeof(DIDEVICEOBJECTINSTANCEW),
            .guidType = GUID_Unknown,
            .dwType = DIDFT_COLLECTION|DIDFT_NODATA|DIDFT_MAKEINSTANCE(0),
            .tszName = L"Collection 0 - Joystick",
            .wUsagePage = HID_USAGE_PAGE_GENERIC,
            .wUsage = HID_USAGE_GENERIC_JOYSTICK,
        },
        {
            .dwSize = sizeof(DIDEVICEOBJECTINSTANCEW),
            .guidType = GUID_Unknown,
            .dwType = DIDFT_COLLECTION|DIDFT_NODATA|DIDFT_MAKEINSTANCE(1),
            .tszName = L"Collection 1 - Joystick",
            .wUsagePage = HID_USAGE_PAGE_GENERIC,
            .wUsage = HID_USAGE_GENERIC_JOYSTICK,
        },
    };
    const struct check_objects_todos objects_todos[ARRAY_SIZE(expect_objects)] =
    {
        {.ofs = TRUE},
        {.ofs = TRUE},
        {.ofs = TRUE},
        {.ofs = TRUE},
        {.ofs = TRUE},
        {.ofs = TRUE},
    };

    struct check_objects_params check_objects_params =
    {
        .expect_count = ARRAY_SIZE(expect_objects),
        .expect_objs = expect_objects,
        .todo_objs = objects_todos,
    };
    DIPROPGUIDANDPATH prop_guid_path =
    {
        .diph =
            {
                .dwSize = sizeof(DIPROPGUIDANDPATH),
                .dwHeaderSize = sizeof(DIPROPHEADER),
                .dwHow = DIPH_DEVICE,
            },
    };
    DIPROPSTRING prop_string =
    {
        .diph =
            {
                .dwSize = sizeof(DIPROPSTRING),
                .dwHeaderSize = sizeof(DIPROPHEADER),
                .dwHow = DIPH_DEVICE,
            },
    };
    DIPROPDWORD prop_dword =
    {
        .diph =
            {
                .dwSize = sizeof(DIPROPDWORD),
                .dwHeaderSize = sizeof(DIPROPHEADER),
                .dwHow = DIPH_DEVICE,
            },
    };
    DIPROPRANGE prop_range =
    {
        .diph =
            {
                .dwSize = sizeof(DIPROPRANGE),
                .dwHeaderSize = sizeof(DIPROPHEADER),
                .dwHow = DIPH_DEVICE,
            },
    };
    DIPROPPOINTER prop_pointer =
    {
        .diph =
            {
                .dwSize = sizeof(DIPROPPOINTER),
                .dwHeaderSize = sizeof(DIPROPHEADER),
            },
    };
    WCHAR cwd[MAX_PATH], tempdir[MAX_PATH];
    DIDEVICEOBJECTDATA objdata[32] = {{0}};
    DIDEVICEOBJECTINSTANCEW objinst = {0};
    DIDEVICEINSTANCEW devinst = {0};
    DIDATAFORMAT dataformat = {0};
    IDirectInputDevice8W *device;
    DIDEVCAPS caps = {0};
    IDirectInput8W *di;
    HANDLE event, file;
    DIJOYSTATE2 state;
    ULONG i, res, ref;
    HRESULT hr;
    WCHAR *tmp;
    GUID guid;
    HWND hwnd;

    GetCurrentDirectoryW( ARRAY_SIZE(cwd), cwd );
    GetTempPathW( ARRAY_SIZE(tempdir), tempdir );
    SetCurrentDirectoryW( tempdir );

    cleanup_registry_keys();
    if (!dinput_driver_start( report_desc, sizeof(report_desc), &hid_caps )) goto done;

    hr = DirectInput8Create( instance, DIRECTINPUT_VERSION, &IID_IDirectInput8W, (void **)&di, NULL );
    if (FAILED(hr))
    {
        win_skip( "DirectInput8Create returned %#x\n", hr );
        goto done;
    }

    hr = IDirectInput8_EnumDevices( di, DI8DEVCLASS_ALL, find_test_device, &devinst, DIEDFL_ALLDEVICES );
    ok( hr == DI_OK, "IDirectInput8_EnumDevices returned: %#x\n", hr );
    if (!IsEqualGUID( &devinst.guidProduct, &expect_guid_product ))
    {
        win_skip( "device not found, skipping tests\n" );
        IDirectInput8_Release( di );
        goto done;
    }

    check_member( devinst, expect_devinst, "%d", dwSize );
    check_member_guid( devinst, expect_devinst, guidProduct );
    check_member( devinst, expect_devinst, "%#x", dwDevType );
    todo_wine
    check_member_wstr( devinst, expect_devinst, tszInstanceName );
    todo_wine
    check_member_wstr( devinst, expect_devinst, tszProductName );
    check_member_guid( devinst, expect_devinst, guidFFDriver );
    check_member( devinst, expect_devinst, "%04x", wUsagePage );
    check_member( devinst, expect_devinst, "%04x", wUsage );

    hr = IDirectInput8_CreateDevice( di, &devinst.guidInstance, NULL, NULL );
    ok( hr == E_POINTER, "IDirectInput8_CreateDevice returned %#x\n", hr );
    hr = IDirectInput8_CreateDevice( di, NULL, &device, NULL );
    ok( hr == E_POINTER, "IDirectInput8_CreateDevice returned %#x\n", hr );
    hr = IDirectInput8_CreateDevice( di, &GUID_NULL, &device, NULL );
    ok( hr == DIERR_DEVICENOTREG, "IDirectInput8_CreateDevice returned %#x\n", hr );
    hr = IDirectInput8_CreateDevice( di, &devinst.guidInstance, &device, NULL );
    ok( hr == DI_OK, "IDirectInput8_CreateDevice returned %#x\n", hr );

    prop_dword.dwData = 0xdeadbeef;
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_VIDPID, &prop_dword.diph );
    ok( hr == DI_OK, "IDirectInputDevice8_GetProperty DIPROP_VIDPID returned %#x\n", hr );
    /* Wine may get the wrong device here, because the test driver creates another instance of
       hidclass.sys, and gets duplicate rawinput handles, which we use in the guidInstance */
    todo_wine_if( prop_dword.dwData != EXPECT_VIDPID )
    ok( prop_dword.dwData == EXPECT_VIDPID, "got %#x expected %#x\n", prop_dword.dwData, EXPECT_VIDPID );

    ref = IDirectInputDevice8_Release( device );
    ok( ref == 0, "IDirectInputDeviceW_Release returned %d\n", ref );

    hr = IDirectInput8_CreateDevice( di, &expect_guid_product, &device, NULL );
    ok( hr == DI_OK, "IDirectInput8_CreateDevice returned %#x\n", hr );

    hr = IDirectInputDevice8_Initialize( device, instance, 0x0700, &GUID_NULL );
    todo_wine
    ok( hr == DIERR_BETADIRECTINPUTVERSION, "IDirectInputDevice8_Initialize returned %#x\n", hr );
    hr = IDirectInputDevice8_Initialize( device, instance, DIRECTINPUT_VERSION, NULL );
    todo_wine
    ok( hr == E_POINTER, "IDirectInputDevice8_Initialize returned %#x\n", hr );
    hr = IDirectInputDevice8_Initialize( device, NULL, DIRECTINPUT_VERSION, &GUID_NULL );
    todo_wine
    ok( hr == DIERR_INVALIDPARAM, "IDirectInputDevice8_Initialize returned %#x\n", hr );
    hr = IDirectInputDevice8_Initialize( device, instance, DIRECTINPUT_VERSION, &GUID_NULL );
    todo_wine
    ok( hr == REGDB_E_CLASSNOTREG, "IDirectInputDevice8_Initialize returned %#x\n", hr );

    hr = IDirectInputDevice8_Initialize( device, instance, DIRECTINPUT_VERSION, &devinst.guidInstance );
    ok( hr == DI_OK, "IDirectInputDevice8_Initialize returned %#x\n", hr );
    guid = devinst.guidInstance;
    memset( &devinst, 0, sizeof(devinst) );
    devinst.dwSize = sizeof(DIDEVICEINSTANCEW);
    hr = IDirectInputDevice8_GetDeviceInfo( device, &devinst );
    ok( hr == DI_OK, "IDirectInputDevice8_GetDeviceInfo returned %#x\n", hr );
    ok( IsEqualGUID( &guid, &devinst.guidInstance ), "got %s expected %s\n", debugstr_guid( &guid ),
        debugstr_guid( &devinst.guidInstance ) );
    hr = IDirectInputDevice8_Initialize( device, instance, DIRECTINPUT_VERSION, &devinst.guidProduct );
    ok( hr == DI_OK, "IDirectInputDevice8_Initialize returned %#x\n", hr );

    hr = IDirectInputDevice8_GetDeviceInfo( device, NULL );
    ok( hr == E_POINTER, "IDirectInputDevice8_GetDeviceInfo returned %#x\n", hr );
    devinst.dwSize = sizeof(DIDEVICEINSTANCEW) + 1;
    hr = IDirectInputDevice8_GetDeviceInfo( device, &devinst );
    ok( hr == DIERR_INVALIDPARAM, "IDirectInputDevice8_GetDeviceInfo returned %#x\n", hr );

    devinst.dwSize = sizeof(DIDEVICEINSTANCE_DX3W);
    hr = IDirectInputDevice8_GetDeviceInfo( device, &devinst );
    ok( hr == DI_OK, "IDirectInputDevice8_GetDeviceInfo returned %#x\n", hr );
    todo_wine
    check_member_guid( devinst, expect_devinst, guidInstance );
    check_member_guid( devinst, expect_devinst, guidProduct );
    check_member( devinst, expect_devinst, "%#x", dwDevType );
    todo_wine
    check_member_wstr( devinst, expect_devinst, tszInstanceName );
    todo_wine
    check_member_wstr( devinst, expect_devinst, tszProductName );

    memset( &devinst, 0, sizeof(devinst) );
    devinst.dwSize = sizeof(DIDEVICEINSTANCEW);
    hr = IDirectInputDevice8_GetDeviceInfo( device, &devinst );
    ok( hr == DI_OK, "IDirectInputDevice8_GetDeviceInfo returned %#x\n", hr );
    check_member( devinst, expect_devinst, "%d", dwSize );
    todo_wine
    check_member_guid( devinst, expect_devinst, guidInstance );
    check_member_guid( devinst, expect_devinst, guidProduct );
    check_member( devinst, expect_devinst, "%#x", dwDevType );
    todo_wine
    check_member_wstr( devinst, expect_devinst, tszInstanceName );
    todo_wine
    check_member_wstr( devinst, expect_devinst, tszProductName );
    check_member_guid( devinst, expect_devinst, guidFFDriver );
    check_member( devinst, expect_devinst, "%04x", wUsagePage );
    check_member( devinst, expect_devinst, "%04x", wUsage );

    hr = IDirectInputDevice8_GetCapabilities( device, NULL );
    ok( hr == E_POINTER, "IDirectInputDevice8_GetCapabilities returned %#x\n", hr );
    hr = IDirectInputDevice8_GetCapabilities( device, &caps );
    todo_wine
    ok( hr == DIERR_INVALIDPARAM, "IDirectInputDevice8_GetCapabilities returned %#x\n", hr );
    caps.dwSize = sizeof(DIDEVCAPS);
    hr = IDirectInputDevice8_GetCapabilities( device, &caps );
    ok( hr == DI_OK, "IDirectInputDevice8_GetCapabilities returned %#x\n", hr );
    check_member( caps, expect_caps, "%d", dwSize );
    check_member( caps, expect_caps, "%#x", dwFlags );
    check_member( caps, expect_caps, "%#x", dwDevType );
    check_member( caps, expect_caps, "%d", dwAxes );
    check_member( caps, expect_caps, "%d", dwButtons );
    check_member( caps, expect_caps, "%d", dwPOVs );
    check_member( caps, expect_caps, "%d", dwFFSamplePeriod );
    check_member( caps, expect_caps, "%d", dwFFMinTimeResolution );
    check_member( caps, expect_caps, "%d", dwFirmwareRevision );
    check_member( caps, expect_caps, "%d", dwHardwareRevision );
    check_member( caps, expect_caps, "%d", dwFFDriverVersion );

    hr = IDirectInputDevice8_GetProperty( device, NULL, NULL );
    ok( hr == DIERR_INVALIDPARAM, "IDirectInputDevice8_GetProperty returned %#x\n", hr );
    hr = IDirectInputDevice8_GetProperty( device, &GUID_NULL, NULL );
    ok( hr == DIERR_INVALIDPARAM, "IDirectInputDevice8_GetProperty returned %#x\n", hr );
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_VIDPID, NULL );
    ok( hr == DIERR_INVALIDPARAM, "IDirectInputDevice8_GetProperty returned %#x\n", hr );
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_VIDPID, &prop_string.diph );
    ok( hr == DIERR_INVALIDPARAM, "IDirectInputDevice8_GetProperty returned %#x\n", hr );
    prop_dword.diph.dwHeaderSize = sizeof(DIPROPHEADER) - 1;
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_VIDPID, &prop_dword.diph );
    ok( hr == DIERR_INVALIDPARAM, "IDirectInputDevice8_GetProperty returned %#x\n", hr );
    prop_dword.diph.dwHeaderSize = sizeof(DIPROPHEADER);

    prop_dword.dwData = 0xdeadbeef;
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_VIDPID, &prop_dword.diph );
    ok( hr == DI_OK, "IDirectInputDevice8_GetProperty DIPROP_VIDPID returned %#x\n", hr );
    ok( prop_dword.dwData == EXPECT_VIDPID, "got %#x expected %#x\n", prop_dword.dwData, EXPECT_VIDPID );

    hr = IDirectInputDevice8_GetProperty( device, DIPROP_GUIDANDPATH, &prop_guid_path.diph );
    ok( hr == DI_OK, "IDirectInputDevice8_GetProperty DIPROP_GUIDANDPATH returned %#x\n", hr );
    todo_wine
    ok( IsEqualGUID( &prop_guid_path.guidClass, &GUID_DEVCLASS_HIDCLASS ), "got guid %s\n",
        debugstr_guid( &prop_guid_path.guidClass ) );
    todo_wine
    ok( !wcsncmp( prop_guid_path.wszPath, expect_path, wcslen( expect_path ) ), "got path %s\n",
        debugstr_w(prop_guid_path.wszPath) );
    if (!(tmp = wcsrchr( prop_guid_path.wszPath, '&' )))
        todo_wine ok( 0, "got path %s\n", debugstr_w(prop_guid_path.wszPath) );
    else
    {
        ok( !wcscmp( wcsrchr( prop_guid_path.wszPath, '&' ), expect_path_end ), "got path %s\n",
            debugstr_w(prop_guid_path.wszPath) );
    }

    hr = IDirectInputDevice8_GetProperty( device, DIPROP_INSTANCENAME, &prop_string.diph );
    ok( hr == DI_OK, "IDirectInputDevice8_GetProperty DIPROP_INSTANCENAME returned %#x\n", hr );
    todo_wine
    ok( !wcscmp( prop_string.wsz, expect_devinst.tszInstanceName ), "got instance %s\n",
        debugstr_w(prop_string.wsz) );
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_PRODUCTNAME, &prop_string.diph );
    ok( hr == DI_OK, "IDirectInputDevice8_GetProperty DIPROP_PRODUCTNAME returned %#x\n", hr );
    todo_wine
    ok( !wcscmp( prop_string.wsz, expect_devinst.tszProductName ), "got product %s\n",
        debugstr_w(prop_string.wsz) );
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_TYPENAME, &prop_string.diph );
    todo_wine
    ok( hr == DI_OK, "IDirectInputDevice8_GetProperty DIPROP_TYPENAME returned %#x\n", hr );
    todo_wine
    ok( !wcscmp( prop_string.wsz, expect_vidpid_str ), "got type %s\n", debugstr_w(prop_string.wsz) );
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_USERNAME, &prop_string.diph );
    ok( hr == S_FALSE, "IDirectInputDevice8_GetProperty DIPROP_USERNAME returned %#x\n", hr );
    todo_wine
    ok( !wcscmp( prop_string.wsz, L"" ), "got user %s\n", debugstr_w(prop_string.wsz) );

    prop_dword.dwData = 0xdeadbeef;
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_JOYSTICKID, &prop_dword.diph );
    ok( hr == DI_OK, "IDirectInputDevice8_GetProperty DIPROP_JOYSTICKID returned %#x\n", hr );
    todo_wine
    ok( prop_dword.dwData == 0, "got %#x expected %#x\n", prop_dword.dwData, 0 );

    prop_dword.dwData = 0xdeadbeef;
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_AXISMODE, &prop_dword.diph );
    todo_wine
    ok( hr == DI_OK, "IDirectInputDevice8_GetProperty DIPROP_AXISMODE returned %#x\n", hr );
    todo_wine
    ok( prop_dword.dwData == DIPROPAXISMODE_ABS, "got %u expected %u\n", prop_dword.dwData, DIPROPAXISMODE_ABS );
    prop_dword.dwData = 0xdeadbeef;
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_BUFFERSIZE, &prop_dword.diph );
    ok( hr == DI_OK, "IDirectInputDevice8_GetProperty DIPROP_BUFFERSIZE returned %#x\n", hr );
    ok( prop_dword.dwData == 0, "got %#x expected %#x\n", prop_dword.dwData, 0 );
    prop_dword.dwData = 0xdeadbeef;
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_FFGAIN, &prop_dword.diph );
    todo_wine
    ok( hr == DI_OK, "IDirectInputDevice8_GetProperty DIPROP_FFGAIN returned %#x\n", hr );
    todo_wine
    ok( prop_dword.dwData == 10000, "got %u expected %u\n", prop_dword.dwData, 10000 );

    hr = IDirectInputDevice8_GetProperty( device, DIPROP_CALIBRATION, &prop_dword.diph );
    ok( hr == DIERR_INVALIDPARAM, "IDirectInputDevice8_GetProperty DIPROP_CALIBRATION returned %#x\n", hr );
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_AUTOCENTER, &prop_dword.diph );
    todo_wine
    ok( hr == DIERR_UNSUPPORTED, "IDirectInputDevice8_GetProperty DIPROP_AUTOCENTER returned %#x\n", hr );
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_DEADZONE, &prop_dword.diph );
    todo_wine
    ok( hr == DIERR_UNSUPPORTED, "IDirectInputDevice8_GetProperty DIPROP_DEADZONE returned %#x\n", hr );
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_FFLOAD, &prop_dword.diph );
    todo_wine
    ok( hr == DIERR_UNSUPPORTED, "IDirectInputDevice8_GetProperty DIPROP_FFLOAD returned %#x\n", hr );
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_GRANULARITY, &prop_dword.diph );
    todo_wine
    ok( hr == DIERR_UNSUPPORTED, "IDirectInputDevice8_GetProperty DIPROP_GRANULARITY returned %#x\n", hr );
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_SATURATION, &prop_dword.diph );
    todo_wine
    ok( hr == DIERR_UNSUPPORTED, "IDirectInputDevice8_GetProperty DIPROP_SATURATION returned %#x\n", hr );
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_RANGE, &prop_range.diph );
    todo_wine
    ok( hr == DIERR_UNSUPPORTED, "IDirectInputDevice8_GetProperty DIPROP_RANGE returned %#x\n", hr );

    prop_dword.diph.dwHow = DIPH_BYUSAGE;
    prop_dword.diph.dwObj = MAKELONG( HID_USAGE_GENERIC_X, HID_USAGE_PAGE_GENERIC );
    prop_dword.dwData = 0xdeadbeef;
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_DEADZONE, &prop_dword.diph );
    todo_wine
    ok( hr == DI_OK, "IDirectInputDevice8_GetProperty DIPROP_DEADZONE returned %#x\n", hr );
    todo_wine
    ok( prop_dword.dwData == 0, "got %u expected %u\n", prop_dword.dwData, 0 );
    prop_dword.dwData = 0xdeadbeef;
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_GRANULARITY, &prop_dword.diph );
    todo_wine
    ok( hr == DI_OK, "IDirectInputDevice8_GetProperty DIPROP_GRANULARITY returned %#x\n", hr );
    todo_wine
    ok( prop_dword.dwData == 1, "got %u expected %u\n", prop_dword.dwData, 1 );
    prop_dword.dwData = 0xdeadbeef;
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_SATURATION, &prop_dword.diph );
    todo_wine
    ok( hr == DI_OK, "IDirectInputDevice8_GetProperty DIPROP_SATURATION returned %#x\n", hr );
    todo_wine
    ok( prop_dword.dwData == 10000, "got %u expected %u\n", prop_dword.dwData, 10000 );

    prop_range.diph.dwHow = DIPH_BYUSAGE;
    prop_range.diph.dwObj = MAKELONG( 0, 0 );
    prop_range.lMin = 0xdeadbeef;
    prop_range.lMax = 0xdeadbeef;
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_RANGE, &prop_range.diph );
    todo_wine
    ok( hr == DIERR_NOTFOUND, "IDirectInputDevice8_GetProperty DIPROP_RANGE returned %#x\n", hr );
    prop_range.diph.dwObj = MAKELONG( 0, HID_USAGE_PAGE_GENERIC );
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_RANGE, &prop_range.diph );
    todo_wine
    ok( hr == DIERR_NOTFOUND, "IDirectInputDevice8_GetProperty DIPROP_RANGE returned %#x\n", hr );
    prop_range.diph.dwObj = MAKELONG( HID_USAGE_PAGE_GENERIC, HID_USAGE_GENERIC_X );
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_RANGE, &prop_range.diph );
    todo_wine
    ok( hr == DIERR_NOTFOUND, "IDirectInputDevice8_GetProperty DIPROP_RANGE returned %#x\n", hr );
    prop_range.diph.dwObj = MAKELONG( HID_USAGE_GENERIC_X, HID_USAGE_PAGE_GENERIC );
    prop_range.lMin = 0xdeadbeef;
    prop_range.lMax = 0xdeadbeef;
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_RANGE, &prop_range.diph );
    ok( hr == DI_OK, "IDirectInputDevice8_GetProperty DIPROP_RANGE returned %#x\n", hr );
    ok( prop_range.lMin == 0, "got %d expected %d\n", prop_range.lMin, 0 );
    ok( prop_range.lMax == 65535, "got %d expected %d\n", prop_range.lMax, 65535 );

    prop_pointer.diph.dwHow = DIPH_BYUSAGE;
    prop_pointer.diph.dwObj = MAKELONG( HID_USAGE_GENERIC_X, HID_USAGE_PAGE_GENERIC );
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_APPDATA, &prop_pointer.diph );
    todo_wine
    ok( hr == DIERR_NOTINITIALIZED, "IDirectInputDevice8_GetProperty DIPROP_APPDATA returned %#x\n", hr );

    hr = IDirectInputDevice8_EnumObjects( device, NULL, NULL, DIDFT_ALL );
    ok( hr == DIERR_INVALIDPARAM, "IDirectInputDevice8_EnumObjects returned %#x\n", hr );
    hr = IDirectInputDevice8_EnumObjects( device, check_object_count, &res, 0x20 );
    todo_wine
    ok( hr == DIERR_INVALIDPARAM, "IDirectInputDevice8_EnumObjects returned %#x\n", hr );
    res = 0;
    hr = IDirectInputDevice8_EnumObjects( device, check_object_count, &res, DIDFT_AXIS | DIDFT_PSHBUTTON );
    ok( hr == DI_OK, "IDirectInputDevice8_EnumObjects returned %#x\n", hr );
    ok( res == 5, "got %u expected %u\n", res, 5 );
    hr = IDirectInputDevice8_EnumObjects( device, check_objects, &check_objects_params, DIDFT_ALL );
    ok( hr == DI_OK, "IDirectInputDevice8_EnumObjects returned %#x\n", hr );
    ok( check_objects_params.index >= check_objects_params.expect_count, "missing %u objects\n",
        check_objects_params.expect_count - check_objects_params.index );

    hr = IDirectInputDevice8_GetObjectInfo( device, NULL, 0, DIPH_DEVICE );
    ok( hr == E_POINTER, "IDirectInputDevice8_GetObjectInfo returned: %#x\n", hr );
    hr = IDirectInputDevice8_GetObjectInfo( device, &objinst, 0, DIPH_DEVICE );
    ok( hr == DIERR_INVALIDPARAM, "IDirectInputDevice8_GetObjectInfo returned: %#x\n", hr );
    objinst.dwSize = sizeof(DIDEVICEOBJECTINSTANCEW);
    hr = IDirectInputDevice8_GetObjectInfo( device, &objinst, 0, DIPH_DEVICE );
    todo_wine
    ok( hr == DIERR_INVALIDPARAM, "IDirectInputDevice8_GetObjectInfo returned: %#x\n", hr );

    res = MAKELONG( HID_USAGE_GENERIC_Z, HID_USAGE_PAGE_GENERIC );
    hr = IDirectInputDevice8_GetObjectInfo( device, &objinst, res, DIPH_BYUSAGE );
    todo_wine
    ok( hr == DIERR_NOTFOUND, "IDirectInputDevice8_GetObjectInfo returned: %#x\n", hr );
    res = MAKELONG( HID_USAGE_GENERIC_X, HID_USAGE_PAGE_GENERIC );
    hr = IDirectInputDevice8_GetObjectInfo( device, &objinst, res, DIPH_BYUSAGE );
    ok( hr == DI_OK, "IDirectInputDevice8_GetObjectInfo returned: %#x\n", hr );

    check_member( objinst, expect_objects[1], "%u", dwSize );
    check_member_guid( objinst, expect_objects[1], guidType );
    todo_wine
    check_member( objinst, expect_objects[1], "%#x", dwOfs );
    check_member( objinst, expect_objects[1], "%#x", dwType );
    check_member( objinst, expect_objects[1], "%#x", dwFlags );
    if (!localized) todo_wine check_member_wstr( objinst, expect_objects[1], tszName );
    check_member( objinst, expect_objects[1], "%u", dwFFMaxForce );
    check_member( objinst, expect_objects[1], "%u", dwFFForceResolution );
    check_member( objinst, expect_objects[1], "%u", wCollectionNumber );
    check_member( objinst, expect_objects[1], "%u", wDesignatorIndex );
    check_member( objinst, expect_objects[1], "%#04x", wUsagePage );
    check_member( objinst, expect_objects[1], "%#04x", wUsage );
    check_member( objinst, expect_objects[1], "%#04x", dwDimension );
    check_member( objinst, expect_objects[1], "%#04x", wExponent );
    check_member( objinst, expect_objects[1], "%u", wReportId );

    hr = IDirectInputDevice8_GetObjectInfo( device, &objinst, 0x14, DIPH_BYOFFSET );
    todo_wine
    ok( hr == DIERR_NOTFOUND, "IDirectInputDevice8_GetObjectInfo returned: %#x\n", hr );
    hr = IDirectInputDevice8_GetObjectInfo( device, &objinst, 0, DIPH_BYOFFSET );
    todo_wine
    ok( hr == DIERR_NOTFOUND, "IDirectInputDevice8_GetObjectInfo returned: %#x\n", hr );
    res = DIDFT_PSHBUTTON | DIDFT_MAKEINSTANCE( 3 );
    hr = IDirectInputDevice8_GetObjectInfo( device, &objinst, res, DIPH_BYID );
    todo_wine
    ok( hr == DIERR_NOTFOUND, "IDirectInputDevice8_GetObjectInfo returned: %#x\n", hr );
    res = DIDFT_PSHBUTTON | DIDFT_MAKEINSTANCE( 1 );
    hr = IDirectInputDevice8_GetObjectInfo( device, &objinst, res, DIPH_BYID );
    ok( hr == DI_OK, "IDirectInputDevice8_GetObjectInfo returned: %#x\n", hr );

    check_member( objinst, expect_objects[5], "%u", dwSize );
    check_member_guid( objinst, expect_objects[5], guidType );
    todo_wine
    check_member( objinst, expect_objects[5], "%#x", dwOfs );
    check_member( objinst, expect_objects[5], "%#x", dwType );
    check_member( objinst, expect_objects[5], "%#x", dwFlags );
    if (!localized) todo_wine check_member_wstr( objinst, expect_objects[5], tszName );
    check_member( objinst, expect_objects[5], "%u", dwFFMaxForce );
    check_member( objinst, expect_objects[5], "%u", dwFFForceResolution );
    check_member( objinst, expect_objects[5], "%u", wCollectionNumber );
    check_member( objinst, expect_objects[5], "%u", wDesignatorIndex );
    check_member( objinst, expect_objects[5], "%#04x", wUsagePage );
    check_member( objinst, expect_objects[5], "%#04x", wUsage );
    check_member( objinst, expect_objects[5], "%#04x", dwDimension );
    check_member( objinst, expect_objects[5], "%#04x", wExponent );
    check_member( objinst, expect_objects[5], "%u", wReportId );

    hr = IDirectInputDevice8_SetDataFormat( device, NULL );
    ok( hr == E_POINTER, "IDirectInputDevice8_SetDataFormat returned: %#x\n", hr );
    hr = IDirectInputDevice8_SetDataFormat( device, &dataformat );
    ok( hr == DIERR_INVALIDPARAM, "IDirectInputDevice8_SetDataFormat returned: %#x\n", hr );
    dataformat.dwSize = sizeof(DIDATAFORMAT);
    hr = IDirectInputDevice8_SetDataFormat( device, &dataformat );
    ok( hr == DIERR_INVALIDPARAM, "IDirectInputDevice8_SetDataFormat returned: %#x\n", hr );
    dataformat.dwObjSize = sizeof(DIOBJECTDATAFORMAT);
    hr = IDirectInputDevice8_SetDataFormat( device, &dataformat );
    ok( hr == DI_OK, "IDirectInputDevice8_SetDataFormat returned: %#x\n", hr );
    hr = IDirectInputDevice8_SetDataFormat( device, &c_dfDIJoystick2 );
    ok( hr == DI_OK, "IDirectInputDevice8_SetDataFormat returned: %#x\n", hr );

    hr = IDirectInputDevice8_GetObjectInfo( device, &objinst, DIJOFS_Y, DIPH_BYOFFSET );
    ok( hr == DI_OK, "IDirectInputDevice8_GetObjectInfo returned: %#x\n", hr );

    check_member( objinst, expect_objects[0], "%u", dwSize );
    check_member_guid( objinst, expect_objects[0], guidType );
    todo_wine
    check_member( objinst, expect_objects[0], "%#x", dwOfs );
    check_member( objinst, expect_objects[0], "%#x", dwType );
    check_member( objinst, expect_objects[0], "%#x", dwFlags );
    if (!localized) todo_wine check_member_wstr( objinst, expect_objects[0], tszName );
    check_member( objinst, expect_objects[0], "%u", dwFFMaxForce );
    check_member( objinst, expect_objects[0], "%u", dwFFForceResolution );
    check_member( objinst, expect_objects[0], "%u", wCollectionNumber );
    check_member( objinst, expect_objects[0], "%u", wDesignatorIndex );
    check_member( objinst, expect_objects[0], "%#04x", wUsagePage );
    check_member( objinst, expect_objects[0], "%#04x", wUsage );
    check_member( objinst, expect_objects[0], "%#04x", dwDimension );
    check_member( objinst, expect_objects[0], "%#04x", wExponent );
    check_member( objinst, expect_objects[0], "%u", wReportId );

    hr = IDirectInputDevice8_SetEventNotification( device, (HANDLE)0xdeadbeef );
    todo_wine
    ok( hr == E_HANDLE, "IDirectInputDevice8_SetEventNotification returned: %#x\n", hr );
    event = CreateEventW( NULL, FALSE, FALSE, NULL );
    ok( event != NULL, "CreateEventW failed, last error %u\n", GetLastError() );
    hr = IDirectInputDevice8_SetEventNotification( device, event );
    ok( hr == DI_OK, "IDirectInputDevice8_SetEventNotification returned: %#x\n", hr );

    file = CreateFileW( prop_guid_path.wszPath, FILE_READ_ACCESS | FILE_WRITE_ACCESS,
                        FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL );
    ok( file != INVALID_HANDLE_VALUE, "got error %u\n", GetLastError() );

    hr = IDirectInputDevice8_SetCooperativeLevel( device, NULL, 0 );
    ok( hr == DIERR_INVALIDPARAM, "IDirectInputDevice8_SetCooperativeLevel returned: %#x\n", hr );
    hr = IDirectInputDevice8_SetCooperativeLevel( device, NULL, DISCL_BACKGROUND );
    ok( hr == DIERR_INVALIDPARAM, "IDirectInputDevice8_SetCooperativeLevel returned: %#x\n", hr );
    hr = IDirectInputDevice8_SetCooperativeLevel( device, NULL, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE );
    ok( hr == E_HANDLE, "IDirectInputDevice8_SetCooperativeLevel returned: %#x\n", hr );
    hr = IDirectInputDevice8_SetCooperativeLevel( device, NULL, DISCL_BACKGROUND | DISCL_EXCLUSIVE );
    ok( hr == E_HANDLE, "IDirectInputDevice8_SetCooperativeLevel returned: %#x\n", hr );
    hr = IDirectInputDevice8_SetCooperativeLevel( device, NULL, DISCL_FOREGROUND | DISCL_EXCLUSIVE );
    ok( hr == E_HANDLE, "IDirectInputDevice8_SetCooperativeLevel returned: %#x\n", hr );

    hwnd = CreateWindowW( L"static", L"dinput", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 10, 10, 200, 200,
                          NULL, NULL, NULL, NULL );

    hr = IDirectInputDevice8_SetCooperativeLevel( device, hwnd, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE );
    ok( hr == DI_OK, "IDirectInputDevice8_SetCooperativeLevel returned: %#x\n", hr );
    hr = IDirectInputDevice8_SetCooperativeLevel( device, hwnd, DISCL_BACKGROUND | DISCL_EXCLUSIVE );
    ok( hr == DI_OK, "IDirectInputDevice8_SetCooperativeLevel returned: %#x\n", hr );
    hr = IDirectInputDevice8_SetCooperativeLevel( device, hwnd, DISCL_FOREGROUND | DISCL_EXCLUSIVE );
    ok( hr == DI_OK, "IDirectInputDevice8_SetCooperativeLevel returned: %#x\n", hr );

    hr = IDirectInputDevice8_Unacquire( device );
    ok( hr == DI_NOEFFECT, "IDirectInputDevice8_Unacquire returned: %#x\n", hr );
    hr = IDirectInputDevice8_Acquire( device );
    ok( hr == DI_OK, "IDirectInputDevice8_Acquire returned: %#x\n", hr );
    hr = IDirectInputDevice8_SetCooperativeLevel( device, hwnd, DISCL_FOREGROUND | DISCL_EXCLUSIVE );
    ok( hr == DIERR_ACQUIRED, "IDirectInputDevice8_SetCooperativeLevel returned: %#x\n", hr );
    hr = IDirectInputDevice8_Unacquire( device );
    ok( hr == DI_OK, "IDirectInputDevice8_Unacquire returned: %#x\n", hr );

    DestroyWindow( hwnd );

    hr = IDirectInputDevice8_SetCooperativeLevel( device, NULL, DISCL_BACKGROUND | DISCL_NONEXCLUSIVE );
    ok( hr == DI_OK, "IDirectInputDevice8_SetCooperativeLevel returned: %#x\n", hr );

    hr = IDirectInputDevice8_GetDeviceState( device, sizeof(DIJOYSTATE2), &state );
    ok( hr == DIERR_NOTACQUIRED, "IDirectInputDevice8_GetDeviceState returned: %#x\n", hr );

    hr = IDirectInputDevice8_Poll( device );
    ok( hr == DIERR_NOTACQUIRED, "IDirectInputDevice8_Poll returned: %#x\n", hr );

    hr = IDirectInputDevice8_Acquire( device );
    ok( hr == DI_OK, "IDirectInputDevice8_Acquire returned: %#x\n", hr );

    hr = IDirectInputDevice8_Poll( device );
    ok( hr == DI_NOEFFECT, "IDirectInputDevice8_Poll returned: %#x\n", hr );

    hr = IDirectInputDevice8_GetDeviceState( device, sizeof(DIJOYSTATE2) + 1, &state );
    ok( hr == DIERR_INVALIDPARAM, "IDirectInputDevice8_GetDeviceState returned: %#x\n", hr );

    for (i = 0; i < ARRAY_SIZE(injected_input); ++i)
    {
        winetest_push_context( "state[%d]", i );
        hr = IDirectInputDevice8_GetDeviceState( device, sizeof(DIJOYSTATE2), &state );
        ok( hr == DI_OK, "IDirectInputDevice8_GetDeviceState returned: %#x\n", hr );
        todo_wine_if( i != 2 )
        check_member( state, expect_state[i], "%d", lX );
        todo_wine_if( i != 2 )
        check_member( state, expect_state[i], "%d", lY );
        todo_wine
        check_member( state, expect_state[i], "%d", lZ );
        check_member( state, expect_state[i], "%d", lRx );
        todo_wine_if( i == 0 )
        check_member( state, expect_state[i], "%#x", rgdwPOV[0] );
        check_member( state, expect_state[i], "%#x", rgdwPOV[1] );
        check_member( state, expect_state[i], "%#x", rgbButtons[0] );
        check_member( state, expect_state[i], "%#x", rgbButtons[1] );
        check_member( state, expect_state[i], "%#x", rgbButtons[2] );

        send_hid_input( file, &injected_input[i], sizeof(*injected_input) );

        res = WaitForSingleObject( event, 100 );
        if (i == 0 || i == 3) todo_wine_if( i == 0 )
        ok( res == WAIT_TIMEOUT, "WaitForSingleObject succeeded\n" );
        else ok( res == WAIT_OBJECT_0, "WaitForSingleObject failed\n" ); ResetEvent( event );
        winetest_pop_context();
    }

    hr = IDirectInputDevice8_GetDeviceState( device, sizeof(DIJOYSTATE2), &state );
    ok( hr == DI_OK, "IDirectInputDevice8_GetDeviceState returned: %#x\n", hr );
    winetest_push_context( "state[%d]", i );
    todo_wine
    check_member( state, expect_state[i], "%d", lX );
    todo_wine
    check_member( state, expect_state[i], "%d", lY );
    todo_wine
    check_member( state, expect_state[i], "%d", lZ );
    check_member( state, expect_state[i], "%d", lRx );
    check_member( state, expect_state[i], "%#x", rgdwPOV[0] );
    check_member( state, expect_state[i], "%#x", rgdwPOV[1] );
    check_member( state, expect_state[i], "%#x", rgbButtons[0] );
    check_member( state, expect_state[i], "%#x", rgbButtons[1] );
    check_member( state, expect_state[i], "%#x", rgbButtons[2] );
    winetest_pop_context();

    res = 1;
    hr = IDirectInputDevice8_GetDeviceData( device, sizeof(DIDEVICEOBJECTDATA) - 1, objdata, &res, DIGDD_PEEK );
    todo_wine
    ok( hr == DIERR_INVALIDPARAM, "IDirectInputDevice8_GetDeviceData returned %#x\n", hr );
    res = 1;
    hr = IDirectInputDevice8_GetDeviceData( device, sizeof(DIDEVICEOBJECTDATA), objdata, &res, DIGDD_PEEK );
    ok( hr == DIERR_NOTBUFFERED, "IDirectInputDevice8_GetDeviceData returned %#x\n", hr );

    hr = IDirectInputDevice8_Unacquire( device );
    ok( hr == DI_OK, "IDirectInputDevice8_Unacquire returned: %#x\n", hr );
    prop_dword.diph.dwHow = DIPH_DEVICE;
    prop_dword.diph.dwObj = 0;
    prop_dword.dwData = 1;
    hr = IDirectInputDevice8_SetProperty( device, DIPROP_BUFFERSIZE, &prop_dword.diph );
    ok( hr == DI_OK, "IDirectInputDevice8_SetProperty DIPROP_BUFFERSIZE returned %#x\n", hr );
    hr = IDirectInputDevice8_Acquire( device );
    ok( hr == DI_OK, "IDirectInputDevice8_Unacquire returned: %#x\n", hr );

    res = 1;
    hr = IDirectInputDevice8_GetDeviceData( device, sizeof(DIDEVICEOBJECTDATA), objdata, &res, DIGDD_PEEK );
    ok( hr == DI_OK, "IDirectInputDevice8_GetDeviceData returned %#x\n", hr );
    ok( res == 0, "got %u expected %u\n", res, 0 );

    send_hid_input( file, &injected_input[0], sizeof(*injected_input) );
    res = WaitForSingleObject( event, 100 );
    ok( res == WAIT_OBJECT_0, "WaitForSingleObject failed\n" );
    ResetEvent( event );

    res = 1;
    hr = IDirectInputDevice8_GetDeviceData( device, sizeof(DIDEVICEOBJECTDATA), objdata, &res, DIGDD_PEEK );
    ok( hr == DI_BUFFEROVERFLOW, "IDirectInputDevice8_GetDeviceData returned %#x\n", hr );
    ok( res == 0, "got %u expected %u\n", res, 0 );
    res = 1;
    hr = IDirectInputDevice8_GetDeviceData( device, sizeof(DIDEVICEOBJECTDATA), objdata, &res, 0 );
    todo_wine
    ok( hr == DI_OK, "IDirectInputDevice8_GetDeviceData returned %#x\n", hr );
    ok( res == 0, "got %u expected %u\n", res, 0 );

    hr = IDirectInputDevice8_Unacquire( device );
    ok( hr == DI_OK, "IDirectInputDevice8_Unacquire returned: %#x\n", hr );
    prop_dword.diph.dwHow = DIPH_DEVICE;
    prop_dword.diph.dwObj = 0;
    prop_dword.dwData = 10;
    hr = IDirectInputDevice8_SetProperty( device, DIPROP_BUFFERSIZE, &prop_dword.diph );
    ok( hr == DI_OK, "IDirectInputDevice8_SetProperty DIPROP_BUFFERSIZE returned %#x\n", hr );
    hr = IDirectInputDevice8_Acquire( device );
    ok( hr == DI_OK, "IDirectInputDevice8_Unacquire returned: %#x\n", hr );

    send_hid_input( file, &injected_input[1], sizeof(*injected_input) );
    res = WaitForSingleObject( event, 100 );
    ok( res == WAIT_OBJECT_0, "WaitForSingleObject failed\n" );
    ResetEvent( event );

    res = 1;
    hr = IDirectInputDevice8_GetDeviceData( device, sizeof(DIDEVICEOBJECTDATA), objdata, &res, DIGDD_PEEK );
    ok( hr == DI_OK, "IDirectInputDevice8_GetDeviceData returned %#x\n", hr );
    ok( res == 1, "got %u expected %u\n", res, 1 );
    check_member( objdata[0], expect_objdata[0], "%#x", dwOfs );
    check_member( objdata[0], expect_objdata[0], "%#x", dwData );
    ok( objdata[0].uAppData == -1, "got %p, expected %p\n", (void *)objdata[0].uAppData, (void *)-1 );
    res = 4;
    hr = IDirectInputDevice8_GetDeviceData( device, sizeof(DIDEVICEOBJECTDATA), objdata, &res, 0 );
    ok( hr == DI_OK, "IDirectInputDevice8_GetDeviceData returned %#x\n", hr );
    ok( res == 4, "got %u expected %u\n", res, 4 );
    for (i = 0; i < 4; ++i)
    {
        winetest_push_context( "objdata[%d]", i );
        check_member( objdata[i], expect_objdata[1 + i], "%#x", dwOfs );
        check_member( objdata[i], expect_objdata[1 + i], "%#x", dwData );
        ok( objdata[i].uAppData == -1, "got %p, expected %p\n", (void *)objdata[i].uAppData, (void *)-1 );
        winetest_pop_context();
    }

    send_hid_input( file, &injected_input[2], sizeof(*injected_input) );
    res = WaitForSingleObject( event, 100 );
    ok( res == WAIT_OBJECT_0, "WaitForSingleObject failed\n" );
    ResetEvent( event );
    send_hid_input( file, &injected_input[4], sizeof(*injected_input) );
    res = WaitForSingleObject( event, 100 );
    ok( res == WAIT_OBJECT_0, "WaitForSingleObject failed\n" );
    ResetEvent( event );

    res = 1;
    hr = IDirectInputDevice8_GetDeviceData( device, sizeof(DIDEVICEOBJECTDATA), objdata, &res, 0 );
    ok( hr == DI_BUFFEROVERFLOW, "IDirectInputDevice8_GetDeviceData returned %#x\n", hr );
    ok( res == 1, "got %u expected %u\n", res, 1 );
    todo_wine
    check_member( objdata[0], expect_objdata[5], "%#x", dwOfs );
    todo_wine
    check_member( objdata[0], expect_objdata[5], "%#x", dwData );
    ok( objdata[0].uAppData == -1, "got %p, expected %p\n", (void *)objdata[0].uAppData, (void *)-1 );
    res = ARRAY_SIZE(objdata);
    hr = IDirectInputDevice8_GetDeviceData( device, sizeof(DIDEVICEOBJECTDATA), objdata, &res, 0 );
    ok( hr == DI_OK, "IDirectInputDevice8_GetDeviceData returned %#x\n", hr );
    ok( res == 8, "got %u expected %u\n", res, 8 );
    for (i = 0; i < 8; ++i)
    {
        winetest_push_context( "objdata[%d]", i );
        todo_wine
        check_member( objdata[i], expect_objdata[6 + i], "%#x", dwOfs );
        todo_wine_if( i != 3 && i != 4 && i != 7 )
        check_member( objdata[i], expect_objdata[6 + i], "%#x", dwData );
        ok( objdata[i].uAppData == -1, "got %p, expected %p\n", (void *)objdata[i].uAppData, (void *)-1 );
        winetest_pop_context();
    }

    send_hid_input( file, &injected_input[3], sizeof(*injected_input) );
    res = WaitForSingleObject( event, 100 );
    ok( res == WAIT_OBJECT_0, "WaitForSingleObject failed\n" );
    ResetEvent( event );

    hr = IDirectInputDevice8_GetDeviceState( device, sizeof(DIJOYSTATE2), &state );
    ok( hr == DI_OK, "IDirectInputDevice8_GetDeviceState returned: %#x\n", hr );
    todo_wine
    check_member( state, expect_state[3], "%d", lX );
    todo_wine
    check_member( state, expect_state[3], "%d", lY );
    todo_wine
    check_member( state, expect_state[3], "%d", lZ );
    check_member( state, expect_state[3], "%d", lRx );
    check_member( state, expect_state[3], "%d", rgdwPOV[0] );
    check_member( state, expect_state[3], "%d", rgdwPOV[1] );
    check_member( state, expect_state[3], "%#x", rgbButtons[0] );
    check_member( state, expect_state[3], "%#x", rgbButtons[1] );
    check_member( state, expect_state[3], "%#x", rgbButtons[2] );

    prop_range.diph.dwHow = DIPH_DEVICE;
    prop_range.diph.dwObj = 0;
    prop_range.lMin = 1000;
    prop_range.lMax = 51000;
    hr = IDirectInputDevice8_SetProperty( device, DIPROP_RANGE, &prop_range.diph );
    ok( hr == DI_OK, "IDirectInputDevice8_SetProperty DIPROP_RANGE returned %#x\n", hr );
    prop_range.diph.dwHow = DIPH_BYUSAGE;
    prop_range.diph.dwObj = MAKELONG( HID_USAGE_GENERIC_X, HID_USAGE_PAGE_GENERIC );
    prop_range.lMin = -4000;
    prop_range.lMax = -14000;
    hr = IDirectInputDevice8_SetProperty( device, DIPROP_RANGE, &prop_range.diph );
    todo_wine
    ok( hr == DIERR_INVALIDPARAM, "IDirectInputDevice8_SetProperty DIPROP_RANGE returned %#x\n", hr );
    prop_range.lMin = -14000;
    prop_range.lMax = -4000;
    hr = IDirectInputDevice8_SetProperty( device, DIPROP_RANGE, &prop_range.diph );
    ok( hr == DI_OK, "IDirectInputDevice8_SetProperty DIPROP_RANGE returned %#x\n", hr );

    prop_range.diph.dwHow = DIPH_DEVICE;
    prop_range.diph.dwObj = 0;
    prop_range.lMin = 0xdeadbeef;
    prop_range.lMax = 0xdeadbeef;
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_RANGE, &prop_range.diph );
    todo_wine
    ok( hr == DIERR_UNSUPPORTED, "IDirectInputDevice8_GetProperty DIPROP_RANGE returned %#x\n", hr );
    prop_range.diph.dwHow = DIPH_BYUSAGE;
    prop_range.diph.dwObj = MAKELONG( HID_USAGE_GENERIC_X, HID_USAGE_PAGE_GENERIC );
    prop_range.lMin = 0xdeadbeef;
    prop_range.lMax = 0xdeadbeef;
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_RANGE, &prop_range.diph );
    ok( hr == DI_OK, "IDirectInputDevice8_GetProperty DIPROP_RANGE returned %#x\n", hr );
    ok( prop_range.lMin == -14000, "got %d expected %d\n", prop_range.lMin, -14000 );
    ok( prop_range.lMax == -4000, "got %d expected %d\n", prop_range.lMax, -4000 );
    prop_range.diph.dwHow = DIPH_BYUSAGE;
    prop_range.diph.dwObj = MAKELONG( HID_USAGE_GENERIC_Y, HID_USAGE_PAGE_GENERIC );
    prop_range.lMin = 0xdeadbeef;
    prop_range.lMax = 0xdeadbeef;
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_RANGE, &prop_range.diph );
    ok( hr == DI_OK, "IDirectInputDevice8_GetProperty DIPROP_RANGE returned %#x\n", hr );
    ok( prop_range.lMin == 1000, "got %d expected %d\n", prop_range.lMin, 1000 );
    ok( prop_range.lMax == 51000, "got %d expected %d\n", prop_range.lMax, 51000 );

    hr = IDirectInputDevice8_GetDeviceState( device, sizeof(DIJOYSTATE2), &state );
    ok( hr == DI_OK, "IDirectInputDevice8_GetDeviceState returned: %#x\n", hr );
    todo_wine
    check_member( state, expect_state_abs[1], "%d", lX );
    todo_wine
    check_member( state, expect_state_abs[1], "%d", lY );
    todo_wine
    check_member( state, expect_state_abs[1], "%d", lZ );
    check_member( state, expect_state_abs[1], "%d", lRx );
    check_member( state, expect_state_abs[1], "%d", rgdwPOV[0] );
    check_member( state, expect_state_abs[1], "%d", rgdwPOV[1] );
    check_member( state, expect_state_abs[1], "%#x", rgbButtons[0] );
    check_member( state, expect_state_abs[1], "%#x", rgbButtons[1] );
    check_member( state, expect_state_abs[1], "%#x", rgbButtons[2] );

    hr = IDirectInputDevice8_SetProperty( device, NULL, NULL );
    ok( hr == DIERR_INVALIDPARAM, "IDirectInputDevice8_SetProperty returned %#x\n", hr );
    hr = IDirectInputDevice8_SetProperty( device, &GUID_NULL, NULL );
    ok( hr == DIERR_INVALIDPARAM, "IDirectInputDevice8_SetProperty returned %#x\n", hr );
    hr = IDirectInputDevice8_SetProperty( device, DIPROP_VIDPID, NULL );
    ok( hr == DIERR_INVALIDPARAM, "IDirectInputDevice8_SetProperty returned %#x\n", hr );
    hr = IDirectInputDevice8_SetProperty( device, DIPROP_VIDPID, &prop_string.diph );
    ok( hr == DIERR_INVALIDPARAM, "IDirectInputDevice8_SetProperty returned %#x\n", hr );

    prop_dword.diph.dwHow = DIPH_DEVICE;
    prop_dword.diph.dwObj = 0;
    prop_dword.dwData = 0xdeadbeef;
    hr = IDirectInputDevice8_SetProperty( device, DIPROP_VIDPID, &prop_dword.diph );
    ok( hr == DIERR_READONLY, "IDirectInputDevice8_SetProperty DIPROP_VIDPID returned %#x\n", hr );
    hr = IDirectInputDevice8_SetProperty( device, DIPROP_GUIDANDPATH, &prop_guid_path.diph );
    ok( hr == DIERR_READONLY, "IDirectInputDevice8_SetProperty DIPROP_GUIDANDPATH returned %#x\n", hr );

    prop_string.diph.dwHow = DIPH_DEVICE;
    prop_string.diph.dwObj = 0;
    wcscpy( prop_string.wsz, L"instance name" );
    hr = IDirectInputDevice8_SetProperty( device, DIPROP_INSTANCENAME, &prop_string.diph );
    ok( hr == DIERR_UNSUPPORTED, "IDirectInputDevice8_SetProperty DIPROP_INSTANCENAME returned %#x\n", hr );

    wcscpy( prop_string.wsz, L"product name" );
    hr = IDirectInputDevice8_SetProperty( device, DIPROP_PRODUCTNAME, &prop_string.diph );
    todo_wine
    ok( hr == DI_OK, "IDirectInputDevice8_SetProperty DIPROP_PRODUCTNAME returned %#x\n", hr );
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_PRODUCTNAME, &prop_string.diph );
    ok( hr == DI_OK, "IDirectInputDevice8_GetProperty DIPROP_PRODUCTNAME returned %#x\n", hr );
    todo_wine
    ok( !wcscmp( prop_string.wsz, expect_devinst.tszProductName ), "got product %s\n",
        debugstr_w(prop_string.wsz) );

    hr = IDirectInputDevice8_SetProperty( device, DIPROP_TYPENAME, &prop_string.diph );
    ok( hr == DIERR_READONLY, "IDirectInputDevice8_SetProperty DIPROP_TYPENAME returned %#x\n", hr );
    hr = IDirectInputDevice8_SetProperty( device, DIPROP_USERNAME, &prop_string.diph );
    ok( hr == DIERR_READONLY, "IDirectInputDevice8_SetProperty DIPROP_USERNAME returned %#x\n", hr );
    hr = IDirectInputDevice8_SetProperty( device, DIPROP_FFLOAD, &prop_dword.diph );
    ok( hr == DIERR_READONLY, "IDirectInputDevice8_SetProperty DIPROP_FFLOAD returned %#x\n", hr );
    hr = IDirectInputDevice8_SetProperty( device, DIPROP_GRANULARITY, &prop_dword.diph );
    ok( hr == DIERR_READONLY, "IDirectInputDevice8_SetProperty DIPROP_GRANULARITY returned %#x\n", hr );

    hr = IDirectInputDevice8_SetProperty( device, DIPROP_JOYSTICKID, &prop_dword.diph );
    todo_wine
    ok( hr == DIERR_ACQUIRED, "IDirectInputDevice8_SetProperty DIPROP_JOYSTICKID returned %#x\n", hr );
    hr = IDirectInputDevice8_SetProperty( device, DIPROP_AXISMODE, &prop_dword.diph );
    ok( hr == DIERR_ACQUIRED, "IDirectInputDevice8_SetProperty DIPROP_AXISMODE returned %#x\n", hr );
    hr = IDirectInputDevice8_SetProperty( device, DIPROP_BUFFERSIZE, &prop_dword.diph );
    ok( hr == DIERR_ACQUIRED, "IDirectInputDevice8_SetProperty DIPROP_BUFFERSIZE returned %#x\n", hr );
    hr = IDirectInputDevice8_SetProperty( device, DIPROP_AUTOCENTER, &prop_dword.diph );
    todo_wine
    ok( hr == DIERR_ACQUIRED, "IDirectInputDevice8_SetProperty DIPROP_AUTOCENTER returned %#x\n", hr );
    prop_pointer.diph.dwHow = DIPH_BYUSAGE;
    prop_pointer.diph.dwObj = MAKELONG( HID_USAGE_GENERIC_X, HID_USAGE_PAGE_GENERIC );
    hr = IDirectInputDevice8_SetProperty( device, DIPROP_APPDATA, &prop_pointer.diph );
    todo_wine
    ok( hr == DIERR_ACQUIRED, "IDirectInputDevice8_SetProperty DIPROP_APPDATA returned %#x\n", hr );

    prop_dword.diph.dwHow = DIPH_DEVICE;
    prop_dword.diph.dwObj = 0;
    prop_dword.dwData = 10001;
    hr = IDirectInputDevice8_SetProperty( device, DIPROP_DEADZONE, &prop_dword.diph );
    todo_wine
    ok( hr == DIERR_INVALIDPARAM, "IDirectInputDevice8_SetProperty DIPROP_DEADZONE returned %#x\n", hr );
    hr = IDirectInputDevice8_SetProperty( device, DIPROP_SATURATION, &prop_dword.diph );
    todo_wine
    ok( hr == DIERR_INVALIDPARAM, "IDirectInputDevice8_SetProperty DIPROP_SATURATION returned %#x\n", hr );
    prop_dword.dwData = 1000;
    hr = IDirectInputDevice8_SetProperty( device, DIPROP_DEADZONE, &prop_dword.diph );
    todo_wine
    ok( hr == DI_OK, "IDirectInputDevice8_SetProperty DIPROP_DEADZONE returned %#x\n", hr );
    prop_dword.dwData = 6000;
    hr = IDirectInputDevice8_SetProperty( device, DIPROP_SATURATION, &prop_dword.diph );
    todo_wine
    ok( hr == DI_OK, "IDirectInputDevice8_SetProperty DIPROP_SATURATION returned %#x\n", hr );

    hr = IDirectInputDevice8_GetProperty( device, DIPROP_DEADZONE, &prop_dword.diph );
    todo_wine
    ok( hr == DIERR_UNSUPPORTED, "IDirectInputDevice8_GetProperty DIPROP_DEADZONE returned %#x\n", hr );
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_SATURATION, &prop_dword.diph );
    todo_wine
    ok( hr == DIERR_UNSUPPORTED, "IDirectInputDevice8_GetProperty DIPROP_SATURATION returned %#x\n", hr );

    prop_dword.diph.dwHow = DIPH_BYUSAGE;
    prop_dword.diph.dwObj = MAKELONG( HID_USAGE_GENERIC_X, HID_USAGE_PAGE_GENERIC );
    prop_dword.dwData = 2000;
    hr = IDirectInputDevice8_SetProperty( device, DIPROP_DEADZONE, &prop_dword.diph );
    todo_wine
    ok( hr == DI_OK, "IDirectInputDevice8_SetProperty DIPROP_DEADZONE returned %#x\n", hr );
    ok( prop_dword.dwData == 2000, "got %u expected %u\n", prop_dword.dwData, 2000 );
    prop_dword.dwData = 7000;
    hr = IDirectInputDevice8_SetProperty( device, DIPROP_SATURATION, &prop_dword.diph );
    todo_wine
    ok( hr == DI_OK, "IDirectInputDevice8_SetProperty DIPROP_SATURATION returned %#x\n", hr );

    prop_dword.diph.dwHow = DIPH_BYUSAGE;
    prop_dword.diph.dwObj = MAKELONG( HID_USAGE_GENERIC_X, HID_USAGE_PAGE_GENERIC );
    prop_dword.dwData = 0xdeadbeef;
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_DEADZONE, &prop_dword.diph );
    todo_wine
    ok( hr == DI_OK, "IDirectInputDevice8_GetProperty DIPROP_DEADZONE returned %#x\n", hr );
    todo_wine
    ok( prop_dword.dwData == 2000, "got %u expected %u\n", prop_dword.dwData, 2000 );
    prop_dword.dwData = 0xdeadbeef;
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_SATURATION, &prop_dword.diph );
    todo_wine
    ok( hr == DI_OK, "IDirectInputDevice8_GetProperty DIPROP_SATURATION returned %#x\n", hr );
    todo_wine
    ok( prop_dword.dwData == 7000, "got %u expected %u\n", prop_dword.dwData, 7000 );

    prop_dword.diph.dwHow = DIPH_BYUSAGE;
    prop_dword.diph.dwObj = MAKELONG( HID_USAGE_GENERIC_Y, HID_USAGE_PAGE_GENERIC );
    prop_dword.dwData = 0xdeadbeef;
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_DEADZONE, &prop_dword.diph );
    todo_wine
    ok( hr == DI_OK, "IDirectInputDevice8_GetProperty DIPROP_DEADZONE returned %#x\n", hr );
    todo_wine
    ok( prop_dword.dwData == 1000, "got %u expected %u\n", prop_dword.dwData, 1000 );
    prop_dword.dwData = 0xdeadbeef;
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_SATURATION, &prop_dword.diph );
    todo_wine
    ok( hr == DI_OK, "IDirectInputDevice8_GetProperty DIPROP_SATURATION returned %#x\n", hr );
    todo_wine
    ok( prop_dword.dwData == 6000, "got %u expected %u\n", prop_dword.dwData, 6000 );

    for (i = 0; i < ARRAY_SIZE(injected_input); ++i)
    {
        winetest_push_context( "state[%d]", i );
        hr = IDirectInputDevice8_GetDeviceState( device, sizeof(DIJOYSTATE2), &state );
        ok( hr == DI_OK, "IDirectInputDevice8_GetDeviceState returned: %#x\n", hr );
        if (broken( state.lX == -10750 )) win_skip( "Ignoring 32-bit rounding\n" );
        else
        {
            todo_wine_if( i != 2 )
            check_member( state, expect_state_abs[i], "%d", lX );
            todo_wine_if( i != 2 )
            check_member( state, expect_state_abs[i], "%d", lY );
        }
        todo_wine
        check_member( state, expect_state_abs[i], "%d", lZ );
        check_member( state, expect_state_abs[i], "%d", lRx );
        check_member( state, expect_state_abs[i], "%d", rgdwPOV[0] );
        check_member( state, expect_state_abs[i], "%d", rgdwPOV[1] );
        check_member( state, expect_state_abs[i], "%#x", rgbButtons[0] );
        check_member( state, expect_state_abs[i], "%#x", rgbButtons[1] );
        check_member( state, expect_state_abs[i], "%#x", rgbButtons[2] );

        send_hid_input( file, &injected_input[i], sizeof(*injected_input) );

        res = WaitForSingleObject( event, 100 );
        if (i == 0 || i == 3) todo_wine_if( i == 0 )
        ok( res == WAIT_TIMEOUT, "WaitForSingleObject succeeded\n" );
        else ok( res == WAIT_OBJECT_0, "WaitForSingleObject failed\n" ); ResetEvent( event );
        winetest_pop_context();
    }

    hr = IDirectInputDevice8_GetDeviceState( device, sizeof(DIJOYSTATE2), &state );
    ok( hr == DI_OK, "IDirectInputDevice8_GetDeviceState returned: %#x\n", hr );
    winetest_push_context( "state[%d]", i );
    todo_wine
    check_member( state, expect_state_abs[i], "%d", lX );
    todo_wine
    check_member( state, expect_state_abs[i], "%d", lY );
    todo_wine
    check_member( state, expect_state_abs[i], "%d", lZ );
    check_member( state, expect_state_abs[i], "%d", lRx );
    check_member( state, expect_state_abs[i], "%d", rgdwPOV[0] );
    check_member( state, expect_state_abs[i], "%d", rgdwPOV[1] );
    check_member( state, expect_state_abs[i], "%#x", rgbButtons[0] );
    check_member( state, expect_state_abs[i], "%#x", rgbButtons[1] );
    check_member( state, expect_state_abs[i], "%#x", rgbButtons[2] );
    winetest_pop_context();

    hr = IDirectInputDevice8_Unacquire( device );
    ok( hr == DI_OK, "IDirectInputDevice8_Unacquire returned: %#x\n", hr );

    prop_dword.diph.dwHow = DIPH_DEVICE;
    prop_dword.diph.dwObj = 0;
    hr = IDirectInputDevice8_SetProperty( device, DIPROP_JOYSTICKID, &prop_dword.diph );
    ok( hr == DIERR_UNSUPPORTED, "IDirectInputDevice8_SetProperty DIPROP_JOYSTICKID returned %#x\n", hr );
    prop_dword.dwData = 0x1000;
    hr = IDirectInputDevice8_SetProperty( device, DIPROP_BUFFERSIZE, &prop_dword.diph );
    ok( hr == DI_OK, "IDirectInputDevice8_SetProperty DIPROP_BUFFERSIZE returned %#x\n", hr );
    prop_dword.dwData = 0xdeadbeef;
    hr = IDirectInputDevice8_SetProperty( device, DIPROP_AUTOCENTER, &prop_dword.diph );
    todo_wine
    ok( hr == DIERR_INVALIDPARAM, "IDirectInputDevice8_SetProperty DIPROP_AUTOCENTER returned %#x\n", hr );
    prop_dword.dwData = DIPROPAUTOCENTER_ON;
    hr = IDirectInputDevice8_SetProperty( device, DIPROP_AUTOCENTER, &prop_dword.diph );
    ok( hr == DIERR_UNSUPPORTED, "IDirectInputDevice8_SetProperty DIPROP_AUTOCENTER returned %#x\n", hr );
    prop_pointer.diph.dwHow = DIPH_BYUSAGE;
    prop_pointer.diph.dwObj = MAKELONG( HID_USAGE_GENERIC_X, HID_USAGE_PAGE_GENERIC );
    prop_pointer.uData = 0xfeedcafe;
    hr = IDirectInputDevice8_SetProperty( device, DIPROP_APPDATA, &prop_pointer.diph );
    todo_wine
    ok( hr == DI_OK, "IDirectInputDevice8_SetProperty DIPROP_APPDATA returned %#x\n", hr );

    prop_dword.dwData = 0xdeadbeef;
    hr = IDirectInputDevice8_SetProperty( device, DIPROP_AXISMODE, &prop_dword.diph );
    todo_wine
    ok( hr == DIERR_INVALIDPARAM, "IDirectInputDevice8_SetProperty DIPROP_AXISMODE returned %#x\n", hr );
    prop_dword.dwData = DIPROPAXISMODE_REL;
    hr = IDirectInputDevice8_SetProperty( device, DIPROP_AXISMODE, &prop_dword.diph );
    ok( hr == DI_OK, "IDirectInputDevice8_SetProperty DIPROP_AXISMODE returned %#x\n", hr );

    hr = IDirectInputDevice8_Acquire( device );
    ok( hr == DI_OK, "IDirectInputDevice8_Unacquire returned: %#x\n", hr );

    prop_dword.dwData = 0xdeadbeef;
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_AXISMODE, &prop_dword.diph );
    todo_wine
    ok( hr == DI_OK, "IDirectInputDevice8_GetProperty DIPROP_AXISMODE returned %#x\n", hr );
    todo_wine
    ok( prop_dword.dwData == DIPROPAXISMODE_REL, "got %u expected %u\n", prop_dword.dwData, DIPROPAXISMODE_REL );

    prop_dword.dwData = 0xdeadbeef;
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_BUFFERSIZE, &prop_dword.diph );
    ok( hr == DI_OK, "IDirectInputDevice8_GetProperty DIPROP_BUFFERSIZE returned %#x\n", hr );
    ok( prop_dword.dwData == 0x1000, "got %#x expected %#x\n", prop_dword.dwData, 0x1000 );

    prop_pointer.diph.dwHow = DIPH_BYUSAGE;
    prop_pointer.diph.dwObj = MAKELONG( HID_USAGE_GENERIC_X, HID_USAGE_PAGE_GENERIC );
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_APPDATA, &prop_pointer.diph );
    todo_wine
    ok( hr == DI_OK, "IDirectInputDevice8_GetProperty DIPROP_APPDATA returned %#x\n", hr );
    ok( prop_pointer.uData == 0xfeedcafe, "got %p expected %p\n", (void *)prop_pointer.uData, (void *)0xfeedcafe );

    prop_dword.diph.dwHow = DIPH_DEVICE;
    prop_dword.diph.dwObj = 0;
    prop_dword.dwData = 0xdeadbeef;
    hr = IDirectInputDevice8_SetProperty( device, DIPROP_FFGAIN, &prop_dword.diph );
    todo_wine
    ok( hr == DIERR_INVALIDPARAM, "IDirectInputDevice8_SetProperty DIPROP_FFGAIN returned %#x\n", hr );
    prop_dword.dwData = 1000;
    hr = IDirectInputDevice8_SetProperty( device, DIPROP_FFGAIN, &prop_dword.diph );
    todo_wine
    ok( hr == DI_OK, "IDirectInputDevice8_SetProperty DIPROP_FFGAIN returned %#x\n", hr );

    prop_dword.dwData = 0xdeadbeef;
    hr = IDirectInputDevice8_SetProperty( device, DIPROP_CALIBRATION, &prop_dword.diph );
    todo_wine
    ok( hr == DIERR_INVALIDPARAM, "IDirectInputDevice8_SetProperty DIPROP_CALIBRATION returned %#x\n", hr );
    prop_dword.dwData = 0xdeadbeef;
    hr = IDirectInputDevice8_SetProperty( device, DIPROP_DEADZONE, &prop_dword.diph );
    todo_wine
    ok( hr == DIERR_INVALIDPARAM, "IDirectInputDevice8_SetProperty DIPROP_DEADZONE returned %#x\n", hr );
    prop_dword.dwData = 0xdeadbeef;
    hr = IDirectInputDevice8_SetProperty( device, DIPROP_SATURATION, &prop_dword.diph );
    todo_wine
    ok( hr == DIERR_INVALIDPARAM, "IDirectInputDevice8_SetProperty DIPROP_SATURATION returned %#x\n", hr );

    for (i = 0; i < ARRAY_SIZE(injected_input); ++i)
    {
        winetest_push_context( "state[%d]", i );
        hr = IDirectInputDevice8_GetDeviceState( device, sizeof(DIJOYSTATE2), &state );
        ok( hr == DI_OK, "IDirectInputDevice8_GetDeviceState returned: %#x\n", hr );
        todo_wine
        check_member( state, expect_state_rel[i], "%d", lX );
        todo_wine
        check_member( state, expect_state_rel[i], "%d", lY );
        todo_wine
        check_member( state, expect_state_rel[i], "%d", lZ );
        check_member( state, expect_state_rel[i], "%d", lRx );
        check_member( state, expect_state_rel[i], "%d", rgdwPOV[0] );
        check_member( state, expect_state_rel[i], "%d", rgdwPOV[1] );
        check_member( state, expect_state_rel[i], "%#x", rgbButtons[0] );
        check_member( state, expect_state_rel[i], "%#x", rgbButtons[1] );
        check_member( state, expect_state_rel[i], "%#x", rgbButtons[2] );

        send_hid_input( file, &injected_input[i], sizeof(*injected_input) );

        res = WaitForSingleObject( event, 100 );
        if (i == 3) ok( res == WAIT_TIMEOUT, "WaitForSingleObject succeeded\n" );
        else ok( res == WAIT_OBJECT_0, "WaitForSingleObject failed\n" );
        ResetEvent( event );
        winetest_pop_context();
    }

    hr = IDirectInputDevice8_GetDeviceState( device, sizeof(DIJOYSTATE2), &state );
    ok( hr == DI_OK, "IDirectInputDevice8_GetDeviceState returned: %#x\n", hr );
    winetest_push_context( "state[%d]", i );
    todo_wine
    check_member( state, expect_state_rel[i], "%d", lX );
    todo_wine
    check_member( state, expect_state_rel[i], "%d", lY );
    todo_wine
    check_member( state, expect_state_rel[i], "%d", lZ );
    check_member( state, expect_state_rel[i], "%d", lRx );
    check_member( state, expect_state_rel[i], "%d", rgdwPOV[0] );
    check_member( state, expect_state_rel[i], "%d", rgdwPOV[1] );
    check_member( state, expect_state_rel[i], "%#x", rgbButtons[0] );
    check_member( state, expect_state_rel[i], "%#x", rgbButtons[1] );
    check_member( state, expect_state_rel[i], "%#x", rgbButtons[2] );
    winetest_pop_context();

    hr = IDirectInputDevice8_Unacquire( device );
    ok( hr == DI_OK, "IDirectInputDevice8_Unacquire returned: %#x\n", hr );

    /* FIXME: we have to wait a bit because Wine DInput internal thread keeps a reference */
    Sleep( 100 );

    ref = IDirectInputDevice8_Release( device );
    ok( ref == 0, "IDirectInputDeviceW_Release returned %d\n", ref );

    CloseHandle( event );
    CloseHandle( file );

    ref = IDirectInput8_Release( di );
    ok( ref == 0, "IDirectInput8_Release returned %d\n", ref );

done:
    pnp_driver_stop();
    cleanup_registry_keys();
    SetCurrentDirectoryW( cwd );
}

struct device_desc
{
    const BYTE *report_desc_buf;
    ULONG report_desc_len;
    HIDP_CAPS hid_caps;
};

static void test_device_types( void )
{
#include "psh_hid_macros.h"
    static const unsigned char unknown_desc[] =
    {
        USAGE_PAGE(1, HID_USAGE_PAGE_GENERIC),
        USAGE(1, HID_USAGE_GENERIC_JOYSTICK),
        COLLECTION(1, Application),
            USAGE(1, HID_USAGE_GENERIC_JOYSTICK),
            COLLECTION(1, Physical),
                USAGE_PAGE(1, HID_USAGE_PAGE_BUTTON),
                USAGE_MINIMUM(1, 1),
                USAGE_MAXIMUM(1, 6),
                LOGICAL_MINIMUM(1, 0),
                LOGICAL_MAXIMUM(1, 1),
                PHYSICAL_MINIMUM(1, 0),
                PHYSICAL_MAXIMUM(1, 1),
                REPORT_SIZE(1, 1),
                REPORT_COUNT(1, 8),
                INPUT(1, Data|Var|Abs),
            END_COLLECTION,
        END_COLLECTION,
    };
    static const unsigned char limited_desc[] =
    {
        USAGE_PAGE(1, HID_USAGE_PAGE_GENERIC),
        USAGE(1, HID_USAGE_GENERIC_JOYSTICK),
        COLLECTION(1, Application),
            USAGE(1, HID_USAGE_GENERIC_JOYSTICK),
            COLLECTION(1, Physical),
                USAGE(1, HID_USAGE_GENERIC_X),
                USAGE(1, HID_USAGE_GENERIC_Y),
                LOGICAL_MINIMUM(1, 0),
                LOGICAL_MAXIMUM(1, 127),
                PHYSICAL_MINIMUM(1, 0),
                PHYSICAL_MAXIMUM(1, 127),
                REPORT_SIZE(1, 8),
                REPORT_COUNT(1, 2),
                INPUT(1, Data|Var|Abs),

                USAGE_PAGE(1, HID_USAGE_PAGE_BUTTON),
                USAGE_MINIMUM(1, 1),
                USAGE_MAXIMUM(1, 6),
                LOGICAL_MINIMUM(1, 0),
                LOGICAL_MAXIMUM(1, 1),
                PHYSICAL_MINIMUM(1, 0),
                PHYSICAL_MAXIMUM(1, 1),
                REPORT_SIZE(1, 1),
                REPORT_COUNT(1, 8),
                INPUT(1, Data|Var|Abs),
            END_COLLECTION,
        END_COLLECTION,
    };
    static const unsigned char gamepad_desc[] =
    {
        USAGE_PAGE(1, HID_USAGE_PAGE_GENERIC),
        USAGE(1, HID_USAGE_GENERIC_GAMEPAD),
        COLLECTION(1, Application),
            USAGE(1, HID_USAGE_GENERIC_GAMEPAD),
            COLLECTION(1, Physical),
                USAGE(1, HID_USAGE_GENERIC_X),
                USAGE(1, HID_USAGE_GENERIC_Y),
                LOGICAL_MINIMUM(1, 0),
                LOGICAL_MAXIMUM(1, 127),
                PHYSICAL_MINIMUM(1, 0),
                PHYSICAL_MAXIMUM(1, 127),
                REPORT_SIZE(1, 8),
                REPORT_COUNT(1, 2),
                INPUT(1, Data|Var|Abs),

                USAGE_PAGE(1, HID_USAGE_PAGE_BUTTON),
                USAGE_MINIMUM(1, 1),
                USAGE_MAXIMUM(1, 6),
                LOGICAL_MINIMUM(1, 0),
                LOGICAL_MAXIMUM(1, 1),
                PHYSICAL_MINIMUM(1, 0),
                PHYSICAL_MAXIMUM(1, 1),
                REPORT_SIZE(1, 1),
                REPORT_COUNT(1, 8),
                INPUT(1, Data|Var|Abs),
            END_COLLECTION,
        END_COLLECTION,
    };
    static const unsigned char joystick_desc[] =
    {
        USAGE_PAGE(1, HID_USAGE_PAGE_GENERIC),
        USAGE(1, HID_USAGE_GENERIC_JOYSTICK),
        COLLECTION(1, Application),
            USAGE(1, HID_USAGE_GENERIC_JOYSTICK),
            COLLECTION(1, Physical),
                USAGE(1, HID_USAGE_GENERIC_X),
                USAGE(1, HID_USAGE_GENERIC_Y),
                USAGE(1, HID_USAGE_GENERIC_Z),
                LOGICAL_MINIMUM(1, 0),
                LOGICAL_MAXIMUM(1, 127),
                PHYSICAL_MINIMUM(1, 0),
                PHYSICAL_MAXIMUM(1, 127),
                REPORT_SIZE(1, 8),
                REPORT_COUNT(1, 3),
                INPUT(1, Data|Var|Abs),

                USAGE(1, HID_USAGE_GENERIC_HATSWITCH),
                LOGICAL_MINIMUM(1, 1),
                LOGICAL_MAXIMUM(1, 8),
                PHYSICAL_MINIMUM(1, 0),
                PHYSICAL_MAXIMUM(1, 8),
                REPORT_SIZE(1, 8),
                REPORT_COUNT(1, 1),
                INPUT(1, Data|Var|Abs|Null),

                USAGE_PAGE(1, HID_USAGE_PAGE_BUTTON),
                USAGE_MINIMUM(1, 1),
                USAGE_MAXIMUM(1, 5),
                LOGICAL_MINIMUM(1, 0),
                LOGICAL_MAXIMUM(1, 1),
                PHYSICAL_MINIMUM(1, 0),
                PHYSICAL_MAXIMUM(1, 1),
                REPORT_SIZE(1, 1),
                REPORT_COUNT(1, 8),
                INPUT(1, Data|Var|Abs),
            END_COLLECTION,
        END_COLLECTION,
    };
#include "pop_hid_macros.h"

    static struct device_desc device_desc[] =
    {
        {
            .report_desc_buf = unknown_desc,
            .report_desc_len = sizeof(unknown_desc),
            .hid_caps =
            {
                .InputReportByteLength = 1,
            },
        },
        {
            .report_desc_buf = limited_desc,
            .report_desc_len = sizeof(limited_desc),
            .hid_caps =
            {
                .InputReportByteLength = 3,
            },
        },
        {
            .report_desc_buf = gamepad_desc,
            .report_desc_len = sizeof(gamepad_desc),
            .hid_caps =
            {
                .InputReportByteLength = 3,
            },
        },
        {
            .report_desc_buf = joystick_desc,
            .report_desc_len = sizeof(joystick_desc),
            .hid_caps =
            {
                .InputReportByteLength = 5,
            },
        },
    };
    static const DIDEVCAPS expect_caps[] =
    {
        {
            .dwSize = sizeof(DIDEVCAPS),
            .dwFlags = DIDC_ATTACHED|DIDC_EMULATED,
            .dwDevType = DIDEVTYPE_HID|(DI8DEVTYPESUPPLEMENTAL_UNKNOWN << 8)|DI8DEVTYPE_SUPPLEMENTAL,
            .dwButtons = 6,
        },
        {
            .dwSize = sizeof(DIDEVCAPS),
            .dwFlags = DIDC_ATTACHED|DIDC_EMULATED,
            .dwDevType = DIDEVTYPE_HID|(DI8DEVTYPEJOYSTICK_LIMITED << 8)|DI8DEVTYPE_JOYSTICK,
            .dwAxes = 2,
            .dwButtons = 6,
        },
        {
            .dwSize = sizeof(DIDEVCAPS),
            .dwFlags = DIDC_ATTACHED|DIDC_EMULATED,
            .dwDevType = DIDEVTYPE_HID|(DI8DEVTYPEGAMEPAD_STANDARD << 8)|DI8DEVTYPE_GAMEPAD,
            .dwAxes = 2,
            .dwButtons = 6,
        },
        {
            .dwSize = sizeof(DIDEVCAPS),
            .dwFlags = DIDC_ATTACHED|DIDC_EMULATED,
            .dwDevType = DIDEVTYPE_HID|(DI8DEVTYPEJOYSTICK_STANDARD << 8)|DI8DEVTYPE_JOYSTICK,
            .dwAxes = 3,
            .dwPOVs = 1,
            .dwButtons = 5,
        },
    };

    const DIDEVICEINSTANCEW expect_devinst[] =
    {
        {
            .dwSize = sizeof(DIDEVICEINSTANCEW),
            .guidInstance = expect_guid_product,
            .guidProduct = expect_guid_product,
            .dwDevType = DIDEVTYPE_HID|(DI8DEVTYPESUPPLEMENTAL_UNKNOWN << 8)|DI8DEVTYPE_SUPPLEMENTAL,
            .tszInstanceName = L"Wine test root driver",
            .tszProductName = L"Wine test root driver",
            .guidFFDriver = GUID_NULL,
            .wUsagePage = HID_USAGE_PAGE_GENERIC,
            .wUsage = HID_USAGE_GENERIC_JOYSTICK,
        },
        {
            .dwSize = sizeof(DIDEVICEINSTANCEW),
            .guidInstance = expect_guid_product,
            .guidProduct = expect_guid_product,
            .dwDevType = DIDEVTYPE_HID|(DI8DEVTYPEJOYSTICK_LIMITED << 8)|DI8DEVTYPE_JOYSTICK,
            .tszInstanceName = L"Wine test root driver",
            .tszProductName = L"Wine test root driver",
            .guidFFDriver = GUID_NULL,
            .wUsagePage = HID_USAGE_PAGE_GENERIC,
            .wUsage = HID_USAGE_GENERIC_JOYSTICK,
        },
        {
            .dwSize = sizeof(DIDEVICEINSTANCEW),
            .guidInstance = expect_guid_product,
            .guidProduct = expect_guid_product,
            .dwDevType = DIDEVTYPE_HID|(DI8DEVTYPEGAMEPAD_STANDARD << 8)|DI8DEVTYPE_GAMEPAD,
            .tszInstanceName = L"Wine test root driver",
            .tszProductName = L"Wine test root driver",
            .guidFFDriver = GUID_NULL,
            .wUsagePage = HID_USAGE_PAGE_GENERIC,
            .wUsage = HID_USAGE_GENERIC_GAMEPAD,
        },
        {
            .dwSize = sizeof(DIDEVICEINSTANCEW),
            .guidInstance = expect_guid_product,
            .guidProduct = expect_guid_product,
            .dwDevType = DIDEVTYPE_HID|(DI8DEVTYPEJOYSTICK_STANDARD << 8)|DI8DEVTYPE_JOYSTICK,
            .tszInstanceName = L"Wine test root driver",
            .tszProductName = L"Wine test root driver",
            .guidFFDriver = GUID_NULL,
            .wUsagePage = HID_USAGE_PAGE_GENERIC,
            .wUsage = HID_USAGE_GENERIC_JOYSTICK,
        },
    };

    DIDEVICEINSTANCEW devinst = {.dwSize = sizeof(DIDEVICEINSTANCEW)};
    DIDEVCAPS caps = {.dwSize = sizeof(DIDEVCAPS)};
    WCHAR cwd[MAX_PATH], tempdir[MAX_PATH];
    IDirectInputDevice8W *device;
    IDirectInput8W *di;
    ULONG i, ref;
    HRESULT hr;

    for (i = 0; i < ARRAY_SIZE(device_desc); ++i)
    {
        winetest_push_context( "desc[%d]", i );
        GetCurrentDirectoryW( ARRAY_SIZE(cwd), cwd );
        GetTempPathW( ARRAY_SIZE(tempdir), tempdir );
        SetCurrentDirectoryW( tempdir );

        cleanup_registry_keys();
        if (!dinput_driver_start( device_desc[i].report_desc_buf, device_desc[i].report_desc_len,
                                  &device_desc[i].hid_caps ))
        {
            i = ARRAY_SIZE(device_desc);
            goto done;
        }

        hr = DirectInput8Create( instance, DIRECTINPUT_VERSION, &IID_IDirectInput8W, (void **)&di, NULL );
        if (FAILED(hr))
        {
            win_skip( "DirectInput8Create returned %#x\n", hr );
            i = ARRAY_SIZE(device_desc);
            goto done;
        }

        hr = IDirectInput8_EnumDevices( di, DI8DEVCLASS_ALL, find_test_device, &devinst, DIEDFL_ALLDEVICES );
        ok( hr == DI_OK, "IDirectInput8_EnumDevices returned: %#x\n", hr );
        if (!IsEqualGUID( &devinst.guidProduct, &expect_guid_product ))
        {
            win_skip( "device not found, skipping tests\n" );
            IDirectInput8_Release( di );
            i = ARRAY_SIZE(device_desc);
            goto done;
        }

        hr = IDirectInput8_CreateDevice( di, &expect_guid_product, &device, NULL );
        ok( hr == DI_OK, "IDirectInput8_CreateDevice returned %#x\n", hr );

        hr = IDirectInputDevice8_GetDeviceInfo( device, &devinst );
        ok( hr == DI_OK, "IDirectInputDevice8_GetDeviceInfo returned %#x\n", hr );
        check_member( devinst, expect_devinst[i], "%d", dwSize );
        todo_wine
        check_member_guid( devinst, expect_devinst[i], guidInstance );
        check_member_guid( devinst, expect_devinst[i], guidProduct );
        check_member( devinst, expect_devinst[i], "%#x", dwDevType );
        todo_wine
        check_member_wstr( devinst, expect_devinst[i], tszInstanceName );
        todo_wine
        check_member_wstr( devinst, expect_devinst[i], tszProductName );
        check_member_guid( devinst, expect_devinst[i], guidFFDriver );
        check_member( devinst, expect_devinst[i], "%04x", wUsagePage );
        check_member( devinst, expect_devinst[i], "%04x", wUsage );

        hr = IDirectInputDevice8_GetCapabilities( device, &caps );
        ok( hr == DI_OK, "IDirectInputDevice8_GetCapabilities returned %#x\n", hr );
        check_member( caps, expect_caps[i], "%d", dwSize );
        check_member( caps, expect_caps[i], "%#x", dwFlags );
        check_member( caps, expect_caps[i], "%#x", dwDevType );
        check_member( caps, expect_caps[i], "%d", dwAxes );
        check_member( caps, expect_caps[i], "%d", dwButtons );
        check_member( caps, expect_caps[i], "%d", dwPOVs );
        check_member( caps, expect_caps[i], "%d", dwFFSamplePeriod );
        check_member( caps, expect_caps[i], "%d", dwFFMinTimeResolution );
        check_member( caps, expect_caps[i], "%d", dwFirmwareRevision );
        check_member( caps, expect_caps[i], "%d", dwHardwareRevision );
        check_member( caps, expect_caps[i], "%d", dwFFDriverVersion );

        ref = IDirectInputDevice8_Release( device );
        ok( ref == 0, "IDirectInputDeviceW_Release returned %d\n", ref );

        ref = IDirectInput8_Release( di );
        ok( ref == 0, "IDirectInput8_Release returned %d\n", ref );

    done:
        pnp_driver_stop();
        cleanup_registry_keys();
        SetCurrentDirectoryW( cwd );
        winetest_pop_context();
    }
}

START_TEST( hid )
{
    HANDLE mapping;
    BOOL is_wow64;

    instance = GetModuleHandleW( NULL );
    localized = GetUserDefaultLCID() != MAKELANGID(LANG_ENGLISH, SUBLANG_DEFAULT);
    pSignerSign = (void *)GetProcAddress( LoadLibraryW( L"mssign32" ), "SignerSign" );

    if (IsWow64Process( GetCurrentProcess(), &is_wow64 ) && is_wow64)
    {
        skip( "Running in WoW64.\n" );
        return;
    }

    mapping = CreateFileMappingW( INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(*test_data),
                                  L"Global\\winetest_dinput_section" );
    if (!mapping && GetLastError() == ERROR_ACCESS_DENIED)
    {
        win_skip( "Failed to create test data mapping.\n" );
        return;
    }
    ok( !!mapping, "got error %u\n", GetLastError() );
    test_data = MapViewOfFile( mapping, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, 1024 );
    test_data->running_under_wine = !strcmp( winetest_platform, "wine" );
    test_data->winetest_report_success = winetest_report_success;
    test_data->winetest_debug = winetest_debug;

    okfile = CreateFileW( L"C:\\windows\\winetest_dinput_okfile", GENERIC_READ | GENERIC_WRITE,
                          FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, 0, NULL );
    ok( okfile != INVALID_HANDLE_VALUE, "failed to create file, error %u\n", GetLastError() );

    subtest( "driver_hid" );
    test_hidp_kdr();
    test_hid_driver( 0, FALSE );
    test_hid_driver( 1, FALSE );
    test_hid_driver( 0, TRUE );
    test_hid_driver( 1, TRUE );

    CoInitialize( NULL );
    test_device_types();
    test_simple_joystick();
    CoUninitialize();

    UnmapViewOfFile( test_data );
    CloseHandle( mapping );
    CloseHandle( okfile );
    DeleteFileW( L"C:\\windows\\winetest_dinput_okfile" );
}
