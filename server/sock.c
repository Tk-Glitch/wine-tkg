/*
 * Server-side socket management
 *
 * Copyright (C) 1999 Marcus Meissner, Ove Kåven
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
 * FIXME: we use read|write access in all cases. Shouldn't we depend that
 * on the access of the current handle?
 */

#include "config.h"

#include <assert.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#ifdef HAVE_NETINET_IN_H
# include <netinet/in.h>
#endif
#ifdef HAVE_POLL_H
# include <poll.h>
#endif
#include <sys/time.h>
#include <sys/types.h>
#ifdef HAVE_SYS_SOCKET_H
# include <sys/socket.h>
#endif
#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif
#ifdef HAVE_SYS_FILIO_H
# include <sys/filio.h>
#endif
#include <time.h>
#include <unistd.h>
#include <limits.h>
#ifdef HAVE_LINUX_RTNETLINK_H
# include <linux/rtnetlink.h>
#endif

#ifdef HAVE_NETIPX_IPX_H
# include <netipx/ipx.h>
#elif defined(HAVE_LINUX_IPX_H)
# ifdef HAVE_ASM_TYPES_H
#  include <asm/types.h>
# endif
# ifdef HAVE_LINUX_TYPES_H
#  include <linux/types.h>
# endif
# include <linux/ipx.h>
#endif
#if defined(SOL_IPX) || defined(SO_DEFAULT_HEADERS)
# define HAS_IPX
#endif

#ifdef HAVE_LINUX_IRDA_H
# ifdef HAVE_LINUX_TYPES_H
#  include <linux/types.h>
# endif
# include <linux/irda.h>
# define HAS_IRDA
#endif

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winternl.h"
#include "winerror.h"
#define USE_WS_PREFIX
#include "winsock2.h"
#include "ws2tcpip.h"
#include "wsipx.h"
#include "af_irda.h"
#include "wine/afd.h"

#include "process.h"
#include "file.h"
#include "handle.h"
#include "thread.h"
#include "request.h"
#include "user.h"

static struct list poll_list = LIST_INIT( poll_list );

struct poll_req
{
    struct list entry;
    struct async *async;
    struct iosb *iosb;
    struct timeout_user *timeout;
    unsigned int count;
    struct poll_socket_output *output;
    struct
    {
        struct sock *sock;
        int flags;
    } sockets[1];
};

struct accept_req
{
    struct list entry;
    struct async *async;
    struct iosb *iosb;
    struct sock *sock, *acceptsock;
    int accepted;
    unsigned int recv_len, local_len;
};

struct connect_req
{
    struct async *async;
    struct iosb *iosb;
    struct sock *sock;
    unsigned int addr_len, send_len, send_cursor;
};

struct sock
{
    struct object       obj;         /* object header */
    struct fd          *fd;          /* socket file descriptor */
    unsigned int        state;       /* status bits */
    unsigned int        mask;        /* event mask */
    /* pending FD_* events which have not yet been reported to the application */
    unsigned int        pending_events;
    /* FD_* events which have already been reported and should not be selected
     * for again until reset by a relevant call.
     *
     * For example, if FD_READ is set here and not in pending_events, it has
     * already been reported and consumed, and we should not report it again,
     * even if POLLIN is signaled, until it is reset by e.g recv().
     *
     * If an event has been signaled and not consumed yet, it will be set in
     * both pending_events and reported_events (as we should only ever report
     * any event once until it is reset.) */
    unsigned int        reported_events;
    unsigned int        flags;       /* socket flags */
    int                 wr_shutdown_pending; /* is a write shutdown pending? */
    unsigned short      proto;       /* socket protocol */
    unsigned short      type;        /* socket type */
    unsigned short      family;      /* socket family */
    struct event       *event;       /* event object */
    user_handle_t       window;      /* window to send the message to */
    unsigned int        message;     /* message to send */
    obj_handle_t        wparam;      /* message wparam (socket handle) */
    unsigned int        errors[FD_MAX_EVENTS]; /* event errors */
    timeout_t           connect_time;/* time the socket was connected */
    struct sock        *deferred;    /* socket that waits for a deferred accept */
    struct async_queue  read_q;      /* queue for asynchronous reads */
    struct async_queue  write_q;     /* queue for asynchronous writes */
    struct async_queue  ifchange_q;  /* queue for interface change notifications */
    struct async_queue  accept_q;    /* queue for asynchronous accepts */
    struct async_queue  connect_q;   /* queue for asynchronous connects */
    struct async_queue  poll_q;      /* queue for asynchronous polls */
    struct object      *ifchange_obj; /* the interface change notification object */
    struct list         ifchange_entry; /* entry in ifchange notification list */
    struct list         accept_list; /* list of pending accept requests */
    struct accept_req  *accept_recv_req; /* pending accept-into request which will recv on this socket */
    struct connect_req *connect_req; /* pending connection request */
};

static void sock_dump( struct object *obj, int verbose );
static struct fd *sock_get_fd( struct object *obj );
static int sock_close_handle( struct object *obj, struct process *process, obj_handle_t handle );
static void sock_destroy( struct object *obj );
static struct object *sock_get_ifchange( struct sock *sock );
static void sock_release_ifchange( struct sock *sock );

static int sock_get_poll_events( struct fd *fd );
static void sock_poll_event( struct fd *fd, int event );
static enum server_fd_type sock_get_fd_type( struct fd *fd );
static int sock_ioctl( struct fd *fd, ioctl_code_t code, struct async *async );
static void sock_queue_async( struct fd *fd, struct async *async, int type, int count );
static void sock_reselect_async( struct fd *fd, struct async_queue *queue );

static int accept_into_socket( struct sock *sock, struct sock *acceptsock );
static struct sock *accept_socket( struct sock *sock );
static int sock_get_ntstatus( int err );
static unsigned int sock_get_error( int err );

static const struct object_ops sock_ops =
{
    sizeof(struct sock),          /* size */
    &file_type,                   /* type */
    sock_dump,                    /* dump */
    add_queue,                    /* add_queue */
    remove_queue,                 /* remove_queue */
    default_fd_signaled,          /* signaled */
    NULL,                         /* get_esync_fd */
    NULL,                         /* get_fsync_idx */
    no_satisfied,                 /* satisfied */
    no_signal,                    /* signal */
    sock_get_fd,                  /* get_fd */
    default_map_access,           /* map_access */
    default_get_sd,               /* get_sd */
    default_set_sd,               /* set_sd */
    no_get_full_name,             /* get_full_name */
    no_lookup_name,               /* lookup_name */
    no_link_name,                 /* link_name */
    NULL,                         /* unlink_name */
    no_open_file,                 /* open_file */
    no_kernel_obj_list,           /* get_kernel_obj_list */
    sock_close_handle,            /* close_handle */
    sock_destroy                  /* destroy */
};

static const struct fd_ops sock_fd_ops =
{
    sock_get_poll_events,         /* get_poll_events */
    sock_poll_event,              /* poll_event */
    sock_get_fd_type,             /* get_fd_type */
    no_fd_read,                   /* read */
    no_fd_write,                  /* write */
    no_fd_flush,                  /* flush */
    default_fd_get_file_info,     /* get_file_info */
    no_fd_get_volume_info,        /* get_volume_info */
    sock_ioctl,                   /* ioctl */
    sock_queue_async,             /* queue_async */
    sock_reselect_async           /* reselect_async */
};

union unix_sockaddr
{
    struct sockaddr addr;
    struct sockaddr_in in;
    struct sockaddr_in6 in6;
#ifdef HAS_IPX
    struct sockaddr_ipx ipx;
#endif
#ifdef HAS_IRDA
    struct sockaddr_irda irda;
#endif
};

static int sockaddr_from_unix( const union unix_sockaddr *uaddr, struct WS_sockaddr *wsaddr, socklen_t wsaddrlen )
{
    memset( wsaddr, 0, wsaddrlen );

    switch (uaddr->addr.sa_family)
    {
    case AF_INET:
    {
        struct WS_sockaddr_in win = {0};

        if (wsaddrlen < sizeof(win)) return -1;
        win.sin_family = WS_AF_INET;
        win.sin_port = uaddr->in.sin_port;
        memcpy( &win.sin_addr, &uaddr->in.sin_addr, sizeof(win.sin_addr) );
        memcpy( wsaddr, &win, sizeof(win) );
        return sizeof(win);
    }

    case AF_INET6:
    {
        struct WS_sockaddr_in6 win = {0};

        if (wsaddrlen < sizeof(struct WS_sockaddr_in6_old)) return -1;
        win.sin6_family = WS_AF_INET6;
        win.sin6_port = uaddr->in6.sin6_port;
        win.sin6_flowinfo = uaddr->in6.sin6_flowinfo;
        memcpy( &win.sin6_addr, &uaddr->in6.sin6_addr, sizeof(win.sin6_addr) );
#ifdef HAVE_STRUCT_SOCKADDR_IN6_SIN6_SCOPE_ID
        win.sin6_scope_id = uaddr->in6.sin6_scope_id;
#endif
        if (wsaddrlen >= sizeof(struct WS_sockaddr_in6))
        {
            memcpy( wsaddr, &win, sizeof(struct WS_sockaddr_in6) );
            return sizeof(struct WS_sockaddr_in6);
        }
        memcpy( wsaddr, &win, sizeof(struct WS_sockaddr_in6_old) );
        return sizeof(struct WS_sockaddr_in6_old);
    }

#ifdef HAS_IPX
    case AF_IPX:
    {
        struct WS_sockaddr_ipx win = {0};

        if (wsaddrlen < sizeof(win)) return -1;
        win.sa_family = WS_AF_IPX;
        memcpy( win.sa_netnum, &uaddr->ipx.sipx_network, sizeof(win.sa_netnum) );
        memcpy( win.sa_nodenum, &uaddr->ipx.sipx_node, sizeof(win.sa_nodenum) );
        win.sa_socket = uaddr->ipx.sipx_port;
        memcpy( wsaddr, &win, sizeof(win) );
        return sizeof(win);
    }
#endif

#ifdef HAS_IRDA
    case AF_IRDA:
    {
        SOCKADDR_IRDA win;

        if (wsaddrlen < sizeof(win)) return -1;
        win.irdaAddressFamily = WS_AF_IRDA;
        memcpy( win.irdaDeviceID, &uaddr->irda.sir_addr, sizeof(win.irdaDeviceID) );
        if (uaddr->irda.sir_lsap_sel != LSAP_ANY)
            snprintf( win.irdaServiceName, sizeof(win.irdaServiceName), "LSAP-SEL%u", uaddr->irda.sir_lsap_sel );
        else
            memcpy( win.irdaServiceName, uaddr->irda.sir_name, sizeof(win.irdaServiceName) );
        memcpy( wsaddr, &win, sizeof(win) );
        return sizeof(win);
    }
#endif

    case AF_UNSPEC:
        return 0;

    default:
        return -1;

    }
}

