/*
 * Misc USER functions
 *
 * Copyright 1995 Thomas Sandford
 * Copyright 1997 Marcus Meissner
 * Copyright 1998 Turchanov Sergey
 * Copyright 2019 Micah N Gorrell for CodeWeavers
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

#include "config.h"

#include <stdarg.h>

#include "windef.h"
#include "wine/windef16.h"
#include "winbase.h"
#include "wingdi.h"
#include "controls.h"
#include "user_private.h"

#include "wine/unicode.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(win);

#define IMM_INIT_MAGIC 0x19650412
static HWND (WINAPI *imm_get_ui_window)(HKL);
BOOL (WINAPI *imm_register_window)(HWND) = NULL;
void (WINAPI *imm_unregister_window)(HWND) = NULL;
void (WINAPI *imm_activate_window)(HWND) = NULL;

/* MSIME messages */
static UINT WM_MSIME_SERVICE;
static UINT WM_MSIME_RECONVERTOPTIONS;
static UINT WM_MSIME_MOUSE;
static UINT WM_MSIME_RECONVERTREQUEST;
static UINT WM_MSIME_RECONVERT;
static UINT WM_MSIME_QUERYPOSITION;
static UINT WM_MSIME_DOCUMENTFEED;

/* USER signal proc flags and codes */
/* See UserSignalProc for comments */
#define USIG_FLAGS_WIN32          0x0001
#define USIG_FLAGS_GUI            0x0002
#define USIG_FLAGS_FEEDBACK       0x0004
#define USIG_FLAGS_FAULT          0x0008

#define USIG_DLL_UNLOAD_WIN16     0x0001
#define USIG_DLL_UNLOAD_WIN32     0x0002
#define USIG_FAULT_DIALOG_PUSH    0x0003
#define USIG_FAULT_DIALOG_POP     0x0004
#define USIG_DLL_UNLOAD_ORPHANS   0x0005
#define USIG_THREAD_INIT          0x0010
#define USIG_THREAD_EXIT          0x0020
#define USIG_PROCESS_CREATE       0x0100
#define USIG_PROCESS_INIT         0x0200
#define USIG_PROCESS_EXIT         0x0300
#define USIG_PROCESS_DESTROY      0x0400
#define USIG_PROCESS_RUNNING      0x0500
#define USIG_PROCESS_LOADED       0x0600

/***********************************************************************
 *		SignalProc32 (USER.391)
 *		UserSignalProc (USER32.@)
 *
 * The exact meaning of the USER signals is undocumented, but this
 * should cover the basic idea:
 *
 * USIG_DLL_UNLOAD_WIN16
 *     This is sent when a 16-bit module is unloaded.
 *
 * USIG_DLL_UNLOAD_WIN32
 *     This is sent when a 32-bit module is unloaded.
 *
 * USIG_DLL_UNLOAD_ORPHANS
 *     This is sent after the last Win3.1 module is unloaded,
 *     to allow removal of orphaned menus.
 *
 * USIG_FAULT_DIALOG_PUSH
 * USIG_FAULT_DIALOG_POP
 *     These are called to allow USER to prepare for displaying a
 *     fault dialog, even though the fault might have happened while
 *     inside a USER critical section.
 *
 * USIG_THREAD_INIT
 *     This is called from the context of a new thread, as soon as it
 *     has started to run.
 *
 * USIG_THREAD_EXIT
 *     This is called, still in its context, just before a thread is
 *     about to terminate.
 *
 * USIG_PROCESS_CREATE
 *     This is called, in the parent process context, after a new process
 *     has been created.
 *
 * USIG_PROCESS_INIT
 *     This is called in the new process context, just after the main thread
 *     has started execution (after the main thread's USIG_THREAD_INIT has
 *     been sent).
 *
 * USIG_PROCESS_LOADED
 *     This is called after the executable file has been loaded into the
 *     new process context.
 *
 * USIG_PROCESS_RUNNING
 *     This is called immediately before the main entry point is called.
 *
 * USIG_PROCESS_EXIT
 *     This is called in the context of a process that is about to
 *     terminate (but before the last thread's USIG_THREAD_EXIT has
 *     been sent).
 *
 * USIG_PROCESS_DESTROY
 *     This is called after a process has terminated.
 *
 *
 * The meaning of the dwFlags bits is as follows:
 *
 * USIG_FLAGS_WIN32
 *     Current process is 32-bit.
 *
 * USIG_FLAGS_GUI
 *     Current process is a (Win32) GUI process.
 *
 * USIG_FLAGS_FEEDBACK
 *     Current process needs 'feedback' (determined from the STARTUPINFO
 *     flags STARTF_FORCEONFEEDBACK / STARTF_FORCEOFFFEEDBACK).
 *
 * USIG_FLAGS_FAULT
 *     The signal is being sent due to a fault.
 */
