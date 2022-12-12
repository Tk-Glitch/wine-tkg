/*
 * Input Context implementation
 *
 * Copyright 1998 Patrik Stridvall
 * Copyright 2002, 2003, 2007 CodeWeavers, Aric Stewart
 * Copyright 2022 Jacek Caban for CodeWeavers
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

#if 0
#pragma makedep unix
#endif

#include <pthread.h>
#include "win32u_private.h"
#include "ntuser_private.h"
#include "ddk/imm.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(imm);


struct imc
{
    struct user_object obj;
    DWORD    thread_id;
    UINT_PTR client_ptr;
};

struct imm_thread_data
{
    struct list entry;
    DWORD thread_id;
    HWND  default_hwnd;
    BOOL  disable_ime;
    UINT  window_cnt;
};

static struct list thread_data_list = LIST_INIT( thread_data_list );
static pthread_mutex_t imm_mutex = PTHREAD_MUTEX_INITIALIZER;
static BOOL disable_ime;

static struct imc *get_imc_ptr( HIMC handle )
{
    struct imc *imc = get_user_handle_ptr( handle, NTUSER_OBJ_IMC );
    if (imc && imc != OBJ_OTHER_PROCESS) return imc;
    WARN( "invalid handle %p\n", handle );
    RtlSetLastWin32Error( ERROR_INVALID_HANDLE );
    return NULL;
}

static void release_imc_ptr( struct imc *imc )
{
    release_user_handle_ptr( imc );
}

/******************************************************************************
 *           NtUserCreateInputContext   (win32u.@)
 */
HIMC WINAPI NtUserCreateInputContext( UINT_PTR client_ptr )
{
    struct imc *imc;
    HIMC handle;

    if (!(imc = malloc( sizeof(*imc) ))) return 0;
    imc->client_ptr = client_ptr;
    imc->thread_id = GetCurrentThreadId();
    if (!(handle = alloc_user_handle( &imc->obj, NTUSER_OBJ_IMC )))
    {
        free( imc );
        return 0;
    }

    TRACE( "%lx returning %p\n", (long)client_ptr, handle );
    return handle;
}

/******************************************************************************
 *           NtUserDestroyInputContext   (win32u.@)
 */
BOOL WINAPI NtUserDestroyInputContext( HIMC handle )
{
    struct imc *imc;

    TRACE( "%p\n", handle );

    if (!(imc = free_user_handle( handle, NTUSER_OBJ_IMC ))) return FALSE;
    if (imc == OBJ_OTHER_PROCESS)
    {
        FIXME( "other process handle %p\n", handle );
        return FALSE;
    }
    free( imc );
    return TRUE;
}

/******************************************************************************
 *           NtUserUpdateInputContext   (win32u.@)
 */
BOOL WINAPI NtUserUpdateInputContext( HIMC handle, UINT attr, UINT_PTR value )
{
    struct imc *imc;
    BOOL ret = TRUE;

    TRACE( "%p %u %lx\n", handle, attr, (long)value );

    if (!(imc = get_imc_ptr( handle ))) return FALSE;

    switch (attr)
    {
    case NtUserInputContextClientPtr:
        imc->client_ptr = value;
        break;

    default:
        FIXME( "unknown attr %u\n", attr );
        ret = FALSE;
    };

    release_imc_ptr( imc );
    return ret;
}

/******************************************************************************
 *           NtUserQueryInputContext   (win32u.@)
 */
UINT_PTR WINAPI NtUserQueryInputContext( HIMC handle, UINT attr )
{
    struct imc *imc;
    UINT_PTR ret;

    if (!(imc = get_imc_ptr( handle ))) return FALSE;

    switch (attr)
    {
    case NtUserInputContextClientPtr:
        ret = imc->client_ptr;
        break;

    case NtUserInputContextThreadId:
        ret = imc->thread_id;
        break;

    default:
        FIXME( "unknown attr %u\n", attr );
        ret = 0;
    };

    release_imc_ptr( imc );
    return ret;
}

/******************************************************************************
 *           NtUserAssociateInputContext   (win32u.@)
 */