/* Permutation of 0..FD_MAX_EVENTS - 1 representing the order in which
 * we post messages if there are multiple events.  Used to send
 * messages.  The problem is if there is both a FD_CONNECT event and,
 * say, an FD_READ event available on the same socket, we want to
 * notify the app of the connect event first.  Otherwise it may
 * discard the read event because it thinks it hasn't connected yet.
 */
static const int event_bitorder[FD_MAX_EVENTS] =
{
    FD_CONNECT_BIT,
    FD_ACCEPT_BIT,
    FD_OOB_BIT,
    FD_WRITE_BIT,
    FD_READ_BIT,
    FD_CLOSE_BIT,
    6, 7, 8, 9  /* leftovers */
};

/* Flags that make sense only for SOCK_STREAM sockets */
#define STREAM_FLAG_MASK ((unsigned int) (FD_CONNECT | FD_ACCEPT | FD_WINE_LISTENING | FD_WINE_CONNECTED))

typedef enum {
    SOCK_SHUTDOWN_ERROR = -1,
    SOCK_SHUTDOWN_EOF = 0,
    SOCK_SHUTDOWN_POLLHUP = 1
} sock_shutdown_t;

static sock_shutdown_t sock_shutdown_type = SOCK_SHUTDOWN_ERROR;

static sock_shutdown_t sock_check_pollhup(void)
{
    sock_shutdown_t ret = SOCK_SHUTDOWN_ERROR;
    int fd[2], n;
    struct pollfd pfd;
    char dummy;

    if ( socketpair( AF_UNIX, SOCK_STREAM, 0, fd ) ) return ret;
    if ( shutdown( fd[0], 1 ) ) goto out;

    pfd.fd = fd[1];
    pfd.events = POLLIN;
    pfd.revents = 0;

    /* Solaris' poll() sometimes returns nothing if given a 0ms timeout here */
    n = poll( &pfd, 1, 1 );
    if ( n != 1 ) goto out; /* error or timeout */
    if ( pfd.revents & POLLHUP )
        ret = SOCK_SHUTDOWN_POLLHUP;
    else if ( pfd.revents & POLLIN &&
              read( fd[1], &dummy, 1 ) == 0 )
        ret = SOCK_SHUTDOWN_EOF;

out:
    close( fd[0] );
    close( fd[1] );
    return ret;
}

void sock_init(void)
{
    sock_shutdown_type = sock_check_pollhup();

    switch ( sock_shutdown_type )
    {
    case SOCK_SHUTDOWN_EOF:
        if (debug_level) fprintf( stderr, "sock_init: shutdown() causes EOF\n" );
        break;
    case SOCK_SHUTDOWN_POLLHUP:
        if (debug_level) fprintf( stderr, "sock_init: shutdown() causes POLLHUP\n" );
        break;
    default:
        fprintf( stderr, "sock_init: ERROR in sock_check_pollhup()\n" );
        sock_shutdown_type = SOCK_SHUTDOWN_EOF;
    }
}

static int sock_reselect( struct sock *sock )
{
    int ev = sock_get_poll_events( sock->fd );

    if (debug_level)
        fprintf(stderr,"sock_reselect(%p): new mask %x\n", sock, ev);

    set_fd_events( sock->fd, ev );
    return ev;
}

/* wake anybody waiting on the socket event or send the associated message */
static void sock_wake_up( struct sock *sock )
{
    unsigned int events = sock->pending_events & sock->mask;
    int i;

    if (sock->event)
    {
        if (debug_level) fprintf(stderr, "signalling events %x ptr %p\n", events, sock->event );
        if (events)
            set_event( sock->event );
    }
    if (sock->window)
    {
        if (debug_level) fprintf(stderr, "signalling events %x win %08x\n", events, sock->window );
        for (i = 0; i < FD_MAX_EVENTS; i++)
        {
            int event = event_bitorder[i];
            if (events & (1 << event))
            {
                lparam_t lparam = (1 << event) | (sock->errors[event] << 16);
                post_message( sock->window, sock->message, sock->wparam, lparam );
            }
        }
        sock->pending_events = 0;
        sock_reselect( sock );
    }
}

static inline int sock_error( struct fd *fd )
{
    unsigned int optval = 0;
    socklen_t optlen = sizeof(optval);

    getsockopt( get_unix_fd(fd), SOL_SOCKET, SO_ERROR, (void *) &optval, &optlen);
    return optval;
}

static void free_accept_req( void *private )
{
    struct accept_req *req = private;
    list_remove( &req->entry );
    if (req->acceptsock)
    {
        req->acceptsock->accept_recv_req = NULL;
        release_object( req->acceptsock );
    }
    release_object( req->async );
    release_object( req->iosb );
    release_object( req->sock );
    free( req );
}

static void fill_accept_output( struct accept_req *req )
{
    struct iosb *iosb = req->iosb;
    union unix_sockaddr unix_addr;
    struct WS_sockaddr *win_addr;
    unsigned int remote_len;
    socklen_t unix_len;
    int fd, size = 0;
    char *out_data;
    int win_len;

    if (!(out_data = mem_alloc( iosb->out_size ))) return;

    fd = get_unix_fd( req->acceptsock->fd );

    if (req->recv_len && (size = recv( fd, out_data, req->recv_len, 0 )) < 0)
    {
        if (!req->accepted && errno == EWOULDBLOCK)
        {
            req->accepted = 1;
            sock_reselect( req->acceptsock );
            set_error( STATUS_PENDING );
            return;
        }

        set_error( sock_get_ntstatus( errno ) );
        free( out_data );
        return;
    }

    if (req->local_len)
    {
        if (req->local_len < sizeof(int))
        {
            set_error( STATUS_BUFFER_TOO_SMALL );
            free( out_data );
            return;
        }

        unix_len = sizeof(unix_addr);
        win_addr = (struct WS_sockaddr *)(out_data + req->recv_len + sizeof(int));
        if (getsockname( fd, &unix_addr.addr, &unix_len ) < 0 ||
            (win_len = sockaddr_from_unix( &unix_addr, win_addr, req->local_len - sizeof(int) )) < 0)
        {
            set_error( sock_get_ntstatus( errno ) );
            free( out_data );
            return;
        }
        memcpy( out_data + req->recv_len, &win_len, sizeof(int) );
    }

    unix_len = sizeof(unix_addr);
    win_addr = (struct WS_sockaddr *)(out_data + req->recv_len + req->local_len + sizeof(int));
    remote_len = iosb->out_size - req->recv_len - req->local_len;
    if (getpeername( fd, &unix_addr.addr, &unix_len ) < 0 ||
        (win_len = sockaddr_from_unix( &unix_addr, win_addr, remote_len - sizeof(int) )) < 0)
    {
        set_error( sock_get_ntstatus( errno ) );
        free( out_data );
        return;
    }
    memcpy( out_data + req->recv_len + req->local_len, &win_len, sizeof(int) );

    iosb->status = STATUS_SUCCESS;
    iosb->result = size;
    iosb->out_data = out_data;
    set_error( STATUS_ALERTED );
}

static void complete_async_accept( struct sock *sock, struct accept_req *req )
{
    struct sock *acceptsock = req->acceptsock;
    struct async *async = req->async;

    if (debug_level) fprintf( stderr, "completing accept request for socket %p\n", sock );

    if (acceptsock)
    {
        if (!accept_into_socket( sock, acceptsock )) return;
        fill_accept_output( req );
    }
    else
    {
        struct iosb *iosb = req->iosb;
        obj_handle_t handle;

        if (!(acceptsock = accept_socket( sock ))) return;
        handle = alloc_handle_no_access_check( async_get_thread( async )->process, &acceptsock->obj,
                                               GENERIC_READ | GENERIC_WRITE | SYNCHRONIZE, OBJ_INHERIT );
        acceptsock->wparam = handle;
        release_object( acceptsock );
        if (!handle) return;

        if (!(iosb->out_data = malloc( sizeof(handle) ))) return;

        iosb->status = STATUS_SUCCESS;
        iosb->out_size = sizeof(handle);
        memcpy( iosb->out_data, &handle, sizeof(handle) );
        set_error( STATUS_ALERTED );
    }
}

static void complete_async_accept_recv( struct accept_req *req )
{
    if (debug_level) fprintf( stderr, "completing accept recv request for socket %p\n", req->acceptsock );

    assert( req->recv_len );

    fill_accept_output( req );
}

static void free_connect_req( void *private )
{
    struct connect_req *req = private;

    req->sock->connect_req = NULL;
    release_object( req->async );
    release_object( req->iosb );
    release_object( req->sock );
    free( req );
}

static void complete_async_connect( struct sock *sock )
{
    struct connect_req *req = sock->connect_req;
    const char *in_buffer;
    struct iosb *iosb;
    size_t len;
    int ret;

    if (debug_level) fprintf( stderr, "completing connect request for socket %p\n", sock );

    sock->pending_events &= ~(FD_CONNECT | FD_READ | FD_WRITE);
    sock->reported_events &= ~(FD_CONNECT | FD_READ | FD_WRITE);
    sock->state |= FD_WINE_CONNECTED;
    sock->state &= ~(FD_CONNECT | FD_WINE_LISTENING);

    if (!req->send_len)
    {
        set_error( STATUS_SUCCESS );
        return;
    }

    iosb = req->iosb;
    in_buffer = (const char *)iosb->in_data + sizeof(struct afd_connect_params) + req->addr_len;
    len = req->send_len - req->send_cursor;

    ret = send( get_unix_fd( sock->fd ), in_buffer + req->send_cursor, len, 0 );
    if (ret < 0 && errno != EWOULDBLOCK)
        set_error( sock_get_ntstatus( errno ) );
    else if (ret == len)
    {
        iosb->result = req->send_len;
        iosb->status = STATUS_SUCCESS;
        set_error( STATUS_ALERTED );
    }
    else
    {
        req->send_cursor += ret;
        set_error( STATUS_PENDING );
    }
}

static void free_poll_req( void *private )
{
    struct poll_req *req = private;
    unsigned int i;

    if (req->timeout) remove_timeout_user( req->timeout );

    for (i = 0; i < req->count; ++i)
        release_object( req->sockets[i].sock );
    release_object( req->async );
    release_object( req->iosb );
    list_remove( &req->entry );
    free( req );
}

static int is_oobinline( struct sock *sock )
{
    int oobinline;
    socklen_t len = sizeof(oobinline);
    return !getsockopt( get_unix_fd( sock->fd ), SOL_SOCKET, SO_OOBINLINE, (char *)&oobinline, &len ) && oobinline;
}

