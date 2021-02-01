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

static HMODULE				hShell32;
static LPVOID				(WINAPI *MemoryLock)(HANDLE, DWORD);
static BOOL					(WINAPI *MemoryUnlock)(LPVOID);
static LPEXCHANGE_REQUEST	lpExchangeRequestList[2];
static CRITICAL_SECTION		csExchangeRequestList;


__inline static
BOOL FindExchangeRequest(LPEXCHANGE_REQUEST lpExchangeRequest)
{
	LPEXCHANGE_REQUEST	lpSeek;

	// Find request
	for (lpSeek = lpExchangeRequestList[HEAD];lpSeek;lpSeek = lpSeek->lpNext)
	{
		if (lpSeek == lpExchangeRequest) return TRUE;
	}
	return FALSE;
}


__inline static
LPDC_USERFILE_REQUEST FindUserFileRequest(LPEXCHANGE_REQUEST lpReq, LPUSERFILE lpUserFile)
{
	LPDC_USERFILE_REQUEST	lpSeek;

	// Find request
	for (lpSeek = lpReq->lpUserFileReqList[HEAD];lpSeek;lpSeek = lpSeek->lpNext)
	{
		if (&lpSeek->UserFile == lpUserFile) return lpSeek;
	}
	return NULL;
}




static
DWORD DataCopy_OnlineData(LPDC_ONLINEDATA lpdcOnlineData, LPVOID lpBase)
{
	LPCLIENT		lpClient;
	PONLINEDATA		lpOnlineData;
	LPTSTR			tszRealPath, tszRealDataPath;
	DWORD			dwReturn, dwTickCount, dwLastCount;

	if (lpdcOnlineData->iOffset-- < -1) return (DWORD)-1;
	//	Find next client
	for (;;)
	{
		if (++lpdcOnlineData->iOffset >= (INT32) dwMaxClientId) return (DWORD)-1;
		lpClient	= LockClient(lpdcOnlineData->iOffset);
		if (lpClient) break;
	}

	lpOnlineData	= &lpClient->Static;
	//	Duplicate data
	CopyMemory(&lpdcOnlineData->OnlineData, lpOnlineData, sizeof(ONLINEDATA));
	tszRealPath	= (LPTSTR)(lpOnlineData->dwRealPath ? AllocateShared(lpOnlineData->tszRealPath, NULL, 0) : NULL);
	tszRealDataPath	= (LPTSTR)(lpOnlineData->dwRealDataPath ? AllocateShared(lpOnlineData->tszRealDataPath, NULL, 0) : NULL);

	dwLastCount = lpClient->dwTransferLastUpdated;

	UnlockClient(lpdcOnlineData->iOffset);

	lpOnlineData	= &lpdcOnlineData->OnlineData;
	lpOnlineData->tszRealPath	= NULL;
	lpOnlineData->tszRealDataPath	= NULL;

	if (lpOnlineData->bTransferStatus)
	{
		dwTickCount = GetTickCount();
		dwTickCount = Time_DifferenceDW32(dwLastCount, dwTickCount);
		if (dwTickCount > ZERO_SPEED_DELAY)
		{
			lpOnlineData->dwIntervalLength = 1; // so bytes/time doesn't generate an error
			lpOnlineData->dwBytesTransfered = 0;
		}
	}


	dwReturn	= (lpOnlineData->dwRealPath +
		lpOnlineData->dwRealDataPath) * sizeof(TCHAR) + sizeof(ONLINEDATA) + sizeof(DC_MESSAGE);
	if (dwReturn < lpdcOnlineData->dwSharedMemorySize)
	{
		if (tszRealPath)
		{
			lpOnlineData->tszRealPath	= (LPTSTR)&lpdcOnlineData[1];
			CopyMemory(lpOnlineData->tszRealPath, tszRealPath, lpOnlineData->dwRealPath * sizeof(TCHAR));
			lpOnlineData->tszRealPath	= (LPTSTR)(sizeof(DC_ONLINEDATA) + (ULONG)lpBase);
		}
		if (tszRealDataPath)
		{
			lpOnlineData->tszRealDataPath	= &((LPTSTR)&lpdcOnlineData[1])[lpOnlineData->dwRealPath];
			CopyMemory(lpOnlineData->tszRealDataPath, tszRealDataPath, lpOnlineData->dwRealDataPath * sizeof(TCHAR));
			lpOnlineData->tszRealDataPath	= (LPTSTR)(sizeof(DC_ONLINEDATA) + lpOnlineData->dwRealPath * sizeof(TCHAR) + (ULONG)lpBase);
		}
		lpdcOnlineData->iOffset++;
		dwReturn	= 0;
	}
	if (tszRealPath) FreeShared(tszRealPath);
	if (tszRealDataPath) FreeShared(tszRealDataPath);

	return dwReturn;
}


