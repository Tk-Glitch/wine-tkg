/*
 * Copyright 2005-2007 Jacek Caban for CodeWeavers
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

#include <stdarg.h>

#define COBJMACROS

#include "windef.h"
#include "winbase.h"
#include "winuser.h"
#include "ole2.h"
#include "shlguid.h"
#include "mshtmdid.h"
#include "idispids.h"
#include "mshtmcid.h"

#include "wine/debug.h"

#include "mshtml_private.h"
#include "binding.h"
#include "resource.h"

WINE_DEFAULT_DEBUG_CHANNEL(mshtml);

#define NSCMD_COPY "cmd_copy"
#define NSCMD_SELECTALL           "cmd_selectAll"

void do_ns_command(HTMLDocumentNode *doc, const char *cmd, nsICommandParams *nsparam)
{
    nsICommandManager *cmdmgr;
    nsresult nsres;

    TRACE("(%p)\n", doc);

    if(!doc->browser || !doc->window)
        return;

    nsres = get_nsinterface((nsISupports*)doc->browser->webbrowser, &IID_nsICommandManager, (void**)&cmdmgr);
    if(NS_FAILED(nsres)) {
        ERR("Could not get nsICommandManager: %08x\n", nsres);
        return;
    }

    nsres = nsICommandManager_DoCommand(cmdmgr, cmd, nsparam, doc->window->base.outer_window->window_proxy);
    if(NS_FAILED(nsres))
        ERR("DoCommand(%s) failed: %08x\n", debugstr_a(cmd), nsres);

    nsICommandManager_Release(cmdmgr);
}

static nsIClipboardCommands *get_clipboard_commands(HTMLDocumentNode *doc)
{
    nsIClipboardCommands *clipboard_commands;
    nsIDocShell *doc_shell;
    nsresult nsres;

    nsres = get_nsinterface((nsISupports*)doc->basedoc.window->nswindow, &IID_nsIDocShell, (void**)&doc_shell);
    if(NS_FAILED(nsres)) {
        ERR("Could not get nsIDocShell interface\n");
        return NULL;
    }

    nsres = nsIDocShell_QueryInterface(doc_shell, &IID_nsIClipboardCommands, (void**)&clipboard_commands);
    nsIDocShell_Release(doc_shell);
    if(NS_FAILED(nsres)) {
        ERR("Could not get nsIClipboardCommands interface\n");
        return NULL;
    }

    return clipboard_commands;
}

/**********************************************************
 * IOleCommandTarget implementation
 */

static inline HTMLDocument *impl_from_IOleCommandTarget(IOleCommandTarget *iface)
{
    return CONTAINING_RECORD(iface, HTMLDocument, IOleCommandTarget_iface);
}

static HRESULT exec_open(HTMLDocumentNode *doc, DWORD nCmdexecopt, VARIANT *pvaIn, VARIANT *pvaOut)
{
    FIXME("(%p)->(%d %p %p)\n", doc, nCmdexecopt, pvaIn, pvaOut);
    return E_NOTIMPL;
}

static HRESULT exec_new(HTMLDocumentNode *doc, DWORD nCmdexecopt, VARIANT *pvaIn, VARIANT *pvaOut)
{
    FIXME("(%p)->(%d %p %p)\n", doc, nCmdexecopt, pvaIn, pvaOut);
    return E_NOTIMPL;
}

static HRESULT exec_save(HTMLDocumentNode *doc, DWORD nCmdexecopt, VARIANT *pvaIn, VARIANT *pvaOut)
{
    FIXME("(%p)->(%d %p %p)\n", doc, nCmdexecopt, pvaIn, pvaOut);
    return E_NOTIMPL;
}

static HRESULT exec_save_as(HTMLDocumentNode *doc, DWORD nCmdexecopt, VARIANT *pvaIn, VARIANT *pvaOut)
{
    FIXME("(%p)->(%d %p %p)\n", doc, nCmdexecopt, pvaIn, pvaOut);
    return E_NOTIMPL;
}

static HRESULT exec_save_copy_as(HTMLDocumentNode *doc, DWORD nCmdexecopt, VARIANT *pvaIn, VARIANT *pvaOut)
{
    FIXME("(%p)->(%d %p %p)\n", doc, nCmdexecopt, pvaIn, pvaOut);
    return E_NOTIMPL;
}

static nsresult set_head_text(nsIPrintSettings *settings, LPCWSTR template, BOOL head, int pos)
{
    if(head) {
        switch(pos) {
        case 0:
            return nsIPrintSettings_SetHeaderStrLeft(settings, template);
        case 1:
            return nsIPrintSettings_SetHeaderStrRight(settings, template);
        case 2:
            return nsIPrintSettings_SetHeaderStrCenter(settings, template);
        }
    }else {
        switch(pos) {
        case 0:
            return nsIPrintSettings_SetFooterStrLeft(settings, template);
        case 1:
            return nsIPrintSettings_SetFooterStrRight(settings, template);
        case 2:
            return nsIPrintSettings_SetFooterStrCenter(settings, template);
        }
    }

    return NS_OK;
}

