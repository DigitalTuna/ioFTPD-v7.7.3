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


typedef struct _DATA_OFFSETS
{
	LPBUFFER			lpBuffer;
	DWORD				dwHave;
	DWORD				dwOffsetType;

	LPUSERFILE			lpUserFile;
	LPGROUPFILE			lpGroupFile;
	INT					iPosition;
	INT					iCreditSection;
	INT					iStatsSection;
	INT					iShareSection;
	PCONNECTION_INFO	pConnectionInfo;
	LPCOMMAND			lpCommandChannel;
	LPDATACHANNEL		lpDataChannel;
	MOUNTFILE			hMountFile;
	LPFTPUSER           lpFtpUserCaller;
	LPFTPUSER           lpFtpUser;

	//	Unknown data
	LPVOID				lpUnknown;

} DATA_OFFSETS, * LPDATA_OFFSETS;

typedef struct _DATA_OFFSET
{
	LPDWORD	lpOffset;
	BOOL	(* ConditionFunc)(LPVOID);
	BYTE	bConditionParam;
	DWORD	dwHave[2];

} DATA_OFFSET;



#define	DATA_CONNECTION		pConnectionInfo
#define	DATA_USERFILE		lpUserFile
#define	DATA_GROUPFILE		lpGroupFile
#define DATA_CCHANNEL		lpCommandChannel
#define DATA_DCHANNEL		lpDataChannel
#define	DATA_CSECTION		iCreditSection
#define	DATA_SSECTION		iStatsSection
#define	DATA_SHARESECTION	iShareSection
#define	DATA_POSITION		lpUnknown
#define	DATA_WHO			lpUnknown
#define	DATA_FILEINFO		lpUnknown
#define	DATA_MOUNTTABLE		hMountFile
#define DATA_USER_CALLER    lpFtpUserCaller
#define DATA_FTPUSER        lpFtpUser



#define	HASDATAEX(x, y)		(x.dwHave & y)
#define	GETOFFSET(x, y)		(x.y)

VOID InitDataOffsets(LPDATA_OFFSETS lpDataOffsets, LPVOID lpBuffer, DWORD dwOffsetType);
