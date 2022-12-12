/*
 * Windows hook functions
 *
 * Copyright 2002 Alexandre Julliard
 * Copyright 2005 Dmitry Timoshkov
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

#include <assert.h>
#include "win32u_private.h"
#include "ntuser_private.h"
#include "wine/server.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(hook);

#define WH_WINEVENT (WH_MAXHOOK+1)

static const char * const hook_names[WH_WINEVENT - WH_MINHOOK + 1] =
{
    "WH_MSGFILTER",
    "WH_JOURNALRECORD",
    "WH_JOURNALPLAYBACK",
    "WH_KEYBOARD",
    "WH_GETMESSAGE",
    "WH_CALLWNDPROC",
    "WH_CBT",
    "WH_SYSMSGFILTER",
    "WH_MOUSE",
    "WH_HARDWARE",
    "WH_DEBUG",
    "WH_SHELL",
    "WH_FOREGROUNDIDLE",
    "WH_CALLWNDPROCRET",
    "WH_KEYBOARD_LL",
    "WH_MOUSE_LL",
    "WH_WINEVENT"
};

static const char *debugstr_hook_id( unsigned int id )
{
    if (id - WH_MINHOOK >= ARRAYSIZE(hook_names)) return wine_dbg_sprintf( "%u", id );
    return hook_names[id - WH_MINHOOK];
}

BOOL is_hooked( INT id )
{
    struct user_thread_info *thread_info = get_user_thread_info();

    if (!thread_info->active_hooks) return TRUE;
    return (thread_info->active_hooks & (1 << (id - WH_MINHOOK))) != 0;
}

/***********************************************************************
 *           NtUserSetWindowsHookEx   (win32u.@)
 */
HHOOK WINAPI NtUserSetWindowsHookEx( HINSTANCE inst, UNICODE_STRING *module, DWORD tid, INT id,
                                     HOOKPROC proc, BOOL ansi )
{
    HHOOK handle = 0;

    if (!proc)
    {
        RtlSetLastWin32Error( ERROR_INVALID_FILTER_PROC );
        return 0;
    }

    if (tid)  /* thread-local hook */
    {
        if (id == WH_JOURNALRECORD ||
            id == WH_JOURNALPLAYBACK ||
            id == WH_KEYBOARD_LL ||
            id == WH_MOUSE_LL ||
            id == WH_SYSMSGFILTER)
        {
            /* these can only be global */
            RtlSetLastWin32Error( ERROR_INVALID_PARAMETER );
            return 0;
        }
    }
    else  /* system-global hook */
    {
        if (id == WH_KEYBOARD_LL || id == WH_MOUSE_LL) inst = 0;
        else if (!inst)
        {
            RtlSetLastWin32Error( ERROR_HOOK_NEEDS_HMOD );
            return 0;
        }
    }

    SERVER_START_REQ( set_hook )
    {
        req->id        = id;
        req->pid       = 0;
        req->tid       = tid;
        req->event_min = EVENT_MIN;
        req->event_max = EVENT_MAX;
        req->flags     = WINEVENT_INCONTEXT;
        req->unicode   = !ansi;
        if (inst) /* make proc relative to the module base */
        {
            req->proc = wine_server_client_ptr( (void *)((char *)proc - (char *)inst) );
            wine_server_add_data( req, module->Buffer, module->Length );
        }
        else req->proc = wine_server_client_ptr( proc );

        if (!wine_server_call_err( req ))
        {
            handle = wine_server_ptr_handle( reply->handle );
            get_user_thread_info()->active_hooks = reply->active_hooks;
        }
    }
    SERVER_END_REQ;

    TRACE( "%s %p %x -> %p\n", debugstr_hook_id(id), proc, (int)tid, handle );
    return handle;
}

/***********************************************************************
 *	     NtUserUnhookWindowsHookEx   (win32u.@)
 */
BOOL WINAPI NtUserUnhookWindowsHookEx( HHOOK handle )
{
    NTSTATUS status;

    SERVER_START_REQ( remove_hook )
    {
        req->handle = wine_server_user_handle( handle );
        req->id     = 0;
        status = wine_server_call_err( req );
        if (!status) get_user_thread_info()->active_hooks = reply->active_hooks;
    }
    SERVER_END_REQ;
    if (status == STATUS_INVALID_HANDLE) RtlSetLastWin32Error( ERROR_INVALID_HOOK_HANDLE );
    return !status;
}

