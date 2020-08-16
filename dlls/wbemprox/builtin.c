/*
 * Copyright 2012 Hans Leidekker for CodeWeavers
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

#define COBJMACROS
#define NONAMELESSUNION
#define NONAMELESSSTRUCT

#include <stdarg.h>
#include <intrin.h>

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winbase.h"
#include "winsock2.h"
#include "ws2tcpip.h"
#include "initguid.h"
#include "wbemcli.h"
#include "wbemprov.h"
#include "iphlpapi.h"
#include "netioapi.h"
#include "tlhelp32.h"
#include "d3d10.h"
#include "winternl.h"
#include "winioctl.h"
#include "winsvc.h"
#include "winver.h"
#include "sddl.h"
#include "ntsecapi.h"
#include "winspool.h"
#include "setupapi.h"
#include "ntddstor.h"

#include "wine/asm.h"
#include "wine/debug.h"
#include "wbemprox_private.h"

WINE_DEFAULT_DEBUG_CHANNEL(wbemprox);

/* column definitions must be kept in sync with record structures below */
static const struct column col_associator[] =
{
    { L"AssocClass", CIM_STRING },
    { L"Class",      CIM_STRING },
    { L"Associator", CIM_STRING }
};
static const struct column col_baseboard[] =
{
    { L"Manufacturer", CIM_STRING|COL_FLAG_DYNAMIC },
    { L"Model",        CIM_STRING },
    { L"Name",         CIM_STRING },
    { L"Product",      CIM_STRING|COL_FLAG_DYNAMIC },
    { L"SerialNumber", CIM_STRING|COL_FLAG_DYNAMIC },
    { L"Tag",          CIM_STRING|COL_FLAG_KEY },
    { L"Version",      CIM_STRING|COL_FLAG_DYNAMIC },
};
static const struct column col_bios[] =
{
    { L"CurrentLanguage",                CIM_STRING },
    { L"Description",                    CIM_STRING },
    { L"EmbeddedControllerMajorVersion", CIM_UINT8 },
    { L"EmbeddedControllerMinorVersion", CIM_UINT8 },
    { L"IdentificationCode",             CIM_STRING },
    { L"Manufacturer",                   CIM_STRING|COL_FLAG_DYNAMIC },
    { L"Name",                           CIM_STRING },
    { L"ReleaseDate",                    CIM_DATETIME|COL_FLAG_DYNAMIC },
    { L"SerialNumber",                   CIM_STRING },
    { L"SMBIOSBIOSVersion",              CIM_STRING|COL_FLAG_DYNAMIC },
    { L"SMBIOSMajorVersion",             CIM_UINT16 },
    { L"SMBIOSMinorVersion",             CIM_UINT16 },
    { L"SystemBiosMajorVersion",         CIM_UINT8 },
    { L"SystemBiosMinorVersion",         CIM_UINT8 },
    { L"Version",                        CIM_STRING|COL_FLAG_KEY },
};
static const struct column col_cdromdrive[] =
{
    { L"DeviceId",    CIM_STRING|COL_FLAG_KEY },
    { L"Drive",       CIM_STRING|COL_FLAG_DYNAMIC },
    { L"MediaType",   CIM_STRING },
    { L"Name",        CIM_STRING },
    { L"PNPDeviceID", CIM_STRING },
};
static const struct column col_compsys[] =
{
    { L"Description",               CIM_STRING },
    { L"Domain",                    CIM_STRING },
    { L"DomainRole",                CIM_UINT16 },
    { L"Manufacturer",              CIM_STRING },
    { L"Model",                     CIM_STRING },
    { L"Name",                      CIM_STRING|COL_FLAG_DYNAMIC },
    { L"NumberOfLogicalProcessors", CIM_UINT32 },
    { L"NumberOfProcessors",        CIM_UINT32 },
    { L"TotalPhysicalMemory",       CIM_UINT64 },
    { L"UserName",                  CIM_STRING|COL_FLAG_DYNAMIC },
};
static const struct column col_compsysproduct[] =
{
    { L"IdentifyingNumber", CIM_STRING|COL_FLAG_DYNAMIC|COL_FLAG_KEY },
    { L"Name",              CIM_STRING|COL_FLAG_DYNAMIC|COL_FLAG_KEY },
    { L"SKUNumber",         CIM_STRING },
    { L"UUID",              CIM_STRING|COL_FLAG_DYNAMIC },
    { L"Vendor",            CIM_STRING|COL_FLAG_DYNAMIC },
    { L"Version",           CIM_STRING|COL_FLAG_DYNAMIC|COL_FLAG_KEY },
};
static const struct column col_datafile[] =
{
    { L"Name",    CIM_STRING|COL_FLAG_DYNAMIC|COL_FLAG_KEY },
    { L"Version", CIM_STRING|COL_FLAG_DYNAMIC },
};
static const struct column col_desktopmonitor[] =
{
    { L"Name",                  CIM_STRING },
    { L"PixelsPerXLogicalInch", CIM_UINT32 },
};
static const struct column col_directory[] =
{
    { L"AccessMask", CIM_UINT32 },
    { L"Name",       CIM_STRING|COL_FLAG_DYNAMIC|COL_FLAG_KEY },
};
static const struct column col_diskdrive[] =
{
    { L"DeviceId",      CIM_STRING|COL_FLAG_DYNAMIC|COL_FLAG_KEY },
    { L"Index",         CIM_UINT32 },
    { L"InterfaceType", CIM_STRING },
    { L"Manufacturer",  CIM_STRING },
    { L"MediaType",     CIM_STRING },
    { L"Model",         CIM_STRING },
    { L"PNPDeviceID",   CIM_STRING },
    { L"SerialNumber",  CIM_STRING|COL_FLAG_DYNAMIC },
    { L"Size",          CIM_UINT64 },
};
static const struct column col_diskdrivetodiskpartition[] =
{
    { L"Antecedent", CIM_REFERENCE|COL_FLAG_DYNAMIC|COL_FLAG_KEY },
    { L"Dependent",  CIM_REFERENCE|COL_FLAG_DYNAMIC|COL_FLAG_KEY },
};
static const struct column col_diskpartition[] =
{
    { L"Bootable",       CIM_BOOLEAN },
    { L"BootPartition",  CIM_BOOLEAN },
    { L"DeviceId",       CIM_STRING|COL_FLAG_DYNAMIC|COL_FLAG_KEY },
    { L"DiskIndex",      CIM_UINT32 },
    { L"Index",          CIM_UINT32 },
    { L"PNPDeviceID",    CIM_STRING|COL_FLAG_DYNAMIC },
    { L"Size",           CIM_UINT64 },
    { L"StartingOffset", CIM_UINT64 },
    { L"Type",           CIM_STRING|COL_FLAG_DYNAMIC },
};
static const struct column col_displaycontrollerconfig[] =
{
    { L"BitsPerPixel",         CIM_UINT32 },
    { L"Caption",              CIM_STRING },
    { L"HorizontalResolution", CIM_UINT32 },
    { L"Name",                 CIM_STRING|COL_FLAG_KEY },
    { L"VerticalResolution",   CIM_UINT32 },
};
static const struct column col_ip4routetable[] =
{
    { L"Destination",    CIM_STRING|COL_FLAG_DYNAMIC|COL_FLAG_KEY },
    { L"InterfaceIndex", CIM_SINT32|COL_FLAG_KEY },
    { L"NextHop",        CIM_STRING|COL_FLAG_DYNAMIC|COL_FLAG_KEY },
};
static const struct column col_logicaldisk[] =
{
    { L"DeviceId",           CIM_STRING|COL_FLAG_DYNAMIC|COL_FLAG_KEY },
    { L"DriveType",          CIM_UINT32 },
    { L"FileSystem",         CIM_STRING|COL_FLAG_DYNAMIC },
    { L"FreeSpace",          CIM_UINT64 },
    { L"Name",               CIM_STRING|COL_FLAG_DYNAMIC },
    { L"Size",               CIM_UINT64 },
    { L"VolumeName",         CIM_STRING|COL_FLAG_DYNAMIC },
    { L"VolumeSerialNumber", CIM_STRING|COL_FLAG_DYNAMIC },
};
static const struct column col_logicaldisktopartition[] =
{
    { L"Antecedent", CIM_REFERENCE|COL_FLAG_DYNAMIC|COL_FLAG_KEY },
    { L"Dependent",  CIM_REFERENCE|COL_FLAG_DYNAMIC|COL_FLAG_KEY },
};
static const struct column col_networkadapter[] =
{
    { L"AdapterType",         CIM_STRING },
    { L"AdapterTypeID",       CIM_UINT16 },
    { L"Description",         CIM_STRING|COL_FLAG_DYNAMIC },
    { L"DeviceId",            CIM_STRING|COL_FLAG_DYNAMIC|COL_FLAG_KEY },
    { L"Index",               CIM_UINT32 },
    { L"InterfaceIndex",      CIM_UINT32 },
    { L"MACAddress",          CIM_STRING|COL_FLAG_DYNAMIC },
    { L"Manufacturer",        CIM_STRING },
    { L"Name",                CIM_STRING|COL_FLAG_DYNAMIC },
    { L"NetConnectionStatus", CIM_UINT16 },
    { L"PhysicalAdapter",     CIM_BOOLEAN },
    { L"PNPDeviceID",         CIM_STRING },
    { L"Speed",               CIM_UINT64 },
};
static const struct column col_networkadapterconfig[] =
{
    { L"DefaultIPGateway",     CIM_STRING|CIM_FLAG_ARRAY|COL_FLAG_DYNAMIC },
    { L"Description",          CIM_STRING|COL_FLAG_DYNAMIC },
    { L"DHCPEnabled",          CIM_BOOLEAN },
    { L"DNSHostName",          CIM_STRING|COL_FLAG_DYNAMIC },
    { L"DNSServerSearchOrder", CIM_STRING|CIM_FLAG_ARRAY|COL_FLAG_DYNAMIC },
    { L"Index",                CIM_UINT32|COL_FLAG_KEY },
    { L"IPAddress",            CIM_STRING|CIM_FLAG_ARRAY|COL_FLAG_DYNAMIC },
    { L"IPConnectionMetric",   CIM_UINT32 },
    { L"IPEnabled",            CIM_BOOLEAN },
    { L"IPSubnet",             CIM_STRING|CIM_FLAG_ARRAY|COL_FLAG_DYNAMIC },
    { L"MACAddress",           CIM_STRING|COL_FLAG_DYNAMIC },
    { L"SettingID",            CIM_STRING|COL_FLAG_DYNAMIC },
};
static const struct column col_operatingsystem[] =
{
    { L"BuildNumber",             CIM_STRING|COL_FLAG_DYNAMIC },
    { L"Caption",                 CIM_STRING|COL_FLAG_DYNAMIC },
    { L"CodeSet",                 CIM_STRING|COL_FLAG_DYNAMIC },
    { L"CountryCode",             CIM_STRING|COL_FLAG_DYNAMIC },
    { L"CSDVersion",              CIM_STRING|COL_FLAG_DYNAMIC },
    { L"CSName",                  CIM_STRING|COL_FLAG_DYNAMIC },
    { L"CurrentTimeZone",         CIM_SINT16 },
    { L"FreePhysicalMemory",      CIM_UINT64 },
    { L"InstallDate",             CIM_DATETIME },
    { L"LastBootUpTime",          CIM_DATETIME|COL_FLAG_DYNAMIC },
    { L"LocalDateTime",           CIM_DATETIME|COL_FLAG_DYNAMIC },
    { L"Locale",                  CIM_STRING|COL_FLAG_DYNAMIC },
    { L"Manufacturer",            CIM_STRING },
    { L"Name",                    CIM_STRING|COL_FLAG_DYNAMIC },
    { L"OperatingSystemSKU",      CIM_UINT32 },
    { L"OSArchitecture",          CIM_STRING },
    { L"OSLanguage",              CIM_UINT32 },
    { L"OSProductSuite",          CIM_UINT32 },
    { L"OSType",                  CIM_UINT16 },
    { L"Primary",                 CIM_BOOLEAN },
    { L"SerialNumber",            CIM_STRING|COL_FLAG_DYNAMIC },
    { L"ServicePackMajorVersion", CIM_UINT16 },
    { L"ServicePackMinorVersion", CIM_UINT16 },
    { L"SuiteMask",               CIM_UINT32 },
    { L"SystemDirectory",         CIM_STRING|COL_FLAG_DYNAMIC },
    { L"SystemDrive",             CIM_STRING|COL_FLAG_DYNAMIC },
    { L"TotalVirtualMemorySize",  CIM_UINT64 },
    { L"TotalVisibleMemorySize",  CIM_UINT64 },
    { L"Version",                 CIM_STRING|COL_FLAG_DYNAMIC },
};
static const struct column col_param[] =
{
    { L"Class",        CIM_STRING },
    { L"Method",       CIM_STRING },
    { L"Direction",    CIM_SINT32 },
    { L"Parameter",    CIM_STRING },
    { L"Type",         CIM_UINT32 },
    { L"DefaultValue", CIM_UINT32 },
};
static const struct column col_physicalmedia[] =
{
    { L"SerialNumber",       CIM_STRING },
    { L"Tag",                CIM_STRING },
};
static const struct column col_physicalmemory[] =
{
    { L"BankLabel",            CIM_STRING },
    { L"Capacity",             CIM_UINT64 },
    { L"Caption",              CIM_STRING },
    { L"ConfiguredClockSpeed", CIM_UINT32 },
    { L"DeviceLocator",        CIM_STRING },
    { L"FormFactor",           CIM_UINT16 },
    { L"MemoryType",           CIM_UINT16 },
    { L"PartNumber",           CIM_STRING },
    { L"SerialNumber",         CIM_STRING },
};
static const struct column col_pnpentity[] =
{
    { L"DeviceId", CIM_STRING|COL_FLAG_DYNAMIC },
};
static const struct column col_printer[] =
{
    { L"Attributes",           CIM_UINT32 },
    { L"DeviceId",             CIM_STRING|COL_FLAG_DYNAMIC|COL_FLAG_KEY },
    { L"DriverName",           CIM_STRING|COL_FLAG_DYNAMIC },
    { L"HorizontalResolution", CIM_UINT32 },
    { L"Local",                CIM_BOOLEAN },
    { L"Location",             CIM_STRING|COL_FLAG_DYNAMIC },
    { L"Name",                 CIM_STRING|COL_FLAG_DYNAMIC },
    { L"Network",              CIM_BOOLEAN },
    { L"PortName",             CIM_STRING|COL_FLAG_DYNAMIC },
};
static const struct column col_process[] =
{
    { L"Caption",         CIM_STRING|COL_FLAG_DYNAMIC },
    { L"CommandLine",     CIM_STRING|COL_FLAG_DYNAMIC },
    { L"Description",     CIM_STRING|COL_FLAG_DYNAMIC },
    { L"Handle",          CIM_STRING|COL_FLAG_DYNAMIC|COL_FLAG_KEY },
    { L"Name",            CIM_STRING|COL_FLAG_DYNAMIC },
    { L"ParentProcessID", CIM_UINT32 },
    { L"ProcessID",       CIM_UINT32 },
    { L"ThreadCount",     CIM_UINT32 },
    { L"WorkingSetSize",  CIM_UINT64 },
    /* methods */
    { L"GetOwner",        CIM_FLAG_ARRAY|COL_FLAG_METHOD },
};
static const struct column col_processor[] =
{
    { L"AddressWidth",              CIM_UINT16 },
    { L"Architecture",              CIM_UINT16 },
    { L"Caption",                   CIM_STRING|COL_FLAG_DYNAMIC },
    { L"CpuStatus",                 CIM_UINT16 },
    { L"CurrentClockSpeed",         CIM_UINT32 },
    { L"DataWidth",                 CIM_UINT16 },
    { L"Description",               CIM_STRING|COL_FLAG_DYNAMIC },
    { L"DeviceId",                  CIM_STRING|COL_FLAG_DYNAMIC|COL_FLAG_KEY },
    { L"Family",                    CIM_UINT16 },
    { L"Level",                     CIM_UINT16 },
    { L"Manufacturer",              CIM_STRING|COL_FLAG_DYNAMIC },
    { L"MaxClockSpeed",             CIM_UINT32 },
    { L"Name",                      CIM_STRING|COL_FLAG_DYNAMIC },
    { L"NumberOfCores",             CIM_UINT32 },
    { L"NumberOfLogicalProcessors", CIM_UINT32 },
    { L"ProcessorId",               CIM_STRING|COL_FLAG_DYNAMIC },
    { L"ProcessorType",             CIM_UINT16 },
    { L"Revision",                  CIM_UINT16 },
    { L"UniqueId",                  CIM_STRING },
    { L"Version",                   CIM_STRING|COL_FLAG_DYNAMIC },
};
static const struct column col_qualifier[] =
{
    { L"Class",        CIM_STRING },
    { L"Member",       CIM_STRING },
    { L"Type",         CIM_UINT32 },
    { L"Flavor",       CIM_SINT32 },
    { L"Name",         CIM_STRING },
    { L"IntegerValue", CIM_SINT32 },
    { L"StringValue",  CIM_STRING },
    { L"BoolValue",    CIM_BOOLEAN },
};
static const struct column col_quickfixengineering[] =
{
    { L"Caption",  CIM_STRING },
    { L"HotFixID", CIM_STRING|COL_FLAG_KEY },
};
static const struct column col_service[] =
{
    { L"AcceptPause",   CIM_BOOLEAN },
    { L"AcceptStop",    CIM_BOOLEAN },
    { L"DisplayName",   CIM_STRING|COL_FLAG_DYNAMIC },
    { L"Name",          CIM_STRING|COL_FLAG_DYNAMIC|COL_FLAG_KEY },
    { L"ProcessID",     CIM_UINT32 },
    { L"ServiceType",   CIM_STRING },
    { L"StartMode",     CIM_STRING },
    { L"State",         CIM_STRING },
    { L"SystemName",    CIM_STRING|COL_FLAG_DYNAMIC },
    /* methods */
    { L"PauseService",  CIM_FLAG_ARRAY|COL_FLAG_METHOD },
    { L"ResumeService", CIM_FLAG_ARRAY|COL_FLAG_METHOD },
    { L"StartService",  CIM_FLAG_ARRAY|COL_FLAG_METHOD },
    { L"StopService",   CIM_FLAG_ARRAY|COL_FLAG_METHOD },
};
static const struct column col_sid[] =
{
    { L"AccountName",          CIM_STRING|COL_FLAG_DYNAMIC },
    { L"BinaryRepresentation", CIM_UINT8|CIM_FLAG_ARRAY|COL_FLAG_DYNAMIC },
    { L"ReferencedDomainName", CIM_STRING|COL_FLAG_DYNAMIC },
    { L"SID",                  CIM_STRING|COL_FLAG_DYNAMIC|COL_FLAG_KEY },
    { L"SidLength",            CIM_UINT32 },
};
static const struct column col_sounddevice[] =
{
    { L"Manufacturer", CIM_STRING },
    { L"Name",         CIM_STRING },
    { L"ProductName",  CIM_STRING },
    { L"StatusInfo",   CIM_UINT16 },
};
static const struct column col_stdregprov[] =
{
    { L"CreateKey",      CIM_FLAG_ARRAY|COL_FLAG_METHOD },
    { L"EnumKey",        CIM_FLAG_ARRAY|COL_FLAG_METHOD },
    { L"EnumValues",     CIM_FLAG_ARRAY|COL_FLAG_METHOD },
    { L"GetStringValue", CIM_FLAG_ARRAY|COL_FLAG_METHOD },
};
static const struct column col_systemenclosure[] =
{
    { L"Caption",      CIM_STRING },
    { L"ChassisTypes", CIM_UINT16|CIM_FLAG_ARRAY|COL_FLAG_DYNAMIC },
    { L"Description",  CIM_STRING },
    { L"LockPresent",  CIM_BOOLEAN },
    { L"Manufacturer", CIM_STRING|COL_FLAG_DYNAMIC },
    { L"Name",         CIM_STRING },
    { L"Tag",          CIM_STRING },
};
static const struct column col_systemsecurity[] =
{
    { L"GetSD", CIM_FLAG_ARRAY|COL_FLAG_METHOD },
    { L"SetSD", CIM_FLAG_ARRAY|COL_FLAG_METHOD },
};
static const struct column col_videocontroller[] =
{
    { L"AdapterCompatibility",        CIM_STRING },
    { L"AdapterDACType",              CIM_STRING },
    { L"AdapterRAM",                  CIM_UINT32 },
    { L"Availability",                CIM_UINT16 },
    { L"Caption",                     CIM_STRING|COL_FLAG_DYNAMIC },
    { L"ConfigManagerErrorCode",      CIM_UINT32 },
    { L"CurrentBitsPerPixel",         CIM_UINT32 },
    { L"CurrentHorizontalResolution", CIM_UINT32 },
    { L"CurrentRefreshRate",          CIM_UINT32 },
    { L"CurrentScanMode",             CIM_UINT16 },
    { L"CurrentVerticalResolution",   CIM_UINT32 },
    { L"Description",                 CIM_STRING|COL_FLAG_DYNAMIC },
    { L"DeviceId",                    CIM_STRING|COL_FLAG_KEY },
    { L"DriverDate",                  CIM_DATETIME },
    { L"DriverVersion",               CIM_STRING },
    { L"InstalledDisplayDrivers",     CIM_STRING },
    { L"Name",                        CIM_STRING|COL_FLAG_DYNAMIC },
    { L"PNPDeviceID",                 CIM_STRING|COL_FLAG_DYNAMIC },
    { L"Status",                      CIM_STRING },
    { L"VideoArchitecture",           CIM_UINT16 },
    { L"VideoMemoryType",             CIM_UINT16 },
    { L"VideoModeDescription",        CIM_STRING|COL_FLAG_DYNAMIC },
    { L"VideoProcessor",              CIM_STRING|COL_FLAG_DYNAMIC },
};
static const struct column col_winsat[] =
{
    { L"CPUScore",              CIM_REAL32 },
    { L"D3DScore",              CIM_REAL32 },
    { L"DiskScore",             CIM_REAL32 },
    { L"GraphicsScore",         CIM_REAL32 },
    { L"MemoryScore",           CIM_REAL32 },
    { L"TimeTaken",             CIM_STRING|COL_FLAG_KEY },
    { L"WinSATAssessmentState", CIM_UINT32 },
    { L"WinSPRLevel",           CIM_REAL32 },
};

