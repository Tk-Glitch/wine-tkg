/*
 * Copyright 2000 Peter Hunnisett
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

#ifndef __WINE_DPLAY_GLOBAL_INCLUDED
#define __WINE_DPLAY_GLOBAL_INCLUDED

#include <stdarg.h>

#include "windef.h"
#include "winbase.h"
#include "wine/dplaysp.h"
#include "lobbysp.h"
#include "dplayx_queue.h"

extern HRESULT DPL_EnumAddress( LPDPENUMADDRESSCALLBACK lpEnumAddressCallback,
                                LPCVOID lpAddress, DWORD dwAddressSize,
                                LPVOID lpContext ) DECLSPEC_HIDDEN;

typedef struct tagEnumSessionAsyncCallbackData
{
  LPSPINITDATA lpSpData;
  GUID         requestGuid;
  DWORD        dwEnumSessionFlags;
  DWORD        dwTimeout;
  HANDLE       hSuicideRequest;
} EnumSessionAsyncCallbackData;

typedef struct tagDP_MSG_REPLY_STRUCT
{
  HANDLE hReceipt;
  WORD   wExpectedReply;
  LPVOID lpReplyMsg;
  DWORD  dwMsgBodySize;
  /* FIXME: Is the message header required as well? */
} DP_MSG_REPLY_STRUCT, *LPDP_MSG_REPLY_STRUCT;

typedef struct tagDP_MSG_REPLY_STRUCT_LIST
{
  DPQ_ENTRY(tagDP_MSG_REPLY_STRUCT_LIST) repliesExpected;
  DP_MSG_REPLY_STRUCT replyExpected;
} DP_MSG_REPLY_STRUCT_LIST, *LPDP_MSG_REPLY_STRUCT_LIST;

struct PlayerData
{
  /* Individual player information */
  DPID dpid;

  DPNAME name;
  HANDLE hEvent;

  ULONG  uRef;  /* What is the reference count on this data? */

  /* View of local data */
  LPVOID lpLocalData;
  DWORD  dwLocalDataSize;

  /* View of remote data */
  LPVOID lpRemoteData;
  DWORD  dwRemoteDataSize;

  /* SP data on a per player basis */
  LPVOID lpSPPlayerData;

  DWORD  dwFlags; /* Special remarks about the type of player */
};
typedef struct PlayerData* lpPlayerData;

struct PlayerList
{
  DPQ_ENTRY(PlayerList) players;

  lpPlayerData lpPData;
};
typedef struct PlayerList* lpPlayerList;

struct GroupData
{
  /* Internal information */
  DPID parent; /* If parent == 0 it's a top level group */

  ULONG uRef; /* Reference count */

  DPQ_HEAD(GroupList)  groups;  /* A group has [0..n] groups */
  DPQ_HEAD(PlayerList) players; /* A group has [0..n] players */

  DPID idGroupOwner; /* ID of player who owns the group */

  DWORD dwFlags; /* Flags describing anything special about the group */

  DPID   dpid;
  DPNAME name;

  /* View of local data */
  LPVOID lpLocalData;
  DWORD  dwLocalDataSize;

  /* View of remote data */
  LPVOID lpRemoteData;
  DWORD  dwRemoteDataSize;
};
typedef struct GroupData  GroupData;
typedef struct GroupData* lpGroupData;

struct GroupList
{
  DPQ_ENTRY(GroupList) groups;

  lpGroupData lpGData;
};
typedef struct GroupList* lpGroupList;

struct DPMSG
{
  DPQ_ENTRY( DPMSG ) msgs;
  DPMSG_GENERIC* msg;
};
typedef struct DPMSG* LPDPMSG;

enum SPSTATE
{
  NO_PROVIDER = 0,
  DP_SERVICE_PROVIDER = 1,
  DP_LOBBY_PROVIDER = 2
};

/* Contains all data members. FIXME: Rename me */
typedef struct tagDirectPlay2Data
{
  BOOL   bConnectionOpen;

  /* For async EnumSessions requests */
  HANDLE hEnumSessionThread;
  HANDLE hKillEnumSessionThreadEvent;
  DWORD  dwEnumSessionLock;

  LPVOID lpNameServerData; /* DPlay interface doesn't know contents */

  BOOL bHostInterface; /* Did this interface create the session */

  lpGroupData lpSysGroup; /* System group with _everything_ in it */

  LPDPSESSIONDESC2 lpSessionDesc;

  /* I/O Msg queues */
  DPQ_HEAD( DPMSG ) receiveMsgs; /* Msg receive queue */
  DPQ_HEAD( DPMSG ) sendMsgs;    /* Msg send pending queue */

  /* Information about the service provider active on this connection */
  SPINITDATA spData;
  BOOL       bSPInitialized;

  /* Information about the lobby server that's attached to this DP object */
  SPDATA_INIT dplspData;
  BOOL        bDPLSPInitialized;

  /* Our service provider */
  HMODULE hServiceProvider;

  /* Our DP lobby provider */
  HMODULE hDPLobbyProvider;

  enum SPSTATE connectionInitialized;

  /* Expected messages queue */
  DPQ_HEAD( tagDP_MSG_REPLY_STRUCT_LIST ) repliesExpected;
} DirectPlay2Data;

typedef struct IDirectPlayImpl
{
  IDirectPlay IDirectPlay_iface;
  IDirectPlay2A IDirectPlay2A_iface;
  IDirectPlay2 IDirectPlay2_iface;
  IDirectPlay3A IDirectPlay3A_iface;
  IDirectPlay3 IDirectPlay3_iface;
  IDirectPlay4A IDirectPlay4A_iface;
  IDirectPlay4  IDirectPlay4_iface;
  LONG numIfaces; /* "in use interfaces" refcount */
  LONG ref, ref2A, ref2, ref3A, ref3, ref4A, ref4;
  CRITICAL_SECTION lock;
  DirectPlay2Data *dp2;
} IDirectPlayImpl;

HRESULT DP_HandleMessage( IDirectPlayImpl *This, const void *lpMessageBody,
        DWORD  dwMessageBodySize, const void *lpMessageHeader, WORD wCommandId, WORD wVersion,
        void **lplpReply, DWORD *lpdwMsgSize ) DECLSPEC_HIDDEN;

/* DP SP external interfaces into DirectPlay */
extern HRESULT DP_GetSPPlayerData( IDirectPlayImpl *lpDP, DPID idPlayer, void **lplpData ) DECLSPEC_HIDDEN;
extern HRESULT DP_SetSPPlayerData( IDirectPlayImpl *lpDP, DPID idPlayer, void *lpData ) DECLSPEC_HIDDEN;

/* DP external interfaces to call into DPSP interface */
extern LPVOID DPSP_CreateSPPlayerData(void) DECLSPEC_HIDDEN;

extern HRESULT dplay_create( REFIID riid, void **ppv ) DECLSPEC_HIDDEN;
extern HRESULT dplobby_create( REFIID riid, void **ppv ) DECLSPEC_HIDDEN;
extern HRESULT dplaysp_create( REFIID riid, void **ppv, IDirectPlayImpl *dp ) DECLSPEC_HIDDEN;
extern HRESULT dplobbysp_create( REFIID riid, void **ppv, IDirectPlayImpl *dp ) DECLSPEC_HIDDEN;

#endif /* __WINE_DPLAY_GLOBAL_INCLUDED */