/* see UnhookWindowsHook */
BOOL unhook_windows_hook( INT id, HOOKPROC proc )
{
    NTSTATUS status;

    TRACE( "%s %p\n", debugstr_hook_id(id), proc );

    SERVER_START_REQ( remove_hook )
    {
        req->handle = 0;
        req->id   = id;
        req->proc = wine_server_client_ptr( proc );
        status = wine_server_call_err( req );
        if (!status) get_user_thread_info()->active_hooks = reply->active_hooks;
    }
    SERVER_END_REQ;
    if (status == STATUS_INVALID_HANDLE) RtlSetLastWin32Error( ERROR_INVALID_HOOK_HANDLE );
    return !status;
}

/***********************************************************************
 *           NtUserCallMsgFilter (win32u.@)
 */
BOOL WINAPI NtUserCallMsgFilter( MSG *msg, INT code )
{
    /* FIXME: We should use NtCallbackReturn instead of passing (potentially kernel) pointer
     * like that, but we need to consequently use syscall thunks first for that to work. */
    if (call_hooks( WH_SYSMSGFILTER, code, 0, (LPARAM)msg, sizeof(*msg) )) return TRUE;
    return call_hooks( WH_MSGFILTER, code, 0, (LPARAM)msg, sizeof(*msg) );
}

static UINT get_ll_hook_timeout(void)
{
    /* FIXME: should retrieve LowLevelHooksTimeout in HKEY_CURRENT_USER\Control Panel\Desktop */
    return 2000;
}

/***********************************************************************
 *	     call_hook
 *
 * Call hook either in current thread or send message to the destination
 * thread.
 */
