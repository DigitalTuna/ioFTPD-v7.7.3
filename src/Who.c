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




VOID HoursMinutesSeconds(LPDWORD lpHours, LPDWORD lpMinutes, LPDWORD lpSeconds)
{
	DWORD	dwSeconds;

	dwSeconds	= lpSeconds[0];
	lpHours[0]	= dwSeconds / 3600;
	lpMinutes[0]	= (dwSeconds % 3600) / 60;
	lpSeconds[0]	= (dwSeconds % 3600) % 60;
}




DWORD ClientToIoWho(LPCLIENT lpClient, DWORD dwTimeNow, DWORD dwTickCount, LPIO_WHO lpWho)
{
	PONLINEDATA    lpOnlineData;

	lpOnlineData = &lpWho->OnlineData;

	//  Copy data
	CopyMemory(lpOnlineData, &lpClient->Static, sizeof(*lpOnlineData));

	// increment ref count so these stay valid after we release client lock
	lpOnlineData->tszRealPath     = (lpOnlineData->dwRealPath ? AllocateShared(lpOnlineData->tszRealPath, NULL, 0) : NULL);
	lpOnlineData->tszRealDataPath = (lpOnlineData->dwRealDataPath ? AllocateShared(lpOnlineData->tszRealDataPath, NULL, 0) : NULL);

	lpWho->dwLoginSeconds = dwTimeNow - lpOnlineData->dwOnlineTime;
	lpWho->dwIdleSeconds	 = Time_DifferenceDW32(lpOnlineData->dwIdleTickCount, dwTickCount) / 1000;

	HoursMinutesSeconds(&lpWho->dwLoginHours, &lpWho->dwLoginMinutes, &lpWho->dwLoginSeconds);
	HoursMinutesSeconds(&lpWho->dwIdleHours,  &lpWho->dwIdleMinutes,  &lpWho->dwIdleSeconds);

	lpWho->dwUsers++;
	lpWho->i64FileSize = 0;

	//	Load userfile
	if (lpOnlineData->Uid == -1 || UserFile_OpenPrimitive(lpOnlineData->Uid, &lpWho->lpUserFile, 0))
	{
		lpWho->lpUserFile  = NULL;
	}

	//	Copy transfer information
	if (lpOnlineData->bTransferStatus)
	{
		if (lpOnlineData->dwIntervalLength)
		{
			lpWho->fTransferSpeed = lpOnlineData->dwBytesTransfered * 0.9765625 / lpOnlineData->dwIntervalLength;
		}
		else lpWho->fTransferSpeed	= 0.;

		switch (lpOnlineData->bTransferStatus)
		{
		case 1:
			lpWho->dwDownloads++;
			lpWho->fTotalDnSpeed	+= lpWho->fTransferSpeed;
			return W_DOWNLOAD;
		case 2:
			lpWho->dwUploads++;
			lpWho->fTotalUpSpeed	+= lpWho->fTransferSpeed;
			return W_UPLOAD;
		}
		return W_LIST;
	}
	lpWho->fTransferSpeed	= 0.;
	return (lpWho->lpUserFile ? W_IDLE : W_LOGIN);
}







