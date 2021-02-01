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

#include <IoFTPD.h>

static VOID MakeCleanPath(LPSTR szPath);
static BOOL GenerateListing(LPLISTING lpList);
static VOID List_PrintError(LPBUFFER lpBuffer, LPTSTR tszCommand, DWORD dwError);
static void ListGetDirSize(LPLISTING lpListing, LPSTR szDirPath, PUINT64 lpu64DirSize);


static VOID MakeCleanPath(LPSTR szPath)
{
	PCHAR	pFrom, pTo;
	CHAR	cLast;

	cLast	= 0;
	pFrom	= pTo	= szPath;

	for (;;)
	{
		switch (pTo[0] = (pFrom++)[0])
		{
		case '/':
			//	Do not allow several '/' chars
			if (cLast != '/')
			{
				cLast	= '/';
				pTo++;
			}
			break;
		case '.':
			//	Do not allow '/./'
			if (cLast == '/' || cLast == 0)
			{
				if (pFrom[0] == '/')
				{
					pFrom++;
					break;
				}
				else if (pFrom[0] == '\0')
				{
					pTo[0]	= '\0';
					return;
				}
			}
		default:
			cLast	= (pTo++)[0];
			break;
		case '\0':
			// it's OK to end in a /
			// if (cLast == '/' && pTo > &szPath[1]) pTo[-1]	= '\0';
			return;
		}
	}
}


static BOOL List_PrintShort(LPLISTING lpListing, LPTSTR tszFileName, BOOL bDotDir, PVIRTUALPATH pvpVirtualPath,
							LPFILEINFO lpFileInfo, BOOL isDir, DWORD dwMountIndexes, LPVIRTUALINFO lpVirtualInfo)
{
	FormatString(lpListing->lpBuffer, _TEXT("%s\r\n"), tszFileName);
	return FALSE;
}


// return TRUE if we should prune this directory entry (i.e. don't desend and display) because it's a junction
// that we resolved to a symlink that will be displayed in this listing elsewhere (they share a common path)
// or because the link is broken.
static BOOL List_PrintLong(LPLISTING lpListing, LPTSTR tszFileName, BOOL bDotDir, PVIRTUALPATH pvpVirtualPath,
						   LPFILEINFO lpFileInfo, BOOL isDir, DWORD dwMountIndexes, LPVIRTUALINFO lpVirtualInfo)
{
	SYSTEMTIME	SystemTime;
	TCHAR		pBuffer[18], tBuffer[21], vBuffer[MAX_SUBMOUNTS*3], tPerms[3500], *pCurrent, tszVfsLink[_MAX_PWD+1];
	LPTSTR		tszLink, tszFullLink, tszUserName, tszGroupName, tszRealPath, tszTemp, tszStart;
	DWORD       dwFileMode, dwLen, n, dwItem, dwFileName, dwRealPath;
	UINT64      u64FileSize, u64Speed;
	LPMOUNT_ENTRIES lpEntries;
	LPFAKEFILEINFO lpFakeInfo;
	BOOL        bReturn = FALSE;

	if (lpVirtualInfo)
	{
		tszUserName = lpVirtualInfo->tszUser;
		tszGroupName = lpVirtualInfo->tszGroup;
		tszLink = (lpVirtualInfo->bHideLink ? NULL : lpVirtualInfo->tszLink);
	}
	else
	{
		tszUserName = Uid2User(lpFileInfo->Uid);
		tszGroupName = Gid2Group(lpFileInfo->Gid);
		tszLink = NULL;
	}

	if (!tszUserName || !tszUserName[0])   tszUserName = _TEXT("nobody");
	if (!tszGroupName || !tszGroupName[0]) tszGroupName = _TEXT("nogroup");

	if (! FileTimeToSystemTime(&lpFileInfo->ftModificationTime, &SystemTime))
	{
		//	Could not retrieve modification time
		SystemTime.wYear    = 2000;
		SystemTime.wDay		= 1;
		SystemTime.wMonth	= 1;
		SystemTime.wHour	= 0;
		SystemTime.wMinute	= 0;
		SystemTime.wSecond	= 0;
	}
	// Date Formats:
	// FULL:    Jun 10 12:34:56 2005 (max = (3)+1+(2)+1+(2+1+2+1+2)+1+(4)+1 = 4+3+9+5=21)
	// Recent:  Dec 15  12:34
	// Old:     Jul 14  2005
	if (lpListing->dwFlags & LIST_DATE_FULL)
	{
		sprintf_s(tBuffer, sizeof(tBuffer), "%s %02d %02d:%02d:%02d %04d",
			Months[SystemTime.wMonth], SystemTime.wDay,
			SystemTime.wHour, SystemTime.wMinute, SystemTime.wSecond,
			SystemTime.wYear);
	}
	else
	{
		if ((CompareFileTime(&lpFileInfo->ftModificationTime, &lpListing->ftSixMonthsAgo) > 0) &&
			(CompareFileTime(&lpFileInfo->ftModificationTime, &lpListing->ftCurrent) <= 0))
		{
			sprintf_s(tBuffer, sizeof(tBuffer), "%s %02d %02d:%02d",
				Months[SystemTime.wMonth], SystemTime.wDay,
				SystemTime.wHour, SystemTime.wMinute);
		}
		else
		{
			sprintf_s(tBuffer, sizeof(tBuffer), "%s %02d  %04d",
				Months[SystemTime.wMonth], SystemTime.wDay, SystemTime.wYear);
		}
	}

	pCurrent = pBuffer;

	if (isDir && !(lpListing->dwFlags & LIST_SUBDIR_SIZE))
	{
		u64FileSize = 0;
	}
	else
	{
		u64FileSize = lpFileInfo->FileSize;
	}

	dwFileMode = lpFileInfo->dwFileMode;

	tszFullLink = NULL;
	if (!tszLink && !bDotDir && (lpFileInfo->dwFileAttributes & FILE_ATTRIBUTE_LINK) && (NtfsReparseMethod == NTFS_REPARSE_SYMLINK) && dwMountIndexes &&
		lpListing->hMountFile && lpListing->hMountFile->lpMountTable && lpListing->hMountFile->lpMountTable->lpEntries)
	{
		// ok, we don't have an explicit virtual link and it's not the "." or ".." special entries, but it IS a linked directory
		// and we want to show them as a symbolic link and we may have the info to do it...
		// HOWEVER, if it's a FAKE fileinfo then we need to look at the real fileinfo's data.
		if (lpFileInfo->lReferenceCount < 0)
		{
			lpFakeInfo = NULL; // keep debugger happy
			lpFakeInfo = (LPFAKEFILEINFO) ((char *) lpFileInfo - ((char *) &lpFakeInfo->Combined - (char *) &lpFakeInfo->lpReal));
			tszRealPath = &lpFakeInfo->lpReal->tszFileName[lpFakeInfo->lpReal->dwFileName+1];
		}
		else
		{
			tszRealPath = &lpFileInfo->tszFileName[lpFileInfo->dwFileName+1];
		}
		dwRealPath = _tcslen(tszRealPath);
		if ((lpFileInfo->dwFileAttributes & FILE_ATTRIBUTE_MASK) && dwRealPath)
		{
			// it's a valid link so recover the real directory path and then try to reverse it
			for(n=1;n<=lpListing->lpMountPoint->dwSubMounts;n++)
			{
				if (dwMountIndexes & (1 << n))
				{
					if (dwItem = ReverseResolve(lpListing->hMountFile, tszRealPath))
					{
						// the path is exported and reversible
						dwItem--;
						lpEntries = lpListing->hMountFile->lpMountTable->lpEntries;
						dwFileName = _tcslen(tszFileName);
						dwLen = dwRealPath - lpEntries->lpRealItemArray[dwItem]->dwFileName;
						if ((dwRealPath < lpEntries->lpRealItemArray[dwItem]->dwFileName) ||
							((dwLen+lpEntries->VirtualItemArray[dwItem].dwFileName)+dwFileName+1 > sizeof(tszVfsLink)))
						{
							// the path is too long, abort
							break;
						}
						CopyMemory(tszVfsLink, lpEntries->VirtualItemArray[dwItem].szFileName, lpEntries->VirtualItemArray[dwItem].dwFileName);
						tszLink = &tszVfsLink[lpEntries->VirtualItemArray[dwItem].dwFileName];
						tszStart = &tszRealPath[lpEntries->lpRealItemArray[dwItem]->dwFileName];
						if (*tszStart == _T('\\'))
						{
							tszStart++;
						}
						for ( tszTemp = tszStart ; *tszTemp ; tszTemp++, tszLink++)
						{
							if (*tszTemp == _T('\\'))
							{
								*tszLink = _T('/');
							}
							else
							{
								*tszLink = *tszTemp;
							}
						}
						*tszLink = 0;
						tszFullLink = tszLink = tszVfsLink;

						// now that we have a VFS path, let's compare it to our current path and make paths in this
						// directory and below relative to here, remember VFS dir paths always end in /'s so if
						// it matches it's below.
						if (!_tcsnicmp(tszLink, pvpVirtualPath->pwd, pvpVirtualPath->len))
						{
							tszLink += pvpVirtualPath->len;
						}
					}
					break;
				}
			}
			*pCurrent++ = ( tszLink ? _T('l') : _T('d') );
		}
		else if ((lpListing->dwFlags & LIST_ADMIN) && dwRealPath)
		{
			// it's a broken link, but we're an admin so print the real directory path
			tszLink = tszRealPath;
			*pCurrent++ = _T('l');
			dwFileMode = 0;
			bReturn = TRUE;
		}
		else
		{
			tszLink = NULL;
			*pCurrent++ = _T('-');
			dwFileMode = 0;
			bReturn = TRUE;
		}
	}
	else if (isDir)
	{
		if (!tszLink && !bDotDir && (lpFileInfo->dwFileMode & S_SYMBOLIC) && (!lpVirtualInfo || !lpVirtualInfo->bHideLink))
		{
			tszLink = (LPTSTR)FindFileContext(SYMBOLICLINK, &lpFileInfo->Context);
			if (tszLink && (lpListing->dwFlags & LIST_SYMLINK_SIZE))
			{
				ListGetDirSize(lpListing, tszLink, &u64FileSize);
			}
		}
		*pCurrent++ = ( tszLink ? _T('l') : _T('d') );
	}
	else
	{
		*pCurrent++ = ( tszLink ? _T('l') : _T('-') );
	}

	if (tszLink && !tszFullLink)
	{
		tszFullLink = tszLink;
	}

	if (tszFullLink && !_tcsnicmp(tszFullLink, lpListing->lpInitialVPath->pwd, lpListing->lpInitialVPath->len))
	{
		// ok the symlink info will be displayed elsewhere, let's just show the link
		bReturn = TRUE;
	}


	*pCurrent++	= (dwFileMode & S_IRUSR ? 'r' : '-');
	*pCurrent++	= (dwFileMode & S_IWUSR ? 'w' : '-');
	*pCurrent++	= (dwFileMode & S_IXUSR ? 'x' : '-');

	*pCurrent++	= (dwFileMode & S_IRGRP ? 'r' : '-');
	*pCurrent++	= (dwFileMode & S_IWGRP ? 'w' : '-');
	*pCurrent++	= (dwFileMode & S_IXGRP ? 'x' : '-');

	*pCurrent++	= (dwFileMode & S_IROTH ? 'r' : '-');
	*pCurrent++	= (dwFileMode & S_IWOTH ? 'w' : '-');
	*pCurrent++	= (dwFileMode & S_IXOTH ? 'x' : '-');
	*pCurrent = 0;

	if (isDir && (lpListing->dwFlags & LIST_PRIVATE_GROUP))
	{
		if (lpFileInfo->dwFileMode & S_PRIVATE)
		{
			if (tszTemp = (LPTSTR)FindFileContext(PRIVATE, &lpFileInfo->Context))
			{
				dwLen = _tcslen(tszTemp);
				if ((dwLen+1) > sizeof(tPerms)/sizeof(TCHAR))
				{
					tszGroupName = _T("-too-long-");
				}
				else
				{
					tszGroupName = tPerms;
					for(n=dwLen+1 ; n ; n--, tszTemp++)
					{
						if (*tszTemp == _T(' '))
						{
							if ((tszGroupName != tPerms) && (tszGroupName[-1] == _T('/')))
							{
								// don't print multiple /'s in a row
								continue;
							}
							*tszGroupName++ = _T('/');
							continue;
						}
						*tszGroupName++ = *tszTemp;
					}
					tszGroupName = tPerms;
				}
			}
			else
			{
				tszGroupName = _T("-error-");
			}
		}
		else
		{
			tszGroupName = _T("");
		}
	}

	if (lpListing->dwFlags & LIST_MERGED_GROUP)
	{
		tszGroupName = vBuffer;
		for(n=0;n<32;n++)
		{
			if (dwMountIndexes & (1 << n))
			{
				if (vBuffer != tszGroupName)
				{
					*tszGroupName++ = _T('-');
				}
				tszGroupName += _stprintf(tszGroupName, "%d", n);
			}
		}
		if (vBuffer == tszGroupName)
		{
			*tszGroupName++ = _T('-');
		}
		*tszGroupName = 0;
		tszGroupName = vBuffer;
	}

	if (lpListing->dwFlags & LIST_UPSPEED_GROUP)
	{
		tszGroupName = vBuffer;
		if (!lpFileInfo->dwUploadTimeInMs || !u64FileSize)
		{
			tszGroupName[0] = _T('-');
			tszGroupName[1] = 0;
		}
		else
		{
			u64Speed = (u64FileSize * 1000) / 1024;
			u64Speed /= lpFileInfo->dwUploadTimeInMs;
			_stprintf(tszGroupName, _T("%I64u_KB/s"), u64Speed);
		}
	}

	if (!tszLink)
	{
		//	Common directory or file
		FormatString(lpListing->lpBuffer, _TEXT("%.10s %3i %-12s %-12s %10I64u %s %s\r\n"),
			pBuffer, (isDir ? lpFileInfo->dwSubDirectories : 1), tszUserName, tszGroupName,
			u64FileSize, tBuffer, tszFileName);
	}
	else
	{
		//	Symbolic link
		FormatString(lpListing->lpBuffer, _TEXT("%.10s %3i %-12s %-12s %10I64u %s %s -> %s\r\n"),
			pBuffer, lpFileInfo->dwSubDirectories, tszUserName, tszGroupName, u64FileSize,
			tBuffer, tszFileName, tszLink);
	}

	return bReturn;
}



