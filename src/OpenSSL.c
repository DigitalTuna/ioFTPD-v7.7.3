/*
* Copyright(c) 2006 Yil@Wondernet.nu
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

#include <openssl/pem.h>
#include <openssl/conf.h>
#include <openssl/x509v3.h>
#include <openssl/engine.h>

// include the "glue" for handling different runtime libraries
#include <ms/applink.c>

static INT     iSecureLockArray;
static HANDLE *SecureLockArray;

struct CRYPTO_dynlock_value
{
	HANDLE hMutex;
};

typedef struct _OPENSSLPROGRESS
{
	LPIOSOCKET lpIoSocket;
	LPBUFFER   lpBuffer;
	LPTSTR     tszPrefix;
	LPTSTR     tszBlank;
	BOOL       bShowDots;
	VOID      (*OutputFunc)(struct LPOPENSSLPROGRESS, LPSTR, ...);

} OPENSSLPROGRESS, *LPOPENSSLPROGRESS;


VOID MyOpenSSL_PutLog(LPOPENSSLPROGRESS lpProgress, LPSTR szFormat, ...)
{
	va_list	Arguments;

	va_start(Arguments, szFormat);
	PutlogVA(LOG_DEBUG, szFormat, Arguments);
	va_end(Arguments);
}


VOID MyOpenSSL_Format(LPOPENSSLPROGRESS lpProgress, LPSTR szFormat, ...)
{
	va_list	Arguments;

	va_start(Arguments, szFormat);
	FormatStringAVA(lpProgress->lpBuffer, szFormat, Arguments);
	va_end(Arguments);
}


int MyOpenSSL_Progress_Callback (int p, int n, BN_GENCB *bn_GenCB)
{
	LPOPENSSLPROGRESS lpProgress;
	LPBUFFER          lpBuffer;
	TCHAR             tc;
	VOID             (*Out)(LPOPENSSLPROGRESS, LPSTR, ...);


	lpProgress = (LPOPENSSLPROGRESS) bn_GenCB->arg;
	lpBuffer   = lpProgress->lpBuffer;
	Out        = lpProgress->OutputFunc;

	if (lpBuffer->len == 0 && lpProgress->bShowDots)
	{
		(Out)(lpProgress, _T("%s"), lpProgress->tszPrefix);
	}

	if (p == 3)
	{
		// we're done...
		(Out)(lpProgress, _T("(DONE)\r\n"));
	}
	else
	{
		tc = _T('*');
		if (p == 0) tc=_T('.');
		if (p == 1) tc=_T('+');
		if (lpProgress->bShowDots)
		{
			(Out)(lpProgress, _T("%c"), tc);
		}
	}

	// now check to see if got 70 characters on the line so far and if we are finished.
	// If so flush the buffer and start a new line...
	if ((lpBuffer->len > 70) || (p == 3))
	{
		if (lpProgress->bShowDots && (p != 3))
		{
			(Out)(lpProgress, _T("\r\n"));
		}
		if (lpProgress->lpIoSocket)
		{
			SendQuick(lpProgress->lpIoSocket, lpBuffer->buf, lpBuffer->len);
			lpBuffer->len = 0;
		}
	}
	return 1;
}



BOOL MakeCert(LPSTR szCertName, LPOPENSSLPROGRESS lpProgress)
{
	LPBUFFER   lpBuffer;
	BN_GENCB   CallBack;
	TCHAR      tszFileName[MAX_PATH+1];
	X509      *pX509  = NULL;
	EVP_PKEY  *PKey   = NULL;
	RSA       *pRsa   = NULL;
	BIGNUM    *pBN_e  = NULL;
	DH        *pDH    = NULL;
	X509_NAME *xName  = NULL;
	FILE       *File  = NULL;
	size_t     sLen;
	errno_t    err;
	int        i;
	BOOL       bReturn = FALSE;
	VOID      (*Out)(LPOPENSSLPROGRESS, LPSTR, ...);

	lpBuffer = lpProgress->lpBuffer;
	Out = lpProgress->OutputFunc;

	sLen = _tcslen(szCertName);
	if (sLen+6 > sizeof(tszFileName)/sizeof(*tszFileName))
	{
		(Out)(lpProgress, _T("%sPath/Filename too long.\r\n"), lpProgress->tszPrefix);
		return FALSE;
	}

	if (!RAND_status())
	{
		(Out)(lpProgress, _T("%sRandom number generator not random enough.\r\n"), lpProgress->tszPrefix);
		goto error;
	}

	srand(GetCurrentThreadId() * GetTickCount() + (DWORD)GetCurrentFiber());

	// just make it generate a random number of bytes...
	RAND_bytes(tszFileName, rand() % sizeof(tszFileName));

	if (!(pBN_e = BN_new()) || !(BN_set_word(pBN_e, RSA_F4)))
	{
		goto error;
	}

	if (!(PKey = EVP_PKEY_new()))
	{
		goto error;
	}

	if (!(pX509 = X509_new()))
	{
		goto error;
	}

	if (!(pRsa = RSA_new()))
	{
		goto error;
	}

	(Out)(lpProgress, _T("%sGenerating and Validating 1024 bit RSA key for certificate:\r\n"), lpProgress->tszPrefix);
	if (lpProgress->lpIoSocket)
	{
		SendQuick(lpProgress->lpIoSocket, lpBuffer->buf, lpBuffer->len);
		lpBuffer->len = 0;
	}

	BN_GENCB_set(&CallBack, MyOpenSSL_Progress_Callback, lpProgress);
	if (!RSA_generate_key_ex(pRsa, 1024, pBN_e, &CallBack))
	{
		(Out)(lpProgress, _T("%s%sFailed to generate RSA key for certificate (1024 bits).\r\n"), lpProgress->tszBlank, lpProgress->tszPrefix);
		goto error;
	}
	if (lpProgress->lpIoSocket && lpBuffer->len)
	{
		SendQuick(lpProgress->lpIoSocket, lpBuffer->buf, lpBuffer->len);
		lpBuffer->len = 0;
	}

	if (!EVP_PKEY_assign_RSA(PKey,pRsa))
	{
		goto error;
	}
	// Rsa now owned by PKey so we don't want to free it
	pRsa = NULL;

	if (!ASN1_INTEGER_set(X509_get_serialNumber(pX509), (LONG) time(NULL)))
	{
		goto error;
	}

	// make it valid as of yesterday to handle clients in a timezone where it's yesterday locally :)
	X509_gmtime_adj(X509_get_notBefore(pX509), -60*60*24);

	X509_gmtime_adj(X509_get_notAfter(pX509),(long)60*60*24*365*10); // ten years

	if (!X509_set_pubkey(pX509, PKey))
	{
		goto error;
	}

	// get the field where we can set Country, Name, etc...
	xName = X509_get_subject_name(pX509);

	// Add name of cert to subject line
	if (!X509_NAME_add_entry_by_txt(xName, "CN", MBSTRING_ASC, szCertName, -1, -1, 0))
	{
		goto error;
	}

	// We are the issuer since self signing it
	if (!X509_set_issuer_name(pX509,xName))
	{
		goto error;
	}

	if (!X509_sign(pX509, PKey, EVP_sha1()))
	{
		goto error;
	}

	if (!X509_verify(pX509, PKey))
	{
		(Out)(lpProgress, _T("%sFailed to verify RSA key for certificate.\r\n"), lpProgress->tszPrefix);
		goto error;
	}


	if (!(pDH = DH_new()))
	{
		goto error;
	}

	(Out)(lpProgress, _T("%s%sGenerating 1024 bit Diffie-Huffman parameters:\r\n"), lpProgress->tszBlank, lpProgress->tszPrefix);
	if (lpProgress->lpIoSocket && lpBuffer->len)
	{
		SendQuick(lpProgress->lpIoSocket, lpBuffer->buf, lpBuffer->len);
		lpBuffer->len = 0;
	}

	if (!DH_generate_parameters_ex(pDH, 1024, DH_GENERATOR_5, &CallBack))
	{
		(Out)(lpProgress, _T("%sFailed to generate RSA key for certificate.\r\n"), lpProgress->tszPrefix);
		goto error;
	}

	if (!DH_check(pDH, &i))
	{
		goto error;
	}

	if (i & DH_CHECK_P_NOT_PRIME)
	{
		(Out)(lpProgress, _T("%sFailed to generate a prime.\r\n"), lpProgress->tszPrefix);
		goto error;
	}
	if (i & DH_CHECK_P_NOT_SAFE_PRIME)
	{
		(Out)(lpProgress, _T("%sFailed to generate a safe prime.\r\n"), lpProgress->tszPrefix);
		goto error;
	}
	if (i & DH_UNABLE_TO_CHECK_GENERATOR)
	{
		(Out)(lpProgress, _T("%sFailed to validate generator.\r\n"), lpProgress->tszPrefix);
		goto error;
	}
	if (i & DH_NOT_SUITABLE_GENERATOR)
	{
	    (Out)(lpProgress, _T("%sInvalid generator.\r\n"), lpProgress->tszPrefix);
		goto error;
	}

	Secure_Delete_Cert(szCertName);

	_sntprintf_s(tszFileName, sizeof(tszFileName)/sizeof(*tszFileName), _TRUNCATE, _T("%s.key"), szCertName);
	if (err = _tfopen_s(&File, tszFileName, _T("wN")))
	{
		goto error;
	}

	if (!PEM_write_PKCS8PrivateKey(File, PKey, NULL, NULL, 0, NULL, NULL))
	{
		goto error;
	}
	fclose(File);
	File = NULL;

	_tcscpy_s(&tszFileName[sLen], sizeof(tszFileName)/sizeof(*tszFileName)-sLen, ".pem");
	if (err = _tfopen_s(&File, tszFileName, _T("wN")))
	{
		goto error;
	}

	if (!PEM_write_X509(File, pX509))
	{
		goto error;
	}
	fclose(File);
	File = NULL;

	_tcscpy_s(&tszFileName[sLen], sizeof(tszFileName)/sizeof(*tszFileName)-sLen, ".dhp");
	if (err = _tfopen_s(&File, tszFileName, _T("wN")))
	{
		goto error;
	}

	if (!PEM_write_DHparams(File, pDH))
	{
		goto error;
	}
	fclose(File);
	File = NULL;

	bReturn = TRUE;

error:
	if (pBN_e) BN_free(pBN_e);
	if (pRsa)  RSA_free(pRsa);
	if (PKey)  EVP_PKEY_free(PKey);
	if (pX509) X509_free(pX509);
	if (pDH)   DH_free(pDH);
	return bReturn;
}



// TRUE if successful
BOOL Secure_MakeCert(LPSTR szCertName)
{
	OPENSSLPROGRESS OpenSslProgress;
	BUFFER          Buffer;
	BOOL            bReturn;

	ZeroMemory(&Buffer, sizeof(Buffer));

	OpenSslProgress.lpBuffer   = &Buffer;
	OpenSslProgress.tszPrefix  = _T("");
	OpenSslProgress.tszBlank   = _T("");
	OpenSslProgress.lpIoSocket = NULL;
	OpenSslProgress.bShowDots  = FALSE;
	OpenSslProgress.OutputFunc = MyOpenSSL_PutLog;

	bReturn = MakeCert(szCertName, &OpenSslProgress);

	if (Buffer.buf)
	{
		Free(Buffer.buf);
	}
	return bReturn;
}


LPTSTR Admin_MakeCert(LPFTPUSER lpUser, LPTSTR tszMultilinePrefix, LPIO_STRING Args)
{
	LPTSTR			tszUserName, tszCommand, tszCert;
	LPIOSERVICE     lpService;
	LPBUFFER        lpBuffer;
	DWORD           dwPrevious, dwError;
	OPENSSLPROGRESS OpenSslProgress;
	TCHAR           tszPrefix[64];


	//	Get arguments
	tszCommand  = GetStringIndexStatic(Args, 0);
	if (GetStringItems(Args) != 1) ERROR_RETURN(ERROR_INVALID_ARGUMENTS, GetStringRange(Args, 1, STR_END));

	tszUserName	= LookupUserName(lpUser->UserFile);
	lpBuffer	= &lpUser->CommandChannel.Out;

	if (HasFlag(lpUser->UserFile, _TEXT("M")))
	{
		ERROR_RETURN(IO_NO_ACCESS, tszCommand);
	}

	lpService = lpUser->Connection.lpService;
	tszCert = 0;

	if (lpService->tszServiceValue)
	{
		tszCert = lpService->tszServiceValue;
		FormatString(lpBuffer, _TEXT("%sNAME=\"%s\" [%s (Certificate_Name)]\r\n"),
			tszMultilinePrefix, tszCert, lpService->tszName);
		if (lpService->dwFoundCredentials == 1)
		{
			ERROR_RETURN(CRYPT_E_EXISTS, tszCommand);
		}
	}
	else if (lpService->tszHostValue)
	{
		tszCert = lpService->tszHostValue;
		FormatString(lpBuffer, _TEXT("%sNAME=\"%s\" [%s Device (HOST=)]\r\n"),
			tszMultilinePrefix, tszCert, lpService->tszName);
		if (lpService->dwFoundCredentials == 2)
		{
			ERROR_RETURN(CRYPT_E_EXISTS, tszCommand);
		}
	}
	else
	{
		tszCert = _T("ioFTPD");
		FormatString(lpBuffer, _TEXT("%sNAME=\"%s\" [%s (default name)]\r\n"),
			tszMultilinePrefix, tszCert, lpService->tszName);
		if (lpService->dwFoundCredentials == 3)
		{
			ERROR_RETURN(CRYPT_E_EXISTS, tszCommand);
		}
	}

	_stprintf(tszPrefix, "%s\r\n", tszMultilinePrefix);

	OpenSslProgress.lpBuffer   = lpBuffer;
	OpenSslProgress.tszPrefix  = tszMultilinePrefix;
	OpenSslProgress.tszBlank   = tszPrefix;
	OpenSslProgress.lpIoSocket = &lpUser->CommandChannel.Socket;
	OpenSslProgress.bShowDots  = TRUE;
	OpenSslProgress.OutputFunc = MyOpenSSL_Format;

	if (!MakeCert(tszCert, &OpenSslProgress))
	{
		dwError = ERROR_COMMAND_FAILED;
		Putlog(LOG_ERROR, _TEXT("Failed to generate new SSL cert \"%s\". User=%s\r\n"),
			tszCert, tszUserName);
		ERROR_RETURN(dwError, tszCommand);
	}

	Putlog(LOG_GENERAL, _TEXT("SSL: \"Successfully generated new cert: %s\" \"User=%s\".\r\n"),
		tszCert, tszUserName);

	// force a reload
	AcquireExclusiveLock(&lpService->loLock);

	dwPrevious = lpService->dwFoundCredentials;

	Secure_Free_Ctx(lpService->pSecureCtx);
	lpService->pSecureCtx = NULL;
	lpService->dwFoundCredentials = 0;

	Service_GetCredentials(lpService, FALSE);

	ReleaseExclusiveLock(&lpService->loLock);

	if (lpService->dwFoundCredentials != 4 && lpService->dwFoundCredentials < dwPrevious)
	{
		FormatString(lpBuffer, _TEXT("\r\n%sSuccessfully loaded new cert!\r\n"), tszMultilinePrefix);
		return NULL;
	}
	else
	{
		dwError = ERROR_COMMAND_FAILED;
		FormatString(lpBuffer, _TEXT("\r\n%sFailed to load new cert.\r\n"), tszMultilinePrefix);
		ERROR_RETURN(dwError, tszCommand);
	}
}





BOOL
Secure_Init_Socket(LPIOSOCKET lpSocket,
				   LPIOSERVICE lpService,
				   DWORD dwCreationFlags)
{
	LPSECURITY  lpSecure;
	DWORD      dwBufSize;

	if (lpSocket->lpSecure || !lpService->pSecureCtx) return TRUE;
	//  Allocate memory for security buffers
	if (! (lpSecure = (LPSECURITY)Allocate("Socket:Secure:Structure", sizeof(SECURITY)))) return TRUE;
	ZeroMemory(lpSecure, sizeof(SECURITY));

	AcquireSharedLock(&lpService->loLock);
	if (lpService->pSecureCtx)
	{
		lpSecure->SSL = SSL_new(lpService->pSecureCtx);
	}
	ReleaseSharedLock(&lpService->loLock);

	if (!lpSecure->SSL)
	{
		Free(lpSecure);
		SetLastError(IO_SSL_FAIL);
		return TRUE;
	}

	InitializeCriticalSectionAndSpinCount(&lpSecure->csLock, 1000);

	if (dwCreationFlags & SSL_ACCEPT)
	{
		SSL_set_accept_state(lpSecure->SSL);
	}
	else
	{
		SSL_set_connect_state(lpSecure->SSL);
	}

	//  Allocate decryption buffer
	if (dwCreationFlags & SSL_LARGE_BUFFER)
	{
		// making it the same size as the receive/send buffers just makes sense :)
		dwBufSize = FtpSettings.dwTransferBuffer;
	}
	else
	{
		// Not using DEFAULT_BUF_SIZE (2048) because supposedly the max TLS record is like 16k, so letting
		// openSSL choose the min default size to prevent multiple writes in a row during the handshake.
		dwBufSize = 0;
	}

	if (!BIO_new_bio_pair(&lpSecure->InternalBio, dwBufSize, &lpSecure->NetworkBio, dwBufSize))
	{
		Free(lpSecure);
		SetLastError(IO_SSL_FAIL);
		return TRUE;
	}

	SSL_set_bio(lpSecure->SSL, lpSecure->InternalBio, lpSecure->InternalBio);

	// Use the BIO_ctrl_pending(), to find out whether data is buffered in the BIO and must be transfered to the network.

	lpSocket->lpSecure    = lpSecure;
	return FALSE;
}



LONG GetSslOptionBit(LPTSTR tszOption)
{
	if (!_tcsicmp(tszOption, _T("MICROSOFT_SESS_ID_BUG")))
	{
		return SSL_OP_MICROSOFT_SESS_ID_BUG;
	}
	if (!_tcsicmp(tszOption, _T("NETSCAPE_CHALLENGE_BUG")))
	{
		return SSL_OP_NETSCAPE_CHALLENGE_BUG;
	}
	if (!_tcsicmp(tszOption, _T("LEGACY_SERVER_CONNECT")))
	{
		return SSL_OP_LEGACY_SERVER_CONNECT;
	}
	if (!_tcsicmp(tszOption, _T("NETSCAPE_REUSE_CIPHER_CHANGE_BUG")))
	{
		return SSL_OP_NETSCAPE_REUSE_CIPHER_CHANGE_BUG;
	}
	if (!_tcsicmp(tszOption, _T("SSLREF2_REUSE_CERT_TYPE_BUG")))
	{
		return SSL_OP_SSLREF2_REUSE_CERT_TYPE_BUG;
	}
	if (!_tcsicmp(tszOption, _T("MICROSOFT_BIG_SSLV3_BUFFER")))
	{
		return SSL_OP_MICROSOFT_BIG_SSLV3_BUFFER;
	}
	if (!_tcsicmp(tszOption, _T("MSIE_SSLV2_RSA_PADDING")))
	{
		return SSL_OP_MSIE_SSLV2_RSA_PADDING;
	}
	if (!_tcsicmp(tszOption, _T("SSLEAY_080_CLIENT_DH_BUG")))
	{
		return SSL_OP_SSLEAY_080_CLIENT_DH_BUG;
	}
	if (!_tcsicmp(tszOption, _T("TLS_D5_BUG")))
	{
		return SSL_OP_TLS_D5_BUG;
	}
	if (!_tcsicmp(tszOption, _T("TLS_BLOCK_PADDING_BUG")))
	{
		return SSL_OP_TLS_BLOCK_PADDING_BUG;
	}
	if (!_tcsicmp(tszOption, _T("DONT_INSERT_EMPTY_FRAGMENTS")))
	{
		return SSL_OP_DONT_INSERT_EMPTY_FRAGMENTS;
	}
	if (!_tcsicmp(tszOption, _T("ALL")))
	{
		return SSL_OP_ALL;
	}
	if (!_tcsicmp(tszOption, _T("NO_QUERY_MTU")))
	{
		return SSL_OP_NO_QUERY_MTU;
	}
	if (!_tcsicmp(tszOption, _T("COOKIE_EXCHANGE")))
	{
		return SSL_OP_COOKIE_EXCHANGE;
	}
	if (!_tcsicmp(tszOption, _T("NO_TICKET")))
	{
		return SSL_OP_NO_TICKET;
	}
	if (!_tcsicmp(tszOption, _T("CISCO_ANYCONNECT")))
	{
		return SSL_OP_CISCO_ANYCONNECT;
	}
	if (!_tcsicmp(tszOption, _T("NO_SESSION_RESUMPTION_ON_RENEGOTIATION")))
	{
		return SSL_OP_NO_SESSION_RESUMPTION_ON_RENEGOTIATION;
	}
	if (!_tcsicmp(tszOption, _T("NO_COMPRESSION")))
	{
		return SSL_OP_NO_COMPRESSION;
	}
	if (!_tcsicmp(tszOption, _T("ALLOW_UNSAFE_LEGACY_RENEGOTIATION")))
	{
		return SSL_OP_ALLOW_UNSAFE_LEGACY_RENEGOTIATION;
	}
	if (!_tcsicmp(tszOption, _T("SINGLE_ECDH_USE")))
	{
		return SSL_OP_SINGLE_ECDH_USE;
	}
	if (!_tcsicmp(tszOption, _T("SINGLE_DH_USE")))
	{
		return SSL_OP_SINGLE_DH_USE;
	}
	if (!_tcsicmp(tszOption, _T("EPHEMERAL_RSA")))
	{
		return SSL_OP_EPHEMERAL_RSA;
	}
	if (!_tcsicmp(tszOption, _T("CIPHER_SERVER_PREFERENCE")))
	{
		return SSL_OP_CIPHER_SERVER_PREFERENCE;
	}
	if (!_tcsicmp(tszOption, _T("TLS_ROLLBACK_BUG")))
	{
		return SSL_OP_TLS_ROLLBACK_BUG;
	}
	if (!_tcsicmp(tszOption, _T("NO_SSLv2")))
	{
		return SSL_OP_NO_SSLv2;
	}
	if (!_tcsicmp(tszOption, _T("NO_SSLv3")))
	{
		return SSL_OP_NO_SSLv3;
	}
	if (!_tcsicmp(tszOption, _T("NO_TLSv1")))
	{
		return SSL_OP_NO_TLSv1;
	}
	if (!_tcsicmp(tszOption, _T("PKCS1_CHECK_1")))
	{
		return SSL_OP_PKCS1_CHECK_1;
	}
	if (!_tcsicmp(tszOption, _T("PKCS1_CHECK_2")))
	{
		return SSL_OP_PKCS1_CHECK_2;
	}
	if (!_tcsicmp(tszOption, _T("NETSCAPE_CA_DN_BUG")))
	{
		return SSL_OP_NETSCAPE_CA_DN_BUG;
	}
	if (!_tcsicmp(tszOption, _T("NETSCAPE_DEMO_CIPHER_CHANGE_BUG")))
	{
		return SSL_OP_NETSCAPE_DEMO_CIPHER_CHANGE_BUG;
	}
	if (!_tcsicmp(tszOption, _T("CRYPTOPRO_TLSEXT_BUG")))
	{
		return SSL_OP_CRYPTOPRO_TLSEXT_BUG;
	}
	return 0;
}



BOOL
Secure_Create_Ctx(LPTSTR tszService, LPSTR szCertificateName, const SSL_METHOD *Method, SSL_CTX **ppCtx)
{
	FILE    *File;
	char     szFileName[MAX_PATH+1];
	SSL_CTX *pCtx;
	size_t   sLen;
	DH      *pDH;
	EC_KEY  *pECDH;
	long     lOptionBits, lBit;
	LPTSTR   tszField, tszOption, tszSeparator;

	if (! szCertificateName) return TRUE;

	sLen = strlen(szCertificateName);
	if (sLen+6 > sizeof(szFileName))
	{
		SetLastError(ERROR_FILENAME_EXCED_RANGE);
		return TRUE;
	}
	_snprintf_s(szFileName, sizeof(szFileName)/sizeof(*szFileName), _TRUNCATE, "%s.pem", szCertificateName);

	pCtx = SSL_CTX_new(Method);
	if (!pCtx) return TRUE;

	if (!SSL_CTX_use_certificate_chain_file(pCtx, szFileName))
	{
		SSL_CTX_free(pCtx);
		return TRUE;
	}

	strcpy_s(&szFileName[sLen], sizeof(szFileName)/sizeof(*szFileName)-sLen, ".key");

	// SSL_CTX_set_default_passwd_cb(ctx, cb);

	if(!SSL_CTX_use_PrivateKey_file(pCtx, szFileName, SSL_FILETYPE_PEM))
	{
		SSL_CTX_free(pCtx);
		return TRUE;
	}

	if (!SSL_CTX_check_private_key(pCtx))
	{
		SSL_CTX_free(pCtx);
		return TRUE;
	}

	if (tszField = Config_Get(&IniConfigFile, tszService, _TEXT("OpenSSL_Options"), NULL, NULL))
	{
		lOptionBits = 0;
		tszOption = tszField;
		while (*tszOption)
		{
			if (tszSeparator = _tcschr(tszOption, _T('|')))
			{
				*tszSeparator = 0;
			}
			lBit = GetSslOptionBit(tszOption);
			if (!lBit)
			{
				Putlog(LOG_ERROR, _T("Unknown option (%s) in OpenSSL_Options for service '%s'.\r\n"), tszOption, tszService);
			}
			lOptionBits |= lBit;
			if (tszSeparator)
			{
				tszOption = tszSeparator+1;
			}
			else
			{
				break;
			}
		}
		Free(tszField);

		SSL_CTX_set_options(pCtx, lOptionBits);
	}

	tszField = tszOption = Config_Get(&IniConfigFile, tszService, _TEXT("OpenSSL_Ciphers"), NULL, NULL);
	if (!tszOption)
	{
		tszOption = _T("DEFAULT:!LOW:!EXPORT");
	}
	if (!SSL_CTX_set_cipher_list(pCtx, tszOption))
	{
		Putlog(LOG_ERROR, _T("No ciphers selected via OpenSSL_Ciphers for service '%s'.\r\n"), tszOption, tszService);
	}
	if (tszField)
	{
		Free(tszField);
	}

	strcpy(&szFileName[sLen], ".dhp");
	if (!fopen_s(&File, szFileName, _T("rN")))
	{
		pDH = PEM_read_DHparams(File, NULL, NULL, NULL);
		if (pDH)
		{
			if (!SSL_CTX_set_tmp_dh(pCtx, pDH))
			{
				// it failed...
				Putlog(LOG_ERROR, _T("Unable to set DH params for cert '%s'."), szCertificateName);
			}
			DH_free(pDH);
		}
		else
		{
			Putlog(LOG_ERROR, _T("Unable to read DH params for cert '%s'."), szCertificateName);
		}
		fclose(File);
	}


	/*
	* Elliptic-Curve Diffie-Hellman parameters are either "named curves"
	* from RFC 4492 section 5.1.1, or explicitly described curves over
	* binary fields. OpenSSL only supports the "named curves", which provide
	* maximum interoperability. The recommended curve for 128-bit work-factor
	* key exchange is "prime256v1" a.k.a. "secp256r1" from Section 2.7 of
	* http://www.secg.org/download/aid-386/sec2_final.pdf
	*/
	if (!(pECDH = EC_KEY_new_by_curve_name(NID_secp256k1)) || !SSL_CTX_set_tmp_ecdh(pCtx, pECDH))
	{
		// it failed...
		Putlog(LOG_ERROR, _T("Unable to set ECDH params for cert '%s'."), szCertificateName);
	}
	if (pECDH)
	{
		EC_KEY_free(pECDH);
	}

	*ppCtx = pCtx;
	return FALSE;
}


