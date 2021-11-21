/*
 * Copyright (c) 2011 Lucas Fialho Zawacki
 * Copyright (c) 2006 Vitaliy Margolen
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
#include "hidusage.h"

#include "wine/test.h"

static HINSTANCE instance;
static BOOL localized; /* object names get translated */

struct enum_data {
    IDirectInput8A *pDI;
    DIACTIONFORMATA *lpdiaf;
    IDirectInputDevice8A *keyboard;
    IDirectInputDevice8A *mouse;
    const char* username;
    int ndevices;
};

/* Dummy GUID */
static const GUID ACTION_MAPPING_GUID = { 0x1, 0x2, 0x3, { 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xa, 0xb } };

enum {
    DITEST_AXIS,
    DITEST_BUTTON,
    DITEST_KEYBOARDSPACE,
    DITEST_MOUSEBUTTON0,
    DITEST_YAXIS
};

static DIACTIONA actionMapping[]=
{
  /* axis */
  { 0, 0x01008A01 /* DIAXIS_DRIVINGR_STEER */,      0, { "Steer.\0" }   },
  /* button */
  { 1, 0x01000C01 /* DIBUTTON_DRIVINGR_SHIFTUP */,  0, { "Upshift.\0" } },
  /* keyboard key */
  { 2, DIKEYBOARD_SPACE,                            0, { "Missile.\0" } },
  /* mouse button */
  { 3, DIMOUSE_BUTTON0,                             0, { "Select\0" }   },
  /* mouse axis */
  { 4, DIMOUSE_YAXIS,                               0, { "Y Axis\0" }   }
};
/* By placing the memory pointed to by lptszActionName right before memory with PAGE_NOACCESS
 * one can find out that the regular ansi string termination is not respected by EnumDevicesBySemantics.
 * Adding a double termination, making it a valid wide string termination, made the test succeed.
 * Therefore it looks like ansi version of EnumDevicesBySemantics forwards the string to
 * the wide variant without conversation. */

static void flush_events(void)
{
    int diff = 200;
    int min_timeout = 100;
    DWORD time = GetTickCount() + diff;

    while (diff > 0)
    {
        if (MsgWaitForMultipleObjects(0, NULL, FALSE, min_timeout, QS_ALLINPUT) == WAIT_TIMEOUT)
            break;
        diff = time - GetTickCount();
        min_timeout = 50;
    }
}

static void test_device_input(IDirectInputDevice8A *lpdid, DWORD event_type, DWORD event, UINT_PTR expected)
{
    HRESULT hr;
    DIDEVICEOBJECTDATA obj_data;
    DWORD data_size = 1;
    int i;

    hr = IDirectInputDevice8_Acquire(lpdid);
    ok (SUCCEEDED(hr), "Failed to acquire device hr=%08x\n", hr);

    if (event_type == INPUT_KEYBOARD)
        keybd_event(0, event, KEYEVENTF_SCANCODE, 0);

    if (event_type == INPUT_MOUSE)
        mouse_event( event, 0, 0, 0, 0);

    flush_events();
    IDirectInputDevice8_Poll(lpdid);
    hr = IDirectInputDevice8_GetDeviceData(lpdid, sizeof(obj_data), &obj_data, &data_size, 0);

    if (data_size != 1)
    {
        win_skip("We're not able to inject input into Windows dinput8 with events\n");
        IDirectInputDevice_Unacquire(lpdid);
        return;
    }

    ok (obj_data.uAppData == expected, "Retrieval of action failed uAppData=%lu expected=%lu\n", obj_data.uAppData, expected);

    /* Check for buffer overflow */
    for (i = 0; i < 17; i++)
        if (event_type == INPUT_KEYBOARD)
        {
            keybd_event( VK_SPACE, DIK_SPACE, 0, 0);
            keybd_event( VK_SPACE, DIK_SPACE, KEYEVENTF_KEYUP, 0);
        }
        else if (event_type == INPUT_MOUSE)
        {
            mouse_event(MOUSEEVENTF_LEFTDOWN, 1, 1, 0, 0);
            mouse_event(MOUSEEVENTF_LEFTUP, 1, 1, 0, 0);
        }

    flush_events();
    IDirectInputDevice8_Poll(lpdid);

    data_size = 1;
    hr = IDirectInputDevice8_GetDeviceData(lpdid, sizeof(obj_data), &obj_data, &data_size, 0);
    ok(hr == DI_BUFFEROVERFLOW, "GetDeviceData() failed: %08x\n", hr);
    data_size = 1;
    hr = IDirectInputDevice8_GetDeviceData(lpdid, sizeof(obj_data), &obj_data, &data_size, 0);
    ok(hr == DI_OK && data_size == 1, "GetDeviceData() failed: %08x cnt:%d\n", hr, data_size);

    /* drain device's queue */
    while (data_size == 1)
    {
        hr = IDirectInputDevice8_GetDeviceData(lpdid, sizeof(obj_data), &obj_data, &data_size, 0);
        ok(hr == DI_OK, "GetDeviceData() failed: %08x cnt:%d\n", hr, data_size);
        if (hr != DI_OK) break;
    }

    IDirectInputDevice_Unacquire(lpdid);
}

static void test_build_action_map(IDirectInputDevice8A *lpdid, DIACTIONFORMATA *lpdiaf,
                                  int action_index, DWORD expected_type, DWORD expected_inst)
{
    HRESULT hr;
    DIACTIONA *actions;
    DWORD instance, type, how;
    GUID assigned_to;
    DIDEVICEINSTANCEA ddi;

    ddi.dwSize = sizeof(ddi);
    IDirectInputDevice_GetDeviceInfo(lpdid, &ddi);

    hr = IDirectInputDevice8_BuildActionMap(lpdid, lpdiaf, NULL, DIDBAM_HWDEFAULTS);
    ok (SUCCEEDED(hr), "BuildActionMap failed hr=%08x\n", hr);

    actions = lpdiaf->rgoAction;
    instance = DIDFT_GETINSTANCE(actions[action_index].dwObjID);
    type = DIDFT_GETTYPE(actions[action_index].dwObjID);
    how = actions[action_index].dwHow;
    assigned_to = actions[action_index].guidInstance;

    ok (how == DIAH_USERCONFIG || how == DIAH_DEFAULT, "Action was not set dwHow=%08x\n", how);
    ok (instance == expected_inst, "Action not mapped correctly instance=%08x expected=%08x\n", instance, expected_inst);
    ok (type == expected_type, "Action type not mapped correctly type=%08x expected=%08x\n", type, expected_type);
    ok (IsEqualGUID(&assigned_to, &ddi.guidInstance), "Action and device GUID do not match action=%d\n", action_index);
}

static BOOL CALLBACK enumeration_callback(const DIDEVICEINSTANCEA *lpddi, IDirectInputDevice8A *lpdid,
                                          DWORD dwFlags, DWORD dwRemaining, LPVOID pvRef)
{
    HRESULT hr;
    DIPROPDWORD dp;
    DIPROPRANGE dpr;
    DIPROPSTRING dps;
    WCHAR usernameW[MAX_PATH];
    DWORD username_size = MAX_PATH;
    struct enum_data *data = pvRef;
    DWORD cnt;
    DIDEVICEOBJECTDATA buffer[5];
    IDirectInputDevice8A *lpdid2;

    if (!data) return DIENUM_CONTINUE;

    data->ndevices++;

    /* Convert username to WCHAR */
    if (data->username != NULL)
    {
        username_size = MultiByteToWideChar(CP_ACP, 0, data->username, -1, usernameW, 0);
        MultiByteToWideChar(CP_ACP, 0, data->username, -1, usernameW, username_size);
    }
    else
        GetUserNameW(usernameW, &username_size);

    /* collect the mouse and keyboard */
    if (IsEqualGUID(&lpddi->guidInstance, &GUID_SysKeyboard))
    {
        IDirectInputDevice_AddRef(lpdid);
        data->keyboard = lpdid;

        ok (dwFlags & DIEDBS_MAPPEDPRI1, "Keyboard should be mapped as pri1 dwFlags=%08x\n", dwFlags);
    }

    if (IsEqualGUID(&lpddi->guidInstance, &GUID_SysMouse))
    {
        IDirectInputDevice_AddRef(lpdid);
        data->mouse = lpdid;

        ok (dwFlags & DIEDBS_MAPPEDPRI1, "Mouse should be mapped as pri1 dwFlags=%08x\n", dwFlags);
    }

    /* Creating second device object to check if it has the same username */
    hr = IDirectInput_CreateDevice(data->pDI, &lpddi->guidInstance, &lpdid2, NULL);
    ok(SUCCEEDED(hr), "IDirectInput_CreateDevice() failed: %08x\n", hr);

    /* Building and setting an action map */
    /* It should not use any pre-stored mappings so we use DIDBAM_HWDEFAULTS */
    hr = IDirectInputDevice8_BuildActionMap(lpdid, data->lpdiaf, NULL, DIDBAM_HWDEFAULTS);
    ok (SUCCEEDED(hr), "BuildActionMap failed hr=%08x\n", hr);

    /* Device has no data format and thus can't be acquired */
    hr = IDirectInputDevice8_Acquire(lpdid);
    ok (hr == DIERR_INVALIDPARAM, "Device was acquired before SetActionMap hr=%08x\n", hr);

    hr = IDirectInputDevice8_SetActionMap(lpdid, data->lpdiaf, data->username, 0);
    ok (SUCCEEDED(hr), "SetActionMap failed hr=%08x\n", hr);

    /* Some joysticks may have no suitable actions and thus should not be tested */
    if (hr == DI_NOEFFECT) return DIENUM_CONTINUE;

    /* Test username after SetActionMap */
    dps.diph.dwSize = sizeof(dps);
    dps.diph.dwHeaderSize = sizeof(DIPROPHEADER);
    dps.diph.dwObj = 0;
    dps.diph.dwHow  = DIPH_DEVICE;
    dps.wsz[0] = '\0';

    hr = IDirectInputDevice_GetProperty(lpdid, DIPROP_USERNAME, &dps.diph);
    ok (SUCCEEDED(hr), "GetProperty failed hr=%08x\n", hr);
    ok (!lstrcmpW(usernameW, dps.wsz), "Username not set correctly expected=%s, got=%s\n", wine_dbgstr_w(usernameW), wine_dbgstr_w(dps.wsz));

    dps.wsz[0] = '\0';
    hr = IDirectInputDevice_GetProperty(lpdid2, DIPROP_USERNAME, &dps.diph);
    ok (SUCCEEDED(hr), "GetProperty failed hr=%08x\n", hr);
    ok (!lstrcmpW(usernameW, dps.wsz), "Username not set correctly expected=%s, got=%s\n", wine_dbgstr_w(usernameW), wine_dbgstr_w(dps.wsz));

    /* Test buffer size */
    memset(&dp, 0, sizeof(dp));
    dp.diph.dwSize = sizeof(dp);
    dp.diph.dwHeaderSize = sizeof(DIPROPHEADER);
    dp.diph.dwHow  = DIPH_DEVICE;

    hr = IDirectInputDevice_GetProperty(lpdid, DIPROP_BUFFERSIZE, &dp.diph);
    ok (SUCCEEDED(hr), "GetProperty failed hr=%08x\n", hr);
    ok (dp.dwData == data->lpdiaf->dwBufferSize, "SetActionMap must set the buffer, buffersize=%d\n", dp.dwData);

    cnt = 1;
    hr = IDirectInputDevice_GetDeviceData(lpdid, sizeof(buffer[0]), buffer, &cnt, 0);
    ok(hr == DIERR_NOTACQUIRED, "GetDeviceData() failed hr=%08x\n", hr);

    /* Test axis range */
    memset(&dpr, 0, sizeof(dpr));
    dpr.diph.dwSize = sizeof(dpr);
    dpr.diph.dwHeaderSize = sizeof(DIPROPHEADER);
    dpr.diph.dwHow  = DIPH_DEVICE;

    hr = IDirectInputDevice_GetProperty(lpdid, DIPROP_RANGE, &dpr.diph);
    /* Only test if device supports the range property */
    if (SUCCEEDED(hr))
    {
        ok (dpr.lMin == data->lpdiaf->lAxisMin, "SetActionMap must set the min axis range expected=%d got=%d\n", data->lpdiaf->lAxisMin, dpr.lMin);
        ok (dpr.lMax == data->lpdiaf->lAxisMax, "SetActionMap must set the max axis range expected=%d got=%d\n", data->lpdiaf->lAxisMax, dpr.lMax);
    }

    /* SetActionMap has set the data format so now it should work */
    hr = IDirectInputDevice8_Acquire(lpdid);
    ok (SUCCEEDED(hr), "Acquire failed hr=%08x\n", hr);

    cnt = 1;
    hr = IDirectInputDevice_GetDeviceData(lpdid, sizeof(buffer[0]), buffer, &cnt, 0);
    ok(hr == DI_OK, "GetDeviceData() failed hr=%08x\n", hr);

    /* SetActionMap should not work on an acquired device */
    hr = IDirectInputDevice8_SetActionMap(lpdid, data->lpdiaf, NULL, 0);
    ok (hr == DIERR_ACQUIRED, "SetActionMap succeeded with an acquired device hr=%08x\n", hr);

    IDirectInputDevice_Release(lpdid2);

    return DIENUM_CONTINUE;
}

