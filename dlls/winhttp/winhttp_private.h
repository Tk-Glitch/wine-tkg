/*
 * Copyright 2008 Hans Leidekker for CodeWeavers
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

#ifndef _WINE_WINHTTP_PRIVATE_H_
#define _WINE_WINHTTP_PRIVATE_H_

#include "wine/heap.h"
#include "wine/list.h"

#include "ole2.h"
#include "sspi.h"
#include "wincrypt.h"

#define WINHTTP_HANDLE_TYPE_SOCKET 4

struct object_header;
struct object_vtbl
{
    void (*destroy)( struct object_header * );
    BOOL (*query_option)( struct object_header *, DWORD, void *, DWORD * );
    BOOL (*set_option)( struct object_header *, DWORD, void *, DWORD );
};

struct object_header
{
    DWORD type;
    HINTERNET handle;
    const struct object_vtbl *vtbl;
    DWORD flags;
    DWORD disable_flags;
    DWORD logon_policy;
    DWORD redirect_policy;
    DWORD error;
    DWORD_PTR context;
    LONG refs;
    WINHTTP_STATUS_CALLBACK callback;
    DWORD notify_mask;
    struct list entry;
};

struct hostdata
{
    struct list entry;
    LONG ref;
    WCHAR *hostname;
    INTERNET_PORT port;
    BOOL secure;
    struct list connections;
};

struct session
{
    struct object_header hdr;
    CRITICAL_SECTION cs;
    WCHAR *agent;
    DWORD access;
    int resolve_timeout;
    int connect_timeout;
    int send_timeout;
    int receive_timeout;
    int receive_response_timeout;
    WCHAR *proxy_server;
    WCHAR *proxy_bypass;
    WCHAR *proxy_username;
    WCHAR *proxy_password;
    struct list cookie_cache;
    HANDLE unload_event;
    DWORD secure_protocols;
    DWORD passport_flags;
};

struct connect
{
    struct object_header hdr;
    struct session *session;
    WCHAR *hostname;    /* final destination of the request */
    WCHAR *servername;  /* name of the server we directly connect to */
    WCHAR *username;
    WCHAR *password;
    INTERNET_PORT hostport;
    INTERNET_PORT serverport;
    struct sockaddr_storage sockaddr;
    BOOL resolved;
};

struct netconn
{
    struct list entry;
    int socket;
    struct sockaddr_storage sockaddr;
    BOOL secure; /* SSL active on connection? */
    struct hostdata *host;
    ULONGLONG keep_until;
    CtxtHandle ssl_ctx;
    SecPkgContext_StreamSizes ssl_sizes;
    char *ssl_buf;
    char *extra_buf;
    size_t extra_len;
    char *peek_msg;
    char *peek_msg_mem;
    size_t peek_len;
};

struct header
{
    WCHAR *field;
    WCHAR *value;
    BOOL is_request; /* part of request headers? */
};

enum auth_target
{
    TARGET_INVALID = -1,
    TARGET_SERVER,
    TARGET_PROXY,
    TARGET_MAX
};

enum auth_scheme
{
    SCHEME_INVALID = -1,
    SCHEME_BASIC,
    SCHEME_NTLM,
    SCHEME_PASSPORT,
    SCHEME_DIGEST,
    SCHEME_NEGOTIATE,
    SCHEME_MAX
};

struct authinfo
{
    enum auth_scheme scheme;
    CredHandle cred;
    CtxtHandle ctx;
    TimeStamp exp;
    ULONG attr;
    ULONG max_token;
    char *data;
    unsigned int data_len;
    BOOL finished; /* finished authenticating */
};

struct queue
{
    TP_POOL *pool;
    TP_CALLBACK_ENVIRON env;
};

enum request_flags
{
    REQUEST_FLAG_WEBSOCKET_UPGRADE = 0x01,
};