VOID
UserFile_Old2New(LPUSERFILE_OLD lpOld, LPUSERFILE lpNew)
{
	lpNew->Uid = lpOld->Uid;
	lpNew->Gid = lpOld->Gid;
	memcpy(lpNew->Tagline, lpOld->Tagline, sizeof(lpOld->Tagline));
	memcpy(lpNew->MountFile, lpOld->MountFile, sizeof(lpOld->MountFile));
	memcpy(lpNew->Home, lpOld->Home, sizeof(lpOld->Home));
	memcpy(lpNew->Flags, lpOld->Flags, sizeof(lpOld->Flags));
	memcpy(lpNew->Limits, lpOld->Limits, sizeof(lpOld->Limits));
	memcpy(lpNew->Password, lpOld->Password, sizeof(lpOld->Password));

	memcpy(lpNew->Ratio, lpOld->Ratio, sizeof(lpOld->Ratio)); // 10 vs 25
	memcpy(lpNew->Credits, lpOld->Credits, sizeof(lpOld->Credits)); // 10 vs 25

	memcpy(lpNew->DayUp, lpOld->DayUp, sizeof(lpOld->DayUp)); // 10 vs 25
	memcpy(lpNew->DayDn, lpOld->DayDn, sizeof(lpOld->DayDn)); // 10 vs 25
	memcpy(lpNew->WkUp, lpOld->WkUp, sizeof(lpOld->WkUp)); // 10 vs 25
	memcpy(lpNew->WkDn, lpOld->WkDn, sizeof(lpOld->WkUp)); // 10 vs 25
	memcpy(lpNew->MonthUp, lpOld->MonthUp, sizeof(lpOld->MonthUp)); // 10 vs 25
	memcpy(lpNew->MonthDn, lpOld->MonthDn, sizeof(lpOld->MonthDn)); // 10 vs 25
	memcpy(lpNew->AllUp, lpOld->AllUp, sizeof(lpOld->AllUp)); // 10 vs 25
	memcpy(lpNew->AllDn, lpOld->AllDn, sizeof(lpOld->AllDn)); // 10 vs 25

	memcpy(lpNew->AdminGroups, lpOld->AdminGroups, sizeof(lpOld->AdminGroups));
	memcpy(lpNew->Groups, lpOld->Groups, sizeof(lpOld->AdminGroups));
	memcpy(lpNew->Ip, lpOld->Ip, sizeof(lpOld->Ip));

	lpNew->lpInternal = lpOld->lpInternal;
	lpNew->lpParent = lpOld->lpParent;

}


