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

typedef struct _CLASS
{
	LONG			lMaxClients;
	LONG volatile 	lClients;
	struct _CLASS	*lpNext;
	TCHAR			tszName[1];

} CLASS, * LPCLASS;


typedef struct _RULE
{
	DWORD			dwType;
	LPVOID			lpContext;
	DWORD			dwContext;
	struct _RULE	*lpNext;

} RULE, * LPRULE;

typedef struct _ACCEPT_RULE
{
	DWORD		dwType;
	LPVOID		lpContext;
	DWORD		dwContext;
	LPRULE		lpNext;

	LONG		lConnectionsPerHost;
	LPCLASS		lpClass;

} ACCEPT_RULE, * LPACCEPT_RULE;


typedef struct _DENY_RULE
{
	DWORD		dwType;
	LPVOID		lpContext;
	DWORD		dwContext;
	LPRULE		lpNext;

	LPTSTR		tszLog;

} DENY_RULE, * LPDENY_RULE;


typedef struct _HOSTCLASS
{
	LPCLASS	lpClass;
	LONG	lConnectionsPerHost;
	DWORD	dwClassListId;

} HOSTCLASS, *LPHOSTCLASS;


typedef struct _BANINFO
{
	DWORD			dwBanDuration;
	BYTE			pNetworkAddress[8];
	DWORD			dwNetworkAddress;
	DWORD			dwConnectionAttempts;
	struct _BANINFO	*lpNext;

} BANINFO, *LPBANINFO;


#define	ACCEPT		0001
#define	DENY		0002
#define	HOSTNAME	0010
#define	IP			0020



typedef struct _HOSTINFO
{
	BYTE			NetworkAddress[8];
	DWORD			dwNetworkAddress;
	LPSTR			szHostName;
	DWORD			dwHostNameCacheTime;
	LPSTR			szIdent;
	DWORD			dwIdentCacheTime;
	DWORD			dwLastOccurance;
	DWORD           dwLastAutoBanLogDelay;
	DWORD			dwLastAutoBanLogTime;
	DWORD           dwLastUnMatchedLogDelay;
	DWORD           dwLastUnMatchedLogTime;
	DWORD           dwLastBadHostLogDelay;
	DWORD           dwLastBadHostLogTime;
	DWORD           dwLastFailedLoginType; // so different messages show up
	DWORD           dwLastFailedLoginLogDelay;
	DWORD           dwLastFailedLoginLogTime;
	DWORD			dwAttemptCount;
	DWORD			dwShareCount;
	DWORD           dwKnockedTicks;
	HOSTCLASS		ClassInfo;
	LONG volatile	lClients;
	LONG volatile	lLock;

	// re-use should be OK since sharecount > 1 and thus not on free list...
	struct _HOSTINFO	*lpNext;	//	Also used as resolve queue
	struct _HOSTINFO	*lpPrev;	//	Also used as ident reading queue

} HOSTINFO, * LPHOSTINFO;


typedef struct _IDENTCLIENT
{
	IOSOCKET	ioSocket;
	CHAR		pBuffer[256];
	LPSTR		szIdentReply;
	LPTIMER		lpTimer;
	DWORD		dwBuffer;
	DWORD		dwLastError;
	DWORD		dwClientId;
	WORD		wServerPort, wClientPort;
//	WORD		wClientPort;
//	BOOL		bSend;
	LPHOSTINFO	lpHostInfo;

} IDENTCLIENT, * LPIDENTCLIENT;


typedef struct _RESOLVE
{
	LPHOSTINFO	lpHostInfo;
	struct _RESOLVE	*lpNext;

} RESOLVE, * LPRESOLVE;

typedef struct _USERIPHOSTMASK {
	INT32  Uid;
	BOOL   bNumeric;
	CHAR   Ip[_IP_LINE_LENGTH + 1];
} USERIPHOSTMASK, *LPUSERIPHOSTMASK;

typedef struct _USERMASKSET {
	volatile DWORD  dwLock;
	volatile DWORD  dwCount;
	volatile DWORD  dwMax;
	volatile LPUSERIPHOSTMASK *lppUserIpHostMasks;
} USERMASKSET, *LPUSERMASKSET;

extern DWORD volatile dwRandomLoginDelay;
extern DWORD volatile dwDynamicDnsLookup;
extern DWORD volatile dwMaxLogSuppression;
extern DWORD volatile dwLogSuppressionIncrement;
extern DWORD volatile dwIdentTimeOut;
extern BOOL  volatile bIgnoreHostmaskIdents;

BOOL IsNumericIP(char *szIpHost);
VOID UserIpHostMaskRemove(INT32 Uid);
BOOL UserIpHostMaskUpdate(LPUSERFILE lpUserFile);
BOOL UserIpHostMaskKnown(LPTSTR tszHostName, struct in_addr *InetAddress);
INT  UserIpHostMaskMatches(LPBUFFER lpBuffer, LPTSTR tszMultilinePrefix, LPTSTR tszHost);
INT  ImmuneMaskList(LPBUFFER lpBuffer, LPTSTR tszMultilinePrefix);

LPBANINFO GetNetworkBans(VOID);
VOID UnbanNetworkAddress(LPTSTR tszNetworkAddressMask, LPBUFFER lpBuffer, LPTSTR tszMultilinePrefix);
DWORD GetCurrentClassListId(VOID);
VOID GetHostClass(LPHOSTINFO lpHostInfo);
BOOL IdentifyClient(LPNEWCLIENT lpNewClient);
BOOL Identify_Init(BOOL bFirstInitialization);
LPHOSTINFO RegisterNetworkAddress(PBYTE pNetworkAddress, DWORD dwNetworkAddress);
VOID UnregisterNetworkAddress(LPHOSTINFO lpHostInfo, DWORD dwCount);
VOID Identify_DeInit(VOID);
VOID Identify_Shutdown(VOID);
BOOL IpHasKnocked(PBYTE pNetworkAddress, DWORD dwAddrLen);
LPSTR Obscure_IP(LPSTR szBuffer, struct in_addr *pAddr);
LPSTR Obscure_Host(LPSTR szBuffer, LPSTR szHost);
LPSTR Obscure_Mask(LPSTR szBuffer, LPSTR szMask);
LPTSTR Admin_Knock(struct _FTP_USER *lpUser, LPTSTR tszMultilinePrefix, LPIO_STRING Args);