#include "pshpack1.h"
struct record_associator
{
    const WCHAR *assocclass;
    const WCHAR *class;
    const WCHAR *associator;
};
struct record_baseboard
{
    const WCHAR *manufacturer;
    const WCHAR *model;
    const WCHAR *name;
    const WCHAR *product;
    const WCHAR *serialnumber;
    const WCHAR *tag;
    const WCHAR *version;
};
struct record_bios
{
    const WCHAR *currentlanguage;
    const WCHAR *description;
    UINT8        ecmajorversion;
    UINT8        ecminorversion;
    const WCHAR *identificationcode;
    const WCHAR *manufacturer;
    const WCHAR *name;
    const WCHAR *releasedate;
    const WCHAR *serialnumber;
    const WCHAR *smbiosbiosversion;
    UINT16       smbiosmajorversion;
    UINT16       smbiosminorversion;
    UINT8        systembiosmajorversion;
    UINT8        systembiosminorversion;
    const WCHAR *version;
};
struct record_cdromdrive
{
    const WCHAR *device_id;
    const WCHAR *drive;
    const WCHAR *mediatype;
    const WCHAR *name;
    const WCHAR *pnpdevice_id;
};
struct record_computersystem
{
    const WCHAR *description;
    const WCHAR *domain;
    UINT16       domainrole;
    const WCHAR *manufacturer;
    const WCHAR *model;
    const WCHAR *name;
    UINT32       num_logical_processors;
    UINT32       num_processors;
    UINT64       total_physical_memory;
    const WCHAR *username;
};
struct record_computersystemproduct
{
    const WCHAR *identifyingnumber;
    const WCHAR *name;
    const WCHAR *skunumber;
    const WCHAR *uuid;
    const WCHAR *vendor;
    const WCHAR *version;
};
struct record_datafile
{
    const WCHAR *name;
    const WCHAR *version;
};
struct record_desktopmonitor
{
    const WCHAR *name;
    UINT32       pixelsperxlogicalinch;
};
struct record_directory
{
    UINT32       accessmask;
    const WCHAR *name;
};
struct record_diskdrive
{
    const WCHAR *device_id;
    UINT32       index;
    const WCHAR *interfacetype;
    const WCHAR *manufacturer;
    const WCHAR *mediatype;
    const WCHAR *model;
    const WCHAR *pnpdevice_id;
    const WCHAR *serialnumber;
    UINT64       size;
};
struct record_diskdrivetodiskpartition
{
    const WCHAR *antecedent;
    const WCHAR *dependent;
};
struct record_diskpartition
{
    int          bootable;
    int          bootpartition;
    const WCHAR *device_id;
    UINT32       diskindex;
    UINT32       index;
    const WCHAR *pnpdevice_id;
    UINT64       size;
    UINT64       startingoffset;
    const WCHAR *type;
};
struct record_displaycontrollerconfig
{
    UINT32       bitsperpixel;
    const WCHAR *caption;
    UINT32       horizontalresolution;
    const WCHAR *name;
    UINT32       verticalresolution;
};
struct record_ip4routetable
{
    const WCHAR *destination;
    INT32        interfaceindex;
    const WCHAR *nexthop;
};
struct record_logicaldisk
{
    const WCHAR *device_id;
    UINT32       drivetype;
    const WCHAR *filesystem;
    UINT64       freespace;
    const WCHAR *name;
    UINT64       size;
    const WCHAR *volumename;
    const WCHAR *volumeserialnumber;
};
struct record_logicaldisktopartition
{
    const WCHAR *antecedent;
    const WCHAR *dependent;
};
struct record_networkadapter
{
    const WCHAR *adaptertype;
    UINT16       adaptertypeid;
    const WCHAR *description;
    const WCHAR *device_id;
    UINT32       index;
    UINT32       interface_index;
    const WCHAR *mac_address;
    const WCHAR *manufacturer;
    const WCHAR *name;
    UINT16       netconnection_status;
    int          physicaladapter;
    const WCHAR *pnpdevice_id;
    UINT64       speed;
};
struct record_networkadapterconfig
{
    const struct array *defaultipgateway;
    const WCHAR        *description;
    int                 dhcpenabled;
    const WCHAR        *dnshostname;
    const struct array *dnsserversearchorder;
    UINT32              index;
    const struct array *ipaddress;
    UINT32              ipconnectionmetric;
    int                 ipenabled;
    const struct array *ipsubnet;
    const WCHAR        *mac_address;
    const WCHAR        *settingid;
};
struct record_operatingsystem
{
    const WCHAR *buildnumber;
    const WCHAR *caption;
    const WCHAR *codeset;
    const WCHAR *countrycode;
    const WCHAR *csdversion;
    const WCHAR *csname;
    INT16        currenttimezone;
    UINT64       freephysicalmemory;
    const WCHAR *installdate;
    const WCHAR *lastbootuptime;
    const WCHAR *localdatetime;
    const WCHAR *locale;
    const WCHAR *manufacturer;
    const WCHAR *name;
    UINT32       operatingsystemsku;
    const WCHAR *osarchitecture;
    UINT32       oslanguage;
    UINT32       osproductsuite;
    UINT16       ostype;
    int          primary;
    const WCHAR *serialnumber;
    UINT16       servicepackmajor;
    UINT16       servicepackminor;
    UINT32       suitemask;
    const WCHAR *systemdirectory;
    const WCHAR *systemdrive;
    UINT64       totalvirtualmemorysize;
    UINT64       totalvisiblememorysize;
    const WCHAR *version;
};
struct record_param
{
    const WCHAR *class;
    const WCHAR *method;
    INT32        direction;
    const WCHAR *parameter;
    UINT32       type;
    UINT32       defaultvalue;
};
struct record_physicalmedia
{
    const WCHAR *serialnumber;
    const WCHAR *tag;
};
struct record_physicalmemory
{
    const WCHAR *banklabel;
    UINT64       capacity;
    const WCHAR *caption;
    UINT32       configuredclockspeed;
    const WCHAR *devicelocator;
    UINT16       formfactor;
    UINT16       memorytype;
    const WCHAR *partnumber;
    const WCHAR *serial;
};
struct record_pnpentity
{
    const WCHAR *device_id;
};
struct record_printer
{
    UINT32       attributes;
    const WCHAR *device_id;
    const WCHAR *drivername;
    UINT32       horizontalresolution;
    int          local;
    const WCHAR *location;
    const WCHAR *name;
    int          network;
    const WCHAR *portname;
};
struct record_process
{
    const WCHAR *caption;
    const WCHAR *commandline;
    const WCHAR *description;
    const WCHAR *handle;
    const WCHAR *name;
    UINT32       pprocess_id;
    UINT32       process_id;
    UINT32       thread_count;
    UINT64       workingsetsize;
    /* methods */
    class_method *get_owner;
};
struct record_processor
{
    UINT16       addresswidth;
    UINT16       architecture;
    const WCHAR *caption;
    UINT16       cpu_status;
    UINT32       currentclockspeed;
    UINT16       datawidth;
    const WCHAR *description;
    const WCHAR *device_id;
    UINT16       family;
    UINT16       level;
    const WCHAR *manufacturer;
    UINT32       maxclockspeed;
    const WCHAR *name;
    UINT32       num_cores;
    UINT32       num_logical_processors;
    const WCHAR *processor_id;
    UINT16       processortype;
    UINT16       revision;
    const WCHAR *unique_id;
    const WCHAR *version;
};
struct record_qualifier
{
    const WCHAR *class;
    const WCHAR *member;
    UINT32       type;
    INT32        flavor;
    const WCHAR *name;
    INT32        intvalue;
    const WCHAR *strvalue;
    int          boolvalue;
};
struct record_quickfixengineering
{
    const WCHAR *caption;
    const WCHAR *hotfixid;
};
struct record_service
{
    int          accept_pause;
    int          accept_stop;
    const WCHAR *displayname;
    const WCHAR *name;
    UINT32       process_id;
    const WCHAR *servicetype;
    const WCHAR *startmode;
    const WCHAR *state;
    const WCHAR *systemname;
    /* methods */
    class_method *pause_service;
    class_method *resume_service;
    class_method *start_service;
    class_method *stop_service;
};
struct record_sid
{
    const WCHAR *accountname;
    const struct array *binaryrepresentation;
    const WCHAR *referenceddomainname;
    const WCHAR *sid;
    UINT32       sidlength;
};
struct record_sounddevice
{
    const WCHAR *manufacturer;
    const WCHAR *name;
    const WCHAR *productname;
    UINT16       statusinfo;
};
struct record_stdregprov
{
    class_method *createkey;
    class_method *enumkey;
    class_method *enumvalues;
    class_method *getstringvalue;
};
struct record_systemsecurity
{
    class_method *getsd;
    class_method *setsd;
};
struct record_systemenclosure
{
    const WCHAR        *caption;
    const struct array *chassistypes;
    const WCHAR        *description;
    int                 lockpresent;
    const WCHAR        *manufacturer;
    const WCHAR        *name;
    const WCHAR        *tag;
};
struct record_videocontroller
{
    const WCHAR *adapter_compatibility;
    const WCHAR *adapter_dactype;
    UINT32       adapter_ram;
    UINT16       availability;
    const WCHAR *caption;
    UINT32       config_errorcode;
    UINT32       current_bitsperpixel;
    UINT32       current_horizontalres;
    UINT32       current_refreshrate;
    UINT16       current_scanmode;
    UINT32       current_verticalres;
    const WCHAR *description;
    const WCHAR *device_id;
    const WCHAR *driverdate;
    const WCHAR *driverversion;
    const WCHAR *installeddriver;
    const WCHAR *name;
    const WCHAR *pnpdevice_id;
    const WCHAR *status;
    UINT16       videoarchitecture;
    UINT16       videomemorytype;
    const WCHAR *videomodedescription;
    const WCHAR *videoprocessor;
};
struct record_winsat
{
    FLOAT        cpuscore;
    FLOAT        d3dscore;
    FLOAT        diskscrore;
    FLOAT        graphicsscore;
    FLOAT        memoryscore;
    const WCHAR *timetaken;
    UINT32       winsatassessmentstate;
    FLOAT        winsprlevel;
};
#include "poppack.h"

static const struct record_associator data_associator[] =
{
    { L"Win32_DiskDriveToDiskPartition", L"Win32_DiskPartition", L"Win32_DiskDrive" },
    { L"Win32_LogicalDiskToPartition", L"Win32_LogicalDisk", L"Win32_DiskPartition" },
};
static const struct record_param data_param[] =
{
    { L"__SystemSecurity", L"GetSD", -1, L"ReturnValue", CIM_UINT32 },
    { L"__SystemSecurity", L"GetSD", -1, L"SD", CIM_UINT8|CIM_FLAG_ARRAY },
    { L"__SystemSecurity", L"SetSD", 1, L"SD", CIM_UINT8|CIM_FLAG_ARRAY },
    { L"__SystemSecurity", L"SetSD", -1, L"ReturnValue", CIM_UINT32 },
    { L"StdRegProv", L"CreateKey", 1, L"hDefKey", CIM_SINT32, 0x80000002 },
    { L"StdRegProv", L"CreateKey", 1, L"sSubKeyName", CIM_STRING },
    { L"StdRegProv", L"CreateKey", -1, L"ReturnValue", CIM_UINT32 },
    { L"StdRegProv", L"EnumKey", 1, L"hDefKey", CIM_SINT32, 0x80000002 },
    { L"StdRegProv", L"EnumKey", 1, L"sSubKeyName", CIM_STRING },
    { L"StdRegProv", L"EnumKey", -1, L"ReturnValue", CIM_UINT32 },
    { L"StdRegProv", L"EnumKey", -1, L"sNames", CIM_STRING|CIM_FLAG_ARRAY },
    { L"StdRegProv", L"EnumValues", 1, L"hDefKey", CIM_SINT32, 0x80000002 },
    { L"StdRegProv", L"EnumValues", 1, L"sSubKeyName", CIM_STRING },
    { L"StdRegProv", L"EnumValues", -1, L"ReturnValue", CIM_UINT32 },
    { L"StdRegProv", L"EnumValues", -1, L"sNames", CIM_STRING|CIM_FLAG_ARRAY },
    { L"StdRegProv", L"EnumValues", -1, L"Types", CIM_SINT32|CIM_FLAG_ARRAY },
    { L"StdRegProv", L"GetStringValue", 1, L"hDefKey", CIM_SINT32, 0x80000002 },
    { L"StdRegProv", L"GetStringValue", 1, L"sSubKeyName", CIM_STRING },
    { L"StdRegProv", L"GetStringValue", 1, L"sValueName", CIM_STRING },
    { L"StdRegProv", L"GetStringValue", -1, L"ReturnValue", CIM_UINT32 },
    { L"StdRegProv", L"GetStringValue", -1, L"sValue", CIM_STRING },
    { L"Win32_Process", L"GetOwner", -1, L"ReturnValue", CIM_UINT32 },
    { L"Win32_Process", L"GetOwner", -1, L"User", CIM_STRING },
    { L"Win32_Process", L"GetOwner", -1, L"Domain", CIM_STRING },
    { L"Win32_Service", L"PauseService", -1, L"ReturnValue", CIM_UINT32 },
    { L"Win32_Service", L"ResumeService", -1, L"ReturnValue", CIM_UINT32 },
    { L"Win32_Service", L"StartService", -1, L"ReturnValue", CIM_UINT32 },
    { L"Win32_Service", L"StopService", -1, L"ReturnValue", CIM_UINT32 },
};

#define FLAVOR_ID (WBEM_FLAVOR_FLAG_PROPAGATE_TO_INSTANCE | WBEM_FLAVOR_NOT_OVERRIDABLE |\
                   WBEM_FLAVOR_ORIGIN_PROPAGATED)

static const struct record_physicalmedia data_physicalmedia[] =
{
    { L"WINEHDISK", L"\\\\.\\PHYSICALDRIVE0" }
};
static const struct record_qualifier data_qualifier[] =
{
    { L"__WIN32_PROCESS_GETOWNER_OUT", L"User", CIM_SINT32, FLAVOR_ID, L"ID", 0 },
    { L"__WIN32_PROCESS_GETOWNER_OUT", L"Domain", CIM_SINT32, FLAVOR_ID, L"ID", 1 }
};
static const struct record_quickfixengineering data_quickfixengineering[] =
{
    { L"http://winehq.org", L"KB1234567" },
};
static const struct record_sounddevice data_sounddevice[] =
{
    { L"The Wine Project", L"Wine Audio Device", L"Wine Audio Device", 3 /* enabled */ }
};
static const struct record_stdregprov data_stdregprov[] =
{
    { reg_create_key, reg_enum_key, reg_enum_values, reg_get_stringvalue }
};
static UINT16 systemenclosure_chassistypes[] =
{
    1,
};
static const struct array systemenclosure_chassistypes_array =
{
    sizeof(*systemenclosure_chassistypes),
    ARRAY_SIZE(systemenclosure_chassistypes),
    &systemenclosure_chassistypes
};
static const struct record_systemsecurity data_systemsecurity[] =
{
    { security_get_sd, security_set_sd }
};
static const struct record_winsat data_winsat[] =
{
    { 8.0f, 8.0f, 8.0f, 8.0f, 8.0f, L"MostRecentAssessment", 1 /* Valid */, 8.0f },
};

/* check if row matches condition and update status */
static BOOL match_row( const struct table *table, UINT row, const struct expr *cond, enum fill_status *status )
{
    LONGLONG val;
    UINT type;

    if (!cond)
    {
        *status = FILL_STATUS_UNFILTERED;
        return TRUE;
    }
    if (eval_cond( table, row, cond, &val, &type ) != S_OK)
    {
        *status = FILL_STATUS_FAILED;
        return FALSE;
    }
    *status = FILL_STATUS_FILTERED;
    return val != 0;
}

