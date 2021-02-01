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

static BOOL CreateWorkerThread(VOID);
static UINT WINAPI WorkerThread(LPTHREADDATA lpThreadData);
static UINT WINAPI IoThreadEx(LPVOID lpContext);


HANDLE				hCompletionPort;
static volatile BOOL bThreadExitFlag;
static LPTHREADDATA	lpObsoleteThreadPool;
static HANDLE		hJobAlert;
static LONG volatile lIoThreadCount, lWorkerThreadCount;
static DWORD		dwThreadDataTlsIndex;
static volatile LONG	lFreeWorkerThreads, lInitialWorkerThreads, lBlockingWorkerThreads, lWorkerThreads, lWorkerTclLock;
static volatile LPJOB	lpJobQueue[2][3], lpFreeJob, lpAllocatedJobs;
static DWORD		dwPriorityCount[3];
static CRITICAL_SECTION	csJobQueue, csWorkerThreadCount, csInheritedHandleLock;
static BOOL         bCreateTclInterpreters, bLogExitingWorkerThreads;


unsigned int crc32_table[256] = {  0x00000000, 0x77073096, 0xEE0E612C, 0x990951BA,
0x076DC419, 0x706AF48F, 0xE963A535, 0x9E6495A3, 0x0EDB8832, 0x79DCB8A4, 0xE0D5E91E,
0x97D2D988, 0x09B64C2B, 0x7EB17CBD, 0xE7B82D07, 0x90BF1D91, 0x1DB71064, 0x6AB020F2,
0xF3B97148, 0x84BE41DE, 0x1ADAD47D, 0x6DDDE4EB, 0xF4D4B551, 0x83D385C7, 0x136C9856,
0x646BA8C0, 0xFD62F97A, 0x8A65C9EC, 0x14015C4F, 0x63066CD9, 0xFA0F3D63, 0x8D080DF5,
0x3B6E20C8, 0x4C69105E, 0xD56041E4, 0xA2677172, 0x3C03E4D1, 0x4B04D447, 0xD20D85FD,
0xA50AB56B, 0x35B5A8FA, 0x42B2986C, 0xDBBBC9D6, 0xACBCF940, 0x32D86CE3, 0x45DF5C75,
0xDCD60DCF, 0xABD13D59, 0x26D930AC, 0x51DE003A, 0xC8D75180, 0xBFD06116, 0x21B4F4B5,
0x56B3C423, 0xCFBA9599, 0xB8BDA50F, 0x2802B89E, 0x5F058808, 0xC60CD9B2, 0xB10BE924,
0x2F6F7C87, 0x58684C11, 0xC1611DAB, 0xB6662D3D, 0x76DC4190, 0x01DB7106, 0x98D220BC,
0xEFD5102A, 0x71B18589, 0x06B6B51F, 0x9FBFE4A5, 0xE8B8D433, 0x7807C9A2, 0x0F00F934,
0x9609A88E, 0xE10E9818, 0x7F6A0DBB, 0x086D3D2D, 0x91646C97, 0xE6635C01, 0x6B6B51F4,
0x1C6C6162, 0x856530D8, 0xF262004E, 0x6C0695ED, 0x1B01A57B, 0x8208F4C1, 0xF50FC457,
0x65B0D9C6, 0x12B7E950, 0x8BBEB8EA, 0xFCB9887C, 0x62DD1DDF, 0x15DA2D49, 0x8CD37CF3,
0xFBD44C65, 0x4DB26158, 0x3AB551CE, 0xA3BC0074, 0xD4BB30E2, 0x4ADFA541, 0x3DD895D7,
0xA4D1C46D, 0xD3D6F4FB, 0x4369E96A, 0x346ED9FC, 0xAD678846, 0xDA60B8D0, 0x44042D73,
0x33031DE5, 0xAA0A4C5F, 0xDD0D7CC9, 0x5005713C, 0x270241AA, 0xBE0B1010, 0xC90C2086,
0x5768B525, 0x206F85B3, 0xB966D409, 0xCE61E49F, 0x5EDEF90E, 0x29D9C998, 0xB0D09822,
0xC7D7A8B4, 0x59B33D17, 0x2EB40D81, 0xB7BD5C3B, 0xC0BA6CAD, 0xEDB88320, 0x9ABFB3B6,
0x03B6E20C, 0x74B1D29A, 0xEAD54739, 0x9DD277AF, 0x04DB2615, 0x73DC1683, 0xE3630B12,
0x94643B84, 0x0D6D6A3E, 0x7A6A5AA8, 0xE40ECF0B, 0x9309FF9D, 0x0A00AE27, 0x7D079EB1,
0xF00F9344, 0x8708A3D2, 0x1E01F268, 0x6906C2FE, 0xF762575D, 0x806567CB, 0x196C3671,
0x6E6B06E7, 0xFED41B76, 0x89D32BE0, 0x10DA7A5A, 0x67DD4ACC, 0xF9B9DF6F, 0x8EBEEFF9,
0x17B7BE43, 0x60B08ED5, 0xD6D6A3E8, 0xA1D1937E, 0x38D8C2C4, 0x4FDFF252, 0xD1BB67F1,
0xA6BC5767, 0x3FB506DD, 0x48B2364B, 0xD80D2BDA, 0xAF0A1B4C, 0x36034AF6, 0x41047A60,
0xDF60EFC3, 0xA867DF55, 0x316E8EEF, 0x4669BE79, 0xCB61B38C, 0xBC66831A, 0x256FD2A0,
0x5268E236, 0xCC0C7795, 0xBB0B4703, 0x220216B9, 0x5505262F, 0xC5BA3BBE, 0xB2BD0B28,
0x2BB45A92, 0x5CB36A04, 0xC2D7FFA7, 0xB5D0CF31, 0x2CD99E8B, 0x5BDEAE1D, 0x9B64C2B0,
0xEC63F226, 0x756AA39C, 0x026D930A, 0x9C0906A9, 0xEB0E363F, 0x72076785, 0x05005713,
0x95BF4A82, 0xE2B87A14, 0x7BB12BAE, 0x0CB61B38, 0x92D28E9B, 0xE5D5BE0D, 0x7CDCEFB7,
0x0BDBDF21, 0x86D3D2D4, 0xF1D4E242, 0x68DDB3F8, 0x1FDA836E, 0x81BE16CD, 0xF6B9265B,
0x6FB077E1, 0x18B74777, 0x88085AE6, 0xFF0F6A70, 0x66063BCA, 0x11010B5C, 0x8F659EFF,
0xF862AE69, 0x616BFFD3, 0x166CCF45, 0xA00AE278, 0xD70DD2EE, 0x4E048354, 0x3903B3C2,
0xA7672661, 0xD06016F7, 0x4969474D, 0x3E6E77DB, 0xAED16A4A, 0xD9D65ADC, 0x40DF0B66,
0x37D83BF0, 0xA9BCAE53, 0xDEBB9EC5, 0x47B2CF7F, 0x30B5FFE9, 0xBDBDF21C, 0xCABAC28A,
0x53B39330, 0x24B4A3A6, 0xBAD03605, 0xCDD70693, 0x54DE5729, 0x23D967BF, 0xB3667A2E,
0xC4614AB8, 0x5D681B02, 0x2A6F2B94, 0xB40BBE37, 0xC30C8EA1, 0x5A05DF1B, 0x2D02EF8D };





