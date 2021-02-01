/*
 * Copyright 2007 Jacek Caban for CodeWeavers
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

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "wine/test.h"
#include <winbase.h>
#include <windef.h>
#include <winnt.h>
#include <winternl.h>
#include <winnls.h>
#include <stdio.h>

#include "oaidl.h"
#include "initguid.h"

static BOOL   (WINAPI *pQueryActCtxSettingsW)(DWORD,HANDLE,LPCWSTR,LPCWSTR,LPWSTR,SIZE_T,SIZE_T*);

static NTSTATUS(NTAPI *pRtlFindActivationContextSectionString)(DWORD,const GUID *,ULONG,PUNICODE_STRING,PACTCTX_SECTION_KEYED_DATA);
static BOOLEAN (NTAPI *pRtlCreateUnicodeStringFromAsciiz)(PUNICODE_STRING, PCSZ);
static VOID    (NTAPI *pRtlFreeUnicodeString)(PUNICODE_STRING);

#ifdef __i386__
#define ARCH "x86"
#elif defined __x86_64__
#define ARCH "amd64"
#elif defined __arm__
#define ARCH "arm"
#elif defined __aarch64__
#define ARCH "arm64"
#else
#define ARCH "none"
#endif

static const char manifest1[] =
"<assembly xmlns=\"urn:schemas-microsoft-com:asm.v1\" manifestVersion=\"1.0\">"
"<assemblyIdentity version=\"1.0.0.0\"  name=\"Wine.Test\" type=\"win32\"></assemblyIdentity>"
"</assembly>";

static const char manifest1_1[] =
"<assembly xmlns = \"urn:schemas-microsoft-com:asm.v1\" manifestVersion = \"1.0\">"
"<assemblyIdentity version = \"1.0.0.0\" name = \"Wine.Test\" type = \"win32\"></assemblyIdentity>"
"</assembly>";

static const char manifest2[] =
"<assembly xmlns=\"urn:schemas-microsoft-com:asm.v1\" manifestVersion=\"1.0\">"
"<assemblyIdentity version=\"1.2.3.4\" name=\"Wine.Test\" type=\"win32\">"
"</assemblyIdentity>"
"<dependency>"
"<dependentAssembly>"
"<assemblyIdentity type=\"win32\" name=\"testdep\" version=\"6.5.4.3\" processorArchitecture=\"" ARCH "\">"
"</assemblyIdentity>"
"</dependentAssembly>"
"</dependency>"
"</assembly>";

DEFINE_GUID(IID_CoTest,    0x12345678, 0x1234, 0x5678, 0x12, 0x34, 0x11, 0x11, 0x22, 0x22, 0x33, 0x33);
DEFINE_GUID(IID_CoTest2,   0x12345678, 0x1234, 0x5678, 0x12, 0x34, 0x11, 0x11, 0x22, 0x22, 0x33, 0x34);
DEFINE_GUID(CLSID_clrclass,0x22345678, 0x1234, 0x5678, 0x12, 0x34, 0x11, 0x11, 0x22, 0x22, 0x33, 0x33);
DEFINE_GUID(IID_TlibTest,  0x99999999, 0x8888, 0x7777, 0x66, 0x66, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55);
DEFINE_GUID(IID_TlibTest2, 0x99999999, 0x8888, 0x7777, 0x66, 0x66, 0x55, 0x55, 0x55, 0x55, 0x55, 0x56);
DEFINE_GUID(IID_TlibTest3, 0x99999999, 0x8888, 0x7777, 0x66, 0x66, 0x55, 0x55, 0x55, 0x55, 0x55, 0x57);
DEFINE_GUID(IID_TlibTest4, 0x99999999, 0x8888, 0x7777, 0x66, 0x66, 0x55, 0x55, 0x55, 0x55, 0x55, 0x58);
DEFINE_GUID(IID_Iifaceps,  0x66666666, 0x8888, 0x7777, 0x66, 0x66, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55);
DEFINE_GUID(IID_Ibifaceps, 0x66666666, 0x8888, 0x7777, 0x66, 0x66, 0x55, 0x55, 0x55, 0x55, 0x55, 0x57);
DEFINE_GUID(IID_Iifaceps2, 0x76666666, 0x8888, 0x7777, 0x66, 0x66, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55);
DEFINE_GUID(IID_Iifaceps3, 0x86666666, 0x8888, 0x7777, 0x66, 0x66, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55);
DEFINE_GUID(IID_Iiface,    0x96666666, 0x8888, 0x7777, 0x66, 0x66, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55);
DEFINE_GUID(IID_PS32,      0x66666666, 0x8888, 0x7777, 0x66, 0x66, 0x55, 0x55, 0x55, 0x55, 0x55, 0x56);

static const char manifest3[] =
"<assembly xmlns=\"urn:schemas-microsoft-com:asm.v1\" manifestVersion=\"1.0\">"
"<assemblyIdentity version=\"1.2.3.4\"  name=\"Wine.Test\" type=\"win32\""
" publicKeyToken=\"6595b6414666f1df\" />"
"<description />"
"<file name=\"testlib.dll\">"
"<windowClass>wndClass</windowClass>"
"    <comClass description=\"Test com class\""
"              clsid=\"{12345678-1234-5678-1234-111122223333}\""
"              tlbid=\"{99999999-8888-7777-6666-555555555555}\""
"              threadingModel=\"Neutral\""
"              progid=\"ProgId.ProgId\""
"              miscStatus=\"cantlinkinside\""
"              miscStatusIcon=\"recomposeonresize\""
"              miscStatusContent=\"insideout\""
"              miscStatusThumbnail=\"alignable\""
"              miscStatusDocPrint=\"simpleframe,setclientsitefirst\""
"    >"
"        <progid>ProgId.ProgId.1</progid>"
"        <progid>ProgId.ProgId.2</progid>"
"        <progid>ProgId.ProgId.3</progid>"
"        <progid>ProgId.ProgId.4</progid>"
"        <progid>ProgId.ProgId.5</progid>"
"        <progid>ProgId.ProgId.6</progid>"
"    </comClass>"
"    <comClass clsid=\"{12345678-1234-5678-1234-111122223334}\" threadingModel=\"Neutral\" >"
"        <progid>ProgId.ProgId.7</progid>"
"    </comClass>"
"    <comInterfaceProxyStub "
"        name=\"Iifaceps\""
"        tlbid=\"{99999999-8888-7777-6666-555555555558}\""
"        iid=\"{66666666-8888-7777-6666-555555555555}\""
"        proxyStubClsid32=\"{66666666-8888-7777-6666-555555555556}\""
"        threadingModel=\"Free\""
"        numMethods=\"10\""
"        baseInterface=\"{66666666-8888-7777-6666-555555555557}\""
"    />"
"</file>"
"    <comInterfaceExternalProxyStub "
"        name=\"Iifaceps2\""
"        tlbid=\"{99999999-8888-7777-6666-555555555558}\""
"        iid=\"{76666666-8888-7777-6666-555555555555}\""
"        proxyStubClsid32=\"{66666666-8888-7777-6666-555555555556}\""
"        numMethods=\"10\""
"        baseInterface=\"{66666666-8888-7777-6666-555555555557}\""
"    />"
"    <comInterfaceExternalProxyStub "
"        name=\"Iifaceps3\""
"        tlbid=\"{99999999-8888-7777-6666-555555555558}\""
"        iid=\"{86666666-8888-7777-6666-555555555555}\""
"        numMethods=\"10\""
"        baseInterface=\"{66666666-8888-7777-6666-555555555557}\""
"    />"
"    <clrSurrogate "
"        clsid=\"{96666666-8888-7777-6666-555555555555}\""
"        name=\"testsurrogate\""
"        runtimeVersion=\"v2.0.50727\""
"    />"
"    <clrClass "
"        clsid=\"{22345678-1234-5678-1234-111122223333}\""
"        name=\"clrclass\""
"        progid=\"clrprogid\""
"        description=\"test description\""
"        tlbid=\"{99999999-8888-7777-6666-555555555555}\""
"        runtimeVersion=\"1.2.3.4\""
"        threadingModel=\"Neutral\""
"    >"
"        <progid>clrprogid.1</progid>"
"        <progid>clrprogid.2</progid>"
"        <progid>clrprogid.3</progid>"
"        <progid>clrprogid.4</progid>"
"        <progid>clrprogid.5</progid>"
"        <progid>clrprogid.6</progid>"
"    </clrClass>"
"</assembly>";

static const char manifest_wndcls1[] =
"<assembly xmlns=\"urn:schemas-microsoft-com:asm.v1\" manifestVersion=\"1.0\">"
"<assemblyIdentity version=\"1.2.3.4\"  name=\"testdep1\" type=\"win32\" processorArchitecture=\"" ARCH "\"/>"
"<file name=\"testlib1.dll\">"
"<windowClass versioned=\"yes\">wndClass1</windowClass>"
"<windowClass>wndClass2</windowClass>"
" <typelib tlbid=\"{99999999-8888-7777-6666-555555555558}\" version=\"1.0\" helpdir=\"\" />"
"</file>"
"<file name=\"testlib1_2.dll\" />"
"</assembly>";

static const char manifest_wndcls2[] =
"<assembly xmlns=\"urn:schemas-microsoft-com:asm.v1\" manifestVersion=\"1.0\">"
"<assemblyIdentity version=\"4.3.2.1\"  name=\"testdep2\" type=\"win32\" processorArchitecture=\"" ARCH "\" />"
"<file name=\"testlib2.dll\">"
" <windowClass versioned=\"no\">wndClass3</windowClass>"
" <windowClass>wndClass4</windowClass>"
" <typelib tlbid=\"{99999999-8888-7777-6666-555555555555}\" version=\"1.0\" helpdir=\"help\" resourceid=\"409\""
"          flags=\"HiddeN,CoNTROL,rESTRICTED\" />"
" <typelib tlbid=\"{99999999-8888-7777-6666-555555555556}\" version=\"1.0\" helpdir=\"help1\" resourceid=\"409\" />"
" <typelib tlbid=\"{99999999-8888-7777-6666-555555555557}\" version=\"1.0\" helpdir=\"\" />"
"</file>"
"<file name=\"testlib2_2.dll\" />"
"</assembly>";

static const char manifest_wndcls_main[] =
"<assembly xmlns=\"urn:schemas-microsoft-com:asm.v1\" manifestVersion=\"1.0\">"
"<assemblyIdentity version=\"1.2.3.4\" name=\"Wine.Test\" type=\"win32\" />"
"<dependency>"
" <dependentAssembly>"
"  <assemblyIdentity type=\"win32\" name=\"testdep1\" version=\"1.2.3.4\" processorArchitecture=\"" ARCH "\" />"
" </dependentAssembly>"
"</dependency>"
"<dependency>"
" <dependentAssembly>"
"  <assemblyIdentity type=\"win32\" name=\"testdep2\" version=\"4.3.2.1\" processorArchitecture=\"" ARCH "\" />"
" </dependentAssembly>"
"</dependency>"
"</assembly>";

static const char manifest4[] =
"<assembly xmlns=\"urn:schemas-microsoft-com:asm.v1\" manifestVersion=\"1.0\">"
"<assemblyIdentity version=\"1.2.3.4\" name=\"Wine.Test\" type=\"win32\">"
"</assemblyIdentity>"
"<dependency>"
"<dependentAssembly>"
"<assemblyIdentity type=\"win32\" name=\"Microsoft.Windows.Common-Controls\" "
    "version=\"6.0.1.0\" processorArchitecture=\"" ARCH "\" publicKeyToken=\"6595b64144ccf1df\">"
"</assemblyIdentity>"
"</dependentAssembly>"
"</dependency>"
"</assembly>";

static const char manifest5[] =
"<assembly xmlns=\"urn:schemas-microsoft-com:asm.v1\" manifestVersion=\"1.0\">"
"<assemblyIdentity version=\"1.2.3.4\" name=\"Wine.Test\" type=\"win32\">"
"</assemblyIdentity>"
"<dependency>"
"    <dependentAssembly dependencyType=\"preRequisite\" allowDelayedBinding=\"true\">"
"        <assemblyIdentity name=\"Missing.Assembly\" version=\"1.0.0.0\" />"
"    </dependentAssembly>"
"</dependency>"
"</assembly>";

static const char manifest6[] =
"<assembly xmlns=\"urn:schemas-microsoft-com:asm.v1\" manifestVersion=\"1.0\">"
"<assemblyIdentity version=\"1.0.0.0\"  name=\"Wine.Test\" type=\"win32\"></assemblyIdentity>"
"<trustInfo xmlns=\"urn:schemas-microsoft-com:asm.v1\">"
"    <security>"
"        <requestedPrivileges>"
"            <requestedExecutionLevel level=\"ASINVOKER\" uiAccess=\"false\"/>"
"        </requestedPrivileges>"
"    </security>"
"</trustInfo>"
"</assembly>";

static const char manifest7[] =
"<assembly xmlns=\"urn:schemas-microsoft-com:asm.v1\" manifestVersion=\"1.0\">"
"<assemblyIdentity version=\"1.0.0.0\"  name=\"Wine.Test\" type=\"win32\"></assemblyIdentity>"
"<trustInfo xmlns=\"urn:schemas-microsoft-com:asm.v2\">"
"    <security>"
"        <requestedPrivileges>"
"            <requestedExecutionLevel level=\"requireAdministrator\" uiAccess=\"TRUE\"/>"
"        </requestedPrivileges>"
"    </security>"
"</trustInfo>"
"</assembly>";

static const char manifest8[] =
"<assembly xmlns=\"urn:schemas-microsoft-com:asm.v1\" manifestVersion=\"1.0\">"
"<assemblyIdentity version=\"1.0.0.0\"  name=\"Wine.Test\" type=\"win32\"></assemblyIdentity>"
"<trustInfo xmlns=\"urn:schemas-microsoft-com:asm.v2\">"
"    <security>"
"        <requestedPrivileges>"
"            <requestedExecutionLevel level=\"requireAdministrator\" uiAccess=\"true\">"
"            </requestedExecutionLevel>"
"        </requestedPrivileges>"
"    </security>"
"</trustInfo>"
"</assembly>";

static const char manifest9[] =
"<assembly xmlns=\"urn:schemas-microsoft-com:asm.v1\" manifestVersion=\"1.0\">"
"<assemblyIdentity version=\"1.0.0.0\"  name=\"Wine.Test\" type=\"win32\"></assemblyIdentity>"
"<trustInfo xmlns=\"urn:schemas-microsoft-com:asm.v2\">"
"    <security>"
"        <requestedPrivileges>"
"            <requestedExecutionLevel level=\"requireAdministrator\"/>"
"        </requestedPrivileges>"
"    </security>"
"</trustInfo>"
"<trustInfo xmlns=\"urn:schemas-microsoft-com:asm.v2\">"
"    <security>"
"        <requestedPrivileges>"
"        </requestedPrivileges>"
"    </security>"
"</trustInfo>"
"</assembly>";

static const char manifest10[] =
"<assembly xmlns=\"urn:schemas-microsoft-com:asm.v1\" xmlns:asmv2=\"urn:schemas-microsoft-com:asm.v2\" manifestVersion=\"1.0\">"
"<assemblyIdentity version=\"1.0.0.0\"  name=\"Wine.Test\" type=\"win32\"></assemblyIdentity>"
"<asmv2:trustInfo>"
"    <asmv2:security>"
"        <asmv2:requestedPrivileges>"
"            <asmv2:requestedExecutionLevel level=\"requireAdministrator\" uiAccess=\"true\"></asmv2:requestedExecutionLevel>"
"        </asmv2:requestedPrivileges>"
"    </asmv2:security>"
"</asmv2:trustInfo>"
"</assembly>";

/* Empty <dependency> element */
static const char manifest11[] =
"<assembly xmlns=\"urn:schemas-microsoft-com:asm.v1\" xmlns:asmv2=\"urn:schemas-microsoft-com:asm.v2\" manifestVersion=\"1.0\">"
"<assemblyIdentity version=\"1.0.0.0\"  name=\"Wine.Test\" type=\"win32\"></assemblyIdentity>"
"<dependency />"
"</assembly>";

static const char testdep_manifest1[] =
"<assembly xmlns=\"urn:schemas-microsoft-com:asm.v1\" manifestVersion=\"1.0\">"
"<assemblyIdentity type=\"win32\" name=\"testdep\" version=\"6.5.4.3\" processorArchitecture=\"" ARCH "\"/>"
"</assembly>";

static const char testdep_manifest2[] =
"<assembly xmlns=\"urn:schemas-microsoft-com:asm.v1\" manifestVersion=\"1.0\">"
"<assemblyIdentity type=\"win32\" name=\"testdep\" version=\"6.5.4.3\" processorArchitecture=\"" ARCH "\" />"
"<file name=\"testlib.dll\"></file>"
"<file name=\"testlib2.dll\" hash=\"63c978c2b53d6cf72b42fb7308f9af12ab19ec53\" hashalg=\"SHA1\" />"
"</assembly>";

static const char testdep_manifest3[] =
"<assembly xmlns=\"urn:schemas-microsoft-com:asm.v1\" manifestVersion=\"1.0\"> "
"<assemblyIdentity type=\"win32\" name=\"testdep\" version=\"6.5.4.3\" processorArchitecture=\"" ARCH "\"/>"
"<file name=\"testlib.dll\"/>"
"<file name=\"testlib2.dll\" hash=\"63c978c2b53d6cf72b42fb7308f9af12ab19ec53\" hashalg=\"SHA1\">"
"<windowClass>wndClass</windowClass>"
"<windowClass>wndClass2</windowClass>"
"</file>"
"</assembly>";

static const char wrong_manifest1[] =
"<assembly manifestVersion=\"1.0\">"
"<assemblyIdentity version=\"1.0.0.0\"  name=\"Wine.Test\" type=\"win32\"></assemblyIdentity>"
"</assembly>";

static const char wrong_manifest2[] =
"<assembly xmlns=\"urn:schemas-microsoft-com:asm.v1\">"
"<assemblyIdentity version=\"1.0.0.0\"  name=\"Wine.Test\" type=\"win32\"></assemblyIdentity>"
"</assembly>";

static const char wrong_manifest3[] =
"<assembly test=\"test\" xmlns=\"urn:schemas-microsoft-com:asm.v1\" manifestVersion=\"1.0\">"
"<assemblyIdentity version=\"1.0.0.0\"  name=\"Wine.Test\" type=\"win32\"></assemblyIdentity>"
"</assembly>";

static const char wrong_manifest4[] =
"<assembly xmlns=\"urn:schemas-microsoft-com:asm.v1\" manifestVersion=\"1.0\">"
"<assemblyIdentity version=\"1.0.0.0\"  name=\"Wine.Test\" type=\"win32\"></assemblyIdentity>"
"<test></test>"
"</assembly>";

static const char wrong_manifest5[] =
"<assembly xmlns=\"urn:schemas-microsoft-com:asm.v1\" manifestVersion=\"1.0\">"
"<assemblyIdentity version=\"1.0.0.0\"  name=\"Wine.Test\" type=\"win32\"></assemblyIdentity>"
"</assembly>"
"<test></test>";

static const char wrong_manifest6[] =
"<assembly xmlns=\"urn:schemas-microsoft-com:asm.v5\" manifestVersion=\"1.0\">"
"<assemblyIdentity version=\"1.0.0.0\"  name=\"Wine.Test\" type=\"win32\"></assemblyIdentity>"
"</assembly>";

static const char wrong_manifest7[] =
"<assembly xmlns=\"urn:schemas-microsoft-com:asm.v1\" manifestVersion=\"1.0\">"
"<assemblyIdentity type=\"win32\" name=\"testdep\" version=\"6.5.4.3\" processorArchitecture=\"" ARCH "\" />"
"<file name=\"testlib.dll\" hash=\"63c978c2b53d6cf72b42fb7308f9af12ab19ec5\" hashalg=\"SHA1\" />"
"</assembly>";

static const char wrong_manifest8[] =
"<assembly xmlns=\"urn:schemas-microsoft-com:asm.v1\" manifestVersion=\"1.0\">"
"<assemblyIdentity version=\"1.2.3.4\"  name=\"Wine.Test\" type=\"win32\"></assemblyIdentity>"
"<file></file>"
"</assembly>";

static const char wrong_manifest9[] =
"<assembly xmlns=\"urn:schemas-microsoft-com:asm.v1\" manifestVersion=\"1.0\">"
"<assemblyIdentity version=\"1.0.0.0\"  name=\"Wine.Test\" type=\"win32\"></assemblyIdentity>"
"<trustInfo xmlns=\"urn:schemas-microsoft-com:asm.v2\">"
"    <security>"
"        <requestedPrivileges>"
"            <requestedExecutionLevel level=\"requireAdministrator\"/>"
"            <requestedExecutionLevel uiAccess=\"true\"/>"
"        </requestedPrivileges>"
"    </security>"
"</trustInfo>"
"</assembly>";

static const char wrong_manifest10[] =
"<assembly xmlns=\"urn:schemas-microsoft-com:asm.v1\" manifestVersion=\"1.0\">"
"<assemblyIdentity version=\"1.0.0.0\"  name=\"Wine.Test\" type=\"win32\"></assemblyIdentity>"
"<trustInfo xmlns=\"urn:schemas-microsoft-com:asm.v2\">"
"    <security>"
"        <requestedPrivileges>"
"            <requestedExecutionLevel level=\"requireAdministrator\"/>"
"        </requestedPrivileges>"
"    </security>"
"</trustInfo>"
"<trustInfo xmlns=\"urn:schemas-microsoft-com:asm.v2\">"
"    <security>"
"        <requestedPrivileges>"
"            <requestedExecutionLevel uiAccess=\"true\"/>"
"        </requestedPrivileges>"
"    </security>"
"</trustInfo>"
"</assembly>";

static const char wrong_depmanifest1[] =
"<assembly xmlns=\"urn:schemas-microsoft-com:asm.v1\" manifestVersion=\"1.0\">"
"<assemblyIdentity type=\"win32\" name=\"testdep\" version=\"6.5.4.4\" processorArchitecture=\"" ARCH "\" />"
"</assembly>";

static const char compat_manifest_no_supportedOs[] =
"<assembly xmlns=\"urn:schemas-microsoft-com:asm.v1\" manifestVersion=\"1.0\">"
"   <assemblyIdentity version=\"1.0.0.0\"  name=\"Wine.Test\" type=\"win32\"></assemblyIdentity>"
"   <compatibility xmlns=\"urn:schemas-microsoft-com:compatibility.v1\">"
"       <application>"
"       </application>"
"   </compatibility>"
"</assembly>";

static const char compat_manifest_vista[] =
"<assembly xmlns=\"urn:schemas-microsoft-com:asm.v1\" manifestVersion=\"1.0\">"
"   <assemblyIdentity version=\"1.0.0.0\"  name=\"Wine.Test\" type=\"win32\"></assemblyIdentity>"
"   <compatibility xmlns=\"urn:schemas-microsoft-com:compatibility.v1\">"
"       <application>"
"           <supportedOS Id=\"{e2011457-1546-43c5-a5fe-008deee3d3f0}\" />"  /* Windows Vista */
"       </application>"
"   </compatibility>"
"</assembly>";

