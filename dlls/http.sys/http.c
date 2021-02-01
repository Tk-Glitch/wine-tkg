/*
 * HTTP server driver
 *
 * Copyright 2019 Zebediah Figura
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

#include <assert.h>
#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "wine/http.h"
#include "winternl.h"
#include "ddk/wdm.h"
#include "wine/debug.h"
#include "wine/heap.h"
#include "wine/list.h"

static HANDLE directory_obj;
static DEVICE_OBJECT *device_obj;

WINE_DEFAULT_DEBUG_CHANNEL(http);

#define DECLARE_CRITICAL_SECTION(cs) \
    static CRITICAL_SECTION cs; \
    static CRITICAL_SECTION_DEBUG cs##_debug = \
    { 0, 0, &cs, { &cs##_debug.ProcessLocksList, &cs##_debug.ProcessLocksList }, \
      0, 0, { (DWORD_PTR)(__FILE__ ": " # cs) }}; \
    static CRITICAL_SECTION cs = { &cs##_debug, -1, 0, 0, 0, 0 };

DECLARE_CRITICAL_SECTION(http_cs);

static HANDLE request_thread, request_event;
static BOOL thread_stop;

static HTTP_REQUEST_ID req_id_counter;

struct connection
{
    struct list entry; /* in "connections" below */

    SOCKET socket;

    char *buffer;
    unsigned int len, size;

    /* If there is a request fully received and waiting to be read, the
     * "available" parameter will be TRUE. Either there is no queue matching
     * the URL of this request yet ("queue" is NULL), there is a queue but no
     * IRPs have arrived for this request yet ("queue" is non-NULL and "req_id"
     * is HTTP_NULL_ID), or an IRP has arrived but did not provide a large
     * enough buffer to read the whole request ("queue" is non-NULL and
     * "req_id" is not HTTP_NULL_ID).
     *
     * If "available" is FALSE, either we are waiting for a new request
     * ("req_id" is HTTP_NULL_ID), or we are waiting for the user to send a
     * response ("req_id" is not HTTP_NULL_ID). */
    BOOL available;
    struct request_queue *queue;
    HTTP_REQUEST_ID req_id;

    /* Things we already parsed out of the request header in parse_request().
     * These are valid only if "available" is TRUE. */
    unsigned int req_len;
    HTTP_VERB verb;
    HTTP_VERSION version;
    const char *url, *host;
    ULONG unk_verb_len, url_len, content_len;
};

static struct list connections = LIST_INIT(connections);

struct request_queue
{
    struct list entry;
    LIST_ENTRY irp_queue;
    HTTP_URL_CONTEXT context;
    char *url;
    SOCKET socket;
};

static struct list request_queues = LIST_INIT(request_queues);

static void accept_connection(SOCKET socket)
{
    struct connection *conn;
    ULONG true = 1;
    SOCKET peer;

    if ((peer = accept(socket, NULL, NULL)) == INVALID_SOCKET)
        return;

    if (!(conn = heap_alloc_zero(sizeof(*conn))))
    {
        ERR("Failed to allocate memory.\n");
        shutdown(peer, SD_BOTH);
        closesocket(peer);
        return;
    }
    if (!(conn->buffer = heap_alloc(8192)))
    {
        ERR("Failed to allocate buffer memory.\n");
        heap_free(conn);
        shutdown(peer, SD_BOTH);
        closesocket(peer);
        return;
    }
    conn->size = 8192;
    WSAEventSelect(peer, request_event, FD_READ | FD_CLOSE);
    ioctlsocket(peer, FIONBIO, &true);
    conn->socket = peer;
    list_add_head(&connections, &conn->entry);
}

static void close_connection(struct connection *conn)
{
    heap_free(conn->buffer);
    shutdown(conn->socket, SD_BOTH);
    closesocket(conn->socket);
    list_remove(&conn->entry);
    heap_free(conn);
}