static LRESULT call_hook( struct win_hook_params *info, const WCHAR *module )
{
    DWORD_PTR ret = 0;
    LRESULT lres = 0;

    if (info->tid)
    {
        struct hook_extra_info h_extra;
        h_extra.handle = info->handle;
        h_extra.lparam = info->lparam;

        TRACE( "calling hook in thread %04x %s code %x wp %lx lp %lx\n",
               (int)info->tid, hook_names[info->id-WH_MINHOOK],
               info->code, (long)info->wparam, info->lparam );

        switch(info->id)
        {
        case WH_KEYBOARD_LL:
            lres = send_internal_message_timeout( info->pid, info->tid, WM_WINE_KEYBOARD_LL_HOOK,
                                           info->wparam, (LPARAM)&h_extra, SMTO_ABORTIFHUNG,
                                           get_ll_hook_timeout(), &ret );
            break;
        case WH_MOUSE_LL:
            lres = send_internal_message_timeout( info->pid, info->tid, WM_WINE_MOUSE_LL_HOOK,
                                           info->wparam, (LPARAM)&h_extra, SMTO_ABORTIFHUNG,
                                           get_ll_hook_timeout(), &ret );
            break;
        default:
            ERR("Unknown hook id %d\n", info->id);
            assert(0);
            break;
        }

        if (!lres && RtlGetLastWin32Error() == ERROR_TIMEOUT)
        {
            TRACE("Hook %p timed out; removing it.\n", info->handle);
            NtUserUnhookWindowsHookEx( info->handle );
        }
    }
    else if (info->proc)
    {
        struct user_thread_info *thread_info = get_user_thread_info();
        HHOOK prev = thread_info->hook;
        BOOL prev_unicode = thread_info->hook_unicode;
        struct win_hook_params *params = info;
        ULONG size = sizeof(*params);
        ULONG lparam_ret_size = params->lparam_size;
        CREATESTRUCTW *cs = NULL;
        void *ret_ptr;
        ULONG ret_len;

        if (params->lparam_size)
        {
            lparam_ret_size = params->lparam_size;

            if (params->id == WH_CBT && params->code == HCBT_CREATEWND)
            {
                cs = ((CBT_CREATEWNDW *)params->lparam)->lpcs;
                params->lparam = 0;
                lparam_ret_size = 0;
                params->lparam_size = sizeof(*cs);
                if (!IS_INTRESOURCE( cs->lpszName ))
                    params->lparam_size += (wcslen( cs->lpszName ) + 1) * sizeof(WCHAR);
                if (!IS_INTRESOURCE( cs->lpszClass ))
                    params->lparam_size += (wcslen( cs->lpszClass ) + 1) * sizeof(WCHAR);
            }

            size += params->lparam_size;
        }

        if (module && module[0]) size += (lstrlenW( module ) + 1) * sizeof(WCHAR);
        if (size != sizeof(*params))
        {
            if (!(params = malloc( size ))) return 0;
            *params = *info;
        }

        if (params->lparam_size)
        {
            if (cs)
            {
                CREATESTRUCTW *params_cs = (CREATESTRUCTW *)(params + 1);
                WCHAR *ptr = (WCHAR *)(params_cs + 1);
                const void *inline_ptr = (void *)0xffffffff;

                *params_cs = *cs;
                if (!IS_INTRESOURCE( cs->lpszName ))
                {
                    UINT len = wcslen( cs->lpszName ) + 1;
                    memcpy( ptr, cs->lpszName, len * sizeof(WCHAR) );
                    ptr += len;
                    params_cs->lpszName = inline_ptr;
                }
                if (!IS_INTRESOURCE( cs->lpszClass ))
                {
                    wcscpy( ptr, cs->lpszClass );
                    params_cs->lpszClass = inline_ptr;
                }
            }
            else
            {
                memcpy( params + 1, (const void *)params->lparam, params->lparam_size );
            }
        }

        if (module && module[0])
            wcscpy( (WCHAR *)((char *)(params + 1) + params->lparam_size), module );

        /*
         * Windows protects from stack overflow in recursive hook calls. Different Windows
         * allow different depths.
         */
        if (thread_info->hook_call_depth >= 25)
        {
            WARN("Too many hooks called recursively, skipping call.\n");
            if (params != info) free( params );
            return 0;
        }

        TRACE( "calling hook %p %s code %x wp %lx lp %lx module %s\n",
               params->proc, hook_names[params->id-WH_MINHOOK], params->code, (long)params->wparam,
               params->lparam, debugstr_w(module) );

        thread_info->hook = params->handle;
        thread_info->hook_unicode = params->next_unicode;
        thread_info->hook_call_depth++;
        ret = KeUserModeCallback( NtUserCallWindowsHook, params, size, &ret_ptr, &ret_len );
        if (ret_len && ret_len == lparam_ret_size)
            memcpy( (void *)params->lparam, ret_ptr, lparam_ret_size );
        thread_info->hook = prev;
        thread_info->hook_unicode = prev_unicode;
        thread_info->hook_call_depth--;

        if (params != info) free( params );
    }

    if (info->id == WH_KEYBOARD_LL || info->id == WH_MOUSE_LL)
        InterlockedIncrement( &global_key_state_counter ); /* force refreshing the key state cache */
    return ret;
}

/***********************************************************************
 *	     NtUserCallNextHookEx (win32u.@)
 */
LRESULT WINAPI NtUserCallNextHookEx( HHOOK hhook, INT code, WPARAM wparam, LPARAM lparam )
{
    struct user_thread_info *thread_info = get_user_thread_info();
    struct win_hook_params info;
    WCHAR module[MAX_PATH];

    memset( &info, 0, sizeof(info) );

    SERVER_START_REQ( get_hook_info )
    {
        req->handle = wine_server_user_handle( thread_info->hook );
        req->get_next = 1;
        req->event = EVENT_MIN;
        wine_server_set_reply( req, module, sizeof(module) - sizeof(WCHAR) );
        if (!wine_server_call_err( req ))
        {
            module[wine_server_reply_size(req) / sizeof(WCHAR)] = 0;
            info.handle       = wine_server_ptr_handle( reply->handle );
            info.id           = reply->id;
            info.pid          = reply->pid;
            info.tid          = reply->tid;
            info.proc         = wine_server_get_ptr( reply->proc );
            info.next_unicode = reply->unicode;
        }
    }
    SERVER_END_REQ;

    info.code   = code;
    info.wparam = wparam;
    info.lparam = lparam;
    info.prev_unicode = thread_info->hook_unicode;
    return call_hook( &info, module );
}