static const char compat_manifest_vista_7_8_10_81[] =
"<assembly xmlns=\"urn:schemas-microsoft-com:asm.v1\" manifestVersion=\"1.0\">"
"   <assemblyIdentity version=\"1.0.0.0\"  name=\"Wine.Test\" type=\"win32\"></assemblyIdentity>"
"   <compatibility xmlns=\"urn:schemas-microsoft-com:compatibility.v1\">"
"       <application>"
"           <supportedOS Id=\"{e2011457-1546-43c5-a5fe-008deee3d3f0}\" ></supportedOS>"  /* Windows Vista */
"           <supportedOS Id=\"{35138b9a-5d96-4fbd-8e2d-a2440225f93a}\" />"  /* Windows 7 */
"           <supportedOS Id=\"{4a2f28e3-53b9-4441-ba9c-d69d4a4a6e38}\" ></supportedOS>"  /* Windows 8 */
"           <supportedOS Id=\"{8e0f7a12-bfb3-4fe8-b9a5-48fd50a15a9a}\" />"  /* Windows 10 */
"           <supportedOS Id=\"{1f676c76-80e1-4239-95bb-83d0f6d0da78}\" />"  /* Windows 8.1 */
"       </application>"
"   </compatibility>"
"</assembly>";

static const char compat_manifest_other_guid[] =
"<assembly xmlns=\"urn:schemas-microsoft-com:asm.v1\" manifestVersion=\"1.0\">"
"   <assemblyIdentity version=\"1.0.0.0\"  name=\"Wine.Test\" type=\"win32\"></assemblyIdentity>"
"   <compatibility xmlns=\"urn:schemas-microsoft-com:compatibility.v1\">"
"       <application>"
"           <supportedOS Id=\"{12345566-1111-2222-3333-444444444444}\" />"
"       </application>"
"   </compatibility>"
"</assembly>";

static const char settings_manifest[] =
"<assembly xmlns=\"urn:schemas-microsoft-com:asm.v1\" manifestVersion=\"1.0\">"
"   <assemblyIdentity version=\"1.0.0.0\"  name=\"Wine.Test\" type=\"win32\"></assemblyIdentity>"
"   <application xmlns=\"urn:schemas-microsoft-com:asm.v3\">"
"       <windowsSettings>"
"           <dpiAware xmlns=\"http://schemas.microsoft.com/SMI/2005/WindowsSettings\">true</dpiAware>"
"           <dpiAwareness xmlns=\"http://schemas.microsoft.com/SMI/2016/WindowsSettings\">true</dpiAwareness>"
"       </windowsSettings>"
"   </application>"
"</assembly>";

static const char settings_manifest2[] =
"<assembly xmlns=\"urn:schemas-microsoft-com:asm.v1\" manifestVersion=\"1.0\">"
"   <assemblyIdentity version=\"1.0.0.0\"  name=\"Wine.Test\" type=\"win32\"></assemblyIdentity>"
"   <application xmlns=\"urn:schemas-microsoft-com:asm.v3\""
"                xmlns:ws05=\"http://schemas.microsoft.com/SMI/2005/WindowsSettings\""
"                xmlns:ws11=\"http://schemas.microsoft.com/SMI/2011/WindowsSettings\""
"                xmlns:ws16=\"http://schemas.microsoft.com/SMI/2016/WindowsSettings\""
"                xmlns:ws17=\"http://schemas.microsoft.com/SMI/2017/WindowsSettings\">"
"       <windowsSettings>"
"           <ws05:autoElevate>true</ws05:autoElevate>"
"           <ws05:disableTheming>true</ws05:disableTheming>"
"           <ws11:disableWindowFiltering>true</ws11:disableWindowFiltering>"
"           <ws05:dpiAware>true</ws05:dpiAware>"
"           <ws16:dpiAwareness>true</ws16:dpiAwareness>"
"           <ws17:gdiScaling>true</ws17:gdiScaling>"
"           <ws17:highResolutionScrollingAware>true</ws17:highResolutionScrollingAware>"
"           <ws16:longPathAware>true</ws16:longPathAware>"
"           <ws17:magicFutureSetting>true</ws17:magicFutureSetting>"
"           <ws11:printerDriverIsolation>true</ws11:printerDriverIsolation>"
"           <ws17:ultraHighResolutionScrollingAware>true</ws17:ultraHighResolutionScrollingAware>"
"       </windowsSettings>"
"   </application>"
"</assembly>";

/* broken manifest found in some binaries: asmv3 namespace is used but not declared */
static const char settings_manifest3[] =
"<assembly xmlns=\"urn:schemas-microsoft-com:asm.v1\" manifestVersion=\"1.0\">"
"   <assemblyIdentity version=\"1.0.0.0\"  name=\"Wine.Test\" type=\"win32\"></assemblyIdentity>"
"   <asmv3:application>"
"     <asmv3:windowsSettings xmlns=\"http://schemas.microsoft.com/SMI/2005/WindowsSettings\">"
"       <dpiAware>true</dpiAware>"
"     </asmv3:windowsSettings>"
"   </asmv3:application>"
"</assembly>";

static const char two_dll_manifest_dll[] =
"<assembly xmlns=\"urn:schemas-microsoft-com:asm.v3\" manifestVersion=\"1.0\">"
"  <assemblyIdentity type=\"win32\" name=\"sxs_dll\" version=\"1.0.0.0\" processorArchitecture=\"x86\" publicKeyToken=\"0000000000000000\"/>"
"  <file name=\"sxs_dll.dll\"></file>"
"</assembly>";

static const char two_dll_manifest_exe[] =
"<assembly xmlns=\"urn:schemas-microsoft-com:asm.v1\" manifestVersion=\"1.0\">"
"  <dependency>"
"    <dependentAssembly>"
"      <assemblyIdentity type=\"win32\" name=\"sxs_dll\" version=\"1.0.0.0\" processorArchitecture=\"x86\" publicKeyToken=\"0000000000000000\" language=\"*\"/>"
"    </dependentAssembly>"
"  </dependency>"
"</assembly>";

static const char builtin_dll_manifest[] =
"<assembly xmlns=\"urn:schemas-microsoft-com:asm.v1\" manifestVersion=\"1.0\">"
"   <dependency>"
"     <dependentAssembly>"
"       <assemblyIdentity"
"           type=\"win32\""
"           name=\"microsoft.vc90.crt\""
"           version=\"9.0.20718.0\""
"           processorArchitecture=\"*\""
"           publicKeyToken=\"1fc8b3b9a1e18e3b\""
"           language=\"*\""
"       />"
"   </dependentAssembly>"
"   </dependency>"
"</assembly>";


DEFINE_GUID(VISTA_COMPAT_GUID,      0xe2011457, 0x1546, 0x43c5, 0xa5, 0xfe, 0x00, 0x8d, 0xee, 0xe3, 0xd3, 0xf0);
DEFINE_GUID(WIN7_COMPAT_GUID,       0x35138b9a, 0x5d96, 0x4fbd, 0x8e, 0x2d, 0xa2, 0x44, 0x02, 0x25, 0xf9, 0x3a);
DEFINE_GUID(WIN8_COMPAT_GUID,       0x4a2f28e3, 0x53b9, 0x4441, 0xba, 0x9c, 0xd6, 0x9d, 0x4a, 0x4a, 0x6e, 0x38);
DEFINE_GUID(WIN81_COMPAT_GUID,      0x1f676c76, 0x80e1, 0x4239, 0x95, 0xbb, 0x83, 0xd0, 0xf6, 0xd0, 0xda, 0x78);
DEFINE_GUID(WIN10_COMPAT_GUID,      0x8e0f7a12, 0xbfb3, 0x4fe8, 0xb9, 0xa5, 0x48, 0xfd, 0x50, 0xa1, 0x5a, 0x9a);
DEFINE_GUID(OTHER_COMPAT_GUID,      0x12345566, 0x1111, 0x2222, 0x33, 0x33, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44);


static const WCHAR testlib_dll[] =
    {'t','e','s','t','l','i','b','.','d','l','l',0};
static const WCHAR testlib2_dll[] =
    {'t','e','s','t','l','i','b','2','.','d','l','l',0};
static const WCHAR wndClassW[] =
    {'w','n','d','C','l','a','s','s',0};
static const WCHAR wndClass1W[] =
    {'w','n','d','C','l','a','s','s','1',0};
static const WCHAR wndClass2W[] =
    {'w','n','d','C','l','a','s','s','2',0};
static const WCHAR wndClass3W[] =
    {'w','n','d','C','l','a','s','s','3',0};

static WCHAR app_dir[MAX_PATH], exe_path[MAX_PATH], work_dir[MAX_PATH], work_dir_subdir[MAX_PATH];
static WCHAR app_manifest_path[MAX_PATH], manifest_path[MAX_PATH], depmanifest_path[MAX_PATH];

static BOOL create_manifest_file(const char *filename, const char *manifest, int manifest_len,
                                 const char *depfile, const char *depmanifest)
{
    DWORD size;
    HANDLE file;
    WCHAR path[MAX_PATH];

    MultiByteToWideChar( CP_ACP, 0, filename, -1, path, MAX_PATH );
    GetFullPathNameW(path, ARRAY_SIZE(manifest_path), manifest_path, NULL);

    if (manifest_len == -1)
        manifest_len = strlen(manifest);

    file = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                       FILE_ATTRIBUTE_NORMAL, NULL);
    ok(file != INVALID_HANDLE_VALUE, "CreateFile failed: %u\n", GetLastError());
    if(file == INVALID_HANDLE_VALUE)
        return FALSE;
    WriteFile(file, manifest, manifest_len, &size, NULL);
    CloseHandle(file);

    if (depmanifest)
    {
        MultiByteToWideChar( CP_ACP, 0, depfile, -1, path, MAX_PATH );
        GetFullPathNameW(path, ARRAY_SIZE(depmanifest_path), depmanifest_path, NULL);
        file = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, NULL);
        ok(file != INVALID_HANDLE_VALUE, "CreateFile failed: %u\n", GetLastError());
        if(file == INVALID_HANDLE_VALUE)
            return FALSE;
        WriteFile(file, depmanifest, strlen(depmanifest), &size, NULL);
        CloseHandle(file);
    }
    return TRUE;
}

static BOOL create_wide_manifest(const char *filename, const char *manifest, BOOL fBOM, BOOL fReverse)
{
    WCHAR *wmanifest = HeapAlloc(GetProcessHeap(), 0, (strlen(manifest)+2) * sizeof(WCHAR));
    BOOL ret;
    int offset = (fBOM ? 0 : 1);

    MultiByteToWideChar(CP_ACP, 0, manifest, -1, &wmanifest[1], (strlen(manifest)+1));
    wmanifest[0] = 0xfeff;
    if (fReverse)
    {
        size_t i;
        for (i = 0; i < strlen(manifest)+1; i++)
            wmanifest[i] = (wmanifest[i] << 8) | ((wmanifest[i] >> 8) & 0xff);
    }
    ret = create_manifest_file(filename, (char *)&wmanifest[offset], (strlen(manifest)+1-offset) * sizeof(WCHAR), NULL, NULL);
    HeapFree(GetProcessHeap(), 0, wmanifest);
    return ret;
}

typedef struct {
    ULONG format_version;
    ULONG assembly_cnt_min;
    ULONG assembly_cnt_max;
    ULONG root_manifest_type;
    LPWSTR root_manifest_path;
    ULONG root_config_type;
    ULONG app_dir_type;
    LPCWSTR app_dir;
} detailed_info_t;

static const detailed_info_t detailed_info0 = {
    0, 0, 0, 0, NULL, 0, 0, NULL
};

static const detailed_info_t detailed_info1 = {
    1, 1, 1, ACTIVATION_CONTEXT_PATH_TYPE_WIN32_FILE, manifest_path,
    ACTIVATION_CONTEXT_PATH_TYPE_NONE, ACTIVATION_CONTEXT_PATH_TYPE_WIN32_FILE,
    app_dir,
};

static const detailed_info_t detailed_info1_child = {
    1, 1, 1, ACTIVATION_CONTEXT_PATH_TYPE_WIN32_FILE, app_manifest_path,
    ACTIVATION_CONTEXT_PATH_TYPE_NONE, ACTIVATION_CONTEXT_PATH_TYPE_WIN32_FILE,
    app_dir,
};

/* On Vista+, there's an extra assembly for Microsoft.Windows.Common-Controls.Resources */
static const detailed_info_t detailed_info2 = {
    1, 2, 3, ACTIVATION_CONTEXT_PATH_TYPE_WIN32_FILE, manifest_path,
    ACTIVATION_CONTEXT_PATH_TYPE_NONE, ACTIVATION_CONTEXT_PATH_TYPE_WIN32_FILE,
    app_dir,
};

static void test_detailed_info(HANDLE handle, const detailed_info_t *exinfo, int line)
{
    ACTIVATION_CONTEXT_DETAILED_INFORMATION detailed_info_tmp, *detailed_info;
    SIZE_T size, exsize, retsize;
    BOOL b;

    exsize = sizeof(ACTIVATION_CONTEXT_DETAILED_INFORMATION)
        + (exinfo->root_manifest_path ? (lstrlenW(exinfo->root_manifest_path)+1)*sizeof(WCHAR):0)
        + (exinfo->app_dir ? (lstrlenW(exinfo->app_dir)+1)*sizeof(WCHAR) : 0);

    if(exsize != sizeof(ACTIVATION_CONTEXT_DETAILED_INFORMATION)) {
        size = 0xdeadbeef;
        b = QueryActCtxW(0, handle, NULL, ActivationContextDetailedInformation, &detailed_info_tmp,
                sizeof(detailed_info_tmp), &size);
        ok_(__FILE__, line)(!b, "QueryActCtx succeeded\n");
        ok_(__FILE__, line)(GetLastError() == ERROR_INSUFFICIENT_BUFFER, "GetLastError() = %u\n", GetLastError());
        ok_(__FILE__, line)(size == exsize, "size=%ld, expected %ld\n", size, exsize);
    }else {
        size = sizeof(ACTIVATION_CONTEXT_DETAILED_INFORMATION);
    }

    detailed_info = HeapAlloc(GetProcessHeap(), 0, size);
    memset(detailed_info, 0xfe, size);
    b = QueryActCtxW(0, handle, NULL, ActivationContextDetailedInformation, detailed_info, size, &retsize);
    ok_(__FILE__, line)(b, "QueryActCtx failed: %u\n", GetLastError());
    ok_(__FILE__, line)(retsize == exsize, "size=%ld, expected %ld\n", retsize, exsize);

    ok_(__FILE__, line)(detailed_info->dwFlags == 0, "detailed_info->dwFlags=%x\n", detailed_info->dwFlags);
    ok_(__FILE__, line)(detailed_info->ulFormatVersion == exinfo->format_version,
       "detailed_info->ulFormatVersion=%u, expected %u\n", detailed_info->ulFormatVersion,
       exinfo->format_version);
    ok_(__FILE__, line)(exinfo->assembly_cnt_min <= detailed_info->ulAssemblyCount &&
       detailed_info->ulAssemblyCount <= exinfo->assembly_cnt_max,
       "detailed_info->ulAssemblyCount=%u, expected between %u and %u\n", detailed_info->ulAssemblyCount,
       exinfo->assembly_cnt_min, exinfo->assembly_cnt_max);
    ok_(__FILE__, line)(detailed_info->ulRootManifestPathType == exinfo->root_manifest_type,
       "detailed_info->ulRootManifestPathType=%u, expected %u\n",
       detailed_info->ulRootManifestPathType, exinfo->root_manifest_type);
    ok_(__FILE__, line)(detailed_info->ulRootManifestPathChars ==
       (exinfo->root_manifest_path ? lstrlenW(exinfo->root_manifest_path) : 0),
       "detailed_info->ulRootManifestPathChars=%u, expected %u\n",
       detailed_info->ulRootManifestPathChars,
       exinfo->root_manifest_path ?lstrlenW(exinfo->root_manifest_path) : 0);
    ok_(__FILE__, line)(detailed_info->ulRootConfigurationPathType == exinfo->root_config_type,
       "detailed_info->ulRootConfigurationPathType=%u, expected %u\n",
       detailed_info->ulRootConfigurationPathType, exinfo->root_config_type);
    ok_(__FILE__, line)(detailed_info->ulRootConfigurationPathChars == 0,
       "detailed_info->ulRootConfigurationPathChars=%d\n", detailed_info->ulRootConfigurationPathChars);
    ok_(__FILE__, line)(detailed_info->ulAppDirPathType == exinfo->app_dir_type,
       "detailed_info->ulAppDirPathType=%u, expected %u\n", detailed_info->ulAppDirPathType,
       exinfo->app_dir_type);
    ok_(__FILE__, line)(detailed_info->ulAppDirPathChars == (exinfo->app_dir ? lstrlenW(exinfo->app_dir) : 0),
       "detailed_info->ulAppDirPathChars=%u, expected %u\n",
       detailed_info->ulAppDirPathChars, exinfo->app_dir ? lstrlenW(exinfo->app_dir) : 0);
    if(exinfo->root_manifest_path) {
        ok_(__FILE__, line)(detailed_info->lpRootManifestPath != NULL, "detailed_info->lpRootManifestPath == NULL\n");
        if(detailed_info->lpRootManifestPath)
            ok_(__FILE__, line)(!lstrcmpiW(detailed_info->lpRootManifestPath, exinfo->root_manifest_path),
               "unexpected detailed_info->lpRootManifestPath\n");
    }else {
        ok_(__FILE__, line)(detailed_info->lpRootManifestPath == NULL, "detailed_info->lpRootManifestPath != NULL\n");
    }
    ok_(__FILE__, line)(detailed_info->lpRootConfigurationPath == NULL,
       "detailed_info->lpRootConfigurationPath=%p\n", detailed_info->lpRootConfigurationPath);
    if(exinfo->app_dir) {
        ok_(__FILE__, line)(detailed_info->lpAppDirPath != NULL, "detailed_info->lpAppDirPath == NULL\n");
        if(detailed_info->lpAppDirPath)
            ok_(__FILE__, line)(!lstrcmpiW(exinfo->app_dir, detailed_info->lpAppDirPath),
                                "unexpected detailed_info->lpAppDirPath %s / %s\n",
                                wine_dbgstr_w(detailed_info->lpAppDirPath), wine_dbgstr_w( exinfo->app_dir ));
    }else {
        ok_(__FILE__, line)(detailed_info->lpAppDirPath == NULL, "detailed_info->lpAppDirPath != NULL\n");
    }

    HeapFree(GetProcessHeap(), 0, detailed_info);
}

typedef struct {
    ULONG flags;
/*    ULONG manifest_path_type; FIXME */
    LPCWSTR manifest_path;
    LPCWSTR encoded_assembly_id;
    BOOL has_assembly_dir;
} info_in_assembly;

static const info_in_assembly manifest1_info = {
    1, manifest_path,
    L"Wine.Test,type=\"win32\",version=\"1.0.0.0\"",
    FALSE
};

static const info_in_assembly manifest1_child_info = {
    1, app_manifest_path,
    L"Wine.Test,type=\"win32\",version=\"1.0.0.0\"",
    FALSE
};

static const info_in_assembly manifest2_info = {
    1, manifest_path,
    L"Wine.Test,type=\"win32\",version=\"1.2.3.4\"",
    FALSE
};

static const info_in_assembly manifest3_info = {
    1, manifest_path,
    L"Wine.Test,publicKeyToken=\"6595b6414666f1df\",type=\"win32\",version=\"1.2.3.4\"",
    FALSE
};

static const info_in_assembly manifest4_info = {
    1, manifest_path,
    L"Wine.Test,type=\"win32\",version=\"1.2.3.4\"",
    FALSE
};

static const info_in_assembly depmanifest1_info = {
    0x10, depmanifest_path,
    L"testdep,processorArchitecture=\"" ARCH "\","
    "type=\"win32\",version=\"6.5.4.3\"",
    TRUE
};

static const info_in_assembly depmanifest2_info = {
    0x10, depmanifest_path,
    L"testdep,processorArchitecture=\"" ARCH "\","
    "type=\"win32\",version=\"6.5.4.3\"",
    TRUE
};

static const info_in_assembly depmanifest3_info = {
    0x10, depmanifest_path,
    L"testdep,processorArchitecture=\"" ARCH "\",type=\"win32\",version=\"6.5.4.3\"",
    TRUE
};

static const info_in_assembly manifest_comctrl_info = {
    0, NULL, NULL, TRUE /* These values may differ between Windows installations */
};