VOID Secure_Free_Ctx(SSL_CTX *pCtx)
{
	if (!pCtx) return;

	SSL_CTX_free(pCtx);
}


BOOL
Secure_Delete_Cert(LPTSTR tszCertificateName)
{
	TCHAR  File[MAX_PATH+1];
	size_t sLen;
	DWORD  dwError, dwReturn;
	BOOL   bReturn, bDeleted;

	if (! tszCertificateName) return FALSE;

	bReturn  = TRUE;
	bDeleted = FALSE;
	dwReturn = NO_ERROR;

	sLen = _tcslen(tszCertificateName);
	if (sLen+6 > sizeof(File)/sizeof(*File))
	{
		SetLastError(ERROR_FILENAME_EXCED_RANGE);
		return FALSE;
	}
	_sntprintf_s(File, sizeof(File)/sizeof(*File), _TRUNCATE, _T("%s.pem"), tszCertificateName);

	dwError = NO_ERROR;

	if (!DeleteFile(File))
	{
		dwError = GetLastError();
		if (dwError != ERROR_FILE_NOT_FOUND)
		{
			bReturn = FALSE;
			dwReturn = dwError;
		}
	}
	else
	{
		bDeleted = TRUE;
	}

	_tcscpy_s(&File[sLen], sizeof(File)/sizeof(*File)-sLen, ".key");
	if (!DeleteFile(File))
	{
		dwError = GetLastError();
		if (dwError != ERROR_FILE_NOT_FOUND)
		{
			bReturn = FALSE;
			if (!dwReturn) dwReturn = dwError;
		}
	}
	else
	{
		bDeleted = TRUE;
	}

	_tcscpy_s(&File[sLen], sizeof(File)/sizeof(*File)-sLen, ".dhp");
	if (!DeleteFile(File))
	{
		dwError = GetLastError();
		if (dwError != ERROR_FILE_NOT_FOUND)
		{
			bReturn = FALSE;
			if (!dwReturn) dwReturn = dwError;
		}
	}
	else
	{
		bDeleted = TRUE;
	}

	if (!bReturn)
	{
		SetLastError(dwReturn);
		return FALSE;
	}

	if (!bDeleted)
	{
		SetLastError(ERROR_FILE_MISSING);
		return FALSE;
	}
	return TRUE;
}


