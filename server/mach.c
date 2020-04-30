/*
 * Server-side debugger support using Mach primitives
 *
 * Copyright (C) 1999, 2006 Alexandre Julliard
 * Copyright (C) 2006 Ken Thomases for CodeWeavers
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
#include <errno.h>
#include <stdio.h>
#include <signal.h>
#include <stdarg.h>
#include <sys/types.h>
#include <unistd.h>
#ifdef HAVE_SYS_SYSCALL_H
#include <sys/syscall.h>
#endif

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "winternl.h"

#include "file.h"
#include "process.h"
#include "thread.h"
#include "request.h"

#ifdef USE_MACH

#include <mach/mach.h>
#include <mach/mach_error.h>
#include <mach/thread_act.h>
#include <mach/mach_vm.h>
#include <servers/bootstrap.h>

static mach_port_t server_mach_port;

void sigchld_callback(void)
{
    assert(0);  /* should never be called on MacOS */
}

static void mach_set_error(kern_return_t mach_error)
{
    switch (mach_error)
    {
        case KERN_SUCCESS:              break;
        case KERN_INVALID_ARGUMENT:     set_error(STATUS_INVALID_PARAMETER); break;
        case KERN_NO_SPACE:             set_error(STATUS_NO_MEMORY); break;
        case KERN_PROTECTION_FAILURE:   set_error(STATUS_ACCESS_DENIED); break;
        case KERN_INVALID_ADDRESS:      set_error(STATUS_ACCESS_VIOLATION); break;
        default:                        set_error(STATUS_UNSUCCESSFUL); break;
    }
}

static mach_port_t get_process_port( struct process *process )
{
    return process->trace_data;
}

/* initialize the process control mechanism */
void init_tracing_mechanism(void)
{
    mach_port_t bp;

    if (task_get_bootstrap_port(mach_task_self(), &bp) != KERN_SUCCESS)
        fatal_error("Can't find bootstrap port\n");
    if (mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &server_mach_port) != KERN_SUCCESS)
        fatal_error("Can't allocate port\n");
    if  (mach_port_insert_right( mach_task_self(),
                                 server_mach_port,
                                 server_mach_port,
                                 MACH_MSG_TYPE_MAKE_SEND ) != KERN_SUCCESS)
            fatal_error("Error inserting rights\n");
    if (bootstrap_register(bp, server_dir, server_mach_port) != KERN_SUCCESS)
        fatal_error("Can't check in server_mach_port\n");
    mach_port_deallocate(mach_task_self(), bp);
}

/* initialize the per-process tracing mechanism */
void init_process_tracing( struct process *process )
{
    int pid, ret;
    struct
    {
        mach_msg_header_t           header;
        mach_msg_body_t             body;
        mach_msg_port_descriptor_t  task_port;
        mach_msg_trailer_t          trailer; /* only present on receive */
    } msg;

    for (;;)
    {
        ret = mach_msg( &msg.header, MACH_RCV_MSG|MACH_RCV_TIMEOUT, 0, sizeof(msg),
                        server_mach_port, 0, 0 );
        if (ret)
        {
            if (ret != MACH_RCV_TIMED_OUT && debug_level)
                fprintf( stderr, "warning: mach port receive failed with %x\n", ret );
            return;
        }

        /* if anything in the message is invalid, ignore it */
        if (msg.header.msgh_size != offsetof(typeof(msg), trailer)) continue;
        if (msg.body.msgh_descriptor_count != 1) continue;
        if (msg.task_port.type != MACH_MSG_PORT_DESCRIPTOR) continue;
        if (msg.task_port.disposition != MACH_MSG_TYPE_PORT_SEND) continue;
        if (msg.task_port.name == MACH_PORT_NULL) continue;
        if (msg.task_port.name == MACH_PORT_DEAD) continue;

        if (!pid_for_task( msg.task_port.name, &pid ))
        {
            struct thread *thread = get_thread_from_pid( pid );

            if (thread && !thread->process->trace_data)
                thread->process->trace_data = msg.task_port.name;
            else
                mach_port_deallocate( mach_task_self(), msg.task_port.name );
        }
    }
}

/* terminate the per-process tracing mechanism */
void finish_process_tracing( struct process *process )
{
    if (process->trace_data)
    {
        mach_port_deallocate( mach_task_self(), process->trace_data );
        process->trace_data = 0;
    }
}

/* initialize registers in new thread if necessary */
void init_thread_context( struct thread *thread )
{
}