UINT WINAPI NtUserAssociateInputContext( HWND hwnd, HIMC ctx, ULONG flags )
{
    WND *win;
    UINT ret = AICR_OK;

    TRACE( "%p %p %x\n", hwnd, ctx, (int)flags );

    switch (flags)
    {
    case 0:
    case IACE_IGNORENOCONTEXT:
    case IACE_DEFAULT:
        break;

    default:
        FIXME( "unknown flags 0x%x\n", (int)flags );
        return AICR_FAILED;
    }

    if (flags == IACE_DEFAULT)
    {
        if (!(ctx = get_default_input_context())) return AICR_FAILED;
    }
    else if (ctx)
    {
        if (NtUserQueryInputContext( ctx, NtUserInputContextThreadId ) != GetCurrentThreadId())
            return AICR_FAILED;
    }

    if (!(win = get_win_ptr( hwnd )) || win == WND_OTHER_PROCESS || win == WND_DESKTOP)
        return AICR_FAILED;

    if (ctx && win->tid != GetCurrentThreadId()) ret = AICR_FAILED;
    else if (flags != IACE_IGNORENOCONTEXT || win->imc)
    {
        if (win->imc != ctx && get_focus() == hwnd) ret = AICR_FOCUS_CHANGED;
        win->imc = ctx;
    }

    release_win_ptr( win );
    return ret;
}

HIMC get_default_input_context(void)
{
    struct ntuser_thread_info *thread_info = NtUserGetThreadInfo();
    if (!thread_info->default_imc)
        thread_info->default_imc = HandleToUlong( NtUserCreateInputContext( 0 ));
    return UlongToHandle( thread_info->default_imc );
}

HIMC get_window_input_context( HWND hwnd )
{
    WND *win;
    HIMC ret;

    if (!(win = get_win_ptr( hwnd )) || win == WND_OTHER_PROCESS || win == WND_DESKTOP)
    {
        RtlSetLastWin32Error( ERROR_INVALID_WINDOW_HANDLE );
        return 0;
    }

    ret = win->imc;
    release_win_ptr( win );
    return ret;
}

static HWND detach_default_window( struct imm_thread_data *thread_data )
{
    HWND hwnd = thread_data->default_hwnd;
    thread_data->default_hwnd = NULL;
    thread_data->window_cnt = 0;
    return hwnd;
}

static struct imm_thread_data *get_imm_thread_data(void)
{
    struct user_thread_info *thread_info = get_user_thread_info();
    if (!thread_info->imm_thread_data)
    {
        struct imm_thread_data *data;
        if (!(data = calloc( 1, sizeof( *data )))) return NULL;
        data->thread_id = GetCurrentThreadId();

        pthread_mutex_lock( &imm_mutex );
        list_add_tail( &thread_data_list, &data->entry );
        pthread_mutex_unlock( &imm_mutex );

        thread_info->imm_thread_data = data;
    }
    return thread_info->imm_thread_data;
}

BOOL register_imm_window( HWND hwnd )
{
    struct imm_thread_data *thread_data;

    TRACE( "(%p)\n", hwnd );

    if (disable_ime || !needs_ime_window( hwnd ))
        return FALSE;

    thread_data = get_imm_thread_data();
    if (!thread_data || thread_data->disable_ime)
        return FALSE;

    TRACE( "window_cnt=%u, default_hwnd=%p\n", thread_data->window_cnt + 1, thread_data->default_hwnd );

    /* Create default IME window */
    if (!thread_data->window_cnt++)
    {
        UNICODE_STRING class_name, name;
        static const WCHAR imeW[] = {'I','M','E',0};
        static const WCHAR default_imeW[] = {'D','e','f','a','u','l','t',' ','I','M','E',0};

        RtlInitUnicodeString( &class_name, imeW );
        RtlInitUnicodeString( &name, default_imeW );
        thread_data->default_hwnd = NtUserCreateWindowEx( 0, &class_name, &class_name, &name,
                                                          WS_POPUP | WS_DISABLED | WS_CLIPSIBLINGS,
                                                          0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, FALSE );
    }

    return TRUE;
}