/*

  Process_SetPriority() - Sets priority class for chosen process

  */
BOOL Process_SetPriority(LPTSTR tszPriority, HANDLE hProcess)
{
	INT	iPriority;

	//	Check that priority string is set
	if (! tszPriority) return FALSE;

	if (! _tcsnicmp(tszPriority, _TEXT("Idle"), 4))
	{
		// Idle
		iPriority	= IDLE_PRIORITY_CLASS;
	}
	else if (! _tcsnicmp(tszPriority, _TEXT("Normal"), 6))
	{
		//	Normal
		iPriority	= NORMAL_PRIORITY_CLASS;
	}
	else if (! _tcsnicmp(tszPriority, _TEXT("High"), 4))
	{
		//	Above normal
		iPriority	= HIGH_PRIORITY_CLASS;
	}
	else if (! _tcsnicmp(tszPriority, _TEXT("Realtime"), 8))
	{
		//	Time Critical
		iPriority	= REALTIME_PRIORITY_CLASS;
	}
	else return FALSE;

	return SetPriorityClass(hProcess, iPriority);
}


/*

  Thread_Init() - Initializes thread pools

  */
BOOL Thread_Init(BOOL bFirstInitialization)
{
	HANDLE			hThread;
	SYSTEM_INFO		SystemInfo;
	LPTSTR			tszPriority;
	DWORD			dwThreadId, n, dwWorkerThreadCount;
	INT             i;
	LPTHREADDATA    lpThreadData;

	if (! bFirstInitialization) return TRUE;
	//	Initialize thread vars
	lWorkerThreads			= 0;
	lInitialWorkerThreads	= 0;
	lBlockingWorkerThreads	= 0;
	lFreeWorkerThreads		= 0;
	lWorkerTclLock          = 0;
	dwThreadDataTlsIndex	= TlsAlloc();
	lpFreeJob	     = NULL;
	lpAllocatedJobs  = NULL;
	hJobAlert	     = CreateSemaphore(NULL, 0, 1000000, NULL);
	InitializeCriticalSectionAndSpinCount(&csJobQueue, 100);
	InitializeCriticalSectionAndSpinCount(&csWorkerThreadCount, 100);
	InitializeCriticalSectionAndSpinCount(&csInheritedHandleLock, 100);

	lpObsoleteThreadPool	= NULL;
	ZeroMemory(&lpJobQueue[HEAD], sizeof(lpJobQueue[HEAD]));
	ZeroMemory(&dwPriorityCount, sizeof(dwPriorityCount));
	GetSystemInfo(&SystemInfo);
	//	Create completion port
	hCompletionPort	= CreateIoCompletionPort(INVALID_HANDLE_VALUE, 0, 0, SystemInfo.dwNumberOfProcessors * 2);
	//	Get amount of threads
	if (Config_Get_Int(&IniConfigFile, _TEXT("Threads"), _TEXT("Worker_Threads"), &dwWorkerThreadCount) ||
		dwWorkerThreadCount < 5) dwWorkerThreadCount	= 5;

	if (Config_Get_Int(&IniConfigFile, _TEXT("Threads"), _TEXT("Io_Threads"), (PLONG)&lIoThreadCount) ||
		! lIoThreadCount) lIoThreadCount	= 2;
	//	Set priority for process
	tszPriority	= Config_Get(&IniConfigFile, _TEXT("Threads"), _TEXT("Process_Priority"), NULL, NULL);
	Process_SetPriority(tszPriority, GetCurrentProcess());
	Free(tszPriority);

	if (Config_Get_Bool(&IniConfigFile, _T("Threads"), _T("Create_Tcl_Interpreters"), &bCreateTclInterpreters))
	{
		bCreateTclInterpreters = FALSE;
	}
	if (Config_Get_Bool(&IniConfigFile, _T("Threads"), _T("Log_Exiting_Worker_Threads"), &bLogExitingWorkerThreads))
	{
		bLogExitingWorkerThreads = FALSE;
	}

	// as a special case, we create a worker ThreadData structure on the main thread so we can parse
	// message cookies correctly because it uses the SetAutoTheme function...
	lpThreadData = Allocate("Thread:Data:MAIN", sizeof(THREADDATA));
	if (! lpThreadData) ERROR_RETURN(ERROR_NOT_ENOUGH_MEMORY, FALSE);
	TlsSetValue(dwThreadDataTlsIndex, lpThreadData);

	for (n = 0;n < dwWorkerThreadCount;n++)
	{
		//	Create worker thread
		if (! CreateWorkerThread())
		{
			return FALSE;
		}
		lWorkerThreads++;
		lFreeWorkerThreads++;
		lInitialWorkerThreads++;
	}

	for (i = 0;i < lIoThreadCount;i++)
	{
		//	Create io thread
		hThread	= CreateThread(0, 0, (LPTHREAD_START_ROUTINE)IoThreadEx, 0, 0, (LPDWORD)&dwThreadId);
		if (hThread == INVALID_HANDLE_VALUE) return FALSE;
		//	Higher thread priority
		SetThreadPriority(hThread, THREAD_PRIORITY_ABOVE_NORMAL);
		CloseHandle(hThread);
	}

	return TRUE;
}