LRESULT call_current_hook( HHOOK hhook, INT code, WPARAM wparam, LPARAM lparam )
{
    struct win_hook_params info;
    WCHAR module[MAX_PATH];

    memset( &info, 0, sizeof(info) );

    SERVER_START_REQ( get_hook_info )
    {
        req->handle = wine_server_user_handle( hhook );
        req->get_next = 0;
        req->event = EVENT_MIN;
        wine_server_set_reply( req, module, sizeof(module) );
        if (!wine_server_call_err( req ))
        {
            module[wine_server_reply_size(req) / sizeof(WCHAR)] = 0;
            info.handle       = wine_server_ptr_handle( reply->handle );
            info.id           = reply->id;
            info.pid          = reply->pid;
            info.tid          = reply->tid;
            info.proc         = wine_server_get_ptr( reply->proc );
            info.next_unicode = reply->unicode;
        }
    }
    SERVER_END_REQ;

    info.code   = code;
    info.wparam = wparam;
    info.lparam = lparam;
    info.prev_unicode = TRUE;  /* assume Unicode for this function */
    return call_hook( &info, module );
}

LRESULT call_hooks( INT id, INT code, WPARAM wparam, LPARAM lparam, size_t lparam_size )
{
    struct user_thread_info *thread_info = get_user_thread_info();
    struct win_hook_params info;
    WCHAR module[MAX_PATH];
    DWORD_PTR ret;

    user_check_not_lock();

    if (!is_hooked( id ))
    {
        TRACE( "skipping hook %s mask %x\n", hook_names[id-WH_MINHOOK], thread_info->active_hooks );
        return 0;
    }

    memset( &info, 0, sizeof(info) );
    info.prev_unicode = TRUE;
    info.id = id;

    SERVER_START_REQ( start_hook_chain )
    {
        req->id = info.id;
        req->event = EVENT_MIN;
        wine_server_set_reply( req, module, sizeof(module)-sizeof(WCHAR) );
        if (!wine_server_call( req ))
        {
            module[wine_server_reply_size(req) / sizeof(WCHAR)] = 0;
            info.handle       = wine_server_ptr_handle( reply->handle );
            info.pid          = reply->pid;
            info.tid          = reply->tid;
            info.proc         = wine_server_get_ptr( reply->proc );
            info.next_unicode = reply->unicode;
            thread_info->active_hooks = reply->active_hooks;
        }
    }
    SERVER_END_REQ;
    if (!info.tid && !info.proc) return 0;

    info.code   = code;
    info.wparam = wparam;
    info.lparam = lparam;
    info.lparam_size = lparam_size;
    ret = call_hook( &info, module );

    SERVER_START_REQ( finish_hook_chain )
    {
        req->id = id;
        wine_server_call( req );
    }
    SERVER_END_REQ;
    return ret;
}

/***********************************************************************
 *           NtUserSetWinEventHook   (win32u.@)
 */
HWINEVENTHOOK WINAPI NtUserSetWinEventHook( DWORD event_min, DWORD event_max, HMODULE inst,
                                            UNICODE_STRING *module, WINEVENTPROC proc,
                                            DWORD pid, DWORD tid, DWORD flags )
{
    HWINEVENTHOOK handle = 0;

    if ((flags & WINEVENT_INCONTEXT) && !inst)
    {
        RtlSetLastWin32Error(ERROR_HOOK_NEEDS_HMOD);
        return 0;
    }

    if (event_min > event_max)
    {
        RtlSetLastWin32Error(ERROR_INVALID_HOOK_FILTER);
        return 0;
    }

    /* FIXME: what if the tid or pid belongs to another process? */
    if (tid) inst = 0; /* thread-local hook */

    SERVER_START_REQ( set_hook )
    {
        req->id        = WH_WINEVENT;
        req->pid       = pid;
        req->tid       = tid;
        req->event_min = event_min;
        req->event_max = event_max;
        req->flags     = flags;
        req->unicode   = 1;
        if (inst) /* make proc relative to the module base */
        {
            req->proc = wine_server_client_ptr( (void *)((char *)proc - (char *)inst) );
            wine_server_add_data( req, module->Buffer, module->Length );
        }
        else req->proc = wine_server_client_ptr( proc );

        if (!wine_server_call_err( req ))
        {
            handle = wine_server_ptr_handle( reply->handle );
            get_user_thread_info()->active_hooks = reply->active_hooks;
        }
    }
    SERVER_END_REQ;

    TRACE("-> %p\n", handle);
    return handle;
}