void unregister_imm_window( HWND hwnd )
{
    struct imm_thread_data *thread_data = get_user_thread_info()->imm_thread_data;

    if (!thread_data) return;
    if (thread_data->default_hwnd == hwnd)
    {
        detach_default_window( thread_data );
        return;
    }

    if (!(win_set_flags( hwnd, 0, WIN_HAS_IME_WIN ) & WIN_HAS_IME_WIN)) return;

    /* destroy default IME window */
    TRACE( "unregister IME window for %p\n", hwnd );
    if (!--thread_data->window_cnt)
    {
        HWND destroy_hwnd = detach_default_window( thread_data );
        if (destroy_hwnd) NtUserDestroyWindow( destroy_hwnd );
    }
}

/***********************************************************************
 *           NtUserDisableThreadIme (win32u.@)
 */
BOOL WINAPI NtUserDisableThreadIme( DWORD thread_id )
{
    struct imm_thread_data *thread_data;

    if (thread_id == -1)
    {
        disable_ime = TRUE;

        pthread_mutex_lock( &imm_mutex );
        LIST_FOR_EACH_ENTRY( thread_data, &thread_data_list, struct imm_thread_data, entry )
        {
            if (thread_data->thread_id == GetCurrentThreadId()) continue;
            if (!thread_data->default_hwnd) continue;
            NtUserMessageCall( thread_data->default_hwnd, WM_WINE_DESTROYWINDOW, 0, 0,
                               0, NtUserSendNotifyMessage, FALSE );
        }
        pthread_mutex_unlock( &imm_mutex );
    }
    else if (!thread_id || thread_id == GetCurrentThreadId())
    {
        if (!(thread_data = get_imm_thread_data())) return FALSE;
        thread_data->disable_ime = TRUE;
    }
    else return FALSE;

    if ((thread_data = get_user_thread_info()->imm_thread_data))
    {
        HWND destroy_hwnd = detach_default_window( thread_data );
        NtUserDestroyWindow( destroy_hwnd );
    }
    return TRUE;
}

HWND get_default_ime_window( HWND hwnd )
{
    struct imm_thread_data *thread_data;
    HWND ret = 0;

    if (hwnd)
    {
        DWORD thread_id;

        if (!(thread_id = get_window_thread( hwnd, NULL ))) return 0;

        pthread_mutex_lock( &imm_mutex );
        LIST_FOR_EACH_ENTRY( thread_data, &thread_data_list, struct imm_thread_data, entry )
        {
            if (thread_data->thread_id != thread_id) continue;
            ret = thread_data->default_hwnd;
            break;
        }
        pthread_mutex_unlock( &imm_mutex );
    }
    else if ((thread_data = get_user_thread_info()->imm_thread_data))
    {
        ret = thread_data->default_hwnd;
    }

    TRACE( "default for %p is %p\n", hwnd, ret );
    return ret;
}

void cleanup_imm_thread(void)
{
    struct user_thread_info *thread_info = get_user_thread_info();

    if (thread_info->imm_thread_data)
    {
        pthread_mutex_lock( &imm_mutex );
        list_remove( &thread_info->imm_thread_data->entry );
        pthread_mutex_unlock( &imm_mutex );
        free( thread_info->imm_thread_data );
        thread_info->imm_thread_data = NULL;
    }

    NtUserDestroyInputContext( UlongToHandle( thread_info->client_info.default_imc ));
}

BOOL WINAPI ImmProcessKey( HWND hwnd, HKL hkl, UINT vkey, LPARAM key_data, DWORD unknown )
{
    struct imm_process_key_params params =
        { .hwnd = hwnd, .hkl = hkl, .vkey = vkey, .key_data = key_data };
    void *ret_ptr;
    ULONG ret_len;
    return KeUserModeCallback( NtUserImmProcessKey, &params, sizeof(params), &ret_ptr, &ret_len );
}

BOOL WINAPI ImmTranslateMessage( HWND hwnd, UINT msg, WPARAM wparam, LPARAM key_data )
{
    struct imm_translate_message_params params =
        { .hwnd = hwnd, .msg = msg, .wparam = wparam, .key_data = key_data };
    void *ret_ptr;
    ULONG ret_len;
    return KeUserModeCallback( NtUserImmTranslateMessage, &params, sizeof(params),
                               &ret_ptr, &ret_len );
}