static HTTP_VERB parse_verb(const char *verb, int len)
{
    static const char *const verbs[] =
    {
        "OPTIONS",
        "GET",
        "HEAD",
        "POST",
        "PUT",
        "DELETE",
        "TRACE",
        "CONNECT",
        "TRACK",
        "MOVE",
        "COPY",
        "PROPFIND",
        "PROPPATCH",
        "MKCOL",
        "LOCK",
        "UNLOCK",
        "SEARCH",
    };
    unsigned int i;

    for (i = 0; i < ARRAY_SIZE(verbs); ++i)
    {
        if (!strncmp(verb, verbs[i], len))
            return HttpVerbOPTIONS + i;
    }
    return HttpVerbUnknown;
}

/* Return the length of a token, as defined in RFC 2616 section 2.2. */
static int parse_token(const char *str, const char *end)
{
    const char *p;
    for (p = str; !end || p < end; ++p)
    {
        if (!isgraph(*p) || strchr("()<>@,;:\\\"/[]?={}", *p))
            break;
    }
    return p - str;
}

static HTTP_HEADER_ID parse_header_name(const char *header, int len)
{
    static const char *const headers[] =
    {
        "Cache-Control",
        "Connection",
        "Date",
        "Keep-Alive",
        "Pragma",
        "Trailer",
        "Transfer-Encoding",
        "Upgrade",
        "Via",
        "Warning",
        "Allow",
        "Content-Length",
        "Content-Type",
        "Content-Encoding",
        "Content-Language",
        "Content-Location",
        "Content-MD5",
        "Content-Range",
        "Expires",
        "Last-Modified",
        "Accept",
        "Accept-Charset",
        "Accept-Encoding",
        "Accept-Language",
        "Authorization",
        "Cookie",
        "Expect",
        "From",
        "Host",
        "If-Match",
        "If-Modified-Since",
        "If-None-Match",
        "If-Range",
        "If-Unmodified-Since",
        "Max-Forwards",
        "Proxy-Authorization",
        "Referer",
        "Range",
        "TE",
        "Translate",
        "User-Agent",
    };
    unsigned int i;

    for (i = 0; i < ARRAY_SIZE(headers); ++i)
    {
        if (!strncmp(header, headers[i], len))
            return i;
    }
    return HttpHeaderRequestMaximum;
}

static void parse_header(const char *name, int *name_len, const char **value, int *value_len)
{
    const char *p = name;
    *name_len = parse_token(name, NULL);
    p += *name_len;
    while (*p == ' ' || *p == '\t') ++p;
    ++p; /* skip colon */
    while (*p == ' ' || *p == '\t') ++p;
    *value = p;
    while (isprint(*p) || *p == '\t') ++p;
    while (isspace(*p)) --p; /* strip trailing LWS */
    *value_len = p - *value + 1;
}

#define http_unknown_header http_unknown_header_64
#define http_data_chunk http_data_chunk_64
#define http_request http_request_64
#define complete_irp complete_irp_64
#define POINTER ULONGLONG
#include "request.h"
#undef http_unknown_header
#undef http_data_chunk
#undef http_request
#undef complete_irp
#undef POINTER

#define http_unknown_header http_unknown_header_32
#define http_data_chunk http_data_chunk_32
#define http_request http_request_32
#define complete_irp complete_irp_32
#define POINTER ULONG
#include "request.h"
#undef http_unknown_header
#undef http_data_chunk
#undef http_request
#undef complete_irp
#undef POINTER

static NTSTATUS complete_irp(struct connection *conn, IRP *irp)
{
    const struct http_receive_request_params params
            = *(struct http_receive_request_params *)irp->AssociatedIrp.SystemBuffer;

    TRACE("Completing IRP %p.\n", irp);

    if (!conn->req_id)
        conn->req_id = ++req_id_counter;

    if (params.bits == 32)
        return complete_irp_32(conn, irp);
    else
        return complete_irp_64(conn, irp);
}

