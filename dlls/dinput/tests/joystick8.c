/*
 * Copyright 2022 Rémi Bernon for CodeWeavers
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

#define COBJMACROS
#include "dinput.h"
#include "dinputd.h"
#include "devguid.h"
#include "mmsystem.h"
#include "roapi.h"
#include "unknwn.h"
#include "winstring.h"

#include "wine/hid.h"

#include "dinput_test.h"

#define WIDL_using_Windows_Foundation
#define WIDL_using_Windows_Foundation_Collections
#include "windows.foundation.h"
#define WIDL_using_Windows_Devices_Haptics
#define WIDL_using_Windows_Gaming_Input
#include "windows.gaming.input.h"
#undef Size

#define MAKE_FUNC(f) static typeof(f) *p ## f
MAKE_FUNC( RoGetActivationFactory );
MAKE_FUNC( RoInitialize );
MAKE_FUNC( WindowsCreateString );
MAKE_FUNC( WindowsDeleteString );
MAKE_FUNC( WindowsGetStringRawBuffer );
#undef MAKE_FUNC

static BOOL load_combase_functions(void)
{
    HMODULE combase = GetModuleHandleW( L"combase.dll" );

#define LOAD_FUNC(m, f) if (!(p ## f = (void *)GetProcAddress( m, #f ))) goto failed;
    LOAD_FUNC( combase, RoGetActivationFactory );
    LOAD_FUNC( combase, RoInitialize );
    LOAD_FUNC( combase, WindowsCreateString );
    LOAD_FUNC( combase, WindowsDeleteString );
    LOAD_FUNC( combase, WindowsGetStringRawBuffer );
#undef LOAD_FUNC

    return TRUE;

failed:
    win_skip("Failed to load combase.dll functions, skipping tests\n");
    return FALSE;
}

struct check_objects_todos
{
    BOOL type;
    BOOL ofs;
    BOOL guid;
    BOOL flags;
    BOOL usage;
    BOOL usage_page;
    BOOL name;
};

struct check_objects_params
{
    DWORD version;
    UINT index;
    UINT expect_count;
    const DIDEVICEOBJECTINSTANCEW *expect_objs;
    const struct check_objects_todos *todo_objs;
    BOOL todo_extra;
};

static BOOL CALLBACK check_objects( const DIDEVICEOBJECTINSTANCEW *obj, void *args )
{
    static const DIDEVICEOBJECTINSTANCEW unexpected_obj = {0};
    static const struct check_objects_todos todo_none = {0};
    struct check_objects_params *params = args;
    const DIDEVICEOBJECTINSTANCEW *exp = params->expect_objs + params->index;
    const struct check_objects_todos *todo;

    if (!params->todo_objs) todo = &todo_none;
    else todo = params->todo_objs + params->index;

    todo_wine_if( params->todo_extra && params->index >= params->expect_count )
    ok( params->index < params->expect_count, "unexpected extra object\n" );
    if (params->index >= params->expect_count) return DIENUM_STOP;

    winetest_push_context( "obj[%d]", params->index );

    ok( params->index < params->expect_count, "unexpected extra object\n" );
    if (params->index >= params->expect_count) exp = &unexpected_obj;

    check_member( *obj, *exp, "%lu", dwSize );
    todo_wine_if( todo->guid )
    check_member_guid( *obj, *exp, guidType );
    todo_wine_if( todo->ofs )
    check_member( *obj, *exp, "%#lx", dwOfs );
    todo_wine_if( todo->type )
    check_member( *obj, *exp, "%#lx", dwType );
    todo_wine_if( todo->flags )
    check_member( *obj, *exp, "%#lx", dwFlags );
    if (!localized) todo_wine_if( todo->name )check_member_wstr( *obj, *exp, tszName );
    check_member( *obj, *exp, "%lu", dwFFMaxForce );
    check_member( *obj, *exp, "%lu", dwFFForceResolution );
    check_member( *obj, *exp, "%u", wCollectionNumber );
    check_member( *obj, *exp, "%u", wDesignatorIndex );
    todo_wine_if( todo->usage_page )
    check_member( *obj, *exp, "%#04x", wUsagePage );
    todo_wine_if( todo->usage )
    check_member( *obj, *exp, "%#04x", wUsage );
    check_member( *obj, *exp, "%#lx", dwDimension );
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

struct check_effects_params
{
    UINT index;
    UINT expect_count;
    const DIEFFECTINFOW *expect_effects;
};

static BOOL CALLBACK check_effects( const DIEFFECTINFOW *effect, void *args )
{
    static const DIEFFECTINFOW unexpected_effect = {0};
    struct check_effects_params *params = args;
    const DIEFFECTINFOW *exp = params->expect_effects + params->index;

    winetest_push_context( "effect[%d]", params->index );

    ok( params->index < params->expect_count, "unexpected extra object\n" );
    if (params->index >= params->expect_count) exp = &unexpected_effect;

    check_member( *effect, *exp, "%lu", dwSize );
    check_member_guid( *effect, *exp, guid );
    check_member( *effect, *exp, "%#lx", dwEffType );
    check_member( *effect, *exp, "%#lx", dwStaticParams );
    check_member( *effect, *exp, "%#lx", dwDynamicParams );
    check_member_wstr( *effect, *exp, tszName );

    winetest_pop_context();
    params->index++;

    return DIENUM_CONTINUE;
}

static BOOL CALLBACK check_effect_count( const DIEFFECTINFOW *effect, void *args )
{
    DWORD *count = args;
    *count = *count + 1;
    return DIENUM_CONTINUE;
}

static BOOL CALLBACK check_no_created_effect_objects( IDirectInputEffect *effect, void *context )
{
    ok( 0, "unexpected effect %p\n", effect );
    return DIENUM_CONTINUE;
}

static BOOL CALLBACK enum_device_count( const DIDEVICEINSTANCEW *devinst, void *context )
{
    DWORD *count = context;
    *count = *count + 1;
    return DIENUM_CONTINUE;
}

static void check_dinput_devices( DWORD version, DIDEVICEINSTANCEW *devinst )
{
    DIPROPDWORD prop_dword =
    {
        .diph =
        {
            .dwSize = sizeof(DIPROPDWORD),
            .dwHeaderSize = sizeof(DIPROPHEADER),
            .dwHow = DIPH_DEVICE,
        },
    };
    IDirectInputDevice8W *device;
    IDirectInput8W *di8;
    IDirectInputW *di;
    ULONG ref, count;
    HRESULT hr;

    if (version >= 0x800)
    {
        hr = DirectInput8Create( instance, version, &IID_IDirectInput8W, (void **)&di8, NULL );
        ok( hr == DI_OK, "DirectInput8Create returned %#lx\n", hr );

        hr = IDirectInput8_EnumDevices( di8, DI8DEVCLASS_ALL, NULL, NULL, DIEDFL_ALLDEVICES );
        ok( hr == DIERR_INVALIDPARAM, "EnumDevices returned: %#lx\n", hr );
        hr = IDirectInput8_EnumDevices( di8, DI8DEVCLASS_ALL, enum_device_count, &count, 0xdeadbeef );
        ok( hr == DIERR_INVALIDPARAM, "EnumDevices returned: %#lx\n", hr );
        hr = IDirectInput8_EnumDevices( di8, 0xdeadbeef, enum_device_count, &count, DIEDFL_ALLDEVICES );
        ok( hr == DIERR_INVALIDPARAM, "EnumDevices returned: %#lx\n", hr );

        count = 0;
        hr = IDirectInput8_EnumDevices( di8, DI8DEVCLASS_ALL, enum_device_count, &count, DIEDFL_ALLDEVICES );
        ok( hr == DI_OK, "EnumDevices returned: %#lx\n", hr );
        ok( count == 3, "got count %lu, expected 0\n", count );
        count = 0;
        hr = IDirectInput8_EnumDevices( di8, DI8DEVCLASS_DEVICE, enum_device_count, &count, DIEDFL_ALLDEVICES );
        ok( hr == DI_OK, "EnumDevices returned: %#lx\n", hr );
        ok( count == 0, "got count %lu, expected 0\n", count );
        count = 0;
        hr = IDirectInput8_EnumDevices( di8, DI8DEVCLASS_POINTER, enum_device_count, &count,
                                        DIEDFL_INCLUDEALIASES | DIEDFL_INCLUDEPHANTOMS | DIEDFL_INCLUDEHIDDEN );
        ok( hr == DI_OK, "EnumDevices returned: %#lx\n", hr );
        todo_wine
        ok( count == 3, "got count %lu, expected 3\n", count );
        count = 0;
        hr = IDirectInput8_EnumDevices( di8, DI8DEVCLASS_KEYBOARD, enum_device_count, &count,
                                        DIEDFL_INCLUDEALIASES | DIEDFL_INCLUDEPHANTOMS | DIEDFL_INCLUDEHIDDEN );
        ok( hr == DI_OK, "EnumDevices returned: %#lx\n", hr );
        todo_wine
        ok( count == 3, "got count %lu, expected 3\n", count );
        count = 0;
        hr = IDirectInput8_EnumDevices( di8, DI8DEVCLASS_GAMECTRL, enum_device_count, &count,
                                        DIEDFL_INCLUDEALIASES | DIEDFL_INCLUDEPHANTOMS | DIEDFL_INCLUDEHIDDEN );
        ok( hr == DI_OK, "EnumDevices returned: %#lx\n", hr );
        ok( count == 1, "got count %lu, expected 1\n", count );

        count = 0;
        hr = IDirectInput8_EnumDevices( di8, (devinst->dwDevType & 0xff), enum_device_count, &count, DIEDFL_ALLDEVICES );
        ok( hr == DI_OK, "EnumDevices returned: %#lx\n", hr );
        ok( count == 1, "got count %lu, expected 1\n", count );

        count = 0;
        hr = IDirectInput8_EnumDevices( di8, (devinst->dwDevType & 0xff), enum_device_count, &count, DIEDFL_FORCEFEEDBACK );
        ok( hr == DI_OK, "EnumDevices returned: %#lx\n", hr );
        if (IsEqualGUID( &devinst->guidFFDriver, &GUID_NULL )) ok( count == 0, "got count %lu, expected 0\n", count );
        else ok( count == 1, "got count %lu, expected 1\n", count );

        count = 0;
        hr = IDirectInput8_EnumDevices( di8, (devinst->dwDevType & 0xff) + 1, enum_device_count, &count, DIEDFL_ALLDEVICES );
        if ((devinst->dwDevType & 0xff) != DI8DEVTYPE_SUPPLEMENTAL) ok( hr == DI_OK, "EnumDevices returned: %#lx\n", hr );
        else ok( hr == DIERR_INVALIDPARAM, "EnumDevices returned: %#lx\n", hr );
        ok( count == 0, "got count %lu, expected 0\n", count );

        hr = IDirectInput8_CreateDevice( di8, &devinst->guidInstance, NULL, NULL );
        ok( hr == E_POINTER, "CreateDevice returned %#lx\n", hr );
        hr = IDirectInput8_CreateDevice( di8, NULL, &device, NULL );
        ok( hr == E_POINTER, "CreateDevice returned %#lx\n", hr );
        hr = IDirectInput8_CreateDevice( di8, &GUID_NULL, &device, NULL );
        ok( hr == DIERR_DEVICENOTREG, "CreateDevice returned %#lx\n", hr );

        hr = IDirectInput8_CreateDevice( di8, &devinst->guidInstance, &device, NULL );
        ok( hr == DI_OK, "CreateDevice returned %#lx\n", hr );

        prop_dword.dwData = 0xdeadbeef;
        hr = IDirectInputDevice8_GetProperty( device, DIPROP_VIDPID, &prop_dword.diph );
        ok( hr == DI_OK, "GetProperty DIPROP_VIDPID returned %#lx\n", hr );
        ok( prop_dword.dwData == EXPECT_VIDPID, "got %#lx expected %#lx\n", prop_dword.dwData, EXPECT_VIDPID );

        ref = IDirectInputDevice8_Release( device );
        ok( ref == 0, "Release returned %ld\n", ref );
    }
    else
    {
        hr = DirectInputCreateEx( instance, version, &IID_IDirectInput2W, (void **)&di, NULL );
        ok( hr == DI_OK, "DirectInputCreateEx returned %#lx\n", hr );

        hr = IDirectInput_EnumDevices( di, 0, NULL, NULL, DIEDFL_ALLDEVICES );
        ok( hr == DIERR_INVALIDPARAM, "EnumDevices returned: %#lx\n", hr );
        hr = IDirectInput_EnumDevices( di, 0, enum_device_count, &count, 0xdeadbeef );
        ok( hr == DIERR_INVALIDPARAM, "EnumDevices returned: %#lx\n", hr );
        hr = IDirectInput_EnumDevices( di, 0xdeadbeef, enum_device_count, &count, DIEDFL_ALLDEVICES );
        ok( hr == DIERR_INVALIDPARAM, "EnumDevices returned: %#lx\n", hr );
        hr = IDirectInput_EnumDevices( di, 0, enum_device_count, &count, DIEDFL_INCLUDEHIDDEN );
        ok( hr == DIERR_INVALIDPARAM, "EnumDevices returned: %#lx\n", hr );

        count = 0;
        hr = IDirectInput_EnumDevices( di, 0, enum_device_count, &count, DIEDFL_ALLDEVICES );
        ok( hr == DI_OK, "EnumDevices returned: %#lx\n", hr );
        ok( count == 3, "got count %lu, expected 0\n", count );
        count = 0;
        hr = IDirectInput_EnumDevices( di, DIDEVTYPE_DEVICE, enum_device_count, &count, DIEDFL_ALLDEVICES );
        ok( hr == DI_OK, "EnumDevices returned: %#lx\n", hr );
        ok( count == 0, "got count %lu, expected 0\n", count );
        count = 0;
        hr = IDirectInput_EnumDevices( di, DIDEVTYPE_MOUSE, enum_device_count, &count,
                                       DIEDFL_INCLUDEALIASES | DIEDFL_INCLUDEPHANTOMS );
        ok( hr == DI_OK, "EnumDevices returned: %#lx\n", hr );
        todo_wine
        ok( count == 3, "got count %lu, expected 3\n", count );
        count = 0;
        hr = IDirectInput_EnumDevices( di, DIDEVTYPE_KEYBOARD, enum_device_count, &count,
                                       DIEDFL_INCLUDEALIASES | DIEDFL_INCLUDEPHANTOMS );
        ok( hr == DI_OK, "EnumDevices returned: %#lx\n", hr );
        todo_wine
        ok( count == 3, "got count %lu, expected 3\n", count );
        count = 0;
        hr = IDirectInput_EnumDevices( di, DIDEVTYPE_JOYSTICK, enum_device_count, &count,
                                       DIEDFL_INCLUDEALIASES | DIEDFL_INCLUDEPHANTOMS );
        ok( hr == DI_OK, "EnumDevices returned: %#lx\n", hr );
        ok( count == 1, "got count %lu, expected 1\n", count );

        count = 0;
        hr = IDirectInput_EnumDevices( di, (devinst->dwDevType & 0xff), enum_device_count, &count, DIEDFL_ALLDEVICES );
        ok( hr == DI_OK, "EnumDevices returned: %#lx\n", hr );
        ok( count == 1, "got count %lu, expected 1\n", count );

        count = 0;
        hr = IDirectInput_EnumDevices( di, (devinst->dwDevType & 0xff), enum_device_count, &count, DIEDFL_FORCEFEEDBACK );
        ok( hr == DI_OK, "EnumDevices returned: %#lx\n", hr );
        if (IsEqualGUID( &devinst->guidFFDriver, &GUID_NULL )) ok( count == 0, "got count %lu, expected 0\n", count );
        else ok( count == 1, "got count %lu, expected 1\n", count );

        hr = IDirectInput_EnumDevices( di, 0x14, enum_device_count, &count, DIEDFL_ALLDEVICES );
        ok( hr == DIERR_INVALIDPARAM, "EnumDevices returned: %#lx\n", hr );
    }
}

static void test_simple_joystick( DWORD version )
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
                USAGE(4, (HID_USAGE_PAGE_SIMULATION<<16)|HID_USAGE_SIMULATION_RUDDER),
                USAGE(4, (HID_USAGE_PAGE_DIGITIZER<<16)|HID_USAGE_DIGITIZER_TIP_PRESSURE),
                USAGE(4, (HID_USAGE_PAGE_CONSUMER<<16)|HID_USAGE_CONSUMER_VOLUME),
                LOGICAL_MINIMUM(1, 0xe7),
                LOGICAL_MAXIMUM(1, 0x38),
                PHYSICAL_MINIMUM(1, 0xe7),
                PHYSICAL_MAXIMUM(1, 0x38),
                REPORT_SIZE(1, 8),
                REPORT_COUNT(1, 7),
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

    struct hid_device_desc desc =
    {
        .use_report_id = TRUE,
        .caps = { .InputReportByteLength = 9 },
        .attributes = default_attributes,
    };
    const DIDEVCAPS expect_caps =
    {
        .dwSize = sizeof(DIDEVCAPS),
        .dwFlags = DIDC_ATTACHED | DIDC_EMULATED,
        .dwDevType = version >= 0x800 ? DIDEVTYPE_HID | (DI8DEVTYPEJOYSTICK_LIMITED << 8) | DI8DEVTYPE_JOYSTICK
                                      : DIDEVTYPE_HID | (DIDEVTYPEJOYSTICK_RUDDER << 8) | DIDEVTYPE_JOYSTICK,
        .dwAxes = 6,
        .dwPOVs = 1,
        .dwButtons = 2,
    };
    struct hid_expect injected_input[] =
    {
        {
            .code = IOCTL_HID_READ_REPORT,
            .report_buf = {1,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0},
        },
        {
            .code = IOCTL_HID_READ_REPORT,
            .report_buf = {1,0x10,0x10,0x38,0x38,0x10,0x10,0x10,0xf8},
        },
        {
            .code = IOCTL_HID_READ_REPORT,
            .report_buf = {1,0x10,0x10,0x01,0x01,0x10,0x10,0x10,0x00},
        },
        {
            .code = IOCTL_HID_READ_REPORT,
            .report_buf = {1,0x10,0x10,0x01,0x01,0x10,0x10,0x10,0x00},
        },
        {
            .code = IOCTL_HID_READ_REPORT,
            .report_buf = {1,0x10,0x10,0x80,0x80,0x10,0x10,0x10,0xff},
        },
        {
            .code = IOCTL_HID_READ_REPORT,
            .report_buf = {1,0x10,0x10,0x10,0xee,0x10,0x10,0x10,0x54},
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
        .dwDevType = version >= 0x800 ? DIDEVTYPE_HID | (DI8DEVTYPEJOYSTICK_LIMITED << 8) | DI8DEVTYPE_JOYSTICK
                                      : DIDEVTYPE_HID | (DIDEVTYPEJOYSTICK_RUDDER << 8) | DIDEVTYPE_JOYSTICK,
        .tszInstanceName = L"Wine Test",
        .tszProductName = L"Wine Test",
        .guidFFDriver = GUID_NULL,
        .wUsagePage = HID_USAGE_PAGE_GENERIC,
        .wUsage = HID_USAGE_GENERIC_JOYSTICK,
    };
    const DIDEVICEOBJECTINSTANCEW expect_objects[] =
    {
        {
            .dwSize = sizeof(DIDEVICEOBJECTINSTANCEW),
            .guidType = GUID_Unknown,
            .dwType = DIDFT_ABSAXIS|DIDFT_MAKEINSTANCE(6),
            .tszName = L"Volume",
            .wCollectionNumber = 1,
            .wUsagePage = HID_USAGE_PAGE_CONSUMER,
            .wUsage = HID_USAGE_CONSUMER_VOLUME,
            .wReportId = 1,
        },
        {
            .dwSize = sizeof(DIDEVICEOBJECTINSTANCEW),
            .guidType = GUID_Unknown,
            .dwOfs = 0x4,
            .dwType = DIDFT_ABSAXIS|DIDFT_MAKEINSTANCE(7),
            .tszName = L"Tip Pressure",
            .wCollectionNumber = 1,
            .wUsagePage = HID_USAGE_PAGE_DIGITIZER,
            .wUsage = HID_USAGE_DIGITIZER_TIP_PRESSURE,
            .wReportId = 1,
        },
        {
            .dwSize = sizeof(DIDEVICEOBJECTINSTANCEW),
            .guidType = GUID_RzAxis,
            .dwOfs = 0x8,
            .dwType = DIDFT_ABSAXIS|DIDFT_MAKEINSTANCE(5),
            .dwFlags = DIDOI_ASPECTPOSITION,
            .tszName = L"Rudder",
            .wCollectionNumber = 1,
            .wUsagePage = HID_USAGE_PAGE_SIMULATION,
            .wUsage = HID_USAGE_SIMULATION_RUDDER,
            .wReportId = 1,
        },
        {
            .dwSize = sizeof(DIDEVICEOBJECTINSTANCEW),
            .guidType = GUID_YAxis,
            .dwOfs = 0xc,
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
            .dwOfs = 0x10,
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
            .dwOfs = 0x18,
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
            .dwOfs = 0x1c,
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
            .dwOfs = 0x20,
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
            .dwOfs = 0x21,
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
    const DIDEVICEOBJECTINSTANCEW expect_objects_5[] =
    {
        {
            .dwSize = sizeof(DIDEVICEOBJECTINSTANCEW),
            .guidType = GUID_XAxis,
            .dwOfs = DIJOFS_X,
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
            .guidType = GUID_YAxis,
            .dwOfs = DIJOFS_Y,
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
            .guidType = GUID_ZAxis,
            .dwOfs = DIJOFS_Z,
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
            .guidType = GUID_RzAxis,
            .dwOfs = DIJOFS_RZ,
            .dwType = DIDFT_ABSAXIS|DIDFT_MAKEINSTANCE(5),
            .dwFlags = DIDOI_ASPECTPOSITION,
            .tszName = L"Rudder",
            .wCollectionNumber = 1,
            .wUsagePage = HID_USAGE_PAGE_SIMULATION,
            .wUsage = HID_USAGE_SIMULATION_RUDDER,
            .wReportId = 1,
        },
        {
            .dwSize = sizeof(DIDEVICEOBJECTINSTANCEW),
            .guidType = GUID_POV,
            .dwOfs = DIJOFS_POV(0),
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
            .dwOfs = DIJOFS_BUTTON(0),
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
            .dwOfs = DIJOFS_BUTTON(1),
            .dwType = DIDFT_PSHBUTTON|DIDFT_MAKEINSTANCE(1),
            .tszName = L"Button 1",
            .wCollectionNumber = 1,
            .wUsagePage = HID_USAGE_PAGE_BUTTON,
            .wUsage = 0x2,
            .wReportId = 1,
        },
    };
    struct check_objects_todos todo_objects_5[ARRAY_SIZE(expect_objects_5)] =
    {
        {.guid = TRUE, .type = TRUE, .flags = TRUE, .usage = TRUE, .usage_page = TRUE, .name = TRUE},
        {.guid = TRUE, .type = TRUE, .flags = TRUE, .usage = TRUE, .usage_page = TRUE, .name = TRUE},
        {.guid = TRUE, .type = TRUE, .usage = TRUE, .usage_page = TRUE, .name = TRUE},
        {.guid = TRUE, .ofs = TRUE, .type = TRUE, .usage = TRUE, .usage_page = TRUE, .name = TRUE},
        {.guid = TRUE, .ofs = TRUE, .type = TRUE, .flags = TRUE, .usage = TRUE, .name = TRUE},
        {.guid = TRUE, .ofs = TRUE, .type = TRUE, .flags = TRUE, .usage = TRUE, .usage_page = TRUE, .name = TRUE},
        {.guid = TRUE, .ofs = TRUE, .type = TRUE, .usage = TRUE, .usage_page = TRUE, .name = TRUE},
    };
    struct check_objects_params check_objects_params =
    {
        .version = version,
        .expect_count = version < 0x700 ? ARRAY_SIZE(expect_objects_5) : ARRAY_SIZE(expect_objects),
        .expect_objs = version < 0x700 ? expect_objects_5 : expect_objects,
        .todo_objs = version < 0x700 ? todo_objects_5 : NULL,
        .todo_extra = version < 0x700 ? TRUE : FALSE,
    };

    const DIEFFECTINFOW expect_effects[] = {};
    struct check_effects_params check_effects_params =
    {
        .expect_count = ARRAY_SIZE(expect_effects),
        .expect_effects = expect_effects,
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
    DIOBJECTDATAFORMAT objdataformat[32] = {{0}};
    DIDEVICEOBJECTDATA objdata[32] = {{0}};
    DIDEVICEOBJECTINSTANCEW objinst = {0};
    DIDEVICEOBJECTINSTANCEW expect_obj;
    DIDEVICEINSTANCEW devinst = {0};
    DIEFFECTINFOW effectinfo = {0};
    DIDATAFORMAT dataformat = {0};
    IDirectInputDevice8W *device;
    IDirectInputEffect *effect;
    DIEFFESCAPE escape = {0};
    ULONG i, size, res, ref;
    DIDEVCAPS caps = {0};
    HANDLE event, file;
    char buffer[1024];
    DIJOYSTATE2 state;
    HRESULT hr;
    GUID guid;
    HWND hwnd;

    winetest_push_context( "%#lx", version );

    cleanup_registry_keys();

    desc.report_descriptor_len = sizeof(report_desc);
    memcpy( desc.report_descriptor_buf, report_desc, sizeof(report_desc) );
    fill_context( __LINE__, desc.context, ARRAY_SIZE(desc.context) );

    if (!hid_device_start( &desc )) goto done;
    if (FAILED(hr = dinput_test_create_device( version, &devinst, &device ))) goto done;

    check_dinput_devices( version, &devinst );

    hr = IDirectInputDevice8_Initialize( device, instance, 0x800 - (version - 0x700), &GUID_NULL );
    if (version == 0x800)
    {
        todo_wine
        ok( hr == DIERR_BETADIRECTINPUTVERSION, "Initialize returned %#lx\n", hr );
    }
    else
    {
        todo_wine
        ok( hr == DIERR_OLDDIRECTINPUTVERSION, "Initialize returned %#lx\n", hr );
    }
    hr = IDirectInputDevice8_Initialize( device, instance, version, NULL );
    todo_wine
    ok( hr == E_POINTER, "Initialize returned %#lx\n", hr );
    hr = IDirectInputDevice8_Initialize( device, NULL, version, &GUID_NULL );
    todo_wine
    ok( hr == DIERR_INVALIDPARAM, "Initialize returned %#lx\n", hr );
    hr = IDirectInputDevice8_Initialize( device, instance, version, &GUID_NULL );
    todo_wine
    ok( hr == REGDB_E_CLASSNOTREG, "Initialize returned %#lx\n", hr );

    hr = IDirectInputDevice8_Initialize( device, instance, version, &devinst.guidInstance );
    ok( hr == DI_OK, "Initialize returned %#lx\n", hr );
    guid = devinst.guidInstance;
    memset( &devinst, 0, sizeof(devinst) );
    devinst.dwSize = sizeof(DIDEVICEINSTANCEW);
    hr = IDirectInputDevice8_GetDeviceInfo( device, &devinst );
    ok( hr == DI_OK, "GetDeviceInfo returned %#lx\n", hr );
    ok( IsEqualGUID( &guid, &devinst.guidInstance ), "got %s expected %s\n", debugstr_guid( &guid ),
        debugstr_guid( &devinst.guidInstance ) );
    hr = IDirectInputDevice8_Initialize( device, instance, version, &devinst.guidProduct );
    ok( hr == DI_OK, "Initialize returned %#lx\n", hr );

    hr = IDirectInputDevice8_GetDeviceInfo( device, NULL );
    ok( hr == E_POINTER, "GetDeviceInfo returned %#lx\n", hr );
    devinst.dwSize = sizeof(DIDEVICEINSTANCEW) + 1;
    hr = IDirectInputDevice8_GetDeviceInfo( device, &devinst );
    ok( hr == DIERR_INVALIDPARAM, "GetDeviceInfo returned %#lx\n", hr );

    devinst.dwSize = sizeof(DIDEVICEINSTANCE_DX3W);
    hr = IDirectInputDevice8_GetDeviceInfo( device, &devinst );
    ok( hr == DI_OK, "GetDeviceInfo returned %#lx\n", hr );
    todo_wine
    check_member_guid( devinst, expect_devinst, guidInstance );
    check_member_guid( devinst, expect_devinst, guidProduct );
    todo_wine_if( version < 0x0800 )
    check_member( devinst, expect_devinst, "%#lx", dwDevType );
    check_member_wstr( devinst, expect_devinst, tszInstanceName );
    check_member_wstr( devinst, expect_devinst, tszProductName );

    memset( &devinst, 0, sizeof(devinst) );
    devinst.dwSize = sizeof(DIDEVICEINSTANCEW);
    hr = IDirectInputDevice8_GetDeviceInfo( device, &devinst );
    ok( hr == DI_OK, "GetDeviceInfo returned %#lx\n", hr );
    check_member( devinst, expect_devinst, "%lu", dwSize );
    todo_wine
    check_member_guid( devinst, expect_devinst, guidInstance );
    check_member_guid( devinst, expect_devinst, guidProduct );
    todo_wine_if( version < 0x0800 )
    check_member( devinst, expect_devinst, "%#lx", dwDevType );
    check_member_wstr( devinst, expect_devinst, tszInstanceName );
    check_member_wstr( devinst, expect_devinst, tszProductName );
    check_member_guid( devinst, expect_devinst, guidFFDriver );
    check_member( devinst, expect_devinst, "%04x", wUsagePage );
    check_member( devinst, expect_devinst, "%04x", wUsage );

    hr = IDirectInputDevice8_GetCapabilities( device, NULL );
    ok( hr == E_POINTER, "GetCapabilities returned %#lx\n", hr );
    hr = IDirectInputDevice8_GetCapabilities( device, &caps );
    ok( hr == DIERR_INVALIDPARAM, "GetCapabilities returned %#lx\n", hr );
    caps.dwSize = sizeof(DIDEVCAPS);
    hr = IDirectInputDevice8_GetCapabilities( device, &caps );
    ok( hr == DI_OK, "GetCapabilities returned %#lx\n", hr );
    check_member( caps, expect_caps, "%lu", dwSize );
    check_member( caps, expect_caps, "%#lx", dwFlags );
    todo_wine_if( version < 0x0800 )
    check_member( caps, expect_caps, "%#lx", dwDevType );
    check_member( caps, expect_caps, "%lu", dwAxes );
    check_member( caps, expect_caps, "%lu", dwButtons );
    check_member( caps, expect_caps, "%lu", dwPOVs );
    check_member( caps, expect_caps, "%lu", dwFFSamplePeriod );
    check_member( caps, expect_caps, "%lu", dwFFMinTimeResolution );
    check_member( caps, expect_caps, "%lu", dwFirmwareRevision );
    check_member( caps, expect_caps, "%lu", dwHardwareRevision );
    check_member( caps, expect_caps, "%lu", dwFFDriverVersion );

    hr = IDirectInputDevice8_GetProperty( device, NULL, NULL );
    ok( hr == DIERR_INVALIDPARAM, "GetProperty returned %#lx\n", hr );
    hr = IDirectInputDevice8_GetProperty( device, &GUID_NULL, NULL );
    ok( hr == DIERR_INVALIDPARAM, "GetProperty returned %#lx\n", hr );
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_VIDPID, NULL );
    ok( hr == DIERR_INVALIDPARAM, "GetProperty returned %#lx\n", hr );
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_VIDPID, &prop_string.diph );
    ok( hr == (version < 0x0800 ? DIERR_UNSUPPORTED : DIERR_INVALIDPARAM),
        "GetProperty DIPROP_VIDPID returned %#lx\n", hr );
    prop_dword.diph.dwHeaderSize = sizeof(DIPROPHEADER) - 1;
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_VIDPID, &prop_dword.diph );
    ok( hr == DIERR_INVALIDPARAM, "GetProperty returned %#lx\n", hr );
    prop_dword.diph.dwHeaderSize = sizeof(DIPROPHEADER);

    prop_dword.dwData = 0xdeadbeef;
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_VIDPID, &prop_dword.diph );
    ok( hr == (version < 0x0800 ? DIERR_UNSUPPORTED : DI_OK),
        "GetProperty DIPROP_VIDPID returned %#lx\n", hr );
    if (hr == DI_OK)
    {
        ok( prop_dword.dwData == EXPECT_VIDPID, "got %#lx expected %#lx\n",
            prop_dword.dwData, EXPECT_VIDPID );
    }

    hr = IDirectInputDevice8_GetProperty( device, DIPROP_GUIDANDPATH, &prop_guid_path.diph );
    ok( hr == DI_OK, "GetProperty DIPROP_GUIDANDPATH returned %#lx\n", hr );
    ok( IsEqualGUID( &prop_guid_path.guidClass, &GUID_DEVCLASS_HIDCLASS ), "got guid %s\n",
        debugstr_guid( &prop_guid_path.guidClass ) );
    todo_wine
    ok( !wcsncmp( prop_guid_path.wszPath, expect_path, wcslen( expect_path ) ), "got path %s\n",
        debugstr_w(prop_guid_path.wszPath) );
    todo_wine
    ok( !wcscmp( wcsrchr( prop_guid_path.wszPath, '&' ), expect_path_end ), "got path %s\n",
        debugstr_w(prop_guid_path.wszPath) );

    hr = IDirectInputDevice8_GetProperty( device, DIPROP_INSTANCENAME, &prop_string.diph );
    ok( hr == DI_OK, "GetProperty DIPROP_INSTANCENAME returned %#lx\n", hr );
    ok( !wcscmp( prop_string.wsz, expect_devinst.tszInstanceName ), "got instance %s\n",
        debugstr_w(prop_string.wsz) );
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_PRODUCTNAME, &prop_string.diph );
    ok( hr == DI_OK, "GetProperty DIPROP_PRODUCTNAME returned %#lx\n", hr );
    ok( !wcscmp( prop_string.wsz, expect_devinst.tszProductName ), "got product %s\n",
        debugstr_w(prop_string.wsz) );
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_TYPENAME, &prop_string.diph );
    todo_wine_if( version >= 0x0800 )
    ok( hr == (version < 0x0800 ? DIERR_UNSUPPORTED : DI_OK),
        "GetProperty DIPROP_TYPENAME returned %#lx\n", hr );
    if (hr == DI_OK)
    {
        todo_wine
        ok( !wcscmp( prop_string.wsz, expect_vidpid_str ), "got type %s\n", debugstr_w(prop_string.wsz) );
    }
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_USERNAME, &prop_string.diph );
    ok( hr == (version < 0x0800 ? DIERR_UNSUPPORTED : DI_NOEFFECT),
        "GetProperty DIPROP_USERNAME returned %#lx\n", hr );
    if (hr == DI_NOEFFECT)
    {
        todo_wine
        ok( !wcscmp( prop_string.wsz, L"" ), "got user %s\n", debugstr_w(prop_string.wsz) );
    }

    prop_dword.dwData = 0xdeadbeef;
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_JOYSTICKID, &prop_dword.diph );
    ok( hr == DI_OK, "GetProperty DIPROP_JOYSTICKID returned %#lx\n", hr );
    todo_wine
    ok( prop_dword.dwData == 0, "got %#lx expected 0\n", prop_dword.dwData );

    prop_dword.dwData = 0xdeadbeef;
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_AXISMODE, &prop_dword.diph );
    todo_wine
    ok( hr == DI_OK, "GetProperty DIPROP_AXISMODE returned %#lx\n", hr );
    todo_wine
    ok( prop_dword.dwData == DIPROPAXISMODE_ABS, "got %lu expected %u\n", prop_dword.dwData, DIPROPAXISMODE_ABS );
    prop_dword.dwData = 0xdeadbeef;
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_BUFFERSIZE, &prop_dword.diph );
    ok( hr == DI_OK, "GetProperty DIPROP_BUFFERSIZE returned %#lx\n", hr );
    ok( prop_dword.dwData == 0, "got %#lx expected %#x\n", prop_dword.dwData, 0 );
    prop_dword.dwData = 0xdeadbeef;
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_FFGAIN, &prop_dword.diph );
    ok( hr == DI_OK, "GetProperty DIPROP_FFGAIN returned %#lx\n", hr );
    ok( prop_dword.dwData == 10000, "got %lu expected %u\n", prop_dword.dwData, 10000 );

    hr = IDirectInputDevice8_GetProperty( device, DIPROP_CALIBRATION, &prop_dword.diph );
    ok( hr == DIERR_INVALIDPARAM, "GetProperty DIPROP_CALIBRATION returned %#lx\n", hr );
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_AUTOCENTER, &prop_dword.diph );
    ok( hr == DIERR_UNSUPPORTED, "GetProperty DIPROP_AUTOCENTER returned %#lx\n", hr );
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_DEADZONE, &prop_dword.diph );
    ok( hr == DIERR_UNSUPPORTED, "GetProperty DIPROP_DEADZONE returned %#lx\n", hr );
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_FFLOAD, &prop_dword.diph );
    ok( hr == DIERR_UNSUPPORTED, "GetProperty DIPROP_FFLOAD returned %#lx\n", hr );
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_GRANULARITY, &prop_dword.diph );
    ok( hr == DIERR_UNSUPPORTED, "GetProperty DIPROP_GRANULARITY returned %#lx\n", hr );
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_SATURATION, &prop_dword.diph );
    ok( hr == DIERR_UNSUPPORTED, "GetProperty DIPROP_SATURATION returned %#lx\n", hr );
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_CALIBRATIONMODE, &prop_dword.diph );
    ok( hr == DIERR_UNSUPPORTED, "GetProperty DIPROP_CALIBRATIONMODE returned %#lx\n", hr );
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_RANGE, &prop_range.diph );
    ok( hr == DIERR_UNSUPPORTED, "GetProperty DIPROP_RANGE returned %#lx\n", hr );
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_KEYNAME, &prop_string.diph );
    ok( hr == (version < 0x0800 ? DIERR_UNSUPPORTED : DIERR_INVALIDPARAM),
        "GetProperty DIPROP_KEYNAME returned %#lx\n", hr );
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_LOGICALRANGE, &prop_range.diph );
    ok( hr == DIERR_UNSUPPORTED, "GetProperty DIPROP_LOGICALRANGE returned %#lx\n", hr );
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_PHYSICALRANGE, &prop_range.diph );
    ok( hr == DIERR_UNSUPPORTED, "GetProperty DIPROP_PHYSICALRANGE returned %#lx\n", hr );

    prop_dword.diph.dwHow = DIPH_BYUSAGE;
    prop_dword.diph.dwObj = MAKELONG( HID_USAGE_GENERIC_X, HID_USAGE_PAGE_GENERIC );
    prop_dword.dwData = 0xdeadbeef;
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_DEADZONE, &prop_dword.diph );
    ok( hr == DI_OK, "GetProperty DIPROP_DEADZONE returned %#lx\n", hr );
    ok( prop_dword.dwData == 0, "got %lu expected %u\n", prop_dword.dwData, 0 );
    prop_dword.dwData = 0xdeadbeef;
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_GRANULARITY, &prop_dword.diph );
    ok( hr == DI_OK, "GetProperty DIPROP_GRANULARITY returned %#lx\n", hr );
    ok( prop_dword.dwData == 1, "got %lu expected %u\n", prop_dword.dwData, 1 );
    prop_dword.dwData = 0xdeadbeef;
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_SATURATION, &prop_dword.diph );
    ok( hr == DI_OK, "GetProperty DIPROP_SATURATION returned %#lx\n", hr );
    ok( prop_dword.dwData == 10000, "got %lu expected %u\n", prop_dword.dwData, 10000 );
    prop_dword.dwData = 0xdeadbeef;
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_CALIBRATIONMODE, &prop_dword.diph );
    ok( hr == DI_OK, "GetProperty DIPROP_CALIBRATIONMODE returned %#lx\n", hr );
    ok( prop_dword.dwData == DIPROPCALIBRATIONMODE_COOKED, "got %lu expected %u\n",
        prop_dword.dwData, DIPROPCALIBRATIONMODE_COOKED );

    prop_string.diph.dwHow = DIPH_BYUSAGE;
    prop_string.diph.dwObj = MAKELONG( HID_USAGE_GENERIC_X, HID_USAGE_PAGE_GENERIC );
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_KEYNAME, &prop_string.diph );
    ok( hr == (version < 0x0800 ? DIERR_UNSUPPORTED : DI_OK),
        "GetProperty DIPROP_KEYNAME returned %#lx\n", hr );
    if (hr == DI_OK)
    {
        ok( !wcscmp( prop_string.wsz, expect_objects[4].tszName ), "got DIPROP_KEYNAME %s\n",
            debugstr_w(prop_string.wsz) );
    }
    prop_string.diph.dwObj = MAKELONG( 0x1, HID_USAGE_PAGE_BUTTON );
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_KEYNAME, &prop_string.diph );
    todo_wine
    ok( hr == DIERR_NOTFOUND, "GetProperty DIPROP_KEYNAME returned %#lx\n", hr );
    prop_string.diph.dwHow = DIPH_BYUSAGE;
    prop_string.diph.dwObj = MAKELONG( HID_USAGE_GENERIC_X, HID_USAGE_PAGE_GENERIC );
    hr = IDirectInputDevice8_SetProperty( device, DIPROP_KEYNAME, &prop_string.diph );
    ok( hr == DIERR_UNSUPPORTED, "SetProperty DIPROP_KEYNAME returned %#lx\n", hr );

    prop_range.diph.dwHow = DIPH_BYUSAGE;
    prop_range.diph.dwObj = MAKELONG( 0, 0 );
    prop_range.lMin = 0xdeadbeef;
    prop_range.lMax = 0xdeadbeef;
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_RANGE, &prop_range.diph );
    todo_wine_if( version < 0x0800 )
    ok( hr == (version < 0x0800 ? DI_OK : DIERR_NOTFOUND),
        "GetProperty DIPROP_RANGE returned %#lx\n", hr );
    prop_range.diph.dwObj = MAKELONG( 0, HID_USAGE_PAGE_GENERIC );
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_RANGE, &prop_range.diph );
    ok( hr == DIERR_NOTFOUND, "GetProperty DIPROP_RANGE returned %#lx\n", hr );
    prop_range.diph.dwObj = MAKELONG( HID_USAGE_PAGE_GENERIC, HID_USAGE_GENERIC_X );
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_RANGE, &prop_range.diph );
    ok( hr == DIERR_NOTFOUND, "GetProperty DIPROP_RANGE returned %#lx\n", hr );
    prop_range.diph.dwObj = MAKELONG( HID_USAGE_GENERIC_X, HID_USAGE_PAGE_GENERIC );
    prop_range.lMin = 0xdeadbeef;
    prop_range.lMax = 0xdeadbeef;
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_RANGE, &prop_range.diph );
    ok( hr == DI_OK, "GetProperty DIPROP_RANGE returned %#lx\n", hr );
    ok( prop_range.lMin == 0, "got %ld expected %d\n", prop_range.lMin, 0 );
    ok( prop_range.lMax == 65535, "got %ld expected %d\n", prop_range.lMax, 65535 );
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_LOGICALRANGE, &prop_range.diph );
    ok( hr == (version < 0x0800 ? DIERR_UNSUPPORTED : DI_OK),
        "GetProperty DIPROP_LOGICALRANGE returned %#lx\n", hr );
    if (hr == DI_OK)
    {
        ok( prop_range.lMin == -25, "got %ld expected %d\n", prop_range.lMin, -25 );
        ok( prop_range.lMax == 56, "got %ld expected %d\n", prop_range.lMax, 56 );
    }
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_PHYSICALRANGE, &prop_range.diph );
    ok( hr == (version < 0x0800 ? DIERR_UNSUPPORTED : DI_OK),
        "GetProperty DIPROP_PHYSICALRANGE returned %#lx\n", hr );
    if (hr == DI_OK)
    {
        ok( prop_range.lMin == -25, "got %ld expected %d\n", prop_range.lMin, -25 );
        ok( prop_range.lMax == 56, "got %ld expected %d\n", prop_range.lMax, 56 );
    }

    prop_pointer.diph.dwHow = DIPH_BYUSAGE;
    prop_pointer.diph.dwObj = MAKELONG( HID_USAGE_GENERIC_X, HID_USAGE_PAGE_GENERIC );
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_APPDATA, &prop_pointer.diph );
    todo_wine_if( version >= 0x0800 )
    ok( hr == (version < 0x0800 ? DIERR_UNSUPPORTED : DIERR_NOTINITIALIZED),
        "GetProperty DIPROP_APPDATA returned %#lx\n", hr );

    hr = IDirectInputDevice8_EnumObjects( device, NULL, NULL, DIDFT_ALL );
    ok( hr == DIERR_INVALIDPARAM, "EnumObjects returned %#lx\n", hr );
    hr = IDirectInputDevice8_EnumObjects( device, check_object_count, &res, 0x20 );
    ok( hr == DIERR_INVALIDPARAM, "EnumObjects returned %#lx\n", hr );
    res = 0;
    hr = IDirectInputDevice8_EnumObjects( device, check_object_count, &res, DIDFT_AXIS | DIDFT_PSHBUTTON );
    ok( hr == DI_OK, "EnumObjects returned %#lx\n", hr );
    todo_wine_if( version < 0x0700 )
    ok( res == (version < 0x0700 ? 6 : 8), "got %lu objects\n", res );
    hr = IDirectInputDevice8_EnumObjects( device, check_objects, &check_objects_params, DIDFT_ALL );
    ok( hr == DI_OK, "EnumObjects returned %#lx\n", hr );
    ok( check_objects_params.index >= check_objects_params.expect_count, "missing %u objects\n",
        check_objects_params.expect_count - check_objects_params.index );

    hr = IDirectInputDevice8_GetObjectInfo( device, NULL, 0, DIPH_DEVICE );
    ok( hr == E_POINTER, "GetObjectInfo returned: %#lx\n", hr );
    hr = IDirectInputDevice8_GetObjectInfo( device, &objinst, 0, DIPH_DEVICE );
    ok( hr == DIERR_INVALIDPARAM, "GetObjectInfo returned: %#lx\n", hr );
    objinst.dwSize = sizeof(DIDEVICEOBJECTINSTANCEW);
    hr = IDirectInputDevice8_GetObjectInfo( device, &objinst, 0, DIPH_DEVICE );
    ok( hr == DIERR_INVALIDPARAM, "GetObjectInfo returned: %#lx\n", hr );

    res = MAKELONG( HID_USAGE_GENERIC_Z, HID_USAGE_PAGE_GENERIC );
    hr = IDirectInputDevice8_GetObjectInfo( device, &objinst, res, DIPH_BYUSAGE );
    todo_wine_if( version < 0x0800 )
    ok( hr == (version < 0x0800 ? DI_OK : DIERR_NOTFOUND), "GetObjectInfo returned: %#lx\n", hr );
    res = MAKELONG( HID_USAGE_GENERIC_X, HID_USAGE_PAGE_GENERIC );
    hr = IDirectInputDevice8_GetObjectInfo( device, &objinst, res, DIPH_BYUSAGE );
    ok( hr == DI_OK, "GetObjectInfo returned: %#lx\n", hr );

    if (version < 0x0700) expect_obj = expect_objects_5[0];
    else expect_obj = expect_objects[4];
    check_member( objinst, expect_obj, "%lu", dwSize );
    check_member_guid( objinst, expect_obj, guidType );
    todo_wine_if( version < 0x0700 )
    check_member( objinst, expect_obj, "%#lx", dwOfs );
    check_member( objinst, expect_obj, "%#lx", dwType );
    check_member( objinst, expect_obj, "%#lx", dwFlags );
    if (!localized) check_member_wstr( objinst, expect_obj, tszName );
    check_member( objinst, expect_obj, "%lu", dwFFMaxForce );
    check_member( objinst, expect_obj, "%lu", dwFFForceResolution );
    check_member( objinst, expect_obj, "%u", wCollectionNumber );
    check_member( objinst, expect_obj, "%u", wDesignatorIndex );
    check_member( objinst, expect_obj, "%#04x", wUsagePage );
    check_member( objinst, expect_obj, "%#04x", wUsage );
    check_member( objinst, expect_obj, "%#lx", dwDimension );
    check_member( objinst, expect_obj, "%#04x", wExponent );
    check_member( objinst, expect_obj, "%u", wReportId );

    hr = IDirectInputDevice8_GetObjectInfo( device, &objinst, 0x14, DIPH_BYOFFSET );
    ok( hr == DIERR_NOTFOUND, "GetObjectInfo returned: %#lx\n", hr );
    hr = IDirectInputDevice8_GetObjectInfo( device, &objinst, 0, DIPH_BYOFFSET );
    ok( hr == DIERR_NOTFOUND, "GetObjectInfo returned: %#lx\n", hr );
    res = DIDFT_PSHBUTTON | DIDFT_MAKEINSTANCE( 3 );
    hr = IDirectInputDevice8_GetObjectInfo( device, &objinst, res, DIPH_BYID );
    ok( hr == DIERR_NOTFOUND, "GetObjectInfo returned: %#lx\n", hr );
    res = DIDFT_PSHBUTTON | DIDFT_MAKEINSTANCE( 1 );
    hr = IDirectInputDevice8_GetObjectInfo( device, &objinst, res, DIPH_BYID );
    ok( hr == DI_OK, "GetObjectInfo returned: %#lx\n", hr );

    if (version < 0x0700) expect_obj = expect_objects_5[6];
    else expect_obj = expect_objects[8];
    check_member( objinst, expect_obj, "%lu", dwSize );
    check_member_guid( objinst, expect_obj, guidType );
    todo_wine_if( version < 0x0700 )
    check_member( objinst, expect_obj, "%#lx", dwOfs );
    check_member( objinst, expect_obj, "%#lx", dwType );
    check_member( objinst, expect_obj, "%#lx", dwFlags );
    if (!localized) check_member_wstr( objinst, expect_obj, tszName );
    check_member( objinst, expect_obj, "%lu", dwFFMaxForce );
    check_member( objinst, expect_obj, "%lu", dwFFForceResolution );
    check_member( objinst, expect_obj, "%u", wCollectionNumber );
    check_member( objinst, expect_obj, "%u", wDesignatorIndex );
    check_member( objinst, expect_obj, "%#04x", wUsagePage );
    check_member( objinst, expect_obj, "%#04x", wUsage );
    check_member( objinst, expect_obj, "%#lx", dwDimension );
    check_member( objinst, expect_obj, "%#04x", wExponent );
    check_member( objinst, expect_obj, "%u", wReportId );

    hr = IDirectInputDevice8_EnumEffects( device, NULL, NULL, DIEFT_ALL );
    ok( hr == DIERR_INVALIDPARAM, "EnumEffects returned %#lx\n", hr );
    res = 0;
    hr = IDirectInputDevice8_EnumEffects( device, check_effect_count, &res, 0xfe );
    ok( hr == DI_OK, "EnumEffects returned %#lx\n", hr );
    ok( res == 0, "got %lu expected %u\n", res, 0 );
    res = 0;
    hr = IDirectInputDevice8_EnumEffects( device, check_effect_count, &res, DIEFT_PERIODIC );
    ok( hr == DI_OK, "EnumEffects returned %#lx\n", hr );
    ok( res == 0, "got %lu expected %u\n", res, 0 );
    hr = IDirectInputDevice8_EnumEffects( device, check_effects, &check_effects_params, DIEFT_ALL );
    ok( hr == DI_OK, "EnumEffects returned %#lx\n", hr );
    ok( check_effects_params.index >= check_effects_params.expect_count, "missing %u effects\n",
        check_effects_params.expect_count - check_effects_params.index );

    hr = IDirectInputDevice8_GetEffectInfo( device, NULL, &GUID_Sine );
    ok( hr == E_POINTER, "GetEffectInfo returned %#lx\n", hr );
    effectinfo.dwSize = sizeof(DIEFFECTINFOW) + 1;
    hr = IDirectInputDevice8_GetEffectInfo( device, &effectinfo, &GUID_Sine );
    ok( hr == DIERR_INVALIDPARAM, "GetEffectInfo returned %#lx\n", hr );
    effectinfo.dwSize = sizeof(DIEFFECTINFOW);
    hr = IDirectInputDevice8_GetEffectInfo( device, &effectinfo, &GUID_NULL );
    ok( hr == DIERR_DEVICENOTREG, "GetEffectInfo returned %#lx\n", hr );
    hr = IDirectInputDevice8_GetEffectInfo( device, &effectinfo, &GUID_Sine );
    ok( hr == DIERR_DEVICENOTREG, "GetEffectInfo returned %#lx\n", hr );

    hr = IDirectInputDevice8_SetDataFormat( device, NULL );
    ok( hr == E_POINTER, "SetDataFormat returned: %#lx\n", hr );
    hr = IDirectInputDevice8_SetDataFormat( device, &dataformat );
    ok( hr == DIERR_INVALIDPARAM, "SetDataFormat returned: %#lx\n", hr );
    dataformat.dwSize = sizeof(DIDATAFORMAT);
    hr = IDirectInputDevice8_SetDataFormat( device, &dataformat );
    ok( hr == DIERR_INVALIDPARAM, "SetDataFormat returned: %#lx\n", hr );
    dataformat.dwObjSize = sizeof(DIOBJECTDATAFORMAT);
    hr = IDirectInputDevice8_SetDataFormat( device, &dataformat );
    ok( hr == DI_OK, "SetDataFormat returned: %#lx\n", hr );
    hr = IDirectInputDevice8_SetDataFormat( device, &c_dfDIJoystick2 );
    ok( hr == DI_OK, "SetDataFormat returned: %#lx\n", hr );

    hr = IDirectInputDevice8_GetObjectInfo( device, &objinst, DIJOFS_Y, DIPH_BYOFFSET );
    ok( hr == DI_OK, "GetObjectInfo returned: %#lx\n", hr );

    if (version < 0x0700) expect_obj = expect_objects_5[1];
    else expect_obj = expect_objects[3];
    if (version < 0x0800) expect_obj.dwOfs = DIJOFS_Y;
    check_member( objinst, expect_obj, "%lu", dwSize );
    check_member_guid( objinst, expect_obj, guidType );
    todo_wine_if( version < 0x0800 )
    check_member( objinst, expect_obj, "%#lx", dwOfs );
    check_member( objinst, expect_obj, "%#lx", dwType );
    check_member( objinst, expect_obj, "%#lx", dwFlags );
    if (!localized) check_member_wstr( objinst, expect_obj, tszName );
    check_member( objinst, expect_obj, "%lu", dwFFMaxForce );
    check_member( objinst, expect_obj, "%lu", dwFFForceResolution );
    check_member( objinst, expect_obj, "%u", wCollectionNumber );
    check_member( objinst, expect_obj, "%u", wDesignatorIndex );
    check_member( objinst, expect_obj, "%#04x", wUsagePage );
    check_member( objinst, expect_obj, "%#04x", wUsage );
    check_member( objinst, expect_obj, "%#lx", dwDimension );
    check_member( objinst, expect_obj, "%#04x", wExponent );
    check_member( objinst, expect_obj, "%u", wReportId );

    hr = IDirectInputDevice8_SetEventNotification( device, (HANDLE)0xdeadbeef );
    todo_wine
    ok( hr == E_HANDLE, "SetEventNotification returned: %#lx\n", hr );
    event = CreateEventW( NULL, FALSE, FALSE, NULL );
    ok( event != NULL, "CreateEventW failed, last error %lu\n", GetLastError() );
    hr = IDirectInputDevice8_SetEventNotification( device, event );
    ok( hr == DI_OK, "SetEventNotification returned: %#lx\n", hr );

    file = CreateFileW( prop_guid_path.wszPath, FILE_READ_ACCESS | FILE_WRITE_ACCESS,
                        FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
                        FILE_FLAG_OVERLAPPED | FILE_FLAG_NO_BUFFERING, NULL );
    ok( file != INVALID_HANDLE_VALUE, "got error %lu\n", GetLastError() );

    hr = IDirectInputDevice8_SetCooperativeLevel( device, NULL, 0 );
    ok( hr == DIERR_INVALIDPARAM, "SetCooperativeLevel returned: %#lx\n", hr );
    hr = IDirectInputDevice8_SetCooperativeLevel( device, NULL, DISCL_BACKGROUND );
    ok( hr == DIERR_INVALIDPARAM, "SetCooperativeLevel returned: %#lx\n", hr );
    hr = IDirectInputDevice8_SetCooperativeLevel( device, NULL, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE );
    ok( hr == E_HANDLE, "SetCooperativeLevel returned: %#lx\n", hr );
    hr = IDirectInputDevice8_SetCooperativeLevel( device, NULL, DISCL_BACKGROUND | DISCL_EXCLUSIVE );
    ok( hr == E_HANDLE, "SetCooperativeLevel returned: %#lx\n", hr );
    hr = IDirectInputDevice8_SetCooperativeLevel( device, NULL, DISCL_FOREGROUND | DISCL_EXCLUSIVE );
    ok( hr == E_HANDLE, "SetCooperativeLevel returned: %#lx\n", hr );

    hwnd = CreateWindowW( L"static", L"dinput", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 10, 10, 200, 200,
                          NULL, NULL, NULL, NULL );
    SetForegroundWindow( hwnd );

    hr = IDirectInputDevice8_SetCooperativeLevel( device, hwnd, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE );
    ok( hr == DI_OK, "SetCooperativeLevel returned: %#lx\n", hr );
    hr = IDirectInputDevice8_SetCooperativeLevel( device, hwnd, DISCL_BACKGROUND | DISCL_EXCLUSIVE );
    ok( hr == DI_OK, "SetCooperativeLevel returned: %#lx\n", hr );
    hr = IDirectInputDevice8_SetCooperativeLevel( device, hwnd, DISCL_FOREGROUND | DISCL_EXCLUSIVE );
    ok( hr == DI_OK, "SetCooperativeLevel returned: %#lx\n", hr );

    hr = IDirectInputDevice8_Unacquire( device );
    ok( hr == DI_NOEFFECT, "Unacquire returned: %#lx\n", hr );
    hr = IDirectInputDevice8_Acquire( device );
    ok( hr == DI_OK, "Acquire returned: %#lx\n", hr );
    hr = IDirectInputDevice8_SetCooperativeLevel( device, hwnd, DISCL_FOREGROUND | DISCL_EXCLUSIVE );
    ok( hr == DIERR_ACQUIRED, "SetCooperativeLevel returned: %#lx\n", hr );
    hr = IDirectInputDevice8_Unacquire( device );
    ok( hr == DI_OK, "Unacquire returned: %#lx\n", hr );

    DestroyWindow( hwnd );

    hr = IDirectInputDevice8_SetCooperativeLevel( device, NULL, DISCL_BACKGROUND | DISCL_NONEXCLUSIVE );
    ok( hr == DI_OK, "SetCooperativeLevel returned: %#lx\n", hr );

    hr = IDirectInputDevice8_GetDeviceState( device, sizeof(DIJOYSTATE2), &state );
    ok( hr == DIERR_NOTACQUIRED, "GetDeviceState returned: %#lx\n", hr );

    hr = IDirectInputDevice8_Poll( device );
    ok( hr == DIERR_NOTACQUIRED, "Poll returned: %#lx\n", hr );

    hr = IDirectInputDevice8_Acquire( device );
    ok( hr == DI_OK, "Acquire returned: %#lx\n", hr );

    hr = IDirectInputDevice8_Poll( device );
    todo_wine_if( version < 0x0700 )
    ok( hr == (version < 0x0700 ? DI_OK : DI_NOEFFECT), "Poll returned: %#lx\n", hr );

    hr = IDirectInputDevice8_GetDeviceState( device, sizeof(DIJOYSTATE2) + 1, &state );
    ok( hr == DIERR_INVALIDPARAM, "GetDeviceState returned: %#lx\n", hr );

    for (i = 0; i < ARRAY_SIZE(injected_input); ++i)
    {
        winetest_push_context( "state[%ld]", i );
        hr = IDirectInputDevice8_GetDeviceState( device, sizeof(DIJOYSTATE2), &state );
        ok( hr == DI_OK, "GetDeviceState returned: %#lx\n", hr );
        check_member( state, expect_state[i], "%ld", lX );
        check_member( state, expect_state[i], "%ld", lY );
        check_member( state, expect_state[i], "%ld", lZ );
        check_member( state, expect_state[i], "%ld", lRx );
        check_member( state, expect_state[i], "%#lx", rgdwPOV[0] );
        check_member( state, expect_state[i], "%#lx", rgdwPOV[1] );
        check_member( state, expect_state[i], "%#x", rgbButtons[0] );
        check_member( state, expect_state[i], "%#x", rgbButtons[1] );
        check_member( state, expect_state[i], "%#x", rgbButtons[2] );

        send_hid_input( file, &injected_input[i], sizeof(*injected_input) );

        res = WaitForSingleObject( event, 100 );
        if (i == 0 || i == 3) ok( res == WAIT_TIMEOUT, "WaitForSingleObject succeeded\n" );
        else ok( res == WAIT_OBJECT_0, "WaitForSingleObject failed\n" );
        ResetEvent( event );
        winetest_pop_context();
    }

    hr = IDirectInputDevice8_GetDeviceState( device, sizeof(DIJOYSTATE2), &state );
    ok( hr == DI_OK, "GetDeviceState returned: %#lx\n", hr );
    winetest_push_context( "state[%ld]", i );
    check_member( state, expect_state[i], "%ld", lX );
    check_member( state, expect_state[i], "%ld", lY );
    check_member( state, expect_state[i], "%ld", lZ );
    check_member( state, expect_state[i], "%ld", lRx );
    check_member( state, expect_state[i], "%#lx", rgdwPOV[0] );
    check_member( state, expect_state[i], "%#lx", rgdwPOV[1] );
    check_member( state, expect_state[i], "%#x", rgbButtons[0] );
    check_member( state, expect_state[i], "%#x", rgbButtons[1] );
    check_member( state, expect_state[i], "%#x", rgbButtons[2] );
    winetest_pop_context();

    res = 1;
    size = version < 0x0800 ? sizeof(DIDEVICEOBJECTDATA_DX3) : sizeof(DIDEVICEOBJECTDATA);
    hr = IDirectInputDevice8_GetDeviceData( device, size - 1, objdata, &res, DIGDD_PEEK );
    todo_wine_if( version >= 0x0800 )
    ok( hr == (version < 0x0800 ? DI_OK : DIERR_INVALIDPARAM), "GetDeviceData returned %#lx\n", hr );
    res = 1;
    hr = IDirectInputDevice8_GetDeviceData( device, size, objdata, &res, DIGDD_PEEK );
    ok( hr == DIERR_NOTBUFFERED, "GetDeviceData returned %#lx\n", hr );

    hr = IDirectInputDevice8_Unacquire( device );
    ok( hr == DI_OK, "Unacquire returned: %#lx\n", hr );
    prop_dword.diph.dwHow = DIPH_BYUSAGE;
    prop_dword.diph.dwObj = MAKELONG( HID_USAGE_GENERIC_X, HID_USAGE_PAGE_GENERIC );
    prop_dword.dwData = 1;
    hr = IDirectInputDevice8_SetProperty( device, DIPROP_BUFFERSIZE, &prop_dword.diph );
    ok( hr == DIERR_UNSUPPORTED, "SetProperty DIPROP_BUFFERSIZE returned %#lx\n", hr );
    prop_dword.diph.dwHow = DIPH_DEVICE;
    prop_dword.diph.dwObj = 0;
    hr = IDirectInputDevice8_SetProperty( device, DIPROP_BUFFERSIZE, &prop_dword.diph );
    ok( hr == DI_OK, "SetProperty DIPROP_BUFFERSIZE returned %#lx\n", hr );
    hr = IDirectInputDevice8_Acquire( device );
    ok( hr == DI_OK, "Unacquire returned: %#lx\n", hr );

    res = 1;
    hr = IDirectInputDevice8_GetDeviceData( device, size, objdata, &res, DIGDD_PEEK );
    ok( hr == DI_OK, "GetDeviceData returned %#lx\n", hr );
    ok( res == 0, "got %lu expected %u\n", res, 0 );

    send_hid_input( file, &injected_input[0], sizeof(*injected_input) );
    res = WaitForSingleObject( event, 100 );
    ok( res == WAIT_OBJECT_0, "WaitForSingleObject failed\n" );
    ResetEvent( event );

    res = 1;
    hr = IDirectInputDevice8_GetDeviceData( device, size, objdata, &res, DIGDD_PEEK );
    todo_wine_if( version < 0x0800 )
    ok( hr == DI_BUFFEROVERFLOW, "GetDeviceData returned %#lx\n", hr );
    ok( res == 0, "got %lu expected %u\n", res, 0 );
    res = 1;
    hr = IDirectInputDevice8_GetDeviceData( device, size, objdata, &res, 0 );
    todo_wine_if( version >= 0x0800 )
    ok( hr == DI_OK, "GetDeviceData returned %#lx\n", hr );
    ok( res == 0, "got %lu expected %u\n", res, 0 );

    hr = IDirectInputDevice8_Unacquire( device );
    ok( hr == DI_OK, "Unacquire returned: %#lx\n", hr );
    prop_dword.diph.dwHow = DIPH_DEVICE;
    prop_dword.diph.dwObj = 0;
    prop_dword.dwData = 10;
    hr = IDirectInputDevice8_SetProperty( device, DIPROP_BUFFERSIZE, &prop_dword.diph );
    ok( hr == DI_OK, "SetProperty DIPROP_BUFFERSIZE returned %#lx\n", hr );
    hr = IDirectInputDevice8_Acquire( device );
    ok( hr == DI_OK, "Unacquire returned: %#lx\n", hr );

    send_hid_input( file, &injected_input[1], sizeof(*injected_input) );
    res = WaitForSingleObject( event, 100 );
    ok( res == WAIT_OBJECT_0, "WaitForSingleObject failed\n" );
    ResetEvent( event );

    res = 1;
    hr = IDirectInputDevice8_GetDeviceData( device, size, objdata, &res, DIGDD_PEEK );
    ok( hr == DI_OK, "GetDeviceData returned %#lx\n", hr );
    ok( res == 1, "got %lu expected %u\n", res, 1 );
    check_member( objdata[0], expect_objdata[0], "%#lx", dwOfs );
    check_member( objdata[0], expect_objdata[0], "%#lx", dwData );
    if (version >= 0x0800) ok( objdata[0].uAppData == -1, "got %p\n", (void *)objdata[0].uAppData );
    res = 4;
    hr = IDirectInputDevice8_GetDeviceData( device, size, objdata, &res, 0 );
    ok( hr == DI_OK, "GetDeviceData returned %#lx\n", hr );
    ok( res == 4, "got %lu expected %u\n", res, 4 );
    for (i = 0; i < 4; ++i)
    {
        DIDEVICEOBJECTDATA *ptr = (DIDEVICEOBJECTDATA *)((char *)objdata + size * i);
        winetest_push_context( "objdata[%ld]", i );
        check_member( *ptr, expect_objdata[1 + i], "%#lx", dwOfs );
        check_member( *ptr, expect_objdata[1 + i], "%#lx", dwData );
        if (version >= 0x0800) ok( ptr->uAppData == -1, "got %p\n", (void *)ptr->uAppData );
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
    hr = IDirectInputDevice8_GetDeviceData( device, size, objdata, &res, 0 );
    todo_wine_if( version < 0x0800 )
    ok( hr == DI_BUFFEROVERFLOW, "GetDeviceData returned %#lx\n", hr );
    ok( res == 1, "got %lu expected %u\n", res, 1 );
    todo_wine
    check_member( objdata[0], expect_objdata[5], "%#lx", dwOfs );
    todo_wine
    check_member( objdata[0], expect_objdata[5], "%#lx", dwData );
    if (version >= 0x0800) ok( objdata[0].uAppData == -1, "got %p\n", (void *)objdata[0].uAppData );
    res = ARRAY_SIZE(objdata);
    hr = IDirectInputDevice8_GetDeviceData( device, size, objdata, &res, 0 );
    ok( hr == DI_OK, "GetDeviceData returned %#lx\n", hr );
    ok( res == 8, "got %lu expected %u\n", res, 8 );
    for (i = 0; i < 8; ++i)
    {
        DIDEVICEOBJECTDATA *ptr = (DIDEVICEOBJECTDATA *)((char *)objdata + size * i);
        winetest_push_context( "objdata[%ld]", i );
        todo_wine
        check_member( *ptr, expect_objdata[6 + i], "%#lx", dwOfs );
        todo_wine_if( i == 1 || i == 2 || i == 6 )
        check_member( *ptr, expect_objdata[6 + i], "%#lx", dwData );
        if (version >= 0x0800) ok( ptr->uAppData == -1, "got %p\n", (void *)ptr->uAppData );
        winetest_pop_context();
    }

    send_hid_input( file, &injected_input[3], sizeof(*injected_input) );
    res = WaitForSingleObject( event, 100 );
    ok( res == WAIT_OBJECT_0, "WaitForSingleObject failed\n" );
    ResetEvent( event );

    hr = IDirectInputDevice8_GetDeviceState( device, sizeof(DIJOYSTATE2), &state );
    ok( hr == DI_OK, "GetDeviceState returned: %#lx\n", hr );
    check_member( state, expect_state[3], "%ld", lX );
    check_member( state, expect_state[3], "%ld", lY );
    check_member( state, expect_state[3], "%ld", lZ );
    check_member( state, expect_state[3], "%ld", lRx );
    check_member( state, expect_state[3], "%ld", rgdwPOV[0] );
    check_member( state, expect_state[3], "%ld", rgdwPOV[1] );
    check_member( state, expect_state[3], "%#x", rgbButtons[0] );
    check_member( state, expect_state[3], "%#x", rgbButtons[1] );
    check_member( state, expect_state[3], "%#x", rgbButtons[2] );

    hr = IDirectInputDevice8_Unacquire( device );
    ok( hr == DI_OK, "Unacquire returned: %#lx\n", hr );

    dataformat.dwSize = sizeof(DIDATAFORMAT);
    dataformat.dwObjSize = sizeof(DIOBJECTDATAFORMAT);
    dataformat.dwFlags = DIDF_ABSAXIS;
    dataformat.dwDataSize = sizeof(buffer);
    dataformat.dwNumObjs = 0;
    dataformat.rgodf = objdataformat;
    hr = IDirectInputDevice8_SetDataFormat( device, &dataformat );
    ok( hr == DI_OK, "SetDataFormat returned: %#lx\n", hr );

    dataformat.dwNumObjs = 1;
    dataformat.dwDataSize = 8;
    objdataformat[0].pguid = NULL;
    objdataformat[0].dwOfs = 2;
    objdataformat[0].dwType = DIDFT_AXIS | DIDFT_ANYINSTANCE;
    objdataformat[0].dwFlags = 0;
    hr = IDirectInputDevice8_SetDataFormat( device, &dataformat );
    todo_wine
    ok( hr == DIERR_INVALIDPARAM, "SetDataFormat returned: %#lx\n", hr );
    objdataformat[0].dwOfs = 4;
    hr = IDirectInputDevice8_SetDataFormat( device, &dataformat );
    ok( hr == DI_OK, "SetDataFormat returned: %#lx\n", hr );
    dataformat.dwDataSize = 10;
    hr = IDirectInputDevice8_SetDataFormat( device, &dataformat );
    todo_wine
    ok( hr == DIERR_INVALIDPARAM, "SetDataFormat returned: %#lx\n", hr );
    dataformat.dwDataSize = 8;
    objdataformat[0].dwOfs = 2;
    objdataformat[0].dwType = DIDFT_OPTIONAL | 0xff | DIDFT_ANYINSTANCE;
    hr = IDirectInputDevice8_SetDataFormat( device, &dataformat );
    todo_wine
    ok( hr == DIERR_INVALIDPARAM, "SetDataFormat returned: %#lx\n", hr );

    dataformat.dwNumObjs = 2;
    dataformat.dwDataSize = 5;
    objdataformat[0].pguid = NULL;
    objdataformat[0].dwOfs = 4;
    objdataformat[0].dwType = DIDFT_BUTTON | DIDFT_ANYINSTANCE;
    objdataformat[0].dwFlags = 0;
    objdataformat[1].pguid = NULL;
    objdataformat[1].dwOfs = 0;
    objdataformat[1].dwType = DIDFT_AXIS | DIDFT_MAKEINSTANCE( 0 );
    objdataformat[1].dwFlags = 0;
    hr = IDirectInputDevice8_SetDataFormat( device, &dataformat );
    todo_wine
    ok( hr == DIERR_INVALIDPARAM, "SetDataFormat returned: %#lx\n", hr );
    dataformat.dwDataSize = 8;
    hr = IDirectInputDevice8_SetDataFormat( device, &dataformat );
    ok( hr == DI_OK, "SetDataFormat returned: %#lx\n", hr );

    dataformat.dwNumObjs = 4;
    dataformat.dwDataSize = 12;
    objdataformat[0].pguid = NULL;
    objdataformat[0].dwOfs = 0;
    objdataformat[0].dwType = DIDFT_AXIS | DIDFT_MAKEINSTANCE( 0 );
    objdataformat[0].dwFlags = 0;
    objdataformat[1].pguid = NULL;
    objdataformat[1].dwOfs = 0;
    objdataformat[1].dwType = DIDFT_AXIS | DIDFT_MAKEINSTANCE( 0 );
    objdataformat[1].dwFlags = 0;
    objdataformat[2].pguid = &GUID_ZAxis;
    objdataformat[2].dwOfs = 8;
    objdataformat[2].dwType = 0xff | DIDFT_ANYINSTANCE;
    objdataformat[2].dwFlags = 0;
    objdataformat[3].pguid = &GUID_POV;
    objdataformat[3].dwOfs = 0;
    objdataformat[3].dwType = 0xff | DIDFT_ANYINSTANCE;
    objdataformat[3].dwFlags = 0;
    hr = IDirectInputDevice8_SetDataFormat( device, &dataformat );
    ok( hr == DIERR_INVALIDPARAM, "SetDataFormat returned: %#lx\n", hr );
    objdataformat[1].dwType = DIDFT_AXIS | DIDFT_MAKEINSTANCE( 12 );
    hr = IDirectInputDevice8_SetDataFormat( device, &dataformat );
    ok( hr == DIERR_INVALIDPARAM, "SetDataFormat returned: %#lx\n", hr );
    objdataformat[1].dwType = DIDFT_AXIS | DIDFT_MAKEINSTANCE( 0xff );
    hr = IDirectInputDevice8_SetDataFormat( device, &dataformat );
    ok( hr == (version < 0x0700 ? DI_OK : DIERR_INVALIDPARAM),
        "SetDataFormat returned: %#lx\n", hr );
    objdataformat[1].dwType = DIDFT_AXIS | DIDFT_MAKEINSTANCE( 1 );
    hr = IDirectInputDevice8_SetDataFormat( device, &dataformat );
    ok( hr == DI_OK, "SetDataFormat returned: %#lx\n", hr );
    objdataformat[1].pguid = &GUID_RzAxis;
    hr = IDirectInputDevice8_SetDataFormat( device, &dataformat );
    ok( hr == DIERR_INVALIDPARAM, "SetDataFormat returned: %#lx\n", hr );
    objdataformat[1].pguid = &GUID_Unknown;
    hr = IDirectInputDevice8_SetDataFormat( device, &dataformat );
    ok( hr == DIERR_INVALIDPARAM, "SetDataFormat returned: %#lx\n", hr );
    objdataformat[1].pguid = &GUID_YAxis;
    hr = IDirectInputDevice8_SetDataFormat( device, &dataformat );
    ok( hr == DI_OK, "SetDataFormat returned: %#lx\n", hr );
    objdataformat[1].pguid = NULL;
    objdataformat[1].dwType = DIDFT_OPTIONAL | DIDFT_AXIS | DIDFT_MAKEINSTANCE( 0 );
    hr = IDirectInputDevice8_SetDataFormat( device, &dataformat );
    ok( hr == DI_OK, "SetDataFormat returned: %#lx\n", hr );

    dataformat.dwNumObjs = 0;
    hr = IDirectInputDevice8_SetDataFormat( device, &dataformat );
    ok( hr == DI_OK, "SetDataFormat returned: %#lx\n", hr );
    hr = IDirectInputDevice8_Acquire( device );
    ok( hr == DI_OK, "Unacquire returned: %#lx\n", hr );

    send_hid_input( file, &injected_input[4], sizeof(*injected_input) );
    res = WaitForSingleObject( event, 100 );
    todo_wine
    ok( res == WAIT_TIMEOUT, "WaitForSingleObject failed\n" );
    ResetEvent( event );

    hr = IDirectInputDevice8_GetDeviceState( device, dataformat.dwDataSize, buffer );
    ok( hr == DI_OK, "GetDeviceState returned: %#lx\n", hr );
    hr = IDirectInputDevice8_Unacquire( device );
    ok( hr == DI_OK, "Unacquire returned: %#lx\n", hr );

    dataformat.dwNumObjs = 4;
    hr = IDirectInputDevice8_SetDataFormat( device, &dataformat );
    ok( hr == DI_OK, "SetDataFormat returned: %#lx\n", hr );
    hr = IDirectInputDevice8_Acquire( device );
    ok( hr == DI_OK, "Unacquire returned: %#lx\n", hr );

    send_hid_input( file, &injected_input[4], sizeof(*injected_input) );
    res = WaitForSingleObject( event, 100 );
    todo_wine
    ok( res == WAIT_OBJECT_0, "WaitForSingleObject failed\n" );
    ResetEvent( event );
    send_hid_input( file, &injected_input[3], sizeof(*injected_input) );
    res = WaitForSingleObject( event, 100 );
    ok( res == WAIT_OBJECT_0, "WaitForSingleObject failed\n" );
    ResetEvent( event );

    hr = IDirectInputDevice8_GetDeviceState( device, dataformat.dwDataSize, buffer );
    ok( hr == DI_OK, "GetDeviceState returned: %#lx\n", hr );
    ok( ((ULONG *)buffer)[0] == 0x512b, "got %#lx, expected %#x\n", ((ULONG *)buffer)[0], 0x512b );
    ok( ((ULONG *)buffer)[1] == 0, "got %#lx, expected %#x\n", ((ULONG *)buffer)[1], 0 );
    ok( ((ULONG *)buffer)[2] == 0x7fff, "got %#lx, expected %#x\n", ((ULONG *)buffer)[2], 0x7fff );
    hr = IDirectInputDevice8_Unacquire( device );
    ok( hr == DI_OK, "Unacquire returned: %#lx\n", hr );

    hr = IDirectInputDevice8_SetDataFormat( device, &c_dfDIJoystick2 );
    ok( hr == DI_OK, "SetDataFormat returned: %#lx\n", hr );
    hr = IDirectInputDevice8_Acquire( device );
    ok( hr == DI_OK, "Unacquire returned: %#lx\n", hr );

    send_hid_input( file, &injected_input[4], sizeof(*injected_input) );
    res = WaitForSingleObject( event, 100 );
    ok( res == WAIT_OBJECT_0, "WaitForSingleObject failed\n" );
    ResetEvent( event );
    send_hid_input( file, &injected_input[3], sizeof(*injected_input) );
    res = WaitForSingleObject( event, 100 );
    ok( res == WAIT_OBJECT_0, "WaitForSingleObject failed\n" );
    ResetEvent( event );

    hr = IDirectInputDevice8_GetDeviceState( device, sizeof(DIJOYSTATE2), &state );
    ok( hr == DI_OK, "GetDeviceState returned: %#lx\n", hr );
    check_member( state, expect_state[3], "%ld", lX );
    check_member( state, expect_state[3], "%ld", lY );
    check_member( state, expect_state[3], "%ld", lZ );
    check_member( state, expect_state[3], "%ld", lRx );
    check_member( state, expect_state[3], "%ld", rgdwPOV[0] );
    check_member( state, expect_state[3], "%ld", rgdwPOV[1] );
    check_member( state, expect_state[3], "%#x", rgbButtons[0] );
    check_member( state, expect_state[3], "%#x", rgbButtons[1] );
    check_member( state, expect_state[3], "%#x", rgbButtons[2] );

    prop_range.diph.dwHow = DIPH_DEVICE;
    prop_range.diph.dwObj = 0;
    prop_range.lMin = 1000;
    prop_range.lMax = 51000;
    hr = IDirectInputDevice8_SetProperty( device, DIPROP_RANGE, &prop_range.diph );
    ok( hr == DI_OK, "SetProperty DIPROP_RANGE returned %#lx\n", hr );
    prop_range.diph.dwHow = DIPH_BYUSAGE;
    prop_range.diph.dwObj = MAKELONG( HID_USAGE_GENERIC_X, HID_USAGE_PAGE_GENERIC );
    prop_range.lMin = -4000;
    prop_range.lMax = -14000;
    hr = IDirectInputDevice8_SetProperty( device, DIPROP_RANGE, &prop_range.diph );
    ok( hr == DIERR_INVALIDPARAM, "SetProperty DIPROP_RANGE returned %#lx\n", hr );
    prop_range.lMin = -14000;
    prop_range.lMax = -4000;
    hr = IDirectInputDevice8_SetProperty( device, DIPROP_RANGE, &prop_range.diph );
    ok( hr == DI_OK, "SetProperty DIPROP_RANGE returned %#lx\n", hr );
    hr = IDirectInputDevice8_SetProperty( device, DIPROP_LOGICALRANGE, &prop_range.diph );
    ok( hr == (version < 0x0800 ? DIERR_UNSUPPORTED : DIERR_ACQUIRED),
        "SetProperty DIPROP_LOGICALRANGE returned %#lx\n", hr );
    hr = IDirectInputDevice8_SetProperty( device, DIPROP_PHYSICALRANGE, &prop_range.diph );
    ok( hr == (version < 0x0800 ? DIERR_UNSUPPORTED : DIERR_ACQUIRED),
        "SetProperty DIPROP_PHYSICALRANGE returned %#lx\n", hr );

    hr = IDirectInputDevice8_Unacquire( device );
    ok( hr == DI_OK, "Unacquire returned: %#lx\n", hr );
    hr = IDirectInputDevice8_SetProperty( device, DIPROP_LOGICALRANGE, &prop_range.diph );
    ok( hr == DIERR_UNSUPPORTED, "SetProperty DIPROP_LOGICALRANGE returned %#lx\n", hr );
    hr = IDirectInputDevice8_SetProperty( device, DIPROP_PHYSICALRANGE, &prop_range.diph );
    ok( hr == DIERR_UNSUPPORTED, "SetProperty DIPROP_PHYSICALRANGE returned %#lx\n", hr );
    hr = IDirectInputDevice8_Acquire( device );
    ok( hr == DI_OK, "Unacquire returned: %#lx\n", hr );

    prop_range.diph.dwHow = DIPH_DEVICE;
    prop_range.diph.dwObj = 0;
    prop_range.lMin = 0xdeadbeef;
    prop_range.lMax = 0xdeadbeef;
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_RANGE, &prop_range.diph );
    ok( hr == DIERR_UNSUPPORTED, "GetProperty DIPROP_RANGE returned %#lx\n", hr );
    prop_range.diph.dwHow = DIPH_BYUSAGE;
    prop_range.diph.dwObj = MAKELONG( HID_USAGE_GENERIC_X, HID_USAGE_PAGE_GENERIC );
    prop_range.lMin = 0xdeadbeef;
    prop_range.lMax = 0xdeadbeef;
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_RANGE, &prop_range.diph );
    ok( hr == DI_OK, "GetProperty DIPROP_RANGE returned %#lx\n", hr );
    ok( prop_range.lMin == -14000, "got %ld expected %d\n", prop_range.lMin, -14000 );
    ok( prop_range.lMax == -4000, "got %ld expected %d\n", prop_range.lMax, -4000 );
    prop_range.diph.dwHow = DIPH_BYUSAGE;
    prop_range.diph.dwObj = MAKELONG( HID_USAGE_GENERIC_Y, HID_USAGE_PAGE_GENERIC );
    prop_range.lMin = 0xdeadbeef;
    prop_range.lMax = 0xdeadbeef;
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_RANGE, &prop_range.diph );
    ok( hr == DI_OK, "GetProperty DIPROP_RANGE returned %#lx\n", hr );
    ok( prop_range.lMin == 1000, "got %ld expected %d\n", prop_range.lMin, 1000 );
    ok( prop_range.lMax == 51000, "got %ld expected %d\n", prop_range.lMax, 51000 );

    hr = IDirectInputDevice8_GetDeviceState( device, sizeof(DIJOYSTATE2), &state );
    ok( hr == DI_OK, "GetDeviceState returned: %#lx\n", hr );
    check_member( state, expect_state_abs[1], "%ld", lX );
    check_member( state, expect_state_abs[1], "%ld", lY );
    check_member( state, expect_state_abs[1], "%ld", lZ );
    check_member( state, expect_state_abs[1], "%ld", lRx );
    check_member( state, expect_state_abs[1], "%ld", rgdwPOV[0] );
    check_member( state, expect_state_abs[1], "%ld", rgdwPOV[1] );
    check_member( state, expect_state_abs[1], "%#x", rgbButtons[0] );
    check_member( state, expect_state_abs[1], "%#x", rgbButtons[1] );
    check_member( state, expect_state_abs[1], "%#x", rgbButtons[2] );

    hr = IDirectInputDevice8_SetProperty( device, NULL, NULL );
    ok( hr == DIERR_INVALIDPARAM, "SetProperty returned %#lx\n", hr );
    hr = IDirectInputDevice8_SetProperty( device, &GUID_NULL, NULL );
    ok( hr == DIERR_INVALIDPARAM, "SetProperty returned %#lx\n", hr );
    hr = IDirectInputDevice8_SetProperty( device, DIPROP_VIDPID, NULL );
    ok( hr == DIERR_INVALIDPARAM, "SetProperty returned %#lx\n", hr );
    hr = IDirectInputDevice8_SetProperty( device, DIPROP_VIDPID, &prop_string.diph );
    ok( hr == (version < 0x0800 ? DIERR_UNSUPPORTED : DIERR_INVALIDPARAM),
        "SetProperty DIPROP_VIDPID returned %#lx\n", hr );

    prop_dword.diph.dwHow = DIPH_DEVICE;
    prop_dword.diph.dwObj = 0;
    prop_dword.dwData = 0xdeadbeef;
    hr = IDirectInputDevice8_SetProperty( device, DIPROP_VIDPID, &prop_dword.diph );
    ok( hr == (version < 0x0800 ? DIERR_UNSUPPORTED : DIERR_READONLY),
        "SetProperty DIPROP_VIDPID returned %#lx\n", hr );
    hr = IDirectInputDevice8_SetProperty( device, DIPROP_GUIDANDPATH, &prop_guid_path.diph );
    ok( hr == DIERR_READONLY, "SetProperty DIPROP_GUIDANDPATH returned %#lx\n", hr );

    prop_string.diph.dwHow = DIPH_DEVICE;
    prop_string.diph.dwObj = 0;
    wcscpy( prop_string.wsz, L"instance name" );
    hr = IDirectInputDevice8_SetProperty( device, DIPROP_INSTANCENAME, &prop_string.diph );
    ok( hr == DIERR_UNSUPPORTED, "SetProperty DIPROP_INSTANCENAME returned %#lx\n", hr );

    wcscpy( prop_string.wsz, L"product name" );
    hr = IDirectInputDevice8_SetProperty( device, DIPROP_PRODUCTNAME, &prop_string.diph );
    todo_wine
    ok( hr == DI_OK, "SetProperty DIPROP_PRODUCTNAME returned %#lx\n", hr );
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_PRODUCTNAME, &prop_string.diph );
    ok( hr == DI_OK, "GetProperty DIPROP_PRODUCTNAME returned %#lx\n", hr );
    ok( !wcscmp( prop_string.wsz, expect_devinst.tszProductName ), "got product %s\n",
        debugstr_w(prop_string.wsz) );

    hr = IDirectInputDevice8_SetProperty( device, DIPROP_TYPENAME, &prop_string.diph );
    ok( hr == (version < 0x0800 ? DIERR_UNSUPPORTED : DIERR_READONLY),
        "SetProperty DIPROP_TYPENAME returned %#lx\n", hr );
    hr = IDirectInputDevice8_SetProperty( device, DIPROP_USERNAME, &prop_string.diph );
    ok( hr == (version < 0x0800 ? DIERR_UNSUPPORTED : DIERR_READONLY),
        "SetProperty DIPROP_USERNAME returned %#lx\n", hr );
    hr = IDirectInputDevice8_SetProperty( device, DIPROP_FFLOAD, &prop_dword.diph );
    ok( hr == DIERR_READONLY, "SetProperty DIPROP_FFLOAD returned %#lx\n", hr );
    hr = IDirectInputDevice8_SetProperty( device, DIPROP_GRANULARITY, &prop_dword.diph );
    ok( hr == DIERR_READONLY, "SetProperty DIPROP_GRANULARITY returned %#lx\n", hr );

    hr = IDirectInputDevice8_SetProperty( device, DIPROP_JOYSTICKID, &prop_dword.diph );
    todo_wine
    ok( hr == DIERR_ACQUIRED, "SetProperty DIPROP_JOYSTICKID returned %#lx\n", hr );
    hr = IDirectInputDevice8_SetProperty( device, DIPROP_AXISMODE, &prop_dword.diph );
    ok( hr == DIERR_ACQUIRED, "SetProperty DIPROP_AXISMODE returned %#lx\n", hr );
    hr = IDirectInputDevice8_SetProperty( device, DIPROP_BUFFERSIZE, &prop_dword.diph );
    ok( hr == DIERR_ACQUIRED, "SetProperty DIPROP_BUFFERSIZE returned %#lx\n", hr );
    hr = IDirectInputDevice8_SetProperty( device, DIPROP_AUTOCENTER, &prop_dword.diph );
    ok( hr == DIERR_ACQUIRED, "SetProperty DIPROP_AUTOCENTER returned %#lx\n", hr );
    prop_pointer.diph.dwHow = DIPH_BYUSAGE;
    prop_pointer.diph.dwObj = MAKELONG( HID_USAGE_GENERIC_X, HID_USAGE_PAGE_GENERIC );
    hr = IDirectInputDevice8_SetProperty( device, DIPROP_APPDATA, &prop_pointer.diph );
    todo_wine_if( version >= 0x0800 )
    ok( hr == (version < 0x0800 ? DIERR_UNSUPPORTED : DIERR_ACQUIRED),
        "SetProperty DIPROP_APPDATA returned %#lx\n", hr );

    prop_dword.diph.dwHow = DIPH_DEVICE;
    prop_dword.diph.dwObj = 0;
    prop_dword.dwData = 10001;
    hr = IDirectInputDevice8_SetProperty( device, DIPROP_DEADZONE, &prop_dword.diph );
    ok( hr == DIERR_INVALIDPARAM, "SetProperty DIPROP_DEADZONE returned %#lx\n", hr );
    hr = IDirectInputDevice8_SetProperty( device, DIPROP_SATURATION, &prop_dword.diph );
    ok( hr == DIERR_INVALIDPARAM, "SetProperty DIPROP_SATURATION returned %#lx\n", hr );
    hr = IDirectInputDevice8_SetProperty( device, DIPROP_CALIBRATIONMODE, &prop_dword.diph );
    ok( hr == DIERR_INVALIDPARAM, "SetProperty DIPROP_CALIBRATIONMODE returned %#lx\n", hr );
    prop_dword.dwData = 1000;
    hr = IDirectInputDevice8_SetProperty( device, DIPROP_DEADZONE, &prop_dword.diph );
    ok( hr == DI_OK, "SetProperty DIPROP_DEADZONE returned %#lx\n", hr );
    prop_dword.dwData = 6000;
    hr = IDirectInputDevice8_SetProperty( device, DIPROP_SATURATION, &prop_dword.diph );
    ok( hr == DI_OK, "SetProperty DIPROP_SATURATION returned %#lx\n", hr );
    prop_dword.dwData = DIPROPCALIBRATIONMODE_COOKED;
    hr = IDirectInputDevice8_SetProperty( device, DIPROP_CALIBRATIONMODE, &prop_dword.diph );
    ok( hr == DI_OK, "SetProperty DIPROP_CALIBRATIONMODE returned %#lx\n", hr );

    hr = IDirectInputDevice8_GetProperty( device, DIPROP_DEADZONE, &prop_dword.diph );
    ok( hr == DIERR_UNSUPPORTED, "GetProperty DIPROP_DEADZONE returned %#lx\n", hr );
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_SATURATION, &prop_dword.diph );
    ok( hr == DIERR_UNSUPPORTED, "GetProperty DIPROP_SATURATION returned %#lx\n", hr );
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_CALIBRATIONMODE, &prop_dword.diph );
    ok( hr == DIERR_UNSUPPORTED, "GetProperty DIPROP_CALIBRATIONMODE returned %#lx\n", hr );

    prop_dword.diph.dwHow = DIPH_BYUSAGE;
    prop_dword.diph.dwObj = MAKELONG( HID_USAGE_GENERIC_X, HID_USAGE_PAGE_GENERIC );
    prop_dword.dwData = 2000;
    hr = IDirectInputDevice8_SetProperty( device, DIPROP_DEADZONE, &prop_dword.diph );
    ok( hr == DI_OK, "SetProperty DIPROP_DEADZONE returned %#lx\n", hr );
    ok( prop_dword.dwData == 2000, "got %lu expected %u\n", prop_dword.dwData, 2000 );
    prop_dword.dwData = 7000;
    hr = IDirectInputDevice8_SetProperty( device, DIPROP_SATURATION, &prop_dword.diph );
    ok( hr == DI_OK, "SetProperty DIPROP_SATURATION returned %#lx\n", hr );

    prop_dword.diph.dwHow = DIPH_BYUSAGE;
    prop_dword.diph.dwObj = MAKELONG( HID_USAGE_GENERIC_X, HID_USAGE_PAGE_GENERIC );
    prop_dword.dwData = 0xdeadbeef;
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_DEADZONE, &prop_dword.diph );
    ok( hr == DI_OK, "GetProperty DIPROP_DEADZONE returned %#lx\n", hr );
    ok( prop_dword.dwData == 2000, "got %lu expected %u\n", prop_dword.dwData, 2000 );
    prop_dword.dwData = 0xdeadbeef;
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_SATURATION, &prop_dword.diph );
    ok( hr == DI_OK, "GetProperty DIPROP_SATURATION returned %#lx\n", hr );
    ok( prop_dword.dwData == 7000, "got %lu expected %u\n", prop_dword.dwData, 7000 );

    prop_dword.diph.dwHow = DIPH_BYUSAGE;
    prop_dword.diph.dwObj = MAKELONG( HID_USAGE_GENERIC_Y, HID_USAGE_PAGE_GENERIC );
    prop_dword.dwData = 0xdeadbeef;
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_DEADZONE, &prop_dword.diph );
    ok( hr == DI_OK, "GetProperty DIPROP_DEADZONE returned %#lx\n", hr );
    ok( prop_dword.dwData == 1000, "got %lu expected %u\n", prop_dword.dwData, 1000 );
    prop_dword.dwData = 0xdeadbeef;
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_SATURATION, &prop_dword.diph );
    ok( hr == DI_OK, "GetProperty DIPROP_SATURATION returned %#lx\n", hr );
    ok( prop_dword.dwData == 6000, "got %lu expected %u\n", prop_dword.dwData, 6000 );

    for (i = 0; i < ARRAY_SIZE(injected_input); ++i)
    {
        winetest_push_context( "state[%ld]", i );
        hr = IDirectInputDevice8_GetDeviceState( device, sizeof(DIJOYSTATE2), &state );
        ok( hr == DI_OK, "GetDeviceState returned: %#lx\n", hr );
        if (broken( state.lX == -10750 )) win_skip( "Ignoring 32-bit rounding\n" );
        else
        {
            check_member( state, expect_state_abs[i], "%ld", lX );
            check_member( state, expect_state_abs[i], "%ld", lY );
        }
        check_member( state, expect_state_abs[i], "%ld", lZ );
        check_member( state, expect_state_abs[i], "%ld", lRx );
        check_member( state, expect_state_abs[i], "%ld", rgdwPOV[0] );
        check_member( state, expect_state_abs[i], "%ld", rgdwPOV[1] );
        check_member( state, expect_state_abs[i], "%#x", rgbButtons[0] );
        check_member( state, expect_state_abs[i], "%#x", rgbButtons[1] );
        check_member( state, expect_state_abs[i], "%#x", rgbButtons[2] );

        send_hid_input( file, &injected_input[i], sizeof(*injected_input) );

        res = WaitForSingleObject( event, 100 );
        if (i == 0 || i == 3) ok( res == WAIT_TIMEOUT, "WaitForSingleObject succeeded\n" );
        else ok( res == WAIT_OBJECT_0, "WaitForSingleObject failed\n" );
        ResetEvent( event );
        winetest_pop_context();
    }

    hr = IDirectInputDevice8_GetDeviceState( device, sizeof(DIJOYSTATE2), &state );
    ok( hr == DI_OK, "GetDeviceState returned: %#lx\n", hr );
    winetest_push_context( "state[%ld]", i );
    check_member( state, expect_state_abs[i], "%ld", lX );
    check_member( state, expect_state_abs[i], "%ld", lY );
    check_member( state, expect_state_abs[i], "%ld", lZ );
    check_member( state, expect_state_abs[i], "%ld", lRx );
    check_member( state, expect_state_abs[i], "%ld", rgdwPOV[0] );
    check_member( state, expect_state_abs[i], "%ld", rgdwPOV[1] );
    check_member( state, expect_state_abs[i], "%#x", rgbButtons[0] );
    check_member( state, expect_state_abs[i], "%#x", rgbButtons[1] );
    check_member( state, expect_state_abs[i], "%#x", rgbButtons[2] );
    winetest_pop_context();

    prop_dword.diph.dwHow = DIPH_BYUSAGE;
    prop_dword.diph.dwObj = MAKELONG( HID_USAGE_GENERIC_X, HID_USAGE_PAGE_GENERIC );
    prop_dword.dwData = DIPROPCALIBRATIONMODE_RAW;
    hr = IDirectInputDevice8_SetProperty( device, DIPROP_CALIBRATIONMODE, &prop_dword.diph );
    ok( hr == DI_OK, "SetProperty DIPROP_CALIBRATIONMODE returned %#lx\n", hr );
    prop_dword.dwData = 0xdeadbeef;
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_CALIBRATIONMODE, &prop_dword.diph );
    ok( hr == DI_OK, "GetProperty DIPROP_CALIBRATIONMODE returned %#lx\n", hr );
    ok( prop_dword.dwData == DIPROPCALIBRATIONMODE_RAW, "got %lu expected %u\n", prop_dword.dwData,
        DIPROPCALIBRATIONMODE_RAW );

    hr = IDirectInputDevice8_GetDeviceState( device, sizeof(DIJOYSTATE2), &state );
    ok( hr == DI_OK, "GetDeviceState returned: %#lx\n", hr );
    winetest_push_context( "state[%ld]", i );
    todo_wine_if( version >= 0x0700 )
    ok( state.lX == (version < 0x0700 ? -9000 : 15), "got lX %ld\n", state.lX );
    check_member( state, expect_state_abs[0], "%ld", lY );
    check_member( state, expect_state_abs[0], "%ld", lZ );
    check_member( state, expect_state_abs[0], "%ld", lRx );
    check_member( state, expect_state_abs[0], "%ld", rgdwPOV[0] );
    check_member( state, expect_state_abs[0], "%ld", rgdwPOV[1] );
    winetest_pop_context();

    prop_dword.dwData = DIPROPCALIBRATIONMODE_COOKED;
    hr = IDirectInputDevice8_SetProperty( device, DIPROP_CALIBRATIONMODE, &prop_dword.diph );
    ok( hr == DI_OK, "SetProperty DIPROP_CALIBRATIONMODE returned %#lx\n", hr );

    send_hid_input( file, &injected_input[ARRAY_SIZE(injected_input) - 1], sizeof(*injected_input) );
    res = WaitForSingleObject( event, 100 );
    ok( res == WAIT_OBJECT_0, "WaitForSingleObject failed\n" );

    hr = IDirectInputDevice8_Unacquire( device );
    ok( hr == DI_OK, "Unacquire returned: %#lx\n", hr );

    prop_dword.diph.dwHow = DIPH_DEVICE;
    prop_dword.diph.dwObj = 0;
    hr = IDirectInputDevice8_SetProperty( device, DIPROP_JOYSTICKID, &prop_dword.diph );
    ok( hr == DIERR_UNSUPPORTED, "SetProperty DIPROP_JOYSTICKID returned %#lx\n", hr );
    prop_dword.dwData = 0x1000;
    hr = IDirectInputDevice8_SetProperty( device, DIPROP_BUFFERSIZE, &prop_dword.diph );
    ok( hr == DI_OK, "SetProperty DIPROP_BUFFERSIZE returned %#lx\n", hr );
    prop_dword.dwData = 0xdeadbeef;
    hr = IDirectInputDevice8_SetProperty( device, DIPROP_AUTOCENTER, &prop_dword.diph );
    ok( hr == DIERR_INVALIDPARAM, "SetProperty DIPROP_AUTOCENTER returned %#lx\n", hr );
    prop_dword.dwData = DIPROPAUTOCENTER_ON;
    hr = IDirectInputDevice8_SetProperty( device, DIPROP_AUTOCENTER, &prop_dword.diph );
    ok( hr == DIERR_UNSUPPORTED, "SetProperty DIPROP_AUTOCENTER returned %#lx\n", hr );
    prop_pointer.diph.dwHow = DIPH_BYUSAGE;
    prop_pointer.diph.dwObj = MAKELONG( HID_USAGE_GENERIC_X, HID_USAGE_PAGE_GENERIC );
    prop_pointer.uData = 0xfeedcafe;
    hr = IDirectInputDevice8_SetProperty( device, DIPROP_APPDATA, &prop_pointer.diph );
    ok( hr == (version < 0x0800 ? DIERR_UNSUPPORTED : DI_OK),
        "SetProperty DIPROP_APPDATA returned %#lx\n", hr );

    prop_dword.dwData = 0xdeadbeef;
    hr = IDirectInputDevice8_SetProperty( device, DIPROP_AXISMODE, &prop_dword.diph );
    ok( hr == DIERR_INVALIDPARAM, "SetProperty DIPROP_AXISMODE returned %#lx\n", hr );
    prop_dword.dwData = DIPROPAXISMODE_REL;
    hr = IDirectInputDevice8_SetProperty( device, DIPROP_AXISMODE, &prop_dword.diph );
    ok( hr == DI_OK, "SetProperty DIPROP_AXISMODE returned %#lx\n", hr );

    hr = IDirectInputDevice8_Acquire( device );
    ok( hr == DI_OK, "Unacquire returned: %#lx\n", hr );

    prop_dword.dwData = 0xdeadbeef;
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_AXISMODE, &prop_dword.diph );
    todo_wine
    ok( hr == DI_OK, "GetProperty DIPROP_AXISMODE returned %#lx\n", hr );
    todo_wine
    ok( prop_dword.dwData == DIPROPAXISMODE_REL, "got %lu expected %u\n", prop_dword.dwData, DIPROPAXISMODE_REL );

    prop_dword.dwData = 0xdeadbeef;
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_BUFFERSIZE, &prop_dword.diph );
    ok( hr == DI_OK, "GetProperty DIPROP_BUFFERSIZE returned %#lx\n", hr );
    ok( prop_dword.dwData == 0x1000, "got %#lx expected %#x\n", prop_dword.dwData, 0x1000 );

    prop_pointer.diph.dwHow = DIPH_BYUSAGE;
    prop_pointer.diph.dwObj = MAKELONG( HID_USAGE_GENERIC_X, HID_USAGE_PAGE_GENERIC );
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_APPDATA, &prop_pointer.diph );
    todo_wine_if( version >= 0x0800 )
    ok( hr == (version < 0x0800 ? DIERR_UNSUPPORTED : DI_OK),
        "GetProperty DIPROP_APPDATA returned %#lx\n", hr );
    if (hr == DI_OK) ok( prop_pointer.uData == 0xfeedcafe, "got %p\n", (void *)prop_pointer.uData );

    prop_dword.diph.dwHow = DIPH_DEVICE;
    prop_dword.diph.dwObj = 0;
    prop_dword.dwData = 0xdeadbeef;
    hr = IDirectInputDevice8_SetProperty( device, DIPROP_FFGAIN, &prop_dword.diph );
    ok( hr == DIERR_INVALIDPARAM, "SetProperty DIPROP_FFGAIN returned %#lx\n", hr );
    prop_dword.dwData = 1000;
    hr = IDirectInputDevice8_SetProperty( device, DIPROP_FFGAIN, &prop_dword.diph );
    ok( hr == DI_OK, "SetProperty DIPROP_FFGAIN returned %#lx\n", hr );

    prop_dword.dwData = 0xdeadbeef;
    hr = IDirectInputDevice8_SetProperty( device, DIPROP_CALIBRATION, &prop_dword.diph );
    todo_wine
    ok( hr == DIERR_INVALIDPARAM, "SetProperty DIPROP_CALIBRATION returned %#lx\n", hr );
    prop_dword.dwData = 0xdeadbeef;
    hr = IDirectInputDevice8_SetProperty( device, DIPROP_DEADZONE, &prop_dword.diph );
    ok( hr == DIERR_INVALIDPARAM, "SetProperty DIPROP_DEADZONE returned %#lx\n", hr );
    prop_dword.dwData = 0xdeadbeef;
    hr = IDirectInputDevice8_SetProperty( device, DIPROP_SATURATION, &prop_dword.diph );
    ok( hr == DIERR_INVALIDPARAM, "SetProperty DIPROP_SATURATION returned %#lx\n", hr );

    for (i = 0; i < ARRAY_SIZE(injected_input); ++i)
    {
        winetest_push_context( "state[%ld]", i );
        hr = IDirectInputDevice8_GetDeviceState( device, sizeof(DIJOYSTATE2), &state );
        ok( hr == DI_OK, "GetDeviceState returned: %#lx\n", hr );
        todo_wine
        check_member( state, expect_state_rel[i], "%ld", lX );
        todo_wine
        check_member( state, expect_state_rel[i], "%ld", lY );
        todo_wine
        check_member( state, expect_state_rel[i], "%ld", lZ );
        check_member( state, expect_state_rel[i], "%ld", lRx );
        check_member( state, expect_state_rel[i], "%ld", rgdwPOV[0] );
        check_member( state, expect_state_rel[i], "%ld", rgdwPOV[1] );
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
    ok( hr == DI_OK, "GetDeviceState returned: %#lx\n", hr );
    winetest_push_context( "state[%ld]", i );
    todo_wine
    check_member( state, expect_state_rel[i], "%ld", lX );
    todo_wine
    check_member( state, expect_state_rel[i], "%ld", lY );
    todo_wine
    check_member( state, expect_state_rel[i], "%ld", lZ );
    check_member( state, expect_state_rel[i], "%ld", lRx );
    check_member( state, expect_state_rel[i], "%ld", rgdwPOV[0] );
    check_member( state, expect_state_rel[i], "%ld", rgdwPOV[1] );
    check_member( state, expect_state_rel[i], "%#x", rgbButtons[0] );
    check_member( state, expect_state_rel[i], "%#x", rgbButtons[1] );
    check_member( state, expect_state_rel[i], "%#x", rgbButtons[2] );
    winetest_pop_context();

    hr = IDirectInputDevice8_GetForceFeedbackState( device, NULL );
    ok( hr == E_POINTER, "GetForceFeedbackState returned %#lx\n", hr );
    res = 0xdeadbeef;
    hr = IDirectInputDevice8_GetForceFeedbackState( device, &res );
    ok( hr == DIERR_UNSUPPORTED, "GetForceFeedbackState returned %#lx\n", hr );

    hr = IDirectInputDevice8_SendForceFeedbackCommand( device, 0xdeadbeef );
    ok( hr == DIERR_INVALIDPARAM, "SendForceFeedbackCommand returned %#lx\n", hr );
    hr = IDirectInputDevice8_SendForceFeedbackCommand( device, DISFFC_RESET );
    ok( hr == DIERR_UNSUPPORTED, "SendForceFeedbackCommand returned %#lx\n", hr );

    objdata[0].dwOfs = 0xd;
    objdata[0].dwData = 0x80;
    res = 1;
    hr = IDirectInputDevice8_SendDeviceData( device, size, objdata, &res, 0xdeadbeef );
    todo_wine
    ok( hr == DIERR_INVALIDPARAM, "SendDeviceData returned %#lx\n", hr );
    res = 1;
    hr = IDirectInputDevice8_SendDeviceData( device, size, objdata, &res, 1 /*DISDD_CONTINUE*/ );
    todo_wine
    ok( hr == DIERR_INVALIDPARAM, "SendDeviceData returned %#lx\n", hr );
    res = 1;
    hr = IDirectInputDevice8_SendDeviceData( device, size, objdata, &res, 0 );
    todo_wine
    ok( hr == DIERR_INVALIDPARAM, "SendDeviceData returned %#lx\n", hr );

    hr = IDirectInputDevice8_CreateEffect( device, &GUID_Sine, NULL, NULL, NULL );
    ok( hr == E_POINTER, "CreateEffect returned %#lx\n", hr );
    hr = IDirectInputDevice8_CreateEffect( device, NULL, NULL, &effect, NULL );
    ok( hr == DIERR_UNSUPPORTED, "CreateEffect returned %#lx\n", hr );
    hr = IDirectInputDevice8_CreateEffect( device, &GUID_NULL, NULL, &effect, NULL );
    ok( hr == DIERR_UNSUPPORTED, "CreateEffect returned %#lx\n", hr );
    hr = IDirectInputDevice8_CreateEffect( device, &GUID_Sine, NULL, &effect, NULL );
    ok( hr == DIERR_UNSUPPORTED, "CreateEffect returned %#lx\n", hr );

    hr = IDirectInputDevice8_Unacquire( device );
    ok( hr == DI_OK, "Unacquire returned: %#lx\n", hr );

    hr = IDirectInputDevice8_CreateEffect( device, &GUID_Sine, NULL, &effect, NULL );
    ok( hr == DIERR_UNSUPPORTED, "CreateEffect returned %#lx\n", hr );

    hr = IDirectInputDevice8_EnumCreatedEffectObjects( device, NULL, effect, 0 );
    ok( hr == DIERR_INVALIDPARAM, "EnumCreatedEffectObjects returned %#lx\n", hr );
    hr = IDirectInputDevice8_EnumCreatedEffectObjects( device, check_no_created_effect_objects, effect, 0xdeadbeef );
    ok( hr == DIERR_INVALIDPARAM, "EnumCreatedEffectObjects returned %#lx\n", hr );
    hr = IDirectInputDevice8_EnumCreatedEffectObjects( device, check_no_created_effect_objects, (void *)0xdeadbeef, 0 );
    ok( hr == DI_OK, "EnumCreatedEffectObjects returned %#lx\n", hr );

    hr = IDirectInputDevice8_Escape( device, NULL );
    todo_wine
    ok( hr == E_POINTER, "Escape returned: %#lx\n", hr );
    hr = IDirectInputDevice8_Escape( device, &escape );
    todo_wine
    ok( hr == DIERR_INVALIDPARAM, "Escape returned: %#lx\n", hr );
    escape.dwSize = sizeof(DIEFFESCAPE) + 1;
    hr = IDirectInputDevice8_Escape( device, &escape );
    todo_wine
    ok( hr == DIERR_INVALIDPARAM, "Escape returned: %#lx\n", hr );
    escape.dwSize = sizeof(DIEFFESCAPE);
    escape.dwCommand = 0;
    escape.lpvInBuffer = buffer;
    escape.cbInBuffer = 10;
    escape.lpvOutBuffer = buffer + 10;
    escape.cbOutBuffer = 10;
    hr = IDirectInputDevice8_Escape( device, &escape );
    todo_wine
    ok( hr == DIERR_UNSUPPORTED, "Escape returned: %#lx\n", hr );

    ref = IDirectInputDevice8_Release( device );
    ok( ref == 0, "Release returned %ld\n", ref );

    CloseHandle( event );
    CloseHandle( file );

