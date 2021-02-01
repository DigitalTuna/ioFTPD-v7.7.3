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


static LPMOUNTCACHE		 *lpMountCache;
static CRITICAL_SECTION	  csMountCache;
static DWORD			  dwMountCache, dwMountCacheSize;

static CHAR               szDefaultSectionName[_MAX_NAME+1];
static INT                iDefaultCreditSection, iDefaultStatsSection, iDefaultShareSection;

VIRTUALDIREVENT    KnownVirtualDirEvents[MAX_VIRTUAL_DIR_MOUNTPOINTS];
DWORD              dwKnownVirtualDirEvents;

static FILEINFO           DefaultVirtualFileInfo;

static VOID PreLoad_Table(LPVFSPRELOADTABLE lpPreTable);



// Reject invalid paths
BOOL Validate_Pathname(LPTSTR tszInPath, LPTSTR tszOutPath, LPSTR *ptszColon)
{
	LPSTR   tszStart, tszColon;
	INT		Dots, Others, i;

	Dots	= 0;
	Others	= 0;
	tszStart = tszInPath;
	tszColon = NULL;

	for (i=0 ; (*tszOutPath = *tszInPath) && (++i <= _MAX_PWD) ; tszOutPath++, tszInPath++ )
	{
		switch (*tszInPath)
		{
		//case '?':
		//case '*':
		case '"':
		case '<':
		case '>':
		case '|':
		case ':':
			SetLastError(ERROR_INVALID_NAME);
			return TRUE;
#if 0
		case ':':
			if (tszColon == NULL)
			{
				tszColon = tszOutPath;
			}
			break;
#endif

		case '\\':
			*tszOutPath = '/';
		case '/':
			// TODO: use this instead, but breaks some scripts who use $pwd/$name where $pwd always ends in /
			// if ((tszStart != tszInPath && !Others && !Dots) || (!Others && Dots > 2))
			if ((tszStart != tszInPath) && tszOutPath[-1] == '/')
			{
				tszOutPath--;
			}
			else if (!Others && Dots > 2)
			{
				// disallow paths starting with multiple dots that aren't relative
				SetLastError(ERROR_INVALID_NAME);
				return TRUE;
			}
			Dots	= 0;
			Others	= 0;
			break;
		case '.':
			Dots++;
			break;

		default:
			Dots	= 0;
			Others++;
		}
	}
	if ((i > _MAX_PWD) || (!Others && Dots > 2))
	{
		SetLastError(ERROR_INVALID_NAME);
		return TRUE;
	}

#if 0 // TODO: drive specifier logic here... add support for 2 digit id's
	if (tszColon)
	{
		if ((tszOutPath != tszColon + 4) || (tszColon[1] != ':') || !isdigit(tszColon[2]) || !isalpha(tszColon[3]) || tszColon[4])
		{
			SetLastError(ERROR_INVALID_NAME);
			return TRUE;
		}
		// truncate the string.
		*tszColon = 0;
		// point at the vfs/drive identifier
		tszColon += 2;
	}

	if (ptszColon)
	{
		*ptszColon = tszColon;
	}
#endif

	return FALSE;
}





// returns NULL if directory isn't a virtual dir, else a pointer to the virtual dir structure
LPVIRTUALDIREVENT FindVirtualDirEvent(LPTSTR tszPath)
{
	LPVIRTUALDIREVENT lpVirtualDirEvent;
	DWORD             n, dwLen;

	for(n=0 ; n < dwKnownVirtualDirEvents ; n++)
	{
		lpVirtualDirEvent = &KnownVirtualDirEvents[n];
		dwLen  = lpVirtualDirEvent->dwName;
		if (!_tcsnicmp(lpVirtualDirEvent->tszName, tszPath, dwLen) && (!tszPath[dwLen] || tszPath[dwLen] == _T('/')))
		{
			return lpVirtualDirEvent;
		}
	}
	return NULL;
}


INT __cdecl
VirtualInfoCompare(LPVIRTUALINFO *lpItem1, LPVIRTUALINFO *lpItem2)
{
	return _tcsicmp(lpItem1[0]->tszName, lpItem2[0]->tszName);
}


BOOL
InsertVirtualInfo(LPVIRTUALDIR lpVirtualDir, LPVIRTUALINFO lpVirtualInfo)
{
	LPVIRTUALINFO *lpNewArray;

	if (lpVirtualDir->dwVirtualInfos+1 >= lpVirtualDir->dwMaxVirtualInfos)
	{
		lpVirtualDir->dwMaxVirtualInfos += INITIAL_VIRTUAL_DIR_ENTRIES;
		lpNewArray = ReAllocate(lpVirtualDir->lpVirtualInfoArray, "VirtInfoArray", lpVirtualDir->dwMaxVirtualInfos*sizeof(*lpVirtualDir->lpVirtualInfoArray));
		if (!lpNewArray)
		{
			lpVirtualDir->dwMaxVirtualInfos -= INITIAL_VIRTUAL_DIR_ENTRIES;
			return FALSE;
		}
		lpVirtualDir->lpVirtualInfoArray = lpNewArray;
	}
	if (QuickInsert(lpVirtualDir->lpVirtualInfoArray, lpVirtualDir->dwVirtualInfos++, lpVirtualInfo, (QUICKCOMPAREPROC) VirtualInfoCompare))
	{
		// there was a matching entry already...
		lpVirtualDir->dwVirtualInfos--;
		return FALSE;
	}
	return TRUE;
}


LPVIRTUALINFO
VirtualDirGetInfo(LPVIRTUALDIR lpVirtualDir, LPSTR szName)
{
	VIRTUALINFO MatchInfo;
	INT iResult;

	MatchInfo.tszName = szName;
	iResult = QuickFind(lpVirtualDir->lpVirtualInfoArray, lpVirtualDir->dwVirtualInfos, &MatchInfo, (QUICKCOMPAREPROC) VirtualInfoCompare);
	if (iResult == 0)
	{
		return NULL;
	}
	return lpVirtualDir->lpVirtualInfoArray[iResult-1];
}


VOID
VirtualDirInfoFree(LPVIRTUALDIR lpVirtualDir, BOOL bFree)
{
	LPVIRTUALINFO lpVirtualInfo;
	DWORD         n;

	for (n=0 ; n< lpVirtualDir->dwVirtualInfos ; n++)
	{
		lpVirtualInfo = lpVirtualDir->lpVirtualInfoArray[n];
		if (lpVirtualInfo->lpFileInfo->lReferenceCount != 0)
		{
			CloseFileInfo(lpVirtualInfo->lpFileInfo);
		}
		Free(lpVirtualInfo);
	}
	if (bFree)
	{
		Free(lpVirtualDir->lpVirtualInfoArray);
		Free(lpVirtualDir);
	}
	else
	{
		lpVirtualDir->dwVirtualInfos = 0;
	}
}



VOID
MarkVirtualDir(PVIRTUALPATH Path, MOUNTFILE hMountFile)
{
	LPVIRTUALDIR lpVirtualDir;
	DWORD n;

	if (!hMountFile) return;

	if (!Path)
	{
		// this means mark everything as dirty
		for (n=0 ; n<dwKnownVirtualDirEvents ; n++ )
		{
			lpVirtualDir = hMountFile->lpVirtualDirArray[n];
			if (lpVirtualDir)
			{
				VirtualDirInfoFree(lpVirtualDir, TRUE);
				hMountFile->lpVirtualDirArray[n] = NULL;
			}
		}
		return;
	}

	if (!Path->lpVirtualDirEvent) return;

	lpVirtualDir = hMountFile->lpVirtualDirArray[Path->lpVirtualDirEvent->dwId];
	if (!lpVirtualDir) return;

	VirtualDirInfoFree(lpVirtualDir, TRUE);
	hMountFile->lpVirtualDirArray[Path->lpVirtualDirEvent->dwId] = NULL;
}



LPVIRTUALDIR
VirtualDirLoad(MOUNTFILE hMountFile, LPVIRTUALDIREVENT lpVirtualDirEvent, LPSTR szPath, LPSTR szGlob, BOOL bUpdate, BOOL bListing, BOOL bExists, LPTSTR tszCommand)
{
	EVENT_DATA EventData;
	LPVIRTUALDIR lpVirtualDir, lpVirtualTemp;
	IO_STRING Args;
	DWORD dwLen, dwCmp;
	INT   iResult;
	LPSTR szOldGlob;
	LPSTR tszExistsNum[] = { "0" , "1" };
	LPSTR tszExists;

	dwLen = strlen(szPath);
	if ((dwLen > _MAX_PWD) || (dwLen < 2) || !hMountFile->lpFtpUser) return NULL;
	if (szPath[dwLen-1] == '/')
	{
		dwCmp = dwLen-1;
	}
	else
	{
		dwCmp = dwLen;
	}

	lpVirtualDir = hMountFile->lpVirtualDirArray[lpVirtualDirEvent->dwId];

	if (!bUpdate && lpVirtualDir && (lpVirtualDir->dwLen == dwCmp+1) && !strnicmp(lpVirtualDir->pwd, szPath, dwCmp))
	{
		// we have a cached copy of the directory being requested and we aren't forcing an update
		if (!bListing)
		{
			// we aren't listing
			return lpVirtualDir;
		}
		if (!lpVirtualDir->bListed && (!szGlob || stricmp(szGlob, lpVirtualDir->szLastGlob)))
		{
			// we haven't listed this dir's contents yet and the glob matches
			lpVirtualDir->bListed = TRUE;
			return lpVirtualDir;
		}
	}

	ZeroMemory(&Args, sizeof(Args));
	if (SplitString(lpVirtualDirEvent->tszEvent, &Args))
	{
		return NULL;
	}
	if (!szGlob) szGlob = "";
	if (!lpVirtualDir || !lpVirtualDir->szLastGlob[0])
	{
		szOldGlob = "";
	}
	else
	{
		szOldGlob = lpVirtualDir->szLastGlob;
	}
	if (!tszCommand) tszCommand = _T("-");
	tszExists = (bExists ? tszExistsNum[1] : tszExistsNum[0]);
	if (AppendQuotedArgToString(&Args, szPath) || AppendQuotedArgToString(&Args, szGlob) || AppendQuotedArgToString(&Args, szOldGlob) ||
		AppendArgToString(&Args, tszExists) || AppendArgToString(&Args, tszCommand))
	{
		FreeString(&Args);
		return NULL;
	}

	lpVirtualTemp = Allocate("VirtualDir", sizeof(VIRTUALDIR));
	if (!lpVirtualTemp) return NULL;
	ZeroMemory(lpVirtualTemp, sizeof(*lpVirtualTemp));

	ZeroMemory(&EventData, sizeof(EventData));
	EventData.dwData = DT_FTPUSER;
	EventData.lpData = hMountFile->lpFtpUser;

	if (RunTclEventWithResult(&EventData, &Args, &iResult, lpVirtualTemp) || !iResult)
	{
		FreeString(&Args);
		VirtualDirInfoFree(lpVirtualTemp, TRUE);
		return NULL;
	}

	FreeString(&Args);

	// TODO: Should the ||RESOLVED|| case have an easier way to return the result that doesn't invalidate the old cache entry
	// if it's a directory listing?
	if  ((lpVirtualTemp->dwVirtualInfos == 1) && lpVirtualTemp->lpVirtualInfoArray && lpVirtualTemp->lpVirtualInfoArray[0] &&
		!stricmp(lpVirtualTemp->lpVirtualInfoArray[0]->tszName, "||RESOLVED||") && lpVirtualTemp->lpVirtualInfoArray[0]->tszLink )
	{
		// we resolved the path to a link instead of a dir listing
		dwLen = strlen(lpVirtualTemp->lpVirtualInfoArray[0]->tszLink);
		if (!lpVirtualDir)
		{
			lpVirtualDir = lpVirtualTemp;
			hMountFile->lpVirtualDirArray[lpVirtualDirEvent->dwId] = lpVirtualDir;
		}
		strncpy_s(lpVirtualDir->szTarget, sizeof(lpVirtualDir->szTarget), lpVirtualTemp->lpVirtualInfoArray[0]->tszLink, dwLen);
		if (lpVirtualDir != lpVirtualTemp)
		{
			VirtualDirInfoFree(lpVirtualTemp, TRUE);
		}
		return lpVirtualDir;
	}

	if (*szGlob)
	{
		strncpy_s(lpVirtualTemp->szLastGlob, sizeof(lpVirtualTemp->szLastGlob)/sizeof(TCHAR), szGlob, _TRUNCATE);
	}
	else if (*szOldGlob)
	{
		strncpy_s(lpVirtualTemp->szLastGlob, sizeof(lpVirtualTemp->szLastGlob)/sizeof(TCHAR), szOldGlob, _TRUNCATE);
	}

	if (lpVirtualDir)
	{
		VirtualDirInfoFree(lpVirtualDir, TRUE);
	}
	hMountFile->lpVirtualDirArray[lpVirtualDirEvent->dwId] = lpVirtualTemp;
	strcpy_s(lpVirtualTemp->pwd, sizeof(lpVirtualTemp->pwd), szPath);
	lpVirtualTemp->dwLen = dwLen;
	if (lpVirtualTemp->pwd[dwLen-1] != '/')
	{
		lpVirtualTemp->pwd[lpVirtualTemp->dwLen++] = '/';
		lpVirtualTemp->pwd[lpVirtualTemp->dwLen]   = 0;
	}
	lpVirtualTemp->szTarget[0] = 0;
	GetSystemTimeAsFileTime(&lpVirtualTemp->ftLastUpdate);
	if (bListing) lpVirtualTemp->bListed = TRUE;
	return lpVirtualTemp;
}





