/*
 * Server-side event management
 *
 * Copyright (C) 1998 Alexandre Julliard
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
#include "wine/port.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winternl.h"

#include "handle.h"
#include "thread.h"
#include "request.h"
#include "security.h"
#include "esync.h"
#include "fsync.h"

struct event
{
    struct object  obj;             /* object header */
    struct list    kernel_object;   /* list of kernel object pointers */
    int            manual_reset;    /* is it a manual reset event? */
    int            signaled;        /* event has been signaled */
    int            esync_fd;        /* esync file descriptor */
    unsigned int   fsync_idx;
};

static void event_dump( struct object *obj, int verbose );
static struct object_type *event_get_type( struct object *obj );
static int event_signaled( struct object *obj, struct wait_queue_entry *entry );
static void event_satisfied( struct object *obj, struct wait_queue_entry *entry );
static int event_get_esync_fd( struct object *obj, enum esync_type *type );
static unsigned int event_get_fsync_idx( struct object *obj, enum fsync_type *type );
static unsigned int event_map_access( struct object *obj, unsigned int access );
static int event_signal( struct object *obj, unsigned int access);
static struct list *event_get_kernel_obj_list( struct object *obj );
static void event_destroy( struct object *obj );

static const struct object_ops event_ops =
{
    sizeof(struct event),      /* size */
    event_dump,                /* dump */
    event_get_type,            /* get_type */
    add_queue,                 /* add_queue */
    remove_queue,              /* remove_queue */
    event_signaled,            /* signaled */
    event_get_esync_fd,        /* get_esync_fd */
    event_get_fsync_idx,       /* get_fsync_idx */
    event_satisfied,           /* satisfied */
    event_signal,              /* signal */
    no_get_fd,                 /* get_fd */
    event_map_access,          /* map_access */
    default_get_sd,            /* get_sd */
    default_set_sd,            /* set_sd */
    default_get_full_name,     /* get_full_name */
    no_lookup_name,            /* lookup_name */
    directory_link_name,       /* link_name */
    default_unlink_name,       /* unlink_name */
    no_open_file,              /* open_file */
    event_get_kernel_obj_list, /* get_kernel_obj_list */
    no_close_handle,           /* close_handle */
    event_destroy              /* destroy */
};


struct keyed_event
{
    struct object  obj;             /* object header */
};

static void keyed_event_dump( struct object *obj, int verbose );
static struct object_type *keyed_event_get_type( struct object *obj );
static int keyed_event_signaled( struct object *obj, struct wait_queue_entry *entry );
static unsigned int keyed_event_map_access( struct object *obj, unsigned int access );

static const struct object_ops keyed_event_ops =
{
    sizeof(struct keyed_event),  /* size */
    keyed_event_dump,            /* dump */
    keyed_event_get_type,        /* get_type */
    add_queue,                   /* add_queue */
    remove_queue,                /* remove_queue */
    keyed_event_signaled,        /* signaled */
    NULL,                        /* get_esync_fd */
    NULL,                        /* get_fsync_idx */
    no_satisfied,                /* satisfied */
    no_signal,                   /* signal */
    no_get_fd,                   /* get_fd */
    keyed_event_map_access,      /* map_access */
    default_get_sd,              /* get_sd */
    default_set_sd,              /* set_sd */
    default_get_full_name,       /* get_full_name */
    no_lookup_name,              /* lookup_name */
    directory_link_name,         /* link_name */
    default_unlink_name,         /* unlink_name */
    no_open_file,                /* open_file */
    no_kernel_obj_list,          /* get_kernel_obj_list */
    no_close_handle,             /* close_handle */
    no_destroy                   /* destroy */
};


struct event *create_event( struct object *root, const struct unicode_str *name,
                            unsigned int attr, int manual_reset, int initial_state,
                            const struct security_descriptor *sd )
{
    struct event *event;

    if ((event = create_named_object( root, &event_ops, name, attr, sd )))
    {
        if (get_error() != STATUS_OBJECT_NAME_EXISTS)
        {
            /* initialize it if it didn't already exist */
            list_init( &event->kernel_object );
            event->manual_reset = manual_reset;
            event->signaled     = initial_state;

            if (do_fsync())
                event->fsync_idx = fsync_alloc_shm( initial_state, 0 );

            if (do_esync())
                event->esync_fd = esync_create_fd( initial_state, 0 );
        }
    }
    return event;
}

struct event *get_event_obj( struct process *process, obj_handle_t handle, unsigned int access )
{
    struct object *obj;

    if (do_fsync() && (obj = get_handle_obj( process, handle, access, &fsync_ops)))
        return (struct event *)obj; /* even though it's not an event */

    if (do_esync() && (obj = get_handle_obj( process, handle, access, &esync_ops)))
        return (struct event *)obj; /* even though it's not an event */

    return (struct event *)get_handle_obj( process, handle, access, &event_ops );
}