LPTSTR Admin_Ciphers(LPFTPUSER lpUser, LPTSTR tszMultilinePrefix, LPIO_STRING Args)
{
	LPTSTR			tszCommand, tszArg;
	LPBUFFER        lpBuffer;
	LPIOSERVICE     lpService;
	SSL_CTX        *pCtx;
	SSL            *tempSSL;
	int             iCiphers, i;
	STACK_OF(SSL_CIPHER) *CipherStack;
	SSL_CIPHER      *Cipher;
	CHAR            szBuf[129];

	// [name of service], or -all

	tszCommand  = GetStringIndexStatic(Args, 0);
	lpBuffer    = &lpUser->CommandChannel.Out;

	if (GetStringItems(Args) > 2) ERROR_RETURN(ERROR_INVALID_ARGUMENTS, tszCommand);

	tempSSL = NULL;
	if (GetStringItems(Args) == 2)
	{
		tszArg = GetStringIndexStatic(Args, 1);
		if (tszArg && !_tcsicmp(tszArg, _T("-all")))
		{
			pCtx = SSL_CTX_new(SSLv23_method());
			if (!pCtx)
			{
				ERROR_RETURN(IO_SSL_FAIL2, tszCommand);
			}
			tempSSL = SSL_new(pCtx);
			if (!tempSSL)
			{
				SSL_CTX_free(pCtx);
				ERROR_RETURN(IO_SSL_FAIL2, tszCommand);
			}
			if (!SSL_set_cipher_list(tempSSL, "ALL"))
			{
				SSL_free(tempSSL);
				SSL_CTX_free(pCtx);
				ERROR_RETURN(IO_SSL_FAIL2, tszCommand);
			}
		}
		else
		{
			ERROR_RETURN(ERROR_INVALID_ARGUMENTS, tszCommand);
		}
	}
	else
	{
		pCtx = NULL;
		lpService   = lpUser->Connection.lpService;

		AcquireSharedLock(&lpService->loLock);
		if (!lpService->pSecureCtx)
		{
			ReleaseSharedLock(&lpService->loLock);
			FormatString(lpBuffer, _T("%sNo certificate loaded for service '%s'.\r\n"), tszMultilinePrefix, lpService->tszName);
			ERROR_RETURN(ERROR_COMMAND_FAILED, tszCommand);
		}
		tempSSL = SSL_new(lpService->pSecureCtx);
		ReleaseSharedLock(&lpService->loLock);
		if (!tempSSL)
		{
			ERROR_RETURN(IO_SSL_FAIL2, tszCommand);
		}
	}

	if (!(CipherStack = SSL_get_ciphers(tempSSL)))
	{
		SSL_free(tempSSL);
		if (pCtx) SSL_CTX_free(pCtx);
		ERROR_RETURN(IO_SSL_FAIL2, tszCommand);
	}

	iCiphers = sk_SSL_CIPHER_num(CipherStack);
	for( i = 0 ; (i < iCiphers) && (Cipher = sk_SSL_CIPHER_value(CipherStack, i)) ; i++ )
	{
		// Could access the CIPHER structure directoy, and replicate the logic in SSL_CIPHER_description()
		// to get properly formatted output, or just accept it's not aligned correctly, but it's likely to
		// get updated with new algorithms which we might not catch in the future, or perhaps the structure
		// changes...
		SSL_CIPHER_description(Cipher, szBuf, sizeof(szBuf));
		//cszName   = SSL_CIPHER_get_name(Cipher);
		//iBits     = SSL_CIPHER_get_bits(Cipher, NULL);
		//szVersion = SSL_CIPHER_get_version(Cipher);
		//FormatString(lpBuffer, _T("%s#%2d: %s (%d bits) [%s]\r\n"), tszMultilinePrefix, i+1, cszName, iBits, szVersion);
		FormatString(lpBuffer, _T("%s#%2d: %s\r\n"), tszMultilinePrefix, i+1, szBuf);
	}

	SSL_free(tempSSL);
	if (pCtx) SSL_CTX_free(pCtx);
	return NULL;
}