LPMOUNT_TABLE PWD_GetTable(LPSTR szVirtualPath, MOUNTFILE hMountFile)
{
	LPMOUNT_TABLE	lpTable;
	PCHAR			pSlash, pLast;
	BOOL			bLoop, bShouldLoop;
	DWORD			dwItem, i;

	//	Sanity check
	if (! hMountFile) return NULL;

	lpTable		= hMountFile->lpMountTable;
	bLoop		= TRUE;
	bShouldLoop	= TRUE;
	pLast		= szVirtualPath;

	while (bLoop)
	{
		bLoop	= FALSE;

		if (! (pSlash = strchr(pLast, '/')))
		{
			bShouldLoop	= FALSE;
			dwItem		= strlen(pLast);
			pSlash		= &pLast[dwItem];
		}
		else dwItem	= pSlash - pLast;

		for (i = 0;i < lpTable->dwMountPoints;i++)
		{
			if (lpTable->lpMountPoints[i]->dwName == dwItem &&
				! memicmp(lpTable->lpMountPoints[i]->szName, pLast, dwItem))
			{
				bLoop	= bShouldLoop;
				pLast	= &pSlash[1];

				if (! (lpTable = (LPMOUNT_TABLE)lpTable->lpMountPoints[i]->lpNextTable)) return NULL;

				break;
			}
		}
	}
	return (bShouldLoop ? NULL : lpTable);
}


BOOL
PWD_IsMountPoint(LPSTR szVirtualPath, MOUNTFILE hMountFile)
{
	LPMOUNT_TABLE	lpTable;
	PCHAR			pSlash, pLast;
	BOOL			bLoop, bShouldLoop;
	DWORD			dwItem, i;

	//	Sanity check
	if (! hMountFile) return FALSE;

	lpTable		= hMountFile->lpMountTable;
	bLoop		= TRUE;
	bShouldLoop	= TRUE;
	pLast		= szVirtualPath;

	while (bLoop)
	{
		bLoop	= FALSE;

		if (! (pSlash = strchr(pLast, '/')))
		{
			bShouldLoop	= FALSE;
			dwItem		= strlen(pLast);
			pSlash		= &pLast[dwItem];
		}
		else dwItem	= pSlash - pLast;

		for (i = 0;i < lpTable->dwMountPoints;i++)
		{
			if (lpTable->lpMountPoints[i]->dwName == dwItem &&
				! memicmp(lpTable->lpMountPoints[i]->szName, pLast, dwItem))
			{
				bLoop	= bShouldLoop;
				pLast	= &pSlash[1];

				if (*pLast == 0)
				{
					return TRUE;
				}
				if (! (lpTable = (LPMOUNT_TABLE)lpTable->lpMountPoints[i]->lpNextTable))
				{
					return FALSE;
				}
				break;
			}
		}
	}
	return FALSE;
}







// If Exists == FALSE then return the first match in the mount table even if it doesn't exist
// If Exists == TRUE then return the first match of an existing item in the filesystem
// If path points to a virtual dir then return NULL but set Data->lpVirtDir to point to it.
LPSTR PWD_Resolve(LPSTR szVirtualPath, MOUNTFILE hMountFile, LPMOUNT_DATA Data, BOOL Exists, INT ExtraMem)
{
	LPMOUNT_TABLE	  lpTable;
	LPMOUNT_POINT	  BestMatch;
	LPVIRTUALDIREVENT lpVirtualDirEvent;
	LPSTR			  Slash, Last, PwdBest, PwdEnd;
	BOOL			  Loop;
	UINT			  Length, i, lLength, vLength, Allocated;

	if (Data)
	{
		Data->lpVirtualDirEvent = NULL;
	}

	if (Data && Data->Initialized)
	{
		//	Get resume point
		BestMatch	= Data->Resume;
		//	Make sure there are more mounts
		if (!BestMatch || (i = Data->Last + 1) >= BestMatch->dwSubMounts)
		{
			SetLastError(ERROR_INVALID_ARGUMENTS);
			return NULL;
		}

		PwdBest	= Data->Pwd;
		lLength = Data->Length;
	}
	else
	{
		//	Sanity check
		if (! hMountFile)
		{
			SetLastError(ERROR_INVALID_ARGUMENTS);
			return NULL;
		}

		lpTable		= hMountFile->lpMountTable;
		PwdEnd		= NULL;
		BestMatch	= NULL;
		lLength     = 0;
		Loop		= TRUE;
		Last		= szVirtualPath;

		while (Loop)
		{
			Loop	= FALSE;
			vLength = lLength;
			//	Find Next Slash
			if (! (Slash = strchr(Last, '/')))
			{
				//	End of string reached
				Length	= strlen(Last);
				Slash	= PwdEnd = &Last[Length];
				lLength += Length;
			}
			else
			{
				//	Slash found
				Length	= Slash - Last;
				lLength += Length + 1;
			}
			vLength += Length;

			for(i=0 ; i < dwKnownVirtualDirEvents ; i++)
			{
				if (KnownVirtualDirEvents[i].dwName == vLength && !_tcsnicmp(KnownVirtualDirEvents[i].tszName, szVirtualPath, vLength) &&
					(szVirtualPath[vLength] == '/' || szVirtualPath[vLength] == 0))
				{
					// we got a match and it's not a partial.
					lpVirtualDirEvent = &KnownVirtualDirEvents[i];
					if (Data)
					{
						//	Initialize data structure
						Data->Initialized	= TRUE;
						Data->Resume		= NULL;
						Data->Pwd			= NULL;
						Data->Length		= 0;
						Data->lpVirtualDirEvent = lpVirtualDirEvent;
						// this guarantees that future calls will return NULL and not try to resolve
						Data->Last          = MAX_SUBMOUNTS;
					}
					SetLastError(IO_VIRTUAL_DIR);
					return NULL;
				}
			}

			for (i = 0U;i < lpTable->dwMountPoints;i++)
			{
				if (lpTable->lpMountPoints[i]->dwName == Length &&
					! _strnicmp(lpTable->lpMountPoints[i]->szName, Last, Length))
				{
					//	Match
					if (! PwdEnd)
					{
						Loop	= TRUE;
						Last	= &Slash[1];
					}
					else Last	= Slash;

					if (lpTable->lpMountPoints[i]->dwSubMounts)
					{
						BestMatch	= lpTable->lpMountPoints[i];
						PwdBest		= Last;
					}

					if (! (lpTable = (LPMOUNT_TABLE)lpTable->lpMountPoints[i]->lpNextTable)) Loop	= FALSE;
					break;
				}
			}
		}

		//	Make sure there is a match
		if (! BestMatch)
		{
			SetLastError(ERROR_PATH_NOT_FOUND);
			return NULL;
		}

		//	Calculate length of string end part
		lLength	= (PwdEnd ? PwdEnd - PwdBest : strlen(PwdBest));

		while (lLength > 0 && PwdBest[lLength - 1] == '/') lLength--;

		if (Data)
		{
			//	Initialize data structure
			Data->Initialized	= TRUE;
			Data->Resume		= BestMatch;
			Data->Pwd			= PwdBest;
			Data->Length		= lLength;
		}

		i	= 0;
	}

	Allocated	= 0;
	Last		= NULL;

	for (;;)
	{
		//	Store current position to data structure (if any)
		if (Data) Data->Last	= i;
		
		Length	= BestMatch->lpSubMount[i].dwFileName;

		if (Allocated < Length + lLength + ExtraMem + 2)
		{
			Allocated	= Length + lLength + ExtraMem + 2;
			FreeShared(Last);
			//	Check allocation
			if (! (Last = (LPSTR)AllocateShared(NULL, "RealPath", Allocated)))
			{
				return NULL;
			}
		}

		//	Append remaining path to result
		if (lLength > 0)
		{
			CopyMemory(Last, BestMatch->lpSubMount[i].szFileName, Length);
			if (Length > 0 && Last[Length - 1] != '\\') Last[Length++]	= '\\';
			CopyMemory(&Last[Length], PwdBest, lLength);
			Last[lLength + Length]	= '\0';

			Slash	= &Last[Length + lLength];
			//	Replace '\' with '/'
			while (Slash-- > Last) if (Slash[0] == '/') Slash[0]	= '\\';
		}
		else CopyMemory(Last, BestMatch->lpSubMount[i].szFileName, Length + 1);

		//	Return last offset
		if (! Exists || GetFileAttributes(Last) != INVALID_FILE_SIZE)
		{
			if (Data) Data->dwLastPath	= Length + lLength;
			return Last;
		}

		if (++i >= BestMatch->dwSubMounts)
		{
			FreeShared(Last);
			SetLastError(ERROR_PATH_NOT_FOUND);
			return NULL;
		}
	}
}



VOID PWD_Free(PVIRTUALPATH VirtualPath)
{
	//	Free path variable
	if (VirtualPath->RealPath)
	{
		FreeShared(VirtualPath->RealPath);
	}
	//	Reset to null
	VirtualPath->RealPath	= NULL;
	VirtualPath->l_RealPath	= 0;
}


VOID PWD_Reset(PVIRTUALPATH VirtualPath)
{
	//	Reset virtual path
	VirtualPath->RealPath	= NULL;
	VirtualPath->l_RealPath	= 0;
	VirtualPath->lpMountPoint      = NULL;
	VirtualPath->lpVirtualDirEvent = NULL;
	VirtualPath->iMountIndex = 0;

}

VOID PWD_Zero(PVIRTUALPATH VirtualPath)
{
	//	Reset virtual path
	VirtualPath->RealPath	= NULL;
	VirtualPath->l_RealPath	= 0;
	// set lengths to zero
	VirtualPath->len    = 0;
	VirtualPath->Symlen = 0;
	// null terminate paths just to help in debugging
	VirtualPath->pwd[0] = 0;
	VirtualPath->Symbolic[0] = 0;
}

VOID PWD_Set(PVIRTUALPATH VirtualPath, LPTSTR tszPath)
{
	//	Reset virtual path
	VirtualPath->Symlen = VirtualPath->len = _tcslen(tszPath);
	if (VirtualPath->len > _MAX_PWD)
	{
		VirtualPath->Symlen = VirtualPath->len = _MAX_PWD;
	}
	CopyMemory(VirtualPath->pwd, tszPath, VirtualPath->len);
	CopyMemory(VirtualPath->Symbolic, tszPath, VirtualPath->len);
	VirtualPath->pwd[VirtualPath->len] = 0;
	VirtualPath->Symbolic[VirtualPath->len] = 0;
}


BOOL PWD_Copy(PVIRTUALPATH Source, PVIRTUALPATH Target, BOOL AllocatePath)
{
	//	Copy Virtual path
	Target->len	= Source->len;
	CopyMemory(Target->pwd, Source->pwd, Source->len + 1);
	if (Source->Symlen == 0)
	{
		Target->Symlen = Source->len;
		CopyMemory(Target->Symbolic, Source->pwd, Source->len + 1);
	}
	else
	{
		Target->Symlen = Source->Symlen;
		CopyMemory(Target->Symbolic, Source->Symbolic, Source->Symlen + 1);
	}
	//	Free previous virtual path
	PWD_Free(Target);
	//	Allocate new virtual path
	if (AllocatePath && Source->l_RealPath && Source->RealPath)
	{
		//	Allocate memory
		Target->RealPath	= (LPSTR)AllocateShared(NULL, "RealPath", Source->l_RealPath + 1);
		//	Check allocation
		if (! Target->RealPath) return TRUE;
		//	Copy Real path
		Target->l_RealPath	= Source->l_RealPath;
		CopyMemory(Target->RealPath, Source->RealPath, Source->l_RealPath + 1);
	}
	Target->lpMountPoint      = Source->lpMountPoint;
	Target->lpVirtualDirEvent = Source->lpVirtualDirEvent;
	Target->iMountIndex       = Source->iMountIndex;
	return FALSE;
}


BOOL PWD_CopyAddSym(PVIRTUALPATH Source, PVIRTUALPATH Target, BOOL AllocatePath)
{
	//	Copy Virtual path
	Target->len	= Source->len;
	CopyMemory(Target->pwd, Source->pwd, Source->len + 1);
	if (Source->Symlen == 0)
	{
		Target->Symlen = Source->len;
		CopyMemory(Target->Symbolic, Source->pwd, Source->len + 1);
	}
	else
	{
		Target->Symlen = Source->Symlen;
		CopyMemory(Target->Symbolic, Source->Symbolic, Source->Symlen + 1);
	}
	//	Free previous virtual path
	PWD_Free(Target);
	//	Allocate new virtual path
	if (AllocatePath)
	{
		//	Allocate memory
		Target->RealPath	= (LPSTR)AllocateShared(NULL, "RealPath", Source->l_RealPath + 1);
		//	Check allocation
		if (! Target->RealPath) return TRUE;
		//	Copy Real path
		Target->l_RealPath	= Source->l_RealPath;
		CopyMemory(Target->RealPath, Source->RealPath, Source->l_RealPath + 1);
	}
	return FALSE;
}





