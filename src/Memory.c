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

#include <ioFTPD.h>


static HANDLE				ioHeap;
static CRITICAL_SECTION	BucketLock;
static DWORD				MemoryInBuckets, LargeAllocations;
static DWORD		BucketSize[MEMORY_BUCKETS];
static LPVOID		MemoryBucket[MEMORY_BUCKETS];


#ifndef _LIMITED

__inline
DWORD GetBucket(DWORD Size)
{
	register DWORD	Bucket;

	for (Bucket = 0 ; Bucket < MEMORY_BUCKETS && BucketSize[Bucket] < Size ; Bucket++);

	return (Bucket < MEMORY_BUCKETS ? Bucket : Size);
}




__inline
LPVOID _FragmentAllocate(register DWORD Bucket)
{
	register ULONG	Memory;

	//	Allocate memory
	if (Bucket >= MEMORY_BUCKETS)
	{
		//	Allocate Block
Bucket_Allocation:
#ifdef _DEBUG_MEM
		if ((Memory = (ULONG)HeapAlloc(ioHeap, 0, (Bucket >= MEMORY_BUCKETS ? Bucket : BucketSize[Bucket]) + 2 * sizeof(DWORD) + sizeof(LPVOID))))
		{
			//	Successfully allocated
			((LPDWORD)Memory)[0]	= 0xFFFFFFFF;
			((LPDWORD)Memory)[1]	= Bucket;
			((LPDWORD)(Memory + sizeof(LPVOID) + sizeof(DWORD) + (Bucket >= MEMORY_BUCKETS ? Bucket : BucketSize[Bucket])))[0]	= 0xFFFFFFFF;
			//	Set return offset
			Memory	+= sizeof(LPVOID) + sizeof(DWORD);
		}
#else
		if ((Memory = (ULONG)HeapAlloc(ioHeap, 0, (Bucket >= MEMORY_BUCKETS ? Bucket : BucketSize[Bucket]) + sizeof(LPVOID))))
		{
			//	Successfully allocated
			((LPDWORD)Memory)[0]	= Bucket;
			//	Set return offset
			Memory	+= sizeof(LPVOID);
		}
#endif
	}
	else if (! MemoryBucket[Bucket])
	{
		//	No memory waiting in this bucket, try larger buckets
		Memory	= (Bucket < (MEMORY_BUCKETS - 4) ? 4 : (MEMORY_BUCKETS - 1) - Bucket);

		for (;Memory-- > 1;)
		{
			if (MemoryBucket[Memory + Bucket])
			{
				//	Suitable bucket found
				Bucket	+= Memory;
				goto Bucket_Reuse;
			}
		}
		//	Allocate new chunk for bucket
		goto Bucket_Allocation;
	}
	else
	{
		//	Get memory from bucket
Bucket_Reuse:
		Memory	= (ULONG)MemoryBucket[Bucket];
#ifdef _DEBUG_MEM
		//	Push bucket by one
		MemoryBucket[Bucket]	= ((LPVOID *)(Memory + sizeof(DWORD)))[0];
		//	Store bucket number
		((LPDWORD)Memory)[1]	= Bucket;
		//	Set return offset
		Memory	+= sizeof(LPVOID) + sizeof(DWORD);
#else
		//	Push bucket by one
		MemoryBucket[Bucket]	= ((LPVOID *)Memory)[0];
		//	Store bucket number
		((LPDWORD)Memory)[0]	= Bucket;
		//	Set return offset
		Memory	+= sizeof(LPVOID);
#endif
		//	Reduce memory counter
		MemoryInBuckets	-= BucketSize[Bucket];
	}

	//	Return pointer
	return (LPVOID)Memory;
}