/* retrieve the thread x86 registers */
void get_thread_context( struct thread *thread, context_t *context, unsigned int flags )
{
#if defined(__i386__) || defined(__x86_64__)
    x86_debug_state_t state;
    mach_msg_type_number_t count = sizeof(state) / sizeof(int);
    mach_msg_type_name_t type;
    mach_port_t port, process_port = get_process_port( thread->process );

    /* all other regs are handled on the client side */
    assert( flags == SERVER_CTX_DEBUG_REGISTERS );

    if (thread->unix_pid == -1 || !process_port ||
        mach_port_extract_right( process_port, thread->unix_tid,
                                 MACH_MSG_TYPE_COPY_SEND, &port, &type ))
    {
        set_error( STATUS_ACCESS_DENIED );
        return;
    }

    if (!thread_get_state( port, x86_DEBUG_STATE, (thread_state_t)&state, &count ))
    {
#ifdef __x86_64__
        assert( state.dsh.flavor == x86_DEBUG_STATE32 ||
                state.dsh.flavor == x86_DEBUG_STATE64 );
#else
        assert( state.dsh.flavor == x86_DEBUG_STATE32 );
#endif

#ifdef __x86_64__
        if (state.dsh.flavor == x86_DEBUG_STATE64)
        {
            context->debug.x86_64_regs.dr0 = state.uds.ds64.__dr0;
            context->debug.x86_64_regs.dr1 = state.uds.ds64.__dr1;
            context->debug.x86_64_regs.dr2 = state.uds.ds64.__dr2;
            context->debug.x86_64_regs.dr3 = state.uds.ds64.__dr3;
            context->debug.x86_64_regs.dr6 = state.uds.ds64.__dr6;
            context->debug.x86_64_regs.dr7 = state.uds.ds64.__dr7;
        }
        else
#endif
        {
/* work around silly renaming of struct members in OS X 10.5 */
#if __DARWIN_UNIX03 && defined(_STRUCT_X86_DEBUG_STATE32)
            context->debug.i386_regs.dr0 = state.uds.ds32.__dr0;
            context->debug.i386_regs.dr1 = state.uds.ds32.__dr1;
            context->debug.i386_regs.dr2 = state.uds.ds32.__dr2;
            context->debug.i386_regs.dr3 = state.uds.ds32.__dr3;
            context->debug.i386_regs.dr6 = state.uds.ds32.__dr6;
            context->debug.i386_regs.dr7 = state.uds.ds32.__dr7;
#else
            context->debug.i386_regs.dr0 = state.uds.ds32.dr0;
            context->debug.i386_regs.dr1 = state.uds.ds32.dr1;
            context->debug.i386_regs.dr2 = state.uds.ds32.dr2;
            context->debug.i386_regs.dr3 = state.uds.ds32.dr3;
            context->debug.i386_regs.dr6 = state.uds.ds32.dr6;
            context->debug.i386_regs.dr7 = state.uds.ds32.dr7;
#endif
        }
        context->flags |= SERVER_CTX_DEBUG_REGISTERS;
    }
    mach_port_deallocate( mach_task_self(), port );
#endif
}