LPTSTR Admin_UsersOnline(LPFTPUSER lpUser, LPTSTR tszMultilinePrefix, LPIO_STRING Args)
{
	IO_WHO			Who;
	LPCLIENT        lpClient;
	LPBUFFER		lpBuffer;
	PBYTE			pBuffer[6];
	DWORD			dwFileName, dwSystemTime, dwTickCount, dwStatus, n, dwHidden;
	INT				i, iLimit;
	TCHAR			*tpCheck;
	LPTSTR			tszBasePath, tszFileName, tszStatus, tszClientId;
	LPUSERSEARCH    lpSearch;


	if (GetStringItems(Args) > 2) ERROR_RETURN(ERROR_INVALID_ARGUMENTS, GetStringRange(Args, 2, STR_END));

	ZeroMemory(&Who, sizeof(IO_WHO));
	Who.dwMyCID = lpUser->Connection.dwUniqueId;
	Who.dwMyUID = lpUser->UserFile->Uid;

	lpBuffer = &lpUser->CommandChannel.Out;

	dwSystemTime	= (DWORD) time((time_t*)NULL);
	dwTickCount     = GetTickCount();

	if (GetStringItems(Args) == 2)
	{
		tszClientId = GetStringIndexStatic(Args, 1);

		i	= _tcstol(tszClientId, &tpCheck, 10);

		if ((tpCheck != tszClientId) && (tpCheck[0] == 0))
		{
			// it was a number all by itself
			if (i < 0 || i >= MAX_CLIENTS)
			{
				// but not a valid one...
				ERROR_RETURN(ERROR_INVALID_ARGUMENTS, tszClientId);
			}

			//	Get client data
			lpClient = LockClient(i);
			if (lpClient)
			{
				Who.dwConnectionId	= i;

				switch (ClientToIoWho(lpClient, dwSystemTime, dwTickCount, &Who))
				{
				case W_LIST:
					tszStatus	= _TEXT("List");
					break;
				case W_UPLOAD:
					tszStatus	= _TEXT("Upload");
					break;
				case W_DOWNLOAD:
					tszStatus	= _TEXT("Download");
					break;
				case W_IDLE:
					tszStatus	= _TEXT("Idle");
					break;
				case W_LOGIN:
					tszStatus	= _TEXT("Login");
					break;
				}
				UnlockClient(i);

				Who.dwMyCID = lpUser->Connection.dwUniqueId;
				Who.dwMyUID = lpUser->UserFile->Uid;

				//	Show message file
				tszBasePath	= Service_MessageLocation(lpUser->Connection.lpService);
				if (tszBasePath)
				{
					dwFileName	= aswprintf(&tszFileName, _TEXT("%s\\ClientInfo.%s"), tszBasePath, tszStatus);
					FreeShared(tszBasePath);

					if (dwFileName)
					{
						MessageFile_Show(tszFileName, lpBuffer, &Who, DT_WHO, tszMultilinePrefix, NULL);
						Free(tszFileName);
					}
				}
				FreeShared(Who.OnlineData.tszRealPath);
				FreeShared(Who.OnlineData.tszRealDataPath);
				UserFile_Close(&Who.lpUserFile, 0);
				return NULL;
			}
			SetLastError(ERROR_USER_NOT_FOUND);
			return GetStringIndexStatic(Args, 1);
		}
	}

	iLimit = -1;
	lpSearch = NULL;
	n = 1;
	if (GetStringItems(Args) > 1)
	{
		//	Get arguments
		tszClientId = GetStringIndexStatic(Args, 1);

		if (!_tcsicmp(tszClientId, _T("up")))
		{
			iLimit = W_UPLOAD;
			n++;
		}
		else if (!_tcsicmp(tszClientId, _T("down")))
		{
			iLimit = W_DOWNLOAD;
			n++;
		}
		else if (!_tcsicmp(tszClientId, _T("idle")))
		{
			iLimit = W_IDLE;
			n++;
		}
		else if (!_tcsicmp(tszClientId, _T("bw")))
		{
			iLimit = W_NONE;
			n++;
		}

		tszClientId = GetStringRange(Args, n, STR_END);
		if (tszClientId && *tszClientId)
		{
			// this should be a site admin only command so no need to pass lpUserFile to check for limited info...
			lpSearch = FindParse(tszClientId, NULL, lpUser, TRUE);
			if (!lpSearch)
			{
				return tszClientId;
			}
		}
	}

	//	Load messages
	tszBasePath	= Service_MessageLocation(lpUser->Connection.lpService);
	if (! tszBasePath)
	{
		if (lpSearch) FindFinished(lpSearch);
		ERROR_RETURN(ERROR_COMMAND_FAILED, GetStringIndexStatic(Args, 1));
	}
	dwFileName	= aswprintf(&tszFileName, _TEXT("%s\\ClientList.Download"), tszBasePath);
	FreeShared(tszBasePath);
	if (! dwFileName)
	{
		if (lpSearch) FindFinished(lpSearch);
		ERROR_RETURN(ERROR_NOT_ENOUGH_MEMORY, GetStringIndexStatic(Args, 1));
	}

	pBuffer[W_DOWNLOAD]	= Message_Load(tszFileName);
	_tcscpy(&tszFileName[dwFileName - 8], _TEXT("Upload"));
	pBuffer[W_UPLOAD]	= Message_Load(tszFileName);
	_tcscpy(&tszFileName[dwFileName - 8], _TEXT("Idle"));
	pBuffer[W_IDLE]	= Message_Load(tszFileName);
	_tcscpy(&tszFileName[dwFileName - 8], _TEXT("List"));
	pBuffer[W_LIST]	= Message_Load(tszFileName);
	_tcscpy(&tszFileName[dwFileName - 8], _TEXT("Login"));
	pBuffer[W_LOGIN]	= Message_Load(tszFileName);
	pBuffer[W_NONE]     = 0;


	//	Show header
	_tcscpy(&tszFileName[dwFileName - 8], _TEXT("Header"));
	MessageFile_Show(tszFileName, lpBuffer, lpUser, DT_FTPUSER, tszMultilinePrefix, NULL);

	dwHidden = 0;


	//	List users online
	for ( Who.dwConnectionId = 0 ; Who.dwConnectionId <= dwMaxClientId ; Who.dwConnectionId++ )
	{
		lpClient  = LockClient(Who.dwConnectionId);
		if (!lpClient) continue;

		dwStatus = ClientToIoWho(lpClient, dwSystemTime, dwTickCount, &Who);

		UnlockClient(Who.dwConnectionId);

		if ((iLimit != -1) && (dwStatus != iLimit))
		{
			// don't print non-matching entries
			dwHidden++;
		}
		else if (!pBuffer[dwStatus])
		{
			// no template to display user...
			dwHidden++;
		}
		else if (!lpSearch || (Who.lpUserFile && !FindIsMatch(lpSearch, Who.lpUserFile, TRUE)))
		{
			//	Show message
			Message_Compile(pBuffer[dwStatus], lpBuffer, FALSE, &Who, DT_WHO, tszMultilinePrefix, NULL);
		}
		else
		{
			dwHidden++;
		}

		//	Free resources
		FreeShared(Who.OnlineData.tszRealPath);
		FreeShared(Who.OnlineData.tszRealDataPath);
		UserFile_Close(&Who.lpUserFile, 0);
	}

	Who.dwLoginHours = dwHidden;

	//	Show footer
	_tcscpy(&tszFileName[dwFileName - 8], _TEXT("Footer"));
	MessageFile_Show(tszFileName, lpBuffer, &Who, DT_WHO, tszMultilinePrefix, NULL);

	if (lpSearch) FindFinished(lpSearch);
	if (pBuffer[W_UPLOAD])   Free(pBuffer[W_UPLOAD]);
	if (pBuffer[W_DOWNLOAD]) Free(pBuffer[W_DOWNLOAD]);
	if (pBuffer[W_IDLE])     Free(pBuffer[W_IDLE]);
	if (pBuffer[W_LIST])     Free(pBuffer[W_LIST]);
	if (pBuffer[W_LOGIN])    Free(pBuffer[W_LOGIN]);
	Free(tszFileName);
	return NULL;
}