VOID
UserFile_New2Old(LPUSERFILE lpNew, LPUSERFILE_OLD lpOld)
{
	lpOld->Uid = lpNew->Uid;
	lpOld->Gid = lpNew->Gid;
	memcpy(lpOld->Tagline, lpNew->Tagline, sizeof(lpOld->Tagline));
	memcpy(lpOld->MountFile, lpNew->MountFile, sizeof(lpOld->MountFile));
	memcpy(lpOld->Home, lpNew->Home, sizeof(lpOld->Home));
	memcpy(lpOld->Flags, lpNew->Flags, sizeof(lpOld->Flags));
	memcpy(lpOld->Limits, lpNew->Limits, sizeof(lpOld->Limits));
	memcpy(lpOld->Password, lpNew->Password, sizeof(lpOld->Password));

	memcpy(lpOld->Ratio, lpNew->Ratio, sizeof(lpOld->Ratio)); // 10 vs 25
	memcpy(lpOld->Credits, lpNew->Credits, sizeof(lpOld->Credits)); // 10 vs 25

	memcpy(lpOld->DayUp, lpNew->DayUp, sizeof(lpOld->DayUp)); // 10 vs 25
	memcpy(lpOld->DayDn, lpNew->DayDn, sizeof(lpOld->DayDn)); // 10 vs 25
	memcpy(lpOld->WkUp, lpNew->WkUp, sizeof(lpOld->WkUp)); // 10 vs 25
	memcpy(lpOld->WkDn, lpNew->WkDn, sizeof(lpOld->WkUp)); // 10 vs 25
	memcpy(lpOld->MonthUp, lpNew->MonthUp, sizeof(lpOld->MonthUp)); // 10 vs 25
	memcpy(lpOld->MonthDn, lpNew->MonthDn, sizeof(lpOld->MonthDn)); // 10 vs 25
	memcpy(lpOld->AllUp, lpNew->AllUp, sizeof(lpOld->AllUp)); // 10 vs 25
	memcpy(lpOld->AllDn, lpNew->AllDn, sizeof(lpOld->AllDn)); // 10 vs 25

	memcpy(lpOld->AdminGroups, lpNew->AdminGroups, sizeof(lpOld->AdminGroups));
	memcpy(lpOld->Groups, lpNew->Groups, sizeof(lpOld->AdminGroups));
	memcpy(lpOld->Ip, lpNew->Ip, sizeof(lpOld->Ip));

	lpOld->lpInternal = lpNew->lpInternal;
	lpOld->lpParent = lpNew->lpParent;
}