struct request
{
    struct object_header hdr;
    struct connect *connect;
    enum request_flags flags;
    WCHAR *verb;
    WCHAR *path;
    WCHAR *version;
    WCHAR *raw_headers;
    void *optional;
    DWORD optional_len;
    struct netconn *netconn;
    DWORD security_flags;
    BOOL check_revocation;
    const CERT_CONTEXT *server_cert;
    const CERT_CONTEXT *client_cert;
    CredHandle cred_handle;
    BOOL cred_handle_initialized;
    int resolve_timeout;
    int connect_timeout;
    int send_timeout;
    int receive_timeout;
    int receive_response_timeout;
    DWORD max_redirects;
    DWORD redirect_count; /* total number of redirects during this request */
    WCHAR *status_text;
    DWORD content_length; /* total number of bytes to be read */
    DWORD content_read;   /* bytes read so far */
    BOOL  read_chunked;   /* are we reading in chunked mode? */
    BOOL  read_chunked_eof;  /* end of stream in chunked mode */
    BOOL  read_chunked_size; /* chunk size remaining */
    DWORD read_pos;       /* current read position in read_buf */
    DWORD read_size;      /* valid data size in read_buf */
    char  read_buf[8192]; /* buffer for already read but not returned data */
    struct header *headers;
    DWORD num_headers;
    struct authinfo *authinfo;
    struct authinfo *proxy_authinfo;
    struct queue queue;
    struct
    {
        WCHAR *username;
        WCHAR *password;
    } creds[TARGET_MAX][SCHEME_MAX];
};

enum socket_state
{
    SOCKET_STATE_OPEN     = 0,
    SOCKET_STATE_SHUTDOWN = 1,
    SOCKET_STATE_CLOSED   = 2,
};

/* rfc6455 */
enum socket_opcode
{
    SOCKET_OPCODE_CONTINUE  = 0x00,
    SOCKET_OPCODE_TEXT      = 0x01,
    SOCKET_OPCODE_BINARY    = 0x02,
    SOCKET_OPCODE_RESERVED3 = 0x03,
    SOCKET_OPCODE_RESERVED4 = 0x04,
    SOCKET_OPCODE_RESERVED5 = 0x05,
    SOCKET_OPCODE_RESERVED6 = 0x06,
    SOCKET_OPCODE_RESERVED7 = 0x07,
    SOCKET_OPCODE_CLOSE     = 0x08,
    SOCKET_OPCODE_PING      = 0x09,
    SOCKET_OPCODE_PONG      = 0x0a,
    SOCKET_OPCODE_INVALID   = 0xff,
};

struct socket
{
    struct object_header hdr;
    struct request *request;
    enum socket_state state;
    struct queue send_q;
    struct queue recv_q;
    enum socket_opcode opcode;
    DWORD read_size;
    USHORT status;
    char reason[123];
    DWORD reason_len;
};

struct send_request
{
    struct request *request;
    WCHAR *headers;
    DWORD headers_len;
    void *optional;
    DWORD optional_len;
    DWORD total_len;
    DWORD_PTR context;
};

struct receive_response
{
    struct request *request;
};

struct query_data
{
    struct request *request;
    DWORD *available;
};

struct read_data
{
    struct request *request;
    void *buffer;
    DWORD to_read;
    DWORD *read;
};

struct write_data
{
    struct request *request;
    const void *buffer;
    DWORD to_write;
    DWORD *written;
};

struct socket_send
{
    struct socket *socket;
    WINHTTP_WEB_SOCKET_BUFFER_TYPE type;
    const void *buf;
    DWORD len;
};

struct socket_receive
{
    struct socket *socket;
    void *buf;
    DWORD len;
};

struct socket_shutdown
{
    struct socket *socket;
    USHORT status;
    char reason[123];
    DWORD len;
};

struct object_header *addref_object( struct object_header * ) DECLSPEC_HIDDEN;
struct object_header *grab_object( HINTERNET ) DECLSPEC_HIDDEN;
void release_object( struct object_header * ) DECLSPEC_HIDDEN;
HINTERNET alloc_handle( struct object_header * ) DECLSPEC_HIDDEN;
BOOL free_handle( HINTERNET ) DECLSPEC_HIDDEN;

void send_callback( struct object_header *, DWORD, LPVOID, DWORD ) DECLSPEC_HIDDEN;
void close_connection( struct request * ) DECLSPEC_HIDDEN;
void stop_queue( struct queue * ) DECLSPEC_HIDDEN;