static void test_appdata_property_vs_map(struct enum_data *data)
{
    HRESULT hr;
    DIPROPPOINTER dp;

    dp.diph.dwSize = sizeof(dp);
    dp.diph.dwHeaderSize = sizeof(DIPROPHEADER);
    dp.diph.dwHow = DIPH_BYID;
    dp.diph.dwObj = DIDFT_MAKEINSTANCE(DIK_SPACE) | DIDFT_PSHBUTTON;
    dp.uData = 10;
    hr = IDirectInputDevice8_SetProperty(data->keyboard, DIPROP_APPDATA, &(dp.diph));
    ok(SUCCEEDED(hr), "IDirectInputDevice8_SetProperty failed hr=%08x\n", hr);

    test_device_input(data->keyboard, INPUT_KEYBOARD, DIK_SPACE, 10);

    dp.diph.dwHow = DIPH_BYID;
    dp.diph.dwObj = DIDFT_MAKEINSTANCE(DIK_V) | DIDFT_PSHBUTTON;
    dp.uData = 11;
    hr = IDirectInputDevice8_SetProperty(data->keyboard, DIPROP_APPDATA, &(dp.diph));
    ok(hr == DIERR_OBJECTNOTFOUND, "IDirectInputDevice8_SetProperty should not find key that's not in the action map hr=%08x\n", hr);

    /* setting format should reset action map */
    hr = IDirectInputDevice8_SetDataFormat(data->keyboard, &c_dfDIKeyboard);
    ok(SUCCEEDED(hr), "SetDataFormat failed: %08x\n", hr);

    test_device_input(data->keyboard, INPUT_KEYBOARD, DIK_SPACE, -1);

    dp.diph.dwHow = DIPH_BYID;
    dp.diph.dwObj = DIDFT_MAKEINSTANCE(DIK_V) | DIDFT_PSHBUTTON;
    dp.uData = 11;
    hr = IDirectInputDevice8_SetProperty(data->keyboard, DIPROP_APPDATA, &(dp.diph));
    ok(SUCCEEDED(hr), "IDirectInputDevice8_SetProperty failed hr=%08x\n", hr);

    test_device_input(data->keyboard, INPUT_KEYBOARD, DIK_V, 11);

    /* back to action map */
    hr = IDirectInputDevice8_SetActionMap(data->keyboard, data->lpdiaf, NULL, 0);
    ok(SUCCEEDED(hr), "SetActionMap failed hr=%08x\n", hr);

    test_device_input(data->keyboard, INPUT_KEYBOARD, DIK_SPACE, 2);
}

static void test_action_mapping(void)
{
    HRESULT hr;
    HINSTANCE hinst = GetModuleHandleA(NULL);
    IDirectInput8A *pDI = NULL;
    DIACTIONFORMATA af;
    DIPROPSTRING dps;
    struct enum_data data =  {pDI, &af, NULL, NULL, NULL, 0};
    HWND hwnd;

    hr = CoCreateInstance(&CLSID_DirectInput8, 0, CLSCTX_INPROC_SERVER, &IID_IDirectInput8A, (LPVOID*)&pDI);
    if (hr == DIERR_OLDDIRECTINPUTVERSION ||
        hr == DIERR_BETADIRECTINPUTVERSION ||
        hr == REGDB_E_CLASSNOTREG)
    {
        win_skip("ActionMapping requires dinput8\n");
        return;
    }
    ok(SUCCEEDED(hr), "DirectInput8 Create failed: hr=%08x\n", hr);
    if (FAILED(hr)) return;

    hr = IDirectInput8_Initialize(pDI,hinst, DIRECTINPUT_VERSION);
    if (hr == DIERR_OLDDIRECTINPUTVERSION || hr == DIERR_BETADIRECTINPUTVERSION)
    {
        win_skip("ActionMapping requires dinput8\n");
        return;
    }
    ok(SUCCEEDED(hr), "DirectInput8 Initialize failed: hr=%08x\n", hr);
    if (FAILED(hr)) return;

    memset (&af, 0, sizeof(af));
    af.dwSize = sizeof(af);
    af.dwActionSize = sizeof(DIACTIONA);
    af.dwDataSize = 4 * ARRAY_SIZE(actionMapping);
    af.dwNumActions = ARRAY_SIZE(actionMapping);
    af.rgoAction = actionMapping;
    af.guidActionMap = ACTION_MAPPING_GUID;
    af.dwGenre = 0x01000000; /* DIVIRTUAL_DRIVING_RACE */
    af.dwBufferSize = 32;

    /* This enumeration builds and sets the action map for all devices */
    data.pDI = pDI;
    hr = IDirectInput8_EnumDevicesBySemantics(pDI, 0, &af, enumeration_callback, &data, DIEDBSFL_ATTACHEDONLY);
    ok (SUCCEEDED(hr), "EnumDevicesBySemantics failed: hr=%08x\n", hr);

    if (data.keyboard)
        IDirectInputDevice_Release(data.keyboard);

    if (data.mouse)
        IDirectInputDevice_Release(data.mouse);

    /* Repeat tests with a non NULL user */
    data.username = "Ninja Brian";
    hr = IDirectInput8_EnumDevicesBySemantics(pDI, NULL, &af, enumeration_callback, &data, DIEDBSFL_ATTACHEDONLY);
    ok (SUCCEEDED(hr), "EnumDevicesBySemantics failed: hr=%08x\n", hr);

    hwnd = CreateWindowExA(WS_EX_TOPMOST, "static", "dinput",
            WS_POPUP | WS_VISIBLE, 0, 0, 100, 100, NULL, NULL, NULL, NULL);
    ok(hwnd != NULL, "failed to create window\n");
    SetCursorPos(50, 50);

    if (data.keyboard != NULL)
    {
        /* Test keyboard BuildActionMap */
        test_build_action_map(data.keyboard, data.lpdiaf, DITEST_KEYBOARDSPACE, DIDFT_PSHBUTTON, DIK_SPACE);
        /* Test keyboard input */
        test_device_input(data.keyboard, INPUT_KEYBOARD, DIK_SPACE, 2);

        /* setting format should reset action map */
        hr = IDirectInputDevice8_SetDataFormat(data.keyboard, &c_dfDIKeyboard);
        ok (SUCCEEDED(hr), "IDirectInputDevice8_SetDataFormat failed: %08x\n", hr);

        test_device_input(data.keyboard, INPUT_KEYBOARD, DIK_SPACE, -1);

        /* back to action map */
        hr = IDirectInputDevice8_SetActionMap(data.keyboard, data.lpdiaf, NULL, 0);
        ok (SUCCEEDED(hr), "SetActionMap should succeed hr=%08x\n", hr);

        test_device_input(data.keyboard, INPUT_KEYBOARD, DIK_SPACE, 2);

        test_appdata_property_vs_map(&data);

        /* Test BuildActionMap with no suitable actions for a device */
        IDirectInputDevice_Unacquire(data.keyboard);
        af.dwDataSize = 4 * DITEST_KEYBOARDSPACE;
        af.dwNumActions = DITEST_KEYBOARDSPACE;

        hr = IDirectInputDevice8_BuildActionMap(data.keyboard, data.lpdiaf, NULL, DIDBAM_HWDEFAULTS);
        ok (hr == DI_NOEFFECT, "BuildActionMap should have no effect with no actions hr=%08x\n", hr);

        hr = IDirectInputDevice8_SetActionMap(data.keyboard, data.lpdiaf, NULL, 0);
        ok (hr == DI_NOEFFECT, "SetActionMap should have no effect with no actions to map hr=%08x\n", hr);

        af.dwDataSize = 4 * ARRAY_SIZE(actionMapping);
        af.dwNumActions = ARRAY_SIZE(actionMapping);

        /* test DIDSAM_NOUSER */
        dps.diph.dwSize = sizeof(dps);
        dps.diph.dwHeaderSize = sizeof(DIPROPHEADER);
        dps.diph.dwObj = 0;
        dps.diph.dwHow = DIPH_DEVICE;
        dps.wsz[0] = '\0';

        hr = IDirectInputDevice_GetProperty(data.keyboard, DIPROP_USERNAME, &dps.diph);
        ok (SUCCEEDED(hr), "GetProperty failed hr=%08x\n", hr);
        ok (dps.wsz[0] != 0, "Expected any username, got=%s\n", wine_dbgstr_w(dps.wsz));

        hr = IDirectInputDevice8_SetActionMap(data.keyboard, data.lpdiaf, NULL, DIDSAM_NOUSER);
        ok (SUCCEEDED(hr), "SetActionMap failed hr=%08x\n", hr);

        dps.diph.dwSize = sizeof(dps);
        dps.diph.dwHeaderSize = sizeof(DIPROPHEADER);
        dps.diph.dwObj = 0;
        dps.diph.dwHow = DIPH_DEVICE;
        dps.wsz[0] = '\0';

        hr = IDirectInputDevice_GetProperty(data.keyboard, DIPROP_USERNAME, &dps.diph);
        ok (SUCCEEDED(hr), "GetProperty failed hr=%08x\n", hr);
        ok (dps.wsz[0] == 0, "Expected empty username, got=%s\n", wine_dbgstr_w(dps.wsz));

        IDirectInputDevice_Release(data.keyboard);
    }

    if (data.mouse != NULL)
    {
        /* Test mouse BuildActionMap */
        test_build_action_map(data.mouse, data.lpdiaf, DITEST_MOUSEBUTTON0, DIDFT_PSHBUTTON, 0x03);
        test_build_action_map(data.mouse, data.lpdiaf, DITEST_YAXIS, DIDFT_RELAXIS, 0x01);

        test_device_input(data.mouse, INPUT_MOUSE, MOUSEEVENTF_LEFTDOWN, 3);

        IDirectInputDevice_Release(data.mouse);
    }

    DestroyWindow(hwnd);
    IDirectInput_Release(pDI);
}

