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
#include <openssl/engine.h>
#include <openssl/conf.h>


LPFN_GETACCEPTEXSOCKADDRS GetAcceptSockAddrs;
LPFN_ACCEPTEX             Accept;
BOOL volatile             bLogOpenSslErrors;
DWORD                     dwDeadlockPort;
SOCKET                    DeadlockSocket;


static CRITICAL_SECTION          csSelectList;
static volatile PSELECT          pSelectList;
static WSADATA                   wsaData;
static volatile LPIODEVICE       lpSchedulerDeviceList;
static DWORD volatile            dwSchedulerUpdateSpeed;
static DWORD volatile            dwSchedulerWakeUp;
static LONG volatile             lSchedulerDeviceList;
static LONG volatile             lIdentifier;

#define SELECT_CONTINUE 0001
#define SELECT_SET      0002
#define SELECT_REMOVED  0010


BOOL
WSAAsyncSelectRemove(PSELECT pSelect)
{
	if (pSelect->dwFlags & SELECT_REMOVED)
	{
		return FALSE;
	}
	//  Remove from list
	if (pSelect->pNext) pSelect->pNext->pPrevious  = pSelect->pPrevious;
	if (pSelect->pPrevious) pSelect->pPrevious->pNext  = pSelect->pNext;
	if (pSelect == pSelectList) pSelectList  = pSelect->pNext;
	pSelect->dwFlags  |= SELECT_REMOVED;
	return TRUE;
}



DWORD
WSAAsyncSelectTimerProc(LPIOSOCKET lpIoSocket,
                        LPTIMER lpTimer)
{
	PSELECT pSelect;
	BOOL    bQueueJob;
	DWORD   dwError;

	bQueueJob  = FALSE;

	EnterCriticalSection(&lpIoSocket->csLock);
	EnterCriticalSection(&csSelectList);

	if (pSelect = lpIoSocket->lpSelectEvent)
	{
		if (WSAAsyncSelectRemove(pSelect) && (lpIoSocket->Socket != INVALID_SOCKET))
		{
			// this will remove the event from being generated unless it has already been triggered...
			if (WSAAsyncSelect(lpIoSocket->Socket, GetMainWindow(), 0, 0))
			{
				dwError = WSAGetLastError();
				Putlog(LOG_DEBUG, _T("WSAAsyncSelectTimerProc error: %d\r\n"), dwError);
			}
		}

		bQueueJob  = (pSelect->dwFlags & SELECT_CONTINUE);
		if (bQueueJob)
		{
			lpIoSocket->lpSelectEvent = NULL;
		}
		pSelect->dwResult  = WSAETIMEDOUT;
		pSelect->dwFlags  |= SELECT_SET;
	}
	LeaveCriticalSection(&csSelectList);
	LeaveCriticalSection(&lpIoSocket->csLock);

    //  Queue job
	if (bQueueJob)
	{
		pSelect->lpResult[0]  = pSelect->dwResult;
		QueueJob(pSelect->lpProc, pSelect->lpContext, JOB_PRIORITY_NORMAL);
		Free(pSelect);
		return INFINITE;
	}

	return 0;
}


BOOL
WSAAsyncSelectWithTimeout(LPIOSOCKET lpIoSocket,
                          DWORD dwTimeOut,
                          DWORD dwFlags,
                          LPDWORD lpResult)
{
	PSELECT    pSelect, pTest;
	BOOL    bSelect;
	DWORD   dwError;

	bSelect  = TRUE;
	//  Allocate memory for object
	pSelect  = (PSELECT)Allocate("Socket:AsyncSelect", sizeof(SELECT));
	if (! pSelect) return TRUE;

	//  Initialize structure
	pSelect->lpResult   = lpResult;
	pSelect->lpIoSocket = lpIoSocket;
	pSelect->dwResult   = NO_ERROR;
	pSelect->dwFlags    = SELECT_REMOVED;
	//  Create timer
	pSelect->lpTimer  = StartIoTimer(NULL, WSAAsyncSelectTimerProc, lpIoSocket, dwTimeOut);

	EnterCriticalSection(&lpIoSocket->csLock);
	EnterCriticalSection(&csSelectList);

	lpIoSocket->lpSelectEvent = pSelect;

	//  Check if timer already has run
	if (! (pSelect->dwFlags & SELECT_SET))
	{
		pSelect->dwFlags  = 0;

		for (pTest = pSelectList ; pTest ; pTest = pTest->pNext)
		{
			if (pTest->lpIoSocket && (pTest->lpIoSocket->Socket == lpIoSocket->Socket))
			{
				Putlog(LOG_DEBUG, "WSAAsyncSelectWithTimeout socket re-use: %d - Flags: %d\r\n", lpIoSocket->Socket, pTest->dwFlags);
				WSAAsyncSelectRemove(pTest);
				break;
			}
		}

		//  Add item to list
		if (pSelectList) pSelectList->pPrevious  = pSelect;
		pSelect->pNext    = pSelectList;
		pSelect->pPrevious  = NULL;
		pSelectList      = pSelect;
	}
	else bSelect  = FALSE;
	LeaveCriticalSection(&csSelectList);

	//  Call async select
	if (bSelect)
	{
		if (WSAAsyncSelect(lpIoSocket->Socket, GetMainWindow(), WM_ASYNC_CALLBACK, dwFlags))
		{
			dwError = WSAGetLastError();
			Putlog(LOG_DEBUG, _T("WSAAsyncSelect error: %d\r\n"), dwError);
		}
	}
	else
	{
		Putlog(LOG_DEBUG, _T("WSA select timer fired immediately.\r\n"));
	}

	LeaveCriticalSection(&lpIoSocket->csLock);
	return FALSE;
}


