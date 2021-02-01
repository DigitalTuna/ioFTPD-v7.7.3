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

#define DAY   0001L
#define WEEK  0002L
#define MONTH 0004L

static UCHAR mDays[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };




BOOL
Event_Run(LPSTR Array,
          LPSTR Variable,
          LPSTR Arguments,
          LPVOID Context,
          INT ContextType,
          LPSTR Prefix)
{
  EVENT_COMMAND  Event;
  TCHAR          pBuffer[_INI_LINE_LENGTH + 1];
  BOOL           bReturn;
  INT            iPos;

  iPos  = 0;
  bReturn  = FALSE;
  Event.tszCommand  = pBuffer;
  Event.tszParameters  = Arguments;
  Event.tszOutputPrefix  = Prefix;
  Event.lpDataSource  = Context;

  switch (Event.dwDataSource = ContextType)
  {
  case DT_FTPUSER:
    Event.lpOutputSocket  = &((LPFTPUSER)Context)->CommandChannel.Socket;
    Event.lpOutputBuffer  = &((LPFTPUSER)Context)->CommandChannel.Out;
    break;

  default:
    Event.lpOutputSocket  = NULL;
    Event.lpOutputBuffer  = NULL;
    break;
  }

  while (! bReturn && Config_Get(&IniConfigFile, Array, Variable, pBuffer, &iPos))
  {
	  //  Execute script
	  if (pBuffer[0] == _T('!'))
	  {
		  //	Show text file
		  bReturn	= MessageFile_Show(&pBuffer[1], Event.lpOutputBuffer, Context, ContextType, Prefix, NULL);
	  }
	  else
	  {
	  bReturn  = RunEvent(&Event);
	  }
  }

  return bReturn;
}

BOOL
HandleIsFile(HANDLE fHandle)
{
  if (GetFileType(fHandle) == FILE_TYPE_DISK)
  {
    return FALSE;
  }
  SetLastError(ERROR_INVALID_NAME);
  return TRUE;
}




INT
TimeHasPassed(FILETIME *fTime)
{
  SYSTEMTIME  fsTime, lTime;
  INT      Return, dT, dW;

  FileTimeToSystemTime(fTime, &fsTime);
  GetSystemTime(&lTime);

  Return  = 0;

  if ( lTime.wYear == fsTime.wYear )
  {
    //  Year has not changed

    if ( lTime.wMonth == fsTime.wMonth )
    {

      if ( lTime.wDay == fsTime.wDay )
      {
        //  Day has not changed

        Return  += DAY + WEEK;
      }
      else if ( fsTime.wDay - fsTime.wDayOfWeek <= lTime.wDay &&
        fsTime.wDay - fsTime.wDayOfWeek + 6 >= lTime.wDay )
      {
        //  Week has not changed

        Return  += WEEK;
      }

      //  Month has not changed

      Return  += MONTH;
    }
    else
    {
      if ( lTime.wMonth - 1 == fsTime.wMonth )
      {
        //  Get number of days remaining on specified month

        dT  = mDays[fsTime.wMonth] - fsTime.wDay;

        //  Calculate amount of days left this week

        dW  = 6 - fsTime.wDayOfWeek;

        //  Calculate amount of days of week left on next month

        dT  = dW - dT;

        if ( dT >= lTime.wDay )
        {
          Return  += WEEK;
        }
      }
    }
  }

  return Return;
}







BOOL
Reduce_Stats(LPUSERFILE *lpUserFile,
             INT64 Amount,
             INT CreditSection,
             INT StatsSection,
			 INT ShareSection,
             INT Mode)
{
  LPUSERFILE  Target;

  //  Section triplex
  StatsSection  *= 3;
  //  Lock userfile
  if (UserFile_Lock(lpUserFile, 0)) return TRUE;
  //  Get offset to data
  Target  = lpUserFile[0];
  //  Update credits
  Target->Credits[ShareSection]  -= (Amount * Target->Ratio[CreditSection]);
  //  Update allup files
  if (Target->AllUp[StatsSection]) Target->AllUp[StatsSection]--;
  //  Sanity check for allup bytes
  if (Target->AllUp[StatsSection + 1] <= Amount)
  {
    //  Reset allup bytes
    Target->AllUp[StatsSection + 1]  = 0;
    Target->AllUp[StatsSection + 2]  = 0;
  }
  else if (Amount)
  {
    //  Update allup bytes
    Target->AllUp[StatsSection + 1]  -= Amount;
    Target->AllUp[StatsSection + 2] -= (INT64)(Target->AllUp[StatsSection + 2] * 1. * Amount / Target->AllUp[StatsSection + 1]);
  }

  //  Update monthup stats?
  if (Mode & MONTH)
  {
    //  Update monthup files
    if (Target->MonthUp[StatsSection]) Target->MonthUp[StatsSection]--;
    //  Sanity check for monthup bytes
    if (Target->MonthUp[StatsSection + 1] <= Amount)
    {
      //  Reset monthup bytes
      Target->MonthUp[StatsSection + 1]  = 0;
      Target->MonthUp[StatsSection + 2]  = 0;
    }
    else if (Amount)
    {
      //  Update monthup bytes
      Target->MonthUp[StatsSection + 1]  -= Amount;
      Target->MonthUp[StatsSection + 2]  -= (INT64)(Target->MonthUp[StatsSection + 2] * 1. * Amount / Target->MonthUp[StatsSection + 1]);
    }
  }

  //  Update weekup stats?
  if (Mode & WEEK)
  {
    //  Update wkup files
    if (Target->WkUp[StatsSection]) Target->WkUp[StatsSection]--;
    //  Sanity check for wkup bytes
    if (Target->WkUp[StatsSection + 1] <= Amount)
    {
      //  Reset wkup bytes
      Target->WkUp[StatsSection + 1]  = 0;
      Target->WkUp[StatsSection + 2]  = 0;
    }
    else if (Amount)
    {
      //  Update wkup bytes
      Target->WkUp[StatsSection + 1]  -= Amount;
      Target->WkUp[StatsSection + 2]  -= (INT64)(Target->WkUp[StatsSection + 2] * 1. * Amount / Target->WkUp[StatsSection + 1]);
    }
  }

  //  Update dayup stats?
  if (Mode & DAY)
  {
    //  Update dayup files
    if (Target->DayUp[StatsSection]) Target->DayUp[StatsSection]--;
    //  Sanity check for dayup bytes
    if (Target->DayUp[StatsSection + 1] <= Amount)
    {
      //  Reset dayup bytes
      Target->DayUp[StatsSection + 1]  = 0;
      Target->DayUp[StatsSection + 2]  = 0;
    }
    else if (Amount)
    {
      //  Update dayup bytes
      Target->DayUp[StatsSection + 1]  -= Amount;
      Target->DayUp[StatsSection + 2] -= (INT64)(Target->DayUp[StatsSection + 2] * 1. * Amount / Target->DayUp[StatsSection + 1]);
    }
  }
  //  Unlock userfile
  UserFile_Unlock(lpUserFile, 0);
  return FALSE;
}