static void set_print_template(nsIPrintSettings *settings, LPCWSTR template, BOOL head)
{
    PRUnichar nstemplate[200]; /* FIXME: Use dynamic allocation */
    PRUnichar *p = nstemplate;
    LPCWSTR ptr=template;
    int pos=0;

    while(*ptr) {
        if(*ptr != '&') {
            *p++ = *ptr++;
            continue;
        }

        switch(*++ptr) {
        case '&':
            *p++ = '&';
            *p++ = '&';
            ptr++;
            break;
        case 'b': /* change align */
            ptr++;
            *p = 0;
            set_head_text(settings, nstemplate, head, pos);
            p = nstemplate;
            pos++;
            break;
        case 'd': { /* short date */
            SYSTEMTIME systime;
            GetLocalTime(&systime);
            GetDateFormatW(LOCALE_SYSTEM_DEFAULT, 0, &systime, NULL, p,
                    sizeof(nstemplate)-(p-nstemplate)*sizeof(WCHAR));
            p += lstrlenW(p);
            ptr++;
            break;
        }
        case 'p': /* page number */
            *p++ = '&';
            *p++ = 'P';
            ptr++;
            break;
        case 'P': /* page count */
            *p++ = '?'; /* FIXME */
            ptr++;
            break;
        case 'u':
            *p++ = '&';
            *p++ = 'U';
            ptr++;
            break;
        case 'w':
            /* FIXME: set window title */
            ptr++;
            break;
        default:
            *p++ = '&';
            *p++ = *ptr++;
        }
    }

    *p = 0;
    set_head_text(settings, nstemplate, head, pos);

    while(++pos < 3)
        set_head_text(settings, p, head, pos);
}

static void set_default_templates(nsIPrintSettings *settings)
{
    WCHAR buf[64];

    nsIPrintSettings_SetHeaderStrLeft(settings, L"");
    nsIPrintSettings_SetHeaderStrRight(settings, L"");
    nsIPrintSettings_SetHeaderStrCenter(settings, L"");
    nsIPrintSettings_SetFooterStrLeft(settings, L"");
    nsIPrintSettings_SetFooterStrRight(settings, L"");
    nsIPrintSettings_SetFooterStrCenter(settings, L"");

    if(LoadStringW(get_shdoclc(), IDS_PRINT_HEADER_TEMPLATE, buf, ARRAY_SIZE(buf)))
        set_print_template(settings, buf, TRUE);

    if(LoadStringW(get_shdoclc(), IDS_PRINT_FOOTER_TEMPLATE, buf, ARRAY_SIZE(buf)))
        set_print_template(settings, buf, FALSE);

}

static HRESULT exec_print(HTMLDocumentNode *doc, DWORD nCmdexecopt, VARIANT *pvaIn, VARIANT *pvaOut)
{
    nsIWebBrowserPrint *nsprint;
    nsIPrintSettings *settings;
    nsresult nsres;

    TRACE("(%p)->(%d %s %p)\n", doc, nCmdexecopt, debugstr_variant(pvaIn), pvaOut);

    if(pvaOut)
        FIXME("unsupported pvaOut\n");

    nsres = get_nsinterface((nsISupports*)doc->browser->webbrowser, &IID_nsIWebBrowserPrint,
            (void**)&nsprint);
    if(NS_FAILED(nsres)) {
        ERR("Could not get nsIWebBrowserPrint: %08x\n", nsres);
        return S_OK;
    }

    nsres = nsIWebBrowserPrint_GetGlobalPrintSettings(nsprint, &settings);
    if(NS_FAILED(nsres))
        ERR("GetCurrentPrintSettings failed: %08x\n", nsres);

    set_default_templates(settings);

    if(pvaIn) {
        switch(V_VT(pvaIn)) {
        case VT_BYREF|VT_ARRAY: {
            VARIANT *opts;
            DWORD opts_cnt;

            if(V_ARRAY(pvaIn)->cDims != 1)
                WARN("cDims = %d\n", V_ARRAY(pvaIn)->cDims);

            SafeArrayAccessData(V_ARRAY(pvaIn), (void**)&opts);
            opts_cnt = V_ARRAY(pvaIn)->rgsabound[0].cElements;

            if(opts_cnt >= 1) {
                switch(V_VT(opts)) {
                case VT_BSTR:
                    TRACE("setting footer %s\n", debugstr_w(V_BSTR(opts)));
                    set_print_template(settings, V_BSTR(opts), TRUE);
                    break;
                case VT_NULL:
                    break;
                default:
                    WARN("opts = %s\n", debugstr_variant(opts));
                }
            }

            if(opts_cnt >= 2) {
                switch(V_VT(opts+1)) {
                case VT_BSTR:
                    TRACE("setting footer %s\n", debugstr_w(V_BSTR(opts+1)));
                    set_print_template(settings, V_BSTR(opts+1), FALSE);
                    break;
                case VT_NULL:
                    break;
                default:
                    WARN("opts[1] = %s\n", debugstr_variant(opts+1));
                }
            }

            if(opts_cnt >= 3)
                FIXME("Unsupported opts_cnt %d\n", opts_cnt);

            SafeArrayUnaccessData(V_ARRAY(pvaIn));
            break;
        }
        default:
            FIXME("unsupported arg %s\n", debugstr_variant(pvaIn));
        }
    }

    nsres = nsIWebBrowserPrint_Print(nsprint, settings, NULL);
    if(NS_FAILED(nsres))
        ERR("Print failed: %08x\n", nsres);

    nsIWebBrowserPrint_Release(nsprint);

    return S_OK;
}

