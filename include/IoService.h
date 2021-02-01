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

typedef struct _IOPORT
{
	USHORT			sLowPort;
	USHORT			sHighPort;
	struct _IOPORT	*lpNext;

} IOPORT, * LPIOPORT;

typedef struct _ACCEPT_SOCKET
{
	SOCKET			Socket;
	IOOVERLAPPED	Overlapped;

} ACCEPT_SOCKET, * LPACCEPT_SOCKET;


typedef struct _BANDWIDTH
{
	LPSOCKETOVERLAPPED	lpIOQueue[3][2];	//	IO Queues (synchronized, delayed, unsynchronized)
	DWORD				dwIOQueue[3];
	DWORD				dwClientBandwidthLimit;	//
	DWORD				dwGlobalBandwidthLimit;	//	Bandwidth limit
	DWORD				dwGlobalBandwidthLeft;	//	Bandwidth left
	BOOL				bGlobalBandwidthLimit;	//	Bandwidth limit is being enforced
	LONG volatile		lLock;				//	Lock

} BANDWIDTH, * LPBANDWIDTH;


typedef struct _IODEVICE
{
	LPTSTR				tszName;
	LONG                lDeviceNum;
	ULONG				lBindAddress;
	ULONG				lHostAddress;
	LPIOPORT			lpPort;
	LPIOPORT			lpOutPorts;
	BOOL				bActive;
	BOOL				bDisabled;
	LONG volatile		lShareCount;
	DWORD				dwLastUpdate;
	DWORD               dwLastConfigCounter;
	BOOL				bRandomizePorts;
	LONG volatile		lLastPort;
	LONG volatile		lLastOutPort;
	struct _IODEVICE	*lpNext;
	struct _IODEVICE	*lpPrev;

	//	Traffic shaping variables
	BANDWIDTH			Inbound;
	BANDWIDTH			Outbound;
	struct _IODEVICE	*lpNextSDevice;
	struct _IODEVICE	*lpPrevSDevice;

} IODEVICE, * LPIODEVICE;


typedef struct _IOADDRESS {
	struct in_addr Addr;
	LPTSTR         tszString;
} IOADDRESS, * LPIOADDRESS;


typedef struct _IOADDRESSLIST {
	LONG volatile lReferenceCount;
	DWORD         dwAddressArray;
	LPIOADDRESS   lpAddressArray;
} IOADDRESSLIST, *LPIOADDRESSLIST;


typedef struct _IOSERVICE
{
	LPTSTR			tszName;
	BOOL volatile	bActive;
	LPIODEVICE		lpDevice;
	DWORD			dwType;
	SOCKET			Socket;
	ULONG			lAddress;
	USHORT			sPort;
	volatile struct sockaddr_in  addrLocal;
	struct sockaddr_in           addrService;
	DWORD           dwTestCounter;
	struct _NEWCLIENT volatile *lpAcceptClients[10];
	LONG  volatile  lAcceptClients;
	LONG  volatile  lLastConnect;

	LPTSTR		tszAllowedUsers;
	LPTSTR		tszRequireSecureAuth;
	LPTSTR		tszRequireSecureData;
	LPTSTR		tszMessageLocation;
	LPTSTR		tszBncAddressArray[10];

	SSL_CTX *   pSecureCtx;
	BOOL		bExplicitEncryption;
	BOOL        bTlsSupported;
	BOOL        bSslSupported;
	DWORD       dwFoundCredentials;
	LPTSTR      tszHostValue;
	LPTSTR      tszServiceValue;

	LPIODEVICE	*lpDataDevices;
	DWORD		dwDataDevices;
	BOOL		bRandomDataDevices;

	LONG		  lMaxClients;
	LONG volatile lClients;

	LONG volatile	lTransfers;
	LONG volatile	lLastDataDevice;
	LPIOADDRESSLIST lpPortDeniedAddresses;

	LPVOID		lpAcceptProc;
	LPVOID		lpCloseProc;

	LOCKOBJECT			loLock;
	struct _IOSERVICE	*lpNext;

} IOSERVICE, * LPIOSERVICE;


#define	BIND_PORT		0001
#define	BIND_IP			0002
#define BIND_DATA		0004
#define BIND_REUSE		0010
#define BIND_MINUS_PORT	0020
#define BIND_TRY_AGAIN  0040
#define BIND_FAKE		0100

#define C_FTP		1

VOID Service_AcceptClient(LPVOID lpClient, DWORD dwBytesReceived, DWORD dwLastError);
BOOL Service_RequireSecureAuth(LPIOSERVICE lpService, LPUSERFILE lpUserFile);
BOOL Service_RequireSecureData(LPIOSERVICE lpService, LPUSERFILE lpUserFile);
BOOL Service_IsAllowedUser(LPIOSERVICE lpService, LPUSERFILE lpUserFile);
LPTSTR Service_MessageLocation(LPIOSERVICE lpService);
VOID Service_GetCredentials(LPIOSERVICE lpService, BOOL bCreate);

BOOL Service_Start(LPTSTR tszServiceName);
BOOL Service_Stop(LPTSTR tszServiceName, BOOL bKillClients);
BOOL Services_Start(VOID);
BOOL Services_Stop(VOID);
BOOL Services_Init(BOOL bFirstInitialization);
VOID Services_DeInit(VOID);

LPIOSERVICE Service_FindByName(LPTSTR tszServiceName);
VOID Service_ReleaseLock(LPIOSERVICE lpService);

LPIODEVICE Device_FindByName(LPTSTR tszDeviceName);
VOID Device_ReleaseLock(LPIODEVICE lpDevice);

BOOL Services_Test(PINT lpiNumServices, PINT lpiActiveServices, PINT lpiOnlineServices, PINT lpiFailedServices);

VOID ioAddressListFree(LPIOADDRESSLIST lpAddressList);
BOOL ioAddressFind(LPIOADDRESSLIST lpAddressList, struct in_addr Addr);