static
DWORD DataCopy_Process(LPEXCHANGE_REQUEST lpRequest, LPDC_MESSAGE lpMessage)
{
	EVENT_COMMAND	Event;
	LPFILEINFO		lpFileInfo;
	VFSUPDATE		UpdateData;
	DWORD			dwReturn, dwFileName, Id;
	LPVOID			lpBuffer, lpContext;
	LPTSTR			tszUserName, tszGroupName, tszFileName;
	LPDC_USERFILE_REQUEST lpUserFileReq;
	LPUSERFILE      lpUserFile;

	//	Process request
	dwReturn	= (DWORD)-1;
	lpBuffer	= (LPVOID)((ULONG)lpMessage->lpContext - (ULONG)lpMessage->lpMemoryBase + (ULONG)lpMessage);

	switch (lpMessage->dwIdentifier)
	{
	case DC_EXECUTE:
		//	Execute script
		ZeroMemory(&Event, sizeof(EVENT_COMMAND));
		Event.tszCommand	= (LPTSTR)lpBuffer;
		dwReturn	= RunEvent(&Event);
		break;
	case DC_CREATE_USER:
		//	Create new user
		dwReturn	= CreateUser(((LPDC_NAMEID)lpBuffer)->tszName, -1);
		break;
	case DC_RENAME_USER:
		//	Rename user
		dwReturn	= RenameUser(((LPDC_RENAME)lpBuffer)->tszName, ((LPDC_RENAME)lpBuffer)->tszNewName);
		break;
	case DC_DELETE_USER:
		//	Delete existing user
		dwReturn	= DeleteUser(((LPDC_NAMEID)lpBuffer)->tszName);
		break;
	case DC_RENAME_GROUP:
		//	Rename group
		dwReturn	= RenameGroup(((LPDC_RENAME)lpBuffer)->tszName, ((LPDC_RENAME)lpBuffer)->tszNewName);
		break;
	case DC_CREATE_GROUP:
		//	Create new group
		dwReturn	= CreateGroup(((LPDC_NAMEID)lpBuffer)->tszName);
		break;
	case DC_DELETE_GROUP:
		//	Delete existing group
		dwReturn	= DeleteGroup(((LPDC_NAMEID)lpBuffer)->tszName);
		break;
	case DC_USER_TO_UID:
		//	Convert user name to id
		dwReturn	= User2Uid((LPTSTR)lpBuffer);
		break;
	case DC_GROUP_TO_GID:
		//	Convert group name to id
		dwReturn	= Group2Gid((LPTSTR)lpBuffer);
		break;
	case DC_UID_TO_USER:
		//	Convert user id to name
		Id	= ((LPDC_NAMEID)lpBuffer)->Id;
		if (Id < MAX_UID)
		{
			tszUserName	= Uid2User(Id);
			if (tszUserName)
			{
				_tcscpy(((LPDC_NAMEID)lpBuffer)->tszName, tszUserName);
				dwReturn	= FALSE;
			}
		}
		break;
	case DC_GID_TO_GROUP:
		//	Convert group id to name
		Id	= ((LPDC_NAMEID)lpBuffer)->Id;
		if (Id < MAX_GID)
		{
			tszGroupName	= Gid2Group(Id);
			if (tszGroupName)
			{
				//	Copy username to buffer
				_tcscpy(((LPDC_NAMEID)lpBuffer)->tszName, tszGroupName);
				dwReturn	= FALSE;
			}
		}
		break;
	case DC_NEW_USERFILE_OPEN:
		//	Open userfile
		if (((LPUSERFILE)lpBuffer)->Uid < 0 ||
			((LPUSERFILE)lpBuffer)->Uid >= MAX_UID) break;
		dwReturn	= UserFile_OpenPrimitive(((LPUSERFILE)lpBuffer)->Uid, (LPUSERFILE *)&lpBuffer, STATIC_SOURCE);
		break;
	case DC_NEW_USERFILE_LOCK:
		//	Lock userfile
		dwReturn	= UserFile_Lock((LPUSERFILE *)&lpBuffer, STATIC_SOURCE);
		break;
	case DC_NEW_USERFILE_UNLOCK:
		//	Unlock userfile
		dwReturn	= UserFile_Unlock((LPUSERFILE *)&lpBuffer, STATIC_SOURCE);
		break;
	case DC_NEW_USERFILE_CLOSE:
		//	Close userfile
		dwReturn	= UserFile_Close((LPUSERFILE *)&lpBuffer, STATIC_SOURCE);
		break;
	case DC_USERFILE_OPEN:
		//	Open userfile
		if (((LPUSERFILE_OLD)lpBuffer)->Uid < 0 ||
			((LPUSERFILE_OLD)lpBuffer)->Uid >= MAX_UID) break;
		lpUserFileReq = Allocate("DC_USERFILE_REQUEST", sizeof(DC_USERFILE_REQUEST));
		if (!lpUserFileReq)	break;

		// OK, this needs to use the OLD userfile structure...
		lpUserFile = &lpUserFileReq->UserFile;
		dwReturn   = UserFile_OpenPrimitive(((LPUSERFILE_OLD)lpBuffer)->Uid, &lpUserFile, STATIC_SOURCE);
		if (dwReturn) 
		{
			Free(lpUserFileReq);
			break;
		}

		APPENDLIST(lpUserFileReq, lpRequest->lpUserFileReqList);

		UserFile_New2Old(&lpUserFileReq->UserFile, lpBuffer);
		break;
	case DC_USERFILE_LOCK:
		//	Lock userfile
		lpUserFileReq = FindUserFileRequest(lpRequest, lpBuffer);

		if (!lpUserFileReq)
		{
			dwReturn = ERROR_USER_NOT_FOUND;
			break;
		}

		lpUserFile = &lpUserFileReq->UserFile;
		dwReturn   = UserFile_Lock(&lpUserFile, STATIC_SOURCE);
		break;
	case DC_USERFILE_UNLOCK:
		//	Unlock userfile
		lpUserFileReq = FindUserFileRequest(lpRequest, lpBuffer);

		if (!lpUserFileReq)
		{
			dwReturn = ERROR_USER_NOT_FOUND;
			break;
		}

		lpUserFile = &lpUserFileReq->UserFile;
		dwReturn   = UserFile_Unlock(&lpUserFile, STATIC_SOURCE);
		break;
	case DC_USERFILE_CLOSE:
		//	Close userfile
		lpUserFileReq = FindUserFileRequest(lpRequest, lpBuffer);

		if (!lpUserFileReq)
		{
			dwReturn = ERROR_USER_NOT_FOUND;
			break;
		}

		lpUserFile = &lpUserFileReq->UserFile;
		dwReturn   = UserFile_Close(&lpUserFile, STATIC_SOURCE);

		DELETELIST(lpUserFileReq, lpRequest->lpUserFileReqList);
		Free(lpUserFileReq);

		break;
	case DC_GROUPFILE_OPEN:
		//	Open groupfile
		if (((LPUSERFILE)lpBuffer)->Uid < 0 ||
			((LPUSERFILE)lpBuffer)->Uid >= MAX_GID) break;
		dwReturn	= GroupFile_OpenPrimitive(((LPGROUPFILE)lpBuffer)->Gid, (LPGROUPFILE *)&lpBuffer, STATIC_SOURCE);
		break;
	case DC_GROUPFILE_LOCK:
		//	Lock groupfile
		dwReturn	= GroupFile_Lock((LPGROUPFILE *)&lpBuffer, STATIC_SOURCE);
		break;
	case DC_GROUPFILE_UNLOCK:
		//	Unlock groupfile
		dwReturn	= GroupFile_Unlock((LPGROUPFILE *)&lpBuffer, STATIC_SOURCE);
		break;
	case DC_GROUPFILE_CLOSE:
		//	Close groupfile
		dwReturn	= GroupFile_Close((LPGROUPFILE *)&lpBuffer, STATIC_SOURCE);
		break;
	case DC_DIRECTORY_MARKDIRTY:
		//	Mark directory as dirty
		tszFileName	= (LPTSTR)lpBuffer;
		dwReturn	= MarkDirectory(tszFileName);
		break;
	case DC_FILEINFO_READ:
		//	Get fileinfo
		tszFileName	= (LPTSTR)((LPDC_VFS)lpBuffer)->pBuffer;
		lpContext	= (LPVOID)((LPDC_VFS)lpBuffer)->pBuffer;

		if (GetFileInfo(tszFileName, &lpFileInfo))
		{
			//	Copy fileinfo
			((LPDC_VFS)lpBuffer)->Uid	= lpFileInfo->Uid;
			((LPDC_VFS)lpBuffer)->Gid	= lpFileInfo->Gid;
			((LPDC_VFS)lpBuffer)->dwFileMode	= lpFileInfo->dwFileMode;

			dwReturn	= 0;
			//	Copy context
			if (lpFileInfo->dwFileAttributes & FILE_ATTRIBUTE_IOFTPD &&
				lpFileInfo->Context.dwData)
			{
				if (((LPDC_VFS)lpBuffer)->dwBuffer < lpFileInfo->Context.dwData)
				{
					((LPDC_VFS)lpBuffer)->dwBuffer	= lpFileInfo->Context.dwData;
					CopyMemory(lpContext, lpFileInfo->Context.lpData, lpFileInfo->Context.dwData);
				}
				else dwReturn	= lpFileInfo->Context.dwData;
			}
			else ((LPDC_VFS)lpBuffer)->dwBuffer	= 0;
			CloseFileInfo(lpFileInfo);
		}
		break;
	case DC_FILEINFO_WRITE:
		//	Get new data
		UpdateData.Uid	= ((LPDC_VFS)lpBuffer)->Uid;
		UpdateData.Gid	= ((LPDC_VFS)lpBuffer)->Gid;
		UpdateData.dwFileMode	= ((LPDC_VFS)lpBuffer)->dwFileMode;

		if (UpdateData.Uid >= 0 && UpdateData.Uid < MAX_UID &&
			UpdateData.Gid >= 0 && UpdateData.Gid < MAX_GID &&
			UpdateData.dwFileMode <= 0777)
		{
			tszFileName	= (LPTSTR)((LPDC_VFS)lpBuffer)->pBuffer;
			dwFileName	= _tcslen(tszFileName);
			UpdateData.Context.lpData	= (LPVOID)&((LPDC_VFS)lpBuffer)->pBuffer[(dwFileName + 1) * sizeof(TCHAR)];
			UpdateData.Context.dwData	= ((LPDC_VFS)lpBuffer)->dwBuffer - (dwFileName + 1) * sizeof(TCHAR);

			if (GetFileInfo(tszFileName, &lpFileInfo))
			{
				UpdateData.ftAlternateTime  = lpFileInfo->ftAlternateTime;
				UpdateData.dwUploadTimeInMs = lpFileInfo->dwUploadTimeInMs;
				CloseFileInfo(lpFileInfo);

				//	Update fileinfo
				if (UpdateFileInfo(tszFileName, &UpdateData)) dwReturn	= 0;
			}
		}
		break;
	case DC_GET_ONLINEDATA:
		dwReturn	= DataCopy_OnlineData((LPDC_ONLINEDATA)lpBuffer, lpMessage->lpContext);
		break;
	}
	return dwReturn;
}