WORD WINAPI UserSignalProc( UINT uCode, DWORD dwThreadOrProcessID,
                            DWORD dwFlags, HMODULE16 hModule )
{
    FIXME("(%04x, %08x, %04x, %04x)\n",
          uCode, dwThreadOrProcessID, dwFlags, hModule );
    /* FIXME: Should chain to GdiSignalProc now. */
    return 0;
}


/**********************************************************************
 * SetLastErrorEx [USER32.@]
 *
 * Sets the last-error code.
 *
 * RETURNS
 *    None.
 */
void WINAPI SetLastErrorEx(
    DWORD error, /* [in] Per-thread error code */
    DWORD type)  /* [in] Error type */
{
    TRACE("(0x%08x, 0x%08x)\n", error,type);
    switch(type) {
        case 0:
            break;
        case SLE_ERROR:
        case SLE_MINORERROR:
        case SLE_WARNING:
            /* Fall through for now */
        default:
            FIXME("(error=%08x, type=%08x): Unhandled type\n", error,type);
            break;
    }
    SetLastError( error );
}

/******************************************************************************
 * GetAltTabInfoA [USER32.@]
 */
BOOL WINAPI GetAltTabInfoA(HWND hwnd, int iItem, PALTTABINFO pati, LPSTR pszItemText, UINT cchItemText)
{
    FIXME("(%p, 0x%08x, %p, %p, 0x%08x)\n", hwnd, iItem, pati, pszItemText, cchItemText);
    return FALSE;
}

/******************************************************************************
 * GetAltTabInfoW [USER32.@]
 */
BOOL WINAPI GetAltTabInfoW(HWND hwnd, int iItem, PALTTABINFO pati, LPWSTR pszItemText, UINT cchItemText)
{
    FIXME("(%p, 0x%08x, %p, %p, 0x%08x)\n", hwnd, iItem, pati, pszItemText, cchItemText);
    return FALSE;
}

/******************************************************************************
 * SetDebugErrorLevel [USER32.@]
 * Sets the minimum error level for generating debugging events
 *
 * PARAMS
 *    dwLevel [I] Debugging error level
 *
 * RETURNS
 *    Nothing.
 */
VOID WINAPI SetDebugErrorLevel( DWORD dwLevel )
{
    FIXME("(%d): stub\n", dwLevel);
}


/***********************************************************************
 *		SetWindowStationUser (USER32.@)
 */
DWORD WINAPI SetWindowStationUser(DWORD x1,DWORD x2)
{
    FIXME("(0x%08x,0x%08x),stub!\n",x1,x2);
    return 1;
}

/***********************************************************************
 *		RegisterLogonProcess (USER32.@)
 */
DWORD WINAPI RegisterLogonProcess(HANDLE hprocess,BOOL x)
{
    FIXME("(%p,%d),stub!\n",hprocess,x);
    return 1;
}

/***********************************************************************
 *		SetLogonNotifyWindow (USER32.@)
 */
