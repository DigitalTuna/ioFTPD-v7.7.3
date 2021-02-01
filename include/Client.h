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

typedef struct _CLIENTSLOT
{
	DWORD				dwId;
	LONG volatile		lJobLock;
	LONG volatile		lDataLock;
	struct _CLIENTSLOT	*lpNext;

} CLIENTSLOT, * LPCLIENTSLOT;



typedef struct _CLIENT
{
	ONLINEDATA		Static;
	DWORD           dwTransferLastUpdated; // didn't want to change exported ONLINEDATA field
	LPIOSERVICE		lpService;
	DWORD			dwLoginTime;

	DWORD			dwJobFilter;
	LPCLIENTJOB		lpJobList;
	LPCLIENTJOB		lpActiveJobList;

	LPCLIENTJOB		lpActiveJob;
	LPCLIENTJOB		lpJobQueue[2];
	DWORD			dwActiveFlags;

	DWORD           lClientCount;  // lClientCounter when created

	//	Synchronization items
	LPCLASS			lpClass;
	LPHOSTINFO		lpHostInfo;
	LPFTPUSER		lpUser;

} CLIENT, * LPCLIENT;


#define	DATA_TRANSFER			1000
#define	DATA_AUTHENTICATE		1001
#define	DATA_CHDIR				1002
#define	DATA_ACTION				1003
#define	DATA_TRANSFER_UPDATE	1004
#define	DATA_IDENT				1005
#define DATA_NOOP               1006
#define	DATA_DEAUTHENTICATE		1007

VOID ShowJobList(LPCSTR Prefix, LPBUFFER lpBuffer);
LRESULT SeekOnlineData(ONLINEDATA *lpOnlineData, DWORD dwClientId);
LRESULT GetOnlineData(ONLINEDATA *lpOnlineData, DWORD dwClientId);
LRESULT KickUser(INT Uid);
LRESULT KillUser(UINT32 dwClientId);
LRESULT	ReleasePath(LPTSTR RealPath, LPTSTR VirtualPath);
INT     KillService(LPIOSERVICE lpService);

LPCLIENT LockClient(DWORD dwClientId);
VOID UnlockClient(DWORD dwClientId);
DWORD RegisterClient(PCONNECTION_INFO pConnectionInfo, LPFTPUSER lpUser);
LPCLIENT UnregisterClient(DWORD dwClientId);
BOOL UpdateClientData(DWORD dwDataType, DWORD dwClientId, ...);
BOOL Client_Init(BOOL bFirstInitialization);
VOID Client_DeInit(VOID);

LONG volatile *GetClientTransferData(DWORD hClient);

extern DWORD volatile dwMaxClientId;