void pulse_event( struct event *event )
{
    event->signaled = 1;
    /* wake up all waiters if manual reset, a single one otherwise */
    wake_up( &event->obj, !event->manual_reset );
    event->signaled = 0;

    if (do_fsync())
        fsync_clear( &event->obj );
}

void set_event( struct event *event )
{
    if (do_fsync() && event->obj.ops == &fsync_ops)
    {
        fsync_set_event( (struct fsync *)event );
        return;
    }

    if (do_esync() && event->obj.ops == &esync_ops)
    {
        esync_set_event( (struct esync *)event );
        return;
    }

    event->signaled = 1;
    /* wake up all waiters if manual reset, a single one otherwise */
    wake_up( &event->obj, !event->manual_reset );
}

void reset_event( struct event *event )
{
    if (do_fsync() && event->obj.ops == &fsync_ops)
    {
        fsync_reset_event( (struct fsync *)event );
        return;
    }

    if (do_esync() && event->obj.ops == &esync_ops)
    {
        esync_reset_event( (struct esync *)event );
        return;
    }
    event->signaled = 0;

    if (do_fsync())
        fsync_clear( &event->obj );

    if (do_esync())
        esync_clear( event->esync_fd );
}

static void event_dump( struct object *obj, int verbose )
{
    struct event *event = (struct event *)obj;
    assert( obj->ops == &event_ops );
    fprintf( stderr, "Event manual=%d signaled=%d\n",
             event->manual_reset, event->signaled );
}

static struct object_type *event_get_type( struct object *obj )
{
    static const struct unicode_str str = { type_Event, sizeof(type_Event) };
    return get_object_type( &str );
}

static int event_signaled( struct object *obj, struct wait_queue_entry *entry )
{
    struct event *event = (struct event *)obj;
    assert( obj->ops == &event_ops );
    return event->signaled;
}

static int event_get_esync_fd( struct object *obj, enum esync_type *type )
{
    struct event *event = (struct event *)obj;
    *type = event->manual_reset ? ESYNC_MANUAL_SERVER : ESYNC_AUTO_SERVER;
    return event->esync_fd;
}

static unsigned int event_get_fsync_idx( struct object *obj, enum fsync_type *type )
{
    struct event *event = (struct event *)obj;
    *type = FSYNC_MANUAL_SERVER;
    return event->fsync_idx;
}

static void event_satisfied( struct object *obj, struct wait_queue_entry *entry )
{
    struct event *event = (struct event *)obj;
    assert( obj->ops == &event_ops );
    /* Reset if it's an auto-reset event */
    if (!event->manual_reset) event->signaled = 0;
}

static unsigned int event_map_access( struct object *obj, unsigned int access )
{
    if (access & GENERIC_READ)    access |= STANDARD_RIGHTS_READ | EVENT_QUERY_STATE;
    if (access & GENERIC_WRITE)   access |= STANDARD_RIGHTS_WRITE | EVENT_MODIFY_STATE;
    if (access & GENERIC_EXECUTE) access |= STANDARD_RIGHTS_EXECUTE | SYNCHRONIZE;
    if (access & GENERIC_ALL)     access |= STANDARD_RIGHTS_ALL | EVENT_QUERY_STATE | EVENT_MODIFY_STATE;
    return access & ~(GENERIC_READ | GENERIC_WRITE | GENERIC_EXECUTE | GENERIC_ALL);
}

static int event_signal( struct object *obj, unsigned int access )
{
    struct event *event = (struct event *)obj;
    assert( obj->ops == &event_ops );

    if (!(access & EVENT_MODIFY_STATE))
    {
        set_error( STATUS_ACCESS_DENIED );
        return 0;
    }
    set_event( event );
    return 1;
}

static struct list *event_get_kernel_obj_list( struct object *obj )
{
    struct event *event = (struct event *)obj;
    return &event->kernel_object;
}

static void event_destroy( struct object *obj )
{
    struct event *event = (struct event *)obj;

    if (do_esync())
        close( event->esync_fd );
}

struct keyed_event *create_keyed_event( struct object *root, const struct unicode_str *name,
                                        unsigned int attr, const struct security_descriptor *sd )
{
    struct keyed_event *event;

    if ((event = create_named_object( root, &keyed_event_ops, name, attr, sd )))
    {
        if (get_error() != STATUS_OBJECT_NAME_EXISTS)
        {
            /* initialize it if it didn't already exist */
        }
    }
    return event;
}

struct keyed_event *get_keyed_event_obj( struct process *process, obj_handle_t handle, unsigned int access )
{
    return (struct keyed_event *)get_handle_obj( process, handle, access, &keyed_event_ops );
}

static void keyed_event_dump( struct object *obj, int verbose )
{
    fputs( "Keyed event\n", stderr );
}