static BOOL List_PrintMLSD(LPLISTING lpListing, LPTSTR tszFileName, BOOL bDotDir, PVIRTUALPATH pvpVirtualPath,
						   LPFILEINFO lpFileInfo, BOOL isDir, DWORD dwMountIndexes, LPVIRTUALINFO lpVirtualInfo)
{
	SYSTEMTIME	SystemTime;
	LPTSTR		tszLink, tszUserName, tszGroupName, tszType;
	UINT64      u64FileSize;

	if (lpVirtualInfo)
	{
		tszUserName = lpVirtualInfo->tszUser;
		tszGroupName = lpVirtualInfo->tszGroup;
		tszLink = lpVirtualInfo->tszLink;
	}
	else
	{
		tszUserName = Uid2User(lpFileInfo->Uid);
		tszGroupName = Gid2Group(lpFileInfo->Gid);
		tszLink = NULL;
	}

	if (!tszUserName || !tszUserName[0])   tszUserName = _TEXT("nobody");
	if (!tszGroupName || !tszGroupName[0]) tszGroupName = _TEXT("nogroup");

	if (isDir)
	{
		if (tszFileName[0] == _T('.') && !tszFileName[1])
		{
			tszType = _T("type=cdir");
		}
		else if (tszFileName[0] == _T('.') && tszFileName[1] == _T('.') && !tszFileName[2])
		{
			tszType = _T("type=pdir");
		}
		else if (tszLink || ((lpFileInfo->dwFileMode & S_SYMBOLIC) && (tszLink = (LPTSTR)FindFileContext(SYMBOLICLINK, &lpFileInfo->Context))))
		{
			tszType = _T("type=OS.Unix-slink:");
		}
		else
		{
			tszType = _T("type=dir");
		}
	}
	else
	{
		tszType = _T("type=file");
	}

	if (isDir && !(lpListing->dwFlags & LIST_SUBDIR_SIZE))
	{
		u64FileSize = 0;
	}
	else
	{
		u64FileSize = lpFileInfo->FileSize;
	}

	if (! FileTimeToSystemTime(&lpFileInfo->ftModificationTime, &SystemTime))
	{
		//	Could not retrieve modification time
		SystemTime.wYear    = 2000;
		SystemTime.wDay		= 1;
		SystemTime.wMonth	= 1;
		SystemTime.wHour	= 0;
		SystemTime.wMinute	= 0;
		SystemTime.wSecond	= 0;
		SystemTime.wMilliseconds = 0;
	}

	FormatString(lpListing->lpBuffer, _TEXT("%s%s;size=%I64u;modify=%04d%02d%02d%02d%02d%02d;UNIX.mode=0%o;UNIX.owner=%s;UNIX.group=%s; %s\r\n"),
		tszType, (tszLink ? tszLink : ""), u64FileSize, 
		SystemTime.wYear, SystemTime.wMonth, SystemTime.wDay,
		SystemTime.wHour, SystemTime.wMinute, SystemTime.wSecond,
		(lpFileInfo->dwFileMode & S_ACCESS), tszUserName, tszGroupName, tszFileName);
	return FALSE;
}