INT __cdecl WhoSortCmp(VOID *pBuffer, LPCVOID Who1, LPCVOID Who2)
{
	LPSTR Array = (LPSTR) pBuffer;
	LPSORT_WHO w1 = (LPSORT_WHO) Who1;
	LPSORT_WHO w2 = (LPSORT_WHO) Who2;

	return _tcsicmp(&Array[w1->dwNameIndex], &Array[w2->dwNameIndex]);
}



LPTSTR Admin_Who(LPFTPUSER lpUser, LPTSTR tszMultilinePrefix, LPIO_STRING Args)
{
	IO_WHO			Who;
	LPCLIENT        lpClient;
	DWORD			dwSystemTime;
	LPBUFFER		lpBuffer;
	PBYTE			pBuffer[6];
	DWORD			dwFileName, dwTickCount, dwStatus, n, dwPaths, dwHidden, dwError, dwSorted, dwMaxSorted;
	INT				iOffset, ActionLimit;
	LPTSTR			tszBasePath, tszFileName, tszSearch, tszExclude, tszSpace, tszSlash, tszName;
	TCHAR           tszPaths[20][_INI_LINE_LENGTH+1], tszHidden[_INI_LINE_LENGTH+1];
	LPUSERSEARCH    lpSearch, lpExclude;
	BOOL            bIsAdmin, bHidden, bSorting;
	BUFFER          TempBuf;
	LPSORT_WHO      lpSortWhoArray, lpNext, lpSortWhoTemp;

	ZeroMemory(&Who, sizeof(IO_WHO));
	lpBuffer = &lpUser->CommandChannel.Out;

	//	Load messages
	tszBasePath	= Service_MessageLocation(lpUser->Connection.lpService);
	if (! tszBasePath) return NULL;
	dwFileName	= aswprintf(&tszFileName, _TEXT("%s\\Who.Download"), tszBasePath);
	FreeShared(tszBasePath);
	if (! dwFileName) return NULL;
	dwError = NO_ERROR;

	bSorting = FALSE;
	ZeroMemory(&TempBuf, sizeof(TempBuf));
	lpSortWhoArray = NULL;

	pBuffer[W_DOWNLOAD]	= Message_Load(tszFileName);
	_tcscpy(&tszFileName[dwFileName - 8], _TEXT("Upload"));
	pBuffer[W_UPLOAD]	= Message_Load(tszFileName);
	_tcscpy(&tszFileName[dwFileName - 8], _TEXT("Idle"));
	pBuffer[W_IDLE]	= Message_Load(tszFileName);
	_tcscpy(&tszFileName[dwFileName - 8], _TEXT("List"));
	pBuffer[W_LIST]	= Message_Load(tszFileName);
	_tcscpy(&tszFileName[dwFileName - 8], _TEXT("Login"));
	pBuffer[W_LOGIN]	= Message_Load(tszFileName);
	pBuffer[W_NONE]     = 0;

	if (tszExclude = Config_Get(&IniConfigFile, _TEXT("FTP"), _TEXT("Who_Hidden_Users"), NULL, NULL))
	{
		lpExclude = FindParse(tszExclude, NULL, NULL, FALSE);
		Free(tszExclude);
	}
	else
	{
		lpExclude = NULL;
	}

	bIsAdmin = !HasFlag(lpUser->UserFile, _T("M1"));

	iOffset = 0;
	dwPaths = 0;
	for (n = 1 ; n <= 20 ; n++)
	{
		_stprintf_s(tszHidden, sizeof(tszHidden)/sizeof(*tszHidden), "Who_Hidden_Paths_%d", n);

		if (!Config_Get(&IniConfigFile, _T("FTP"), tszHidden, tszPaths[dwPaths], &iOffset))
		{
			break;
		}
		dwPaths++;
	}

	ActionLimit = W_ANY;
	if (GetStringItems(Args) < 2)
	{
		lpSearch = NULL;
		tszSearch = NULL;
	}
	else
	{
		n = 1;
		tszSearch = GetStringIndexStatic(Args, 1);

		if (!_tcsicmp(tszSearch, _T("up")))
		{
			ActionLimit = W_UPLOAD;
			n++;
		}
		else if (!_tcsicmp(tszSearch, _T("down")))
		{
			ActionLimit = W_DOWNLOAD;
			n++;
		}
		else if (!_tcsicmp(tszSearch, _T("idle")))
		{
			ActionLimit = W_IDLE;
			n++;
		}
		else if (!_tcsicmp(tszSearch, _T("bw")))
		{
			ActionLimit = W_NONE;
			n++;
		}

		tszSearch = GetStringRange(Args, n, STR_END);
		if (!tszSearch || !*tszSearch)
		{
			lpSearch = NULL;
		}
		else
		{
			lpSearch = FindParse(tszSearch, lpUser->UserFile, lpUser, FALSE);
			if (!lpSearch)
			{
				dwError = GetLastError();
				goto cleanup;
			}
		}
	}

	if (bSorting = FtpSettings.bWhoSortOutput)
	{
		// might be lengthy so don't start out small...
		AllocateBuffer(&TempBuf, 8192);
		dwSorted    = 0;
		dwMaxSorted = 200;
		lpSortWhoArray = Allocate("SortWhoArray", dwMaxSorted * sizeof(*lpSortWhoArray));
		if (!lpSortWhoArray)
		{
			dwError = ERROR_OUTOFMEMORY;
			goto cleanup;
		}
	}


	//	Show header
	_tcscpy(&tszFileName[dwFileName - 8], _TEXT("Header"));
	MessageFile_Show(tszFileName, lpBuffer, lpUser, DT_FTPUSER, tszMultilinePrefix, NULL);

	dwSystemTime	  = (DWORD) time((time_t*)NULL);
	dwTickCount = GetTickCount();
	dwHidden          = 0;

	Who.dwMyCID = lpUser->Connection.dwUniqueId;
	Who.dwMyUID = lpUser->UserFile->Uid;

	//	List users online

	for ( Who.dwConnectionId = 0 ; Who.dwConnectionId <= dwMaxClientId ; Who.dwConnectionId++ )
	{
		lpClient  = LockClient(Who.dwConnectionId);
		if (!lpClient) continue;

		dwStatus = ClientToIoWho(lpClient, dwSystemTime, dwTickCount, &Who);

		UnlockClient(Who.dwConnectionId);

		if ((dwStatus == W_LOGIN) || !Who.lpUserFile)
		{
			// W_LOGIN implies ClientData.lpUserFile was NULL so stop here...
			Who.dwUsers--;
		}
		else if ((ActionLimit != W_ANY) && (dwStatus != ActionLimit))
		{
			// don't print non-matching entries
			dwHidden++;
		}
		else if (!pBuffer[dwStatus])
		{
			// we don't have a template to use...
			dwHidden++;
		}
		else if ((!lpExclude || FindIsMatch(lpExclude, Who.lpUserFile, FALSE) || (bIsAdmin && (dwStatus == W_UPLOAD || dwStatus == W_DOWNLOAD))) &&
			(!lpSearch || !FindIsMatch(lpSearch, Who.lpUserFile, TRUE)))
		{
			// need to sanitize action...
			// show just first word of action for port/pasv commands,
			// or first 2 words if a site command, PASS is already sanitized elsewhere

			bHidden = FALSE;
			if (!bIsAdmin)
			{
				for(n=0 ; n<dwPaths ; n++)
				{
					if (!PathCompare(tszPaths[n], Who.OnlineData.tszVirtualPath))
					{
						bHidden = TRUE;
						_tcscpy(Who.OnlineData.tszVirtualPath, _T("<hidden>"));
					}
					if (!PathCompare(tszPaths[n], Who.OnlineData.tszVirtualDataPath))
					{
						bHidden = TRUE;
						_tcscpy(Who.OnlineData.tszVirtualDataPath, _T("<hidden>"));
					}
				}
			}

			tszSpace = _tcschr(Who.OnlineData.tszAction, _T(' '));
			if (tszSpace && !bIsAdmin)
			{
				if (!_tcsnicmp(Who.OnlineData.tszAction, _T("site "), 5))
				{
					tszSpace = _tcschr(&Who.OnlineData.tszAction[5], _T(' '));
					if (tszSpace)
					{
						*tszSpace = 0;
					}
				}
				else if ((!_tcsnicmp(Who.OnlineData.tszAction, _T("PORT "), 5)))
				{
					*tszSpace = 0;
				}
				else if (bHidden)
				{
					*tszSpace = 0;
				}
				else if (tszSlash = _tcschr(Who.OnlineData.tszAction, _T('/')))
				{
					if (((tszSlash - Who.OnlineData.tszAction)/sizeof(TCHAR)) < (sizeof(Who.OnlineData.tszAction)/sizeof(TCHAR) - 9))
					{
						_tcscpy(tszSlash, _T("<hidden>"));
					}
					else
					{
						*tszSpace = 0;
					}
				}
			}

			//	Show message
			if (!bSorting)
			{
				Message_Compile(pBuffer[dwStatus], lpBuffer, FALSE, &Who, DT_WHO, tszMultilinePrefix, NULL);
			}
			else
			{
				if (dwSorted == dwMaxSorted)
				{
					dwMaxSorted *= 2;
					lpSortWhoTemp = ReAllocate(lpSortWhoArray, "SortWhoArray", dwMaxSorted * sizeof(*lpSortWhoArray));
					if (!lpSortWhoTemp)
					{
						dwError = ERROR_OUTOFMEMORY;
						FreeShared(Who.OnlineData.tszRealPath);
						FreeShared(Who.OnlineData.tszRealDataPath);
						UserFile_Close(&Who.lpUserFile, 0);
						break;
					}
					lpSortWhoArray = lpSortWhoTemp;
				}

				tszName = Uid2User(Who.lpUserFile->Uid);
				if (tszName)
				{
					lpNext = &lpSortWhoArray[dwSorted++];

					lpNext->dwNameIndex = TempBuf.len;
					Put_Buffer(&TempBuf, tszName, (_tcslen(tszName)+1)*sizeof(TCHAR));
					lpNext->dwLineIndex = TempBuf.len;
					Message_Compile(pBuffer[dwStatus], &TempBuf, FALSE, &Who, DT_WHO, tszMultilinePrefix, NULL);
					lpNext->dwLineLen   = TempBuf.len - lpNext->dwLineIndex;
				}
			}
		}
		else
		{
			dwHidden++;
		}
		//	Free resources
		FreeShared(Who.OnlineData.tszRealPath);
		FreeShared(Who.OnlineData.tszRealDataPath);
		UserFile_Close(&Who.lpUserFile, 0);
	}

	Who.dwLoginHours = dwHidden;

	if (bSorting)
	{
		// sort by name
		qsort_s(lpSortWhoArray, dwSorted, sizeof(*lpSortWhoArray), WhoSortCmp, TempBuf.buf);

		// now stuff the sorted entries into the real output buffer, but first make sure it's big enough
		if (lpBuffer->len + TempBuf.len > 10000)
		{
			n = 0;
		}
		if (AllocateBuffer(lpBuffer, lpBuffer->len + TempBuf.len))
		{
			dwError = ERROR_OUTOFMEMORY;
			goto cleanup;
		}

		for( n = 0 ; n < dwSorted ; n++ )
		{
			lpNext = &lpSortWhoArray[n];

			CopyMemory(&lpBuffer->buf[lpBuffer->len], &TempBuf.buf[lpNext->dwLineIndex], lpNext->dwLineLen);
			lpBuffer->len += lpNext->dwLineLen;
		}
	}

	//	Show footer
	_tcscpy(&tszFileName[dwFileName - 8], _TEXT("Footer"));
	MessageFile_Show(tszFileName, lpBuffer, &Who, DT_WHO, tszMultilinePrefix, NULL);

cleanup:
	if (lpSearch)  FindFinished(lpSearch);
	if (lpExclude) FindFinished(lpExclude);

	if (pBuffer[W_UPLOAD])   Free(pBuffer[W_UPLOAD]);
	if (pBuffer[W_DOWNLOAD]) Free(pBuffer[W_DOWNLOAD]);
	if (pBuffer[W_IDLE])     Free(pBuffer[W_IDLE]);
	if (pBuffer[W_LIST])     Free(pBuffer[W_LIST]);
	if (pBuffer[W_LOGIN])    Free(pBuffer[W_LOGIN]);
	Free(tszFileName);

	if (bSorting)
	{
		if (TempBuf.buf) Free(TempBuf.buf);
		if (lpSortWhoArray) Free(lpSortWhoArray);
	}

	if (dwError && tszSearch)
	{
		ERROR_RETURN(ERROR_INVALID_ARGUMENTS, tszSearch);
	}
	if (dwError)
	{
		ERROR_RETURN(dwError, GetStringIndexStatic(Args, 1));
	}
	return NULL;
}