static void test_save_settings(void)
{
    HRESULT hr;
    HINSTANCE hinst = GetModuleHandleA(NULL);
    IDirectInput8A *pDI = NULL;
    DIACTIONFORMATA af;
    IDirectInputDevice8A *pKey;

    static const GUID mapping_guid = { 0xcafecafe, 0x2, 0x3, { 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xa, 0xb } };
    static const GUID other_guid = { 0xcafe, 0xcafe, 0x3, { 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xa, 0xb } };

    static DIACTIONA actions[] = {
        { 0, DIKEYBOARD_A , 0, { "Blam" } },
        { 1, DIKEYBOARD_B , 0, { "Kapow"} }
    };
    static const DWORD results[] = {
        DIDFT_MAKEINSTANCE(DIK_A) | DIDFT_PSHBUTTON,
        DIDFT_MAKEINSTANCE(DIK_B) | DIDFT_PSHBUTTON
    };
    static const DWORD other_results[] = {
        DIDFT_MAKEINSTANCE(DIK_C) | DIDFT_PSHBUTTON,
        DIDFT_MAKEINSTANCE(DIK_D) | DIDFT_PSHBUTTON
    };

    hr = CoCreateInstance(&CLSID_DirectInput8, 0, CLSCTX_INPROC_SERVER, &IID_IDirectInput8A, (LPVOID*)&pDI);
    if (hr == DIERR_OLDDIRECTINPUTVERSION ||
        hr == DIERR_BETADIRECTINPUTVERSION ||
        hr == REGDB_E_CLASSNOTREG)
    {
        win_skip("ActionMapping requires dinput8\n");
        return;
    }
    ok (SUCCEEDED(hr), "DirectInput8 Create failed: hr=%08x\n", hr);
    if (FAILED(hr)) return;

    hr = IDirectInput8_Initialize(pDI,hinst, DIRECTINPUT_VERSION);
    if (hr == DIERR_OLDDIRECTINPUTVERSION || hr == DIERR_BETADIRECTINPUTVERSION)
    {
        win_skip("ActionMapping requires dinput8\n");
        return;
    }
    ok (SUCCEEDED(hr), "DirectInput8 Initialize failed: hr=%08x\n", hr);
    if (FAILED(hr)) return;

    hr = IDirectInput_CreateDevice(pDI, &GUID_SysKeyboard, &pKey, NULL);
    ok (SUCCEEDED(hr), "IDirectInput_Create device failed hr: 0x%08x\n", hr);
    if (FAILED(hr)) return;

    memset (&af, 0, sizeof(af));
    af.dwSize = sizeof(af);
    af.dwActionSize = sizeof(DIACTIONA);
    af.dwDataSize = 4 * ARRAY_SIZE(actions);
    af.dwNumActions = ARRAY_SIZE(actions);
    af.rgoAction = actions;
    af.guidActionMap = mapping_guid;
    af.dwGenre = 0x01000000; /* DIVIRTUAL_DRIVING_RACE */
    af.dwBufferSize = 32;

    /* Easy case. Ask for default mapping, save, ask for previous map and read it back */
    hr = IDirectInputDevice8_BuildActionMap(pKey, &af, NULL, DIDBAM_HWDEFAULTS);
    ok (SUCCEEDED(hr), "BuildActionMap failed hr=%08x\n", hr);
    ok (results[0] == af.rgoAction[0].dwObjID,
        "Mapped incorrectly expected: 0x%08x got: 0x%08x\n", results[0], af.rgoAction[0].dwObjID);

    ok (results[1] == af.rgoAction[1].dwObjID,
        "Mapped incorrectly expected: 0x%08x got: 0x%08x\n", results[1], af.rgoAction[1].dwObjID);

    hr = IDirectInputDevice8_SetActionMap(pKey, &af, NULL, DIDSAM_FORCESAVE);
    ok (SUCCEEDED(hr), "SetActionMap failed hr=%08x\n", hr);

    if (hr == DI_SETTINGSNOTSAVED)
    {
        skip ("Can't test saving settings if SetActionMap returns DI_SETTINGSNOTSAVED\n");
        return;
    }

    af.rgoAction[0].dwObjID = 0;
    af.rgoAction[1].dwObjID = 0;
    memset(&af.rgoAction[0].guidInstance, 0, sizeof(GUID));
    memset(&af.rgoAction[1].guidInstance, 0, sizeof(GUID));

    hr = IDirectInputDevice8_BuildActionMap(pKey, &af, NULL, 0);
    ok (SUCCEEDED(hr), "BuildActionMap failed hr=%08x\n", hr);

    ok (results[0] == af.rgoAction[0].dwObjID,
        "Mapped incorrectly expected: 0x%08x got: 0x%08x\n", results[0], af.rgoAction[0].dwObjID);
    ok (IsEqualGUID(&GUID_SysKeyboard, &af.rgoAction[0].guidInstance), "Action should be mapped to keyboard\n");

    ok (results[1] == af.rgoAction[1].dwObjID,
        "Mapped incorrectly expected: 0x%08x got: 0x%08x\n", results[1], af.rgoAction[1].dwObjID);
    ok (IsEqualGUID(&GUID_SysKeyboard, &af.rgoAction[1].guidInstance), "Action should be mapped to keyboard\n");

    /* Test that a different action map with no pre-stored settings, in spite of the flags,
       does not try to load mappings and instead applies the default mapping */
    af.guidActionMap = other_guid;

    af.rgoAction[0].dwObjID = 0;
    af.rgoAction[1].dwObjID = 0;
    memset(&af.rgoAction[0].guidInstance, 0, sizeof(GUID));
    memset(&af.rgoAction[1].guidInstance, 0, sizeof(GUID));

    hr = IDirectInputDevice8_BuildActionMap(pKey, &af, NULL, 0);
    ok (SUCCEEDED(hr), "BuildActionMap failed hr=%08x\n", hr);

    ok (results[0] == af.rgoAction[0].dwObjID,
        "Mapped incorrectly expected: 0x%08x got: 0x%08x\n", results[0], af.rgoAction[0].dwObjID);
    ok (IsEqualGUID(&GUID_SysKeyboard, &af.rgoAction[0].guidInstance), "Action should be mapped to keyboard\n");

    ok (results[1] == af.rgoAction[1].dwObjID,
        "Mapped incorrectly expected: 0x%08x got: 0x%08x\n", results[1], af.rgoAction[1].dwObjID);
    ok (IsEqualGUID(&GUID_SysKeyboard, &af.rgoAction[1].guidInstance), "Action should be mapped to keyboard\n");

    af.guidActionMap = mapping_guid;
    /* Hard case. Customized mapping, save, ask for previous map and read it back */
    af.rgoAction[0].dwObjID = other_results[0];
    af.rgoAction[0].dwHow = DIAH_USERCONFIG;
    af.rgoAction[0].guidInstance = GUID_SysKeyboard;
    af.rgoAction[1].dwObjID = other_results[1];
    af.rgoAction[1].dwHow = DIAH_USERCONFIG;
    af.rgoAction[1].guidInstance = GUID_SysKeyboard;

    hr = IDirectInputDevice8_SetActionMap(pKey, &af, NULL, DIDSAM_FORCESAVE);
    ok (SUCCEEDED(hr), "SetActionMap failed hr=%08x\n", hr);

    if (hr == DI_SETTINGSNOTSAVED)
    {
        skip ("Can't test saving settings if SetActionMap returns DI_SETTINGSNOTSAVED\n");
        return;
    }

    af.rgoAction[0].dwObjID = 0;
    af.rgoAction[1].dwObjID = 0;
    memset(&af.rgoAction[0].guidInstance, 0, sizeof(GUID));
    memset(&af.rgoAction[1].guidInstance, 0, sizeof(GUID));

    hr = IDirectInputDevice8_BuildActionMap(pKey, &af, NULL, 0);
    ok (SUCCEEDED(hr), "BuildActionMap failed hr=%08x\n", hr);

    ok (other_results[0] == af.rgoAction[0].dwObjID,
        "Mapped incorrectly expected: 0x%08x got: 0x%08x\n", other_results[0], af.rgoAction[0].dwObjID);
    ok (IsEqualGUID(&GUID_SysKeyboard, &af.rgoAction[0].guidInstance), "Action should be mapped to keyboard\n");

    ok (other_results[1] == af.rgoAction[1].dwObjID,
        "Mapped incorrectly expected: 0x%08x got: 0x%08x\n", other_results[1], af.rgoAction[1].dwObjID);
    ok (IsEqualGUID(&GUID_SysKeyboard, &af.rgoAction[1].guidInstance), "Action should be mapped to keyboard\n");

    IDirectInputDevice_Release(pKey);
    IDirectInput_Release(pDI);
}