VOID Thread_DeInit(VOID)
{
	LPTHREADDATA lpThreadData;
	LPJOB        lpJob;
	DWORD        n;

	bThreadExitFlag = TRUE;

	// need to use a local var because lIoThreadCount is volatile and being decremented...
	n = lIoThreadCount;
	while (n--)
	{
		//	Tell io threads to suicide
		PostQueuedCompletionStatus(hCompletionPort, 0, -6, NULL);
	}

	// release a few more just in case...
	ReleaseSemaphore(hJobAlert, lWorkerThreadCount+10, NULL);

	SleepEx(100, TRUE);

#ifdef _DEBUG
	// we need a lot of time if running under purify because TCL can be really slow to cleanup
	n = GetTickCount() + 100000;
#else
	n = GetTickCount() + 10000;
#endif

	while ((lIoThreadCount || lWorkerThreadCount) && (GetTickCount() < n))
	{
		// wait a bit to see if stuff finishes because system thrashing
		SleepEx(100, TRUE);
	}

	// let any decremented counters but not yet cleaned up threads finish
	SleepEx(50, TRUE);

	if (lIoThreadCount || lWorkerThreadCount)
	{
		Putlog(LOG_ERROR, _T("Threads running at exit: worker %d,  io %d\r\n"),
			lWorkerThreadCount, lIoThreadCount);
		return;
	}
		
	CloseHandle(hCompletionPort);
	CloseHandle(hJobAlert);
	DeleteCriticalSection(&csJobQueue);
	DeleteCriticalSection(&csWorkerThreadCount);
	DeleteCriticalSection(&csInheritedHandleLock);

	lpThreadData = (LPTHREADDATA)TlsGetValue(dwThreadDataTlsIndex);
	if (lpThreadData)
	{
		Free(lpThreadData);
	}
	TlsFree(dwThreadDataTlsIndex);

	while (lpJob = lpAllocatedJobs)
	{
		lpAllocatedJobs = lpAllocatedJobs->lpNext;
		Free(lpJob);
	}
}