static int get_poll_flags( struct sock *sock, int event )
{
    int flags = 0;

    /* A connection-mode socket which has never been connected does not return
     * write or hangup events, but Linux reports POLLOUT | POLLHUP. */
    if (sock->type == WS_SOCK_STREAM && !(sock->state & (FD_CONNECT | FD_WINE_CONNECTED | FD_WINE_LISTENING)))
        event &= ~(POLLOUT | POLLHUP);

    if (event & POLLIN)
    {
        if (sock->state & FD_WINE_LISTENING)
            flags |= AFD_POLL_ACCEPT;
        else
            flags |= AFD_POLL_READ;
    }
    if (event & POLLPRI)
        flags |= is_oobinline( sock ) ? AFD_POLL_READ : AFD_POLL_OOB;
    if (event & POLLOUT)
        flags |= AFD_POLL_WRITE;
    if (sock->state & FD_WINE_CONNECTED)
        flags |= AFD_POLL_CONNECT;
    if (event & POLLHUP)
        flags |= AFD_POLL_HUP;
    if (event & POLLERR)
        flags |= AFD_POLL_CONNECT_ERR;

    return flags;
}

static void complete_async_polls( struct sock *sock, int event, int error )
{
    int flags = get_poll_flags( sock, event );
    struct poll_req *req, *next;

    LIST_FOR_EACH_ENTRY_SAFE( req, next, &poll_list, struct poll_req, entry )
    {
        struct iosb *iosb = req->iosb;
        unsigned int i;

        if (iosb->status != STATUS_PENDING) continue;

        for (i = 0; i < req->count; ++i)
        {
            if (req->sockets[i].sock != sock) continue;
            if (!(req->sockets[i].flags & flags)) continue;

            if (debug_level)
                fprintf( stderr, "completing poll for socket %p, wanted %#x got %#x\n",
                         sock, req->sockets[i].flags, flags );

            req->output[i].flags = req->sockets[i].flags & flags;
            req->output[i].status = sock_get_ntstatus( error );

            iosb->status = STATUS_SUCCESS;
            iosb->out_data = req->output;
            iosb->out_size = req->count * sizeof(*req->output);
            async_terminate( req->async, STATUS_ALERTED );
            break;
        }
    }
}

static void async_poll_timeout( void *private )
{
    struct poll_req *req = private;
    struct iosb *iosb = req->iosb;

    req->timeout = NULL;

    if (iosb->status != STATUS_PENDING) return;

    iosb->status = STATUS_TIMEOUT;
    iosb->out_data = req->output;
    iosb->out_size = req->count * sizeof(*req->output);
    async_terminate( req->async, STATUS_ALERTED );
}

static int sock_dispatch_asyncs( struct sock *sock, int event, int error )
{
    if (event & (POLLIN | POLLPRI))
    {
        struct accept_req *req;

        LIST_FOR_EACH_ENTRY( req, &sock->accept_list, struct accept_req, entry )
        {
            if (req->iosb->status == STATUS_PENDING && !req->accepted)
            {
                complete_async_accept( sock, req );
                if (get_error() != STATUS_PENDING)
                    async_terminate( req->async, get_error() );
                break;
            }
        }

        if (sock->accept_recv_req && sock->accept_recv_req->iosb->status == STATUS_PENDING)
        {
            complete_async_accept_recv( sock->accept_recv_req );
            if (get_error() != STATUS_PENDING)
                async_terminate( sock->accept_recv_req->async, get_error() );
        }
    }

    if ((event & POLLOUT) && sock->connect_req && sock->connect_req->iosb->status == STATUS_PENDING)
    {
        complete_async_connect( sock );
        if (get_error() != STATUS_PENDING)
            async_terminate( sock->connect_req->async, get_error() );
    }

    if (event & (POLLIN | POLLPRI) && async_waiting( &sock->read_q ))
    {
        if (debug_level) fprintf( stderr, "activating read queue for socket %p\n", sock );
        async_wake_up( &sock->read_q, STATUS_ALERTED );
        event &= ~(POLLIN | POLLPRI);
    }

    if (event & POLLOUT && async_waiting( &sock->write_q ))
    {
        if (debug_level) fprintf( stderr, "activating write queue for socket %p\n", sock );
        async_wake_up( &sock->write_q, STATUS_ALERTED );
        event &= ~POLLOUT;
    }

    if (event & (POLLERR | POLLHUP))
    {
        int status = sock_get_ntstatus( error );
        struct accept_req *req, *next;

        if (!(sock->state & FD_READ))
            async_wake_up( &sock->read_q, status );
        if (!(sock->state & FD_WRITE))
            async_wake_up( &sock->write_q, status );

        LIST_FOR_EACH_ENTRY_SAFE( req, next, &sock->accept_list, struct accept_req, entry )
        {
            if (req->iosb->status == STATUS_PENDING)
                async_terminate( req->async, status );
        }

        if (sock->accept_recv_req && sock->accept_recv_req->iosb->status == STATUS_PENDING)
            async_terminate( sock->accept_recv_req->async, status );

        if (sock->connect_req)
            async_terminate( sock->connect_req->async, status );
    }

    return event;
}

static void post_socket_event( struct sock *sock, unsigned int event_bit, unsigned int error )
{
    unsigned int event = (1 << event_bit);

    if (!(sock->reported_events & event))
    {
        sock->pending_events |= event;
        sock->reported_events |= event;
        sock->errors[event_bit] = error;
    }
}

static void sock_dispatch_events( struct sock *sock, int prevstate, int event, int error )
{
    if (prevstate & FD_CONNECT)
    {
        post_socket_event( sock, FD_CONNECT_BIT, sock_get_error( error ) );
        goto end;
    }
    if (prevstate & FD_WINE_LISTENING)
    {
        post_socket_event( sock, FD_ACCEPT_BIT, sock_get_error( error ) );
        goto end;
    }

    if (event & POLLIN)
        post_socket_event( sock, FD_READ_BIT, 0 );

    if (event & POLLOUT)
        post_socket_event( sock, FD_WRITE_BIT, 0 );

    if (event & POLLPRI)
        post_socket_event( sock, FD_OOB_BIT, 0 );

    if (event & (POLLERR|POLLHUP))
        post_socket_event( sock, FD_CLOSE_BIT, sock_get_error( error ) );

end:
    sock_wake_up( sock );
}

static void sock_poll_event( struct fd *fd, int event )
{
    struct sock *sock = get_fd_user( fd );
    int hangup_seen = 0;
    int prevstate = sock->state;
    int error = 0;

    assert( sock->obj.ops == &sock_ops );
    if (debug_level)
        fprintf(stderr, "socket %p select event: %x\n", sock, event);

    /* we may change event later, remove from loop here */
    if (event & (POLLERR|POLLHUP)) set_fd_events( sock->fd, -1 );

    if (sock->state & FD_CONNECT)
    {
        if (event & (POLLERR|POLLHUP))
        {
            /* we didn't get connected? */
            sock->state &= ~FD_CONNECT;
            event &= ~POLLOUT;
            error = sock_error( fd );
        }
        else if (event & POLLOUT)
        {
            /* we got connected */
            sock->state |= FD_WINE_CONNECTED|FD_READ|FD_WRITE;
            sock->state &= ~FD_CONNECT;
            sock->connect_time = current_time;
        }
    }
    else if (sock->state & FD_WINE_LISTENING)
    {
        /* listening */
        if (event & (POLLERR|POLLHUP))
            error = sock_error( fd );
    }
    else
    {
        /* normal data flow */
        if (sock->type == WS_SOCK_STREAM && (event & POLLIN))
        {
            char dummy;
            int nr;

            /* Linux 2.4 doesn't report POLLHUP if only one side of the socket
             * has been closed, so we need to check for it explicitly here */
            nr  = recv( get_unix_fd( fd ), &dummy, 1, MSG_PEEK );
            if ( nr == 0 )
            {
                hangup_seen = 1;
                event &= ~POLLIN;
            }
            else if ( nr < 0 )
            {
                event &= ~POLLIN;
                /* EAGAIN can happen if an async recv() falls between the server's poll()
                   call and the invocation of this routine */
                if ( errno != EAGAIN )
                {
                    error = errno;
                    event |= POLLERR;
                    if ( debug_level )
                        fprintf( stderr, "recv error on socket %p: %d\n", sock, errno );
                }
            }
        }

        if ( (hangup_seen || event & (POLLHUP|POLLERR)) && (sock->state & (FD_READ|FD_WRITE)) )
        {
            error = error ? error : sock_error( fd );
            if ( (event & POLLERR) || ( sock_shutdown_type == SOCK_SHUTDOWN_EOF && (event & POLLHUP) ))
                sock->state &= ~FD_WRITE;
            sock->state &= ~FD_READ;

            if (debug_level)
                fprintf(stderr, "socket %p aborted by error %d, event: %x\n", sock, error, event);
        }

        if (hangup_seen)
            event |= POLLHUP;
    }

    complete_async_polls( sock, event, error );

    event = sock_dispatch_asyncs( sock, event, error );
    sock_dispatch_events( sock, prevstate, event, error );

    sock_reselect( sock );
}

static void sock_dump( struct object *obj, int verbose )
{
    struct sock *sock = (struct sock *)obj;
    assert( obj->ops == &sock_ops );
    fprintf( stderr, "Socket fd=%p, state=%x, mask=%x, pending=%x, reported=%x\n",
            sock->fd, sock->state,
            sock->mask, sock->pending_events, sock->reported_events );
}

static int poll_flags_from_afd( struct sock *sock, int flags )
{
    int ev = 0;

    /* A connection-mode socket which has never been connected does
     * not return write or hangup events, but Linux returns
     * POLLOUT | POLLHUP. */
    if (sock->type == WS_SOCK_STREAM && !(sock->state & (FD_CONNECT | FD_WINE_CONNECTED | FD_WINE_LISTENING)))
        return -1;

    if (flags & (AFD_POLL_READ | AFD_POLL_ACCEPT))
        ev |= POLLIN;
    if ((flags & AFD_POLL_HUP) && sock->type == WS_SOCK_STREAM)
        ev |= POLLIN;
    if (flags & AFD_POLL_OOB)
        ev |= is_oobinline( sock ) ? POLLIN : POLLPRI;
    if (flags & AFD_POLL_WRITE)
        ev |= POLLOUT;

    return ev;
}