static struct object_type *keyed_event_get_type( struct object *obj )
{
    static const struct unicode_str str = { type_KeyedEvent, sizeof(type_KeyedEvent) };
    return get_object_type( &str );
}

static enum select_op matching_op( enum select_op op )
{
    return op ^ (SELECT_KEYED_EVENT_WAIT ^ SELECT_KEYED_EVENT_RELEASE);
}

static int keyed_event_signaled( struct object *obj, struct wait_queue_entry *entry )
{
    struct wait_queue_entry *ptr;
    struct process *process;
    enum select_op select_op;

    assert( obj->ops == &keyed_event_ops );

    process = get_wait_queue_thread( entry )->process;
    select_op = get_wait_queue_select_op( entry );
    if (select_op != SELECT_KEYED_EVENT_WAIT && select_op != SELECT_KEYED_EVENT_RELEASE) return 1;

    LIST_FOR_EACH_ENTRY( ptr, &obj->wait_queue, struct wait_queue_entry, entry )
    {
        if (ptr == entry) continue;
        if (get_wait_queue_thread( ptr )->process != process) continue;
        if (get_wait_queue_select_op( ptr ) != matching_op( select_op )) continue;
        if (get_wait_queue_key( ptr ) != get_wait_queue_key( entry )) continue;
        if (wake_thread_queue_entry( ptr )) return 1;
    }
    return 0;
}

static unsigned int keyed_event_map_access( struct object *obj, unsigned int access )
{
    if (access & GENERIC_READ)    access |= STANDARD_RIGHTS_READ | KEYEDEVENT_WAIT;
    if (access & GENERIC_WRITE)   access |= STANDARD_RIGHTS_WRITE | KEYEDEVENT_WAKE;
    if (access & GENERIC_EXECUTE) access |= STANDARD_RIGHTS_EXECUTE;
    if (access & GENERIC_ALL)     access |= KEYEDEVENT_ALL_ACCESS;
    return access & ~(GENERIC_READ | GENERIC_WRITE | GENERIC_EXECUTE | GENERIC_ALL);
}

/* create an event */
DECL_HANDLER(create_event)
{
    struct event *event;
    struct unicode_str name;
    struct object *root;
    const struct security_descriptor *sd;
    const struct object_attributes *objattr = get_req_object_attributes( &sd, &name, &root );

    if (!objattr) return;

    if ((event = create_event( root, &name, objattr->attributes,
                               req->manual_reset, req->initial_state, sd )))
    {
        if (get_error() == STATUS_OBJECT_NAME_EXISTS)
            reply->handle = alloc_handle( current->process, event, req->access, objattr->attributes );
        else
            reply->handle = alloc_handle_no_access_check( current->process, event,
                                                          req->access, objattr->attributes );
        release_object( event );
    }

    if (root) release_object( root );
}

/* open a handle to an event */
DECL_HANDLER(open_event)
{
    struct unicode_str name = get_req_unicode_str();

    reply->handle = open_object( current->process, req->rootdir, req->access,
                                 &event_ops, &name, req->attributes );
}

/* do an event operation */
DECL_HANDLER(event_op)
{
    struct event *event;

    if (!(event = get_event_obj( current->process, req->handle, EVENT_MODIFY_STATE ))) return;
    reply->state = event->signaled;
    switch(req->op)
    {
    case PULSE_EVENT:
        pulse_event( event );
        break;
    case SET_EVENT:
        set_event( event );
        break;
    case RESET_EVENT:
        reset_event( event );
        break;
    default:
        set_error( STATUS_INVALID_PARAMETER );
        break;
    }
    release_object( event );
}

/* return details about the event */
DECL_HANDLER(query_event)
{
    struct event *event;

    if (!(event = get_event_obj( current->process, req->handle, EVENT_QUERY_STATE ))) return;

    reply->manual_reset = event->manual_reset;
    reply->state = event->signaled;

    release_object( event );
}

/* create a keyed event */
DECL_HANDLER(create_keyed_event)
{
    struct keyed_event *event;
    struct unicode_str name;
    struct object *root;
    const struct security_descriptor *sd;
    const struct object_attributes *objattr = get_req_object_attributes( &sd, &name, &root );

    if (!objattr) return;

    if ((event = create_keyed_event( root, &name, objattr->attributes, sd )))
    {
        if (get_error() == STATUS_OBJECT_NAME_EXISTS)
            reply->handle = alloc_handle( current->process, event, req->access, objattr->attributes );
        else
            reply->handle = alloc_handle_no_access_check( current->process, event,
                                                          req->access, objattr->attributes );
        release_object( event );
    }
    if (root) release_object( root );
}

/* open a handle to a keyed event */
DECL_HANDLER(open_keyed_event)
{
    struct unicode_str name = get_req_unicode_str();

    reply->handle = open_object( current->process, req->rootdir, req->access,
                                 &keyed_event_ops, &name, req->attributes );
}