// New OpenSSL dynamic lock create function
static struct CRYPTO_dynlock_value *
Secure_Dyn_Create_Function(const char *cszSourceFile, int iLine)
{
    struct CRYPTO_dynlock_value *pLock;

	pLock = Allocate("OpenSSL:Dynlock", sizeof(*pLock));
	if (!pLock)
    {
		SetLastError(ERROR_NOT_ENOUGH_MEMORY);
		return NULL;
	}
	pLock->hMutex = CreateMutex(NULL, FALSE, NULL);
	if (pLock->hMutex == INVALID_HANDLE_VALUE)
	{
		Free(pLock);
		return NULL;
	}

	return pLock;
}

// New OpenSSL dynamic lock lock/unlock function
static void Secure_Dyn_Lock_Function(int iMode, struct CRYPTO_dynlock_value *pLock, const char *cszSourceFile, int iLine)
{
    if (iMode & CRYPTO_LOCK)
    {
		if (WaitForSingleObject(pLock->hMutex,INFINITE) != WAIT_OBJECT_0)
		{
			Putlog(LOG_DEBUG, "OpenSSL dynamic lock wait error: %d\r\n", GetLastError());
		}
	}
	else
	{
		if (! ReleaseMutex(pLock->hMutex))
		{
			Putlog(LOG_DEBUG, "OpenSSL dynamic lock release error: %d\r\n", GetLastError());
		}
	}
}