static HRESULT exec_print_preview(HTMLDocumentNode *doc, DWORD nCmdexecopt, VARIANT *pvaIn, VARIANT *pvaOut)
{
    FIXME("(%p)->(%d %p %p)\n", doc, nCmdexecopt, pvaIn, pvaOut);
    return E_NOTIMPL;
}

static HRESULT exec_page_setup(HTMLDocumentNode *doc, DWORD nCmdexecopt, VARIANT *pvaIn, VARIANT *pvaOut)
{
    FIXME("(%p)->(%d %p %p)\n", doc, nCmdexecopt, pvaIn, pvaOut);
    return E_NOTIMPL;
}

static HRESULT exec_spell(HTMLDocumentNode *doc, DWORD nCmdexecopt, VARIANT *pvaIn, VARIANT *pvaOut)
{
    FIXME("(%p)->(%d %p %p)\n", doc, nCmdexecopt, pvaIn, pvaOut);
    return E_NOTIMPL;
}

static HRESULT exec_properties(HTMLDocumentNode *doc, DWORD nCmdexecopt, VARIANT *pvaIn, VARIANT *pvaOut)
{
    FIXME("(%p)->(%d %p %p)\n", doc, nCmdexecopt, pvaIn, pvaOut);
    return E_NOTIMPL;
}

static HRESULT exec_cut(HTMLDocumentNode *doc, DWORD nCmdexecopt, VARIANT *pvaIn, VARIANT *pvaOut)
{
    FIXME("(%p)->(%d %p %p)\n", doc, nCmdexecopt, pvaIn, pvaOut);
    return E_NOTIMPL;
}

static HRESULT exec_copy(HTMLDocumentNode *doc, DWORD nCmdexecopt, VARIANT *pvaIn, VARIANT *pvaOut)
{
    TRACE("(%p)->(%d %s %p)\n", doc, nCmdexecopt, debugstr_variant(pvaIn), pvaOut);

    do_ns_command(doc, NSCMD_COPY, NULL);
    return S_OK;
}

static HRESULT exec_paste(HTMLDocumentNode *doc, DWORD nCmdexecopt, VARIANT *pvaIn, VARIANT *pvaOut)
{
    FIXME("(%p)->(%d %p %p)\n", doc, nCmdexecopt, pvaIn, pvaOut);
    return E_NOTIMPL;
}

static HRESULT exec_paste_special(HTMLDocumentNode *doc, DWORD nCmdexecopt, VARIANT *pvaIn, VARIANT *pvaOut)
{
    FIXME("(%p)->(%d %p %p)\n", doc, nCmdexecopt, pvaIn, pvaOut);
    return E_NOTIMPL;
}

static HRESULT exec_undo(HTMLDocumentNode *doc, DWORD nCmdexecopt, VARIANT *pvaIn, VARIANT *pvaOut)
{
    FIXME("(%p)->(%d %p %p)\n", doc, nCmdexecopt, pvaIn, pvaOut);
    return E_NOTIMPL;
}

static HRESULT exec_rendo(HTMLDocumentNode *doc, DWORD nCmdexecopt, VARIANT *pvaIn, VARIANT *pvaOut)
{
    FIXME("(%p)->(%d %p %p)\n", doc, nCmdexecopt, pvaIn, pvaOut);
    return E_NOTIMPL;
}

static HRESULT exec_select_all(HTMLDocumentNode *doc, DWORD nCmdexecopt, VARIANT *in, VARIANT *out)
{
    TRACE("(%p)\n", doc);

    if(in || out)
        FIXME("unsupported args\n");

    if(!doc->browser)
        return E_UNEXPECTED;

    do_ns_command(doc, NSCMD_SELECTALL, NULL);
    update_doc(doc->browser->doc, UPDATE_UI);
    return S_OK;
}

static HRESULT exec_clear_selection(HTMLDocumentNode *doc, DWORD nCmdexecopt, VARIANT *pvaIn, VARIANT *pvaOut)
{
    FIXME("(%p)->(%d %p %p)\n", doc, nCmdexecopt, pvaIn, pvaOut);
    return E_NOTIMPL;
}

static HRESULT exec_zoom(HTMLDocumentNode *doc, DWORD nCmdexecopt, VARIANT *pvaIn, VARIANT *pvaOut)
{
    FIXME("(%p)->(%d %p %p)\n", doc, nCmdexecopt, pvaIn, pvaOut);
    return E_NOTIMPL;
}

static HRESULT exec_get_zoom_range(HTMLDocumentNode *doc, DWORD nCmdexecopt, VARIANT *pvaIn, VARIANT *pvaOut)
{
    FIXME("(%p)->(%d %p %p)\n", doc, nCmdexecopt, pvaIn, pvaOut);
    return E_NOTIMPL;
}

typedef struct {
    task_t header;
    HTMLOuterWindow *window;
}refresh_task_t;

static void refresh_proc(task_t *_task)
{
    refresh_task_t *task = (refresh_task_t*)_task;
    HTMLOuterWindow *window = task->window;

    TRACE("%p\n", window);

    window->readystate = READYSTATE_UNINITIALIZED;

    if(window->browser && window->browser->doc->client_cmdtrg) {
        VARIANT var;

        V_VT(&var) = VT_I4;
        V_I4(&var) = 0;
        IOleCommandTarget_Exec(window->browser->doc->client_cmdtrg, &CGID_ShellDocView, 37, 0, &var, NULL);
    }

    load_uri(task->window, task->window->uri, BINDING_REFRESH|BINDING_NOFRAG);
}