/***********************************************************************
 *           NtUserUnhookWinEvent   (win32u.@)
 */
BOOL WINAPI NtUserUnhookWinEvent( HWINEVENTHOOK handle )
{
    BOOL ret;

    SERVER_START_REQ( remove_hook )
    {
        req->handle = wine_server_user_handle( handle );
        req->id     = WH_WINEVENT;
        ret = !wine_server_call_err( req );
        if (ret) get_user_thread_info()->active_hooks = reply->active_hooks;
    }
    SERVER_END_REQ;
    return ret;
}

/***********************************************************************
 *           NtUserNotifyWinEvent   (win32u.@)
 */
void WINAPI NtUserNotifyWinEvent( DWORD event, HWND hwnd, LONG object_id, LONG child_id )
{
    struct user_thread_info *thread_info = get_user_thread_info();
    struct win_event_hook_params info;
    void *ret_ptr;
    ULONG ret_len;
    BOOL ret;

    TRACE( "%04x, %p, %d, %d\n", (int)event, hwnd, (int)object_id, (int)child_id );

    user_check_not_lock();

    if (!hwnd)
    {
        RtlSetLastWin32Error( ERROR_INVALID_WINDOW_HANDLE );
        return;
    }

    if (!is_hooked( WH_WINEVENT ))
    {
        TRACE( "skipping hook mask %x\n", thread_info->active_hooks );
        return;
    }

    info.event     = event;
    info.hwnd      = hwnd;
    info.object_id = object_id;
    info.child_id  = child_id;
    info.tid       = GetCurrentThreadId();

    SERVER_START_REQ( start_hook_chain )
    {
        req->id        = WH_WINEVENT;
        req->event     = event;
        req->window    = wine_server_user_handle( hwnd );
        req->object_id = object_id;
        req->child_id  = child_id;
        wine_server_set_reply( req, info.module, sizeof(info.module) - sizeof(WCHAR) );
        ret = !wine_server_call( req ) && reply->proc;
        if (ret)
        {
            info.module[wine_server_reply_size(req) / sizeof(WCHAR)] = 0;
            info.handle = wine_server_ptr_handle( reply->handle );
            info.proc   = wine_server_get_ptr( reply->proc );
            thread_info->active_hooks = reply->active_hooks;
        }
    }
    SERVER_END_REQ;
    if (!ret) return;

    do
    {
        TRACE( "calling WH_WINEVENT hook %p event %x hwnd %p %x %x module %s\n",
               info.proc, (int)event, hwnd, (int)object_id, (int)child_id, debugstr_w(info.module) );

        info.time = NtGetTickCount();
        KeUserModeCallback( NtUserCallWinEventHook, &info,
                            FIELD_OFFSET( struct win_event_hook_params, module[lstrlenW(info.module) + 1] ),
                            &ret_ptr, &ret_len );

        SERVER_START_REQ( get_hook_info )
        {
            req->handle    = wine_server_user_handle( info.handle );
            req->get_next  = 1;
            req->event     = event;
            req->window    = wine_server_user_handle( hwnd );
            req->object_id = object_id;
            req->child_id  = child_id;
            wine_server_set_reply( req, info.module, sizeof(info.module) - sizeof(WCHAR) );
            ret = !wine_server_call( req ) && reply->proc;
            if (ret)
            {
                info.module[wine_server_reply_size(req) / sizeof(WCHAR)] = 0;
                info.handle = wine_server_ptr_handle( reply->handle );
                info.proc   = wine_server_get_ptr( reply->proc );
            }
        }
        SERVER_END_REQ;
    }
    while (ret);

    SERVER_START_REQ( finish_hook_chain )
    {
        req->id = WH_WINEVENT;
        wine_server_call( req );
    }
    SERVER_END_REQ;
}