__inline
VOID _FragmentFree(register LPVOID Memory, register DWORD Bucket)
{
	LPVOID	Last;

#ifdef _DEBUG_MEM
	TCHAR	MemoryOffset[128];

	if (((LPDWORD)Memory)[0] != 0xFFFFFFFF ||
		((LPDWORD)((ULONG)Memory + sizeof(LPVOID) + sizeof(DWORD) + (Bucket >= MEMORY_BUCKETS ? Bucket : BucketSize[Bucket])))[0] != 0xFFFFFFFF)
	{	
		wsprintf(MemoryOffset, "0x%X", Memory);
		MessageBox(NULL, MemoryOffset, "Corrupted memory block", 0);
	}
#endif
	//	Freed

	//	Deallocate memory
	if (Bucket < MEMORY_BUCKETS)
	{
		//	Store memory for reuse
#ifdef _DEBUG_MEM
		((LPVOID *)((ULONG)Memory + sizeof(DWORD)))[0]	= MemoryBucket[Bucket];
		MemoryBucket[Bucket]	= Memory;
#else
		((LPVOID *)Memory)[0]	= MemoryBucket[Bucket];
		MemoryBucket[Bucket]	= Memory;
#endif
		//	Increase memory counter
		if ((MemoryInBuckets += BucketSize[Bucket]) > MAX_MEMORY_IN_BUCKETS)
		{
MemoryCleanUp:
			for (Bucket = 0; Bucket < MEMORY_BUCKETS ; Bucket++)
			{
				Memory	= MemoryBucket[Bucket];
				while (Memory)
				{
					//	Store current address
					Last	= Memory;
					//	Get next address
#ifdef _DEBUG_MEM
					Memory	= ((LPVOID *)((ULONG)Memory + sizeof(DWORD)))[0];
#else
					Memory	= ((LPVOID *)Memory)[0];
#endif
					//	Free memory
					HeapFree(ioHeap, 0, Last);
				}
				//	Set storage pointer to null
				MemoryBucket[Bucket]	= NULL;
			}
			//	Compact heap
			HeapCompact(ioHeap, 0);
			//	Set memory counters to zero
			MemoryInBuckets		= 0;
			LargeAllocations	= 0;
		}
	}
	else
	{
		//	Free memory
		HeapFree(ioHeap, 0, Memory);
		//	Increase Large Allocations counter
		if (++LargeAllocations == MAX_LARGE_ALLOCATIONS) goto MemoryCleanUp;
	}
}





LPVOID FragmentAllocate(DWORD Size)
{
	register LPVOID	Return;
	register DWORD	Bucket;

	if (! Size) return NULL;
	//	Get bucket
	Bucket	= GetBucket(Size);

	//	Allocate memory
	EnterCriticalSection(&BucketLock);
	Return	= _FragmentAllocate(Bucket);
	LeaveCriticalSection(&BucketLock);

	return Return;
}




BOOL FragmentFree(register LPVOID lpMemory)
{
	register DWORD	Bucket;

	if (! lpMemory) return FALSE;
#ifdef _DEBUG_MEM
	//	Caclulate memory offset
	lpMemory	= (LPVOID)((ULONG)lpMemory - sizeof(LPVOID) - sizeof(DWORD));
	//	Get bucket
	Bucket	= ((LPDWORD)lpMemory)[1];
#else
	//	Caclulate memory offset
	lpMemory	= (LPVOID)((ULONG)lpMemory - sizeof(LPVOID));
	//	Get bucket
	Bucket	= ((LPDWORD)lpMemory)[0];
#endif
	//	Free memory
	EnterCriticalSection(&BucketLock);
	_FragmentFree(lpMemory, Bucket);
	LeaveCriticalSection(&BucketLock);
	return FALSE;
}