void netconn_close( struct netconn * ) DECLSPEC_HIDDEN;
DWORD netconn_create( struct hostdata *, const struct sockaddr_storage *, int, struct netconn ** ) DECLSPEC_HIDDEN;
void netconn_unload( void ) DECLSPEC_HIDDEN;
ULONG netconn_query_data_available( struct netconn * ) DECLSPEC_HIDDEN;
DWORD netconn_recv( struct netconn *, void *, size_t, int, int * ) DECLSPEC_HIDDEN;
DWORD netconn_resolve( WCHAR *, INTERNET_PORT, struct sockaddr_storage *, int ) DECLSPEC_HIDDEN;
DWORD netconn_secure_connect( struct netconn *, WCHAR *, DWORD, CredHandle *, BOOL ) DECLSPEC_HIDDEN;
DWORD netconn_send( struct netconn *, const void *, size_t, int * ) DECLSPEC_HIDDEN;
DWORD netconn_set_timeout( struct netconn *, BOOL, int ) DECLSPEC_HIDDEN;
BOOL netconn_is_alive( struct netconn * ) DECLSPEC_HIDDEN;
const void *netconn_get_certificate( struct netconn * ) DECLSPEC_HIDDEN;
int netconn_get_cipher_strength( struct netconn * ) DECLSPEC_HIDDEN;

BOOL set_cookies( struct request *, const WCHAR * ) DECLSPEC_HIDDEN;
DWORD add_cookie_headers( struct request * ) DECLSPEC_HIDDEN;
DWORD add_request_headers( struct request *, const WCHAR *, DWORD, DWORD ) DECLSPEC_HIDDEN;
void destroy_cookies( struct session * ) DECLSPEC_HIDDEN;
BOOL set_server_for_hostname( struct connect *, const WCHAR *, INTERNET_PORT ) DECLSPEC_HIDDEN;
void destroy_authinfo( struct authinfo * ) DECLSPEC_HIDDEN;

void release_host( struct hostdata * ) DECLSPEC_HIDDEN;
DWORD process_header( struct request *, const WCHAR *, const WCHAR *, DWORD, BOOL ) DECLSPEC_HIDDEN;

extern HRESULT WinHttpRequest_create( void ** ) DECLSPEC_HIDDEN;
void release_typelib( void ) DECLSPEC_HIDDEN;

static inline void* __WINE_ALLOC_SIZE(2) heap_realloc_zero( LPVOID mem, SIZE_T size )
{
    return HeapReAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, mem, size );
}

static inline WCHAR *strdupW( const WCHAR *src )
{
    WCHAR *dst;

    if (!src) return NULL;
    dst = heap_alloc( (lstrlenW( src ) + 1) * sizeof(WCHAR) );
    if (dst) lstrcpyW( dst, src );
    return dst;
}

static inline WCHAR *strdupAW( const char *src )
{
    WCHAR *dst = NULL;
    if (src)
    {
        int len = MultiByteToWideChar( CP_ACP, 0, src, -1, NULL, 0 );
        if ((dst = heap_alloc( len * sizeof(WCHAR) )))
            MultiByteToWideChar( CP_ACP, 0, src, -1, dst, len );
    }
    return dst;
}

static inline char *strdupWA( const WCHAR *src )
{
    char *dst = NULL;
    if (src)
    {
        int len = WideCharToMultiByte( CP_ACP, 0, src, -1, NULL, 0, NULL, NULL );
        if ((dst = heap_alloc( len )))
            WideCharToMultiByte( CP_ACP, 0, src, -1, dst, len, NULL, NULL );
    }
    return dst;
}

static inline char *strdupWA_sized( const WCHAR *src, DWORD size )
{
    char *dst = NULL;
    if (src)
    {
        int len = WideCharToMultiByte( CP_ACP, 0, src, size, NULL, 0, NULL, NULL ) + 1;
        if ((dst = heap_alloc( len )))
        {
            WideCharToMultiByte( CP_ACP, 0, src, size, dst, len, NULL, NULL );
            dst[len - 1] = 0;
        }
    }
    return dst;
}

extern HINSTANCE winhttp_instance DECLSPEC_HIDDEN;

#endif /* _WINE_WINHTTP_PRIVATE_H_ */