static void test_info_in_assembly(HANDLE handle, DWORD id, const info_in_assembly *exinfo, int line)
{
    ACTIVATION_CONTEXT_ASSEMBLY_DETAILED_INFORMATION *info, info_tmp;
    SIZE_T size, exsize;
    ULONG len;
    BOOL b;

    exsize = sizeof(ACTIVATION_CONTEXT_ASSEMBLY_DETAILED_INFORMATION);
    if (exinfo->manifest_path) exsize += (lstrlenW(exinfo->manifest_path)+1) * sizeof(WCHAR);
    if (exinfo->encoded_assembly_id) exsize += (lstrlenW(exinfo->encoded_assembly_id) + 1) * sizeof(WCHAR);

    size = 0xdeadbeef;
    b = QueryActCtxW(0, handle, &id, AssemblyDetailedInformationInActivationContext, &info_tmp, sizeof(info_tmp), &size);
    ok_(__FILE__, line)(!b, "QueryActCtx succeeded\n");
    ok_(__FILE__, line)(GetLastError() == ERROR_INSUFFICIENT_BUFFER, "GetLastError() = %u\n", GetLastError());

    ok_(__FILE__, line)(size >= exsize, "size=%lu, expected %lu\n", size, exsize);

    if (size == 0xdeadbeef)
    {
        skip("bad size\n");
        return;
    }

    info = HeapAlloc(GetProcessHeap(), 0, size);
    memset(info, 0xfe, size);

    size = 0xdeadbeef;
    b = QueryActCtxW(0, handle, &id, AssemblyDetailedInformationInActivationContext, info, size, &size);
    ok_(__FILE__, line)(b, "QueryActCtx failed: %u\n", GetLastError());
    if (!exinfo->manifest_path)
        exsize += info->ulManifestPathLength + sizeof(WCHAR);
    if (!exinfo->encoded_assembly_id)
        exsize += info->ulEncodedAssemblyIdentityLength + sizeof(WCHAR);
    if (exinfo->has_assembly_dir)
        exsize += info->ulAssemblyDirectoryNameLength + sizeof(WCHAR);
    ok_(__FILE__, line)(size == exsize, "size=%lu, expected %lu\n", size, exsize);

    if (0)  /* FIXME: flags meaning unknown */
    {
        ok_(__FILE__, line)((info->ulFlags) == exinfo->flags, "info->ulFlags = %x, expected %x\n",
           info->ulFlags, exinfo->flags);
    }
    if(exinfo->encoded_assembly_id) {
        len = lstrlenW(exinfo->encoded_assembly_id)*sizeof(WCHAR);
        ok_(__FILE__, line)(info->ulEncodedAssemblyIdentityLength == len,
           "info->ulEncodedAssemblyIdentityLength = %u, expected %u\n",
           info->ulEncodedAssemblyIdentityLength, len);
    } else {
        ok_(__FILE__, line)(info->ulEncodedAssemblyIdentityLength != 0,
           "info->ulEncodedAssemblyIdentityLength == 0\n");
    }
    ok_(__FILE__, line)(info->ulManifestPathType == ACTIVATION_CONTEXT_PATH_TYPE_WIN32_FILE,
       "info->ulManifestPathType = %x\n", info->ulManifestPathType);
    if(exinfo->manifest_path) {
        len = lstrlenW(exinfo->manifest_path)*sizeof(WCHAR);
        ok_(__FILE__, line)(info->ulManifestPathLength == len, "info->ulManifestPathLength = %u, expected %u\n",
           info->ulManifestPathLength, len);
    } else {
        ok_(__FILE__, line)(info->ulManifestPathLength != 0, "info->ulManifestPathLength == 0\n");
    }

    ok_(__FILE__, line)(info->ulPolicyPathType == ACTIVATION_CONTEXT_PATH_TYPE_NONE,
       "info->ulPolicyPathType = %x\n", info->ulPolicyPathType);
    ok_(__FILE__, line)(info->ulPolicyPathLength == 0,
       "info->ulPolicyPathLength = %u, expected 0\n", info->ulPolicyPathLength);
    ok_(__FILE__, line)(info->ulMetadataSatelliteRosterIndex == 0, "info->ulMetadataSatelliteRosterIndex = %x\n",
       info->ulMetadataSatelliteRosterIndex);
    ok_(__FILE__, line)(info->ulManifestVersionMajor == 1,"info->ulManifestVersionMajor = %x\n",
       info->ulManifestVersionMajor);
    ok_(__FILE__, line)(info->ulManifestVersionMinor == 0, "info->ulManifestVersionMinor = %x\n",
       info->ulManifestVersionMinor);
    ok_(__FILE__, line)(info->ulPolicyVersionMajor == 0, "info->ulPolicyVersionMajor = %x\n",
       info->ulPolicyVersionMajor);
    ok_(__FILE__, line)(info->ulPolicyVersionMinor == 0, "info->ulPolicyVersionMinor = %x\n",
       info->ulPolicyVersionMinor);
    if(exinfo->has_assembly_dir)
        ok_(__FILE__, line)(info->ulAssemblyDirectoryNameLength != 0,
           "info->ulAssemblyDirectoryNameLength == 0\n");
    else
        ok_(__FILE__, line)(info->ulAssemblyDirectoryNameLength == 0,
           "info->ulAssemblyDirectoryNameLength != 0\n");

    ok_(__FILE__, line)(info->lpAssemblyEncodedAssemblyIdentity != NULL,
       "info->lpAssemblyEncodedAssemblyIdentity == NULL\n");
    if(info->lpAssemblyEncodedAssemblyIdentity && exinfo->encoded_assembly_id) {
        ok_(__FILE__, line)(!lstrcmpW(info->lpAssemblyEncodedAssemblyIdentity, exinfo->encoded_assembly_id),
           "unexpected info->lpAssemblyEncodedAssemblyIdentity %s / %s\n",
           wine_dbgstr_w(info->lpAssemblyEncodedAssemblyIdentity), wine_dbgstr_w(exinfo->encoded_assembly_id));
    }
    if(exinfo->manifest_path) {
        ok_(__FILE__, line)(info->lpAssemblyManifestPath != NULL, "info->lpAssemblyManifestPath == NULL\n");
        if(info->lpAssemblyManifestPath)
            ok_(__FILE__, line)(!lstrcmpiW(info->lpAssemblyManifestPath, exinfo->manifest_path),
               "unexpected info->lpAssemblyManifestPath\n");
    }else {
        ok_(__FILE__, line)(info->lpAssemblyManifestPath != NULL, "info->lpAssemblyManifestPath == NULL\n");
    }

    ok_(__FILE__, line)(info->lpAssemblyPolicyPath == NULL, "info->lpAssemblyPolicyPath != NULL\n");
    if(info->lpAssemblyPolicyPath)
        ok_(__FILE__, line)(*(WORD*)info->lpAssemblyPolicyPath == 0, "info->lpAssemblyPolicyPath is not empty\n");
    if(exinfo->has_assembly_dir)
        ok_(__FILE__, line)(info->lpAssemblyDirectoryName != NULL, "info->lpAssemblyDirectoryName == NULL\n");
    else
        ok_(__FILE__, line)(info->lpAssemblyDirectoryName == NULL, "info->lpAssemblyDirectoryName = %s\n",
           wine_dbgstr_w(info->lpAssemblyDirectoryName));
    HeapFree(GetProcessHeap(), 0, info);
}

static void test_file_info(HANDLE handle, ULONG assid, ULONG fileid, LPCWSTR filename, int line)
{
    ASSEMBLY_FILE_DETAILED_INFORMATION *info, info_tmp;
    ACTIVATION_CONTEXT_QUERY_INDEX index = {assid, fileid};
    SIZE_T size, exsize;
    BOOL b;

    exsize = sizeof(ASSEMBLY_FILE_DETAILED_INFORMATION)
        +(lstrlenW(filename)+1)*sizeof(WCHAR);

    size = 0xdeadbeef;
    b = QueryActCtxW(0, handle, &index, FileInformationInAssemblyOfAssemblyInActivationContext, &info_tmp,
                      sizeof(info_tmp), &size);
    ok_(__FILE__, line)(!b, "QueryActCtx succeeded\n");
    ok_(__FILE__, line)(GetLastError() == ERROR_INSUFFICIENT_BUFFER, "GetLastError() = %u\n", GetLastError());
    ok_(__FILE__, line)(size == exsize, "size=%lu, expected %lu\n", size, exsize);

    if(size == 0xdeadbeef)
    {
        skip("bad size\n");
        return;
    }

    info = HeapAlloc(GetProcessHeap(), 0, size);
    memset(info, 0xfe, size);

    b = QueryActCtxW(0, handle, &index, FileInformationInAssemblyOfAssemblyInActivationContext, info, size, &size);
    ok_(__FILE__, line)(b, "QueryActCtx failed: %u\n", GetLastError());
    ok_(__FILE__, line)(!size, "size=%lu, expected 0\n", size);

    ok_(__FILE__, line)(info->ulFlags == 2, "info->ulFlags=%x, expected 2\n", info->ulFlags);
    ok_(__FILE__, line)(info->ulFilenameLength == lstrlenW(filename)*sizeof(WCHAR),
       "info->ulFilenameLength=%u, expected %u*sizeof(WCHAR)\n",
       info->ulFilenameLength, lstrlenW(filename));
    ok_(__FILE__, line)(info->ulPathLength == 0, "info->ulPathLength=%u\n", info->ulPathLength);
    ok_(__FILE__, line)(info->lpFileName != NULL, "info->lpFileName == NULL\n");
    if(info->lpFileName)
        ok_(__FILE__, line)(!lstrcmpiW(info->lpFileName, filename), "unexpected info->lpFileName\n");
    ok_(__FILE__, line)(info->lpFilePath == NULL, "info->lpFilePath != NULL\n");
    HeapFree(GetProcessHeap(), 0, info);
}

typedef struct {
    ACTCTX_REQUESTED_RUN_LEVEL run_level;
    DWORD ui_access;
} runlevel_info_t;

static const runlevel_info_t runlevel_info0 = {
    ACTCTX_RUN_LEVEL_UNSPECIFIED, FALSE,
};

static const runlevel_info_t runlevel_info6 = {
    ACTCTX_RUN_LEVEL_AS_INVOKER, FALSE,
};

static const runlevel_info_t runlevel_info7 = {
    ACTCTX_RUN_LEVEL_REQUIRE_ADMIN, TRUE,
};

static const runlevel_info_t runlevel_info8 = {
    ACTCTX_RUN_LEVEL_REQUIRE_ADMIN, TRUE,
};

static const runlevel_info_t runlevel_info9 = {
    ACTCTX_RUN_LEVEL_REQUIRE_ADMIN, FALSE,
};

static void test_runlevel_info(HANDLE handle, const runlevel_info_t *exinfo, int line)
{
    ACTIVATION_CONTEXT_RUN_LEVEL_INFORMATION runlevel_info;
    SIZE_T size, retsize;
    BOOL b;

    size = sizeof(runlevel_info);
    b = QueryActCtxW(0, handle, NULL, RunlevelInformationInActivationContext, &runlevel_info,
                      sizeof(runlevel_info), &retsize);
    if (!b && GetLastError() == ERROR_INVALID_PARAMETER)
    {
        win_skip("RunlevelInformationInActivationContext not supported.\n");
        return;
    }

    ok_(__FILE__, line)(b, "QueryActCtx failed: %u\n", GetLastError());
    ok_(__FILE__, line)(retsize == size, "size=%ld, expected %ld\n", retsize, size);

    ok_(__FILE__, line)(runlevel_info.ulFlags == 0, "runlevel_info.ulFlags=%x\n", runlevel_info.ulFlags);
    ok_(__FILE__, line)(runlevel_info.RunLevel == exinfo->run_level,
       "runlevel_info.RunLevel=%u, expected %u\n", runlevel_info.RunLevel, exinfo->run_level);
    ok_(__FILE__, line)(runlevel_info.UiAccess == exinfo->ui_access,
       "runlevel_info.UiAccess=%u, expected %u\n", runlevel_info.UiAccess, exinfo->ui_access);
}

static HANDLE test_create(const char *file)
{
    ACTCTXW actctx;
    HANDLE handle;
    WCHAR path[MAX_PATH];

    MultiByteToWideChar( CP_ACP, 0, file, -1, path, MAX_PATH );
    memset(&actctx, 0, sizeof(ACTCTXW));
    actctx.cbSize = sizeof(ACTCTXW);
    actctx.lpSource = path;

    handle = CreateActCtxW(&actctx);
    /* to be tested outside of this helper, including last error */
    if (handle == INVALID_HANDLE_VALUE) return handle;

    ok(actctx.cbSize == sizeof(actctx), "actctx.cbSize=%d\n", actctx.cbSize);
    ok(actctx.dwFlags == 0, "actctx.dwFlags=%d\n", actctx.dwFlags);
    ok(actctx.lpSource == path, "actctx.lpSource=%p\n", actctx.lpSource);
    ok(actctx.wProcessorArchitecture == 0,
       "actctx.wProcessorArchitecture=%d\n", actctx.wProcessorArchitecture);
    ok(actctx.wLangId == 0, "actctx.wLangId=%d\n", actctx.wLangId);
    ok(actctx.lpAssemblyDirectory == NULL,
       "actctx.lpAssemblyDirectory=%p\n", actctx.lpAssemblyDirectory);
    ok(actctx.lpResourceName == NULL, "actctx.lpResourceName=%p\n", actctx.lpResourceName);
    ok(actctx.lpApplicationName == NULL, "actctx.lpApplicationName=%p\n",
       actctx.lpApplicationName);
    ok(actctx.hModule == NULL, "actctx.hModule=%p\n", actctx.hModule);

    return handle;
}

static void test_create_and_fail(const char *manifest, const char *depmanifest, int todo, BOOL is_broken)
{
    ACTCTXW actctx;
    HANDLE handle;
    WCHAR path[MAX_PATH];

    MultiByteToWideChar( CP_ACP, 0, "bad.manifest", -1, path, MAX_PATH );
    memset(&actctx, 0, sizeof(ACTCTXW));
    actctx.cbSize = sizeof(ACTCTXW);
    actctx.lpSource = path;

    create_manifest_file("bad.manifest", manifest, -1, "testdep.manifest", depmanifest);
    handle = CreateActCtxW(&actctx);
    todo_wine_if(todo)
    {
        if (is_broken)
            ok(broken(handle != INVALID_HANDLE_VALUE) || handle == INVALID_HANDLE_VALUE,
                "Unexpected context handle %p.\n", handle);
        else
            ok(handle == INVALID_HANDLE_VALUE, "Unexpected context handle %p.\n", handle);

        if (handle == INVALID_HANDLE_VALUE)
            ok(GetLastError() == ERROR_SXS_CANT_GEN_ACTCTX, "Unexpected error %d.\n", GetLastError());
    }
    if (handle != INVALID_HANDLE_VALUE) ReleaseActCtx( handle );
    DeleteFileA("bad.manifest");
    DeleteFileA("testdep.manifest");
}

static void test_create_wide_and_fail(const char *manifest, BOOL fBOM)
{
    ACTCTXW actctx;
    HANDLE handle;
    WCHAR path[MAX_PATH];

    MultiByteToWideChar( CP_ACP, 0, "bad.manifest", -1, path, MAX_PATH );
    memset(&actctx, 0, sizeof(ACTCTXW));
    actctx.cbSize = sizeof(ACTCTXW);
    actctx.lpSource = path;

    create_wide_manifest("bad.manifest", manifest, fBOM, FALSE);
    handle = CreateActCtxW(&actctx);
    ok(handle == INVALID_HANDLE_VALUE, "handle != INVALID_HANDLE_VALUE\n");
    ok(GetLastError() == ERROR_SXS_CANT_GEN_ACTCTX, "GetLastError == %u\n", GetLastError());

    if (handle != INVALID_HANDLE_VALUE) ReleaseActCtx( handle );
    DeleteFileA("bad.manifest");
}

static void test_create_fail(void)
{
    ACTCTXW actctx;
    HANDLE handle;
    WCHAR path[MAX_PATH];

    MultiByteToWideChar( CP_ACP, 0, "nonexistent.manifest", -1, path, MAX_PATH );
    memset(&actctx, 0, sizeof(ACTCTXW));
    actctx.cbSize = sizeof(ACTCTXW);
    actctx.lpSource = path;

    handle = CreateActCtxW(&actctx);
    ok(handle == INVALID_HANDLE_VALUE, "handle != INVALID_HANDLE_VALUE\n");
    ok(GetLastError() == ERROR_FILE_NOT_FOUND, "GetLastError == %u\n", GetLastError());

    trace("wrong_manifest1\n");
    test_create_and_fail(wrong_manifest1, NULL, 0, FALSE);
    trace("wrong_manifest2\n");
    test_create_and_fail(wrong_manifest2, NULL, 0, FALSE);
    trace("wrong_manifest3\n");
    test_create_and_fail(wrong_manifest3, NULL, 1, FALSE);
    trace("wrong_manifest4\n");
    test_create_and_fail(wrong_manifest4, NULL, 1, FALSE);
    trace("wrong_manifest5\n");
    test_create_and_fail(wrong_manifest5, NULL, 0, FALSE);
    trace("wrong_manifest6\n");
    test_create_and_fail(wrong_manifest6, NULL, 0, FALSE);
    trace("wrong_manifest7\n");
    test_create_and_fail(wrong_manifest7, NULL, 1, FALSE);
    trace("wrong_manifest8\n");
    test_create_and_fail(wrong_manifest8, NULL, 0, FALSE);
    trace("wrong_manifest9\n");
    test_create_and_fail(wrong_manifest9, NULL, 0, TRUE /* WinXP */);
    trace("wrong_manifest10\n");
    test_create_and_fail(wrong_manifest10, NULL, 0, TRUE /* WinXP */);
    trace("UTF-16 manifest1 without BOM\n");
    test_create_wide_and_fail(manifest1, FALSE );
    trace("manifest2\n");
    test_create_and_fail(manifest2, NULL, 0, FALSE);
    trace("manifest2+depmanifest1\n");
    test_create_and_fail(manifest2, wrong_depmanifest1, 0, FALSE);
}

struct strsection_header
{
    DWORD magic;
    ULONG size;
    DWORD unk1[3];
    ULONG count;
    ULONG index_offset;
    DWORD unk2[2];
    ULONG global_offset;
    ULONG global_len;
};

struct string_index
{
    ULONG hash;
    ULONG name_offset;
    ULONG name_len;
    ULONG data_offset;
    ULONG data_len;
    ULONG rosterindex;
};

struct guidsection_header
{
    DWORD magic;
    ULONG size;
    DWORD unk[3];
    ULONG count;
    ULONG index_offset;
    DWORD unk2;
    ULONG names_offset;
    ULONG names_len;
};

struct guid_index
{
    GUID  guid;
    ULONG data_offset;
    ULONG data_len;
    ULONG rosterindex;
};

struct wndclass_redirect_data
{
    ULONG size;
    DWORD res;
    ULONG name_len;
    ULONG name_offset;  /* versioned name offset */
    ULONG module_len;
    ULONG module_offset;/* container name offset */
};

struct dllredirect_data
{
    ULONG size;
    ULONG unk;
    DWORD res[3];
};

struct tlibredirect_data
{
    ULONG  size;
    DWORD  res;
    ULONG  name_len;
    ULONG  name_offset;
    LANGID langid;
    WORD   flags;
    ULONG  help_len;
    ULONG  help_offset;
    WORD   major_version;
    WORD   minor_version;
};

struct progidredirect_data
{
    ULONG size;
    DWORD reserved;
    ULONG clsid_offset;
};

static void test_find_dll_redirection(HANDLE handle, LPCWSTR libname, ULONG exid, int line)
{
    ACTCTX_SECTION_KEYED_DATA data;
    BOOL ret;

    memset(&data, 0xfe, sizeof(data));
    data.cbSize = sizeof(data);

    ret = FindActCtxSectionStringW(0, NULL, ACTIVATION_CONTEXT_SECTION_DLL_REDIRECTION, libname, &data);
    ok_(__FILE__, line)(ret, "FindActCtxSectionStringW failed: %u\n", GetLastError());
    if (!ret) return;

    ok_(__FILE__, line)(data.cbSize == sizeof(data), "data.cbSize=%u\n", data.cbSize);
    ok_(__FILE__, line)(data.ulDataFormatVersion == 1, "data.ulDataFormatVersion=%u\n", data.ulDataFormatVersion);
    ok_(__FILE__, line)(data.lpData != NULL, "data.lpData == NULL\n");
    ok_(__FILE__, line)(data.ulLength == 20, "data.ulLength=%u\n", data.ulLength);

    if (data.lpData)
    {
        struct dllredirect_data *dlldata = (struct dllredirect_data*)data.lpData;
        ok_(__FILE__, line)(dlldata->size == data.ulLength, "got wrong size %d\n", dlldata->size);
        ok_(__FILE__, line)(dlldata->unk == 2, "got wrong field value %d\n", dlldata->unk);
        ok_(__FILE__, line)(dlldata->res[0] == 0, "got wrong res[0] value %d\n", dlldata->res[0]);
        ok_(__FILE__, line)(dlldata->res[1] == 0, "got wrong res[1] value %d\n", dlldata->res[1]);
        ok_(__FILE__, line)(dlldata->res[2] == 0, "got wrong res[2] value %d\n", dlldata->res[2]);
    }

    ok_(__FILE__, line)(data.lpSectionGlobalData == NULL, "data.lpSectionGlobalData != NULL\n");
    ok_(__FILE__, line)(data.ulSectionGlobalDataLength == 0, "data.ulSectionGlobalDataLength=%u\n",
       data.ulSectionGlobalDataLength);
    ok_(__FILE__, line)(data.lpSectionBase != NULL, "data.lpSectionBase == NULL\n");
    ok_(__FILE__, line)(data.ulSectionTotalLength > 0, "data.ulSectionTotalLength=%u\n",
       data.ulSectionTotalLength);
    ok_(__FILE__, line)(data.hActCtx == NULL, "data.hActCtx=%p\n", data.hActCtx);
    ok_(__FILE__, line)(data.ulAssemblyRosterIndex == exid, "data.ulAssemblyRosterIndex=%u, expected %u\n",
       data.ulAssemblyRosterIndex, exid);

    memset(&data, 0xfe, sizeof(data));
    data.cbSize = sizeof(data);

    ret = FindActCtxSectionStringW(FIND_ACTCTX_SECTION_KEY_RETURN_HACTCTX, NULL,
            ACTIVATION_CONTEXT_SECTION_DLL_REDIRECTION, libname, &data);
    ok_(__FILE__, line)(ret, "FindActCtxSectionStringW failed: %u\n", GetLastError());
    if (!ret) return;

    ok_(__FILE__, line)(data.cbSize == sizeof(data), "data.cbSize=%u\n", data.cbSize);
    ok_(__FILE__, line)(data.ulDataFormatVersion == 1, "data.ulDataFormatVersion=%u\n", data.ulDataFormatVersion);
    ok_(__FILE__, line)(data.lpData != NULL, "data.lpData == NULL\n");
    ok_(__FILE__, line)(data.ulLength == 20, "data.ulLength=%u\n", data.ulLength);
    ok_(__FILE__, line)(data.lpSectionGlobalData == NULL, "data.lpSectionGlobalData != NULL\n");
    ok_(__FILE__, line)(data.ulSectionGlobalDataLength == 0, "data.ulSectionGlobalDataLength=%u\n",
       data.ulSectionGlobalDataLength);
    ok_(__FILE__, line)(data.lpSectionBase != NULL, "data.lpSectionBase == NULL\n");
    ok_(__FILE__, line)(data.ulSectionTotalLength > 0, "data.ulSectionTotalLength=%u\n",
       data.ulSectionTotalLength);
    ok_(__FILE__, line)(data.hActCtx == handle, "data.hActCtx=%p\n", data.hActCtx);
    ok_(__FILE__, line)(data.ulAssemblyRosterIndex == exid, "data.ulAssemblyRosterIndex=%u, expected %u\n",
       data.ulAssemblyRosterIndex, exid);

    ReleaseActCtx(handle);
}

