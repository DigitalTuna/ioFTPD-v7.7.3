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


struct _PACKAGETRANSFER;

typedef struct _LINEBUFFER
{
	struct _PACKAGETRANSFER *lpTransfer;
	VOID				     (*lpCallBack)(LPVOID, LPSTR, DWORD, ULONG);
	LPVOID				     lpContext;

	DWORD	dwSize, dwLength, dwOffset, dwSlack;
	BOOL	bOverflow;
	CHAR	pBuffer[1];

} LINEBUFFER, *LPLINEBUFFER;


typedef struct _SECURITY
{
	CRITICAL_SECTION            csLock;
	SSL                        *SSL;
	BIO                        *NetworkBio;
	BIO                        *InternalBio;
	BOOL                        bCommunicating;  // during handshake TRUE if we need to send/receive stuff before trying again
	BOOL                        bSending;        // during handshake TRUE if sending
	BOOL                        bReceiving;      // during handshake TRUE if receiving or need to receive after sending

} * LPSECURITY, SECURITY;


typedef struct _SOCKET_OPTIONS
{
	DWORD	dwReceiveLimit;
	DWORD	dwSendLimit;
	DWORD	dwPriority;

} SOCKET_OPTIONS;


typedef struct _SENDQUICK
{
	struct _PACKAGETRANSFER *lpTransfer;
	VOID				     (*lpCallBack)(LPVOID, LPSTR, DWORD);
	LPVOID				     lpContext;
	SOCKETOVERLAPPED         Overlapped;

	HANDLE   hEvent;
	DWORD    dwLastError;

} SENDQUICK, *LPSENDQUICK;


typedef struct _IOSOCKET
{
	BOOL                    bInitialized;
	CRITICAL_SECTION        csLock;
	SOCKET volatile         Socket;
	PSELECT                 lpSelectEvent;
	// overlapped: receive, send
	SOCKETOVERLAPPED		Overlapped[2];
	DWORD					dwBandwidthLimit[2];
	SOCKET_OPTIONS			Options;
	LPSECURITY				lpSecure;  // if set then socket is using SSL
	DWORD                   dwClientId;
	LPIODEVICE				lpDevice;
	LPLINEBUFFER			lpLineBuffer;
	char                    szPrintedIP[16];
	USHORT                  usPort;
	SENDQUICK               SQ;

} IOSOCKET, * PIOSOCKET, * LPIOSOCKET;


#define SSL_ACCEPT			0001
#define SSL_CONNECT			0002
#define	SSL_LARGE_BUFFER	0100

#define	RECEIVE_LIMIT			0
#define	SEND_LIMIT				1
#define SOCKET_PRIORITY			2
#define IO_SOCKET				298

typedef void (*TRANSFERPROC)(struct _PACKAGETRANSFER *, DWORD, DWORD);

typedef struct _IOBUFFER
{
	PCHAR	buf;         // Buffer to send, or NULL if buffer of size 'size' should be allocated
	DWORD	len;         // Number of bytes to send/receive
	DWORD   offset;      // Current index into buf of next send/receive
	DWORD   size;        // Size of buffer to allocate/send if buf==NULL
	DWORD	dwType;      // PACKAGE type
	DWORD   dwTimerType; // Timeout for entire package or per transmit?
	DWORD   dwTimeOut;   // Timeout before aborting operation
	DWORD   ReturnAfter; // 0 = fill buffer, >1 after at least this many bytes received
	BOOL	bProcessed;  // TRUE when we have started processing the buffer
	BOOL    bAllocated;  // True if buf field was allocated by TransmitPackage and needs to be freed during cleanup

	LPIOFILE	ioFile;  // Handle to file being transferred if processing a file

} IOBUFFER, * LPIOBUFFER;

typedef struct _TRANSFERRATE
{
	DWORD			dwBytesTransferred;
	DWORD			dwTickCount;
	VOID			(*lpCallBack)(LPVOID, DWORD, DWORD, DWORD);

} TRANSFERRATE, * LPTRANSFERRATE;

typedef struct _PACKAGETRANSFER
{
	LPIOSOCKET			lpSocket;
	LPTIMER				lpTimer;
	LPIOBUFFER			lpIoBuffer;
	DWORD				dwBuffer;
	DWORD				dwLastError;
	ULONG               ulOpenSslError;
	LPTRANSFERRATE		lpTransferRate;
	INT64               i64Total;
	DWORD volatile		dwTime;
	BOOL                bNoFree;
	BOOL                bClosed; // Set to TRUE when a socket indicates it's been closed
	BOOL                bQuick;  // Set to TRUE if SendQuick which implies different overlapped structure
	BOOL                bTimedOut;
	VOID				(*lpCallBack)(LPVOID, DWORD, INT64, ULONG);
	LPVOID				lpContext;
	TRANSFERPROC        lpContinueProc; // When send/receive to socket completes call this function...

} PACKAGETRANSFER, *LPPACKAGETRANSFER;

#define PACKAGE_BUFFER_SEND			1
#define PACKAGE_LIST_BUFFER_SEND	2
#define PACKAGE_RECEIVE_LINE        3 // faked out as special case
#define PACKAGE_FILE_SEND			4
#define PACKAGE_FILE_RECEIVE		5
#define PACKAGE_SSL_ACCEPT          6
#define PACKAGE_SSL_CONNECT         7
#define PACKAGE_SHUTDOWN			8