LPVOID FragmentReAllocate(LPVOID lpMem, DWORD Size)
{
	register LPVOID	Memory, OldMemory;
	register DWORD	Bucket, OldBucket;

	if (! lpMem)
	{
		//	Normal allocation
		return FragmentAllocate(Size);
	}

	//	Get old bucket
#ifdef _DEBUG_MEM
	OldMemory	= (LPVOID)((ULONG)lpMem - sizeof(LPVOID) - sizeof(DWORD));
	OldBucket	= ((LPDWORD)OldMemory)[1];
#else
	OldMemory	= (LPVOID)((ULONG)lpMem - sizeof(LPVOID));
	OldBucket	= ((LPDWORD)OldMemory)[0];
#endif
	//	Check wheter allocation is needed
	if (OldBucket < MEMORY_BUCKETS && BucketSize[OldBucket] >= Size) return lpMem;

	//	Get new bucket
	Bucket	= GetBucket(Size);

	EnterCriticalSection(&BucketLock);
	//	Allocate new memory
	if ((Memory = _FragmentAllocate(Bucket)))
	{
		//	Copy memory
		CopyMemory(Memory, lpMem,
			(OldBucket < MEMORY_BUCKETS ? BucketSize[OldBucket] : OldBucket));
		//	Free old memory
		_FragmentFree(OldMemory, OldBucket);
	}
	//	Unlock memory
	LeaveCriticalSection(&BucketLock);
	return Memory;
}
#endif








BOOL MyHeapFree(LPVOID lpMem)
{
	//	Free memory from heap
	return HeapFree(ioHeap, 0, lpMem);
}



LPVOID MyHeapAllocate(DWORD dwSize)
{
	//	Allocate memory from heap
	return HeapAlloc(ioHeap, 0, dwSize);
}



LPVOID MyHeapReAllocate(LPVOID lpMem, DWORD Size)
{
	//	If null pointer, use allocate
	if (! lpMem) return MyHeapAllocate(Size);

	//	Use reallocate
	return HeapReAlloc(ioHeap, 0, lpMem, Size);
}









#ifdef _DEBUG_MEM

typedef struct _MEMORY_DEBUGINFO
{
	LPCSTR						szDescription;
	struct _MEMORY_DEBUGINFO	*lpPrev;
	struct _MEMORY_DEBUGINFO	*lpNext;

} MEMORY_DEBUGINFO, * LPMEMORY_DEBUGINFO;


CRITICAL_SECTION	csMemoryDebug;
LPMEMORY_DEBUGINFO	lpMemoryDebugHead, lpMemoryDebugTail;

LPVOID DebugAllocate(LPCSTR szDescription, DWORD dwSize)
{
	LPMEMORY_DEBUGINFO	lpDebug;

	if (! dwSize) return NULL;
	//	Allocate memory
	lpDebug	= (LPMEMORY_DEBUGINFO)_Allocate(dwSize + sizeof(MEMORY_DEBUGINFO));
	if (! lpDebug) return NULL;
	//	Store string
	lpDebug->szDescription	= szDescription;
	//	Enter critical section
	EnterCriticalSection(&csMemoryDebug);
	//	Append to list
	if (lpMemoryDebugHead)
	{
		//	Edit tail
		lpMemoryDebugTail->lpNext	= lpDebug;
		//	Set pointers
		lpDebug->lpPrev	= lpMemoryDebugTail;
		lpDebug->lpNext	= NULL;
	}
	else
	{
		//	Set new head
		lpMemoryDebugHead	= lpDebug;
		//	Set pointers
		lpDebug->lpPrev	= NULL;
		lpDebug->lpNext	= NULL;
	}
	//	Set new tail
	lpMemoryDebugTail	= lpDebug;
	//	Leave critical section
	LeaveCriticalSection(&csMemoryDebug);

	return (LPVOID)((ULONG)lpDebug + sizeof(MEMORY_DEBUGINFO));
}


BOOL DebugFree(LPVOID lpMem)
{
	LPMEMORY_DEBUGINFO	lpDebug;

	if (! lpMem) return FALSE;
	//	Calculate offset
	lpDebug	= (LPMEMORY_DEBUGINFO)((ULONG)lpMem - sizeof(MEMORY_DEBUGINFO));

	EnterCriticalSection(&csMemoryDebug);
	//	Update previous item
	if (lpDebug->lpPrev)
	{
		lpDebug->lpPrev->lpNext	= lpDebug->lpNext;
	}
	else lpMemoryDebugHead	= lpDebug->lpNext;
	//	Update following item
	if (lpDebug->lpNext)
	{
		lpDebug->lpNext->lpPrev	= lpDebug->lpPrev;
	}
	else lpMemoryDebugTail	= lpDebug->lpPrev;
	LeaveCriticalSection(&csMemoryDebug);

	//	Free memory
	return TRUE;
}