BOOL QueueJob(LPVOID lpProc, LPVOID lpContext, DWORD dwFlags)
{
	LPJOB	lpJob;
	DWORD	n, dwPriority;

	dwPriority	= (dwFlags & 0xFFFF);
	EnterCriticalSection(&csJobQueue);
	if (! (lpJob = lpFreeJob))
	{
		//	Allocate more objects if pool is empty
		if (! (lpFreeJob = (LPJOB)Allocate("Jobs", sizeof(JOB) * 256)))
		{
			//	Out of memory
			LeaveCriticalSection(&csJobQueue);
			return TRUE;
		}
		// first entry is just a fake to hold allocation for later freeing
		lpFreeJob->lpNext = lpAllocatedJobs;
		lpAllocatedJobs = lpFreeJob;
		lpFreeJob = &lpFreeJob[1];
		//	Initialize freejob list
		lpJob	= lpFreeJob;
		for (n = 0;n < 254;n++) lpJob	= (lpJob->lpNext = &lpJob[1]);
		lpJob->lpNext	= NULL;
		lpJob			= lpFreeJob;
	}
	lpFreeJob	= lpFreeJob->lpNext;

	//	Update job structure
	lpJob->lpProc		= lpProc;
	lpJob->lpContext	= lpContext;
	lpJob->dwFlags		= dwFlags;
	lpJob->lpNext		= NULL;

	//	Append job item to priority queue
	if (! lpJobQueue[HEAD][dwPriority])
	{
		lpJobQueue[HEAD][dwPriority]	= lpJob;
	}
	else lpJobQueue[TAIL][dwPriority]->lpNext	= lpJob;
	lpJobQueue[TAIL][dwPriority]	= lpJob;

	//	Release lock
	LeaveCriticalSection(&csJobQueue);
	ReleaseSemaphore(hJobAlert, 1, NULL);

	return FALSE;
}


static BOOL CreateWorkerThread(VOID)
{
	LPTHREADDATA	lpThreadData;
	HANDLE			hThread;
	DWORD			dwLastError, dwThreadId;

	//	Allocate thread structure
	lpThreadData	= Allocate("Thread:Data", sizeof(THREADDATA));
	if (! lpThreadData) ERROR_RETURN(ERROR_NOT_ENOUGH_MEMORY, FALSE);

	ZeroMemory(lpThreadData, sizeof(*lpThreadData));
	lpThreadData->hEvent	     = CreateEvent(NULL, FALSE, FALSE, NULL);

	if (lpThreadData->hEvent)
	{
		//	Create new thread
		InterlockedIncrement(&lWorkerThreadCount);
		hThread	= CreateThread(NULL, 0,
			(LPTHREAD_START_ROUTINE)WorkerThread, lpThreadData, 0, &dwThreadId);
		if (hThread != INVALID_HANDLE_VALUE)
		{
			CloseHandle(hThread);
			return TRUE;
		}

		dwLastError	= GetLastError();
		CloseHandle(lpThreadData->hEvent);
		InterlockedDecrement(&lWorkerThreadCount);
	}
	else dwLastError	= GetLastError();
	Free(lpThreadData);
	ERROR_RETURN(dwLastError, FALSE);
}