DWORD WINAPI SetLogonNotifyWindow(HWINSTA hwinsta,HWND hwnd)
{
    FIXME("(%p,%p),stub!\n",hwinsta,hwnd);
    return 1;
}

/***********************************************************************
 *		RegisterSystemThread (USER32.@)
 */
void WINAPI RegisterSystemThread(DWORD flags, DWORD reserved)
{
    FIXME("(%08x, %08x)\n", flags, reserved);
}

/***********************************************************************
 *           RegisterShellHookWindow			[USER32.@]
 */
BOOL WINAPI RegisterShellHookWindow(HWND hWnd)
{
    FIXME("(%p): stub\n", hWnd);
    return FALSE;
}


/***********************************************************************
 *           DeregisterShellHookWindow			[USER32.@]
 */
BOOL WINAPI DeregisterShellHookWindow(HWND hWnd)
{
    FIXME("(%p): stub\n", hWnd);
    return FALSE;
}


/***********************************************************************
 *           RegisterTasklist   			[USER32.@]
 */
DWORD WINAPI RegisterTasklist (DWORD x)
{
    FIXME("0x%08x\n",x);
    return TRUE;
}

/***********************************************************************
 *           GetAppCompatFlags   (USER32.@)
 */
DWORD WINAPI GetAppCompatFlags( HTASK hTask )
{
    FIXME("(%p) stub\n", hTask);
    return 0;
}

/***********************************************************************
 *           GetAppCompatFlags2   (USER32.@)
 */
DWORD WINAPI GetAppCompatFlags2( HTASK hTask )
{
    FIXME("(%p) stub\n", hTask);
    return 0;
}


/***********************************************************************
 *           AlignRects   (USER32.@)
 */
BOOL WINAPI AlignRects(LPRECT rect, DWORD b, DWORD c, DWORD d)
{
    FIXME("(%p, %d, %d, %d): stub\n", rect, b, c, d);
    if (rect)
        FIXME("rect: %s\n", wine_dbgstr_rect(rect));
    /* Calls OffsetRect */
    return FALSE;
}


/***********************************************************************
 *		LoadLocalFonts (USER32.@)
 */
VOID WINAPI LoadLocalFonts(VOID)
{
    /* are loaded. */
    return;
}


/***********************************************************************
 *		User32InitializeImmEntryTable
 */
BOOL WINAPI User32InitializeImmEntryTable(DWORD magic)
{
    static const WCHAR imm32_dllW[] = {'i','m','m','3','2','.','d','l','l',0};
    HMODULE imm32 = GetModuleHandleW(imm32_dllW);

    TRACE("(%x)\n", magic);

    if (!imm32 || magic != IMM_INIT_MAGIC)
        return FALSE;

    if (imm_get_ui_window)
        return TRUE;

    WM_MSIME_SERVICE = RegisterWindowMessageA("MSIMEService");
    WM_MSIME_RECONVERTOPTIONS = RegisterWindowMessageA("MSIMEReconvertOptions");
    WM_MSIME_MOUSE = RegisterWindowMessageA("MSIMEMouseOperation");
    WM_MSIME_RECONVERTREQUEST = RegisterWindowMessageA("MSIMEReconvertRequest");
    WM_MSIME_RECONVERT = RegisterWindowMessageA("MSIMEReconvert");
    WM_MSIME_QUERYPOSITION = RegisterWindowMessageA("MSIMEQueryPosition");
    WM_MSIME_DOCUMENTFEED = RegisterWindowMessageA("MSIMEDocumentFeed");

    /* this part is not compatible with native imm32.dll */
    imm_get_ui_window = (void*)GetProcAddress(imm32, "__wine_get_ui_window");
    imm_register_window = (void*)GetProcAddress(imm32, "__wine_register_window");
    imm_unregister_window = (void*)GetProcAddress(imm32, "__wine_unregister_window");
    imm_activate_window = (void*)GetProcAddress(imm32, "__wine_activate_window");
    if (!imm_get_ui_window)
        FIXME("native imm32.dll not supported\n");
    return TRUE;
}