static BOOL resize_table( struct table *table, UINT row_count, UINT row_size )
{
    if (!table->num_rows_allocated)
    {
        if (!(table->data = heap_alloc( row_count * row_size ))) return FALSE;
        table->num_rows_allocated = row_count;
        return TRUE;
    }
    if (row_count > table->num_rows_allocated)
    {
        BYTE *data;
        UINT count = max( row_count, table->num_rows_allocated * 2 );
        if (!(data = heap_realloc( table->data, count * row_size ))) return FALSE;
        table->data = data;
        table->num_rows_allocated = count;
    }
    return TRUE;
}

#include "pshpack1.h"
struct smbios_prologue
{
    BYTE  calling_method;
    BYTE  major_version;
    BYTE  minor_version;
    BYTE  revision;
    DWORD length;
};

enum smbios_type
{
    SMBIOS_TYPE_BIOS,
    SMBIOS_TYPE_SYSTEM,
    SMBIOS_TYPE_BASEBOARD,
    SMBIOS_TYPE_CHASSIS,
};

struct smbios_header
{
    BYTE type;
    BYTE length;
    WORD handle;
};

struct smbios_baseboard
{
    struct smbios_header hdr;
    BYTE                 vendor;
    BYTE                 product;
    BYTE                 version;
    BYTE                 serial;
};

struct smbios_bios
{
    struct smbios_header hdr;
    BYTE                 vendor;
    BYTE                 version;
    WORD                 start;
    BYTE                 date;
    BYTE                 size;
    UINT64               characteristics;
    BYTE                 characteristics_ext[2];
    BYTE                 system_bios_major_release;
    BYTE                 system_bios_minor_release;
    BYTE                 ec_firmware_major_release;
    BYTE                 ec_firmware_minor_release;
};

struct smbios_chassis
{
    struct smbios_header hdr;
    BYTE                 vendor;
    BYTE                 type;
    BYTE                 version;
    BYTE                 serial;
    BYTE                 asset_tag;
};

struct smbios_system
{
    struct smbios_header hdr;
    BYTE                 vendor;
    BYTE                 product;
    BYTE                 version;
    BYTE                 serial;
    BYTE                 uuid[16];
};
#include "poppack.h"

#define RSMB (('R' << 24) | ('S' << 16) | ('M' << 8) | 'B')

static const struct smbios_header *find_smbios_entry( enum smbios_type type, const char *buf, UINT len )
{
    const char *ptr, *start;
    const struct smbios_prologue *prologue;
    const struct smbios_header *hdr;

    if (len < sizeof(struct smbios_prologue)) return NULL;
    prologue = (const struct smbios_prologue *)buf;
    if (prologue->length > len - sizeof(*prologue) || prologue->length < sizeof(*hdr)) return NULL;

    start = (const char *)(prologue + 1);
    hdr = (const struct smbios_header *)start;

    for (;;)
    {
        if ((const char *)hdr - start >= prologue->length - sizeof(*hdr)) return NULL;

        if (!hdr->length)
        {
            WARN( "invalid entry\n" );
            return NULL;
        }

        if (hdr->type == type)
        {
            if ((const char *)hdr - start + hdr->length > prologue->length) return NULL;
            break;
        }
        else /* skip other entries and their strings */
        {
            for (ptr = (const char *)hdr + hdr->length; ptr - buf < len && *ptr; ptr++)
            {
                for (; ptr - buf < len; ptr++) if (!*ptr) break;
            }
            if (ptr == (const char *)hdr + hdr->length) ptr++;
            hdr = (const struct smbios_header *)(ptr + 1);
        }
    }

    return hdr;
}

static WCHAR *get_smbios_string( BYTE id, const char *buf, UINT offset, UINT buflen )
{
    const char *ptr = buf + offset;
    UINT i = 0;

    if (!id || offset >= buflen) return NULL;
    for (ptr = buf + offset; ptr - buf < buflen && *ptr; ptr++)
    {
        if (++i == id) return heap_strdupAW( ptr );
        for (; ptr - buf < buflen; ptr++) if (!*ptr) break;
    }
    return NULL;
}

static WCHAR *get_baseboard_string( BYTE id, const char *buf, UINT len )
{
    const struct smbios_header *hdr;
    const struct smbios_baseboard *baseboard;
    UINT offset;

    if (!(hdr = find_smbios_entry( SMBIOS_TYPE_BASEBOARD, buf, len ))) return NULL;

    baseboard = (const struct smbios_baseboard *)hdr;
    offset = (const char *)baseboard - buf + baseboard->hdr.length;
    return get_smbios_string( id, buf, offset, len );
}

static WCHAR *get_baseboard_manufacturer( const char *buf, UINT len )
{
    WCHAR *ret = get_baseboard_string( 1, buf, len );
    if (!ret) return heap_strdupW( L"Intel Corporation" );
    return ret;
}

static WCHAR *get_baseboard_product( const char *buf, UINT len )
{
    WCHAR *ret = get_baseboard_string( 2, buf, len );
    if (!ret) return heap_strdupW( L"Base Board" );
    return ret;
}

static WCHAR *get_baseboard_serialnumber( const char *buf, UINT len )
{
    WCHAR *ret = get_baseboard_string( 4, buf, len );
    if (!ret) return heap_strdupW( L"None" );
    return ret;
}

static WCHAR *get_baseboard_version( const char *buf, UINT len )
{
    WCHAR *ret = get_baseboard_string( 3, buf, len );
    if (!ret) return heap_strdupW( L"1.0" );
    return ret;
}

static enum fill_status fill_baseboard( struct table *table, const struct expr *cond )
{
    struct record_baseboard *rec;
    enum fill_status status = FILL_STATUS_UNFILTERED;
    UINT row = 0, len;
    char *buf;

    if (!resize_table( table, 1, sizeof(*rec) )) return FILL_STATUS_FAILED;

    len = GetSystemFirmwareTable( RSMB, 0, NULL, 0 );
    if (!(buf = heap_alloc( len ))) return FILL_STATUS_FAILED;
    GetSystemFirmwareTable( RSMB, 0, buf, len );

    rec = (struct record_baseboard *)table->data;
    rec->manufacturer = get_baseboard_manufacturer( buf, len );
    rec->model        = L"Base Board";
    rec->name         = L"Base Board";
    rec->product      = get_baseboard_product( buf, len );
    rec->serialnumber = get_baseboard_serialnumber( buf, len );
    rec->tag          = L"Base Board";
    rec->version      = get_baseboard_version( buf, len );
    if (!match_row( table, row, cond, &status )) free_row_values( table, row );
    else row++;

    heap_free( buf );

    TRACE("created %u rows\n", row);
    table->num_rows = row;
    return status;
}

static UINT16 get_bios_smbiosmajorversion( const char *buf, UINT len )
{
    const struct smbios_prologue *prologue = (const struct smbios_prologue *)buf;
    if (len < sizeof(*prologue)) return 2;
    return prologue->major_version;
}

static UINT16 get_bios_smbiosminorversion( const char *buf, UINT len )
{
    const struct smbios_prologue *prologue = (const struct smbios_prologue *)buf;
    if (len < sizeof(*prologue)) return 0;
    return prologue->minor_version;
}

static WCHAR *get_bios_string( BYTE id, const char *buf, UINT len )
{
    const struct smbios_header *hdr;
    const struct smbios_bios *bios;
    UINT offset;

    if (!(hdr = find_smbios_entry( SMBIOS_TYPE_BIOS, buf, len ))) return NULL;

    bios = (const struct smbios_bios *)hdr;
    offset = (const char *)bios - buf + bios->hdr.length;
    return get_smbios_string( id, buf, offset, len );
}

static WCHAR *get_bios_manufacturer( const char *buf, UINT len )
{
    WCHAR *ret = get_bios_string( 1, buf, len );
    if (!ret) return heap_strdupW( L"The Wine Project" );
    return ret;
}

static WCHAR *convert_bios_date( const WCHAR *str )
{
    static const WCHAR fmtW[] = L"%04u%02u%02u000000.000000+000";
    UINT year, month, day, len = lstrlenW( str );
    const WCHAR *p = str, *q;
    WCHAR *ret;

    while (len && iswspace( *p )) { p++; len--; }
    while (len && iswspace( p[len - 1] )) { len--; }

    q = p;
    while (len && is_digit( *q )) { q++; len--; };
    if (q - p != 2 || !len || *q != '/') return NULL;
    month = (p[0] - '0') * 10 + p[1] - '0';

    p = ++q; len--;
    while (len && is_digit( *q )) { q++; len--; };
    if (q - p != 2 || !len || *q != '/') return NULL;
    day = (p[0] - '0') * 10 + p[1] - '0';

    p = ++q; len--;
    while (len && is_digit( *q )) { q++; len--; };
    if (q - p == 4) year = (p[0] - '0') * 1000 + (p[1] - '0') * 100 + (p[2] - '0') * 10 + p[3] - '0';
    else if (q - p == 2) year = 1900 + (p[0] - '0') * 10 + p[1] - '0';
    else return NULL;

    if (!(ret = heap_alloc( sizeof(fmtW) ))) return NULL;
    swprintf( ret, ARRAY_SIZE(fmtW), fmtW, year, month, day );
    return ret;
}

static WCHAR *get_bios_releasedate( const char *buf, UINT len )
{
    WCHAR *ret, *date = get_bios_string( 3, buf, len );
    if (!date || !(ret = convert_bios_date( date ))) ret = heap_strdupW( L"20120608000000.000000+000" );
    heap_free( date );
    return ret;
}

static WCHAR *get_bios_smbiosbiosversion( const char *buf, UINT len )
{
    WCHAR *ret = get_bios_string( 2, buf, len );
    if (!ret) return heap_strdupW( L"Wine" );
    return ret;
}

static BYTE get_bios_ec_firmware_major_release( const char *buf, UINT len )
{
    const struct smbios_header *hdr;
    const struct smbios_bios *bios;

    if (!(hdr = find_smbios_entry( SMBIOS_TYPE_BIOS, buf, len ))) return 0xFF;

    bios = (const struct smbios_bios *)hdr;
    if (bios->hdr.length >= 0x18) return bios->ec_firmware_major_release;
    else return 0xFF;
}

static BYTE get_bios_ec_firmware_minor_release( const char *buf, UINT len )
{
    const struct smbios_header *hdr;
    const struct smbios_bios *bios;

    if (!(hdr = find_smbios_entry( SMBIOS_TYPE_BIOS, buf, len ))) return 0xFF;

    bios = (const struct smbios_bios *)hdr;
    if (bios->hdr.length >= 0x18) return bios->ec_firmware_minor_release;
    else return 0xFF;
}

static BYTE get_bios_system_bios_major_release( const char *buf, UINT len )
{
    const struct smbios_header *hdr;
    const struct smbios_bios *bios;

    if (!(hdr = find_smbios_entry( SMBIOS_TYPE_BIOS, buf, len ))) return 0xFF;

    bios = (const struct smbios_bios *)hdr;
    if (bios->hdr.length >= 0x18) return bios->system_bios_major_release;
    else return 0xFF;
}

static BYTE get_bios_system_bios_minor_release( const char *buf, UINT len )
{
    const struct smbios_header *hdr;
    const struct smbios_bios *bios;

    if (!(hdr = find_smbios_entry( SMBIOS_TYPE_BIOS, buf, len ))) return 0xFF;

    bios = (const struct smbios_bios *)hdr;
    if (bios->hdr.length >= 0x18) return bios->system_bios_minor_release;
    else return 0xFF;
}

static enum fill_status fill_bios( struct table *table, const struct expr *cond )
{
    struct record_bios *rec;
    enum fill_status status = FILL_STATUS_UNFILTERED;
    UINT row = 0, len;
    char *buf;

    if (!resize_table( table, 1, sizeof(*rec) )) return FILL_STATUS_FAILED;

    len = GetSystemFirmwareTable( RSMB, 0, NULL, 0 );
    if (!(buf = heap_alloc( len ))) return FILL_STATUS_FAILED;
    GetSystemFirmwareTable( RSMB, 0, buf, len );

    rec = (struct record_bios *)table->data;
    rec->currentlanguage        = NULL;
    rec->description            = L"Default System BIOS";
    rec->ecmajorversion         = get_bios_ec_firmware_major_release( buf, len );
    rec->ecminorversion         = get_bios_ec_firmware_minor_release( buf, len );
    rec->identificationcode     = NULL;
    rec->manufacturer           = get_bios_manufacturer( buf, len );
    rec->name                   = L"Default System BIOS";
    rec->releasedate            = get_bios_releasedate( buf, len );
    rec->serialnumber           = L"0";
    rec->smbiosbiosversion      = get_bios_smbiosbiosversion( buf, len );
    rec->smbiosmajorversion     = get_bios_smbiosmajorversion( buf, len );
    rec->smbiosminorversion     = get_bios_smbiosminorversion( buf, len );
    rec->systembiosmajorversion = get_bios_system_bios_major_release( buf, len );
    rec->systembiosminorversion = get_bios_system_bios_minor_release( buf, len );
    rec->version                = L"WINE   - 1";
    if (!match_row( table, row, cond, &status )) free_row_values( table, row );
    else row++;

    heap_free( buf );

    TRACE("created %u rows\n", row);
    table->num_rows = row;
    return status;
}

static enum fill_status fill_cdromdrive( struct table *table, const struct expr *cond )
{
    WCHAR drive[3], root[] = {'A',':','\\',0};
    struct record_cdromdrive *rec;
    UINT i, row = 0, offset = 0;
    DWORD drives = GetLogicalDrives();
    enum fill_status status = FILL_STATUS_UNFILTERED;

    if (!resize_table( table, 1, sizeof(*rec) )) return FILL_STATUS_FAILED;

    for (i = 0; i < 26; i++)
    {
        if (drives & (1 << i))
        {
            root[0] = 'A' + i;
            if (GetDriveTypeW( root ) != DRIVE_CDROM)
                continue;

            if (!resize_table( table, row + 1, sizeof(*rec) )) return FILL_STATUS_FAILED;

            rec = (struct record_cdromdrive *)(table->data + offset);
            rec->device_id    = L"IDE\\CDROMWINE_CD-ROM_____________________________1.0_____\\5&3A2A5854&0&1.0.0";
            swprintf( drive, ARRAY_SIZE( drive ), L"%c:", 'A' + i );
            rec->drive        = heap_strdupW( drive );
            rec->mediatype    = L"CR-ROM";
            rec->name         = L"Wine CD_ROM ATA Device";
            rec->pnpdevice_id = L"IDE\\CDROMWINE_CD-ROM_____________________________1.0_____\\5&3A2A5854&0&1.0.0";
            if (!match_row( table, row, cond, &status ))
            {
                free_row_values( table, row );
                continue;
            }
            offset += sizeof(*rec);
            row++;
        }
    }
    TRACE("created %u rows\n", row);
    table->num_rows = row;
    return status;
}

static UINT get_processor_count(void)
{
    SYSTEM_BASIC_INFORMATION info;

    if (NtQuerySystemInformation( SystemBasicInformation, &info, sizeof(info), NULL )) return 1;
    return info.NumberOfProcessors;
}

static UINT get_logical_processor_count( UINT *num_physical, UINT *num_packages )
{
    SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX *buf, *entry;
    UINT core_relation_count = 0, package_relation_count = 0;
    NTSTATUS status;
    ULONG len, offset = 0;
    BOOL smt_enabled = FALSE;
    DWORD all = RelationAll;

    if (num_packages) *num_packages = 1;
    status = NtQuerySystemInformationEx( SystemLogicalProcessorInformationEx, &all, sizeof(all), NULL, 0, &len );
    if (status != STATUS_INFO_LENGTH_MISMATCH) return get_processor_count();

    if (!(buf = heap_alloc( len ))) return get_processor_count();
    status = NtQuerySystemInformationEx( SystemLogicalProcessorInformationEx, &all, sizeof(all), buf, len, NULL );
    if (status != STATUS_SUCCESS)
    {
        heap_free( buf );
        return get_processor_count();
    }

    while (offset < len)
    {
        entry = (SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX *)((char *)buf + offset);

        if (entry->Relationship == RelationProcessorCore)
        {
            core_relation_count++;
            if (entry->u.Processor.Flags & LTP_PC_SMT) smt_enabled = TRUE;
        }
        else if (entry->Relationship == RelationProcessorPackage)
        {
            package_relation_count++;
        }
        offset += entry->Size;
    }

    heap_free( buf );
    if (num_physical) *num_physical = core_relation_count;
    if (num_packages) *num_packages = package_relation_count;
    return smt_enabled ? core_relation_count * 2 : core_relation_count;
}

static UINT64 get_total_physical_memory(void)
{
    MEMORYSTATUSEX status;

    status.dwLength = sizeof(status);
    if (!GlobalMemoryStatusEx( &status )) return 1024 * 1024 * 1024;
    return status.ullTotalPhys;
}

static UINT64 get_available_physical_memory(void)
{
    MEMORYSTATUSEX status;

    status.dwLength = sizeof(status);
    if (!GlobalMemoryStatusEx( &status )) return 1024 * 1024 * 1024;
    return status.ullAvailPhys;
}

static WCHAR *get_computername(void)
{
    WCHAR *ret;
    DWORD size = MAX_COMPUTERNAME_LENGTH + 1;

    if (!(ret = heap_alloc( size * sizeof(WCHAR) ))) return NULL;
    GetComputerNameW( ret, &size );
    return ret;
}

static WCHAR *get_username(void)
{
    WCHAR *ret;
    DWORD compsize, usersize;
    DWORD size;

    compsize = 0;
    GetComputerNameW( NULL, &compsize );
    usersize = 0;
    GetUserNameW( NULL, &usersize );
    size = compsize + usersize; /* two null terminators account for the \ */
    if (!(ret = heap_alloc( size * sizeof(WCHAR) ))) return NULL;
    GetComputerNameW( ret, &compsize );
    ret[compsize] = '\\';
    GetUserNameW( ret + compsize + 1, &usersize );
    return ret;
}

static enum fill_status fill_compsys( struct table *table, const struct expr *cond )
{
    struct record_computersystem *rec;
    enum fill_status status = FILL_STATUS_UNFILTERED;
    UINT row = 0;

    if (!resize_table( table, 1, sizeof(*rec) )) return FILL_STATUS_FAILED;

    rec = (struct record_computersystem *)table->data;
    rec->description            = L"AT/AT COMPATIBLE";
    rec->domain                 = L"WORKGROUP";
    rec->domainrole             = 0; /* standalone workstation */
    rec->manufacturer           = L"The Wine Project";
    rec->model                  = L"Wine";
    rec->name                   = get_computername();
    rec->num_logical_processors = get_logical_processor_count( NULL, &rec->num_processors );
    rec->total_physical_memory  = get_total_physical_memory();
    rec->username               = get_username();
    if (!match_row( table, row, cond, &status )) free_row_values( table, row );
    else row++;

