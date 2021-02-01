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


//	Local declarations
static BOOL	Is_Authenticated(PCONNECTION_INFO pConnectionInfo);
static BOOL	IsUserFileLoaded(IO_WHO *lpUserData);



static DWORD DataOffsets_Who[] =
{
	-1, TRUE,
	offsetof(IO_WHO, lpUserFile), FALSE,
	-1, TRUE,
	-1, TRUE,
	-1, TRUE,
	-1, TRUE,
	-1, TRUE,
	-1, TRUE,
	0, TRUE,
	-1, TRUE,
	-1, TRUE,
	-1, TRUE
};


static DWORD DataOffsets_Stats[] =
{
	-1, TRUE,
	offsetof(IO_STATS, lpUserFile), FALSE,
	-1, TRUE,
	-1, TRUE,
	-1, TRUE,
	-1, TRUE,
	offsetof(IO_STATS, iSection), FALSE,
	-1, TRUE,
	offsetof(IO_STATS, iPosition), FALSE,
	-1, TRUE,
	-1, TRUE,
	-1, TRUE
};


static DWORD DataOffsets_UserFile[] =
{
	-1, TRUE,
	0, TRUE,
	-1, TRUE,
	-1, TRUE,
	-1, TRUE,
	-1, TRUE,
	-1, TRUE,
	-1, TRUE,
	-1, TRUE,
	-1, TRUE,
	-1, TRUE,
	-1, TRUE
};


static DWORD DataOffsets_UserFile_Plus[] =
{
	-1, TRUE,
	offsetof(USERFILE_PLUS, lpUserFile), FALSE,
	-1, TRUE,
	offsetof(USERFILE_PLUS, lpCommandChannel), FALSE,
	-1, TRUE,
	-1, TRUE,
	offsetof(USERFILE_PLUS, iGID), FALSE,
	-1, TRUE,
	-1, TRUE,
	-1, TRUE,
	offsetof(USERFILE_PLUS, lpFtpUserCaller), FALSE,
	-1, TRUE
};


static DWORD DataOffsets_GroupFile[] =
{
	-1, TRUE,
	-1, TRUE,
	0, TRUE,
	-1, TRUE,
	-1, TRUE,
	-1, TRUE,
	-1, TRUE,
	-1, TRUE,
	-1, TRUE,
	-1, TRUE,
	-1, TRUE,
	-1, TRUE
};


static DWORD DataOffsets_Connection[] =
{
	0, TRUE,
	-1, TRUE,
	-1, TRUE,
	-1, TRUE,
	-1, TRUE,
	-1, TRUE,
	-1, TRUE,
	-1, TRUE,
	-1, TRUE,
	-1, TRUE,
	-1, TRUE,
	-1, TRUE
};


static DWORD DataOffsets_Ftp_User[] =
{
	offsetof(FTP_USER, Connection), TRUE,
	offsetof(FTP_USER, UserFile), FALSE,
	-1, TRUE,
	offsetof(FTP_USER, CommandChannel), TRUE,
	offsetof(FTP_USER, DataChannel), TRUE,
	offsetof(FTP_USER, CommandChannel.Path.CreditSection), FALSE,
	offsetof(FTP_USER, CommandChannel.Path.StatsSection), FALSE,
	offsetof(FTP_USER, CommandChannel.Path.ShareSection), FALSE,
	offsetof(FTP_USER, FtpVariables.iPos), FALSE,
	offsetof(FTP_USER, hMountFile), FALSE,
	-1, TRUE,
	0, TRUE
};


