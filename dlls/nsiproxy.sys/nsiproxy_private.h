/*
 * nsiproxy.sys
 *
 * Copyright 2021 Huw Davies
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
struct icmp_listen_params
{
    HANDLE handle;
    void *reply;
    ULONGLONG user_reply_ptr;
    unsigned int bits, reply_len;
    int timeout;
};

struct icmp_send_echo_params
{
    SOCKADDR_INET *dst;
    void *request, *reply;
    DWORD request_size, reply_len;
    BYTE bits, ttl, tos;
    HANDLE handle;
};

/* output for IOCTL_NSIPROXY_WINE_ICMP_ECHO - cf. ICMP_ECHO_REPLY */
struct icmp_echo_reply_32
{
    ULONG addr;
    ULONG status;
    ULONG round_trip_time;
    USHORT data_size;
    USHORT num_of_pkts;
    ULONG data_ptr;
    struct
    {
        BYTE ttl;
        BYTE tos;
        BYTE flags;
        BYTE options_size;
        ULONG options_ptr;
    } opts;
};

struct icmp_echo_reply_64
{
    ULONG addr;
    ULONG status;
    ULONG round_trip_time;
    USHORT data_size;
    USHORT num_of_pkts;
    ULONGLONG data_ptr;
    struct
    {
        BYTE ttl;
        BYTE tos;
        BYTE flags;
        BYTE options_size;
        ULONGLONG options_ptr;
    } opts;
};