static
BOOL DataCopy_Free(LPEXCHANGE_REQUEST lpRequest, BOOL bNoCheck)
{
	LPDC_USERFILE_REQUEST lpUserFileReq, lpNext;
	BOOL	bFree;

	if (! bNoCheck)
	{
		if (! lpRequest) return FALSE;

		bFree	= FALSE;
		//	Find message from list
		EnterCriticalSection(&csExchangeRequestList);
		if (FindExchangeRequest(lpRequest))
		{
			//	Delete request
			if (lpRequest->wStatus == ER_AVAILABLE)
			{
				DELETELIST(lpRequest, lpExchangeRequestList);
				bFree	= TRUE;
			}
			lpRequest->wStatus	= ER_REMOVED;
		}
		LeaveCriticalSection(&csExchangeRequestList);

		if (! bFree) return FALSE;
	}

	//	Free resources associated with request
	switch (lpRequest->wType)
	{
	case SHELL:
		MemoryUnlock(lpRequest->lpMessage);
		break;
	case FILEMAP:
		UnmapViewOfFile(lpRequest->lpMessage);
		break;
	}
	if (lpRequest->hEvent) CloseHandle(lpRequest->hEvent);
	if (lpRequest->hMemory != INVALID_HANDLE_VALUE) CloseHandle(lpRequest->hMemory);
	for(lpUserFileReq = lpRequest->lpUserFileReqList[HEAD] ; lpUserFileReq ; lpUserFileReq=lpNext)
	{
		lpNext = lpUserFileReq->lpNext;
		Free(lpUserFileReq);
	}
	Free(lpRequest);
	return TRUE;
}