static void test_find_window_class(HANDLE handle, LPCWSTR clsname, ULONG exid, int line)
{
    struct wndclass_redirect_data *wnddata;
    struct strsection_header *header;
    ACTCTX_SECTION_KEYED_DATA data;
    BOOL ret;

    memset(&data, 0xfe, sizeof(data));
    data.cbSize = sizeof(data);

    ret = FindActCtxSectionStringW(0, NULL, ACTIVATION_CONTEXT_SECTION_WINDOW_CLASS_REDIRECTION, clsname, &data);
    ok_(__FILE__, line)(ret, "FindActCtxSectionStringW failed: %u, class %s\n", GetLastError(),
        wine_dbgstr_w(clsname));
    if (!ret) return;

    header = (struct strsection_header*)data.lpSectionBase;
    wnddata = (struct wndclass_redirect_data*)data.lpData;

    ok_(__FILE__, line)(header->magic == 0x64487353, "got wrong magic 0x%08x\n", header->magic);
    ok_(__FILE__, line)(header->count > 0, "got count %d\n", header->count);
    ok_(__FILE__, line)(data.cbSize == sizeof(data), "data.cbSize=%u\n", data.cbSize);
    ok_(__FILE__, line)(data.ulDataFormatVersion == 1, "data.ulDataFormatVersion=%u\n", data.ulDataFormatVersion);
    ok_(__FILE__, line)(data.lpData != NULL, "data.lpData == NULL\n");
    ok_(__FILE__, line)(wnddata->size == sizeof(*wnddata), "got %d for header size\n", wnddata->size);
    if (data.lpData && wnddata->size == sizeof(*wnddata))
    {
        static const WCHAR verW[] = {'6','.','5','.','4','.','3','!',0};
        WCHAR buff[50];
        WCHAR *ptr;
        ULONG len;

        ok_(__FILE__, line)(wnddata->res == 0, "got reserved as %d\n", wnddata->res);
        /* redirect class name (versioned or not) is stored just after header data */
        ok_(__FILE__, line)(wnddata->name_offset == wnddata->size, "got name offset as %d\n", wnddata->name_offset);
        ok_(__FILE__, line)(wnddata->module_len > 0, "got module name length as %d\n", wnddata->module_len);

        /* expected versioned name */
        lstrcpyW(buff, verW);
        lstrcatW(buff, clsname);
        ptr = (WCHAR*)((BYTE*)wnddata + wnddata->name_offset);
        ok_(__FILE__, line)(!lstrcmpW(ptr, buff), "got wrong class name %s, expected %s\n", wine_dbgstr_w(ptr), wine_dbgstr_w(buff));
        ok_(__FILE__, line)(lstrlenW(ptr)*sizeof(WCHAR) == wnddata->name_len,
            "got wrong class name length %d, expected %d\n", wnddata->name_len, lstrlenW(ptr));

        /* data length is simply header length + string data length including nulls */
        len = wnddata->size + wnddata->name_len + wnddata->module_len + 2*sizeof(WCHAR);
        ok_(__FILE__, line)(data.ulLength == len, "got wrong data length %d, expected %d\n", data.ulLength, len);

        if (data.ulSectionTotalLength > wnddata->module_offset)
        {
            WCHAR *modulename, *sectionptr;

            /* just compare pointers */
            modulename = (WCHAR*)((BYTE*)wnddata + wnddata->size + wnddata->name_len + sizeof(WCHAR));
            sectionptr = (WCHAR*)((BYTE*)data.lpSectionBase + wnddata->module_offset);
            ok_(__FILE__, line)(modulename == sectionptr, "got wrong name offset %p, expected %p\n", sectionptr, modulename);
        }
    }

    ok_(__FILE__, line)(data.lpSectionGlobalData == NULL, "data.lpSectionGlobalData != NULL\n");
    ok_(__FILE__, line)(data.ulSectionGlobalDataLength == 0, "data.ulSectionGlobalDataLength=%u\n",
       data.ulSectionGlobalDataLength);
    ok_(__FILE__, line)(data.lpSectionBase != NULL, "data.lpSectionBase == NULL\n");
    ok_(__FILE__, line)(data.ulSectionTotalLength > 0, "data.ulSectionTotalLength=%u\n",
       data.ulSectionTotalLength);
    ok_(__FILE__, line)(data.hActCtx == NULL, "data.hActCtx=%p\n", data.hActCtx);
    ok_(__FILE__, line)(data.ulAssemblyRosterIndex == exid, "data.ulAssemblyRosterIndex=%u, expected %u\n",
       data.ulAssemblyRosterIndex, exid);

    memset(&data, 0xfe, sizeof(data));
    data.cbSize = sizeof(data);

    ret = FindActCtxSectionStringW(FIND_ACTCTX_SECTION_KEY_RETURN_HACTCTX, NULL,
            ACTIVATION_CONTEXT_SECTION_WINDOW_CLASS_REDIRECTION, clsname, &data);
    ok_(__FILE__, line)(ret, "FindActCtxSectionStringW failed: %u, class %s\n", GetLastError(),
        wine_dbgstr_w(clsname));
    if (!ret) return;

    ok_(__FILE__, line)(data.cbSize == sizeof(data), "data.cbSize=%u\n", data.cbSize);
    ok_(__FILE__, line)(data.ulDataFormatVersion == 1, "data.ulDataFormatVersion=%u\n", data.ulDataFormatVersion);
    ok_(__FILE__, line)(data.lpData != NULL, "data.lpData == NULL\n");
    ok_(__FILE__, line)(data.ulLength > 0, "data.ulLength=%u\n", data.ulLength);
    ok_(__FILE__, line)(data.lpSectionGlobalData == NULL, "data.lpSectionGlobalData != NULL\n");
    ok_(__FILE__, line)(data.ulSectionGlobalDataLength == 0, "data.ulSectionGlobalDataLength=%u\n",
       data.ulSectionGlobalDataLength);
    ok_(__FILE__, line)(data.lpSectionBase != NULL, "data.lpSectionBase == NULL\n");
    ok_(__FILE__, line)(data.ulSectionTotalLength > 0, "data.ulSectionTotalLength=%u\n", data.ulSectionTotalLength);
    ok_(__FILE__, line)(data.hActCtx == handle, "data.hActCtx=%p\n", data.hActCtx);
    ok_(__FILE__, line)(data.ulAssemblyRosterIndex == exid, "data.ulAssemblyRosterIndex=%u, expected %u\n",
       data.ulAssemblyRosterIndex, exid);

    ReleaseActCtx(handle);
}

static void test_find_string_fail(void)
{
    ACTCTX_SECTION_KEYED_DATA data = {sizeof(data)};
    BOOL ret;

    ret = FindActCtxSectionStringW(0, NULL, 100, testlib_dll, &data);
    ok(!ret, "FindActCtxSectionStringW succeeded\n");
    ok(GetLastError() == ERROR_SXS_SECTION_NOT_FOUND, "GetLastError()=%u\n", GetLastError());

    ret = FindActCtxSectionStringW(0, NULL, ACTIVATION_CONTEXT_SECTION_DLL_REDIRECTION, testlib2_dll, &data);
    ok(!ret, "FindActCtxSectionStringW succeeded\n");
    ok(GetLastError() == ERROR_SXS_KEY_NOT_FOUND, "GetLastError()=%u\n", GetLastError());

    ret = FindActCtxSectionStringW(0, NULL, ACTIVATION_CONTEXT_SECTION_DLL_REDIRECTION, testlib_dll, NULL);
    ok(!ret, "FindActCtxSectionStringW succeeded\n");
    ok(GetLastError() == ERROR_INVALID_PARAMETER, "GetLastError()=%u\n", GetLastError());

    ret = FindActCtxSectionStringW(0, NULL, ACTIVATION_CONTEXT_SECTION_DLL_REDIRECTION, NULL, &data);
    ok(!ret, "FindActCtxSectionStringW succeeded\n");
    ok(GetLastError() == ERROR_INVALID_PARAMETER, "GetLastError()=%u\n", GetLastError());

    data.cbSize = 0;
    ret = FindActCtxSectionStringW(0, NULL, ACTIVATION_CONTEXT_SECTION_DLL_REDIRECTION, testlib_dll, &data);
    ok(!ret, "FindActCtxSectionStringW succeeded\n");
    ok(GetLastError() == ERROR_INVALID_PARAMETER, "GetLastError()=%u\n", GetLastError());

    data.cbSize = 35;
    ret = FindActCtxSectionStringW(0, NULL, ACTIVATION_CONTEXT_SECTION_DLL_REDIRECTION, testlib_dll, &data);
    ok(!ret, "FindActCtxSectionStringW succeeded\n");
    ok(GetLastError() == ERROR_INVALID_PARAMETER, "GetLastError()=%u\n", GetLastError());
}


static void test_basic_info(HANDLE handle, int line)
{
    ACTIVATION_CONTEXT_BASIC_INFORMATION basic;
    SIZE_T size;
    BOOL b;

    b = QueryActCtxW(QUERY_ACTCTX_FLAG_NO_ADDREF, handle, NULL, ActivationContextBasicInformation, &basic,
                          sizeof(basic), &size);

    ok_(__FILE__, line) (b,"ActivationContextBasicInformation failed\n");
    ok_(__FILE__, line) (size == sizeof(ACTIVATION_CONTEXT_BASIC_INFORMATION),"size mismatch\n");
    ok_(__FILE__, line) (basic.dwFlags == 0, "unexpected flags %x\n",basic.dwFlags);
    ok_(__FILE__, line) (basic.hActCtx == handle, "unexpected handle\n");

    b = QueryActCtxW(QUERY_ACTCTX_FLAG_USE_ACTIVE_ACTCTX | QUERY_ACTCTX_FLAG_NO_ADDREF, handle, NULL,
                          ActivationContextBasicInformation, &basic,
                          sizeof(basic), &size);
    if (handle)
    {
        ok_(__FILE__, line) (!b,"ActivationContextBasicInformation succeeded\n");
        ok_(__FILE__, line) (size == 0,"size mismatch\n");
        ok_(__FILE__, line) (GetLastError() == ERROR_INVALID_PARAMETER, "Wrong last error\n");
        ok_(__FILE__, line) (basic.dwFlags == 0, "unexpected flags %x\n",basic.dwFlags);
        ok_(__FILE__, line) (basic.hActCtx == handle, "unexpected handle\n");
    }
    else
    {
        ok_(__FILE__, line) (b,"ActivationContextBasicInformation failed\n");
        ok_(__FILE__, line) (size == sizeof(ACTIVATION_CONTEXT_BASIC_INFORMATION),"size mismatch\n");
        ok_(__FILE__, line) (basic.dwFlags == 0, "unexpected flags %x\n",basic.dwFlags);
        ok_(__FILE__, line) (basic.hActCtx == handle, "unexpected handle\n");
    }
}

enum comclass_threadingmodel {
    ThreadingModel_Apartment = 1,
    ThreadingModel_Free      = 2,
    ThreadingModel_No        = 3,
    ThreadingModel_Both      = 4,
    ThreadingModel_Neutral   = 5
};

enum comclass_miscfields {
    MiscStatus          = 1,
    MiscStatusIcon      = 2,
    MiscStatusContent   = 4,
    MiscStatusThumbnail = 8,
    MiscStatusDocPrint  = 16
};

struct comclassredirect_data {
    ULONG size;
    ULONG flags;
    DWORD model;
    GUID  clsid;
    GUID  alias;
    GUID  clsid2;
    GUID  tlid;
    ULONG name_len;
    ULONG name_offset;
    ULONG progid_len;
    ULONG progid_offset;
    ULONG clrdata_len;
    ULONG clrdata_offset;
    DWORD miscstatus;
    DWORD miscstatuscontent;
    DWORD miscstatusthumbnail;
    DWORD miscstatusicon;
    DWORD miscstatusdocprint;
};

struct clrclass_data {
    ULONG size;
    DWORD res[2];
    ULONG module_len;
    ULONG module_offset;
    ULONG name_len;
    ULONG name_offset;
    ULONG version_len;
    ULONG version_offset;
    DWORD res2[2];
};

static void test_find_com_redirection(HANDLE handle, const GUID *clsid, const GUID *tlid, const WCHAR *progid, ULONG exid, int line)
{
    struct comclassredirect_data *comclass, *comclass2;
    ACTCTX_SECTION_KEYED_DATA data, data2;
    struct guidsection_header *header;
    BOOL ret;

    memset(&data, 0xfe, sizeof(data));
    data.cbSize = sizeof(data);

    ret = FindActCtxSectionGuid(0, NULL, ACTIVATION_CONTEXT_SECTION_COM_SERVER_REDIRECTION, clsid, &data);
    if (!ret)
    {
        skip("failed for guid %s\n", wine_dbgstr_guid(clsid));
        return;
    }
    ok_(__FILE__, line)(ret, "FindActCtxSectionGuid failed: %u\n", GetLastError());

    comclass = (struct comclassredirect_data*)data.lpData;

    ok_(__FILE__, line)(data.cbSize == sizeof(data), "data.cbSize=%u\n", data.cbSize);
    ok_(__FILE__, line)(data.ulDataFormatVersion == 1, "data.ulDataFormatVersion=%u\n", data.ulDataFormatVersion);
    ok_(__FILE__, line)(data.lpData != NULL, "data.lpData == NULL\n");
    ok_(__FILE__, line)(comclass->size == sizeof(*comclass), "got %d for header size\n", comclass->size);
    if (data.lpData && comclass->size == sizeof(*comclass))
    {
        ULONG len, miscmask;
        WCHAR *ptr;

        ok_(__FILE__, line)(comclass->model == ThreadingModel_Neutral, "got model %d\n", comclass->model);
        ok_(__FILE__, line)(IsEqualGUID(&comclass->clsid, clsid), "got wrong clsid %s\n", wine_dbgstr_guid(&comclass->clsid));
        ok_(__FILE__, line)(IsEqualGUID(&comclass->clsid2, clsid), "got wrong clsid2 %s\n", wine_dbgstr_guid(&comclass->clsid2));
        if (tlid)
            ok_(__FILE__, line)(IsEqualGUID(&comclass->tlid, tlid), "got wrong tlid %s\n", wine_dbgstr_guid(&comclass->tlid));
        ok_(__FILE__, line)(comclass->name_len > 0, "got modulename len %d\n", comclass->name_len);

        if (progid)
        {
            len = comclass->size + comclass->clrdata_len;
            ok_(__FILE__, line)(comclass->progid_offset == len, "got progid offset %d, expected %d\n", comclass->progid_offset, len);
        }
        else
            ok_(__FILE__, line)(comclass->progid_offset == 0, "got progid offset %d, expected 0\n", comclass->progid_offset);

        if (comclass->progid_offset)
        {
            ptr = (WCHAR*)((BYTE*)comclass + comclass->progid_offset);
            ok_(__FILE__, line)(!lstrcmpW(ptr, progid), "got wrong progid %s, expected %s\n", wine_dbgstr_w(ptr), wine_dbgstr_w(progid));
            ok_(__FILE__, line)(lstrlenW(progid)*sizeof(WCHAR) == comclass->progid_len,
                "got progid name length %d\n", comclass->progid_len);
        }

        /* data length is simply header length + string data length including nulls */
        len = comclass->size + comclass->clrdata_len;
        if (comclass->progid_len) len += comclass->progid_len + sizeof(WCHAR);
        ok_(__FILE__, line)(data.ulLength == len, "got wrong data length %d, expected %d\n", data.ulLength, len);

        /* keyed data structure doesn't include module name, it's available from section data */
        ok_(__FILE__, line)(data.ulSectionTotalLength > comclass->name_offset, "got wrong offset %d\n", comclass->name_offset);

        /* check misc fields are set */
        miscmask = (comclass->flags >> 8) & 0xff;
        if (miscmask)
        {
            if (miscmask & MiscStatus)
                ok_(__FILE__, line)(comclass->miscstatus != 0, "got miscstatus 0x%08x\n", comclass->miscstatus);
            if (miscmask & MiscStatusIcon)
                ok_(__FILE__, line)(comclass->miscstatusicon != 0, "got miscstatusicon 0x%08x\n", comclass->miscstatusicon);
            if (miscmask & MiscStatusContent)
                ok_(__FILE__, line)(comclass->miscstatuscontent != 0, "got miscstatuscontent 0x%08x\n", comclass->miscstatuscontent);
            if (miscmask & MiscStatusThumbnail)
                ok_(__FILE__, line)(comclass->miscstatusthumbnail != 0, "got miscstatusthumbnail 0x%08x\n", comclass->miscstatusthumbnail);
            if (miscmask & MiscStatusDocPrint)
                ok_(__FILE__, line)(comclass->miscstatusdocprint != 0, "got miscstatusdocprint 0x%08x\n", comclass->miscstatusdocprint);
        }
        ok_(__FILE__, line)(!(comclass->flags & 0xffff00ff), "Unexpected flags %#x.\n", comclass->flags);

        /* part used for clrClass only */
        if (comclass->clrdata_len)
        {
            static const WCHAR mscoreeW[] = {'M','S','C','O','R','E','E','.','D','L','L',0};
            static const WCHAR mscoree2W[] = {'m','s','c','o','r','e','e','.','d','l','l',0};
            struct clrclass_data *clrclass;
            WCHAR *ptrW;

            clrclass = (struct clrclass_data*)((BYTE*)data.lpData + comclass->clrdata_offset);
            ok_(__FILE__, line)(clrclass->size == sizeof(*clrclass), "clrclass: got size %d\n", clrclass->size);
            ok_(__FILE__, line)(clrclass->res[0] == 0, "clrclass: got res[0]=0x%08x\n", clrclass->res[0]);
            ok_(__FILE__, line)(clrclass->res[1] == 2, "clrclass: got res[1]=0x%08x\n", clrclass->res[1]);
            ok_(__FILE__, line)(clrclass->module_len == lstrlenW(mscoreeW)*sizeof(WCHAR), "clrclass: got module len %d\n", clrclass->module_len);
            ok_(__FILE__, line)(clrclass->module_offset > 0, "clrclass: got module offset %d\n", clrclass->module_offset);

            ok_(__FILE__, line)(clrclass->name_len > 0, "clrclass: got name len %d\n", clrclass->name_len);
            ok_(__FILE__, line)(clrclass->name_offset == clrclass->size, "clrclass: got name offset %d\n", clrclass->name_offset);
            ok_(__FILE__, line)(clrclass->version_len > 0, "clrclass: got version len %d\n", clrclass->version_len);
            ok_(__FILE__, line)(clrclass->version_offset > 0, "clrclass: got version offset %d\n", clrclass->version_offset);

            ok_(__FILE__, line)(clrclass->res2[0] == 0, "clrclass: got res2[0]=0x%08x\n", clrclass->res2[0]);
            ok_(__FILE__, line)(clrclass->res2[1] == 0, "clrclass: got res2[1]=0x%08x\n", clrclass->res2[1]);

            /* clrClass uses mscoree.dll as module name, but in two variants - comclass data points to module name
               in lower case, clsclass subsection - in upper case */
            ok_(__FILE__, line)(comclass->name_len == lstrlenW(mscoree2W)*sizeof(WCHAR), "clrclass: got com name len %d\n", comclass->name_len);
            ok_(__FILE__, line)(comclass->name_offset > 0, "clrclass: got name offset %d\n", clrclass->name_offset);

            ptrW = (WCHAR*)((BYTE*)data.lpSectionBase + comclass->name_offset);
            ok_(__FILE__, line)(!lstrcmpW(ptrW, mscoreeW), "clrclass: module name %s\n", wine_dbgstr_w(ptrW));

            ptrW = (WCHAR*)((BYTE*)data.lpSectionBase + clrclass->module_offset);
            ok_(__FILE__, line)(!lstrcmpW(ptrW, mscoree2W), "clrclass: module name2 %s\n", wine_dbgstr_w(ptrW));
        }
    }

    header = (struct guidsection_header*)data.lpSectionBase;
    ok_(__FILE__, line)(data.lpSectionGlobalData == ((BYTE*)header + header->names_offset), "data.lpSectionGlobalData == NULL\n");
    ok_(__FILE__, line)(data.ulSectionGlobalDataLength == header->names_len, "data.ulSectionGlobalDataLength=%u\n",
       data.ulSectionGlobalDataLength);
    ok_(__FILE__, line)(data.lpSectionBase != NULL, "data.lpSectionBase == NULL\n");
    ok_(__FILE__, line)(data.ulSectionTotalLength > 0, "data.ulSectionTotalLength=%u\n",
       data.ulSectionTotalLength);
    ok_(__FILE__, line)(data.hActCtx == NULL, "data.hActCtx=%p\n", data.hActCtx);
    ok_(__FILE__, line)(data.ulAssemblyRosterIndex == exid, "data.ulAssemblyRosterIndex=%u, expected %u\n",
       data.ulAssemblyRosterIndex, exid);

    /* generated guid for this class works as key guid in search */
    memset(&data2, 0xfe, sizeof(data2));
    data2.cbSize = sizeof(data2);
    ret = FindActCtxSectionGuid(0, NULL, ACTIVATION_CONTEXT_SECTION_COM_SERVER_REDIRECTION, &comclass->alias, &data2);
    ok_(__FILE__, line)(ret, "FindActCtxSectionGuid failed: %u\n", GetLastError());

    comclass2 = (struct comclassredirect_data*)data2.lpData;
    ok_(__FILE__, line)(comclass->size == comclass2->size, "got wrong data length %d, expected %d\n", comclass2->size, comclass->size);
    ok_(__FILE__, line)(!memcmp(comclass, comclass2, comclass->size), "got wrong data\n");
}

enum ifaceps_mask
{
    NumMethods = 1,
    BaseIface  = 2
};

struct ifacepsredirect_data
{
    ULONG size;
    DWORD mask;
    GUID  iid;
    ULONG nummethods;
    GUID  tlbid;
    GUID  base;
    ULONG name_len;
    ULONG name_offset;
};