    TRACE("created %u rows\n", row);
    table->num_rows = row;
    return status;
}

static WCHAR *get_compsysproduct_string( BYTE id, const char *buf, UINT len )
{
    const struct smbios_header *hdr;
    const struct smbios_system *system;
    UINT offset;

    if (!(hdr = find_smbios_entry( SMBIOS_TYPE_SYSTEM, buf, len ))) return NULL;

    system = (const struct smbios_system *)hdr;
    offset = (const char *)system - buf + system->hdr.length;
    return get_smbios_string( id, buf, offset, len );
}

static WCHAR *get_compsysproduct_identifyingnumber( const char *buf, UINT len )
{
    WCHAR *ret = get_compsysproduct_string( 4, buf, len );
    if (!ret) return heap_strdupW( L"0" );
    return ret;
}

static WCHAR *get_compsysproduct_name( const char *buf, UINT len )
{
    WCHAR *ret = get_compsysproduct_string( 2, buf, len );
    if (!ret) return heap_strdupW( L"Wine" );
    return ret;
}

static WCHAR *get_compsysproduct_uuid( const char *buf, UINT len )
{
    static const BYTE none[] = {0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff};
    const struct smbios_header *hdr;
    const struct smbios_system *system;
    const BYTE *ptr;
    WCHAR *ret = NULL;

    if (!(hdr = find_smbios_entry( SMBIOS_TYPE_SYSTEM, buf, len )) || hdr->length < sizeof(*system)) goto done;
    system = (const struct smbios_system *)hdr;
    if (!memcmp( system->uuid, none, sizeof(none) ) || !(ret = heap_alloc( 37 * sizeof(WCHAR) ))) goto done;

    ptr = system->uuid;
    swprintf( ret, 37, L"%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X", ptr[0], ptr[1],
              ptr[2], ptr[3], ptr[4], ptr[5], ptr[6], ptr[7], ptr[8], ptr[9], ptr[10], ptr[11], ptr[12], ptr[13],
              ptr[14], ptr[15] );
done:
    if (!ret) ret = heap_strdupW( L"deaddead-dead-dead-dead-deaddeaddead" );
    return ret;
}

static WCHAR *get_compsysproduct_vendor( const char *buf, UINT len )
{
    WCHAR *ret = get_compsysproduct_string( 1, buf, len );
    if (!ret) return heap_strdupW( L"The Wine Project" );
    return ret;
}

static WCHAR *get_compsysproduct_version( const char *buf, UINT len )
{
    WCHAR *ret = get_compsysproduct_string( 3, buf, len );
    if (!ret) return heap_strdupW( L"1.0" );
    return ret;
}

static enum fill_status fill_compsysproduct( struct table *table, const struct expr *cond )
{
    struct record_computersystemproduct *rec;
    enum fill_status status = FILL_STATUS_UNFILTERED;
    UINT row = 0, len;
    char *buf;

    if (!resize_table( table, 1, sizeof(*rec) )) return FILL_STATUS_FAILED;

    len = GetSystemFirmwareTable( RSMB, 0, NULL, 0 );
    if (!(buf = heap_alloc( len ))) return FILL_STATUS_FAILED;
    GetSystemFirmwareTable( RSMB, 0, buf, len );

    rec = (struct record_computersystemproduct *)table->data;
    rec->identifyingnumber = get_compsysproduct_identifyingnumber( buf, len );
    rec->name              = get_compsysproduct_name( buf, len );
    rec->skunumber         = NULL;
    rec->uuid              = get_compsysproduct_uuid( buf, len );
    rec->vendor            = get_compsysproduct_vendor( buf, len );
    rec->version           = get_compsysproduct_version( buf, len );
    if (!match_row( table, row, cond, &status )) free_row_values( table, row );
    else row++;

    heap_free( buf );

    TRACE("created %u rows\n", row);
    table->num_rows = row;
    return status;
}

struct dirstack
{
    WCHAR **dirs;
    UINT   *len_dirs;
    UINT    num_dirs;
    UINT    num_allocated;
};

static struct dirstack *alloc_dirstack( UINT size )
{
    struct dirstack *dirstack;

    if (!(dirstack = heap_alloc( sizeof(*dirstack) ))) return NULL;
    if (!(dirstack->dirs = heap_alloc( sizeof(WCHAR *) * size )))
    {
        heap_free( dirstack );
        return NULL;
    }
    if (!(dirstack->len_dirs = heap_alloc( sizeof(UINT) * size )))
    {
        heap_free( dirstack->dirs );
        heap_free( dirstack );
        return NULL;
    }
    dirstack->num_dirs = 0;
    dirstack->num_allocated = size;
    return dirstack;
}

static void clear_dirstack( struct dirstack *dirstack )
{
    UINT i;
    for (i = 0; i < dirstack->num_dirs; i++) heap_free( dirstack->dirs[i] );
    dirstack->num_dirs = 0;
}

static void free_dirstack( struct dirstack *dirstack )
{
    clear_dirstack( dirstack );
    heap_free( dirstack->dirs );
    heap_free( dirstack->len_dirs );
    heap_free( dirstack );
}

static BOOL push_dir( struct dirstack *dirstack, WCHAR *dir, UINT len )
{
    UINT size, i = dirstack->num_dirs;

    if (!dir) return FALSE;

    if (i == dirstack->num_allocated)
    {
        WCHAR **tmp;
        UINT *len_tmp;

        size = dirstack->num_allocated * 2;
        if (!(tmp = heap_realloc( dirstack->dirs, size * sizeof(WCHAR *) ))) return FALSE;
        dirstack->dirs = tmp;
        if (!(len_tmp = heap_realloc( dirstack->len_dirs, size * sizeof(UINT) ))) return FALSE;
        dirstack->len_dirs = len_tmp;
        dirstack->num_allocated = size;
    }
    dirstack->dirs[i] = dir;
    dirstack->len_dirs[i] = len;
    dirstack->num_dirs++;
    return TRUE;
}

static WCHAR *pop_dir( struct dirstack *dirstack, UINT *len )
{
    if (!dirstack->num_dirs)
    {
        *len = 0;
        return NULL;
    }
    dirstack->num_dirs--;
    *len = dirstack->len_dirs[dirstack->num_dirs];
    return dirstack->dirs[dirstack->num_dirs];
}

static const WCHAR *peek_dir( struct dirstack *dirstack )
{
    if (!dirstack->num_dirs) return NULL;
    return dirstack->dirs[dirstack->num_dirs - 1];
}

static WCHAR *build_glob( WCHAR drive, const WCHAR *path, UINT len )
{
    UINT i = 0;
    WCHAR *ret;

    if (!(ret = heap_alloc( (len + 6) * sizeof(WCHAR) ))) return NULL;
    ret[i++] = drive;
    ret[i++] = ':';
    ret[i++] = '\\';
    if (path && len)
    {
        memcpy( ret + i, path, len * sizeof(WCHAR) );
        i += len;
        ret[i++] = '\\';
    }
    ret[i++] = '*';
    ret[i] = 0;
    return ret;
}

static WCHAR *build_name( WCHAR drive, const WCHAR *path )
{
    UINT i = 0, len = 0;
    const WCHAR *p;
    WCHAR *ret;

    for (p = path; *p; p++)
    {
        if (*p == '\\') len += 2;
        else len++;
    };
    if (!(ret = heap_alloc( (len + 5) * sizeof(WCHAR) ))) return NULL;
    ret[i++] = drive;
    ret[i++] = ':';
    ret[i++] = '\\';
    ret[i++] = '\\';
    for (p = path; *p; p++)
    {
        if (*p != '\\') ret[i++] = *p;
        else
        {
            ret[i++] = '\\';
            ret[i++] = '\\';
        }
    }
    ret[i] = 0;
    return ret;
}

static WCHAR *build_dirname( const WCHAR *path, UINT *ret_len )
{
    const WCHAR *p = path, *start;
    UINT len, i;
    WCHAR *ret;

    if (!iswalpha( p[0] ) || p[1] != ':' || p[2] != '\\' || p[3] != '\\' || !p[4]) return NULL;
    start = path + 4;
    len = lstrlenW( start );
    p = start + len - 1;
    if (*p == '\\') return NULL;

    while (p >= start && *p != '\\') { len--; p--; };
    while (p >= start && *p == '\\') { len--; p--; };

    if (!(ret = heap_alloc( (len + 1) * sizeof(WCHAR) ))) return NULL;
    for (i = 0, p = start; p < start + len; p++)
    {
        if (p[0] == '\\' && p[1] == '\\')
        {
            ret[i++] = '\\';
            p++;
        }
        else ret[i++] = *p;
    }
    ret[i] = 0;
    *ret_len = i;
    return ret;
}

static BOOL seen_dir( struct dirstack *dirstack, const WCHAR *path )
{
    UINT i;
    for (i = 0; i < dirstack->num_dirs; i++) if (!wcscmp( dirstack->dirs[i], path )) return TRUE;
    return FALSE;
}

/* optimize queries of the form WHERE Name='...' [OR Name='...']* */
static UINT seed_dirs( struct dirstack *dirstack, const struct expr *cond, WCHAR root, UINT *count )
{
    const struct expr *left, *right;

    if (!cond || cond->type != EXPR_COMPLEX) return *count = 0;

    left = cond->u.expr.left;
    right = cond->u.expr.right;
    if (cond->u.expr.op == OP_EQ)
    {
        UINT len;
        WCHAR *path;
        const WCHAR *str = NULL;

        if (left->type == EXPR_PROPVAL && right->type == EXPR_SVAL &&
            !wcscmp( left->u.propval->name, L"Name" ) &&
            towupper( right->u.sval[0] ) == towupper( root ))
        {
            str = right->u.sval;
        }
        else if (left->type == EXPR_SVAL && right->type == EXPR_PROPVAL &&
                 !wcscmp( right->u.propval->name, L"Name" ) &&
                 towupper( left->u.sval[0] ) == towupper( root ))
        {
            str = left->u.sval;
        }
        if (str && (path = build_dirname( str, &len )))
        {
            if (seen_dir( dirstack, path ))
            {
                heap_free( path );
                return ++*count;
            }
            else if (push_dir( dirstack, path, len )) return ++*count;
            heap_free( path );
            return *count = 0;
        }
    }
    else if (cond->u.expr.op == OP_OR)
    {
        UINT left_count = 0, right_count = 0;

        if (!(seed_dirs( dirstack, left, root, &left_count ))) return *count = 0;
        if (!(seed_dirs( dirstack, right, root, &right_count ))) return *count = 0;
        return *count += left_count + right_count;
    }
    return *count = 0;
}

static WCHAR *append_path( const WCHAR *path, const WCHAR *segment, UINT *len )
{
    UINT len_path = 0, len_segment = lstrlenW( segment );
    WCHAR *ret;

    *len = 0;
    if (path) len_path = lstrlenW( path );
    if (!(ret = heap_alloc( (len_path + len_segment + 2) * sizeof(WCHAR) ))) return NULL;
    if (path && len_path)
    {
        memcpy( ret, path, len_path * sizeof(WCHAR) );
        ret[len_path] = '\\';
        *len += len_path + 1;
    }
    memcpy( ret + *len, segment, len_segment * sizeof(WCHAR) );
    *len += len_segment;
    ret[*len] = 0;
    return ret;
}

static WCHAR *get_file_version( const WCHAR *filename )
{
    VS_FIXEDFILEINFO *info;
    DWORD size, len = 4 * 5 + ARRAY_SIZE( L"%u.%u.%u.%u" );
    void *block;
    WCHAR *ret;

    if (!(ret = heap_alloc( len * sizeof(WCHAR) ))) return NULL;
    if (!(size = GetFileVersionInfoSizeW( filename, NULL )) || !(block = heap_alloc( size )))
    {
        heap_free( ret );
        return NULL;
    }
    if (!GetFileVersionInfoW( filename, 0, size, block ) ||
        !VerQueryValueW( block, L"\\", (void **)&info, &size ))
    {
        heap_free( block );
        heap_free( ret );
        return NULL;
    }
    swprintf( ret, len, L"%u.%u.%u.%u", info->dwFileVersionMS >> 16, info->dwFileVersionMS & 0xffff,
                                        info->dwFileVersionLS >> 16, info->dwFileVersionLS & 0xffff );
    heap_free( block );
    return ret;
}

static enum fill_status fill_datafile( struct table *table, const struct expr *cond )
{
    struct record_datafile *rec;
    UINT i, len, row = 0, offset = 0, num_expected_rows;
    WCHAR *glob = NULL, *path = NULL, *new_path, root[] = {'A',':','\\',0};
    DWORD drives = GetLogicalDrives();
    WIN32_FIND_DATAW data;
    HANDLE handle;
    struct dirstack *dirstack;
    enum fill_status status = FILL_STATUS_UNFILTERED;

    if (!resize_table( table, 8, sizeof(*rec) )) return FILL_STATUS_FAILED;

    dirstack = alloc_dirstack(2);

    for (i = 0; i < 26; i++)
    {
        if (!(drives & (1 << i))) continue;

        root[0] = 'A' + i;
        if (GetDriveTypeW( root ) != DRIVE_FIXED) continue;

        num_expected_rows = 0;
        if (!seed_dirs( dirstack, cond, root[0], &num_expected_rows )) clear_dirstack( dirstack );

        for (;;)
        {
            heap_free( glob );
            heap_free( path );
            path = pop_dir( dirstack, &len );
            if (!(glob = build_glob( root[0], path, len )))
            {
                status = FILL_STATUS_FAILED;
                goto done;
            }
            if ((handle = FindFirstFileW( glob, &data )) != INVALID_HANDLE_VALUE)
            {
                do
                {
                    if (!resize_table( table, row + 1, sizeof(*rec) ))
                    {
                        status = FILL_STATUS_FAILED;
                        FindClose( handle );
                        goto done;
                    }
                    if (!wcscmp( data.cFileName, L"." ) || !wcscmp( data.cFileName, L".." )) continue;

                    if (!(new_path = append_path( path, data.cFileName, &len )))
                    {
                        status = FILL_STATUS_FAILED;
                        FindClose( handle );
                        goto done;
                    }

                    if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                    {
                        if (push_dir( dirstack, new_path, len )) continue;
                        heap_free( new_path );
                        FindClose( handle );
                        status = FILL_STATUS_FAILED;
                        goto done;
                    }
                    rec = (struct record_datafile *)(table->data + offset);
                    rec->name    = build_name( root[0], new_path );
                    rec->version = get_file_version( rec->name );
                    heap_free( new_path );
                    if (!match_row( table, row, cond, &status ))
                    {
                        free_row_values( table, row );
                        continue;
                    }
                    else if (num_expected_rows && row == num_expected_rows - 1)
                    {
                        row++;
                        FindClose( handle );
                        status = FILL_STATUS_FILTERED;
                        goto done;
                    }
                    offset += sizeof(*rec);
                    row++;
                }
                while (FindNextFileW( handle, &data ));
                FindClose( handle );
            }
            if (!peek_dir( dirstack )) break;
        }
    }

done:
    free_dirstack( dirstack );
    heap_free( glob );
    heap_free( path );

    TRACE("created %u rows\n", row);
    table->num_rows = row;
    return status;
}

static UINT32 get_pixelsperxlogicalinch(void)
{
    HDC hdc = GetDC( NULL );
    UINT32 ret;

    if (!hdc) return 96;
    ret = GetDeviceCaps( hdc, LOGPIXELSX );
    ReleaseDC( NULL, hdc );
    return ret;
}

static enum fill_status fill_desktopmonitor( struct table *table, const struct expr *cond )
{
    struct record_desktopmonitor *rec;
    enum fill_status status = FILL_STATUS_UNFILTERED;
    UINT row = 0;

    if (!resize_table( table, 1, sizeof(*rec) )) return FILL_STATUS_FAILED;

    rec = (struct record_desktopmonitor *)table->data;
    rec->name                  = L"Generic Non-PnP Monitor";
    rec->pixelsperxlogicalinch = get_pixelsperxlogicalinch();

    if (match_row( table, row, cond, &status )) row++;

    TRACE("created %u rows\n", row);
    table->num_rows = row;
    return status;
}

static enum fill_status fill_directory( struct table *table, const struct expr *cond )
{
    struct record_directory *rec;
    UINT i, len, row = 0, offset = 0, num_expected_rows;
    WCHAR *glob = NULL, *path = NULL, *new_path, root[] = {'A',':','\\',0};
    DWORD drives = GetLogicalDrives();
    WIN32_FIND_DATAW data;
    HANDLE handle;
    struct dirstack *dirstack;
    enum fill_status status = FILL_STATUS_UNFILTERED;

    if (!resize_table( table, 4, sizeof(*rec) )) return FILL_STATUS_FAILED;

    dirstack = alloc_dirstack(2);