#define NO_TIMER		0
#define TIMER_PER_TRANSMIT	1
#define TIMER_PER_PACKAGE	2
typedef void(*TRANSFERPROC)(LPPACKAGETRANSFER, DWORD, DWORD);

//	Initialization routines
BOOL Socket_Init(BOOL bFirstInitialization);
VOID Socket_DeInit(VOID);
BOOL Security_Init(BOOL bFirstInitialization);
VOID Security_DeInit(VOID);
VOID IoSocketInit(LPIOSOCKET lpSocket);

//	Tcp socket api
BOOL SetSocketOption(LPIOSOCKET lpSocket, INT iLevel, INT iOptionName, LPVOID lpValue, INT iValue);
BOOL ReceiveOverlapped(LPIOSOCKET lpSocket, LPSOCKETOVERLAPPED lpOverlapped);
BOOL SendOverlapped(LPIOSOCKET lpSocket, LPSOCKETOVERLAPPED lpOverlapped);
BOOL SendQueuedIO(LPSOCKETOVERLAPPED lpOverlapped);
BOOL ReceiveQueuedIO(LPSOCKETOVERLAPPED lpOverlapped);
DWORD FlushLineBuffer(LPIOSOCKET lpSocket, LPVOID lpBuffer, DWORD dwBuffer);
BOOL CloseSocket(LPIOSOCKET lpSocket, BOOL bNoLinger);
BOOL ioCloseSocket(LPIOSOCKET lpSocket, BOOL bNoLinger);
BOOL ioDeleteSocket(LPIOSOCKET lpSocket, BOOL bNoLinger);
//	Secure socket api
BOOL Secure_Init_Socket(LPIOSOCKET lpSocket, LPIOSERVICE lpService, DWORD dwCreationFlags);
BOOL Secure_Create_Ctx(LPTSTR tszService, LPSTR szCertificateName, const SSL_METHOD *Method, SSL_CTX **ppCtx);
VOID Secure_Free_Ctx(SSL_CTX *pCtx);
BOOL Secure_Delete_Cert(LPTSTR tszCertificateName);
BOOL Secure_MakeCert(LPSTR szCertName);
//	Hostname handling
BOOL BindSocket(SOCKET Socket, ULONG lAddress, USHORT sPort, BOOL bReuse);
BOOL BindSocketToDevice(LPIOSERVICE lpService, LPIOSOCKET lpSocket, PULONG pNetworkAddress, PUSHORT pPort, DWORD dwFlags);
VOID UnbindSocket(LPIOSOCKET lpSocket);
ULONG HostToAddress(LPSTR szHostName);
//	Socket scheduling
UINT WINAPI SocketSchedulerThread(LPVOID lpNull);
VOID UnregisterSchedulerDevice(LPIODEVICE lpDevice);
VOID RegisterSchedulerDevice(LPIODEVICE lpDevice);
//	Asynchronous message based functions notification
BOOL WSAAsyncSelectWithTimeout(LPIOSOCKET lpIoSocket, DWORD dwTimeOut, DWORD dwFlags, LPDWORD lpResult);
BOOL WSAAsyncSelectCancel(LPIOSOCKET lpIoSocket);
BOOL WSAAsyncSelectContinue(LPIOSOCKET lpIoSocket, LPVOID lpProc, LPVOID lpContext);

//	Transmit package API
BOOL TransmitPackage_Init(BOOL bFirstInitialization);
VOID TransmitPackage_DeInit(VOID);
VOID TransmitPackages(LPIOSOCKET lpSocket, LPIOBUFFER lpBuffer, DWORD dwBuffer, VOID (* lpTransferRateCallBack)(LPVOID, DWORD, DWORD, DWORD),
					  VOID (* lpCallBack)(LPVOID, DWORD, INT64, ULONG), LPVOID lpContext);
VOID TransmitPackage_ReadSocket(LPPACKAGETRANSFER lpTransfer, DWORD dwBytesRead, DWORD dwLastError);
VOID TransmitPackage_WriteSocket(LPPACKAGETRANSFER lpTransfer, DWORD dwBytesRead, DWORD dwLastError);
VOID TransmitPackage_ReadSecureSocket(LPPACKAGETRANSFER lpTransfer, DWORD dwBytesRead, DWORD dwLastError);
VOID TransmitPackage_WriteSecureSocket(LPPACKAGETRANSFER lpTransfer, DWORD dwBytesRead, DWORD dwLastError);

BOOL SendQuick(LPIOSOCKET lpSocket, LPTSTR tszString, DWORD dwBytes);
SOCKET OpenSocket(VOID);

VOID ReceiveLine(LPIOSOCKET lpSocket, DWORD dwLineLength, VOID (* lpCallBack)(LPVOID, LPSTR, DWORD, ULONG), LPVOID lpContext);

//	MsWsock extensions
extern LPFN_ACCEPTEX				Accept;
extern LPFN_GETACCEPTEXSOCKADDRS	GetAcceptSockAddrs;


extern BOOL volatile bLogOpenSslErrors;
extern DWORD dwDeadlockPort;