/* Complete an IOCTL_HTTP_RECEIVE_REQUEST IRP if there is one to complete. */
static void try_complete_irp(struct connection *conn)
{
    LIST_ENTRY *entry;
    if (conn->queue && (entry = RemoveHeadList(&conn->queue->irp_queue)) != &conn->queue->irp_queue)
    {
        IRP *irp = CONTAINING_RECORD(entry, IRP, Tail.Overlay.ListEntry);
        irp->IoStatus.Status = complete_irp(conn, irp);
        IoCompleteRequest(irp, IO_NO_INCREMENT);
    }
}

/* Return 1 if str matches expect, 0 if str is incomplete, -1 if they don't match. */
static int compare_exact(const char *str, const char *expect, const char *end)
{
    while (*expect)
    {
        if (str >= end) return 0;
        if (*str++ != *expect++) return -1;
    }
    return 1;
}

static int parse_number(const char *str, const char **endptr, const char *end)
{
    int n = 0;
    while (str < end && isdigit(*str))
        n = n * 10 + (*str++ - '0');
    *endptr = str;
    return n;
}

static BOOL host_matches(const struct connection *conn, const struct request_queue *queue)
{
    const char *conn_host = (conn->url[0] == '/') ? conn->host : conn->url + 7;

    if (queue->url[7] == '+')
    {
        const char *queue_port = strchr(queue->url + 7, ':');
        return !strncmp(queue_port, strchr(conn_host, ':'), strlen(queue_port) - 1 /* strip final slash */);
    }

    return !memicmp(queue->url + 7, conn_host, strlen(queue->url) - 8 /* strip final slash */);
}

/* Upon receiving a request, parse it to ensure that it is a valid HTTP request,
 * and mark down some information that we will use later. Returns 1 if we parsed
 * a complete request, 0 if incomplete, -1 if invalid. */
static int parse_request(struct connection *conn)
{
    const char *const req = conn->buffer, *const end = conn->buffer + conn->len;
    struct request_queue *queue;
    const char *p = req, *q;
    int len, ret;

    if (!conn->len) return 0;

    TRACE("%s\n", wine_dbgstr_an(conn->buffer, conn->len));

    len = parse_token(p, end);
    if (p + len >= end) return 0;
    if (!len || p[len] != ' ') return -1;

    /* verb */
    if ((conn->verb = parse_verb(p, len)) == HttpVerbUnknown)
        conn->unk_verb_len = len;
    p += len + 1;

    TRACE("Got verb %u (%s).\n", conn->verb, debugstr_an(req, len));

    /* URL */
    conn->url = p;
    while (p < end && isgraph(*p)) ++p;
    conn->url_len = p - conn->url;
    if (p >= end) return 0;
    if (!conn->url_len) return -1;

    TRACE("Got URI %s.\n", debugstr_an(conn->url, conn->url_len));

    /* version */
    if ((ret = compare_exact(p, " HTTP/", end)) <= 0) return ret;
    p += 6;
    conn->version.MajorVersion = parse_number(p, &q, end);
    if (q >= end) return 0;
    if (q == p || *q != '.') return -1;
    p = q + 1;
    if (p >= end) return 0;
    conn->version.MinorVersion = parse_number(p, &q, end);
    if (q >= end) return 0;
    if (q == p) return -1;
    p = q;
    if ((ret = compare_exact(p, "\r\n", end)) <= 0) return ret;
    p += 2;

    TRACE("Got version %hu.%hu.\n", conn->version.MajorVersion, conn->version.MinorVersion);

    /* headers */
    conn->host = NULL;
    conn->content_len = 0;
    for (;;)
    {
        const char *name = p;

        if (!(ret = compare_exact(p, "\r\n", end))) return 0;
        else if (ret > 0) break;

        len = parse_token(p, end);
        if (p + len >= end) return 0;
        if (!len) return -1;
        p += len;
        while (p < end && (*p == ' ' || *p == '\t')) ++p;
        if (p >= end) return 0;
        if (*p != ':') return -1;
        ++p;
        while (p < end && (*p == ' ' || *p == '\t')) ++p;

        TRACE("Got %s header.\n", debugstr_an(name, len));

        if (!strncmp(name, "Host", len))
            conn->host = p;
        else if (!strncmp(name, "Content-Length", len))
        {
            conn->content_len = parse_number(p, &q, end);
            if (q >= end) return 0;
            if (q == p) return -1;
        }
        else if (!strncmp(name, "Transfer-Encoding", len))
            FIXME("Unhandled Transfer-Encoding header.\n");
        while (p < end && (isprint(*p) || *p == '\t')) ++p;
        if ((ret = compare_exact(p, "\r\n", end)) <= 0) return ret;
        p += 2;
    }
    p += 2;
    if (conn->url[0] == '/' && !conn->host) return -1;

    if (end - p < conn->content_len) return 0;

    conn->req_len = (p - req) + conn->content_len;

    TRACE("Received a full request, length %u bytes.\n", conn->req_len);

    conn->queue = NULL;
    /* Find a queue which can receive this request. */
    LIST_FOR_EACH_ENTRY(queue, &request_queues, struct request_queue, entry)
    {
        if (host_matches(conn, queue))
        {
            TRACE("Assigning request to queue %p.\n", queue);
            conn->queue = queue;
            break;
        }
    }

    /* Stop selecting on incoming data until a response is queued. */
    WSAEventSelect(conn->socket, request_event, FD_CLOSE);

    conn->available = TRUE;
    try_complete_irp(conn);

    return 1;
}