VOID EndWorkerThread(LPTHREADDATA lpThreadData)
{
	LPRESOURCE_DTOR	lpDestructor;

	if (bLogExitingWorkerThreads && !bThreadExitFlag)
	{
		// weird case: if you have no free worker threads and you try to print to the log it's
		// possible this will create a new thread and it might be a viscous cycle where every
		// 2 minutes you create one just to delete it later... so don't use just 1 worker thread
		// as that would make it more likely.  not sure this happens or that it's really bad.
		Putlog(LOG_DEBUG, _T("Worker exit: total=%d, free=%d, blocking=%d, initial=%d.\r\n"),
			lWorkerThreads, lFreeWorkerThreads, lBlockingWorkerThreads, lInitialWorkerThreads);
	}

	//	Call thread resource destructors	
	while (lpDestructor = lpThreadData->lpDestructor)
	{
		lpThreadData->lpDestructor	= lpDestructor->lpNext;
		lpDestructor->lpProc();
		Free(lpDestructor);
	}

	//	Free resources
	CloseHandle(lpThreadData->hEvent);
	FreeShared(lpThreadData->lpTheme);
	Free(lpThreadData);
	InterlockedDecrement(&lWorkerThreadCount);
	ExitThread(0);
}




BOOL SetBlockingThreadFlag2(BOOL bDecrement)
{
	LPTHREADDATA	lpThreadData;
	BOOL	bCreateThread;

	lpThreadData	= (LPTHREADDATA)TlsGetValue(dwThreadDataTlsIndex);

	// handle the case of code not running on a worker thread such as the user/group 3rd-party modules wrapping code
	if (!lpThreadData) return TRUE;

	if (InterlockedIncrement(&lpThreadData->lBlockingCount) != 1)
	{
		// we are already supposedly a blocking thread... just return
		return TRUE;
	}

	bCreateThread	= FALSE;
	EnterCriticalSection(&csWorkerThreadCount);
	//	Check thread counters
	lBlockingWorkerThreads++;
	if (bDecrement)
	{
		lFreeWorkerThreads--;
	}
	if (! lFreeWorkerThreads &&
		lWorkerThreads - lBlockingWorkerThreads < lInitialWorkerThreads)
	{
		if (lpObsoleteThreadPool)
		{
			SetEvent(lpObsoleteThreadPool->hEvent);
			lpObsoleteThreadPool->bReuse	= TRUE;
			lpObsoleteThreadPool	= lpObsoleteThreadPool->lpNext;
			if (lpObsoleteThreadPool) lpObsoleteThreadPool->lpPrev	= NULL;
		}
		else bCreateThread	= TRUE;
		lWorkerThreads++;
		lFreeWorkerThreads++;
	}
	LeaveCriticalSection(&csWorkerThreadCount);

	//	Create dynamic worker thread
	if (bCreateThread)
	{
		if (! CreateWorkerThread())
		{
			EnterCriticalSection(&csWorkerThreadCount);
			lWorkerThreads--;
			lFreeWorkerThreads--;
			LeaveCriticalSection(&csWorkerThreadCount);
			return FALSE;
		}
	}
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
	return TRUE;
}


BOOL SetBlockingThreadFlag(VOID)
{
	return SetBlockingThreadFlag2(FALSE);
}


VOID SetNonBlockingThreadFlag(VOID)
{
	LPTHREADDATA	lpThreadData;

	lpThreadData	= (LPTHREADDATA)TlsGetValue(dwThreadDataTlsIndex);

	// handle the case of code not running on a worker thread such as the user/group 3rd-party modules wrapping code
	if (!lpThreadData) return;

	if (InterlockedDecrement(&lpThreadData->lBlockingCount) != 0)
	{
		// we are still a blocking thread...
		return;
	}

	EnterCriticalSection(&csWorkerThreadCount);
	lBlockingWorkerThreads--;
	LeaveCriticalSection(&csWorkerThreadCount);
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);
}



BOOL InstallResourceDestructor(VOID (* lpProc)(VOID))
{
	LPRESOURCE_DTOR	lpDestructor;
	LPTHREADDATA	lpThreadData;

	lpThreadData	= (LPTHREADDATA)TlsGetValue(dwThreadDataTlsIndex);
	for (lpDestructor = lpThreadData->lpDestructor;lpDestructor;lpDestructor = lpDestructor->lpNext)
	{
		if (lpDestructor->lpProc == lpProc) return TRUE;
	}
	lpDestructor	= (LPRESOURCE_DTOR)Allocate("Thread:Dtor", sizeof(RESOURCE_DTOR));
	if (! lpDestructor) return FALSE;
	lpDestructor->lpNext	= lpThreadData->lpDestructor;
	lpDestructor->lpProc	= lpProc;
	lpThreadData->lpDestructor	= lpDestructor;
	return TRUE;
}