BOOL
WSAAsyncSelectCancel(LPIOSOCKET lpIoSocket)
{
	PSELECT pSelect;
	DWORD   dwError;

	// lpSelectEvent can be NULL in which case we do nothing
	if (!lpIoSocket->lpSelectEvent)
	{
		return FALSE;
	}

	EnterCriticalSection(&lpIoSocket->csLock);
	EnterCriticalSection(&csSelectList);

	//  Remove item from list
	pSelect = lpIoSocket->lpSelectEvent;

	if (pSelect)
	{
		if (WSAAsyncSelectRemove(pSelect) && (lpIoSocket->Socket != INVALID_SOCKET))
		{
			// this will remove the event from being generated unless it has already been triggered...
			if (WSAAsyncSelect(lpIoSocket->Socket, GetMainWindow(), 0, 0))
			{
				dwError = WSAGetLastError();
				Putlog(LOG_DEBUG, _T("WSAAsyncSelectCancel error: %d\r\n"), dwError);
			}
		}
		lpIoSocket->lpSelectEvent = NULL;
	}

	LeaveCriticalSection(&csSelectList);
	LeaveCriticalSection(&lpIoSocket->csLock);

	if (pSelect)
	{
		//  Free resources
		StopIoTimer(pSelect->lpTimer, FALSE);
		Free(pSelect);
		return TRUE;
	}

	return FALSE;
}


BOOL
WSAAsyncSelectContinue(LPIOSOCKET lpIoSocket,
                       LPVOID lpProc,
                       LPVOID lpContext)
{
	PSELECT  pSelect;
	BOOL  bQueueJob;

	EnterCriticalSection(&lpIoSocket->csLock);
	EnterCriticalSection(&csSelectList);

	if (!(pSelect = lpIoSocket->lpSelectEvent))
	{
		LeaveCriticalSection(&csSelectList);
		LeaveCriticalSection(&lpIoSocket->csLock);
		return TRUE;
	}

	//  Copy information
	pSelect->lpProc     = lpProc;
	pSelect->lpContext  = lpContext;

	if ((bQueueJob = (pSelect->dwFlags & SELECT_SET)))
	{
		//  Remove item from list since event has already been signaled...
		WSAAsyncSelectRemove(pSelect);
		lpIoSocket->lpSelectEvent = NULL;
	}
	else pSelect->dwFlags  |= SELECT_CONTINUE;

	LeaveCriticalSection(&csSelectList);
	LeaveCriticalSection(&lpIoSocket->csLock);

	//  Queue job
	if (bQueueJob)
	{
		StopIoTimer(pSelect->lpTimer, FALSE);
		pSelect->lpResult[0]  = pSelect->dwResult;
		QueueJob(lpProc, lpContext, JOB_PRIORITY_NORMAL);
		Free(pSelect);
	}
	return FALSE;
}




LRESULT
AsyncSelectProc(WPARAM wParam,
                LPARAM lParam)
{
	PSELECT    pSelect;
	SOCKET    Socket;
	BOOL    bQueueJob;
	DWORD   dwError, dwEvent;
	DWORD   dwTry;

	dwError = WSAGETSELECTERROR(lParam);
	dwEvent = WSAGETSELECTEVENT(lParam);

	bQueueJob  = FALSE;
	Socket     = INVALID_SOCKET;

	EnterCriticalSection(&csSelectList);
	//  Event message received
	for (pSelect = pSelectList ; pSelect ; pSelect = pSelect->pNext)
	{
		if (pSelect->lpIoSocket->Socket != wParam) continue;
		if (pSelect->lpIoSocket->Socket == -1)
		{
			Putlog(LOG_DEBUG, _T("AsyncSelectProc: closed socket found.\r\n"));
			continue;
		}
		//  Lock item if we can, but give up if we can't get it quicky...
		dwTry = 5;
		while (dwTry)
		{
			if (TryEnterCriticalSection(&pSelect->lpIoSocket->csLock)) break;
			Sleep(10);
			dwTry--;
		}
		if (dwTry)
		{
			// we got the lock...
			// double check now that we hold the lock...
			if ((pSelect->lpIoSocket->Socket != wParam) || (pSelect != pSelect->lpIoSocket->lpSelectEvent))
			{
				LeaveCriticalSection(&pSelect->lpIoSocket->csLock);
				Putlog(LOG_DEBUG, _T("AsyncSelectProc: Data mismatch.\r\n"));
				continue;
			}
		
			//  Remove object from list
			WSAAsyncSelectRemove(pSelect);
			//  Update structure
			pSelect->dwResult  = WSAGETSELECTERROR(lParam);
			pSelect->dwFlags  |= SELECT_SET;
			if (bQueueJob = (pSelect->dwFlags & SELECT_CONTINUE))
			{
				pSelect->lpIoSocket->lpSelectEvent = NULL;
			}
			LeaveCriticalSection(&pSelect->lpIoSocket->csLock);
			break;
		}
		else
		{
			Putlog(LOG_DEBUG, _T("AsyncSelectProc: Failed locking socket: %d\r\n"), wParam);
		}
	}
	LeaveCriticalSection(&csSelectList);
	if (!pSelect)
	{
		Putlog(LOG_DEBUG, _T("AsyncSelectProc: Socket %d not found.\r\n"), wParam);
	}
	//  Queue job
	if (bQueueJob)
	{
		StopIoTimer(pSelect->lpTimer, FALSE);
		pSelect->lpResult[0]  = pSelect->dwResult;
		QueueJob(pSelect->lpProc, pSelect->lpContext, JOB_PRIORITY_NORMAL);
		Free(pSelect);
	}
	return FALSE;
}