static void format_date(char *buffer)
{
    static const char day_names[7][4] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    static const char month_names[12][4] =
            {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    SYSTEMTIME date;
    GetSystemTime(&date);
    sprintf(buffer + strlen(buffer), "Date: %s, %02u %s %u %02u:%02u:%02u GMT\r\n",
            day_names[date.wDayOfWeek], date.wDay, month_names[date.wMonth - 1],
            date.wYear, date.wHour, date.wMinute, date.wSecond);
}

/* Send a 400 Bad Request response. */
static void send_400(struct connection *conn)
{
    static const char response_header[] = "HTTP/1.1 400 Bad Request\r\n";
    static const char response_body[] =
        "Content-Type: text/html; charset=utf-8\r\n"
        "Content-Language: en\r\n"
        "Connection: close\r\n";
    char buffer[sizeof(response_header) + sizeof(response_body) + 37];

    strcpy(buffer, response_header);
    format_date(buffer + strlen(buffer));
    strcat(buffer, response_body);
    if (send(conn->socket, buffer, strlen(buffer), 0) < 0)
        ERR("Failed to send 400 response, error %u.\n", WSAGetLastError());
}

static void receive_data(struct connection *conn)
{
    int len, ret;

    /* We might be waiting for an IRP, but always call recv() anyway, since we
     * might have been woken up by the socket closing. */
    if ((len = recv(conn->socket, conn->buffer + conn->len, conn->size - conn->len, 0)) <= 0)
    {
        if (WSAGetLastError() == WSAEWOULDBLOCK)
            return; /* nothing to receive */
        else if (!len)
            TRACE("Connection was shut down by peer.\n");
        else
            ERR("Got error %u; shutting down connection.\n", WSAGetLastError());
        close_connection(conn);
        return;
    }
    conn->len += len;

    if (conn->available)
        return; /* waiting for an HttpReceiveHttpRequest() call */
    if (conn->req_id != HTTP_NULL_ID)
        return; /* waiting for an HttpSendHttpResponse() call */

    TRACE("Received %u bytes of data.\n", len);

    if (!(ret = parse_request(conn)))
    {
        ULONG available;
        ioctlsocket(conn->socket, FIONREAD, &available);
        if (available)
        {
            TRACE("%u more bytes of data available, trying with larger buffer.\n", available);
            if (!(conn->buffer = heap_realloc(conn->buffer, conn->len + available)))
            {
                ERR("Failed to allocate %u bytes of memory.\n", conn->len + available);
                close_connection(conn);
                return;
            }
            conn->size = conn->len + available;

            if ((len = recv(conn->socket, conn->buffer + conn->len, conn->size - conn->len, 0)) < 0)
            {
                ERR("Got error %u; shutting down connection.\n", WSAGetLastError());
                close_connection(conn);
                return;
            }
            TRACE("Received %u bytes of data.\n", len);
            conn->len += len;
            ret = parse_request(conn);
        }
    }
    if (!ret)
        TRACE("Request is incomplete, waiting for more data.\n");
    else if (ret < 0)
    {
        WARN("Failed to parse request; shutting down connection.\n");
        send_400(conn);
        close_connection(conn);
    }
}

static DWORD WINAPI request_thread_proc(void *arg)
{
    struct connection *conn, *cursor;
    struct request_queue *queue;

    TRACE("Starting request thread.\n");

    while (!WaitForSingleObject(request_event, INFINITE))
    {
        EnterCriticalSection(&http_cs);

        LIST_FOR_EACH_ENTRY(queue, &request_queues, struct request_queue, entry)
        {
            if (queue->socket != -1)
                accept_connection(queue->socket);
        }

        LIST_FOR_EACH_ENTRY_SAFE(conn, cursor, &connections, struct connection, entry)
        {
            receive_data(conn);
        }

        LeaveCriticalSection(&http_cs);
    }

    TRACE("Stopping request thread.\n");

    return 0;
}

static NTSTATUS http_add_url(struct request_queue *queue, IRP *irp)
{
    const struct http_add_url_params *params = irp->AssociatedIrp.SystemBuffer;
    struct sockaddr_in addr;
    struct connection *conn;
    unsigned int count = 0;
    char *url, *endptr;
    ULONG true = 1;
    const char *p;
    SOCKET s;

    TRACE("host %s, context %s.\n", debugstr_a(params->url), wine_dbgstr_longlong(params->context));

    if (!strncmp(params->url, "https://", 8))
    {
        FIXME("HTTPS is not implemented.\n");
        return STATUS_NOT_IMPLEMENTED;
    }
    else if (strncmp(params->url, "http://", 7) || !strchr(params->url + 7, ':')
            || params->url[strlen(params->url) - 1] != '/')
        return STATUS_INVALID_PARAMETER;
    if (!(addr.sin_port = htons(strtol(strchr(params->url + 7, ':') + 1, &endptr, 10))) || *endptr != '/')
        return STATUS_INVALID_PARAMETER;

    if (!(url = heap_alloc(strlen(params->url)+1)))
        return STATUS_NO_MEMORY;
    strcpy(url, params->url);

    for (p = url; *p; ++p)
        if (*p == '/') ++count;
    if (count > 3)
        FIXME("Binding to relative URIs is not implemented; binding to all URIs instead.\n");

    EnterCriticalSection(&http_cs);

    if (queue->url && !strcmp(queue->url, url))
    {
        LeaveCriticalSection(&http_cs);
        heap_free(url);
        return STATUS_OBJECT_NAME_COLLISION;
    }
    else if (queue->url)
    {
        FIXME("Binding to multiple URLs is not implemented.\n");
        LeaveCriticalSection(&http_cs);
        heap_free(url);
        return STATUS_NOT_IMPLEMENTED;
    }

    if ((s = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET)
    {
        ERR("Failed to create socket, error %u.\n", WSAGetLastError());
        LeaveCriticalSection(&http_cs);
        heap_free(url);
        return STATUS_UNSUCCESSFUL;
    }

    addr.sin_family = AF_INET;
    addr.sin_addr.S_un.S_addr = INADDR_ANY;
    if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) == -1)
    {
        LeaveCriticalSection(&http_cs);
        closesocket(s);
        heap_free(url);
        if (WSAGetLastError() == WSAEADDRINUSE)
        {
            WARN("Address %s is already in use.\n", debugstr_a(params->url));
            return STATUS_SHARING_VIOLATION;
        }
        else if (WSAGetLastError() == WSAEACCES)
        {
            WARN("Not enough permissions to bind to address %s.\n", debugstr_a(params->url));
            return STATUS_ACCESS_DENIED;
        }
        ERR("Failed to bind socket, error %u.\n", WSAGetLastError());
        return STATUS_UNSUCCESSFUL;
    }

    if (listen(s, SOMAXCONN) == -1)
    {
        ERR("Failed to listen to port %u, error %u.\n", addr.sin_port, WSAGetLastError());
        LeaveCriticalSection(&http_cs);
        closesocket(s);
        heap_free(url);
        return STATUS_OBJECT_NAME_COLLISION;
    }

    ioctlsocket(s, FIONBIO, &true);
    WSAEventSelect(s, request_event, FD_ACCEPT);
    queue->socket = s;
    queue->url = url;
    queue->context = params->context;

    /* See if any pending requests now match this queue. */
    LIST_FOR_EACH_ENTRY(conn, &connections, struct connection, entry)
    {
        if (conn->available && !conn->queue && host_matches(conn, queue))
        {
            conn->queue = queue;
            try_complete_irp(conn);
        }
    }

    LeaveCriticalSection(&http_cs);

    return STATUS_SUCCESS;
}