static int sock_get_poll_events( struct fd *fd )
{
    struct sock *sock = get_fd_user( fd );
    unsigned int mask = sock->mask & ~sock->reported_events;
    unsigned int smask = sock->state & mask;
    struct poll_req *req;
    int ev = 0;

    assert( sock->obj.ops == &sock_ops );

    if (!sock->type) /* not initialized yet */
        return -1;

    /* A connection-mode Windows socket which has never been connected does not
     * return any events, but Linux returns POLLOUT | POLLHUP. Hence we need to
     * return -1 here, to prevent the socket from being polled on at all. */
    if (sock->type == WS_SOCK_STREAM && !(sock->state & (FD_CONNECT | FD_WINE_CONNECTED | FD_WINE_LISTENING)))
        return -1;

    if (sock->state & FD_CONNECT)
        /* connecting, wait for writable */
        return POLLOUT;

    if (!list_empty( &sock->accept_list ) || sock->accept_recv_req )
    {
        ev |= POLLIN | POLLPRI;
    }
    else if (async_queued( &sock->read_q ))
    {
        if (async_waiting( &sock->read_q )) ev |= POLLIN | POLLPRI;
    }
    else if (smask & FD_READ || (sock->state & FD_WINE_LISTENING && mask & FD_ACCEPT))
        ev |= POLLIN | POLLPRI;
    /* We use POLLIN with 0 bytes recv() as FD_CLOSE indication for stream sockets. */
    else if (sock->type == WS_SOCK_STREAM && (mask & FD_CLOSE) && !(sock->reported_events & FD_READ))
        ev |= POLLIN;

    if (async_queued( &sock->write_q ))
    {
        if (async_waiting( &sock->write_q )) ev |= POLLOUT;
    }
    else if (smask & FD_WRITE)
        ev |= POLLOUT;

    LIST_FOR_EACH_ENTRY( req, &poll_list, struct poll_req, entry )
    {
        unsigned int i;

        for (i = 0; i < req->count; ++i)
        {
            if (req->sockets[i].sock != sock) continue;

            ev |= poll_flags_from_afd( sock, req->sockets[i].flags );
        }
    }

    return ev;
}

static enum server_fd_type sock_get_fd_type( struct fd *fd )
{
    return FD_TYPE_SOCKET;
}

static void sock_queue_async( struct fd *fd, struct async *async, int type, int count )
{
    struct sock *sock = get_fd_user( fd );
    struct async_queue *queue;

    assert( sock->obj.ops == &sock_ops );

    switch (type)
    {
    case ASYNC_TYPE_READ:
        queue = &sock->read_q;
        break;
    case ASYNC_TYPE_WRITE:
        queue = &sock->write_q;
        break;
    default:
        set_error( STATUS_INVALID_PARAMETER );
        return;
    }

    if ( ( !( sock->state & (FD_READ|FD_CONNECT|FD_WINE_LISTENING) ) && type == ASYNC_TYPE_READ  ) ||
         ( !( sock->state & (FD_WRITE|FD_CONNECT) ) && type == ASYNC_TYPE_WRITE ) )
    {
        set_error( STATUS_PIPE_DISCONNECTED );
        return;
    }

    queue_async( queue, async );
    sock_reselect( sock );

    set_error( STATUS_PENDING );
}

static void sock_reselect_async( struct fd *fd, struct async_queue *queue )
{
    struct sock *sock = get_fd_user( fd );

    if (sock->wr_shutdown_pending && list_empty( &sock->write_q.queue ))
        shutdown( get_unix_fd( sock->fd ), SHUT_WR );

    /* Don't reselect the ifchange queue; we always ask for POLLIN.
     * Don't reselect an uninitialized socket; we can't call set_fd_events() on
     * a pseudo-fd. */
    if (queue != &sock->ifchange_q && sock->type)
        sock_reselect( sock );
}

static struct fd *sock_get_fd( struct object *obj )
{
    struct sock *sock = (struct sock *)obj;
    return (struct fd *)grab_object( sock->fd );
}

static int sock_close_handle( struct object *obj, struct process *process, obj_handle_t handle )
{
    struct sock *sock = (struct sock *)obj;

    if (sock->obj.handle_count == 1) /* last handle */
    {
        struct accept_req *accept_req, *accept_next;
        struct poll_req *poll_req, *poll_next;

        if (sock->accept_recv_req)
            async_terminate( sock->accept_recv_req->async, STATUS_CANCELLED );

        LIST_FOR_EACH_ENTRY_SAFE( accept_req, accept_next, &sock->accept_list, struct accept_req, entry )
            async_terminate( accept_req->async, STATUS_CANCELLED );

        if (sock->connect_req)
            async_terminate( sock->connect_req->async, STATUS_CANCELLED );

        LIST_FOR_EACH_ENTRY_SAFE( poll_req, poll_next, &poll_list, struct poll_req, entry )
        {
            struct iosb *iosb = poll_req->iosb;
            unsigned int i;

            if (iosb->status != STATUS_PENDING) continue;

            for (i = 0; i < poll_req->count; ++i)
            {
                if (poll_req->sockets[i].sock == sock)
                {
                    iosb->status = STATUS_SUCCESS;
                    poll_req->output[i].flags = AFD_POLL_CLOSE;
                    poll_req->output[i].status = 0;
                }
            }

            if (iosb->status != STATUS_PENDING)
            {
                iosb->out_data = poll_req->output;
                iosb->out_size = poll_req->count * sizeof(*poll_req->output);
                async_terminate( poll_req->async, STATUS_ALERTED );
            }
        }
    }

    return 1;
}

static void sock_destroy( struct object *obj )
{
    struct sock *sock = (struct sock *)obj;

    assert( obj->ops == &sock_ops );

    /* FIXME: special socket shutdown stuff? */

    if ( sock->deferred )
        release_object( sock->deferred );

    async_wake_up( &sock->ifchange_q, STATUS_CANCELLED );
    sock_release_ifchange( sock );
    free_async_queue( &sock->read_q );
    free_async_queue( &sock->write_q );
    free_async_queue( &sock->ifchange_q );
    free_async_queue( &sock->accept_q );
    free_async_queue( &sock->connect_q );
    free_async_queue( &sock->poll_q );
    if (sock->event) release_object( sock->event );
    if (sock->fd)
    {
        /* shut the socket down to force pending poll() calls in the client to return */
        shutdown( get_unix_fd(sock->fd), SHUT_RDWR );
        release_object( sock->fd );
    }
}

static struct sock *create_socket(void)
{
    struct sock *sock;

    if (!(sock = alloc_object( &sock_ops ))) return NULL;
    sock->fd      = NULL;
    sock->state   = 0;
    sock->mask    = 0;
    sock->pending_events = 0;
    sock->reported_events = 0;
    sock->wr_shutdown_pending = 0;
    sock->flags   = 0;
    sock->proto   = 0;
    sock->type    = 0;
    sock->family  = 0;
    sock->event   = NULL;
    sock->window  = 0;
    sock->message = 0;
    sock->wparam  = 0;
    sock->connect_time = 0;
    sock->deferred = NULL;
    sock->ifchange_obj = NULL;
    sock->accept_recv_req = NULL;
    sock->connect_req = NULL;
    init_async_queue( &sock->read_q );
    init_async_queue( &sock->write_q );
    init_async_queue( &sock->ifchange_q );
    init_async_queue( &sock->accept_q );
    init_async_queue( &sock->connect_q );
    init_async_queue( &sock->poll_q );
    memset( sock->errors, 0, sizeof(sock->errors) );
    list_init( &sock->accept_list );
    return sock;
}

static int get_unix_family( int family )
{
    switch (family)
    {
        case WS_AF_INET: return AF_INET;
        case WS_AF_INET6: return AF_INET6;
#ifdef HAS_IPX
        case WS_AF_IPX: return AF_IPX;
#endif
#ifdef AF_IRDA
        case WS_AF_IRDA: return AF_IRDA;
#endif
        case WS_AF_UNSPEC: return AF_UNSPEC;
        default: return -1;
    }
}

static int get_unix_type( int type )
{
    switch (type)
    {
        case WS_SOCK_DGRAM: return SOCK_DGRAM;
        case WS_SOCK_RAW: return SOCK_RAW;
        case WS_SOCK_STREAM: return SOCK_STREAM;
        default: return -1;
    }
}

static int get_unix_protocol( int protocol )
{
    if (protocol >= WS_NSPROTO_IPX && protocol <= WS_NSPROTO_IPX + 255)
        return protocol;

    switch (protocol)
    {
        case WS_IPPROTO_ICMP: return IPPROTO_ICMP;
        case WS_IPPROTO_IGMP: return IPPROTO_IGMP;
        case WS_IPPROTO_IP: return IPPROTO_IP;
        case WS_IPPROTO_IPV4: return IPPROTO_IPIP;
        case WS_IPPROTO_IPV6: return IPPROTO_IPV6;
        case WS_IPPROTO_RAW: return IPPROTO_RAW;
        case WS_IPPROTO_TCP: return IPPROTO_TCP;
        case WS_IPPROTO_UDP: return IPPROTO_UDP;
        default: return -1;
    }
}

static void set_dont_fragment( int fd, int level, int value )
{
    int optname;

    if (level == IPPROTO_IP)
    {
#ifdef IP_DONTFRAG
        optname = IP_DONTFRAG;
#elif defined(IP_MTU_DISCOVER) && defined(IP_PMTUDISC_DO) && defined(IP_PMTUDISC_DONT)
        optname = IP_MTU_DISCOVER;
        value = value ? IP_PMTUDISC_DO : IP_PMTUDISC_DONT;
#else
        return;
#endif
    }
    else
    {
#ifdef IPV6_DONTFRAG
        optname = IPV6_DONTFRAG;
#elif defined(IPV6_MTU_DISCOVER) && defined(IPV6_PMTUDISC_DO) && defined(IPV6_PMTUDISC_DONT)
        optname = IPV6_MTU_DISCOVER;
        value = value ? IPV6_PMTUDISC_DO : IPV6_PMTUDISC_DONT;
#else
        return;
#endif
    }

    setsockopt( fd, level, optname, &value, sizeof(value) );
}