static
LRESULT DataCopy_Allocate(DWORD dwProcessId, HANDLE hSharedMemory, DWORD dwType)
{
	LPDC_MESSAGE		lpMessage;
	LPEXCHANGE_REQUEST	lpRequest, lpGhost[2], lpSeek;
	HANDLE				hProcess;
	BOOL				bReturn;

	//	Allocate request object
	lpRequest	= (LPEXCHANGE_REQUEST)Allocate("DataExchange:Request", sizeof(EXCHANGE_REQUEST));
	if (! lpRequest) return 0;

	//	Open process
	hProcess	= OpenProcess(PROCESS_ALL_ACCESS, FALSE, dwProcessId);
	if (hProcess)
	{
		lpMessage	= NULL;
		lpRequest->hEvent	= NULL;
		lpRequest->hMemory	= INVALID_HANDLE_VALUE;
		//	Get access to shared allocation
		switch (dwType)
		{
		case SHELL:
			lpMessage = (LPDC_MESSAGE)MemoryLock(hSharedMemory, dwProcessId);
			if (lpMessage) bReturn	= TRUE;
			break;
		case FILEMAP:
			bReturn	= DuplicateHandle(hProcess, hSharedMemory,
				GetCurrentProcess(), &lpRequest->hMemory, 0, FALSE, DUPLICATE_SAME_ACCESS);
			if (bReturn) lpMessage	= MapViewOfFile(lpRequest->hMemory, FILE_MAP_ALL_ACCESS, 0, 0, 0);
			break;
		default:
			bReturn	= FALSE;
		}

		//	Duplicate event handle
		if (lpMessage &&
			(lpMessage->hEvent && lpMessage->hEvent != INVALID_HANDLE_VALUE))
		{
			bReturn	= DuplicateHandle(hProcess, lpMessage->hEvent,
				GetCurrentProcess(), &lpRequest->hEvent, 0, FALSE, DUPLICATE_SAME_ACCESS);
		}
		CloseHandle(hProcess);

		if (bReturn)
		{
			lpRequest->wStatus	= ER_AVAILABLE;
			lpRequest->wType	= (WORD)dwType;
			lpRequest->lpMessage	= lpMessage;
			lpRequest->dwTickCount	= GetTickCount();
			lpRequest->lpUserFileReqList[HEAD] = NULL;
			lpRequest->lpUserFileReqList[TAIL] = NULL;

			lpGhost[HEAD]	= NULL;
			lpGhost[TAIL]	= NULL;

			EnterCriticalSection(&csExchangeRequestList);
			//	Get ghost entries (allocated for longer than 10minutes)
			while (lpExchangeRequestList[HEAD] &&
				lpExchangeRequestList[HEAD]->wStatus == ER_AVAILABLE &&
				Time_DifferenceDW32(lpExchangeRequestList[HEAD]->dwTickCount, lpRequest->dwTickCount) > 600000)
			{
				if (! lpGhost[HEAD]) lpGhost[HEAD]	= lpExchangeRequestList[HEAD];
				lpGhost[TAIL]	= lpExchangeRequestList[HEAD];
				lpExchangeRequestList[HEAD]	= lpExchangeRequestList[HEAD]->lpNext;
				if (lpExchangeRequestList[HEAD]) lpExchangeRequestList[HEAD]->lpPrev	= NULL;
			}
			//	Add request to global list
			APPENDLIST(lpRequest, lpExchangeRequestList);
			LeaveCriticalSection(&csExchangeRequestList);

			//	Free ghost resources
			if (lpGhost[HEAD])
			{
				lpGhost[TAIL]->lpNext	= NULL;
				do
				{
					lpSeek	= lpGhost[HEAD]->lpNext;
					DataCopy_Free(lpGhost[HEAD], TRUE);

				} while (lpGhost[HEAD] = lpSeek);
			}

			return (LRESULT)lpRequest;
		}

		//	Free resources
		switch (dwType)
		{
		case SHELL:
			if (lpMessage) MemoryUnlock(lpMessage);
			break;
		case FILEMAP:
			if (lpMessage) UnmapViewOfFile(lpMessage);
			break;
		}
		if (lpRequest->hEvent) CloseHandle(lpRequest->hEvent);
		if (lpRequest->hMemory != INVALID_HANDLE_VALUE) CloseHandle(lpRequest->hMemory);
	}
	Free(lpRequest);
	return 0;
}