static NTSTATUS http_remove_url(struct request_queue *queue, IRP *irp)
{
    const char *url = irp->AssociatedIrp.SystemBuffer;

    TRACE("host %s.\n", debugstr_a(url));

    EnterCriticalSection(&http_cs);

    if (!queue->url || strcmp(url, queue->url))
    {
        LeaveCriticalSection(&http_cs);
        return STATUS_OBJECT_NAME_NOT_FOUND;
    }
    heap_free(queue->url);
    queue->url = NULL;

    LeaveCriticalSection(&http_cs);
    return STATUS_SUCCESS;
}

static struct connection *get_connection(HTTP_REQUEST_ID req_id)
{
    struct connection *conn;

    LIST_FOR_EACH_ENTRY(conn, &connections, struct connection, entry)
    {
        if (conn->req_id == req_id)
            return conn;
    }
    return NULL;
}

static void WINAPI http_receive_request_cancel(DEVICE_OBJECT *device, IRP *irp)
{
    TRACE("device %p, irp %p.\n", device, irp);

    IoReleaseCancelSpinLock(irp->CancelIrql);

    EnterCriticalSection(&http_cs);
    RemoveEntryList(&irp->Tail.Overlay.ListEntry);
    LeaveCriticalSection(&http_cs);

    irp->IoStatus.Status = STATUS_CANCELLED;
    IoCompleteRequest(irp, IO_NO_INCREMENT);
}