done:
    hid_device_stop( &desc );
    cleanup_registry_keys();
    winetest_pop_context();
}

struct device_desc
{
    const BYTE *report_desc_buf;
    ULONG report_desc_len;
    HIDP_CAPS hid_caps;
};

static BOOL test_device_types( DWORD version )
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
    static const unsigned char wheel_steering_only_desc[] =
    {
        USAGE_PAGE(1, HID_USAGE_PAGE_GENERIC),
        USAGE(1, HID_USAGE_GENERIC_JOYSTICK),
        COLLECTION(1, Application),
            USAGE(1, HID_USAGE_GENERIC_JOYSTICK),
            COLLECTION(1, Physical),
                USAGE(4, (HID_USAGE_PAGE_SIMULATION<<16)|HID_USAGE_SIMULATION_STEERING),
                LOGICAL_MINIMUM(1, 0),
                LOGICAL_MAXIMUM(1, 127),
                PHYSICAL_MINIMUM(1, 0),
                PHYSICAL_MAXIMUM(1, 127),
                REPORT_SIZE(1, 8),
                REPORT_COUNT(1, 1),
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
    static const unsigned char wheel_dualpedals_desc[] =
    {
        USAGE_PAGE(1, HID_USAGE_PAGE_GENERIC),
        USAGE(1, HID_USAGE_GENERIC_JOYSTICK),
        COLLECTION(1, Application),
            USAGE(1, HID_USAGE_GENERIC_JOYSTICK),
            COLLECTION(1, Physical),
                USAGE(4, (HID_USAGE_PAGE_SIMULATION<<16)|HID_USAGE_SIMULATION_STEERING),
                USAGE(4, (HID_USAGE_PAGE_SIMULATION<<16)|HID_USAGE_SIMULATION_ACCELERATOR),
                USAGE(4, (HID_USAGE_PAGE_SIMULATION<<16)|HID_USAGE_SIMULATION_BRAKE),
                USAGE(1, HID_USAGE_GENERIC_X),
                LOGICAL_MINIMUM(1, 0),
                LOGICAL_MAXIMUM(1, 127),
                PHYSICAL_MINIMUM(1, 0),
                PHYSICAL_MAXIMUM(1, 127),
                REPORT_SIZE(1, 8),
                REPORT_COUNT(1, 4),
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
    static const unsigned char wheel_threepedals_desc[] =
    {
        USAGE_PAGE(1, HID_USAGE_PAGE_GENERIC),
        USAGE(1, HID_USAGE_GENERIC_JOYSTICK),
        COLLECTION(1, Application),
            USAGE(1, HID_USAGE_GENERIC_JOYSTICK),
            COLLECTION(1, Physical),
                USAGE(4, (HID_USAGE_PAGE_SIMULATION<<16)|HID_USAGE_SIMULATION_STEERING),
                USAGE(4, (HID_USAGE_PAGE_SIMULATION<<16)|HID_USAGE_SIMULATION_ACCELERATOR),
                USAGE(4, (HID_USAGE_PAGE_SIMULATION<<16)|HID_USAGE_SIMULATION_BRAKE),
                USAGE(4, (HID_USAGE_PAGE_SIMULATION<<16)|HID_USAGE_SIMULATION_CLUTCH),
                USAGE(1, HID_USAGE_GENERIC_Y),
                LOGICAL_MINIMUM(1, 0),
                LOGICAL_MAXIMUM(1, 127),
                PHYSICAL_MINIMUM(1, 0),
                PHYSICAL_MAXIMUM(1, 127),
                REPORT_SIZE(1, 8),
                REPORT_COUNT(1, 5),
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
        {
            .report_desc_buf = wheel_steering_only_desc,
            .report_desc_len = sizeof(wheel_steering_only_desc),
            .hid_caps =
            {
                .InputReportByteLength = 3,
            },
        },
        {
            .report_desc_buf = wheel_dualpedals_desc,
            .report_desc_len = sizeof(wheel_dualpedals_desc),
            .hid_caps =
            {
                .InputReportByteLength = 6,
            },
        },
        {
            .report_desc_buf = wheel_threepedals_desc,
            .report_desc_len = sizeof(wheel_threepedals_desc),
            .hid_caps =
            {
                .InputReportByteLength = 7,
            },
        },
    };
    const DIDEVCAPS expect_caps[] =
    {
        {
            .dwSize = sizeof(DIDEVCAPS),
            .dwFlags = DIDC_ATTACHED|DIDC_EMULATED,
            .dwDevType = version >= 0x800 ? DIDEVTYPE_HID|(DI8DEVTYPESUPPLEMENTAL_UNKNOWN << 8)|DI8DEVTYPE_SUPPLEMENTAL
                                          : DIDEVTYPE_HID|(DIDEVTYPEJOYSTICK_UNKNOWN << 8)|DIDEVTYPE_JOYSTICK,
            .dwButtons = 6,
        },
        {
            .dwSize = sizeof(DIDEVCAPS),
            .dwFlags = DIDC_ATTACHED|DIDC_EMULATED,
            .dwDevType = version >= 0x800 ? DIDEVTYPE_HID|(DI8DEVTYPEJOYSTICK_LIMITED << 8)|DI8DEVTYPE_JOYSTICK
                                          : DIDEVTYPE_HID|(DIDEVTYPEJOYSTICK_UNKNOWN << 8)|DIDEVTYPE_JOYSTICK,
            .dwAxes = 2,
            .dwButtons = 6,
        },
        {
            .dwSize = sizeof(DIDEVCAPS),
            .dwFlags = DIDC_ATTACHED|DIDC_EMULATED,
            .dwDevType = version >= 0x800 ? DIDEVTYPE_HID|(DI8DEVTYPEGAMEPAD_STANDARD << 8)|DI8DEVTYPE_GAMEPAD
                                          : DIDEVTYPE_HID|(DIDEVTYPEJOYSTICK_GAMEPAD << 8)|DIDEVTYPE_JOYSTICK,
            .dwAxes = 2,
            .dwButtons = 6,
        },
        {
            .dwSize = sizeof(DIDEVCAPS),
            .dwFlags = DIDC_ATTACHED|DIDC_EMULATED,
            .dwDevType = version >= 0x800 ? DIDEVTYPE_HID|(DI8DEVTYPEJOYSTICK_STANDARD << 8)|DI8DEVTYPE_JOYSTICK
                                          : DIDEVTYPE_HID|(DIDEVTYPEJOYSTICK_UNKNOWN << 8)|DIDEVTYPE_JOYSTICK,
            .dwAxes = 3,
            .dwPOVs = 1,
            .dwButtons = 5,
        },
        {
            .dwSize = sizeof(DIDEVCAPS),
            .dwFlags = DIDC_ATTACHED|DIDC_EMULATED,
            .dwDevType = version >= 0x800 ? DIDEVTYPE_HID|(DI8DEVTYPEDRIVING_LIMITED << 8)|DI8DEVTYPE_DRIVING
                                          : DIDEVTYPE_HID|(DIDEVTYPEJOYSTICK_WHEEL << 8)|DIDEVTYPE_JOYSTICK,
            .dwAxes = 1,
            .dwPOVs = 1,
            .dwButtons = 5,
        },
        {
            .dwSize = sizeof(DIDEVCAPS),
            .dwFlags = DIDC_ATTACHED|DIDC_EMULATED,
            .dwDevType = version >= 0x800 ? DIDEVTYPE_HID|(DI8DEVTYPEDRIVING_DUALPEDALS << 8)|DI8DEVTYPE_DRIVING
                                          : DIDEVTYPE_HID|(DIDEVTYPEJOYSTICK_WHEEL << 8)|DIDEVTYPE_JOYSTICK,
            .dwAxes = 4,
            .dwPOVs = 1,
            .dwButtons = 5,
        },
        {
            .dwSize = sizeof(DIDEVCAPS),
            .dwFlags = DIDC_ATTACHED|DIDC_EMULATED,
            .dwDevType = version >= 0x800 ? DIDEVTYPE_HID|(DI8DEVTYPEDRIVING_THREEPEDALS << 8)|DI8DEVTYPE_DRIVING
                                          : DIDEVTYPE_HID|(DIDEVTYPEJOYSTICK_WHEEL << 8)|DIDEVTYPE_JOYSTICK,
            .dwAxes = 5,
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
            .dwDevType = version >= 0x800 ? DIDEVTYPE_HID|(DI8DEVTYPESUPPLEMENTAL_UNKNOWN << 8)|DI8DEVTYPE_SUPPLEMENTAL
                                          : DIDEVTYPE_HID|(DIDEVTYPEJOYSTICK_UNKNOWN << 8)|DIDEVTYPE_JOYSTICK,
            .tszInstanceName = L"Wine Test",
            .tszProductName = L"Wine Test",
            .guidFFDriver = GUID_NULL,
            .wUsagePage = HID_USAGE_PAGE_GENERIC,
            .wUsage = HID_USAGE_GENERIC_JOYSTICK,
        },
        {
            .dwSize = sizeof(DIDEVICEINSTANCEW),
            .guidInstance = expect_guid_product,
            .guidProduct = expect_guid_product,
            .dwDevType = version >= 0x800 ? DIDEVTYPE_HID|(DI8DEVTYPEJOYSTICK_LIMITED << 8)|DI8DEVTYPE_JOYSTICK
                                          : DIDEVTYPE_HID|(DIDEVTYPEJOYSTICK_UNKNOWN << 8)|DIDEVTYPE_JOYSTICK,
            .tszInstanceName = L"Wine Test",
            .tszProductName = L"Wine Test",
            .guidFFDriver = GUID_NULL,
            .wUsagePage = HID_USAGE_PAGE_GENERIC,
            .wUsage = HID_USAGE_GENERIC_JOYSTICK,
        },
        {
            .dwSize = sizeof(DIDEVICEINSTANCEW),
            .guidInstance = expect_guid_product,
            .guidProduct = expect_guid_product,
            .dwDevType = version >= 0x800 ? DIDEVTYPE_HID|(DI8DEVTYPEGAMEPAD_STANDARD << 8)|DI8DEVTYPE_GAMEPAD
                                          : DIDEVTYPE_HID|(DIDEVTYPEJOYSTICK_GAMEPAD << 8)|DIDEVTYPE_JOYSTICK,
            .tszInstanceName = L"Wine Test",
            .tszProductName = L"Wine Test",
            .guidFFDriver = GUID_NULL,
            .wUsagePage = HID_USAGE_PAGE_GENERIC,
            .wUsage = HID_USAGE_GENERIC_GAMEPAD,
        },
        {
            .dwSize = sizeof(DIDEVICEINSTANCEW),
            .guidInstance = expect_guid_product,
            .guidProduct = expect_guid_product,
            .dwDevType = version >= 0x800 ? DIDEVTYPE_HID|(DI8DEVTYPEJOYSTICK_STANDARD << 8)|DI8DEVTYPE_JOYSTICK
                                          : DIDEVTYPE_HID|(DIDEVTYPEJOYSTICK_UNKNOWN << 8)|DIDEVTYPE_JOYSTICK,
            .tszInstanceName = L"Wine Test",
            .tszProductName = L"Wine Test",
            .guidFFDriver = GUID_NULL,
            .wUsagePage = HID_USAGE_PAGE_GENERIC,
            .wUsage = HID_USAGE_GENERIC_JOYSTICK,
        },
        {
            .dwSize = sizeof(DIDEVICEINSTANCEW),
            .guidInstance = expect_guid_product,
            .guidProduct = expect_guid_product,
            .dwDevType = version >= 0x800 ? DIDEVTYPE_HID|(DI8DEVTYPEDRIVING_LIMITED << 8)|DI8DEVTYPE_DRIVING
                                          : DIDEVTYPE_HID|(DIDEVTYPEJOYSTICK_WHEEL << 8)|DIDEVTYPE_JOYSTICK,
            .tszInstanceName = L"Wine Test",
            .tszProductName = L"Wine Test",
            .guidFFDriver = GUID_NULL,
            .wUsagePage = HID_USAGE_PAGE_GENERIC,
            .wUsage = HID_USAGE_GENERIC_JOYSTICK,
        },
        {
            .dwSize = sizeof(DIDEVICEINSTANCEW),
            .guidInstance = expect_guid_product,
            .guidProduct = expect_guid_product,
            .dwDevType = version >= 0x800 ? DIDEVTYPE_HID|(DI8DEVTYPEDRIVING_DUALPEDALS << 8)|DI8DEVTYPE_DRIVING
                                          : DIDEVTYPE_HID|(DIDEVTYPEJOYSTICK_WHEEL << 8)|DIDEVTYPE_JOYSTICK,
            .tszInstanceName = L"Wine Test",
            .tszProductName = L"Wine Test",
            .guidFFDriver = GUID_NULL,
            .wUsagePage = HID_USAGE_PAGE_GENERIC,
            .wUsage = HID_USAGE_GENERIC_JOYSTICK,
        },
        {
            .dwSize = sizeof(DIDEVICEINSTANCEW),
            .guidInstance = expect_guid_product,
            .guidProduct = expect_guid_product,
            .dwDevType = version >= 0x800 ? DIDEVTYPE_HID|(DI8DEVTYPEDRIVING_THREEPEDALS << 8)|DI8DEVTYPE_DRIVING
                                          : DIDEVTYPE_HID|(DIDEVTYPEJOYSTICK_WHEEL << 8)|DIDEVTYPE_JOYSTICK,
            .tszInstanceName = L"Wine Test",
            .tszProductName = L"Wine Test",
            .guidFFDriver = GUID_NULL,
            .wUsagePage = HID_USAGE_PAGE_GENERIC,
            .wUsage = HID_USAGE_GENERIC_JOYSTICK,
        },
    };
    struct hid_device_desc desc =
    {
        .use_report_id = TRUE,
        .attributes = default_attributes,
    };

    C_ASSERT(ARRAY_SIZE(expect_caps) == ARRAY_SIZE(device_desc));
    C_ASSERT(ARRAY_SIZE(expect_devinst) == ARRAY_SIZE(device_desc));

    DIDEVICEINSTANCEW devinst = {.dwSize = sizeof(DIDEVICEINSTANCEW)};
    DIDEVCAPS caps = {.dwSize = sizeof(DIDEVCAPS)};
    IDirectInputDevice8W *device;
    BOOL success = TRUE;
    ULONG i, ref;
    HRESULT hr;

    winetest_push_context( "%#lx", version );

    for (i = 0; i < ARRAY_SIZE(device_desc) && success; ++i)
    {
        winetest_push_context( "desc[%ld]", i );
        cleanup_registry_keys();

        desc.caps = device_desc[i].hid_caps;
        desc.report_descriptor_len = device_desc[i].report_desc_len;
        memcpy( desc.report_descriptor_buf, device_desc[i].report_desc_buf, device_desc[i].report_desc_len );
        fill_context( __LINE__, desc.context, ARRAY_SIZE(desc.context) );

        if (!hid_device_start( &desc ))
        {
            success = FALSE;
            goto done;
        }

        if (FAILED(hr = dinput_test_create_device( version, &devinst, &device )))
        {
            success = FALSE;
            goto done;
        }

        hr = IDirectInputDevice8_GetDeviceInfo( device, &devinst );
        ok( hr == DI_OK, "GetDeviceInfo returned %#lx\n", hr );
        check_member( devinst, expect_devinst[i], "%lu", dwSize );
        check_member_guid( devinst, expect_devinst[i], guidProduct );
        todo_wine_if( version <= 0x700 && i == 3 )
        check_member( devinst, expect_devinst[i], "%#lx", dwDevType );
        check_member_guid( devinst, expect_devinst[i], guidFFDriver );
        check_member( devinst, expect_devinst[i], "%04x", wUsagePage );
        check_member( devinst, expect_devinst[i], "%04x", wUsage );

        hr = IDirectInputDevice8_GetCapabilities( device, &caps );
        ok( hr == DI_OK, "GetCapabilities returned %#lx\n", hr );
        check_member( caps, expect_caps[i], "%lu", dwSize );
        check_member( caps, expect_caps[i], "%#lx", dwFlags );
        todo_wine_if( version <= 0x700 && i == 3 )
        check_member( caps, expect_caps[i], "%#lx", dwDevType );
        check_member( caps, expect_caps[i], "%lu", dwAxes );
        check_member( caps, expect_caps[i], "%lu", dwButtons );
        check_member( caps, expect_caps[i], "%lu", dwPOVs );
        check_member( caps, expect_caps[i], "%lu", dwFFSamplePeriod );
        check_member( caps, expect_caps[i], "%lu", dwFFMinTimeResolution );
        check_member( caps, expect_caps[i], "%lu", dwFirmwareRevision );
        check_member( caps, expect_caps[i], "%lu", dwHardwareRevision );
        check_member( caps, expect_caps[i], "%lu", dwFFDriverVersion );

        ref = IDirectInputDevice8_Release( device );
        ok( ref == 0, "Release returned %ld\n", ref );

    done:
        hid_device_stop( &desc );
        cleanup_registry_keys();
        winetest_pop_context();
    }

    winetest_pop_context();

    return success;
}

static void test_many_axes_joystick(void)
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

                USAGE(1, HID_USAGE_GENERIC_DIAL),
                USAGE(1, HID_USAGE_GENERIC_SLIDER),
                USAGE(4, (HID_USAGE_PAGE_SIMULATION<<16)|HID_USAGE_SIMULATION_RUDDER),
                USAGE(4, (HID_USAGE_PAGE_SIMULATION<<16)|HID_USAGE_SIMULATION_THROTTLE),
                USAGE(1, HID_USAGE_GENERIC_RZ),
                USAGE(1, HID_USAGE_GENERIC_RY),
                USAGE(1, HID_USAGE_GENERIC_RX),
                USAGE(1, HID_USAGE_GENERIC_Z),
                USAGE(1, HID_USAGE_GENERIC_Y),
                USAGE(1, HID_USAGE_GENERIC_X),
                LOGICAL_MINIMUM(1, 0),
                LOGICAL_MAXIMUM(1, 0x7f),
                PHYSICAL_MINIMUM(1, 0),
                PHYSICAL_MAXIMUM(1, 0x7f),
                REPORT_SIZE(1, 8),
                REPORT_COUNT(1, 10),
                INPUT(1, Data|Var|Abs),

                USAGE(1, HID_USAGE_GENERIC_Z),
                USAGE(1, HID_USAGE_GENERIC_Y),
                USAGE(1, HID_USAGE_GENERIC_X),
                LOGICAL_MINIMUM(1, 0),
                LOGICAL_MAXIMUM(1, 0x7f),
                PHYSICAL_MINIMUM(1, 0),
                PHYSICAL_MAXIMUM(1, 0x7f),
                UNIT(4, 0xf011), /* cm * s^-1 */
                REPORT_SIZE(1, 8),
                REPORT_COUNT(1, 3),
                INPUT(1, Data|Var|Abs),

                USAGE(1, HID_USAGE_GENERIC_Z),
                USAGE(1, HID_USAGE_GENERIC_Y),
                USAGE(1, HID_USAGE_GENERIC_X),
                LOGICAL_MINIMUM(1, 0),
                LOGICAL_MAXIMUM(1, 0x7f),
                PHYSICAL_MINIMUM(1, 0),
                PHYSICAL_MAXIMUM(1, 0x7f),
                UNIT(4, 0xe011), /* cm * s^-2 */
                REPORT_SIZE(1, 8),
                REPORT_COUNT(1, 3),
                INPUT(1, Data|Var|Abs),

                USAGE(1, HID_USAGE_GENERIC_Z),
                USAGE(1, HID_USAGE_GENERIC_Y),
                USAGE(1, HID_USAGE_GENERIC_X),
                LOGICAL_MINIMUM(1, 0),
                LOGICAL_MAXIMUM(1, 0x7f),
                PHYSICAL_MINIMUM(1, 0),
                PHYSICAL_MAXIMUM(1, 0x7f),
                UNIT(4, 0xe111), /* g * cm * s^-2 */
                REPORT_SIZE(1, 8),
                REPORT_COUNT(1, 3),
                INPUT(1, Data|Var|Abs),
                UNIT(1, 0), /* None */
            END_COLLECTION,
        END_COLLECTION,
    };
