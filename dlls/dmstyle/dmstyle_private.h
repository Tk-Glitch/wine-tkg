/* DirectMusicStyle Private Include
 *
 * Copyright (C) 2003-2004 Rok Mandeljc
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#ifndef __WINE_DMSTYLE_PRIVATE_H
#define __WINE_DMSTYLE_PRIVATE_H

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#define COBJMACROS

#include "windef.h"
#include "winbase.h"
#include "winnt.h"
#include "wingdi.h"
#include "winuser.h"

#include "wine/debug.h"
#include "wine/list.h"
#include "winreg.h"
#include "objbase.h"

#include "dmusici.h"
#include "dmusicf.h"
#include "dmusics.h"

/*****************************************************************************
 * ClassFactory
 */
extern HRESULT WINAPI create_dmstyle(REFIID lpcGUID, LPVOID* ppobj) DECLSPEC_HIDDEN;
extern HRESULT WINAPI create_dmauditiontrack(REFIID riid, void **ret_iface) DECLSPEC_HIDDEN;
extern HRESULT WINAPI create_dmchordtrack(REFIID riid, void **ret_iface) DECLSPEC_HIDDEN;
extern HRESULT WINAPI create_dmcommandtrack(REFIID riid, void **ret_iface) DECLSPEC_HIDDEN;
extern HRESULT WINAPI create_dmmotiftrack(REFIID riid, void **ret_iface) DECLSPEC_HIDDEN;
extern HRESULT WINAPI create_dmmutetrack(REFIID riid, void **ret_iface) DECLSPEC_HIDDEN;
extern HRESULT WINAPI create_dmstyletrack(REFIID riid, void **ret_iface) DECLSPEC_HIDDEN;

/*****************************************************************************
 * Auxiliary definitions
 */
typedef struct _DMUS_PRIVATE_STYLE_ITEM {
  struct list entry; /* for listing elements */
  DWORD dwTimeStamp;
  IDirectMusicStyle8* pObject;
} DMUS_PRIVATE_STYLE_ITEM, *LPDMUS_PRIVATE_STYLE_ITEM;


typedef struct _DMUS_PRIVATE_COMMAND {
	struct list entry; /* for listing elements */
	DMUS_IO_COMMAND pCommand;
	IDirectMusicCollection* ppReferenceCollection;
} DMUS_PRIVATE_COMMAND, *LPDMUS_PRIVATE_COMMAND;

/**********************************************************************
 * Dll lifetime tracking declaration for dmstyle.dll
 */
extern LONG DMSTYLE_refCount DECLSPEC_HIDDEN;
static inline void DMSTYLE_LockModule(void) { InterlockedIncrement( &DMSTYLE_refCount ); }
static inline void DMSTYLE_UnlockModule(void) { InterlockedDecrement( &DMSTYLE_refCount ); }

/*****************************************************************************
 * Misc.
 */
#include "dmutils.h"

#endif	/* __WINE_DMSTYLE_PRIVATE_H */