BOOL
CloseSocket(LPIOSOCKET lpIoSocket,
            BOOL bNoLinger)
{
	if (!lpIoSocket->bInitialized)
	{
		Putlog(LOG_ERROR, _T("Uninitialized socket used!\r\n"));
		return TRUE;
	}
	
	EnterCriticalSection(&lpIoSocket->csLock);
	if (lpIoSocket->lpSelectEvent)
	{
		WSAAsyncSelectCancel(lpIoSocket);
	}
	if (lpIoSocket->Socket != INVALID_SOCKET)
	{
		closesocket(lpIoSocket->Socket);
		lpIoSocket->Socket = INVALID_SOCKET;
		LeaveCriticalSection(&lpIoSocket->csLock);
		return FALSE;
	}
	LeaveCriticalSection(&lpIoSocket->csLock);
	return TRUE;
}




BOOL
ioCloseSocket(LPIOSOCKET lpIoSocket, BOOL bNoLinger)
{
  LPSECURITY    lpSecure;
  BOOL          bReturn;

  //  Close socket if it was initialized
  bReturn = CloseSocket(lpIoSocket, bNoLinger);
  UnbindSocket(lpIoSocket);
  //  Free linebuffer
  if (lpIoSocket->lpLineBuffer)
  {
	  if (lpIoSocket->lpLineBuffer->lpTransfer)
	  {
		  Free(lpIoSocket->lpLineBuffer->lpTransfer);
	  }
	  Free(lpIoSocket->lpLineBuffer);
	  lpIoSocket->lpLineBuffer  = NULL;
  }

  if (lpIoSocket->SQ.lpTransfer)
  {
	  Free(lpIoSocket->SQ.lpTransfer);
	  lpIoSocket->SQ.lpTransfer = NULL;
  }

  if (lpIoSocket->SQ.hEvent && lpIoSocket->SQ.hEvent != INVALID_HANDLE_VALUE)
  {
	  CloseHandle(lpIoSocket->SQ.hEvent);
	  lpIoSocket->SQ.hEvent = INVALID_HANDLE_VALUE;
  }

  ZeroMemory(&lpIoSocket->Options, sizeof(SOCKET_OPTIONS));
  ZeroMemory(&lpIoSocket->dwBandwidthLimit, sizeof(lpIoSocket->dwBandwidthLimit));

  //  Process secure socket
  if ((lpSecure = lpIoSocket->lpSecure))
  {
	  DeleteCriticalSection(&lpSecure->csLock);
	  if (lpSecure->SSL)
	  {
		  SSL_free(lpSecure->SSL);         // implicitly frees InternalBio
		  BIO_free(lpSecure->NetworkBio);
	  }
	  Free(lpSecure);
	  lpIoSocket->lpSecure  = NULL;
  }

  return bReturn;
}



BOOL
ioDeleteSocket(LPIOSOCKET lpIoSocket, BOOL bNoLinger)
{
	if (lpIoSocket->bInitialized)
	{
		ioCloseSocket(lpIoSocket, bNoLinger);
		DeleteCriticalSection(&lpIoSocket->csLock);
		lpIoSocket->bInitialized = FALSE;
	}
	return FALSE;
}




BOOL
SendQueuedIO(LPSOCKETOVERLAPPED lpOverlapped)
{
  LPIOSOCKET  lpSocket;
  DWORD    dwBytesSent;

  lpSocket  = lpOverlapped->hSocket;
  //  Send data
  if ((WSASend(lpSocket->Socket, &lpOverlapped->Buffer, 1, &dwBytesSent, 0, (LPWSAOVERLAPPED)lpOverlapped, NULL) == SOCKET_ERROR) &&
	  (WSAGetLastError() != WSA_IO_PENDING))
  {
	  return TRUE;
  }
  return FALSE;
}


BOOL
ReceiveQueuedIO(LPSOCKETOVERLAPPED lpOverlapped)
{
  LPIOSOCKET  lpSocket;
  DWORD    dwFlags, dwBytesReceived;

  lpSocket  = lpOverlapped->hSocket;
  dwFlags    = 0;
  //  Send data
  if ((WSARecv(lpSocket->Socket, &lpOverlapped->Buffer, 1, &dwBytesReceived, &dwFlags, (LPWSAOVERLAPPED)lpOverlapped, NULL) == SOCKET_ERROR) &&
	  (WSAGetLastError() != WSA_IO_PENDING))
  {
	  return TRUE;
  }
  return FALSE;
}