static UINT WINAPI WorkerThread(LPTHREADDATA lpThreadData)
{
	DWORD	dwPriority, dwDelay;
	BOOL	bCreateThread;
	LPJOB	lpJob;
	LPTCL_INTERPRETER lpTclInterpreter;

	//	Store TLS data
	TlsSetValue(dwThreadDataTlsIndex, lpThreadData);
	//	Set random seed
	srand(GetCurrentThreadId() + GetTickCount() + (DWORD)GetCurrentFiber());
	lpJob	= NULL;
	bCreateThread	= FALSE;

	// use a delay of between 5 and 10 seconds
	dwDelay = (DWORD) ((double)rand() / (RAND_MAX + 1) * 5000 + 5000);

	for (;;)
	{
		if (!bCreateTclInterpreters)
		{
			dwDelay = INFINITE;
		}
		if (WaitForSingleObject(hJobAlert, dwDelay) == WAIT_TIMEOUT)
		{
			// we timed out before getting a new job
			if (dwTclInterpreterTlsIndex != TLS_OUT_OF_INDEXES)
			{
				// The TCL system has been initialized so it's safe to create interpreters now.

				if (bThreadExitFlag)
				{
					// woops, we are supposed to be exiting!
					EndWorkerThread(lpThreadData);
				}

				lpTclInterpreter = (LPTCL_INTERPRETER)TlsGetValue(dwTclInterpreterTlsIndex);

				if (lpTclInterpreter && (lpTclInterpreter->dwConfigCounter == dwConfigCounter))
				{
					// all is good, just go back to waiting
					continue;
				}

				if (!InterlockedExchange(&lWorkerTclLock, TRUE))
				{
					// interpreter missing or out of data and we got the lock so try to create
					// an interpreter.  The lock (which we don't await for!) is designed to
					// reduces system load since this is speculative creation.

					// NOTE: used to use SetBlockingThread here but it could create extra
					// worker threads when debugging and this is fast enough and doesn't
					// block anyway...
					SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);

					// don't care about the return value or if it even succeeds
					Tcl_GetInterpreter(FALSE);

					SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);

					// free creation lock
					InterlockedExchange(&lWorkerTclLock, FALSE);
				}
			}

			// wait for job/timeout again
			continue;
		}

		if (bThreadExitFlag)
		{
			EndWorkerThread(lpThreadData);
		}

		EnterCriticalSection(&csJobQueue);
		//	Push job to list of free jobs
		if (lpJob)
		{
			lpJob->lpNext	= lpFreeJob;
			lpFreeJob	= lpJob;
		}
		//	Get job based on priority counters
		if (lpJobQueue[HEAD][JOB_PRIORITY_HIGH] &&
			(! lpJobQueue[HEAD][JOB_PRIORITY_NORMAL] || ++dwPriorityCount[JOB_PRIORITY_NORMAL] < 2))
		{
			if (! lpJobQueue[HEAD][JOB_PRIORITY_LOW] || ++dwPriorityCount[JOB_PRIORITY_LOW] < 4)
			{
				dwPriority	= JOB_PRIORITY_HIGH;
			}
			else dwPriority	= JOB_PRIORITY_LOW;
		}
		else
		{
			if (lpJobQueue[HEAD][JOB_PRIORITY_NORMAL] &&
				(lpJobQueue[HEAD][JOB_PRIORITY_HIGH] || (! lpJobQueue[HEAD][JOB_PRIORITY_LOW] || ++dwPriorityCount[JOB_PRIORITY_LOW] < 4)))
			{
				dwPriority	= JOB_PRIORITY_NORMAL;
			}
			else dwPriority	= JOB_PRIORITY_LOW;
		}
		//	Pop pending job list
		lpJob	= lpJobQueue[HEAD][dwPriority];
		lpJobQueue[HEAD][dwPriority]	= lpJob->lpNext;
		dwPriorityCount[dwPriority]		= 0;
		LeaveCriticalSection(&csJobQueue);

		EnterCriticalSection(&csWorkerThreadCount);
		//	Check thread counters
		if (! --lFreeWorkerThreads &&
			lWorkerThreads - lBlockingWorkerThreads < lInitialWorkerThreads)
		{
			if (lpObsoleteThreadPool)
			{
				SetEvent(lpObsoleteThreadPool->hEvent);
				lpObsoleteThreadPool->bReuse	= TRUE;
				lpObsoleteThreadPool	= lpObsoleteThreadPool->lpNext;
				if (lpObsoleteThreadPool) lpObsoleteThreadPool->lpPrev	= NULL;
			}
			else bCreateThread	= TRUE;
			lWorkerThreads++;
			lFreeWorkerThreads++;
		}
		LeaveCriticalSection(&csWorkerThreadCount);

		//	Create dynamic thread
		if (bCreateThread)
		{
			if (! CreateWorkerThread())
			{
				EnterCriticalSection(&csWorkerThreadCount);
				lWorkerThreads--;
				lFreeWorkerThreads--;
				LeaveCriticalSection(&csWorkerThreadCount);
			}
			bCreateThread	= FALSE;
		}

		//	Execute job
		((BOOL (__cdecl *)(LPVOID))lpJob->lpProc)(lpJob->lpContext);

		// clear theme
		FreeShared(lpThreadData->lpTheme);
		lpThreadData->lpTheme = NULL;
		lpThreadData->iAutoTheme = 0;
		lpThreadData->lpFtpUser = NULL;

		EnterCriticalSection(&csWorkerThreadCount);
		//	Destroy obsolete threads
		if (++lFreeWorkerThreads > lInitialWorkerThreads)
		{
			lWorkerThreads--;
			lFreeWorkerThreads--;
			if (lpThreadData->lpNext = lpObsoleteThreadPool) lpObsoleteThreadPool->lpPrev	= lpThreadData;
			lpObsoleteThreadPool	= lpThreadData;
			// resetting the event here just in case a thread somewhere signaled it...
			ResetEvent(lpThreadData->hEvent);
			LeaveCriticalSection(&csWorkerThreadCount);

			//	Wait for reuse, 120 second grace period
			WaitForSingleObject(lpThreadData->hEvent, 120000);
			EnterCriticalSection(&csWorkerThreadCount);
			if (! lpThreadData->bReuse)
			{
				if (lpThreadData->lpPrev)
				{
					lpThreadData->lpPrev->lpNext	= lpThreadData->lpNext;
				}
				else lpObsoleteThreadPool	= lpThreadData->lpNext;
				if (lpThreadData->lpNext) lpThreadData->lpNext->lpPrev	= lpThreadData->lpPrev;
			}
			LeaveCriticalSection(&csWorkerThreadCount);

			if (! lpThreadData->bReuse) EndWorkerThread(lpThreadData);

			lpThreadData->lpPrev	= NULL;
			lpThreadData->bReuse	= FALSE;
		}
		else LeaveCriticalSection(&csWorkerThreadCount);
	}
}