// New OpenSSL dynamic lock destroy function
static void Secure_Dyn_Destroy_Function(struct CRYPTO_dynlock_value *pLock, const char *cszSourceFile, int iLine)
{
	CloseHandle(pLock->hMutex);
	Free(pLock);
}


void Secure_Locking_Callback(int iMode, int iType, const char *cszSourceFile, int iLine)
{
	if (iMode & CRYPTO_LOCK)
	{
		if (WaitForSingleObject(SecureLockArray[iType],INFINITE) != WAIT_OBJECT_0)
		{
			Putlog(LOG_DEBUG, "OpenSSL secure lock wait error: %d\r\n", GetLastError());
		}
	}
	else
	{
		if (! ReleaseMutex(SecureLockArray[iType]))
		{
			Putlog(LOG_DEBUG, "OpenSSL secure lock release error: %d\r\n", GetLastError());
		}
	}
}


BOOL
Security_Init(BOOL bFirstInitialization)
{
	INT i;

	if (! bFirstInitialization) return TRUE;

	// This has to come first so all memory allocation done are using our free/malloc routines so memory
	// is in our heap...
	CRYPTO_malloc_init();
	
	iSecureLockArray = CRYPTO_num_locks();

	SecureLockArray = Allocate("SecureLockArray", iSecureLockArray * sizeof(*SecureLockArray));
	if (!SecureLockArray)
	{
		return FALSE;
	}

	for (i = 0 ; i < iSecureLockArray ; i++ )
	{
		if ((SecureLockArray[i] = CreateMutex(NULL,FALSE,NULL)) == INVALID_HANDLE_VALUE)
		{
			return FALSE;
		}
	}

	CRYPTO_set_locking_callback(Secure_Locking_Callback);

	CRYPTO_set_locking_callback(Secure_Locking_Callback);
	CRYPTO_set_dynlock_create_callback(Secure_Dyn_Create_Function);
	CRYPTO_set_dynlock_lock_callback(Secure_Dyn_Lock_Function);
	CRYPTO_set_dynlock_destroy_callback(Secure_Dyn_Destroy_Function);

	SSL_load_error_strings();
	ERR_load_BIO_strings();
	SSL_library_init();
	OPENSSL_add_all_algorithms_noconf();

	return TRUE;
}


VOID Security_DeInit(VOID)
{
	INT i;

	// try to get rid of as much OpenSSL memory as possible to detect leaks better...
	ERR_remove_state(0);
	ENGINE_cleanup();
	CONF_modules_unload(1);
	ERR_free_strings();
	EVP_cleanup();
	CRYPTO_cleanup_all_ex_data();

	CRYPTO_set_locking_callback(NULL);
	for ( i = 0; i < iSecureLockArray ; i++ )
	{
		CloseHandle(SecureLockArray[i]);
	}
	Free(SecureLockArray);
}