static int init_socket( struct sock *sock, int family, int type, int protocol, unsigned int flags )
{
    unsigned int options = 0;
    int sockfd, unix_type, unix_family, unix_protocol;

    unix_family = get_unix_family( family );
    unix_type = get_unix_type( type );
    unix_protocol = get_unix_protocol( protocol );

    if (unix_protocol < 0)
    {
        if (type && unix_type < 0)
            set_win32_error( WSAESOCKTNOSUPPORT );
        else
            set_win32_error( WSAEPROTONOSUPPORT );
        return -1;
    }
    if (unix_family < 0)
    {
        if (family >= 0 && unix_type < 0)
            set_win32_error( WSAESOCKTNOSUPPORT );
        else
            set_win32_error( WSAEAFNOSUPPORT );
        return -1;
    }

    sockfd = socket( unix_family, unix_type, unix_protocol );
    if (sockfd == -1)
    {
        if (errno == EINVAL) set_win32_error( WSAESOCKTNOSUPPORT );
        else set_win32_error( sock_get_error( errno ));
        return -1;
    }
    fcntl(sockfd, F_SETFL, O_NONBLOCK); /* make socket nonblocking */

    if (family == WS_AF_IPX && protocol >= WS_NSPROTO_IPX && protocol <= WS_NSPROTO_IPX + 255)
    {
#ifdef HAS_IPX
        int ipx_type = protocol - WS_NSPROTO_IPX;

#ifdef SOL_IPX
        setsockopt( sockfd, SOL_IPX, IPX_TYPE, &ipx_type, sizeof(ipx_type) );
#else
        struct ipx val;
        /* Should we retrieve val using a getsockopt call and then
         * set the modified one? */
        val.ipx_pt = ipx_type;
        setsockopt( sockfd, 0, SO_DEFAULT_HEADERS, &val, sizeof(val) );
#endif
#endif
    }

    if (unix_family == AF_INET || unix_family == AF_INET6)
    {
        /* ensure IP_DONTFRAGMENT is disabled for SOCK_DGRAM and SOCK_RAW, enabled for SOCK_STREAM */
        if (unix_type == SOCK_DGRAM || unix_type == SOCK_RAW) /* in Linux the global default can be enabled */
            set_dont_fragment( sockfd, unix_family == AF_INET6 ? IPPROTO_IPV6 : IPPROTO_IP, FALSE );
        else if (unix_type == SOCK_STREAM)
            set_dont_fragment( sockfd, unix_family == AF_INET6 ? IPPROTO_IPV6 : IPPROTO_IP, TRUE );
    }

#ifdef IPV6_V6ONLY
    if (unix_family == AF_INET6)
    {
        static const int enable = 1;
        setsockopt( sockfd, IPPROTO_IPV6, IPV6_V6ONLY, &enable, sizeof(enable) );
    }
#endif

    sock->state  = (type != SOCK_STREAM) ? (FD_READ|FD_WRITE) : 0;
    sock->flags  = flags;
    sock->proto  = protocol;
    sock->type   = type;
    sock->family = family;

    if (sock->fd)
    {
        options = get_fd_options( sock->fd );
        release_object( sock->fd );
    }

    if (!(sock->fd = create_anonymous_fd( &sock_fd_ops, sockfd, &sock->obj, options )))
    {
        return -1;
    }

    /* We can't immediately allow caching for a connection-mode socket, since it
     * might be accepted into (changing the underlying fd object.) */
    if (sock->type != WS_SOCK_STREAM) allow_fd_caching( sock->fd );

    return 0;
}

/* accepts a socket and inits it */
static int accept_new_fd( struct sock *sock )
{

    /* Try to accept(2). We can't be safe that this an already connected socket
     * or that accept() is allowed on it. In those cases we will get -1/errno
     * return.
     */
    struct sockaddr saddr;
    socklen_t slen = sizeof(saddr);
    int acceptfd = accept( get_unix_fd(sock->fd), &saddr, &slen );
    if (acceptfd != -1)
        fcntl( acceptfd, F_SETFL, O_NONBLOCK );
    else
        set_error( sock_get_ntstatus( errno ));
    return acceptfd;
}

/* accept a socket (creates a new fd) */
static struct sock *accept_socket( struct sock *sock )
{
    struct sock *acceptsock;
    int	acceptfd;

    if (get_unix_fd( sock->fd ) == -1) return NULL;

    if ( sock->deferred )
    {
        acceptsock = sock->deferred;
        sock->deferred = NULL;
    }
    else
    {
        if ((acceptfd = accept_new_fd( sock )) == -1) return NULL;
        if (!(acceptsock = create_socket()))
        {
            close( acceptfd );
            return NULL;
        }

        /* newly created socket gets the same properties of the listening socket */
        acceptsock->state  = FD_WINE_CONNECTED|FD_READ|FD_WRITE;
        if (sock->state & FD_WINE_NONBLOCKING)
            acceptsock->state |= FD_WINE_NONBLOCKING;
        acceptsock->mask    = sock->mask;
        acceptsock->proto   = sock->proto;
        acceptsock->type    = sock->type;
        acceptsock->family  = sock->family;
        acceptsock->window  = sock->window;
        acceptsock->message = sock->message;
        acceptsock->connect_time = current_time;
        if (sock->event) acceptsock->event = (struct event *)grab_object( sock->event );
        acceptsock->flags = sock->flags;
        if (!(acceptsock->fd = create_anonymous_fd( &sock_fd_ops, acceptfd, &acceptsock->obj,
                                                    get_fd_options( sock->fd ) )))
        {
            release_object( acceptsock );
            return NULL;
        }
    }
    clear_error();
    sock->pending_events &= ~FD_ACCEPT;
    sock->reported_events &= ~FD_ACCEPT;
    sock_reselect( sock );
    return acceptsock;
}

static int accept_into_socket( struct sock *sock, struct sock *acceptsock )
{
    int acceptfd;
    struct fd *newfd;

    if (get_unix_fd( sock->fd ) == -1) return FALSE;

    if ( sock->deferred )
    {
        newfd = dup_fd_object( sock->deferred->fd, 0, 0,
                               get_fd_options( acceptsock->fd ) );
        if ( !newfd )
            return FALSE;

        set_fd_user( newfd, &sock_fd_ops, &acceptsock->obj );

        release_object( sock->deferred );
        sock->deferred = NULL;
    }
    else
    {
        if ((acceptfd = accept_new_fd( sock )) == -1)
            return FALSE;

        if (!(newfd = create_anonymous_fd( &sock_fd_ops, acceptfd, &acceptsock->obj,
                                            get_fd_options( acceptsock->fd ) )))
            return FALSE;
    }

    acceptsock->state  |= FD_WINE_CONNECTED|FD_READ|FD_WRITE;
    acceptsock->pending_events = 0;
    acceptsock->reported_events = 0;
    acceptsock->proto   = sock->proto;
    acceptsock->type    = sock->type;
    acceptsock->family  = sock->family;
    acceptsock->wparam  = 0;
    acceptsock->deferred = NULL;
    acceptsock->connect_time = current_time;
    fd_copy_completion( acceptsock->fd, newfd );
    release_object( acceptsock->fd );
    acceptsock->fd = newfd;

    clear_error();
    sock->pending_events &= ~FD_ACCEPT;
    sock->reported_events &= ~FD_ACCEPT;
    sock_reselect( sock );

    return TRUE;
}

/* return an errno value mapped to a WSA error */
static unsigned int sock_get_error( int err )
{
    switch (err)
    {
        case EINTR:             return WSAEINTR;
        case EBADF:             return WSAEBADF;
        case EPERM:
        case EACCES:            return WSAEACCES;
        case EFAULT:            return WSAEFAULT;
        case EINVAL:            return WSAEINVAL;
        case EMFILE:            return WSAEMFILE;
        case EINPROGRESS:
        case EWOULDBLOCK:       return WSAEWOULDBLOCK;
        case EALREADY:          return WSAEALREADY;
        case ENOTSOCK:          return WSAENOTSOCK;
        case EDESTADDRREQ:      return WSAEDESTADDRREQ;
        case EMSGSIZE:          return WSAEMSGSIZE;
        case EPROTOTYPE:        return WSAEPROTOTYPE;
        case ENOPROTOOPT:       return WSAENOPROTOOPT;
        case EPROTONOSUPPORT:   return WSAEPROTONOSUPPORT;
        case ESOCKTNOSUPPORT:   return WSAESOCKTNOSUPPORT;
        case EOPNOTSUPP:        return WSAEOPNOTSUPP;
        case EPFNOSUPPORT:      return WSAEPFNOSUPPORT;
        case EAFNOSUPPORT:      return WSAEAFNOSUPPORT;
        case EADDRINUSE:        return WSAEADDRINUSE;
        case EADDRNOTAVAIL:     return WSAEADDRNOTAVAIL;
        case ENETDOWN:          return WSAENETDOWN;
        case ENETUNREACH:       return WSAENETUNREACH;
        case ENETRESET:         return WSAENETRESET;
        case ECONNABORTED:      return WSAECONNABORTED;
        case EPIPE:
        case ECONNRESET:        return WSAECONNRESET;
        case ENOBUFS:           return WSAENOBUFS;
        case EISCONN:           return WSAEISCONN;
        case ENOTCONN:          return WSAENOTCONN;
        case ESHUTDOWN:         return WSAESHUTDOWN;
        case ETOOMANYREFS:      return WSAETOOMANYREFS;
        case ETIMEDOUT:         return WSAETIMEDOUT;
        case ECONNREFUSED:      return WSAECONNREFUSED;
        case ELOOP:             return WSAELOOP;
        case ENAMETOOLONG:      return WSAENAMETOOLONG;
        case EHOSTDOWN:         return WSAEHOSTDOWN;
        case EHOSTUNREACH:      return WSAEHOSTUNREACH;
        case ENOTEMPTY:         return WSAENOTEMPTY;
#ifdef EPROCLIM
        case EPROCLIM:          return WSAEPROCLIM;
#endif
#ifdef EUSERS
        case EUSERS:            return WSAEUSERS;
#endif
#ifdef EDQUOT
        case EDQUOT:            return WSAEDQUOT;
#endif
#ifdef ESTALE
        case ESTALE:            return WSAESTALE;
#endif
#ifdef EREMOTE
        case EREMOTE:           return WSAEREMOTE;
#endif

        case 0:                 return 0;
        default:
            errno = err;
            perror("wineserver: sock_get_error() can't map error");
            return WSAEFAULT;
    }
}

static int sock_get_ntstatus( int err )
{
    switch ( err )
    {
        case EBADF:             return STATUS_INVALID_HANDLE;
        case EBUSY:             return STATUS_DEVICE_BUSY;
        case EPERM:
        case EACCES:            return STATUS_ACCESS_DENIED;
        case EFAULT:            return STATUS_ACCESS_VIOLATION;
        case EINVAL:            return STATUS_INVALID_PARAMETER;
        case ENFILE:
        case EMFILE:            return STATUS_TOO_MANY_OPENED_FILES;
        case EINPROGRESS:
        case EWOULDBLOCK:       return STATUS_DEVICE_NOT_READY;
        case EALREADY:          return STATUS_NETWORK_BUSY;
        case ENOTSOCK:          return STATUS_OBJECT_TYPE_MISMATCH;
        case EDESTADDRREQ:      return STATUS_INVALID_PARAMETER;
        case EMSGSIZE:          return STATUS_BUFFER_OVERFLOW;
        case EPROTONOSUPPORT:
        case ESOCKTNOSUPPORT:
        case EPFNOSUPPORT:
        case EAFNOSUPPORT:
        case EPROTOTYPE:        return STATUS_NOT_SUPPORTED;
        case ENOPROTOOPT:       return STATUS_INVALID_PARAMETER;
        case EOPNOTSUPP:        return STATUS_NOT_SUPPORTED;
        case EADDRINUSE:        return STATUS_SHARING_VIOLATION;
        case EADDRNOTAVAIL:     return STATUS_INVALID_PARAMETER;
        case ECONNREFUSED:      return STATUS_CONNECTION_REFUSED;
        case ESHUTDOWN:         return STATUS_PIPE_DISCONNECTED;
        case ENOTCONN:          return STATUS_INVALID_CONNECTION;
        case ETIMEDOUT:         return STATUS_IO_TIMEOUT;
        case ENETUNREACH:       return STATUS_NETWORK_UNREACHABLE;
        case EHOSTUNREACH:      return STATUS_HOST_UNREACHABLE;
        case ENETDOWN:          return STATUS_NETWORK_BUSY;
        case EPIPE:
        case ECONNRESET:        return STATUS_CONNECTION_RESET;
        case ECONNABORTED:      return STATUS_CONNECTION_ABORTED;
        case EISCONN:           return STATUS_CONNECTION_ACTIVE;

        case 0:                 return STATUS_SUCCESS;
        default:
            errno = err;
            perror("wineserver: sock_get_ntstatus() can't map error");
            return STATUS_UNSUCCESSFUL;
    }
}