BOOL
List_DoVirtualDir(LPLISTING lpListing)
{
	LPVIRTUALDIR   lpVirtualDir;
	LPVIRTUALINFO  lpVirtualInfo;
	LPUSERFILE     lpUserFile;
	MOUNT_DATA     MountData;
	VIRTUALPATH    VPath;
	FILEINFO       FakeInfo, *lpFileInfo;
	LPSTR          szGlobber, szFileName;
	BOOL           bShowFiles, bShowHidden;
	DWORD          n;
	INT            iCmp;

	lpVirtualDir = lpListing->lpVirtualDir;
	lpUserFile   = lpListing->lpUserFile;
	PWD_Reset(&VPath);
	PWD_Copy(&lpListing->lpDirNext->vpVirtPath, &VPath, FALSE);

	ZeroMemory(&FakeInfo, sizeof(FakeInfo));
	FakeInfo.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
	FakeInfo.Uid = lpUserFile->Uid;
	FakeInfo.Gid = lpUserFile->Gid;
	FakeInfo.ftAlternateTime = FakeInfo.ftModificationTime = lpListing->lpVirtualDir->ftLastUpdate;
	FakeInfo.dwFileMode = S_ACCESS;
	FakeInfo.dwSubDirectories = 0;

	szGlobber = lpListing->szGlobber;
	n=0;

	if ((lpListing->dwFlags & LIST_ALL))
	{
		if (!szGlobber || !iCompare(szGlobber, "."))
		{
			if (lpVirtualDir->dwVirtualInfos &&	(lpVirtualInfo = lpVirtualDir->lpVirtualInfoArray[0]) && !strcmp(".", lpVirtualInfo->tszName))
			{
				n++;
				lpListing->lpPrint(lpListing, ".", TRUE, &VPath, lpVirtualInfo->lpFileInfo, TRUE, 0, lpVirtualInfo);
			}
			else
			{
				lpListing->lpPrint(lpListing, ".", TRUE, &VPath, &FakeInfo, TRUE, 0, NULL);
			}
		}

		if (VPath.len > 1 && (!szGlobber || !iCompare(szGlobber, "..")))
		{
			// see if parent dir is real or virtual
			for ( iCmp = VPath.len-2 ; iCmp >= 0 ; iCmp--)
			{
				if (VPath.pwd[iCmp] == _T('/'))
				{
					break;
				}
			}
			if (iCmp >= 0)
			{
				VPath.pwd[iCmp] = 0;
				ZeroMemory(&MountData, sizeof(MountData));

				szFileName = PWD_Resolve(VPath.pwd, lpListing->hMountFile, &MountData, FALSE, 0);
				if (szFileName && GetFileInfo(szFileName, &lpFileInfo))
				{
					lpListing->lpPrint(lpListing, "..", TRUE, &VPath, lpFileInfo, TRUE, 0, NULL);
					CloseFileInfo(lpFileInfo);
				}
				else
				{
					// use current rather than cache time for parent
					FakeInfo.ftAlternateTime = FakeInfo.ftModificationTime = lpListing->ftCurrent;
					lpListing->lpPrint(lpListing, "..", TRUE, &VPath, &FakeInfo, TRUE, 0, NULL);
				}
				VPath.pwd[iCmp] = _T('/');
				FreeShared(szFileName);
			}
		}
	}

	bShowFiles = lpListing->dwFlags & LIST_DIRECTORIES_ONLY;
	bShowHidden = lpListing->dwFlags & LIST_HIDDEN;

	// n set up top so "." entry can be skipped here if already printed...
	for( ; n<lpVirtualDir->dwVirtualInfos; n++)
	{
		lpVirtualInfo = lpVirtualDir->lpVirtualInfoArray[n];
		lpFileInfo    = lpVirtualInfo->lpFileInfo;
		szFileName    = lpVirtualInfo->tszName;

		if (szFileName[0] == '.' && !bShowHidden) continue;
		if (lpFileInfo->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		{
			// if it's not a virtual subdir make sure the link target isn't hidden because it's private
			if (lpVirtualInfo->tszLink && !Access(lpUserFile, lpFileInfo, 0)) continue;

			//	Show directory
			lpListing->lpPrint(lpListing, szFileName, FALSE, &VPath, lpFileInfo, TRUE, 0, lpVirtualInfo);
		}
		else 
		{
			lpListing->lpPrint(lpListing, szFileName, FALSE, &VPath, lpFileInfo, FALSE, 0, lpVirtualInfo);
		}
	}

	Free(lpListing->lpDirNext);
	return FALSE;
}




static BOOL GenerateListing(LPLISTING lpList)
{
	if (lpList->dwFlags & LIST_VIRTUAL_DIR)
	{
		List_DoVirtualDir(lpList);
		return FALSE;
	}

	while (ListNextDir(lpList));
	if (lpList->lpDirNext)
	{
		// we still have more to do, so reset flag
		lpList->dwFlags &= ~LIST_DID_ONE;
		return TRUE;
	}
	return FALSE;
}



VOID FTP_ContinueListing(LPFTPUSER lpUser)
{
	LPLISTING lpList = lpUser->Listing;
	while (ListNextDir(lpList));
	if (lpList->lpDirNext)
	{
		// we still have more to do, so reset flag
		lpList->dwFlags &= ~LIST_DID_ONE;
	}
	else
	{
		// all done
		lpUser->Listing = NULL;
		Free(lpList);
	}
}

VOID FTP_AbortListing(LPFTPUSER lpUser)
{
	LPLISTING lpList = lpUser->Listing;
	LPDIRLISTING lpDL;

	while (lpList->lpDirNext)
	{
		lpDL = lpList->lpDirNext->lpNext;
		PWD_Free(&lpList->lpDirNext->vpVirtPath);
		if (lpList->lpDirNext->FakeParentInfo.Combined.lReferenceCount == -2 && lpList->lpDirNext->FakeParentInfo.lpReal)
		{
			CloseFileInfo(lpList->lpDirNext->FakeParentInfo.lpReal);
		}
		Free(lpList->lpDirNext);
		lpList->lpDirNext = lpDL;
	}
	Free(lpList);
	lpUser->Listing = NULL;
}


// If CmdLine contains a slash then not globbing!
BOOL InitListing(LPLISTING lpListing, BOOL bNoVirtual)
{
	LPDIRLISTING lpDL;
	LPVIRTUALDIREVENT lpVirtualDirEvent;
	LPTSTR szGlob, szPath, szMaybePath, tszTemp;
	TCHAR *lpFirstGlob, *lpMaybeGlob, *lpLastPath;
	LPFILEINFO lpFileInfo;
	MOUNT_DATA MountData;
	LPSTR szRealPath;
	UINT uPathLen;
	BOOL bHasEndSlash, bRecheckPath, bVerified;
	DWORD dwFlags;
	unsigned __int64 time64;

	if (lpListing->dwFlags & LIST_RECURSIVE)
	{
		// temporarily increment reference count
		while (InterlockedExchange(&FtpSettings.lStringLock, TRUE)) SwitchToThread();
		tszTemp = (LPTSTR) AllocateShared(FtpSettings.tszAllowedRecursive, NULL, 0);
		InterlockedExchange(&FtpSettings.lStringLock, FALSE);

		if (!tszTemp || HavePermission(lpListing->lpUserFile, tszTemp))
		{
			// no permission, disable flag
			lpListing->dwFlags &= ~LIST_RECURSIVE;
		}
		FreeShared(tszTemp);
	}

	if (!(lpListing->dwFlags & LIST_SUBDIR_SIZE))
	{
		// size was not explicitely requested, but see if it should be
		if ((lpListing->dwFlags & LIST_LONG) && !FtpSettings.bNoSubDirSizing)
		{
			lpListing->dwFlags |= LIST_SUBDIR_SIZE;
		}
	}

	if (!HasFlag(lpListing->lpUserFile, "MV"))
	{
		lpListing->dwFlags |= LIST_ADMIN;
	}
	else
	{
		// user isn't a VM so disable priviledged options
		lpListing->dwFlags &= ~(LIST_MERGED_GROUP | LIST_PRIVATE_GROUP);
	}

#if 0
	// TODO: SET THESE FLAGS?
	lpListing->dwFlags |= LIST_PRIVATE_GROUP | LIST_MERGED_GROUP;
#endif

	bRecheckPath = FALSE;
	szPath = szMaybePath = NULL;
	if (szGlob = lpListing->szCmdLine)
	{
		lpFirstGlob = lpMaybeGlob = lpLastPath = NULL;
		for(;*szGlob;szGlob++)
		{
			if (!lpFirstGlob && (*szGlob == _T('*') || *szGlob == _T('?')))
			{
				lpFirstGlob = szGlob;
				continue;
			}
			if (*szGlob == _T('/'))
			{
				lpLastPath = szGlob;
				continue;
			}
			if (*szGlob == _T('['))
			{
				lpMaybeGlob = szGlob;
			}
		}
		if (!lpLastPath)
		{
			// no '/' in there so it's . .. a local dir/file or glob
			if (lpFirstGlob)
			{
				// it is definitely a glob
				szGlob = lpListing->szCmdLine;
			}
			else if (lpMaybeGlob)
			{
				// who knows... figure it out below
				szGlob = NULL;
				szMaybePath = lpListing->szCmdLine;
			}
			else if (szGlob[0] == _T('.'))
			{
				szGlob = NULL;
				if (!szGlob[1])
				{
					// it's just this dir... boring...
				}
				else if (szGlob[1] == _T('.') && !szGlob[2])
				{
					// it's ..
					szPath = lpListing->szCmdLine;
				}
				else
				{
					// it's a hidden subdir, filename
					szMaybePath = lpListing->szCmdLine;
				}
			}
			else
			{
				// it's a subdir, filename
				szGlob = NULL;
				szMaybePath = lpListing->szCmdLine;
			}
		}
		else // it had a path component
		{
			if (lpFirstGlob && lpFirstGlob < lpLastPath)
			{
				// the glob was before the last path, which is illegal for non-virtual dirs
				bRecheckPath = TRUE;
			}
			if (!lpLastPath[1])
			{
				// since it ended in a / it's a path
				szGlob = NULL;
				szPath = lpListing->szCmdLine;
			}
			if (lpLastPath == lpListing->szCmdLine)
			{
				// since it's an absolute path or glob starting from / so
				// we need to shove the cmdline down by 1, stuff a NULL
				// in so we can return / as Path, and path/glob separately
				szGlob = lpListing->szCmdLine ;
				while (*szGlob++);
				while (--szGlob > lpListing->szCmdLine ) szGlob[1] = szGlob[0];
				lpLastPath++;
			}
			*lpLastPath++ = 0;
			szPath = lpListing->szCmdLine;
			if (!lpFirstGlob && !lpMaybeGlob)
			{
				// it's a path
				szGlob = NULL;
				szMaybePath = lpLastPath;
			}
			else
			{
				// the glob was after the last path separator, so it's path
				// then glob.  Any []'s before the last path separator are
				// assumed to be part of the path
				szGlob = lpLastPath;
			}
		}
	}

	// ASSERT (For non-virtual dirs)
	//         szGlob holds a glob pattern or NULL
	//         szPath holds a directory specifier (relative or absolute)
	//         szMaybePath holds a file or directory specifier, or possibly
	//         a glob pattern with []'s if lpMaybeGlob not NULL.
	// ASSERT (For virtual dirs)
	//         szGlob could be a path containing a glob pattern like *foo*/bar or NULL
	//         szPath holds a directory specifier (relative or absolute)
	//         szMaybePath holds something

	lpListing->szGlobber = szGlob;

	lpDL = (LPDIRLISTING)Allocate("DirListing", sizeof(DIRLISTING));
	ZeroMemory(lpDL, sizeof(DIRLISTING));

	uPathLen = 0;
	bHasEndSlash = FALSE;
	if (szPath)
	{
		MakeCleanPath(szPath);
		if (szPath[0] == _T('.') && ( szPath[1] == 0 || (szPath[1] == _T('/') && szPath[2] == 0)))
		{
			// save a path resolve later on
			szPath = NULL;
		}
		else
		{
			uPathLen = _tcslen(szPath);
			if (uPathLen && szPath[uPathLen-1] == _T('/'))
			{
				bHasEndSlash = TRUE;
			}
			// test for terminating '/'
		}
	}

	// first update the virtual/relative paths. Relative shouldn't end
	// in a '/', and vpVirtPath.pwd must.
	if (szPath && szPath[0] == _T('/'))
	{
		// absolute path
		_tcscpy_s(lpDL->szRelativeVPathName, _MAX_PWD+1, szPath);
		_tcscpy_s(lpDL->vpVirtPath.pwd, _MAX_PWD+1, szPath);
		_tcscpy_s(lpDL->vpVirtPath.Symbolic, _MAX_PWD+1, szPath);
		if (bHasEndSlash)
		{
			lpDL->vpVirtPath.len = uPathLen;
			lpDL->vpVirtPath.Symlen = uPathLen;
			if (uPathLen > 1)
			{
				// lop off trailing '/' - unless path just a '/'
				lpDL->szRelativeVPathName[uPathLen-1] = 0;
			}
		}
		else
		{
			// can't start with a / and not end with a / if just a /
			// so it must need an ending / added
			lpDL->vpVirtPath.pwd[uPathLen]   = _T('/');
			lpDL->vpVirtPath.pwd[uPathLen+1] = 0;
			lpDL->vpVirtPath.len = uPathLen+1;
			lpDL->vpVirtPath.Symbolic[uPathLen]   = _T('/');
			lpDL->vpVirtPath.Symbolic[uPathLen+1] = 0;
			lpDL->vpVirtPath.Symlen = uPathLen+1;
		}
	}
	else
	{
		// relative or no path specified
		PWD_Reset(&lpDL->vpVirtPath);
		if (!szPath)
		{
			PWD_CopyAddSym(lpListing->lpInitialVPath, &lpDL->vpVirtPath, FALSE);
			lpDL->szRelativeVPathName[0] = _T('.');
			lpDL->szRelativeVPathName[1] = 0;
		}
		else
		{
			// resolve the relative path to an absolute path, this updates
			// lpDL->vpVirtPath and the directory specified must exist
			// since the szMaybePath component is not included.
			_tcscpy_s(lpDL->szRelativeVPathName, _MAX_PWD+1, szPath);

			dwFlags = ( bNoVirtual ? (EXISTS|TYPE_DIRECTORY) : (EXISTS|TYPE_DIRECTORY|VIRTUAL_PWD) );
			szRealPath = PWD_CWD2(lpListing->lpUserFile, &lpDL->vpVirtPath, szPath, lpListing->hMountFile, &MountData, dwFlags, lpListing->lpFtpUser, _T("LIST"), NULL);
			if (!szRealPath && !lpDL->vpVirtPath.lpVirtualDirEvent)
			{
				Free(lpDL);
				SetLastError(IO_INVALID_FILENAME);
				return FALSE;
			}
		}
	}

	// if we had part of a path specified that contained a glob pattern and it's not part of a virtual dir it's an error.
	if (bRecheckPath && (bNoVirtual || !FindVirtualDirEvent(lpDL->vpVirtPath.pwd)))
	{
		PWD_Free(&lpDL->vpVirtPath);
		Free(lpDL);
		ERROR_RETURN(ERROR_INVALID_ARGUMENTS, FALSE);
	}


	// ok, now deal with the szMaybePath component.  if it resolves
	// to a file treat then we'll treat it as a glob and it will
	// end up just listing that file.  If it resolves to a directory
	// then it isn't a glob so add it to the path.  If it doesn't
	// resolve at all then assume it's a real glob.
	if (szMaybePath)
	{
		uPathLen = lpDL->vpVirtPath.len;

		lpMaybeGlob = szMaybePath;
		while (*lpMaybeGlob) lpDL->vpVirtPath.pwd[lpDL->vpVirtPath.len++] = *lpMaybeGlob++;
		lpMaybeGlob = szMaybePath;
		while (*lpMaybeGlob) lpDL->vpVirtPath.Symbolic[lpDL->vpVirtPath.Symlen++] = *lpMaybeGlob++;
		lpDL->vpVirtPath.pwd[lpDL->vpVirtPath.len] = 0;
		lpDL->vpVirtPath.Symbolic[lpDL->vpVirtPath.Symlen] = 0;

		ZeroMemory(&MountData,sizeof(MountData));
		tszTemp = szRealPath = PWD_Resolve(lpDL->vpVirtPath.pwd, lpListing->hMountFile, &MountData, TRUE, 0);
		lpDL->vpVirtPath.len = uPathLen;
		lpDL->vpVirtPath.pwd[uPathLen] = 0;
		lpDL->vpVirtPath.Symlen = uPathLen;
		lpDL->vpVirtPath.Symbolic[uPathLen] = 0;
		if (!szRealPath && !MountData.lpVirtualDirEvent)
		{
			// didn't find it, assume glob pattern
			lpListing->szGlobber = szMaybePath;
		}
		else
		{
			bVerified = FALSE;
			if (!szRealPath)
			{
				dwFlags = ( bNoVirtual ? (EXISTS) : (EXISTS|VIRTUAL_PWD) );
				szRealPath = PWD_CWD2(lpListing->lpUserFile, &lpDL->vpVirtPath, szMaybePath, lpListing->hMountFile, &MountData, dwFlags, lpListing->lpFtpUser, _T("LISTTEST"), NULL);
				if (szRealPath) bVerified = TRUE;
			}
			
			if (szRealPath)
			{
				if (GetFileInfo(szRealPath, &lpFileInfo))
				{
					FreeShared(tszTemp);
					if (lpFileInfo->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
					{
						// hey, it's a real directory, so not really globbing!
						if (!bVerified && !PWD_CWD2(lpListing->lpUserFile, &lpDL->vpVirtPath, szMaybePath, lpListing->hMountFile, &MountData,
							EXISTS|TYPE_DIRECTORY, lpListing->lpFtpUser, _T("LISTACCESS"), NULL))
						{
							// rut ro, permission issue maybe, bail!
							CloseFileInfo(lpFileInfo);
							PWD_Free(&lpDL->vpVirtPath);
							Free(lpDL);
							return FALSE;
						}
						// Now fix the relative path
						lpMaybeGlob = szMaybePath;
						lpLastPath = lpDL->szRelativeVPathName;
						if (!(lpLastPath[0] == _T('/') && lpLastPath[1] == 0))
						{
							while (*lpLastPath) lpLastPath++;
						}
						*lpLastPath++ = '/';
						while (*lpMaybeGlob) *lpLastPath++ = *lpMaybeGlob++;
						*lpLastPath++ = 0;
					}
					else
					{
						// hey it's not a directory so it's a file!
						lpListing->szGlobber = szMaybePath;
					}
					CloseFileInfo(lpFileInfo);
				}
				else
				{
					// unable to get fileinfo, bail...
					FreeShared(tszTemp);
					PWD_Free(&lpDL->vpVirtPath);
					Free(lpDL);
					return FALSE;
				}
			}
			else if (lpDL->vpVirtPath.lpVirtualDirEvent)
			{
				// it came up as a virtual subdir, so fix the relative path
				lpMaybeGlob = szMaybePath;
				lpLastPath = lpDL->szRelativeVPathName;
				if (!(lpLastPath[0] == _T('/') && lpLastPath[1] == 0))
				{
					while (*lpLastPath) lpLastPath++;
				}
				*lpLastPath++ = '/';
				while (*lpMaybeGlob) *lpLastPath++ = *lpMaybeGlob++;
				*lpLastPath++ = 0;
			}
			else if (MountData.lpVirtualDirEvent)
			{
				// it has a virtual root, assume it's a glob
				lpListing->szGlobber = szMaybePath;
			}
			else
			{
				// it just doesn't exist
				PWD_Free(&lpDL->vpVirtPath);
				Free(lpDL);
				return FALSE;
			}
		}
	}

	GetSystemTimeAsFileTime(&lpListing->ftCurrent);
	memcpy(&time64, &lpListing->ftCurrent, sizeof(FILETIME));

	// 100ns*10 = 1us * 1000 = 1ms * 1000 = 1 sec = 10,000,000
	// * 60sec * 60min * 24hours = 864,000,000,000
	time64 += (__int64) 10*1000*1000*60*60*24;
	memcpy(&lpListing->ftCurrent, &time64, sizeof(FILETIME));
	time64 -= (__int64) 10*1000*1000*60*60*24*366/2;
	memcpy(&lpListing->ftSixMonthsAgo, &time64, sizeof(FILETIME));

	// now lets make sure we handle updating virtual dirs
	if (lpVirtualDirEvent = FindVirtualDirEvent(lpDL->vpVirtPath.pwd))
	{
		// I don't think it's possible to be here if bNoVirtual is TRUE and the target directory
		// is a virtual dir, but just in case...
		if (bNoVirtual)
		{
			PWD_Free(&lpDL->vpVirtPath);
			Free(lpDL);
			ERROR_RETURN(ERROR_INVALID_ARGUMENTS, FALSE);
		}
		if (lpVirtualDirEvent->tszPrivate && lpListing->lpUserFile && HavePermission(lpListing->lpUserFile, lpVirtualDirEvent->tszPrivate) &&
				HasFlag(lpListing->lpUserFile, "MV"))
		{
			PWD_Free(&lpDL->vpVirtPath);
			Free(lpDL);
			ERROR_RETURN(ERROR_PATH_NOT_FOUND, FALSE);
		}
		if (!(lpListing->lpVirtualDir = VirtualDirLoad(lpListing->hMountFile, lpVirtualDirEvent, lpDL->vpVirtPath.pwd,lpListing->szGlobber, FALSE, TRUE, TRUE, _T("LIST"))))
		{
			PWD_Free(&lpDL->vpVirtPath);
			Free(lpDL);
			return FALSE;
		}
		if (lpListing->lpVirtualDir->szTarget[0] != 0)
		{
			// it resolved! which means we should be listing the contents of the target instead!
			strncpy_s(lpDL->vpVirtPath.pwd, sizeof(lpDL->vpVirtPath.pwd), lpListing->lpVirtualDir->szTarget, _TRUNCATE);
			lpDL->vpVirtPath.len = strlen(lpDL->vpVirtPath.pwd);
		}
		lpListing->dwFlags |= LIST_VIRTUAL_DIR;
	}

	lpListing->lpDirNext = lpDL;
	return TRUE;
}


// NOTE! copying the fileinfo structure would normally be bad because:
// A) It is a read only structure with a reference count, but we are just
//    copying it (so we can modify the local copy) just for printing
//    purposes.
// B) It has a pointer to the file Context argument which is only valid
//    as long as the file context we copied from hasn't been closed.  THUS
//    WE MOST HOLD THE FILEINFO OPEN IF WE WANT TO REFER TO THE CONTEXT.
//    If bFakeFileInfo is specified then LPFILEINFO is really a pointer to a 
//    FAKEFILEINFO.Combined structure which means a little pointer math gets
//    us back to FAKEFILEINFO.lpReal which we set to lpToAdd and the CALLER
//    MUST make sure the FileInfo pointer reference count is kept > 0.
// C) The allocation is actually bigger than the structure size because the
//    file/dir name is the last field and runs past the end.  This function
//    assumes the caller will allocate a large enough size for this so we
//    can copy the name if bCopyName true, or else we set it to zero for
//    safety reasons.

// WARNING: getting a FakeFileInfo pointer from a FileInfo is tricky because
// of compiler alignment issues.  It evidently isn't as simple as just subtracting
// the size of the pointer we know is before it!!!

// if bCopying then just copying, not merging.
void ListMergeInfo(LPFILEINFO lpMerged, LPFILEINFO lpToAdd, BOOL bCopying, BOOL bFakeFileInfo)
{
	LPFAKEFILEINFO lpFakeFileInfo;

	if (bCopying)
	{
		// we're just copying and cleaning up
		CopyMemory(lpMerged, lpToAdd, sizeof(FILEINFO));
		if (bFakeFileInfo && lpToAdd->Context.dwData)
		{
			// ok, we want to preserve the file context and it's not empty...
			lpFakeFileInfo = NULL; // keep debugger happy
			lpFakeFileInfo = (LPFAKEFILEINFO) ((char *) lpMerged - ((char *) &lpFakeFileInfo->Combined - (char *) &lpFakeFileInfo->lpReal));
			lpFakeFileInfo->lpReal = lpToAdd;
			lpMerged->lReferenceCount = -2; // identify as lpFakeFileInfo
			InterlockedIncrement(&lpToAdd->lReferenceCount);
		}
		else if (bFakeFileInfo)
		{
			// safety measure, zero out lpReal pointer
			lpFakeFileInfo = NULL; // keep debugger happy
			lpFakeFileInfo = (LPFAKEFILEINFO) ((char *) lpMerged - ((char *) &lpFakeFileInfo->Combined - (char *) &lpFakeFileInfo->lpReal));
			lpFakeFileInfo->lpReal = 0;
			lpMerged->lReferenceCount = -1;
		}
		else
		{
			lpMerged->lReferenceCount = -1; // identify as plain fake
		}
		lpMerged->tszFileName[0] = 0;
		lpMerged->dwFileName = 0;
		return;
	}

	lpMerged->FileSize += lpToAdd->FileSize;
	lpMerged->dwSubDirectories += lpToAdd->dwSubDirectories;
	if (CompareFileTime(&lpMerged->ftAlternateTime, &lpToAdd->ftAlternateTime) < 0)
	{
		CopyMemory(&lpMerged->ftAlternateTime, &lpToAdd->ftAlternateTime, sizeof(FILETIME));
	}
	if (CompareFileTime(&lpMerged->ftModificationTime, &lpToAdd->ftModificationTime) < 0)
	{
		CopyMemory(&lpMerged->ftModificationTime, &lpToAdd->ftModificationTime, sizeof(FILETIME));
	}
}



// To generate a complete listing we need to list the actual directory
// contents for non-mount points, or the contents of all the directories
// mounted on the current virtual directory.  To this any virtual mount
// points that are subdirectories of the current directory must be added.
//
// To generate ordered listings I note that each real directory is
// accessed via OpenDirectory() which calls UpdateDirectory() which 
// returns entries in case insensitive alphabetical order with
// directories first.  Thus to create a completely ordered listing an
// optimized merge of multiple lists is straightforward and efficient.
//
// Essentially the core algorithm is:
// 1) Generate the list of virtual subdir mount points within the current
//    directory and order them. There will never be more than 35, usually 0.
// 2) Open up each directory making up the current directory and then merge
//    the resulting lists along with those from step 1 if any.
// 3) Print the directory listing and if recursing add subdirectories to
//    the todo list.
// 4) Stop, or don't both even starting, if the buffer is reasonably full,
//    and at least one dir was generated.

// MountFile safety check!
// TELNET and FTP are the only services that can request directory
// listings and both services open the mount file and keep a reference
// to it via the MountFile_Open() call in Login_Second().  Other calls
// to MountFile_Open() may update the cache and returned new info
// if the disk file changed, but the original call result stored in
// the ftp or telnet structure and passed in via lpList->hMountFile
// is still valid since it's reference count is positive.  That count
// isn't decremented until EndClientJob has removed the last active job
// for the client and calls the appropriate *_Close_Connection routine.
// At that point we can't be here since this would be an active job.
// Thus there is no need to open/close the mountfile or to copy the
// data inside during this function.  In fact doing so might force
// an update which could mean what the user actually has access to and
// what LIST returns might be different since the user mountfile
// used everywhere else is never updated.  If the mountfile does get
// updated in the future it clearly should be done while not actively
// trying to generate a listing :)


BOOL ListNextDir(LPLISTING lpList)
{
	LPDIRLISTING    lpDir, lpDirHead, lpDirTail, lpDirNew;
	LPDIRECTORYINFO lpInfo;
	LPFILEINFO      lpFileInfo;
	LPSTR           szFileName, szGlobber, szRealPath;
	MOUNT_TABLE	   *lpMountTable;
	LPMOUNT_POINT   lpMountPoint;
	MOUNT_DATA      MountData;
	DWORD			i,j,k, dwPos;
	int             iCmp;
	TCHAR           *pOffset, *pTarget;

	LPDIRECTORYINFO  *lpDirInfoArray;
	DWORD            dwDirInfoUsed;
	LPDIRECTORYINFO  lpDirInfo;
	DWORD            dwDirInfoIndex[MAX_SUBMOUNTS+1];
	DWORD            *lpFromArray, *lpFromTemp, *lpFromNext, dwDirFrom;

	LPFILEINFO     *lpVirtSubDirs;
	DWORD           dwVirtSubCount;
	LPFILEINFO      lpCombinedInfo;
	LPFAKEFILEINFO  lpFakeFileInfo;

	DWORD			dwTotalEntries, dwFileCount;
	LPFILEINFO     *lpFileInfoSorted, *lpFileInfoTemp;
	LPFILEINFO     *lpFileInfo1, *lpFileInfo2, lpFileInfoMerged;
	DWORD           dwFileInfo1, dwFileInfo2;

	LPFAKEFILEINFO  lpThisFakeDirInfo;
	DWORD           dwError = NO_ERROR;
	BOOL            bOnlyFiles, bOnlyDirs, bShowHidden, bRecursive, bShowSize, bFull = FALSE, bFirst;

	LPVIRTUALDIREVENT lpVirtualDirEvent;

	if (!(lpDir = lpList->lpDirNext))
	{
		// we're done
		return FALSE;
	}

	// check remaining buffer space here, force output if we've used half
	// the output buffer and produced at least one listing
	if ((lpList->dwFlags & LIST_DID_ONE) &&
		(lpList->lpBuffer->len > lpList->lpBuffer->size/2))
	{
		return FALSE;
	}

	// NOTE: virtual mount points obscure a real directory of the same name.

	// lookup the virtual path. Mountpoints will contain the names of virtual
	// subdirs and SubMounts of each name will contain the real dirs.

	dwDirInfoUsed  = 0;
	lpDirInfo      = NULL;

	lpVirtSubDirs  = NULL;
	dwVirtSubCount = 0;
	lpCombinedInfo = NULL;


	if (lpMountTable = PWD_GetTable(lpDir->vpVirtPath.pwd, lpList->hMountFile))
	{
		// not sure we'll need them all, but who cares it won't be a big number, make room for virtual as well
		lpVirtSubDirs = (LPFILEINFO *)_alloca(sizeof(LPFILEINFO)*(lpMountTable->dwMountPoints+dwKnownVirtualDirEvents));

		for (i = 0;i < lpMountTable->dwMountPoints;i++)
		{
			lpMountPoint = lpMountTable->lpMountPoints[i];

			// check for no submounts or no name
			if (! lpMountPoint->dwSubMounts && !lpMountPoint->dwName) continue;

			for (j = 0; j < lpMountPoint->dwSubMounts; j++)
			{
				if (GetFileInfo(lpMountPoint->lpSubMount[j].szFileName, &lpFileInfo))
				{
					//	check access to make sure it's not a Private directory
					if (Access(lpList->lpUserFile, lpFileInfo, 0))
					{
						if (!(lpFileInfo->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
						{
							// non-directories cannot be mountpoints, neither can symlinks
							// but since the resolver doesn't know that and will treat it
							// as a symlink we will avoid testing for symlinks here so we
							// display the link correctly in the listing rather than making
							// it appear as a regular dir.
							CloseFileInfo(lpFileInfo);
							continue;
						}
						if (!lpCombinedInfo)
						{
							// Using a FakeFileInfo since we need to keep a pointer to
							// the open FileInfo open so the context will still be valid.
							lpFakeFileInfo = (LPFAKEFILEINFO)_alloca(sizeof(FAKEFILEINFO) +
								(lpMountPoint->dwName+1) * sizeof(TCHAR));
							lpCombinedInfo = &lpFakeFileInfo->Combined;
							// The first directory contains the access rights so use it as
							// template.
							ListMergeInfo(lpCombinedInfo, lpFileInfo, TRUE, TRUE);
							_tcscpy_s(lpCombinedInfo->tszFileName, lpMountPoint->dwName+1,
								lpMountPoint->szName);
							lpCombinedInfo->dwFileName = lpMountPoint->dwName;
							// NOTE: lpFileInfo may not have it's reference count incremented
							//       if we are keeping it around for it's context argument.
						}
						else
						{
							// it must be a merged directory
							ListMergeInfo(lpCombinedInfo, lpFileInfo, FALSE, FALSE);
						}
					}
					CloseFileInfo(lpFileInfo);
				}
			} // for j loop

			if (lpCombinedInfo)
			{
				if (!QuickInsert(lpVirtSubDirs, dwVirtSubCount, lpCombinedInfo, (QUICKCOMPAREPROC) CompareFileName))
				{
					dwVirtSubCount++;
				}
				lpCombinedInfo = NULL;
			}
		} // for i loop
	}

	// virtual dirs...
	k = lpDir->vpVirtPath.len;
	for (i=0 ; i<dwKnownVirtualDirEvents ; i++)
	{
		lpVirtualDirEvent = &KnownVirtualDirEvents[i];
		if ((lpVirtualDirEvent->dwName >= k) &&	!strnicmp(lpVirtualDirEvent->tszName, lpDir->vpVirtPath.pwd, k))
		{
			szFileName = &lpVirtualDirEvent->tszName[k];
			for(j=0 ; szFileName[j] && (szFileName[j] != '/') ; j++);
			if (j==0) continue;

			if (lpVirtualDirEvent->tszPrivate && lpList->lpUserFile && HavePermission(lpList->lpUserFile, lpVirtualDirEvent->tszPrivate) &&
				HasFlag(lpList->lpUserFile, "MV"))
			{
				// it's a private/hidden dir so we shouldn't show it!
				continue;
			}

			if (!lpVirtSubDirs)
			{
				lpVirtSubDirs = (LPFILEINFO *)_alloca(sizeof(LPFILEINFO)*dwKnownVirtualDirEvents);
			}

			lpFileInfo = (LPFILEINFO)_alloca(sizeof(FILEINFO) + (j) * sizeof(TCHAR));
			lpFileInfo->lReferenceCount = -1;
			lpFileInfo->Uid = lpList->lpUserFile->Uid;
			lpFileInfo->Gid = lpList->lpUserFile->Gid;
			lpFileInfo->dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
			lpFileInfo->dwFileMode = S_ACCESS;
			lpFileInfo->FileSize   = 0;
			lpFileInfo->ftAlternateTime = lpFileInfo->ftModificationTime = lpList->ftCurrent;
			lpFileInfo->dwSubDirectories = 0;
			lpFileInfo->Context.dwData = 0;
			lpFileInfo->Context.lpData = NULL;
			lpFileInfo->dwFileName = j;
			_tcsncpy_s(lpFileInfo->tszFileName, j+1, szFileName, j);
			lpFileInfo->tszFileName[j] = 0;

			if (!QuickInsert(lpVirtSubDirs, dwVirtSubCount, lpFileInfo, (QUICKCOMPAREPROC) CompareFileName))
			{
				dwVirtSubCount++;
			}
			// If the above insert failed then a mountpoint and a virtual dir overlap, or several
			// virtual subdirs share a path like /0day/incomplete and /0day/latest which could
			// both be virtual and from / would be duplicated
		}
	}

	dwTotalEntries = dwVirtSubCount;


	lpThisFakeDirInfo = NULL;
	bShowSize = lpList->dwFlags & LIST_SUBDIR_SIZE;
	dwDirFrom = 0;

	lpDirInfoArray = lpList->lpDirInfoArray;
	// safety check (no real reason to zero lpDirInfoArray)
	ZeroMemory(lpDirInfoArray, sizeof(lpList->lpDirInfoArray));
	lpList->lpMountPoint = NULL;
	ZeroMemory(&MountData, sizeof(MOUNT_DATA));
	while (szRealPath = PWD_Resolve(lpDir->vpVirtPath.pwd, lpList->hMountFile, &MountData, TRUE, 0))
	{
		lpList->lpMountPoint = MountData.Resume;
		if (lpInfo = OpenDirectory(szRealPath, bShowSize, !bShowSize, FALSE, NULL, NULL))
		{
			if (!dwDirInfoUsed && (!Access(lpList->lpUserFile, lpInfo->lpRootEntry, _I_READ)))
			{
				// no access to first directory which controls permissions... bail
				CloseDirectory(lpInfo);
				break;
			}

			lpDirInfoArray[dwDirInfoUsed] = lpInfo;
			dwDirInfoIndex[dwDirInfoUsed++] = MountData.Last+1;
			dwDirFrom |= (1 << (MountData.Last+1));
			dwTotalEntries += lpInfo->dwDirectorySize;

			if (!lpThisFakeDirInfo)
			{
				lpThisFakeDirInfo = (LPFAKEFILEINFO)_alloca(sizeof(FAKEFILEINFO));
				// need the permission/size info and copying it the first
				// entry will display them correctly, we don't need to copy
				// the name of the directory as we never use it, BUT we do need
				// to make sure the file context is still valid so we will
				// increment the reference count and use a FakeFileInfo.
				ListMergeInfo(&lpThisFakeDirInfo->Combined, lpInfo->lpRootEntry, TRUE, TRUE);
			}
			else
			{
				// it must be a merged directory
				ListMergeInfo(&lpThisFakeDirInfo->Combined, lpInfo->lpRootEntry, FALSE, FALSE);
			}
		}

		// we don't need the real path anymore
		FreeShared(szRealPath);
	}

	lpFileInfoSorted = lpFileInfoTemp = NULL;
	lpFromArray = lpFromTemp = NULL;
	dwFileCount = 0;
	lpDirHead = lpDirTail = NULL;
	// think it will fit, and we've already generated unsent data in the
	// buffer just flush it and we can restart.
	if (lpList->dwFlags & LIST_DID_ONE)
	{
		if (dwTotalEntries*128 > (lpList->lpBuffer->size - lpList->lpBuffer->len))
		{
			bFull = TRUE;
			goto CLEANUP;
		}
	}
	else
	{
		// we have to proceed, it's the first one
		lpList->dwFlags |= LIST_DID_ONE;
	}

	// merge real directory listings together with modified merge sort
	// that takes advantage of the fact the lists are already ordered.

	// NOTE: all allocations for fileinfo pointers are for the max
	// number of entries so they can be swapped back and forth...
	if (dwVirtSubCount == 0 && dwDirInfoUsed == 1)
	{
		// no merge necessary
		lpFileInfoSorted = lpDirInfoArray[0]->lpFileInfo;
		dwFileCount = lpDirInfoArray[0]->dwDirectorySize;
		lpFromArray = _alloca(sizeof(DWORD)*dwTotalEntries);
		if (!lpFromArray)
		{
			dwError = ERROR_NOT_ENOUGH_MEMORY;
			goto CLEANUP;
		}
		for(i=0;i<dwFileCount;i++) lpFromArray[i] = ( 1 << dwDirInfoIndex[0] );
	}
	else if (dwVirtSubCount > 0 && dwDirInfoUsed == 0)
	{
		// no real directories so no merge necessary
		lpFileInfoSorted = lpVirtSubDirs;
		dwFileCount = dwVirtSubCount;
		lpFromArray = _alloca(sizeof(DWORD)*dwTotalEntries);
		if (!lpFromArray)
		{
			dwError = ERROR_NOT_ENOUGH_MEMORY;
			goto CLEANUP;
		}
		for(i=0;i<dwFileCount;i++) lpFromArray[i] = 1;
	}
	else
	{
		if (dwVirtSubCount > 0)
		{
			// copy the few virtual subdirs into the answer array
			lpFileInfoSorted = (LPFILEINFO *)_alloca(sizeof(void *)*dwTotalEntries);
			if (!lpFileInfoSorted)
			{
				dwError = ERROR_NOT_ENOUGH_MEMORY;
				goto CLEANUP;
			}

			lpFromArray = _alloca(sizeof(DWORD)*dwTotalEntries);
			if (!lpFromArray)
			{
				dwError = ERROR_NOT_ENOUGH_MEMORY;
				goto CLEANUP;
			}

			lpFileInfo1 = lpVirtSubDirs;
			while (dwFileCount < dwVirtSubCount)
			{
				lpFromArray[dwFileCount] = 1;
				lpFileInfoSorted[dwFileCount++] = *lpFileInfo1++;
			}
		}

		// start merging
		bFirst = TRUE;
		for(i=0;i<dwDirInfoUsed;i++)
		{
			if (!lpFileInfoSorted)
			{
				// must not have had any virtual subdirs, but to be here there
				// has to be at least 2 real directories to merge
				lpFileInfoSorted = (LPFILEINFO *)_alloca(sizeof(void *)*dwTotalEntries);
				if (!lpFileInfoSorted)
				{
					dwError = ERROR_NOT_ENOUGH_MEMORY;
					goto CLEANUP;
				}

				lpFromArray = _alloca(sizeof(DWORD)*dwTotalEntries);
				if (!lpFromArray)
				{
					dwError = ERROR_NOT_ENOUGH_MEMORY;
					goto CLEANUP;
				}

				lpFileInfo1 = lpDirInfoArray[i]->lpFileInfo;
				dwFileInfo1 = lpDirInfoArray[i]->dwDirectorySize;
				i++;
			}
			else 
			{
				bFirst = FALSE;
				// lpFileInfoSorted contains sorted here
				if (!lpFileInfoTemp)
				{
					// Sorted (our temp array is already used) so we need to
					// make another one...
					lpFileInfoTemp = (LPFILEINFO *)_alloca(sizeof(void *)*dwTotalEntries);
					if (!lpFileInfoTemp)
					{
						dwError = ERROR_NOT_ENOUGH_MEMORY;
						goto CLEANUP;
					}
					lpFromTemp = _alloca(sizeof(DWORD)*dwTotalEntries);
					if (!lpFromTemp)
					{
						dwError = ERROR_NOT_ENOUGH_MEMORY;
						goto CLEANUP;
					}
				}
				// swap the roles of sorted (new dest) and temp (source)
				lpFileInfo1 = lpFileInfoSorted;
				lpFileInfoSorted = lpFileInfoTemp;
				lpFileInfoTemp = lpFileInfo1;
				dwFileInfo1 = dwFileCount;

				lpFromNext  = lpFromArray;
				lpFromArray = lpFromTemp;
				lpFromTemp  = lpFromNext;
			}
			lpFileInfo2 = lpDirInfoArray[i]->lpFileInfo;
			dwFileInfo2 = lpDirInfoArray[i]->dwDirectorySize;

			dwFileCount = 0;
			j = k = 0;
			while(j<dwFileInfo1 && k<dwFileInfo2)
			{
				iCmp = CompareFileName(lpFileInfo1, lpFileInfo2);
				if (iCmp < 0)
				{
					// If bFirst is true then we don't have an array representation to just look
					// the answer up in, but we do know what the answer is so just use that...
					lpFromArray[dwFileCount] = (bFirst ? (1 << dwDirInfoIndex[0]) : lpFromNext[j]);
					lpFileInfoSorted[dwFileCount++] = *lpFileInfo1++;
					j++;
					continue;
				}
				else if (iCmp > 0)
				{
					lpFromArray[dwFileCount] = ( 1 << dwDirInfoIndex[i] );
					lpFileInfoSorted[dwFileCount++] = *lpFileInfo2++;
					k++;
					continue;
				}
				// because CompareFileName considers Directories > Files we can only
				// be here if 2 directories match or 2 files match.
				lpFromArray[dwFileCount] = (bFirst ? (1 << dwDirInfoIndex[0]) : lpFromNext[j]) | (1 << dwDirInfoIndex[i]);
				if (((*lpFileInfo1)->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
					((*lpFileInfo2)->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
				{
					// two directories.  consolidate the info
					if ((*lpFileInfo1)->lReferenceCount < 0)
					{
						// it's already a fake/merged entry, no need to allocate anything
						lpFileInfoSorted[dwFileCount] = *lpFileInfo1;
						ListMergeInfo(*lpFileInfo1, *lpFileInfo2, FALSE, FALSE);
					}
					else
					{
						// need to allocate a new entry since we can't touch the two
						// real structures.  No need to use a FakeFileInfo here since
						// we are dealing with FileInfo's gotten from an OpenDirectory
						// and we don't have to individually free those.
						lpFileInfoMerged = (LPFILEINFO)_alloca(sizeof(FILEINFO) + 
							((*lpFileInfo1)->dwFileName * sizeof(TCHAR)));
						lpFileInfoSorted[dwFileCount] = lpFileInfoMerged;
						ListMergeInfo(lpFileInfoMerged, *lpFileInfo1, TRUE, FALSE);
						lpFileInfoMerged->dwFileName = (*lpFileInfo1)->dwFileName;
						_tcscpy_s(lpFileInfoMerged->tszFileName, lpFileInfoMerged->dwFileName+1,
							(*lpFileInfo1)->tszFileName);
						ListMergeInfo(lpFileInfoMerged, *lpFileInfo2, FALSE, FALSE);
					}
				}
				else
				{
					// just print 1st entry since that is what PWD_Resolve will return
					lpFileInfoSorted[dwFileCount] = *lpFileInfo1;
				}
				dwFileCount++;
				lpFileInfo1++;
				lpFileInfo2++;
				j++;
				k++;
			}
			// add rest of entries...
			while (j++<dwFileInfo1)
			{
				lpFromArray[dwFileCount] = (bFirst ? (1 << dwDirInfoIndex[0]) : lpFromNext[j]);
				lpFileInfoSorted[dwFileCount++] = *lpFileInfo1++;
			}
			while (k++<dwFileInfo2)
			{
				lpFromArray[dwFileCount] = ( 1 << dwDirInfoIndex[i] );
				lpFileInfoSorted[dwFileCount++] = *lpFileInfo2++;
			}
		}
	}

	// Finally!  we can actually produce some output!
	szGlobber = lpList->szGlobber;

	if (bRecursive = (lpList->dwFlags & LIST_RECURSIVE))
	{
		if (lpList->dwFlags & LIST_FIRST_DIR)
		{
			lpList->dwFlags &= ~LIST_FIRST_DIR;
			FormatString(lpList->lpBuffer, _TEXT("%s:\r\n"),
				lpDir->szRelativeVPathName);
		}
		else
		{
			FormatString(lpList->lpBuffer, _TEXT("\r\n%s:\r\n"),
				lpDir->szRelativeVPathName);
		}
	}

	if ((lpList->dwFlags & LIST_ALL))
	{
		if (lpThisFakeDirInfo && (!szGlobber || !iCompare(szGlobber, ".")))
		{
			lpList->lpPrint(lpList, ".", TRUE, &lpDir->vpVirtPath, &lpThisFakeDirInfo->Combined, TRUE, dwDirFrom, NULL);
		}
		if (lpDir->vpVirtPath.len > 1 && (!szGlobber || !iCompare(szGlobber, "..")))
		{
			if (lpDir->FakeParentInfo.Combined.lReferenceCount >= 0)
			{
				// accurate info not available (not recursive I guess)
				for ( iCmp = lpDir->vpVirtPath.len-2 ; iCmp >= 0 ; iCmp--)
				{
					if (lpDir->vpVirtPath.pwd[iCmp] == _T('/'))
					{
						break;
					}
				}
				if (iCmp >= 0)
				{
					lpDir->vpVirtPath.pwd[iCmp] = 0;
					szFileName = PWD_Resolve(lpDir->vpVirtPath.pwd, lpList->hMountFile, NULL, FALSE, 0);
					lpDir->vpVirtPath.pwd[iCmp] = _T('/');
					if (szFileName)
					{
						if (GetFileInfo(szFileName, &lpFileInfo))
						{
							ListMergeInfo(&lpDir->FakeParentInfo.Combined, lpFileInfo, TRUE, TRUE);
							// the FakeFileInfo will hold open the real FileInfo until we free it at end
							CloseFileInfo(lpFileInfo);
						}
						FreeShared(szFileName);
					}
				}
			}
			if (lpDir->FakeParentInfo.Combined.lReferenceCount < 0)
			{
				lpList->lpPrint(lpList, "..", TRUE, &lpDir->vpVirtPath, &lpDir->FakeParentInfo.Combined, TRUE, 0, NULL);
			}
		}
	}

	bOnlyFiles = (lpList->dwFlags & LIST_FILES_ONLY ? TRUE : FALSE);
	bOnlyDirs  = (lpList->dwFlags & LIST_DIRECTORIES_ONLY ? TRUE : FALSE);
	bShowHidden = lpList->dwFlags & LIST_HIDDEN;

	for (i=0;i<dwFileCount;i++)
	{
		lpFileInfo = lpFileInfoSorted[i];
		szFileName = lpFileInfo->tszFileName;

		if (lpFileInfo->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		{
			// we skip recursing into hidden dirs if not showing them
			if (szFileName[0] == '.' && !bShowHidden) continue;
			if (!Access(lpList->lpUserFile, lpFileInfo, 0)) continue;
			if (bRecursive && !(lpFileInfo->dwFileMode & S_SYMBOLIC))
			{
				// The logic to reverse paths and figure out if it should be printed
				// is all in List_PrintLong which is the only routine that supports
				// recursive listings.  So we need to call that function, BUT if the
				// globber would have hidden the entry we want to quickly undo our
				// work so we'll just reset the output position :)
				dwPos = lpList->lpBuffer->len;

				if (!(lpList->lpPrint(lpList, szFileName, FALSE, &lpDir->vpVirtPath, lpFileInfo, TRUE, lpFromArray[i], NULL)))
				{
					// push directory onto todo list in correct location
					lpDirNew = (LPDIRLISTING)Allocate("lpDirListing", sizeof(DIRLISTING));
					if (!lpDirNew)
					{
						dwError = ERROR_NOT_ENOUGH_MEMORY;
						goto CLEANUP;
					}
					// Virt Path = original + name + /
					PWD_Reset(&lpDirNew->vpVirtPath);
					pOffset = lpDir->vpVirtPath.pwd;
					pTarget = lpDirNew->vpVirtPath.pwd;
					while(*pOffset) *pTarget++ = *pOffset++;
					pOffset = szFileName;
					while(*pOffset) *pTarget++ = *pOffset++;
					*pTarget++ = _T('/');
					*pTarget = 0;
					lpDirNew->vpVirtPath.len = (pTarget - lpDirNew->vpVirtPath.pwd)/sizeof(TCHAR);

					// Relative Path = original + / + name ; unless original == just /
					pOffset = lpDir->szRelativeVPathName;
					pTarget = lpDirNew->szRelativeVPathName;
					if (!(pOffset[0] == _T('/') && pOffset[1] == 0))
					{
						while(*pOffset) *pTarget++ = *pOffset++;
					}
					*pTarget++ = _T('/');
					pOffset = szFileName;
					while(*pOffset) *pTarget++ = *pOffset++;
					*pTarget = 0;

					if (lpThisFakeDirInfo)
					{
						// we created what we are copying here above and it has no valid name, but might
						// have a copy of the context and if so we need update the reference counter...
						CopyMemory(&lpDirNew->FakeParentInfo, lpThisFakeDirInfo, sizeof(FAKEFILEINFO));
						if (lpThisFakeDirInfo->Combined.lReferenceCount == -2)
						{
							InterlockedIncrement(&lpDirNew->FakeParentInfo.lpReal->lReferenceCount);
						}
					}
					else
					{
						ZeroMemory(&lpDirNew->FakeParentInfo, sizeof(lpDirNew->FakeParentInfo));
					}
					lpDirNew->lpNext = NULL;

					if (lpDirTail)
					{
						lpDirTail->lpNext = lpDirNew;
					}
					else
					{
						lpDirHead = lpDirNew;
					}
					lpDirTail = lpDirNew;
				}
				if (szGlobber && iCompare(szGlobber, szFileName))
				{
					// undo the entry we printed
					lpList->lpBuffer->len = dwPos;
				}
				continue;
			}
			if (bOnlyFiles) continue;
			if (szGlobber && iCompare(szGlobber, szFileName)) continue;
			//	Show directory
			lpList->lpPrint(lpList, szFileName, FALSE, &lpDir->vpVirtPath, lpFileInfo, TRUE, lpFromArray[i], NULL);
		}
		else 
		{
			if (bOnlyDirs) continue;
			if (szFileName[0] == '.' && !bShowHidden) continue;
			if (szGlobber && iCompare(szGlobber, szFileName)) continue;
			lpList->lpPrint(lpList, szFileName, FALSE, &lpDir->vpVirtPath, lpFileInfo, FALSE, lpFromArray[i], NULL);
		}
	}

CLEANUP:
	for(i=0;i<dwDirInfoUsed;i++)
	{
		CloseDirectory(lpDirInfoArray[i]);
	}
	// virtual dirs are not fake, but mountpoints are, so clean them up if necessary
	for(i=0;i<dwVirtSubCount;i++)
	{
		lpFileInfo = lpVirtSubDirs[i];
		if (lpFileInfo->lReferenceCount == -2)
		{
			// See warning in ListMergeInfo about getting address correct for this!
			lpFakeFileInfo = NULL; // keep debugger happy
			lpFakeFileInfo = (LPFAKEFILEINFO) ((char *) lpFileInfo - ((char *) &lpFakeFileInfo->Combined - (char *) &lpFakeFileInfo->lpReal));
			if (lpFakeFileInfo->lpReal)
			{
				CloseFileInfo(lpFakeFileInfo->lpReal);
			}
		}
	}
	if (lpDir->FakeParentInfo.Combined.lReferenceCount == -2 && lpDir->FakeParentInfo.lpReal)
	{
		CloseFileInfo(lpDir->FakeParentInfo.lpReal);
	}

	if (lpThisFakeDirInfo && lpThisFakeDirInfo->Combined.lReferenceCount == -2 && lpThisFakeDirInfo->lpReal)
	{
		CloseFileInfo(lpThisFakeDirInfo->lpReal);
	}

	// this means we wanted to jump out because we thought the buffer full...
	if (bFull)
	{
		return FALSE;
	}

	PWD_Free(&lpDir->vpVirtPath);

	if (lpDirHead)
	{
		lpDirTail->lpNext = lpDir->lpNext;
		lpList->lpDirNext = lpDirHead;
	}
	else
	{
		lpList->lpDirNext = lpDir->lpNext;
	}
	Free(lpDir);
	if (dwError != NO_ERROR)
	{
		SetLastError(dwError);
		return FALSE;
	}
	return TRUE;
}


LPLISTING List_ParseCmdLine(IO_STRING *Args, PVIRTUALPATH lpVPath, MOUNTFILE hMountFile)
{
	LPLISTING    lpListing;
	LPSTR		 szCmdLine;
	int          iCmdLen;
	DWORD        dwFlags;

	dwFlags = LIST_LONG | LIST_FIRST_DIR;

	// Process arguments
	if (GetStringItems(Args) > 0 &&
		(szCmdLine = GetStringIndexStatic(Args, 0))[0] == '-')
	{
		while ((++szCmdLine)[0])
		{
			switch (szCmdLine[0])
			{
			case '1':
				// Supress output (1 per line)
				dwFlags &= ~LIST_LONG;
				break;
			case 'a':
				// Show files starting with .
				dwFlags |= LIST_ALL;
				// fall through
			case 'A':
				// Show all files starting with . except for . and ..
				dwFlags |= LIST_HIDDEN;
				break;
			case 'd':
				// Show directories only
				dwFlags = (dwFlags & ~LIST_FILES_ONLY) | LIST_DIRECTORIES_ONLY;
				break;
			case 'f':
				// Show files only
				dwFlags = (dwFlags & ~LIST_DIRECTORIES_ONLY) | LIST_FILES_ONLY;
				break;
			case 'l':
				// Use long listing format
				dwFlags |= LIST_LONG;
				break;
			case 'L':
				// show size of symlink'd dir
				dwFlags |= LIST_SYMLINK_SIZE;
				break;
			case 'R':
				// Recursive Listing
				dwFlags |= LIST_RECURSIVE;
				break;
			case 's':
				// show size of subdirs
				dwFlags |= LIST_SUBDIR_SIZE;
				break;
			case 'T':
				// Use large date/time field
				dwFlags |= LIST_DATE_FULL;
				break;
			case 'U':
				// Show all files starting with . except for . and ..
				dwFlags = (dwFlags & ~LIST_GROUP_OPTIONS) | LIST_UPSPEED_GROUP;
				break;
			case 'V':
				// show VFS location info
				dwFlags = (dwFlags & ~LIST_GROUP_OPTIONS) | LIST_MERGED_GROUP;
				break;
			case 'Z':
				// show PRIVATE info
				dwFlags = (dwFlags & ~LIST_GROUP_OPTIONS) | LIST_PRIVATE_GROUP;
				break;
#if 0
			case 'P':
				// show alternate timestamp if valid/available (chattr 2)
				dwFlags |= LIST_ALTERNATE_TIME;
				break;
#endif
			}
		}
		szCmdLine	= GetStringRange(Args, 1, STR_END);
	}
	else szCmdLine	= GetStringRange(Args, STR_BEGIN, STR_END);

	if (!szCmdLine || !szCmdLine[0])
	{
		iCmdLen = 0;
	}
	else
	{
		iCmdLen = strnlen(szCmdLine,_MAX_PWD+1);
		if (iCmdLen >= _MAX_PWD)
		{
			// string is just too long... bail now
			SetLastError(IO_INVALID_ARGUMENTS);
			return NULL;
		}
	}

	// add space for cmdline + NULL + room to slide cmdline down by 1 in InitListing()
	if (!(lpListing = (LPLISTING)Allocate("lpListing", sizeof(LISTING) + (iCmdLen + 2)*sizeof(TCHAR))))
	{
		SetLastError(ERROR_NOT_ENOUGH_MEMORY);
		return NULL;
	}

	//	Initialize some of structure
	if (!iCmdLen)
	{
		lpListing->szCmdLine = NULL;
	}
	else
	{
		// we have to copy the input params in case CreateList can't generate
		// the complete listing on the first pass.
		lpListing->szCmdLine = (LPTSTR) ((char *) lpListing + sizeof(LISTING));
		memcpy(lpListing->szCmdLine, szCmdLine, (iCmdLen+1)*sizeof(TCHAR));
	}

	lpListing->dwFlags    = dwFlags;
	lpListing->hMountFile = hMountFile;
	return lpListing;
}



BOOL FTP_MLSD(LPFTPUSER lpUser, IO_STRING *Args)
{
	LPLISTING	lpListing;
	DWORD       dwLen;
	LPSTR       tszGlob;

	//	Initialize transfer
	lpUser->DataChannel.bSpecial	   = LIST;
	lpUser->DataChannel.bDirection	   = SEND;
	lpUser->DataChannel.bFree		   = TRUE;

	// there are no "-" arguments allowed to MLSD but we'll support globbing...
	tszGlob = _T("");
	if (GetStringItems(Args) > 0)
	{
		tszGlob = GetStringRange(Args, STR_BEGIN, STR_END);
	}
	dwLen = _tcslen(tszGlob);

	if (!(lpListing = (LPLISTING)Allocate("lpListing", sizeof(LISTING) + (dwLen + 2)*sizeof(TCHAR))))
	{
		SetLastError(ERROR_NOT_ENOUGH_MEMORY);
		return TRUE;
	}

	//	Initialize some of structure
	if (!dwLen)
	{
		lpListing->szCmdLine = NULL;
	}
	else
	{
		// we have to copy the input params in case CreateList can't generate
		// the complete listing on the first pass which shouldn't happen since MLSD
		// doesn't support recursive listings.
		lpListing->szCmdLine = (LPTSTR) ((char *) lpListing + sizeof(LISTING));
		memcpy(lpListing->szCmdLine, tszGlob, (dwLen+1)*sizeof(TCHAR));
	}

	lpListing->dwFlags    = LIST_ALL | LIST_FIRST_DIR | LIST_SUBDIR_SIZE;
	lpListing->hMountFile = lpUser->hMountFile;

	//	we're committed to sending something after this
	if (FTP_Data_Init_Transfer(lpUser, "directory listing")) return TRUE;

	//	Initialize rest of structure
	lpListing->lpFtpUser      = lpUser;
	lpListing->lpUserFile     = lpUser->UserFile;
	lpListing->lpInitialVPath = &lpUser->CommandChannel.Path;
	lpListing->lpBuffer	 	  = &lpUser->DataChannel.Buffer;
	lpListing->lpPrint        = List_PrintMLSD;

	if (InitListing(lpListing, FALSE))
	{
		GenerateListing(lpListing);
	}
	Free(lpListing);

	//	Begin transfer
	FTP_Data_Begin_Transfer(lpUser);

	return FALSE;
}


BOOL FTP_List(LPFTPUSER lpUser, IO_STRING *Args)
{
	LPLISTING	lpListing;

	//	Initialize transfer
	lpUser->DataChannel.bSpecial	= LIST;
	lpUser->DataChannel.bDirection	= SEND;
	lpUser->DataChannel.bFree		= TRUE;

	//	we're committed to sending something after this
	if (FTP_Data_Init_Transfer(lpUser, "directory listing")) return TRUE;

	lpListing = List_ParseCmdLine(Args, &lpUser->CommandChannel.Path, lpUser->hMountFile);

	if (lpListing)
	{
		//	Initialize rest of structure
		lpListing->lpFtpUser      = lpUser;
		lpListing->lpUserFile     = lpUser->UserFile;
		lpListing->lpInitialVPath = &lpUser->CommandChannel.Path;
		lpListing->lpBuffer	 	  = &lpUser->DataChannel.Buffer;
		lpListing->lpPrint        = (lpListing->dwFlags & LIST_LONG ? List_PrintLong : List_PrintShort);

		if (InitListing(lpListing, FALSE) && GenerateListing(lpListing))
		{
			lpUser->Listing = lpListing;
		}
		// could be unfinished if doing recursive listings...
		if (!lpUser->Listing)
		{
			Free(lpListing);
		}
	}

	//	Begin transfer
	FTP_Data_Begin_Transfer(lpUser);

	return FALSE;
}


BOOL FTP_Nlist(LPFTPUSER lpUser, IO_STRING *Args)
{
	LPLISTING	lpListing;

	//	Initialize transfer
	lpUser->DataChannel.bSpecial	= LIST;
	lpUser->DataChannel.bDirection	= SEND;
	lpUser->DataChannel.bFree		= TRUE;

	//	we're committed to sending something at this point
	if (FTP_Data_Init_Transfer(lpUser, "directory listing"))  return TRUE;

	lpListing = List_ParseCmdLine(Args, &lpUser->CommandChannel.Path, lpUser->hMountFile);

	if (lpListing)
	{
		// disallow recursive listings
		lpListing->dwFlags &= ~(LIST_RECURSIVE | LIST_ALL | LIST_HIDDEN);

		//	Initialize rest of structure
		lpListing->lpFtpUser      = lpUser;
		lpListing->lpUserFile     = lpUser->UserFile;
		lpListing->lpInitialVPath = &lpUser->CommandChannel.Path;
		lpListing->lpBuffer	 	  = &lpUser->DataChannel.Buffer;
		lpListing->lpPrint        = List_PrintShort;

		if (InitListing(lpListing, FALSE))
		{
			GenerateListing(lpListing);
		}
		Free(lpListing);
	}

	//	Begin transfer
	FTP_Data_Begin_Transfer(lpUser);

	return FALSE;
}


/*

FTP_Stat() - List contents of directory

*/
BOOL FTP_Stat(LPFTPUSER lpUser, IO_STRING *Args)
{
	LPLISTING	lpListing;

	lpListing = List_ParseCmdLine(Args, &lpUser->CommandChannel.Path, lpUser->hMountFile);

	if (!lpListing)
	{
		List_PrintError(&lpUser->CommandChannel.Out, _T("STAT"), GetLastError());
		return TRUE;
	}

	// disallow recursive listings
	lpListing->dwFlags &= ~LIST_RECURSIVE;

	//	Initialize rest of structure
	lpListing->lpFtpUser      = lpUser;
	lpListing->lpUserFile     = lpUser->UserFile;
	lpListing->lpInitialVPath = &lpUser->CommandChannel.Path;
	lpListing->lpBuffer	 	  = &lpUser->CommandChannel.Out;
	lpListing->lpPrint        = (lpListing->dwFlags & LIST_LONG ? List_PrintLong : List_PrintShort);

	//	Initialize directory listing
	if (!InitListing(lpListing, FALSE))
	{
		List_PrintError(&lpUser->CommandChannel.Out, _T("STAT"), GetLastError());
		Free(lpListing);
		return TRUE;
	}

	FormatString(&lpUser->CommandChannel.Out,
		_TEXT("212-Status of %s:\r\n"), lpListing->lpDirNext->szRelativeVPathName);

	GenerateListing(lpListing);
	Free(lpListing);

	FormatString(&lpUser->CommandChannel.Out, _TEXT("212 End of Status\r\n"));

	//	End of status

	return FALSE;
}




// This was a failure for FTP use with LIST/NLST but not sure why though...
// If you return an error code immediately after the LIST command the data channel should
// remain unused and another LIST command can be issued or the client can abort the
// connection.  FlashFXP sees the failed LIST and re-issues a plain no-argument LIST
// command which probably isn't a bad thing.  All is good on a regular connection, but
// when using SSL FlashFXP closes the data connection before issueing the plain LIST
// which appears to create some sort of race condition that might cause heap corruption
// for us.  Under VS2005 I get heap corruption warnings when compiled with debugging
// in socketclose() with a deep stack trace in winsock stuff.  Don't think that can be
// our fault!  Under purify no errors happen so it's a race issue if it exists, and
// because heap corruption is a pain in the rear to track backwards I give up...
// I will join the club of servers who just return nothing and then close the
// data channel gracefully instead of giving useful info... :(
static VOID List_PrintError(LPBUFFER lpBuffer, LPTSTR tszCommand, DWORD dwError)
{
	DWORD  dwUserError = 0;
	LPTSTR tszError = NULL;

	switch(dwError)
	{
	case IO_INVALID_ARGUMENTS:
		// input string too long
		dwUserError = 500;
		break;
	case ERROR_NOT_ENOUGH_MEMORY:
		dwUserError = 451;
		break;
	default:
		// ERROR_INVALID_ARGUMENTS: glob before last path
		// ERROR_INVALID_NAME: invalid relative directory/path name
		// IO_INVALID_FILENAME: invalid relative directory/path name
		// ERROR_FILE_NOT_FOUND:
		// IO_NO_ACCESS: unable to access directory
		dwUserError = 501;
	}

	FormatString(lpBuffer, _TEXT("%d %s: %E\r\n"), dwUserError, tszCommand, dwError);
}



static void ListGetDirSize(LPLISTING lpListing, LPSTR szDirPath, PUINT64 lpu64DirSize)
{
	MOUNT_DATA       MountData;
	LPDIRECTORYINFO  lpDirInfo;
	LPSTR            szRealPath;
	UINT64           u64Size = 0;
	BOOL             bAccess = FALSE, bCheck = TRUE;

	ZeroMemory(&MountData, sizeof(MOUNT_DATA));
	while (szRealPath = PWD_Resolve(szDirPath, lpListing->hMountFile, &MountData, TRUE, 0))
	{
		if (lpDirInfo = OpenDirectory(szRealPath, FALSE, TRUE, FALSE, NULL, NULL))
		{
			if (bCheck)
			{
				if (Access(lpListing->lpUserFile, lpDirInfo->lpRootEntry, _I_READ))
				{
					bAccess = TRUE;
				}
				bCheck = FALSE;
			}

			if (bAccess)
			{
				u64Size += lpDirInfo->lpRootEntry->FileSize;
			}
			CloseDirectory(lpDirInfo);
		}
		FreeShared(szRealPath);
	}
	*lpu64DirSize = u64Size;
}