static void refresh_destr(task_t *_task)
{
    refresh_task_t *task = (refresh_task_t*)_task;

    IHTMLWindow2_Release(&task->window->base.IHTMLWindow2_iface);
    heap_free(task);
}

static HRESULT exec_refresh(HTMLDocumentNode *doc, DWORD nCmdexecopt, VARIANT *pvaIn, VARIANT *pvaOut)
{
    HTMLDocumentObj *doc_obj;
    refresh_task_t *task;
    HRESULT hres;

    TRACE("(%p)->(%d %s %p)\n", doc, nCmdexecopt, debugstr_variant(pvaIn), pvaOut);

    if(doc != doc->browser->doc->basedoc.doc_node) {
        FIXME("Unsupported on frame documents\n");
        return E_NOTIMPL;
    }
    doc_obj = doc->browser->doc;

    if(doc_obj->client) {
        IOleCommandTarget *olecmd;

        hres = IOleClientSite_QueryInterface(doc_obj->client, &IID_IOleCommandTarget, (void**)&olecmd);
        if(SUCCEEDED(hres)) {
            hres = IOleCommandTarget_Exec(olecmd, &CGID_DocHostCommandHandler, 2300, nCmdexecopt, pvaIn, pvaOut);
            IOleCommandTarget_Release(olecmd);
            if(SUCCEEDED(hres))
                return S_OK;
        }
    }

    if(!doc->basedoc.window)
        return E_UNEXPECTED;

    task = heap_alloc(sizeof(*task));
    if(!task)
        return E_OUTOFMEMORY;

    IHTMLWindow2_AddRef(&doc->basedoc.window->base.IHTMLWindow2_iface);
    task->window = doc->basedoc.window;

    return push_task(&task->header, refresh_proc, refresh_destr, doc->basedoc.window->task_magic);
}

static HRESULT exec_stop(HTMLDocumentNode *doc, DWORD nCmdexecopt, VARIANT *pvaIn, VARIANT *pvaOut)
{
    FIXME("(%p)->(%d %p %p)\n", doc, nCmdexecopt, pvaIn, pvaOut);
    return E_NOTIMPL;
}

static HRESULT exec_stop_download(HTMLDocumentNode *doc, DWORD nCmdexecopt, VARIANT *pvaIn, VARIANT *pvaOut)
{
    FIXME("(%p)->(%d %p %p)\n", doc, nCmdexecopt, pvaIn, pvaOut);
    return E_NOTIMPL;
}

static HRESULT exec_find(HTMLDocumentNode *doc, DWORD nCmdexecopt, VARIANT *pvaIn, VARIANT *pvaOut)
{
    FIXME("(%p)->(%d %p %p)\n", doc, nCmdexecopt, pvaIn, pvaOut);
    return E_NOTIMPL;
}

static HRESULT exec_delete(HTMLDocumentNode *doc, DWORD nCmdexecopt, VARIANT *pvaIn, VARIANT *pvaOut)
{
    FIXME("(%p)->(%d %p %p)\n", doc, nCmdexecopt, pvaIn, pvaOut);
    return E_NOTIMPL;
}

static HRESULT exec_enable_interaction(HTMLDocumentNode *doc, DWORD nCmdexecopt, VARIANT *pvaIn, VARIANT *pvaOut)
{
    FIXME("(%p)->(%d %p %p)\n", doc, nCmdexecopt, pvaIn, pvaOut);
    return E_NOTIMPL;
}

static HRESULT exec_on_unload(HTMLDocumentNode *doc, DWORD nCmdexecopt, VARIANT *pvaIn, VARIANT *pvaOut)
{
    TRACE("(%p)->(%d %p %p)\n", doc, nCmdexecopt, pvaIn, pvaOut);

    /* Tests show that we have nothing more to do here */

    if(pvaOut) {
        V_VT(pvaOut) = VT_BOOL;
        V_BOOL(pvaOut) = VARIANT_TRUE;
    }

    return S_OK;
}

static HRESULT exec_show_page_setup(HTMLDocumentNode *doc, DWORD nCmdexecopt, VARIANT *pvaIn, VARIANT *pvaOut)
{
    FIXME("(%p)->(%d %p %p)\n", doc, nCmdexecopt, pvaIn, pvaOut);
    return E_NOTIMPL;
}

static HRESULT exec_show_print(HTMLDocumentNode *doc, DWORD nCmdexecopt, VARIANT *pvaIn, VARIANT *pvaOut)
{
    FIXME("(%p)->(%d %p %p)\n", doc, nCmdexecopt, pvaIn, pvaOut);
    return E_NOTIMPL;
}

static HRESULT exec_close(HTMLDocumentNode *doc, DWORD nCmdexecopt, VARIANT *pvaIn, VARIANT *pvaOut)
{
    FIXME("(%p)->(%d %p %p)\n", doc, nCmdexecopt, pvaIn, pvaOut);
    return E_NOTIMPL;
}

static HRESULT exec_set_print_template(HTMLDocumentNode *doc, DWORD nCmdexecopt, VARIANT *pvaIn, VARIANT *pvaOut)
{
    FIXME("(%p)->(%d %p %p)\n", doc, nCmdexecopt, pvaIn, pvaOut);
    return E_NOTIMPL;
}

static HRESULT exec_get_print_template(HTMLDocumentNode *doc, DWORD nCmdexecopt, VARIANT *pvaIn, VARIANT *pvaOut)
{
    FIXME("(%p)->(%d %p %p)\n", doc, nCmdexecopt, pvaIn, pvaOut);
    return E_NOTIMPL;
}