static struct accept_req *alloc_accept_req( struct sock *sock, struct sock *acceptsock, struct async *async,
                                            const struct afd_accept_into_params *params )
{
    struct accept_req *req = mem_alloc( sizeof(*req) );

    if (req)
    {
        req->async = (struct async *)grab_object( async );
        req->iosb = async_get_iosb( async );
        req->sock = (struct sock *)grab_object( sock );
        req->acceptsock = acceptsock;
        if (acceptsock) grab_object( acceptsock );
        req->accepted = 0;
        req->recv_len = 0;
        req->local_len = 0;
        if (params)
        {
            req->recv_len = params->recv_len;
            req->local_len = params->local_len;
        }
    }
    return req;
}

static int sock_ioctl( struct fd *fd, ioctl_code_t code, struct async *async )
{
    struct sock *sock = get_fd_user( fd );
    int unix_fd;

    assert( sock->obj.ops == &sock_ops );

    if (code != IOCTL_AFD_WINE_CREATE && (unix_fd = get_unix_fd( fd )) < 0) return 0;

    switch(code)
    {
    case IOCTL_AFD_WINE_CREATE:
    {
        const struct afd_create_params *params = get_req_data();

        if (get_req_data_size() != sizeof(*params))
        {
            set_error( STATUS_INVALID_PARAMETER );
            return 0;
        }
        init_socket( sock, params->family, params->type, params->protocol, params->flags );
        return 0;
    }

    case IOCTL_AFD_WINE_ACCEPT:
    {
        struct sock *acceptsock;
        obj_handle_t handle;

        if (get_reply_max_size() != sizeof(handle))
        {
            set_error( STATUS_BUFFER_TOO_SMALL );
            return 0;
        }

        if (!(acceptsock = accept_socket( sock )))
        {
            struct accept_req *req;

            if (sock->state & FD_WINE_NONBLOCKING) return 0;
            if (get_error() != STATUS_DEVICE_NOT_READY) return 0;

            if (!(req = alloc_accept_req( sock, NULL, async, NULL ))) return 0;
            list_add_tail( &sock->accept_list, &req->entry );

            async_set_completion_callback( async, free_accept_req, req );
            queue_async( &sock->accept_q, async );
            sock_reselect( sock );
            set_error( STATUS_PENDING );
            return 1;
        }
        handle = alloc_handle( current->process, &acceptsock->obj,
                               GENERIC_READ | GENERIC_WRITE | SYNCHRONIZE, OBJ_INHERIT );
        acceptsock->wparam = handle;
        release_object( acceptsock );
        set_reply_data( &handle, sizeof(handle) );
        return 0;
    }

    case IOCTL_AFD_WINE_ACCEPT_INTO:
    {
        static const int access = FILE_READ_ATTRIBUTES | FILE_WRITE_ATTRIBUTES | FILE_READ_DATA;
        const struct afd_accept_into_params *params = get_req_data();
        struct sock *acceptsock;
        unsigned int remote_len;
        struct accept_req *req;

        if (get_req_data_size() != sizeof(*params) ||
            get_reply_max_size() < params->recv_len ||
            get_reply_max_size() - params->recv_len < params->local_len)
        {
            set_error( STATUS_BUFFER_TOO_SMALL );
            return 0;
        }

        remote_len = get_reply_max_size() - params->recv_len - params->local_len;
        if (remote_len < sizeof(int))
        {
            set_error( STATUS_INVALID_PARAMETER );
            return 0;
        }

        if (!(acceptsock = (struct sock *)get_handle_obj( current->process, params->accept_handle, access, &sock_ops )))
            return 0;

        if (acceptsock->accept_recv_req)
        {
            release_object( acceptsock );
            set_error( STATUS_INVALID_PARAMETER );
            return 0;
        }

        if (!(req = alloc_accept_req( sock, acceptsock, async, params )))
        {
            release_object( acceptsock );
            return 0;
        }
        list_add_tail( &sock->accept_list, &req->entry );
        acceptsock->accept_recv_req = req;
        release_object( acceptsock );

        acceptsock->wparam = params->accept_handle;
        async_set_completion_callback( async, free_accept_req, req );
        queue_async( &sock->accept_q, async );
        sock_reselect( sock );
        set_error( STATUS_PENDING );
        return 1;
    }

    case IOCTL_AFD_LISTEN:
    {
        const struct afd_listen_params *params = get_req_data();

        if (get_req_data_size() < sizeof(*params))
        {
            set_error( STATUS_INVALID_PARAMETER );
            return 0;
        }

        if (listen( unix_fd, params->backlog ) < 0)
        {
            set_error( sock_get_ntstatus( errno ) );
            return 0;
        }

        sock->pending_events &= ~FD_ACCEPT;
        sock->reported_events &= ~FD_ACCEPT;
        sock->state |= FD_WINE_LISTENING;
        sock->state &= ~(FD_CONNECT | FD_WINE_CONNECTED);

        /* a listening socket can no longer be accepted into */
        allow_fd_caching( sock->fd );

        /* we may already be selecting for FD_ACCEPT */
        sock_reselect( sock );
        return 0;
    }

    case IOCTL_AFD_WINE_CONNECT:
    {
        const struct afd_connect_params *params = get_req_data();
        const struct sockaddr *addr;
        struct connect_req *req;
        int send_len, ret;

        if (get_req_data_size() < sizeof(*params) ||
            get_req_data_size() - sizeof(*params) < params->addr_len)
        {
            set_error( STATUS_BUFFER_TOO_SMALL );
            return 0;
        }
        send_len = get_req_data_size() - sizeof(*params) - params->addr_len;
        addr = (const struct sockaddr *)(params + 1);

        if (sock->accept_recv_req)
        {
            set_error( STATUS_INVALID_PARAMETER );
            return 0;
        }

        if (sock->connect_req)
        {
            set_error( params->synchronous ? STATUS_INVALID_PARAMETER : STATUS_CONNECTION_ACTIVE );
            return 0;
        }

        ret = connect( unix_fd, addr, params->addr_len );
        if (ret < 0 && errno != EINPROGRESS)
        {
            set_error( sock_get_ntstatus( errno ) );
            return 0;
        }

        /* a connected or connecting socket can no longer be accepted into */
        allow_fd_caching( sock->fd );

        sock->pending_events &= ~(FD_CONNECT | FD_READ | FD_WRITE);
        sock->reported_events &= ~(FD_CONNECT | FD_READ | FD_WRITE);

        if (!ret)
        {
            sock->state |= FD_WINE_CONNECTED | FD_READ | FD_WRITE;
            sock->state &= ~FD_CONNECT;

            if (!send_len) return 1;
        }

        if (!(req = mem_alloc( sizeof(*req) )))
            return 0;

        sock->state |= FD_CONNECT;

        if (params->synchronous && (sock->state & FD_WINE_NONBLOCKING))
        {
            sock_reselect( sock );
            set_error( STATUS_DEVICE_NOT_READY );
            return 0;
        }

        req->async = (struct async *)grab_object( async );
        req->iosb = async_get_iosb( async );
        req->sock = (struct sock *)grab_object( sock );
        req->addr_len = params->addr_len;
        req->send_len = send_len;
        req->send_cursor = 0;

        async_set_completion_callback( async, free_connect_req, req );
        sock->connect_req = req;
        queue_async( &sock->connect_q, async );
        sock_reselect( sock );
        set_error( STATUS_PENDING );
        return 1;
    }

    case IOCTL_AFD_WINE_SHUTDOWN:
    {
        unsigned int how;

        if (get_req_data_size() < sizeof(int))
        {
            set_error( STATUS_BUFFER_TOO_SMALL );
            return 0;
        }
        how = *(int *)get_req_data();

        if (how > SD_BOTH)
        {
            set_error( STATUS_INVALID_PARAMETER );
            return 0;
        }

        if (sock->type == WS_SOCK_STREAM && !(sock->state & FD_WINE_CONNECTED))
        {
            set_error( STATUS_INVALID_CONNECTION );
            return 0;
        }

        if (how != SD_SEND)
        {
            sock->state &= ~FD_READ;
        }
        if (how != SD_RECEIVE)
        {
            sock->state &= ~FD_WRITE;
            if (list_empty( &sock->write_q.queue ))
                shutdown( unix_fd, SHUT_WR );
            else
                sock->wr_shutdown_pending = 1;
        }

        if (how == SD_BOTH)
        {
            if (sock->event) release_object( sock->event );
            sock->event = NULL;
            sock->window = 0;
            sock->mask = 0;
            sock->state |= FD_WINE_NONBLOCKING;
        }

        sock_reselect( sock );
        return 1;
    }

    case IOCTL_AFD_WINE_ADDRESS_LIST_CHANGE:
        if ((sock->state & FD_WINE_NONBLOCKING) && async_is_blocking( async ))
        {
            set_error( STATUS_DEVICE_NOT_READY );
            return 0;
        }
        if (!sock_get_ifchange( sock )) return 0;
        queue_async( &sock->ifchange_q, async );
        set_error( STATUS_PENDING );
        return 1;

    case IOCTL_AFD_WINE_FIONBIO:
        if (get_req_data_size() < sizeof(int))
        {
            set_error( STATUS_BUFFER_TOO_SMALL );
            return 0;
        }
        if (*(int *)get_req_data())
        {
            sock->state |= FD_WINE_NONBLOCKING;
        }
        else
        {
            if (sock->mask)
            {
                set_error( STATUS_INVALID_PARAMETER );
                return 0;
            }
            sock->state &= ~FD_WINE_NONBLOCKING;
        }
        return 1;

    default:
        set_error( STATUS_NOT_SUPPORTED );
        return 0;
    }
}

