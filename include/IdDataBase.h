/*
 * Copyright(c) 2006 iniCom Networks, Inc.
 *
 * This file is part of ioFTPD.
 *
 * ioFTPD is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * ioFTPD is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with ioFTPD; see the file COPYING.  if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 */

typedef struct _IDITEM
{
	INT32	Id;
	TCHAR	tszModuleName[_MAX_NAME + 1];
	TCHAR	tszName[_MAX_NAME + 1];

} IDITEM, * LPIDITEM;

typedef struct _FREED_IDITEM
{
	struct _FREED_IDITEM	*lpNext;

} FREED_IDITEM, * LPFREED_IDITEM; 

typedef INT (__cdecl *ID_DB_CB)(LPVOID, LPSTR, INT32);
typedef VOID (__cdecl *ID_DB_ERROR)(int, LPTSTR, int, LPTSTR, LPTSTR);

typedef struct _IDDATABASE
{
	LPTSTR		*lpNameTable[1024];
	LPIDITEM	*lpIdArray;
	DWORD		dwIdArraySize;
	DWORD		dwIdArrayItems;
	LPTSTR		tszFileName;

	LPFREED_IDITEM	lpFreedId;
	INT32		iNextFreeId;

	LOCKOBJECT	loDataBase;

} IDDATABASE, * LPIDDATABASE;

#define	DB_SUCCESS	0
#define	DB_DELETED	1
#define	DB_FATAL	2


BOOL IdDataBase_Init(LPTSTR tszTableLocation, LPIDDATABASE lpDataBase, LPVOID lpFindModuleProc, ID_DB_CB OpenIdDataProc, ID_DB_ERROR ErrorProc);
BOOL IdDataBase_Rename(LPTSTR tszName, LPTSTR tszModuleName, LPTSTR tszNewName, LPIDDATABASE lpDataBase);
INT32 IdDataBase_Add(LPTSTR tszName, LPTSTR tszModuleName, LPIDDATABASE lpDataBase);
BOOL IdDataBase_Remove(LPTSTR tszName, LPTSTR tszModuleName, LPIDDATABASE lpDataBase);
INT32 IdDataBase_SearchByName(LPTSTR tszName, LPIDDATABASE lpDataBase);
LPTSTR IdDataBase_SearchById(INT32 Id, LPIDDATABASE lpIdDataBase);
INT32 IdDataBase_GetNextId(LPINT lpOffset, LPIDDATABASE lpDataBase);
PINT32 IdDataBase_GetIdList(LPIDDATABASE lpDataBase, LPDWORD lpIdCount);
VOID IdDataBase_Free(LPIDDATABASE lpDataBase);
BOOL IdDataBase_Write(LPIDDATABASE lpDataBase);