HANDLE GetThreadEvent(VOID)
{
	return ((LPTHREADDATA)TlsGetValue(dwThreadDataTlsIndex))->hEvent;
}


VOID AcquireHandleLock(VOID)
{
	EnterCriticalSection(&csInheritedHandleLock);
}

VOID ReleaseHandleLock(VOID)
{
	LeaveCriticalSection(&csInheritedHandleLock);
}




// First off, the window's overlapped structure is used as the first entry in a
// variety of structures to allow us to pass additional context in the overlapped
// callback which receives the pointer and can casts it appropriately.  This is
// commonly done.  Worker threads while on the other hand usually never call
// read/write functions directly, but rather they post an overlapped structure
// with a bytes written of -1 which the callbacks are designed to mean they should
// actually start the read/write.  I'm not sure what real benefit this design
// has or why it was done except that outstanding calls of a Worker thread that
// exit would be canceled and that would be bad.  It seems like their might
// be easier ways to avoid that scenario by just keeping track of outstanding
// requests on the thread via an interlocked counter and hold up exiting the
// rare terminated Worker until things are finished.  It would also save a full
// context switch just to start the transfer so should be faster...
//
// Overlapped reuse problems?  I've gone through and it looks like everything
// associated with transfers over data connections is fine because only 1 request
// is outstanding at a time.  It also appears that the control connection input
// should be fine as well.  However I think it's possible that control connection
// output can be an issue therefore all output must use CientJob #2 and special
// care is taken to also use this with SendQuick...