    for (i = 0; i < 26; i++)
    {
        if (!(drives & (1 << i))) continue;

        root[0] = 'A' + i;
        if (GetDriveTypeW( root ) != DRIVE_FIXED) continue;

        num_expected_rows = 0;
        if (!seed_dirs( dirstack, cond, root[0], &num_expected_rows )) clear_dirstack( dirstack );

        for (;;)
        {
            heap_free( glob );
            heap_free( path );
            path = pop_dir( dirstack, &len );
            if (!(glob = build_glob( root[0], path, len )))
            {
                status = FILL_STATUS_FAILED;
                goto done;
            }
            if ((handle = FindFirstFileW( glob, &data )) != INVALID_HANDLE_VALUE)
            {
                do
                {
                    if (!resize_table( table, row + 1, sizeof(*rec) ))
                    {
                        FindClose( handle );
                        status = FILL_STATUS_FAILED;
                        goto done;
                    }
                    if (!(data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ||
                        !wcscmp( data.cFileName, L"." ) || !wcscmp( data.cFileName, L".." ))
                        continue;

                    if (!(new_path = append_path( path, data.cFileName, &len )))
                    {
                        FindClose( handle );
                        status = FILL_STATUS_FAILED;
                        goto done;
                    }

                    if (!(push_dir( dirstack, new_path, len )))
                    {
                        heap_free( new_path );
                        FindClose( handle );
                        status = FILL_STATUS_FAILED;
                        goto done;
                    }
                    rec = (struct record_directory *)(table->data + offset);
                    rec->accessmask = FILE_ALL_ACCESS;
                    rec->name       = build_name( root[0], new_path );
                    heap_free( new_path );
                    if (!match_row( table, row, cond, &status ))
                    {
                        free_row_values( table, row );
                        continue;
                    }
                    else if (num_expected_rows && row == num_expected_rows - 1)
                    {
                        row++;
                        FindClose( handle );
                        status = FILL_STATUS_FILTERED;
                        goto done;
                    }
                    offset += sizeof(*rec);
                    row++;
                }
                while (FindNextFileW( handle, &data ));
                FindClose( handle );
            }
            if (!peek_dir( dirstack )) break;
        }
    }

done:
    free_dirstack( dirstack );
    heap_free( glob );
    heap_free( path );

    TRACE("created %u rows\n", row);
    table->num_rows = row;
    return status;
}

static UINT64 get_freespace( const WCHAR *dir, UINT64 *disksize )
{
    WCHAR root[] = {'\\','\\','.','\\','A',':',0};
    ULARGE_INTEGER free;
    DISK_GEOMETRY_EX info;
    HANDLE handle;
    DWORD bytes_returned;

    free.QuadPart = 512 * 1024 * 1024;
    GetDiskFreeSpaceExW( dir, NULL, NULL, &free );

    root[4] = dir[0];
    handle = CreateFileW( root, GENERIC_READ, FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, 0 );
    if (handle != INVALID_HANDLE_VALUE)
    {
        if (DeviceIoControl( handle, IOCTL_DISK_GET_DRIVE_GEOMETRY_EX, NULL, 0, &info, sizeof(info), &bytes_returned, NULL ))
            *disksize = info.DiskSize.QuadPart;
        CloseHandle( handle );
    }
    return free.QuadPart;
}
static WCHAR *get_diskdrive_serialnumber( WCHAR letter )
{
    WCHAR *ret = NULL;
    STORAGE_DEVICE_DESCRIPTOR *desc;
    HANDLE handle = INVALID_HANDLE_VALUE;
    STORAGE_PROPERTY_QUERY query = {0};
    WCHAR drive[7];
    DWORD size;

    swprintf( drive, ARRAY_SIZE(drive), L"\\\\.\\%c:", letter );
    handle = CreateFileW( drive, 0, FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, 0 );
    if (handle == INVALID_HANDLE_VALUE) goto done;

    query.PropertyId = StorageDeviceProperty;
    query.QueryType  = PropertyStandardQuery;

    size = sizeof(*desc) + 256;
    for (;;)
    {
        if (!(desc = heap_alloc( size ))) break;
        if (DeviceIoControl( handle, IOCTL_STORAGE_QUERY_PROPERTY, &query, sizeof(query), desc, size, NULL, NULL ))
        {
            if (desc->SerialNumberOffset) ret = heap_strdupAW( (const char *)desc + desc->SerialNumberOffset );
            heap_free( desc );
            break;
        }
        size = desc->Size;
        heap_free( desc );
        if (GetLastError() != ERROR_MORE_DATA) break;
    }

done:
    if (handle != INVALID_HANDLE_VALUE) CloseHandle( handle );
    if (!ret) ret = heap_strdupW( L"WINEHDISK" );
    return ret;
}

static enum fill_status fill_diskdrive( struct table *table, const struct expr *cond )
{
    static const WCHAR fmtW[] = L"\\\\\\\\.\\\\PHYSICALDRIVE%u";
    WCHAR device_id[ARRAY_SIZE( fmtW ) + 10], root[] = {'A',':','\\',0};
    struct record_diskdrive *rec;
    UINT i, row = 0, offset = 0, index = 0, type;
    UINT64 size = 1024 * 1024 * 1024;
    DWORD drives = GetLogicalDrives();
    enum fill_status status = FILL_STATUS_UNFILTERED;

    if (!resize_table( table, 2, sizeof(*rec) )) return FILL_STATUS_FAILED;

    for (i = 0; i < 26; i++)
    {
        if (drives & (1 << i))
        {
            root[0] = 'A' + i;
            type = GetDriveTypeW( root );
            if (type != DRIVE_FIXED && type != DRIVE_REMOVABLE)
                continue;

            if (!resize_table( table, row + 1, sizeof(*rec) )) return FILL_STATUS_FAILED;

            rec = (struct record_diskdrive *)(table->data + offset);
            swprintf( device_id, ARRAY_SIZE( device_id ), fmtW, index );
            rec->device_id     = heap_strdupW( device_id );
            rec->index         = index++;
            rec->interfacetype = L"IDE";
            rec->manufacturer  = L"(Standard disk drives)";
            rec->mediatype     = (type == DRIVE_FIXED) ? L"Fixed hard disk" : L"Removable media";
            rec->model         = L"Wine Disk Drive";
            rec->pnpdevice_id  = L"IDE\\Disk\\VEN_WINE";
            rec->serialnumber  = get_diskdrive_serialnumber( root[0] );
            get_freespace( root, &size );
            rec->size          = size;
            if (!match_row( table, row, cond, &status ))
            {
                free_row_values( table, row );
                continue;
            }
            offset += sizeof(*rec);
            row++;
        }
    }
    TRACE("created %u rows\n", row);
    table->num_rows = row;
    return status;
}

struct association
{
    WCHAR *ref;
    WCHAR *ref2;
};

static void free_associations( struct association *assoc, UINT count )
{
    UINT i;
    if (!assoc) return;
    for (i = 0; i < count; i++)
    {
        heap_free( assoc[i].ref );
        heap_free( assoc[i].ref2 );
    }
    heap_free( assoc );
}

static struct association *get_diskdrivetodiskpartition_pairs( UINT *count )
{
    struct association *ret = NULL;
    struct query *query, *query2 = NULL;
    VARIANT val;
    HRESULT hr;
    UINT i;

    if (!(query = create_query())) return NULL;
    if ((hr = parse_query( L"SELECT * FROM Win32_DiskDrive", &query->view, &query->mem )) != S_OK) goto done;
    if ((hr = execute_view( query->view )) != S_OK) goto done;

    if (!(query2 = create_query())) return FALSE;
    if ((hr = parse_query( L"SELECT * FROM Win32_DiskPartition", &query2->view, &query2->mem )) != S_OK) goto done;
    if ((hr = execute_view( query2->view )) != S_OK) goto done;

    if (!(ret = heap_alloc_zero( query->view->result_count * sizeof(*ret) ))) goto done;

    for (i = 0; i < query->view->result_count; i++)
    {
        if ((hr = get_propval( query->view, i, L"__PATH", &val, NULL, NULL )) != S_OK) goto done;
        if (!(ret[i].ref = heap_strdupW( V_BSTR(&val) ))) goto done;
        VariantClear( &val );

        if ((hr = get_propval( query2->view, i, L"__PATH", &val, NULL, NULL )) != S_OK) goto done;
        if (!(ret[i].ref2 = heap_strdupW( V_BSTR(&val) ))) goto done;
        VariantClear( &val );
    }

    *count = query->view->result_count;

done:
    if (!ret) free_associations( ret, query->view->result_count );
    free_query( query );
    free_query( query2 );
    return ret;
}

static enum fill_status fill_diskdrivetodiskpartition( struct table *table, const struct expr *cond )
{
    struct record_diskdrivetodiskpartition *rec;
    UINT i, row = 0, offset = 0, count = 0;
    enum fill_status status = FILL_STATUS_UNFILTERED;
    struct association *assoc;

    if (!(assoc = get_diskdrivetodiskpartition_pairs( &count ))) return FILL_STATUS_FAILED;
    if (!count)
    {
        free_associations( assoc, count );
        return FILL_STATUS_UNFILTERED;
    }
    if (!resize_table( table, count, sizeof(*rec) ))
    {
        free_associations( assoc, count );
        return FILL_STATUS_FAILED;
    }

    for (i = 0; i < count; i++)
    {
        rec = (struct record_diskdrivetodiskpartition *)(table->data + offset);
        rec->antecedent = assoc[i].ref;
        rec->dependent  = assoc[i].ref2;
        if (!match_row( table, row, cond, &status ))
        {
            free_row_values( table, row );
            continue;
        }
        offset += sizeof(*rec);
        row++;
    }

    heap_free( assoc );

    TRACE("created %u rows\n", row);
    table->num_rows = row;
    return status;
}

static WCHAR *get_filesystem( const WCHAR *root )
{
    WCHAR buffer[MAX_PATH + 1];

    if (GetVolumeInformationW( root, NULL, 0, NULL, NULL, NULL, buffer, MAX_PATH + 1 ))
        return heap_strdupW( buffer );
    return heap_strdupW( L"NTFS" );
}

static enum fill_status fill_diskpartition( struct table *table, const struct expr *cond )
{
    WCHAR device_id[32], root[] = {'A',':','\\',0};
    struct record_diskpartition *rec;
    UINT i, row = 0, offset = 0, type, index = 0;
    UINT64 size = 1024 * 1024 * 1024;
    DWORD drives = GetLogicalDrives();
    enum fill_status status = FILL_STATUS_UNFILTERED;

    if (!resize_table( table, 4, sizeof(*rec) )) return FILL_STATUS_FAILED;

    for (i = 0; i < 26; i++)
    {
        if (drives & (1 << i))
        {
            root[0] = 'A' + i;
            type = GetDriveTypeW( root );
            if (type != DRIVE_FIXED && type != DRIVE_REMOVABLE)
                continue;

            if (!resize_table( table, row + 1, sizeof(*rec) )) return FILL_STATUS_FAILED;

            rec = (struct record_diskpartition *)(table->data + offset);
            rec->bootable       = (i == 2) ? -1 : 0;
            rec->bootpartition  = (i == 2) ? -1 : 0;
            swprintf( device_id, ARRAY_SIZE( device_id ), L"Disk #%u, Partition #0", index );
            rec->device_id      = heap_strdupW( device_id );
            rec->diskindex      = index++;
            rec->index          = 0;
            rec->pnpdevice_id   = heap_strdupW( device_id );
            get_freespace( root, &size );
            rec->size           = size;
            rec->startingoffset = 0;
            rec->type           = get_filesystem( root );
            if (!match_row( table, row, cond, &status ))
            {
                free_row_values( table, row );
                continue;
            }
            offset += sizeof(*rec);
            row++;
        }
    }
    TRACE("created %u rows\n", row);
    table->num_rows = row;
    return status;
}

static UINT32 get_bitsperpixel( UINT *hres, UINT *vres )
{
    HDC hdc = GetDC( NULL );
    UINT32 ret;

    if (!hdc) return 32;
    ret = GetDeviceCaps( hdc, BITSPIXEL );
    *hres = GetDeviceCaps( hdc, HORZRES );
    *vres = GetDeviceCaps( hdc, VERTRES );
    ReleaseDC( NULL, hdc );
    return ret;
}

static enum fill_status fill_displaycontrollerconfig( struct table *table, const struct expr *cond )
{
    struct record_displaycontrollerconfig *rec;
    UINT row = 0, hres = 1024, vres = 768;
    enum fill_status status = FILL_STATUS_UNFILTERED;

    if (!resize_table( table, 1, sizeof(*rec) )) return FILL_STATUS_FAILED;

    rec = (struct record_displaycontrollerconfig *)table->data;
    rec->bitsperpixel         = get_bitsperpixel( &hres, &vres );
    rec->caption              = L"VideoController1";
    rec->horizontalresolution = hres;
    rec->name                 = L"VideoController1";
    rec->verticalresolution   = vres;
    if (!match_row( table, row, cond, &status )) free_row_values( table, row );
    else row++;

    TRACE("created %u rows\n", row);
    table->num_rows = row;
    return status;
}

static WCHAR *get_ip4_string( DWORD addr )
{
    DWORD len = sizeof("ddd.ddd.ddd.ddd");
    WCHAR *ret;

    if (!(ret = heap_alloc( len * sizeof(WCHAR) ))) return NULL;
    swprintf( ret, len, L"%u.%u.%u.%u", (addr >> 24) & 0xff, (addr >> 16) & 0xff, (addr >> 8) & 0xff, addr & 0xff );
    return ret;
}

static enum fill_status fill_ip4routetable( struct table *table, const struct expr *cond )
{
    struct record_ip4routetable *rec;
    UINT i, row = 0, offset = 0, size = 0;
    MIB_IPFORWARDTABLE *forwards;
    enum fill_status status = FILL_STATUS_UNFILTERED;

    if (GetIpForwardTable( NULL, &size, TRUE ) != ERROR_INSUFFICIENT_BUFFER) return FILL_STATUS_FAILED;
    if (!(forwards = heap_alloc( size ))) return FILL_STATUS_FAILED;
    if (GetIpForwardTable( forwards, &size, TRUE ))
    {
        heap_free( forwards );
        return FILL_STATUS_FAILED;
    }
    if (!resize_table( table, max(forwards->dwNumEntries, 1), sizeof(*rec) ))
    {
        heap_free( forwards );
        return FILL_STATUS_FAILED;
    }

    for (i = 0; i < forwards->dwNumEntries; i++)
    {
        rec = (struct record_ip4routetable *)(table->data + offset);

        rec->destination    = get_ip4_string( ntohl(forwards->table[i].dwForwardDest) );
        rec->interfaceindex = forwards->table[i].dwForwardIfIndex;
        rec->nexthop        = get_ip4_string( ntohl(forwards->table[i].dwForwardNextHop) );

        if (!match_row( table, row, cond, &status ))
        {
            free_row_values( table, row );
            continue;
        }
        offset += sizeof(*rec);
        row++;
    }
    TRACE("created %u rows\n", row);
    table->num_rows = row;

    heap_free( forwards );
    return status;
}

static WCHAR *get_volumename( const WCHAR *root )
{
    WCHAR buf[MAX_PATH + 1] = {0};
    GetVolumeInformationW( root, buf, ARRAY_SIZE( buf ), NULL, NULL, NULL, NULL, 0 );
    return heap_strdupW( buf );
}
static WCHAR *get_volumeserialnumber( const WCHAR *root )
{
    DWORD serial = 0;
    WCHAR buffer[9];

    GetVolumeInformationW( root, NULL, 0, &serial, NULL, NULL, NULL, 0 );
    swprintf( buffer, ARRAY_SIZE( buffer ), L"%08X", serial );
    return heap_strdupW( buffer );
}

static enum fill_status fill_logicaldisk( struct table *table, const struct expr *cond )
{
    WCHAR device_id[3], root[] = {'A',':','\\',0};
    struct record_logicaldisk *rec;
    UINT i, row = 0, offset = 0, type;
    UINT64 size = 1024 * 1024 * 1024;
    DWORD drives = GetLogicalDrives();
    enum fill_status status = FILL_STATUS_UNFILTERED;

    if (!resize_table( table, 4, sizeof(*rec) )) return FILL_STATUS_FAILED;

    for (i = 0; i < 26; i++)
    {
        if (drives & (1 << i))
        {
            root[0] = 'A' + i;
            type = GetDriveTypeW( root );
            if (type != DRIVE_FIXED && type != DRIVE_CDROM && type != DRIVE_REMOVABLE)
                continue;

            if (!resize_table( table, row + 1, sizeof(*rec) )) return FILL_STATUS_FAILED;

            rec = (struct record_logicaldisk *)(table->data + offset);
            swprintf( device_id, ARRAY_SIZE( device_id ), L"%c:", 'A' + i );
            rec->device_id          = heap_strdupW( device_id );
            rec->drivetype          = type;
            rec->filesystem         = get_filesystem( root );
            rec->freespace          = get_freespace( root, &size );
            rec->name               = heap_strdupW( device_id );
            rec->size               = size;
            rec->volumename         = get_volumename( root );
            rec->volumeserialnumber = get_volumeserialnumber( root );
            if (!match_row( table, row, cond, &status ))
            {
                free_row_values( table, row );
                continue;
            }
            offset += sizeof(*rec);
            row++;
        }
    }
    TRACE("created %u rows\n", row);
    table->num_rows = row;
    return status;
}

static struct association *get_logicaldisktopartition_pairs( UINT *count )
{
    struct association *ret = NULL;
    struct query *query, *query2 = NULL;
    VARIANT val;
    HRESULT hr;
    UINT i;

    if (!(query = create_query())) return NULL;
    if ((hr = parse_query( L"SELECT * FROM Win32_DiskPartition", &query->view, &query->mem )) != S_OK) goto done;
    if ((hr = execute_view( query->view )) != S_OK) goto done;

    if (!(query2 = create_query())) return FALSE;
    if ((hr = parse_query( L"SELECT * FROM Win32_LogicalDisk WHERE DriveType=2 OR DriveType=3", &query2->view,
                           &query2->mem )) != S_OK) goto done;
    if ((hr = execute_view( query2->view )) != S_OK) goto done;

    if (!(ret = heap_alloc_zero( query->view->result_count * sizeof(*ret) ))) goto done;

    /* assume fixed and removable disks are enumerated in the same order as partitions */
    for (i = 0; i < query->view->result_count; i++)
    {
        if ((hr = get_propval( query->view, i, L"__PATH", &val, NULL, NULL )) != S_OK) goto done;
        if (!(ret[i].ref = heap_strdupW( V_BSTR(&val) ))) goto done;
        VariantClear( &val );

        if ((hr = get_propval( query2->view, i, L"__PATH", &val, NULL, NULL )) != S_OK) goto done;
        if (!(ret[i].ref2 = heap_strdupW( V_BSTR(&val) ))) goto done;
        VariantClear( &val );
    }

    *count = query->view->result_count;

done:
    if (!ret) free_associations( ret, query->view->result_count );
    free_query( query );
    free_query( query2 );
    return ret;
}

static enum fill_status fill_logicaldisktopartition( struct table *table, const struct expr *cond )
{
    struct record_logicaldisktopartition *rec;
    UINT i, row = 0, offset = 0, count = 0;
    enum fill_status status = FILL_STATUS_UNFILTERED;
    struct association *assoc;

    if (!(assoc = get_logicaldisktopartition_pairs( &count ))) return FILL_STATUS_FAILED;
    if (!count)
    {
        free_associations( assoc, count );
        return FILL_STATUS_UNFILTERED;
    }
    if (!resize_table( table, count, sizeof(*rec) ))
    {
        free_associations( assoc, count );
        return FILL_STATUS_FAILED;
    }

    for (i = 0; i < count; i++)
    {
        rec = (struct record_logicaldisktopartition *)(table->data + offset);
        rec->antecedent = assoc[i].ref;
        rec->dependent  = assoc[i].ref2;
        if (!match_row( table, row, cond, &status ))
        {
            free_row_values( table, row );
            continue;
        }
        offset += sizeof(*rec);
        row++;
    }

    heap_free( assoc );

    TRACE("created %u rows\n", row);
    table->num_rows = row;
    return status;
}

static UINT16 get_connection_status( IF_OPER_STATUS status )
{
    switch (status)
    {
    case IfOperStatusDown:
        return 0; /* Disconnected */
    case IfOperStatusUp:
        return 2; /* Connected */
    default:
        ERR("unhandled status %u\n", status);
        break;
    }
    return 0;
}
static WCHAR *get_mac_address( const BYTE *addr, DWORD len )
{
    WCHAR *ret;
    if (len != 6 || !(ret = heap_alloc( 18 * sizeof(WCHAR) ))) return NULL;
    swprintf( ret, 18, L"%02x:%02x:%02x:%02x:%02x:%02x", addr[0], addr[1], addr[2], addr[3], addr[4], addr[5] );
    return ret;
}
static const WCHAR *get_adaptertype( DWORD type, int *id, int *physical )
{
    switch (type)
    {
    case IF_TYPE_ETHERNET_CSMACD:
        *id = 0;
        *physical = -1;
        return L"Ethernet 802.3";

    case IF_TYPE_IEEE80211:
        *id = 9;
        *physical = -1;
        return L"Wireless";

    case IF_TYPE_IEEE1394:
        *id = 13;
        *physical = -1;
        return L"1394";

    case IF_TYPE_TUNNEL:
        *id = 15;
        *physical = 0;
        return L"Tunnel";

    default:
        *id = -1;
        *physical = 0;
        return NULL;
    }
}

static enum fill_status fill_networkadapter( struct table *table, const struct expr *cond )
{
    WCHAR device_id[11];
    struct record_networkadapter *rec;
    IP_ADAPTER_ADDRESSES *aa, *buffer;
    UINT row = 0, offset = 0, count = 0;
    DWORD size = 0, ret;
    int adaptertypeid, physical;
    enum fill_status status = FILL_STATUS_UNFILTERED;