static HRESULT exec_optical_zoom(HTMLDocumentNode *doc, DWORD nCmdexecopt, VARIANT *pvaIn, VARIANT *pvaOut)
{
    TRACE("(%p)->(%d %s %p)\n", doc, nCmdexecopt, debugstr_variant(pvaIn), pvaOut);

    if(pvaIn && V_VT(pvaIn) != VT_I4) {
        FIXME("Unsupported argument %s\n", debugstr_variant(pvaIn));
        return E_NOTIMPL;
    }

    if(pvaIn)
        set_viewer_zoom(doc->browser, (float)V_I4(pvaIn)/100);
    if(pvaOut) {
        V_VT(pvaOut) = VT_I4;
        V_I4(pvaOut) = get_viewer_zoom(doc->browser) * 100;
    }
    return S_OK;
}

static HRESULT query_mshtml_copy(HTMLDocumentNode *doc, OLECMD *cmd)
{
    FIXME("(%p)\n", doc);
    cmd->cmdf = OLECMDF_SUPPORTED|OLECMDF_ENABLED;
    return S_OK;
}

static HRESULT exec_mshtml_copy(HTMLDocumentNode *doc, DWORD cmdexecopt, VARIANT *in, VARIANT *out)
{
    TRACE("(%p)->(%08x %p %p)\n", doc, cmdexecopt, in, out);

    if(doc->browser->usermode == EDITMODE)
        return editor_exec_copy(doc, cmdexecopt, in, out);

    do_ns_command(doc, NSCMD_COPY, NULL);
    return S_OK;
}

static HRESULT query_mshtml_cut(HTMLDocumentNode *doc, OLECMD *cmd)
{
    FIXME("(%p)\n", doc);
    cmd->cmdf = OLECMDF_SUPPORTED|OLECMDF_ENABLED;
    return S_OK;
}

static HRESULT exec_mshtml_cut(HTMLDocumentNode *doc, DWORD cmdexecopt, VARIANT *in, VARIANT *out)
{
    nsIClipboardCommands *clipboard_commands;
    nsresult nsres;

    TRACE("(%p)->(%08x %p %p)\n", doc, cmdexecopt, in, out);

    if(doc->browser->usermode == EDITMODE)
        return editor_exec_cut(doc, cmdexecopt, in, out);

    clipboard_commands = get_clipboard_commands(doc);
    if(!clipboard_commands)
        return E_UNEXPECTED;

    nsres = nsIClipboardCommands_CutSelection(clipboard_commands);
    nsIClipboardCommands_Release(clipboard_commands);
    if(NS_FAILED(nsres)) {
        ERR("Paste failed: %08x\n", nsres);
        return E_FAIL;
    }

    return S_OK;
}

static HRESULT query_mshtml_paste(HTMLDocumentNode *doc, OLECMD *cmd)
{
    FIXME("(%p)\n", doc);
    cmd->cmdf = OLECMDF_SUPPORTED|OLECMDF_ENABLED;
    return S_OK;
}

static HRESULT exec_mshtml_paste(HTMLDocumentNode *doc, DWORD cmdexecopt, VARIANT *in, VARIANT *out)
{
    nsIClipboardCommands *clipboard_commands;
    nsresult nsres;

    TRACE("(%p)->(%08x %p %p)\n", doc, cmdexecopt, in, out);

    if(doc->browser->usermode == EDITMODE)
        return editor_exec_paste(doc, cmdexecopt, in, out);

    clipboard_commands = get_clipboard_commands(doc);
    if(!clipboard_commands)
        return E_UNEXPECTED;

    nsres = nsIClipboardCommands_Paste(clipboard_commands);
    nsIClipboardCommands_Release(clipboard_commands);
    if(NS_FAILED(nsres)) {
        ERR("Paste failed: %08x\n", nsres);
        return E_FAIL;
    }

    return S_OK;
}

static HRESULT query_selall_status(HTMLDocumentNode *doc, OLECMD *cmd)
{
    TRACE("(%p)->(%p)\n", doc, cmd);

    cmd->cmdf = OLECMDF_SUPPORTED|OLECMDF_ENABLED;
    return S_OK;
}

static HRESULT exec_browsemode(HTMLDocumentNode *doc, DWORD cmdexecopt, VARIANT *in, VARIANT *out)
{
    WARN("(%p)->(%08x %p %p)\n", doc, cmdexecopt, in, out);

    if(in || out)
        FIXME("unsupported args\n");

    doc->browser->usermode = BROWSEMODE;

    return S_OK;
}

static HRESULT exec_editmode(HTMLDocumentNode *doc, DWORD cmdexecopt, VARIANT *in, VARIANT *out)
{
    TRACE("(%p)->(%08x %p %p)\n", doc, cmdexecopt, in, out);

    if(in || out)
        FIXME("unsupported args\n");

    return setup_edit_mode(doc->browser->doc);
}

static HRESULT exec_htmleditmode(HTMLDocumentNode *doc, DWORD cmdexecopt, VARIANT *in, VARIANT *out)
{
    FIXME("(%p)->(%08x %p %p)\n", doc, cmdexecopt, in, out);
    return S_OK;
}