LPSTR
Rename_Prepare_From(LPFTPUSER lpUser,
					PVIRTUALPATH CurrentPath,
                    LPUSERFILE lpUserFile,
                    MOUNTFILE hMountFile,
                    LPSTR File,
                    PINT pError)
{
  LPFILEINFO  lpParent, lpFileInfo;
  LPTSTR    tszFileName;
  DWORD    dwError;
  BOOL    bResult, bReturn;
  MOUNT_DATA MountData;

  bReturn  = FALSE;
  //  Resolve path
  PWD_Free(CurrentPath);

  tszFileName = PWD_CWD2(lpUserFile, CurrentPath, File, hMountFile, &MountData, EXISTS|TYPE_LINK|VIRTUAL_PWD, lpUser, _T("RNFR"), NULL);
  if (! tszFileName)
  {
    pError[0]  = GetLastError();
    return NULL;
  }

  if (CurrentPath->len <= 1)
  {
	  // can't rename/move the root directory!
	  pError[0]  = ERROR_WRITE_PROTECT;
	  return NULL;
  }

  if (CurrentPath->pwd[CurrentPath->len-1] == _T('/'))
  {
	  // we're trying to move a directory, make sure it isn't a mount point
	  if (PWD_IsMountPoint(CurrentPath->pwd, hMountFile))
	  {
		  // rut ro, it's a mount point, reject rename/move!
		  pError[0]  = ERROR_WRITE_PROTECT;
		  return NULL;
	  }
  }


  //  Get parent's fileinfo
  bResult = GetVfsParentFileInfo(lpUserFile, hMountFile, CurrentPath, &lpParent, FALSE);

  if (bResult)
  {
    //  Get fileinfo
    if (GetFileInfo(tszFileName, &lpFileInfo))
    {
      //  Check filemode
      if (Access(lpUserFile, lpParent, _I_WRITE) &&
        ((! PathCheck(lpUserFile, CurrentPath->pwd, "Rename") ||
        (Access(lpUserFile, lpFileInfo, _I_OWN) &&
        ! PathCheck(lpUserFile, CurrentPath->pwd, "RenameOwn")))))
      {
        bReturn  = TRUE;
      }
      else dwError  = GetLastError();
      CloseFileInfo(lpFileInfo);
    }
    else dwError  = GetLastError();
    CloseFileInfo(lpParent);
  }
  else dwError  = GetLastError();

  //  Set error
  if (! bReturn)
  {
    PWD_Free(CurrentPath);
    tszFileName  = NULL;
    pError[0]  = dwError;
  }
  return tszFileName;
}


PCHAR
Rename_Prepare_To(LPFTPUSER lpUser,
				  PVIRTUALPATH CurrentPath,
                  LPUSERFILE lpUserFile,
                  MOUNTFILE  hMountFile,
                  LPSTR File,
                  PINT pError)
{
  VIRTUALPATH  Source;
  LPFILEINFO  lpParent;
  BOOL    bResult, bReturn;
  DWORD    dwError;
  LPTSTR    tszFileName;
  MOUNT_DATA MountData;

  bReturn  = FALSE;
  //  Resolve path
  PWD_Reset(&Source);
  PWD_Copy(CurrentPath, &Source, FALSE);

  // YIL TODO:
  // 1) When using merged directories, make sure PWD_CWD doesn't pick
  //    a bogus/missing merged directory (i.e. mountdir was renamed/moved)
  // 2) Make sure we pick the same filesystem/directory as the original
  //    so we don't copy stuff across filesystems.
  // 3) with 1/2 we might as well add the don't fill up filesystem and
  //    create new dir on next drive logic all into pwd_cwd...
  tszFileName = PWD_CWD2(lpUserFile, &Source, File, hMountFile, &MountData, KEEP_CASE|VIRTUAL_PWD, lpUser, _T("RNTO"), NULL);
  if (! tszFileName)
  {
    pError[0]  = GetLastError();
    return NULL;
  }

  //  Get parent's fileinfo
  bResult = GetVfsParentFileInfo(lpUserFile, hMountFile, &Source, &lpParent, FALSE);

  if (bResult)
  {
    if (Access(lpUserFile, lpParent, _I_WRITE) &&
      (! PathCheck(lpUserFile, Source.pwd, "Rename") ||
      ! PathCheck(lpUserFile, Source.pwd, "RenameOwn")))
    {
      bReturn  = TRUE;
    }
    else dwError  = GetLastError();
    CloseFileInfo(lpParent);
  }
  else dwError  = GetLastError();

  //  Set error
  if (! bReturn)
  {
    PWD_Free(&Source);
    tszFileName  = NULL;
    pError[0]  = dwError;
  }
  return tszFileName;
}