static NTSTATUS http_receive_request(struct request_queue *queue, IRP *irp)
{
    const struct http_receive_request_params *params = irp->AssociatedIrp.SystemBuffer;
    struct connection *conn;
    NTSTATUS ret;

    TRACE("addr %s, id %s, flags %#x, bits %u.\n", wine_dbgstr_longlong(params->addr),
            wine_dbgstr_longlong(params->id), params->flags, params->bits);

    EnterCriticalSection(&http_cs);

    if ((conn = get_connection(params->id)) && conn->available && conn->queue == queue)
    {
        ret = complete_irp(conn, irp);
        LeaveCriticalSection(&http_cs);
        return ret;
    }

    if (params->id == HTTP_NULL_ID)
    {
        TRACE("Queuing IRP %p.\n", irp);

        IoSetCancelRoutine(irp, http_receive_request_cancel);
        if (irp->Cancel && !IoSetCancelRoutine(irp, NULL))
        {
            /* The IRP was canceled before we set the cancel routine. */
            ret = STATUS_CANCELLED;
        }
        else
        {
            IoMarkIrpPending(irp);
            InsertTailList(&queue->irp_queue, &irp->Tail.Overlay.ListEntry);
            ret = STATUS_PENDING;
        }
    }
    else
        ret = STATUS_CONNECTION_INVALID;

    LeaveCriticalSection(&http_cs);

    return ret;
}