/* set the thread x86 registers */
void set_thread_context( struct thread *thread, const context_t *context, unsigned int flags )
{
#if defined(__i386__) || defined(__x86_64__)
    x86_debug_state_t state;
    mach_msg_type_number_t count = sizeof(state) / sizeof(int);
    mach_msg_type_name_t type;
    mach_port_t port, process_port = get_process_port( thread->process );
    unsigned int dr7;

    /* all other regs are handled on the client side */
    assert( flags == SERVER_CTX_DEBUG_REGISTERS );

    if (thread->unix_pid == -1 || !process_port ||
        mach_port_extract_right( process_port, thread->unix_tid,
                                 MACH_MSG_TYPE_COPY_SEND, &port, &type ))
    {
        set_error( STATUS_ACCESS_DENIED );
        return;
    }


#ifdef __x86_64__
    if (thread->process->cpu == CPU_x86_64)
    {
        /* Mac OS doesn't allow setting the global breakpoint flags */
        dr7 = (context->debug.x86_64_regs.dr7 & ~0xaa) | ((context->debug.x86_64_regs.dr7 & 0xaa) >> 1);

        state.dsh.flavor = x86_DEBUG_STATE64;
        state.dsh.count = sizeof(state.uds.ds64) / sizeof(int);
        state.uds.ds64.__dr0 = context->debug.x86_64_regs.dr0;
        state.uds.ds64.__dr1 = context->debug.x86_64_regs.dr1;
        state.uds.ds64.__dr2 = context->debug.x86_64_regs.dr2;
        state.uds.ds64.__dr3 = context->debug.x86_64_regs.dr3;
        state.uds.ds64.__dr4 = 0;
        state.uds.ds64.__dr5 = 0;
        state.uds.ds64.__dr6 = context->debug.x86_64_regs.dr6;
        state.uds.ds64.__dr7 = dr7;
    }
    else
#endif
    {
        dr7 = (context->debug.i386_regs.dr7 & ~0xaa) | ((context->debug.i386_regs.dr7 & 0xaa) >> 1);

        state.dsh.flavor = x86_DEBUG_STATE32;
        state.dsh.count = sizeof(state.uds.ds32) / sizeof(int);
#if __DARWIN_UNIX03 && defined(_STRUCT_X86_DEBUG_STATE32)
        state.uds.ds32.__dr0 = context->debug.i386_regs.dr0;
        state.uds.ds32.__dr1 = context->debug.i386_regs.dr1;
        state.uds.ds32.__dr2 = context->debug.i386_regs.dr2;
        state.uds.ds32.__dr3 = context->debug.i386_regs.dr3;
        state.uds.ds32.__dr4 = 0;
        state.uds.ds32.__dr5 = 0;
        state.uds.ds32.__dr6 = context->debug.i386_regs.dr6;
        state.uds.ds32.__dr7 = dr7;
#else
        state.uds.ds32.dr0 = context->debug.i386_regs.dr0;
        state.uds.ds32.dr1 = context->debug.i386_regs.dr1;
        state.uds.ds32.dr2 = context->debug.i386_regs.dr2;
        state.uds.ds32.dr3 = context->debug.i386_regs.dr3;
        state.uds.ds32.dr4 = 0;
        state.uds.ds32.dr5 = 0;
        state.uds.ds32.dr6 = context->debug.i386_regs.dr6;
        state.uds.ds32.dr7 = dr7;
#endif
    }
    thread_set_state( port, x86_DEBUG_STATE, (thread_state_t)&state, count );
    mach_port_deallocate( mach_task_self(), port );
#endif
}

int send_thread_signal( struct thread *thread, int sig )
{
    int ret = -1;
    mach_port_t process_port = get_process_port( thread->process );

    if (thread->unix_pid != -1 && process_port)
    {
        mach_msg_type_name_t type;
        mach_port_t port;

        if (!mach_port_extract_right( process_port, thread->unix_tid,
                                      MACH_MSG_TYPE_COPY_SEND, &port, &type ))
        {
            ret = syscall( SYS___pthread_kill, port, sig );
            mach_port_deallocate( mach_task_self(), port );
        }
        else errno = ESRCH;

        if (ret == -1 && errno == ESRCH) /* thread got killed */
        {
            thread->unix_pid = -1;
            thread->unix_tid = -1;
        }
    }
    if (debug_level && ret != -1)
        fprintf( stderr, "%04x: *sent signal* signal=%d\n", thread->id, sig );
    return (ret != -1);
}

/* read data from a process memory space */
int read_process_memory( struct process *process, client_ptr_t ptr, data_size_t size, char *dest )
{
    kern_return_t ret;
    mach_msg_type_number_t bytes_read;
    mach_vm_offset_t offset;
    vm_offset_t data;
    mach_vm_address_t aligned_address;
    mach_vm_size_t aligned_size;
    unsigned int page_size = get_page_size();
    mach_port_t process_port = get_process_port( process );

    if (!process_port)
    {
        set_error( STATUS_ACCESS_DENIED );
        return 0;
    }
    if ((mach_vm_address_t)ptr != ptr)
    {
        set_error( STATUS_ACCESS_DENIED );
        return 0;
    }

    if ((ret = task_suspend( process_port )) != KERN_SUCCESS)
    {
        mach_set_error( ret );
        return 0;
    }

    offset = ptr % page_size;
    aligned_address = (mach_vm_address_t)(ptr - offset);
    aligned_size = (size + offset + page_size - 1) / page_size * page_size;

    ret = mach_vm_read( process_port, aligned_address, aligned_size, &data, &bytes_read );
    if (ret != KERN_SUCCESS) mach_set_error( ret );
    else
    {
        memcpy( dest, (char *)data + offset, size );
        mach_vm_deallocate( mach_task_self(), data, bytes_read );
    }
    task_resume( process_port );
    return (ret == KERN_SUCCESS);
}