    ret = GetAdaptersAddresses( AF_UNSPEC, 0, NULL, NULL, &size );
    if (ret != ERROR_BUFFER_OVERFLOW) return FILL_STATUS_FAILED;

    if (!(buffer = heap_alloc( size ))) return FILL_STATUS_FAILED;
    if (GetAdaptersAddresses( AF_UNSPEC, 0, NULL, buffer, &size ))
    {
        heap_free( buffer );
        return FILL_STATUS_FAILED;
    }
    for (aa = buffer; aa; aa = aa->Next)
    {
        if (aa->IfType != IF_TYPE_SOFTWARE_LOOPBACK) count++;
    }
    if (!resize_table( table, count, sizeof(*rec) ))
    {
        heap_free( buffer );
        return FILL_STATUS_FAILED;
    }
    for (aa = buffer; aa; aa = aa->Next)
    {
        if (aa->IfType == IF_TYPE_SOFTWARE_LOOPBACK) continue;

        rec = (struct record_networkadapter *)(table->data + offset);
        swprintf( device_id, ARRAY_SIZE( device_id ), L"%u", aa->u.s.IfIndex );
        rec->adaptertype          = get_adaptertype( aa->IfType, &adaptertypeid, &physical );
        rec->adaptertypeid        = adaptertypeid;
        rec->description          = heap_strdupW( aa->Description );
        rec->device_id            = heap_strdupW( device_id );
        rec->index                = aa->u.s.IfIndex;
        rec->interface_index      = aa->u.s.IfIndex;
        rec->mac_address          = get_mac_address( aa->PhysicalAddress, aa->PhysicalAddressLength );
        rec->manufacturer         = L"The Wine Project";
        rec->name                 = heap_strdupW( aa->FriendlyName );
        rec->netconnection_status = get_connection_status( aa->OperStatus );
        rec->physicaladapter      = physical;
        rec->pnpdevice_id         = L"PCI\\VEN_8086&DEV_100E&SUBSYS_001E8086&REV_02\\3&267A616A&1&18";
        rec->speed                = 1000000;
        if (!match_row( table, row, cond, &status ))
        {
            free_row_values( table, row );
            continue;
        }
        offset += sizeof(*rec);
        row++;
    }
    TRACE("created %u rows\n", row);
    table->num_rows = row;

    heap_free( buffer );
    return status;
}

static WCHAR *get_dnshostname( IP_ADAPTER_UNICAST_ADDRESS *addr )
{
    const SOCKET_ADDRESS *sa = &addr->Address;
    WCHAR buf[NI_MAXHOST];

    if (!addr) return NULL;
    if (GetNameInfoW( sa->lpSockaddr, sa->iSockaddrLength, buf, ARRAY_SIZE( buf ), NULL,
                      0, NI_NAMEREQD )) return NULL;
    return heap_strdupW( buf );
}
static struct array *get_defaultipgateway( IP_ADAPTER_GATEWAY_ADDRESS *list )
{
    IP_ADAPTER_GATEWAY_ADDRESS *gateway;
    struct array *ret;
    ULONG buflen, i = 0, count = 0;
    WCHAR **ptr, buf[54]; /* max IPv6 address length */

    if (!list) return NULL;
    for (gateway = list; gateway; gateway = gateway->Next) count++;

    if (!(ret = heap_alloc( sizeof(*ret) ))) return NULL;
    if (!(ptr = heap_alloc( sizeof(*ptr) * count )))
    {
        heap_free( ret );
        return NULL;
    }
    for (gateway = list; gateway; gateway = gateway->Next)
    {
        buflen = ARRAY_SIZE( buf );
        if (WSAAddressToStringW( gateway->Address.lpSockaddr, gateway->Address.iSockaddrLength,
                                 NULL, buf, &buflen) || !(ptr[i++] = heap_strdupW( buf )))
        {
            for (; i > 0; i--) heap_free( ptr[i - 1] );
            heap_free( ptr );
            heap_free( ret );
            return NULL;
        }
    }
    ret->elem_size = sizeof(*ptr);
    ret->count     = count;
    ret->ptr       = ptr;
    return ret;
}
static struct array *get_dnsserversearchorder( IP_ADAPTER_DNS_SERVER_ADDRESS *list )
{
    IP_ADAPTER_DNS_SERVER_ADDRESS *server;
    struct array *ret;
    ULONG buflen, i = 0, count = 0;
    WCHAR **ptr, *p, buf[54]; /* max IPv6 address length */

    if (!list) return NULL;
    for (server = list; server; server = server->Next) count++;

    if (!(ret = heap_alloc( sizeof(*ret) ))) return NULL;
    if (!(ptr = heap_alloc( sizeof(*ptr) * count )))
    {
        heap_free( ret );
        return NULL;
    }
    for (server = list; server; server = server->Next)
    {
        buflen = ARRAY_SIZE( buf );
        if (WSAAddressToStringW( server->Address.lpSockaddr, server->Address.iSockaddrLength,
                                 NULL, buf, &buflen) || !(ptr[i++] = heap_strdupW( buf )))
        {
            for (; i > 0; i--) heap_free( ptr[i - 1] );
            heap_free( ptr );
            heap_free( ret );
            return NULL;
        }
        if ((p = wcsrchr( ptr[i - 1], ':' ))) *p = 0;
    }
    ret->elem_size = sizeof(*ptr);
    ret->count     = count;
    ret->ptr       = ptr;
    return ret;
}
static struct array *get_ipaddress( IP_ADAPTER_UNICAST_ADDRESS_LH *list )
{
    IP_ADAPTER_UNICAST_ADDRESS_LH *address;
    struct array *ret;
    ULONG buflen, i = 0, count = 0;
    WCHAR **ptr, buf[54]; /* max IPv6 address length */

    if (!list) return NULL;
    for (address = list; address; address = address->Next) count++;

    if (!(ret = heap_alloc( sizeof(*ret) ))) return NULL;
    if (!(ptr = heap_alloc( sizeof(*ptr) * count )))
    {
        heap_free( ret );
        return NULL;
    }
    for (address = list; address; address = address->Next)
    {
        buflen = ARRAY_SIZE( buf );
        if (WSAAddressToStringW( address->Address.lpSockaddr, address->Address.iSockaddrLength,
                                 NULL, buf, &buflen) || !(ptr[i++] = heap_strdupW( buf )))
        {
            for (; i > 0; i--) heap_free( ptr[i - 1] );
            heap_free( ptr );
            heap_free( ret );
            return NULL;
        }
    }
    ret->elem_size = sizeof(*ptr);
    ret->count     = count;
    ret->ptr       = ptr;
    return ret;
}
static struct array *get_ipsubnet( IP_ADAPTER_UNICAST_ADDRESS_LH *list )
{
    IP_ADAPTER_UNICAST_ADDRESS_LH *address;
    struct array *ret;
    ULONG i = 0, count = 0;
    WCHAR **ptr;

    if (!list) return NULL;
    for (address = list; address; address = address->Next) count++;

    if (!(ret = heap_alloc( sizeof(*ret) ))) return NULL;
    if (!(ptr = heap_alloc( sizeof(*ptr) * count )))
    {
        heap_free( ret );
        return NULL;
    }
    for (address = list; address; address = address->Next)
    {
        if (address->Address.lpSockaddr->sa_family == AF_INET)
        {
            WCHAR buf[INET_ADDRSTRLEN];
            SOCKADDR_IN addr;
            ULONG buflen = ARRAY_SIZE( buf );

            memset( &addr, 0, sizeof(addr) );
            addr.sin_family = AF_INET;
            if (ConvertLengthToIpv4Mask( address->OnLinkPrefixLength, &addr.sin_addr.S_un.S_addr ) != NO_ERROR
                    || WSAAddressToStringW( (SOCKADDR*)&addr, sizeof(addr), NULL, buf, &buflen))
                ptr[i] = NULL;
            else
                ptr[i] = heap_strdupW( buf );
        }
        else
        {
            WCHAR buf[11];
            swprintf( buf, ARRAY_SIZE( buf ), L"%u", address->OnLinkPrefixLength );
            ptr[i] = heap_strdupW( buf );
        }
        if (!ptr[i++])
        {
            for (; i > 0; i--) heap_free( ptr[i - 1] );
            heap_free( ptr );
            heap_free( ret );
            return NULL;
        }
    }
    ret->elem_size = sizeof(*ptr);
    ret->count     = count;
    ret->ptr       = ptr;
    return ret;
}
static WCHAR *get_settingid( UINT32 index )
{
    GUID guid;
    WCHAR *ret, *str;
    memset( &guid, 0, sizeof(guid) );
    guid.Data1 = index;
    UuidToStringW( &guid, &str );
    ret = heap_strdupW( str );
    RpcStringFreeW( &str );
    return ret;
}

static enum fill_status fill_networkadapterconfig( struct table *table, const struct expr *cond )
{
    struct record_networkadapterconfig *rec;
    IP_ADAPTER_ADDRESSES *aa, *buffer;
    UINT row = 0, offset = 0, count = 0;
    DWORD size = 0, ret;
    enum fill_status status = FILL_STATUS_UNFILTERED;

    ret = GetAdaptersAddresses( AF_UNSPEC, GAA_FLAG_INCLUDE_ALL_GATEWAYS, NULL, NULL, &size );
    if (ret != ERROR_BUFFER_OVERFLOW) return FILL_STATUS_FAILED;

    if (!(buffer = heap_alloc( size ))) return FILL_STATUS_FAILED;
    if (GetAdaptersAddresses( AF_UNSPEC, GAA_FLAG_INCLUDE_ALL_GATEWAYS, NULL, buffer, &size ))
    {
        heap_free( buffer );
        return FILL_STATUS_FAILED;
    }
    for (aa = buffer; aa; aa = aa->Next)
    {
        if (aa->IfType != IF_TYPE_SOFTWARE_LOOPBACK) count++;
    }
    if (!resize_table( table, count, sizeof(*rec) ))
    {
        heap_free( buffer );
        return FILL_STATUS_FAILED;
    }
    for (aa = buffer; aa; aa = aa->Next)
    {
        if (aa->IfType == IF_TYPE_SOFTWARE_LOOPBACK) continue;

        rec = (struct record_networkadapterconfig *)(table->data + offset);
        rec->defaultipgateway     = get_defaultipgateway( aa->FirstGatewayAddress );
        rec->description          = heap_strdupW( aa->Description );
        rec->dhcpenabled          = -1;
        rec->dnshostname          = get_dnshostname( aa->FirstUnicastAddress );
        rec->dnsserversearchorder = get_dnsserversearchorder( aa->FirstDnsServerAddress );
        rec->index                = aa->u.s.IfIndex;
        rec->ipaddress            = get_ipaddress( aa->FirstUnicastAddress );
        rec->ipconnectionmetric   = 20;
        rec->ipenabled            = -1;
        rec->ipsubnet             = get_ipsubnet( aa->FirstUnicastAddress );
        rec->mac_address          = get_mac_address( aa->PhysicalAddress, aa->PhysicalAddressLength );
        rec->settingid            = get_settingid( rec->index );
        if (!match_row( table, row, cond, &status ))
        {
            free_row_values( table, row );
            continue;
        }
        offset += sizeof(*rec);
        row++;
    }
    TRACE("created %u rows\n", row);
    table->num_rows = row;

    heap_free( buffer );
    return status;
}

static enum fill_status fill_physicalmemory( struct table *table, const struct expr *cond )
{
    struct record_physicalmemory *rec;
    enum fill_status status = FILL_STATUS_UNFILTERED;
    UINT row = 0;

    if (!resize_table( table, 1, sizeof(*rec) )) return FILL_STATUS_FAILED;

    rec = (struct record_physicalmemory *)table->data;
    rec->banklabel            = L"BANK 0";
    rec->capacity             = get_total_physical_memory();
    rec->caption              = L"Physical Memory";
    rec->configuredclockspeed = 1600;
    rec->devicelocator        = L"DIMM 0";
    rec->formfactor           = 8; /* DIMM */
    rec->memorytype           = 9; /* RAM */
    rec->partnumber           = L"";
    rec->serial               = L"";
    if (!match_row( table, row, cond, &status )) free_row_values( table, row );
    else row++;

    TRACE("created %u rows\n", row);
    table->num_rows = row;
    return status;
}

static enum fill_status fill_pnpentity( struct table *table, const struct expr *cond )
{
    struct record_pnpentity *rec;
    enum fill_status status = FILL_STATUS_UNFILTERED;
    HDEVINFO device_info_set;
    SP_DEVINFO_DATA devinfo = {0};
    DWORD idx;

    device_info_set = SetupDiGetClassDevsW( NULL, NULL, NULL, DIGCF_ALLCLASSES|DIGCF_PRESENT );

    devinfo.cbSize = sizeof(devinfo);

    idx = 0;
    while (SetupDiEnumDeviceInfo( device_info_set, idx++, &devinfo ))
    {
        /* noop */
    }

    resize_table( table, idx, sizeof(*rec) );
    table->num_rows = 0;
    rec = (struct record_pnpentity *)table->data;

    idx = 0;
    while (SetupDiEnumDeviceInfo( device_info_set, idx++, &devinfo ))
    {
        WCHAR device_id[MAX_PATH];
        if (SetupDiGetDeviceInstanceIdW( device_info_set, &devinfo, device_id,
                    ARRAY_SIZE(device_id), NULL ))
        {
            rec->device_id = heap_strdupW( device_id );

            table->num_rows++;
            if (!match_row( table, table->num_rows - 1, cond, &status ))
            {
                free_row_values( table, table->num_rows - 1 );
                table->num_rows--;
            }
            else
                rec++;
        }
    }

    SetupDiDestroyDeviceInfoList( device_info_set );

    return status;
}

static enum fill_status fill_printer( struct table *table, const struct expr *cond )
{
    struct record_printer *rec;
    enum fill_status status = FILL_STATUS_UNFILTERED;
    PRINTER_INFO_2W *info;
    DWORD i, offset = 0, count = 0, size = 0, num_rows = 0;
    WCHAR id[20];

    EnumPrintersW( PRINTER_ENUM_LOCAL, NULL, 2, NULL, 0, &size, &count );
    if (!count) return FILL_STATUS_UNFILTERED;

    if (!(info = heap_alloc( size ))) return FILL_STATUS_FAILED;
    if (!EnumPrintersW( PRINTER_ENUM_LOCAL, NULL, 2, (BYTE *)info, size, &size, &count ))
    {
        heap_free( info );
        return FILL_STATUS_FAILED;
    }
    if (!resize_table( table, count, sizeof(*rec) ))
    {
        heap_free( info );
        return FILL_STATUS_FAILED;
    }

    for (i = 0; i < count; i++)
    {
        rec = (struct record_printer *)(table->data + offset);
        rec->attributes           = info[i].Attributes;
        swprintf( id, ARRAY_SIZE( id ), L"Printer%u", i );
        rec->device_id            = heap_strdupW( id );
        rec->drivername           = heap_strdupW( info[i].pDriverName );
        rec->horizontalresolution = info[i].pDevMode->u1.s1.dmPrintQuality;
        rec->local                = -1;
        rec->location             = heap_strdupW( info[i].pLocation );
        rec->name                 = heap_strdupW( info[i].pPrinterName );
        rec->network              = 0;
        rec->portname             = heap_strdupW( info[i].pPortName );
        if (!match_row( table, i, cond, &status ))
        {
            free_row_values( table, i );
            continue;
        }
        offset += sizeof(*rec);
        num_rows++;
    }
    TRACE("created %u rows\n", num_rows);
    table->num_rows = num_rows;

    heap_free( info );
    return status;
}

static WCHAR *get_cmdline( DWORD process_id )
{
    if (process_id == GetCurrentProcessId()) return heap_strdupW( GetCommandLineW() );
    return NULL; /* FIXME handle different process case */
}

static enum fill_status fill_process( struct table *table, const struct expr *cond )
{
    WCHAR handle[11];
    struct record_process *rec;
    PROCESSENTRY32W entry;
    HANDLE snap;
    enum fill_status status = FILL_STATUS_FAILED;
    UINT row = 0, offset = 0;

    snap = CreateToolhelp32Snapshot( TH32CS_SNAPPROCESS, 0 );
    if (snap == INVALID_HANDLE_VALUE) return FILL_STATUS_FAILED;

    entry.dwSize = sizeof(entry);
    if (!Process32FirstW( snap, &entry )) goto done;
    if (!resize_table( table, 8, sizeof(*rec) )) goto done;

    do
    {
        if (!resize_table( table, row + 1, sizeof(*rec) ))
        {
            status = FILL_STATUS_FAILED;
            goto done;
        }

        rec = (struct record_process *)(table->data + offset);
        rec->caption        = heap_strdupW( entry.szExeFile );
        rec->commandline    = get_cmdline( entry.th32ProcessID );
        rec->description    = heap_strdupW( entry.szExeFile );
        swprintf( handle, ARRAY_SIZE( handle ), L"%u", entry.th32ProcessID );
        rec->handle         = heap_strdupW( handle );
        rec->name           = heap_strdupW( entry.szExeFile );
        rec->process_id     = entry.th32ProcessID;
        rec->pprocess_id    = entry.th32ParentProcessID;
        rec->thread_count   = entry.cntThreads;
        rec->workingsetsize = 0;
        rec->get_owner      = process_get_owner;
        if (!match_row( table, row, cond, &status ))
        {
            free_row_values( table, row );
            continue;
        }
        offset += sizeof(*rec);
        row++;
    } while (Process32NextW( snap, &entry ));