static HRESULT exec_baselinefont3(HTMLDocumentNode *doc, DWORD cmdexecopt, VARIANT *in, VARIANT *out)
{
    FIXME("(%p)->(%08x %p %p)\n", doc, cmdexecopt, in, out);
    return S_OK;
}

static HRESULT exec_respectvisibility_indesign(HTMLDocumentNode *doc, DWORD cmdexecopt,
        VARIANT *in, VARIANT *out)
{
    TRACE("(%p)->(%x %s %p)\n", doc, cmdexecopt, debugstr_variant(in), out);

    /* This is turned on by default in Gecko. */
    if(!in || V_VT(in) != VT_BOOL || !V_BOOL(in))
        FIXME("Unsupported argument %s\n", debugstr_variant(in));

    return S_OK;
}

static HRESULT query_enabled_stub(HTMLDocumentNode *doc, OLECMD *cmd)
{
    switch(cmd->cmdID) {
    case IDM_PRINT:
        FIXME("CGID_MSHTML: IDM_PRINT\n");
        cmd->cmdf = OLECMDF_SUPPORTED|OLECMDF_ENABLED;
        break;
    case IDM_BLOCKDIRLTR:
        FIXME("CGID_MSHTML: IDM_BLOCKDIRLTR\n");
        cmd->cmdf = OLECMDF_SUPPORTED|OLECMDF_ENABLED;
        break;
    case IDM_BLOCKDIRRTL:
        FIXME("CGID_MSHTML: IDM_BLOCKDIRRTL\n");
        cmd->cmdf = OLECMDF_SUPPORTED|OLECMDF_ENABLED;
        break;
    }

    return S_OK;
}

static const struct {
    OLECMDF cmdf;
    HRESULT (*func)(HTMLDocumentNode*,DWORD,VARIANT*,VARIANT*);
} exec_table[] = {
    {0},
    { OLECMDF_SUPPORTED,                  exec_open                 }, /* OLECMDID_OPEN */
    { OLECMDF_SUPPORTED,                  exec_new                  }, /* OLECMDID_NEW */
    { OLECMDF_SUPPORTED,                  exec_save                 }, /* OLECMDID_SAVE */
    { OLECMDF_SUPPORTED|OLECMDF_ENABLED,  exec_save_as              }, /* OLECMDID_SAVEAS */
    { OLECMDF_SUPPORTED,                  exec_save_copy_as         }, /* OLECMDID_SAVECOPYAS */
    { OLECMDF_SUPPORTED|OLECMDF_ENABLED,  exec_print                }, /* OLECMDID_PRINT */
    { OLECMDF_SUPPORTED|OLECMDF_ENABLED,  exec_print_preview        }, /* OLECMDID_PRINTPREVIEW */
    { OLECMDF_SUPPORTED|OLECMDF_ENABLED,  exec_page_setup           }, /* OLECMDID_PAGESETUP */
    { OLECMDF_SUPPORTED,                  exec_spell                }, /* OLECMDID_SPELL */
    { OLECMDF_SUPPORTED|OLECMDF_ENABLED,  exec_properties           }, /* OLECMDID_PROPERTIES */
    { OLECMDF_SUPPORTED,                  exec_cut                  }, /* OLECMDID_CUT */
    { OLECMDF_SUPPORTED,                  exec_copy                 }, /* OLECMDID_COPY */
    { OLECMDF_SUPPORTED,                  exec_paste                }, /* OLECMDID_PASTE */
    { OLECMDF_SUPPORTED,                  exec_paste_special        }, /* OLECMDID_PASTESPECIAL */
    { OLECMDF_SUPPORTED,                  exec_undo                 }, /* OLECMDID_UNDO */
    { OLECMDF_SUPPORTED,                  exec_rendo                }, /* OLECMDID_REDO */
    { OLECMDF_SUPPORTED|OLECMDF_ENABLED,  exec_select_all           }, /* OLECMDID_SELECTALL */
    { OLECMDF_SUPPORTED,                  exec_clear_selection      }, /* OLECMDID_CLEARSELECTION */
    { OLECMDF_SUPPORTED,                  exec_zoom                 }, /* OLECMDID_ZOOM */
    { OLECMDF_SUPPORTED,                  exec_get_zoom_range       }, /* OLECMDID_GETZOOMRANGE */
    {0},
    { OLECMDF_SUPPORTED|OLECMDF_ENABLED,  exec_refresh              }, /* OLECMDID_REFRESH */
    { OLECMDF_SUPPORTED|OLECMDF_ENABLED,  exec_stop                 }, /* OLECMDID_STOP */
    {0},{0},{0},{0},{0},{0},
    { OLECMDF_SUPPORTED,                  exec_stop_download        }, /* OLECMDID_STOPDOWNLOAD */
    {0},
    { OLECMDF_SUPPORTED|OLECMDF_ENABLED,  exec_find                 }, /* OLECMDID_FIND */
    { OLECMDF_SUPPORTED,                  exec_delete               }, /* OLECMDID_DELETE */
    {0},{0},
    { OLECMDF_SUPPORTED,                  exec_enable_interaction   }, /* OLECMDID_ENABLE_INTERACTION */
    { OLECMDF_SUPPORTED,                  exec_on_unload            }, /* OLECMDID_ONUNLOAD */
    {0},{0},{0},{0},{0},
    { OLECMDF_SUPPORTED,                  exec_show_page_setup      }, /* OLECMDID_SHOWPAGESETUP */
    { OLECMDF_SUPPORTED,                  exec_show_print           }, /* OLECMDID_SHOWPRINT */
    {0},{0},
    { OLECMDF_SUPPORTED,                  exec_close                }, /* OLECMDID_CLOSE */
    {0},{0},{0},
    { OLECMDF_SUPPORTED,                  exec_set_print_template   }, /* OLECMDID_SETPRINTTEMPLATE */
    { OLECMDF_SUPPORTED,                  exec_get_print_template   }, /* OLECMDID_GETPRINTTEMPLATE */
    {0},{0},{0},{0},{0},{0},{0},{0},{0},{0},
    { 0, /* not reported as supported */  exec_optical_zoom         }  /* OLECMDID_OPTICAL_ZOOM */
};