#undef REPORT_ID_OR_USAGE_PAGE
#include "pop_hid_macros.h"

    struct hid_device_desc desc =
    {
        .use_report_id = TRUE,
        .caps = { .InputReportByteLength = 20 },
        .attributes = default_attributes,
    };
    const DIDEVCAPS expect_caps =
    {
        .dwSize = sizeof(DIDEVCAPS),
        .dwFlags = DIDC_ATTACHED | DIDC_EMULATED,
        .dwDevType = DIDEVTYPE_HID | (DI8DEVTYPE1STPERSON_LIMITED << 8) | DI8DEVTYPE_1STPERSON,
        .dwAxes = 19,
    };
    const DIDEVICEINSTANCEW expect_devinst =
    {
        .dwSize = sizeof(DIDEVICEINSTANCEW),
        .guidInstance = expect_guid_product,
        .guidProduct = expect_guid_product,
        .dwDevType = DIDEVTYPE_HID | (DI8DEVTYPE1STPERSON_LIMITED << 8) | DI8DEVTYPE_1STPERSON,
        .tszInstanceName = L"Wine Test",
        .tszProductName = L"Wine Test",
        .guidFFDriver = GUID_NULL,
        .wUsagePage = HID_USAGE_PAGE_GENERIC,
        .wUsage = HID_USAGE_GENERIC_JOYSTICK,
    };
    const DIDEVICEOBJECTINSTANCEW expect_objects[] =
    {
        {
            .dwSize = sizeof(DIDEVICEOBJECTINSTANCEW),
            .guidType = GUID_XAxis,
            .dwOfs = 0,
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
            .guidType = GUID_YAxis,
            .dwOfs = 0x4,
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
            .guidType = GUID_ZAxis,
            .dwOfs = 0x8,
            .dwType = DIDFT_ABSAXIS|DIDFT_MAKEINSTANCE(2),
            .dwFlags = DIDOI_ASPECTPOSITION,
            .tszName = L"Z Axis",
            .wCollectionNumber = 1,
            .wUsagePage = HID_USAGE_PAGE_GENERIC,
            .wUsage = HID_USAGE_GENERIC_Z,
            .wReportId = 1,
        },
        {
            .dwSize = sizeof(DIDEVICEOBJECTINSTANCEW),
            .guidType = GUID_RxAxis,
            .dwOfs = 0xc,
            .dwType = DIDFT_ABSAXIS|DIDFT_MAKEINSTANCE(3),
            .dwFlags = DIDOI_ASPECTPOSITION,
            .tszName = L"X Rotation",
            .wCollectionNumber = 1,
            .wUsagePage = HID_USAGE_PAGE_GENERIC,
            .wUsage = HID_USAGE_GENERIC_RX,
            .wReportId = 1,
        },
        {
            .dwSize = sizeof(DIDEVICEOBJECTINSTANCEW),
            .guidType = GUID_RyAxis,
            .dwOfs = 0x10,
            .dwType = DIDFT_ABSAXIS|DIDFT_MAKEINSTANCE(4),
            .dwFlags = DIDOI_ASPECTPOSITION,
            .tszName = L"Y Rotation",
            .wCollectionNumber = 1,
            .wUsagePage = HID_USAGE_PAGE_GENERIC,
            .wUsage = HID_USAGE_GENERIC_RY,
            .wReportId = 1,
        },
        {
            .dwSize = sizeof(DIDEVICEOBJECTINSTANCEW),
            .guidType = GUID_RzAxis,
            .dwOfs = 0x14,
            .dwType = DIDFT_ABSAXIS|DIDFT_MAKEINSTANCE(5),
            .dwFlags = DIDOI_ASPECTPOSITION,
            .tszName = L"Z Rotation",
            .wCollectionNumber = 1,
            .wUsagePage = HID_USAGE_PAGE_GENERIC,
            .wUsage = HID_USAGE_GENERIC_RZ,
            .wReportId = 1,
        },
        {
            .dwSize = sizeof(DIDEVICEOBJECTINSTANCEW),
            .guidType = GUID_Slider,
            .dwOfs = 0x18,
            .dwType = DIDFT_ABSAXIS|DIDFT_MAKEINSTANCE(6),
            .dwFlags = DIDOI_ASPECTPOSITION,
            .tszName = L"Throttle",
            .wCollectionNumber = 1,
            .wUsagePage = HID_USAGE_PAGE_SIMULATION,
            .wUsage = HID_USAGE_SIMULATION_THROTTLE,
            .wReportId = 1,
        },
        {
            .dwSize = sizeof(DIDEVICEOBJECTINSTANCEW),
            .guidType = GUID_RzAxis,
            .dwOfs = 0x1c,
            .dwType = DIDFT_ABSAXIS|DIDFT_MAKEINSTANCE(7),
            .dwFlags = DIDOI_ASPECTPOSITION,
            .tszName = L"Rudder",
            .wCollectionNumber = 1,
            .wUsagePage = HID_USAGE_PAGE_SIMULATION,
            .wUsage = HID_USAGE_SIMULATION_RUDDER,
            .wReportId = 1,
        },
        {
            .dwSize = sizeof(DIDEVICEOBJECTINSTANCEW),
            .guidType = GUID_Slider,
            .dwOfs = 0x20,
            .dwType = DIDFT_ABSAXIS|DIDFT_MAKEINSTANCE(8),
            .dwFlags = DIDOI_ASPECTPOSITION,
            .tszName = L"Slider",
            .wCollectionNumber = 1,
            .wUsagePage = HID_USAGE_PAGE_GENERIC,
            .wUsage = HID_USAGE_GENERIC_SLIDER,
            .wReportId = 1,
        },
        {
            .dwSize = sizeof(DIDEVICEOBJECTINSTANCEW),
            .guidType = GUID_Slider,
            .dwOfs = 0x24,
            .dwType = DIDFT_ABSAXIS|DIDFT_MAKEINSTANCE(9),
            .dwFlags = DIDOI_ASPECTPOSITION,
            .tszName = L"Dial",
            .wCollectionNumber = 1,
            .wUsagePage = HID_USAGE_PAGE_GENERIC,
            .wUsage = HID_USAGE_GENERIC_DIAL,
            .wReportId = 1,
        },
        {
            .dwSize = sizeof(DIDEVICEOBJECTINSTANCEW),
            .guidType = GUID_XAxis,
            .dwOfs = 0x28,
            .dwType = DIDFT_ABSAXIS|DIDFT_MAKEINSTANCE(10),
            .dwFlags = DIDOI_ASPECTVELOCITY,
            .tszName = L"X Axis",
            .wCollectionNumber = 1,
            .dwDimension = 0xf011,
            .wUsagePage = HID_USAGE_PAGE_GENERIC,
            .wUsage = HID_USAGE_GENERIC_X,
            .wReportId = 1,
        },
        {
            .dwSize = sizeof(DIDEVICEOBJECTINSTANCEW),
            .guidType = GUID_YAxis,
            .dwOfs = 0x2c,
            .dwType = DIDFT_ABSAXIS|DIDFT_MAKEINSTANCE(11),
            .dwFlags = DIDOI_ASPECTVELOCITY,
            .tszName = L"Y Axis",
            .wCollectionNumber = 1,
            .dwDimension = 0xf011,
            .wUsagePage = HID_USAGE_PAGE_GENERIC,
            .wUsage = HID_USAGE_GENERIC_Y,
            .wReportId = 1,
        },
        {
            .dwSize = sizeof(DIDEVICEOBJECTINSTANCEW),
            .guidType = GUID_ZAxis,
            .dwOfs = 0x30,
            .dwType = DIDFT_ABSAXIS|DIDFT_MAKEINSTANCE(12),
            .dwFlags = DIDOI_ASPECTVELOCITY,
            .tszName = L"Z Axis",
            .wCollectionNumber = 1,
            .dwDimension = 0xf011,
            .wUsagePage = HID_USAGE_PAGE_GENERIC,
            .wUsage = HID_USAGE_GENERIC_Z,
            .wReportId = 1,
        },
        {
            .dwSize = sizeof(DIDEVICEOBJECTINSTANCEW),
            .guidType = GUID_XAxis,
            .dwOfs = 0x34,
            .dwType = DIDFT_ABSAXIS|DIDFT_MAKEINSTANCE(13),
            .dwFlags = DIDOI_ASPECTACCEL,
            .tszName = L"X Axis",
            .wCollectionNumber = 1,
            .dwDimension = 0xe011,
            .wUsagePage = HID_USAGE_PAGE_GENERIC,
            .wUsage = HID_USAGE_GENERIC_X,
            .wReportId = 1,
        },
        {
            .dwSize = sizeof(DIDEVICEOBJECTINSTANCEW),
            .guidType = GUID_YAxis,
            .dwOfs = 0x38,
            .dwType = DIDFT_ABSAXIS|DIDFT_MAKEINSTANCE(14),
            .dwFlags = DIDOI_ASPECTACCEL,
            .tszName = L"Y Axis",
            .wCollectionNumber = 1,
            .dwDimension = 0xe011,
            .wUsagePage = HID_USAGE_PAGE_GENERIC,
            .wUsage = HID_USAGE_GENERIC_Y,
            .wReportId = 1,
        },
        {
            .dwSize = sizeof(DIDEVICEOBJECTINSTANCEW),
            .guidType = GUID_ZAxis,
            .dwOfs = 0x3c,
            .dwType = DIDFT_ABSAXIS|DIDFT_MAKEINSTANCE(15),
            .dwFlags = DIDOI_ASPECTACCEL,
            .tszName = L"Z Axis",
            .wCollectionNumber = 1,
            .dwDimension = 0xe011,
            .wUsagePage = HID_USAGE_PAGE_GENERIC,
            .wUsage = HID_USAGE_GENERIC_Z,
            .wReportId = 1,
        },
        {
            .dwSize = sizeof(DIDEVICEOBJECTINSTANCEW),
            .guidType = GUID_XAxis,
            .dwOfs = 0x40,
            .dwType = DIDFT_ABSAXIS|DIDFT_MAKEINSTANCE(16),
            .dwFlags = DIDOI_ASPECTFORCE,
            .tszName = L"X Axis",
            .wCollectionNumber = 1,
            .dwDimension = 0xe111,
            .wUsagePage = HID_USAGE_PAGE_GENERIC,
            .wUsage = HID_USAGE_GENERIC_X,
            .wReportId = 1,
        },
        {
            .dwSize = sizeof(DIDEVICEOBJECTINSTANCEW),
            .guidType = GUID_YAxis,
            .dwOfs = 0x44,
            .dwType = DIDFT_ABSAXIS|DIDFT_MAKEINSTANCE(17),
            .dwFlags = DIDOI_ASPECTFORCE,
            .tszName = L"Y Axis",
            .wCollectionNumber = 1,
            .dwDimension = 0xe111,
            .wUsagePage = HID_USAGE_PAGE_GENERIC,
            .wUsage = HID_USAGE_GENERIC_Y,
            .wReportId = 1,
        },
        {
            .dwSize = sizeof(DIDEVICEOBJECTINSTANCEW),
            .guidType = GUID_ZAxis,
            .dwOfs = 0x48,
            .dwType = DIDFT_ABSAXIS|DIDFT_MAKEINSTANCE(18),
            .dwFlags = DIDOI_ASPECTFORCE,
            .tszName = L"Z Axis",
            .wCollectionNumber = 1,
            .dwDimension = 0xe111,
            .wUsagePage = HID_USAGE_PAGE_GENERIC,
            .wUsage = HID_USAGE_GENERIC_Z,
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
    struct check_objects_todos todo_objects[ARRAY_SIZE(expect_objects)] =
    {
        {0},
        {0},
        {0},
        {0},
        {0},
        {0},
        {0},
        {0},
        {.name = TRUE},
        {.name = TRUE, .guid = TRUE},
        {.flags = TRUE},
        {.flags = TRUE},
        {.flags = TRUE},
        {.flags = TRUE},
        {.flags = TRUE},
        {.flags = TRUE},
        {.flags = TRUE},
        {.flags = TRUE},
        {.flags = TRUE},
    };
    struct check_objects_params check_objects_params =
    {
        .version = DIRECTINPUT_VERSION,
        .expect_count = ARRAY_SIZE(expect_objects),
        .expect_objs = expect_objects,
        .todo_objs = todo_objects,
    };

    DIDEVICEOBJECTINSTANCEW objinst = {.dwSize = sizeof(DIDEVICEOBJECTINSTANCEW)};
    DIDEVICEINSTANCEW devinst = {0};
    IDirectInputDevice8W *device;
    DIDEVCAPS caps = {0};
    HRESULT hr;
    ULONG ref;

    cleanup_registry_keys();

    desc.report_descriptor_len = sizeof(report_desc);
    memcpy( desc.report_descriptor_buf, report_desc, sizeof(report_desc) );
    fill_context( __LINE__, desc.context, ARRAY_SIZE(desc.context) );

    if (!hid_device_start( &desc )) goto done;
    if (FAILED(hr = dinput_test_create_device( DIRECTINPUT_VERSION, &devinst, &device ))) goto done;

    check_dinput_devices( DIRECTINPUT_VERSION, &devinst );

    memset( &devinst, 0, sizeof(devinst) );
    devinst.dwSize = sizeof(DIDEVICEINSTANCEW);
    hr = IDirectInputDevice8_GetDeviceInfo( device, &devinst );
    ok( hr == DI_OK, "GetDeviceInfo returned %#lx\n", hr );
    check_member( devinst, expect_devinst, "%lu", dwSize );
    todo_wine
    check_member_guid( devinst, expect_devinst, guidInstance );
    check_member_guid( devinst, expect_devinst, guidProduct );
    todo_wine
    check_member( devinst, expect_devinst, "%#lx", dwDevType );
    check_member_wstr( devinst, expect_devinst, tszInstanceName );
    check_member_wstr( devinst, expect_devinst, tszProductName );
    check_member_guid( devinst, expect_devinst, guidFFDriver );
    check_member( devinst, expect_devinst, "%04x", wUsagePage );
    check_member( devinst, expect_devinst, "%04x", wUsage );

    hr = IDirectInputDevice8_GetCapabilities( device, NULL );
    ok( hr == E_POINTER, "GetCapabilities returned %#lx\n", hr );
    hr = IDirectInputDevice8_GetCapabilities( device, &caps );
    ok( hr == DIERR_INVALIDPARAM, "GetCapabilities returned %#lx\n", hr );
    caps.dwSize = sizeof(DIDEVCAPS);
    hr = IDirectInputDevice8_GetCapabilities( device, &caps );
    ok( hr == DI_OK, "GetCapabilities returned %#lx\n", hr );
    check_member( caps, expect_caps, "%lu", dwSize );
    check_member( caps, expect_caps, "%#lx", dwFlags );
    todo_wine
    check_member( caps, expect_caps, "%#lx", dwDevType );
    check_member( caps, expect_caps, "%lu", dwAxes );
    check_member( caps, expect_caps, "%lu", dwButtons );
    check_member( caps, expect_caps, "%lu", dwPOVs );
    check_member( caps, expect_caps, "%lu", dwFFSamplePeriod );
    check_member( caps, expect_caps, "%lu", dwFFMinTimeResolution );
    check_member( caps, expect_caps, "%lu", dwFirmwareRevision );
    check_member( caps, expect_caps, "%lu", dwHardwareRevision );
    check_member( caps, expect_caps, "%lu", dwFFDriverVersion );

    hr = IDirectInputDevice8_EnumObjects( device, check_objects, &check_objects_params, DIDFT_ALL );
    ok( hr == DI_OK, "EnumObjects returned %#lx\n", hr );
    ok( check_objects_params.index >= check_objects_params.expect_count, "missing %u objects\n",
        check_objects_params.expect_count - check_objects_params.index );

    hr = IDirectInputDevice8_SetDataFormat( device, &c_dfDIJoystick2 );
    ok( hr == DI_OK, "SetDataFormat returned: %#lx\n", hr );

    hr = IDirectInputDevice8_GetObjectInfo( device, &objinst, DIJOFS_RZ, DIPH_BYOFFSET );
    ok( hr == DI_OK, "GetObjectInfo returned: %#lx\n", hr );

    check_member( objinst, expect_objects[5], "%lu", dwSize );
    check_member_guid( objinst, expect_objects[5], guidType );
    check_member( objinst, expect_objects[5], "%#lx", dwOfs );
    check_member( objinst, expect_objects[5], "%#lx", dwType );
    check_member( objinst, expect_objects[5], "%#lx", dwFlags );
    if (!localized) check_member_wstr( objinst, expect_objects[5], tszName );
    check_member( objinst, expect_objects[5], "%lu", dwFFMaxForce );
    check_member( objinst, expect_objects[5], "%lu", dwFFForceResolution );
    check_member( objinst, expect_objects[5], "%u", wCollectionNumber );
    check_member( objinst, expect_objects[5], "%u", wDesignatorIndex );
    check_member( objinst, expect_objects[5], "%#04x", wUsagePage );
    check_member( objinst, expect_objects[5], "%#04x", wUsage );
    check_member( objinst, expect_objects[5], "%#lx", dwDimension );
    check_member( objinst, expect_objects[5], "%#04x", wExponent );
    check_member( objinst, expect_objects[5], "%u", wReportId );

    hr = IDirectInputDevice8_GetObjectInfo( device, &objinst, offsetof(DIJOYSTATE2, rglSlider[0]), DIPH_BYOFFSET );
    ok( hr == DI_OK, "GetObjectInfo returned: %#lx\n", hr );

    check_member( objinst, expect_objects[6], "%lu", dwSize );
    check_member_guid( objinst, expect_objects[6], guidType );
    check_member( objinst, expect_objects[6], "%#lx", dwOfs );
    check_member( objinst, expect_objects[6], "%#lx", dwType );
    check_member( objinst, expect_objects[6], "%#lx", dwFlags );
    if (!localized) check_member_wstr( objinst, expect_objects[6], tszName );
    check_member( objinst, expect_objects[6], "%lu", dwFFMaxForce );
    check_member( objinst, expect_objects[6], "%lu", dwFFForceResolution );
    check_member( objinst, expect_objects[6], "%u", wCollectionNumber );
    check_member( objinst, expect_objects[6], "%u", wDesignatorIndex );
    check_member( objinst, expect_objects[6], "%#04x", wUsagePage );
    check_member( objinst, expect_objects[6], "%#04x", wUsage );
    check_member( objinst, expect_objects[6], "%#lx", dwDimension );
    check_member( objinst, expect_objects[6], "%#04x", wExponent );
    check_member( objinst, expect_objects[6], "%u", wReportId );

    hr = IDirectInputDevice8_GetObjectInfo( device, &objinst, offsetof(DIJOYSTATE2, rglSlider[1]), DIPH_BYOFFSET );
    ok( hr == DI_OK, "GetObjectInfo returned: %#lx\n", hr );

    check_member( objinst, expect_objects[8], "%lu", dwSize );
    check_member_guid( objinst, expect_objects[8], guidType );
    check_member( objinst, expect_objects[8], "%#lx", dwOfs );
    check_member( objinst, expect_objects[8], "%#lx", dwType );
    check_member( objinst, expect_objects[8], "%#lx", dwFlags );
    if (!localized) todo_wine check_member_wstr( objinst, expect_objects[8], tszName );
    check_member( objinst, expect_objects[8], "%lu", dwFFMaxForce );
    check_member( objinst, expect_objects[8], "%lu", dwFFForceResolution );
    check_member( objinst, expect_objects[8], "%u", wCollectionNumber );
    check_member( objinst, expect_objects[8], "%u", wDesignatorIndex );
    check_member( objinst, expect_objects[8], "%#04x", wUsagePage );
    check_member( objinst, expect_objects[8], "%#04x", wUsage );
    check_member( objinst, expect_objects[8], "%#lx", dwDimension );
    check_member( objinst, expect_objects[8], "%#04x", wExponent );
    check_member( objinst, expect_objects[8], "%u", wReportId );

    hr = IDirectInputDevice8_GetObjectInfo( device, &objinst, offsetof(DIJOYSTATE2, lVX), DIPH_BYOFFSET );
    ok( hr == DI_OK, "GetObjectInfo returned: %#lx\n", hr );

    check_member( objinst, expect_objects[10], "%lu", dwSize );
    check_member_guid( objinst, expect_objects[10], guidType );
    check_member( objinst, expect_objects[10], "%#lx", dwOfs );
    check_member( objinst, expect_objects[10], "%#lx", dwType );
    todo_wine
    check_member( objinst, expect_objects[10], "%#lx", dwFlags );
    if (!localized) check_member_wstr( objinst, expect_objects[10], tszName );
    check_member( objinst, expect_objects[10], "%lu", dwFFMaxForce );
    check_member( objinst, expect_objects[10], "%lu", dwFFForceResolution );
    check_member( objinst, expect_objects[10], "%u", wCollectionNumber );
    check_member( objinst, expect_objects[10], "%u", wDesignatorIndex );
    check_member( objinst, expect_objects[10], "%#04x", wUsagePage );
    check_member( objinst, expect_objects[10], "%#04x", wUsage );
    check_member( objinst, expect_objects[10], "%#lx", dwDimension );
    check_member( objinst, expect_objects[10], "%#04x", wExponent );
    check_member( objinst, expect_objects[10], "%u", wReportId );

    hr = IDirectInputDevice8_GetObjectInfo( device, &objinst, offsetof(DIJOYSTATE2, lAX), DIPH_BYOFFSET );
    ok( hr == DI_OK, "GetObjectInfo returned: %#lx\n", hr );

    check_member( objinst, expect_objects[13], "%lu", dwSize );
    check_member_guid( objinst, expect_objects[13], guidType );
    check_member( objinst, expect_objects[13], "%#lx", dwOfs );
    check_member( objinst, expect_objects[13], "%#lx", dwType );
    todo_wine
    check_member( objinst, expect_objects[13], "%#lx", dwFlags );
    if (!localized) check_member_wstr( objinst, expect_objects[13], tszName );
    check_member( objinst, expect_objects[13], "%lu", dwFFMaxForce );
    check_member( objinst, expect_objects[13], "%lu", dwFFForceResolution );
    check_member( objinst, expect_objects[13], "%u", wCollectionNumber );
    check_member( objinst, expect_objects[13], "%u", wDesignatorIndex );
    check_member( objinst, expect_objects[13], "%#04x", wUsagePage );
    check_member( objinst, expect_objects[13], "%#04x", wUsage );
    check_member( objinst, expect_objects[13], "%#lx", dwDimension );
    check_member( objinst, expect_objects[13], "%#04x", wExponent );
    check_member( objinst, expect_objects[13], "%u", wReportId );

    hr = IDirectInputDevice8_GetObjectInfo( device, &objinst, offsetof(DIJOYSTATE2, lFX), DIPH_BYOFFSET );
    ok( hr == DI_OK, "GetObjectInfo returned: %#lx\n", hr );

    check_member( objinst, expect_objects[16], "%lu", dwSize );
    check_member_guid( objinst, expect_objects[16], guidType );
    check_member( objinst, expect_objects[16], "%#lx", dwOfs );
    check_member( objinst, expect_objects[16], "%#lx", dwType );
    todo_wine
    check_member( objinst, expect_objects[16], "%#lx", dwFlags );
    if (!localized) check_member_wstr( objinst, expect_objects[16], tszName );
    check_member( objinst, expect_objects[16], "%lu", dwFFMaxForce );
    check_member( objinst, expect_objects[16], "%lu", dwFFForceResolution );
    check_member( objinst, expect_objects[16], "%u", wCollectionNumber );
    check_member( objinst, expect_objects[16], "%u", wDesignatorIndex );
    check_member( objinst, expect_objects[16], "%#04x", wUsagePage );
    check_member( objinst, expect_objects[16], "%#04x", wUsage );
    check_member( objinst, expect_objects[16], "%#lx", dwDimension );
    check_member( objinst, expect_objects[16], "%#04x", wExponent );
    check_member( objinst, expect_objects[16], "%u", wReportId );

    ref = IDirectInputDevice8_Release( device );
    ok( ref == 0, "Release returned %ld\n", ref );

done:
    hid_device_stop( &desc );
    cleanup_registry_keys();
    winetest_pop_context();
}

static void test_driving_wheel_axes(void)
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

                USAGE_PAGE(1, HID_USAGE_PAGE_SIMULATION),
                USAGE(1, HID_USAGE_SIMULATION_RUDDER),
                USAGE(1, HID_USAGE_SIMULATION_THROTTLE),
                USAGE(1, HID_USAGE_SIMULATION_ACCELERATOR),
                USAGE(1, HID_USAGE_SIMULATION_BRAKE),
                USAGE(1, HID_USAGE_SIMULATION_CLUTCH),
                USAGE(1, HID_USAGE_SIMULATION_STEERING),
                LOGICAL_MINIMUM(1, 0),
                LOGICAL_MAXIMUM(1, 0x7f),
                PHYSICAL_MINIMUM(1, 0),
                PHYSICAL_MAXIMUM(1, 0x7f),
                REPORT_SIZE(1, 8),
                REPORT_COUNT(1, 6),
                INPUT(1, Data|Var|Abs),
            END_COLLECTION,
        END_COLLECTION,
    };