LPVOID DebugReAllocate(LPVOID lpMem, LPCSTR szDescription, DWORD dwSize)
{
	LPMEMORY_DEBUGINFO	lpDebug;

	//	Check previous allocation
	if (! lpMem) return DebugAllocate(szDescription, dwSize);

	EnterCriticalSection(&csMemoryDebug);
	//	Reallocate memory
	lpDebug	= (LPMEMORY_DEBUGINFO)_ReAllocate(
		(LPVOID)((ULONG)lpMem - sizeof(MEMORY_DEBUGINFO)), dwSize + sizeof(MEMORY_DEBUGINFO));

	if (lpDebug)
	{
		//	Update previous item
		if (lpDebug->lpPrev)
		{
			lpDebug->lpPrev->lpNext	= lpDebug;
		}
		else lpMemoryDebugHead	= lpDebug;
		//	Update following item
		if (lpDebug->lpNext)
		{
			lpDebug->lpNext->lpPrev	= lpDebug;
		}
		else lpMemoryDebugTail	= lpDebug;

		lpDebug	= (LPMEMORY_DEBUGINFO)((ULONG)lpDebug + sizeof(MEMORY_DEBUGINFO));
	}
	LeaveCriticalSection(&csMemoryDebug);

	return (LPVOID)lpDebug;
}


VOID DebugMemoryDump(VOID)
{
	LPMEMORY_DEBUGINFO	lpDebug;
	HANDLE				hDumpFile;
	CHAR				pBuffer[4096];
	DWORD				dwBytesToWrite, dwBytesWritten;

	//	Open file for writing
	hDumpFile	= CreateFile("c:\\ioFTPD.memory.dump", GENERIC_WRITE,
		FILE_SHARE_READ, NULL, OPEN_ALWAYS, 0, NULL);
	if (hDumpFile == INVALID_HANDLE_VALUE) return;

	//	Dump debug info
	for (lpDebug = lpMemoryDebugHead;lpDebug;lpDebug = lpDebug->lpNext)
	{
		//	Check description
		if (lpDebug->szDescription)
		{
			//	Format string
			dwBytesToWrite	= sprintf(pBuffer, "0x%X %s\r\n", (ULONG)&lpDebug[1], lpDebug->szDescription);
			//	Write to file
			WriteFile(hDumpFile, pBuffer, dwBytesToWrite, &dwBytesWritten, NULL);
		}
	}
	//	Close file
	SetEndOfFile(hDumpFile);
	CloseHandle(hDumpFile);
}

#endif



#ifdef _DEBUG_MEM
LPVOID _AllocateShared(register LPVOID lpMem, LPSTR szDescription, DWORD dwSize)
#else
LPVOID _AllocateShared(register LPVOID lpMem, DWORD dwSize)
#endif
{
	//	Share existing?
	if (lpMem)
	{
		if ( (((PLONG)((ULONG)lpMem - sizeof(LONG)))[0] != 0xDEADBEAF) )
		{
			Putlog(LOG_ERROR, "AllocateShared: Discovered corrupted shared header.\r\n");
			return lpMem;
		}
	    InterlockedIncrement((PLONG)((ULONG)lpMem - sizeof(LONG)*2));
	}
	else
	{
		if (! dwSize) return NULL;
		//	Allocate memory
#ifdef _DEBUG_MEM
		lpMem	= DebugAllocate(szDescription, dwSize + sizeof(LONG volatile));
#else
		lpMem	= _Allocate(dwSize + sizeof(LONG volatile)*2);
#endif
		if (lpMem)
		{
			//	Set share count
			((PLONG)lpMem)[0]	= 1;
			((PLONG)lpMem)[1]	= 0xDEADBEAF;
			//	Push lpmem by dword
			lpMem	= (LPVOID)((ULONG)lpMem + sizeof(LONG)*2);
		}
	}
	return lpMem;
}