/**********************************************************************
 * WINNLSGetIMEHotkey [USER32.@]
 *
 */
UINT WINAPI WINNLSGetIMEHotkey(HWND hwnd)
{
    FIXME("hwnd %p: stub!\n", hwnd);
    return 0; /* unknown */
}

/**********************************************************************
 * WINNLSEnableIME [USER32.@]
 *
 */
BOOL WINAPI WINNLSEnableIME(HWND hwnd, BOOL enable)
{
    FIXME("hwnd %p enable %d: stub!\n", hwnd, enable);
    return TRUE; /* success (?) */
}

/**********************************************************************
 * WINNLSGetEnableStatus [USER32.@]
 *
 */
BOOL WINAPI WINNLSGetEnableStatus(HWND hwnd)
{
    FIXME("hwnd %p: stub!\n", hwnd);
    return TRUE; /* success (?) */
}

/**********************************************************************
 * SendIMEMessageExA [USER32.@]
 *
 */
LRESULT WINAPI SendIMEMessageExA(HWND hwnd, LPARAM lparam)
{
  FIXME("(%p,%lx): stub\n", hwnd, lparam);
  SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
  return 0;
}

/**********************************************************************
 * SendIMEMessageExW [USER32.@]
 *
 */
LRESULT WINAPI SendIMEMessageExW(HWND hwnd, LPARAM lparam)
{
  FIXME("(%p,%lx): stub\n", hwnd, lparam);
  SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
  return 0;
}

/**********************************************************************
 * DisableProcessWindowsGhosting [USER32.@]
 *
 */
VOID WINAPI DisableProcessWindowsGhosting(VOID)
{
  FIXME(": stub\n");
  SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
  return;
}

/**********************************************************************
 * UserHandleGrantAccess [USER32.@]
 *
 */
BOOL WINAPI UserHandleGrantAccess(HANDLE handle, HANDLE job, BOOL grant)
{
    FIXME("(%p,%p,%d): stub\n", handle, job, grant);
    return TRUE;
}

/**********************************************************************
 * RegisterPowerSettingNotification [USER32.@]
 */
HPOWERNOTIFY WINAPI RegisterPowerSettingNotification(HANDLE recipient, const GUID *guid, DWORD flags)
{
    FIXME("(%p,%s,%x): stub\n", recipient, debugstr_guid(guid), flags);
    return (HPOWERNOTIFY)0xdeadbeef;
}

/**********************************************************************
 * UnregisterPowerSettingNotification [USER32.@]
 */
BOOL WINAPI UnregisterPowerSettingNotification(HPOWERNOTIFY handle)
{
    FIXME("(%p): stub\n", handle);
    return TRUE;
}

/*****************************************************************************
 * GetGestureConfig (USER32.@)
 */
BOOL WINAPI GetGestureConfig( HWND hwnd, DWORD reserved, DWORD flags, UINT *count, GESTURECONFIG *config, UINT size )
{
    FIXME("(%p %08x %08x %p %p %u): stub\n", hwnd, reserved, flags, count, config, size);
    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return FALSE;
}

/**********************************************************************
 * SetGestureConfig [USER32.@]
 */
BOOL WINAPI SetGestureConfig( HWND hwnd, DWORD reserved, UINT id, PGESTURECONFIG config, UINT size )
{
    FIXME("(%p %08x %u %p %u): stub\n", hwnd, reserved, id, config, size);
    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return FALSE;
}

/**********************************************************************
 * IsTouchWindow [USER32.@]
 */
BOOL WINAPI IsTouchWindow( HWND hwnd, PULONG flags )
{
    FIXME("(%p %p): stub\n", hwnd, flags);
    return FALSE;
}