static void test_find_ifaceps_redirection(HANDLE handle, const GUID *iid, const GUID *tlbid, const GUID *base,
    const GUID *ps32, ULONG exid, int line)
{
    struct ifacepsredirect_data *ifaceps;
    ACTCTX_SECTION_KEYED_DATA data;
    BOOL ret;

    memset(&data, 0xfe, sizeof(data));
    data.cbSize = sizeof(data);

    ret = FindActCtxSectionGuid(0, NULL, ACTIVATION_CONTEXT_SECTION_COM_INTERFACE_REDIRECTION, iid, &data);
    ok_(__FILE__, line)(ret, "FindActCtxSectionGuid failed: %u\n", GetLastError());

    ifaceps = (struct ifacepsredirect_data*)data.lpData;

    ok_(__FILE__, line)(data.cbSize == sizeof(data), "data.cbSize=%u\n", data.cbSize);
    ok_(__FILE__, line)(data.ulDataFormatVersion == 1, "data.ulDataFormatVersion=%u\n", data.ulDataFormatVersion);
    ok_(__FILE__, line)(data.lpData != NULL, "data.lpData == NULL\n");
    ok_(__FILE__, line)(ifaceps->size == sizeof(*ifaceps), "got %d for header size\n", ifaceps->size);
    if (data.lpData && ifaceps->size == sizeof(*ifaceps))
    {
        ULONG len;

        /* for external proxy stubs it contains a value from 'proxyStubClsid32' */
        if (ps32)
        {
            ok_(__FILE__, line)(IsEqualGUID(&ifaceps->iid, ps32), "got wrong iid %s\n", wine_dbgstr_guid(&ifaceps->iid));
        }
        else
            ok_(__FILE__, line)(IsEqualGUID(&ifaceps->iid, iid), "got wrong iid %s\n", wine_dbgstr_guid(&ifaceps->iid));

        ok_(__FILE__, line)(IsEqualGUID(&ifaceps->tlbid, tlbid), "got wrong tlid %s\n", wine_dbgstr_guid(&ifaceps->tlbid));
        ok_(__FILE__, line)(ifaceps->name_len > 0, "got modulename len %d\n", ifaceps->name_len);
        ok_(__FILE__, line)(ifaceps->name_offset == ifaceps->size, "got progid offset %d\n", ifaceps->name_offset);

        /* data length is simply header length + string data length including nulls */
        len = ifaceps->size + ifaceps->name_len + sizeof(WCHAR);
        ok_(__FILE__, line)(data.ulLength == len, "got wrong data length %d, expected %d\n", data.ulLength, len);

        /* mask purpose is to indicate if attribute was specified, for testing purposes assume that manifest
           always has non-zero value for it */
        if (ifaceps->mask & NumMethods)
            ok_(__FILE__, line)(ifaceps->nummethods != 0, "got nummethods %d\n", ifaceps->nummethods);
        if (ifaceps->mask & BaseIface)
            ok_(__FILE__, line)(IsEqualGUID(&ifaceps->base, base), "got base %s\n", wine_dbgstr_guid(&ifaceps->base));
    }

    ok_(__FILE__, line)(data.lpSectionGlobalData == NULL, "data.lpSectionGlobalData != NULL\n");
    ok_(__FILE__, line)(data.ulSectionGlobalDataLength == 0, "data.ulSectionGlobalDataLength=%u\n",
       data.ulSectionGlobalDataLength);
    ok_(__FILE__, line)(data.lpSectionBase != NULL, "data.lpSectionBase == NULL\n");
    ok_(__FILE__, line)(data.ulSectionTotalLength > 0, "data.ulSectionTotalLength=%u\n",
       data.ulSectionTotalLength);
    ok_(__FILE__, line)(data.hActCtx == NULL, "data.hActCtx=%p\n", data.hActCtx);
    ok_(__FILE__, line)(data.ulAssemblyRosterIndex == exid, "data.ulAssemblyRosterIndex=%u, expected %u\n",
       data.ulAssemblyRosterIndex, exid);
}

struct clrsurrogate_data
{
    ULONG size;
    DWORD res;
    GUID  clsid;
    ULONG version_offset;
    ULONG version_len;
    ULONG name_offset;
    ULONG name_len;
};

static void test_find_surrogate(HANDLE handle, const GUID *clsid, const WCHAR *name, const WCHAR *version,
    ULONG exid, int line)
{
    struct clrsurrogate_data *surrogate;
    ACTCTX_SECTION_KEYED_DATA data;
    BOOL ret;

    memset(&data, 0xfe, sizeof(data));
    data.cbSize = sizeof(data);

    ret = FindActCtxSectionGuid(0, NULL, ACTIVATION_CONTEXT_SECTION_CLR_SURROGATES, clsid, &data);
    if (!ret)
    {
        skip("surrogate sections are not supported\n");
        return;
    }
    ok_(__FILE__, line)(ret, "FindActCtxSectionGuid failed: %u\n", GetLastError());

    surrogate = (struct clrsurrogate_data*)data.lpData;

    ok_(__FILE__, line)(data.cbSize == sizeof(data), "data.cbSize=%u\n", data.cbSize);
    ok_(__FILE__, line)(data.ulDataFormatVersion == 1, "data.ulDataFormatVersion=%u\n", data.ulDataFormatVersion);
    ok_(__FILE__, line)(data.lpData != NULL, "data.lpData == NULL\n");
    ok_(__FILE__, line)(surrogate->size == sizeof(*surrogate), "got %d for header size\n", surrogate->size);
    if (data.lpData && surrogate->size == sizeof(*surrogate))
    {
        WCHAR *ptrW;
        ULONG len;

        ok_(__FILE__, line)(surrogate->res == 0, "invalid res value %d\n", surrogate->res);
        ok_(__FILE__, line)(IsEqualGUID(&surrogate->clsid, clsid), "got wrong clsid %s\n", wine_dbgstr_guid(&surrogate->clsid));

        ok_(__FILE__, line)(surrogate->version_len == lstrlenW(version)*sizeof(WCHAR), "got version len %d\n", surrogate->version_len);
        ok_(__FILE__, line)(surrogate->version_offset == surrogate->size, "got version offset %d\n", surrogate->version_offset);

        ok_(__FILE__, line)(surrogate->name_len == lstrlenW(name)*sizeof(WCHAR), "got name len %d\n", surrogate->name_len);
        ok_(__FILE__, line)(surrogate->name_offset > surrogate->version_offset, "got name offset %d\n", surrogate->name_offset);

        len = surrogate->size + surrogate->name_len + surrogate->version_len + 2*sizeof(WCHAR);
        ok_(__FILE__, line)(data.ulLength == len, "got wrong data length %d, expected %d\n", data.ulLength, len);

        ptrW = (WCHAR*)((BYTE*)surrogate + surrogate->name_offset);
        ok(!lstrcmpW(ptrW, name), "got wrong name %s\n", wine_dbgstr_w(ptrW));

        ptrW = (WCHAR*)((BYTE*)surrogate + surrogate->version_offset);
        ok(!lstrcmpW(ptrW, version), "got wrong name %s\n", wine_dbgstr_w(ptrW));
    }

    ok_(__FILE__, line)(data.lpSectionGlobalData == NULL, "data.lpSectionGlobalData != NULL\n");
    ok_(__FILE__, line)(data.ulSectionGlobalDataLength == 0, "data.ulSectionGlobalDataLength=%u\n",
       data.ulSectionGlobalDataLength);
    ok_(__FILE__, line)(data.lpSectionBase != NULL, "data.lpSectionBase == NULL\n");
    ok_(__FILE__, line)(data.ulSectionTotalLength > 0, "data.ulSectionTotalLength=%u\n",
       data.ulSectionTotalLength);
    ok_(__FILE__, line)(data.hActCtx == NULL, "data.hActCtx=%p\n", data.hActCtx);
    ok_(__FILE__, line)(data.ulAssemblyRosterIndex == exid, "data.ulAssemblyRosterIndex=%u, expected %u\n",
       data.ulAssemblyRosterIndex, exid);
}

static void test_find_progid_redirection(HANDLE handle, const GUID *clsid, const char *progid, ULONG exid, int line)
{
    struct progidredirect_data *progiddata;
    struct comclassredirect_data *comclass;
    ACTCTX_SECTION_KEYED_DATA data, data2;
    struct strsection_header *header;
    BOOL ret;

    memset(&data, 0xfe, sizeof(data));
    data.cbSize = sizeof(data);

    ret = FindActCtxSectionStringA(0, NULL, ACTIVATION_CONTEXT_SECTION_COM_PROGID_REDIRECTION, progid, &data);
    ok_(__FILE__, line)(ret, "FindActCtxSectionStringA failed: %u\n", GetLastError());

    progiddata = (struct progidredirect_data*)data.lpData;

    ok_(__FILE__, line)(data.cbSize == sizeof(data), "data.cbSize=%u\n", data.cbSize);
    ok_(__FILE__, line)(data.ulDataFormatVersion == 1, "data.ulDataFormatVersion=%u\n", data.ulDataFormatVersion);
    ok_(__FILE__, line)(data.lpData != NULL, "data.lpData == NULL\n");
    ok_(__FILE__, line)(progiddata->size == sizeof(*progiddata), "got %d for header size\n", progiddata->size);
    if (data.lpData && progiddata->size == sizeof(*progiddata))
    {
        GUID *guid;

        ok_(__FILE__, line)(progiddata->reserved == 0, "got reserved as %d\n", progiddata->reserved);
        ok_(__FILE__, line)(progiddata->clsid_offset > 0, "got clsid_offset as %d\n", progiddata->clsid_offset);

        /* progid data points to generated alias guid */
        guid = (GUID*)((BYTE*)data.lpSectionBase + progiddata->clsid_offset);

        memset(&data2, 0, sizeof(data2));
        data2.cbSize = sizeof(data2);
        ret = FindActCtxSectionGuid(0, NULL, ACTIVATION_CONTEXT_SECTION_COM_SERVER_REDIRECTION, guid, &data2);
        ok_(__FILE__, line)(ret, "FindActCtxSectionGuid failed: %u\n", GetLastError());

        comclass = (struct comclassredirect_data*)data2.lpData;
        ok_(__FILE__, line)(IsEqualGUID(guid, &comclass->alias), "got wrong alias referenced from progid %s, %s\n", progid, wine_dbgstr_guid(guid));
        ok_(__FILE__, line)(IsEqualGUID(clsid, &comclass->clsid), "got wrong class referenced from progid %s, %s\n", progid, wine_dbgstr_guid(clsid));
    }

    header = (struct strsection_header*)data.lpSectionBase;
    ok_(__FILE__, line)(data.lpSectionGlobalData == (BYTE*)header + header->global_offset, "data.lpSectionGlobalData == NULL\n");
    ok_(__FILE__, line)(data.ulSectionGlobalDataLength == header->global_len, "data.ulSectionGlobalDataLength=%u\n", data.ulSectionGlobalDataLength);
    ok_(__FILE__, line)(data.lpSectionBase != NULL, "data.lpSectionBase == NULL\n");
    ok_(__FILE__, line)(data.ulSectionTotalLength > 0, "data.ulSectionTotalLength=%u\n", data.ulSectionTotalLength);
    ok_(__FILE__, line)(data.hActCtx == NULL, "data.hActCtx=%p\n", data.hActCtx);
    ok_(__FILE__, line)(data.ulAssemblyRosterIndex == exid, "data.ulAssemblyRosterIndex=%u, expected %u\n",
        data.ulAssemblyRosterIndex, exid);
}

static void test_wndclass_section(void)
{
    static const WCHAR cls1W[] = {'1','.','2','.','3','.','4','!','w','n','d','C','l','a','s','s','1',0};
    ACTCTX_SECTION_KEYED_DATA data, data2;
    struct wndclass_redirect_data *classdata;
    struct strsection_header *section;
    ULONG_PTR cookie;
    HANDLE handle;
    WCHAR *ptrW;
    BOOL ret;

    /* use two dependent manifests, each defines 2 window class redirects */
    create_manifest_file("testdep1.manifest", manifest_wndcls1, -1, NULL, NULL);
    create_manifest_file("testdep2.manifest", manifest_wndcls2, -1, NULL, NULL);
    create_manifest_file("main_wndcls.manifest", manifest_wndcls_main, -1, NULL, NULL);

    handle = test_create("main_wndcls.manifest");
    ok(handle != INVALID_HANDLE_VALUE, "handle == INVALID_HANDLE_VALUE, error %u\n", GetLastError());

    DeleteFileA("testdep1.manifest");
    DeleteFileA("testdep2.manifest");
    DeleteFileA("main_wndcls.manifest");

    ret = ActivateActCtx(handle, &cookie);
    ok(ret, "ActivateActCtx failed: %u\n", GetLastError());

    memset(&data, 0, sizeof(data));
    memset(&data2, 0, sizeof(data2));
    data.cbSize = sizeof(data);
    data2.cbSize = sizeof(data2);

    /* get data for two classes from different assemblies */
    ret = FindActCtxSectionStringW(0, NULL, ACTIVATION_CONTEXT_SECTION_WINDOW_CLASS_REDIRECTION, wndClass1W, &data);
    ok(ret, "got %d\n", ret);
    ret = FindActCtxSectionStringW(0, NULL, ACTIVATION_CONTEXT_SECTION_WINDOW_CLASS_REDIRECTION, wndClass3W, &data2);
    ok(ret, "got %d\n", ret);

    section = (struct strsection_header*)data.lpSectionBase;
    ok(section->count == 4, "got %d\n", section->count);
    ok(section->size == sizeof(*section), "got %d\n", section->size);

    /* For both string same section is returned, meaning it's one wndclass section per context */
    ok(data.lpSectionBase == data2.lpSectionBase, "got %p, %p\n", data.lpSectionBase, data2.lpSectionBase);
    ok(data.ulSectionTotalLength == data2.ulSectionTotalLength, "got %u, %u\n", data.ulSectionTotalLength,
        data2.ulSectionTotalLength);

    /* wndClass1 is versioned, wndClass3 is not */
    classdata = (struct wndclass_redirect_data*)data.lpData;
    ptrW = (WCHAR*)((BYTE*)data.lpData + classdata->name_offset);
    ok(!lstrcmpW(ptrW, cls1W), "got %s\n", wine_dbgstr_w(ptrW));

    classdata = (struct wndclass_redirect_data*)data2.lpData;
    ptrW = (WCHAR*)((BYTE*)data2.lpData + classdata->name_offset);
    ok(!lstrcmpW(ptrW, wndClass3W), "got %s\n", wine_dbgstr_w(ptrW));

    ret = DeactivateActCtx(0, cookie);
    ok(ret, "DeactivateActCtx failed: %u\n", GetLastError());

    ReleaseActCtx(handle);
}

static void test_dllredirect_section(void)
{
    static const WCHAR testlib1W[] = {'t','e','s','t','l','i','b','1','.','d','l','l',0};
    static const WCHAR testlib2W[] = {'t','e','s','t','l','i','b','2','.','d','l','l',0};
    ACTCTX_SECTION_KEYED_DATA data, data2;
    struct strsection_header *section;
    ULONG_PTR cookie;
    HANDLE handle;
    BOOL ret;

    /* use two dependent manifests, 4 'files' total */
    create_manifest_file("testdep1.manifest", manifest_wndcls1, -1, NULL, NULL);
    create_manifest_file("testdep2.manifest", manifest_wndcls2, -1, NULL, NULL);
    create_manifest_file("main_wndcls.manifest", manifest_wndcls_main, -1, NULL, NULL);

    handle = test_create("main_wndcls.manifest");
    ok(handle != INVALID_HANDLE_VALUE, "handle == INVALID_HANDLE_VALUE, error %u\n", GetLastError());

    DeleteFileA("testdep1.manifest");
    DeleteFileA("testdep2.manifest");
    DeleteFileA("main_wndcls.manifest");

    ret = ActivateActCtx(handle, &cookie);
    ok(ret, "ActivateActCtx failed: %u\n", GetLastError());

    memset(&data, 0, sizeof(data));
    memset(&data2, 0, sizeof(data2));
    data.cbSize = sizeof(data);
    data2.cbSize = sizeof(data2);

    /* get data for two files from different assemblies */
    ret = FindActCtxSectionStringW(0, NULL, ACTIVATION_CONTEXT_SECTION_DLL_REDIRECTION, testlib1W, &data);
    ok(ret, "got %d\n", ret);
    ret = FindActCtxSectionStringW(0, NULL, ACTIVATION_CONTEXT_SECTION_DLL_REDIRECTION, testlib2W, &data2);
    ok(ret, "got %d\n", ret);

    section = (struct strsection_header*)data.lpSectionBase;
    ok(section->count == 4, "got %d\n", section->count);
    ok(section->size == sizeof(*section), "got %d\n", section->size);

    /* For both string same section is returned, meaning it's one dll redirect section per context */
    ok(data.lpSectionBase == data2.lpSectionBase, "got %p, %p\n", data.lpSectionBase, data2.lpSectionBase);
    ok(data.ulSectionTotalLength == data2.ulSectionTotalLength, "got %u, %u\n", data.ulSectionTotalLength,
        data2.ulSectionTotalLength);

    ret = DeactivateActCtx(0, cookie);
    ok(ret, "DeactivateActCtx failed: %u\n", GetLastError());

    ReleaseActCtx(handle);
}

static void test_typelib_section(void)
{
    static const WCHAR helpW[] = {'h','e','l','p'};
    ACTCTX_SECTION_KEYED_DATA data, data2;
    struct guidsection_header *section;
    struct tlibredirect_data *tlib;
    ULONG_PTR cookie;
    HANDLE handle;
    BOOL ret;

    /* use two dependent manifests, 4 'files' total */
    create_manifest_file("testdep1.manifest", manifest_wndcls1, -1, NULL, NULL);
    create_manifest_file("testdep2.manifest", manifest_wndcls2, -1, NULL, NULL);
    create_manifest_file("main_wndcls.manifest", manifest_wndcls_main, -1, NULL, NULL);

    handle = test_create("main_wndcls.manifest");
    ok(handle != INVALID_HANDLE_VALUE, "handle == INVALID_HANDLE_VALUE, error %u\n", GetLastError());

    DeleteFileA("testdep1.manifest");
    DeleteFileA("testdep2.manifest");
    DeleteFileA("main_wndcls.manifest");

    ret = ActivateActCtx(handle, &cookie);
    ok(ret, "ActivateActCtx failed: %u\n", GetLastError());

    memset(&data, 0, sizeof(data));
    memset(&data2, 0, sizeof(data2));
    data.cbSize = sizeof(data);
    data2.cbSize = sizeof(data2);

    /* get data for two typelibs from different assemblies */
    ret = FindActCtxSectionGuid(0, NULL, ACTIVATION_CONTEXT_SECTION_COM_TYPE_LIBRARY_REDIRECTION, &IID_TlibTest, &data);
    ok(ret, "got %d\n", ret);

    ret = FindActCtxSectionGuid(0, NULL, ACTIVATION_CONTEXT_SECTION_COM_TYPE_LIBRARY_REDIRECTION,
            &IID_TlibTest4, &data2);
    ok(ret, "got %d\n", ret);

    section = (struct guidsection_header*)data.lpSectionBase;
    ok(section->count == 4, "got %d\n", section->count);
    ok(section->size == sizeof(*section), "got %d\n", section->size);

    /* For both GUIDs same section is returned */
    ok(data.lpSectionBase == data2.lpSectionBase, "got %p, %p\n", data.lpSectionBase, data2.lpSectionBase);
    ok(data.ulSectionTotalLength == data2.ulSectionTotalLength, "got %u, %u\n", data.ulSectionTotalLength,
        data2.ulSectionTotalLength);

    ok(data.lpSectionGlobalData == ((BYTE*)section + section->names_offset), "data.lpSectionGlobalData == NULL\n");
    ok(data.ulSectionGlobalDataLength == section->names_len, "data.ulSectionGlobalDataLength=%u\n",
       data.ulSectionGlobalDataLength);

    /* test some actual data */
    tlib = (struct tlibredirect_data*)data.lpData;
    ok(tlib->size == sizeof(*tlib), "got %d\n", tlib->size);
    ok(tlib->major_version == 1, "got %d\n", tlib->major_version);
    ok(tlib->minor_version == 0, "got %d\n", tlib->minor_version);
    ok(tlib->help_offset > 0, "got %d\n", tlib->help_offset);
    ok(tlib->help_len == sizeof(helpW), "got %d\n", tlib->help_len);
    ok(tlib->flags == (LIBFLAG_FHIDDEN|LIBFLAG_FCONTROL|LIBFLAG_FRESTRICTED), "got %x\n", tlib->flags);

    ret = DeactivateActCtx(0, cookie);
    ok(ret, "DeactivateActCtx failed: %u\n", GetLastError());

    ReleaseActCtx(handle);
}

static void test_allowDelayedBinding(void)
{
    HANDLE handle;

    if (!create_manifest_file("test5.manifest", manifest5, -1, NULL, NULL)) {
        skip("Could not create manifest file\n");
        return;
    }

    handle = test_create("test5.manifest");
    if (handle == INVALID_HANDLE_VALUE) {
        win_skip("allowDelayedBinding attribute is not supported.\n");
        return;
    }

    DeleteFileA("test5.manifest");
    DeleteFileA("testdep.manifest");
    if (handle != INVALID_HANDLE_VALUE) {
        test_basic_info(handle, __LINE__);
        ReleaseActCtx(handle);
    }
}

