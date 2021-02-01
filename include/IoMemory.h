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

BOOL	Memory_Init(BOOL bFirstInitialization);
VOID	Memory_DeInit(VOID);

BOOL FreeShared(LPVOID lpMem);

#ifdef _DEBUG_MEM

   LPVOID DebugReAllocate(LPVOID lpMem, LPCSTR szDescription, DWORD dwSize);
   BOOL DebugFree(LPVOID lpMem);
   LPVOID DebugAllocate(LPCSTR szDescription, DWORD dwSize);
   LPVOID _AllocateShared(LPVOID lpMem, LPSTR szDescription, DWORD dwSize);

#  define AllocateShared(lpMem, szDescription, dwSize)	_AllocateShared(lpMem, szDescription, dwSize)

#else

   LPVOID _AllocateShared(LPVOID lpMem, DWORD dwSize);

#  define AllocateShared(lpMem, szDescription, dwSize)	_AllocateShared(lpMem, dwSize)

#endif



#ifdef USE_MALLOC

#  define Allocate(szDescription, dwSize)			malloc(dwSize)
#  define ReAllocate(lpMem, szDescription, dwSize)	realloc(lpMem, dwSize)
#  define Free(lpMem)					free(lpMem)

#  define _Allocate	malloc
#  define _ReAllocate	realloc
#  define _Free		free

#else

#  define	SHARECOUNT(lpMem)	(((LPDWORD)lpMem)[-1])
#  ifdef _LIMITED
   //	Limited mode
   LPVOID	MyHeapAllocate(DWORD dwSize);
   BOOL	MyHeapFree(LPVOID lpMem);
   LPVOID	MyHeapReAllocate(LPVOID lpMem, DWORD Size);

#  define	_Allocate		MyHeapAllocate
#  define _Free			MyHeapFree
#  define _ReAllocate		MyHeapReAllocate

#  else

//	Full Mode
   LPVOID	FragmentAllocate(DWORD Size);
   BOOL	FragmentFree(LPVOID Memory);
   LPVOID	FragmentReAllocate(LPVOID lpMem, DWORD Size);

#  define _Allocate		FragmentAllocate
#  define _ReAllocate		FragmentReAllocate
#  define _Free			FragmentFree

#  endif

#  ifdef _DEBUG_MEM
#    define Allocate(szDescription, dwSize)				DebugAllocate(szDescription, dwSize)
#    define AllocateShared(lpMem, szDescription, dwSize)	_AllocateShared(lpMem, szDescription, dwSize)
#    define ReAllocate(lpMem, szDescription, dwSize)	DebugReAllocate(lpMem, szDescription, dwSize)
#    define Free(lpMem)									DebugFree(lpMem)
#  else

#    define	Allocate(szDescription, dwSize)				_Allocate(dwSize)
#    define AllocateShared(lpMem, szDescription, dwSize)	_AllocateShared(lpMem, dwSize)
#    define	ReAllocate(lpMem, szDescription, dwSize)	_ReAllocate(lpMem, dwSize)
#    define	Free(lpMem)									_Free(lpMem)

#  endif

#endif // USE_MALLOC