// Returns TRUE if an error occurred, else FALSE
BOOL
SendOverlapped(LPIOSOCKET lpSocket, LPSOCKETOVERLAPPED lpOverlapped)
{
	LPIODEVICE  lpDevice;
	LPBANDWIDTH  lpBandwidth;
	DWORD    dwBytesSent, dwResult, dwError;
	LONG     lIdent;

	if (dwSchedulerUpdateSpeed &&
		(lpDevice = lpSocket->lpDevice) &&  
		(lpDevice->Outbound.bGlobalBandwidthLimit || lpSocket->Options.dwSendLimit))
	{
		lpBandwidth  = &lpDevice->Outbound;
		if (lpOverlapped->Buffer.len > 1024)
		{
			lpOverlapped->Buffer.len = 1024;
		}

		while (InterlockedExchange(&lpBandwidth->lLock, TRUE)) SwitchToThread();
		//  Check available bandwidth on device
		if (! lpBandwidth->dwGlobalBandwidthLeft)
		{
			//  Push item to queue
			if (! lpBandwidth->lpIOQueue[0][HEAD])
			{
				lpBandwidth->lpIOQueue[0][HEAD]  = lpOverlapped;
			}
			else lpBandwidth->lpIOQueue[0][TAIL]->lpNext  = lpOverlapped;
			lpBandwidth->lpIOQueue[0][TAIL]  = lpOverlapped;
			lpBandwidth->dwIOQueue[0]++;
			InterlockedExchange(&lpBandwidth->lLock, FALSE);
			return FALSE;
		}
		//  Check available bandwidth for user
		if (lpSocket->Options.dwSendLimit &&
			! lpSocket->dwBandwidthLimit[0]--)
		{
			if (! lpBandwidth->lpIOQueue[1][HEAD])
			{
				lpBandwidth->lpIOQueue[1][HEAD]  = lpOverlapped;
			}
			else lpBandwidth->lpIOQueue[1][TAIL]->lpNext  = lpOverlapped;
			lpBandwidth->lpIOQueue[1][TAIL]  = lpOverlapped;
			lpBandwidth->dwIOQueue[1]++;
			lpSocket->dwBandwidthLimit[0]  = lpSocket->Options.dwSendLimit - 1;
			InterlockedExchange(&lpBandwidth->lLock, FALSE);
			return FALSE;
		}
		lpBandwidth->dwGlobalBandwidthLeft--;
		InterlockedExchange(&lpBandwidth->lLock, FALSE);
	}

	lpOverlapped->Internal       = 0;
	lpOverlapped->InternalHigh   = 0;
	lpOverlapped->Offset         = 0;
	lpOverlapped->OffsetHigh     = 0;
	// hEvent is never set and thus is zero anyway

	lIdent = InterlockedIncrement(&lIdentifier);
	if (InterlockedExchange(&lpOverlapped->lIdentifier, lIdent) != 0)
	{
		Putlog(LOG_ERROR, "Detected overlapped re-use (send).\r\n");
	}

	dwBytesSent = 0;
	//  Send data
	EnterCriticalSection(&lpSocket->csLock);
	if (lpSocket->Socket == INVALID_SOCKET)
	{
		LeaveCriticalSection(&lpSocket->csLock);
		SetLastError(ERROR_CLOSED_SOCKET);
		return TRUE;
	}
	dwResult = WSASend(lpSocket->Socket, &lpOverlapped->Buffer, 1, &dwBytesSent, 0, (LPWSAOVERLAPPED)lpOverlapped, NULL);
	LeaveCriticalSection(&lpSocket->csLock);
	if (dwResult == SOCKET_ERROR)
	{
		dwError = WSAGetLastError();
		if (dwError == WSA_IO_PENDING)
		{
			// Notification will be via overlapped callback
			return FALSE;
		}
		return TRUE;
	}
	return FALSE;
}





// Returns TRUE if an error occurred, else FALSE
BOOL
ReceiveOverlapped(LPIOSOCKET lpSocket,
                  LPSOCKETOVERLAPPED lpOverlapped)
{
	LPIODEVICE  lpDevice;
	LPBANDWIDTH  lpBandwidth;
	DWORD    dwFlags, dwBytesReceived, dwResult, dwError;
	LONG     lIdent;

	if (dwSchedulerUpdateSpeed &&
		(lpDevice = lpSocket->lpDevice) &&
		(lpDevice->Inbound.bGlobalBandwidthLimit || lpSocket->Options.dwReceiveLimit))
	{
		lpBandwidth  = &lpDevice->Inbound;
		//  Calculate maximum receive amount
		if (lpOverlapped->Buffer.len > 1024)
		{
			lpOverlapped->Buffer.len = 1024;
		}

		while (InterlockedExchange(&lpBandwidth->lLock, TRUE)) SwitchToThread();
		//  Check available bandwidth on device
		if (! lpBandwidth->dwGlobalBandwidthLeft)
		{
			//  Push item to queue
			if (! lpBandwidth->lpIOQueue[0][HEAD])
			{
				lpBandwidth->lpIOQueue[0][HEAD]  = lpOverlapped;
			}
			else lpBandwidth->lpIOQueue[0][TAIL]->lpNext  = lpOverlapped;
			lpBandwidth->lpIOQueue[0][TAIL]  = lpOverlapped;
			lpBandwidth->dwIOQueue[0]++;
			InterlockedExchange(&lpBandwidth->lLock, FALSE);
			return FALSE;
		}
		//  Check available bandwidth for user
		if (lpSocket->Options.dwReceiveLimit &&
			! lpSocket->dwBandwidthLimit[1]--)
		{
			//  Push item to queue
			if (! lpBandwidth->lpIOQueue[1][HEAD])
			{
				lpBandwidth->lpIOQueue[1][HEAD]  = lpOverlapped;
			}
			else lpBandwidth->lpIOQueue[1][TAIL]->lpNext  = lpOverlapped;
			lpBandwidth->lpIOQueue[1][TAIL]  = lpOverlapped;
			lpBandwidth->dwIOQueue[1]++;
			lpSocket->dwBandwidthLimit[1]  = lpSocket->Options.dwReceiveLimit - 1;

			InterlockedExchange(&lpBandwidth->lLock, FALSE);
			return FALSE;
		}
		lpBandwidth->dwGlobalBandwidthLeft--;
		InterlockedExchange(&lpBandwidth->lLock, FALSE);
	}

	lpOverlapped->Internal       = 0;
	lpOverlapped->InternalHigh   = 0;
	lpOverlapped->Offset         = 0;
	lpOverlapped->OffsetHigh     = 0;
	// hEvent is never set and thus is zero anyway

	dwFlags         = 0;
	dwBytesReceived = 0;

	if (lpOverlapped->Buffer.len > 65536)
	{
		lpOverlapped->Buffer.len = 65536;
	}

	lIdent = InterlockedIncrement(&lIdentifier);
	if (InterlockedExchange(&lpOverlapped->lIdentifier, lIdent) != 0)
	{
		Putlog(LOG_ERROR, "Detected overlapped re-use (recv).\r\n");
	}

	//  Receive data
	EnterCriticalSection(&lpSocket->csLock);
	if (lpSocket->Socket == INVALID_SOCKET)
	{
		LeaveCriticalSection(&lpSocket->csLock);
		SetLastError(ERROR_CLOSED_SOCKET);
		return TRUE;
	}
	dwResult = WSARecv(lpSocket->Socket, &lpOverlapped->Buffer, 1, &dwBytesReceived, &dwFlags, (LPWSAOVERLAPPED)lpOverlapped, NULL);
	LeaveCriticalSection(&lpSocket->csLock);
	if (dwResult == SOCKET_ERROR)
	{
		dwError = WSAGetLastError();
		if (dwError == WSA_IO_PENDING)
		{
			// Notification will be via overlapped callback
			return FALSE;
		}
		return TRUE;
	}
	return FALSE;
}