/**********************************************************************
 * IsWindowRedirectedForPrint [USER32.@]
 */
BOOL WINAPI IsWindowRedirectedForPrint( HWND hwnd )
{
    FIXME("(%p): stub\n", hwnd);
    return FALSE;
}

/**********************************************************************
 * RegisterPointerDeviceNotifications [USER32.@]
 */
BOOL WINAPI RegisterPointerDeviceNotifications(HWND hwnd, BOOL notifyrange)
{
    FIXME("(%p %d): stub\n", hwnd, notifyrange);
    return TRUE;
}

/**********************************************************************
 * GetPointerDevices [USER32.@]
 */
BOOL WINAPI GetPointerDevices(UINT32 *device_count, POINTER_DEVICE_INFO *devices)
{
    FIXME("(%p %p): partial stub\n", device_count, devices);

    if (!device_count)
        return FALSE;

    if (devices)
        return FALSE;

    *device_count = 0;
    return TRUE;
}

/**********************************************************************
 * RegisterTouchHitTestingWindow [USER32.@]
 */
BOOL WINAPI RegisterTouchHitTestingWindow(HWND hwnd, ULONG value)
{
    FIXME("(%p %d): stub\n", hwnd, value);
    return TRUE;
}

/**********************************************************************
 * GetPointerType [USER32.@]
 */
BOOL WINAPI GetPointerType(UINT32 id, POINTER_INPUT_TYPE *type)
{
    FIXME("(%d %p): stub\n", id, type);

    if(!id || !type)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    *type = PT_MOUSE;
    return TRUE;
}

static const WCHAR imeW[] = {'I','M','E',0};
const struct builtin_class_descr IME_builtin_class =
{
    imeW,               /* name */
    0,                  /* style  */
    WINPROC_IME,        /* proc */
    2*sizeof(LONG_PTR), /* extra */
    IDC_ARROW,          /* cursor */
    0                   /* brush */
};

static BOOL is_ime_ui_msg( UINT msg )
{
    switch(msg) {
    case WM_IME_STARTCOMPOSITION:
    case WM_IME_ENDCOMPOSITION:
    case WM_IME_COMPOSITION:
    case WM_IME_SETCONTEXT:
    case WM_IME_NOTIFY:
    case WM_IME_CONTROL:
    case WM_IME_COMPOSITIONFULL:
    case WM_IME_SELECT:
    case WM_IME_CHAR:
    case WM_IME_REQUEST:
    case WM_IME_KEYDOWN:
    case WM_IME_KEYUP:
        return TRUE;
    default:
        if ((msg == WM_MSIME_RECONVERTOPTIONS) ||
                (msg == WM_MSIME_SERVICE) ||
                (msg == WM_MSIME_MOUSE) ||
                (msg == WM_MSIME_RECONVERTREQUEST) ||
                (msg == WM_MSIME_RECONVERT) ||
                (msg == WM_MSIME_QUERYPOSITION) ||
                (msg == WM_MSIME_DOCUMENTFEED))
            return TRUE;

        return FALSE;
    }
}

LRESULT WINAPI ImeWndProcA( HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
    HWND uiwnd;

    if (msg==WM_CREATE)
        return TRUE;

    if (imm_get_ui_window && is_ime_ui_msg(msg))
    {
        if ((uiwnd = imm_get_ui_window(GetKeyboardLayout(0))))
            return SendMessageA(uiwnd, msg, wParam, lParam);
        return FALSE;
    }

    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

LRESULT WINAPI ImeWndProcW( HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
    HWND uiwnd;

    if (msg==WM_CREATE)
        return TRUE;

    if (imm_get_ui_window && is_ime_ui_msg(msg))
    {
        if ((uiwnd = imm_get_ui_window(GetKeyboardLayout(0))))
            return SendMessageW(uiwnd, msg, wParam, lParam);
        return FALSE;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