/* write data to a process memory space */
int write_process_memory( struct process *process, client_ptr_t ptr, data_size_t size, const char *src )
{
    kern_return_t ret;
    mach_vm_address_t aligned_address, region_address;
    mach_vm_size_t aligned_size, region_size;
    mach_msg_type_number_t info_size, bytes_read;
    mach_vm_offset_t offset;
    vm_offset_t task_mem = 0;
    struct vm_region_basic_info_64 info;
    mach_port_t dummy;
    unsigned int page_size = get_page_size();
    mach_port_t process_port = get_process_port( process );

    if (!process_port)
    {
        set_error( STATUS_ACCESS_DENIED );
        return 0;
    }
    if ((mach_vm_address_t)ptr != ptr)
    {
        set_error( STATUS_ACCESS_DENIED );
        return 0;
    }

    offset = ptr % page_size;
    aligned_address = (mach_vm_address_t)(ptr - offset);
    aligned_size = (size + offset + page_size - 1) / page_size * page_size;

    if ((ret = task_suspend( process_port )) != KERN_SUCCESS)
    {
        mach_set_error( ret );
        return 0;
    }

    ret = mach_vm_read( process_port, aligned_address, aligned_size, &task_mem, &bytes_read );
    if (ret != KERN_SUCCESS)
    {
        mach_set_error( ret );
        goto failed;
    }
    region_address = aligned_address;
    info_size = sizeof(info);
    ret = mach_vm_region( process_port, &region_address, &region_size, VM_REGION_BASIC_INFO_64,
                     (vm_region_info_t)&info, &info_size, &dummy );
    if (ret != KERN_SUCCESS)
    {
        mach_set_error( ret );
        goto failed;
    }
    if (region_address > aligned_address ||
        region_address + region_size < aligned_address + aligned_size)
    {
        /* FIXME: should support multiple regions */
        set_error( ERROR_ACCESS_DENIED );
        goto failed;
    }
    ret = mach_vm_protect( process_port, aligned_address, aligned_size, 0, VM_PROT_READ | VM_PROT_WRITE );
    if (ret != KERN_SUCCESS)
    {
        mach_set_error( ret );
        goto failed;
    }

    /* FIXME: there's an optimization that can be made: check first and last */
    /* pages for writability; read first and last pages; write interior */
    /* pages to task without ever reading&modifying them; if that succeeds, */
    /* modify first and last pages and write them. */

    memcpy( (char*)task_mem + offset, src, size );

    ret = mach_vm_write( process_port, aligned_address, task_mem, bytes_read );
    if (ret != KERN_SUCCESS) mach_set_error( ret );
    else
    {
        mach_vm_deallocate( mach_task_self(), task_mem, bytes_read );
        /* restore protection */
        mach_vm_protect( process_port, aligned_address, aligned_size, 0, info.protection );
        task_resume( process_port );
        return 1;
    }

failed:
    if (task_mem) mach_vm_deallocate( mach_task_self(), task_mem, bytes_read );
    task_resume( process_port );
    return 0;
}

/* retrieve an LDT selector entry */
void get_selector_entry( struct thread *thread, int entry, unsigned int *base,
                         unsigned int *limit, unsigned char *flags )
{
    const unsigned int total_size = (2 * sizeof(int) + 1) * 8192;
    struct process *process = thread->process;
    unsigned int page_size = get_page_size();
    vm_offset_t data;
    kern_return_t ret;
    mach_msg_type_number_t bytes_read;
    mach_port_t process_port = get_process_port( thread->process );

    if (!process->ldt_copy || !process_port)
    {
        set_error( STATUS_ACCESS_DENIED );
        return;
    }
    if (entry >= 8192)
    {
        set_error( STATUS_INVALID_PARAMETER );  /* FIXME */
        return;
    }

    if ((ret = task_suspend( process_port )) == KERN_SUCCESS)
    {
        mach_vm_offset_t offset = process->ldt_copy % page_size;
        mach_vm_address_t aligned_address = (mach_vm_address_t)(process->ldt_copy - offset);
        mach_vm_size_t aligned_size = (total_size + offset + page_size - 1) / page_size * page_size;

        ret = mach_vm_read( process_port, aligned_address, aligned_size, &data, &bytes_read );
        if (ret != KERN_SUCCESS) mach_set_error( ret );
        else
        {
            const int *ldt = (const int *)((char *)data + offset);
            memcpy( base, ldt + entry, sizeof(int) );
            memcpy( limit, ldt + entry + 8192, sizeof(int) );
            memcpy( flags, (char *)(ldt + 2 * 8192) + entry, 1 );
            mach_vm_deallocate( mach_task_self(), data, bytes_read );
        }
        task_resume( process_port );
    }
    else mach_set_error( ret );
}

#endif  /* USE_MACH */