    TRACE("created %u rows\n", row);
    table->num_rows = row;

done:
    CloseHandle( snap );
    return status;
}

void do_cpuid( unsigned int ax, int *p )
{
#if defined(__i386__) || defined(__x86_64__)
    __cpuid( p, ax );
#else
    FIXME("\n");
#endif
}

static unsigned int get_processor_model( unsigned int reg0, unsigned int *stepping, unsigned int *family )
{
    unsigned int model, family_id = (reg0 & (0x0f << 8)) >> 8;

    model = (reg0 & (0x0f << 4)) >> 4;
    if (family_id == 6 || family_id == 15) model |= (reg0 & (0x0f << 16)) >> 12;
    if (family)
    {
        *family = family_id;
        if (family_id == 15) *family += (reg0 & (0xff << 20)) >> 20;
    }
    *stepping = reg0 & 0x0f;
    return model;
}
static void regs_to_str( int *regs, unsigned int len, WCHAR *buffer )
{
    unsigned int i;
    unsigned char *p = (unsigned char *)regs;

    for (i = 0; i < len; i++) { buffer[i] = *p++; }
    buffer[i] = 0;
}
static void get_processor_manufacturer( WCHAR *manufacturer, UINT len )
{
    int tmp, regs[4] = {0, 0, 0, 0};

    do_cpuid( 0, regs );
    tmp = regs[2];      /* swap edx and ecx */
    regs[2] = regs[3];
    regs[3] = tmp;

    regs_to_str( regs + 1, min( 12, len ), manufacturer );
}
static const WCHAR *get_osarchitecture(void)
{
    SYSTEM_INFO info;
    GetNativeSystemInfo( &info );
    if (info.u.s.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64) return L"64-bit";
    return L"32-bit";
}
static void get_processor_caption( WCHAR *caption, UINT len )
{
    const WCHAR *arch;
    WCHAR manufacturer[13];
    int regs[4] = {0, 0, 0, 0};
    unsigned int family, model, stepping;

    get_processor_manufacturer( manufacturer, ARRAY_SIZE( manufacturer ) );
    if (!wcscmp( get_osarchitecture(), L"32-bit" )) arch = L"x86";
    else if (!wcscmp( manufacturer, L"AuthenticAMD" )) arch = L"AMD64";
    else arch = L"Intel64";

    do_cpuid( 1, regs );

    model = get_processor_model( regs[0], &stepping, &family );
    swprintf( caption, len, L"%s Family %u Model %u Stepping %u", arch, family, model, stepping );
}
static void get_processor_version( WCHAR *version, UINT len )
{
    int regs[4] = {0, 0, 0, 0};
    unsigned int model, stepping;

    do_cpuid( 1, regs );

    model = get_processor_model( regs[0], &stepping, NULL );
    swprintf( version, len, L"Model %u Stepping %u", model, stepping );
}
static UINT16 get_processor_revision(void)
{
    int regs[4] = {0, 0, 0, 0};
    do_cpuid( 1, regs );
    return regs[0];
}
static void get_processor_id( WCHAR *processor_id, UINT len )
{
    int regs[4] = {0, 0, 0, 0};

    do_cpuid( 1, regs );
    swprintf( processor_id, len, L"%08X%08X", regs[3], regs[0] );
}
static void get_processor_name( WCHAR *name )
{
    int regs[4] = {0, 0, 0, 0};
    int i;

    do_cpuid( 0x80000000, regs );
    if (regs[0] >= 0x80000004)
    {
        do_cpuid( 0x80000002, regs );
        regs_to_str( regs, 16, name );
        do_cpuid( 0x80000003, regs );
        regs_to_str( regs, 16, name + 16 );
        do_cpuid( 0x80000004, regs );
        regs_to_str( regs, 16, name + 32 );
    }
    for (i = lstrlenW(name) - 1; i >= 0 && name[i] == ' '; i--) name[i] = 0;
}
static UINT get_processor_currentclockspeed( UINT index )
{
    PROCESSOR_POWER_INFORMATION *info;
    UINT ret = 1000, size = get_processor_count() * sizeof(PROCESSOR_POWER_INFORMATION);
    NTSTATUS status;

    if ((info = heap_alloc( size )))
    {
        status = NtPowerInformation( ProcessorInformation, NULL, 0, info, size );
        if (!status) ret = info[index].CurrentMhz;
        heap_free( info );
    }
    return ret;
}
static UINT get_processor_maxclockspeed( UINT index )
{
    PROCESSOR_POWER_INFORMATION *info;
    UINT ret = 1000, size = get_processor_count() * sizeof(PROCESSOR_POWER_INFORMATION);
    NTSTATUS status;

    if ((info = heap_alloc( size )))
    {
        status = NtPowerInformation( ProcessorInformation, NULL, 0, info, size );
        if (!status) ret = info[index].MaxMhz;
        heap_free( info );
    }
    return ret;
}

static enum fill_status fill_processor( struct table *table, const struct expr *cond )
{
    WCHAR caption[100], device_id[14], processor_id[17], manufacturer[13], name[49] = {0}, version[50];
    struct record_processor *rec;
    UINT i, offset = 0, num_rows = 0, num_logical, num_physical, num_packages;
    enum fill_status status = FILL_STATUS_UNFILTERED;

    num_logical = get_logical_processor_count( &num_physical, &num_packages );

    if (!resize_table( table, num_packages, sizeof(*rec) )) return FILL_STATUS_FAILED;

    get_processor_caption( caption, ARRAY_SIZE( caption ) );
    get_processor_id( processor_id, ARRAY_SIZE( processor_id ) );
    get_processor_manufacturer( manufacturer, ARRAY_SIZE( manufacturer ) );
    get_processor_name( name );
    get_processor_version( version, ARRAY_SIZE( version ) );

    for (i = 0; i < num_packages; i++)
    {
        rec = (struct record_processor *)(table->data + offset);
        rec->addresswidth           = !wcscmp( get_osarchitecture(), L"32-bit" ) ? 32 : 64;
        rec->architecture           = !wcscmp( get_osarchitecture(), L"32-bit" ) ? 0 : 9;
        rec->caption                = heap_strdupW( caption );
        rec->cpu_status             = 1; /* CPU Enabled */
        rec->currentclockspeed      = get_processor_currentclockspeed( i );
        rec->datawidth              = !wcscmp( get_osarchitecture(), L"32-bit" ) ? 32 : 64;
        rec->description            = heap_strdupW( caption );
        swprintf( device_id, ARRAY_SIZE( device_id ), L"CPU%u", i );
        rec->device_id              = heap_strdupW( device_id );
        rec->family                 = 2; /* Unknown */
        rec->level                  = 15;
        rec->manufacturer           = heap_strdupW( manufacturer );
        rec->maxclockspeed          = get_processor_maxclockspeed( i );
        rec->name                   = heap_strdupW( name );
        rec->num_cores              = num_physical / num_packages;
        rec->num_logical_processors = num_logical / num_packages;
        rec->processor_id           = heap_strdupW( processor_id );
        rec->processortype          = 3; /* central processor */
        rec->revision               = get_processor_revision();
        rec->unique_id              = NULL;
        rec->version                = heap_strdupW( version );
        if (!match_row( table, i, cond, &status ))
        {
            free_row_values( table, i );
            continue;
        }
        offset += sizeof(*rec);
        num_rows++;
    }

    TRACE("created %u rows\n", num_rows);
    table->num_rows = num_rows;
    return status;
}

static WCHAR *get_lastbootuptime(void)
{
    SYSTEM_TIMEOFDAY_INFORMATION ti;
    TIME_FIELDS tf;
    WCHAR *ret;

    if (!(ret = heap_alloc( 26 * sizeof(WCHAR) ))) return NULL;

    NtQuerySystemInformation( SystemTimeOfDayInformation, &ti, sizeof(ti), NULL );
    RtlTimeToTimeFields( &ti.liKeBootTime, &tf );
    swprintf( ret, 26, L"%04u%02u%02u%02u%02u%02u.%06u+000", tf.Year, tf.Month, tf.Day, tf.Hour, tf.Minute,
              tf.Second, tf.Milliseconds * 1000 );
    return ret;
}
static WCHAR *get_localdatetime(void)
{
    TIME_ZONE_INFORMATION tzi;
    SYSTEMTIME st;
    WCHAR *ret;
    DWORD Status;
    LONG Bias;

    Status = GetTimeZoneInformation(&tzi);

    if(Status == TIME_ZONE_ID_INVALID) return NULL;
    Bias = tzi.Bias;
    if(Status == TIME_ZONE_ID_DAYLIGHT)
        Bias+= tzi.DaylightBias;
    else
        Bias+= tzi.StandardBias;
    if (!(ret = heap_alloc( 26 * sizeof(WCHAR) ))) return NULL;

    GetLocalTime(&st);
    swprintf( ret, 26, L"%04u%02u%02u%02u%02u%02u.%06u%+03d", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute,
              st.wSecond, st.wMilliseconds * 1000, -Bias );
    return ret;
}
static WCHAR *get_systemdirectory(void)
{
    void *redir;
    WCHAR *ret;

    if (!(ret = heap_alloc( MAX_PATH * sizeof(WCHAR) ))) return NULL;
    Wow64DisableWow64FsRedirection( &redir );
    GetSystemDirectoryW( ret, MAX_PATH );
    Wow64RevertWow64FsRedirection( redir );
    return ret;
}
static WCHAR *get_systemdrive(void)
{
    WCHAR *ret = heap_alloc( 3 * sizeof(WCHAR) ); /* "c:" */
    if (ret && GetEnvironmentVariableW( L"SystemDrive", ret, 3 )) return ret;
    heap_free( ret );
    return NULL;
}
static WCHAR *get_codeset(void)
{
    WCHAR *ret = heap_alloc( 11 * sizeof(WCHAR) );
    if (ret) swprintf( ret, 11, L"%u", GetACP() );
    return ret;
}
static WCHAR *get_countrycode(void)
{
    WCHAR *ret = heap_alloc( 6 * sizeof(WCHAR) );
    if (ret) GetLocaleInfoW( LOCALE_SYSTEM_DEFAULT, LOCALE_ICOUNTRY, ret, 6 );
    return ret;
}
static WCHAR *get_locale(void)
{
    WCHAR *ret = heap_alloc( 5 * sizeof(WCHAR) );
    if (ret) GetLocaleInfoW( LOCALE_SYSTEM_DEFAULT, LOCALE_ILANGUAGE, ret, 5 );
    return ret;
}
static WCHAR *get_osbuildnumber( OSVERSIONINFOEXW *ver )
{
    WCHAR *ret = heap_alloc( 11 * sizeof(WCHAR) );
    if (ret) swprintf( ret, 11, L"%u", ver->dwBuildNumber );
    return ret;
}
static WCHAR *get_oscaption( OSVERSIONINFOEXW *ver )
{
    static const WCHAR windowsW[] = L"Microsoft Windows ";
    static const WCHAR win2000W[] = L"2000 Professional";
    static const WCHAR win2003W[] = L"Server 2003 Standard Edition";
    static const WCHAR winxpW[] = L"XP Professional";
    static const WCHAR winxp64W[] = L"XP Professional x64 Edition";
    static const WCHAR vistaW[] = L"Vista Ultimate";
    static const WCHAR win2008W[] = L"Server 2008 Standard";
    static const WCHAR win7W[] = L"7 Professional";
    static const WCHAR win2008r2W[] = L"Server 2008 R2 Standard";
    static const WCHAR win8W[] = L"8 Pro";
    static const WCHAR win81W[] = L"8.1 Pro";
    static const WCHAR win10W[] = L"10 Pro";
    int len = ARRAY_SIZE( windowsW ) - 1;
    WCHAR *ret;

    if (!(ret = heap_alloc( len * sizeof(WCHAR) + sizeof(win2003W) ))) return NULL;
    memcpy( ret, windowsW, sizeof(windowsW) );
    if (ver->dwMajorVersion == 10 && ver->dwMinorVersion == 0) memcpy( ret + len, win10W, sizeof(win10W) );
    else if (ver->dwMajorVersion == 6 && ver->dwMinorVersion == 3) memcpy( ret + len, win8W, sizeof(win8W) );
    else if (ver->dwMajorVersion == 6 && ver->dwMinorVersion == 2) memcpy( ret + len, win81W, sizeof(win81W) );
    else if (ver->dwMajorVersion == 6 && ver->dwMinorVersion == 1)
    {
        if (ver->wProductType == VER_NT_WORKSTATION) memcpy( ret + len, win7W, sizeof(win7W) );
        else memcpy( ret + len, win2008r2W, sizeof(win2008r2W) );
    }
    else if (ver->dwMajorVersion == 6 && ver->dwMinorVersion == 0)
    {
        if (ver->wProductType == VER_NT_WORKSTATION) memcpy( ret + len, vistaW, sizeof(vistaW) );
        else memcpy( ret + len, win2008W, sizeof(win2008W) );
    }
    else if (ver->dwMajorVersion == 5 && ver->dwMinorVersion == 2)
    {
        if (ver->wProductType == VER_NT_WORKSTATION) memcpy( ret + len, winxp64W, sizeof(winxp64W) );
        else memcpy( ret + len, win2003W, sizeof(win2003W) );
    }
    else if (ver->dwMajorVersion == 5 && ver->dwMinorVersion == 1) memcpy( ret + len, winxpW, sizeof(winxpW) );
    else memcpy( ret + len, win2000W, sizeof(win2000W) );
    return ret;
}
static WCHAR *get_osname( const WCHAR *caption )
{
    static const WCHAR partitionW[] = L"|C:\\WINDOWS|\\Device\\Harddisk0\\Partition1";
    int len = lstrlenW( caption );
    WCHAR *ret;

    if (!(ret = heap_alloc( len * sizeof(WCHAR) + sizeof(partitionW) ))) return NULL;
    memcpy( ret, caption, len * sizeof(WCHAR) );
    memcpy( ret + len, partitionW, sizeof(partitionW) );
    return ret;
}
static WCHAR *get_osserialnumber(void)
{
    HKEY hkey = 0;
    DWORD size, type;
    WCHAR *ret = NULL;

    if (!RegOpenKeyExW( HKEY_LOCAL_MACHINE, L"Software\\Microsoft\\Windows NT\\CurrentVersion", 0, KEY_READ, &hkey ) &&
        !RegQueryValueExW( hkey, L"ProductId", NULL, &type, NULL, &size ) && type == REG_SZ &&
        (ret = heap_alloc( size + sizeof(WCHAR) )))
    {
        size += sizeof(WCHAR);
        if (RegQueryValueExW( hkey, L"ProductId", NULL, NULL, (BYTE *)ret, &size ))
        {
            heap_free( ret );
            ret = NULL;
        }
    }
    if (hkey) RegCloseKey( hkey );
    if (!ret) return heap_strdupW( L"12345-OEM-1234567-12345" );
    return ret;
}
static WCHAR *get_osversion( OSVERSIONINFOEXW *ver )
{
    WCHAR *ret = heap_alloc( 33 * sizeof(WCHAR) );
    if (ret) swprintf( ret, 33, L"%u.%u.%u", ver->dwMajorVersion, ver->dwMinorVersion, ver->dwBuildNumber );
    return ret;
}
static DWORD get_operatingsystemsku(void)
{
    DWORD ret = PRODUCT_UNDEFINED;
    GetProductInfo( 6, 0, 0, 0, &ret );
    return ret;
}
static INT16 get_currenttimezone(void)
{
    TIME_ZONE_INFORMATION info;
    DWORD status = GetTimeZoneInformation( &info );
    if (status == TIME_ZONE_ID_INVALID) return 0;
    if (status == TIME_ZONE_ID_DAYLIGHT) return -(info.Bias + info.DaylightBias);
    return -(info.Bias + info.StandardBias);
}

static enum fill_status fill_operatingsystem( struct table *table, const struct expr *cond )
{
    struct record_operatingsystem *rec;
    enum fill_status status = FILL_STATUS_UNFILTERED;
    OSVERSIONINFOEXW ver;
    UINT row = 0;

    if (!resize_table( table, 1, sizeof(*rec) )) return FILL_STATUS_FAILED;

    ver.dwOSVersionInfoSize = sizeof(ver);
    GetVersionExW( (OSVERSIONINFOW *)&ver );

    rec = (struct record_operatingsystem *)table->data;
    rec->buildnumber            = get_osbuildnumber( &ver );
    rec->caption                = get_oscaption( &ver );
    rec->codeset                = get_codeset();
    rec->countrycode            = get_countrycode();
    rec->csdversion             = ver.szCSDVersion[0] ? heap_strdupW( ver.szCSDVersion ) : NULL;
    rec->csname                 = get_computername();
    rec->currenttimezone        = get_currenttimezone();
    rec->freephysicalmemory     = get_available_physical_memory() / 1024;
    rec->installdate            = L"20140101000000.000000+000";
    rec->lastbootuptime         = get_lastbootuptime();
    rec->localdatetime          = get_localdatetime();
    rec->locale                 = get_locale();
    rec->manufacturer           = L"The Wine Project";
    rec->name                   = get_osname( rec->caption );
    rec->operatingsystemsku     = get_operatingsystemsku();
    rec->osarchitecture         = get_osarchitecture();
    rec->oslanguage             = GetSystemDefaultLangID();
    rec->osproductsuite         = 2461140; /* Windows XP Professional  */
    rec->ostype                 = 18;      /* WINNT */
    rec->primary                = -1;
    rec->serialnumber           = get_osserialnumber();
    rec->servicepackmajor       = ver.wServicePackMajor;
    rec->servicepackminor       = ver.wServicePackMinor;
    rec->suitemask              = 272;     /* Single User + Terminal */
    rec->systemdirectory        = get_systemdirectory();
    rec->systemdrive            = get_systemdrive();
    rec->totalvirtualmemorysize = get_total_physical_memory() / 1024;
    rec->totalvisiblememorysize = rec->totalvirtualmemorysize;
    rec->version                = get_osversion( &ver );
    if (!match_row( table, row, cond, &status )) free_row_values( table, row );
    else row++;

    TRACE("created %u rows\n", row);
    table->num_rows = row;
    return status;
}

static const WCHAR *get_service_type( DWORD type )
{
    if (type & SERVICE_KERNEL_DRIVER)            return L"Kernel Driver";
    else if (type & SERVICE_FILE_SYSTEM_DRIVER)  return L"File System Driver";
    else if (type & SERVICE_WIN32_OWN_PROCESS)   return L"Own Process";
    else if (type & SERVICE_WIN32_SHARE_PROCESS) return L"Share Process";
    else ERR("unhandled type 0x%08x\n", type);
    return NULL;
}
static const WCHAR *get_service_state( DWORD state )
{
    switch (state)
    {
    case SERVICE_STOPPED:       return L"Stopped";
    case SERVICE_START_PENDING: return L"Start Pending";
    case SERVICE_STOP_PENDING:  return L"Stop Pending";
    case SERVICE_RUNNING:       return L"Running";
    default:
        ERR("unknown state %u\n", state);
        return L"Unknown";
    }
}
static const WCHAR *get_service_startmode( DWORD mode )
{
    switch (mode)
    {
    case SERVICE_BOOT_START:   return L"Boot";
    case SERVICE_SYSTEM_START: return L"System";
    case SERVICE_AUTO_START:   return L"Auto";
    case SERVICE_DEMAND_START: return L"Manual";
    case SERVICE_DISABLED:     return L"Disabled";
    default:
        ERR("unknown mode 0x%x\n", mode);
        return L"Unknown";
    }
}
static QUERY_SERVICE_CONFIGW *query_service_config( SC_HANDLE manager, const WCHAR *name )
{
    QUERY_SERVICE_CONFIGW *config = NULL;
    SC_HANDLE service;
    DWORD size;

    if (!(service = OpenServiceW( manager, name, SERVICE_QUERY_CONFIG ))) return NULL;
    QueryServiceConfigW( service, NULL, 0, &size );
    if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) goto done;
    if (!(config = heap_alloc( size ))) goto done;
    if (QueryServiceConfigW( service, config, size, &size )) goto done;
    heap_free( config );
    config = NULL;

done:
    CloseServiceHandle( service );
    return config;
}