LPTSTR Admin_Groups(LPFTPUSER lpUser, LPTSTR tszMultilinePrefix, LPIO_STRING Args)
{
	LPGROUPFILE		lpGroupFile;
	LPBUFFER		lpBuffer;
	PBYTE			pBuffer;
	INT				iOffset, i, Gid;
	LPTSTR			tszFileName, tszBasePath, tszArg;
	DWORD			dwFileName, dwHidden;
	BOOL            bAdmin, bAll;

	if (GetStringItems(Args) > 2) ERROR_RETURN(ERROR_INVALID_ARGUMENTS, GetStringRange(Args, 2, STR_END));

	bAll = FALSE;
	if (GetStringItems(Args) == 2)
	{
		tszArg = GetStringIndexStatic(Args, 1);
		if (!tszArg || _tcsicmp(tszArg, _T("-all")))
		{
			ERROR_RETURN(ERROR_INVALID_ARGUMENTS, tszArg);
		}
		bAll = TRUE;
	}

	lpBuffer = &lpUser->CommandChannel.Out;

	//	Show header
	tszBasePath	= Service_MessageLocation(lpUser->Connection.lpService);
	if (! tszBasePath) return NULL;
	dwFileName	= aswprintf(&tszFileName, _TEXT("%s\\GroupList.Header"), tszBasePath);
	FreeShared(tszBasePath);
	if (! dwFileName) return NULL;
	MessageFile_Show(tszFileName, lpBuffer, lpUser, DT_FTPUSER, tszMultilinePrefix, NULL);

	bAdmin = !HasFlag(lpUser->UserFile, "1M");
	dwHidden = 0;

	//	Load body
	_tcscpy(&tszFileName[dwFileName - 6], _TEXT("Body"));
	pBuffer	= Message_Load(tszFileName);

	if (pBuffer)
	{
		iOffset	= -1;
		while (! GroupFile_OpenNext(&lpGroupFile, &iOffset))
		{
			if (!bAdmin)
			{
				for (i = 0 ; i < MAX_GROUPS && ((Gid = lpUser->UserFile->AdminGroups[i]) != -1) ; i++)
				{
					if (Gid == lpGroupFile->Gid) break;
				}
				if (((i >= MAX_GROUPS) || (Gid == -1)) && (lpGroupFile->Gid != 1))
				{
					GroupFile_Close(&lpGroupFile, 0);
					dwHidden++;
					continue;
				}
			}

			if (!bAll && !lpGroupFile->Users && !stricmp(lpGroupFile->szDescription, "-"))
			{
				dwHidden++;
			}
			else
			{
				Message_Compile(pBuffer, lpBuffer, FALSE, lpGroupFile, DT_GROUPFILE, tszMultilinePrefix, NULL);
			}
			GroupFile_Close(&lpGroupFile, 0);
		}
		Free(pBuffer);
	}

	//	Show footer
	lpUser->FtpVariables.iPos = dwHidden;
	_tcscpy(&tszFileName[dwFileName - 6], _TEXT("Footer"));
	MessageFile_Show(tszFileName, lpBuffer, lpUser, DT_FTPUSER, tszMultilinePrefix, NULL);


	Free(tszFileName);
	return NULL;
}