static const cmdtable_t base_cmds[] = {
    {IDM_COPY,             query_mshtml_copy,     exec_mshtml_copy},
    {IDM_PASTE,            query_mshtml_paste,    exec_mshtml_paste},
    {IDM_CUT,              query_mshtml_cut,      exec_mshtml_cut},
    {IDM_SELECTALL,        query_selall_status,   exec_select_all},
    {IDM_BROWSEMODE,       NULL,                  exec_browsemode},
    {IDM_EDITMODE,         NULL,                  exec_editmode},
    {IDM_PRINT,            query_enabled_stub,    exec_print},
    {IDM_HTMLEDITMODE,     NULL,                  exec_htmleditmode},
    {IDM_BASELINEFONT3,    NULL,                  exec_baselinefont3},
    {IDM_BLOCKDIRLTR,      query_enabled_stub,    NULL},
    {IDM_BLOCKDIRRTL,      query_enabled_stub,    NULL},
    {IDM_RESPECTVISIBILITY_INDESIGN, NULL,        exec_respectvisibility_indesign},
    {0,NULL,NULL}
};

static HRESULT WINAPI OleCommandTarget_QueryInterface(IOleCommandTarget *iface, REFIID riid, void **ppv)
{
    HTMLDocument *This = impl_from_IOleCommandTarget(iface);
    return htmldoc_query_interface(This, riid, ppv);
}

static ULONG WINAPI OleCommandTarget_AddRef(IOleCommandTarget *iface)
{
    HTMLDocument *This = impl_from_IOleCommandTarget(iface);
    return htmldoc_addref(This);
}

static ULONG WINAPI OleCommandTarget_Release(IOleCommandTarget *iface)
{
    HTMLDocument *This = impl_from_IOleCommandTarget(iface);
    return htmldoc_release(This);
}

static HRESULT query_from_table(HTMLDocumentNode *doc, const cmdtable_t *cmdtable, OLECMD *cmd)
{
    const cmdtable_t *iter = cmdtable;

    cmd->cmdf = 0;

    while(iter->id && iter->id != cmd->cmdID)
        iter++;

    if(!iter->id || !iter->query)
        return OLECMDERR_E_NOTSUPPORTED;

    return iter->query(doc, cmd);
}

static HRESULT WINAPI OleCommandTarget_QueryStatus(IOleCommandTarget *iface, const GUID *pguidCmdGroup,
        ULONG cCmds, OLECMD prgCmds[], OLECMDTEXT *pCmdText)
{
    HTMLDocument *This = impl_from_IOleCommandTarget(iface);
    HRESULT hres;

    TRACE("(%p)->(%s %d %p %p)\n", This, debugstr_guid(pguidCmdGroup), cCmds, prgCmds, pCmdText);

    if(pCmdText)
        FIXME("Unsupported pCmdText\n");
    if(!This->doc_node->browser)
        return E_UNEXPECTED;
    if(!cCmds)
        return S_OK;

    if(!pguidCmdGroup) {
        ULONG i;

        for(i=0; i<cCmds; i++) {
            if(prgCmds[i].cmdID < OLECMDID_OPEN || prgCmds[i].cmdID >= ARRAY_SIZE(exec_table)) {
                WARN("Unsupported cmdID = %d\n", prgCmds[i].cmdID);
                prgCmds[i].cmdf = 0;
            }else {
                if(prgCmds[i].cmdID == OLECMDID_OPEN || prgCmds[i].cmdID == OLECMDID_NEW) {
                    IOleCommandTarget *cmdtrg = NULL;
                    OLECMD olecmd;

                    prgCmds[i].cmdf = OLECMDF_SUPPORTED;
                    if(This->doc_obj->client) {
                        hres = IOleClientSite_QueryInterface(This->doc_obj->client, &IID_IOleCommandTarget,
                                (void**)&cmdtrg);
                        if(SUCCEEDED(hres)) {
                            olecmd.cmdID = prgCmds[i].cmdID;
                            olecmd.cmdf = 0;

                            hres = IOleCommandTarget_QueryStatus(cmdtrg, NULL, 1, &olecmd, NULL);
                            if(SUCCEEDED(hres) && olecmd.cmdf)
                                prgCmds[i].cmdf = olecmd.cmdf;
                        }
                    }else {
                        ERR("This->client == NULL, native would crash\n");
                    }
                }else {
                    prgCmds[i].cmdf = exec_table[prgCmds[i].cmdID].cmdf;
                    TRACE("cmdID = %d  returning %x\n", prgCmds[i].cmdID, prgCmds[i].cmdf);
                }
            }
        }

        return (prgCmds[cCmds-1].cmdf & OLECMDF_SUPPORTED) ? S_OK : OLECMDERR_E_NOTSUPPORTED;
    }

    if(IsEqualGUID(&CGID_MSHTML, pguidCmdGroup)) {
        ULONG i;

        for(i=0; i<cCmds; i++) {
            hres = query_from_table(This->doc_node, base_cmds, prgCmds+i);
            if(hres == OLECMDERR_E_NOTSUPPORTED)
                hres = query_from_table(This->doc_node, editmode_cmds, prgCmds+i);
            if(hres == OLECMDERR_E_NOTSUPPORTED)
                FIXME("CGID_MSHTML: unsupported cmdID %d\n", prgCmds[i].cmdID);
        }

        return (prgCmds[cCmds-1].cmdf & OLECMDF_SUPPORTED) ? S_OK : OLECMDERR_E_NOTSUPPORTED;
    }

    FIXME("Unsupported pguidCmdGroup %s\n", debugstr_guid(pguidCmdGroup));
    return OLECMDERR_E_UNKNOWNGROUP;
}