static int poll_socket( struct sock *poll_sock, struct async *async, timeout_t timeout,
                        unsigned int count, const struct poll_socket_input *input )
{
    struct poll_socket_output *output;
    struct poll_req *req;
    unsigned int i, j;

    if (!(output = mem_alloc( count * sizeof(*output) )))
        return 0;
    memset( output, 0, count * sizeof(*output) );

    if (!(req = mem_alloc( offsetof( struct poll_req, sockets[count] ) )))
    {
        free( output );
        return 0;
    }

    req->timeout = NULL;
    if (timeout && timeout != TIMEOUT_INFINITE &&
        !(req->timeout = add_timeout_user( timeout, async_poll_timeout, req )))
    {
        free( req );
        free( output );
        return 0;
    }

    for (i = 0; i < count; ++i)
    {
        req->sockets[i].sock = (struct sock *)get_handle_obj( current->process, input[i].socket, 0, &sock_ops );
        if (!req->sockets[i].sock)
        {
            for (j = 0; j < i; ++j) release_object( req->sockets[i].sock );
            if (req->timeout) remove_timeout_user( req->timeout );
            free( req );
            free( output );
            return 0;
        }
        req->sockets[i].flags = input[i].flags;
    }

    req->count = count;
    req->async = (struct async *)grab_object( async );
    req->iosb = async_get_iosb( async );
    req->output = output;

    list_add_tail( &poll_list, &req->entry );
    async_set_completion_callback( async, free_poll_req, req );
    queue_async( &poll_sock->poll_q, async );

    if (!timeout) req->iosb->status = STATUS_SUCCESS;

    for (i = 0; i < count; ++i)
    {
        struct sock *sock = req->sockets[i].sock;
        struct pollfd pollfd;
        int flags;

        pollfd.fd = get_unix_fd( sock->fd );
        pollfd.events = poll_flags_from_afd( sock, req->sockets[i].flags );
        if (pollfd.events < 0 || poll( &pollfd, 1, 0 ) < 0) continue;

        if ((req->sockets[i].flags & AFD_POLL_HUP) && (pollfd.revents & POLLIN) &&
            sock->type == WS_SOCK_STREAM)
        {
            char dummy;

            if (!recv( get_unix_fd( sock->fd ), &dummy, 1, MSG_PEEK ))
            {
                pollfd.revents &= ~POLLIN;
                pollfd.revents |= POLLHUP;
            }
        }

        flags = get_poll_flags( sock, pollfd.revents ) & req->sockets[i].flags;
        if (flags)
        {
            req->iosb->status = STATUS_SUCCESS;
            output[i].flags = flags;
            output[i].status = sock_get_ntstatus( sock_error( sock->fd ) );
        }
    }

    if (req->iosb->status != STATUS_PENDING)
    {
        req->iosb->out_data = output;
        req->iosb->out_size = count * sizeof(*output);
        async_terminate( req->async, STATUS_ALERTED );
    }

    for (i = 0; i < req->count; ++i)
        sock_reselect( req->sockets[i].sock );
    set_error( STATUS_PENDING );
    return 1;
}

#ifdef HAVE_LINUX_RTNETLINK_H

/* only keep one ifchange object around, all sockets waiting for wakeups will look to it */
static struct object *ifchange_object;

static void ifchange_dump( struct object *obj, int verbose );
static struct fd *ifchange_get_fd( struct object *obj );
static void ifchange_destroy( struct object *obj );

static int ifchange_get_poll_events( struct fd *fd );
static void ifchange_poll_event( struct fd *fd, int event );

struct ifchange
{
    struct object       obj;     /* object header */
    struct fd          *fd;      /* interface change file descriptor */
    struct list         sockets; /* list of sockets to send interface change notifications */
};

static const struct object_ops ifchange_ops =
{
    sizeof(struct ifchange), /* size */
    &no_type,                /* type */
    ifchange_dump,           /* dump */
    no_add_queue,            /* add_queue */
    NULL,                    /* remove_queue */
    NULL,                    /* signaled */
    NULL,                    /* get_esync_fd */
    NULL,                    /* get_fsync_idx */
    no_satisfied,            /* satisfied */
    no_signal,               /* signal */
    ifchange_get_fd,         /* get_fd */
    default_map_access,      /* map_access */
    default_get_sd,          /* get_sd */
    default_set_sd,          /* set_sd */
    no_get_full_name,        /* get_full_name */
    no_lookup_name,          /* lookup_name */
    no_link_name,            /* link_name */
    NULL,                    /* unlink_name */
    no_open_file,            /* open_file */
    no_kernel_obj_list,      /* get_kernel_obj_list */
    no_close_handle,         /* close_handle */
    ifchange_destroy         /* destroy */
};

static const struct fd_ops ifchange_fd_ops =
{
    ifchange_get_poll_events, /* get_poll_events */
    ifchange_poll_event,      /* poll_event */
    NULL,                     /* get_fd_type */
    no_fd_read,               /* read */
    no_fd_write,              /* write */
    no_fd_flush,              /* flush */
    no_fd_get_file_info,      /* get_file_info */
    no_fd_get_volume_info,    /* get_volume_info */
    no_fd_ioctl,              /* ioctl */
    NULL,                     /* queue_async */
    NULL                      /* reselect_async */
};

static void ifchange_dump( struct object *obj, int verbose )
{
    assert( obj->ops == &ifchange_ops );
    fprintf( stderr, "Interface change\n" );
}

static struct fd *ifchange_get_fd( struct object *obj )
{
    struct ifchange *ifchange = (struct ifchange *)obj;
    return (struct fd *)grab_object( ifchange->fd );
}

static void ifchange_destroy( struct object *obj )
{
    struct ifchange *ifchange = (struct ifchange *)obj;
    assert( obj->ops == &ifchange_ops );

    release_object( ifchange->fd );

    /* reset the global ifchange object so that it will be recreated if it is needed again */
    assert( obj == ifchange_object );
    ifchange_object = NULL;
}

static int ifchange_get_poll_events( struct fd *fd )
{
    return POLLIN;
}

/* wake up all the sockets waiting for a change notification event */
static void ifchange_wake_up( struct object *obj, unsigned int status )
{
    struct ifchange *ifchange = (struct ifchange *)obj;
    struct list *ptr, *next;
    assert( obj->ops == &ifchange_ops );
    assert( obj == ifchange_object );

    LIST_FOR_EACH_SAFE( ptr, next, &ifchange->sockets )
    {
        struct sock *sock = LIST_ENTRY( ptr, struct sock, ifchange_entry );

        assert( sock->ifchange_obj );
        async_wake_up( &sock->ifchange_q, status ); /* issue ifchange notification for the socket */
        sock_release_ifchange( sock ); /* remove socket from list and decrement ifchange refcount */
    }
}

static void ifchange_poll_event( struct fd *fd, int event )
{
    struct object *ifchange = get_fd_user( fd );
    unsigned int status = STATUS_PENDING;
    char buffer[PIPE_BUF];
    int r;

    r = recv( get_unix_fd(fd), buffer, sizeof(buffer), MSG_DONTWAIT );
    if (r < 0)
    {
        if (errno == EWOULDBLOCK || (EWOULDBLOCK != EAGAIN && errno == EAGAIN))
            return;  /* retry when poll() says the socket is ready */
        status = sock_get_ntstatus( errno );
    }
    else if (r > 0)
    {
        struct nlmsghdr *nlh;

        for (nlh = (struct nlmsghdr *)buffer; NLMSG_OK(nlh, r); nlh = NLMSG_NEXT(nlh, r))
        {
            if (nlh->nlmsg_type == NLMSG_DONE)
                break;
            if (nlh->nlmsg_type == RTM_NEWADDR || nlh->nlmsg_type == RTM_DELADDR)
                status = STATUS_SUCCESS;
        }
    }
    else status = STATUS_CANCELLED;

    if (status != STATUS_PENDING) ifchange_wake_up( ifchange, status );
}

#endif

/* we only need one of these interface notification objects, all of the sockets dependent upon
 * it will wake up when a notification event occurs */
 static struct object *get_ifchange( void )
 {
#ifdef HAVE_LINUX_RTNETLINK_H
    struct ifchange *ifchange;
    struct sockaddr_nl addr;
    int unix_fd;

    if (ifchange_object)
    {
        /* increment the refcount for each socket that uses the ifchange object */
        return grab_object( ifchange_object );
    }

    /* create the socket we need for processing interface change notifications */
    unix_fd = socket( PF_NETLINK, SOCK_RAW, NETLINK_ROUTE );
    if (unix_fd == -1)
    {
        set_error( sock_get_ntstatus( errno ));
        return NULL;
    }
    fcntl( unix_fd, F_SETFL, O_NONBLOCK ); /* make socket nonblocking */
    memset( &addr, 0, sizeof(addr) );
    addr.nl_family = AF_NETLINK;
    addr.nl_groups = RTMGRP_IPV4_IFADDR;
    /* bind the socket to the special netlink kernel interface */
    if (bind( unix_fd, (struct sockaddr *)&addr, sizeof(addr) ) == -1)
    {
        close( unix_fd );
        set_error( sock_get_ntstatus( errno ));
        return NULL;
    }
    if (!(ifchange = alloc_object( &ifchange_ops )))
    {
        close( unix_fd );
        set_error( STATUS_NO_MEMORY );
        return NULL;
    }
    list_init( &ifchange->sockets );
    if (!(ifchange->fd = create_anonymous_fd( &ifchange_fd_ops, unix_fd, &ifchange->obj, 0 )))
    {
        release_object( ifchange );
        set_error( STATUS_NO_MEMORY );
        return NULL;
    }
    set_fd_events( ifchange->fd, POLLIN ); /* enable read wakeup on the file descriptor */

    /* the ifchange object is now successfully configured */
    ifchange_object = &ifchange->obj;
    return &ifchange->obj;
#else
    set_error( STATUS_NOT_SUPPORTED );
    return NULL;
#endif
}

/* add the socket to the interface change notification list */
static void ifchange_add_sock( struct object *obj, struct sock *sock )
{
#ifdef HAVE_LINUX_RTNETLINK_H
    struct ifchange *ifchange = (struct ifchange *)obj;

    list_add_tail( &ifchange->sockets, &sock->ifchange_entry );
#endif
}

/* create a new ifchange queue for a specific socket or, if one already exists, reuse the existing one */
static struct object *sock_get_ifchange( struct sock *sock )
{
    struct object *ifchange;

    if (sock->ifchange_obj) /* reuse existing ifchange_obj for this socket */
        return sock->ifchange_obj;

    if (!(ifchange = get_ifchange()))
        return NULL;

    /* add the socket to the ifchange notification list */
    ifchange_add_sock( ifchange, sock );
    sock->ifchange_obj = ifchange;
    return ifchange;
}

/* destroy an existing ifchange queue for a specific socket */
static void sock_release_ifchange( struct sock *sock )
{
    if (sock->ifchange_obj)
    {
        list_remove( &sock->ifchange_entry );
        release_object( sock->ifchange_obj );
        sock->ifchange_obj = NULL;
    }
}

static void socket_device_dump( struct object *obj, int verbose );
static struct object *socket_device_lookup_name( struct object *obj, struct unicode_str *name,
                                                 unsigned int attr, struct object *root );
static struct object *socket_device_open_file( struct object *obj, unsigned int access,
                                               unsigned int sharing, unsigned int options );