static void test_mouse_keyboard(void)
{
    HRESULT hr;
    HWND hwnd, di_hwnd = INVALID_HANDLE_VALUE;
    IDirectInput8A *di = NULL;
    IDirectInputDevice8A *di_mouse, *di_keyboard;
    UINT raw_devices_count;
    RAWINPUTDEVICE raw_devices[3];

    hwnd = CreateWindowExA(WS_EX_TOPMOST, "static", "dinput", WS_POPUP | WS_VISIBLE, 0, 0, 100, 100, NULL, NULL, NULL, NULL);
    ok(hwnd != NULL, "CreateWindowExA failed\n");

    hr = CoCreateInstance(&CLSID_DirectInput8, 0, CLSCTX_INPROC_SERVER, &IID_IDirectInput8A, (LPVOID*)&di);
    if (hr == DIERR_OLDDIRECTINPUTVERSION ||
        hr == DIERR_BETADIRECTINPUTVERSION ||
        hr == REGDB_E_CLASSNOTREG)
    {
        win_skip("test_mouse_keyboard requires dinput8\n");
        return;
    }
    ok(SUCCEEDED(hr), "DirectInput8Create failed: %08x\n", hr);

    hr = IDirectInput8_Initialize(di, GetModuleHandleA(NULL), DIRECTINPUT_VERSION);
    if (hr == DIERR_OLDDIRECTINPUTVERSION || hr == DIERR_BETADIRECTINPUTVERSION)
    {
        win_skip("test_mouse_keyboard requires dinput8\n");
        return;
    }
    ok(SUCCEEDED(hr), "IDirectInput8_Initialize failed: %08x\n", hr);

    hr = IDirectInput8_CreateDevice(di, &GUID_SysMouse, &di_mouse, NULL);
    ok(SUCCEEDED(hr), "IDirectInput8_CreateDevice failed: %08x\n", hr);
    hr = IDirectInputDevice8_SetDataFormat(di_mouse, &c_dfDIMouse);
    ok(SUCCEEDED(hr), "IDirectInputDevice8_SetDataFormat failed: %08x\n", hr);

    hr = IDirectInput8_CreateDevice(di, &GUID_SysKeyboard, &di_keyboard, NULL);
    ok(SUCCEEDED(hr), "IDirectInput8_CreateDevice failed: %08x\n", hr);
    hr = IDirectInputDevice8_SetDataFormat(di_keyboard, &c_dfDIKeyboard);
    ok(SUCCEEDED(hr), "IDirectInputDevice8_SetDataFormat failed: %08x\n", hr);

    raw_devices_count = ARRAY_SIZE(raw_devices);
    GetRegisteredRawInputDevices(NULL, &raw_devices_count, sizeof(RAWINPUTDEVICE));
    ok(raw_devices_count == 0, "Unexpected raw devices registered: %d\n", raw_devices_count);

    hr = IDirectInputDevice8_Acquire(di_keyboard);
    ok(SUCCEEDED(hr), "IDirectInputDevice8_Acquire failed: %08x\n", hr);
    raw_devices_count = ARRAY_SIZE(raw_devices);
    memset(raw_devices, 0, sizeof(raw_devices));
    hr = GetRegisteredRawInputDevices(raw_devices, &raw_devices_count, sizeof(RAWINPUTDEVICE));
    todo_wine
    ok(hr == 1, "GetRegisteredRawInputDevices returned %d, raw_devices_count: %d\n", hr, raw_devices_count);
    todo_wine
    ok(raw_devices[0].usUsagePage == 1, "Unexpected raw device usage page: %x\n", raw_devices[0].usUsagePage);
    todo_wine
    ok(raw_devices[0].usUsage == 6, "Unexpected raw device usage: %x\n", raw_devices[0].usUsage);
    todo_wine
    ok(raw_devices[0].dwFlags == RIDEV_INPUTSINK, "Unexpected raw device flags: %x\n", raw_devices[0].dwFlags);
    todo_wine
    ok(raw_devices[0].hwndTarget != NULL, "Unexpected raw device target: %p\n", raw_devices[0].hwndTarget);
    hr = IDirectInputDevice8_Unacquire(di_keyboard);
    ok(SUCCEEDED(hr), "IDirectInputDevice8_Acquire failed: %08x\n", hr);
    raw_devices_count = ARRAY_SIZE(raw_devices);
    GetRegisteredRawInputDevices(NULL, &raw_devices_count, sizeof(RAWINPUTDEVICE));
    ok(raw_devices_count == 0, "Unexpected raw devices registered: %d\n", raw_devices_count);

    if (raw_devices[0].hwndTarget != NULL)
    {
        WCHAR str[16];
        int i;

        di_hwnd = raw_devices[0].hwndTarget;
        i = GetClassNameW(di_hwnd, str, ARRAY_SIZE(str));
        ok(i == lstrlenW(L"DIEmWin"), "GetClassName returned incorrect length\n");
        ok(!lstrcmpW(L"DIEmWin", str), "GetClassName returned incorrect name for this window's class\n");

        i = GetWindowTextW(di_hwnd, str, ARRAY_SIZE(str));
        ok(i == lstrlenW(L"DIEmWin"), "GetClassName returned incorrect length\n");
        ok(!lstrcmpW(L"DIEmWin", str), "GetClassName returned incorrect name for this window's class\n");
    }

    hr = IDirectInputDevice8_Acquire(di_mouse);
    ok(SUCCEEDED(hr), "IDirectInputDevice8_Acquire failed: %08x\n", hr);
    raw_devices_count = ARRAY_SIZE(raw_devices);
    memset(raw_devices, 0, sizeof(raw_devices));
    hr = GetRegisteredRawInputDevices(raw_devices, &raw_devices_count, sizeof(RAWINPUTDEVICE));
    ok(hr == 1, "GetRegisteredRawInputDevices returned %d, raw_devices_count: %d\n", hr, raw_devices_count);
    ok(raw_devices[0].usUsagePage == 1, "Unexpected raw device usage page: %x\n", raw_devices[0].usUsagePage);
    ok(raw_devices[0].usUsage == 2, "Unexpected raw device usage: %x\n", raw_devices[0].usUsage);
    ok(raw_devices[0].dwFlags == RIDEV_INPUTSINK, "Unexpected raw device flags: %x\n", raw_devices[0].dwFlags);
    todo_wine
    ok(raw_devices[0].hwndTarget == di_hwnd, "Unexpected raw device target: %p\n", raw_devices[0].hwndTarget);
    hr = IDirectInputDevice8_Unacquire(di_mouse);
    ok(SUCCEEDED(hr), "IDirectInputDevice8_Acquire failed: %08x\n", hr);
    raw_devices_count = ARRAY_SIZE(raw_devices);
    GetRegisteredRawInputDevices(NULL, &raw_devices_count, sizeof(RAWINPUTDEVICE));
    ok(raw_devices_count == 0, "Unexpected raw devices registered: %d\n", raw_devices_count);

    if (raw_devices[0].hwndTarget != NULL)
        di_hwnd = raw_devices[0].hwndTarget;

    /* expect dinput8 to take over any activated raw input devices */
    raw_devices[0].usUsagePage = 0x01;
    raw_devices[0].usUsage = 0x05;
    raw_devices[0].dwFlags = 0;
    raw_devices[0].hwndTarget = hwnd;
    raw_devices[1].usUsagePage = 0x01;
    raw_devices[1].usUsage = 0x06;
    raw_devices[1].dwFlags = 0;
    raw_devices[1].hwndTarget = hwnd;
    raw_devices[2].usUsagePage = 0x01;
    raw_devices[2].usUsage = 0x02;
    raw_devices[2].dwFlags = 0;
    raw_devices[2].hwndTarget = hwnd;
    raw_devices_count = ARRAY_SIZE(raw_devices);
    hr = RegisterRawInputDevices(raw_devices, raw_devices_count, sizeof(RAWINPUTDEVICE));
    ok(hr == TRUE, "RegisterRawInputDevices failed\n");

    hr = IDirectInputDevice8_Acquire(di_keyboard);
    ok(SUCCEEDED(hr), "IDirectInputDevice8_Acquire failed: %08x\n", hr);
    hr = IDirectInputDevice8_Acquire(di_mouse);
    ok(SUCCEEDED(hr), "IDirectInputDevice8_Acquire failed: %08x\n", hr);
    raw_devices_count = ARRAY_SIZE(raw_devices);
    memset(raw_devices, 0, sizeof(raw_devices));
    hr = GetRegisteredRawInputDevices(raw_devices, &raw_devices_count, sizeof(RAWINPUTDEVICE));
    ok(hr == 3, "GetRegisteredRawInputDevices returned %d, raw_devices_count: %d\n", hr, raw_devices_count);
    ok(raw_devices[0].usUsagePage == 1, "Unexpected raw device usage page: %x\n", raw_devices[0].usUsagePage);
    ok(raw_devices[0].usUsage == 2, "Unexpected raw device usage: %x\n", raw_devices[0].usUsage);
    ok(raw_devices[0].dwFlags == RIDEV_INPUTSINK, "Unexpected raw device flags: %x\n", raw_devices[0].dwFlags);
    ok(raw_devices[0].hwndTarget == di_hwnd, "Unexpected raw device target: %p\n", raw_devices[0].hwndTarget);
    ok(raw_devices[1].usUsagePage == 1, "Unexpected raw device usage page: %x\n", raw_devices[1].usUsagePage);
    ok(raw_devices[1].usUsage == 5, "Unexpected raw device usage: %x\n", raw_devices[1].usUsage);
    ok(raw_devices[1].dwFlags == 0, "Unexpected raw device flags: %x\n", raw_devices[1].dwFlags);
    ok(raw_devices[1].hwndTarget == hwnd, "Unexpected raw device target: %p\n", raw_devices[1].hwndTarget);
    ok(raw_devices[2].usUsagePage == 1, "Unexpected raw device usage page: %x\n", raw_devices[1].usUsagePage);
    ok(raw_devices[2].usUsage == 6, "Unexpected raw device usage: %x\n", raw_devices[1].usUsage);
    todo_wine
    ok(raw_devices[2].dwFlags == RIDEV_INPUTSINK, "Unexpected raw device flags: %x\n", raw_devices[1].dwFlags);
    todo_wine
    ok(raw_devices[2].hwndTarget == di_hwnd, "Unexpected raw device target: %p\n", raw_devices[1].hwndTarget);
    hr = IDirectInputDevice8_Unacquire(di_keyboard);
    ok(SUCCEEDED(hr), "IDirectInputDevice8_Acquire failed: %08x\n", hr);
    hr = IDirectInputDevice8_Unacquire(di_mouse);
    ok(SUCCEEDED(hr), "IDirectInputDevice8_Acquire failed: %08x\n", hr);
    raw_devices_count = ARRAY_SIZE(raw_devices);
    GetRegisteredRawInputDevices(NULL, &raw_devices_count, sizeof(RAWINPUTDEVICE));
    todo_wine
    ok(raw_devices_count == 1, "Unexpected raw devices registered: %d\n", raw_devices_count);

    IDirectInputDevice8_SetCooperativeLevel(di_mouse, hwnd, DISCL_FOREGROUND|DISCL_EXCLUSIVE);
    IDirectInputDevice8_SetCooperativeLevel(di_keyboard, hwnd, DISCL_FOREGROUND|DISCL_EXCLUSIVE);

    hr = IDirectInputDevice8_Acquire(di_keyboard);
    ok(SUCCEEDED(hr), "IDirectInputDevice8_Acquire failed: %08x\n", hr);
    hr = IDirectInputDevice8_Acquire(di_mouse);
    ok(SUCCEEDED(hr), "IDirectInputDevice8_Acquire failed: %08x\n", hr);
    raw_devices_count = ARRAY_SIZE(raw_devices);
    memset(raw_devices, 0, sizeof(raw_devices));
    hr = GetRegisteredRawInputDevices(raw_devices, &raw_devices_count, sizeof(RAWINPUTDEVICE));
    ok(hr == 3, "GetRegisteredRawInputDevices returned %d, raw_devices_count: %d\n", hr, raw_devices_count);
    ok(raw_devices[0].dwFlags == (RIDEV_CAPTUREMOUSE|RIDEV_NOLEGACY), "Unexpected raw device flags: %x\n", raw_devices[0].dwFlags);
    todo_wine
    ok(raw_devices[2].dwFlags == (RIDEV_NOHOTKEYS|RIDEV_NOLEGACY), "Unexpected raw device flags: %x\n", raw_devices[1].dwFlags);
    hr = IDirectInputDevice8_Unacquire(di_keyboard);
    ok(SUCCEEDED(hr), "IDirectInputDevice8_Acquire failed: %08x\n", hr);
    hr = IDirectInputDevice8_Unacquire(di_mouse);
    ok(SUCCEEDED(hr), "IDirectInputDevice8_Acquire failed: %08x\n", hr);

    raw_devices_count = ARRAY_SIZE(raw_devices);
    hr = GetRegisteredRawInputDevices(raw_devices, &raw_devices_count, sizeof(RAWINPUTDEVICE));
    todo_wine
    ok(hr == 1, "GetRegisteredRawInputDevices returned %d, raw_devices_count: %d\n", hr, raw_devices_count);
    ok(raw_devices[0].usUsagePage == 1, "Unexpected raw device usage page: %x\n", raw_devices[0].usUsagePage);
    ok(raw_devices[0].usUsage == 5, "Unexpected raw device usage: %x\n", raw_devices[0].usUsage);
    ok(raw_devices[0].dwFlags == 0, "Unexpected raw device flags: %x\n", raw_devices[0].dwFlags);
    ok(raw_devices[0].hwndTarget == hwnd, "Unexpected raw device target: %p\n", raw_devices[0].hwndTarget);

    IDirectInputDevice8_Release(di_mouse);
    IDirectInputDevice8_Release(di_keyboard);
    IDirectInput8_Release(di);

    DestroyWindow(hwnd);
}

static void test_keyboard_events(void)
{
    HRESULT hr;
    HWND hwnd = INVALID_HANDLE_VALUE;
    IDirectInput8A *di;
    IDirectInputDevice8A *di_keyboard;
    DIPROPDWORD dp;
    DIDEVICEOBJECTDATA obj_data[10];
    DWORD data_size;
    BYTE kbdata[256];

    hr = CoCreateInstance(&CLSID_DirectInput8, 0, CLSCTX_INPROC_SERVER, &IID_IDirectInput8A, (LPVOID*)&di);
    if (hr == DIERR_OLDDIRECTINPUTVERSION ||
        hr == DIERR_BETADIRECTINPUTVERSION ||
        hr == REGDB_E_CLASSNOTREG)
    {
        win_skip("test_keyboard_events requires dinput8\n");
        return;
    }
    ok(SUCCEEDED(hr), "DirectInput8Create failed: %08x\n", hr);

    hr = IDirectInput8_Initialize(di, GetModuleHandleA(NULL), DIRECTINPUT_VERSION);
    if (hr == DIERR_OLDDIRECTINPUTVERSION || hr == DIERR_BETADIRECTINPUTVERSION)
    {
        win_skip("test_keyboard_events requires dinput8\n");
        IDirectInput8_Release(di);
        return;
    }
    ok(SUCCEEDED(hr), "IDirectInput8_Initialize failed: %08x\n", hr);

    hwnd = CreateWindowExA(WS_EX_TOPMOST, "static", "dinput", WS_POPUP | WS_VISIBLE, 0, 0, 100, 100, NULL, NULL, NULL, NULL);
    ok(hwnd != NULL, "CreateWindowExA failed\n");

    hr = IDirectInput8_CreateDevice(di, &GUID_SysKeyboard, &di_keyboard, NULL);
    ok(SUCCEEDED(hr), "IDirectInput8_CreateDevice failed: %08x\n", hr);
    hr = IDirectInputDevice8_SetCooperativeLevel(di_keyboard, hwnd, DISCL_BACKGROUND | DISCL_NONEXCLUSIVE);
    ok(SUCCEEDED(hr), "IDirectInput8_SetCooperativeLevel failed: %08x\n", hr);
    hr = IDirectInputDevice8_SetDataFormat(di_keyboard, &c_dfDIKeyboard);
    ok(SUCCEEDED(hr), "IDirectInputDevice8_SetDataFormat failed: %08x\n", hr);
    dp.diph.dwSize = sizeof(DIPROPDWORD);
    dp.diph.dwHeaderSize = sizeof(DIPROPHEADER);
    dp.diph.dwObj = 0;
    dp.diph.dwHow = DIPH_DEVICE;
    dp.dwData = ARRAY_SIZE(obj_data);
    IDirectInputDevice8_SetProperty(di_keyboard, DIPROP_BUFFERSIZE, &(dp.diph));

    hr = IDirectInputDevice8_Acquire(di_keyboard);
    ok(SUCCEEDED(hr), "IDirectInputDevice8_Acquire failed: %08x\n", hr);

    /* Test injecting keyboard events with both VK and scancode given. */
    keybd_event(VK_SPACE, DIK_SPACE, 0, 0);
    flush_events();
    IDirectInputDevice8_Poll(di_keyboard);
    data_size = ARRAY_SIZE(obj_data);
    hr = IDirectInputDevice8_GetDeviceData(di_keyboard, sizeof(DIDEVICEOBJECTDATA), obj_data, &data_size, 0);
    ok(SUCCEEDED(hr), "Failed to get data hr=%08x\n", hr);
    ok(data_size == 1, "Expected 1 element, received %d\n", data_size);

    hr = IDirectInputDevice8_GetDeviceState(di_keyboard, sizeof(kbdata), kbdata);
    ok(SUCCEEDED(hr), "IDirectInputDevice8_GetDeviceState failed: %08x\n", hr);
    ok(kbdata[DIK_SPACE], "Expected DIK_SPACE key state down\n");

    keybd_event(VK_SPACE, DIK_SPACE, KEYEVENTF_KEYUP, 0);
    flush_events();
    IDirectInputDevice8_Poll(di_keyboard);
    data_size = ARRAY_SIZE(obj_data);
    hr = IDirectInputDevice8_GetDeviceData(di_keyboard, sizeof(DIDEVICEOBJECTDATA), obj_data, &data_size, 0);
    ok(SUCCEEDED(hr), "Failed to get data hr=%08x\n", hr);
    ok(data_size == 1, "Expected 1 element, received %d\n", data_size);

    /* Test injecting keyboard events with scancode=0.
     * Windows DInput ignores the VK, sets scancode 0 to be pressed, and GetDeviceData returns no elements. */
    keybd_event(VK_SPACE, 0, 0, 0);
    flush_events();
    IDirectInputDevice8_Poll(di_keyboard);
    data_size = ARRAY_SIZE(obj_data);
    hr = IDirectInputDevice8_GetDeviceData(di_keyboard, sizeof(DIDEVICEOBJECTDATA), obj_data, &data_size, 0);
    ok(SUCCEEDED(hr), "Failed to get data hr=%08x\n", hr);
    ok(data_size == 0, "Expected 0 elements, received %d\n", data_size);

    hr = IDirectInputDevice8_GetDeviceState(di_keyboard, sizeof(kbdata), kbdata);
    ok(SUCCEEDED(hr), "IDirectInputDevice8_GetDeviceState failed: %08x\n", hr);
    todo_wine
    ok(kbdata[0], "Expected key 0 state down\n");

    keybd_event(VK_SPACE, 0, KEYEVENTF_KEYUP, 0);
    flush_events();
    IDirectInputDevice8_Poll(di_keyboard);
    data_size = ARRAY_SIZE(obj_data);
    hr = IDirectInputDevice8_GetDeviceData(di_keyboard, sizeof(DIDEVICEOBJECTDATA), obj_data, &data_size, 0);
    ok(SUCCEEDED(hr), "Failed to get data hr=%08x\n", hr);
    ok(data_size == 0, "Expected 0 elements, received %d\n", data_size);

    hr = IDirectInputDevice8_Unacquire(di_keyboard);
    ok(SUCCEEDED(hr), "IDirectInputDevice8_Unacquire failed: %08x\n", hr);

    IDirectInputDevice8_Release(di_keyboard);
    IDirectInput8_Release(di);

    DestroyWindow(hwnd);
}

