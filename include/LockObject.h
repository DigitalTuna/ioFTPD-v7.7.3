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

#define LOCK_MAX_COUNT	10000

typedef struct _LOCKOBJECT
{
	HANDLE			hEvent[2];
	LONG volatile	lExclusive;

} LOCKOBJECT, *LPLOCKOBJECT;


VOID ReleaseExclusiveLock(LPLOCKOBJECT lpLockObject);
VOID AcquireExclusiveLock(LPLOCKOBJECT lpLockObject);
VOID ReleaseSharedLock(LPLOCKOBJECT lpLockObject);
VOID AcquireSharedLock(LPLOCKOBJECT lpLockObject);
VOID DeleteLockObject(LPLOCKOBJECT lpLockObject);
BOOL InitializeLockObject(LPLOCKOBJECT lpLockObject);


/*

typedef struct _LOCKOBJECT
{
	LONG volatile	lCounter;
	LONG volatile	lLock;
	BOOL			bExclusiveLock;
	HANDLE			hEvent;

} LOCKOBJECT, * LPLOCKOBJECT;


BOOL Lock_Init(VOID);
VOID Lock_DeInit(VOID);

VOID ReleaseExclusiveLock(LPLOCKOBJECT lpLock);
VOID AcquireExclusiveLock(LPLOCKOBJECT lpLock);
VOID ReleaseSharedLock(LPLOCKOBJECT lpLock);
VOID AcquireSharedLock(LPLOCKOBJECT lpLock);
VOID DeleteLockObject(LPLOCKOBJECT lpLock);
BOOL InitializeLockObject(LPLOCKOBJECT lpLock); */