static UINT WINAPI IoThreadEx(LPVOID lpContext)
{
	LPFILEOVERLAPPED   lpOverlapped; // it could really be anything, but this uses less casts!
	LPSOCKETOVERLAPPED lpSockOver;
	DWORD			   dwBytesTransmitted, dwKey, dwLastError;
	BOOL			   bResult;

	for (;;)
	{
		//	Get queued io completion status
		bResult	= GetQueuedCompletionStatus(hCompletionPort, &dwBytesTransmitted,
			&dwKey, (LPOVERLAPPED *)&lpOverlapped, INFINITE);

		if (dwKey == (DWORD) -6)
		{
			//	Make thread exit
			InterlockedDecrement(&lIoThreadCount);
			ExitThread(0);
		}

		if (lpOverlapped)
		{
			lpSockOver = (LPSOCKETOVERLAPPED) lpOverlapped;

			switch (dwKey)
			{
			case 0:
				dwLastError = (! bResult ? GetLastError() : NO_ERROR);
				break;
			case (DWORD)-1:
				//	File IO, success
				dwLastError	= (! bResult ? GetLastError() : NO_ERROR);
				// see if we need to start a queued request because too many were outstanding on device...
				PopIOQueue(lpOverlapped->hFile);
				break;
			case (DWORD)-2:
				//	Pending socket send
				if (!SendQueuedIO(lpSockOver)) continue;
				dwLastError	= WSAGetLastError();
				break;
			case (DWORD)-3:
				//	Pending socket receive
				if (!ReceiveQueuedIO(lpSockOver)) continue;
				dwLastError	= WSAGetLastError();
				break;
			case (DWORD)-5:
				//	File io error
				dwLastError	= lpOverlapped->Internal;
				// see if we need to start a queued request because too many were outstanding on device...
				PopIOQueue(lpOverlapped->hFile);
				break;
			}
			// reset identifier so it's fresh and we can detect overlapped re-use in Send/Receive
			InterlockedExchange(&lpOverlapped->lIdentifier, 0);
			lpOverlapped->lpProc(lpOverlapped->lpContext, dwBytesTransmitted, dwLastError);
		}
	}
	return FALSE;
}



//__inline
DWORD CalculateCrc32(register PCHAR pOffset, register DWORD dwBytes, register PUINT32 pCrc32)
{
	register UINT32	Crc32;

	//	Calculate crc
	if (pCrc32)
	{
		for (Crc32 = pCrc32[0];dwBytes--;)
		{
			Crc32	= (Crc32 >> 8) ^ crc32_table[(BYTE)((BYTE)Crc32 ^ (pOffset++)[0])];
		}
		pCrc32[0]	= Crc32;
	}
	else
	{
		for (Crc32 = 0xFFFFFFFF;dwBytes--;)
		{
			Crc32	= (Crc32 >> 8) ^ crc32_table[(BYTE)((BYTE)Crc32 ^ (pOffset++)[0])];
		}
	}
	return Crc32;
}


void SetTheme(LPOUTPUT_THEME lpTheme)
{
	LPTHREADDATA	lpThreadData;

	lpThreadData	= (LPTHREADDATA)TlsGetValue(dwThreadDataTlsIndex);
	if (!lpThreadData) return;
	if (lpTheme)
	{
		AllocateShared(lpTheme, "", 0);
	}
	FreeShared(lpThreadData->lpTheme);
	lpThreadData->lpTheme = lpTheme;
}


LPOUTPUT_THEME GetTheme()
{
	LPTHREADDATA	lpThreadData;

	lpThreadData	= (LPTHREADDATA)TlsGetValue(dwThreadDataTlsIndex);
	if (!lpThreadData) return NULL;
	return lpThreadData->lpTheme;
}


// Event_Init calls Message_PreCompile which ends up checking for suffix's which resets this to 0
// and all that is done from a non-worker thread, so must handle the no threaddata case...
void SetAutoTheme(INT32 iTheme)
{
	LPTHREADDATA	lpThreadData;

	lpThreadData	= (LPTHREADDATA)TlsGetValue(dwThreadDataTlsIndex);
	if (!lpThreadData) return;
	lpThreadData->iAutoTheme = iTheme;
}


INT32 GetAutoTheme()
{
	LPTHREADDATA	lpThreadData;

	lpThreadData	= (LPTHREADDATA)TlsGetValue(dwThreadDataTlsIndex);
	if (!lpThreadData) return 0;
	return lpThreadData->iAutoTheme;
}



VOID SetFtpUser(LPFTPUSER lpFtpUser)
{
	LPTHREADDATA	lpThreadData;

	lpThreadData	= (LPTHREADDATA)TlsGetValue(dwThreadDataTlsIndex);
	if (!lpThreadData) return;
	lpThreadData->lpFtpUser = lpFtpUser;
}


LPFTPUSER GetFtpUser()
{
	LPTHREADDATA	lpThreadData;

	lpThreadData	= (LPTHREADDATA)TlsGetValue(dwThreadDataTlsIndex);
	if (!lpThreadData) return NULL;
	return lpThreadData->lpFtpUser;
}