BOOL BindSocket(SOCKET Socket, ULONG lAddress, USHORT sPort, BOOL bReuse)
{
  struct sockaddr_in  SockAddr;
  INT          iReturn;
  //  Initialize structure
  SockAddr.sin_port      = htons(sPort);
  SockAddr.sin_addr.s_addr  = lAddress;
  SockAddr.sin_family      = AF_INET;
  ZeroMemory(&SockAddr.sin_zero, sizeof(SockAddr.sin_zero));
  //  Reuse address
  if (bReuse)
  {
	  bReuse  = TRUE;
	  setsockopt(Socket, SOL_SOCKET, SO_REUSEADDR, (LPSTR)&bReuse, sizeof(BOOL));
  }
  //  Bind socket
  iReturn  = bind(Socket, (struct sockaddr *)&SockAddr, sizeof(struct sockaddr_in));

  return (iReturn != SOCKET_ERROR ? FALSE : TRUE);
}




ULONG HostToAddress(LPSTR szHostName)
{
  struct hostent  *lpHostEnt;
  ULONG      lAddress;
  //  Sanity check
  if (! szHostName) return INADDR_NONE;
  //  Try ipv4 conversion
  if ((lAddress = inet_addr(szHostName)) != INADDR_NONE) return lAddress;
  //  Try by hostname
  if (lpHostEnt = gethostbyname(szHostName)) lAddress = ((PULONG)*lpHostEnt->h_addr_list)[0];
  return lAddress;

}






BOOL SetSocketOption(LPIOSOCKET lpSocket, INT iLevel, INT iOptionName, LPVOID lpValue, INT iValue)
{
  if (iLevel != IO_SOCKET)
  {
    return setsockopt(lpSocket->Socket, iLevel, iOptionName, (LPCSTR)lpValue, iValue);
  }

  switch (iOptionName)
  {
  case RECEIVE_LIMIT:
    //  Receive limit, kb/sec
    if (((LPDWORD)lpValue)[0])
    {
      lpSocket->Options.dwReceiveLimit = ((LPDWORD)lpValue)[0];
      lpSocket->dwBandwidthLimit[1]  =
        max(2, (DWORD)(((LPDWORD)lpValue)[0] / 1000. * min(Time_DifferenceDW32(GetTickCount(), dwSchedulerWakeUp), 1000)));
    }
    break;
  case SEND_LIMIT:
    //  Send limit, kb/sec
    if (((LPDWORD)lpValue)[0])
    {
      lpSocket->Options.dwSendLimit = ((LPDWORD)lpValue)[0];
      lpSocket->dwBandwidthLimit[0]  =
        max(2, (DWORD)(((LPDWORD)lpValue)[0] / 1000. * min(Time_DifferenceDW32(GetTickCount(), dwSchedulerWakeUp), 1000)));
    }
    break;
  case SOCKET_PRIORITY:
    //  Socket priority
    lpSocket->Options.dwPriority  = ((LPDWORD)lpValue)[0];
    break;
  default:
    return SOCKET_ERROR;
  }
  return FALSE;
}




VOID RegisterSchedulerDevice(LPIODEVICE lpDevice)
{
  while (InterlockedExchange(&lSchedulerDeviceList, TRUE)) SwitchToThread();
  //  Push new device to scheduler device list
  if (lpSchedulerDeviceList) lpSchedulerDeviceList->lpPrevSDevice  = lpSchedulerDeviceList;
  lpDevice->lpNextSDevice  = lpSchedulerDeviceList;
  lpSchedulerDeviceList  = lpDevice;
  lpDevice->lpPrevSDevice  = NULL;
  InterlockedExchange(&lSchedulerDeviceList, FALSE);
}





VOID UnregisterSchedulerDevice(LPIODEVICE lpDevice)
{
  while (InterlockedExchange(&lSchedulerDeviceList, TRUE)) SwitchToThread();
  //  Unregister device from scheduler
  if (lpDevice->lpNextSDevice) lpDevice->lpNextSDevice->lpPrevSDevice  = lpDevice->lpPrevSDevice;
  if (lpDevice->lpPrevSDevice)
  {
    lpDevice->lpPrevSDevice->lpNextSDevice  = lpDevice->lpNextSDevice;
  }
  else lpSchedulerDeviceList  = lpDevice->lpNextSDevice;
  InterlockedExchange(&lSchedulerDeviceList, FALSE);
}