static
LRESULT WindowMessage_DataExchange(WPARAM wParam, LPARAM lParam)
{
	LPEXCHANGE_REQUEST	lpRequest;
	register DWORD		dwTickCount;
	BOOL				bFree;


	lpRequest	= (LPEXCHANGE_REQUEST)lParam;
	dwTickCount	= GetTickCount();

	//	Validate request, and update position & status
	EnterCriticalSection(&csExchangeRequestList);
	if (FindExchangeRequest(lpRequest) &&
		lpRequest->wStatus == ER_AVAILABLE)
	{
		DELETELIST(lpRequest, lpExchangeRequestList);
		APPENDLIST(lpRequest, lpExchangeRequestList);
		lpRequest->dwTickCount	= dwTickCount;
		lpRequest->wStatus	= ER_IN_USE;
	}
	else lpRequest	= NULL;
	LeaveCriticalSection(&csExchangeRequestList);

	if (lpRequest)
	{
		//	Process request and signal event
		lpRequest->lpMessage->dwReturn	= DataCopy_Process(lpRequest, lpRequest->lpMessage);

		bFree	= FALSE;
		dwTickCount	= GetTickCount();
		//	Update status and position
		EnterCriticalSection(&csExchangeRequestList);
		DELETELIST(lpRequest, lpExchangeRequestList);
		if (lpRequest->wStatus == ER_IN_USE)
		{
			APPENDLIST(lpRequest, lpExchangeRequestList);
			lpRequest->dwTickCount	= dwTickCount;
			if (lpRequest->hEvent) SetEvent(lpRequest->hEvent);
		}
		else bFree	= TRUE;
		lpRequest->wStatus	= ER_AVAILABLE;
		LeaveCriticalSection(&csExchangeRequestList);

		if (bFree) DataCopy_Free(lpRequest, TRUE);
	}
	return FALSE;
}