static enum fill_status fill_service( struct table *table, const struct expr *cond )
{
    struct record_service *rec;
    SC_HANDLE manager;
    ENUM_SERVICE_STATUS_PROCESSW *tmp, *services = NULL;
    SERVICE_STATUS_PROCESS *status;
    WCHAR sysnameW[MAX_COMPUTERNAME_LENGTH + 1];
    DWORD len = ARRAY_SIZE( sysnameW );
    UINT i, row = 0, offset = 0, size = 256, needed, count;
    enum fill_status fill_status = FILL_STATUS_FAILED;
    BOOL ret;

    if (!(manager = OpenSCManagerW( NULL, NULL, SC_MANAGER_ENUMERATE_SERVICE ))) return FILL_STATUS_FAILED;
    if (!(services = heap_alloc( size ))) goto done;

    ret = EnumServicesStatusExW( manager, SC_ENUM_PROCESS_INFO, SERVICE_TYPE_ALL,
                                 SERVICE_STATE_ALL, (BYTE *)services, size, &needed,
                                 &count, NULL, NULL );
    if (!ret)
    {
        if (GetLastError() != ERROR_MORE_DATA) goto done;
        size = needed;
        if (!(tmp = heap_realloc( services, size ))) goto done;
        services = tmp;
        ret = EnumServicesStatusExW( manager, SC_ENUM_PROCESS_INFO, SERVICE_TYPE_ALL,
                                     SERVICE_STATE_ALL, (BYTE *)services, size, &needed,
                                     &count, NULL, NULL );
        if (!ret) goto done;
    }
    if (!resize_table( table, count, sizeof(*rec) )) goto done;

    GetComputerNameW( sysnameW, &len );
    fill_status = FILL_STATUS_UNFILTERED;

    for (i = 0; i < count; i++)
    {
        QUERY_SERVICE_CONFIGW *config;

        if (!(config = query_service_config( manager, services[i].lpServiceName ))) continue;

        status = &services[i].ServiceStatusProcess;
        rec = (struct record_service *)(table->data + offset);
        rec->accept_pause   = (status->dwControlsAccepted & SERVICE_ACCEPT_PAUSE_CONTINUE) ? -1 : 0;
        rec->accept_stop    = (status->dwControlsAccepted & SERVICE_ACCEPT_STOP) ? -1 : 0;
        rec->displayname    = heap_strdupW( services[i].lpDisplayName );
        rec->name           = heap_strdupW( services[i].lpServiceName );
        rec->process_id     = status->dwProcessId;
        rec->servicetype    = get_service_type( status->dwServiceType );
        rec->startmode      = get_service_startmode( config->dwStartType );
        rec->state          = get_service_state( status->dwCurrentState );
        rec->systemname     = heap_strdupW( sysnameW );
        rec->pause_service  = service_pause_service;
        rec->resume_service = service_resume_service;
        rec->start_service  = service_start_service;
        rec->stop_service   = service_stop_service;
        heap_free( config );
        if (!match_row( table, row, cond, &fill_status ))
        {
            free_row_values( table, row );
            continue;
        }
        offset += sizeof(*rec);
        row++;
    }

    TRACE("created %u rows\n", row);
    table->num_rows = row;

done:
    CloseServiceHandle( manager );
    heap_free( services );
    return fill_status;
}

static WCHAR *get_accountname( LSA_TRANSLATED_NAME *name )
{
    if (!name || !name->Name.Buffer) return NULL;
    return heap_strdupW( name->Name.Buffer );
}
static struct array *get_binaryrepresentation( PSID sid, UINT len )
{
    struct array *ret;
    UINT8 *ptr;

    if (!(ret = heap_alloc( sizeof(*ret) ))) return NULL;
    if (!(ptr = heap_alloc( len )))
    {
        heap_free( ret );
        return NULL;
    }
    memcpy( ptr, sid, len );
    ret->elem_size = sizeof(*ptr);
    ret->count     = len;
    ret->ptr       = ptr;
    return ret;
}
static WCHAR *get_referenceddomainname( LSA_REFERENCED_DOMAIN_LIST *domain )
{
    if (!domain || !domain->Domains || !domain->Domains->Name.Buffer) return NULL;
    return heap_strdupW( domain->Domains->Name.Buffer );
}
static const WCHAR *find_sid_str( const struct expr *cond )
{
    const struct expr *left, *right;
    const WCHAR *ret = NULL;

    if (!cond || cond->type != EXPR_COMPLEX || cond->u.expr.op != OP_EQ) return NULL;

    left = cond->u.expr.left;
    right = cond->u.expr.right;
    if (left->type == EXPR_PROPVAL && right->type == EXPR_SVAL && !wcsicmp( left->u.propval->name, L"SID" ))
    {
        ret = right->u.sval;
    }
    else if (left->type == EXPR_SVAL && right->type == EXPR_PROPVAL && !wcsicmp( right->u.propval->name, L"SID" ))
    {
        ret = left->u.sval;
    }
    return ret;
}

static enum fill_status fill_sid( struct table *table, const struct expr *cond )
{
    PSID sid;
    LSA_REFERENCED_DOMAIN_LIST *domain;
    LSA_TRANSLATED_NAME *name;
    LSA_HANDLE handle;
    LSA_OBJECT_ATTRIBUTES attrs;
    const WCHAR *str;
    struct record_sid *rec;
    UINT len;

    if (!(str = find_sid_str( cond ))) return FILL_STATUS_FAILED;
    if (!resize_table( table, 1, sizeof(*rec) )) return FILL_STATUS_FAILED;

    if (!ConvertStringSidToSidW( str, &sid )) return FILL_STATUS_FAILED;
    len = GetLengthSid( sid );

    memset( &attrs, 0, sizeof(attrs) );
    attrs.Length = sizeof(attrs);
    if (LsaOpenPolicy( NULL, &attrs, POLICY_ALL_ACCESS, &handle ))
    {
        LocalFree( sid );
        return FILL_STATUS_FAILED;
    }
    if (LsaLookupSids( handle, 1, &sid, &domain, &name ))
    {
        LocalFree( sid );
        LsaClose( handle );
        return FILL_STATUS_FAILED;
    }

    rec = (struct record_sid *)table->data;
    rec->accountname            = get_accountname( name );
    rec->binaryrepresentation   = get_binaryrepresentation( sid, len );
    rec->referenceddomainname   = get_referenceddomainname( domain );
    rec->sid                    = heap_strdupW( str );
    rec->sidlength              = len;

    TRACE("created 1 row\n");
    table->num_rows = 1;

    LsaFreeMemory( domain );
    LsaFreeMemory( name );
    LocalFree( sid );
    LsaClose( handle );
    return FILL_STATUS_FILTERED;
}

static WCHAR *get_systemenclosure_string( BYTE id, const char *buf, UINT len )
{
    const struct smbios_header *hdr;
    const struct smbios_chassis *chassis;
    UINT offset;

    if (!(hdr = find_smbios_entry( SMBIOS_TYPE_CHASSIS, buf, len ))) return NULL;

    chassis = (const struct smbios_chassis *)hdr;
    offset = (const char *)chassis - buf + chassis->hdr.length;
    return get_smbios_string( id, buf, offset, len );
}

static WCHAR *get_systemenclosure_manufacturer( const char *buf, UINT len )
{
    WCHAR *ret = get_systemenclosure_string( 1, buf, len );
    if (!ret) return heap_strdupW( L"Wine" );
    return ret;
}

static int get_systemenclosure_lockpresent( const char *buf, UINT len )
{
    const struct smbios_header *hdr;
    const struct smbios_chassis *chassis;

    if (!(hdr = find_smbios_entry( SMBIOS_TYPE_CHASSIS, buf, len )) || hdr->length < sizeof(*chassis)) return 0;

    chassis = (const struct smbios_chassis *)hdr;
    return (chassis->type & 0x80) ? -1 : 0;
}

static struct array *dup_array( const struct array *src )
{
    struct array *dst;
    if (!(dst = heap_alloc( sizeof(*dst) ))) return NULL;
    if (!(dst->ptr = heap_alloc( src->count * src->elem_size )))
    {
        heap_free( dst );
        return NULL;
    }
    memcpy( dst->ptr, src->ptr, src->count * src->elem_size );
    dst->elem_size = src->elem_size;
    dst->count     = src->count;
    return dst;
}

static struct array *get_systemenclosure_chassistypes( const char *buf, UINT len )
{
    const struct smbios_header *hdr;
    const struct smbios_chassis *chassis;
    struct array *ret = NULL;
    UINT16 *types;

    if (!(hdr = find_smbios_entry( SMBIOS_TYPE_CHASSIS, buf, len )) || hdr->length < sizeof(*chassis)) goto done;
    chassis = (const struct smbios_chassis *)hdr;

    if (!(ret = heap_alloc( sizeof(*ret) ))) goto done;
    if (!(types = heap_alloc( sizeof(*types) )))
    {
        heap_free( ret );
        return NULL;
    }
    types[0] = chassis->type & ~0x80;

    ret->elem_size = sizeof(*types);
    ret->count     = 1;
    ret->ptr       = types;

done:
    if (!ret) ret = dup_array( &systemenclosure_chassistypes_array );
    return ret;
}

static enum fill_status fill_systemenclosure( struct table *table, const struct expr *cond )
{
    struct record_systemenclosure *rec;
    enum fill_status status = FILL_STATUS_UNFILTERED;
    UINT row = 0, len;
    char *buf;

    if (!resize_table( table, 1, sizeof(*rec) )) return FILL_STATUS_FAILED;

    len = GetSystemFirmwareTable( RSMB, 0, NULL, 0 );
    if (!(buf = heap_alloc( len ))) return FILL_STATUS_FAILED;
    GetSystemFirmwareTable( RSMB, 0, buf, len );

    rec = (struct record_systemenclosure *)table->data;
    rec->caption      = L"System Enclosure";
    rec->chassistypes = get_systemenclosure_chassistypes( buf, len );
    rec->description  = L"System Enclosure";
    rec->lockpresent  = get_systemenclosure_lockpresent( buf, len );
    rec->manufacturer = get_systemenclosure_manufacturer( buf, len );
    rec->name         = L"System Enclosure";
    rec->tag          = L"System Enclosure 0";
    if (!match_row( table, row, cond, &status )) free_row_values( table, row );
    else row++;

    heap_free( buf );

    TRACE("created %u rows\n", row);
    table->num_rows = row;
    return status;
}

static WCHAR *get_pnpdeviceid( DXGI_ADAPTER_DESC *desc )
{
    static const WCHAR fmtW[] = L"PCI\\VEN_%04X&DEV_%04X&SUBSYS_%08X&REV_%02X\\0&DEADBEEF&0&DEAD";
    UINT len = sizeof(fmtW) + 2;
    WCHAR *ret;

    if (!(ret = heap_alloc( len * sizeof(WCHAR) ))) return NULL;
    swprintf( ret, len, fmtW, desc->VendorId, desc->DeviceId, desc->SubSysId, desc->Revision );
    return ret;
}

#define HW_VENDOR_AMD    0x1002
#define HW_VENDOR_NVIDIA 0x10de
#define HW_VENDOR_VMWARE 0x15ad
#define HW_VENDOR_INTEL  0x8086

static const WCHAR *get_installeddriver( UINT vendorid )
{
    /* FIXME: wined3d has a better table, but we cannot access this information through dxgi */

    if (vendorid == HW_VENDOR_AMD) return L"aticfx32.dll";
    else if (vendorid == HW_VENDOR_NVIDIA) return L"nvd3dum.dll";
    else if (vendorid == HW_VENDOR_INTEL) return L"igdudim32.dll";
    return L"wine.dll";
}

static enum fill_status fill_videocontroller( struct table *table, const struct expr *cond )
{
    struct record_videocontroller *rec;
    HRESULT hr;
    IDXGIFactory *factory = NULL;
    IDXGIAdapter *adapter = NULL;
    DXGI_ADAPTER_DESC desc;
    UINT row = 0, hres = 1024, vres = 768, vidmem = 512 * 1024 * 1024;
    const WCHAR *name = L"VideoController1";
    enum fill_status status = FILL_STATUS_UNFILTERED;
    WCHAR mode[44];

    if (!resize_table( table, 1, sizeof(*rec) )) return FILL_STATUS_FAILED;

    memset (&desc, 0, sizeof(desc));
    hr = CreateDXGIFactory( &IID_IDXGIFactory, (void **)&factory );
    if (FAILED(hr)) goto done;

    hr = IDXGIFactory_EnumAdapters( factory, 0, &adapter );
    if (FAILED(hr)) goto done;

    hr = IDXGIAdapter_GetDesc( adapter, &desc );
    if (SUCCEEDED(hr))
    {
        vidmem = desc.DedicatedVideoMemory;
        name   = desc.Description;
    }

done:
    rec = (struct record_videocontroller *)table->data;
    rec->adapter_compatibility = L"(Standard display types)";
    rec->adapter_dactype       = L"Integrated RAMDAC";
    rec->adapter_ram           = vidmem;
    rec->availability          = 3; /* Running or Full Power */
    rec->config_errorcode      = 0; /* no error */
    rec->caption               = heap_strdupW( name );
    rec->current_bitsperpixel  = get_bitsperpixel( &hres, &vres );
    rec->current_horizontalres = hres;
    rec->current_refreshrate   = 0; /* default refresh rate */
    rec->current_scanmode      = 2; /* Unknown */
    rec->current_verticalres   = vres;
    rec->description           = heap_strdupW( name );
    rec->device_id             = L"VideoController1";
    rec->driverdate            = L"20170101000000.000000+000";
    rec->driverversion         = L"1.0";
    rec->installeddriver       = get_installeddriver( desc.VendorId );
    rec->name                  = heap_strdupW( name );
    rec->pnpdevice_id          = get_pnpdeviceid( &desc );
    rec->status                = L"OK";
    rec->videoarchitecture     = 2; /* Unknown */
    rec->videomemorytype       = 2; /* Unknown */
    swprintf( mode, ARRAY_SIZE( mode ), L"%u x %u x %I64u colors", hres, vres, (UINT64)1 << rec->current_bitsperpixel );
    rec->videomodedescription  = heap_strdupW( mode );
    rec->videoprocessor        = heap_strdupW( name );
    if (!match_row( table, row, cond, &status )) free_row_values( table, row );
    else row++;

    TRACE("created %u rows\n", row);
    table->num_rows = row;

    if (adapter) IDXGIAdapter_Release( adapter );
    if (factory) IDXGIFactory_Release( factory );
    return status;
}

#define C(c) sizeof(c)/sizeof(c[0]), c
#define D(d) sizeof(d)/sizeof(d[0]), 0, (BYTE *)d
static struct table builtin_classes[] =
{
    { L"__ASSOCIATORS", C(col_associator), D(data_associator) },
    { L"__PARAMETERS", C(col_param), D(data_param) },
    { L"__QUALIFIERS", C(col_qualifier), D(data_qualifier) },
    { L"__SystemSecurity", C(col_systemsecurity), D(data_systemsecurity) },
    { L"CIM_DataFile", C(col_datafile), 0, 0, NULL, fill_datafile },
    { L"CIM_LogicalDisk", C(col_logicaldisk), 0, 0, NULL, fill_logicaldisk },
    { L"CIM_Processor", C(col_processor), 0, 0, NULL, fill_processor },
    { L"StdRegProv", C(col_stdregprov), D(data_stdregprov) },
    { L"Win32_BIOS", C(col_bios), 0, 0, NULL, fill_bios },
    { L"Win32_BaseBoard", C(col_baseboard), 0, 0, NULL, fill_baseboard },
    { L"Win32_CDROMDrive", C(col_cdromdrive), 0, 0, NULL, fill_cdromdrive },
    { L"Win32_ComputerSystem", C(col_compsys), 0, 0, NULL, fill_compsys },
    { L"Win32_ComputerSystemProduct", C(col_compsysproduct), 0, 0, NULL, fill_compsysproduct },
    { L"Win32_DesktopMonitor", C(col_desktopmonitor), 0, 0, NULL, fill_desktopmonitor },
    { L"Win32_Directory", C(col_directory), 0, 0, NULL, fill_directory },
    { L"Win32_DiskDrive", C(col_diskdrive), 0, 0, NULL, fill_diskdrive },
    { L"Win32_DiskDriveToDiskPartition", C(col_diskdrivetodiskpartition), 0, 0, NULL, fill_diskdrivetodiskpartition },
    { L"Win32_DiskPartition", C(col_diskpartition), 0, 0, NULL, fill_diskpartition },
    { L"Win32_DisplayControllerConfiguration", C(col_displaycontrollerconfig), 0, 0, NULL, fill_displaycontrollerconfig },
    { L"Win32_IP4RouteTable", C(col_ip4routetable), 0, 0, NULL, fill_ip4routetable },
    { L"Win32_LogicalDisk", C(col_logicaldisk), 0, 0, NULL, fill_logicaldisk },
    { L"Win32_LogicalDiskToPartition", C(col_logicaldisktopartition), 0, 0, NULL, fill_logicaldisktopartition },
    { L"Win32_NetworkAdapter", C(col_networkadapter), 0, 0, NULL, fill_networkadapter },
    { L"Win32_NetworkAdapterConfiguration", C(col_networkadapterconfig), 0, 0, NULL, fill_networkadapterconfig },
    { L"Win32_OperatingSystem", C(col_operatingsystem), 0, 0, NULL, fill_operatingsystem },
    { L"Win32_PhysicalMedia", C(col_physicalmedia), D(data_physicalmedia) },
    { L"Win32_PhysicalMemory", C(col_physicalmemory), 0, 0, NULL, fill_physicalmemory },
    { L"Win32_PnPEntity", C(col_pnpentity), 0, 0, NULL, fill_pnpentity },
    { L"Win32_Printer", C(col_printer), 0, 0, NULL, fill_printer },
    { L"Win32_Process", C(col_process), 0, 0, NULL, fill_process },
    { L"Win32_Processor", C(col_processor), 0, 0, NULL, fill_processor },
    { L"Win32_QuickFixEngineering", C(col_quickfixengineering), D(data_quickfixengineering) },
    { L"Win32_SID", C(col_sid), 0, 0, NULL, fill_sid },
    { L"Win32_Service", C(col_service), 0, 0, NULL, fill_service },
    { L"Win32_SoundDevice", C(col_sounddevice), D(data_sounddevice) },
    { L"Win32_SystemEnclosure", C(col_systemenclosure), 0, 0, NULL, fill_systemenclosure },
    { L"Win32_VideoController", C(col_videocontroller), 0, 0, NULL, fill_videocontroller },
    { L"Win32_WinSAT", C(col_winsat), D(data_winsat) },
};
#undef C
#undef D

void init_table_list( void )
{
    static struct list tables = LIST_INIT( tables );
    UINT i;

    for (i = 0; i < ARRAY_SIZE(builtin_classes); i++) list_add_tail( &tables, &builtin_classes[i].entry );
    table_list = &tables;
}