static NTSTATUS http_send_response(struct request_queue *queue, IRP *irp)
{
    const struct http_response *response = irp->AssociatedIrp.SystemBuffer;
    struct connection *conn;

    TRACE("id %s, len %d.\n", wine_dbgstr_longlong(response->id), response->len);

    EnterCriticalSection(&http_cs);

    if ((conn = get_connection(response->id)))
    {
        if (send(conn->socket, response->buffer, response->len, 0) >= 0)
        {
            if (conn->content_len)
            {
                /* Discard whatever entity body is left. */
                memmove(conn->buffer, conn->buffer + conn->content_len, conn->len - conn->content_len);
                conn->len -= conn->content_len;
            }

            conn->queue = NULL;
            conn->req_id = HTTP_NULL_ID;
            WSAEventSelect(conn->socket, request_event, FD_READ | FD_CLOSE);
            irp->IoStatus.Information = response->len;
            /* We might have another request already in the buffer. */
            if (parse_request(conn) < 0)
            {
                WARN("Failed to parse request; shutting down connection.\n");
                send_400(conn);
                close_connection(conn);
            }
        }
        else
        {
            ERR("Got error %u; shutting down connection.\n", WSAGetLastError());
            close_connection(conn);
        }

        LeaveCriticalSection(&http_cs);
        return STATUS_SUCCESS;
    }

    LeaveCriticalSection(&http_cs);
    return STATUS_CONNECTION_INVALID;
}

static NTSTATUS http_receive_body(struct request_queue *queue, IRP *irp)
{
    const struct http_receive_body_params *params = irp->AssociatedIrp.SystemBuffer;
    IO_STACK_LOCATION *stack = IoGetCurrentIrpStackLocation(irp);
    const DWORD output_len = stack->Parameters.DeviceIoControl.OutputBufferLength;
    struct connection *conn;
    NTSTATUS ret;

    TRACE("id %s, bits %u.\n", wine_dbgstr_longlong(params->id), params->bits);

    EnterCriticalSection(&http_cs);

    if ((conn = get_connection(params->id)))
    {
        TRACE("%u bits remaining.\n", conn->content_len);

        if (conn->content_len)
        {
            ULONG len = min(conn->content_len, output_len);
            memcpy(irp->AssociatedIrp.SystemBuffer, conn->buffer, len);
            memmove(conn->buffer, conn->buffer + len, conn->len - len);
            conn->content_len -= len;
            conn->len -= len;

            irp->IoStatus.Information = len;
            ret = STATUS_SUCCESS;
        }
        else
            ret = STATUS_END_OF_FILE;
    }
    else
        ret = STATUS_CONNECTION_INVALID;

    LeaveCriticalSection(&http_cs);

    return ret;
}

static NTSTATUS WINAPI dispatch_ioctl(DEVICE_OBJECT *device, IRP *irp)
{
    IO_STACK_LOCATION *stack = IoGetCurrentIrpStackLocation(irp);
    struct request_queue *queue = stack->FileObject->FsContext;
    NTSTATUS ret;

    switch (stack->Parameters.DeviceIoControl.IoControlCode)
    {
    case IOCTL_HTTP_ADD_URL:
        ret = http_add_url(queue, irp);
        break;
    case IOCTL_HTTP_REMOVE_URL:
        ret = http_remove_url(queue, irp);
        break;
    case IOCTL_HTTP_RECEIVE_REQUEST:
        ret = http_receive_request(queue, irp);
        break;
    case IOCTL_HTTP_SEND_RESPONSE:
        ret = http_send_response(queue, irp);
        break;
    case IOCTL_HTTP_RECEIVE_BODY:
        ret = http_receive_body(queue, irp);
        break;
    default:
        FIXME("Unhandled ioctl %#x.\n", stack->Parameters.DeviceIoControl.IoControlCode);
        ret = STATUS_NOT_IMPLEMENTED;
    }

    if (ret != STATUS_PENDING)
    {
        irp->IoStatus.Status = ret;
        IoCompleteRequest(irp, IO_NO_INCREMENT);
    }
    return ret;
}

