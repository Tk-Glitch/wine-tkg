/*
 * Regedit main function
 *
 * Copyright (C) 2002 Robert Dickenson <robd@reactos.org>
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

#define WIN32_LEAN_AND_MEAN     /* Exclude rarely-used stuff from Windows headers */
#include <windows.h>
#include <commctrl.h>
#include <stdlib.h>
#include <fcntl.h>
#include "wine/debug.h"

#define REGEDIT_DECLARE_FUNCTIONS
#include "main.h"

WINE_DEFAULT_DEBUG_CHANNEL(regedit);

WCHAR g_pszDefaultValueName[64];

BOOL ProcessCmdLine(WCHAR *cmdline);

static const WCHAR hkey_local_machine[] = {'H','K','E','Y','_','L','O','C','A','L','_','M','A','C','H','I','N','E',0};
static const WCHAR hkey_users[] = {'H','K','E','Y','_','U','S','E','R','S',0};
static const WCHAR hkey_classes_root[] = {'H','K','E','Y','_','C','L','A','S','S','E','S','_','R','O','O','T',0};
static const WCHAR hkey_current_config[] = {'H','K','E','Y','_','C','U','R','R','E','N','T','_','C','O','N','F','I','G',0};
static const WCHAR hkey_current_user[] = {'H','K','E','Y','_','C','U','R','R','E','N','T','_','U','S','E','R',0};
static const WCHAR hkey_dyn_data[] = {'H','K','E','Y','_','D','Y','N','_','D','A','T','A',0};

const WCHAR *reg_class_namesW[] = {hkey_local_machine, hkey_users,
                                   hkey_classes_root, hkey_current_config,
                                   hkey_current_user, hkey_dyn_data
                                  };

/*******************************************************************************
 * Global Variables:
 */

HINSTANCE hInst;
HWND hFrameWnd;
HWND hStatusBar;
HMENU hMenuFrame;
HMENU hPopupMenus = 0;
UINT nClipboardFormat;
const WCHAR strClipboardFormat[] = {'T','O','D','O',':',' ','S','E','T',' ','C','O','R','R','E','C','T',' ','F','O','R','M','A','T',0};


#define MAX_LOADSTRING  100
WCHAR szTitle[MAX_LOADSTRING];
const WCHAR szFrameClass[] = {'R','e','g','E','d','i','t','_','R','e','g','E','d','i','t',0};
const WCHAR szChildClass[] = {'R','E','G','E','D','I','T',0};

static BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
    WCHAR empty = 0;
    WNDCLASSEXW wndclass = {0};

    /* Frame class */
    wndclass.cbSize = sizeof(WNDCLASSEXW);
    wndclass.style = CS_HREDRAW | CS_VREDRAW;
    wndclass.lpfnWndProc = FrameWndProc;
    wndclass.hInstance = hInstance;
    wndclass.hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_REGEDIT));
    wndclass.hCursor = LoadCursorW(0, (LPCWSTR)IDC_ARROW);
    wndclass.lpszClassName = szFrameClass;
    wndclass.hIconSm = LoadImageW(hInstance, MAKEINTRESOURCEW(IDI_REGEDIT), IMAGE_ICON,
                                  GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_SHARED);
    RegisterClassExW(&wndclass);

    /* Child class */
    wndclass.lpfnWndProc = ChildWndProc;
    wndclass.cbWndExtra = sizeof(HANDLE);
    wndclass.lpszClassName = szChildClass;
    RegisterClassExW(&wndclass);

    hMenuFrame = LoadMenuW(hInstance, MAKEINTRESOURCEW(IDR_REGEDIT_MENU));
    hPopupMenus = LoadMenuW(hInstance, MAKEINTRESOURCEW(IDR_POPUP_MENUS));

    /* Initialize the Windows Common Controls DLL */
    InitCommonControls();

    /* register our hex editor control */
    HexEdit_Register();

    nClipboardFormat = RegisterClipboardFormatW(strClipboardFormat);

    hFrameWnd = CreateWindowExW(0, szFrameClass, szTitle,
                                WS_OVERLAPPEDWINDOW | WS_EX_CLIENTEDGE,
                                CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                                NULL, hMenuFrame, hInstance, NULL/*lpParam*/);

    if (!hFrameWnd) {
        return FALSE;
    }

    /* Create the status bar */
    hStatusBar = CreateStatusWindowW(WS_VISIBLE|WS_CHILD|WS_CLIPSIBLINGS|SBT_NOBORDERS,
                                    &empty, hFrameWnd, STATUS_WINDOW);
    if (hStatusBar) {
        /* Create the status bar panes */
        SetupStatusBar(hFrameWnd, FALSE);
        CheckMenuItem(GetSubMenu(hMenuFrame, ID_VIEW_MENU), ID_VIEW_STATUSBAR, MF_BYCOMMAND|MF_CHECKED);
    }
    ShowWindow(hFrameWnd, nCmdShow);
    UpdateWindow(hFrameWnd);
    return TRUE;
}

/******************************************************************************/

static void ExitInstance(void)
{
    DestroyMenu(hMenuFrame);
}

static BOOL TranslateChildTabMessage(MSG *msg)
{
    if (msg->message != WM_KEYDOWN) return FALSE;
    if (msg->wParam != VK_TAB) return FALSE;
    if (GetParent(msg->hwnd) != g_pChildWnd->hWnd) return FALSE;
    PostMessageW(g_pChildWnd->hWnd, WM_COMMAND, ID_SWITCH_PANELS, 0);
    return TRUE;
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
    MSG msg;
    HACCEL hAccel;
    BOOL is_wow64;

    if (ProcessCmdLine(GetCommandLineW())) {
        return 0;
    }

    if (IsWow64Process( GetCurrentProcess(), &is_wow64 ) && is_wow64)
    {
        STARTUPINFOW si;
        PROCESS_INFORMATION pi;
        WCHAR filename[MAX_PATH];
        void *redir;
        DWORD exit_code;

        memset( &si, 0, sizeof(si) );
        si.cb = sizeof(si);
        GetModuleFileNameW( 0, filename, MAX_PATH );

        Wow64DisableWow64FsRedirection( &redir );
        if (CreateProcessW( filename, GetCommandLineW(), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi ))
        {
            WINE_TRACE( "restarting %s\n", wine_dbgstr_w(filename) );
            WaitForSingleObject( pi.hProcess, INFINITE );
            GetExitCodeProcess( pi.hProcess, &exit_code );
            ExitProcess( exit_code );
        }
        else WINE_ERR( "failed to restart 64-bit %s, err %d\n", wine_dbgstr_w(filename), GetLastError() );
        Wow64RevertWow64FsRedirection( redir );
    }

    /* Initialize global strings */
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, ARRAY_SIZE(szTitle));
    LoadStringW(hInstance, IDS_REGISTRY_DEFAULT_VALUE, g_pszDefaultValueName, ARRAY_SIZE(g_pszDefaultValueName));

    /* Store instance handle in our global variable */
    hInst = hInstance;

    /* Perform application initialization */
    if (!InitInstance(hInstance, nCmdShow)) {
        return FALSE;
    }
    hAccel = LoadAcceleratorsW(hInstance, MAKEINTRESOURCEW(IDC_REGEDIT));

    /* Main message loop */
    while (GetMessageW(&msg, NULL, 0, 0)) {
        if (!TranslateAcceleratorW(hFrameWnd, hAccel, &msg)
           && !TranslateChildTabMessage(&msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    ExitInstance();
    return msg.wParam;
}