#undef REPORT_ID_OR_USAGE_PAGE
#include "pop_hid_macros.h"

    struct hid_device_desc desc =
    {
        .use_report_id = TRUE,
        .caps = { .InputReportByteLength = 7 },
        .attributes = default_attributes,
    };
    const DIDEVCAPS expect_caps =
    {
        .dwSize = sizeof(DIDEVCAPS),
        .dwFlags = DIDC_ATTACHED | DIDC_EMULATED,
        .dwDevType = DIDEVTYPE_HID | (DI8DEVTYPEDRIVING_LIMITED << 8) | DI8DEVTYPE_DRIVING,
        .dwAxes = 6,
    };
    const DIDEVICEINSTANCEW expect_devinst =
    {
        .dwSize = sizeof(DIDEVICEINSTANCEW),
        .guidInstance = expect_guid_product,
        .guidProduct = expect_guid_product,
        .dwDevType = DIDEVTYPE_HID | (DI8DEVTYPEDRIVING_LIMITED << 8) | DI8DEVTYPE_DRIVING,
        .tszInstanceName = L"Wine Test",
        .tszProductName = L"Wine Test",
        .guidFFDriver = GUID_NULL,
        .wUsagePage = HID_USAGE_PAGE_GENERIC,
        .wUsage = HID_USAGE_GENERIC_JOYSTICK,
    };
    const DIDEVICEOBJECTINSTANCEW expect_objects[] =
    {
        {
            .dwSize = sizeof(DIDEVICEOBJECTINSTANCEW),
            .guidType = GUID_XAxis,
            .dwOfs = 0,
            .dwType = DIDFT_ABSAXIS|DIDFT_MAKEINSTANCE(0),
            .dwFlags = DIDOI_ASPECTPOSITION,
            .tszName = L"Steering",
            .wCollectionNumber = 1,
            .wUsagePage = HID_USAGE_PAGE_SIMULATION,
            .wUsage = HID_USAGE_SIMULATION_STEERING,
            .wReportId = 1,
        },
        {
            .dwSize = sizeof(DIDEVICEOBJECTINSTANCEW),
            .guidType = GUID_Unknown,
            .dwOfs = 0x4,
            .dwType = DIDFT_ABSAXIS|DIDFT_MAKEINSTANCE(6),
            .dwFlags = 0,
            .tszName = L"Clutch",
            .wCollectionNumber = 1,
            .wUsagePage = HID_USAGE_PAGE_SIMULATION,
            .wUsage = HID_USAGE_SIMULATION_CLUTCH,
            .wReportId = 1,
        },
        {
            .dwSize = sizeof(DIDEVICEOBJECTINSTANCEW),
            .guidType = GUID_RzAxis,
            .dwOfs = 0x8,
            .dwType = DIDFT_ABSAXIS|DIDFT_MAKEINSTANCE(5),
            .dwFlags = DIDOI_ASPECTPOSITION,
            .tszName = L"Brake",
            .wCollectionNumber = 1,
            .wUsagePage = HID_USAGE_PAGE_SIMULATION,
            .wUsage = HID_USAGE_SIMULATION_BRAKE,
            .wReportId = 1,
        },
        {
            .dwSize = sizeof(DIDEVICEOBJECTINSTANCEW),
            .guidType = GUID_YAxis,
            .dwOfs = 0xc,
            .dwType = DIDFT_ABSAXIS|DIDFT_MAKEINSTANCE(1),
            .dwFlags = DIDOI_ASPECTPOSITION,
            .tszName = L"Accelerator",
            .wCollectionNumber = 1,
            .wUsagePage = HID_USAGE_PAGE_SIMULATION,
            .wUsage = HID_USAGE_SIMULATION_ACCELERATOR,
            .wReportId = 1,
        },
        {
            .dwSize = sizeof(DIDEVICEOBJECTINSTANCEW),
            .guidType = GUID_Slider,
            .dwOfs = 0x10,
            .dwType = DIDFT_ABSAXIS|DIDFT_MAKEINSTANCE(2),
            .dwFlags = DIDOI_ASPECTPOSITION,
            .tszName = L"Throttle",
            .wCollectionNumber = 1,
            .wUsagePage = HID_USAGE_PAGE_SIMULATION,
            .wUsage = HID_USAGE_SIMULATION_THROTTLE,
            .wReportId = 1,
        },
        {
            .dwSize = sizeof(DIDEVICEOBJECTINSTANCEW),
            .guidType = GUID_RzAxis,
            .dwOfs = 0x14,
            .dwType = DIDFT_ABSAXIS|DIDFT_MAKEINSTANCE(7),
            .dwFlags = DIDOI_ASPECTPOSITION,
            .tszName = L"Rudder",
            .wCollectionNumber = 1,
            .wUsagePage = HID_USAGE_PAGE_SIMULATION,
            .wUsage = HID_USAGE_SIMULATION_RUDDER,
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
    struct check_objects_params check_objects_params =
    {
        .version = DIRECTINPUT_VERSION,
        .expect_count = ARRAY_SIZE(expect_objects),
        .expect_objs = expect_objects,
    };

    DIDEVICEINSTANCEW devinst = {0};
    IDirectInputDevice8W *device;
    DIDEVCAPS caps = {0};
    HRESULT hr;
    ULONG ref;

    cleanup_registry_keys();

    desc.report_descriptor_len = sizeof(report_desc);
    memcpy( desc.report_descriptor_buf, report_desc, sizeof(report_desc) );
    fill_context( __LINE__, desc.context, ARRAY_SIZE(desc.context) );

    if (!hid_device_start( &desc )) goto done;
    if (FAILED(hr = dinput_test_create_device( DIRECTINPUT_VERSION, &devinst, &device ))) goto done;

    check_dinput_devices( DIRECTINPUT_VERSION, &devinst );

    memset( &devinst, 0, sizeof(devinst) );
    devinst.dwSize = sizeof(DIDEVICEINSTANCEW);
    hr = IDirectInputDevice8_GetDeviceInfo( device, &devinst );
    ok( hr == DI_OK, "GetDeviceInfo returned %#lx\n", hr );
    check_member( devinst, expect_devinst, "%lu", dwSize );
    todo_wine
    check_member_guid( devinst, expect_devinst, guidInstance );
    check_member_guid( devinst, expect_devinst, guidProduct );
    check_member( devinst, expect_devinst, "%#lx", dwDevType );
    check_member_wstr( devinst, expect_devinst, tszInstanceName );
    check_member_wstr( devinst, expect_devinst, tszProductName );
    check_member_guid( devinst, expect_devinst, guidFFDriver );
    check_member( devinst, expect_devinst, "%04x", wUsagePage );
    check_member( devinst, expect_devinst, "%04x", wUsage );

    hr = IDirectInputDevice8_GetCapabilities( device, NULL );
    ok( hr == E_POINTER, "GetCapabilities returned %#lx\n", hr );
    hr = IDirectInputDevice8_GetCapabilities( device, &caps );
    ok( hr == DIERR_INVALIDPARAM, "GetCapabilities returned %#lx\n", hr );
    caps.dwSize = sizeof(DIDEVCAPS);
    hr = IDirectInputDevice8_GetCapabilities( device, &caps );
    ok( hr == DI_OK, "GetCapabilities returned %#lx\n", hr );
    check_member( caps, expect_caps, "%lu", dwSize );
    check_member( caps, expect_caps, "%#lx", dwFlags );
    check_member( caps, expect_caps, "%#lx", dwDevType );
    check_member( caps, expect_caps, "%lu", dwAxes );
    check_member( caps, expect_caps, "%lu", dwButtons );
    check_member( caps, expect_caps, "%lu", dwPOVs );
    check_member( caps, expect_caps, "%lu", dwFFSamplePeriod );
    check_member( caps, expect_caps, "%lu", dwFFMinTimeResolution );
    check_member( caps, expect_caps, "%lu", dwFirmwareRevision );
    check_member( caps, expect_caps, "%lu", dwHardwareRevision );
    check_member( caps, expect_caps, "%lu", dwFFDriverVersion );

    hr = IDirectInputDevice8_EnumObjects( device, check_objects, &check_objects_params, DIDFT_ALL );
    ok( hr == DI_OK, "EnumObjects returned %#lx\n", hr );
    ok( check_objects_params.index >= check_objects_params.expect_count, "missing %u objects\n",
        check_objects_params.expect_count - check_objects_params.index );

    ref = IDirectInputDevice8_Release( device );
    ok( ref == 0, "Release returned %ld\n", ref );

done:
    hid_device_stop( &desc );
    cleanup_registry_keys();
    winetest_pop_context();
}

static BOOL test_winmm_joystick(void)
{
#include "psh_hid_macros.h"
    const unsigned char report_desc[] = {
        USAGE_PAGE(1, HID_USAGE_PAGE_GENERIC),
        USAGE(1, HID_USAGE_GENERIC_JOYSTICK),
        COLLECTION(1, Application),
            USAGE(1, HID_USAGE_GENERIC_JOYSTICK),
            COLLECTION(1, Report),
                REPORT_ID(1, 1),

                USAGE(1, HID_USAGE_GENERIC_X),
                USAGE(1, HID_USAGE_GENERIC_Y),
                USAGE(1, HID_USAGE_GENERIC_Z),
                USAGE(1, HID_USAGE_GENERIC_WHEEL),
                USAGE(1, HID_USAGE_GENERIC_SLIDER),
                USAGE(1, HID_USAGE_GENERIC_RX),
                USAGE(1, HID_USAGE_GENERIC_RY),
                USAGE(1, HID_USAGE_GENERIC_RZ),
                LOGICAL_MINIMUM(1, 1),
                LOGICAL_MAXIMUM(4, 0xffff),
                PHYSICAL_MINIMUM(1, 1),
                PHYSICAL_MAXIMUM(4, 0xffff),
                REPORT_SIZE(1, 16),
                REPORT_COUNT(1, 8),
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
                USAGE_MAXIMUM(1, 4),
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
#include "pop_hid_macros.h"

    struct hid_device_desc desc =
    {
        .use_report_id = TRUE,
        .caps = { .InputReportByteLength = 18 },
        .attributes = default_attributes,
    };
    static const JOYCAPS2W expect_regcaps =
    {
        .szRegKey = L"DINPUT.DLL",
    };
    static const JOYCAPS2W expect_caps =
    {
        .wMid = 0x1209,
        .wPid = 0x0001,
        .szPname = L"Microsoft PC-joystick driver",
        .wXmax = 0xffff,
        .wYmax = 0xffff,
        .wZmax = 0xffff,
        .wNumButtons = 4,
        .wPeriodMin = 10,
        .wPeriodMax = 1000,
        .wRmax = 0xffff,
        .wUmax = 0xffff,
        .wVmax = 0xffff,
        .wCaps = JOYCAPS_HASZ | JOYCAPS_HASR | JOYCAPS_HASU | JOYCAPS_HASV | JOYCAPS_HASPOV | JOYCAPS_POV4DIR,
        .wMaxAxes = 6,
        .wNumAxes = 6,
        .wMaxButtons = 32,
        .szRegKey = L"DINPUT.DLL",
    };
    struct hid_expect injected_input[] =
    {
        {
            .code = IOCTL_HID_READ_REPORT,
            .report_buf = {1,0x00,0x00,0x00,0x08,0x00,0x10,0x00,0x18,0x00,0x20,0x00,0x28,0x00,0x30,0x00,0x38,0xf1},
        },
        {
            .code = IOCTL_HID_READ_REPORT,
            .report_buf = {1,0x00,0x38,0x00,0x30,0x00,0x28,0x00,0x20,0x00,0x18,0x00,0x10,0x00,0x08,0x00,0x00,0x63},
        },
    };
    static const JOYINFOEX expect_infoex[] =
    {
        {
            .dwSize = sizeof(JOYINFOEX), .dwFlags = 0xff,
            .dwXpos = 0x7fff, .dwYpos = 0x7fff, .dwZpos = 0x7fff, .dwRpos = 0x7fff, .dwUpos = 0x7fff, .dwVpos = 0x7fff,
            .dwButtons = 0, .dwButtonNumber = 0, .dwPOV = 0xffff,
            .dwReserved1 = 0xcdcdcdcd, .dwReserved2 = 0xcdcdcdcd,
        },
        {
            .dwSize = sizeof(JOYINFOEX), .dwFlags = 0xff,
            .dwXpos = 0, .dwYpos = 0x07ff, .dwZpos = 0x17ff, .dwRpos = 0x37ff, .dwUpos = 0x1fff, .dwVpos = 0x27ff,
            .dwButtons = 0xf, .dwButtonNumber = 0x4, .dwPOV = 0,
            .dwReserved1 = 0xcdcdcdcd, .dwReserved2 = 0xcdcdcdcd,
        },
        {
            .dwSize = sizeof(JOYINFOEX), .dwFlags = 0xff,
            .dwXpos = 0x37ff, .dwYpos = 0x2fff, .dwZpos = 0x1fff, .dwRpos = 0, .dwUpos = 0x17ff, .dwVpos = 0x0fff,
            .dwButtons = 0x6, .dwButtonNumber = 0x2, .dwPOV = 0x2328,
            .dwReserved1 = 0xcdcdcdcd, .dwReserved2 = 0xcdcdcdcd,
        },
    };
    static const JOYINFO expect_info =
    {
        .wXpos = 0x7fff,
        .wYpos = 0x7fff,
        .wZpos = 0x7fff,
        .wButtons = 0,
    };
    JOYINFOEX infoex = {.dwSize = sizeof(JOYINFOEX)};
    DIPROPGUIDANDPATH prop_guid_path =
    {
        .diph =
        {
            .dwSize = sizeof(DIPROPGUIDANDPATH),
            .dwHeaderSize = sizeof(DIPROPHEADER),
            .dwHow = DIPH_DEVICE,
        },
    };
    IDirectInputDevice8W *device = NULL;
    DIDEVICEINSTANCEW devinst = {0};
    JOYCAPS2W caps = {0};
    JOYINFO info = {0};
    HANDLE event, file;
    HRESULT hr;
    UINT ret;

    cleanup_registry_keys();

    ret = joyGetNumDevs();
    ok( ret == 16, "joyGetNumDevs returned %u\n", ret );

    ret = joyGetDevCapsW( 0, (JOYCAPSW *)&caps, sizeof(JOYCAPSW) );
    ok( ret == JOYERR_PARMS, "joyGetDevCapsW returned %u\n", ret );

    memset( &caps, 0xcd, sizeof(caps) );
    ret = joyGetDevCapsW( -1, (JOYCAPSW *)&caps, sizeof(caps) );
    ok( ret == 0, "joyGetDevCapsW returned %u\n", ret );
    check_member( caps, expect_regcaps, "%#x", wMid );
    check_member( caps, expect_regcaps, "%#x", wPid );
    check_member_wstr( caps, expect_regcaps, szPname );
    check_member( caps, expect_regcaps, "%#x", wXmin );
    check_member( caps, expect_regcaps, "%#x", wXmax );
    check_member( caps, expect_regcaps, "%#x", wYmin );
    check_member( caps, expect_regcaps, "%#x", wYmax );
    check_member( caps, expect_regcaps, "%#x", wZmin );
    check_member( caps, expect_regcaps, "%#x", wZmax );
    check_member( caps, expect_regcaps, "%#x", wNumButtons );
    check_member( caps, expect_regcaps, "%#x", wPeriodMin );
    check_member( caps, expect_regcaps, "%#x", wPeriodMax );
    check_member( caps, expect_regcaps, "%#x", wRmin );
    check_member( caps, expect_regcaps, "%#x", wRmax );
    check_member( caps, expect_regcaps, "%#x", wUmin );
    check_member( caps, expect_regcaps, "%#x", wUmax );
    check_member( caps, expect_regcaps, "%#x", wVmin );
    check_member( caps, expect_regcaps, "%#x", wVmax );
    check_member( caps, expect_regcaps, "%#x", wCaps );
    check_member( caps, expect_regcaps, "%#x", wMaxAxes );
    check_member( caps, expect_regcaps, "%#x", wNumAxes );
    check_member( caps, expect_regcaps, "%#x", wMaxButtons );
    check_member_wstr( caps, expect_regcaps, szRegKey );
    check_member_wstr( caps, expect_regcaps, szOEMVxD );
    check_member_guid( caps, expect_regcaps, ManufacturerGuid );
    check_member_guid( caps, expect_regcaps, ProductGuid );
    check_member_guid( caps, expect_regcaps, NameGuid );

    desc.report_descriptor_len = sizeof(report_desc);
    memcpy( desc.report_descriptor_buf, report_desc, sizeof(report_desc) );
    fill_context( __LINE__, desc.context, ARRAY_SIZE(desc.context) );

    if (!hid_device_start( &desc )) goto done;

    ret = joyGetNumDevs();
    ok( ret == 16, "joyGetNumDevs returned %u\n", ret );

    ret = joyGetPosEx( 1, &infoex );
    ok( ret == JOYERR_PARMS, "joyGetPosEx returned %u\n", ret );
    ret = joyGetPosEx( 0, &infoex );
    /* first call for an index sometimes fail */
    if (ret == JOYERR_PARMS) ret = joyGetPosEx( 0, &infoex );
    ok( ret == 0, "joyGetPosEx returned %u\n", ret );

    ret = joyGetDevCapsW( 1, (JOYCAPSW *)&caps, sizeof(JOYCAPSW) );
    ok( ret == JOYERR_PARMS, "joyGetDevCapsW returned %u\n", ret );

    memset( &caps, 0xcd, sizeof(caps) );
    ret = joyGetDevCapsW( 0, (JOYCAPSW *)&caps, sizeof(caps) );
    ok( ret == 0, "joyGetDevCapsW returned %u\n", ret );
    check_member( caps, expect_caps, "%#x", wMid );
    check_member( caps, expect_caps, "%#x", wPid );
    todo_wine
    check_member_wstr( caps, expect_caps, szPname );
    check_member( caps, expect_caps, "%#x", wXmin );
    check_member( caps, expect_caps, "%#x", wXmax );
    check_member( caps, expect_caps, "%#x", wYmin );
    check_member( caps, expect_caps, "%#x", wYmax );
    check_member( caps, expect_caps, "%#x", wZmin );
    check_member( caps, expect_caps, "%#x", wZmax );
    check_member( caps, expect_caps, "%#x", wNumButtons );
    check_member( caps, expect_caps, "%#x", wPeriodMin );
    check_member( caps, expect_caps, "%#x", wPeriodMax );
    check_member( caps, expect_caps, "%#x", wRmin );
    check_member( caps, expect_caps, "%#x", wRmax );
    check_member( caps, expect_caps, "%#x", wUmin );
    check_member( caps, expect_caps, "%#x", wUmax );
    check_member( caps, expect_caps, "%#x", wVmin );
    check_member( caps, expect_caps, "%#x", wVmax );
    check_member( caps, expect_caps, "%#x", wCaps );
    check_member( caps, expect_caps, "%#x", wMaxAxes );
    check_member( caps, expect_caps, "%#x", wNumAxes );
    check_member( caps, expect_caps, "%#x", wMaxButtons );
    check_member_wstr( caps, expect_caps, szRegKey );
    check_member_wstr( caps, expect_caps, szOEMVxD );
    check_member_guid( caps, expect_caps, ManufacturerGuid );
    check_member_guid( caps, expect_caps, ProductGuid );
    check_member_guid( caps, expect_caps, NameGuid );

    ret = joyGetDevCapsW( 0, (JOYCAPSW *)&caps, sizeof(JOYCAPSW) );
    ok( ret == 0, "joyGetDevCapsW returned %u\n", ret );
    ret = joyGetDevCapsW( 0, NULL, sizeof(JOYCAPSW) );
    ok( ret == MMSYSERR_INVALPARAM, "joyGetDevCapsW returned %u\n", ret );
    ret = joyGetDevCapsW( 0, (JOYCAPSW *)&caps, sizeof(JOYCAPSW) + 4 );
    ok( ret == JOYERR_PARMS, "joyGetDevCapsW returned %u\n", ret );
    ret = joyGetDevCapsW( 0, (JOYCAPSW *)&caps, sizeof(JOYCAPSW) - 4 );
    ok( ret == JOYERR_PARMS, "joyGetDevCapsW returned %u\n", ret );

    infoex.dwSize = sizeof(JOYINFOEX);
    infoex.dwFlags = JOY_RETURNALL;
    ret = joyGetPosEx( -1, &infoex );
    ok( ret == JOYERR_PARMS, "joyGetPosEx returned %u\n", ret );
    ret = joyGetPosEx( 1, &infoex );
    ok( ret == JOYERR_PARMS, "joyGetPosEx returned %u\n", ret );
    ret = joyGetPosEx( 16, &infoex );
    ok( ret == JOYERR_PARMS, "joyGetPosEx returned %u\n", ret );

    memset( &infoex, 0xcd, sizeof(infoex) );
    infoex.dwSize = sizeof(JOYINFOEX);
    infoex.dwFlags = JOY_RETURNALL;
    ret = joyGetPosEx( 0, &infoex );
    ok( ret == 0, "joyGetPosEx returned %u\n", ret );
    check_member( infoex, expect_infoex[0], "%#lx", dwSize );
    check_member( infoex, expect_infoex[0], "%#lx", dwFlags );
    check_member( infoex, expect_infoex[0], "%#lx", dwXpos );
    check_member( infoex, expect_infoex[0], "%#lx", dwYpos );
    check_member( infoex, expect_infoex[0], "%#lx", dwZpos );
    check_member( infoex, expect_infoex[0], "%#lx", dwRpos );
    check_member( infoex, expect_infoex[0], "%#lx", dwUpos );
    check_member( infoex, expect_infoex[0], "%#lx", dwVpos );
    check_member( infoex, expect_infoex[0], "%#lx", dwButtons );
    check_member( infoex, expect_infoex[0], "%#lx", dwButtonNumber );
    check_member( infoex, expect_infoex[0], "%#lx", dwPOV );
    check_member( infoex, expect_infoex[0], "%#lx", dwReserved1 );
    check_member( infoex, expect_infoex[0], "%#lx", dwReserved2 );

    infoex.dwSize = sizeof(JOYINFOEX) - 4;
    ret = joyGetPosEx( 0, &infoex );
    ok( ret == JOYERR_PARMS, "joyGetPosEx returned %u\n", ret );

    ret = joyGetPos( -1, &info );
    ok( ret == JOYERR_PARMS, "joyGetPos returned %u\n", ret );
    ret = joyGetPos( 1, &info );
    ok( ret == JOYERR_PARMS, "joyGetPos returned %u\n", ret );
    memset( &info, 0xcd, sizeof(info) );
    ret = joyGetPos( 0, &info );
    ok( ret == 0, "joyGetPos returned %u\n", ret );
    check_member( info, expect_info, "%#x", wXpos );
    check_member( info, expect_info, "%#x", wYpos );
    check_member( info, expect_info, "%#x", wZpos );
    check_member( info, expect_info, "%#x", wButtons );

    if (FAILED(hr = dinput_test_create_device( DIRECTINPUT_VERSION, &devinst, &device ))) goto done;

    event = CreateEventW( NULL, FALSE, FALSE, NULL );
    ok( event != NULL, "CreateEventW failed, last error %lu\n", GetLastError() );
    hr = IDirectInputDevice8_SetEventNotification( device, event );
    ok( hr == DI_OK, "SetEventNotification returned: %#lx\n", hr );

    hr = IDirectInputDevice8_GetProperty( device, DIPROP_GUIDANDPATH, &prop_guid_path.diph );
    ok( hr == DI_OK, "GetProperty DIPROP_GUIDANDPATH returned %#lx\n", hr );
    file = CreateFileW( prop_guid_path.wszPath, FILE_READ_ACCESS | FILE_WRITE_ACCESS,
                        FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
                        FILE_FLAG_OVERLAPPED | FILE_FLAG_NO_BUFFERING, NULL );
    ok( file != INVALID_HANDLE_VALUE, "got error %lu\n", GetLastError() );

    hr = IDirectInputDevice8_SetDataFormat( device, &c_dfDIJoystick2 );
    ok( hr == DI_OK, "SetDataFormat returned: %#lx\n", hr );
    hr = IDirectInputDevice8_SetCooperativeLevel( device, NULL, DISCL_BACKGROUND | DISCL_NONEXCLUSIVE );
    ok( hr == DI_OK, "SetCooperativeLevel returned: %#lx\n", hr );
    hr = IDirectInputDevice8_Acquire( device );
    ok( hr == DI_OK, "Acquire returned: %#lx\n", hr );

    send_hid_input( file, &injected_input[0], sizeof(struct hid_expect) );
    ret = WaitForSingleObject( event, 100 );
    ok( ret != WAIT_TIMEOUT, "WaitForSingleObject returned %#x\n", ret );
    Sleep( 50 ); /* leave some time for winmm to keep up */

    memset( &infoex, 0xcd, sizeof(infoex) );
    infoex.dwSize = sizeof(JOYINFOEX);
    infoex.dwFlags = JOY_RETURNALL;
    ret = joyGetPosEx( 0, &infoex );
    ok( ret == 0, "joyGetPosEx returned %u\n", ret );
    check_member( infoex, expect_infoex[1], "%#lx", dwSize );
    check_member( infoex, expect_infoex[1], "%#lx", dwFlags );
    check_member( infoex, expect_infoex[1], "%#lx", dwXpos );
    check_member( infoex, expect_infoex[1], "%#lx", dwYpos );
    check_member( infoex, expect_infoex[1], "%#lx", dwZpos );
    check_member( infoex, expect_infoex[1], "%#lx", dwRpos );
    check_member( infoex, expect_infoex[1], "%#lx", dwUpos );
    check_member( infoex, expect_infoex[1], "%#lx", dwVpos );
    check_member( infoex, expect_infoex[1], "%#lx", dwButtons );
    check_member( infoex, expect_infoex[1], "%#lx", dwButtonNumber );
    check_member( infoex, expect_infoex[1], "%#lx", dwPOV );

    send_hid_input( file, &injected_input[1], sizeof(struct hid_expect) );
    ret = WaitForSingleObject( event, 100 );
    ok( ret != WAIT_TIMEOUT, "WaitForSingleObject returned %#x\n", ret );
    Sleep( 50 ); /* leave some time for winmm to keep up */

    memset( &infoex, 0xcd, sizeof(infoex) );
    infoex.dwSize = sizeof(JOYINFOEX);
    infoex.dwFlags = JOY_RETURNALL;
    ret = joyGetPosEx( 0, &infoex );
    ok( ret == 0, "joyGetPosEx returned %u\n", ret );
    check_member( infoex, expect_infoex[2], "%#lx", dwSize );
    check_member( infoex, expect_infoex[2], "%#lx", dwFlags );
    check_member( infoex, expect_infoex[2], "%#lx", dwXpos );
    check_member( infoex, expect_infoex[2], "%#lx", dwYpos );
    check_member( infoex, expect_infoex[2], "%#lx", dwZpos );
    check_member( infoex, expect_infoex[2], "%#lx", dwRpos );
    check_member( infoex, expect_infoex[2], "%#lx", dwUpos );
    check_member( infoex, expect_infoex[2], "%#lx", dwVpos );
    check_member( infoex, expect_infoex[2], "%#lx", dwButtons );
    check_member( infoex, expect_infoex[2], "%#lx", dwButtonNumber );
    check_member( infoex, expect_infoex[2], "%#lx", dwPOV );

    ret = IDirectInputDevice8_Release( device );
    ok( ret == 0, "Release returned %d\n", ret );

    CloseHandle( event );
    CloseHandle( file );

done:
    hid_device_stop( &desc );
    cleanup_registry_keys();

    return device != NULL;
}

#define check_interface( a, b, c ) check_interface_( __LINE__, a, b, c )
static void check_interface_( unsigned int line, void *iface_ptr, REFIID iid, BOOL supported )
{
    IUnknown *iface = iface_ptr;
    HRESULT hr, expected;
    IUnknown *unk;

    expected = supported ? S_OK : E_NOINTERFACE;
    hr = IUnknown_QueryInterface( iface, iid, (void **)&unk );
    ok_ (__FILE__, line)( hr == expected, "got hr %#lx, expected %#lx.\n", hr, expected );
    if (SUCCEEDED(hr)) IUnknown_Release( unk );
}

#define check_runtimeclass( a, b ) check_runtimeclass_( __LINE__, (IInspectable *)a, b )
static void check_runtimeclass_( int line, IInspectable *inspectable, const WCHAR *class_name )
{
    const WCHAR *buffer;
    UINT32 length;
    HSTRING str;
    HRESULT hr;

    hr = IInspectable_GetRuntimeClassName( inspectable, &str );
    ok_ (__FILE__, line)( hr == S_OK, "GetRuntimeClassName returned %#lx\n", hr );
    buffer = pWindowsGetStringRawBuffer( str, &length );
    ok_ (__FILE__, line)( !wcscmp( buffer, class_name ), "got class name %s\n", debugstr_w(buffer) );
    pWindowsDeleteString( str );
}

struct controller_handler
{
    IEventHandler_RawGameController IEventHandler_RawGameController_iface;
    HANDLE event;
    BOOL invoked;
};

static inline struct controller_handler *impl_from_IEventHandler_RawGameController( IEventHandler_RawGameController *iface )
{
    return CONTAINING_RECORD( iface, struct controller_handler, IEventHandler_RawGameController_iface );
}

static HRESULT WINAPI controller_handler_QueryInterface( IEventHandler_RawGameController *iface, REFIID iid, void **out )
{
    if (IsEqualGUID( iid, &IID_IUnknown ) ||
        IsEqualGUID( iid, &IID_IAgileObject ) ||
        IsEqualGUID( iid, &IID_IEventHandler_RawGameController ))
    {
        IUnknown_AddRef( iface );
        *out = iface;
        return S_OK;
    }

    trace( "%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid( iid ) );
    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI controller_handler_AddRef( IEventHandler_RawGameController *iface )
{
    return 2;
}

static ULONG WINAPI controller_handler_Release( IEventHandler_RawGameController *iface )
{
    return 1;
}

static HRESULT WINAPI controller_handler_Invoke( IEventHandler_RawGameController *iface,
                                                 IInspectable *sender, IRawGameController *controller )
{
    struct controller_handler *impl = impl_from_IEventHandler_RawGameController( iface );

    trace( "iface %p, sender %p, controller %p\n", iface, sender, controller );

    ok( sender == NULL, "got sender %p\n", sender );
    SetEvent( impl->event );
    impl->invoked = TRUE;

    return S_OK;
}

static const IEventHandler_RawGameControllerVtbl controller_handler_vtbl =
{
    controller_handler_QueryInterface,
    controller_handler_AddRef,
    controller_handler_Release,
    controller_handler_Invoke,
};

static struct controller_handler controller_added = {{&controller_handler_vtbl}};

static void test_windows_gaming_input(void)
{
#include "psh_hid_macros.h"
    const unsigned char report_desc[] = {
        USAGE_PAGE(1, HID_USAGE_PAGE_GENERIC),
        USAGE(1, HID_USAGE_GENERIC_GAMEPAD),
        COLLECTION(1, Application),
            USAGE(1, HID_USAGE_GENERIC_GAMEPAD),
            COLLECTION(1, Physical),
                USAGE(1, HID_USAGE_GENERIC_X),
                USAGE(1, HID_USAGE_GENERIC_Y),
                USAGE(1, HID_USAGE_GENERIC_RX),
                USAGE(1, HID_USAGE_GENERIC_RY),
                USAGE(1, HID_USAGE_GENERIC_Z),
                USAGE(1, HID_USAGE_GENERIC_RZ),
                LOGICAL_MINIMUM(1, 0),
                LOGICAL_MAXIMUM(1, 127),
                PHYSICAL_MINIMUM(1, 0),
                PHYSICAL_MAXIMUM(1, 127),
                REPORT_SIZE(1, 8),
                REPORT_COUNT(1, 6),
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
                USAGE_MAXIMUM(1, 12),
                LOGICAL_MINIMUM(1, 0),
                LOGICAL_MAXIMUM(1, 1),
                PHYSICAL_MINIMUM(1, 0),
                PHYSICAL_MAXIMUM(1, 1),
                REPORT_SIZE(1, 1),
                REPORT_COUNT(1, 12),
                INPUT(1, Data|Var|Abs),
            END_COLLECTION,
        END_COLLECTION,
    };
#include "pop_hid_macros.h"

    struct hid_device_desc desc =
    {
        .use_report_id = TRUE,
        .caps = { .InputReportByteLength = 8 },
        .attributes = default_attributes,
    };
    static const WCHAR *controller_class_name = RuntimeClass_Windows_Gaming_Input_RawGameController;
    static const WCHAR *gamepad_class_name = RuntimeClass_Windows_Gaming_Input_Gamepad;

    IRawGameController *raw_controller, *tmp_raw_controller;
    IVectorView_RawGameController *controllers_view;
    IRawGameControllerStatics *controller_statics;
    EventRegistrationToken controller_added_token;
    IVectorView_Gamepad *gamepads_view;
    IGamepadStatics *gamepad_statics;
    IGameController *game_controller;
    UINT32 size;
    HSTRING str;
    HRESULT hr;

    if (!load_combase_functions()) return;

    cleanup_registry_keys();

    hr = pRoInitialize( RO_INIT_MULTITHREADED );
    ok( hr == RPC_E_CHANGED_MODE, "RoInitialize returned %#lx\n", hr );

    hr = pWindowsCreateString( controller_class_name, wcslen( controller_class_name ), &str );
    ok( hr == S_OK, "WindowsCreateString returned %#lx\n", hr );
    hr = pRoGetActivationFactory( str, &IID_IRawGameControllerStatics, (void **)&controller_statics );
    ok( hr == S_OK || broken( hr == REGDB_E_CLASSNOTREG ), "RoGetActivationFactory returned %#lx\n", hr );
    pWindowsDeleteString( str );

    if (hr == REGDB_E_CLASSNOTREG)
    {
        win_skip( "%s runtimeclass not registered, skipping tests.\n", wine_dbgstr_w( controller_class_name ) );
        goto done;
    }

    hr = IRawGameControllerStatics_get_RawGameControllers( controller_statics, &controllers_view );
    ok( hr == S_OK, "get_RawGameControllers returned %#lx\n", hr );
    hr = IVectorView_RawGameController_get_Size( controllers_view, &size );
    ok( hr == S_OK, "get_Size returned %#lx\n", hr );
    ok( size == 0, "got size %u\n", size );

    controller_added.event = CreateEventW( NULL, FALSE, FALSE, NULL );
    ok( !!controller_added.event, "CreateEventW failed, error %lu\n", GetLastError() );

    hr = IRawGameControllerStatics_add_RawGameControllerAdded( controller_statics, &controller_added.IEventHandler_RawGameController_iface,
                                                               &controller_added_token );
    ok( hr == S_OK, "add_RawGameControllerAdded returned %#lx\n", hr );
    ok( controller_added_token.value, "got token %I64u\n", controller_added_token.value );

    desc.report_descriptor_len = sizeof(report_desc);
    memcpy( desc.report_descriptor_buf, report_desc, sizeof(report_desc) );
    fill_context( __LINE__, desc.context, ARRAY_SIZE(desc.context) );

    if (!hid_device_start( &desc )) goto done;
    WaitForSingleObject( controller_added.event, INFINITE );
    CloseHandle( controller_added.event );

    hr = IVectorView_RawGameController_get_Size( controllers_view, &size );
    ok( hr == S_OK, "get_Size returned %#lx\n", hr );
    ok( size == 0, "got size %u\n", size );
    IVectorView_RawGameController_Release( controllers_view );

    hr = IRawGameControllerStatics_get_RawGameControllers( controller_statics, &controllers_view );
    ok( hr == S_OK, "get_RawGameControllers returned %#lx\n", hr );
    hr = IVectorView_RawGameController_get_Size( controllers_view, &size );
    ok( hr == S_OK, "get_Size returned %#lx\n", hr );
    ok( size == 1, "got size %u\n", size );
    hr = IVectorView_RawGameController_GetAt( controllers_view, 0, &raw_controller );
    ok( hr == S_OK, "GetAt returned %#lx\n", hr );
    IVectorView_RawGameController_Release( controllers_view );

    /* HID gamepads aren't exposed as WGI gamepads on Windows */

    hr = pWindowsCreateString( gamepad_class_name, wcslen( gamepad_class_name ), &str );
    ok( hr == S_OK, "WindowsCreateString returned %#lx\n", hr );
    hr = pRoGetActivationFactory( str, &IID_IGamepadStatics, (void **)&gamepad_statics );
    ok( hr == S_OK, "RoGetActivationFactory returned %#lx\n", hr );
    pWindowsDeleteString( str );
    hr = IGamepadStatics_get_Gamepads( gamepad_statics, &gamepads_view );
    ok( hr == S_OK, "get_Gamepads returned %#lx\n", hr );
    hr = IVectorView_Gamepad_get_Size( gamepads_view, &size );
    ok( hr == S_OK, "get_Size returned %#lx\n", hr );
    todo_wine /* but Wine currently intentionally does */
    ok( size == 0, "got size %u\n", size );
    IVectorView_Gamepad_Release( gamepads_view );
    IGamepadStatics_Release( gamepad_statics );

    check_runtimeclass( raw_controller, RuntimeClass_Windows_Gaming_Input_RawGameController );
    check_interface( raw_controller, &IID_IUnknown, TRUE );
    check_interface( raw_controller, &IID_IInspectable, TRUE );
    check_interface( raw_controller, &IID_IAgileObject, TRUE );
    check_interface( raw_controller, &IID_IRawGameController, TRUE );
    todo_wine
    check_interface( raw_controller, &IID_IRawGameController2, TRUE );
    check_interface( raw_controller, &IID_IGameController, TRUE );
    check_interface( raw_controller, &IID_IGamepad, FALSE );

    hr = IRawGameController_QueryInterface( raw_controller, &IID_IGameController, (void **)&game_controller );
    ok( hr == S_OK, "QueryInterface returned %#lx\n", hr );

    check_runtimeclass( game_controller, RuntimeClass_Windows_Gaming_Input_RawGameController );
    check_interface( game_controller, &IID_IUnknown, TRUE );
    check_interface( game_controller, &IID_IInspectable, TRUE );
    check_interface( game_controller, &IID_IAgileObject, TRUE );
    check_interface( game_controller, &IID_IRawGameController, TRUE );
    todo_wine
    check_interface( game_controller, &IID_IRawGameController2, TRUE );
    check_interface( game_controller, &IID_IGameController, TRUE );
    check_interface( game_controller, &IID_IGamepad, FALSE );

    hr = IRawGameControllerStatics_FromGameController( controller_statics, game_controller, &tmp_raw_controller );
    ok( hr == S_OK, "FromGameController returned %#lx\n", hr );
    ok( tmp_raw_controller == raw_controller, "got unexpected IGameController interface\n" );
    IRawGameController_Release( tmp_raw_controller );

    IGameController_Release( game_controller );
    IRawGameController_Release( raw_controller );

    hr = IRawGameControllerStatics_remove_RawGameControllerAdded( controller_statics, controller_added_token );
    ok( hr == S_OK, "remove_RawGameControllerAdded returned %#lx\n", hr );

    IRawGameControllerStatics_Release( controller_statics );

done:
    hid_device_stop( &desc );
    cleanup_registry_keys();
}

START_TEST( joystick8 )
{
    if (!dinput_test_init()) return;
    if (!bus_device_start()) goto done;

    CoInitialize( NULL );
    if (test_device_types( 0x800 ))
    {
        /* This needs to be done before doing anything involving dinput.dll
         * on Windows, or the tests will fail, dinput8.dll is fine though. */
        test_winmm_joystick();

        test_device_types( 0x500 );
        test_device_types( 0x700 );

        test_simple_joystick( 0x500 );
        test_simple_joystick( 0x700 );
        test_simple_joystick( 0x800 );

        test_many_axes_joystick();
        test_driving_wheel_axes();
        test_windows_gaming_input();
    }
    CoUninitialize();

done:
    bus_device_stop();
    dinput_test_exit();
}