//	Flags ( TYPE_LINK, TYPE_DIRECTORY, EXISTS, KEEP_CASE, KEEP_LINKS, VIRTUAL_UPDATE, VIRTUAL_PWD, VIRTUAL_ONCE, VIRTUAL_DONE )
LPTSTR PWD_CWD2(LPUSERFILE lpUserFile, PVIRTUALPATH Pwd, LPTSTR tszChangeTo, MOUNTFILE hMountFile, LPMOUNT_DATA lpMountData, DWORD dwFlags,
                struct _FTP_USER *lpUser, LPTSTR tszCommand, IO_STRING *Args)
{
	LPFILEINFO					lpFileInfo;
	VIRTUALPATH					VirtualPath;
	LPTSTR						tszCurrentPath, tszPath, tszLink, tszNewChangeTo, tszNewTemp, tszTemp, tszColon, tszBestParent;
	TCHAR						*tpSlash, *tpOffset, tszLocalChangeTo[_MAX_PWD+1], tszSymbolicChangeto[_MAX_PWD+1];
	DWORD						dwLastError, dwError, dwCurrentPath, dwLink, dwPathItem, dwDepth, dwLen, dwFollowingLinks, dwBestDepth;
	BOOL						bStringEnd, bFileExists, bKeepLinks, bOverride;
	LPVIRTUALDIREVENT           lpVirtualDirEvent;
	LPVIRTUALDIR                lpVirtualDir, lpVirtualNew;
	LPVIRTUALINFO               lpVirtualInfo;
	MOUNT_DATA                  MountData;
	LPDIRECTORYINFO             lpDirInfo;

	if (!lpMountData) lpMountData = &MountData;
	lpMountData->Initialized	= FALSE;
	Pwd->lpVirtualDirEvent = NULL;

	//	Check validity of virtual path
	if (Validate_Pathname(tszChangeTo, tszLocalChangeTo, &tszColon)) return NULL;
	tszChangeTo = tszLocalChangeTo;
	PWD_Reset(&VirtualPath);
	bKeepLinks = (dwFlags & KEEP_LINKS) || (hMountFile->lpFtpUser && hMountFile->lpFtpUser->FtpVariables.bKeepLinksInPath);
	bOverride  = (dwFlags & VIRTUAL_ONCE);

	if (tszChangeTo[0] == _TEXT('/'))
	{
		//	Find first non '/ character
		while ((++tszChangeTo)[0] == _TEXT('/'));
		//	Switch pwd to /
		PWD_Set(&VirtualPath, _T("/"));
	}
	else
	{
		if (!bKeepLinks)
		{
			PWD_Copy(Pwd, &VirtualPath, FALSE);
		}
		else
		{
			if ((Pwd->Symlen == 0) && (Pwd->len != 0))
			{
				Putlog(LOG_ERROR, "Invalid symbolic path (pwd was %s).\r\n", Pwd->pwd);
				CopyMemory(Pwd->Symbolic, Pwd->pwd, Pwd->len+1);
				Pwd->Symlen = Pwd->len;
			}
			// we want to process the change relative to the symbolic path not the resolved path
			// so it makes sense from the user's perspective...
			if (!PWD_Normalize(tszChangeTo, tszSymbolicChangeto, Pwd->Symbolic))
			{
				return NULL;
			}
			if (tszChangeTo[0] == _T('.') && !tszChangeTo[1])
			{
				// as a special case an original change to of "." requires disabling TYPE_LINK
				// as normalize would have removed that as redundant and now we would be getting
				// a symbolic link if the last dir was one instead of the actual dir...
				dwFlags &= ~TYPE_LINK;
			}
			tszChangeTo = tszSymbolicChangeto;
			PWD_Set(&VirtualPath, _T("/"));
			if (tszChangeTo[0] == _TEXT('/'))
			{
				//	Find first non '/ character
				while ((++tszChangeTo)[0] == _TEXT('/'));
			}
		}
	}

	dwLastError		= NO_ERROR;
	dwError			= NO_ERROR;
	tszNewChangeTo	= NULL;
	tszCurrentPath	= NULL;
	tszBestParent   = NULL;
	dwBestDepth     = 0;
	dwDepth			= 0;
	bStringEnd		= (tszChangeTo[0] == _TEXT('\0') ? TRUE : FALSE);
	dwFollowingLinks = 0;