BOOL
Delete_File(LPFTPUSER lpUser,
			PVIRTUALPATH CurrentPath,
			LPUSERFILE lpUserFile,
			MOUNTFILE hMountFile,
			LPSTR File,
			PINT pError)
{
	VIRTUALPATH Path;
	LPFILEINFO  lpFileInfo, lpParent;
	LPDIRECTORYINFO lpDirInfo;
	LPTSTR      tszFileName;
	BOOL        bMethod, bNTFS, bDir;
	INT         Time;
	INT         _Error;
	MOUNT_DATA  MountData;

	lpParent   = NULL;
	lpFileInfo = NULL;
	lpDirInfo  = NULL;
	if(!pError) pError=&_Error;

	//  Copy Pwd
	PWD_Reset(&Path);
	PWD_Copy(CurrentPath, &Path, FALSE);

	tszFileName=PWD_CWD2(lpUserFile, &Path, File, hMountFile, &MountData, EXISTS|TYPE_LINK|VIRTUAL_PWD, lpUser, _T("DELE"), NULL);
	if ( !tszFileName || !GetVfsParentFileInfo(lpUserFile, hMountFile, &Path, &lpParent, FALSE) )
	{
		// we couldn't resolve the path or get info on the parent so won't be able to check perms
		*pError=GetLastError();
		goto error;
	}

	bNTFS    = FALSE;
	bDir     = FALSE;

	if (!GetFileInfo2(tszFileName, &lpFileInfo, FALSE, &lpDirInfo) )
	{
		*pError=GetLastError();
		goto error;
	}
	// NOTE: lpFileInfo is for the TARGET dir if NTFS symlink/junction is valid...

	if ((lpDirInfo->lpRootEntry->dwFileAttributes & FILE_ATTRIBUTE_LINK) && (NtfsReparseMethod == NTFS_REPARSE_SYMLINK) )
	{
		// this means we are a NTFS junction/symlink which looks like a file
		bNTFS = TRUE;
		bDir  = TRUE;
	}
	else if (lpFileInfo->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
	{
		if (lpFileInfo->dwFileMode & S_SYMBOLIC)
		{
			// ioFTPD symbolic links are treated as files.
			bDir  = TRUE;
		}
		else
		{
			// can't delete directories with the file delete command
			*pError = IO_NO_ACCESS;
			goto error;
		}
	}

	// now check permissions
	if (Access(lpUserFile, lpParent, _I_WRITE) &&
		((Access(lpUserFile, lpFileInfo, _I_OWN) &&
		! PathCheck(lpUserFile, Path.pwd, "DeleteOwn")) ||
		! PathCheck(lpUserFile, Path.pwd, "Delete")) )
	{
		// OK
	}
	else
	{
		*pError = GetLastError();
		goto error;
	}

	if (bNTFS && IoRemoveReparsePoint(tszFileName))
	{
		// we need to undo the NTFS junction/symlink first and couldn't
		*pError = GetLastError();
		goto error;
	}

	if (bDir)
	{
		if (IoRemoveDirectory(tszFileName))
		{
			// we deleted the dir
			MarkParent(tszFileName, FALSE);
			MarkVirtualDir(&Path, hMountFile);
		}
		else
		{
			*pError = GetLastError();
			goto error;
		}
	}
	else if (IoDeleteFile(tszFileName, Path.l_RealPath))
	{
		//  Modify stats
		if (lpFileInfo->dwFileAttributes & FILE_ATTRIBUTE_IOFTPD &&
			! UserFile_OpenPrimitive(lpFileInfo->Uid, &lpUserFile, 0))
		{
			//  Choose config method
			if (Config_Get_Bool(&IniConfigFile, "VFS", "Modify_Stats_On_Delete", &bMethod) ||
				!bMethod ||
				!PathCheck(lpUserFile, Path.pwd, "NoStats"))
			{
				//  Reduce credits
				if (lpUserFile->Ratio[Path.CreditSection] &&
					! UserFile_Lock(&lpUserFile, 0))
				{
					//  Reduce credits
					lpUserFile->Credits[Path.ShareSection] -=
						lpUserFile->Ratio[Path.CreditSection] * lpFileInfo->FileSize / 1024;
					UserFile_Unlock(&lpUserFile, 0);
				}
			}
			else
			{
				//  Alternative method
				Time  = TimeHasPassed(&lpFileInfo->ftModificationTime);
				//  Reduce stats
				Reduce_Stats(&lpUserFile,
					lpFileInfo->FileSize / 1024,
					Path.CreditSection,
					Path.StatsSection,
					Path.ShareSection,
					Time);
			}

			UserFile_Close(&lpUserFile, 0);
		}
		MarkParent(tszFileName, TRUE);
		MarkVirtualDir(&Path, hMountFile);
	}
	else
	{
		// file didn't get deleted...
		*pError = GetLastError();
		goto error;
	}

	CloseDirectory(lpDirInfo);
	CloseFileInfo(lpFileInfo);
	CloseFileInfo(lpParent);
	PWD_Free(&Path);
	return FALSE;

error:
	if (lpDirInfo)   CloseDirectory(lpDirInfo);
	if (lpFileInfo)  CloseFileInfo(lpFileInfo);
	if (lpParent)    CloseFileInfo(lpParent);
	PWD_Free(&Path);
	return TRUE;
}


BOOL
Create_Directory(LPFTPUSER lpUser,
				 PVIRTUALPATH CurrentPath,
                 LPUSERFILE lpUserFile,
                 MOUNTFILE hMountFile,
                 LPSTR tszDirectoryName,
                 LPVOID Event,
                 LPVOID EventParam,
                 PINT pError)
{
  VFSUPDATE  UpdateData;
  VIRTUALPATH  Path;
  LPFILEINFO  lpParent;
  LPTSTR    tszFileName, tszArguments;
  BOOL    bReturn, bResult;
  DWORD    dwError;
  MOUNT_DATA MountData;

  bReturn  = FALSE;
  dwError  = NO_ERROR;

  //  Copy Pwd
  PWD_Reset(&Path);
  PWD_Copy(CurrentPath, &Path, FALSE);

  tszFileName  = PWD_CWD2(lpUserFile, &Path, tszDirectoryName, hMountFile, &MountData, VIRTUAL_PWD, lpUser, _T("MKD"), NULL);
  if (! tszFileName)
  {
    pError[0]  = GetLastError();
    return TRUE;
  }

  if (GetFileAttributes(tszFileName) != INVALID_FILE_ATTRIBUTES)
  {
	  // it exists already...
	  PWD_Free(&Path);
	  *pError = ERROR_ALREADY_EXISTS;
	  return TRUE;
  }

  //  Get parent's fileinfo
  bResult = GetVfsParentFileInfo(lpUserFile, hMountFile, &Path, &lpParent, FALSE);

  if (bResult)
  {
	  //  Check access
	  if (Access(lpUserFile, lpParent, _I_WRITE) &&
		  ! PathCheck(lpUserFile, Path.pwd, "MakeDir"))
	  {
		  // +1 NULL, +4 QUOTES, +2 SPACES, +10 (max dword printed)
		  tszArguments  = (LPTSTR)Allocate("CreateDirectory:Event", (Path.l_RealPath + Path.len + 17)*sizeof(TCHAR));
		  //  Execute events
		  if (tszArguments)
		  {
			  //  "Real path" "Virtual path"
			  wsprintf(tszArguments, "\"%s\" \"%s\"", tszFileName, Path.pwd);

			  if (((BOOL (__cdecl *)(LPVOID, LPTSTR, DWORD))Event)(EventParam, tszArguments, NO_ERROR))
			  {
				  dwError  = IO_NEWDIR_SCRIPT;
			  }
			  else
			  {
				  //  Create new directory
				  if (CreateDirectory(tszFileName, NULL))
				  {
					  //  Set directory ownership
					  ZeroMemory(&UpdateData, sizeof(VFSUPDATE));
					  UpdateData.Uid  = lpUserFile->Uid;
					  UpdateData.Gid  = lpUserFile->Gid;
					  UpdateData.dwFileMode  = lpParent->dwFileMode;

					  UpdateFileInfo(tszFileName, &UpdateData);
					  MarkVirtualDir(&Path, hMountFile);
					  //  Log event
					  Putlog(LOG_GENERAL, "NEWDIR: \"%s\" \"%s\" \"%s\" \"%s\"\r\n",
						  Uid2User(lpUserFile->Uid), Gid2Group(lpUserFile->Gid), Path.pwd, tszFileName);
					  bReturn  = TRUE;
				  }
				  else 
				  {
					  dwError = GetLastError();
					  wsprintf(tszArguments, "\"%s\" \"%s\" %u", tszFileName, Path.pwd, dwError);
					  ((BOOL (__cdecl *)(LPVOID, LPTSTR, DWORD))Event)(EventParam, tszArguments, dwError);
				  }
			  }

			  Free(tszArguments);
		  }
		  else dwError  = ERROR_NOT_ENOUGH_MEMORY;
	  }
	  else dwError  = GetLastError();
	  CloseFileInfo(lpParent);
  }
  else dwError  = GetLastError();
  PWD_Free(&Path);

  //  Set error
  if (! bReturn)
  {
    pError[0]  = dwError;
    return TRUE;
  }
  return FALSE;
}


BOOL
Remove_Directory(LPFTPUSER lpUser,
				 PVIRTUALPATH CurrentPath,
                 LPUSERFILE lpUserFile,
                 MOUNTFILE hMountFile,
                 LPTSTR tszDirectoryName,
                 LPVOID Event,
                 LPVOID EventParam,
                 PINT pError)
{
  VIRTUALPATH    Path;
  LPFILEINFO    lpParent, lpFileInfo;
  LPTSTR      tszFileName, tszArguments;
  DWORD      dwError;
  BOOL      bResult, bReturn;
  MOUNT_DATA MountData;


  bReturn  = FALSE;
  dwError  = NO_ERROR;
  //  Copy Pwd
  PWD_Reset(&Path);
  PWD_Copy(CurrentPath, &Path, FALSE);

  tszFileName  = PWD_CWD2(lpUserFile, &Path, tszDirectoryName, hMountFile, &MountData, EXISTS|TYPE_LINK|VIRTUAL_PWD, lpUser, _T("RMD"), NULL);
  if (! tszFileName)
  {
    pError[0]  = GetLastError();
    return TRUE;
  }

  //  Get parent's fileinfo
  bResult = GetVfsParentFileInfo(lpUserFile, hMountFile, &Path, &lpParent, FALSE);

  if (bResult)
  {
    //  Check access for parent
    if (Access(lpUserFile, lpParent, _I_WRITE))
    {
      //  Get fileinfo
      if (GetFileInfo(tszFileName, &lpFileInfo))
      {
		  if (!(lpFileInfo->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ||
			  (lpFileInfo->dwFileMode & S_SYMBOLIC))
		  {
			  dwError = ERROR_DIRECTORY;
		  }
		  else if (((Access(lpUserFile, lpFileInfo, _I_OWN) &&
			  ! PathCheck(lpUserFile, Path.pwd, "RemoveOwnDir")) ||
			  ! PathCheck(lpUserFile, Path.pwd, "RemoveDir")) &&
			  IoRemoveDirectory(tszFileName))
		  {
			  tszArguments  = (LPTSTR)Allocate("RemoveDirectory:Event", Path.l_RealPath + Path.len + 6);
			  if (tszArguments)
			  {
				  wsprintf(tszArguments, "\"%s\" \"%s\"", tszFileName, Path.pwd);
				  //  Run Event
				  ((BOOL (__cdecl *)(LPVOID, LPSTR))Event)(EventParam, tszArguments);
				  Free(tszArguments);
			  }  
			  MarkVirtualDir(&Path, hMountFile);
			  //  Write to log
			  Putlog(LOG_GENERAL, "DELDIR: \"%s\" \"%s\" \"%s\" \"%s\"\r\n",
				  Uid2User(lpUserFile->Uid), Gid2Group(lpUserFile->Gid), Path.pwd, tszFileName);
			  bReturn  = TRUE;
		  }
		  else dwError  = GetLastError();
		  CloseFileInfo(lpFileInfo);
	  }
      else dwError  = GetLastError();
    }
    else dwError  = GetLastError();
    CloseFileInfo(lpParent);
  }
  else dwError  = GetLastError();
  PWD_Free(&Path);

  //  Set error
  if (! bReturn)
  {
    pError[0]  = dwError;
    return TRUE;
  }
  return FALSE;
}


BOOL
Upload_Resume(LPFTPUSER lpUser,
			  PVIRTUALPATH CurrentPath,
              DATA *Data,
			  DWORD dwClientId,
              LPUSERFILE lpUserFile,
              MOUNTFILE hMountFile,
              LPTSTR tszUploadFileName,
              LPVOID Event,
              LPVOID EventParam)
{
  VFSUPDATE  UpdateData;
  LPTSTR    tszFileName, tszArguments;
  LPFILEINFO  lpFileInfo;
  BOOL    bResult, bReturn;
  DWORD    dwError;

  dwError  = NO_ERROR;
  bReturn  = FALSE;
  tszFileName = Data->File.RealPath;

  //  Open download file
  bResult  = ioOpenFile(&Data->IoFile, dwClientId, tszFileName,
    GENERIC_READ|GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_DELETE, OPEN_EXISTING);
  if (bResult)
  {
    Data->dwLastError  = GetLastError();
    return TRUE;
  }

  tszArguments  = (LPTSTR)Allocate("Upload:Resume:Event", Data->File.l_RealPath + Data->File.len + 6);
  if (tszArguments)
  {
    wsprintf(tszArguments, "\"%s\" \"%s\"", tszFileName, Data->File.pwd);
    if (((BOOL (__cdecl *)(LPVOID, LPTSTR))Event)(EventParam, tszArguments))
    {
      dwError  = IO_APPEND_SCRIPT;
    }
    Free(tszArguments);
  }
  else dwError  = ERROR_NOT_ENOUGH_MEMORY;

  //  Resume upload
  if (dwError == NO_ERROR)
  {
    if (GetFileInfo(tszFileName, &lpFileInfo))
    {
		if (Access(lpUserFile, lpFileInfo, _I_WRITE) &&
			! PathCheck(lpUserFile, Data->File.pwd, "Resume"))
		{
			UpdateData.Uid  = lpUserFile->Uid;
			UpdateData.Gid  = lpFileInfo->Gid;
			UpdateData.dwFileMode  = lpFileInfo->dwFileMode;
			UpdateData.ftAlternateTime  = lpFileInfo->ftAlternateTime;
			UpdateData.dwUploadTimeInMs = lpFileInfo->dwUploadTimeInMs;
			UpdateData.Context.lpData  = lpFileInfo->Context.lpData;
			UpdateData.Context.dwData  = lpFileInfo->Context.dwData;
			if (UpdateFileInfo(tszFileName, &UpdateData))
			{
				Data->bDirection  = RECEIVE;
				bReturn  = TRUE;
				MarkVirtualDir(&Data->File, hMountFile);
			}
			else dwError  = GetLastError();
		}
		else dwError = GetLastError();
		CloseFileInfo(lpFileInfo);
	}
	else dwError  = GetLastError();
  }

  if (! bReturn)
  {
    Data->dwLastError  = dwError;
    return TRUE;
  }
  return FALSE;
}


BOOL
Upload(LPFTPUSER lpUser,
	   PVIRTUALPATH CurrentPath,
       DATA *Data,
	   DWORD dwClientId,
       LPUSERFILE lpUserFile,
       MOUNTFILE hMountFile,
       LPTSTR tszUploadFileName,
       LPVOID Event,
       LPVOID EventParam)
{
  VFSUPDATE  UpdateData;
  LPFILEINFO  lpFileInfo;
  LPTSTR    tszFileName, tszArguments;
  DWORD    dwCreationDisposition, dwRequiredFlags, dwError;
  BOOL    bNoUpdate, bReturn, bResult;
  LPTSTR  tszCheck;

  bReturn  = FALSE;
  dwError  = NO_ERROR;
  tszFileName = Data->File.RealPath;

  //  Get fileinfo
  if (GetFileInfo(tszFileName, &lpFileInfo))
  {
	  dwCreationDisposition  = OPEN_EXISTING;
	  if (lpFileInfo->FileSize == 0)
	  {
		  // it's a zero byte file, use resume rule
		  dwRequiredFlags = _I_WRITE;
		  tszCheck = _T("Resume");
		  bNoUpdate = FALSE;
	  }
	  else
	  {
		  dwRequiredFlags  = _I_OWN|_I_WRITE;
		  tszCheck = _T("Overwrite");
		  bNoUpdate = TRUE;
	  }
  }
  else
  {
	  dwError = GetLastError();
	  
	  if (dwError == ERROR_FILE_NOT_FOUND)
	  {
		  bNoUpdate = FALSE;
		  //  Get parent's fileinfo
		  bResult = GetVfsParentFileInfo(lpUserFile, hMountFile, &Data->File, &lpFileInfo, FALSE);
		  if (! bResult)
		  {
			  dwError  = GetLastError();
		  }
		  else
		  {
			  dwError = NO_ERROR;
		  }
		  dwCreationDisposition  = OPEN_ALWAYS;
		  dwRequiredFlags  = _I_WRITE;
		  tszCheck = _T("Upload");
	  }
  }

  if (dwError == NO_ERROR)
  {
	  //  Check access
	  if (! PathCheck(lpUserFile, Data->File.pwd, tszCheck) &&
		  Access(lpUserFile, lpFileInfo, dwRequiredFlags))
	  {
		  tszArguments  = (LPTSTR)Allocate("Upload:Event", Data->File.l_RealPath + Data->File.len + 6);
		  if (tszArguments)
		  {
			  wsprintf(tszArguments, "\"%s\" \"%s\"", tszFileName, Data->File.pwd);
			  //  Execute Event
			  if (((BOOL (__cdecl *)(LPVOID, LPTSTR))Event)(EventParam, tszArguments))
			  {
				  dwError  = IO_STORE_SCRIPT;
			  }
			  Free(tszArguments);
		  }
		  else dwError  = ERROR_NOT_ENOUGH_MEMORY;

		  //  Create/open file
		  if (dwError == NO_ERROR)
		  {
			  if (! ioOpenFile(&Data->IoFile, dwClientId, tszFileName, GENERIC_READ|GENERIC_WRITE,
				  FILE_SHARE_READ|FILE_SHARE_DELETE, dwCreationDisposition))
			  {
				  if (dwCreationDisposition != OPEN_EXISTING)
				  {
					  Data->IoFile.dwFlags |= IOFILE_CREATED;
					  UpdateData.ftAlternateTime.dwHighDateTime = 0;
					  UpdateData.ftAlternateTime.dwLowDateTime  = 0;
					  UpdateData.dwUploadTimeInMs = 0;
				  }
				  else
				  {
					  UpdateData.ftAlternateTime  = lpFileInfo->ftAlternateTime;
					  UpdateData.dwUploadTimeInMs = lpFileInfo->dwUploadTimeInMs;
				  }

				  UpdateData.Uid  = lpUserFile->Uid;
				  UpdateData.Gid  = lpUserFile->Gid;
				  UpdateData.dwFileMode  = dwDefaultFileMode[0];
				  ZeroMemory(&UpdateData.Context, sizeof(FILECONTEXT));
				  //  Update file ownership
				  if (bNoUpdate || UpdateFileInfo(tszFileName, &UpdateData))
				  {
					  Data->bDirection  = RECEIVE;
					  bReturn  = TRUE;
				  }
				  else
				  {
					  dwError  = GetLastError();
				  }
				  MarkVirtualDir(&Data->File, hMountFile);
			  }
			  else
			  {
				  dwError  = GetLastError();
			  }
		  }
	  }
	  else 
	  {
		  dwError  = GetLastError();  
	  }
	  CloseFileInfo(lpFileInfo);
  }

  if (! bReturn)
  {
	  Data->dwLastError  = dwError;
	  return TRUE;
  }
  return FALSE;
}


BOOL
User_CheckIp(PCONNECTION_INFO pConnection,
             LPUSERFILE lpUserFile)
{
  LPSTR    szIdent, szHostName, szEntry, szHost, szTemp;
  CHAR    szIp[32];
  CHAR    szMatchBuf[_IP_LINE_LENGTH+1];
  INT      i;
  BOOL    bNumeric, bBlocking, bReturn, bDynamic, bMatch;
  PHOSTENT	pHostEnt;
  DWORD     dwStart, dwStop;
  DWORD     dwAddr, dwLen;


  bReturn   = TRUE;
  bDynamic  = FALSE;
  bMatch    = FALSE;
  bBlocking = FALSE;
  dwStart = GetTickCount();

  //  Get ident
  szIdent  = (pConnection->szIdent ? pConnection->szIdent : "*");
  //  Get hostname
  szHostName  = pConnection->szHostName;
  sprintf(szIp, "%s", inet_ntoa(pConnection->ClientAddress.sin_addr));

  for ( ; bReturn && !bDynamic ; bDynamic = TRUE)
  {
	  for (i = 0;i < MAX_IPS && lpUserFile->Ip[i][0] != '\0';i++)
	  {
		  szEntry = lpUserFile->Ip[i];

		  szTemp = strchr(szEntry, '@');
		  if (!szTemp) continue;
		  szHost = szTemp+1;
		  if (!*szHost) continue;

		  if (*szEntry != ':')
		  {
			  if (bDynamic) continue;

			  // only compare hostname masks that aren't all numeric to avoid 192.168.*
			  // being faked out by a reverse name like 192.168.foo.bar.com
			  if (!(szHostName && !IsNumericIP(szHost) && !iCompare(szHost, szHostName)) &&
				  iCompare(szHost, szIp))
			  {
				  // no match
				  continue;
			  }
		  }
		  else if (!bDynamic)
		  {
			  // if not doing dynamic lookups this round just go to the next one
			  continue;
		  }
		  else
		  {
			  szEntry++;
			  // dynamic lookup
			  bNumeric = TRUE;

			  for( ; *szTemp ; szTemp++ )
			  {
				  if (*szTemp == '*') break;
				  if (!isdigit(*szTemp) && *szTemp != '.')
				  {
					  bNumeric = FALSE;
				  }
			  }

			  if (*szTemp == '*' || bNumeric)
			  {
				  // it's a wildcard or all numeric IP
				  continue;
			  }

			  if (!bBlocking)
			  {
				  bBlocking = TRUE;
				  SetBlockingThreadFlag();
			  }

			  pHostEnt	= gethostbyname(szHost);

			  if (!pHostEnt || !pHostEnt->h_addr_list[0]) continue;

			  dwAddr = *((ULONG *) pHostEnt->h_addr_list[0]);
			  if (pConnection->ClientAddress.sin_addr.s_addr != dwAddr) continue;
		  }

		  // we have a host match!
		  bMatch = TRUE;

		  if (bIgnoreHostmaskIdents)
		  {
			  bReturn = FALSE;
			  break;
		  }

		  szTemp = szMatchBuf;
		  for(dwLen = sizeof(szMatchBuf) ; --dwLen && (*szTemp = *szEntry) && (*szTemp != '@') ; szTemp++, szEntry++ );
		  *szTemp = 0;

		  if (!iCompare(szMatchBuf, szIdent))
		  {
			  // ident matched
			  bReturn = FALSE;
			  break;
		  }
	  }

	  if ((dwDynamicDnsLookup == 0) || !bReturn)
	  {
		  // no dynamic lookups allowed, just fail or succeed if a match found
		  break;
	  }
	  // bDynamic gets set above in for loop...
  }

  if (dwRandomLoginDelay)
  {
	  if (!bBlocking)
	  {
		  SetBlockingThreadFlag();
	  }
	  dwStop = GetTickCount();

	  if (dwStop > dwStart)
	  {
		  dwStop -= dwStart;
		  dwStart = (DWORD) ((double) rand() / (RAND_MAX + 1) * dwRandomLoginDelay);
		  if (dwStop < dwStart)
		  {
			  // wakeup if we have an event, else sleep for a bit
			  SleepEx(dwStart - dwStop, TRUE);
		  }
	  }
  }

  if (bBlocking)
  {
	  SetNonBlockingThreadFlag();
  }

  if (!bReturn) return FALSE;

  if (bMatch)
  {
	  SetLastError(ERROR_IDENT_FAILURE);
	  return TRUE;
  }

  SetLastError(ERROR_CLIENT_HOST);
  return TRUE;
}


BOOL
User_CheckPassword(LPSTR szPassword,
                   PUCHAR pHashedPassword)
{
  BYTE  pHash[20];
  //  Hash password
  HashString(szPassword, pHash);
  //  Compare hashes
  return (! memcmp(pHash, pHashedPassword, sizeof(pHash)) ? FALSE : TRUE);
}




BOOL LogLoginErrorP(LPHOSTINFO lpHostInfo, DWORD dwType)
{
	DWORD dwTickCount;

	if (!lpHostInfo) return TRUE;
	dwTickCount = GetTickCount();

	if (lpHostInfo->dwLastFailedLoginType != dwType)
	{
		lpHostInfo->dwLastFailedLoginType = dwType;
		lpHostInfo->dwLastFailedLoginLogTime = dwTickCount;
		return TRUE;
	}
	lpHostInfo->dwLastFailedLoginType = dwType;

	if (Time_DifferenceDW32(lpHostInfo->dwLastFailedLoginLogTime, dwTickCount) >= lpHostInfo->dwLastFailedLoginLogDelay * 60 * 1000)
	{
		if (lpHostInfo->dwLastFailedLoginLogDelay < dwMaxLogSuppression)
		{
			lpHostInfo->dwLastFailedLoginLogDelay += dwLogSuppressionIncrement;
			if (lpHostInfo->dwLastFailedLoginLogDelay > dwMaxLogSuppression)
			{
				lpHostInfo->dwLastFailedLoginLogDelay = dwMaxLogSuppression;
			}
		}
		lpHostInfo->dwLastFailedLoginLogTime = dwTickCount;
		return TRUE;
	}
	return FALSE;
}



/*
 *  Login_First() - Logs user in
 */
BOOL
Login_First(LPUSERFILE *hUserFile,
            LPSTR szUserName,
            PCONNECTION_INFO Connection,
            BOOL Secure,
            PINT pError)
{
  LPUSERFILE lpUserFile;
  LPSTR szIdent;
  CHAR szObscuredHost[MAX_HOSTNAME];
  CHAR szObscuredIP[MAX_HOSTNAME];
  INT _Error;
  TCHAR tBanFlag;


  if(!pError) pError=&_Error;

  //  Close userfile
  if (hUserFile[0]) UserFile_Close(hUserFile, 0);

  if('!'==szUserName[0]) {
    Connection->dwStatus|=U_KILL; /* kill previous connections */
    ++szUserName;
  }

  if (UserFile_Open(szUserName, hUserFile, 0)) {
    *pError=GetLastError(); /* error while opening userfile */
    return TRUE;
  }

  lpUserFile  = hUserFile[0];
  szIdent     = Connection->szIdent ? Connection->szIdent : "*";

  if(!Secure && !Service_RequireSecureAuth(Connection->lpService, lpUserFile)) {
    //  Want ssl encryption
    *pError=IO_ENCRYPTION_REQUIRED;
    return TRUE;
  }

  if(User_CheckIp(Connection, lpUserFile))
  {
	  *pError = GetLastError();
	  // failed check
	  if (LogLoginErrorP(Connection->lpHostInfo, 1))
	  {
		  if (*pError != ERROR_IDENT_FAILURE)
		  {
			  Putlog(LOG_ERROR,
				  "Host '%s@%s' (%s) did not match any of user '%s' allowed hosts.\r\n",
				  szIdent,
				  Obscure_IP(szObscuredIP, &Connection->ClientAddress.sin_addr),
				  (Connection->szHostName ? Obscure_Host(szObscuredHost, Connection->szHostName) : ""),
				  szUserName);
		  }
		  else
		  {
			  Putlog(LOG_ERROR,
				  "Host '%s@%s' (%s) did not match any of user '%s' allowed ident responses.\r\n",
				  szIdent,
				  Obscure_IP(szObscuredIP, &Connection->ClientAddress.sin_addr),
				  (Connection->szHostName ? Obscure_Host(szObscuredHost, Connection->szHostName) : ""),
				  szUserName);
		  }
	  }

	  return TRUE;
  }

  if(Service_IsAllowedUser(Connection->lpService, lpUserFile))
  {
    //  No right to use this service
	  if (LogLoginErrorP(Connection->lpHostInfo, 2))
	  {
		  Putlog(LOG_ERROR,
			  "User '%s' tried to login with improper rights to service '%s'.\r\n",
			  szUserName,
			  Connection->lpService->tszName);
	  }
	  *pError=IO_NO_ACCESS;
	  return TRUE;
  }

  if (lpUserFile->DeletedOn)
  {
	  // the account has been deleted but hasn't been purged yet
	  *pError=ERROR_USER_DELETED;
	  return TRUE;
  }

  if (HasFlag(lpUserFile, _T("M")))
  {
	  if (lpUserFile->ExpiresAt && (time((time_t) NULL) > lpUserFile->ExpiresAt))
	  {
		  // the account has expired
		  *pError=ERROR_USER_EXPIRED;
		  return TRUE;
	  }

	  tBanFlag = FtpSettings.tBannedFlag;
	  if (tBanFlag && _tcschr(lpUserFile->Flags, tBanFlag))
	  {
		  // the account has been banned from logging in
		  *pError=ERROR_USER_BANNED;
		  return TRUE;
	  }
  }

  return FALSE;
}


/*
 * Login_Second() - Verifies user's password
 */
BOOL
Login_Second(LPUSERFILE *hUserFile,
             LPTSTR szPassword,
             COMMAND *CommandChan,
             PCONNECTION_INFO Connection,
             MOUNTFILE *lpMountFile,
             PINT pError,
			 LPFTPUSER lpFtpUser)
{
  LPGROUPFILE lpGroupFile;
  LPUSERFILE lpUserFile;
  LPSTR szIdent, szHostName, szUserName, szMountFile;
  CHAR pBuffer[_INI_LINE_LENGTH + 1];
  INT _Error;
  CHAR szObscuredHost[MAX_HOSTNAME];


  if(!pError) pError=&_Error;

  if ( !hUserFile || !(lpUserFile = hUserFile[0]) )
  {
    //  Store Error
    *pError=IO_INVALID_ARGUMENTS;
    return TRUE;
  }

  lpGroupFile  = NULL;
  szMountFile  = NULL;
  szUserName  = Uid2User(lpUserFile->Uid);
  szIdent    = (Connection->szIdent ? Connection->szIdent : "*");

  szHostName  = (Connection->szHostName ?
    Obscure_Host(szObscuredHost, Connection->szHostName) :
    Obscure_IP(szObscuredHost, &Connection->ClientAddress.sin_addr));

  if (HasFlag(lpUserFile, "A"))
  {
	  if (User_CheckPassword(szPassword, lpUserFile->Password))
	  {
		  // glftpd allows passwords with a "-" prefix which indicates the client doesn't want multi-line
		  // status return codes...
		  if ( (*szPassword == '-') && ! User_CheckPassword(&szPassword[1], lpUserFile->Password) )
		  {
			  lpFtpUser->FtpVariables.bSingleLineMode = TRUE;
		  }
		  else
		  {
			  //  Not Anonymous account & Password check failed
			  if (LogLoginErrorP(Connection->lpHostInfo, 3))
			  {
				  Putlog(LOG_ERROR, TEXT("User '%s' (%s@%s) tried to login with invalid password.\r\n"),
					  szUserName, szIdent, szHostName);
			  }
			  *pError=ERROR_PASSWORD;
			  return TRUE;
		  }
	  }

	  //  Kill all previous connections if user logged in with !username
	  // TODO: This should really only kick "zombies"...
	  if (Connection->dwStatus & U_KILL) KickUser(lpUserFile->Uid);
  }

  // password is correct, now double check SSL status...
  if ( *pError == IO_ENCRYPTION_REQUIRED) {
	  return TRUE;
  }

  //  Get mountfile
  if ( lpUserFile->MountFile[0] )
  {
	  szMountFile  = lpUserFile->MountFile;
	  lpFtpUser->FtpVariables.dwMountFileFrom = MOUNTFILE_USERFILE;
  }
  else
  {
	  //  No user specific groupfile, see if group has one...
	  if (GroupFile_OpenPrimitive(lpUserFile->Gid, &lpGroupFile, 0))
	  {
		  if (LogLoginErrorP(Connection->lpHostInfo, 4))
		  {
			  //  Log error
			  Putlog(LOG_ERROR, _TEXT("Warning: Group file for group '%s' does not exist, or is corrupted.\r\n"), Gid2Group(lpUserFile->Gid));
		  }
		  *pError = ERROR_VFS_FILE;
	  }
	  else if (lpGroupFile->szVfsFile[0])
	  {
		  szMountFile  = lpGroupFile->szVfsFile;
		  lpFtpUser->FtpVariables.dwMountFileFrom = MOUNTFILE_GROUPFILE;
	  }
	  else
	  {
		  if ( !(szMountFile = Config_Get(&IniConfigFile, "Locations", "Default_VFS", pBuffer, 0)) )
		  {
			  *pError=ERROR_VFS_FILE;
			  if (LogLoginErrorP(Connection->lpHostInfo, 6))
			  {
				  Putlog(LOG_ERROR, "Ini entry 'Default_VFS' under 'Locations' is missing.\r\n");
			  }
		  }
		  lpFtpUser->FtpVariables.dwMountFileFrom = MOUNTFILE_DEFAULT;
	  }
  }

  //  Sanity check
  if (szMountFile && ! (*lpMountFile = MountFile_Open(szMountFile, lpFtpUser)) )
  {
	  *pError=ERROR_VFS_FILE;
	  if (LogLoginErrorP(Connection->lpHostInfo, 5))
	  {
		  Putlog(LOG_ERROR, "VFS file '%s' is invalid or missing.\r\n", szMountFile);
	  }
  }
  
  //  Close groupfile if open
  if (lpGroupFile) GroupFile_Close(&lpGroupFile, 0);

  if (*pError != NO_ERROR)
  {
	  return TRUE;
  }

  //  Switch to home directory
  if((lpUserFile->Home[0] == '\0' ||
      !PWD_CWD(lpUserFile,
               &CommandChan->Path,
               lpUserFile->Home,
               *lpMountFile,
               EXISTS|TYPE_DIRECTORY|VIRTUAL_PWD)) &&
      !PWD_CWD(lpUserFile,
               &CommandChan->Path,
               "/",
               *lpMountFile,
               EXISTS|TYPE_DIRECTORY))
  {
	  //  Could not locate accessible directory, log error
	  if (LogLoginErrorP(Connection->lpHostInfo, 7))
	  {
		  Putlog(LOG_ERROR, "User '%s' does not have access to root directory.\r\n", szUserName);
	  }
	  *pError = ERROR_HOME_DIR;
	  return TRUE;
  }

  if ((FtpSettings.dwShutdownTimeLeft > 0) && HasFlag(lpUserFile, _TEXT("M")))
  {
	  *pError = ERROR_SHUTTING_DOWN;
	  return TRUE;
  }

  // could test for just ftp connections here, but going to honor a closed
  // "ftp" for all services...
  if (FtpSettings.tmSiteClosedOn)
  {
	  while (InterlockedExchange(&FtpSettings.lStringLock, TRUE)) SwitchToThread();
	  // note, it's possible it got opened after the test above, and before we grabbed the lock, so retest
	  if ( FtpSettings.tmSiteClosedOn )
	  {
		  if ( (FtpSettings.iSingleCloseUID != -1) && (FtpSettings.iSingleCloseUID == lpUserFile->Uid) )
		  {
			  // we are exempt... we can probably login provided we aren't starting up (tested below)
		  }
		  else if (!HavePermission(lpUserFile, FtpSettings.tszCloseExempt))
		  {
			  // we should be exempt, however need to double check it isn't closed to a single user!
			  if ( (FtpSettings.iSingleCloseUID != -1) && ( FtpSettings.iServerSingleExemptUID != lpUserFile->Uid) )
			  {
				  InterlockedExchange(&FtpSettings.lStringLock, FALSE);
				  *pError = ERROR_SERVER_SINGLE;
				  return TRUE;
			  }
		  }
		  else
		  {
			  // user didn't match, so they can't login...
			  InterlockedExchange(&FtpSettings.lStringLock, FALSE);
			  *pError = ERROR_SERVER_CLOSED;
			  return TRUE;
		  }
	  }
	  InterlockedExchange(&FtpSettings.lStringLock, FALSE);
  }

  if ( FtpSettings.bServerStartingUp && ( ( FtpSettings.iServerSingleExemptUID == -1 ) || ( FtpSettings.iServerSingleExemptUID != lpUserFile->Uid ) ) )
  {
	  // NOBODY, but the exempt uid can login while the startup scripts are still running...
	  *pError = ERROR_STARTING_UP;
	  return TRUE;
  }

  // NOTE: any additional login checks must occur above here
  // because UpdateClientData and the U_IDENTIFIED flags have
  // site effects elsewhere that will cause problems if the
  // login is later denied.

  //  Perform login check
  switch(UpdateClientData(DATA_AUTHENTICATE,
                          Connection->dwUniqueId,
                          lpUserFile,
                          &CommandChan->Path)) {
  case -1:
    //  Store Error
    *pError=ERROR_SERVICE_LOGINS;
    return TRUE;

  case -2:
    //  Store error
    *pError=ERROR_CLASS_LOGINS;
    return TRUE;

  case -3:
    //  Store error
    *pError=ERROR_IP_LOGINS;
    return TRUE;

  case -4:
    //  Store Error
    *pError=ERROR_USER_LOGINS;
    return TRUE;

  case -5:
	  //  Store Error
	  *pError=ERROR_USER_IP_LOGINS;
	  return TRUE;
  }

  if (! UserFile_Lock(&lpUserFile, 0))
  {
	  lpUserFile->LogonCount++;
	  lpUserFile->LogonLast = time((time_t) NULL);
	  _sntprintf_s(lpUserFile->LogonHost, sizeof(lpUserFile->LogonHost)/sizeof(*lpUserFile->LogonHost), _TRUNCATE, "%s@%s", szIdent, szHostName);
	  UserFile_Unlock(&lpUserFile, 0);
	  hUserFile[0] = lpUserFile;
  }
  else
  {
	  *pError = GetLastError();
	  // undo above...
	  UpdateClientData(DATA_DEAUTHENTICATE, Connection->dwUniqueId, lpUserFile);
	  UserFile_Close(&lpUserFile, 0);
	  hUserFile[0] = NULL;
	  return TRUE;
  }

  //  Login Successful
  Connection->dwStatus  |= U_IDENTIFIED;

  if (!FtpSettings.tQuietLoginFlag || !_tcschr(lpUserFile->Flags, FtpSettings.tQuietLoginFlag))
  {
	  Putlog(LOG_GENERAL, "LOGIN: \"%s\" \"%s\" \"%s\" \"%s\" \"%s@%s\"\r\n",    
		  Connection->lpService->tszName,
		  szUserName,
		  Gid2Group(lpUserFile->Gid),
		  lpUserFile->Tagline,
		  szIdent,
		  szHostName);
  }
  return FALSE;
}