UINT WINAPI SocketSchedulerThread(LPVOID lpNull)
{
  LPSOCKETOVERLAPPED  lpQueue[2][2], lpOverlapped;
  LPBANDWIDTH      lpBandwidth;
  LPIODEVICE      lpDevice;
  BOOL        bGetAll;
  DWORD        dwQueue[2];
  DWORD        dwUpStream, dwDownStream, dwSleep, dwLoops, dwNextWakeUp;

  for (dwLoops = 1;;)
  {
    dwNextWakeUp  = GetTickCount() + (1000 / dwSchedulerUpdateSpeed);
    if ((bGetAll = (dwLoops++ % dwSchedulerUpdateSpeed ? FALSE : TRUE))) dwSchedulerWakeUp  = dwNextWakeUp;

    while (InterlockedExchange(&lSchedulerDeviceList, TRUE)) SwitchToThread();
    //  Go through all devices
    for (lpDevice = lpSchedulerDeviceList;lpDevice;lpDevice = lpDevice->lpNextSDevice)
    {
      lpBandwidth  = &lpDevice->Outbound;
      dwUpStream  = lpBandwidth->dwGlobalBandwidthLimit / dwSchedulerUpdateSpeed;
      dwQueue[0]  = lpBandwidth->dwIOQueue[2];
      lpQueue[0][HEAD]  = lpBandwidth->lpIOQueue[2][HEAD];
      lpQueue[0][TAIL]  = lpBandwidth->lpIOQueue[2][TAIL];

      while (InterlockedExchange(&lpBandwidth->lLock, TRUE)) SwitchToThread();
      //  Get primary upstream queue
      if (lpQueue[0][HEAD])
      {
        lpQueue[0][TAIL]->lpNext  = lpBandwidth->lpIOQueue[0][HEAD];
      }
      else lpQueue[0][HEAD]  = lpBandwidth->lpIOQueue[0][HEAD];
      lpQueue[0][TAIL]  = lpBandwidth->lpIOQueue[0][TAIL];
      dwQueue[0]  += lpBandwidth->dwIOQueue[0];

      lpBandwidth->lpIOQueue[0][HEAD]  = NULL;
      lpBandwidth->dwIOQueue[0]    = 0;
      dwUpStream  += (lpBandwidth->dwGlobalBandwidthLeft > dwUpStream / 2 ? dwUpStream : lpBandwidth->dwGlobalBandwidthLeft) / 2;

      //  Get secondary queue (once per sec)
      if (bGetAll)
      {
        if (lpBandwidth->lpIOQueue[1][HEAD])
        {
          if (lpQueue[0][HEAD])
          {
            lpQueue[0][TAIL]->lpNext  = lpBandwidth->lpIOQueue[1][HEAD];
          }
          else lpQueue[0][HEAD]  = lpBandwidth->lpIOQueue[1][HEAD];
          lpQueue[0][TAIL]  = lpBandwidth->lpIOQueue[1][TAIL];
          dwQueue[0]  += lpBandwidth->dwIOQueue[1];
        }
        lpBandwidth->lpIOQueue[1][HEAD]  = NULL;
        lpBandwidth->dwIOQueue[1]    = 0;
        dwUpStream  += (lpBandwidth->dwGlobalBandwidthLimit % dwSchedulerUpdateSpeed);
      }
      //  Set upstream limit
      lpBandwidth->dwGlobalBandwidthLeft  = (dwQueue[0] >= dwUpStream ? 0 : dwUpStream - dwQueue[0]);
      InterlockedExchange(&lpBandwidth->lLock, FALSE);


      lpBandwidth    = &lpDevice->Inbound;
      dwDownStream  = lpBandwidth->dwGlobalBandwidthLimit / dwSchedulerUpdateSpeed;
      dwQueue[1]    = lpBandwidth->dwIOQueue[2];
      lpQueue[1][HEAD]  = lpBandwidth->lpIOQueue[2][HEAD];
      lpQueue[1][TAIL]  = lpBandwidth->lpIOQueue[2][TAIL];
      while (InterlockedExchange(&lpBandwidth->lLock, TRUE)) SwitchToThread();
      //  Get primary downstream queue
      if (lpQueue[1][HEAD])
      {
        lpQueue[1][TAIL]->lpNext  = lpBandwidth->lpIOQueue[0][HEAD];
      }
      else lpQueue[1][HEAD]  = lpBandwidth->lpIOQueue[0][HEAD];
      lpQueue[1][TAIL]  = lpBandwidth->lpIOQueue[0][TAIL];
      dwQueue[1]  += lpBandwidth->dwIOQueue[0];

      lpBandwidth->lpIOQueue[0][HEAD]  = NULL;
      lpBandwidth->dwIOQueue[0]    = 0;
      dwDownStream  += (lpBandwidth->dwGlobalBandwidthLeft > dwDownStream / 2 ? dwDownStream : lpBandwidth->dwGlobalBandwidthLeft) / 2;

      //  Get secondary queue (once per sec)
      if (bGetAll)
      {
        if (lpBandwidth->lpIOQueue[1][HEAD])
        {
          if (lpQueue[1][HEAD])
          {
            lpQueue[1][TAIL]->lpNext  = lpBandwidth->lpIOQueue[1][HEAD];
          }
          else lpQueue[1][HEAD]  = lpBandwidth->lpIOQueue[1][HEAD];
          lpQueue[1][TAIL]  = lpBandwidth->lpIOQueue[1][TAIL];
          dwQueue[1]  += lpBandwidth->dwIOQueue[1];
        }
        lpBandwidth->lpIOQueue[1][HEAD]  = NULL;
        lpBandwidth->dwIOQueue[1]    = 0;
        dwDownStream  += (lpBandwidth->dwGlobalBandwidthLimit % dwSchedulerUpdateSpeed);
      }

      //  Set downstream limit
      lpBandwidth->dwGlobalBandwidthLeft  = (dwQueue[1] >= dwDownStream ? 0 : dwDownStream - dwQueue[1]);
      InterlockedExchange(&lpBandwidth->lLock, FALSE);

      dwUpStream    = min(dwUpStream, dwQueue[0]);
      dwDownStream  = min(dwDownStream, dwQueue[1]);
      if (! (dwQueue[0] -= dwUpStream) &&
        dwUpStream) lpQueue[0][TAIL]->lpNext  = NULL;
      if (! (dwQueue[1] -= dwDownStream) &&
        dwDownStream) lpQueue[1][TAIL]->lpNext  = NULL;

      //  Release upstream queues
      for (;dwUpStream--;)
      {
        lpQueue[0][HEAD]  = (lpOverlapped = lpQueue[0][HEAD])->lpNext;
        //  Post continue notification to io thread
        PostQueuedCompletionStatus(hCompletionPort, 0, (DWORD)-2, (LPWSAOVERLAPPED)lpOverlapped);
      }
      //  Release downstream queues
      for (;dwDownStream--;)
      {
        lpQueue[1][HEAD]  = (lpOverlapped = lpQueue[1][HEAD])->lpNext;
        //  Post continue notification to io thread
        PostQueuedCompletionStatus(hCompletionPort, 0, (DWORD)-3, (LPWSAOVERLAPPED)lpOverlapped);
      }

      //  Add remaining queues for device
      lpDevice->Outbound.lpIOQueue[2][HEAD]  = lpQueue[0][HEAD];
      lpDevice->Outbound.lpIOQueue[2][TAIL]  = lpQueue[0][TAIL];
      lpDevice->Outbound.dwIOQueue[2]      = dwQueue[0];
      lpDevice->Inbound.lpIOQueue[2][HEAD]  = lpQueue[1][HEAD];
      lpDevice->Inbound.lpIOQueue[2][TAIL]  = lpQueue[1][TAIL];
      lpDevice->Inbound.dwIOQueue[2]      = dwQueue[1];
    }
    InterlockedExchange(&lSchedulerDeviceList, FALSE);

    //  Sleep
    if ((dwSleep = Time_DifferenceDW32(GetTickCount(), dwNextWakeUp)) <= (1000 / dwSchedulerUpdateSpeed))
    {
      Sleep(dwSleep);
    }
  }
  ExitThread(0);
}