static HRESULT exec_from_table(HTMLDocumentNode *doc, const cmdtable_t *cmdtable, DWORD cmdid,
                               DWORD cmdexecopt, VARIANT *in, VARIANT *out)
{
    const cmdtable_t *iter = cmdtable;

    while(iter->id && iter->id != cmdid)
        iter++;

    if(!iter->id || !iter->exec)
        return OLECMDERR_E_NOTSUPPORTED;

    return iter->exec(doc, cmdexecopt, in, out);
}

static HRESULT WINAPI OleCommandTarget_Exec(IOleCommandTarget *iface, const GUID *pguidCmdGroup,
        DWORD nCmdID, DWORD nCmdexecopt, VARIANT *pvaIn, VARIANT *pvaOut)
{
    HTMLDocument *This = impl_from_IOleCommandTarget(iface);

    TRACE("(%p)->(%s %d %d %s %p)\n", This, debugstr_guid(pguidCmdGroup), nCmdID, nCmdexecopt, wine_dbgstr_variant(pvaIn), pvaOut);

    if(!This->doc_node->browser)
        return E_UNEXPECTED;

    if(!pguidCmdGroup) {
        if(nCmdID < OLECMDID_OPEN || nCmdID >= ARRAY_SIZE(exec_table) || !exec_table[nCmdID].func) {
            WARN("Unsupported cmdID = %d\n", nCmdID);
            return OLECMDERR_E_NOTSUPPORTED;
        }

        return exec_table[nCmdID].func(This->doc_node, nCmdexecopt, pvaIn, pvaOut);
    }else if(IsEqualGUID(&CGID_Explorer, pguidCmdGroup)) {
        FIXME("unsupported nCmdID %d of CGID_Explorer group\n", nCmdID);
        TRACE("%p %p\n", pvaIn, pvaOut);
        return OLECMDERR_E_NOTSUPPORTED;
    }else if(IsEqualGUID(&CGID_ShellDocView, pguidCmdGroup)) {
        FIXME("unsupported nCmdID %d of CGID_ShellDocView group\n", nCmdID);
        return OLECMDERR_E_NOTSUPPORTED;
    }else if(IsEqualGUID(&CGID_MSHTML, pguidCmdGroup)) {
        HRESULT hres = exec_from_table(This->doc_node, base_cmds, nCmdID, nCmdexecopt, pvaIn, pvaOut);
        if(hres == OLECMDERR_E_NOTSUPPORTED)
            hres = exec_from_table(This->doc_node, editmode_cmds, nCmdID,
                                   nCmdexecopt, pvaIn, pvaOut);
        if(hres == OLECMDERR_E_NOTSUPPORTED)
            FIXME("unsupported nCmdID %d of CGID_MSHTML group\n", nCmdID);

        return hres;
    }

    FIXME("Unsupported pguidCmdGroup %s\n", debugstr_guid(pguidCmdGroup));
    return OLECMDERR_E_UNKNOWNGROUP;
}

static const IOleCommandTargetVtbl OleCommandTargetVtbl = {
    OleCommandTarget_QueryInterface,
    OleCommandTarget_AddRef,
    OleCommandTarget_Release,
    OleCommandTarget_QueryStatus,
    OleCommandTarget_Exec
};

void show_context_menu(HTMLDocumentObj *This, DWORD dwID, POINT *ppt, IDispatch *elem)
{
    HMENU menu_res, menu;
    DWORD cmdid;

    if(This->hostui && S_OK == IDocHostUIHandler_ShowContextMenu(This->hostui,
            dwID, ppt, (IUnknown*)&This->basedoc.IOleCommandTarget_iface, elem))
        return;

    menu_res = LoadMenuW(get_shdoclc(), MAKEINTRESOURCEW(IDR_BROWSE_CONTEXT_MENU));
    menu = GetSubMenu(menu_res, dwID);

    cmdid = TrackPopupMenu(menu, TPM_LEFTALIGN | TPM_RIGHTBUTTON | TPM_RETURNCMD,
            ppt->x, ppt->y, 0, This->hwnd, NULL);
    DestroyMenu(menu_res);

    if(cmdid)
        IOleCommandTarget_Exec(&This->basedoc.IOleCommandTarget_iface, &CGID_MSHTML, cmdid, 0,
                NULL, NULL);
}

void HTMLDocument_OleCmd_Init(HTMLDocument *This)
{
    This->IOleCommandTarget_iface.lpVtbl = &OleCommandTargetVtbl;
}