	for (;dwError == NO_ERROR && ! bStringEnd;tszChangeTo = tpSlash)
	{
		if (! (tpSlash = _tcschr(tszChangeTo, _TEXT('/'))))
		{
			//	End of string reached
			bStringEnd	= TRUE;
			dwPathItem	= _tcslen(tszChangeTo);
			tpSlash		= &tszChangeTo[dwPathItem];
		}
		else
		{
			//	Calculate length
			dwPathItem	= tpSlash - tszChangeTo;
			//	Find first non '/' character
			while ((++tpSlash)[0] == _TEXT('/'));
			//	Check for end of string
			if (tpSlash[0] == _TEXT('\0')) bStringEnd	= TRUE;
		}

		//	Depth check
		if (dwDepth++ > 25)
		{
			dwError	= ERROR_PATH_NOT_FOUND;
			break;
		}
		if ((dwError = dwLastError) != NO_ERROR) break;

		if (tszChangeTo[0] == _TEXT('.'))
		{
			switch (dwPathItem)
			{
			case 1:
				//	CWD "."
				continue;
			case 2:
				if (tszChangeTo[1] == _TEXT('.'))
				{
					//	CWD ".."
					if (bKeepLinks)
					{
						if ((VirtualPath.Symlen < 2) || (VirtualPath.len < 2))
						{
							dwError = ERROR_PATH_NOT_FOUND;
							break;
						}

						if (!dwFollowingLinks)
						{
							while (VirtualPath.Symlen && VirtualPath.Symbolic[--VirtualPath.Symlen - 1] != _TEXT('/'));
							VirtualPath.Symbolic[VirtualPath.Symlen]	= _T('\0');
						}
					}
					else if (VirtualPath.len < 2)
					{
						dwError = ERROR_PATH_NOT_FOUND;
						break;
					}
					while (VirtualPath.len && VirtualPath.pwd[--VirtualPath.len - 1] != _TEXT('/'));
					VirtualPath.pwd[VirtualPath.len]	= _T('\0');

					lpMountData->Initialized	= FALSE;
					FreeShared(tszCurrentPath);
					if ((tszCurrentPath = PWD_Resolve(VirtualPath.pwd, hMountFile, lpMountData, TRUE, 0)))
					{
						// it's a valid resolved path so far
						dwCurrentPath = lpMountData->dwLastPath;
						// reset best parent path since we moved up a directory...
						if (tszBestParent) FreeShared(tszBestParent);
						tszBestParent = AllocateShared(tszCurrentPath, NULL, 0);
						dwBestDepth = dwDepth;
						continue;
					}
					if (lpMountData->Initialized && lpMountData->lpVirtualDirEvent != NULL)
					{
						// it's a virtual dir and thus the path doesn't exist
						dwCurrentPath = 0;
						continue;
					}
					//	Could not resolve path
					dwError	= GetLastError();
				}
				break;

			default:
				if (dwPathItem >= 7 && ! _tcsncmp(&tszChangeTo[1], _TEXT("ioFTPD"), 6 ))
				{
					//	.ioFTPD*
					dwError	= ERROR_INVALID_NAME;
					break;
				}
			}
			if (dwError != NO_ERROR) break;
		}

		if (dwFollowingLinks && (tszChangeTo[0] == '|') && (dwPathItem == 1))
		{
			// just skip this fake entry...
			dwFollowingLinks--;
			continue;
		}

		if ((dwPathItem + VirtualPath.len + 1 >= _MAX_PWD) || (bKeepLinks && (dwPathItem + VirtualPath.Symlen + 1 >= _MAX_PWD)))
		{
			dwError	= ERROR_INVALID_NAME;
			break;
		}

		FreeShared(tszCurrentPath);
		tszCurrentPath	= NULL;
		dwCurrentPath	= 0;
		bFileExists		= FALSE;
		tpOffset		= &VirtualPath.pwd[VirtualPath.len];
		lpMountData->Initialized	= FALSE;
		//	Update virtual path
		CopyMemory(tpOffset, tszChangeTo, dwPathItem * sizeof(TCHAR));
		VirtualPath.len	 += dwPathItem;
		VirtualPath.pwd[VirtualPath.len]	= _TEXT('\0');
		if (bKeepLinks && !dwFollowingLinks)
		{
			CopyMemory(&VirtualPath.Symbolic[VirtualPath.Symlen], tszChangeTo, dwPathItem * sizeof(TCHAR));
			VirtualPath.Symlen += dwPathItem;
			VirtualPath.Symbolic[VirtualPath.Symlen]	= _TEXT('\0');
		}

		//	Update real path
		while (! bFileExists &&
			(tszPath = PWD_Resolve(VirtualPath.pwd, hMountFile, lpMountData, FALSE, 0)))
		{
			FreeShared(tszCurrentPath);
			tszCurrentPath	= tszPath;
			dwCurrentPath	= lpMountData->dwLastPath;

			//	Get fileinfo
			if (GetFileInfo2(tszCurrentPath, &lpFileInfo, FALSE, &lpDirInfo))
			{
				if (!tszBestParent || (dwDepth != dwBestDepth))
				{
					// if we've descended into the filesystem record a new best parent...
					if (tszBestParent) FreeShared(tszBestParent);
					tszBestParent = AllocateShared(tszCurrentPath, NULL, 0);
					dwBestDepth = dwDepth;
				}
				if ( (lpFileInfo->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && (lpDirInfo->lpRootEntry->dwFileAttributes & FILE_ATTRIBUTE_LINK) )
				{
					if (!(lpDirInfo->lpRootEntry->dwFileAttributes & FILE_ATTRIBUTE_MASK) || !lpDirInfo->lpLinkedInfo)
					{
						dwError = ERROR_PATH_NOT_FOUND;
						CloseDirectory(lpDirInfo);
						CloseFileInfo(lpFileInfo);
						break;
					}

					if (bVfsExportedPathsOnly && (NtfsReparseMethod != NTFS_REPARSE_IGNORE))
					{
						if (!ReverseResolve(hMountFile, lpDirInfo->lpLinkedInfo->tszRealPath))
						{
							// rut ro, the path isn't exported via the VFS file so return an error
							dwError = ERROR_INVALID_FS_TARGET;
							CloseDirectory(lpDirInfo);
							CloseFileInfo(lpFileInfo);
							break;
						}
					}

					if (!(dwFlags & KEEP_CASE))
					{
						CopyMemory(tpOffset, lpDirInfo->lpRootEntry->tszFileName, dwPathItem * sizeof(TCHAR));
						CopyMemory(&tszCurrentPath[dwCurrentPath - dwPathItem],	lpDirInfo->lpRootEntry->tszFileName, dwPathItem * sizeof(TCHAR));
					}
				}
				else if (lpMountData->Length && !(dwFlags & KEEP_CASE))
				{
					CopyMemory(tpOffset, lpFileInfo->tszFileName, dwPathItem * sizeof(TCHAR));
					CopyMemory(&tszCurrentPath[dwCurrentPath - dwPathItem], lpFileInfo->tszFileName, dwPathItem * sizeof(TCHAR));
				}

#if 0
				if ( !(lpFileInfo->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && (lpFileInfo->dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) && bVfsExportedPathsOnly)
				{
					// We must be a symbolic link to another file, need to check the path!
					// TODO!
				}
#endif

				if (lpFileInfo->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
				{
					if (lpFileInfo->dwFileMode & S_SYMBOLIC)
					{
						tszLink	= (LPTSTR)FindFileContext(SYMBOLICLINK, &lpFileInfo->Context);
						bOverride = FALSE;
					}
					else
					{
						tszLink = NULL;
					}
					if (( VirtualPath.len + 1 >= _MAX_PWD) || (bKeepLinks && (VirtualPath.Symlen + 1 >= _MAX_PWD)))
					{
						dwError	= ERROR_INVALID_NAME;
					}
					//	Check access
					else if ((dwFlags & IGNORE_PERMS) || Access(lpUserFile, lpFileInfo, _I_READ | (bOverride ? _I_PASS : 0)))
					{
						VirtualPath.pwd[VirtualPath.len++] = '/';
						VirtualPath.pwd[VirtualPath.len]   = 0;
						if (bKeepLinks && !dwFollowingLinks)
						{
							VirtualPath.Symbolic[VirtualPath.Symlen++] = '/';
							VirtualPath.Symbolic[VirtualPath.Symlen]   = 0;
						}
						//	Handle symbolic link
						if (tszLink && (dwLink = _tcslen(tszLink)) > 0 &&
							(! (dwFlags & TYPE_LINK) || ! bStringEnd))
						{
							dwLen	= _tcslen(tpSlash);
							tszNewTemp = tszTemp = (LPTSTR)Allocate("PWD:Link", (dwLen + dwLink + 4) * sizeof(TCHAR));
							if (tszTemp)
							{
								FreeShared(tszCurrentPath);
								tszCurrentPath	= NULL;

								//	Remove leading '/' characters
								if (tszLink[0] == _TEXT('/'))
								{
									dwLink--;
									while ((++tszLink)[0] == _TEXT('/')) dwLink--;
									VirtualPath.len	    = 1;
									VirtualPath.pwd[0]  = '/';
									VirtualPath.pwd[1]  = 0;
								}
								else
								{
									// it's relative, so undo one virtual path level
									VirtualPath.len -= dwPathItem + 1;
									VirtualPath.pwd[VirtualPath.len] = 0;
								}
								CopyMemory(tszTemp, tszLink, dwLink * sizeof(TCHAR));
								tszTemp += dwLink;
								*tszTemp++ = _T('/');
								if (bKeepLinks)
								{
									*tszTemp++ = _T('|'); // '|' should be illegal in links...
									*tszTemp++ = _T('/');
									dwFollowingLinks++;
								}
								CopyMemory(tszTemp, tpSlash, dwLen * sizeof(TCHAR));
								tszTemp[dwLen] = 0;
								bStringEnd	= (! dwLink ? TRUE : FALSE);

								if (tszNewChangeTo) Free(tszNewChangeTo);
								tszNewChangeTo = tpSlash = tszNewTemp;
							}
							else dwError	= ERROR_OUTOFMEMORY;
						}
					}
					else dwError	= GetLastError();
				}
				else if (dwFlags & TYPE_DIRECTORY)
				{
					//	Not a directory
					dwError	= ERROR_DIRECTORY;
				}
				CloseDirectory(lpDirInfo);
				CloseFileInfo(lpFileInfo);
				bFileExists	= TRUE;
			}
		}

		if ((lpMountData->Initialized) && (lpMountData->lpVirtualDirEvent != NULL))
		{
			// It has a virtual dir prefix.  At this point we don't care if
			// it's a valid file or dir because that would involve calling the
			// script which is expensive.  Finished resolving the full VFS
			// path and then we'll examine it at the end.
			FreeShared(tszCurrentPath);
			tszCurrentPath	= NULL;
			dwCurrentPath   = 0;
			if (!bStringEnd)
			{
				VirtualPath.pwd[VirtualPath.len++] = '/';
				VirtualPath.pwd[VirtualPath.len]   = 0;
				if (bKeepLinks)
				{
					VirtualPath.Symbolic[VirtualPath.Symlen++] = '/';
					VirtualPath.Symbolic[VirtualPath.Symlen]   = 0;
				}
			}
			continue;
		}

		//	Existence checks
		if (! bFileExists && (dwError == NO_ERROR) )
		{
			if ((dwFlags & EXISTS) || ! bStringEnd)
			{
				dwError	= ERROR_PATH_NOT_FOUND;
			}
			else
			{
				// it doesn't have to exist and it's the last component...
				if (!tszBestParent)
				{
					// we never got PWD_Resolve to return a path to a parent directory we know exists, so try
					// to find the parent of the path we are looking for now...
					dwLink = VirtualPath.len;
					while (dwLink && VirtualPath.pwd[--dwLink - 1] != _TEXT('/'));
					if (dwLink)
					{
						VirtualPath.pwd[dwLink - 1]	= _T('\0');

						lpMountData->Initialized	= FALSE;
						if (!(tszBestParent = PWD_Resolve(VirtualPath.pwd, hMountFile, lpMountData, TRUE, 0)))
						{
							dwError = GetLastError();
						}
						VirtualPath.pwd[dwLink - 1]	= _T('/');
					}
				}

				if (tszBestParent && (tszBestParent != tszCurrentPath))
				{
					// The problem is when dealing with merged/raided directories is the tszCurrentPath string
					// will be for the last listed real directory for the mountpoint which may not actually
					// have the full path to the item as it may have been found on a different path, so use
					// the path that got us this far...
					if (tszCurrentPath) FreeShared(tszCurrentPath);
					dwLen = _tcslen(tszBestParent);
					if (tszCurrentPath = (LPTSTR)AllocateShared(NULL, _T("PWD:BestParent"), (dwLen + dwPathItem + 2) * sizeof(TCHAR)))
					{
						dwCurrentPath = dwLen;
						CopyMemory(tszCurrentPath, tszBestParent, dwLen * sizeof(TCHAR));
						tszCurrentPath[dwCurrentPath++] = _T('\\');
						CopyMemory(&tszCurrentPath[dwCurrentPath], tszChangeTo, dwPathItem * sizeof(TCHAR));
						dwCurrentPath += dwPathItem;
						tszCurrentPath[dwCurrentPath] = 0;
						// MountData will be invalid here since the item doesn't exist
					}
					else dwError = GetLastError();
				}
				else
				{
					if (tszCurrentPath) FreeShared(tszCurrentPath);
					tszCurrentPath = NULL;
				}
				dwLastError	= ERROR_FILE_NOT_FOUND;
			}
		}
	}

	if (tszNewChangeTo) Free(tszNewChangeTo);
	if (tszBestParent)  FreeShared(tszBestParent);

	if (dwError == NO_ERROR && !tszCurrentPath && !lpMountData->Initialized)
	{
		// handle the CWD '/' or CWD '.' cases...
		if (tszCurrentPath = PWD_Resolve(VirtualPath.pwd, hMountFile, lpMountData, TRUE, 0))
		{
			dwCurrentPath = lpMountData->dwLastPath;
		}
	}

	//	Check result
	if ((lpMountData->Initialized) && (lpMountData->lpVirtualDirEvent != NULL))
	{
		FreeShared(tszCurrentPath);
		tszCurrentPath = NULL;
		dwCurrentPath = 0;

		if ( (dwFlags & VIRTUAL_DONE) || !(tpSlash = strrchr(VirtualPath.pwd, '/')) )
		{
			dwError = ERROR_PATH_NOT_FOUND;
		}

		if (dwError != NO_ERROR)
		{
			ERROR_RETURN(dwError, NULL);
		}

		// ok, it's a virtual path of some sort...

		// There are now two possibilities.  The first is the path is a true virtual
		// dir which means it's a virtual mountpoint/event or it was declared as
		// a virtual subdir by a virtual parent.  These directories don't exist as
		// real paths anywhere so no real directory path can be returned so it will
		// return NULL, but the 'Pwd' variable will have it's virtual path, the
		// lpVirtualDirEvent and lpMountPoint variables all updated if the VIRTUAL_PWD
		// flag was passed in.
		//
		// The second possibility is the path is actually a link to a real file/directory
		// in which case we just return the information about the target of the link.

		// First, let's see if we can resolve the path with already available information
		// and the VIRTUAL_UPDATE flag wasn't passed which would invalidate the cache.

		lpVirtualDirEvent = lpMountData->lpVirtualDirEvent;
		lpVirtualDir      = hMountFile->lpVirtualDirArray[lpVirtualDirEvent->dwId];
		lpVirtualNew      = NULL;
		lpVirtualInfo     = NULL;
		tszTemp           = NULL;
		dwCurrentPath     = VirtualPath.len;
		if (VirtualPath.pwd[dwCurrentPath-1] == '/') dwCurrentPath--;

		if (lpVirtualDirEvent->tszPrivate && HavePermission(lpUserFile, lpVirtualDirEvent->tszPrivate) && HasFlag(lpUserFile, "MV"))
		{
			ERROR_RETURN(ERROR_PATH_NOT_FOUND, FALSE);
		}

		// This section checks to see if we have the directory cached, or else if we have the parent cached and can
		// look up the child.
		if (lpVirtualDir && !(dwFlags & VIRTUAL_NOCACHE))
		{
			// see if we have the directory cached as a pure virtual directory
			if ((lpVirtualDir->dwLen == dwCurrentPath+1) && !strnicmp(lpVirtualDir->pwd, VirtualPath.pwd, dwCurrentPath))
			{
				lpVirtualNew = lpVirtualDir;
			}
			else
			{
				*tpSlash = 0;
				dwPathItem = strlen(VirtualPath.pwd);
				*tpSlash = '/';
				if ((lpVirtualDir->dwLen == dwPathItem+1) && !strnicmp(lpVirtualDir->pwd, VirtualPath.pwd, dwPathItem))
				{
					bFileExists = TRUE;
					lpVirtualInfo = VirtualDirGetInfo(lpVirtualDir, tpSlash+1);
					if (lpVirtualInfo)
					{
						if (lpVirtualInfo->tszLink)
						{
							tszTemp = lpVirtualInfo->tszLink;
							if (!stricmp(Pwd->pwd, tszTemp))
							{
								// SPECIAL CASE: a symbolic link that resolves to the same dir means clear the glob cache
								// everything else stays the same so just return.
								lpVirtualDir->szLastGlob[0] = 0;
								Pwd->lpVirtualDirEvent = lpVirtualDirEvent;
								SetLastError(IO_VIRTUAL_DIR);
								return NULL;
							}
						}
						else if (!(lpVirtualInfo->lpFileInfo->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
						{
							// pure virtual files can't be manipulated/entered
							ERROR_RETURN(IO_VIRTUAL_DIR, NULL);
						}
						// either tszTemp is set if it's a link to a file or a directory
						// or it's a virtual sub dir and we will load it below... 
					}
					else
					{
						dwFlags |= VIRTUAL_UPDATE;
					}
				}
			}
		}

		// if we didn't find it in the cache...
		if (!lpVirtualNew && !tszTemp)
		{
			lpVirtualNew = VirtualDirLoad(hMountFile, lpMountData->lpVirtualDirEvent, VirtualPath.pwd, NULL, (dwFlags & VIRTUAL_UPDATE), FALSE, (dwFlags & EXISTS), tszCommand);
			if (lpVirtualNew)
			{
				if (lpVirtualNew->szTarget[0] != 0)
				{
					tszTemp = lpVirtualNew->szTarget;
					if (!stricmp(Pwd->pwd, tszTemp))
					{
						// SPECIAL CASE: a virtual dir link that resolves to the same dir means clear the glob cache
						// everything else stays the same so just return.
						lpVirtualDir->szLastGlob[0] = 0;
						Pwd->lpVirtualDirEvent = lpVirtualDirEvent;
						SetLastError(IO_VIRTUAL_DIR);
						return NULL;
					}
					if (lpVirtualNew->szTarget)		lpVirtualNew = NULL;
				}
			}
		}

		if (tszTemp)
		{
			if (dwFlags & VIRTUAL_ONCE) dwFlags |= VIRTUAL_DONE;
			tszPath = PWD_CWD2(lpUserFile, Pwd, tszTemp, hMountFile, lpMountData, dwFlags | VIRTUAL_ONCE, lpUser, tszCommand, Args);
			if (!tszPath) return NULL;
			// It resolved to a real path, that's fine, but update it with the associated virtual event...
			Pwd->lpVirtualDirEvent = lpVirtualDirEvent;
			if (!bKeepLinks) return tszPath;
			// ok, need to fix up the virtual dir path here
			CopyMemory(Pwd->Symbolic, VirtualPath.Symbolic, (VirtualPath.Symlen+1)*sizeof(TCHAR));
			Pwd->Symlen = VirtualPath.Symlen;
			dwLink = GetFileAttributes(tszPath);
			if ((dwLink != INVALID_FILE_ATTRIBUTES) && (dwLink & FILE_ATTRIBUTE_DIRECTORY) && Pwd->Symlen)
			{
				// we have a directory, make sure symbolic path ends in a '/'.
				if (Pwd->Symbolic[Pwd->Symlen-1] != '/')
				{
					Pwd->Symbolic[Pwd->Symlen++] = '/';
					Pwd->Symbolic[Pwd->Symlen]   = 0;
				}
			}
			return tszPath;
		}

		if (!lpVirtualNew || !lpVirtualNew->dwLen)
		{
			if (dwFlags & EXISTS)
			{
				ERROR_RETURN(ERROR_PATH_NOT_FOUND, NULL);
			}
			ERROR_RETURN(IO_VIRTUAL_DIR, NULL);
		}

		if (!(dwFlags & VIRTUAL_PWD))
		{
			// it's in a virtual dir or not found
			ERROR_RETURN(IO_VIRTUAL_DIR, NULL);
		}

		PWD_Free(Pwd);
		CopyMemory(Pwd->pwd, lpVirtualNew->pwd, lpVirtualNew->dwLen+1);
		Pwd->len = lpVirtualNew->dwLen;
		Pwd->lpVirtualDirEvent = lpVirtualDirEvent;

		Pwd->lpMountPoint = NULL;
		Pwd->iMountIndex  = 0;
		strcpy_s(Pwd->SectionName, sizeof(Pwd->SectionName), szDefaultSectionName);
		Pwd->CreditSection = iDefaultCreditSection;
		Pwd->ShareSection  = iDefaultShareSection;
		Pwd->StatsSection  = iDefaultStatsSection;
		if (bKeepLinks)
		{
			CopyMemory(Pwd->Symbolic, lpVirtualNew->pwd, lpVirtualNew->dwLen+1);
			Pwd->Symlen = lpVirtualNew->dwLen;
		}

		// setting not found error in case we end up printing it somehow.
		SetLastError(IO_VIRTUAL_DIR);
		return NULL;
	}

	if ((dwError != NO_ERROR) || ! tszCurrentPath)
	{
		if (tszCurrentPath) FreeShared(tszCurrentPath);
		if (dwError == NO_ERROR)
		{
			if (bStringEnd)
			{
				dwError = ERROR_FILE_NOT_FOUND;
			}
			else
			{
				dwError = ERROR_PATH_NOT_FOUND;
			}
		}
		ERROR_RETURN(dwError, NULL);
	}

	PWD_Free(Pwd);
	//	Update virtual path structure
	if (!bKeepLinks)
	{
		VirtualPath.Symlen = 0;
		VirtualPath.Symbolic[0] = 0;
	}
	PWD_Copy(&VirtualPath, Pwd, FALSE);
	Pwd->l_RealPath	= dwCurrentPath;
	Pwd->RealPath	= tszCurrentPath;
	Pwd->lpVirtualDirEvent = NULL;
	Pwd->lpMountPoint = lpMountData->Resume;
	Pwd->iMountIndex  = lpMountData->Last;
	Config_Get_Section(VirtualPath.pwd, Pwd->SectionName, &Pwd->CreditSection,
		&Pwd->StatsSection, &Pwd->ShareSection);

	return tszCurrentPath;
}


//	Flags ( TYPE_LINK, TYPE_DIRECTORY, EXISTS, KEEP_CASE, VIRTUAL_UPDATE, VIRTUAL_PWD, VIRTUAL_DONE )
LPTSTR PWD_CWD(LPUSERFILE lpUserFile, PVIRTUALPATH Pwd, LPTSTR tszChangeTo, MOUNTFILE hMountFile, DWORD dwFlags)
{
	MOUNT_DATA MountData;

	return PWD_CWD2(lpUserFile, Pwd, tszChangeTo, hMountFile, &MountData, dwFlags, NULL, NULL, NULL);
}




// take Path return normalized path, start from optional CWD (else /)
// normalized must be at least _MAX_PWD size.
// '/' at start or '//' in middle resets path to root (/) unlike during
// vfs link following where multiple /'s are treated as a single divider.
BOOL PWD_Normalize(LPTSTR tszPath, LPTSTR tszNormalized, LPTSTR tszCWD)
{
	LPTSTR tszNorm, tszSlash, tszPrev;
	DWORD dwNorm, dwNext;

	tszNorm = tszNormalized;
	if (!tszCWD || (*tszPath == _T('/')))
	{
		dwNorm = 1;
		*tszNorm++ = _T('/');
	}
	else
	{
		dwNorm = _tcslen(tszCWD);
		CopyMemory(tszNorm, tszCWD, dwNorm);
		tszNorm += dwNorm;
	}

	tszSlash = tszPath;
	for ( ; tszSlash && *tszPath ; tszPath = tszSlash + 1)
	{
		tszSlash = _tcschr(tszPath, _T('/'));

		if (!tszSlash)
		{
			dwNext = _tcslen(tszPath);
		}
		else
		{
			dwNext = (tszSlash - tszPath)/sizeof(TCHAR);
			if (!dwNext)
			{
				if (tszSlash[1])
				{
					// not end of string so double slashes (//) found or start of path
					tszNorm = tszNormalized;
					dwNorm = 1;
					*tszNorm++ = _T('/');
					continue;
				}
				if (dwNorm > 0 && (tszNorm[-1] == _T('/')))
				{
					// normalized path already ends in a / which means it's root...
					continue;
				}
				// path ended with a '/' so just add that to end
				if (++dwNorm > _MAX_PWD)
				{
					*tszNormalized = 0;
					SetLastError(ERROR_PATH_NOT_FOUND);
					return FALSE;
				}
				*tszNorm++ = _T('/');
				break;
			}
		}

		if (*tszPath == '.')
		{
			if (dwNext == 1)
			{
				// handle ./ or . which is a NOOP
				continue;
			}
			if (dwNext == 2)
			{
				// handle '..' or up one dir case
				tszPrev = tszNorm-1;
				while ((tszPrev >= tszNormalized) && (*--tszPrev != _T('/')));
				if (tszPrev < tszNormalized)
				{
					*tszNormalized = 0;
					SetLastError(ERROR_PATH_NOT_FOUND);
					return FALSE;
				}
				tszNorm = tszPrev+1;
				dwNorm = (tszNorm - tszNormalized)/sizeof(TCHAR);
				continue;
			}
			// it's a file/dir just starting with a . so process it normally
		}

		if (tszSlash) dwNext++;
		dwNorm += dwNext;
		if (dwNorm > _MAX_PWD)
		{
			*tszNormalized = 0;
			SetLastError(ERROR_PATH_NOT_FOUND);
			return FALSE;
		}

		CopyMemory(tszNorm, tszPath, dwNext);
		tszNorm += dwNext;
	}
	*tszNorm = 0;
	return TRUE;
}



INT MountCacheCompare(LPMOUNTCACHE *lpItem1, LPMOUNTCACHE *lpItem2)
{
	return stricmp(lpItem1[0]->szFileName, lpItem2[0]->szFileName);
}




BOOL MountFile_Init(BOOL bFirstInitialization)
{
	LPVIRTUALDIREVENT lpVirtualDirEvent;
	TCHAR	          pBuffer[_INI_LINE_LENGTH + 1], tszName[_MAX_NAME + 1];
	LPVOID	          lpOffset;
	LPTSTR            tszEvent, tszPrivate;
	DWORD             n, dwName, dwEvent, dwPrivate;

	if (! bFirstInitialization) return TRUE;

	lpMountCache		= NULL;
	dwMountCache		= 0;
	dwMountCacheSize	= 0;
	//	Initialize lock item
	if (! InitializeCriticalSectionAndSpinCount(&csMountCache, 100)) return FALSE;

	Config_Get_Section(_T("/"), szDefaultSectionName, &iDefaultCreditSection, &iDefaultStatsSection, &iDefaultShareSection);
	strcpy_s(szDefaultSectionName, sizeof(szDefaultSectionName), "*_VIRTUAL_*");

	// Virtual dir changes require a restart, they cannot be rehashed!
	ZeroMemory(KnownVirtualDirEvents, sizeof(KnownVirtualDirEvents));

	for (n=1, lpOffset = NULL ; Config_Get_Linear(&IniConfigFile, _T("Virtual_Dirs"), tszName, pBuffer, &lpOffset) ; n++)
	{
		if (dwKnownVirtualDirEvents >= MAX_VIRTUAL_DIR_MOUNTPOINTS)
		{
			Config_Get_Linear_End(&IniConfigFile);
			break;
		}

		// verify name begins and ends with a '/' and isn't the root dir
		dwName = _tcslen(tszName);
		if (!dwName)
		{
			Putlog(LOG_ERROR, _T("[Virtual_Dirs] entry #%u - missing the pathname.\r\n"), n);
			continue;
		}
		if (tszName[0] != _T('/'))
		{
			Putlog(LOG_ERROR, _T("[Virtual_Dirs] entry #%u - path doesn't start with a slash: %s\r\n"), n, tszName);
			continue;
		}
		if (dwName == 1)
		{
			Putlog(LOG_ERROR, _T("[Virtual_Dirs] entry #%u - the root dir '/' cannot be virtual.\r\n"), n);
			continue;
		}
		if (tszName[dwName-1] == _T('/'))
		{
			dwName--;
		}
		dwEvent = _tcslen(pBuffer);
		if (!dwEvent)
		{
			Putlog(LOG_ERROR, _T("[Virtual_Dirs] entry #%u - no event defined for path: %s\r\n"), n, tszName);
			continue;
		}
		tszEvent = pBuffer;
		if (*tszEvent == _T('"'))
		{
			// it's a private/hidden dir identifier.
			tszPrivate = ++tszEvent;
			if (!(tszEvent = _tcschr(tszEvent, _T('"'))))
			{
				Putlog(LOG_ERROR, _T("[Virtual_Dirs] entry #%u - unclosed quote around hidden/private setting: %s\r\n"), n, tszName);
				continue;
			}
			*tszEvent++ = 0;
			while (*tszEvent && (*tszEvent == _T(' ') || *tszEvent == _T('\t'))) tszEvent++;
			dwEvent = _tcslen(tszEvent);
			if (!dwEvent)
			{
				Putlog(LOG_ERROR, _T("[Virtual_Dirs] entry #%u - no event defined for path: %s\r\n"), n, tszName);
				continue;
			}
		}
		else
		{
			tszPrivate = 0;
		}

		if (_tcsnicmp(tszEvent, _T("TCL "), 4))
		{
			Putlog(LOG_ERROR, _T("[Virtual_Dirs] entry #%u - only TCL module supported: %s\r\n"), n, tszName);
			continue;
		}
		dwEvent -= 4;
		tszEvent += 4;
		while (*tszEvent && dwEvent && (*tszEvent == _T(' ') || *tszEvent == _T('\t')))
		{
			tszEvent++;
			dwEvent--;
		}
		if (!dwEvent)
		{
			Putlog(LOG_ERROR, _T("[Virtual_Dirs] entry #%u - missing script filename for event: %s\r\n"), n, tszName);
			continue;
		}

		lpVirtualDirEvent = &KnownVirtualDirEvents[dwKnownVirtualDirEvents];
		lpVirtualDirEvent->dwId = dwKnownVirtualDirEvents++;
		if (tszPrivate)
		{
			dwPrivate = _tcslen(tszPrivate);
			lpVirtualDirEvent->tszPrivate = (LPTSTR) Allocate(_T("Virtual private"), (dwName+1)*sizeof(TCHAR));
			if(!lpVirtualDirEvent->tszPrivate)
			{
				dwKnownVirtualDirEvents--;
				Config_Get_Linear_End(&IniConfigFile);
				return FALSE;
			}
			_tcsncpy_s(lpVirtualDirEvent->tszPrivate, dwPrivate+1, tszPrivate, dwPrivate);
		}
		else
		{
			lpVirtualDirEvent->tszPrivate = NULL;
		}

		lpVirtualDirEvent->tszName = (LPTSTR) Allocate(_T("Virtual name"), (dwName+1)*sizeof(TCHAR));
		if(!lpVirtualDirEvent->tszName)
		{
			dwKnownVirtualDirEvents--;
			if (lpVirtualDirEvent->tszPrivate) Free(lpVirtualDirEvent->tszPrivate);
			Config_Get_Linear_End(&IniConfigFile);
			return FALSE;
		}
		lpVirtualDirEvent->dwName   = dwName;
		_tcsncpy_s(lpVirtualDirEvent->tszName, dwName+1, tszName, dwName+1);
		lpVirtualDirEvent->tszName[dwName] = 0;


		lpVirtualDirEvent->tszEvent = (LPTSTR) Allocate(_T("Virtual event"), (dwEvent+1)*sizeof(TCHAR));
		if (!lpVirtualDirEvent->tszEvent)
		{
			dwKnownVirtualDirEvents--;
			if (lpVirtualDirEvent->tszPrivate) Free(lpVirtualDirEvent->tszPrivate);
			Free(lpVirtualDirEvent->tszName);
			Config_Get_Linear_End(&IniConfigFile);
			return FALSE;
		}
		lpVirtualDirEvent->dwEvent  = dwEvent;
		_tcsncpy_s(lpVirtualDirEvent->tszEvent, dwEvent+1, tszEvent, dwEvent);
	}

	return TRUE;
}



VOID MountFile_DeInit(VOID)
{
	DWORD n;

	//	Free memory
	for (;dwMountCache--;)
	{
		MountFile_Close(lpMountCache[dwMountCache]->lpCacheData);
		Free(lpMountCache[dwMountCache]);
	}
	Free(lpMountCache);

	for(n=0 ; n<dwKnownVirtualDirEvents; n++)
	{
		Free(KnownVirtualDirEvents[n].tszName);
		Free(KnownVirtualDirEvents[n].tszEvent);
		if (KnownVirtualDirEvents[n].tszPrivate) Free(KnownVirtualDirEvents[n].tszPrivate);
	}

	//	Delete lock item
	DeleteCriticalSection(&csMountCache);
}



VOID MountTable_Free(LPMOUNT_TABLE lpMountTable)
{
	DWORD	i, j;

	if (lpMountTable)
	{
		if (lpMountTable->lpEntries)
		{
			Free(lpMountTable->lpEntries->lpRealItemArray);
			Free(lpMountTable->lpEntries->VirtualItemArray);
			Free(lpMountTable->lpEntries);
		}

		for (i = 0;i < lpMountTable->dwMountPoints;i++)
		{
			for (j = 0;j < lpMountTable->lpMountPoints[i]->dwSubMounts;j++)
			{
				//	Free mount name
				Free(lpMountTable->lpMountPoints[i]->lpSubMount[j].szFileName);
			}

			if (lpMountTable->lpMountPoints[i]->lpNextTable)
			{
				//	Free next mount table
				MountTable_Free((LPMOUNT_TABLE)lpMountTable->lpMountPoints[i]->lpNextTable);
			}
			//	Free mount point
			Free(lpMountTable->lpMountPoints[i]);
		}
		Free(lpMountTable->lpMountPoints);
		Free(lpMountTable);
	}
}


// This does a depth first traversal of the parsed file adding real->vfs entries
// to the entry cache.  The trick here is to add the longer vfs paths first so when reverse
// resolving we can process them in an array in order to make it faster.  This solves some
// of the bizarre edge cases better...
BOOL MountFile_AddEntries(LPMOUNT_ENTRIES lpEntries, LPMOUNT_TABLE lpTable)
{
	LPMOUNT_ITEM  *lpRealArray, lpVirtArray, lpItem;
	LPMOUNT_POINT  lpPoint;
	DWORD i, j;

	for (i = 0; i < lpTable->dwMountPoints ; i++)
	{
		lpPoint = lpTable->lpMountPoints[i];
		if (lpPoint->lpNextTable && MountFile_AddEntries(lpEntries, lpPoint->lpNextTable))
		{
			// bail if an error recursing
			return TRUE;
		}
		for (j = 0 ; j < lpPoint->dwSubMounts ; j++)
		{
			if (lpEntries->dwEntries >= lpEntries->dwAllocatedSize)
			{
				lpEntries->dwAllocatedSize += 10;
				lpRealArray = (LPMOUNT_ITEM *) ReAllocate(lpEntries->lpRealItemArray, "MountFile:RealItemArray",
					sizeof(*lpEntries->lpRealItemArray) * lpEntries->dwAllocatedSize);
				if (!lpRealArray)
				{
					ERROR_RETURN(ERROR_NOT_ENOUGH_MEMORY, TRUE);
				}
				lpEntries->lpRealItemArray = lpRealArray;

				lpVirtArray = (LPMOUNT_ITEM) ReAllocate(lpEntries->VirtualItemArray, "MountFile:VirtualItemArray",
					sizeof(*lpEntries->VirtualItemArray) * lpEntries->dwAllocatedSize);
				if (!lpVirtArray)
				{
					ERROR_RETURN(ERROR_NOT_ENOUGH_MEMORY, TRUE);
				}
				lpEntries->VirtualItemArray = lpVirtArray;
			}

			lpEntries->lpRealItemArray[lpEntries->dwEntries] = &lpPoint->lpSubMount[j];
			lpItem = &lpEntries->VirtualItemArray[lpEntries->dwEntries];
			// can increment count here now that we know both array entries will be initialized
			lpEntries->dwEntries++;

			lpItem->dwFileName = lpPoint->dwPathLen;
			lpItem->szFileName = lpPoint->tszFullPath;
			if (!lpItem->szFileName)
			{
				ERROR_RETURN(ERROR_NOT_ENOUGH_MEMORY, TRUE);
			}
		}
	}
	return FALSE;
}



MOUNTFILE MountFile_Parse(LPVOID lpBuffer, DWORD dwBuffer, LPSTR szVfsFileName)
{
	MOUNTFILE           hMountFile;
	LPMOUNT_POINT		*lpMountPoints, lpMountPoint, lpPrevMount;
	LPMOUNT_TABLE		lpRootTable, lpTable;
	LPMOUNT_ENTRIES     lpEntries;
	LPMOUNT_ITEM        lpItem;
	LPSTR				szFileName;
	PCHAR				pEnd, pNewline, pQuote, pSlash, pVirtual, pLine, pLineEnd, pNewSlash;
	DWORD				dwError, dwLine, dwItem, dwFileName, i, dwLineNum, dwSize, dwVfsNameLen;
	CHAR                szDirName[MAX_PATH+1];
	WCHAR               wszErrBuf[256], *wszErr;

	pEnd	= &((PCHAR)lpBuffer)[dwBuffer];
	pLine	= (PCHAR)lpBuffer;
	dwError	= NO_ERROR;
	dwLineNum = 0;
	dwVfsNameLen = strlen(szVfsFileName);
	//	Allocate memory for main item
	hMountFile	= (MOUNTFILE)Allocate("MountFile:Main", sizeof(*hMountFile) + dwVfsNameLen);
	//	Allocate memory for root table
	lpRootTable	= (LPMOUNT_TABLE)Allocate("MountFile:Root", sizeof(*lpRootTable));
	// Allocate memory for real->vfs entries
	lpEntries = (LPMOUNT_ENTRIES)Allocate("MountFile:Entries", sizeof(*lpEntries));
	//	Verify allocation
	if (! hMountFile || !lpRootTable || !lpEntries)
	{
		Free(hMountFile);
		Free(lpRootTable);
		Free(lpEntries);
		ERROR_RETURN(ERROR_NOT_ENOUGH_MEMORY, NULL);
	}
	ZeroMemory(lpRootTable, sizeof(MOUNT_TABLE));
	ZeroMemory(hMountFile, sizeof(*hMountFile));
	ZeroMemory(lpEntries, sizeof(*lpEntries));
	hMountFile->dwFileName = dwVfsNameLen;
	strcpy_s(hMountFile->szFileName, dwVfsNameLen+1, szVfsFileName);
	lpRootTable->lpEntries = lpEntries;

	//	Find newlines - note: extra \n appended to end so last char always matches
	for (;pNewline = (PCHAR)memchr(pLine, '\n', pLine - pEnd);pLine = &pNewline[1])
	{
		dwLineNum++;
		
		for (pQuote = pLine ; pQuote < pNewline ; pQuote++)
		{
			// replace tabs with spaces
			if (*pQuote == '\t') *pQuote = ' ';
		}

		// squash the \r and any trailing spaces on line
		for (pLineEnd = pNewline ; pLineEnd >= pLine && isspace(*pLineEnd) ; pLineEnd--)
		{
			*pLineEnd = 0;
		}

		// find the first non-whitespace character in the line
		for (pQuote = pLine ; (pQuote < pNewline) && (*pQuote == ' ') ; pQuote++);

		// line is a comment, or blank, so just skip it
		if (*pQuote == '#' || *pQuote == '\r' || *pQuote == '\n' || *pQuote == 0) continue;

		if (*pLine != '"') {
			Putlog(LOG_ERROR, _T("VFS ERROR: file '%s', line #%u doesn't begin with a comment (#) or a double quote (\"): '%s'\r\n"),
				szVfsFileName, dwLineNum, pLine);
			continue;
		}

		dwLine	= pNewline - pLine;

		//	Find quote
		if (! (pQuote = (PCHAR)memchr(&pLine[1], '"', pNewline - &pLine[1])))
		{
			Putlog(LOG_ERROR, _T("VFS ERROR: file '%s', line #%u doesn't contain a closing double quote (\"): '%s'\r\n"),
				szVfsFileName, dwLineNum, pLine);
			continue;
		}

		// verify real path doesn't end with a '\'
		if (isalpha(pLine[1]) && (pLine[2] == ':') && (pLine[3] == '\\'))
		{
			if ((&pLine[3] != &pQuote[-1]) && (pQuote[-1] == '\\'))
			{
				Putlog(LOG_ERROR, _T("VFS WARNING: file '%s', line #%u has a trailing backslash (\\) and it's not a drive specifier (i.e. c:\\):  '%s'\r\n"),
					szVfsFileName, dwLineNum, pLine);
				pQuote--;
			}
		}
		else if ((pLine[1] == '\\') && (pLine[2] == '\\'))
		{
			if ((&pLine[2] == &pQuote[-1]) || (pQuote[-1] == '\\'))
			{
				Putlog(LOG_ERROR, _T("VFS ERROR: file '%s', line #%u has a trailing backslash (\\) on a network drive specifier:  '%s'\r\n"),
					szVfsFileName, dwLineNum, pLine);
				continue;
			}
		}
		else if (pLine[1] == '.' && pLine[2] == '.')
		{
			Putlog(LOG_ERROR, _T("VFS ERROR: file '%s', line #%u has a real path that is relative (i.e. ..\\foo): '%s'\r\n"),
				szVfsFileName, dwLineNum, pLine);
			continue;
		}

		if (pQuote - pLine > MAX_PATH)
		{
			Putlog(LOG_ERROR, _T("VFS ERROR: file '%s', line #%u has a real path that is too long.\r\n"),
				szVfsFileName, dwLineNum);
			continue;
		}

		//	Find slash
		for (pSlash = &pQuote[1] ; (pSlash < pNewline) && (*pSlash == ' ') ; pSlash++);

		if (*pSlash != '/') {
			Putlog(LOG_ERROR, _T("VFS ERROR: file '%s', line #%u has a VFS path that doesn't begin with slash (/): '%s'\r\n"),
				szVfsFileName, dwLineNum, pLine);
			continue;
		}
		pVirtual = pSlash;

		strncpy_s(szDirName, sizeof(szDirName), &pLine[1], pQuote - pLine - 1);
		if (GetFileAttributes(szDirName) == INVALID_FILE_ATTRIBUTES)
		{
			dwSize = sizeof(wszErrBuf)/sizeof(wszErrBuf[0]);
			wszErr = FormatError(GetLastError(), wszErrBuf, &dwSize);

			Putlog(LOG_ERROR, _T("VFS WARNING: file '%s', line #%u has a real path that is invalid (error = %ws): %s\r\n"),
				szVfsFileName, dwLineNum, wszErr, pLine);
		}

		//	Add trailing '/'
		if ((pLineEnd++)[0] != '/') (pLineEnd++)[0]	= '/';

		lpTable	    = lpRootTable;
		lpPrevMount = NULL;
		lpItem      = NULL;

		for (;pNewSlash = (PCHAR)memchr(pSlash, '/', pLineEnd - pSlash);pSlash = &pNewSlash[1])
		{
			dwItem	= pNewSlash - pSlash;
			//	Check item length
			if (dwItem > MAX_PATH)
			{
				dwError	= ERROR_INVALID_MOUNT_ENTRY;
				break;
			}

			//	Find table
			for (i = 0;i < lpTable->dwMountPoints;i++)
			{
				if (lpTable->lpMountPoints[i]->dwName == dwItem &&
					! _strnicmp(lpTable->lpMountPoints[i]->szName, pSlash, dwItem)) break;
			}

			//	Check match
			if (i >= lpTable->dwMountPoints)
			{
				//	Append to table
				if (lpTable->dwMountPoints == lpTable->dwMountPointsAllocated)
				{
					//	Increase table size
					lpMountPoints	= (LPMOUNT_POINT *)ReAllocate(lpTable->lpMountPoints,
						"MountFile: MountList", sizeof(LPMOUNT_POINT) * (lpTable->dwMountPointsAllocated + 5));
					//	Verify re-allocation
					if (! lpMountPoints)
					{
						dwError	= ERROR_NOT_ENOUGH_MEMORY;
						break;
					}
					lpTable->dwMountPointsAllocated	+= 5;
					lpTable->lpMountPoints	= lpMountPoints;
				}
				//	Allocate memory for mount point
				dwSize = (lpPrevMount ? lpPrevMount->dwPathLen : 0);
				lpMountPoint	= (LPMOUNT_POINT)Allocate("MountFile:MountPoint", sizeof(MOUNT_POINT) + dwItem*2 + dwSize + 3);
				//	Verify allocation
				if (! lpMountPoint)
				{
					dwError	= ERROR_NOT_ENOUGH_MEMORY;
					break;
				}
				//	Update mountpoint structure
				ZeroMemory(lpMountPoint, sizeof(MOUNT_POINT));
				lpMountPoint->szName	= (LPSTR)((ULONG)lpMountPoint + sizeof(MOUNT_POINT));
				lpMountPoint->dwName	= dwItem;
				CopyMemory(lpMountPoint->szName, pSlash, dwItem);
				lpMountPoint->szName[dwItem]	= _T('\0');
				lpMountPoint->lpParent = lpPrevMount;
				lpMountPoint->dwPathLen = dwItem + 1;
				lpMountPoint->tszFullPath = &lpMountPoint->szName[dwItem+1];
				if (lpPrevMount)
				{
					CopyMemory(lpMountPoint->tszFullPath, lpPrevMount->tszFullPath, lpPrevMount->dwPathLen);
					CopyMemory(&lpMountPoint->tszFullPath[lpPrevMount->dwPathLen], pSlash, dwItem);
					lpMountPoint->dwPathLen += lpPrevMount->dwPathLen;
					lpMountPoint->tszFullPath[lpMountPoint->dwPathLen-1] = _T('/');
					lpMountPoint->tszFullPath[lpMountPoint->dwPathLen] = 0;
				}
				else
				{
					lpMountPoint->tszFullPath[0] = _T('/');
					lpMountPoint->tszFullPath[1] = 0;
				}

				lpTable->lpMountPoints[lpTable->dwMountPoints++] = lpMountPoint;
			}
			else lpMountPoint = lpTable->lpMountPoints[i];

			lpPrevMount = lpMountPoint;

			if (&pNewSlash[1] == pLineEnd)
			{
				//	Verify size
				if (lpMountPoint->dwSubMounts >= MAX_SUBMOUNTS)
				{
					dwError	= ERROR_INVALID_MOUNT_ENTRY;
					break;
				}
				//	Insert path
				dwFileName	= pQuote - &pLine[1];
				//	Allocate memory for filename
				szFileName	= (LPSTR)Allocate("MountFile:FileName", dwFileName + 1);
				//	Verify allocation
				if (! szFileName)
				{
					dwError	= ERROR_NOT_ENOUGH_MEMORY;
					break;
				}

				szFileName[dwFileName]	= _T('\0');
				CopyMemory(szFileName, &pLine[1], dwFileName);
				lpItem = &lpMountPoint->lpSubMount[lpMountPoint->dwSubMounts++];
				lpItem->dwFileName  = dwFileName;
				lpItem->szFileName  = szFileName;
			}
			else
			{
				if (! lpMountPoint->lpNextTable)
				{
					//	Allocate memory for new table
					lpMountPoint->lpNextTable	= Allocate("MountFile:SubTable", sizeof(MOUNT_TABLE));
					//	Verify allocation
					if (! lpMountPoint->lpNextTable)
					{
						dwError	= ERROR_NOT_ENOUGH_MEMORY;
						break;
					}
					ZeroMemory(lpMountPoint->lpNextTable, sizeof(MOUNT_TABLE));
				}
				lpTable	= (LPMOUNT_TABLE)lpMountPoint->lpNextTable;
			}
		}

		if (dwError != NO_ERROR) break;
	}

	if ((dwError == NO_ERROR) && MountFile_AddEntries(lpEntries, lpRootTable))
	{
		dwError = GetLastError();
	}

	//	Handle error
	if (dwError != NO_ERROR)
	{
		MountTable_Free(lpRootTable);
		Free(hMountFile);
		ERROR_RETURN(dwError, NULL);
	}
	lpRootTable->lShareCount    = 1;
	hMountFile->lpMountTable	= lpRootTable;

	return hMountFile;
}









VOID MountFile_Close(MOUNTFILE hMountFile)
{
	LPVIRTUALDIR        lpVirtualDir;
	DWORD               n;

	//	Sanity check
	if (! hMountFile) return;

	//	Decrement share count
	if (! InterlockedDecrement(&hMountFile->lpMountTable->lShareCount))
	{
		//	Free memory
		MountTable_Free(hMountFile->lpMountTable);
	}
	for (n=0 ; n<dwKnownVirtualDirEvents ; n++ )
	{
		lpVirtualDir = hMountFile->lpVirtualDirArray[n];
		if (lpVirtualDir)
		{
			VirtualDirInfoFree(lpVirtualDir, TRUE);
		}
	}
	Free(hMountFile);
}









MOUNTFILE MountFile_Open(LPSTR szFileName, LPFTPUSER lpFtpUser)
{
	LPMOUNTCACHE	lpCache;
	MOUNTFILE       hMountFile;
	LPVOID			lpMemory, lpReallocated;
	HANDLE			hFileHandle;
	LPVOID			lpBuffer;
	DWORD			dwFileName, dwFileSize, dwBytesRead;
	BOOL			bRead, bFree;
	INT				iResult;

	bRead		= FALSE;
	bFree		= TRUE;
	dwFileName	= strlen(szFileName);
	lpMemory	= Allocate("MountFile:CacheItem", sizeof(MOUNTCACHE) + dwFileName + 1);
	//	Verify allocation
	if (! lpMemory) return NULL;
	hMountFile  = NULL;

	lpCache	= (LPMOUNTCACHE)lpMemory;
	ZeroMemory(lpCache, sizeof(MOUNTCACHE));
	lpCache->szFileName	= szFileName;
	//	Open file
	hFileHandle	= CreateFile(szFileName, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
	//	Verify handle
	if (hFileHandle != INVALID_HANDLE_VALUE)
	{
		//	Get filetime
		GetFileTime(hFileHandle, NULL, NULL, &lpCache->ftCacheTime);

		EnterCriticalSection(&csMountCache);
		//	Check memory
		if (dwMountCacheSize == dwMountCache)
		{
			//	Allocate memory
			lpReallocated	= ReAllocate(lpMountCache,
				"MountCache:Array", sizeof(LPMOUNTCACHE) * (dwMountCache + 64));
			//	Verify allocation
			if (! lpReallocated)
			{
				goto FATAL;
			}
			dwMountCacheSize	+= 64;
			lpMountCache		= (LPMOUNTCACHE *)lpReallocated;
		}
		//	Insert new cache item
		iResult	= QuickInsert(lpMountCache, dwMountCache, lpCache, (QUICKCOMPAREPROC) MountCacheCompare);
		//	Check result
		if (iResult--)
		{
			//	Compare filetimes
			if (CompareFileTime(&lpCache->ftCacheTime, &lpMountCache[iResult]->ftCacheTime) ||
				! lpMountCache[iResult]->lpCacheData)
			{
				CopyMemory(&lpMountCache[iResult]->ftCacheTime, &lpCache->ftCacheTime, sizeof(FILETIME));
				bRead	= TRUE;
				lpCache	= lpMountCache[iResult];
				MountFile_Close(lpCache->lpCacheData);
				lpCache->lpCacheData	= NULL;
			}
			else lpCache	= lpMountCache[iResult];
		}
		else
		{
			dwMountCache++;
			lpCache->szFileName	= (LPSTR)((ULONG)lpCache + sizeof(MOUNTCACHE));
			CopyMemory(lpCache->szFileName, szFileName, dwFileName + 1);
			bRead	= TRUE;
			bFree	= FALSE;
		}
		//	Read file
		if (bRead)
		{
			dwFileSize	= GetFileSize(hFileHandle, NULL);
			//	Verify filesize
			if (dwFileSize != INVALID_FILE_SIZE &&
				dwFileSize < 10000000 && dwFileSize > 4)
			{
				//	Allocate read buffer
				lpBuffer	= Allocate("MountFile:ReadBuffer", dwFileSize + 1);
				//	Verify buffer
				if (lpBuffer)
				{
					if (ReadFile(hFileHandle, lpBuffer, dwFileSize, &dwBytesRead, NULL))
					{
						((PCHAR)lpBuffer)[dwBytesRead]	= '\n';
						lpCache->lpCacheData	= MountFile_Parse(lpBuffer, dwBytesRead + 1, lpCache->szFileName);
					}
					//	Free buffer
					Free(lpBuffer);
				}
			}			
		}
		//	Increment share count
		if (lpCache->lpCacheData)
		{
			hMountFile = Allocate("Mountfile:Main2", sizeof(*hMountFile) + lpCache->lpCacheData->dwFileName);
			if (hMountFile)
			{
				CopyMemory(hMountFile, lpCache->lpCacheData, sizeof(*hMountFile) + lpCache->lpCacheData->dwFileName);
				InterlockedIncrement(&hMountFile->lpMountTable->lShareCount);
				hMountFile->lpFtpUser = lpFtpUser;
			}
		}
FATAL:
		LeaveCriticalSection(&csMountCache);

		//	Close handle
		CloseHandle(hFileHandle);
	}
	//	Free cache item
	if (bFree) Free(lpMemory);

	return hMountFile;
}



BOOL GetVfsParentFileInfo(LPUSERFILE lpUserFile, MOUNTFILE hMountFile, PVIRTUALPATH pvpPath, LPFILEINFO *lppParentInfo, BOOL bRootOk)
{
	VIRTUALPATH   vpParent;
	MOUNT_DATA    MountData;
	LPVIRTUALDIR  lpVirtualDir;
	LPVIRTUALINFO lpVirtualInfo;
	LPFILEINFO    lpFileInfo, lpFakeInfo;   
	INT32         Id;

	if (pvpPath->len == 1)
	{
		// we can't get the parent of root!
		if (!bRootOk)
		{
			SetLastError(ERROR_PATH_NOT_FOUND);
			return FALSE;
		}
		return GetFileInfoNoCheck(pvpPath->RealPath, lppParentInfo);
	}

	PWD_Reset(&vpParent);
	PWD_Copy(pvpPath, &vpParent, FALSE);
	// not enabling virtual directory lookups, if it is one we'll return the special version...
	if (!PWD_CWD2(lpUserFile, &vpParent, _T(".."), hMountFile, &MountData, (EXISTS|TYPE_DIRECTORY|VIRTUAL_PWD), NULL, _T("PARENT"), NULL))
	{
		// if vpParent.lpVirtualDirEvent is set then the mountpoint isn't hidden to us...
		if (!vpParent.lpVirtualDirEvent || !(lpVirtualDir = hMountFile->lpVirtualDirArray[vpParent.lpVirtualDirEvent->dwId]))
		{
			return FALSE;
		}

		if (!lpVirtualDir->dwVirtualInfos || !(lpVirtualInfo = lpVirtualDir->lpVirtualInfoArray[0]) || strcmp(".", lpVirtualInfo->tszName))
		{
			return FALSE;
		}

		lpFileInfo = lpVirtualInfo->lpFileInfo;
		// we need to fake out a fileinfo structure from what we know about the root virtual fileinfo...
		if (!lpFileInfo || !(lpFakeInfo = Allocate("FakeVirtFileInfo", sizeof(FILEINFO) + lpFileInfo->dwFileName*sizeof(TCHAR))))
		{
			return FALSE;
		}

		CopyMemory(lpFakeInfo, lpFileInfo, sizeof(FILEINFO) + lpFileInfo->dwFileName*sizeof(TCHAR));

		// now we need to clean up one or two things since it could have been either a real or fake fileinfo...
		if (!lpFileInfo->lReferenceCount)
		{
			// it was a fake fileinfo already... that means uid/gid are the defaults, try to set them based on virtual info
			if ((Id = User2Uid(lpVirtualInfo->tszName)) >= 0)
			{
				lpFakeInfo->Uid = Id;
			}
			if ((Id = Group2Gid(lpVirtualInfo->tszGroup)) >= 0)
			{
				lpFakeInfo->Gid = Id;

			}
		}
		lpFakeInfo->lReferenceCount = 1;
		lpFakeInfo->dwFileMode     &= S_ACCESS;
		lpFakeInfo->Context.lpData  = NULL;
		lpFakeInfo->Context.dwData  = 0;
		lpFakeInfo->lpLinkedRoot    = NULL;

		*lppParentInfo = lpFakeInfo;
		return TRUE;
	}
	if (!GetFileInfoNoCheck(vpParent.RealPath, lppParentInfo))
	{
		PWD_Free(&vpParent);
		return FALSE;
	}
	PWD_Free(&vpParent);
	return TRUE;
}


static BOOL PreLoad_VFS_CallBack(PVIRTUALPATH lpVPath, LPFILEINFO lpFileInfo,
								 LPFILEINFO lpParentInfo, LPVFSPRELOAD lpPre)
{
	// don't really need to do anything here since just getting here meant the
	// directory was opened... return TRUE if we should still descend, else FALSE.
	InterlockedIncrement(lpPre->lplCount);
	return TRUE;
}



static DWORD WINAPI PreLoad_Directory(LPVFSPRELOADDIR lpPreDir)
{
	LPDIRECTORYINFO  lpDirInfo;

	// Because this doesn't fake out subdirs it could take a while on large fanouts...
	if (lpDirInfo = OpenDirectory(lpPreDir->szDirName, TRUE, FALSE, FALSE, NULL, NULL))
	{
		InterlockedIncrement(lpPreDir->lpPre->lplCount);
		CloseDirectory(lpDirInfo);
	}
	if (lpPreDir->hSema != INVALID_HANDLE_VALUE)
	{
		ReleaseSemaphore(lpPreDir->hSema, 1, NULL);
		Free(lpPreDir);
	}
	return 0;
}


static DWORD WINAPI PreLoad_Point(LPVFSPRELOADPOINT lpPrePoint)
{
	VFSPRELOADTABLE  PreTable;
	VFSPRELOADDIR   *lpPreDir;
	LPMOUNT_POINT    lpMountPoint;
	LPDIRECTORYINFO  lpDirInfo;
	DWORD            m;
	HANDLE           hSema, hThread;

	hSema = lpPrePoint->lpPre->hSema;

	lpMountPoint = lpPrePoint->lpMountPoint;

	for(m=0; m < lpMountPoint->dwSubMounts ; m++)
	{
		if (dwDaemonStatus != DAEMON_ACTIVE) break;
		if (hSema != INVALID_HANDLE_VALUE)
		{
			// we have a valid semaphore, so spawn threads to do work
			lpPreDir = Allocate("PreDir", sizeof(*lpPreDir));
			if (lpPreDir)
			{
				lpPreDir->lpPre     = lpPrePoint->lpPre;
				lpPreDir->szDirName = lpMountPoint->lpSubMount[m].szFileName;
				lpPreDir->hSema     = hSema;

				InterlockedIncrement(&lpPrePoint->lpPre->lThreads);
				hThread	= CreateThread(0, 0, PreLoad_Directory, lpPreDir, 0, 0);
				if (hThread != INVALID_HANDLE_VALUE)
				{
					SetThreadPriority(hThread, THREAD_PRIORITY_BELOW_NORMAL);
					CloseHandle(hThread);
					continue;
				}
				InterlockedDecrement(&lpPrePoint->lpPre->lThreads);
				Free(lpPreDir);
			}
		}
		if (lpDirInfo = OpenDirectory(lpMountPoint->lpSubMount[m].szFileName, TRUE, FALSE, FALSE, NULL, NULL))
		{
			InterlockedIncrement(lpPrePoint->lpPre->lplCount);
			CloseDirectory(lpDirInfo);
		}
	}

	// process children
	if (lpMountPoint->lpNextTable && (dwDaemonStatus == DAEMON_ACTIVE))
	{
		PreTable.lpPre        = lpPrePoint->lpPre;
		PreTable.lpMountTable = lpMountPoint->lpNextTable;
		PreLoad_Table(&PreTable);
	}

	if (lpPrePoint->bThreaded)
	{
		// signal parent we are done and free memory
		ReleaseSemaphore(lpPrePoint->lpPre->hSema, 1, NULL);
		Free(lpPrePoint);
	}

	return 0;
}


static VOID
PreLoad_Table(LPVFSPRELOADTABLE lpPreTable)
{
	LPMOUNT_TABLE    lpMountTable;
	LPMOUNT_POINT    lpMountPoint;
	DWORD            n;
	HANDLE           hThread, hSema;
	VFSPRELOADPOINT  PrePoint, *lpPrePoint;
	VFSPRELOADTABLE  PreTable;

	lpMountTable = lpPreTable->lpMountTable;
	hSema = lpPreTable->lpPre->hSema;

	PrePoint.lpPre     = lpPreTable->lpPre;
	PrePoint.bThreaded = FALSE;

	PreTable.lpPre = lpPreTable->lpPre;

	for (n=0 ; n < lpMountTable->dwMountPoints ; n++)
	{
		if (dwDaemonStatus != DAEMON_ACTIVE) break;
		lpMountPoint = lpMountTable->lpMountPoints[n];

		if (hSema != INVALID_HANDLE_VALUE)
		{
			// we have a valid semaphore, so spawn threads to do work
			lpPrePoint = Allocate("PrePoint", sizeof(*lpPrePoint));
			if (lpPrePoint)
			{
				lpPrePoint->lpPre        = lpPreTable->lpPre;
				lpPrePoint->lpMountPoint = lpMountPoint;
				lpPrePoint->bThreaded    = TRUE;

				InterlockedIncrement(&lpPreTable->lpPre->lThreads);
				hThread	= CreateThread(0, 0, PreLoad_Point, lpPrePoint, 0, 0);
				if (hThread != INVALID_HANDLE_VALUE)
				{
					SetThreadPriority(hThread, THREAD_PRIORITY_BELOW_NORMAL);
					CloseHandle(hThread);
					continue;
				}
				InterlockedDecrement(&lpPrePoint->lpPre->lThreads);
				Free(lpPrePoint);
			}
		}

		PrePoint.lpMountPoint = lpMountPoint;
		PreLoad_Point(&PrePoint);

		// children are not likely to be subdirs of this directory on disk
		// else they wouldn't be mountpoints...
		if (lpMountPoint->lpNextTable)
		{
			PreTable.lpMountTable = lpMountPoint->lpNextTable;
			PreLoad_Table(&PreTable);
		}
	}
}




static DWORD WINAPI PreLoad_RecursiveDirectory(LPVFSPRELOAD lpVfsPre)
{
	RecursiveAction(NULL, lpVfsPre->hMountFile, lpVfsPre->pBuffer, FALSE, TRUE, lpVfsPre->depth, PreLoad_VFS_CallBack, lpVfsPre);
	ReleaseSemaphore(lpVfsPre->hSema, 1, NULL);
	Free(lpVfsPre);
	return 0;
}






// Just going to ignore the fact that we actually have worker threads pre-created for this
// type of stuff and go ahead and just create threads as we need them during start only.
static VOID
PreLoad_MountFile(MOUNTFILE hMountFile, BOOL bLogCount)
{
	LPMOUNT_TABLE    lpMountTable;
	INT              i, iThreads, iLen;
	LPVOID	         lpOffset;
	TCHAR	         tszName[_MAX_NAME + 1];
	LPTSTR           tszRealPath;
	MOUNT_DATA       MountData;
	VFSPRELOAD       PreLoad, *lpPreLoad2;
	HANDLE           hThread;
	DWORD            dwAttr, dwArray, n;
	volatile LONG    lCount, lPoints;
	VFSPRELOADTABLE  PreLoadTable;
	LPTSTR           tszPath, tszPathArray[50];
	INT              iDepthArray[50];

	lpMountTable = hMountFile->lpMountTable;
	iThreads = 0;
	lCount = 0;

	ZeroMemory(&PreLoad, sizeof(PreLoad));
	PreLoad.lplCount = &lCount;
	if (bLogCount)
	{
		Putlog(LOG_GENERAL, "PRELOAD: \"begin\" \"%s\"\r\n", hMountFile->szFileName);
		PreLoad.hSema = CreateSemaphore(NULL, 0, 10000, NULL);
	}
	else
	{
		PreLoad.hSema = INVALID_HANDLE_VALUE;
	}

	// First make sure all the mountpoints themselves are loaded
	PreLoadTable.lpPre        = &PreLoad;
	PreLoadTable.lpMountTable = lpMountTable;

	if (dwDaemonStatus != DAEMON_ACTIVE) return;

	PreLoad_Table(&PreLoadTable);

	// now wait for any threads we started to finish
	while (PreLoad.lThreads)
	{
		if (WaitForSingleObject(PreLoad.hSema, 500) == WAIT_OBJECT_0)
		{
			InterlockedDecrement(&PreLoad.lThreads);
		}
	}
	lPoints = lCount;

	if (bLogCount)
	{
		Putlog(LOG_GENERAL, "PRELOAD: \"points=%d\" \"%s\"\r\n", lCount, hMountFile->szFileName);
	}

	if (dwDaemonStatus != DAEMON_ACTIVE) return;

	// Going to use RecursiveAction to do all the work.  It's important to note that
	// since it doesn't actually use PWD_CWD nor does it check permissions via Access
	// it doesn't need an active userfile!  It also won't pick up on virtual subdirs!
	lpPreLoad2 = (LPVFSPRELOAD) Allocate("Preload", sizeof(*lpPreLoad2));
	if (!lpPreLoad2) return;

	// Grab the whole array because we can't afford to have it locked for long periods...
	dwArray = 0;
	for (lpOffset = NULL ; (dwArray < 50) && (tszPath = Config_Get_Linear(&IniConfigFile, _T("VFS_PreLoad"), tszName, NULL, &lpOffset)) ; )
	{
		if ( !_tcsicmp(tszName, _T("VFS")) || !_tcsicmp(tszName, _T("DELAY")) )
		{
			// vfs or delay specifier
			Free(tszPath);
			continue;
		}
		if ((1 != sscanf(tszName, "%d", &i)) || i <= 0)
		{
			if (bLogCount)
			{
				Putlog(LOG_ERROR, "Invalid VFS_PreLoad depth on line: %s = %s\r\n", tszName, lpPreLoad2->pBuffer);
			}
			Free(tszPath);
			continue;
		}
		iDepthArray[dwArray] = i;
		tszPathArray[dwArray] = tszPath;
		dwArray++;
	}
	if (dwArray < 50)
	{
		// need to unlock the config file as we ended before Config_Get_Linear did it for us...
		Config_Get_Linear_End(&IniConfigFile);
	}

	for (n=0 ; n < dwArray ; Free(tszPathArray[n++]))
	{
		if (dwDaemonStatus != DAEMON_ACTIVE) continue;
		strcpy(lpPreLoad2->pBuffer, tszPathArray[n]);

		iLen = strlen(lpPreLoad2->pBuffer);
		ZeroMemory(&MountData, sizeof(MountData));
		if (iLen && (tszRealPath = PWD_Resolve(lpPreLoad2->pBuffer, hMountFile, &MountData, TRUE, 0)))
		{
			dwAttr = GetFileAttributes(tszRealPath);
			FreeShared(tszRealPath);
			tszRealPath = NULL;
			if ( (dwAttr != INVALID_FILE_ATTRIBUTES) && (dwAttr & FILE_ATTRIBUTE_DIRECTORY) )
			{
				// we know it's a dir, so make sure it ends in a '/'
				if (lpPreLoad2->pBuffer[iLen-1] != '/')
				{
					lpPreLoad2->pBuffer[iLen]    = '/';
					lpPreLoad2->pBuffer[iLen+1]  = 0;
				}
				if (PreLoad.hSema != INVALID_HANDLE_VALUE)
				{
					// try to do this work in another thread...
					lpPreLoad2->hMountFile = hMountFile;
					lpPreLoad2->lplCount   = &lCount;
					lpPreLoad2->depth      = iDepthArray[n];
					lpPreLoad2->hSema      = PreLoad.hSema;

					hThread	= CreateThread(0, 0, PreLoad_RecursiveDirectory, lpPreLoad2, 0, 0);
					if (hThread != INVALID_HANDLE_VALUE)
					{
						SetThreadPriority(hThread, THREAD_PRIORITY_BELOW_NORMAL);

						iThreads++;
						CloseHandle(hThread);

						lpPreLoad2 = (LPVFSPRELOAD) Allocate("Preload", sizeof(*lpPreLoad2));
						if (!lpPreLoad2) 
						{
							Config_Get_Linear_End(&IniConfigFile);
							break;
						}

						continue;
					}
				}
				PreLoad.depth = iDepthArray[n];
				// not threaded or thread creation failed...
				RecursiveAction(NULL, hMountFile, lpPreLoad2->pBuffer, FALSE, TRUE, PreLoad.depth, PreLoad_VFS_CallBack, &PreLoad);
				continue;
			}
		}
		if (bLogCount)
		{
			Putlog(LOG_ERROR, "Invalid VFS_PreLoad path: %s\r\n", lpPreLoad2->pBuffer);
		}
	}

	Free(lpPreLoad2);

	// wait for all the threads we created to finish
	while (iThreads)
	{
		iThreads--;
		WaitForSingleObject(PreLoad.hSema, INFINITE);
	}

	if (bLogCount && (lPoints != lCount))
	{
		Putlog(LOG_GENERAL, "PRELOAD: \"count=%d\" \"%s\"\r\n", lCount, hMountFile->szFileName);
	}

	if (PreLoad.hSema != INVALID_HANDLE_VALUE)
	{
		CloseHandle(PreLoad.hSema);
	}
}



// Called only during first initialization with bLogCount enabled
VOID
PreLoad_VFS(LPVOID LogCount)
{
	CHAR             pBuffer[_INI_LINE_LENGTH + 1];
	LPTSTR           tszDefaultVfs, tszVfsFile;
	MOUNTFILE        hDefaultFile, hMountFile;
	BOOL             bLogCount = (BOOL) LogCount;

	if (tszVfsFile = Config_Get(&IniConfigFile, "VFS_PreLoad", "VFS", pBuffer, 0))
	{
		if (!_tcsicmp(tszVfsFile, _T("DISABLE")))
		{
			// disable preloading
			return;
		}

		if ( hMountFile = MountFile_Open(tszVfsFile, NULL))
		{
			PreLoad_MountFile(hMountFile, bLogCount);
			MountFile_Close(hMountFile);
			return;
		}
		else if (bLogCount)
		{
			Putlog(LOG_ERROR, _T("Unable to open/parse pre-load VFS file: %s\r\n"), tszVfsFile);
		}
	}

	if (tszDefaultVfs = Config_Get(&IniConfigFile, "Locations", "Default_VFS", pBuffer, 0))
	{
		if (hDefaultFile = MountFile_Open(tszDefaultVfs, NULL))
		{
			PreLoad_MountFile(hDefaultFile, bLogCount);
			MountFile_Close(hDefaultFile);
		}
		else if (bLogCount)
		{
			Putlog(LOG_ERROR, _T("Unable to open/parse default VFS file: %s\r\n"), tszDefaultVfs);
		}
	}
	else if (bLogCount)
	{
		Putlog(LOG_ERROR, _T("Default_Vfs is undefined in .ini file under [Locations]\r\n"));
	}
}


DWORD ReverseResolve(MOUNTFILE hMountFile, LPTSTR tszRealPath)
{
	LPMOUNT_ENTRIES lpEntries;
	LPMOUNT_ITEM    lpItem;
	DWORD dwLen, n;

	lpEntries = hMountFile->lpMountTable->lpEntries;
	if (!lpEntries)
	{
		ERROR_RETURN(ERROR_PATH_NOT_FOUND, 0);
	}

	dwLen = _tcslen(tszRealPath);

	for(n=0;n<lpEntries->dwEntries;n++)
	{
		lpItem = lpEntries->lpRealItemArray[n];
		if (lpItem->dwFileName > dwLen)
		{
			continue;
		}
		if (!_tcsnicmp(tszRealPath, lpItem->szFileName, lpItem->dwFileName) && (!tszRealPath[lpItem->dwFileName] || tszRealPath[lpItem->dwFileName] == _T('\\')))
		{
			return n+1;
		}
	}
	ERROR_RETURN(ERROR_PATH_NOT_FOUND, 0);
}