SOCKET
OpenSocket()
{
	DWORD dwError;
	SOCKET s;
	struct linger  Linger;

	AcquireHandleLock();
	s = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, 0, 0, WSA_FLAG_OVERLAPPED);

	if (s == INVALID_SOCKET)
	{
		Putlog(LOG_ERROR, _T("Unable to create socket.\r\n"));
	}
	else if (!SetHandleInformation((HANDLE)s, HANDLE_FLAG_INHERIT, 0))
	{
		// a debug point
		dwError = GetLastError();
	}

	ReleaseHandleLock();

	if (s != INVALID_SOCKET)
	{
		Linger.l_onoff  = 1;
		Linger.l_linger  = 0;
		//  Set for hard close
		setsockopt(s, SOL_SOCKET, SO_LINGER, (PCHAR)&Linger, sizeof(struct linger));
	}


	return s;
}


VOID
IoSocketInit(LPIOSOCKET lpSocket)
{
	if (!lpSocket->bInitialized)
	{
		InitializeCriticalSectionAndSpinCount(&lpSocket->csLock, 1000);
		lpSocket->bInitialized = TRUE;
	}
	lpSocket->Overlapped[0].hSocket = lpSocket;
	lpSocket->Overlapped[1].hSocket = lpSocket;
	lpSocket->Overlapped[0].lpProc = TransmitPackage_ReadSocket;
	lpSocket->Overlapped[1].lpProc = TransmitPackage_WriteSocket;
}


#if 0
BOOL
ioAsyncCallbackTest()
{
	struct sockaddr_in  addrLocal;
	SOCKET  Socket;
	DWORD   dwError;

	addrLocal.sin_addr.s_addr == 0x0100007f;
	addrLocal.sin_port = 0;

	Socket = OpenSocket();

	if (Socket == INVALID_SOCKET)
	{
		Putlog(LOG_DEBUG, _T("ioAsyncCallbackTest: Invalid socket returned.\r\n"));
		goto failure;
	}

	if (bind(Socket, (struct sockaddr *) &addrLocal, sizeof(addrLocal)))
	{
		// rut ro, that didn't work...
		dwError = WSAGetLastError();
		Putlog(LOG_DEBUG, _T("ioAsyncCallbackTest: Bind failed: %d.\r\n"), dwError);
		goto failure;
	}

#if 0
	// do we care what local port we got?
	iAddrLen = sizeof(boundAddr);
	if (getsockname(Socket, (struct sockaddr *) &boundAddr, &iAddrLen))
	{
		goto failure;
	}
#endif

	// need to reset this each time...
	if (WSAAsyncSelect(Socket, GetMainWindow(), WM_ASYNC_TEST, dwFlags))
	{
		dwError = WSAGetLastError();
		Putlog(LOG_DEBUG, _T("WSAAsyncSelect error: %d\r\n"), dwError);
	}



	if (WSAConnect(Socket, (struct sockaddr *) &lpService->addrService, sizeof(lpService->addrService), NULL, NULL, NULL, NULL))
	{
		// rut ro, it failed...
		dwError = WSAGetLastError();
		Putlog(LOG_DEBUG, _T("Services_Test: Connect failed for service '%s' IP=%s: %d.\r\n"), lpService->tszName, inet_ntoa(lpService->addrLocal.sin_addr), dwError);
		goto failure;
	}

	// we think we connected, just drop connection...
	closesocket(Socket);

	if (bFailed)
	{
		// we never actually connected last time, just got accepted and lost somewhere...
		goto failure;
	}

	iOnline++;
	lpService->dwTestCounter = 0;
	continue;

failure:
	if (Socket != INVALID_SOCKET)
	{
		// if things have gone really bad and we're locked up this might hang, but then we'd be declared unresponsive...
		closesocket(Socket);
	}
	if (++lpService->dwTestCounter > 2)
	{
		// twice in a row... that's bad... record failure!
		iFailed++;
		if (lpService->dwTestCounter == 3)
		{
			Putlog(LOG_ERROR, _T("Services_Test: Failed to connect to service '%s' (IP=%s) %d times in a row!\r\n"),
				lpService->tszName, inet_ntoa(lpService->addrLocal.sin_addr), lpService->dwTestCounter);
		}
		Putlog(LOG_DEBUG, _T("Services_Test: Failed to connect to service '%s' (IP=%s) %d times in a row!\r\n"),
			lpService->tszName, inet_ntoa(lpService->addrLocal.sin_addr), lpService->dwTestCounter);
		ReleaseSharedLock(&loServiceList);
	}
}
#endif