static void test_actctx(void)
{
    ULONG_PTR cookie;
    HANDLE handle;
    BOOL b;

    trace("default actctx\n");

    b = GetCurrentActCtx(&handle);
    ok(handle == NULL, "handle = %p, expected NULL\n", handle);
    ok(b, "GetCurrentActCtx failed: %u\n", GetLastError());
    if(b) {
        test_basic_info(handle, __LINE__);
        test_detailed_info(handle, &detailed_info0, __LINE__);
        test_runlevel_info(handle, &runlevel_info0, __LINE__);
        ReleaseActCtx(handle);
    }

    /* test for whitespace handling in Eq ::= S? '=' S? */
    create_manifest_file("test1_1.manifest", manifest1_1, -1, NULL, NULL);
    handle = test_create("test1_1.manifest");
    ok(handle != INVALID_HANDLE_VALUE, "handle == INVALID_HANDLE_VALUE, error %u\n", GetLastError());
    DeleteFileA("test1_1.manifest");
    ReleaseActCtx(handle);

    if(!create_manifest_file("test1.manifest", manifest1, -1, NULL, NULL)) {
        skip("Could not create manifest file\n");
        return;
    }

    trace("manifest1\n");

    handle = test_create("test1.manifest");
    ok(handle != INVALID_HANDLE_VALUE, "handle == INVALID_HANDLE_VALUE, error %u\n", GetLastError());
    DeleteFileA("test1.manifest");
    if(handle != INVALID_HANDLE_VALUE) {
        test_basic_info(handle, __LINE__);
        test_detailed_info(handle, &detailed_info1, __LINE__);
        test_info_in_assembly(handle, 1, &manifest1_info, __LINE__);

        if (!IsDebuggerPresent())
        {
            /* CloseHandle will generate an exception if a debugger is present */
            b = CloseHandle(handle);
            ok(!b, "CloseHandle succeeded\n");
            ok(GetLastError() == ERROR_INVALID_HANDLE, "GetLastError() == %u\n", GetLastError());
        }

        ReleaseActCtx(handle);
    }

    if(!create_manifest_file("test2.manifest", manifest2, -1, "testdep.manifest", testdep_manifest1)) {
        skip("Could not create manifest file\n");
        return;
    }

    trace("manifest2 depmanifest1\n");

    handle = test_create("test2.manifest");
    ok(handle != INVALID_HANDLE_VALUE, "handle == INVALID_HANDLE_VALUE, error %u\n", GetLastError());
    DeleteFileA("test2.manifest");
    DeleteFileA("testdep.manifest");
    if(handle != INVALID_HANDLE_VALUE) {
        test_basic_info(handle, __LINE__);
        test_detailed_info(handle, &detailed_info2, __LINE__);
        test_info_in_assembly(handle, 1, &manifest2_info, __LINE__);
        test_info_in_assembly(handle, 2, &depmanifest1_info, __LINE__);
        ReleaseActCtx(handle);
    }

    if(!create_manifest_file("test2-2.manifest", manifest2, -1, "testdep.manifest", testdep_manifest2)) {
        skip("Could not create manifest file\n");
        return;
    }

    trace("manifest2 depmanifest2\n");

    handle = test_create("test2-2.manifest");
    ok(handle != INVALID_HANDLE_VALUE, "handle == INVALID_HANDLE_VALUE, error %u\n", GetLastError());
    DeleteFileA("test2-2.manifest");
    DeleteFileA("testdep.manifest");
    if(handle != INVALID_HANDLE_VALUE) {
        test_basic_info(handle, __LINE__);
        test_detailed_info(handle, &detailed_info2, __LINE__);
        test_info_in_assembly(handle, 1, &manifest2_info, __LINE__);
        test_info_in_assembly(handle, 2, &depmanifest2_info, __LINE__);
        test_file_info(handle, 1, 0, testlib_dll, __LINE__);
        test_file_info(handle, 1, 1, testlib2_dll, __LINE__);

        b = ActivateActCtx(handle, &cookie);
        ok(b, "ActivateActCtx failed: %u\n", GetLastError());
        test_find_dll_redirection(handle, testlib_dll, 2, __LINE__);
        test_find_dll_redirection(handle, testlib2_dll, 2, __LINE__);
        b = DeactivateActCtx(0, cookie);
        ok(b, "DeactivateActCtx failed: %u\n", GetLastError());

        ReleaseActCtx(handle);
    }

    trace("manifest2 depmanifest3\n");

    if(!create_manifest_file("test2-3.manifest", manifest2, -1, "testdep.manifest", testdep_manifest3)) {
        skip("Could not create manifest file\n");
        return;
    }

    handle = test_create("test2-3.manifest");
    ok(handle != INVALID_HANDLE_VALUE, "handle == INVALID_HANDLE_VALUE, error %u\n", GetLastError());
    DeleteFileA("test2-3.manifest");
    DeleteFileA("testdep.manifest");
    if(handle != INVALID_HANDLE_VALUE) {
        test_basic_info(handle, __LINE__);
        test_detailed_info(handle, &detailed_info2, __LINE__);
        test_info_in_assembly(handle, 1, &manifest2_info, __LINE__);
        test_info_in_assembly(handle, 2, &depmanifest3_info, __LINE__);
        test_file_info(handle, 1, 0, testlib_dll, __LINE__);
        test_file_info(handle, 1, 1, testlib2_dll, __LINE__);

        b = ActivateActCtx(handle, &cookie);
        ok(b, "ActivateActCtx failed: %u\n", GetLastError());
        test_find_dll_redirection(handle, testlib_dll, 2, __LINE__);
        test_find_dll_redirection(handle, testlib2_dll, 2, __LINE__);
        test_find_window_class(handle, wndClassW, 2, __LINE__);
        test_find_window_class(handle, wndClass2W, 2, __LINE__);
        b = DeactivateActCtx(0, cookie);
        ok(b, "DeactivateActCtx failed: %u\n", GetLastError());

        ReleaseActCtx(handle);
    }

    trace("manifest3\n");

    if(!create_manifest_file("test3.manifest", manifest3, -1, NULL, NULL)) {
        skip("Could not create manifest file\n");
        return;
    }

    handle = test_create("test3.manifest");
    ok(handle != INVALID_HANDLE_VALUE || broken(handle == INVALID_HANDLE_VALUE) /* XP pre-SP2, win2k3 w/o SP */,
        "handle == INVALID_HANDLE_VALUE, error %u\n", GetLastError());
    if (handle == INVALID_HANDLE_VALUE)
        win_skip("Some activation context features not supported, skipping a test (possibly old XP/Win2k3 system\n");
    DeleteFileA("test3.manifest");
    if(handle != INVALID_HANDLE_VALUE) {
        static const WCHAR nameW[] = {'t','e','s','t','s','u','r','r','o','g','a','t','e',0};
        static const WCHAR versionW[] = {'v','2','.','0','.','5','0','7','2','7',0};
        static const WCHAR progidW[] = {'P','r','o','g','I','d','.','P','r','o','g','I','d',0};
        static const WCHAR clrprogidW[] = {'c','l','r','p','r','o','g','i','d',0};

        test_basic_info(handle, __LINE__);
        test_detailed_info(handle, &detailed_info1, __LINE__);
        test_info_in_assembly(handle, 1, &manifest3_info, __LINE__);
        test_file_info(handle, 0, 0, testlib_dll, __LINE__);

        b = ActivateActCtx(handle, &cookie);
        ok(b, "ActivateActCtx failed: %u\n", GetLastError());
        test_find_dll_redirection(handle, testlib_dll, 1, __LINE__);
        test_find_dll_redirection(handle, testlib_dll, 1, __LINE__);
        test_find_com_redirection(handle, &IID_CoTest, &IID_TlibTest, progidW, 1, __LINE__);
        test_find_com_redirection(handle, &IID_CoTest2, NULL, NULL, 1, __LINE__);
        test_find_com_redirection(handle, &CLSID_clrclass, &IID_TlibTest, clrprogidW, 1, __LINE__);
        test_find_progid_redirection(handle, &IID_CoTest, "ProgId.ProgId", 1, __LINE__);
        test_find_progid_redirection(handle, &IID_CoTest, "ProgId.ProgId.1", 1, __LINE__);
        test_find_progid_redirection(handle, &IID_CoTest, "ProgId.ProgId.2", 1, __LINE__);
        test_find_progid_redirection(handle, &IID_CoTest, "ProgId.ProgId.3", 1, __LINE__);
        test_find_progid_redirection(handle, &IID_CoTest, "ProgId.ProgId.4", 1, __LINE__);
        test_find_progid_redirection(handle, &IID_CoTest, "ProgId.ProgId.5", 1, __LINE__);
        test_find_progid_redirection(handle, &IID_CoTest, "ProgId.ProgId.6", 1, __LINE__);
        test_find_progid_redirection(handle, &CLSID_clrclass, "clrprogid", 1, __LINE__);
        test_find_progid_redirection(handle, &CLSID_clrclass, "clrprogid.1", 1, __LINE__);
        test_find_progid_redirection(handle, &CLSID_clrclass, "clrprogid.2", 1, __LINE__);
        test_find_progid_redirection(handle, &CLSID_clrclass, "clrprogid.3", 1, __LINE__);
        test_find_progid_redirection(handle, &CLSID_clrclass, "clrprogid.4", 1, __LINE__);
        test_find_progid_redirection(handle, &CLSID_clrclass, "clrprogid.5", 1, __LINE__);
        test_find_progid_redirection(handle, &CLSID_clrclass, "clrprogid.6", 1, __LINE__);
        test_find_surrogate(handle, &IID_Iiface, nameW, versionW, 1, __LINE__);
        test_find_ifaceps_redirection(handle, &IID_Iifaceps, &IID_TlibTest4, &IID_Ibifaceps, NULL, 1, __LINE__);
        test_find_ifaceps_redirection(handle, &IID_Iifaceps2, &IID_TlibTest4, &IID_Ibifaceps, &IID_PS32, 1, __LINE__);
        test_find_ifaceps_redirection(handle, &IID_Iifaceps3, &IID_TlibTest4, &IID_Ibifaceps, NULL, 1, __LINE__);
        test_find_string_fail();

        b = DeactivateActCtx(0, cookie);
        ok(b, "DeactivateActCtx failed: %u\n", GetLastError());
        ReleaseActCtx(handle);
    }

    trace("manifest6\n");

    if(create_manifest_file("test6.manifest", manifest6, -1, NULL, NULL)) {
        handle = test_create("test6.manifest");
        ok(handle != INVALID_HANDLE_VALUE || broken(handle == INVALID_HANDLE_VALUE) /* WinXP */,
            "Unexpected context handle %p.\n", handle);
        DeleteFileA("test6.manifest");
        DeleteFileA("testdep.manifest");
        if(handle != INVALID_HANDLE_VALUE)
        {
            test_runlevel_info(handle, &runlevel_info6, __LINE__);
            ReleaseActCtx(handle);
        }
    }
    else
        skip("Could not create manifest file 6\n");

    trace("manifest7\n");

    if(create_manifest_file("test7.manifest", manifest7, -1, NULL, NULL)) {
        handle = test_create("test7.manifest");
        ok(handle != INVALID_HANDLE_VALUE, "handle == INVALID_HANDLE_VALUE, error %u\n", GetLastError());
        DeleteFileA("test7.manifest");
        DeleteFileA("testdep.manifest");
        if(handle != INVALID_HANDLE_VALUE)
        {
            test_runlevel_info(handle, &runlevel_info7, __LINE__);
            ReleaseActCtx(handle);
        }
    }
    else
        skip("Could not create manifest file 7\n");

    trace("manifest8\n");

    if(create_manifest_file("test8.manifest", manifest8, -1, NULL, NULL)) {
        handle = test_create("test8.manifest");
        ok(handle != INVALID_HANDLE_VALUE, "handle == INVALID_HANDLE_VALUE, error %u\n", GetLastError());
        DeleteFileA("test8.manifest");
        DeleteFileA("testdep.manifest");
        if(handle != INVALID_HANDLE_VALUE)
        {
            test_runlevel_info(handle, &runlevel_info8, __LINE__);
            ReleaseActCtx(handle);
        }
    }
    else
        skip("Could not create manifest file 8\n");

    trace("manifest9\n");

    if(create_manifest_file("test9.manifest", manifest9, -1, NULL, NULL)) {
        handle = test_create("test9.manifest");
        ok(handle != INVALID_HANDLE_VALUE, "handle == INVALID_HANDLE_VALUE, error %u\n", GetLastError());
        DeleteFileA("test9.manifest");
        DeleteFileA("testdep.manifest");
        if(handle != INVALID_HANDLE_VALUE)
        {
            test_runlevel_info(handle, &runlevel_info9, __LINE__);
            ReleaseActCtx(handle);
        }
    }
    else
        skip("Could not create manifest file 9\n");

    if(create_manifest_file("test10.manifest", manifest10, -1, NULL, NULL)) {
        handle = test_create("test10.manifest");
        ok(handle != INVALID_HANDLE_VALUE, "handle == INVALID_HANDLE_VALUE, error %u\n", GetLastError());
        DeleteFileA("test10.manifest");
        DeleteFileA("testdep.manifest");
        if(handle != INVALID_HANDLE_VALUE)
        {
            test_runlevel_info(handle, &runlevel_info8, __LINE__);
            ReleaseActCtx(handle);
        }
    }
    else
        skip("Could not create manifest file 10\n");

    if (create_manifest_file("test11.manifest", manifest11, -1, NULL, NULL))
    {
        handle = test_create("test11.manifest");
        ok(handle != INVALID_HANDLE_VALUE, "Failed to create activation context for %s, error %u\n",
                "manifest11", GetLastError());
        DeleteFileA("test11.manifest");
        if (handle != INVALID_HANDLE_VALUE)
            ReleaseActCtx(handle);
    }
    else
        skip("Could not create manifest file 11\n");

    trace("manifest4\n");

    if(!create_manifest_file("test4.manifest", manifest4, -1, NULL, NULL)) {
        skip("Could not create manifest file\n");
        return;
    }

    handle = test_create("test4.manifest");
    ok(handle != INVALID_HANDLE_VALUE, "handle == INVALID_HANDLE_VALUE, error %u\n", GetLastError());
    DeleteFileA("test4.manifest");
    DeleteFileA("testdep.manifest");
    if(handle != INVALID_HANDLE_VALUE) {
        test_basic_info(handle, __LINE__);
        test_detailed_info(handle, &detailed_info2, __LINE__);
        test_info_in_assembly(handle, 1, &manifest4_info, __LINE__);
        test_info_in_assembly(handle, 2, &manifest_comctrl_info, __LINE__);
        ReleaseActCtx(handle);
    }

    trace("manifest1 in subdir\n");

    CreateDirectoryW(work_dir_subdir, NULL);
    if (SetCurrentDirectoryW(work_dir_subdir))
    {
        if(!create_manifest_file("..\\test1.manifest", manifest1, -1, NULL, NULL)) {
            skip("Could not create manifest file\n");
            return;
        }
        handle = test_create("..\\test1.manifest");
        ok(handle != INVALID_HANDLE_VALUE, "handle == INVALID_HANDLE_VALUE, error %u\n", GetLastError());
        DeleteFileA("..\\test1.manifest");
        if(handle != INVALID_HANDLE_VALUE) {
            test_basic_info(handle, __LINE__);
            test_detailed_info(handle, &detailed_info1, __LINE__);
            test_info_in_assembly(handle, 1, &manifest1_info, __LINE__);
            ReleaseActCtx(handle);
        }
        SetCurrentDirectoryW(work_dir);
    }
    else
        skip("Couldn't change directory\n");
    RemoveDirectoryW(work_dir_subdir);

    trace("UTF-16 manifest1, with BOM\n");
    if(!create_wide_manifest("test1.manifest", manifest1, TRUE, FALSE)) {
        skip("Could not create manifest file\n");
        return;
    }

    handle = test_create("test1.manifest");
    ok(handle != INVALID_HANDLE_VALUE, "handle == INVALID_HANDLE_VALUE, error %u\n", GetLastError());
    DeleteFileA("test1.manifest");
    if (handle != INVALID_HANDLE_VALUE) {
        test_basic_info(handle, __LINE__);
        test_detailed_info(handle, &detailed_info1, __LINE__);
        test_info_in_assembly(handle, 1, &manifest1_info, __LINE__);
        ReleaseActCtx(handle);
    }

    trace("UTF-16 manifest1, reverse endian, with BOM\n");
    if(!create_wide_manifest("test1.manifest", manifest1, TRUE, TRUE)) {
        skip("Could not create manifest file\n");
        return;
    }

    handle = test_create("test1.manifest");
    ok(handle != INVALID_HANDLE_VALUE, "handle == INVALID_HANDLE_VALUE, error %u\n", GetLastError());
    DeleteFileA("test1.manifest");
    if (handle != INVALID_HANDLE_VALUE) {
        test_basic_info(handle, __LINE__);
        test_detailed_info(handle, &detailed_info1, __LINE__);
        test_info_in_assembly(handle, 1, &manifest1_info, __LINE__);
        ReleaseActCtx(handle);
    }

    test_wndclass_section();
    test_dllredirect_section();
    test_typelib_section();
    test_allowDelayedBinding();
}

static void test_app_manifest(void)
{
    HANDLE handle;
    BOOL b;

    trace("child process manifest1\n");

    b = GetCurrentActCtx(&handle);
    ok(handle == NULL, "handle != NULL\n");
    ok(b, "GetCurrentActCtx failed: %u\n", GetLastError());
    if(b) {
        test_basic_info(handle, __LINE__);
        test_detailed_info(handle, &detailed_info1_child, __LINE__);
        test_info_in_assembly(handle, 1, &manifest1_child_info, __LINE__);
        ReleaseActCtx(handle);
    }
}

static HANDLE create_manifest(const char *filename, const char *data, int line)
{
    HANDLE handle;
    create_manifest_file(filename, data, -1, NULL, NULL);

    handle = test_create(filename);
    ok_(__FILE__, line)(handle != INVALID_HANDLE_VALUE,
        "handle == INVALID_HANDLE_VALUE, error %u\n", GetLastError());

    DeleteFileA(filename);
    return handle;
}

static void kernel32_find(ULONG section, const char *string_to_find, BOOL should_find, int line)
{
    UNICODE_STRING string_to_findW;
    ACTCTX_SECTION_KEYED_DATA data;
    BOOL ret;
    DWORD err;

    pRtlCreateUnicodeStringFromAsciiz(&string_to_findW, string_to_find);

    memset(&data, 0xfe, sizeof(data));
    data.cbSize = sizeof(data);

    SetLastError(0);
    ret = FindActCtxSectionStringA(0, NULL, section, string_to_find, &data);
    err = GetLastError();
    ok_(__FILE__, line)(ret == should_find,
        "FindActCtxSectionStringA: expected ret = %u, got %u\n", should_find, ret);
    ok_(__FILE__, line)(err == (should_find ? ERROR_SUCCESS : ERROR_SXS_KEY_NOT_FOUND),
        "FindActCtxSectionStringA: unexpected error %u\n", err);

    memset(&data, 0xfe, sizeof(data));
    data.cbSize = sizeof(data);

    SetLastError(0);
    ret = FindActCtxSectionStringW(0, NULL, section, string_to_findW.Buffer, &data);
    err = GetLastError();
    ok_(__FILE__, line)(ret == should_find,
        "FindActCtxSectionStringW: expected ret = %u, got %u\n", should_find, ret);
    ok_(__FILE__, line)(err == (should_find ? ERROR_SUCCESS : ERROR_SXS_KEY_NOT_FOUND),
        "FindActCtxSectionStringW: unexpected error %u\n", err);

    SetLastError(0);
    ret = FindActCtxSectionStringA(0, NULL, section, string_to_find, NULL);
    err = GetLastError();
    ok_(__FILE__, line)(!ret,
        "FindActCtxSectionStringA: expected failure, got %u\n", ret);
    ok_(__FILE__, line)(err == ERROR_INVALID_PARAMETER,
        "FindActCtxSectionStringA: unexpected error %u\n", err);

    SetLastError(0);
    ret = FindActCtxSectionStringW(0, NULL, section, string_to_findW.Buffer, NULL);
    err = GetLastError();
    ok_(__FILE__, line)(!ret,
        "FindActCtxSectionStringW: expected failure, got %u\n", ret);
    ok_(__FILE__, line)(err == ERROR_INVALID_PARAMETER,
        "FindActCtxSectionStringW: unexpected error %u\n", err);

    pRtlFreeUnicodeString(&string_to_findW);
}

static void ntdll_find(ULONG section, const char *string_to_find, BOOL should_find, int line)
{
    UNICODE_STRING string_to_findW;
    ACTCTX_SECTION_KEYED_DATA data;
    NTSTATUS ret;

    pRtlCreateUnicodeStringFromAsciiz(&string_to_findW, string_to_find);

    memset(&data, 0xfe, sizeof(data));
    data.cbSize = sizeof(data);

    ret = pRtlFindActivationContextSectionString(0, NULL, section, &string_to_findW, &data);
    ok_(__FILE__, line)(ret == (should_find ? STATUS_SUCCESS : STATUS_SXS_KEY_NOT_FOUND),
        "RtlFindActivationContextSectionString: unexpected status 0x%x\n", ret);

    ret = pRtlFindActivationContextSectionString(0, NULL, section, &string_to_findW, NULL);
    ok_(__FILE__, line)(ret == (should_find ? STATUS_SUCCESS : STATUS_SXS_KEY_NOT_FOUND),
        "RtlFindActivationContextSectionString: unexpected status 0x%x\n", ret);

    pRtlFreeUnicodeString(&string_to_findW);
}

static void test_findsectionstring(void)
{
    HANDLE handle;
    BOOL ret;
    ULONG_PTR cookie;

    handle = create_manifest("test.manifest", testdep_manifest3, __LINE__);
    ret = ActivateActCtx(handle, &cookie);
    ok(ret, "ActivateActCtx failed: %u\n", GetLastError());

    /* first we show the parameter validation from kernel32 */
    kernel32_find(ACTIVATION_CONTEXT_SECTION_ASSEMBLY_INFORMATION, "testdep", FALSE, __LINE__);
    kernel32_find(ACTIVATION_CONTEXT_SECTION_DLL_REDIRECTION, "testlib.dll", TRUE, __LINE__);
    kernel32_find(ACTIVATION_CONTEXT_SECTION_DLL_REDIRECTION, "testlib2.dll", TRUE, __LINE__);
    kernel32_find(ACTIVATION_CONTEXT_SECTION_DLL_REDIRECTION, "testlib3.dll", FALSE, __LINE__);
    kernel32_find(ACTIVATION_CONTEXT_SECTION_WINDOW_CLASS_REDIRECTION, "wndClass", TRUE, __LINE__);
    kernel32_find(ACTIVATION_CONTEXT_SECTION_WINDOW_CLASS_REDIRECTION, "wndClass2", TRUE, __LINE__);
    kernel32_find(ACTIVATION_CONTEXT_SECTION_WINDOW_CLASS_REDIRECTION, "wndClass3", FALSE, __LINE__);

    /* then we show that ntdll plays by different rules */
    ntdll_find(ACTIVATION_CONTEXT_SECTION_ASSEMBLY_INFORMATION, "testdep", FALSE, __LINE__);
    ntdll_find(ACTIVATION_CONTEXT_SECTION_DLL_REDIRECTION, "testlib.dll", TRUE, __LINE__);
    ntdll_find(ACTIVATION_CONTEXT_SECTION_DLL_REDIRECTION, "testlib2.dll", TRUE, __LINE__);
    ntdll_find(ACTIVATION_CONTEXT_SECTION_DLL_REDIRECTION, "testlib3.dll", FALSE, __LINE__);
    ntdll_find(ACTIVATION_CONTEXT_SECTION_WINDOW_CLASS_REDIRECTION, "wndClass", TRUE, __LINE__);
    ntdll_find(ACTIVATION_CONTEXT_SECTION_WINDOW_CLASS_REDIRECTION, "wndClass2", TRUE, __LINE__);
    ntdll_find(ACTIVATION_CONTEXT_SECTION_WINDOW_CLASS_REDIRECTION, "wndClass3", FALSE, __LINE__);

    ret = DeactivateActCtx(0, cookie);
    ok(ret, "DeactivateActCtx failed: %u\n", GetLastError());
    ReleaseActCtx(handle);
}

