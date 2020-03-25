/*
 * Copyright (C) 2015 Austin English
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

#ifndef _NTDEF_
#define _NTDEF_

#ifdef __cplusplus
extern "C" {
#endif

typedef enum _EVENT_TYPE {
    NotificationEvent,
    SynchronizationEvent
} EVENT_TYPE;

typedef enum _TIMER_TYPE {
    NotificationTimer,
    SynchronizationTimer
} TIMER_TYPE;

typedef enum _WAIT_TYPE {
    WaitAll,
    WaitAny,
    WaitNotification
} WAIT_TYPE;

#ifdef __cplusplus
}
#endif

#define NT_SUCCESS(status)      (((NTSTATUS)(status)) >= 0)
#define NT_INFORMATION(status)  ((((NTSTATUS)(status)) & 0xc0000000) == 0x40000000)
#define NT_WARNING(status)      ((((NTSTATUS)(status)) & 0xc0000000) == 0x80000000)
#define NT_ERROR(status)        ((((NTSTATUS)(status)) & 0xc0000000) == 0xc0000000)

#endif /* _NTDEF_ */