BOOL
Socket_Init(BOOL bFirstInitialization)
{
  GUID          GuidAcceptEx  = WSAID_ACCEPTEX;
  GUID          GuidGetAcceptExSockAddrs  = WSAID_GETACCEPTEXSOCKADDRS;
  OSVERSIONINFO      VersionInfo;
  SOCKET          Socket;
  DWORD          dwThreadID, dwBytes, dwLastError;
  HANDLE          hThread;
  LPTSTR          tszSchedulerUpdateSpeed;
  BOOL            bLogErrors;

  bLogErrors = FALSE;
  Config_Get_Bool(&IniConfigFile, _TEXT("Network"), _TEXT("Log_OpenSSL_Transfer_Errors"), &bLogErrors);
  bLogOpenSslErrors = bLogErrors;

  if (! bFirstInitialization) return TRUE;

  //  Reset variables
  Accept  = NULL;
  GetAcceptSockAddrs  = NULL;
  pSelectList  = NULL;
  lpSchedulerDeviceList  = NULL;
  lSchedulerDeviceList  = FALSE;
  dwSchedulerUpdateSpeed  = 10;

  //  Get windows version
  VersionInfo.dwOSVersionInfoSize  = sizeof(OSVERSIONINFO);
  if (! GetVersionEx(&VersionInfo)) return FALSE;

  if ((tszSchedulerUpdateSpeed = Config_Get(&IniConfigFile, _TEXT("Network"), _TEXT("Scheduler_Update_Speed"), NULL, NULL)))
  {
    if (! _tcsnicmp(tszSchedulerUpdateSpeed, _TEXT("High"), 4))
    {
      dwSchedulerUpdateSpeed  = 20;
    }
    else if (! _tcsnicmp(tszSchedulerUpdateSpeed, _TEXT("Low"), 3))
    {
      dwSchedulerUpdateSpeed  = 4;
    }
    else if (! _tcsnicmp(tszSchedulerUpdateSpeed, _TEXT("Disabled"), 8))
    {
      dwSchedulerUpdateSpeed  = 0;
    }
    Free(tszSchedulerUpdateSpeed);
  }

  //  Initialize WinSock
  if (WSAStartup(MAKEWORD(2,2), &wsaData)) return FALSE;

  //  Install event select handler
  if (! InitializeCriticalSectionAndSpinCount(&csSelectList, 50) ||
    ! InstallMessageHandler(WM_ASYNC_CALLBACK, AsyncSelectProc, TRUE, TRUE)) return FALSE;

  if (dwSchedulerUpdateSpeed)
  {
    //  Create socket scheduler
    hThread = CreateThread(NULL, 0, SocketSchedulerThread, NULL, 0, &dwThreadID);  
    if (hThread == INVALID_HANDLE_VALUE) return FALSE;
    //  Raise thread priority
    SetThreadPriority(hThread, THREAD_PRIORITY_HIGHEST);
    CloseHandle(hThread);
  }


  //  Get AcceptEx & GetAcceptExSockAddrs address's
  Socket=WSASocket(AF_INET,
                   SOCK_STREAM,
                   IPPROTO_TCP,
                   0,
                   0,
                   WSA_FLAG_OVERLAPPED);

  if(WSAIoctl(Socket,
              SIO_GET_EXTENSION_FUNCTION_POINTER,
              &GuidAcceptEx,
              sizeof(GuidAcceptEx),
              &Accept,
              sizeof(Accept),
              &dwBytes,
              NULL,
              NULL) == SOCKET_ERROR
     || WSAIoctl(Socket,
                 SIO_GET_EXTENSION_FUNCTION_POINTER,
                 &GuidGetAcceptExSockAddrs,
                 sizeof(GuidGetAcceptExSockAddrs),
                 &GetAcceptSockAddrs,
                 sizeof(GetAcceptSockAddrs),
                 &dwBytes,
                 NULL,
                 NULL) == SOCKET_ERROR)
  {
    dwLastError  = GetLastError();
  }
  else
  {
    dwLastError  = NO_ERROR;
  }
  closesocket(Socket);

  if (dwLastError != NO_ERROR) ERROR_RETURN(dwLastError, FALSE);

#if 0
  dwDeadlockPort = 0;
  DeadlockSocket = INVALID_SOCKET;;
  if (!Config_Get_Int(&IniConfigFile, _TEXT("Network"), _TEXT("Deadlock_Port"), (PINT)&dwDeadlockPort) && dwDeadlockPort)
  {
	  DeadlockSocket = OpenSocket();
	  if (DeadlockSocket == INVALID_SOCKET)
	  {
		  dwLastError = WSAGetLastError();
		  ERROR_RETURN(dwLastError, FALSE);
	  }
	  //  Listen socket
	  if (listen(DeadlockSocket, SOMAXCONN))
	  {
		  dwLastError = WSAGetLastError();
		  closesocket(DeadlockSocket);
		  ERROR_RETURN(dwLastError, FALSE);
	  }
  }
#endif
  return TRUE;
}


VOID Socket_DeInit(VOID)
{
	DWORD dwError;

	//  Free resources
	if (WSACleanup())
	{
		dwError = WSAGetLastError();
		Putlog(LOG_ERROR, "WSACleanup reported error #%d\r\n", dwError);
	}
	DeleteCriticalSection(&csSelectList);
}