static void run_child_process(void)
{
    char cmdline[MAX_PATH];
    char path[MAX_PATH];
    char **argv;
    PROCESS_INFORMATION pi;
    STARTUPINFOA si = { 0 };
    HANDLE file;
    FILETIME now;
    BOOL ret;

    GetModuleFileNameA(NULL, path, MAX_PATH);
    strcat(path, ".manifest");
    if(!create_manifest_file(path, manifest1, -1, NULL, NULL)) {
        skip("Could not create manifest file\n");
        return;
    }

    si.cb = sizeof(si);
    winetest_get_mainargs( &argv );
    /* Vista+ seems to cache presence of .manifest files. Change last modified
       date to defeat the cache */
    file = CreateFileA(argv[0], FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE,
                       NULL, OPEN_EXISTING, 0, NULL);
    if (file != INVALID_HANDLE_VALUE) {
        GetSystemTimeAsFileTime(&now);
        SetFileTime(file, NULL, NULL, &now);
        CloseHandle(file);
    }
    sprintf(cmdline, "\"%s\" %s manifest1", argv[0], argv[1]);
    ret = CreateProcessA(argv[0], cmdline, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
    ok(ret, "Could not create process: %u\n", GetLastError());
    wait_child_process( pi.hProcess );
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    DeleteFileA(path);
}

static void init_paths(void)
{
    LPWSTR ptr;

    static const WCHAR dot_manifest[] = {'.','M','a','n','i','f','e','s','t',0};
    static const WCHAR backslash[] = {'\\',0};
    static const WCHAR subdir[] = {'T','e','s','t','S','u','b','d','i','r','\\',0};

    GetModuleFileNameW(NULL, exe_path, ARRAY_SIZE(exe_path));
    lstrcpyW(app_dir, exe_path);
    for(ptr=app_dir+lstrlenW(app_dir); *ptr != '\\' && *ptr != '/'; ptr--);
    ptr[1] = 0;

    GetCurrentDirectoryW(MAX_PATH, work_dir);
    ptr = work_dir + lstrlenW( work_dir ) - 1;
    if (*ptr != '\\' && *ptr != '/')
        lstrcatW(work_dir, backslash);
    lstrcpyW(work_dir_subdir, work_dir);
    lstrcatW(work_dir_subdir, subdir);

    GetModuleFileNameW(NULL, app_manifest_path, ARRAY_SIZE(app_manifest_path));
    lstrcpyW(app_manifest_path+lstrlenW(app_manifest_path), dot_manifest);
}

static void write_manifest(const char *filename, const char *manifest)
{
    HANDLE file;
    DWORD size;
    CHAR path[MAX_PATH];

    GetTempPathA(ARRAY_SIZE(path), path);
    strcat(path, filename);

    file = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    ok(file != INVALID_HANDLE_VALUE, "CreateFile failed: %u\n", GetLastError());
    WriteFile(file, manifest, strlen(manifest), &size, NULL);
    CloseHandle(file);
}

static void delete_manifest_file(const char *filename)
{
    CHAR path[MAX_PATH];

    GetTempPathA(ARRAY_SIZE(path), path);
    strcat(path, filename);
    DeleteFileA(path);
}

static void extract_resource(const char *name, const char *type, const char *path)
{
    DWORD written;
    HANDLE file;
    HRSRC res;
    void *ptr;

    file = CreateFileA(path, GENERIC_READ|GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, 0);
    ok(file != INVALID_HANDLE_VALUE, "file creation failed, at %s, error %d\n", path, GetLastError());

    res = FindResourceA(NULL, name, type);
    ok( res != 0, "couldn't find resource\n" );
    ptr = LockResource( LoadResource( GetModuleHandleA(NULL), res ));
    WriteFile( file, ptr, SizeofResource( GetModuleHandleA(NULL), res ), &written, NULL );
    ok( written == SizeofResource( GetModuleHandleA(NULL), res ), "couldn't write resource\n" );
    CloseHandle( file );
}

static void test_CreateActCtx(void)
{
    CHAR path[MAX_PATH], dir[MAX_PATH], dll[MAX_PATH];
    ACTCTXA actctx;
    HANDLE handle;

    GetTempPathA(ARRAY_SIZE(path), path);
    strcat(path, "main_wndcls.manifest");

    write_manifest("testdep1.manifest", manifest_wndcls1);
    write_manifest("testdep2.manifest", manifest_wndcls2);
    write_manifest("main_wndcls.manifest", manifest_wndcls_main);

    memset(&actctx, 0, sizeof(ACTCTXA));
    actctx.cbSize = sizeof(ACTCTXA);
    actctx.lpSource = path;

    /* create using lpSource without specified directory */
    handle = CreateActCtxA(&actctx);
    ok(handle != INVALID_HANDLE_VALUE, "failed to generate context, error %u\n", GetLastError());
    ReleaseActCtx(handle);

    /* with specified directory, that doesn't contain dependent assembly */
    GetWindowsDirectoryA(dir, ARRAY_SIZE(dir));

    memset(&actctx, 0, sizeof(ACTCTXA));
    actctx.cbSize = sizeof(ACTCTXA);
    actctx.dwFlags = ACTCTX_FLAG_ASSEMBLY_DIRECTORY_VALID;
    actctx.lpAssemblyDirectory = dir;
    actctx.lpSource = path;

    SetLastError(0xdeadbeef);
    handle = CreateActCtxA(&actctx);
todo_wine {
    ok(handle == INVALID_HANDLE_VALUE, "got handle %p\n", handle);
    ok(GetLastError() == ERROR_SXS_CANT_GEN_ACTCTX, "got error %d\n", GetLastError());
}
    if (handle != INVALID_HANDLE_VALUE) ReleaseActCtx(handle);

    /* with specified directory, that does contain dependent assembly */
    GetTempPathA(ARRAY_SIZE(dir), dir);
    actctx.lpAssemblyDirectory = dir;
    handle = CreateActCtxA(&actctx);
    ok(handle != INVALID_HANDLE_VALUE, "got handle %p\n", handle);
    ReleaseActCtx(handle);

    /* Should still work if we add a dll with the same name, but without manifest */
    strcpy(dll, dir);
    strcat(dll, "testdep1.dll");
    extract_resource("dummy.dll", "TESTDLL", dll);
    handle = CreateActCtxA(&actctx);
    ok(handle != INVALID_HANDLE_VALUE || broken(GetLastError() == ERROR_SXS_CANT_GEN_ACTCTX) , "got error %d\n", GetLastError());
    ReleaseActCtx(handle);
    DeleteFileA(dll);

    delete_manifest_file("main_wndcls.manifest");
    delete_manifest_file("testdep1.manifest");
    delete_manifest_file("testdep2.manifest");

    /* ACTCTX_FLAG_HMODULE_VALID but hModule is not set */
    memset(&actctx, 0, sizeof(ACTCTXA));
    actctx.cbSize = sizeof(ACTCTXA);
    actctx.dwFlags = ACTCTX_FLAG_HMODULE_VALID;
    SetLastError(0xdeadbeef);
    handle = CreateActCtxA(&actctx);
    ok(handle == INVALID_HANDLE_VALUE, "got handle %p\n", handle);
todo_wine
    ok(GetLastError() == ERROR_SXS_CANT_GEN_ACTCTX || broken(GetLastError() == ERROR_NOT_ENOUGH_MEMORY) /* XP, win2k3 */,
        "got error %d\n", GetLastError());

    /* create from HMODULE - resource doesn't exist, lpSource is set */
    memset(&actctx, 0, sizeof(ACTCTXA));
    actctx.cbSize = sizeof(ACTCTXA);
    actctx.dwFlags = ACTCTX_FLAG_RESOURCE_NAME_VALID | ACTCTX_FLAG_HMODULE_VALID;
    actctx.lpSource = "dummyfile.dll";
    actctx.lpResourceName = MAKEINTRESOURCEA(20);
    actctx.hModule = GetModuleHandleA(NULL);

    SetLastError(0xdeadbeef);
    handle = CreateActCtxA(&actctx);
    ok(handle == INVALID_HANDLE_VALUE, "got handle %p\n", handle);
    ok(GetLastError() == ERROR_RESOURCE_TYPE_NOT_FOUND, "got error %d\n", GetLastError());

    /* load manifest from lpAssemblyDirectory directory */
    write_manifest("testdir.manifest", manifest1);
    GetTempPathA(ARRAY_SIZE(path), path);
    SetCurrentDirectoryA(path);
    strcat(path, "assembly_dir");
    strcpy(dir, path);
    strcat(path, "\\testdir.manifest");

    memset(&actctx, 0, sizeof(actctx));
    actctx.cbSize = sizeof(actctx);
    actctx.dwFlags = ACTCTX_FLAG_ASSEMBLY_DIRECTORY_VALID;
    actctx.lpSource = "testdir.manifest";
    actctx.lpAssemblyDirectory = dir;

    SetLastError(0xdeadbeef);
    handle = CreateActCtxA(&actctx);
    ok(handle == INVALID_HANDLE_VALUE, "got handle %p\n", handle);
    ok(GetLastError()==ERROR_PATH_NOT_FOUND ||
            broken(GetLastError()==ERROR_FILE_NOT_FOUND) /* WinXP */,
            "got error %d\n", GetLastError());

    CreateDirectoryA(dir, NULL);
    memset(&actctx, 0, sizeof(actctx));
    actctx.cbSize = sizeof(actctx);
    actctx.dwFlags = ACTCTX_FLAG_ASSEMBLY_DIRECTORY_VALID;
    actctx.lpSource = "testdir.manifest";
    actctx.lpAssemblyDirectory = dir;

    SetLastError(0xdeadbeef);
    handle = CreateActCtxA(&actctx);
    ok(handle == INVALID_HANDLE_VALUE, "got handle %p\n", handle);
    ok(GetLastError() == ERROR_FILE_NOT_FOUND, "got error %d\n", GetLastError());
    SetCurrentDirectoryW(work_dir);

    write_manifest("assembly_dir\\testdir.manifest", manifest1);
    memset(&actctx, 0, sizeof(actctx));
    actctx.cbSize = sizeof(actctx);
    actctx.dwFlags = ACTCTX_FLAG_ASSEMBLY_DIRECTORY_VALID;
    actctx.lpSource = "testdir.manifest";
    actctx.lpAssemblyDirectory = dir;

    handle = CreateActCtxA(&actctx);
    ok(handle != INVALID_HANDLE_VALUE, "got handle %p\n", handle);
    ReleaseActCtx(handle);

    memset(&actctx, 0, sizeof(actctx));
    actctx.cbSize = sizeof(actctx);
    actctx.dwFlags = ACTCTX_FLAG_ASSEMBLY_DIRECTORY_VALID;
    actctx.lpSource = path;
    actctx.lpAssemblyDirectory = dir;

    handle = CreateActCtxA(&actctx);
    ok(handle != INVALID_HANDLE_VALUE, "got handle %p\n", handle);
    ReleaseActCtx(handle);

    delete_manifest_file("testdir.manifest");
    delete_manifest_file("assembly_dir\\testdir.manifest");
    RemoveDirectoryA(dir);
}

static BOOL init_funcs(void)
{
    HMODULE hLibrary = GetModuleHandleA("kernel32.dll");

#define X(f) if (!(p##f = (void*)GetProcAddress(hLibrary, #f))) return FALSE;
    pQueryActCtxSettingsW = (void *)GetProcAddress( hLibrary, "QueryActCtxSettingsW" );

    hLibrary = GetModuleHandleA("ntdll.dll");
    X(RtlFindActivationContextSectionString);
    X(RtlCreateUnicodeStringFromAsciiz);
    X(RtlFreeUnicodeString);
#undef X

    return TRUE;
}

static void test_ZombifyActCtx(void)
{
    ACTIVATION_CONTEXT_BASIC_INFORMATION basicinfo;
    ULONG_PTR cookie;
    HANDLE handle, current;
    BOOL ret;

    SetLastError(0xdeadbeef);
    ret = ZombifyActCtx(NULL);
todo_wine
    ok(!ret && GetLastError() == ERROR_INVALID_PARAMETER, "got %d, error %d\n", ret, GetLastError());

    handle = create_manifest("test.manifest", testdep_manifest3, __LINE__);

    ret = GetCurrentActCtx(&current);
    ok(ret, "got %d, error %d\n", ret, GetLastError());
    ok(current == NULL, "got %p\n", current);

    ret = ActivateActCtx(handle, &cookie);
    ok(ret, "ActivateActCtx failed: %u\n", GetLastError());

    ret = GetCurrentActCtx(&current);
    ok(ret, "got %d, error %d\n", ret, GetLastError());
    ok(handle == current, "got %p, %p\n", current, handle);

    memset(&basicinfo, 0xff, sizeof(basicinfo));
    ret = QueryActCtxW(0, handle, 0, ActivationContextBasicInformation, &basicinfo, sizeof(basicinfo), NULL);
    ok(ret, "got %d, error %d\n", ret, GetLastError());
    ok(basicinfo.hActCtx == handle, "got %p\n", basicinfo.hActCtx);
    ok(basicinfo.dwFlags == 0, "got %x\n", basicinfo.dwFlags);

    memset(&basicinfo, 0xff, sizeof(basicinfo));
    ret = QueryActCtxW(QUERY_ACTCTX_FLAG_USE_ACTIVE_ACTCTX, NULL, 0, ActivationContextBasicInformation,
        &basicinfo, sizeof(basicinfo), NULL);
    ok(ret, "got %d, error %d\n", ret, GetLastError());
    ok(basicinfo.hActCtx == handle, "got %p\n", basicinfo.hActCtx);
    ok(basicinfo.dwFlags == 0, "got %x\n", basicinfo.dwFlags);

    ret = ZombifyActCtx(handle);
todo_wine
    ok(ret, "got %d\n", ret);

    memset(&basicinfo, 0xff, sizeof(basicinfo));
    ret = QueryActCtxW(0, handle, 0, ActivationContextBasicInformation, &basicinfo, sizeof(basicinfo), NULL);
    ok(ret, "got %d, error %d\n", ret, GetLastError());
    ok(basicinfo.hActCtx == handle, "got %p\n", basicinfo.hActCtx);
    ok(basicinfo.dwFlags == 0, "got %x\n", basicinfo.dwFlags);

    memset(&basicinfo, 0xff, sizeof(basicinfo));
    ret = QueryActCtxW(QUERY_ACTCTX_FLAG_USE_ACTIVE_ACTCTX, NULL, 0, ActivationContextBasicInformation,
            &basicinfo, sizeof(basicinfo), NULL);
    ok(ret, "got %d, error %d\n", ret, GetLastError());
    ok(basicinfo.hActCtx == handle, "got %p\n", basicinfo.hActCtx);
    ok(basicinfo.dwFlags == 0, "got %x\n", basicinfo.dwFlags);

    ret = GetCurrentActCtx(&current);
    ok(ret, "got %d, error %d\n", ret, GetLastError());
    ok(current == handle, "got %p\n", current);

    /* one more time */
    ret = ZombifyActCtx(handle);
todo_wine
    ok(ret, "got %d\n", ret);

    ret = DeactivateActCtx(0, cookie);
    ok(ret, "DeactivateActCtx failed: %u\n", GetLastError());
    ReleaseActCtx(handle);
}

/* Test structure to verify alignment */
typedef struct _test_act_ctx_compat_info {
    DWORD ElementCount;
    COMPATIBILITY_CONTEXT_ELEMENT Elements[10];
} test_act_ctx_compat_info;

static void test_no_compat(HANDLE handle, int line)
{
    test_act_ctx_compat_info compat_info;
    SIZE_T size;
    BOOL b;

    memset(&compat_info, 0, sizeof(compat_info));
    b = QueryActCtxW(QUERY_ACTCTX_FLAG_NO_ADDREF, handle, NULL, CompatibilityInformationInActivationContext,
            &compat_info, sizeof(compat_info), &size);

    ok_(__FILE__, line)(b, "CompatibilityInformationInActivationContext failed\n");
    ok_(__FILE__, line)(size == sizeof(DWORD), "size mismatch (got %lu, expected 4)\n", size);
    ok_(__FILE__, line)(compat_info.ElementCount == 0, "unexpected ElementCount %u\n", compat_info.ElementCount);
}

static void test_with_compat(HANDLE handle, DWORD num_compat, const GUID* expected_compat[], int line)
{
    test_act_ctx_compat_info compat_info;
    SIZE_T size;
    SIZE_T expected = sizeof(COMPATIBILITY_CONTEXT_ELEMENT) * num_compat + sizeof(DWORD);
    DWORD n;
    BOOL b;

    memset(&compat_info, 0, sizeof(compat_info));
    b = QueryActCtxW(QUERY_ACTCTX_FLAG_NO_ADDREF, handle, NULL, CompatibilityInformationInActivationContext,
            &compat_info, sizeof(compat_info), &size);

    ok_(__FILE__, line)(b, "CompatibilityInformationInActivationContext failed\n");
    ok_(__FILE__, line)(size == expected, "size mismatch (got %lu, expected %lu)\n", size, expected);
    ok_(__FILE__, line)(compat_info.ElementCount == num_compat, "unexpected ElementCount %u\n", compat_info.ElementCount);

    for (n = 0; n < num_compat; ++n)
    {
        ok_(__FILE__, line)(IsEqualGUID(&compat_info.Elements[n].Id, expected_compat[n]),
                            "got wrong clsid %s, expected %s for %u\n",
                            wine_dbgstr_guid(&compat_info.Elements[n].Id),
                            wine_dbgstr_guid(expected_compat[n]),
                            n);
        ok_(__FILE__, line)(compat_info.Elements[n].Type == ACTCTX_COMPATIBILITY_ELEMENT_TYPE_OS,
                            "Wrong type, got %u for %u\n", (DWORD)compat_info.Elements[n].Type, n);
    }
}

static void test_compatibility(void)
{
    HANDLE handle;

    /* No compat results returned */
    trace("manifest1\n");
    if(!create_manifest_file("test1.manifest", manifest1, -1, NULL, NULL))
    {
        skip("Could not create manifest file\n");
        return;
    }
    handle = test_create("test1.manifest");
    ok(handle != INVALID_HANDLE_VALUE, "handle == INVALID_HANDLE_VALUE, error %u\n", GetLastError());
    DeleteFileA("test1.manifest");
    if(handle != INVALID_HANDLE_VALUE)
    {
        char buffer[sizeof(COMPATIBILITY_CONTEXT_ELEMENT) * 2 + sizeof(DWORD)];
        SIZE_T size;
        BOOL b;

        memset(buffer, 0, sizeof(buffer));
        b = QueryActCtxW(QUERY_ACTCTX_FLAG_NO_ADDREF, handle, NULL, CompatibilityInformationInActivationContext,
                buffer, sizeof(buffer), &size);

        if (!b && GetLastError() == ERROR_INVALID_PARAMETER)
        {
            win_skip("CompatibilityInformationInActivationContext not supported.\n");
            ReleaseActCtx(handle);
            return;
        }

        test_basic_info(handle, __LINE__);
        test_no_compat(handle, __LINE__);
        ReleaseActCtx(handle);
    }

    /* Still no compat results returned */
    trace("no_supportedOs\n");
    if(!create_manifest_file("no_supportedOs.manifest", compat_manifest_no_supportedOs, -1, NULL, NULL))
    {
        skip("Could not create manifest file\n");
        return;
    }
    handle = test_create("no_supportedOs.manifest");
    ok(handle != INVALID_HANDLE_VALUE, "handle == INVALID_HANDLE_VALUE, error %u\n", GetLastError());
    DeleteFileA("no_supportedOs.manifest");
    if(handle != INVALID_HANDLE_VALUE)
    {
        test_basic_info(handle, __LINE__);
        test_no_compat(handle, __LINE__);
        ReleaseActCtx(handle);
    }

    /* Just one result returned */
    trace("manifest_vista\n");
    if(!create_manifest_file("manifest_vista.manifest", compat_manifest_vista, -1, NULL, NULL))
    {
        skip("Could not create manifest file\n");
        return;
    }
    handle = test_create("manifest_vista.manifest");
    ok(handle != INVALID_HANDLE_VALUE, "handle == INVALID_HANDLE_VALUE, error %u\n", GetLastError());
    DeleteFileA("manifest_vista.manifest");
    if(handle != INVALID_HANDLE_VALUE)
    {
        static const GUID* expect_manifest[] =
        {
            &VISTA_COMPAT_GUID
        };
        test_basic_info(handle, __LINE__);
        test_with_compat(handle, 1, expect_manifest, __LINE__);
        ReleaseActCtx(handle);
    }

    /* Show that the order is retained */
    trace("manifest_vista_7_8_10_81\n");
    if(!create_manifest_file("manifest_vista_7_8_10_81.manifest", compat_manifest_vista_7_8_10_81, -1, NULL, NULL))
    {
        skip("Could not create manifest file\n");
        return;
    }
    handle = test_create("manifest_vista_7_8_10_81.manifest");
    ok(handle != INVALID_HANDLE_VALUE, "handle == INVALID_HANDLE_VALUE, error %u\n", GetLastError());
    DeleteFileA("manifest_vista_7_8_10_81.manifest");
    if(handle != INVALID_HANDLE_VALUE)
    {
        static const GUID* expect_manifest[] =
        {
            &VISTA_COMPAT_GUID,
            &WIN7_COMPAT_GUID,
            &WIN8_COMPAT_GUID,
            &WIN10_COMPAT_GUID,
            &WIN81_COMPAT_GUID,
        };
        test_basic_info(handle, __LINE__);
        test_with_compat(handle, 5, expect_manifest, __LINE__);
        ReleaseActCtx(handle);
    }

    /* Show that even unknown GUID's are stored */
    trace("manifest_other_guid\n");
    if(!create_manifest_file("manifest_other_guid.manifest", compat_manifest_other_guid, -1, NULL, NULL))
    {
        skip("Could not create manifest file\n");
        return;
    }
    handle = test_create("manifest_other_guid.manifest");
    ok(handle != INVALID_HANDLE_VALUE, "handle == INVALID_HANDLE_VALUE, error %u\n", GetLastError());
    DeleteFileA("manifest_other_guid.manifest");
    if(handle != INVALID_HANDLE_VALUE)
    {
        static const GUID* expect_manifest[] =
        {
            &OTHER_COMPAT_GUID,
        };
        test_basic_info(handle, __LINE__);
        test_with_compat(handle, 1, expect_manifest, __LINE__);
        ReleaseActCtx(handle);
    }
}

static void test_settings(void)
{
    static const WCHAR dpiAwareW[] = {'d','p','i','A','w','a','r','e',0};
    static const WCHAR dpiAwarenessW[] = {'d','p','i','A','w','a','r','e','n','e','s','s',0};
    static const WCHAR dummyW[] = {'d','u','m','m','y',0};
    static const WCHAR trueW[] = {'t','r','u','e',0};
    static const WCHAR namespace2005W[] = {'h','t','t','p',':','/','/','s','c','h','e','m','a','s','.','m','i','c','r','o','s','o','f','t','.','c','o','m','/','S','M','I','/','2','0','0','5','/','W','i','n','d','o','w','s','S','e','t','t','i','n','g','s',0};
    static const WCHAR namespace2016W[] = {'h','t','t','p',':','/','/','s','c','h','e','m','a','s','.','m','i','c','r','o','s','o','f','t','.','c','o','m','/','S','M','I','/','2','0','1','6','/','W','i','n','d','o','w','s','S','e','t','t','i','n','g','s',0};
    WCHAR buffer[80];
    SIZE_T size;
    HANDLE handle;
    BOOL ret;

    if (!pQueryActCtxSettingsW)
    {
        win_skip( "QueryActCtxSettingsW is missing\n" );
        return;
    }
    create_manifest_file( "manifest_settings.manifest", settings_manifest, -1, NULL, NULL );
    handle = test_create("manifest_settings.manifest");
    ok( handle != INVALID_HANDLE_VALUE, "handle == INVALID_HANDLE_VALUE, error %u\n", GetLastError() );
    DeleteFileA( "manifest_settings.manifest" );

    SetLastError( 0xdeadbeef );
    ret = pQueryActCtxSettingsW( 1, handle, NULL, dpiAwareW, buffer, 80, &size );
    ok( !ret, "QueryActCtxSettingsW failed err %u\n", GetLastError() );
    ok( GetLastError() == ERROR_INVALID_PARAMETER, "wrong error %u\n", GetLastError() );
    SetLastError( 0xdeadbeef );
    ret = pQueryActCtxSettingsW( 0, handle, dummyW, dpiAwareW, buffer, 80, &size );
    ok( !ret, "QueryActCtxSettingsW failed err %u\n", GetLastError() );
    ok( GetLastError() == ERROR_INVALID_PARAMETER, "wrong error %u\n", GetLastError() );
    SetLastError( 0xdeadbeef );
    size = 0xdead;
    memset( buffer, 0xcc, sizeof(buffer) );
    ret = pQueryActCtxSettingsW( 0, handle, NULL, dpiAwareW, buffer, 80, &size );
    ok( ret, "QueryActCtxSettingsW failed err %u\n", GetLastError() );
    ok( !lstrcmpW( buffer, trueW ), "got %s\n", wine_dbgstr_w(buffer) );
    ok( size == lstrlenW( buffer ) + 1, "wrong len %lu\n", size );
    SetLastError( 0xdeadbeef );
    size = 0xdead;
    memset( buffer, 0xcc, sizeof(buffer) );
    ret = pQueryActCtxSettingsW( 0, handle, NULL, dummyW, buffer, 80, &size );
    ok( !ret, "QueryActCtxSettingsW succeeded\n" );
    ok( GetLastError() == ERROR_SXS_KEY_NOT_FOUND, "wrong error %u\n", GetLastError() );
    ok( buffer[0] == 0xcccc, "got %s\n", wine_dbgstr_w(buffer) );
    SetLastError( 0xdeadbeef );
    size = 0xdead;
    memset( buffer, 0xcc, sizeof(buffer) );
    ret = pQueryActCtxSettingsW( 0, handle, namespace2005W, dpiAwareW, buffer, 80, &size );
    ok( ret, "QueryActCtxSettingsW failed err %u\n", GetLastError() );
    ok( !lstrcmpW( buffer, trueW ), "got %s\n", wine_dbgstr_w(buffer) );
    ok( size == ARRAY_SIZE(trueW), "wrong len %lu\n", size );
    SetLastError( 0xdeadbeef );
    size = 0xdead;
    memset( buffer, 0xcc, sizeof(buffer) );
    ret = pQueryActCtxSettingsW( 0, handle, namespace2005W, dpiAwareW, buffer, lstrlenW(trueW) + 1, &size );
    ok( ret, "QueryActCtxSettingsW failed err %u\n", GetLastError() );
    ok( !lstrcmpW( buffer, trueW ), "got %s\n", wine_dbgstr_w(buffer) );
    ok( size == ARRAY_SIZE(trueW), "wrong len %lu\n", size );
    SetLastError( 0xdeadbeef );
    size = 0xdead;
    memset( buffer, 0xcc, sizeof(buffer) );
    ret = pQueryActCtxSettingsW( 0, handle, namespace2016W, dpiAwareW, buffer, lstrlenW(trueW) + 1, &size );
    ok( !ret, "QueryActCtxSettingsW succeeded\n" );
    ok( GetLastError() == ERROR_SXS_KEY_NOT_FOUND || broken( GetLastError() == ERROR_INVALID_PARAMETER ),
        "wrong error %u\n", GetLastError() );
    ok( buffer[0] == 0xcccc, "got %s\n", wine_dbgstr_w(buffer) );
    SetLastError( 0xdeadbeef );
    size = 0xdead;
    memset( buffer, 0xcc, sizeof(buffer) );
    ret = pQueryActCtxSettingsW( 0, handle, NULL, dpiAwarenessW, buffer, lstrlenW(trueW) + 1, &size );
    ok( !ret, "QueryActCtxSettingsW succeeded\n" );
    ok( GetLastError() == ERROR_SXS_KEY_NOT_FOUND, "wrong error %u\n", GetLastError() );
    ok( buffer[0] == 0xcccc, "got %s\n", wine_dbgstr_w(buffer) );
    SetLastError( 0xdeadbeef );
    size = 0xdead;
    memset( buffer, 0xcc, sizeof(buffer) );
    ret = pQueryActCtxSettingsW( 0, handle, namespace2005W, dpiAwarenessW, buffer, lstrlenW(trueW) + 1, &size );
    ok( !ret, "QueryActCtxSettingsW succeeded\n" );
    ok( GetLastError() == ERROR_SXS_KEY_NOT_FOUND, "wrong error %u\n", GetLastError() );
    ok( buffer[0] == 0xcccc, "got %s\n", wine_dbgstr_w(buffer) );
    SetLastError( 0xdeadbeef );
    size = 0xdead;
    memset( buffer, 0xcc, sizeof(buffer) );
    ret = pQueryActCtxSettingsW( 0, handle, namespace2016W, dpiAwarenessW, buffer, lstrlenW(trueW) + 1, &size );
    ok( ret  || broken( GetLastError() == ERROR_INVALID_PARAMETER ),
        "QueryActCtxSettingsW failed err %u\n", GetLastError() );
    if (ret)
    {
        ok( !lstrcmpW( buffer, trueW ), "got %s\n", wine_dbgstr_w(buffer) );
        ok( size == ARRAY_SIZE(trueW), "wrong len %lu\n", size );
    }
    else ok( buffer[0] == 0xcccc, "got %s\n", wine_dbgstr_w(buffer) );
    SetLastError( 0xdeadbeef );
    size = 0xdead;
    memset( buffer, 0xcc, sizeof(buffer) );
    ret = pQueryActCtxSettingsW( 0, handle, NULL, dpiAwareW, buffer, lstrlenW(trueW), &size );
    ok( ret, "QueryActCtxSettingsW failed err %u\n", GetLastError() );
    ok( !lstrcmpW( buffer, trueW ), "got %s\n", wine_dbgstr_w(buffer) );
    ok( size == ARRAY_SIZE(trueW), "wrong len %lu\n", size );
    SetLastError( 0xdeadbeef );
    size = 0xdead;
    memset( buffer, 0xcc, sizeof(buffer) );
    ret = pQueryActCtxSettingsW( 0, handle, NULL, dpiAwareW, buffer, lstrlenW(trueW) - 1, &size );
    ok( !ret, "QueryActCtxSettingsW failed err %u\n", GetLastError() );
    ok( GetLastError() == ERROR_INSUFFICIENT_BUFFER, "wrong error %u\n", GetLastError() );
    ok( buffer[0] == 0xcccc, "got %s\n", wine_dbgstr_w(buffer) );
    ok( size == ARRAY_SIZE(trueW), "wrong len %lu\n", size );
    ReleaseActCtx(handle);

    create_manifest_file( "manifest_settings2.manifest", settings_manifest2, -1, NULL, NULL );
    handle = test_create("manifest_settings2.manifest");
    ok( handle != INVALID_HANDLE_VALUE || broken( handle == INVALID_HANDLE_VALUE ), /* <= vista */
        "handle == INVALID_HANDLE_VALUE, error %u\n", GetLastError() );
    DeleteFileA( "manifest_settings2.manifest" );
    if (handle != INVALID_HANDLE_VALUE)
    {
        SetLastError( 0xdeadbeef );
        size = 0xdead;
        memset( buffer, 0xcc, sizeof(buffer) );
        ret = pQueryActCtxSettingsW( 0, handle, NULL, dpiAwareW, buffer, 80, &size );
        ok( ret, "QueryActCtxSettingsW failed err %u\n", GetLastError() );
        ok( !lstrcmpW( buffer, trueW ), "got %s\n", wine_dbgstr_w(buffer) );
        ok( size == lstrlenW( buffer ) + 1, "wrong len %lu\n", size );
        ReleaseActCtx(handle);
    }

    create_manifest_file( "manifest_settings3.manifest", settings_manifest3, -1, NULL, NULL );
    handle = test_create("manifest_settings3.manifest");
    ok( handle != INVALID_HANDLE_VALUE, "handle == INVALID_HANDLE_VALUE, error %u\n", GetLastError() );
    DeleteFileA( "manifest_settings3.manifest" );
    SetLastError( 0xdeadbeef );
    size = 0xdead;
    memset( buffer, 0xcc, sizeof(buffer) );
    ret = pQueryActCtxSettingsW( 0, handle, NULL, dpiAwareW, buffer, 80, &size );
    ok( !ret, "QueryActCtxSettingsW succeeded\n" );
    ok( GetLastError() == ERROR_SXS_KEY_NOT_FOUND, "wrong error %u\n", GetLastError() );
    ReleaseActCtx(handle);
}

typedef struct
{
    char path_tmp[MAX_PATH];
    char path_dll[MAX_PATH + 11];
    char path_manifest_exe[MAX_PATH + 12];
    char path_manifest_dll[MAX_PATH + 16];
    ACTCTXA context;
    ULONG_PTR cookie;
    HANDLE handle_context;
    HMODULE module;
    void (WINAPI *get_path)(char *buffer, int buffer_size);
} sxs_info;

static BOOL fill_sxs_info(sxs_info *info, const char *temp, const char *path_dll, const char *exe_manifest, const char *dll_manifest, BOOL do_load)
{
    BOOL success;

    GetTempPathA(MAX_PATH, info->path_tmp);
    strcat(info->path_tmp, temp);
    strcat(info->path_tmp, "\\");
    CreateDirectoryA(info->path_tmp, NULL);

    sprintf(info->path_dll, "%s%s", info->path_tmp, "sxs_dll.dll");
    extract_resource(path_dll, "TESTDLL", info->path_dll);

    sprintf(info->path_manifest_exe, "%s%s", info->path_tmp, "exe.manifest");
    create_manifest_file(info->path_manifest_exe, exe_manifest, -1, NULL, NULL);

    sprintf(info->path_manifest_dll, "%s%s", info->path_tmp, "sxs_dll.manifest");
    create_manifest_file(info->path_manifest_dll, dll_manifest, -1, NULL, NULL);

    info->context.cbSize = sizeof(ACTCTXA);
    info->context.lpSource = info->path_manifest_exe;
    info->context.lpAssemblyDirectory = info->path_tmp;
    info->context.dwFlags = ACTCTX_FLAG_ASSEMBLY_DIRECTORY_VALID;

    info->handle_context = CreateActCtxA(&info->context);
    ok((info->handle_context != NULL && info->handle_context != INVALID_HANDLE_VALUE )
            || broken(GetLastError() == ERROR_SXS_CANT_GEN_ACTCTX), /* XP doesn't support manifests outside of PE files */
            "CreateActCtxA failed: %d\n", GetLastError());
    if (GetLastError() == ERROR_SXS_CANT_GEN_ACTCTX)
    {
        skip("Failed to create activation context.\n");
        return FALSE;
    }

    if (do_load)
    {
        success = ActivateActCtx(info->handle_context, &info->cookie);
        ok(success, "ActivateActCtx failed: %d\n", GetLastError());

        info->module = LoadLibraryA("sxs_dll.dll");
        ok(info->module != NULL, "LoadLibrary failed\n");

        info->get_path = (void *)GetProcAddress(info->module, "get_path");
        ok(info->get_path != NULL, "GetProcAddress failed\n");

        DeactivateActCtx(0, info->cookie);
    }
    return TRUE;
}

static void clean_sxs_info(sxs_info *info)
{
    if (info->handle_context)
        ReleaseActCtx(info->handle_context);
    if (*info->path_dll)
    {
        BOOL ret = DeleteFileA(info->path_dll);
        ok(ret, "DeleteFileA failed for %s: %d\n", info->path_dll, GetLastError());
    }
    if (*info->path_manifest_exe)
    {
        BOOL ret = DeleteFileA(info->path_manifest_exe);
        ok(ret, "DeleteFileA failed for %s: %d\n", info->path_manifest_exe, GetLastError());
    }
    if (*info->path_manifest_dll)
    {
        BOOL ret = DeleteFileA(info->path_manifest_dll);
        ok(ret, "DeleteFileA failed for %s: %d\n", info->path_manifest_dll, GetLastError());
    }
    if (*info->path_tmp)
    {
        BOOL ret = RemoveDirectoryA(info->path_tmp);
        ok(ret, "RemoveDirectoryA failed for %s: %d\n", info->path_tmp, GetLastError());
    }
}

static void get_application_directory(char *buffer, int buffer_size)
{
    char *end;
    GetModuleFileNameA(NULL, buffer, buffer_size);
    end = strrchr(buffer, '\\');
    end[1] = 0;
}

/* Test loading two sxs dlls at the same time */
static void test_two_dlls_at_same_time(void)
{
    sxs_info dll_1;
    sxs_info dll_2;
    char path1[MAX_PATH], path2[MAX_PATH];

    if (!fill_sxs_info(&dll_1, "1", "dummy.dll", two_dll_manifest_exe, two_dll_manifest_dll, TRUE))
        goto cleanup1;
    if (!fill_sxs_info(&dll_2, "2", "dummy.dll", two_dll_manifest_exe, two_dll_manifest_dll, TRUE))
        goto cleanup2;

    ok(dll_1.module != dll_2.module, "Libraries are the same\n");
    dll_1.get_path(path1, sizeof(path1));
    ok(strcmp(path1, dll_1.path_dll) == 0, "Got '%s', expected '%s'\n", path1, dll_1.path_dll);
    dll_2.get_path(path2, sizeof(path2));
    ok(strcmp(path2, dll_2.path_dll) == 0, "Got '%s', expected '%s'\n", path2, dll_2.path_dll);

cleanup2:
    if (dll_2.module)
        FreeLibrary(dll_2.module);
    clean_sxs_info(&dll_2);
cleanup1:
    if (dll_1.module)
        FreeLibrary(dll_1.module);
    clean_sxs_info(&dll_1);
}

/* Test loading a normal dll and then a sxs dll with the same name */
static void test_one_sxs_and_one_local_1(void)
{
    sxs_info dll;
    char path_dll_local[MAX_PATH + 11];
    char path_application[MAX_PATH];
    HMODULE module = NULL;
    char path1[MAX_PATH], path2[MAX_PATH];
    void (WINAPI *get_path)(char *buffer, int buffer_size);

    get_application_directory(path_application, sizeof(path_application));

    sprintf(path_dll_local, "%s%s", path_application, "sxs_dll.dll");
    extract_resource("dummy.dll", "TESTDLL", path_dll_local);

    module = LoadLibraryA(path_dll_local);
    get_path = (void *)GetProcAddress(module, "get_path");

    if (!fill_sxs_info(&dll, "1", "dummy.dll", two_dll_manifest_exe, two_dll_manifest_dll, TRUE))
        goto cleanup;

    ok(dll.module != module, "Libraries are the same\n");
    dll.get_path(path1, sizeof(path1));
    ok(strcmp(path1, dll.path_dll) == 0, "Got '%s', expected '%s'\n", path1, dll.path_dll);
    get_path(path2, sizeof(path2));
    ok(strcmp(path2, path_dll_local) == 0, "Got '%s', expected '%s'\n", path2, path_dll_local);

cleanup:
    if (module)
        FreeLibrary(module);
    if (dll.module)
        FreeLibrary(dll.module);
    if (*path_dll_local)
    {
        BOOL success = DeleteFileA(path_dll_local);
        ok(success, "DeleteFileA failed for %s: %d\n", path_dll_local, GetLastError());
    }
    clean_sxs_info(&dll);
}

/* Test if sxs dll has priority over normal dll */
static void test_one_sxs_and_one_local_2(void)
{
    sxs_info dll;
    char path_dll_local[MAX_PATH + 11];
    char path_application[MAX_PATH];
    HMODULE module = NULL;
    char path1[MAX_PATH], path2[MAX_PATH];
    void (WINAPI *get_path)(char *buffer, int buffer_size);

    get_application_directory(path_application, sizeof(path_application));

    sprintf(path_dll_local, "%s%s", path_application, "sxs_dll.dll");
    extract_resource("dummy.dll", "TESTDLL", path_dll_local);

    if (!fill_sxs_info(&dll, "1", "dummy.dll", two_dll_manifest_exe, two_dll_manifest_dll, TRUE))
        goto cleanup;

    module = LoadLibraryA(path_dll_local);
    get_path = (void *)GetProcAddress(module, "get_path");

    ok(dll.module != module, "Libraries are the same\n");
    dll.get_path(path1, sizeof(path1));
    ok(strcmp(path1, dll.path_dll) == 0, "Got '%s', expected '%s'\n", path1, dll.path_dll);
    get_path(path2, sizeof(path2));
    ok(strcmp(path2, path_dll_local) == 0, "Got '%s', expected '%s'\n", path2, path_dll_local);

cleanup:
    if (module)
        FreeLibrary(module);
    if (dll.module)
        FreeLibrary(dll.module);
    if (*path_dll_local)
    {
        BOOL success = DeleteFileA(path_dll_local);
        ok(success, "DeleteFileA failed for %s: %d\n", path_dll_local, GetLastError());
    }
    clean_sxs_info(&dll);
}


/* Test if we can get a module handle from loaded normal dll while context is active */
static void test_one_with_sxs_and_GetModuleHandleA(void)
{
    sxs_info dll;
    char path_dll_local[MAX_PATH + 11];
    char path_tmp[MAX_PATH];
    HMODULE module = NULL, module_temp;
    BOOL success;

    GetTempPathA(sizeof(path_tmp), path_tmp);

    sprintf(path_dll_local, "%s%s", path_tmp, "sxs_dll.dll");
    extract_resource("dummy.dll", "TESTDLL", path_dll_local);

    module = LoadLibraryA(path_dll_local);

    if (!fill_sxs_info(&dll, "1", "dummy.dll", two_dll_manifest_exe, two_dll_manifest_dll, FALSE))
       goto cleanup;

    success = ActivateActCtx(dll.handle_context, &dll.cookie);
    ok(success, "ActivateActCtx failed: %d\n", GetLastError());

    module_temp = GetModuleHandleA("sxs_dll.dll");
    ok (module_temp == 0, "Expected 0, got %p\n", module_temp);

    DeactivateActCtx(0, dll.cookie);

cleanup:
    if (module)
        FreeLibrary(module);
    if (dll.module)
        FreeLibrary(dll.module);
    if (*path_dll_local)
    {
        success = DeleteFileA(path_dll_local);
        ok(success, "DeleteFileA failed for %s: %d\n", path_dll_local, GetLastError());
    }
    clean_sxs_info(&dll);
}

static void test_builtin_sxs(void)
{
    char path_manifest[MAX_PATH + 12];
    char path_tmp[MAX_PATH];
    HMODULE module_msvcp = 0, module_msvcr = 0;
    char path_msvcp[MAX_PATH], path_msvcr[MAX_PATH];
    ACTCTXA context;
    ULONG_PTR cookie;
    HANDLE handle_context;
    BOOL success;
    static const char *expected_path = "C:\\Windows\\WinSxS";

    GetTempPathA(sizeof(path_tmp), path_tmp);

    sprintf(path_manifest, "%s%s", path_tmp, "exe.manifest");
    create_manifest_file(path_manifest, builtin_dll_manifest, -1, NULL, NULL);

    context.cbSize = sizeof(ACTCTXA);
    context.lpSource = path_manifest;
    context.lpAssemblyDirectory = path_tmp;
    context.dwFlags = ACTCTX_FLAG_ASSEMBLY_DIRECTORY_VALID;

    handle_context = CreateActCtxA(&context);
    ok((handle_context != NULL && handle_context != INVALID_HANDLE_VALUE )
        || broken(GetLastError() == ERROR_SXS_CANT_GEN_ACTCTX), /* XP doesn't support manifests outside of PE files */
        "CreateActCtxA failed: %d\n", GetLastError());
    if (GetLastError() == ERROR_SXS_CANT_GEN_ACTCTX)
    {
        skip("Failed to create activation context.\n");
        goto cleanup;
    }


    success = ActivateActCtx(handle_context, &cookie);
    ok(success, "ActivateActCtx failed: %d\n", GetLastError());

    module_msvcp = LoadLibraryA("msvcp90.dll");
    ok (module_msvcp != 0 || broken(module_msvcp == 0), "LoadLibraryA failed, %d\n", GetLastError());
    module_msvcr = LoadLibraryA("msvcr90.dll");
    ok (module_msvcr != 0 || broken(module_msvcr == 0), "LoadLibraryA failed, %d\n", GetLastError());
    if (!module_msvcp || !module_msvcr)
    {
        skip("Failed to find msvcp90 or msvcr90.\n");
        goto cleanup;
    }

    GetModuleFileNameA(module_msvcp, path_msvcp, sizeof(path_msvcp));
    GetModuleFileNameA(module_msvcr, path_msvcr, sizeof(path_msvcr));
    ok(strnicmp(expected_path, path_msvcp, strlen(expected_path)) == 0, "Expected path to start with %s, got %s\n", expected_path, path_msvcp);
    ok(strnicmp(expected_path, path_msvcr, strlen(expected_path)) == 0, "Expected path to start with %s, got %s\n", expected_path, path_msvcr);

    DeactivateActCtx(0, cookie);

cleanup:
    if (module_msvcp)
        FreeLibrary(module_msvcp);
    if (module_msvcr)
        FreeLibrary(module_msvcr);
    if (*path_manifest)
    {
        success = DeleteFileA(path_manifest);
        ok(success, "DeleteFileA failed for %s: %d\n", path_manifest, GetLastError());
    }
}

static void run_sxs_test(int run)
{
    switch(run)
    {
    case 1:
        test_two_dlls_at_same_time();
        break;
    case 2:
        test_one_sxs_and_one_local_1();
        break;
    case 3:
        test_one_sxs_and_one_local_2();
        break;
    case 4:
        test_one_with_sxs_and_GetModuleHandleA();
        break;
    case 5:
       test_builtin_sxs();
       break;
    }
}

static void run_child_process_two_dll(int run)
{
    char cmdline[MAX_PATH];
    char exe[MAX_PATH];
    char **argv;
    PROCESS_INFORMATION pi;
    STARTUPINFOA si = { 0 };
    BOOL ret;

    winetest_get_mainargs( &argv );

    if (strstr(argv[0], ".exe"))
        sprintf(exe, "%s", argv[0]);
    else
        sprintf(exe, "%s.exe", argv[0]);
    sprintf(cmdline, "\"%s\" %s two_dll %d", argv[0], argv[1], run);

    si.cb = sizeof(si);
    ret = CreateProcessA(exe, cmdline, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
    ok(ret, "Could not create process: %u\n", GetLastError());

    wait_child_process( pi.hProcess );

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
}

START_TEST(actctx)
{
    int argc;
    char **argv;

    argc = winetest_get_mainargs(&argv);

    if (!init_funcs())
    {
        win_skip("Needed functions are not available\n");
        return;
    }
    init_paths();

    if(argc > 2 && !strcmp(argv[2], "manifest1")) {
        test_app_manifest();
        return;
    }

    if (argc > 2 && !strcmp(argv[2], "two_dll"))
    {
        int run = atoi(argv[3]);
        run_sxs_test(run);
        return;
    }

    test_actctx();
    test_create_fail();
    test_CreateActCtx();
    test_findsectionstring();
    test_ZombifyActCtx();
    run_child_process();
    test_compatibility();
    test_settings();
    run_child_process_two_dll(1);
    run_child_process_two_dll(2);
    run_child_process_two_dll(3);
    run_child_process_two_dll(4);
    run_child_process_two_dll(5);
}