LPTSTR Admin_Users(LPFTPUSER lpUser, LPTSTR tszMultilinePrefix, LPIO_STRING Args)
{
	LPUSERFILE		lpUserFile;
	LPBUFFER		lpBuffer;
	PBYTE			pBuffer;
	LPUSERSEARCH	lpSearch;
	LPTSTR			tszWildCard, tszFileName, tszBasePath, tszName;
	DWORD			dwFileName, dwError, dwMatches, dwUsers, n, dwBad;
	USERFILE_PLUS   UserFile_Plus;
	BOOL            bShowErrors;
	INT32           Uid;
	TCHAR           tszAsterisk[] = _T("*");

	//	Get arguments
	lpBuffer = &lpUser->CommandChannel.Out;
	bShowErrors = FALSE;

	if (GetStringItems(Args) > 1)
	{
		tszWildCard		= GetStringIndexStatic(Args, 1);
		if (!_tcsicmp(tszWildCard, _T("-errors")))
		{
			bShowErrors = TRUE;
			if (GetStringItems(Args) > 2)
			{
				tszWildCard	  = GetStringRange(Args, 2, STR_END);
			}
			else
			{
				tszWildCard   = tszAsterisk;
			}
		}
		else
		{
			tszWildCard		  = GetStringRange(Args, 1, STR_END);
		}
	}
	else tszWildCard	      = tszAsterisk;

	// let's test the user match string for errors before displaying anything...
	lpSearch = FindParse(tszWildCard, lpUser->UserFile, lpUser, TRUE);
	if (!lpSearch)
	{
		if (tszWildCard == tszAsterisk)
		{
			return GetStringIndexStatic(Args, 0);
		}
		return tszWildCard;
	}

	//	Show header
	tszBasePath	= Service_MessageLocation(lpUser->Connection.lpService);
	if (! tszBasePath)
	{
		FindFinished(lpSearch);
		return NULL;
	}
	dwFileName	= aswprintf(&tszFileName, _TEXT("%s\\UserList.Header"), tszBasePath);
	FreeShared(tszBasePath);
	if (! dwFileName)
	{
		FindFinished(lpSearch);
		return NULL;
	}
	MessageFile_Show(tszFileName, lpBuffer, lpUser, DT_FTPUSER, tszMultilinePrefix, NULL);

	//	Load body
	_tcscpy(&tszFileName[dwFileName - 6], _TEXT("Body"));
	pBuffer	= Message_Load(tszFileName);

	dwMatches = 0;
	dwUsers   = 0;
	dwError   = NO_ERROR;

	if (pBuffer)
	{
		dwUsers = lpSearch->dwUidList; // this is number of id's returned...
		//	List users matching globber
		while (!FindNextUser(lpSearch, &lpUserFile))
		{
			dwMatches++;
			UserFile_Plus.lpCommandChannel = &lpUser->CommandChannel;
			UserFile_Plus.lpFtpUserCaller = lpUser;
			UserFile_Plus.lpUserFile = lpUserFile;
			Message_Compile(pBuffer, lpBuffer, FALSE, &UserFile_Plus, DT_USERFILE_PLUS, tszMultilinePrefix, NULL);
			UserFile_Close(&lpUserFile, 0);
		}
		Free(pBuffer);
	}
	else
	{
		FindFinished(lpSearch);
	}

	dwBad = 0;
	if (!dwError)
	{
		//	Show footer
		lpUser->FtpVariables.iPos = dwMatches;
		lpUser->FtpVariables.iMax = dwUsers;
		_tcscpy(&tszFileName[dwFileName - 6], _TEXT("Footer"));
		MessageFile_Show(tszFileName, lpBuffer, lpUser, DT_FTPUSER, tszMultilinePrefix, NULL);

		// now run through and report users with errors if caller is 1M user.
		if (!HasFlag(lpUser->UserFile, _T("M1")) && lpUser->FtpVariables.lpUidList && lpUser->FtpVariables.lpUidMatches)
		{
			for ( n=0 ; n<lpUser->FtpVariables.dwUidList ; n++ )
			{
				if ( ( lpUser->FtpVariables.lpUidMatches[n] < -1 ) && ( (Uid = lpUser->FtpVariables.lpUidList[n]) > -1 ) )
				{
					dwBad++;
					if (bShowErrors)
					{
						if (dwBad == 1)
						{
							FormatString(lpBuffer, _T("%s%2TErrors:%0T\r\n"), tszMultilinePrefix);
						}

						//  Seek by id
						tszName = IdDataBase_SearchById(Uid, &dbUserId);
						if (tszName)
						{
							FormatString(lpBuffer, _T("%s  %4T[Uid: %d, Name: \"%s\"]%0T\r\n"), tszMultilinePrefix, Uid, tszName);
						}
						else
						{
							FormatString(lpBuffer, _T("%s  %4T[Uid: %d, Name: ?]%0T\r\n"), tszMultilinePrefix, Uid);
						}
					}
				}
			}
			if ( !bShowErrors && dwBad )
			{
				FormatString(lpBuffer, _T("%s%2TErrors: %4T%d%0T\r\n%s Use 'site %s -errors' to see them.\r\n"),
					tszMultilinePrefix, dwBad, tszMultilinePrefix, GetStringIndexStatic(Args, 0));
			}
		}
	}
	Free(tszFileName);
	if (dwError)
	{
		ERROR_RETURN(dwError, tszWildCard);
	}
	return NULL;
}