BOOL FreeShared(LPVOID lpMem)
{
	register BOOL				bReturn;
	register LPLONG volatile	lpOffset;

	if (! lpMem) return FALSE;

	lpOffset	= (LPLONG)((ULONG)lpMem - sizeof(LONG)*2);
	//	Decrease share count
	if (lpOffset[1] != 0xDEADBEAF)
	{
		Putlog(LOG_ERROR, "FreeShared: Discovered corrupted shared header.\r\n");
		return FALSE;
	}
	if (lpOffset[0] == 1 || ! InterlockedDecrement(lpOffset))
	{
		//	Free memory, share count = 0/1
#ifdef _DEBUG_MEM
		bReturn	= DebugFree(lpOffset);
#else
#  ifdef USE_MALLOC
		lpOffset[1] = 0;
		_Free(lpOffset);
		return TRUE;
#  else
		bReturn	= _Free(lpOffset);
#  endif
#endif
	}
	else bReturn	= FALSE;

	return bReturn;
}





BOOL Memory_Init(BOOL bFirstInitialization)
{
	UINT	Bucket, Size, Incr;

	if (! bFirstInitialization) return TRUE;

	//	Reset buckets
	ZeroMemory(MemoryBucket, sizeof(MemoryBucket));
	//	No Memory in buckets
	MemoryInBuckets		= 0;
	LargeAllocations	= 0;
	//	Start size
	Size	= 8;
	//	Increment by 8
	Incr	= 8;
	//	Initialize bucket sizes
	for (Bucket = 0; Bucket < MEMORY_BUCKETS ; Bucket++)
	{
		BucketSize[Bucket]	= Size;
		//	Increment size by Increment
		Size	+= Incr;
		//	Increment Increment by 8
		Incr	+= 8;
	}
	//	Heap lock
	if (! InitializeCriticalSectionAndSpinCount(&BucketLock, 100)) return FALSE;
#ifdef _DEBUG_MEM
	//	Debug lock
	InitializeCriticalSectionAndSpinCount(&csMemoryDebug, 100);
	//	Allocated memory list pointers
	lpMemoryDebugHead	= NULL;
	lpMemoryDebugTail	= NULL;
#endif
	//	Create Process Heap
#ifdef _LIMITED
	if ((ioHeap = GetProcessHeap()) != INVALID_HANDLE_VALUE) return TRUE;
#else
#ifndef USE_MALLOC
	if ((ioHeap = HeapCreate(HEAP_NO_SERIALIZE, 1024 * 1024, 0)) == INVALID_HANDLE_VALUE) return FALSE;
#endif
#endif
	return TRUE;
}





VOID Memory_DeInit(VOID)
{
	LPVOID	Memory, Last;
	DWORD	Bucket;

	for (Bucket = 0; Bucket < MEMORY_BUCKETS ; Bucket++)
	{
		Memory	= MemoryBucket[Bucket];
		while (Memory)
		{
			//	Store current address
			Last	= Memory;
			//	Get next address
#ifdef _DEBUG_MEM
			Memory	= ((LPVOID *)((ULONG)Memory + sizeof(DWORD)))[0];
#else
			Memory	= ((LPVOID *)Memory)[0];
#endif
			//	Free memory
#ifndef USE_MALLOC
			HeapFree(ioHeap, 0, Last);
#endif
		}
		//	Set storage pointer to null
		MemoryBucket[Bucket]	= NULL;
	}
	//	Delete Lock
	DeleteCriticalSection(&BucketLock);
#ifdef _DEBUG_MEM
	DeleteCriticalSection(&csMemoryDebug);
	//	Dump unreleased memory
	DebugMemoryDump();
#endif
}

