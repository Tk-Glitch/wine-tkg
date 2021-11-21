/*
 * WineCfg Staging panel
 *
 * Copyright 2014 Michael Müller
 * Copyright 2015 Sebastian Lackner
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

#define COBJMACROS

#include <windows.h>

#include "resource.h"
#include "winecfg.h"

/*
 * Command stream multithreading
 */
static BOOL csmt_get(void)
{
    WCHAR *buf = get_reg_key(config_key, L"Direct3D", L"csmt", NULL);
    // (dword=0 csmt=off, dword=1 csmt=on )
    // since we want this toggle to disable upstream's CSMT
    // flip existing csmt dword, returning false if not set.
    BOOL ret = buf ? !*buf : FALSE;
    HeapFree(GetProcessHeap(), 0, buf);
    return ret;
}
static void csmt_set(BOOL status)
{
    if (status)
    {
        // TRUE, we disable upstream's csmt by setting dword to 0
        set_reg_key_dword(config_key, L"Direct3D", L"csmt", 0);
    }
    else
    {
        // FALSE, we remove the csmt key letting wine use its default
        set_reg_key(config_key, "Direct3D", L"csmt", NULL);
    }
}

/*
 * DXVA2
 */
static BOOL vaapi_get(void)
{
    BOOL ret;
    WCHAR *value = get_reg_key(config_key, keypath(L"DXVA2"), L"backend", NULL);
    ret = (value && !wcscmp(value, L"va"));
    HeapFree(GetProcessHeap(), 0, value);
    return ret;
}
static void vaapi_set(BOOL status)
{
    set_reg_key(config_key, keypath(L"DXVA2"), L"backend", status ? L"va" : NULL);
}

/*
 * EAX
 */
static BOOL eax_get(void)
{
    BOOL ret;
    WCHAR *value = get_reg_key(config_key, keypath(L"DirectSound"), L"EAXEnabled", L"N");
    ret = IS_OPTION_TRUE(*value);
    HeapFree(GetProcessHeap(), 0, value);
    return ret;
}
static void eax_set(BOOL status)
{
    set_reg_key(config_key, keypath(L"DirectSound"), L"EAXEnabled", status ? L"Y" : L"N");
}

/*
 * Hide Wine exports from applications
 */
static BOOL hidewine_get(void)
{
    BOOL ret;
    WCHAR *value = get_reg_key(config_key, keypath(L""), L"HideWineExports", L"N");
    ret = IS_OPTION_TRUE(*value);
    HeapFree(GetProcessHeap(), 0, value);
    return ret;
}
static void hidewine_set(BOOL status)
{
    set_reg_key(config_key, keypath(L""), L"HideWineExports", status ? L"Y" : L"N");
}

/*
 * GTK3
 */
static BOOL gtk3_get(void)
{
    BOOL ret;
    WCHAR *value = get_reg_key(config_key, keypath(L""), L"ThemeEngine", NULL);
    ret = (value && !wcsicmp(value, L"GTK"));
    HeapFree(GetProcessHeap(), 0, value);
    return ret;
}
static void gtk3_set(BOOL status)
{
    set_reg_key(config_key, keypath(L""), L"ThemeEngine", status ? L"GTK" : NULL);
}

static void load_staging_settings(HWND dialog)
{
    CheckDlgButton(dialog, IDC_DISABLE_CSMT, csmt_get() ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(dialog, IDC_ENABLE_VAAPI, vaapi_get() ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(dialog, IDC_ENABLE_EAX, eax_get() ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(dialog, IDC_ENABLE_HIDEWINE, hidewine_get() ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(dialog, IDC_ENABLE_GTK3, gtk3_get() ? BST_CHECKED : BST_UNCHECKED);
}

INT_PTR CALLBACK StagingDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_INITDIALOG:
        break;

    case WM_NOTIFY:
        if (((LPNMHDR)lParam)->code == PSN_SETACTIVE)
            load_staging_settings(hDlg);
        break;

    case WM_SHOWWINDOW:
        set_window_title(hDlg);
        break;

    case WM_DESTROY:
        break;

    case WM_COMMAND:
        if (HIWORD(wParam) != BN_CLICKED) break;
        switch (LOWORD(wParam))
        {
        case IDC_DISABLE_CSMT:
            csmt_set(IsDlgButtonChecked(hDlg, IDC_DISABLE_CSMT) == BST_CHECKED);
            SendMessageW(GetParent(hDlg), PSM_CHANGED, 0, 0);
            return TRUE;
        case IDC_ENABLE_VAAPI:
            vaapi_set(IsDlgButtonChecked(hDlg, IDC_ENABLE_VAAPI) == BST_CHECKED);
            SendMessageW(GetParent(hDlg), PSM_CHANGED, 0, 0);
            return TRUE;
        case IDC_ENABLE_EAX:
            eax_set(IsDlgButtonChecked(hDlg, IDC_ENABLE_EAX) == BST_CHECKED);
            SendMessageW(GetParent(hDlg), PSM_CHANGED, 0, 0);
            return TRUE;
        case IDC_ENABLE_HIDEWINE:
            hidewine_set(IsDlgButtonChecked(hDlg, IDC_ENABLE_HIDEWINE) == BST_CHECKED);
            SendMessageW(GetParent(hDlg), PSM_CHANGED, 0, 0);
            return TRUE;
        case IDC_ENABLE_GTK3:
            gtk3_set(IsDlgButtonChecked(hDlg, IDC_ENABLE_GTK3) == BST_CHECKED);
            SendMessageW(GetParent(hDlg), PSM_CHANGED, 0, 0);
            return TRUE;
        }
        break;
    }
    return FALSE;
}