LPTSTR Admin_Kill(LPFTPUSER lpUser, LPTSTR tszMultilinePrefix, LPIO_STRING Args)
{
	DWORD		dwError;
	LPTSTR		tszConnectionId;
	TCHAR		*tpCheck;
	DWORD		dwConnectionId;

	dwError	= NO_ERROR;
	if (GetStringItems(Args) < 2) ERROR_RETURN(ERROR_MISSING_ARGUMENT, GetStringIndexStatic(Args, 0));
	if (GetStringItems(Args) > 2) ERROR_RETURN(ERROR_INVALID_ARGUMENTS, GetStringRange(Args, 2, STR_END));

	//	Get arguments
	tszConnectionId	= GetStringIndexStatic(Args, 1);
	dwConnectionId	= _tcstoul(tszConnectionId, &tpCheck, 10);
	if (dwConnectionId >= MAX_CLIENTS ||
		tpCheck <= tszConnectionId ||
		tpCheck[0] != _TEXT('\0')) ERROR_RETURN(ERROR_INVALID_ARGUMENTS, tszConnectionId);

	if (KillUser(dwConnectionId))
	{
		ERROR_RETURN(ERROR_USER_NOT_ONLINE, tszConnectionId);
	}
	return NULL;
}




LPTSTR Admin_Kick(LPFTPUSER lpUser, LPTSTR tszMultilinePrefix, LPIO_STRING Args)
{
	LPUSERFILE		lpUserFile;
	DWORD			dwError;
	LPTSTR			tszUserName;
	LRESULT			lResult;

	dwError	= NO_ERROR;
	if (GetStringItems(Args) < 2) ERROR_RETURN(ERROR_MISSING_ARGUMENT, GetStringIndexStatic(Args, 0));
	if (GetStringItems(Args) > 2) ERROR_RETURN(ERROR_INVALID_ARGUMENTS, GetStringRange(Args, 2, STR_END));

	//	Get arguments
	tszUserName	= GetStringIndexStatic(Args, 1);

	if (! UserFile_Open(tszUserName, &lpUserFile, 0))
	{
		//	Check permission
		dwError = CheckForMasterAccount(lpUser, lpUserFile);

		//	Kick user
		if (dwError == NO_ERROR)
		{
			lResult	= KickUser(lpUserFile->Uid);
			if (! lResult) dwError	= ERROR_USER_NOT_ONLINE;
		}

		UserFile_Close(&lpUserFile, 0);
		if (dwError == NO_ERROR) return NULL;
		SetLastError(dwError);
	}
	return tszUserName;
}