static
LRESULT WindowMessage_ProcessId(WPARAM wParam, LPARAM lParam)
{
	return (LRESULT)GetCurrentProcessId();
}

static
LRESULT WindowMessage_ProcessHandle(WPARAM wParam, LPARAM lParam)
{
	return (LRESULT)GetCurrentProcess();
}

static
LRESULT WindowMessage_FreeMemory(WPARAM wParam, LPARAM lParam)
{
	return DataCopy_Free((LPEXCHANGE_REQUEST)lParam, FALSE);
}

static
LRESULT WindowMessage_ShellAlloc(WPARAM wParam, LPARAM lParam)
{
	if (! MemoryLock || ! MemoryUnlock) return 0;
	return DataCopy_Allocate((DWORD)wParam, (HANDLE)lParam, SHELL);
}

static
LRESULT WindowMessage_FileMap(WPARAM wParam, LPARAM lParam)
{
	return DataCopy_Allocate((DWORD)wParam, (HANDLE)lParam, FILEMAP);
}

static
LRESULT WindowMessage_KillUser(WPARAM wParam, LPARAM lParam)
{
	return KillUser(wParam);
}

static
LRESULT WindowMessage_KickUser(WPARAM wParam, LPARAM lParam)
{
	return KickUser(lParam);
}









BOOL DataCopy_Init(BOOL bFirstInitialization)
{
	if (! bFirstInitialization) return TRUE;
	//	Install message handlers
	InstallMessageHandler(WM_PID, WindowMessage_ProcessId, TRUE, FALSE);
	InstallMessageHandler(WM_PHANDLE, WindowMessage_ProcessHandle, TRUE, FALSE);
	InstallMessageHandler(WM_DATACOPY_FREE, WindowMessage_FreeMemory, TRUE, FALSE);
	InstallMessageHandler(WM_DATACOPY_SHELLALLOC, WindowMessage_ShellAlloc, FALSE, FALSE);
	InstallMessageHandler(WM_DATACOPY_FILEMAP, WindowMessage_FileMap, FALSE, FALSE);
	InstallMessageHandler(WM_SHMEM, WindowMessage_DataExchange, FALSE, FALSE);
	InstallMessageHandler(WM_KICK, WindowMessage_KickUser, FALSE, FALSE);
	InstallMessageHandler(WM_KILL, WindowMessage_KillUser, FALSE, FALSE);

	//	Load shell library
	hShell32	= LoadLibrary(_TEXT("shell32.dll"));
	lpExchangeRequestList[HEAD]	= NULL;
	lpExchangeRequestList[TAIL]	= NULL;

	//	Initialize shared memory routines
	if (hShell32)
	{
		MemoryLock	= (LPVOID (WINAPI *)(HANDLE, DWORD))GetProcAddress(hShell32, _TEXT("SHLockShared"));
		if (! MemoryLock) MemoryLock	= (LPVOID (WINAPI *)(HANDLE, DWORD))GetProcAddress(hShell32, (LPCSTR)521);
		MemoryUnlock	= (BOOL (WINAPI *)(LPVOID))GetProcAddress(hShell32, _TEXT("SHUnlockShared"));
		if (! MemoryUnlock) MemoryUnlock	= (BOOL (WINAPI *)(LPVOID))GetProcAddress(hShell32, (LPCSTR)522);
	}
	return InitializeCriticalSectionAndSpinCount(&csExchangeRequestList, 100);;
}


VOID DataCopy_DeInit(VOID)
{
	LPEXCHANGE_REQUEST	lpSeek, lpNext;

	// Find request
	for (lpSeek = lpExchangeRequestList[HEAD];lpSeek;lpSeek = lpNext)
	{
		lpNext = lpSeek->lpNext;
		DataCopy_Free(lpSeek, TRUE);
	}

	if (hShell32) FreeLibrary(hShell32);
	DeleteCriticalSection(&csExchangeRequestList);
}