static NTSTATUS WINAPI dispatch_create(DEVICE_OBJECT *device, IRP *irp)
{
    IO_STACK_LOCATION *stack = IoGetCurrentIrpStackLocation(irp);
    struct request_queue *queue;

    if (!(queue = heap_alloc_zero(sizeof(*queue))))
        return STATUS_NO_MEMORY;
    stack->FileObject->FsContext = queue;
    InitializeListHead(&queue->irp_queue);

    EnterCriticalSection(&http_cs);
    list_add_head(&request_queues, &queue->entry);
    LeaveCriticalSection(&http_cs);

    TRACE("Created queue %p.\n", queue);

    irp->IoStatus.Status = STATUS_SUCCESS;
    IoCompleteRequest(irp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;
}

static void close_queue(struct request_queue *queue)
{
    EnterCriticalSection(&http_cs);
    list_remove(&queue->entry);
    if (queue->socket != -1)
    {
        shutdown(queue->socket, SD_BOTH);
        closesocket(queue->socket);
    }
    LeaveCriticalSection(&http_cs);

    heap_free(queue->url);
    heap_free(queue);
}

static NTSTATUS WINAPI dispatch_close(DEVICE_OBJECT *device, IRP *irp)
{
    IO_STACK_LOCATION *stack = IoGetCurrentIrpStackLocation(irp);
    struct request_queue *queue = stack->FileObject->FsContext;
    LIST_ENTRY *entry;

    TRACE("Closing queue %p.\n", queue);

    EnterCriticalSection(&http_cs);

    while ((entry = queue->irp_queue.Flink) != &queue->irp_queue)
    {
        IRP *queued_irp = CONTAINING_RECORD(entry, IRP, Tail.Overlay.ListEntry);
        IoCancelIrp(queued_irp);
    }

    LeaveCriticalSection(&http_cs);

    close_queue(queue);

    irp->IoStatus.Status = STATUS_SUCCESS;
    IoCompleteRequest(irp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;
}

static void WINAPI unload(DRIVER_OBJECT *driver)
{
    struct request_queue *queue, *queue_next;
    struct connection *conn, *conn_next;

    thread_stop = TRUE;
    SetEvent(request_event);
    WaitForSingleObject(request_thread, INFINITE);
    CloseHandle(request_thread);
    CloseHandle(request_event);

    LIST_FOR_EACH_ENTRY_SAFE(conn, conn_next, &connections, struct connection, entry)
    {
        close_connection(conn);
    }

    LIST_FOR_EACH_ENTRY_SAFE(queue, queue_next, &request_queues, struct request_queue, entry)
    {
        close_queue(queue);
    }

    WSACleanup();

    IoDeleteDevice(device_obj);
    NtClose(directory_obj);
}

NTSTATUS WINAPI DriverEntry(DRIVER_OBJECT *driver, UNICODE_STRING *path)
{
    OBJECT_ATTRIBUTES attr = {sizeof(attr)};
    UNICODE_STRING string;
    WSADATA wsadata;
    NTSTATUS ret;

    TRACE("driver %p, path %s.\n", driver, debugstr_w(path->Buffer));

    RtlInitUnicodeString(&string, L"\\Device\\Http");
    attr.ObjectName = &string;
    if ((ret = NtCreateDirectoryObject(&directory_obj, 0, &attr)) && ret != STATUS_OBJECT_NAME_COLLISION)
        ERR("Failed to create \\Device\\Http directory, status %#x.\n", ret);

    RtlInitUnicodeString(&string, L"\\Device\\Http\\ReqQueue");
    if ((ret = IoCreateDevice(driver, 0, &string, FILE_DEVICE_UNKNOWN, 0, FALSE, &device_obj)))
    {
        ERR("Failed to create request queue device, status %#x.\n", ret);
        NtClose(directory_obj);
        return ret;
    }

    driver->MajorFunction[IRP_MJ_CREATE] = dispatch_create;
    driver->MajorFunction[IRP_MJ_CLOSE] = dispatch_close;
    driver->MajorFunction[IRP_MJ_DEVICE_CONTROL] = dispatch_ioctl;
    driver->DriverUnload = unload;

    WSAStartup(MAKEWORD(1,1), &wsadata);

    request_event = CreateEventW(NULL, FALSE, FALSE, NULL);
    request_thread = CreateThread(NULL, 0, request_thread_proc, NULL, 0, NULL);

    return STATUS_SUCCESS;
}