static const struct object_ops socket_device_ops =
{
    sizeof(struct object),      /* size */
    &device_type,               /* type */
    socket_device_dump,         /* dump */
    no_add_queue,               /* add_queue */
    NULL,                       /* remove_queue */
    NULL,                       /* signaled */
    NULL,                       /* get_esync_fd */
    NULL,                       /* get_fsync_idx */
    no_satisfied,               /* satisfied */
    no_signal,                  /* signal */
    no_get_fd,                  /* get_fd */
    default_map_access,         /* map_access */
    default_get_sd,             /* get_sd */
    default_set_sd,             /* set_sd */
    default_get_full_name,      /* get_full_name */
    socket_device_lookup_name,  /* lookup_name */
    directory_link_name,        /* link_name */
    default_unlink_name,        /* unlink_name */
    socket_device_open_file,    /* open_file */
    no_kernel_obj_list,         /* get_kernel_obj_list */
    no_close_handle,            /* close_handle */
    no_destroy                  /* destroy */
};

static void socket_device_dump( struct object *obj, int verbose )
{
    fputs( "Socket device\n", stderr );
}

static struct object *socket_device_lookup_name( struct object *obj, struct unicode_str *name,
                                                 unsigned int attr, struct object *root )
{
    if (name) name->len = 0;
    return NULL;
}

static struct object *socket_device_open_file( struct object *obj, unsigned int access,
                                               unsigned int sharing, unsigned int options )
{
    struct sock *sock;

    if (!(sock = create_socket())) return NULL;
    if (!(sock->fd = alloc_pseudo_fd( &sock_fd_ops, &sock->obj, options )))
    {
        release_object( sock );
        return NULL;
    }
    return &sock->obj;
}

struct object *create_socket_device( struct object *root, const struct unicode_str *name,
                                     unsigned int attr, const struct security_descriptor *sd )
{
    return create_named_object( root, &socket_device_ops, name, attr, sd );
}

/* set socket event parameters */
DECL_HANDLER(set_socket_event)
{
    struct sock *sock;
    struct event *old_event;

    if (!(sock = (struct sock *)get_handle_obj( current->process, req->handle,
                                                FILE_WRITE_ATTRIBUTES, &sock_ops))) return;
    if (get_unix_fd( sock->fd ) == -1) return;
    old_event = sock->event;
    sock->mask    = req->mask;
    if (req->window)
    {
        sock->pending_events &= ~req->mask;
        sock->reported_events &= ~req->mask;
    }
    sock->event   = NULL;
    sock->window  = req->window;
    sock->message = req->msg;
    sock->wparam  = req->handle;  /* wparam is the socket handle */
    if (req->event) sock->event = get_event_obj( current->process, req->event, EVENT_MODIFY_STATE );

    if (debug_level && sock->event) fprintf(stderr, "event ptr: %p\n", sock->event);

    sock_reselect( sock );

    sock->state |= FD_WINE_NONBLOCKING;

    /* if a network event is pending, signal the event object
       it is possible that FD_CONNECT or FD_ACCEPT network events has happened
       before a WSAEventSelect() was done on it.
       (when dealing with Asynchronous socket)  */
    sock_wake_up( sock );

    if (old_event) release_object( old_event ); /* we're through with it */
    release_object( &sock->obj );
}

/* get socket event parameters */
DECL_HANDLER(get_socket_event)
{
    struct sock *sock;

    if (!(sock = (struct sock *)get_handle_obj( current->process, req->handle,
                                                FILE_READ_ATTRIBUTES, &sock_ops ))) return;
    if (get_unix_fd( sock->fd ) == -1) return;
    reply->mask  = sock->mask;
    reply->pmask = sock->pending_events;
    reply->state = sock->state;
    set_reply_data( sock->errors, min( get_reply_max_size(), sizeof(sock->errors) ));

    if (req->service)
    {
        if (req->c_event)
        {
            struct event *cevent = get_event_obj( current->process, req->c_event,
                                                  EVENT_MODIFY_STATE );
            if (cevent)
            {
                reset_event( cevent );
                release_object( cevent );
            }
        }
        sock->pending_events = 0;
        sock_reselect( sock );
    }
    release_object( &sock->obj );
}

DECL_HANDLER(set_socket_deferred)
{
    struct sock *sock, *acceptsock;

    sock=(struct sock *)get_handle_obj( current->process, req->handle, FILE_WRITE_ATTRIBUTES, &sock_ops );
    if ( !sock )
        return;

    acceptsock = (struct sock *)get_handle_obj( current->process, req->deferred, 0, &sock_ops );
    if ( !acceptsock )
    {
        release_object( sock );
        return;
    }
    sock->deferred = acceptsock;
    release_object( sock );
}

DECL_HANDLER(get_socket_info)
{
    struct sock *sock;

    sock = (struct sock *)get_handle_obj( current->process, req->handle, FILE_READ_ATTRIBUTES, &sock_ops );
    if (!sock) return;

    if (get_unix_fd( sock->fd ) == -1) return;

    reply->family   = sock->family;
    reply->type     = sock->type;
    reply->protocol = sock->proto;
    reply->connect_time = -(current_time - sock->connect_time);

    release_object( &sock->obj );
}

DECL_HANDLER(recv_socket)
{
    struct sock *sock = (struct sock *)get_handle_obj( current->process, req->async.handle, 0, &sock_ops );
    unsigned int status = req->status;
    timeout_t timeout = 0;
    struct async *async;
    struct fd *fd;

    if (!sock) return;
    fd = sock->fd;

    /* recv() returned EWOULDBLOCK, i.e. no data available yet */
    if (status == STATUS_DEVICE_NOT_READY && !(sock->state & FD_WINE_NONBLOCKING))
    {
#ifdef SO_RCVTIMEO
        struct timeval tv;
        socklen_t len = sizeof(tv);

        /* Set a timeout on the async if necessary.
         *
         * We want to do this *only* if the client gave us STATUS_DEVICE_NOT_READY.
         * If the client gave us STATUS_PENDING, it expects the async to always
         * block (it was triggered by WSARecv*() with a valid OVERLAPPED
         * structure) and for the timeout not to be respected. */
        if (is_fd_overlapped( fd ) && !getsockopt( get_unix_fd( fd ), SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, &len ))
            timeout = tv.tv_sec * -10000000 + tv.tv_usec * -10;
#endif

        status = STATUS_PENDING;
    }

    /* are we shut down? */
    if (status == STATUS_PENDING && !(sock->state & FD_READ)) status = STATUS_PIPE_DISCONNECTED;

    sock->pending_events &= ~(req->oob ? FD_OOB : FD_READ);
    sock->reported_events &= ~(req->oob ? FD_OOB : FD_READ);

    if ((async = create_request_async( fd, get_fd_comp_flags( fd ), &req->async )))
    {
        int success = 0;

        if (status == STATUS_SUCCESS)
        {
            struct iosb *iosb = async_get_iosb( async );
            iosb->result = req->total;
            release_object( iosb );
            success = 1;
        }
        else if (status == STATUS_PENDING)
        {
            success = 1;
        }
        set_error( status );

        if (timeout)
            async_set_timeout( async, timeout, STATUS_IO_TIMEOUT );

        if (status == STATUS_PENDING)
            queue_async( &sock->read_q, async );

        /* always reselect; we changed reported_events above */
        sock_reselect( sock );

        reply->wait = async_handoff( async, success, NULL, 0 );
        reply->options = get_fd_options( fd );
        release_object( async );
    }
    release_object( sock );
}

DECL_HANDLER(poll_socket)
{
    struct sock *sock = (struct sock *)get_handle_obj( current->process, req->async.handle, 0, &sock_ops );
    const struct poll_socket_input *input = get_req_data();
    struct async *async;
    unsigned int count;

    if (!sock) return;

    count = get_req_data_size() / sizeof(*input);

    if ((async = create_request_async( sock->fd, get_fd_comp_flags( sock->fd ), &req->async )))
    {
        reply->wait = async_handoff( async, poll_socket( sock, async, req->timeout, count, input ), NULL, 0 );
        reply->options = get_fd_options( sock->fd );
        release_object( async );
    }

    release_object( sock );
}

DECL_HANDLER(send_socket)
{
    struct sock *sock = (struct sock *)get_handle_obj( current->process, req->async.handle, 0, &sock_ops );
    unsigned int status = req->status;
    timeout_t timeout = 0;
    struct async *async;
    struct fd *fd;

    if (!sock) return;
    fd = sock->fd;

    if (status != STATUS_SUCCESS)
    {
        /* send() calls only clear and reselect events if unsuccessful. */
        sock->pending_events &= ~FD_WRITE;
        sock->reported_events &= ~FD_WRITE;
    }

    /* If we had a short write and the socket is nonblocking (and the client is
     * not trying to force the operation to be asynchronous), return success.
     * Windows actually refuses to send any data in this case, and returns
     * EWOULDBLOCK, but we have no way of doing that. */
    if (status == STATUS_DEVICE_NOT_READY && req->total && (sock->state & FD_WINE_NONBLOCKING))
        status = STATUS_SUCCESS;

    /* send() returned EWOULDBLOCK or a short write, i.e. cannot send all data yet */
    if (status == STATUS_DEVICE_NOT_READY && !(sock->state & FD_WINE_NONBLOCKING))
    {
#ifdef SO_SNDTIMEO
        struct timeval tv;
        socklen_t len = sizeof(tv);

        /* Set a timeout on the async if necessary.
         *
         * We want to do this *only* if the client gave us STATUS_DEVICE_NOT_READY.
         * If the client gave us STATUS_PENDING, it expects the async to always
         * block (it was triggered by WSASend*() with a valid OVERLAPPED
         * structure) and for the timeout not to be respected. */
        if (is_fd_overlapped( fd ) && !getsockopt( get_unix_fd( fd ), SOL_SOCKET, SO_SNDTIMEO, (char *)&tv, &len ))
            timeout = tv.tv_sec * -10000000 + tv.tv_usec * -10;
#endif

        status = STATUS_PENDING;
    }

    /* are we shut down? */
    if (status == STATUS_PENDING && !(sock->state & FD_WRITE)) status = STATUS_PIPE_DISCONNECTED;

    if ((async = create_request_async( fd, get_fd_comp_flags( fd ), &req->async )))
    {
        int success = 0;

        if (status == STATUS_SUCCESS)
        {
            struct iosb *iosb = async_get_iosb( async );
            iosb->result = req->total;
            release_object( iosb );
            success = 1;
        }
        else if (status == STATUS_PENDING)
        {
            success = 1;
        }
        set_error( status );

        if (timeout)
            async_set_timeout( async, timeout, STATUS_IO_TIMEOUT );

        if (status == STATUS_PENDING)
            queue_async( &sock->write_q, async );

        /* always reselect; we changed reported_events above */
        sock_reselect( sock );

        reply->wait = async_handoff( async, success, NULL, 0 );
        reply->options = get_fd_options( fd );
        release_object( async );
    }
    release_object( sock );
}