static void test_appdata_property(void)
{
    HRESULT hr;
    HINSTANCE hinst = GetModuleHandleA(NULL);
    IDirectInputDevice8A *di_keyboard;
    IDirectInput8A *pDI = NULL;
    HWND hwnd;
    DIPROPDWORD dw;
    DIPROPPOINTER dp;

    hr = CoCreateInstance(&CLSID_DirectInput8, 0, CLSCTX_INPROC_SERVER, &IID_IDirectInput8A, (LPVOID*)&pDI);
    if (hr == DIERR_OLDDIRECTINPUTVERSION ||
        hr == DIERR_BETADIRECTINPUTVERSION ||
        hr == REGDB_E_CLASSNOTREG)
    {
        win_skip("DIPROP_APPDATA requires dinput8\n");
        return;
    }
    ok(SUCCEEDED(hr), "DirectInput8 Create failed: hr=%08x\n", hr);
    if (FAILED(hr)) return;

    hr = IDirectInput8_Initialize(pDI,hinst, DIRECTINPUT_VERSION);
    if (hr == DIERR_OLDDIRECTINPUTVERSION || hr == DIERR_BETADIRECTINPUTVERSION)
    {
        win_skip("DIPROP_APPDATA requires dinput8\n");
        return;
    }
    ok(SUCCEEDED(hr), "DirectInput8 Initialize failed: hr=%08x\n", hr);
    if (FAILED(hr)) return;

    hwnd = CreateWindowExA(WS_EX_TOPMOST, "static", "dinput",
            WS_POPUP | WS_VISIBLE, 0, 0, 100, 100, NULL, NULL, NULL, NULL);
    ok(hwnd != NULL, "failed to create window\n");

    hr = IDirectInput8_CreateDevice(pDI, &GUID_SysKeyboard, &di_keyboard, NULL);
    ok(SUCCEEDED(hr), "IDirectInput8_CreateDevice failed: %08x\n", hr);

    hr = IDirectInputDevice8_SetDataFormat(di_keyboard, &c_dfDIKeyboard);
    ok(SUCCEEDED(hr), "IDirectInputDevice8_SetDataFormat failed: %08x\n", hr);

    dw.diph.dwSize = sizeof(DIPROPDWORD);
    dw.diph.dwHeaderSize = sizeof(DIPROPHEADER);
    dw.diph.dwObj = 0;
    dw.diph.dwHow = DIPH_DEVICE;
    dw.dwData = 32;
    hr = IDirectInputDevice8_SetProperty(di_keyboard, DIPROP_BUFFERSIZE, &(dw.diph));
    ok(SUCCEEDED(hr), "IDirectInputDevice8_SetProperty failed hr=%08x\n", hr);

    /* the default value */
    test_device_input(di_keyboard, INPUT_KEYBOARD, DIK_A, -1);

    dp.diph.dwHow = DIPH_DEVICE;
    dp.diph.dwObj = 0;
    dp.uData = 1;
    hr = IDirectInputDevice8_SetProperty(di_keyboard, DIPROP_APPDATA, &(dp.diph));
    ok(hr == DIERR_INVALIDPARAM, "IDirectInputDevice8_SetProperty APPDATA for the device should be invalid hr=%08x\n", hr);

    dp.diph.dwSize = sizeof(dp);
    dp.diph.dwHeaderSize = sizeof(DIPROPHEADER);
    dp.diph.dwHow = DIPH_BYUSAGE;
    dp.diph.dwObj = 2;
    dp.uData = 2;
    hr = IDirectInputDevice8_SetProperty(di_keyboard, DIPROP_APPDATA, &(dp.diph));
    ok(hr == DIERR_UNSUPPORTED, "IDirectInputDevice8_SetProperty APPDATA by usage should be unsupported hr=%08x\n", hr);

    dp.diph.dwHow = DIPH_BYID;
    dp.diph.dwObj = DIDFT_MAKEINSTANCE(DIK_SPACE) | DIDFT_PSHBUTTON;
    dp.uData = 3;
    hr = IDirectInputDevice8_SetProperty(di_keyboard, DIPROP_APPDATA, &(dp.diph));
    ok(SUCCEEDED(hr), "IDirectInputDevice8_SetProperty failed hr=%08x\n", hr);

    dp.diph.dwHow = DIPH_BYOFFSET;
    dp.diph.dwObj = DIK_A;
    dp.uData = 4;
    hr = IDirectInputDevice8_SetProperty(di_keyboard, DIPROP_APPDATA, &(dp.diph));
    ok(SUCCEEDED(hr), "IDirectInputDevice8_SetProperty failed hr=%08x\n", hr);

    dp.diph.dwHow = DIPH_BYOFFSET;
    dp.diph.dwObj = DIK_B;
    dp.uData = 5;
    hr = IDirectInputDevice8_SetProperty(di_keyboard, DIPROP_APPDATA, &(dp.diph));
    ok(SUCCEEDED(hr), "IDirectInputDevice8_SetProperty failed hr=%08x\n", hr);

    test_device_input(di_keyboard, INPUT_KEYBOARD, DIK_SPACE, 3);
    test_device_input(di_keyboard, INPUT_KEYBOARD, DIK_A, 4);
    test_device_input(di_keyboard, INPUT_KEYBOARD, DIK_B, 5);
    test_device_input(di_keyboard, INPUT_KEYBOARD, DIK_C, -1);

    /* setting data format resets APPDATA */
    hr = IDirectInputDevice8_SetDataFormat(di_keyboard, &c_dfDIKeyboard);
    ok(SUCCEEDED(hr), "IDirectInputDevice8_SetDataFormat failed: %08x\n", hr);

    test_device_input(di_keyboard, INPUT_KEYBOARD, VK_SPACE, -1);
    test_device_input(di_keyboard, INPUT_KEYBOARD, DIK_A, -1);
    test_device_input(di_keyboard, INPUT_KEYBOARD, DIK_B, -1);
    test_device_input(di_keyboard, INPUT_KEYBOARD, DIK_C, -1);

    DestroyWindow(hwnd);
    IDirectInputDevice_Release(di_keyboard);
    IDirectInput_Release(pDI);
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

struct check_objects_todos
{
    BOOL offset;
    BOOL type;
    BOOL name;
};

struct check_objects_params
{
    UINT index;
    UINT stop_count;
    UINT expect_count;
    const DIDEVICEOBJECTINSTANCEW *expect_objs;
    const struct check_objects_todos *todo_objs;
    BOOL todo_extra;
};

static BOOL CALLBACK check_objects( const DIDEVICEOBJECTINSTANCEW *obj, void *args )
{
    struct check_objects_params *params = args;
    const DIDEVICEOBJECTINSTANCEW *exp = params->expect_objs + params->index;
    const struct check_objects_todos *todo = params->todo_objs + params->index;

    todo_wine_if( params->todo_extra && params->index >= params->expect_count )
    ok( params->index < params->expect_count, "unexpected extra object\n" );
    if (params->index >= params->expect_count) return DIENUM_CONTINUE;

    winetest_push_context( "obj[%d]", params->index );

    check_member( *obj, *exp, "%u", dwSize );
    check_member_guid( *obj, *exp, guidType );
    todo_wine_if( todo->offset )
    check_member( *obj, *exp, "%#x", dwOfs );
    todo_wine_if( todo->type )
    check_member( *obj, *exp, "%#x", dwType );
    check_member( *obj, *exp, "%#x", dwFlags );
    if (!localized) todo_wine_if( todo->name ) check_member_wstr( *obj, *exp, tszName );
    check_member( *obj, *exp, "%u", dwFFMaxForce );
    check_member( *obj, *exp, "%u", dwFFForceResolution );
    check_member( *obj, *exp, "%u", wCollectionNumber );
    check_member( *obj, *exp, "%u", wDesignatorIndex );
    check_member( *obj, *exp, "%#04x", wUsagePage );
    check_member( *obj, *exp, "%#04x", wUsage );
    check_member( *obj, *exp, "%#04x", dwDimension );
    check_member( *obj, *exp, "%#04x", wExponent );
    check_member( *obj, *exp, "%u", wReportId );

    winetest_pop_context();

    params->index++;
    if (params->stop_count && params->index == params->stop_count) return DIENUM_STOP;
    return DIENUM_CONTINUE;
}

static BOOL CALLBACK check_object_count( const DIDEVICEOBJECTINSTANCEW *obj, void *args )
{
    DWORD *count = args;
    *count = *count + 1;
    return DIENUM_CONTINUE;
}

static void test_mouse_info(void)
{
    static const DIDEVCAPS expect_caps =
    {
        .dwSize = sizeof(DIDEVCAPS),
        .dwFlags = DIDC_ATTACHED | DIDC_EMULATED,
        .dwDevType = (DI8DEVTYPEMOUSE_UNKNOWN << 8) | DI8DEVTYPE_MOUSE,
        .dwAxes = 3,
        .dwButtons = 5,
    };
    const DIDEVICEINSTANCEW expect_devinst =
    {
        .dwSize = sizeof(DIDEVICEINSTANCEW),
        .guidInstance = GUID_SysMouse,
        .guidProduct = GUID_SysMouse,
        .dwDevType = (DI8DEVTYPEMOUSE_UNKNOWN << 8) | DI8DEVTYPE_MOUSE,
        .tszInstanceName = L"Mouse",
        .tszProductName = L"Mouse",
        .guidFFDriver = GUID_NULL,
    };
    const DIDEVICEOBJECTINSTANCEW expect_objects[] =
    {
        {
            .dwSize = sizeof(DIDEVICEOBJECTINSTANCEW),
            .guidType = GUID_XAxis,
            .dwOfs = 0x0,
            .dwType = DIDFT_RELAXIS|DIDFT_MAKEINSTANCE(0),
            .dwFlags = DIDOI_ASPECTPOSITION,
            .tszName = L"X-axis",
        },
        {
            .dwSize = sizeof(DIDEVICEOBJECTINSTANCEW),
            .guidType = GUID_YAxis,
            .dwOfs = 0x4,
            .dwType = DIDFT_RELAXIS|DIDFT_MAKEINSTANCE(1),
            .dwFlags = DIDOI_ASPECTPOSITION,
            .tszName = L"Y-axis",
        },
        {
            .dwSize = sizeof(DIDEVICEOBJECTINSTANCEW),
            .guidType = GUID_ZAxis,
            .dwOfs = 0x8,
            .dwType = DIDFT_RELAXIS|DIDFT_MAKEINSTANCE(2),
            .dwFlags = DIDOI_ASPECTPOSITION,
            .tszName = L"Wheel",
        },
        {
            .dwSize = sizeof(DIDEVICEOBJECTINSTANCEW),
            .guidType = GUID_Button,
            .dwOfs = 0xc,
            .dwType = DIDFT_PSHBUTTON|DIDFT_MAKEINSTANCE(3),
            .tszName = L"Button 0",
        },
        {
            .dwSize = sizeof(DIDEVICEOBJECTINSTANCEW),
            .guidType = GUID_Button,
            .dwOfs = 0xd,
            .dwType = DIDFT_PSHBUTTON|DIDFT_MAKEINSTANCE(4),
            .tszName = L"Button 1",
        },
        {
            .dwSize = sizeof(DIDEVICEOBJECTINSTANCEW),
            .guidType = GUID_Button,
            .dwOfs = 0xe,
            .dwType = DIDFT_PSHBUTTON|DIDFT_MAKEINSTANCE(5),
            .tszName = L"Button 2",
        },
        {
            .dwSize = sizeof(DIDEVICEOBJECTINSTANCEW),
            .guidType = GUID_Button,
            .dwOfs = 0xf,
            .dwType = DIDFT_PSHBUTTON|DIDFT_MAKEINSTANCE(6),
            .tszName = L"Button 3",
        },
        {
            .dwSize = sizeof(DIDEVICEOBJECTINSTANCEW),
            .guidType = GUID_Button,
            .dwOfs = 0x10,
            .dwType = DIDFT_PSHBUTTON|DIDFT_MAKEINSTANCE(7),
            .tszName = L"Button 4",
        },
    };
    struct check_objects_todos todo_objects[ARRAY_SIZE(expect_objects)] = {{0}};
    struct check_objects_params check_objects_params =
    {
        .expect_count = ARRAY_SIZE(expect_objects),
        .expect_objs = expect_objects,
        .todo_objs = todo_objects,
        .todo_extra = TRUE,
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
    DIDEVICEOBJECTINSTANCEW objinst = {0};
    DIDEVICEINSTANCEW devinst = {0};
    IDirectInputDevice8W *device;
    DIDEVCAPS caps = {0};
    IDirectInput8W *di;
    ULONG res, ref;
    HRESULT hr;
    GUID guid;

    localized = LOWORD( GetKeyboardLayout( 0 ) ) != 0x0409;

    hr = DirectInput8Create( instance, DIRECTINPUT_VERSION, &IID_IDirectInput8W, (void **)&di, NULL );
    ok( hr == DI_OK, "DirectInput8Create returned %#x\n", hr );
    hr = IDirectInput8_CreateDevice( di, &GUID_SysMouse, &device, NULL );
    ok( hr == DI_OK, "CreateDevice returned %#x\n", hr );

    hr = IDirectInputDevice8_Initialize( device, instance, DIRECTINPUT_VERSION, &GUID_SysMouseEm );
    ok( hr == DI_OK, "Initialize returned %#x\n", hr );
    guid = GUID_SysMouseEm;
    memset( &devinst, 0, sizeof(devinst) );
    devinst.dwSize = sizeof(DIDEVICEINSTANCEW);
    hr = IDirectInputDevice8_GetDeviceInfo( device, &devinst );
    ok( hr == DI_OK, "GetDeviceInfo returned %#x\n", hr );
    ok( IsEqualGUID( &guid, &GUID_SysMouseEm ), "got %s expected %s\n", debugstr_guid( &guid ),
        debugstr_guid( &GUID_SysMouseEm ) );

    hr = IDirectInputDevice8_Initialize( device, instance, DIRECTINPUT_VERSION, &GUID_SysMouse );
    ok( hr == DI_OK, "Initialize returned %#x\n", hr );

    memset( &devinst, 0, sizeof(devinst) );
    devinst.dwSize = sizeof(DIDEVICEINSTANCEW);
    hr = IDirectInputDevice8_GetDeviceInfo( device, &devinst );
    ok( hr == DI_OK, "GetDeviceInfo returned %#x\n", hr );
    check_member( devinst, expect_devinst, "%d", dwSize );
    check_member_guid( devinst, expect_devinst, guidInstance );
    check_member_guid( devinst, expect_devinst, guidProduct );
    todo_wine
    check_member( devinst, expect_devinst, "%#x", dwDevType );
    if (!localized) check_member_wstr( devinst, expect_devinst, tszInstanceName );
    if (!localized) todo_wine check_member_wstr( devinst, expect_devinst, tszProductName );
    check_member_guid( devinst, expect_devinst, guidFFDriver );
    check_member( devinst, expect_devinst, "%04x", wUsagePage );
    check_member( devinst, expect_devinst, "%04x", wUsage );

    caps.dwSize = sizeof(DIDEVCAPS);
    hr = IDirectInputDevice8_GetCapabilities( device, &caps );
    ok( hr == DI_OK, "GetCapabilities returned %#x\n", hr );
    check_member( caps, expect_caps, "%d", dwSize );
    check_member( caps, expect_caps, "%#x", dwFlags );
    todo_wine
    check_member( caps, expect_caps, "%#x", dwDevType );
    check_member( caps, expect_caps, "%d", dwAxes );
    check_member( caps, expect_caps, "%d", dwButtons );
    check_member( caps, expect_caps, "%d", dwPOVs );
    check_member( caps, expect_caps, "%d", dwFFSamplePeriod );
    check_member( caps, expect_caps, "%d", dwFFMinTimeResolution );
    todo_wine
    check_member( caps, expect_caps, "%d", dwFirmwareRevision );
    todo_wine
    check_member( caps, expect_caps, "%d", dwHardwareRevision );
    check_member( caps, expect_caps, "%d", dwFFDriverVersion );

    prop_dword.diph.dwHow = DIPH_BYOFFSET;
    prop_dword.diph.dwObj = DIMOFS_X;
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_GRANULARITY, &prop_dword.diph );
    ok( hr == DIERR_NOTFOUND, "GetProperty DIPROP_GRANULARITY returned %#x\n", hr );
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_DEADZONE, &prop_dword.diph );
    ok( hr == DIERR_NOTFOUND, "GetProperty DIPROP_DEADZONE returned %#x\n", hr );
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_SATURATION, &prop_dword.diph );
    ok( hr == DIERR_NOTFOUND, "GetProperty DIPROP_SATURATION returned %#x\n", hr );
    prop_range.diph.dwHow = DIPH_BYOFFSET;
    prop_range.diph.dwObj = DIMOFS_X;
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_RANGE, &prop_range.diph );
    ok( hr == DIERR_NOTFOUND, "GetProperty DIPROP_RANGE returned %#x\n", hr );

    prop_dword.diph.dwHow = DIPH_DEVICE;
    prop_dword.diph.dwObj = 0;
    prop_dword.dwData = 0xdeadbeef;
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_AXISMODE, &prop_dword.diph );
    todo_wine
    ok( hr == DI_OK, "GetProperty DIPROP_AXISMODE returned %#x\n", hr );
    todo_wine
    ok( prop_dword.dwData == DIPROPAXISMODE_ABS, "got %u expected %u\n", prop_dword.dwData, DIPROPAXISMODE_ABS );
    prop_dword.dwData = 0xdeadbeef;
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_BUFFERSIZE, &prop_dword.diph );
    ok( hr == DI_OK, "GetProperty DIPROP_BUFFERSIZE returned %#x\n", hr );
    ok( prop_dword.dwData == 0, "got %#x expected %#x\n", prop_dword.dwData, 0 );
    prop_dword.dwData = 0xdeadbeef;
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_FFGAIN, &prop_dword.diph );
    ok( hr == DI_OK, "GetProperty DIPROP_FFGAIN returned %#x\n", hr );
    ok( prop_dword.dwData == 10000, "got %u expected %u\n", prop_dword.dwData, 10000 );

    hr = IDirectInputDevice8_SetDataFormat( device, &c_dfDIMouse2 );
    ok( hr == DI_OK, "SetDataFormat returned %#x\n", hr );

    prop_dword.diph.dwHow = DIPH_DEVICE;
    prop_dword.diph.dwObj = 0;
    prop_dword.dwData = 0xdeadbeef;
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_AXISMODE, &prop_dword.diph );
    todo_wine
    ok( hr == DI_OK, "GetProperty DIPROP_AXISMODE returned %#x\n", hr );
    todo_wine
    ok( prop_dword.dwData == DIPROPAXISMODE_REL, "got %u expected %u\n", prop_dword.dwData, DIPROPAXISMODE_ABS );

    prop_dword.dwData = 0xdeadbeef;
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_VIDPID, &prop_dword.diph );
    ok( hr == DIERR_UNSUPPORTED, "GetProperty DIPROP_VIDPID returned %#x\n", hr );
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_GUIDANDPATH, &prop_guid_path.diph );
    ok( hr == DIERR_UNSUPPORTED, "GetProperty DIPROP_GUIDANDPATH returned %#x\n", hr );
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_INSTANCENAME, &prop_string.diph );
    ok( hr == DIERR_UNSUPPORTED, "GetProperty DIPROP_INSTANCENAME returned %#x\n", hr );
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_PRODUCTNAME, &prop_string.diph );
    ok( hr == DIERR_UNSUPPORTED, "GetProperty DIPROP_PRODUCTNAME returned %#x\n", hr );
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_TYPENAME, &prop_string.diph );
    ok( hr == DIERR_UNSUPPORTED, "GetProperty DIPROP_TYPENAME returned %#x\n", hr );
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_USERNAME, &prop_string.diph );
    ok( hr == S_FALSE, "GetProperty DIPROP_USERNAME returned %#x\n", hr );
    ok( !wcscmp( prop_string.wsz, L"" ), "got user %s\n", debugstr_w(prop_string.wsz) );

    hr = IDirectInputDevice8_GetProperty( device, DIPROP_JOYSTICKID, &prop_dword.diph );
    ok( hr == DIERR_UNSUPPORTED, "GetProperty DIPROP_VIDPID returned %#x\n", hr );

    hr = IDirectInputDevice8_GetProperty( device, DIPROP_CALIBRATION, &prop_dword.diph );
    ok( hr == DIERR_INVALIDPARAM, "GetProperty DIPROP_CALIBRATION returned %#x\n", hr );
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_AUTOCENTER, &prop_dword.diph );
    ok( hr == DIERR_UNSUPPORTED, "GetProperty DIPROP_AUTOCENTER returned %#x\n", hr );
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_DEADZONE, &prop_dword.diph );
    ok( hr == DIERR_UNSUPPORTED, "GetProperty DIPROP_DEADZONE returned %#x\n", hr );
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_FFLOAD, &prop_dword.diph );
    ok( hr == DIERR_UNSUPPORTED, "GetProperty DIPROP_FFLOAD returned %#x\n", hr );
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_GRANULARITY, &prop_dword.diph );
    ok( hr == DIERR_UNSUPPORTED, "GetProperty DIPROP_GRANULARITY returned %#x\n", hr );

    prop_dword.diph.dwHow = DIPH_BYUSAGE;
    prop_dword.diph.dwObj = MAKELONG( HID_USAGE_GENERIC_X, HID_USAGE_PAGE_GENERIC );
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_GRANULARITY, &prop_dword.diph );
    ok( hr == DIERR_UNSUPPORTED, "GetProperty DIPROP_GRANULARITY returned %#x\n", hr );

    prop_dword.diph.dwHow = DIPH_BYOFFSET;
    prop_dword.diph.dwObj = DIMOFS_X;
    prop_dword.dwData = 0xdeadbeef;
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_GRANULARITY, &prop_dword.diph );
    ok( hr == DI_OK, "GetProperty DIPROP_GRANULARITY returned %#x\n", hr );
    ok( prop_dword.dwData == 1, "got %d expected 1\n", prop_dword.dwData );
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_DEADZONE, &prop_dword.diph );
    ok( hr == DIERR_UNSUPPORTED, "GetProperty DIPROP_DEADZONE returned %#x\n", hr );
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_SATURATION, &prop_dword.diph );
    ok( hr == DIERR_UNSUPPORTED, "GetProperty DIPROP_SATURATION returned %#x\n", hr );
    prop_range.lMin = 0xdeadbeef;
    prop_range.lMax = 0xdeadbeef;
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_RANGE, &prop_range.diph );
    ok( hr == DI_OK, "GetProperty DIPROP_RANGE returned %#x\n", hr );
    ok( prop_range.lMin == DIPROPRANGE_NOMIN, "got %d expected %d\n", prop_range.lMin, DIPROPRANGE_NOMIN );
    ok( prop_range.lMax == DIPROPRANGE_NOMAX, "got %d expected %d\n", prop_range.lMax, DIPROPRANGE_NOMAX );

    res = 0;
    hr = IDirectInputDevice8_EnumObjects( device, check_object_count, &res, DIDFT_AXIS | DIDFT_PSHBUTTON );
    ok( hr == DI_OK, "EnumObjects returned %#x\n", hr );
    ok( res == 8, "got %u expected %u\n", res, 8 );
    hr = IDirectInputDevice8_EnumObjects( device, check_objects, &check_objects_params, DIDFT_ALL );
    ok( hr == DI_OK, "EnumObjects returned %#x\n", hr );
    ok( check_objects_params.index >= check_objects_params.expect_count, "missing %u objects\n",
        check_objects_params.expect_count - check_objects_params.index );

    objinst.dwSize = sizeof(DIDEVICEOBJECTINSTANCEW);
    res = MAKELONG( HID_USAGE_GENERIC_X, HID_USAGE_PAGE_GENERIC );
    hr = IDirectInputDevice8_GetObjectInfo( device, &objinst, res, DIPH_BYUSAGE );
    ok( hr == DIERR_UNSUPPORTED, "GetObjectInfo returned: %#x\n", hr );

    hr = IDirectInputDevice8_GetObjectInfo( device, &objinst, DIMOFS_Y, DIPH_BYOFFSET );
    ok( hr == DI_OK, "GetObjectInfo returned: %#x\n", hr );

    check_member( objinst, expect_objects[1], "%u", dwSize );
    check_member_guid( objinst, expect_objects[1], guidType );
    check_member( objinst, expect_objects[1], "%#x", dwOfs );
    check_member( objinst, expect_objects[1], "%#x", dwType );
    check_member( objinst, expect_objects[1], "%#x", dwFlags );
    if (!localized) check_member_wstr( objinst, expect_objects[1], tszName );
    check_member( objinst, expect_objects[1], "%u", dwFFMaxForce );
    check_member( objinst, expect_objects[1], "%u", dwFFForceResolution );
    check_member( objinst, expect_objects[1], "%u", wCollectionNumber );
    check_member( objinst, expect_objects[1], "%u", wDesignatorIndex );
    check_member( objinst, expect_objects[1], "%#04x", wUsagePage );
    check_member( objinst, expect_objects[1], "%#04x", wUsage );
    check_member( objinst, expect_objects[1], "%#04x", dwDimension );
    check_member( objinst, expect_objects[1], "%#04x", wExponent );
    check_member( objinst, expect_objects[1], "%u", wReportId );

    hr = IDirectInputDevice8_GetObjectInfo( device, &objinst, 7, DIPH_BYOFFSET );
    ok( hr == DIERR_NOTFOUND, "GetObjectInfo returned: %#x\n", hr );
    res = DIDFT_TGLBUTTON | DIDFT_MAKEINSTANCE( 3 );
    hr = IDirectInputDevice8_GetObjectInfo( device, &objinst, res, DIPH_BYID );
    ok( hr == DIERR_NOTFOUND, "GetObjectInfo returned: %#x\n", hr );
    res = DIDFT_PSHBUTTON | DIDFT_MAKEINSTANCE( 3 );
    hr = IDirectInputDevice8_GetObjectInfo( device, &objinst, res, DIPH_BYID );
    ok( hr == DI_OK, "GetObjectInfo returned: %#x\n", hr );

    check_member( objinst, expect_objects[3], "%u", dwSize );
    check_member_guid( objinst, expect_objects[3], guidType );
    check_member( objinst, expect_objects[3], "%#x", dwOfs );
    check_member( objinst, expect_objects[3], "%#x", dwType );
    check_member( objinst, expect_objects[3], "%#x", dwFlags );
    if (!localized) check_member_wstr( objinst, expect_objects[3], tszName );
    check_member( objinst, expect_objects[3], "%u", dwFFMaxForce );
    check_member( objinst, expect_objects[3], "%u", dwFFForceResolution );
    check_member( objinst, expect_objects[3], "%u", wCollectionNumber );
    check_member( objinst, expect_objects[3], "%u", wDesignatorIndex );
    check_member( objinst, expect_objects[3], "%#04x", wUsagePage );
    check_member( objinst, expect_objects[3], "%#04x", wUsage );
    check_member( objinst, expect_objects[3], "%#04x", dwDimension );
    check_member( objinst, expect_objects[3], "%#04x", wExponent );
    check_member( objinst, expect_objects[3], "%u", wReportId );

    ref = IDirectInputDevice8_Release( device );
    ok( ref == 0, "Release returned %d\n", ref );

    ref = IDirectInput8_Release( di );
    ok( ref == 0, "Release returned %d\n", ref );
}