// MessageFile.h defines the DT_* macros which are indexes into this array
static DATA_OFFSET DataOffset[] =
{
	DataOffsets_Ftp_User,		(BOOL (__cdecl *)(LPVOID))Is_Authenticated, offsetof(FTP_USER, Connection),
	{ (B_USERFILE|B_CONNECTION|B_COMMAND|B_DATA|B_ANY|B_FTPUSER),  (B_CONNECTION|B_ANY) },
	DataOffsets_UserFile,		0, 0,
	{ B_USERFILE|B_ANY, 0 },
	DataOffsets_GroupFile,		0, 0,
	{ B_GROUPFILE|B_ANY, 0 },
	DataOffsets_Stats,			0, 0,
	{ B_USERFILE|B_ANY, 0 },
	DataOffsets_Who,			(BOOL (__cdecl *)(LPVOID))IsUserFileLoaded, 0,
	{ (B_USERFILE|B_WHO|B_ANY), (B_WHO|B_ANY) },
	DataOffsets_UserFile_Plus,	0, 0,
	{ B_USERFILE|B_COMMAND|B_ANY, 0 },
	DataOffsets_Connection,     0, 0,
	{ B_CONNECTION|B_ANY, 0 },
};





static BOOL Is_Authenticated(PCONNECTION_INFO pConnectionInfo)
{
	return (pConnectionInfo->dwStatus & U_IDENTIFIED);
}

static BOOL IsUserFileLoaded(IO_WHO *lpUserData)
{
	return (lpUserData->lpUserFile ? TRUE : FALSE);
}



#define	_OFFSETOF(x, y, z, t) (DataOffset[z].lpOffset[y * 2] == -1 ? (t) NULL : \
		(! DataOffset[z].lpOffset[y * 2 + 1] ? \
		((t *)((ULONG)x + DataOffset[z].lpOffset[y * 2]))[0] : \
		(t)((ULONG)x + DataOffset[z].lpOffset[y * 2])))


VOID InitDataOffsets(LPDATA_OFFSETS lpDataOffsets, LPVOID lpBuffer, DWORD dwOffsetType)
{
	//	Sanity check
	if (! dwOffsetType--)
	{
		ZeroMemory(lpDataOffsets, sizeof(DATA_OFFSETS));
		return;
	}
	//	Copy type
	lpDataOffsets->dwOffsetType	= dwOffsetType;
	//	Copy buffer offset
	lpDataOffsets->lpBuffer	= lpBuffer;
	//	Get information of data
	lpDataOffsets->dwHave	=
		(! DataOffset[dwOffsetType].ConditionFunc ||
		DataOffset[dwOffsetType].ConditionFunc((LPVOID)((ULONG)lpBuffer + DataOffset[dwOffsetType].bConditionParam)) ?
		DataOffset[dwOffsetType].dwHave[0] : DataOffset[dwOffsetType].dwHave[1]);
	//	Set pointers
	lpDataOffsets->pConnectionInfo	= _OFFSETOF(lpBuffer, 0, dwOffsetType, PCONNECTION_INFO);
	lpDataOffsets->lpUserFile		= _OFFSETOF(lpBuffer, 1, dwOffsetType, LPUSERFILE);
	lpDataOffsets->lpGroupFile		= _OFFSETOF(lpBuffer, 2, dwOffsetType, LPGROUPFILE);
	lpDataOffsets->lpCommandChannel	= _OFFSETOF(lpBuffer, 3, dwOffsetType, LPCOMMAND);
	lpDataOffsets->lpDataChannel	= _OFFSETOF(lpBuffer, 4, dwOffsetType, LPDATACHANNEL);
	lpDataOffsets->iCreditSection	= _OFFSETOF(lpBuffer, 5, dwOffsetType, INT);
	lpDataOffsets->iStatsSection	= _OFFSETOF(lpBuffer, 6, dwOffsetType, INT);
	lpDataOffsets->iShareSection	= _OFFSETOF(lpBuffer, 7, dwOffsetType, INT);
	lpDataOffsets->lpUnknown		= _OFFSETOF(lpBuffer, 8, dwOffsetType, LPVOID);
	lpDataOffsets->hMountFile		= _OFFSETOF(lpBuffer, 9, dwOffsetType, LPVOID);
	lpDataOffsets->lpFtpUserCaller	= _OFFSETOF(lpBuffer, 10, dwOffsetType, LPFTPUSER);
	lpDataOffsets->lpFtpUser  	    = _OFFSETOF(lpBuffer, 11, dwOffsetType, LPFTPUSER);
}