static void test_keyboard_info(void)
{
    static const DIDEVCAPS expect_caps =
    {
        .dwSize = sizeof(DIDEVCAPS),
        .dwFlags = DIDC_ATTACHED | DIDC_EMULATED,
        .dwDevType = (DI8DEVTYPEKEYBOARD_PCENH << 8) | DI8DEVTYPE_KEYBOARD,
        .dwButtons = 128,
    };
    const DIDEVICEINSTANCEW expect_devinst =
    {
        .dwSize = sizeof(DIDEVICEINSTANCEW),
        .guidInstance = GUID_SysKeyboard,
        .guidProduct = GUID_SysKeyboard,
        .dwDevType = (DI8DEVTYPEKEYBOARD_PCENH << 8) | DI8DEVTYPE_KEYBOARD,
        .tszInstanceName = L"Keyboard",
        .tszProductName = L"Keyboard",
        .guidFFDriver = GUID_NULL,
    };

    DIDEVICEOBJECTINSTANCEW expect_objects[] =
    {
        {
            .dwSize = sizeof(DIDEVICEOBJECTINSTANCEW),
            .guidType = GUID_Key,
            .dwOfs = DIK_ESCAPE,
            .dwType = DIDFT_PSHBUTTON|DIDFT_MAKEINSTANCE(DIK_ESCAPE),
            .tszName = L"Esc",
        },
        {
            .dwSize = sizeof(DIDEVICEOBJECTINSTANCEW),
            .guidType = GUID_Key,
            .dwOfs = DIK_1,
            .dwType = DIDFT_PSHBUTTON|DIDFT_MAKEINSTANCE(DIK_1),
            .tszName = L"1",
        },
        {
            .dwSize = sizeof(DIDEVICEOBJECTINSTANCEW),
            .guidType = GUID_Key,
            .dwOfs = DIK_2,
            .dwType = DIDFT_PSHBUTTON|DIDFT_MAKEINSTANCE(DIK_2),
            .tszName = L"2",
        },
        {
            .dwSize = sizeof(DIDEVICEOBJECTINSTANCEW),
            .guidType = GUID_Key,
            .dwOfs = DIK_3,
            .dwType = DIDFT_PSHBUTTON|DIDFT_MAKEINSTANCE(DIK_3),
            .tszName = L"3",
        },
        {
            .dwSize = sizeof(DIDEVICEOBJECTINSTANCEW),
            .guidType = GUID_Key,
            .dwOfs = DIK_4,
            .dwType = DIDFT_PSHBUTTON|DIDFT_MAKEINSTANCE(DIK_4),
            .tszName = L"4",
        },
    };
    struct check_objects_todos todo_objects[ARRAY_SIZE(expect_objects)] = {{0}};
    struct check_objects_params check_objects_params =
    {
        .stop_count = ARRAY_SIZE(expect_objects),
        .expect_count = ARRAY_SIZE(expect_objects),
        .expect_objs = expect_objects,
        .todo_objs = todo_objects,
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
    DIDEVICEOBJECTINSTANCEW objinst = {0};
    DIDEVICEINSTANCEW devinst = {0};
    IDirectInputDevice8W *device;
    DIDEVCAPS caps = {0};
    IDirectInput8W *di;
    ULONG res, ref;
    HRESULT hr;
    GUID guid;

    localized = TRUE; /* Skip name tests, Wine sometimes succeeds depending on the host key names */

    hr = DirectInput8Create( instance, DIRECTINPUT_VERSION, &IID_IDirectInput8W, (void **)&di, NULL );
    ok( hr == DI_OK, "DirectInput8Create returned %#x\n", hr );
    hr = IDirectInput8_CreateDevice( di, &GUID_SysKeyboard, &device, NULL );
    ok( hr == DI_OK, "CreateDevice returned %#x\n", hr );

    hr = IDirectInputDevice8_Initialize( device, instance, DIRECTINPUT_VERSION, &GUID_SysKeyboardEm );
    ok( hr == DI_OK, "Initialize returned %#x\n", hr );
    guid = GUID_SysKeyboardEm;
    memset( &devinst, 0, sizeof(devinst) );
    devinst.dwSize = sizeof(DIDEVICEINSTANCEW);
    hr = IDirectInputDevice8_GetDeviceInfo( device, &devinst );
    ok( hr == DI_OK, "GetDeviceInfo returned %#x\n", hr );
    ok( IsEqualGUID( &guid, &GUID_SysKeyboardEm ), "got %s expected %s\n", debugstr_guid( &guid ),
        debugstr_guid( &GUID_SysKeyboardEm ) );

    hr = IDirectInputDevice8_Initialize( device, instance, DIRECTINPUT_VERSION, &GUID_SysKeyboard );
    ok( hr == DI_OK, "Initialize returned %#x\n", hr );

    memset( &devinst, 0, sizeof(devinst) );
    devinst.dwSize = sizeof(DIDEVICEINSTANCEW);
    hr = IDirectInputDevice8_GetDeviceInfo( device, &devinst );
    ok( hr == DI_OK, "GetDeviceInfo returned %#x\n", hr );
    check_member( devinst, expect_devinst, "%d", dwSize );
    check_member_guid( devinst, expect_devinst, guidInstance );
    check_member_guid( devinst, expect_devinst, guidProduct );
    check_member( devinst, expect_devinst, "%#x", dwDevType );
    if (!localized) check_member_wstr( devinst, expect_devinst, tszInstanceName );
    if (!localized) todo_wine check_member_wstr( devinst, expect_devinst, tszProductName );
    check_member_guid( devinst, expect_devinst, guidFFDriver );
    check_member( devinst, expect_devinst, "%04x", wUsagePage );
    check_member( devinst, expect_devinst, "%04x", wUsage );

    caps.dwSize = sizeof(DIDEVCAPS);
    hr = IDirectInputDevice8_GetCapabilities( device, &caps );
    ok( hr == DI_OK, "GetCapabilities returned %#x\n", hr );
    check_member( caps, expect_caps, "%d", dwSize );
    check_member( caps, expect_caps, "%#x", dwFlags );
    check_member( caps, expect_caps, "%#x", dwDevType );
    check_member( caps, expect_caps, "%d", dwAxes );
    todo_wine
    check_member( caps, expect_caps, "%d", dwButtons );
    check_member( caps, expect_caps, "%d", dwPOVs );
    check_member( caps, expect_caps, "%d", dwFFSamplePeriod );
    check_member( caps, expect_caps, "%d", dwFFMinTimeResolution );
    todo_wine
    check_member( caps, expect_caps, "%d", dwFirmwareRevision );
    todo_wine
    check_member( caps, expect_caps, "%d", dwHardwareRevision );
    check_member( caps, expect_caps, "%d", dwFFDriverVersion );

    prop_dword.diph.dwHow = DIPH_BYOFFSET;
    prop_dword.diph.dwObj = 1;
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_GRANULARITY, &prop_dword.diph );
    ok( hr == DIERR_NOTFOUND, "GetProperty DIPROP_GRANULARITY returned %#x\n", hr );
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_DEADZONE, &prop_dword.diph );
    ok( hr == DIERR_NOTFOUND, "GetProperty DIPROP_DEADZONE returned %#x\n", hr );
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_SATURATION, &prop_dword.diph );
    ok( hr == DIERR_NOTFOUND, "GetProperty DIPROP_SATURATION returned %#x\n", hr );
    prop_range.diph.dwHow = DIPH_BYOFFSET;
    prop_range.diph.dwObj = 1;
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_RANGE, &prop_range.diph );
    ok( hr == DIERR_NOTFOUND, "GetProperty DIPROP_RANGE returned %#x\n", hr );

    prop_dword.diph.dwHow = DIPH_DEVICE;
    prop_dword.diph.dwObj = 0;
    prop_dword.dwData = 0xdeadbeef;
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_AXISMODE, &prop_dword.diph );
    todo_wine
    ok( hr == DI_OK, "GetProperty DIPROP_AXISMODE returned %#x\n", hr );
    todo_wine
    ok( prop_dword.dwData == DIPROPAXISMODE_ABS, "got %u expected %u\n", prop_dword.dwData, DIPROPAXISMODE_ABS );
    prop_dword.dwData = 0xdeadbeef;
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_BUFFERSIZE, &prop_dword.diph );
    ok( hr == DI_OK, "GetProperty DIPROP_BUFFERSIZE returned %#x\n", hr );
    ok( prop_dword.dwData == 0, "got %#x expected %#x\n", prop_dword.dwData, 0 );
    prop_dword.dwData = 0xdeadbeef;
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_FFGAIN, &prop_dword.diph );
    ok( hr == DI_OK, "GetProperty DIPROP_FFGAIN returned %#x\n", hr );
    ok( prop_dword.dwData == 10000, "got %u expected %u\n", prop_dword.dwData, 10000 );

    hr = IDirectInputDevice8_SetDataFormat( device, &c_dfDIKeyboard );
    ok( hr == DI_OK, "SetDataFormat returned %#x\n", hr );

    prop_dword.diph.dwHow = DIPH_DEVICE;
    prop_dword.diph.dwObj = 0;
    prop_dword.dwData = 0xdeadbeef;
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_AXISMODE, &prop_dword.diph );
    todo_wine
    ok( hr == DI_OK, "GetProperty DIPROP_AXISMODE returned %#x\n", hr );
    todo_wine
    ok( prop_dword.dwData == DIPROPAXISMODE_REL, "got %u expected %u\n", prop_dword.dwData, DIPROPAXISMODE_ABS );

    prop_dword.dwData = 0xdeadbeef;
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_VIDPID, &prop_dword.diph );
    ok( hr == DIERR_UNSUPPORTED, "GetProperty DIPROP_VIDPID returned %#x\n", hr );
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_GUIDANDPATH, &prop_guid_path.diph );
    ok( hr == DIERR_UNSUPPORTED, "GetProperty DIPROP_GUIDANDPATH returned %#x\n", hr );
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_INSTANCENAME, &prop_string.diph );
    ok( hr == DIERR_UNSUPPORTED, "GetProperty DIPROP_INSTANCENAME returned %#x\n", hr );
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_PRODUCTNAME, &prop_string.diph );
    ok( hr == DIERR_UNSUPPORTED, "GetProperty DIPROP_PRODUCTNAME returned %#x\n", hr );
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_TYPENAME, &prop_string.diph );
    ok( hr == DIERR_UNSUPPORTED, "GetProperty DIPROP_TYPENAME returned %#x\n", hr );
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_USERNAME, &prop_string.diph );
    ok( hr == S_FALSE, "GetProperty DIPROP_USERNAME returned %#x\n", hr );
    ok( !wcscmp( prop_string.wsz, L"" ), "got user %s\n", debugstr_w(prop_string.wsz) );

    hr = IDirectInputDevice8_GetProperty( device, DIPROP_JOYSTICKID, &prop_dword.diph );
    ok( hr == DIERR_UNSUPPORTED, "GetProperty DIPROP_VIDPID returned %#x\n", hr );

    hr = IDirectInputDevice8_GetProperty( device, DIPROP_CALIBRATION, &prop_dword.diph );
    ok( hr == DIERR_INVALIDPARAM, "GetProperty DIPROP_CALIBRATION returned %#x\n", hr );
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_AUTOCENTER, &prop_dword.diph );
    ok( hr == DIERR_UNSUPPORTED, "GetProperty DIPROP_AUTOCENTER returned %#x\n", hr );
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_DEADZONE, &prop_dword.diph );
    ok( hr == DIERR_UNSUPPORTED, "GetProperty DIPROP_DEADZONE returned %#x\n", hr );
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_FFLOAD, &prop_dword.diph );
    ok( hr == DIERR_UNSUPPORTED, "GetProperty DIPROP_FFLOAD returned %#x\n", hr );
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_GRANULARITY, &prop_dword.diph );
    ok( hr == DIERR_UNSUPPORTED, "GetProperty DIPROP_GRANULARITY returned %#x\n", hr );

    prop_dword.diph.dwHow = DIPH_BYUSAGE;
    prop_dword.diph.dwObj = MAKELONG( HID_USAGE_KEYBOARD_LCTRL, HID_USAGE_PAGE_KEYBOARD );
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_GRANULARITY, &prop_dword.diph );
    ok( hr == DIERR_UNSUPPORTED, "GetProperty DIPROP_GRANULARITY returned %#x\n", hr );

    prop_dword.diph.dwHow = DIPH_BYOFFSET;
    prop_dword.diph.dwObj = 1;
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_GRANULARITY, &prop_dword.diph );
    ok( hr == DIERR_UNSUPPORTED, "GetProperty DIPROP_GRANULARITY returned %#x\n", hr );
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_DEADZONE, &prop_dword.diph );
    ok( hr == DIERR_UNSUPPORTED, "GetProperty DIPROP_DEADZONE returned %#x\n", hr );
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_SATURATION, &prop_dword.diph );
    ok( hr == DIERR_UNSUPPORTED, "GetProperty DIPROP_SATURATION returned %#x\n", hr );
    prop_range.diph.dwHow = DIPH_BYOFFSET;
    prop_range.diph.dwObj = 1;
    hr = IDirectInputDevice8_GetProperty( device, DIPROP_RANGE, &prop_range.diph );
    ok( hr == DIERR_UNSUPPORTED, "GetProperty DIPROP_RANGE returned %#x\n", hr );

    res = 0;
    hr = IDirectInputDevice8_EnumObjects( device, check_object_count, &res, DIDFT_AXIS | DIDFT_PSHBUTTON );
    ok( hr == DI_OK, "EnumObjects returned %#x\n", hr );
    if (!localized) todo_wine ok( res == 127, "got %u expected %u\n", res, 127 );
    hr = IDirectInputDevice8_EnumObjects( device, check_objects, &check_objects_params, DIDFT_ALL );
    ok( hr == DI_OK, "EnumObjects returned %#x\n", hr );
    ok( check_objects_params.index >= check_objects_params.expect_count, "missing %u objects\n",
        check_objects_params.expect_count - check_objects_params.index );

    objinst.dwSize = sizeof(DIDEVICEOBJECTINSTANCEW);
    res = MAKELONG( HID_USAGE_KEYBOARD_LCTRL, HID_USAGE_PAGE_KEYBOARD );
    hr = IDirectInputDevice8_GetObjectInfo( device, &objinst, res, DIPH_BYUSAGE );
    ok( hr == DIERR_UNSUPPORTED, "GetObjectInfo returned: %#x\n", hr );

    hr = IDirectInputDevice8_GetObjectInfo( device, &objinst, 2, DIPH_BYOFFSET );
    ok( hr == DI_OK, "GetObjectInfo returned: %#x\n", hr );

    check_member( objinst, expect_objects[1], "%u", dwSize );
    check_member_guid( objinst, expect_objects[1], guidType );
    check_member( objinst, expect_objects[1], "%#x", dwOfs );
    check_member( objinst, expect_objects[1], "%#x", dwType );
    check_member( objinst, expect_objects[1], "%#x", dwFlags );
    if (!localized) check_member_wstr( objinst, expect_objects[1], tszName );
    check_member( objinst, expect_objects[1], "%u", dwFFMaxForce );
    check_member( objinst, expect_objects[1], "%u", dwFFForceResolution );
    check_member( objinst, expect_objects[1], "%u", wCollectionNumber );
    check_member( objinst, expect_objects[1], "%u", wDesignatorIndex );
    check_member( objinst, expect_objects[1], "%#04x", wUsagePage );
    check_member( objinst, expect_objects[1], "%#04x", wUsage );
    check_member( objinst, expect_objects[1], "%#04x", dwDimension );
    check_member( objinst, expect_objects[1], "%#04x", wExponent );
    check_member( objinst, expect_objects[1], "%u", wReportId );

    hr = IDirectInputDevice8_GetObjectInfo( device, &objinst, 423, DIPH_BYOFFSET );
    ok( hr == DIERR_NOTFOUND, "GetObjectInfo returned: %#x\n", hr );
    res = DIDFT_TGLBUTTON | DIDFT_MAKEINSTANCE( 3 );
    hr = IDirectInputDevice8_GetObjectInfo( device, &objinst, res, DIPH_BYID );
    ok( hr == DIERR_NOTFOUND, "GetObjectInfo returned: %#x\n", hr );
    res = DIDFT_PSHBUTTON | DIDFT_MAKEINSTANCE( 3 );
    hr = IDirectInputDevice8_GetObjectInfo( device, &objinst, res, DIPH_BYID );
    ok( hr == DI_OK, "GetObjectInfo returned: %#x\n", hr );

    check_member( objinst, expect_objects[2], "%u", dwSize );
    check_member_guid( objinst, expect_objects[2], guidType );
    check_member( objinst, expect_objects[2], "%#x", dwOfs );
    check_member( objinst, expect_objects[2], "%#x", dwType );
    check_member( objinst, expect_objects[2], "%#x", dwFlags );
    if (!localized) check_member_wstr( objinst, expect_objects[2], tszName );
    check_member( objinst, expect_objects[2], "%u", dwFFMaxForce );
    check_member( objinst, expect_objects[2], "%u", dwFFForceResolution );
    check_member( objinst, expect_objects[2], "%u", wCollectionNumber );
    check_member( objinst, expect_objects[2], "%u", wDesignatorIndex );
    check_member( objinst, expect_objects[2], "%#04x", wUsagePage );
    check_member( objinst, expect_objects[2], "%#04x", wUsage );
    check_member( objinst, expect_objects[2], "%#04x", dwDimension );
    check_member( objinst, expect_objects[2], "%#04x", wExponent );
    check_member( objinst, expect_objects[2], "%u", wReportId );

    ref = IDirectInputDevice8_Release( device );
    ok( ref == 0, "Release returned %d\n", ref );

    ref = IDirectInput8_Release( di );
    ok( ref == 0, "Release returned %d\n", ref );
}

START_TEST(device)
{
    instance = GetModuleHandleW( NULL );

    CoInitialize(NULL);

    test_mouse_info();
    test_keyboard_info();
    test_action_mapping();
    test_save_settings();
    test_mouse_keyboard();
    test_keyboard_events();
    test_appdata_property();

    CoUninitialize();
}
