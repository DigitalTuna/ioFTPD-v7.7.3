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
#include <tcl.h>

//  Local declarations
static INT Tcl_Client(LPTCL_INTERPRETER lpTclInterpreter,
					  Tcl_Interp *Interp,
					  UINT Objc,
					  Tcl_Obj *CONST Objv[]);
static INT Tcl_Buffer(LPTCL_INTERPRETER lpTclInterpreter,
					  Tcl_Interp *Interp,
					  UINT Objc,
					  Tcl_Obj *CONST Objv[]);
static INT Tcl_Change(LPTCL_INTERPRETER lpTclInterpreter,
					  Tcl_Interp *Interp,
					  UINT Objc,
					  Tcl_Obj *CONST Objv[]);
static INT Tcl_Configuration(LPTCL_INTERPRETER lpTclInterpreter,
					  Tcl_Interp *Interp,
					  UINT Objc,
					  Tcl_Obj *CONST Objv[]);
static INT Tcl_Crc32(LPTCL_INTERPRETER lpTclInterpreter,
					 Tcl_Interp *Interp,
					 UINT Objc,
					 Tcl_Obj *CONST Objv[]);
static INT Tcl_Group(LPTCL_INTERPRETER lpTclInterpreter,
					 Tcl_Interp *Interp,
					 UINT Objc,
					 Tcl_Obj *CONST Objv[]);
static INT Tcl_GroupFile(LPTCL_INTERPRETER lpTclInterpreter,
						 Tcl_Interp *Interp,
						 UINT Objc,
						 Tcl_Obj *CONST Objv[]);
static INT Tcl_ioDisk(LPTCL_INTERPRETER lpTclInterpreter,
					  Tcl_Interp *Interp,
					  UINT Objc,
					  Tcl_Obj *CONST Objv[]);
static INT Tcl_ioHandleLock(LPTCL_INTERPRETER lpTclInterpreter,
							Tcl_Interp *Interp,
							UINT Objc,
							Tcl_Obj *CONST Objv[]);
static INT Tcl_ioMsg(LPTCL_INTERPRETER lpTclInterpreter,
					 Tcl_Interp *Interp,
					 UINT Objc,
					 Tcl_Obj *CONST Objv[]);
static INT Tcl_ioServer(LPTCL_INTERPRETER lpTclInterpreter,
						Tcl_Interp *Interp,
						UINT Objc,
						Tcl_Obj *CONST Objv[]);
static INT Tcl_ioTheme(LPTCL_INTERPRETER lpTclInterpreter,
					   Tcl_Interp *Interp,
					   UINT Objc,
					   Tcl_Obj *CONST Objv[]);
static INT Tcl_ioTransferStats(LPTCL_INTERPRETER lpTclInterpreter,
							   Tcl_Interp *Interp,
							   UINT Objc,
							   Tcl_Obj *CONST Objv[]);
static INT Tcl_ioUptime(LPTCL_INTERPRETER lpTclInterpreter,
					    Tcl_Interp *Interp,
					    UINT Objc,
					    Tcl_Obj *CONST Objv[]);
static INT Tcl_ioVirtual(LPTCL_INTERPRETER lpTclInterpreter,
						 Tcl_Interp *Interp,
						 UINT Objc,
						 Tcl_Obj *CONST Objv[]);
static INT Tcl_iPuts(LPTCL_INTERPRETER lpTclInterpreter,
					 Tcl_Interp *Interp,
					 UINT Objc,
					 Tcl_Obj *CONST Objv[]);
static INT Tcl_MountFile(LPTCL_INTERPRETER lpTclInterpreter,
						 Tcl_Interp *Interp,
						 UINT Objc,
						 Tcl_Obj *CONST Objv[]);
static INT Tcl_MountPoints(LPTCL_INTERPRETER lpTclInterpreter,
						   Tcl_Interp *Interp,
						   UINT Objc,
						   Tcl_Obj *CONST Objv[]);
static INT Tcl_Putlog(LPTCL_INTERPRETER lpTclInterpreter,
					  Tcl_Interp *Interp,
					  UINT Objc,
					  Tcl_Obj *CONST Objv[]);
static INT Tcl_Resolve(LPTCL_INTERPRETER lpTclInterpreter,
					   Tcl_Interp *Interp,
					   UINT Objc,
					   Tcl_Obj *CONST Objv[]);
static INT Tcl_Sections(LPTCL_INTERPRETER lpTclInterpreter,
					    Tcl_Interp *Interp,
					    UINT Objc,
					    Tcl_Obj *CONST Objv[]);
static INT Tcl_Sha1(LPTCL_INTERPRETER lpTclInterpreter,
					Tcl_Interp *Interp,
					UINT Objc,
					Tcl_Obj *CONST Objv[]);
static INT Tcl_Timer(LPTCL_INTERPRETER lpTclInterpreter,
					 Tcl_Interp *Interp,
					 UINT Objc,
					 Tcl_Obj *CONST Objv[]);
static INT Tcl_User(LPTCL_INTERPRETER lpTclInterpreter,
					Tcl_Interp *Interp,
					UINT Objc,
					Tcl_Obj *CONST Objv[]);
static INT Tcl_UserFile(LPTCL_INTERPRETER lpTclInterpreter,
						Tcl_Interp *Interp,
						UINT Objc,
						Tcl_Obj *CONST Objv[]);
static INT Tcl_Variable(LPTCL_INTERPRETER lpTclInterpreter,
						Tcl_Interp *Interp,
						UINT Objc,
						Tcl_Obj *CONST Objv[]);

static INT Tcl_Vfs(LPTCL_INTERPRETER lpTclInterpreter,
				   Tcl_Interp *Interp,
				   UINT Objc,
				   Tcl_Obj *CONST Objv[]);
static INT Tcl_WaitObject(LPTCL_INTERPRETER lpTclInterpreter,
						  Tcl_Interp *Interp,
						  UINT Objc,
						  Tcl_Obj *CONST Objv[]);


//  Local variables

static TCL_COMMAND      TCL_Command[] =
{
  "buffer",       Tcl_Buffer,
  "change",       Tcl_Change,
  "client",       Tcl_Client,
  "config",       Tcl_Configuration,
  "crc32",        Tcl_Crc32,
  "group",        Tcl_Group,
  "groupfile",    Tcl_GroupFile,
  "ioDisk",       Tcl_ioDisk,
  "ioHandleLock", Tcl_ioHandleLock,
  "ioMsg",        Tcl_ioMsg,
  "ioServer",     Tcl_ioServer,
  "ioTheme",      Tcl_ioTheme,
  "ioTransferStats", Tcl_ioTransferStats,
  "ioUptime",     Tcl_ioUptime,
  "ioVirtual",    Tcl_ioVirtual,
  "iputs",        Tcl_iPuts,
  "mountfile",    Tcl_MountFile,
  "mountpoints",  Tcl_MountPoints,
  "putlog",       Tcl_Putlog,
  "resolve",      Tcl_Resolve,
  "sections",     Tcl_Sections,
  "sha1",         Tcl_Sha1,
  "timer",        Tcl_Timer,
  "user",         Tcl_User,
  "userfile",     Tcl_UserFile,
  "var",          Tcl_Variable,
  "vfs",          Tcl_Vfs,
  "waitobject",   Tcl_WaitObject,
  NULL,           NULL
};


static CRITICAL_SECTION    csGlobalVariables;
static LPTCL_VARIABLE    *lpGlobalVariables;
static DWORD        dwGlobalVariables, dwGlobalVariablesSize;
static Tcl_ObjType      *lpListType;
static CRITICAL_SECTION    csTclWaitObjectList;
static LPTCL_WAITOBJECT    lpTclWaitObjectList;
static DWORD        dwTclUniqueId;
static BOOL         bDebugTclInterpreters;

static LONG volatile  lTclInterps;

// checked by WorkerThread to see if it's safe to try to create interpreters.
DWORD        dwTclInterpreterTlsIndex = TLS_OUT_OF_INDEXES;


// Returns NULL if the object doesn't have a string representation or there
// is an error with an input parameter.  Length is the max size of string.
// If length==0 then dynamically allocate a string.
// The pointer returned is one of:
//   1) the string pointer passed to the function if the object held a valid
//      UTF-8 string and it was translated
//   2) A newly allocated string if length=0 and the object held a valid UTF-8
//      string and it was translated
//   3) or the actual string from TCL.
// You can tell which by examining the pbValid result, or if you are treating
// the string as read only and temporary you can ignore it.  If length == 0
// and a pointer was returned, you still have to free it.
static CHAR *
Tcl_IoGetString(Tcl_Obj *objPtr, char *string, int length, BOOL *pbValid)
{
	unsigned char *tcl, *buf, *tclStart;
	int            tclLen, newLen;
	unsigned char  byte;
	unsigned char *pTclEnd, *pBufEnd;
	BOOL           valid = TRUE;
	WCHAR          uniChar;

	if (string && !length)
	{
		if (*pbValid) *pbValid = FALSE;
		return NULL;
	}

	tclStart = tcl = (unsigned char *) Tcl_GetStringFromObj(objPtr, &tclLen);
	buf = (unsigned char *) string;

	if (!tcl)
	{
		if (buf) *buf = 0;
		if (*pbValid) *pbValid = FALSE;
		return NULL;
	}

	newLen = length;
	if (length == 0)
	{
		newLen = tclLen + 1;
		string = buf = Allocate("", newLen);
		if (!buf)
		{
			if (pbValid) *pbValid = FALSE;
			return NULL;
		}
	}

	// always save room for the NULL at the end
	pBufEnd = buf + newLen - 1;
	pTclEnd = tcl + tclLen;
	
	while ((tcl < pTclEnd) && (buf < pBufEnd))
	{
		byte = *tcl++;
		if (!byte)
		{
			// An embedded NULL should terminate our translation
			break;
		}
		else if (byte < 0x80)
		{
			// UTF-8 chars between 0x01 and 0x7F are equal to themselves.
			*buf++ = byte;
		}
		else if (byte < 0xC2)
		{
			// 0x80-0xBF are invalid lead bytes, but just copy them over anyway.
			valid = FALSE;
			*buf++ = byte;
		}
		else if (byte < 0xE0)
		{
			if (tcl != pTclEnd && ((tcl[0] & 0xC0) == 0x80))
			{
				if (byte < 0xC2)
				{
					// C0, C1 lead bytes are invalid
					valid = FALSE;
					break;
				}

				// 2 byte character with valid trail byte
				uniChar = ((byte & 0x1F) << 6) | (tcl[0] & 0x3F);
				if (uniChar > 255) uniChar = '?';
				*buf++ = (unsigned char) uniChar;
				tcl++;
			}
			else
			{
				// missing or invalid 2nd byte of a 2 byte sequence
				valid = FALSE;
				break;
			}
		}
		else if (byte < 0xF0)
		{
			if ((tcl + 1 < pTclEnd) && ((tcl[0] & 0xC0) == 0x80) && ((tcl[1] & 0xC0) == 0x80))
			{
				// 3 byte character with 2 valid trailing bytes...
				// but no way to map this into an 8 bit char :(
				// uniChar = (((byte & 0x0F) << 12) | ((tcl[0] & 0x3F) << 6) | (tcl[1] & 0x3F));
				*buf++ = '?';
				tcl += 2;
			}
			else
			{
				// missing or invalid 2nd or 3rd byte of a 3 byte sequence, just copy them
				valid = FALSE;
				break;
			}
		}
		else if (byte < 0xF5)
		{
			// NOTE: 0xF5-0xF7 are technically valid 4 byte sequences, but no unicode characters exist there
			if ((tcl + 2 < pTclEnd) && ((tcl[0] & 0xC0) == 0x80) && ((tcl[1] & 0xC0) == 0x80) && ((tcl[2] & 0xC0) == 0x80))
			{
				// 4 byte character with 3 valid trailing bytes...
				// but no way to map this into an 8 bit char!
				*buf++ = '?';
				tcl += 3;
			}
			else
			{
				// missing or invalid 2nd, 3rd, or 4th byte of a 4 byte sequence
				valid = FALSE;
				break;
			}
		}
		else
		{
			// it's an invalid char
			valid = FALSE;
			break;
		}
	}
	
	*buf = 0;
	if ((tcl < pTclEnd) && (buf == pBufEnd)) valid = FALSE;
	if (pbValid && !valid) *pbValid = FALSE;
	if (valid)
	{
		return string;
	}
	Putlog(LOG_SYSTEM, _T("Error converting string: %s\r\n"), tclStart);
	if (length == 0)
	{
		CopyMemory(string, tclStart, tclLen+1);
		return string;
	}
	return tclStart;
}


static BOOL
Tcl_IoConvertString(char *in, int iInSize, char *out, int iOutSize)
{
	unsigned char c;
	char *end;

	if (iOutSize <= 0)
	{
		SetLastError(ERROR_INSUFFICIENT_BUFFER);
		return TRUE;
	}
	end = out + iOutSize - 1;
	if (iInSize < 0) iInSize = 10*1024;
	for ( ; out < end && iInSize > 0 ; iInSize--, in++)
	{
		c = (unsigned char) *in;
		if (!c)
		{
			*out = 0;
			return FALSE;
		}
		if (c < 0x7F)
		{
			*out++ = c;
			continue;
		}
		*out++ = (char) ((c >> 6) | 0xC0);
		*out++ = (char) ((c | 0x80) & 0xBF);
	}

	if (out == end && iInSize > 0)
	{
		// we ran out of space
		out[-1] = 0;
		SetLastError(ERROR_INSUFFICIENT_BUFFER);
		return TRUE;
	}
	*out = 0;
	return FALSE;
}


static void
Tcl_IoSetStringObj(Tcl_Obj *objPtr, char *string, int length)
{
	char  temp[4096];
	char *buf;
	int   iBufLen;

	if (length < 0)
	{
		length = strlen(string);
	}

	if (length > sizeof(temp))
	{
		iBufLen = length*2;
		buf = Allocate("", iBufLen);
		if (!buf) return;
	}
	else
	{
		buf = temp;
		iBufLen = sizeof(temp);
	}

	if (!Tcl_IoConvertString(string, length, buf, iBufLen))
	{
		// length < 0 means all bytes up to first NULL
		Tcl_SetStringObj(objPtr, buf, -1);
	}
	else
	{
		Putlog(LOG_SYSTEM, _T("Error setting TCL string\r\n"));
	}
	if (length > sizeof(temp))
	{
		Free(buf);
	}
}


static void
Tcl_IoAppendResult(Tcl_Interp *interp, char *string, int length)
{
	char  temp[4096];
	char *buf;
	int   iBufLen;

	if (!string) return;
	if (length < 0)
	{
		length = strlen(string);
	}

	if (length > sizeof(temp))
	{
		iBufLen = length*2+2;
		buf = Allocate("", iBufLen);
		if (!buf) return;
	}
	else
	{
		buf = temp;
		iBufLen = sizeof(temp);
	}

	if (!Tcl_IoConvertString(string, length, buf, iBufLen))
	{
		Tcl_AppendResult(interp, buf, NULL);
	}
	if (length > sizeof(temp))
	{
		Free(buf);
	}
}


INT  Base10ToBaseX(INT iValue, INT iBaseX)
{
  INT    iReturn, iRounded, iMultiplier;

  iReturn    = 0;
  iMultiplier  = 1;
  //  Convert 10-Base figure to X-Base figure
  do
  {
    iRounded  = iValue / iBaseX;
    iReturn    += (iValue - (iRounded * iBaseX)) * iMultiplier;
    iMultiplier  *= 10;
  
  } while ((iValue = iRounded));

  return iReturn;
}


// Must hold csTclWaitObjectList lock
VOID Tcl_CloseWaitObject(LPTCL_WAITOBJECT lpWaitObject)
{
  if (! (InterlockedDecrement(&lpWaitObject->lReferenceCount)))
  {
    //  Delete object
    switch (lpWaitObject->dwType)
    {
    case SEMAPHORE:
    case EVENT:
      CloseHandle((HANDLE)lpWaitObject->lpData);
    }

	//  Remove object from list
	if (lpWaitObject->lpNext ||
		lpWaitObject->lpPrev || lpWaitObject == lpTclWaitObjectList)
	{
		if (lpWaitObject->lpNext)
		{
			lpWaitObject->lpNext->lpPrev  = lpWaitObject->lpPrev;
		}
		if (lpWaitObject->lpPrev)
		{
			lpWaitObject->lpPrev->lpNext  = lpWaitObject->lpNext;
		}
		else lpTclWaitObjectList  = lpWaitObject->lpNext;

	}
	Free(lpWaitObject);
  }
}


INT
Tcl_WaitObject(LPTCL_INTERPRETER lpTclInterpreter,
                           Tcl_Interp *Interp,
                           UINT Objc,
                           Tcl_Obj *CONST Objv[])
{
  Tcl_Obj        *lpResult, *lpLockName, *lpLockRefs;
  LPTCL_WAITOBJECT  lpWaitObject;
  LPTCL_DATA      lpTclData;
  LPTSTR        tszObjectName;
  LPSTR        szCommand, szObjectType, szParam;
  DWORD        dwObjectName, dwSetCount, n;
  LONG        lResult, lWait, lMaxCount;
  BOOL        bManualReset, bError;
  INT          iReturn;
  char        temp2[1024];

  lResult  = -1;
  iReturn  = TCL_ERROR;
  lpTclData  = lpTclInterpreter->lpTclData;
  if (Objc < 2U ||
    ! (szCommand = Tcl_GetString(Objv[1]))) return TCL_ERROR;
  //  Create result object
  lpResult  = Tcl_NewObj();
  if (! lpResult) return TCL_ERROR;

  if (! stricmp(szCommand, "open"))
  {
    if (lpTclData->dwWaitObject < TCL_MAX_WAITOBJECTS && Objc >= 3 &&
      (tszObjectName = Tcl_IoGetString(Objv[2], temp2, sizeof(temp2), 0)))
    {
      EnterCriticalSection(&csTclWaitObjectList);
      //  Find object from list
      for (lpWaitObject = lpTclWaitObjectList;lpWaitObject;lpWaitObject = lpWaitObject->lpNext)
      {
        if (! _tcscmp(lpWaitObject->tszName, tszObjectName))
        {
          //  Increment reference count
			InterlockedIncrement(&lpWaitObject->lReferenceCount);
			lpTclData->lpWaitObject[lpTclData->dwWaitObject++]  = lpWaitObject;
			iReturn  = TCL_OK;
			lResult  = (LONG)lpWaitObject;
			break;
		}
      }

      //  Create new object
      if (! lpWaitObject && Objc > 3U &&
        (szObjectType = Tcl_GetString(Objv[3])))
      {
        dwObjectName  = _tcslen(tszObjectName);
        lpWaitObject  = (LPTCL_WAITOBJECT)Allocate("Tcl:WaitObject", sizeof(TCL_WAITOBJECT) + dwObjectName * sizeof(TCHAR));

        if (lpWaitObject)
        {
          CopyMemory(lpWaitObject->tszName, tszObjectName, (dwObjectName + 1) * sizeof(TCHAR));
          lpWaitObject->lReferenceCount  = 1;

          if (! stricmp(szObjectType, "spinlock"))
          {
            lpWaitObject->dwType  = SPINLOCK;
            lpWaitObject->lpData  = (LPVOID)FALSE;
            iReturn  = TCL_OK;
          }
          else if (! stricmp(szObjectType, "event"))
          {
            bManualReset  = FALSE;
            //  Get parameters
            if (Objc == 5U &&
              (szParam = Tcl_GetString(Objv[4])) && ! stricmp(szParam, "true"))
            {
              bManualReset  = TRUE;
            }
            //  Create event
            if (lpWaitObject->lpData = (LPVOID)CreateEvent(NULL, bManualReset, TRUE, NULL))
            {
              lpWaitObject->dwType  = EVENT;
              iReturn  = TCL_OK;
            }
          }
          else if (! stricmp(szObjectType, "semaphore"))
          {
            //  Get parameters
            if (Objc != 5U ||
              Tcl_GetLongFromObj(Interp, Objv[4], &lMaxCount) == TCL_ERROR) lMaxCount  = 1;
            //  Create semaphore
            if (lpWaitObject->lpData = (LPVOID)CreateSemaphore(NULL, lMaxCount, lMaxCount, NULL))
            {
              lpWaitObject->dwType  = SEMAPHORE;
              iReturn  = TCL_OK;
            }
          }

          //  Push object to list
          if (iReturn == TCL_OK)
          {
            if (lpWaitObject->lpNext = lpTclWaitObjectList)
            {
              lpTclWaitObjectList->lpPrev = lpWaitObject;
            }
            lpWaitObject->lpPrev  = NULL;
            lpTclWaitObjectList  = lpWaitObject;
            lpTclData->lpWaitObject[lpTclData->dwWaitObject++]  = lpWaitObject;
            lResult  = (LONG)lpWaitObject;
          }
          else Free(lpWaitObject);
        }
      }
	  LeaveCriticalSection(&csTclWaitObjectList);
    }
  }
  else if (! stricmp(szCommand, "close"))
  {
	  //  close/delete wait object
	  if (Objc == 3U &&
		  Tcl_GetLongFromObj(Interp, Objv[2], (LPLONG)&lpWaitObject) == TCL_OK)
	  {
		  iReturn  = TCL_OK;
		  EnterCriticalSection(&csTclWaitObjectList);
		  Tcl_CloseWaitObject(lpWaitObject);
		  LeaveCriticalSection(&csTclWaitObjectList);
		  // lpWaitObject the address is still valid, but the thing pointed at may no longer be

		  //  Remove wait object
		  for (n = lpTclData->dwWaitObject;n--;)
		  {
			  if (lpTclData->lpWaitObject[n] == lpWaitObject)
			  {
				  lpTclData->lpWaitObject[n]  = lpTclData->lpWaitObject[--lpTclData->dwWaitObject];
				  break;
			  }
		  }
	  }
  }
  else if (! stricmp(szCommand, "preserve"))
  {
	  if (Objc == 3U &&
		  Tcl_GetLongFromObj(Interp, Objv[2], (LPLONG)&lpWaitObject) == TCL_OK)
	  {
		  iReturn = TCL_OK;
		  lResult = InterlockedIncrement(&lpWaitObject->lReferenceCount);
	  }
  }
  else if (! stricmp(szCommand, "wait"))
  {
    dwSetCount = 1;
    //  Wait for object
    if ((Objc == 4U ||
      (Objc == 5U && Tcl_GetLongFromObj(Interp, Objv[4], (LPLONG)&dwSetCount) == TCL_OK)) &&
      Tcl_GetLongFromObj(Interp, Objv[2], (LPLONG)&lpWaitObject) == TCL_OK &&
      Tcl_GetLongFromObj(Interp, Objv[3], &lWait) == TCL_OK)
    {
		SetBlockingThreadFlag();
		switch (lpWaitObject->dwType)
		{
		case SPINLOCK:
			while (InterlockedExchange((LPLONG)&lpWaitObject->lpData, TRUE) && lWait--) SwitchToThread();
			lResult  = (lWait == -1 ? 1 : 0);
			iReturn  = TCL_OK;
			break;
		case EVENT:
			dwSetCount  = 1;
		case SEMAPHORE:
			bError  = FALSE;
			for (n = dwSetCount;! bError && n--;)
			{
				switch (WaitForSingleObject((HANDLE)lpWaitObject->lpData, lWait))
				{
				case WAIT_OBJECT_0:
					lResult  = 0;
					iReturn  = TCL_OK;
					break;
				case WAIT_TIMEOUT:
					lResult  = 1;
					iReturn  = TCL_OK;
					bError  = TRUE;
					break;
				default:
					bError  = TRUE;
				}
			}
			//  Restore semaphore count, on error
			if (bError && (n + 1) != dwSetCount)
			{
				ReleaseSemaphore((HANDLE)lpWaitObject->lpData, dwSetCount - (n + 1), NULL);
			}
		}
		SetNonBlockingThreadFlag();
    }
  }
  else if (! stricmp(szCommand, "set"))
  {
    //  Set object state to signaled
    dwSetCount  = 1;
    if ((Objc == 3U || (Objc == 4U && Tcl_GetLongFromObj(Interp, Objv[3], (LPLONG)&dwSetCount) == TCL_OK)) &&
      Tcl_GetLongFromObj(Interp, Objv[2], (LPLONG)&lpWaitObject) == TCL_OK)
    {
      switch (lpWaitObject->dwType)
      {
      case SPINLOCK:
        lResult  = InterlockedExchange((LPLONG)&lpWaitObject->lpData, FALSE);
        iReturn  = TCL_OK;
        break;
      case SEMAPHORE:
        lResult  = (ReleaseSemaphore((HANDLE)lpWaitObject->lpData, dwSetCount, NULL) ? 0 : -1);
        iReturn  = TCL_OK;
        break;
      case EVENT:
        lResult  = (SetEvent((HANDLE)lpWaitObject->lpData) ? 0 : -1);
        iReturn  = TCL_OK;
        break;
      }
    }
  }
  else if (! stricmp(szCommand, "reset"))
  {
    //  Set object state to non-signaled
    dwSetCount  = 1;
    if ((Objc == 3U || (Objc == 4U && Tcl_GetLongFromObj(Interp, Objv[3], (LPLONG)&dwSetCount) == TCL_OK)) &&
      Tcl_GetLongFromObj(Interp, Objv[2], (LPLONG)&lpWaitObject) == TCL_OK)
    {
      switch (lpWaitObject->dwType)
      {
      case SPINLOCK:
        lResult  = InterlockedExchange((LPLONG)&lpWaitObject->lpData, TRUE);
        iReturn  = TCL_OK;
        break;
      case SEMAPHORE:
        lResult  = 0;
        for (;dwSetCount--;lResult++)
        {
          if (WaitForSingleObject((HANDLE)lpWaitObject->lpData, 0) != WAIT_OBJECT_0) break;
        }
        iReturn  = TCL_OK;
        break;
      case EVENT:
        lResult  = (ResetEvent((HANDLE)lpWaitObject->lpData) ? 0 : -1);
        iReturn  = TCL_OK;
        break;
      }
    }
  }
  else if (!stricmp(szCommand, "list"))
  {
	  EnterCriticalSection(&csTclWaitObjectList);
	  for (lpWaitObject = lpTclWaitObjectList;lpWaitObject;lpWaitObject = lpWaitObject->lpNext)
	  {
		  lpLockName  = Tcl_NewStringObj(lpWaitObject->tszName, -1);
		  lpLockRefs  = Tcl_NewIntObj(lpWaitObject->lReferenceCount);
		  if (lpLockName && lpLockRefs)
		  {
			  Tcl_ListObjAppendElement(NULL, lpResult, lpLockName);
			  Tcl_ListObjAppendElement(NULL, lpResult, lpLockRefs);
		  }
	  }
	  LeaveCriticalSection(&csTclWaitObjectList);

	  Tcl_SetObjResult(Interp, lpResult);
	  return TCL_OK;
  }

  Tcl_SetLongObj(lpResult, lResult);
  Tcl_SetObjResult(Interp, lpResult);
  return iReturn;
}





INT
Tcl_Sha1(LPTCL_INTERPRETER lpTclInterpreter,
                 Tcl_Interp *Interp,
                 UINT Objc,
                 Tcl_Obj *CONST Objv[])
{
  LPSTR  tszString;
  CHAR  pBuffer[128];
  BYTE  pHash[20];
  DWORD  i;
  Tcl_Obj  *lpResult;

  if (Objc != 2U ||
    ! (tszString = Tcl_IoGetString(Objv[1], 0, 0, 0))) return TCL_ERROR;
  //  Create result object
  lpResult  = Tcl_NewObj();
  if (! lpResult)
  {
	  Free(tszString);
	  return TCL_ERROR;
  }

  //  Hash string
  sha1(pHash, tszString, _tcslen(tszString));
  for (i = 0;i < 20;i++) sprintf(&pBuffer[i << 1], "%02x", pHash[i]);

  //  Set result
  Tcl_SetStringObj(lpResult, pBuffer, 40);
  Tcl_SetObjResult(Interp, lpResult);
  Free(tszString);

  return TCL_OK;
}


INT
Tcl_Crc32(LPTCL_INTERPRETER lpTclInterpreter,
                 Tcl_Interp *Interp,
                 UINT Objc,
                 Tcl_Obj *CONST Objv[])
{
  LPSTR  tszString;
  DWORD  dwCrc, dwFileSize, dwFileSizeHigh;
  Tcl_Obj  *lpResult;
  UINT64 u64StartPos, u64Len, u64FileSize;
  HANDLE hFile;
  CHAR   temp1[MAX_PATH+1];

  if (Objc < 2U || !(tszString = Tcl_IoGetString(Objv[1], temp1, sizeof(temp1), 0)))
  {
	  return TCL_ERROR;
  }

  hFile = CreateFile(tszString, GENERIC_READ, FILE_SHARE_READ|FILE_SHARE_DELETE,
	  0, OPEN_EXISTING, 0, 0);
  if (hFile == INVALID_HANDLE_VALUE)
  {
	  return TCL_ERROR;
  }
  if ((dwFileSize = GetFileSize(hFile, &dwFileSizeHigh)) == INVALID_FILE_SIZE &&
	  GetLastError() != NO_ERROR)
  {
	  CloseHandle(hFile);
	  return TCL_ERROR;
  }
  u64FileSize = dwFileSizeHigh * 0x100000000 + dwFileSize;

  u64StartPos = 0;
  if (Objc >= 3U)
  {
	  if (Tcl_GetWideIntFromObj(Interp, Objv[2], &u64StartPos) == TCL_ERROR)
	  {
		  CloseHandle(hFile);
		  return TCL_ERROR;
	  }
  }

  u64Len = 0;
  if (Objc >= 4U)
  {
	  if (Tcl_GetWideIntFromObj(Interp, Objv[3], &u64Len) == TCL_ERROR)
	  {
		  CloseHandle(hFile);
		  return TCL_ERROR;
	  }
  }
  if (!u64Len)
  {
	  u64Len = u64FileSize;
  }
  if ((u64StartPos + u64Len) > u64FileSize)
  {
	  CloseHandle(hFile);
	  return TCL_ERROR;
  }

  dwCrc = 0xFFFFFFFF;
  if (Objc >= 5U)
  {
	  if (Tcl_GetLongFromObj(Interp, Objv[4], &dwCrc) == TCL_ERROR)
	  {
		  CloseHandle(hFile);
		  return TCL_ERROR;
	  }
	  dwCrc = ~dwCrc;
  }

  if (!FileCrc32(hFile, u64StartPos, u64Len, &dwCrc))
  {
	  CloseHandle(hFile);
	  return TCL_ERROR;
  }

  CloseHandle(hFile);

  //  Create result object
  lpResult  = Tcl_NewObj();
  if (! lpResult) return TCL_ERROR;

  //  Set result
  Tcl_SetLongObj(lpResult, ~dwCrc);
  Tcl_SetObjResult(Interp, lpResult);

  return TCL_OK;
}





BOOL
Tcl_RunJob(LPEVENT_COMMAND lpEvent)
{
	BOOL bResult;

	bResult = RunEvent(lpEvent);
	Free(lpEvent->tszCommand);
	Free(lpEvent);
	return bResult;
}


DWORD Tcl_JobTimerProc(LPEVENT_COMMAND lpEvent, LPTIMER lpTimer)
{
	//  Execute event
	RunEvent(lpEvent);
	Free(lpEvent->tszCommand);
	return INFINITE;
}


INT
Tcl_Timer(LPTCL_INTERPRETER lpTclInterpreter,
		  Tcl_Interp *Interp,
		  UINT Objc,
		  Tcl_Obj *CONST Objv[])
{
	LPEVENT_COMMAND	 lpEvent;
	LPTIMER   lpTimer;
	Tcl_Obj  *lpResult;
	DWORD     dwTimeOut, dwLen;
	LPTSTR    tszCommand;

	Tcl_ResetResult(Interp);
	lpResult = Tcl_NewObj();
	if (! lpResult)
	{
		return TCL_ERROR;
	}

	//  Check arguments
	if (Objc < 3)
	{
		Tcl_SetStringObj(lpResult, "Insufficient arguments", -1);
		Tcl_SetObjResult(Interp, lpResult);
		return TCL_ERROR;
	}

	if (Objc > 3)
	{
		Tcl_SetStringObj(lpResult, "Too many arguments", -1);
		Tcl_SetObjResult(Interp, lpResult);
		return TCL_ERROR;
	}

	if (Tcl_GetIntFromObj(Interp, Objv[1], (PINT)&dwTimeOut) == TCL_ERROR)
	{
		Tcl_SetStringObj(lpResult, "Invalid timeout", -1);
		Tcl_SetObjResult(Interp, lpResult);
		return TCL_ERROR;
	}

	if ( !(tszCommand = Tcl_IoGetString(Objv[2], NULL, 0, NULL)) )
	{
		Tcl_SetStringObj(lpResult, "Invalid/missing command", -1);
		Tcl_SetObjResult(Interp, lpResult);
		return TCL_ERROR;
	}
	dwLen = _tcslen(tszCommand);

	lpEvent = (LPEVENT_COMMAND) Allocate("lpEventCommand", sizeof(*lpEvent));
	if (!lpEvent)
	{
		Free(tszCommand);
		return TCL_ERROR;
	}

	ZeroMemory(lpEvent, sizeof(*lpEvent));
	lpEvent->tszCommand = tszCommand;

	if (dwTimeOut == 0)
	{
		if (QueueJob(Tcl_RunJob, lpEvent, JOB_PRIORITY_LOW))
		{
			Free(tszCommand);
			Free(lpEvent);
			Tcl_SetStringObj(lpResult, "Failed to queue job", -1);
			Tcl_SetObjResult(Interp, lpResult);
			return TCL_ERROR;
		}
	}
	else
	{
		//  Launch timer
		if (! (lpTimer = StartIoTimer(NULL, Tcl_JobTimerProc, lpEvent, dwTimeOut)))
		{
			Free(tszCommand);
			Free(lpEvent);
			Tcl_SetStringObj(lpResult, "Failed to create timer", -1);
			Tcl_SetObjResult(Interp, lpResult);
			return TCL_ERROR;
		}
	}

	Tcl_SetIntObj(lpResult, 0);
	Tcl_SetObjResult(Interp, lpResult);
	return TCL_OK;
}





INT
Tcl_Client(LPTCL_INTERPRETER lpTclInterpreter,
                   Tcl_Interp *Interp,
                   UINT Objc,
                   Tcl_Obj *CONST Objv[])
{
  Tcl_Obj      *ObjectList, *Object;
  ONLINEDATA    OnlineData;
  LPTCL_DATA    lpTclData;
  LPTSTR      tszPath;
  LPSTR      szCommand, szIp, szArgument;
  TCHAR      *tpOffset;
  CHAR      pBuffer[32], temp3[_MAX_PWD+1];
  DOUBLE      dSpeed;
  INT        UserID, iResult, iReturn;
  UINT       i, n, iOffset;
  struct in_addr  inaddr;

  iReturn  = TCL_ERROR;
  iResult  = -1;
  lpTclData  = lpTclInterpreter->lpTclData;
  Tcl_ResetResult(Interp);

  if (Objc > 2U &&
    (szCommand = Tcl_GetString(Objv[1])) &&
    (szArgument = Tcl_GetString(Objv[2])))
  {
    if (! stricmp(szCommand, "KILL"))
    {
      if (Objc != 4U) return TCL_ERROR;

      if (! stricmp(szArgument, "VIRTUALPATH"))
      {
        //  Kill client by virtual path
        if ((tszPath = Tcl_IoGetString(Objv[3], temp3, sizeof(temp3), 0)))
        {
          iReturn  = TCL_OK;
          iResult  = ReleasePath(NULL, tszPath);
        }
      }
      else if (! stricmp(szArgument, "REALPATH"))
      {
        //  Kill client by real path
        if ((tszPath = Tcl_IoGetString(Objv[3], temp3, sizeof(temp3), 0)))
        {
          iReturn  = TCL_OK;
          iResult  = ReleasePath(tszPath, 0);
        }
      }
      else if (! stricmp(szArgument, "UID"))
      {
        //  Kill client by user id
        if (Tcl_GetIntFromObj(Interp, Objv[3], &UserID) != TCL_ERROR)
        {
          iReturn  = TCL_OK;
          iResult  = KickUser(UserID);
        }
      }
      else if (! stricmp(szArgument, "CLIENTID"))
      {
        //  Kill client by user id
        if (Tcl_GetIntFromObj(Interp, Objv[3], &UserID) != TCL_ERROR &&
          UserID >= 0 && UserID < MAX_CLIENTS)
        {
          iReturn  = TCL_OK;
          iResult  = KillUser(UserID);
        }
      }
    }
    else if (! stricmp(szCommand, "WHO"))
    {
      if (! stricmp(szArgument, "FETCH"))
      {
        iReturn  = TCL_OK;
        ObjectList  = Tcl_NewObj();
        if (! ObjectList) return TCL_ERROR;

        if (Objc == 4U)
        {
          if (Tcl_GetIntFromObj(Interp, Objv[3], &iOffset) == TCL_ERROR) return TCL_ERROR;
          if (! GetOnlineData(&OnlineData, iOffset)) return TCL_OK;
        }
        else
        {
          lpTclData->WhoData.iOffset = SeekOnlineData(&OnlineData, lpTclData->WhoData.iOffset);
          if (lpTclData->WhoData.iOffset == -1) return TCL_OK;
          iOffset  = lpTclData->WhoData.iOffset - 1;
        }

        //  Create objects for parameters
        for (i = 0U;i < lpTclData->WhoData.dwCount;i++)
        {
          Object = Tcl_NewObj();
          if (! Object)
          {
            iReturn  = TCL_ERROR;
            break;
          }

          switch (lpTclData->WhoData.dwType[i])
          {
          case TCL_REALPATH:
            if (OnlineData.tszRealPath)
            {
				strncpy_s(temp3, sizeof(temp3), OnlineData.tszRealPath, _TRUNCATE);
				tpOffset = temp3;
				while ((tpOffset = _tcschr(tpOffset, _TEXT('\\')))) (tpOffset++)[0]  = _TEXT('/');
				Tcl_IoSetStringObj(Object, temp3, OnlineData.dwRealPath);
            }
            else Tcl_SetStringObj(Object, "", 0);
            break;
          case TCL_REALDATAPATH:
            if ((tpOffset = OnlineData.tszRealDataPath))
            {
				strncpy_s(temp3, sizeof(temp3), OnlineData.tszRealDataPath, _TRUNCATE);
				tpOffset = temp3;
				while ((tpOffset = _tcschr(tpOffset, _TEXT('\\')))) (tpOffset++)[0]  = _TEXT('/');
				Tcl_IoSetStringObj(Object, temp3, OnlineData.dwRealDataPath);
			}
            else Tcl_SetStringObj(Object, "", 0);
            break;
          case TCL_UID:
            Tcl_SetIntObj(Object, OnlineData.Uid);
            break;
          case TCL_VIRTUALPATH:
            Tcl_IoSetStringObj(Object, OnlineData.tszVirtualPath, -1);
            break;
          case TCL_VIRTUALDATAPATH:
            Tcl_IoSetStringObj(Object, OnlineData.tszVirtualDataPath, -1);
            break;
          case TCL_TRANSFEROFFSET:
            Tcl_SetWideIntObj(Object, OnlineData.i64TotalBytesTransfered);
            break;
          case TCL_SPEED:
            dSpeed  = (OnlineData.dwIntervalLength && OnlineData.bTransferStatus ?
              OnlineData.dwBytesTransfered / OnlineData.dwIntervalLength : 0.);
            Tcl_SetDoubleObj(Object, dSpeed);
            break;
          case TCL_CLIENTID:
			  Tcl_SetIntObj(Object, iOffset);
            break;
          case TCL_STATUS:
            Tcl_SetIntObj(Object, OnlineData.bTransferStatus);
            break;
          case TCL_LOGINTIME:
            Tcl_SetLongObj(Object, OnlineData.dwOnlineTime);
            break;
          case TCL_TIMEIDLE:
            Tcl_SetLongObj(Object, Time_DifferenceDW32(OnlineData.dwIdleTickCount, GetTickCount()) / 1000);
            break;
          case TCL_HOSTNAME:
            Tcl_IoSetStringObj(Object, OnlineData.szHostName, -1);
            break;
          case TCL_IDENT:
            Tcl_IoSetStringObj(Object, OnlineData.szIdent, -1);
            break;
          case  TCL_ACTION:
            Tcl_IoSetStringObj(Object, OnlineData.tszAction, -1);
            break;
          case TCL_IP:
            inaddr.s_addr  = OnlineData.ulClientIp;
            szIp  = inet_ntoa(inaddr);
            if (! szIp) szIp = "";
            Tcl_SetStringObj(Object, szIp, -1);
            break;
		  case TCL_DATAIP:
			  inaddr.s_addr  = OnlineData.ulDataClientIp;
			  szIp  = inet_ntoa(inaddr);
			  if (! szIp) szIp = "";
			  Tcl_SetStringObj(Object, szIp, -1);
			  break;
		  case TCL_SERVICE:
			  Tcl_IoSetStringObj(Object, OnlineData.tszServiceName, -1);
			  break;
		  case TCL_DEVICE:
			  Tcl_SetIntObj(Object, OnlineData.usDeviceNum);
			  break;
          }
          Tcl_ListObjAppendElement(Interp, ObjectList, Object);
        }
        FreeShared(OnlineData.tszRealPath);
        FreeShared(OnlineData.tszRealDataPath);
        Tcl_SetObjResult(Interp, ObjectList);
        return iReturn;
      }
      else if (! stricmp(szArgument, "INIT"))
      {
        //  Initialize who data
        lpTclData->WhoData.iOffset  = 0;
        n  = 0;
        iResult  = 0;
        iReturn  = TCL_OK;
        //  Process arguments
        for (i = 3U;n < 32U && i < Objc;i++)
        {
          szArgument  = Tcl_GetString(Objv[i]);
          if (! szArgument)
          {
            iResult  = -1;
            break;
          }
          if (! stricmp(szArgument, "UID"))
          {
            lpTclData->WhoData.dwType[n++]  = TCL_UID;
          }
          else if (! stricmp(szArgument, "VIRTUALPATH"))
          {
            lpTclData->WhoData.dwType[n++]  = TCL_VIRTUALPATH;
          }
          else if (! stricmp(szArgument, "VIRTUALDATAPATH"))
          {
            lpTclData->WhoData.dwType[n++]  = TCL_VIRTUALDATAPATH;
          }
          else if (! stricmp(szArgument, "REALPATH"))
          {
            lpTclData->WhoData.dwType[n++]  = TCL_REALPATH;
          }
          else if (! stricmp(szArgument, "REALDATAPATH"))
          {
            lpTclData->WhoData.dwType[n++]  = TCL_REALDATAPATH;
          }
          else if (! stricmp(szArgument, "TRANSFERSIZE"))
          {
            lpTclData->WhoData.dwType[n++]  = TCL_TRANSFEROFFSET;
          }
          else if (! stricmp(szArgument, "TRANSFERSPEED"))
          {
            lpTclData->WhoData.dwType[n++]  = TCL_SPEED;
          }
          else if (! stricmp(szArgument, "STATUS"))
          {
            lpTclData->WhoData.dwType[n++]  = TCL_STATUS;
          }
          else if (! stricmp(szArgument, "LOGINTIME"))
          {
            lpTclData->WhoData.dwType[n++]  = TCL_LOGINTIME;
          }
          else if (! stricmp(szArgument, "TIMEIDLE"))
          {
            lpTclData->WhoData.dwType[n++]  = TCL_TIMEIDLE;
          }
          else if (! stricmp(szArgument, "HOSTNAME"))
          {
            lpTclData->WhoData.dwType[n++]  = TCL_HOSTNAME;
          }
          else if (! stricmp(szArgument, "IP"))
          {
            lpTclData->WhoData.dwType[n++]  = TCL_IP;
          }
          else if (! stricmp(szArgument, "IDENT"))
          {
            lpTclData->WhoData.dwType[n++]  = TCL_IDENT;
          }
          else if (! stricmp(szArgument, "CID"))
          {
            lpTclData->WhoData.dwType[n++]  = TCL_CLIENTID;
          }
          else if (! stricmp(szArgument, "ACTION"))
          {
            lpTclData->WhoData.dwType[n++]  = TCL_ACTION;
          }
		  else if (! stricmp(szArgument, "DATAIP"))
		  {
			  lpTclData->WhoData.dwType[n++]  = TCL_DATAIP;
		  }
		  else if (! stricmp(szArgument, "DEVICE"))
		  {
			  lpTclData->WhoData.dwType[n++]  = TCL_DEVICE;
		  }
		  else if (! stricmp(szArgument, "SERVICE"))
		  {
			  lpTclData->WhoData.dwType[n++]  = TCL_SERVICE;
		  }
        }
        lpTclData->WhoData.dwCount  = n;
      }
    }
  }

  //  Apply result
  sprintf(pBuffer, "%i", iResult);
  Tcl_AppendResult(Interp, pBuffer, NULL);

  return iReturn;
}



INT
Tcl_Buffer(LPTCL_INTERPRETER lpTclInterpreter,
                   Tcl_Interp *Interp,
                   UINT Objc,
                   Tcl_Obj *CONST Objv[])
{
	LPBUFFER  lpBuffer;
	LPTSTR    tszString;
	LPSTR    szCommand;
	INT      iReturn;

	iReturn    = TCL_ERROR;
	lpBuffer  = lpTclInterpreter->lpEventData->lpBuffer;
	Tcl_ResetResult(Interp);

	if (lpBuffer &&
		Objc > 1U && (szCommand = Tcl_GetString(Objv[1])))
	{
		if (! stricmp(szCommand, "GET"))
		{
			//  Copy result
			Put_Buffer(lpBuffer, "", 1);
			Tcl_IoAppendResult(Interp, lpBuffer->buf, -1);
			//  Restore character from temporary buffer
			lpBuffer->len--;
			iReturn  = TCL_OK;
		}
		else if (! stricmp(szCommand, "SET"))
		{
			//  Set output buffer to chosen string
			if (Objc == 3U && (tszString = Tcl_IoGetString(Objv[2], 0, 0, 0)))
			{
				iReturn  = TCL_OK;
				lpBuffer->len  = 0;
				//  Copy string to buffer
				Put_Buffer(lpBuffer, tszString, _tcslen(tszString));
				Free(tszString);
			}
		}
	}
	return iReturn;
}



INT
Tcl_Change(LPTCL_INTERPRETER lpTclInterpreter,
                   Tcl_Interp *Interp,
                   UINT Objc,
                   Tcl_Obj *CONST Objv[])
{
  IO_STRING  Arguments;
  LPTSTR    tszArguments;
  char     temp1[512+8];

  //  Reset result
  Tcl_ResetResult(Interp);

  CopyMemory(temp1, _TEXT("CHANGE "), 7);

  if (Objc == 2U &&
	  (tszArguments = Tcl_IoGetString(Objv[1], &temp1[7], sizeof(temp1)-7,0)))
  {
	  if (! SplitString(tszArguments, &Arguments))
	  {
		  Admin_Change(NULL, _T(""), &Arguments);
		  FreeString(&Arguments);
		  return TCL_OK;
	  }
  }
  return TCL_ERROR;
}




static BOOL Tcl_List_Print(LPLISTING lpListing, LPTSTR tszFileName, BOOL bDotDir, PVIRTUALPATH pvpVirtualPath,
						   LPFILEINFO lpFileInfo, BOOL isDir, DWORD dwMountIndexes, LPVIRTUALINFO lpVirtualInfo)
{
	Tcl_Interp      *lpInterp;
	Tcl_Obj         *lpResult, *lpNew, *lpList;
	LPTSTR		     tszTemp;
	TCHAR            tszRealPath[MAX_PATH+1];
	DWORD            n, m;
	UINT64           u64Time;
	LPMOUNT_ITEM     lpMountItem;

	if (!lpListing->lpTclDirList) return FALSE;

	if (lpListing->lpTclDirList->iResult != TCL_OK) return FALSE;

	if (!_tcscmp(tszFileName, _T(".."))) return FALSE;

	lpInterp = (Tcl_Interp *) lpListing->lpTclDirList->Interp;

	// name type uid user gid group size mode attributes win-last-time unix-last-time win-alt-time unix-alt-time subdir-count {realpath link ...} {chattr-0 chattr-1 chattr-2 chattr-3}

	lpResult = Tcl_NewObj();
	if ( !lpResult ) goto NO_MEMORY;

	if (!(lpNew = Tcl_NewObj())) goto NO_MEMORY;
	Tcl_IoSetStringObj(lpNew, tszFileName, -1);
	if (Tcl_ListObjAppendElement(lpInterp, lpResult, lpNew) != TCL_OK) goto FAILED;

	// type: d (dir), f (file), l (link)
	if (isDir)
	{
		tszTemp = ( (lpFileInfo->dwFileMode & S_SYMBOLIC) ? _T("l") : _T("d") );
	}
	else
	{
		tszTemp = _T("f");
	}
	if (!(lpNew = Tcl_NewStringObj(tszTemp, 1))) goto NO_MEMORY;
	if (Tcl_ListObjAppendElement(lpInterp, lpResult, lpNew) != TCL_OK) goto FAILED;

	if (!(lpNew = Tcl_NewIntObj(lpFileInfo->Uid))) goto NO_MEMORY;
	if (Tcl_ListObjAppendElement(lpInterp, lpResult, lpNew) != TCL_OK) goto FAILED;

	if (! (tszTemp = Uid2User(lpFileInfo->Uid)) ) tszTemp = _T("");
	if (!(lpNew = Tcl_NewStringObj(tszTemp, -1))) goto NO_MEMORY;
	if (Tcl_ListObjAppendElement(lpInterp, lpResult, lpNew) != TCL_OK) goto FAILED;

	if (!(lpNew = Tcl_NewIntObj(lpFileInfo->Gid))) goto NO_MEMORY;
	if (Tcl_ListObjAppendElement(lpInterp, lpResult, lpNew) != TCL_OK) goto FAILED;

	if (! (tszTemp = Gid2Group(lpFileInfo->Gid)) ) tszTemp = _T("");
	if (!(lpNew = Tcl_NewStringObj(tszTemp, -1))) goto NO_MEMORY;
	if (Tcl_ListObjAppendElement(lpInterp, lpResult, lpNew) != TCL_OK) goto FAILED;

	if (!(lpNew = Tcl_NewWideIntObj(lpFileInfo->FileSize)))	goto NO_MEMORY;
	if (Tcl_ListObjAppendElement(lpInterp, lpResult, lpNew) != TCL_OK) goto FAILED;

	if (!(lpNew = Tcl_NewIntObj(lpFileInfo->dwFileMode))) goto NO_MEMORY;
	if (Tcl_ListObjAppendElement(lpInterp, lpResult, lpNew) != TCL_OK) goto FAILED;

	if (!(lpNew = Tcl_NewIntObj(lpFileInfo->dwFileAttributes)))	goto NO_MEMORY;
	if (Tcl_ListObjAppendElement(lpInterp, lpResult, lpNew) != TCL_OK) goto FAILED;

	((ULARGE_INTEGER *) &u64Time)->HighPart = lpFileInfo->ftModificationTime.dwHighDateTime;
	((ULARGE_INTEGER *) &u64Time)->LowPart  = lpFileInfo->ftModificationTime.dwLowDateTime;
	if (!(lpNew = Tcl_NewWideIntObj(u64Time))) goto NO_MEMORY;
	if (Tcl_ListObjAppendElement(lpInterp, lpResult, lpNew) != TCL_OK) goto FAILED;
	u64Time /= 1000 * 1000 * 10;
	u64Time -= 11644473600;
	if (!(lpNew = Tcl_NewIntObj((UINT) u64Time)))	goto NO_MEMORY;
	if (Tcl_ListObjAppendElement(lpInterp, lpResult, lpNew) != TCL_OK) goto FAILED;

	((ULARGE_INTEGER *) &u64Time)->HighPart = lpFileInfo->ftAlternateTime.dwHighDateTime;
	((ULARGE_INTEGER *) &u64Time)->LowPart  = lpFileInfo->ftAlternateTime.dwLowDateTime;
	if (!(lpNew = Tcl_NewWideIntObj(u64Time))) goto NO_MEMORY;
	if (Tcl_ListObjAppendElement(lpInterp, lpResult, lpNew) != TCL_OK) goto FAILED;
	u64Time /= 1000 * 1000 * 10;
	u64Time -= 11644473600;
	if (!(lpNew = Tcl_NewIntObj((UINT) u64Time)))	goto NO_MEMORY;
	if (Tcl_ListObjAppendElement(lpInterp, lpResult, lpNew) != TCL_OK) goto FAILED;

	if (!(lpNew = Tcl_NewIntObj(lpFileInfo->dwSubDirectories)))	goto NO_MEMORY;
	if (Tcl_ListObjAppendElement(lpInterp, lpResult, lpNew) != TCL_OK) goto FAILED;


	// {locations}
	if ( !(lpList = Tcl_NewObj()) ) goto NO_MEMORY;
	if (lpListing->lpMountPoint && dwMountIndexes)
	{
		for(n=1;n<=lpListing->lpMountPoint->dwSubMounts;n++)
		{
			if (dwMountIndexes & (1 << n))
			{
				lpMountItem = &lpListing->lpMountPoint->lpSubMount[n-1];
				tszTemp     = lpMountItem->szFileName;
				for(m=0 ; m<sizeof(tszRealPath) && m<lpMountItem->dwFileName ; m++, tszTemp++)
				{
					if (*tszTemp == _T('\\'))
					{
						tszRealPath[m] = _T('/');
					}
					else
					{
						tszRealPath[m] = *tszTemp;
					}
				}
				if (tszRealPath[m-1] != _T('/')) tszRealPath[m++] = _T('/');
				// now strip off the vfs path of the mountpoint and copy the rest to the real path
				if (pvpVirtualPath->len > lpListing->lpMountPoint->dwPathLen)
				{
					tszTemp = &pvpVirtualPath->pwd[lpListing->lpMountPoint->dwPathLen];
					while (*tszTemp && m<sizeof(tszRealPath))
					{
						tszRealPath[m++] = *tszTemp++;
					}
				}
				tszTemp = lpFileInfo->tszFileName;
				while (*tszTemp && m<sizeof(tszRealPath))
				{
					tszRealPath[m++] = *tszTemp++;
				}
				if (tszRealPath[m-1] != _T('/')) tszRealPath[m++] = _T('/');
				tszRealPath[m] = 0;

				if (!(lpNew = Tcl_NewObj())) goto NO_MEMORY;
				Tcl_IoSetStringObj(lpNew, tszRealPath, -1);
				if (Tcl_ListObjAppendElement(lpInterp, lpList, lpNew) != TCL_OK) goto FAILED;

				if (!(lpNew = Tcl_NewObj())) goto NO_MEMORY;
				if (lpFileInfo->dwFileAttributes & FILE_ATTRIBUTE_LINK)
				{
					// the directory is a linked directory... target is stashed after root entry
					Tcl_IoSetStringObj(lpNew, &lpFileInfo->tszFileName[lpFileInfo->dwFileName+1], -1);
				}
				if (Tcl_ListObjAppendElement(lpInterp, lpList, lpNew) != TCL_OK) goto FAILED;
			}
		}
	}
	if (Tcl_ListObjAppendElement(lpInterp, lpResult, lpList) != TCL_OK) goto FAILED;


	// context list
	if ( !(lpList = Tcl_NewObj()) ) goto NO_MEMORY;
	for(n=0;n<4;n++)
	{
		if (lpFileInfo->dwFileMode & (S_PRIVATE << n))
		{
			tszTemp = (LPTSTR)FindFileContext((BYTE) n, &lpFileInfo->Context);
		}
		else
		{
			tszTemp = _T("");
		}
		if (!(lpNew = Tcl_NewStringObj(tszTemp, -1))) goto NO_MEMORY;
		if (Tcl_ListObjAppendElement(lpInterp, lpList, lpNew) != TCL_OK) goto FAILED;
	}
	if (Tcl_ListObjAppendElement(lpInterp, lpResult, lpList) != TCL_OK) goto FAILED;

	if (!(lpNew = Tcl_NewIntObj(lpFileInfo->dwUploadTimeInMs)))	goto NO_MEMORY;
	if (Tcl_ListObjAppendElement(lpInterp, lpResult, lpNew) != TCL_OK) goto FAILED;

	if (Tcl_ListObjAppendElement(lpInterp, (Tcl_Obj *) lpListing->lpTclDirList->lpList, lpResult) != TCL_OK) goto FAILED;
	return FALSE;


NO_MEMORY:
	Tcl_ResetResult(lpInterp);
	Tcl_SetResult(lpInterp, _T("Out of Memory"), NULL);

FAILED:
	lpListing->lpTclDirList->iResult = TCL_ERROR;
	return FALSE;
}


INT
Tcl_Resolve(LPTCL_INTERPRETER lpTclInterpreter,
                        Tcl_Interp *Interp,
                        UINT Objc,
                        Tcl_Obj *CONST Objv[])
{
  VIRTUALPATH    Path;
  LPTCL_DATA     lpTclData;
  MOUNT_DATA     MountData;
  LPMOUNT_POINT  lpMountPoint;
  DATA_OFFSETS   DataOffsets;
  Tcl_Obj       *lpResult, *lpNew;
  LPSTR          szCommand, szRealPath;
  LPTSTR         tszFileName, tszCurrentDir, tszName;
  TCHAR         *tpOffset;
  DWORD          Id;
  char           temp2[_MAX_PWD+1], temp3[_MAX_PWD+1], *c;
  IO_STRING      Arguments;
  LPLISTING      lpListing;
  TCL_DIRLIST    TclDirList;

  lpTclData  = lpTclInterpreter->lpTclData;
  Tcl_ResetResult(Interp);
  lpResult  = Tcl_NewObj();
  if (! lpResult) return TCL_ERROR;

  if (Objc > 2U && (szCommand = Tcl_GetString(Objv[1])))
  {    
    if (! stricmp(szCommand, "pwd"))
    {
      InitDataOffsets(&DataOffsets, lpTclInterpreter->lpEventData->lpData, lpTclInterpreter->lpEventData->dwData);
      // Resolve pwd to path
      if (! lpTclData->hMountFile ||
        ! lpTclData->lpUserFile) return TCL_ERROR;

      //  Copy current virtual path
      PWD_Reset(&Path);
      if (! DataOffsets.lpCommandChannel)
      {
		  PWD_Set(&Path, _T("/"));
      }
      else PWD_Copy(&DataOffsets.lpCommandChannel->Path, &Path, FALSE);

      if ((tpOffset = Tcl_IoGetString(Objv[2], temp2, sizeof(temp2), 0)) &&
        ((tszFileName = PWD_CWD2(lpTclData->lpUserFile, &Path, tpOffset, lpTclData->hMountFile, &MountData, EXISTS, NULL, NULL, NULL)) ||
        (tszFileName = PWD_CWD2(lpTclData->lpUserFile, &Path, tpOffset, lpTclData->hMountFile, &MountData, 0, NULL, NULL, NULL))))
      {
        //  Replace '\' with tcl friendly '/'
        tpOffset  = tszFileName;
        while ((tpOffset = _tcschr(tpOffset, _TEXT('\\')))) (tpOffset++)[0]  = _TEXT('/');
        //  Append path to result
        Tcl_IoSetStringObj(lpResult, tszFileName, Path.l_RealPath);
        PWD_Free(&Path);
      }
    }
	else if (! stricmp(szCommand, "link"))
	{
		InitDataOffsets(&DataOffsets, lpTclInterpreter->lpEventData->lpData, lpTclInterpreter->lpEventData->dwData);
		// Resolve pwd to path
		if (! lpTclData->hMountFile ||
			! lpTclData->lpUserFile) return TCL_ERROR;

		//  Copy current virtual path
		PWD_Reset(&Path);
		if (! DataOffsets.lpCommandChannel)
		{
			PWD_Set(&Path, _T("/"));
		}
		else PWD_Copy(&DataOffsets.lpCommandChannel->Path, &Path, FALSE);

		if ((tpOffset = Tcl_IoGetString(Objv[2], temp2, sizeof(temp2), 0)) &&
			((tszFileName = PWD_CWD2(lpTclData->lpUserFile, &Path, tpOffset, lpTclData->hMountFile, &MountData, EXISTS|TYPE_LINK, NULL, NULL, NULL)) ||
			(tszFileName = PWD_CWD2(lpTclData->lpUserFile, &Path, tpOffset, lpTclData->hMountFile, &MountData, 0, NULL, NULL, NULL))))
		{
			//  Replace '\' with tcl friendly '/'
			tpOffset  = tszFileName;
			while ((tpOffset = _tcschr(tpOffset, _TEXT('\\')))) (tpOffset++)[0]  = _TEXT('/');
			//  Append path to result
			Tcl_IoSetStringObj(lpResult, tszFileName, Path.l_RealPath);
			PWD_Free(&Path);
		}
	}
	else if (! stricmp(szCommand, "target"))
	{
		InitDataOffsets(&DataOffsets, lpTclInterpreter->lpEventData->lpData, lpTclInterpreter->lpEventData->dwData);
		// Resolve vfs path
		if (! lpTclData->hMountFile ||
			! lpTclData->lpUserFile) return TCL_ERROR;

		//  Setup current virtual path
		PWD_Reset(&Path);
		if (Objc > 3U)
		{
			if (!(tszCurrentDir = Tcl_IoGetString(Objv[3], temp3, sizeof(temp3), 0)))
			{
				return TCL_ERROR;
			}
			PWD_Set(&Path, _T("/"));
			if (PWD_CWD2(lpTclData->lpUserFile, &Path, tszCurrentDir, lpTclData->hMountFile, &MountData, EXISTS, NULL, NULL, NULL) ||
				PWD_CWD2(lpTclData->lpUserFile, &Path, tszCurrentDir, lpTclData->hMountFile, &MountData, 0, NULL, NULL, NULL))
			{
				PWD_Free(&Path);
			}
		}
		else if (! DataOffsets.lpCommandChannel)
		{
			PWD_Set(&Path, _T("/"));
		}
		else PWD_CopyAddSym(&DataOffsets.lpCommandChannel->Path, &Path, FALSE);

		if ((tpOffset = Tcl_IoGetString(Objv[2], temp2, sizeof(temp2), 0)) &&
			((tszFileName = PWD_CWD2(lpTclData->lpUserFile, &Path, tpOffset, lpTclData->hMountFile, &MountData, EXISTS, NULL, NULL, NULL)) ||
			(tszFileName = PWD_CWD2(lpTclData->lpUserFile, &Path, tpOffset, lpTclData->hMountFile, &MountData, 0, NULL, NULL, NULL))))
		{
			PWD_Free(&Path);
			//  Append path to result
			Tcl_IoSetStringObj(lpResult, Path.pwd, Path.len);
		}
	}
	else if (! stricmp(szCommand, "vfs"))
	{
		InitDataOffsets(&DataOffsets, lpTclInterpreter->lpEventData->lpData, lpTclInterpreter->lpEventData->dwData);
		// Resolve vfs path
		if (! lpTclData->hMountFile ||
			! lpTclData->lpUserFile) return TCL_ERROR;

		//  Setup current virtual path
		PWD_Reset(&Path);
		if (Objc > 3U)
		{
			if (!(tszCurrentDir = Tcl_IoGetString(Objv[3], temp3, sizeof(temp3), 0)))
			{
				return TCL_ERROR;
			}
			PWD_Set(&Path, _T("/"));
			if (PWD_CWD2(lpTclData->lpUserFile, &Path, tszCurrentDir, lpTclData->hMountFile, &MountData, EXISTS, NULL, NULL, NULL) ||
				PWD_CWD2(lpTclData->lpUserFile, &Path, tszCurrentDir, lpTclData->hMountFile, &MountData, 0, NULL, NULL, NULL))
			{
				PWD_Free(&Path);
			}
		}
		else if (! DataOffsets.lpCommandChannel)
		{
			PWD_Set(&Path, _T("/"));
		}
		else PWD_CopyAddSym(&DataOffsets.lpCommandChannel->Path, &Path, FALSE);

		if ((tpOffset = Tcl_IoGetString(Objv[2], temp2, sizeof(temp2), 0)) &&
			((tszFileName = PWD_CWD2(lpTclData->lpUserFile, &Path, tpOffset, lpTclData->hMountFile, &MountData, EXISTS|TYPE_LINK, NULL, NULL, NULL)) ||
			(tszFileName = PWD_CWD2(lpTclData->lpUserFile, &Path, tpOffset, lpTclData->hMountFile, &MountData, 0, NULL, NULL, NULL))))
		{
			PWD_Free(&Path);
			//  Append path to result
			Tcl_IoSetStringObj(lpResult, Path.pwd, Path.len);
		}
	}
	else if (! stricmp(szCommand, "symbolic"))
	{
		InitDataOffsets(&DataOffsets, lpTclInterpreter->lpEventData->lpData, lpTclInterpreter->lpEventData->dwData);
		// Resolve vfs path
		if (! lpTclData->hMountFile ||
			! lpTclData->lpUserFile) return TCL_ERROR;

		//  Setup current virtual path
		PWD_Reset(&Path);
		if (Objc > 3U)
		{
			if (!(tszCurrentDir = Tcl_IoGetString(Objv[3], temp3, sizeof(temp3), 0)))
			{
				return TCL_ERROR;
			}
			PWD_Set(&Path, _T("/"));
			if (PWD_CWD2(lpTclData->lpUserFile, &Path, tszCurrentDir, lpTclData->hMountFile, &MountData, EXISTS|KEEP_LINKS, NULL, NULL, NULL) ||
				PWD_CWD2(lpTclData->lpUserFile, &Path, tszCurrentDir, lpTclData->hMountFile, &MountData, KEEP_LINKS, NULL, NULL, NULL))
			{
				PWD_Free(&Path);
			}
		}
		else if (! DataOffsets.lpCommandChannel)
		{
			PWD_Set(&Path, _T("/"));
		}
		else
		{
			PWD_CopyAddSym(&DataOffsets.lpCommandChannel->Path, &Path, FALSE);
		}

		if ((tpOffset = Tcl_IoGetString(Objv[2], temp2, sizeof(temp2), 0)) &&
			((tszFileName = PWD_CWD2(lpTclData->lpUserFile, &Path, tpOffset, lpTclData->hMountFile, &MountData, EXISTS|TYPE_LINK|KEEP_LINKS, NULL, NULL, NULL)) ||
			(tszFileName = PWD_CWD2(lpTclData->lpUserFile, &Path, tpOffset, lpTclData->hMountFile, &MountData, KEEP_LINKS, NULL, NULL, NULL))))
		{
			PWD_Free(&Path);
			//  Append path to result
			Tcl_IoSetStringObj(lpResult, Path.Symbolic, Path.Symlen);
		}
	}
	else if (! stricmp(szCommand, "normalize"))
	{
		InitDataOffsets(&DataOffsets, lpTclInterpreter->lpEventData->lpData, lpTclInterpreter->lpEventData->dwData);
		// Resolve vfs path
		if (! lpTclData->hMountFile) return TCL_ERROR;

		//  Setup current virtual path
		PWD_Reset(&Path);
		if (Objc > 3U)
		{
			if (!(tszCurrentDir = Tcl_IoGetString(Objv[3], temp3, sizeof(temp3), 0)))
			{
				return TCL_ERROR;
			}
			PWD_Set(&Path, tszCurrentDir);
		}
		else if (! DataOffsets.lpCommandChannel)
		{
			PWD_Set(&Path, _T("/"));
		}
		else PWD_Copy(&DataOffsets.lpCommandChannel->Path, &Path, FALSE);

		if ((tpOffset = Tcl_IoGetString(Objv[2], temp2, sizeof(temp2), 0)) &&
			PWD_Normalize(tpOffset, temp3, Path.pwd))
		{
			//  Append path to result
			Tcl_IoSetStringObj(lpResult, temp3, -1);
		}
	}
	else if (! stricmp(szCommand, "mount"))
	{
		InitDataOffsets(&DataOffsets, lpTclInterpreter->lpEventData->lpData, lpTclInterpreter->lpEventData->dwData);
		// Resolve vfs path
		if (! lpTclData->hMountFile ||
			! lpTclData->lpUserFile) return TCL_ERROR;

		//  Setup current virtual path
		if (tszFileName = Tcl_IoGetString(Objv[2], temp2, sizeof(temp2), 0))
		{
			ZeroMemory(&MountData, sizeof(MOUNT_DATA));
			while (szRealPath = PWD_Resolve(tszFileName, lpTclData->hMountFile, &MountData, TRUE, 0))
			{
				if (!(lpNew = Tcl_NewIntObj(MountData.Last)))
				{
					FreeShared(szRealPath);
					return TCL_ERROR;
				}
				Tcl_ListObjAppendElement(NULL, lpResult, lpNew);
				if (! (lpNew = Tcl_NewObj()))
				{
					FreeShared(szRealPath);
					return TCL_ERROR;
				}
				Tcl_IoSetStringObj(lpNew, szRealPath, -1);
				Tcl_ListObjAppendElement(NULL, lpResult, lpNew);
				FreeShared(szRealPath);
			}

			if (!MountData.Resume && (szRealPath = PWD_Resolve(tszFileName, lpTclData->hMountFile, &MountData, FALSE, 0)))
			{
				// no existing match was found, but we still want to return info on the mountpoint if possible...
				FreeShared(szRealPath);
			}

			if (MountData.Resume)
			{
				lpMountPoint = MountData.Resume;
				c = &temp3[lpMountPoint->dwPathLen-1];
				c[1] = 0;
				while (lpMountPoint)
				{
					*c = '/';
					c -= lpMountPoint->dwName;
					strncpy(c, lpMountPoint->szName, lpMountPoint->dwName);
					c--;
					lpMountPoint = lpMountPoint->lpParent;
				}

				if (!(lpNew = Tcl_NewObj())) return TCL_ERROR;
				Tcl_IoSetStringObj(lpNew, temp3, -1);
				// append to front
				if (Tcl_ListObjReplace(Interp, lpResult, 0, 0, 1, &lpNew) != TCL_OK)
				{
					return TCL_ERROR;
				}
			}
		}
	}
	else if (! stricmp(szCommand, "list"))
	{
		InitDataOffsets(&DataOffsets, lpTclInterpreter->lpEventData->lpData, lpTclInterpreter->lpEventData->dwData);
		// Resolve vfs path
		if (! lpTclData->hMountFile ||
			! lpTclData->lpUserFile) return TCL_ERROR;

		ZeroMemory(&Arguments, sizeof(Arguments));

		//  Setup current virtual path
		if (tszCurrentDir = Tcl_IoGetString(Objv[2], temp2, sizeof(temp2), 0))
		{
			PWD_Reset(&Path);
			// Previously this used PWD_Set and trusted that the user passed directory paths ending in a '/', but
			// they could also pass in symbolic paths and if symbolic paths are disabled that would confuse things
			// so just run the path through the resolver again to play it safe...
			if (!PWD_CWD(lpTclData->lpUserFile, &Path, tszCurrentDir, lpTclData->hMountFile, EXISTS))
			{
				return TCL_ERROR;
			}
			TclDirList.Interp  = Interp;
			TclDirList.iResult = TCL_OK;
			TclDirList.lpList  = lpResult;
			if (!(lpListing = List_ParseCmdLine(&Arguments, &Path, lpTclData->hMountFile)))
			{
				PWD_Free(&Path);
				Tcl_SetStringObj(lpResult, "invalid path", -1);
				Tcl_SetObjResult(Interp, lpResult);
				return TCL_ERROR;
			}
			//	Initialize rest of structure
			lpListing->lpFtpUser      = NULL;  // This field only used during recursive listings, so ok to be NULL here
			lpListing->lpUserFile     = lpTclData->lpUserFile;
			lpListing->lpInitialVPath = &Path;
			lpListing->lpBuffer	 	  = DataOffsets.lpBuffer;  // won't be printing to any buffer, so ok to be NULL
			lpListing->lpPrint        = Tcl_List_Print;
			lpListing->lpTclDirList   = &TclDirList;
			lpListing->dwFlags        = LIST_LONG | LIST_FIRST_DIR | LIST_ALL;

			if (InitListing(lpListing, TRUE))
			{
				// this will do all the work...
				ListNextDir(lpListing);
			}
			Tcl_SetObjResult(Interp, TclDirList.lpList);
			Free(lpListing);
			PWD_Free(&Path);
			return TclDirList.iResult;
		}
	}
	else if (! stricmp(szCommand, "uid"))
    {
      if (Tcl_GetIntFromObj(Interp, Objv[2], (PINT)&Id) != TCL_ERROR &&
        Id < MAX_UID)
      {
        //  Resolve user id to user name
        tszName  = Uid2User(Id);
        if (tszName) Tcl_IoSetStringObj(lpResult, tszName, -1);
      }
    }
    else if (! stricmp(szCommand, "gid"))
    {
      if (Tcl_GetIntFromObj(Interp, Objv[2], (PINT)&Id) != TCL_ERROR &&
        Id < MAX_GID)
      {
        //  Resolve group id to group name
        tszName  = Gid2Group(Id);
        if (tszName) Tcl_IoSetStringObj(lpResult, tszName, -1);
      }
    }
    else if (! stricmp(szCommand, "user"))
    {
      if ((tszName = Tcl_IoGetString(Objv[2], temp2, sizeof(temp2), 0)))
      {
        //  Resolve user name to user id
        Tcl_SetIntObj(lpResult, User2Uid(tszName));
      }
    }
    else if (! stricmp(szCommand, "group"))
    {
      if ((tszName = Tcl_IoGetString(Objv[2], temp2, sizeof(temp2), 0)))
      {
        //  Resolve group name to group id
        Tcl_SetIntObj(lpResult, Group2Gid(tszName));
      }
    }
  }
  Tcl_SetObjResult(Interp, lpResult);
  return TCL_OK;
}






INT
Tcl_Vfs(LPTCL_INTERPRETER lpTclInterpreter,
		Tcl_Interp *Interp,
		UINT Objc,
		Tcl_Obj *CONST Objv[])
{
	LPFILEINFO       lpFileInfo;
	Tcl_Obj         *lpResult, *lpList, *lpNew, *lpContext[4];
	VFSUPDATE        UpdateData;
	LPDIRECTORYINFO  lpDirInfo;
	LPSTR    szCommand, szFileMode;
	DWORD    dwFileMode, dwData, dwContextType, n, id;
	LPTSTR   tszFileName, tszData;
	CHAR     pBuffer[3501];
	INT      iReturn, Uid, Gid;
	LPBYTE   lpOffset, lpEnd;
	char     temp2[_MAX_PWD+1], temp3[3501];

	Tcl_ResetResult(Interp);

	if (Objc == 1 || !(szCommand = Tcl_GetString(Objv[1])))
	{
		if (! (lpResult = Tcl_NewObj()) ) return TCL_ERROR;
		Tcl_SetStringObj(lpResult, "missing command", -1);
		Tcl_SetObjResult(Interp, lpResult);
		return TCL_ERROR;
	}

	if (Objc < 3U || !(tszFileName = Tcl_IoGetString(Objv[2], temp2, sizeof(temp2), 0)))
	{
		if (! (lpResult = Tcl_NewObj()) ) return TCL_ERROR;
		Tcl_SetStringObj(lpResult, "missing path or failed conversion", -1);
		Tcl_SetObjResult(Interp, lpResult);
		return TCL_ERROR;
	}

	if (! stricmp(szCommand, "dir"))
	{
		if (! (lpResult = Tcl_NewObj()) ) return TCL_ERROR;

		if (!(lpDirInfo = OpenDirectory(tszFileName, FALSE, TRUE, FALSE, NULL, NULL)))
		{
			Tcl_SetStringObj(lpResult, "invalid path", -1);
			Tcl_SetObjResult(Interp, lpResult);
			return TCL_ERROR;
		}

		for (n=0 ; n < lpDirInfo->dwDirectorySize ; n++ )
		{
			if (! (lpList = Tcl_NewListObj(0,NULL)) ) goto DirError;

			lpFileInfo = lpDirInfo->lpFileInfo[n];

			// { name uid gid mode attr0 attr1 attr2 attr3 }
			if (! (lpNew = Tcl_NewObj()) ) goto DirError;
			Tcl_IoSetStringObj(lpNew, lpFileInfo->tszFileName, lpFileInfo->dwFileName);
			if (Tcl_ListObjAppendElement(NULL, lpList, lpNew) != TCL_OK) goto DirError;

			if (! (lpNew = Tcl_NewIntObj(lpFileInfo->Uid)) ) goto DirError;
			if (Tcl_ListObjAppendElement(NULL, lpList, lpNew) != TCL_OK) goto DirError;

			if (! (lpNew = Tcl_NewIntObj(lpFileInfo->Gid)) ) goto DirError;
			if (Tcl_ListObjAppendElement(NULL, lpList, lpNew) != TCL_OK) goto DirError;

			if (! (lpNew = Tcl_NewIntObj(Base10ToBaseX(lpFileInfo->dwFileMode, 8))) ) goto DirError;
			if (Tcl_ListObjAppendElement(NULL, lpList, lpNew) != TCL_OK) goto DirError;

			lpContext[0] = lpContext[1] = lpContext[2] = lpContext[3] = NULL;

			//  Seek through buffer
			lpOffset  = lpFileInfo->Context.lpData;
			lpEnd  = &lpOffset[lpFileInfo->Context.dwData];
			for (;lpOffset < lpEnd;lpOffset += ((PUINT32)&lpOffset[1])[0])
			{
				id = lpOffset[0];
				if (id < 4 && lpContext[id] == NULL)
				{
					if (! (lpNew = Tcl_NewObj()) ) goto DirError;
					Tcl_IoSetStringObj(lpNew, &lpOffset[sizeof(BYTE) + sizeof(UINT32)], ((PUINT32)&lpOffset[1])[0]);
					lpContext[id] = lpNew;
				}
			}

			for (id=0;id<4;id++)
			{
				if (lpContext[id] != NULL)
				{
					if (Tcl_ListObjAppendElement(NULL, lpList, lpContext[id]) != TCL_OK) goto DirError;
				}
				else
				{
					if (! (lpNew = Tcl_NewObj()) ) goto DirError;
					if (Tcl_ListObjAppendElement(NULL, lpList, lpNew) != TCL_OK) goto DirError;
				}
			}

			if (Tcl_ListObjAppendElement(NULL, lpResult, lpList) != TCL_OK) goto DirError;
		}
		CloseDirectory(lpDirInfo);
		Tcl_SetObjResult(Interp, lpResult);
		return TCL_OK;

DirError:
		CloseDirectory(lpDirInfo);
		return TCL_ERROR;
	}

	if (!GetFileInfo(tszFileName, &lpFileInfo))
	{
		if (! (lpResult = Tcl_NewObj()) ) return TCL_ERROR;
		Tcl_SetStringObj(lpResult, "missing file/directory", -1);
		Tcl_SetObjResult(Interp, lpResult);
		return TCL_ERROR;
	}

	iReturn     = TCL_ERROR;
	pBuffer[0]  = '\0';

	if (! stricmp(szCommand, "read"))
	{
		//  Format result
		wsprintfA(pBuffer, "%i %i %u",
			lpFileInfo->Uid, lpFileInfo->Gid, Base10ToBaseX(lpFileInfo->dwFileMode, 8));
		iReturn  = TCL_OK;
	}
	else if (! stricmp(szCommand, "flush"))
	{
		MarkDirectory(tszFileName);
		if (lpTclInterpreter->lpTclData->hMountFile)
		{
			MarkVirtualDir(NULL, lpTclInterpreter->lpTclData->hMountFile);
		}
		iReturn  = TCL_OK;
	}
	else if (! stricmp(szCommand, "write"))
	{
		//  Check arguments
		if (Objc == 6U &&
			Tcl_GetIntFromObj(Interp, Objv[3], &Uid) != TCL_ERROR &&
			Uid >= -1 && Uid < MAX_UID &&
			Tcl_GetIntFromObj(Interp, Objv[4], &Gid) != TCL_ERROR &&
			Gid >= -1 && Gid < MAX_GID &&
			// file mode doesn't need to be converted from utf...
			Tcl_GetIntFromObj(Interp, Objv[5], (PINT)&dwFileMode) != TCL_ERROR &&
			(szFileMode = Tcl_GetString(Objv[5])) &&
			((dwFileMode = strtol(szFileMode, NULL, 8)) <= 0777L || dwFileMode == (DWORD)-1))
		{
			UpdateData.Uid  = (Uid != -1 ? Uid : lpFileInfo->Uid);
			UpdateData.Gid  = (Gid != -1 ? Gid : lpFileInfo->Gid);
			UpdateData.dwFileMode  = (dwFileMode != (DWORD)-1 ? dwFileMode : lpFileInfo->dwFileMode);
			UpdateData.ftAlternateTime  = lpFileInfo->ftAlternateTime;
			UpdateData.dwUploadTimeInMs = lpFileInfo->dwUploadTimeInMs;
			UpdateData.Context.dwData   = lpFileInfo->Context.dwData;
			UpdateData.Context.lpData   = lpFileInfo->Context.lpData;
			//  Call update
			wsprintfA(pBuffer, "%u",
				(UpdateFileInfo(tszFileName, &UpdateData) ? FALSE : TRUE));
			iReturn  = TCL_OK;
		}
	}
	else if (! stricmp(szCommand, "chattr"))
	{
		if (Objc > 3U &&
			Tcl_GetIntFromObj(Interp, Objv[3], (LPINT)&dwContextType) != TCL_ERROR)
		{
			if (Objc == 4U)
			{
				//  View context
				tszData  = FindFileContext((BYTE)dwContextType, &lpFileInfo->Context);
				if (tszData) sprintf(pBuffer, "%.*s", sizeof(pBuffer) / sizeof(TCHAR) - 1, tszData);
				iReturn  = TCL_OK;
			}
			else if (Objc == 5U && (tszData = Tcl_IoGetString(Objv[4], temp3, sizeof(temp3), 0)))
			{
				dwData  = _tcslen(tszData);
				//  Update context
				UpdateData.Uid  = lpFileInfo->Uid;
				UpdateData.Gid  = lpFileInfo->Gid;
				UpdateData.dwFileMode  = lpFileInfo->dwFileMode;

				if (CreateFileContext(&UpdateData.Context, &lpFileInfo->Context))
				{
					if (InsertFileContext(&UpdateData.Context, (BYTE)dwContextType, tszData, dwData * sizeof(TCHAR)))
					{
						wsprintfA(pBuffer, "%u",
							(UpdateFileInfo(tszFileName, &UpdateData) ? FALSE : TRUE));
					}
					FreeFileContext(&UpdateData.Context);
				}
				iReturn  = TCL_OK;
			}
		}
	}
	CloseFileInfo(lpFileInfo);

	Tcl_IoAppendResult(Interp, pBuffer, -1);
	return iReturn;
}



INT
Tcl_Configuration(LPTCL_INTERPRETER lpTclInterpreter,
                   Tcl_Interp *Interp,
                   UINT Objc,
                   Tcl_Obj *CONST Objv[])
{
  LPSTR  szCommand;
  LPTSTR  tszArray, tszVariable, tszValue;
  INT    iReturn;
  char   temp[_INI_LINE_LENGTH];

  iReturn  = TCL_ERROR;
  Tcl_ResetResult(Interp);

  if (Objc > 1U && (szCommand = Tcl_GetString(Objv[1])))
  {
    if (! stricmp(szCommand, "read"))
    {
      //  Read item from config
      if (Objc == 4U &&
        (tszArray = Tcl_GetString(Objv[2])) && (tszVariable = Tcl_IoGetString(Objv[3], temp, sizeof(temp), 0)))
      {
        //  Get value from config
        tszValue  = Config_Get(&IniConfigFile, tszArray, tszVariable, NULL, NULL);
        iReturn  = TCL_OK;
        //  Append result if valid
		if (tszValue)
		{
			Tcl_IoAppendResult(Interp, tszValue, -1);
		}
        Free(tszValue);
      }
    }
    else if (! stricmp(szCommand, "reload"))
    {
      //  Reload config
      if (! Config_Read(&IniConfigFile))
      {
        InitializeDaemon(FALSE);
        iReturn  = TCL_OK;
      }
    }
    else if (! stricmp(szCommand, "save"))
    {
      //  Dump config to file
      if (! Config_Write(&IniConfigFile)) iReturn  = TCL_OK;
    }
	else if (!stricmp(szCommand, "version"))
	{
		sprintf_s(temp, sizeof(temp), "%u %u %u", dwIoVersion[0], dwIoVersion[1], dwIoVersion[2]);
		// no need to convert UTF8 here
		Tcl_AppendResult(Interp, temp, NULL);
		iReturn = TCL_OK;
	}
	else if (!stricmp(szCommand, "counter"))
	{
		sprintf_s(temp, sizeof(temp), "%u", dwConfigCounter);
		// no need to convert UTF8 here
		Tcl_AppendResult(Interp, temp, NULL);
		iReturn = TCL_OK;
	}
  }
  return iReturn;
}


INT
Tcl_ioDisk(LPTCL_INTERPRETER lpTclInterpreter,
		   Tcl_Interp *Interp,
		   UINT Objc,
		   Tcl_Obj *CONST Objv[])
{
	LPSTR  szCommand;
	Tcl_Obj   *lpResult, *lpNew;
	UINT64  u64MyFree, u64Size, u64Free;
	char    szPath[_MAX_PWD+1];

	Tcl_ResetResult(Interp);
	lpResult  = Tcl_NewObj();
	if (! lpResult) return TCL_ERROR;

	if (Objc < 3)
	{
		Tcl_SetStringObj(lpResult, "Insufficient arguments", -1);
		Tcl_SetObjResult(Interp, lpResult);
		return TCL_ERROR;
	}

	szCommand = Tcl_GetString(Objv[1]);

	if (! stricmp(szCommand, "info"))
	{
		if (Objc < 2)
		{
			Tcl_SetStringObj(lpResult, "missing path", -1);
			Tcl_SetObjResult(Interp, lpResult);
			return TCL_ERROR;
		}
		if (! Tcl_IoGetString(Objv[2], szPath, sizeof(szPath), 0) )
		{
			Tcl_SetStringObj(lpResult, "invalid path", -1);
			Tcl_SetObjResult(Interp, lpResult);
			return TCL_ERROR;
		}

		if (!GetDiskFreeSpaceEx(szPath, (PULARGE_INTEGER) &u64MyFree, (PULARGE_INTEGER) &u64Size, (PULARGE_INTEGER) &u64Free))
		{
			Tcl_SetStringObj(lpResult, "failed command", -1);
			Tcl_SetObjResult(Interp, lpResult);
			return TCL_ERROR;
		}

		lpNew = Tcl_NewWideIntObj(u64MyFree);
		if (! lpNew) return TCL_ERROR;
		Tcl_ListObjAppendElement(NULL, lpResult, lpNew);
		lpNew = Tcl_NewWideIntObj(u64Size);
		if (! lpNew) return TCL_ERROR;
		Tcl_ListObjAppendElement(NULL, lpResult, lpNew);
		lpNew = Tcl_NewWideIntObj(u64Free);
		if (! lpNew) return TCL_ERROR;
		Tcl_ListObjAppendElement(NULL, lpResult, lpNew);
		Tcl_SetObjResult(Interp, lpResult);
		return TCL_OK;
	}

	Tcl_SetStringObj(lpResult, "Invalid command", -1);
	Tcl_SetObjResult(Interp, lpResult);
	return TCL_ERROR;
}


// Extra care taken with not aborting when handle lock may be held to make sure
// we always release it!
INT
Tcl_ioHandleLock(LPTCL_INTERPRETER lpTclInterpreter,
			 Tcl_Interp *Interp,
			 UINT Objc,
			 Tcl_Obj *CONST Objv[])
{
	LPSTR    szCommand;
	Tcl_Obj *lpResult;

	Tcl_ResetResult(Interp);
	lpResult  = Tcl_NewObj();

	if (Objc < 2)
	{
		if (lpResult)
		{
			Tcl_SetStringObj(lpResult, "Insufficient arguments", -1);
			Tcl_SetObjResult(Interp, lpResult);
		}
		return TCL_ERROR;
	}

	szCommand = Tcl_GetString(Objv[1]);

	if (! stricmp(szCommand, "acquire"))
	{
		// trying to acquire it, OK to bail!
		if (!lpResult) return TCL_ERROR;

		if (lpTclInterpreter->lpTclData->dwFlags & TCL_HANDLE_LOCK)
		{
			// rut ro!
			Tcl_SetStringObj(lpResult, "Lock already acquired!", -1);
			Tcl_SetObjResult(Interp, lpResult);
			return TCL_ERROR;
		}
		lpTclInterpreter->lpTclData->dwFlags |= TCL_HANDLE_LOCK;
		AcquireHandleLock();
		Tcl_SetIntObj(lpResult, 1);
		Tcl_SetObjResult(Interp, lpResult);
		return TCL_OK;
	}
	else if (! stricmp(szCommand, "release"))
	{
		if (!(lpTclInterpreter->lpTclData->dwFlags & TCL_HANDLE_LOCK))
		{
			// rut ro!
			if (lpResult)
			{
				Tcl_SetStringObj(lpResult, "Lock not acquired!", -1);
				Tcl_SetObjResult(Interp, lpResult);
			}
			return TCL_ERROR;
		}
		lpTclInterpreter->lpTclData->dwFlags &= ~TCL_HANDLE_LOCK;
		ReleaseHandleLock();
		if (lpResult)
		{
			Tcl_SetIntObj(lpResult, 1);
			Tcl_SetObjResult(Interp, lpResult);
			return TCL_OK;
		}
		return TCL_ERROR;
	}

	if (lpResult)
	{
		Tcl_SetStringObj(lpResult, "Invalid command", -1);
		Tcl_SetObjResult(Interp, lpResult);
	}
	return TCL_ERROR;
}


INT
Tcl_ioMsg(LPTCL_INTERPRETER lpTclInterpreter,
		  Tcl_Interp *Interp,
		  UINT Objc,
		  Tcl_Obj *CONST Objv[])
{
	LPSTR  szCommand, tszMsg, tszNewMsg;
	Tcl_Obj   *lpResult;
	INT      iUid, iCid, iNum, iLen;
	LPCLIENT  lpClient;
	char line[_INI_LINE_LENGTH];
	LPFTPUSER  lpFtpUser;

	Tcl_ResetResult(Interp);

	if (Objc < 5)
	{
		lpResult = Tcl_NewStringObj("Insufficient arguments", -1);
		if (! lpResult) return TCL_ERROR;
		Tcl_SetObjResult(Interp, lpResult);
		return TCL_ERROR;
	}

	szCommand = Tcl_GetString(Objv[1]);
	if (stricmp(szCommand, "get") && stricmp(szCommand, "set"))
	{
		lpResult = Tcl_NewStringObj("Invalid action (set/get)", -1);
		if (! lpResult) return TCL_ERROR;
		Tcl_SetObjResult(Interp, lpResult);
		return TCL_ERROR;
	}

	if (Tcl_GetIntFromObj(Interp, Objv[2], &iUid) != TCL_OK)
	{
		return TCL_ERROR;
	}
	if (Uid2User(iUid) == NULL)
	{
		lpResult = Tcl_NewStringObj("Unknown UID", -1);
		if (! lpResult) return TCL_ERROR;
		Tcl_SetObjResult(Interp, lpResult);
		return TCL_ERROR;
	}

	if (Tcl_GetIntFromObj(Interp, Objv[3], &iCid) != TCL_OK)
	{
		return TCL_ERROR;
	}
	if (iCid < 0 || iCid >= MAX_CLIENTS)
	{
		lpResult = Tcl_NewStringObj("Invalid CID", -1);
		if (! lpResult) return TCL_ERROR;
		Tcl_SetObjResult(Interp, lpResult);
		return TCL_ERROR;
	}

	if (Tcl_GetIntFromObj(Interp, Objv[4], &iNum) != TCL_OK)
	{
		return TCL_ERROR;
	}
	if (iNum < 1 || iNum > MAX_MESSAGES)
	{
		lpResult = Tcl_NewStringObj("Invalid msg number", -1);
		if (! lpResult) return TCL_ERROR;
		Tcl_SetObjResult(Interp, lpResult);
		return TCL_ERROR;
	}
	iNum--;

	if ( !(lpClient = LockClient(iCid)) )
	{
		lpResult = Tcl_NewStringObj("CID inactive", -1);
		if (! lpResult) return TCL_ERROR;
		Tcl_SetObjResult(Interp, lpResult);
		return TCL_ERROR;
	}

	if (lpClient->lpService->dwType != C_FTP || !(lpFtpUser = lpClient->lpUser))
	{
		UnlockClient(iCid);
		lpResult = Tcl_NewStringObj("FTP connections only", -1);
		if (! lpResult) return TCL_ERROR;
		Tcl_SetObjResult(Interp, lpResult);
		return TCL_ERROR;
	}

	if (! stricmp(szCommand, "get"))
	{
		if (lpFtpUser->FtpVariables.tszMsgStringArray[iNum])
		{
			lpResult = Tcl_NewObj();
			if (! lpResult ) 
			{
				UnlockClient(iCid);
				return TCL_ERROR;
			}
			Tcl_IoSetStringObj(lpResult, lpFtpUser->FtpVariables.tszMsgStringArray[iNum], -1);
			Tcl_SetObjResult(Interp, lpResult);
		}
		UnlockClient(iCid);
		return TCL_OK;
	}

	// clear it
	FreeShared(lpFtpUser->FtpVariables.tszMsgStringArray[iNum]);
	lpFtpUser->FtpVariables.tszMsgStringArray[iNum] = 0;

	if (Objc > 5)
	{
		if (!(tszMsg = Tcl_IoGetString(Objv[5], line, sizeof(line), 0)))
		{
			UnlockClient(iCid);
			lpResult = Tcl_NewStringObj("msg read error", -1);
			if (! lpResult) return TCL_ERROR;
			Tcl_SetObjResult(Interp, lpResult);
			return TCL_ERROR;
		}

		iLen = _tcslen(tszMsg);
		if (tszNewMsg = AllocateShared(0, "TCLMSG", iLen+1))
		{
			_tcscpy_s(tszNewMsg, iLen+1, tszMsg);
		}
		lpFtpUser->FtpVariables.tszMsgStringArray[iNum] = tszNewMsg;
	}
	UnlockClient(iCid);
	return TCL_OK;
}


INT
Tcl_ioServer(LPTCL_INTERPRETER lpTclInterpreter,
			 Tcl_Interp *Interp,
			 UINT Objc,
			 Tcl_Obj *CONST Objv[])
{
	LPTCL_DATA   lpTclData;
	Tcl_Obj     *lpResult, *lpNew;
	LPSTR        szCommand;
	time_t       tClosed;
	LPTSTR       tszMsg, tszUserName, tszGroupName, tszSingle, tszReason, tszTemp;
	UINT         u;
	TCHAR        tszMsgTemp[1024];
	BOOL         bNew, bSingle;
	INT32        Uid;


	Tcl_ResetResult(Interp);
	lpResult  = Tcl_NewObj();
	if (! lpResult) return TCL_ERROR;

	if ( Objc == 1U )
	{
		Tcl_SetStringObj(lpResult, "insufficient arguments", -1);
		Tcl_SetObjResult(Interp, lpResult);
		return TCL_ERROR;
	}

	szCommand = Tcl_GetString(Objv[1]);

	// return -1 if open else time and reason
	if (! stricmp(szCommand, "closedfor") )
	{
		while (InterlockedExchange(&FtpSettings.lStringLock, TRUE)) SwitchToThread();

		if (!FtpSettings.tmSiteClosedOn)
		{
			InterlockedExchange(&FtpSettings.lStringLock, FALSE);
			Tcl_SetIntObj(lpResult, -1);
		}
		else
		{
			Tcl_SetIntObj(lpResult, (INT) FtpSettings.tmSiteClosedOn);
			if (FtpSettings.tszCloseMsg)
			{
				lpNew = Tcl_NewStringObj("", -1);
			}
			else
			{
				lpNew = Tcl_NewStringObj(FtpSettings.tszCloseMsg, -1);
			}

			InterlockedExchange(&FtpSettings.lStringLock, FALSE);

			if (! lpNew) return TCL_ERROR;
			Tcl_ListObjAppendElement(NULL, lpResult, lpNew);
		}
		Tcl_SetObjResult(Interp, lpResult);
		return TCL_OK;
	}

	// lookup name/group from the currently open userfile, else "<unknown>"
	lpTclData = lpTclInterpreter->lpTclData;
	if ( lpTclData->lpUserFile && (tszUserName = Uid2User(lpTclData->lpUserFile->Uid)) ) {}
	else tszUserName = _T("<unknown>");

	if ( lpTclData->lpUserFile && (tszGroupName = Gid2Group(lpTclData->lpUserFile->Gid)) ) {}
	else tszGroupName = _T("<unknown>");

	if (! stricmp(szCommand, "open") )
	{
		// Ignore extra arguments...
		while (InterlockedExchange(&FtpSettings.lStringLock, TRUE)) SwitchToThread();

		if (!FtpSettings.tmSiteClosedOn)
		{
			InterlockedExchange(&FtpSettings.lStringLock, FALSE);
			Tcl_SetIntObj(lpResult, -1);
		}
		else
		{
			tClosed = FtpSettings.tmSiteClosedOn;
			FtpSettings.tmSiteClosedOn = 0;
			tszMsg = FtpSettings.tszCloseMsg;
			FtpSettings.tszCloseMsg = NULL;

			tClosed = time((time_t) NULL) - tClosed;

			InterlockedExchange(&FtpSettings.lStringLock, FALSE);

			Putlog(LOG_GENERAL, _TEXT("OPEN: \"%s\" \"%s\" \"%d\" \"%s\"\r\n"), 
				tszUserName, tszGroupName, (DWORD) tClosed, (tszMsg ? tszMsg : _T("<none>")));

			if (tszMsg)
			{
				FreeShared(tszMsg);
			}
			Tcl_SetIntObj(lpResult, (INT32) tClosed);
		}
		Tcl_SetObjResult(Interp, lpResult);
		return TCL_OK;
	}


	if (! stricmp(szCommand, "close") )
	{
		// parse args...
		tszMsg  = NULL;
		bNew    = FALSE;
		bSingle = FALSE;
		for ( u = 2 ; u < Objc ; )
		{
			szCommand = Tcl_GetString(Objv[u++]);

			if (! stricmp(szCommand, "-single") )
			{
				if (u >= Objc)
				{
					Tcl_SetStringObj(lpResult, "Missing username after -single argument", -1);
					Tcl_SetObjResult(Interp, lpResult);
					return TCL_ERROR;
				}

				tszSingle = Tcl_GetString(Objv[u++]);
				if ( ( Uid = User2Uid(tszSingle) ) == -1 )
				{
					Tcl_SetStringObj(lpResult, "Invalid username after -single argument", -1);
					Tcl_SetObjResult(Interp, lpResult);
					return TCL_ERROR;
				}
				bSingle = TRUE;
				continue;
			}

			if (! stricmp(szCommand, "-new") )
			{
				bNew = TRUE;
				continue;
			}

			tszMsg = Tcl_IoGetString(Objv[u-1], tszMsgTemp, sizeof(tszMsgTemp), 0);
			if (!tszMsg)
			{
				Tcl_SetStringObj(lpResult, "Invalid <reason> argument", -1);
				Tcl_SetObjResult(Interp, lpResult);
				return TCL_ERROR;
			}
			break;
		}

		tszReason = NULL;
		if (tszMsg)
		{
			u = _tcslen(tszMsg)+sizeof(*tszMsg);
			tszReason = AllocateShared(NULL, "CloseMsg", u);
			if (tszReason)
			{
				_tcscpy_s(tszReason, u, tszMsg);
			}
		}

		// Ignore extra arguments...
		while (InterlockedExchange(&FtpSettings.lStringLock, TRUE)) SwitchToThread();

		if (!FtpSettings.tmSiteClosedOn)
		{
			FtpSettings.tmSiteClosedOn = time((time_t) NULL);
			tClosed = 0;
		}
		else
		{
			tClosed = FtpSettings.tmSiteClosedOn;
		}

		tszTemp = FtpSettings.tszCloseMsg;
		FtpSettings.tszCloseMsg = tszReason;

		FtpSettings.iSingleCloseUID = ( bSingle ? Uid : -1 );

		InterlockedExchange(&FtpSettings.lStringLock, FALSE);

		Putlog(LOG_GENERAL, _TEXT("CLOSE: \"%s\" \"%s\" \"%s\"\r\n"), tszUserName, tszGroupName, (tszTemp ? tszTemp : _T("<none>")));

		if (tszTemp)
		{
			FreeShared(tszTemp);
		}
		Tcl_SetIntObj(lpResult, (INT32) tClosed);
		Tcl_SetObjResult(Interp, lpResult);
		return TCL_OK;
	}

	Tcl_SetStringObj(lpResult, "Invalid command", -1);
	Tcl_SetObjResult(Interp, lpResult);
	return TCL_ERROR;
}



INT
Tcl_ioTheme(LPTCL_INTERPRETER lpTclInterpreter,
            Tcl_Interp *Interp,
            UINT Objc,
            Tcl_Obj *CONST Objv[])
{
	LPOUTPUT_THEME lpTheme, lpTheme2;
	LPFTPUSER      lpFtpUser;
	Tcl_Obj  *lpResult, *lpNew;
	LPSTR     szCommand, szNum;
	INT       iNum, i;
	char      szSubTheme[_MAX_NAME];


	Tcl_ResetResult(Interp);
	lpResult  = Tcl_NewObj();
	if (! lpResult) return TCL_ERROR;

	if (Objc > 3U)
	{
		Tcl_SetStringObj(lpResult, "Too many arguments", -1);
		Tcl_SetObjResult(Interp, lpResult);
		return TCL_ERROR;
	}

	if ( Objc == 1U )
	{
		Tcl_SetStringObj(lpResult, "insufficient arguments", -1);
		Tcl_SetObjResult(Interp, lpResult);
		return TCL_ERROR;
	}

	szCommand = Tcl_GetString(Objv[1]);

	if (! stricmp(szCommand, "colors") )
	{
		// this command can be used at any time
		if (Objc == 2U)
		{
			Tcl_SetStringObj(lpResult, "insufficient arguments", -1);
			Tcl_SetObjResult(Interp, lpResult);
			return TCL_ERROR;
		}

		szNum = Tcl_GetString(Objv[2]);
		if (!szNum || 1 != sscanf(szNum, "%d", &iNum))
		{
			Tcl_SetStringObj(lpResult, "2nd argument not an integer", -1);
			Tcl_SetObjResult(Interp, lpResult);
			return TCL_ERROR;
		}
		if (iNum < 0 || iNum > (int) dwNumThemes || !(lpTheme2 = LookupTheme(iNum)) )
		{
			Tcl_SetStringObj(lpResult, "invalid theme", -1);
			Tcl_SetObjResult(Interp, lpResult);
			return TCL_ERROR;
		}
		lpNew = Tcl_NewStringObj(lpTheme2->tszName, -1);
		if (! lpNew) return TCL_ERROR;
		Tcl_ListObjAppendElement(NULL, lpResult, lpNew);

		for (i = 1; i<=MAX_COLORS; i++)
		{
			lpNew = Tcl_NewIntObj(lpTheme2->ThemeFieldsArray[i].i);
			if (! lpNew) return TCL_ERROR;
			Tcl_ListObjAppendElement(NULL, lpResult, lpNew);
		}
		Tcl_SetObjResult(Interp, lpResult);
		return TCL_OK;
	}

	if (lpTclInterpreter->lpEventData->dwData != DT_FTPUSER)
	{
		Tcl_SetStringObj(lpResult, "Invalid command or non-interactive FTP login", -1);
		Tcl_SetObjResult(Interp, lpResult);
		return TCL_ERROR;
	}
	lpFtpUser = (LPFTPUSER) lpTclInterpreter->lpEventData->lpData;
	lpTheme = lpFtpUser->FtpVariables.lpTheme;

	if (! stricmp(szCommand, "status") )
	{
		Tcl_SetIntObj(lpResult, lpFtpUser->FtpVariables.iTheme);
		Tcl_SetObjResult(Interp, lpResult);
		return TCL_OK;
	}
	  
	if (! stricmp(szCommand, "off") )
	{
		lpFtpUser->FtpVariables.iTheme = 0;
		lpFtpUser->FtpVariables.lpTheme = 0;
		FreeShared(lpTheme);
		SetTheme(NULL);
		lpTclInterpreter->lpTclData->dwFlags &= ~TCL_THEME_CHANGED;
		return TCL_OK;
	}

	if (!stricmp(szCommand, "subtheme"))
	{
		lpTheme = GetTheme();
		if (!lpTheme)
		{
			Tcl_SetIntObj(lpResult, 0);
			Tcl_SetObjResult(Interp, lpResult);
			return TCL_OK;
		}

		if (Objc < 3)
		{
			if (lpTclInterpreter->lpTclData->dwFlags & TCL_THEME_CHANGED)
			{
				lpTclInterpreter->lpTclData->dwFlags &= ~TCL_THEME_CHANGED;
				SetTheme(lpTclInterpreter->lpTclData->lpTheme);
			}
			Tcl_SetIntObj(lpResult, 0);
			Tcl_SetObjResult(Interp, lpResult);
			return TCL_OK;
		}

		if (! Tcl_IoGetString(Objv[2], szSubTheme, sizeof(szSubTheme), 0))
		{
			Tcl_SetStringObj(lpResult, "insufficient arguments", -1);
			Tcl_SetObjResult(Interp, lpResult);
			return TCL_ERROR;
		}

		if (!(lpTclInterpreter->lpTclData->dwFlags & TCL_THEME_CHANGED))
		{
			lpTclInterpreter->lpTclData->dwFlags |= TCL_THEME_CHANGED;
			lpTclInterpreter->lpTclData->lpTheme = lpTheme;
		}
		Tcl_SetIntObj(lpResult, MessageObject_SetSubTheme(szSubTheme));
		Tcl_SetObjResult(Interp, lpResult);
		return TCL_OK;
	}

  if (Objc < 3)
  {
	  Tcl_SetStringObj(lpResult, "invalid command or insufficient arguments", -1);
	  Tcl_SetObjResult(Interp, lpResult);
	  return TCL_ERROR;
  }

  szNum = Tcl_GetString(Objv[2]);
  if (!szNum || 1 != sscanf(szNum, "%d", &iNum))
  {
	  Tcl_SetStringObj(lpResult, "2nd argument not an integer", -1);
	  Tcl_SetObjResult(Interp, lpResult);
	  return TCL_ERROR;
  }

  if (! stricmp(szCommand, "get") )
  {
	  lpTheme = GetTheme();
	  if (!lpTheme || iNum < 1 || iNum > MAX_COLORS)
	  {
		  Tcl_SetIntObj(lpResult, 0);
	  }
	  else
	  {
		  Tcl_SetIntObj(lpResult, lpTheme->ThemeFieldsArray[iNum].i);
	  }
	  Tcl_SetObjResult(Interp, lpResult);
	  return TCL_OK;
  }

  if (! stricmp(szCommand, "on") )
  {
	  if (iNum < 1 || iNum > (int) dwNumThemes || !(lpTheme2 = LookupTheme(iNum)) )
	  {
		  Tcl_SetStringObj(lpResult, "Invalid theme", -1);
		  Tcl_SetObjResult(Interp, lpResult);
		  return TCL_ERROR;
	  }

	  lpFtpUser->FtpVariables.iTheme = iNum;
	  lpFtpUser->FtpVariables.lpTheme = lpTheme2;
	  SetTheme(lpTheme2);
	  FreeShared(lpTheme);
	  lpTclInterpreter->lpTclData->dwFlags &= ~TCL_THEME_CHANGED;
	  return TCL_OK;
  }

  Tcl_SetStringObj(lpResult, "Invalid command", -1);
  Tcl_SetObjResult(Interp, lpResult);
  return TCL_ERROR;
}


INT
Tcl_Putlog(LPTCL_INTERPRETER lpTclInterpreter,
                   Tcl_Interp *Interp,
                   UINT Objc,
                   Tcl_Obj *CONST Objv[])
{
	DWORD dwWhere = LOG_GENERAL;
	LPTSTR  tszLogLine, tszWhere;
	char line[_INI_LINE_LENGTH];

	if (Objc == 3U && (tszWhere = Tcl_GetString(Objv[1])) && (tszLogLine = Tcl_IoGetString(Objv[2], line, sizeof(line), 0)))
	{
		if (!_tcsicmp(tszWhere, _T("-sysop"))) dwWhere = LOG_SYSOP;
		else if (!_tcsicmp(tszWhere, _T("-error"))) dwWhere = LOG_ERROR;
		else if (!_tcsicmp(tszWhere, _T("-system"))) dwWhere = LOG_SYSTEM;
		else if (!_tcsicmp(tszWhere, _T("-general"))) dwWhere = LOG_GENERAL;
		else return TCL_ERROR;
		Putlog(dwWhere, _TEXT("%s\r\n"), tszLogLine);
		return TCL_OK;
	}
	if (Objc != 2U ||
		! (tszLogLine = Tcl_IoGetString(Objv[1], line, sizeof(line), 0))) return TCL_ERROR;
	//  Put line to log
	Putlog(LOG_GENERAL, _TEXT("%s\r\n"), tszLogLine);
	return TCL_OK;
}


INT
Tcl_iPuts(LPTCL_INTERPRETER lpTclInterpreter,
                  Tcl_Interp *Interp,
                  UINT Objc,
                  Tcl_Obj *CONST Objv[])
{
  LPEVENT_DATA    lpEventData;
  BUFFER              Buffer;
  LPTSTR        tszLine;
  LPSTR        szOption;
  BOOL        bPrefix, bBuffer, bNewLine, bRaw;
  DWORD        dwLine;
  INT          iReturn;
  UINT i;
  char        temp[_INI_LINE_LENGTH];

  iReturn  = TCL_ERROR;
  //  Reset result
  Tcl_ResetResult(Interp);

  if (Objc >= 2U)
  {
    //  Set defaults
    lpEventData  = lpTclInterpreter->lpEventData;
    bPrefix    = (lpEventData->dwFlags & EVENT_PREFIX);
    bBuffer    = TRUE;
    bNewLine  = TRUE;
	bRaw      = FALSE;
 
    for (i = 1U ; i < (Objc - 1) && (szOption = Tcl_GetString(Objv[i])) ; i++)
    {
      if (! stricmp(szOption, "-nonewline"))
      {
        //  No newlines
        bNewLine  = FALSE;
      }
      else if (! stricmp(szOption, "-nobuffer"))
      {
        //  No buffering
        bBuffer  = (lpEventData->lpSocket ? FALSE : TRUE);
      }
      else if (! stricmp(szOption, "-noprefix"))
      {
        //  No prefix
        bPrefix  = FALSE;
      }
	  else if (! stricmp(szOption, "-raw"))
	  {
		  bRaw = TRUE;
	  }
      else
      {
        i  = -1;
        break;
      }
    }

	if (i > 0 && ( tszLine = (bRaw ? Tcl_GetString(Objv[i]) : Tcl_IoGetString(Objv[i], temp, sizeof(temp), 0)) ) )
	{
	  dwLine  = _tcslen(tszLine);

      if (lpEventData->dwFlags & EVENT_SILENT)
      {
        //  No output
      }
      else if (! bBuffer)
      {
		  if (lpEventData->lpSocket)
		  {
			  ZeroMemory(&Buffer, sizeof(Buffer));
			  //  Copy line to buffer
			  if (bPrefix) Put_Buffer(&Buffer, lpEventData->tszPrefix, lpEventData->dwPrefix);
			  Put_Buffer(&Buffer, tszLine, dwLine * sizeof(TCHAR));
			  if (bNewLine) Put_Buffer(&Buffer, _TEXT("\r\n"), 2 * sizeof(TCHAR));

			  if (!SendQuick(lpEventData->lpSocket, Buffer.buf, Buffer.len))
			  {
				  lpEventData->dwFlags  |= EVENT_SILENT;
			  }
			  Free(Buffer.buf);
		  }
      }
      else if (lpEventData->lpBuffer)
      {
        //  Copy line to buffer
        if (bPrefix) Put_Buffer(lpEventData->lpBuffer, lpEventData->tszPrefix, lpEventData->dwPrefix);
        Put_Buffer(lpEventData->lpBuffer, tszLine, dwLine * sizeof(TCHAR));
        if (bNewLine) Put_Buffer(lpEventData->lpBuffer, _TEXT("\r\n"), 2 * sizeof(TCHAR));
      }
      iReturn  = TCL_OK;
    }
  }
  return iReturn;
}


INT
Tcl_MountFile(LPTCL_INTERPRETER lpTclInterpreter,
                          Tcl_Interp *Interp,
                          UINT Objc,
                          Tcl_Obj *CONST Objv[])
{
  MOUNTFILE   hMountFile;
  LPTCL_DATA  lpTclData;
  TCHAR    *tpOffset;
  TCHAR    tszFileName[_MAX_PATH + 1];
  DWORD    dwFileName;
  LPSTR    szCommand, tszArgument;
  Tcl_Obj    *lpResult;
  INT      iResult, iReturn;
  char     temp2[_MAX_PATH+1];

  iResult  = -1;
  iReturn  = TCL_OK;
  lpTclData  = lpTclInterpreter->lpTclData;
  lpResult  = Tcl_NewObj();
  if (! lpResult) return TCL_ERROR;

  if (Objc > 2U &&
    (szCommand = Tcl_GetString(Objv[1])) &&
    (tszArgument = Tcl_IoGetString(Objv[2], temp2, sizeof(temp2), 0)))
  {
    if (! stricmp(szCommand, "OPEN"))
    {
      //  Copy string to work buffer
      dwFileName  = _tcslen(tszArgument);
      if (dwFileName > _MAX_PATH) dwFileName  = _MAX_PATH;
      CopyMemory(tszFileName, tszArgument, dwFileName * sizeof(TCHAR));
      tszFileName[dwFileName]  = '\0';
      //  Convert string to native form
      for (tpOffset = tszFileName;(tpOffset = _tcschr(tpOffset, _TEXT('/')));) (tpOffset++)[0]  = _TEXT('\\');
      //  Open mount file
      hMountFile  = MountFile_Open(tszFileName, NULL);
      if (hMountFile)
      {
        if (lpTclData->dwFlags & TCL_MOUNTFILE_OPEN) MountFile_Close(lpTclData->hMountFile);
        lpTclData->dwFlags  |= TCL_MOUNTFILE_OPEN;
        lpTclData->hMountFile  = hMountFile;
        iResult  = 0;
      }
    }
    else iReturn  = TCL_ERROR;
  }
  else iReturn  = TCL_ERROR;

  //  Set result
  Tcl_SetIntObj(lpResult, iResult);
  Tcl_SetObjResult(Interp, lpResult);

  return iReturn;
}



INT
Tcl_AppendMountPoint(LPTSTR tszPath, DWORD dwLen, DWORD dwLeft, Tcl_Obj *lpResult, LPMOUNT_TABLE lpMountTable)
{
	Tcl_Obj       *lpNew, *lpList;
	LPMOUNT_ITEM   lpMountItem;
	LPMOUNT_POINT  lpMountPoint;
	INT            i;
	DWORD          n, m, dwLen2;
	CHAR           szReal[MAX_PATH];
	LPSTR          szName, szTemp;

	for (n=0 ; n<lpMountTable->dwMountPoints ; n++)
	{
		lpMountPoint = lpMountTable->lpMountPoints[n];
		_tcsncpy_s(&tszPath[dwLen], dwLeft, lpMountPoint->szName, _TRUNCATE);
		dwLen2 = dwLen + (lpMountPoint->dwName < dwLeft ? lpMountPoint->dwName : dwLeft);

		if (lpMountPoint->dwSubMounts)
		{
			lpNew = Tcl_NewObj();
			if ( !lpNew ) return TCL_ERROR;

			Tcl_IoSetStringObj(lpNew, tszPath, dwLen2);
			Tcl_ListObjAppendElement(NULL, lpResult, lpNew);

			lpList = Tcl_NewObj();
			if ( !lpList ) return TCL_ERROR;

			lpMountItem = lpMountPoint->lpSubMount;
			for ( m=0 ; m<lpMountPoint->dwSubMounts ; m++ )
			{
				i = lpMountItem[m].dwFileName;
				if (i > MAX_PATH) i = MAX_PATH-1;
				szName = lpMountItem[m].szFileName;
				szTemp = szReal;

				for ( ; i ; i--, szName++, szTemp++) 
				{
					if (*szName == '\\') *szTemp = '/';
					else *szTemp = *szName;
				}
				*szTemp = 0;

				if ( !(lpNew = Tcl_NewObj()) ) return TCL_ERROR;
				Tcl_IoSetStringObj(lpNew, szReal, -1);
				Tcl_ListObjAppendElement(NULL, lpList, lpNew);
			}

			Tcl_ListObjAppendElement(NULL, lpResult, lpList);
		}

		if (lpMountPoint->lpNextTable && ( dwLeft - dwLen2 > 1 ))
		{
			if (lpMountPoint->dwName != 0)
			{
				tszPath[dwLen2] = _T('/');
				tszPath[++dwLen2] = 0;
			}
			if (Tcl_AppendMountPoint(tszPath, dwLen2, dwLeft-dwLen2, lpResult, (LPMOUNT_TABLE) lpMountPoint->lpNextTable) == TCL_ERROR)
			{
				return TCL_ERROR;
			}
			tszPath[dwLen2] = 0;
		}
	}
	return TCL_OK;
}


INT
Tcl_MountPoints(LPTCL_INTERPRETER lpTclInterpreter,
			  Tcl_Interp *Interp,
			  UINT Objc,
			  Tcl_Obj *CONST Objv[])
{
	MOUNTFILE      hMountFile;
	Tcl_Obj       *lpResult; // *lpNew;
	TCHAR          tszPath[_MAX_PWD + 1];
	TCHAR          tszFileName[_MAX_PATH + 1];
	LPTSTR         tszArg, tpOffset;
	DWORD          dwFileName;
	BOOL           bOpened;
	INT            iResult;
	char           temp1[_MAX_PWD+1];

	Tcl_ResetResult(Interp);
	bOpened = FALSE;
	iResult = TCL_OK;
	if (Objc > 1U &&
		(tszArg = Tcl_IoGetString(Objv[1], temp1, sizeof(temp1), 0)))
	{
		dwFileName  = _tcslen(tszArg);
		if (dwFileName > _MAX_PATH) dwFileName  = _MAX_PATH;
		CopyMemory(tszFileName, tszArg, dwFileName * sizeof(TCHAR));
		tszFileName[dwFileName] = '\0';
		//  Convert string to native form
		for (tpOffset = tszFileName;(tpOffset = _tcschr(tpOffset, _TEXT('/')));) (tpOffset++)[0]  = _TEXT('\\');

		if ( ! (hMountFile = MountFile_Open(tszFileName, NULL)) )
		{
			return TCL_ERROR;
		}
		bOpened = TRUE;
	}
	else
	{
		if ( !lpTclInterpreter->lpTclData->hMountFile ) return TCL_ERROR;
		hMountFile = lpTclInterpreter->lpTclData->hMountFile;
	}

	lpResult  = Tcl_NewObj();
	if ( !lpResult ) return TCL_ERROR;

	//lpNew = Tcl_NewObj();
	//if ( !lpNew ) return TCL_ERROR;
	//Tcl_IoSetStringObj(lpNew, lpMountCacheData->szFileName, -1);
	//Tcl_ListObjAppendElement(NULL, lpResult, lpNew);

	tszPath[0] = _T('/');
	tszPath[1] = 0;
	if (Tcl_AppendMountPoint(tszPath, 1, _MAX_PWD-1, lpResult, hMountFile->lpMountTable) != TCL_OK)
	{
		iResult = TCL_ERROR;
	}

	if (bOpened) MountFile_Close(hMountFile);

	Tcl_SetObjResult(Interp, lpResult);
	return iResult;
}



INT
Tcl_Sections(LPTCL_INTERPRETER lpTclInterpreter,
			 Tcl_Interp *Interp,
			 UINT Objc,
			 Tcl_Obj *CONST Objv[])
{
	LPCONFIG_LINE_ARRAY	 lpArray;
	LPCONFIG_LINE		 lpLine;
	TCHAR				 *tpCheck, *tpLineOffset;
	INT				 	 iCreditSection, iStatsSection, iShareSection, iResult;
	Tcl_Obj             *lpResult, *lpNew;
	BOOL                 bEmpty;

	Tcl_ResetResult(Interp);
	lpResult  = Tcl_NewObj();
	iResult   = TCL_OK;
	if ( !lpResult ) return TCL_ERROR;

	Config_Lock(&IniConfigFile, FALSE);
	lpArray = IniConfigFile.lpLineArray;

	//	Find sections array from config
	for ( ; lpArray ; lpArray = lpArray->Next)
	{
		if (lpArray->Name_Len == 8 &&
			! _tcsncmp(lpArray->Name, _TEXT("Sections"), 8))
		{
			break;
		}
	}

	bEmpty = TRUE;
	if (lpArray)
	{
		//	Find matching line from config
		for (lpLine = lpArray->First_Line ; lpLine ; lpLine = lpLine->Next)
		{
			if (lpLine->Active)
			{
				//	Get credit section
				tpLineOffset	= lpLine->Value;
				iCreditSection	= strtol(tpLineOffset, &tpCheck, 10);
				//	Check validity
				if (tpCheck == tpLineOffset ||
					(tpCheck[0] != _TEXT(' ') && tpCheck[0] != _TEXT('\t'))) continue;
				//	Get stats section
				tpLineOffset	= &tpCheck[1];
				iStatsSection	= _tcstol(tpLineOffset, &tpCheck, 10);
				//	Check validity
				if (tpCheck == tpLineOffset ||
					(tpCheck[0] != _TEXT(' ') && tpCheck[0] != _TEXT('\t')))
				{
					//	Invalid
					iStatsSection	= iCreditSection;
				}
				else
				{
					//	Valid
					tpLineOffset	= &tpCheck[1];
				}
				//	Get share section
				tpLineOffset	= &tpCheck[1];
				iShareSection	= _tcstol(tpLineOffset, &tpCheck, 10);
				//	Check validity
				if (tpCheck == tpLineOffset ||
					(tpCheck[0] != _TEXT(' ') && tpCheck[0] != _TEXT('\t')))
				{
					//	Invalid
					iShareSection	= iCreditSection;
				}
				else
				{
					//	Valid
					tpLineOffset	= &tpCheck[1];
				}
				//	Make sure section numbers are within specified bounds
				if (iCreditSection < 0 || iCreditSection >= MAX_SECTIONS) iCreditSection = 0;
				if (iStatsSection < 0 || iStatsSection >= MAX_SECTIONS) iStatsSection = iCreditSection;
				if (iShareSection < 0 || iShareSection >= MAX_SECTIONS) iShareSection = iCreditSection;
				//	Skip spaces
				for (;tpLineOffset[0] == _TEXT(' ') || tpLineOffset[0] == _TEXT('\t');tpLineOffset++);

				//	Final step - push everything onto list
				lpNew = Tcl_NewObj();
				if ( !lpNew ) {
					iResult = TCL_ERROR;
					break;
				}
				Tcl_IoSetStringObj(lpNew, lpLine->Variable, lpLine->Variable_l);
				Tcl_ListObjAppendElement(NULL, lpResult, lpNew);

				lpNew = Tcl_NewIntObj(iCreditSection);
				if ( !lpNew ) {
					iResult = TCL_ERROR;
					break;
				}
				Tcl_ListObjAppendElement(Interp, lpResult, lpNew);

				lpNew = Tcl_NewIntObj(iStatsSection);
				if ( !lpNew ) {
					iResult = TCL_ERROR;
					break;
				}
				Tcl_ListObjAppendElement(Interp, lpResult, lpNew);

				lpNew = Tcl_NewIntObj(iShareSection);
				if ( !lpNew ) {
					iResult = TCL_ERROR;
					break;
				}
				Tcl_ListObjAppendElement(Interp, lpResult, lpNew);

				lpNew = Tcl_NewObj();
				if ( !lpNew ) {
					iResult = TCL_ERROR;
					break;
				}
				Tcl_IoSetStringObj(lpNew, tpLineOffset, -1);
				Tcl_ListObjAppendElement(NULL, lpResult, lpNew);

				bEmpty = FALSE;
			}
		}
	}

	Config_Unlock(&IniConfigFile, FALSE);


	for ( ; bEmpty ; bEmpty = FALSE)
	{
		lpNew = Tcl_NewObj();
		if ( !lpNew ) {
			iResult = TCL_ERROR;
			break;
		}
		Tcl_SetStringObj(lpNew, _T("Default"), 7);
		Tcl_ListObjAppendElement(NULL, lpResult, lpNew);

		lpNew = Tcl_NewIntObj(0);
		if ( !lpNew ) {
			iResult = TCL_ERROR;
			break;
		}
		Tcl_ListObjAppendElement(Interp, lpResult, lpNew);
		Tcl_ListObjAppendElement(Interp, lpResult, lpNew);
		Tcl_ListObjAppendElement(Interp, lpResult, lpNew);

		lpNew = Tcl_NewObj();
		if ( !lpNew ) {
			iResult = TCL_ERROR;
			break;
		}
		Tcl_SetStringObj(lpNew, _T("/*"), 2);
		Tcl_ListObjAppendElement(NULL, lpResult, lpNew);
	}

	Tcl_SetObjResult(Interp, lpResult);
	return iResult;
}



INT
Tcl_User(LPTCL_INTERPRETER lpTclInterpreter,
                 Tcl_Interp *Interp,
                 UINT Objc,
                 Tcl_Obj *CONST Objv[])
{
  LPTSTR  tszUserName, tszNewUserName;
  LPSTR  szCommand;
  Tcl_Obj  *lpResult, *Object;
  PINT32  lpUserList;
  DWORD  dwUserList, n;
  INT    iResult, iReturn;
  LPFTPUSER lpFtpUser;
  char    temp2[_MAX_NAME+1], temp3[_MAX_NAME+1];
  INT32   Gid;
  LPUSERSEARCH hFind;
  LPUSERFILE lpUserFile;


  iReturn  = TCL_ERROR;
  iResult  = -1;
  lpResult  = Tcl_NewObj();
  if (! lpResult) return TCL_ERROR;

  switch (Objc)
  {
  case 2:
    if (! (szCommand = Tcl_GetString(Objv[1]))) break;
    if (! stricmp(szCommand, "LIST"))
    {
      //  Get userlist
      lpUserList  = GetUsers(&dwUserList);
      if (lpUserList)
      {
        iReturn  = TCL_OK;
        for (n = 0;n < dwUserList;n++)
        {
          Object  = Tcl_NewObj();
          if (! Object) break;
          Tcl_SetIntObj(Object, lpUserList[n]);
          Tcl_ListObjAppendElement(Interp, lpResult, Object);
        }
        Free(lpUserList);
      }
    }
	else if (! stricmp(szCommand, "ClientType"))
	{
		lpFtpUser = (LPFTPUSER) lpTclInterpreter->lpEventData->lpData;
		iReturn = TCL_OK;
		if (lpTclInterpreter->lpEventData->dwData != DT_FTPUSER ||
			!lpFtpUser->FtpVariables.tszClientType)
		{
			Tcl_SetIntObj(lpResult, -1);
		}
		else
		{
			Tcl_IoSetStringObj(lpResult, lpFtpUser->FtpVariables.tszClientType, -1);
		}
	}
	else if (! stricmp(szCommand, "SingleLineResponse"))
	{
		lpFtpUser = (LPFTPUSER) lpTclInterpreter->lpEventData->lpData;
		iReturn = TCL_OK;
		if ( ( lpTclInterpreter->lpEventData->dwData != DT_FTPUSER ) || ! lpFtpUser->FtpVariables.bSingleLineMode )
		{
			Tcl_SetIntObj(lpResult, 0);
		}
		else
		{
			Tcl_SetIntObj(lpResult, 1);
		}
	}
    break;
  default:
    if (Objc < 3U) break;
    //  Get arguments
    if (! (szCommand = Tcl_GetString(Objv[1]))) break;
    if (! (tszUserName = Tcl_IoGetString(Objv[2], temp2, sizeof(temp2), 0))) break;

	if (! stricmp(szCommand, "MATCH"))
	{
		// username is really a pattern...
		hFind	= FindFirstUser(tszUserName, &lpUserFile, NULL, NULL, NULL);
		if (hFind)
		{
			do
			{
				Object  = Tcl_NewIntObj(lpUserFile->Uid);
				if (Object)
				{
					Tcl_ListObjAppendElement(Interp, lpResult, Object);
				}
				UserFile_Close(&lpUserFile, 0);
			} while (! FindNextUser(hFind, &lpUserFile));
		}
		iReturn  = TCL_OK;
	}
	else
	{
		if (! stricmp(szCommand, "CREATE"))
		{
			//  Create new user
			if (Objc < 4U || (Tcl_GetIntFromObj(Interp, Objv[3], &Gid) == TCL_ERROR))
			{
				Gid = -1;
			}

			iResult  = CreateUser(tszUserName, Gid);
			iReturn  = TCL_OK;
		}
		else if (! stricmp(szCommand, "DELETE"))
		{
			//  Delete user
			iResult  = DeleteUser(tszUserName);
			iReturn  = TCL_OK;
		}
		else if (! stricmp(szCommand, "RENAME"))
		{
			//  Rename user
			if (Objc >= 4U &&
				(tszNewUserName = Tcl_IoGetString(Objv[3], temp3, sizeof(temp3), 0)))
			{
				iResult  = RenameUser(tszUserName, tszNewUserName);
				iReturn  = TCL_OK;
			}
		}
		//  Set result
		Tcl_SetIntObj(lpResult, iResult);
	}
  }

  Tcl_SetObjResult(Interp, lpResult);
  return iReturn;
}


INT
Tcl_Group(LPTCL_INTERPRETER lpTclInterpreter,
                  Tcl_Interp *Interp,
                  UINT Objc,
                  Tcl_Obj *CONST Objv[])
{
  LPTSTR  tszGroupName, tszNewGroupName;
  LPSTR  szCommand;
  Tcl_Obj  *lpResult, *Object;
  PINT32  lpGroupList;
  DWORD  dwGroupList, n;
  INT    iResult, iReturn;
  char    temp2[_MAX_NAME+1], temp3[_MAX_NAME+1];

  iReturn  = TCL_ERROR;
  iResult  = -1;
  lpResult  = Tcl_NewObj();
  if (! lpResult) return TCL_ERROR;

  switch (Objc)
  {
  case 2:
    if (! (szCommand = Tcl_GetString(Objv[1]))) break;
    if (! stricmp(szCommand, "LIST"))
    {
      //  Get Grouplist
      lpGroupList  = GetGroups(&dwGroupList);
      if (lpGroupList)
      {
        iReturn  = TCL_OK;
        for (n = 0;n < dwGroupList;n++)
        {
          Object  = Tcl_NewObj();
          if (! Object) break;
          Tcl_SetIntObj(Object, lpGroupList[n]);
          Tcl_ListObjAppendElement(Interp, lpResult, Object);
        }
        Free(lpGroupList);
      }
    }
    break;
  default:
    if (Objc < 3U) break;
    //  Get arguments
    if (! (szCommand = Tcl_GetString(Objv[1]))) break;
    if (! (tszGroupName = Tcl_IoGetString(Objv[2], temp2, sizeof(temp2), 0))) break;

    if (! stricmp(szCommand, "CREATE"))
    {
      //  Create new Group
      iResult  = CreateGroup(tszGroupName);
      iReturn  = TCL_OK;
    }
    else if (! stricmp(szCommand, "DELETE"))
    {
      //  Delete Group
      iResult  = DeleteGroup(tszGroupName);
      iReturn  = TCL_OK;
    }
    else if (! stricmp(szCommand, "RENAME"))
    {
      //  Rename Group
      if (Objc >= 4U &&
        (tszNewGroupName = Tcl_IoGetString(Objv[3], temp3, sizeof(temp3), 0)))
      {
        iResult  = RenameGroup(tszGroupName, tszNewGroupName);
        iReturn  = TCL_OK;
      }
    }
    //  Set result
    Tcl_SetIntObj(lpResult, iResult);
  }

  Tcl_SetObjResult(Interp, lpResult);

  return iReturn;
}


INT
Tcl_UserFile(LPTCL_INTERPRETER lpTclInterpreter,
                         Tcl_Interp *Interp,
                         UINT Objc,
                         Tcl_Obj *CONST Objv[])
{
  LPSTR      szCommand;
  LPTSTR      tszUserName;
  LPEVENT_DATA  lpEventData;
  LPTCL_DATA    lpTclData;
  PCHAR      pUserFile;
  CHAR      pBuffer[32];
  BUFFER      Out;
  LPUSERFILE    lpUserFile;
  INT        iResult, iReturn;
  char       temp[4096];

  lpEventData  = lpTclInterpreter->lpEventData;
  lpTclData  = lpTclInterpreter->lpTclData;
  iResult    = -1;
  iReturn    = TCL_ERROR;

  Tcl_ResetResult(Interp);

  if (Objc >= 2U &&
    (szCommand = Tcl_GetString(Objv[1])))
  {
    ZeroMemory(&Out, sizeof(BUFFER));
    if (! stricmp(szCommand, "open"))
    {
      if (Objc == 3U &&
        (tszUserName = Tcl_IoGetString(Objv[2], temp, sizeof(temp), 0)))
      {
        //  Open userfile
        if (! (iResult = UserFile_Open(tszUserName, &lpUserFile, 0)))
        {
          if (lpTclData->dwFlags & TCL_USERFILE_LOCK)
            UserFile_Unlock(&lpTclData->lpUserFile, 0);
          if (lpTclData->dwFlags & TCL_USERFILE_OPEN)
            UserFile_Close(&lpTclData->lpUserFile, 0);
          lpTclData->lpUserFile  = lpUserFile;
          lpTclData->dwFlags  = (lpTclData->dwFlags|TCL_USERFILE_OPEN) & (0xFFFFFFFF - TCL_USERFILE_LOCK);
        }
        iReturn  = TCL_OK;
      }
    }
    else if (! stricmp(szCommand, "reload"))
    {
      //  Synchronize user file
      if (lpTclData->lpUserFile) iResult  = UserFile_Sync(&lpTclData->lpUserFile);
      iReturn  = TCL_OK;
    }
    else if (! stricmp(szCommand, "isopen"))
    {
      //  Return user id from userfile
      if (lpTclData->lpUserFile) iResult  = lpTclData->lpUserFile->Uid;
      iReturn  = TCL_OK;
    }
    else if (! stricmp(szCommand, "lock"))
    {
      //  Lock userfile
      if (lpTclData->lpUserFile &&
        ! (lpTclData->dwFlags & TCL_USERFILE_LOCK))
      {
        iResult = UserFile_Lock(&lpTclData->lpUserFile, 0);
        if (! iResult) lpTclData->dwFlags  |= TCL_USERFILE_LOCK;
      }
      iReturn  = TCL_OK;
    }
    else if (! stricmp(szCommand, "unlock"))
    {
      //  Unlock userfile
      if (lpTclData->dwFlags & TCL_USERFILE_LOCK)
      {
        iResult  = UserFile_Unlock(&lpTclData->lpUserFile, 0);
        lpTclData->dwFlags  &= (0xFFFFFFFF - TCL_USERFILE_LOCK);
      }
      iReturn  = TCL_OK;
    }
    else if (! stricmp(szCommand, "bin2ascii"))
    {
      //  Convert userfile to ascii
      Out.size  = 4096;
      if (lpTclData->lpUserFile &&
        (Out.buf = (PCHAR)Allocate("UserFile:Bin2Ascii", Out.size)))
      {
        if (! (iResult = UserFile2Ascii(&Out, lpTclData->lpUserFile)))
        {
          Put_Buffer(&Out, "", 1);
          Tcl_IoAppendResult(Interp, Out.buf, -1);
        }
        Free(Out.buf);
      }
      return TCL_OK;
    }
    else if (! stricmp(szCommand, "ascii2bin"))
    {
      //  Convert ascii to userfile
      if (Objc == 3U &&
        (pUserFile = Tcl_IoGetString(Objv[2], temp, sizeof(temp), 0)))
      {
        Out.len  = strlen(pUserFile);
        if (lpTclData->dwFlags & TCL_USERFILE_LOCK &&
          (Out.buf = (PCHAR)Allocate("UserFile:Ascii2Bin", Out.len + 1)))
        {
          CopyMemory(Out.buf, pUserFile, Out.len);
          Out.buf[Out.len++]  = '\n';
          iResult  = Ascii2UserFile(Out.buf, Out.len, lpTclData->lpUserFile);
          Free(Out.buf);
        }
        iReturn  = TCL_OK;
      }
    }
  }
  //  Append result
  if (iReturn == TCL_OK)
  {
    wsprintf(pBuffer, "%i", iResult);
    Tcl_AppendResult(Interp, pBuffer, NULL);
  }

  return iReturn;
}


INT
Tcl_GroupFile(LPTCL_INTERPRETER lpTclInterpreter,
                          Tcl_Interp *Interp,
                          UINT Objc,
                          Tcl_Obj *CONST Objv[])
{
  LPSTR      szCommand;
  LPTSTR      tszGroupName;
  LPEVENT_DATA  lpEventData;
  LPTCL_DATA    lpTclData;
  PCHAR      pGroupFile;
  CHAR      pBuffer[32], temp[4096];
  BUFFER      Out;
  LPGROUPFILE    lpGroupFile;
  INT        iResult, iReturn;

  lpEventData  = lpTclInterpreter->lpEventData;
  lpTclData  = lpTclInterpreter->lpTclData;
  iResult    = -1;
  iReturn    = TCL_ERROR;

  Tcl_ResetResult(Interp);

  if (Objc >= 2U &&
    (szCommand = Tcl_GetString(Objv[1])))
  {
    ZeroMemory(&Out, sizeof(BUFFER));
    if (! stricmp(szCommand, "open"))
    {
      if (Objc == 3U &&
        (tszGroupName = Tcl_IoGetString(Objv[2], temp, sizeof(temp), 0)))
      {
        //  Open Groupfile
        if (! (iResult = GroupFile_Open(tszGroupName, &lpGroupFile, 0)))
        {
          if (lpTclData->dwFlags & TCL_GROUPFILE_LOCK)
            GroupFile_Unlock(&lpTclData->lpGroupFile, 0);
          if (lpTclData->dwFlags & TCL_GROUPFILE_OPEN)
            GroupFile_Close(&lpTclData->lpGroupFile, 0);
          lpTclData->lpGroupFile  = lpGroupFile;
          lpTclData->dwFlags  = (lpTclData->dwFlags|TCL_GROUPFILE_OPEN) & (0xFFFFFFFF - TCL_GROUPFILE_LOCK);
        }
        iReturn  = TCL_OK;
      }
    }
    else if (! stricmp(szCommand, "isopen"))
    {
      //  Return group id from userfile
      if (lpTclData->dwFlags & TCL_GROUPFILE_OPEN) iResult  = lpTclData->lpGroupFile->Gid;
      iReturn  = TCL_OK;
    }
    else if (! stricmp(szCommand, "lock"))
    {
      //  Lock Groupfile
      if (lpTclData->dwFlags & TCL_GROUPFILE_OPEN &&
        ! (lpTclData->dwFlags & TCL_GROUPFILE_LOCK))
      {
        iResult = GroupFile_Lock(&lpTclData->lpGroupFile, 0);
        if (! iResult) lpTclData->dwFlags  |= TCL_GROUPFILE_LOCK;
      }
      iReturn  = TCL_OK;
    }
    else if (! stricmp(szCommand, "unlock"))
    {
      //  Unlock groupfile
      if (lpTclData->dwFlags & TCL_GROUPFILE_LOCK)
      {
        iResult  = GroupFile_Unlock(&lpTclData->lpGroupFile, 0);
        lpTclData->dwFlags  &= (0xFFFFFFFF - TCL_GROUPFILE_LOCK);
      }
      iReturn  = TCL_OK;
    }
    else if (! stricmp(szCommand, "bin2ascii"))
    {
      //  Convert groupfile to ascii
      Out.size  = 4096;
      if (lpTclData->dwFlags & TCL_GROUPFILE_OPEN &&
        (Out.buf = (PCHAR)Allocate("GroupFile:Bin2Ascii", Out.size)))
      {
        if (! (iResult = GroupFile2Ascii(&Out, lpTclData->lpGroupFile)))
        {
          Put_Buffer(&Out, "", 1);
          Tcl_IoAppendResult(Interp, Out.buf, -1);
        }
        Free(Out.buf);
      }
      return TCL_OK;
    }
    else if (! stricmp(szCommand, "ascii2bin"))
    {
      //  Convert ascii to Groupfile
      if (Objc == 3U &&
        (pGroupFile = Tcl_IoGetString(Objv[2], temp, sizeof(temp), 0)))
      {
        Out.len  = strlen(pGroupFile);
        if (lpTclData->dwFlags & TCL_GROUPFILE_LOCK &&
          (Out.buf = (PCHAR)Allocate("GroupFile:Ascii2Bin", Out.len + 1)))
        {
          CopyMemory(Out.buf, pGroupFile, Out.len);
          Out.buf[Out.len++]  = '\n';
          iResult  = Ascii2GroupFile(Out.buf, Out.len, lpTclData->lpGroupFile);
          Free(Out.buf);
        }
        iReturn  = TCL_OK;
      }
    }
  }
  //  Append result
  if (iReturn == TCL_OK)
  {
    wsprintf(pBuffer, "%i", iResult);
    Tcl_AppendResult(Interp, pBuffer, NULL);
  }

  return iReturn;
}



INT Tcl_FindArgument(LPSTR szArgument, ...)
{
  LPSTR  szArg;
  va_list  Arg;
  INT    n;

  va_start(Arg, szArgument);
  for (n = 0;szArg = va_arg(Arg, LPSTR);n++)
  {
    if (! stricmp(szArg, szArgument)) return n;
  }
  return -1;
}



INT __cdecl Tcl_VariableCompare(LPTCL_VARIABLE *lpVar1, LPTCL_VARIABLE *lpVar2)
{
  return strcmp(lpVar1[0]->szName, lpVar2[0]->szName);
}



INT
Tcl_Variable(LPTCL_INTERPRETER lpTclInterpreter,
                         Tcl_Interp *Interp,
                         UINT Objc,
                         Tcl_Obj *CONST Objv[])
{
  LPTCL_VARIABLE  lpVariable;
  LPSTR      szArgument;
  LPVOID      lpMemory;
  DWORD      dwArgument;
  INT        iArgument, iResult;
  LPSTR      szValue;
  char       temp2[64];

  Tcl_ResetResult(Interp);

  //  Get arguments
  if (Objc < 3U ||
    ! (szArgument = Tcl_GetString(Objv[1]))) return TCL_ERROR;

  iArgument  = Tcl_FindArgument(szArgument, "SET", "GET", "UNSET", NULL);
  if (iArgument == -1) return TCL_ERROR;

  if (! (szArgument = Tcl_IoGetString(Objv[2], temp2, sizeof(temp2), 0)) ) return TCL_ERROR;

  lpVariable  = NULL;

  switch (iArgument)
  {
  case 0:
    //  Set variable
    if (Objc == 4U)
    {
      dwArgument  = strlen(szArgument);
      lpVariable  = (LPTCL_VARIABLE)Allocate("GlobalVariable", sizeof(TCL_VARIABLE) + dwArgument);
      if (! lpVariable) return TCL_ERROR;
	  lpVariable->szTclStr = NULL;
      CopyMemory(lpVariable->szName, szArgument, dwArgument + 1);

      EnterCriticalSection(&csGlobalVariables);
      if (dwGlobalVariables == dwGlobalVariablesSize)
      {
        lpMemory  = ReAllocate(lpGlobalVariables, "GlobalVariables", sizeof(LPTCL_VARIABLE) * (dwGlobalVariables + 128));
        if (! lpMemory)
        {
          LeaveCriticalSection(&csGlobalVariables);
          return TCL_ERROR;
        }
        dwGlobalVariablesSize  += 128;
        lpGlobalVariables  = (LPTCL_VARIABLE *)lpMemory;
      }
      iResult  = QuickInsert(lpGlobalVariables, dwGlobalVariables, lpVariable, (QUICKCOMPAREPROC) Tcl_VariableCompare);

      if (iResult)
      {
		  Free(lpVariable);
		  lpVariable = (LPTCL_VARIABLE) lpGlobalVariables[iResult-1];
	  }
	  else dwGlobalVariables++;

	  if (lpVariable)
	  {
		  if (lpVariable->szTclStr) Free(lpVariable->szTclStr);
		  lpVariable->szTclStr  = Tcl_IoGetString(Objv[3], 0, 0, 0);
		  // not freeing memory from IoGetString...
	  }
	  LeaveCriticalSection(&csGlobalVariables);
	  if (!lpVariable || !lpVariable->szTclStr) return TCL_ERROR;
      return TCL_OK;
    }
    break;
  case 1:
    //  Get variable
    if (Objc == 3U)
    {
 	  szValue = NULL;
      lpVariable  = (LPTCL_VARIABLE)((LONG)szArgument - offsetof(TCL_VARIABLE, szName));
      EnterCriticalSection(&csGlobalVariables);
      lpMemory  = bsearch(&lpVariable, lpGlobalVariables, dwGlobalVariables, sizeof(LPTCL_VARIABLE), (QUICKCOMPAREPROC) Tcl_VariableCompare);
	  if (lpMemory)
	  {
		  szValue = ((LPTCL_VARIABLE *)lpMemory)[0]->szTclStr;
		  if (szValue) Tcl_IoAppendResult(Interp, szValue, -1);
	  }
      LeaveCriticalSection(&csGlobalVariables);
      if (!lpMemory || !szValue) return TCL_ERROR;
      return TCL_OK;
    }
    break;
  case 2:
    //  Unset variable
    if (Objc == 3U)
    {
      lpVariable  = (LPTCL_VARIABLE)((LONG)szArgument - offsetof(TCL_VARIABLE, szName));
      EnterCriticalSection(&csGlobalVariables);
      lpVariable  = (LPTCL_VARIABLE)QuickDelete(lpGlobalVariables, dwGlobalVariables, lpVariable,
		                                        (QUICKCOMPAREPROC) Tcl_VariableCompare, NULL);
      if (lpVariable) dwGlobalVariables--;
      LeaveCriticalSection(&csGlobalVariables);

      if (! lpVariable) return TCL_ERROR;
      if (lpVariable->szTclStr) Free(lpVariable->szTclStr);
      Free(lpVariable);
      return TCL_OK;
    }
    break;
  }
  return TCL_ERROR;
}







Tcl_Obj *Tcl_SetVariable(Tcl_Interp *lpInterp, LPSTR szVariableName, Tcl_Obj *lpNewValue)
{
  if (lpNewValue)
  {
    return Tcl_SetVar2Ex(lpInterp, szVariableName, NULL, lpNewValue, TCL_NAMESPACE_ONLY);
  }
  Tcl_UnsetVar(lpInterp, szVariableName, TCL_NAMESPACE_ONLY);
  return NULL;
}




INT
Tcl_AddVirtualEntry(LPTCL_INTERPRETER lpTclInterpreter,
					Tcl_Interp *Interp,
					Tcl_Obj    *lpResult,
					BOOL bDirFlag,
					BOOL bVirtual,
					UINT Objc,
					Tcl_Obj *CONST Objv[])
{
	LPTCL_DATA  lpData;
	TCHAR      *tszName, *tszLink, *tszUser, *tszGroup, *tszPos;
	TCHAR       tszNameBuf[_MAX_PWD+1], tszLinkBuf[_MAX_PWD+1], tszUserBuf[_MAX_PWD+1], tszGroupBuf[_MAX_PWD+1];
	DWORD       dwName, dwLink, dwArgs, dwUser, dwGroup;
	LPVIRTUALINFO  lpVirtualInfo;
	LPFILEINFO     lpFileInfo;
	UINT64         u64Time;
	BOOL           bHideLink;

	lpFileInfo    = NULL;
	lpVirtualInfo = NULL;
	bHideLink     = FALSE;

	lpData = lpTclInterpreter->lpTclData;
	if (!(lpData->dwFlags & TCL_VIRTUAL_EVENT))
	{
		Tcl_SetStringObj(lpResult, "Command only available during virtual directory events", -1);
		goto FAILED;
	}

	// dirs:    Size ModTime AltTime User Group Mode Name Link
	// files:   Size ModTime AltTime User Group Mode Name Link
	// virtual: Size ModTime AltTime User Group Mode Name
	// virtual => dir
	// all dirs have a subdir count of 1

	if (bVirtual && Objc == 3)
	{

		tszName = Tcl_IoGetString(Objv[2], tszNameBuf, sizeof(tszNameBuf), 0);
		if (!tszName || !(dwName = _tcslen(tszName)) || (dwName > _MAX_PWD))
		{
			Tcl_SetStringObj(lpResult, "Invalid Filename", -1);
			goto FAILED;
		}
		dwLink = 0;
		if (lpData->lpUserFile)
		{
			tszUser  = Uid2User(lpData->lpUserFile->Uid);
			tszGroup = Gid2Group(lpData->lpUserFile->Gid);
		}
		else
		{
			tszUser  = Uid2User(DefaultUid[1]);
			tszGroup = Gid2Group(DefaultGid[1]);
		}
		if (!tszUser)
		{
			tszUser = tszUserBuf;
			tszUserBuf[0] = 0;
		}
		if (!tszGroup)
		{
			tszGroup = tszGroupBuf;
			tszGroupBuf[0] = 0;
		}
		dwUser  = _tcslen(tszUser);
		dwGroup = _tcslen(tszGroup);
	}
	else
	{
		if (bVirtual)
		{
			dwArgs = 9;
		}
		else
		{
			dwArgs = 10;
		}

		if ((bVirtual && (Objc < dwArgs)) || (!bVirtual && (Objc < dwArgs-1)))
		{
			Tcl_SetStringObj(lpResult, "Insufficient arguments", -1);
			goto FAILED;
		}

		if (Objc > dwArgs)
		{
			Tcl_SetStringObj(lpResult, "Too many arguments", -1);
			goto FAILED;
		}

		if (!bVirtual && (Objc == dwArgs))
		{
			tszLink = Tcl_IoGetString(Objv[9], tszLinkBuf, sizeof(tszLinkBuf), 0);
			if (!tszLink || ((dwLink = _tcslen(tszLink)) > _MAX_PWD))
			{
				Tcl_SetStringObj(lpResult, "Invalid Link Target", -1);
				goto FAILED;
			}
			if (*tszLink == _T(':'))
			{
				bHideLink = TRUE;
				tszLink++;
				dwLink--;
			}
			if (!*tszLink) {
				Tcl_SetStringObj(lpResult, "Invalid Link Target", -1);
				goto FAILED;
			}
		}
		else dwLink = 0;

		tszName = Tcl_IoGetString(Objv[8], tszNameBuf, sizeof(tszNameBuf), 0);
		if (!tszName || !(dwName = _tcslen(tszName)) || (dwName > _MAX_PWD))
		{
			Tcl_SetStringObj(lpResult, "Invalid Filename", -1);
			goto FAILED;
		}

		tszUser = Tcl_IoGetString(Objv[5], tszUserBuf, sizeof(tszUserBuf), 0);
		if (!tszUser || !(dwUser = _tcslen(tszUser)) || (dwUser > _MAX_PWD))
		{
			Tcl_SetStringObj(lpResult, "Invalid User", -1);
			goto FAILED;
		}

		tszGroup = Tcl_IoGetString(Objv[6], tszGroupBuf, sizeof(tszGroupBuf), 0);
		if (!tszGroup || !(dwGroup = _tcslen(tszGroup)) || (dwGroup > _MAX_PWD))
		{
			Tcl_SetStringObj(lpResult, "Invalid Group", -1);
			goto FAILED;
		}
	}

	// we now know the size we'll need to fake out a FileInfo, so allocate one
	lpVirtualInfo = Allocate("VirtInfo", sizeof(VIRTUALINFO) + sizeof(FILEINFO) + (dwName + dwLink + dwUser + dwGroup + 3)*sizeof(TCHAR));
	if (!lpVirtualInfo) return TCL_ERROR;

	lpFileInfo = (LPFILEINFO) &lpVirtualInfo[1];
	ZeroMemory(lpFileInfo, sizeof(*lpFileInfo));
	lpVirtualInfo->lpFileInfo = lpFileInfo;
	lpVirtualInfo->tszName    = lpFileInfo->tszFileName;
	lpVirtualInfo->bHideLink  = bHideLink;

	lpFileInfo->dwFileName = dwName;
	CopyMemory(lpFileInfo->tszFileName, tszName, dwName + sizeof(TCHAR));
	if (bDirFlag)
	{
		lpFileInfo->Uid = DefaultUid[1];
		lpFileInfo->Gid = DefaultGid[1];
	}
	else
	{
		lpFileInfo->Uid = DefaultUid[0];
		lpFileInfo->Gid = DefaultGid[0];
	}

	tszPos = &lpFileInfo->tszFileName[dwName+1];
	lpVirtualInfo->tszUser = tszPos;
	CopyMemory(lpVirtualInfo->tszUser, tszUser, dwUser + sizeof(TCHAR));
	tszPos += dwUser + 1;

	lpVirtualInfo->tszGroup = tszPos;
	CopyMemory(lpVirtualInfo->tszGroup, tszGroup, dwGroup + sizeof(TCHAR));
	tszPos += dwGroup + 1;


	if (bVirtual && Objc == 3)
	{
		GetSystemTimeAsFileTime(&lpFileInfo->ftModificationTime);
		lpFileInfo->ftAlternateTime = lpFileInfo->ftModificationTime;
		// r-x for ugo
		lpFileInfo->dwFileMode = S_IRUSR|S_IXUSR|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH;
	}
	else
	{
		// Size ModTime AltTime Uid Gid Mode SubDirCount Name [Link]
		if (Tcl_GetWideIntFromObj(Interp, Objv[2], &lpFileInfo->FileSize) != TCL_OK)
		{
			Tcl_SetStringObj(lpResult, "Invalid File Size", -1);
			goto FAILED;
		}

		if (Tcl_GetWideIntFromObj(Interp, Objv[3], &u64Time) != TCL_OK)
		{
			Tcl_SetStringObj(lpResult, "Invalid Modification Time", -1);
			goto FAILED;
		}
		u64Time *= 1000 * 1000 * 10;
		u64Time += 116444736000000000;
		lpFileInfo->ftModificationTime.dwHighDateTime = ((ULARGE_INTEGER *) &u64Time)->HighPart;
		lpFileInfo->ftModificationTime.dwLowDateTime = ((ULARGE_INTEGER *) &u64Time)->LowPart;

		if (Tcl_GetWideIntFromObj(Interp, Objv[4], &u64Time) != TCL_OK)
		{
			Tcl_SetStringObj(lpResult, "Invalid Alternate Time", -1);
			goto FAILED;
		}
		u64Time *= 1000 * 1000 * 10;
		u64Time += 116444736000000000;
		lpFileInfo->ftAlternateTime.dwHighDateTime = ((ULARGE_INTEGER *) &u64Time)->HighPart;
		lpFileInfo->ftAlternateTime.dwLowDateTime = ((ULARGE_INTEGER *) &u64Time)->LowPart;

		if ((Tcl_GetIntFromObj(Interp, Objv[7], &lpFileInfo->dwFileMode) != TCL_OK) || (lpFileInfo->dwFileMode & ~S_ACCESS))
		{
			Tcl_SetStringObj(lpResult, "Invalid Mode", -1);
			goto FAILED;
		}
	}

	if (dwLink)
	{
		lpVirtualInfo->tszLink = tszPos;
		lpFileInfo->dwFileMode |= S_SYMBOLIC;
		CopyMemory(lpVirtualInfo->tszLink, tszLink, dwLink + sizeof(TCHAR));
		tszPos += dwLink + 1;
	}
	else
	{
		lpVirtualInfo->tszLink = NULL;
	}

	if (bDirFlag)
	{
		lpFileInfo->dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_IOFTPD | FILE_ATTRIBUTE_FAKE;
	}
	else
	{
		lpFileInfo->dwFileAttributes = FILE_ATTRIBUTE_IOFTPD;
	}
	lpFileInfo->dwSubDirectories = 1;

	if (!InsertVirtualInfo(lpData->lpVirtualDir, lpVirtualInfo))
	{
		Free(lpVirtualInfo);
		Tcl_SetIntObj(lpResult, 0);
	}
	else
	{
		Tcl_SetIntObj(lpResult, 1);
	}

	Tcl_SetObjResult(Interp, lpResult);
	return TCL_OK;


FAILED:
	if (lpVirtualInfo)
	{
		Free(lpVirtualInfo);
	}
	Tcl_SetObjResult(Interp, lpResult);
	return TCL_ERROR;
}


INT
Tcl_AddVirtualLink(LPTCL_INTERPRETER lpTclInterpreter,
				   Tcl_Interp *Interp,
				   Tcl_Obj    *lpResult,
				   UINT Objc,
				   Tcl_Obj *CONST Objv[])
{
	LPTCL_DATA  lpData;
	TCHAR      *tszLink, *tszName;
	TCHAR       tszLinkBuf[_MAX_PWD+1], tszNameBuf[_MAX_PWD+1];
	DWORD       dwName, dwLink;
	LPSTR       szRealPath;
	LPVIRTUALINFO  lpVirtualInfo;
	LPFILEINFO     lpFileInfo;
	BOOL           bHideLink;

	lpData = lpTclInterpreter->lpTclData;
	if (!(lpData->dwFlags & TCL_VIRTUAL_EVENT))
	{
		Tcl_SetStringObj(lpResult, "Command only available during virtual directory events", -1);
		goto FAILED;
	}

	if (!lpData->hMountFile)
	{
		Tcl_SetStringObj(lpResult, "No active mountfile", -1);
		goto FAILED;
	}

	// PATH [Name]
	if (Objc < 3)
	{
		Tcl_SetStringObj(lpResult, "Insufficient arguments", -1);
		goto FAILED;
	}

	if (Objc > 4)
	{
		Tcl_SetStringObj(lpResult, "Too many arguments", -1);
		goto FAILED;
	}

	tszLink = Tcl_IoGetString(Objv[2], tszLinkBuf, sizeof(tszLinkBuf), 0);
	if (!tszLink || !(dwLink = _tcslen(tszLink)) || (dwLink > _MAX_PWD))
	{
		Tcl_SetStringObj(lpResult, "Invalid Link Target", -1);
		goto FAILED;
	}
	if (*tszLink == _T(':'))
	{
		bHideLink = TRUE;
		tszLink++;
		dwLink--;
		if (!dwLink) goto FAILED;
	}
	else
	{
		bHideLink = FALSE;
	}

	if (Objc == 3)
	{
		tszName = Tcl_IoGetString(Objv[3], tszNameBuf, sizeof(tszNameBuf), 0);
		if (!tszName || !(dwName = _tcslen(tszName)) || (dwName > _MAX_PWD))
		{
			Tcl_SetStringObj(lpResult, "Invalid Name", -1);
			goto FAILED;
		}
	}
	else
	{
		tszName = NULL;
		dwName  = 0;
	}

	// PWD_CWD would resolve symbolic links, but we don't want that and it's slower
	szRealPath = PWD_Resolve(tszLink, lpData->hMountFile, NULL, TRUE, 0);
	if (!szRealPath || !GetFileInfo(szRealPath, &lpFileInfo))
	{
		// not finding the item is not a fatal error, just return 0
		Tcl_SetIntObj(lpResult, 0);
		Tcl_SetObjResult(Interp, lpResult);
		return TCL_OK;
	}

	lpVirtualInfo = Allocate("VirtFileInfo", sizeof(VIRTUALINFO) + (dwName + dwLink + 2)*sizeof(TCHAR));
	if (!lpVirtualInfo)
	{
		CloseFileInfo(lpFileInfo);
		Free(lpVirtualInfo);
		return TCL_ERROR;
	}

	lpVirtualInfo->lpFileInfo = lpFileInfo;
	lpVirtualInfo->bHideLink  = bHideLink;
	lpVirtualInfo->tszLink    = (LPSTR) &lpVirtualInfo[1];
	CopyMemory(lpVirtualInfo->tszLink, tszLink, dwLink + sizeof(TCHAR));

	if (dwName)
	{
		lpVirtualInfo->tszName = &lpVirtualInfo->tszLink[dwLink+1];
		CopyMemory(lpVirtualInfo->tszName, tszName, dwName + sizeof(TCHAR));
	}
	else
	{
		lpVirtualInfo->tszName = lpFileInfo->tszFileName;
	}
	lpVirtualInfo->tszUser  = Uid2User(lpFileInfo->Uid);
	lpVirtualInfo->tszGroup = Gid2Group(lpFileInfo->Gid);

	if (!InsertVirtualInfo(lpData->lpVirtualDir, lpVirtualInfo))
	{
		CloseFileInfo(lpFileInfo);
		Free(lpVirtualInfo);
		return TCL_ERROR;
	}
	Tcl_SetIntObj(lpResult, 1);
	Tcl_SetObjResult(Interp, lpResult);
	return TCL_OK;

FAILED:
	Tcl_SetObjResult(Interp, lpResult);
	return TCL_ERROR;
}



INT
Tcl_ioVirtual(LPTCL_INTERPRETER lpTclInterpreter,
			  Tcl_Interp *Interp,
			  UINT Objc,
			  Tcl_Obj *CONST Objv[])
{
	Tcl_Obj    *lpResult;
	TCHAR      *tszCommand, tszBuf[_MAX_NAME];

	Tcl_ResetResult(Interp);
	lpResult  = Tcl_NewObj();
	if (! lpResult) return TCL_ERROR;

	tszCommand = Tcl_IoGetString(Objv[1], tszBuf, sizeof(tszBuf), 0);
	if (!tszCommand)
	{
		Tcl_SetStringObj(lpResult, "Missing command", -1);
		Tcl_SetObjResult(Interp, lpResult);
		return TCL_ERROR;
	}

	if (!_tcsicmp(tszCommand, _T("AddLink")))
	{
		return Tcl_AddVirtualLink(lpTclInterpreter, Interp, lpResult, Objc, Objv);
	}
	else if (!_tcsicmp(tszCommand, _T("AddDir")))
	{
		return Tcl_AddVirtualEntry(lpTclInterpreter, Interp, lpResult, TRUE, FALSE, Objc, Objv);
	}
	else if (!_tcsicmp(tszCommand, _T("AddFile")))
	{
		return Tcl_AddVirtualEntry(lpTclInterpreter, Interp, lpResult, FALSE, FALSE, Objc, Objv);
	}
	else if (!_tcsicmp(tszCommand, _T("AddSubDir")))
	{
		return Tcl_AddVirtualEntry(lpTclInterpreter, Interp, lpResult, TRUE, TRUE, Objc, Objv);
	}

	Tcl_SetStringObj(lpResult, "Unknown command", -1);
	Tcl_SetObjResult(Interp, lpResult);
	return TCL_ERROR;
}


INT
Tcl_ioUptime(LPTCL_INTERPRETER lpTclInterpreter,
			 Tcl_Interp *Interp,
			 UINT Objc,
			 Tcl_Obj *CONST Objv[])
{
	Tcl_Obj    *lpResult, *lpNew;
	UINT64      u64FileTime, u64FileTime2;

	Tcl_ResetResult(Interp);
	lpResult  = Tcl_NewObj();
	if (! lpResult) return TCL_ERROR;

	GetSystemTimeAsFileTime((FILETIME *) &u64FileTime);
	u64FileTime2 = u64FileTime;

	u64FileTime -= u64WindowsStartTime;
	u64FileTime /= 10000000;

	u64FileTime2 -= u64FtpStartTime;
	u64FileTime2 /= 10000000;

	lpNew = Tcl_NewIntObj((int) u64FileTime);
	if ( !lpNew ) return TCL_ERROR;

	Tcl_ListObjAppendElement(NULL, lpResult, lpNew);

	lpNew = Tcl_NewIntObj((int) u64FileTime2);
	if ( !lpNew ) return TCL_ERROR;

	Tcl_ListObjAppendElement(NULL, lpResult, lpNew);

	Tcl_SetObjResult(Interp, lpResult);
	return TCL_OK;
}


INT
Tcl_ioTransferStats(LPTCL_INTERPRETER lpTclInterpreter,
					Tcl_Interp *Interp,
					UINT Objc,
					Tcl_Obj *CONST Objv[])
{
	Tcl_Obj    *lpResult, *lpNew;
	LPFTPUSER   lpUser;

	Tcl_ResetResult(Interp);
	lpResult  = Tcl_NewObj();
	if (! lpResult) return TCL_ERROR;

	if (!(lpUser = lpTclInterpreter->lpTclData->lpFtpUser))
	{
		// without a userfile this command is an error
		Tcl_SetStringObj(lpResult, "Not interactive user account", -1);
		Tcl_SetObjResult(Interp, lpResult);
		return TCL_ERROR;
	}

	lpNew = Tcl_NewWideIntObj(lpUser->DataChannel.Size);
	if ( !lpNew ) return TCL_ERROR;
	Tcl_ListObjAppendElement(NULL, lpResult, lpNew);

	lpNew = Tcl_NewIntObj(lpUser->DataChannel.dwDuration);
	if ( !lpNew ) return TCL_ERROR;
	Tcl_ListObjAppendElement(NULL, lpResult, lpNew);

	Tcl_SetObjResult(Interp, lpResult);
	return TCL_OK;
}



VOID Tcl_InterpDeleteProcCB(ClientData clientData, Tcl_Interp *interp)
{
	DWORD dwUniqueId = (DWORD) clientData;

	if (bDebugTclInterpreters)
	{
		Putlog(LOG_DEBUG, "Deleting TCL Interpreter #%d\r\n", dwUniqueId);
	}
}



VOID Tcl_DeleteInterpreter(LPTCL_INTERPRETER lpTclInterpreter)
{
	TlsSetValue(dwTclInterpreterTlsIndex, NULL);

	if (bDebugTclInterpreters)
	{
		Putlog(LOG_DEBUG, "Marked for deletion TCL Interpreter #%d\r\n", lpTclInterpreter->dwUniqueId);
	}
	Tcl_DeleteInterp((Tcl_Interp *)lpTclInterpreter->lpInterp);
	Free(lpTclInterpreter);

	// this is supposed to cleanup all memory TCL is using for the thread, only do this when
	// debugging since keeping the infrastructure is fine when just doing a rehash.
	if (bDebugTclInterpreters)
	{
		Tcl_FinalizeThread();
	}
}


VOID Tcl_InterpreterDestructor(VOID)
{
	LPTCL_INTERPRETER lpTclInterpreter;

	lpTclInterpreter = (LPTCL_INTERPRETER)TlsGetValue(dwTclInterpreterTlsIndex);

	if (lpTclInterpreter)
	{
		Tcl_DeleteInterpreter(lpTclInterpreter);
		Tcl_FinalizeThread();
		TlsSetValue(dwTclInterpreterTlsIndex, 0);
	}
}


VOID Tcl_ThreadExitProc(ClientData clientData)
{
	DWORD dwUniqueId = (DWORD) clientData;

	if (bDebugTclInterpreters)
	{
		Putlog(LOG_DEBUG, "Finalizing TCL Interpreter #%d for thread %d\r\n", dwUniqueId, GetCurrentThreadId());
	}
	InterlockedDecrement(&lTclInterps);
}


LPTCL_INTERPRETER Tcl_GetInterpreter(BOOL bCreate)
{
	LPTCL_INTERPRETER  lpTclInterpreter;
	DWORD              n;
	CHAR               pBuffer[128];
	BOOL               bKeepInterpreter;

	lpTclInterpreter  = (LPTCL_INTERPRETER)TlsGetValue(dwTclInterpreterTlsIndex);

	if (lpTclInterpreter && lpTclInterpreter->dwConfigCounter == dwConfigCounter)
	{
		return lpTclInterpreter;
	}

	if (lpTclInterpreter)
	{
		Tcl_DeleteInterpreter(lpTclInterpreter);
		lpTclInterpreter = 0;
	}

	if (!bCreate && bDebugTclInterpreters)
	{
		return NULL;
	}
	
	wsprintfA(pBuffer, "%u-%u-%u", GetCurrentThreadId(), (DWORD)GetCurrentFiber(), GetTickCount());
	//  Create interpreter
	lpTclInterpreter  = (LPTCL_INTERPRETER)Allocate("TCL:Interpreter", sizeof(TCL_INTERPRETER));
	if (! lpTclInterpreter) return NULL;
	ZeroMemory(lpTclInterpreter, sizeof(TCL_INTERPRETER));

	lpTclInterpreter->lpInterp  = Tcl_CreateInterp();
	if (! lpTclInterpreter->lpInterp)
	{
		Free(lpTclInterpreter);
		return NULL;
	}

	InterlockedIncrement(&lTclInterps);

	//  Install interpreter destructor for thread
	bKeepInterpreter  = InstallResourceDestructor(Tcl_InterpreterDestructor);
	if (bKeepInterpreter)
	{
		TlsSetValue(dwTclInterpreterTlsIndex, lpTclInterpreter);
	}
	lpTclInterpreter->dwUniqueId = InterlockedIncrement(&dwTclUniqueId);
	lpTclInterpreter->dwConfigCounter = dwConfigCounter;
	Tcl_CallWhenDeleted(lpTclInterpreter->lpInterp, Tcl_InterpDeleteProcCB, (VOID *) lpTclInterpreter->dwUniqueId);
	if (bDebugTclInterpreters)
	{
		Putlog(LOG_DEBUG, "Created TCL Interpreter #%d\r\n", lpTclInterpreter->dwUniqueId);
	}
	Tcl_CreateThreadExitHandler(Tcl_ThreadExitProc, (VOID *) lpTclInterpreter->dwUniqueId);

	//  Initialize tcl interpreter
	Tcl_SetVar(lpTclInterpreter->lpInterp, "tcl_interactive", "1", TCL_GLOBAL_ONLY);
	Tcl_SetVar(lpTclInterpreter->lpInterp, "io_id", pBuffer, TCL_GLOBAL_ONLY);
	Tcl_Init(lpTclInterpreter->lpInterp);

	for (n = 0;TCL_Command[n].szCommand;n++)
	{
		Tcl_CreateObjCommand(lpTclInterpreter->lpInterp,
			TCL_Command[n].szCommand, (TCL_CMD)TCL_Command[n].lpProc, (LPVOID)lpTclInterpreter, NULL);
	}

	Tcl_EvalFile(lpTclInterpreter->lpInterp, "../scripts/init.itcl");

	return lpTclInterpreter;
}



BOOL TclExecute2(LPEVENT_DATA lpEventData, IO_STRING *Arguments, LPINT lpiResult, LPVIRTUALDIR lpVirtualDir)
{
  DATA_OFFSETS      DataOffset;
  LPTCL_INTERPRETER    lpTclInterpreter;
  TCL_DATA        TclData;
  Tcl_Obj          *lpSpeed, *lpGid, *lpUid, *lpStatsSection, *lpCreditSection, *lpCid, *lpGroupList, *lpResult, *lpNew;
  Tcl_Obj          *lpArguments, *lpArgList, *lpFlags, *lpUserName, *lpGroupName, *lpPath, *lpVirtualPath, *lpSymbolicPath, *lpUniqueId, *lpPrefix;
  Tcl_Interp        *lpInterp;
  BUFFER          DataBuffer;
  DOUBLE          dSpeed;
  BOOL          bReturn, bKeepInterpreter;
  LPTSTR          tszFileName, szErrorCode, szErrorInfo, tszResult;
  PCHAR          pNewline;
  DWORD          n, dwErrorCode;

  //  Initialize data offsets
  InitDataOffsets(&DataOffset, lpEventData->lpData, lpEventData->dwData);
  ZeroMemory(&TclData, sizeof(TCL_DATA));

  if (lpEventData->dwData == DT_FTPUSER) TclData.lpFtpUser = lpEventData->lpData;

  tszFileName  = GetStringIndexStatic(Arguments, 0);
  TclData.lpUserFile  = DataOffset.lpUserFile;
  TclData.hMountFile  = DataOffset.hMountFile;
  if (lpVirtualDir)
  {
	  TclData.lpVirtualDir = lpVirtualDir;
	  TclData.dwFlags |= TCL_VIRTUAL_EVENT;
  }

  if (! (lpTclInterpreter = Tcl_GetInterpreter(TRUE)) )
  {
	  return TRUE;
  }

  bKeepInterpreter = ((LPTCL_INTERPRETER)TlsGetValue(dwTclInterpreterTlsIndex) ? TRUE : FALSE);

  lpTclInterpreter->lpEventData  = lpEventData;
  lpTclInterpreter->lpTclData    = &TclData;
  lpInterp  = (Tcl_Interp *)lpTclInterpreter->lpInterp;

  if (GetStringItems(Arguments) > 1) {
    lpArguments=Tcl_NewObj();
    if (lpArguments) {
		tszResult=GetStringRange(Arguments, 1, STR_END);
      Tcl_IoSetStringObj(lpArguments, tszResult, -1);
    }
	lpArgList=Tcl_NewObj();
	if (lpArgList)
	{
		for(n=1;n<GetStringItems(Arguments);n++)
		{
			if ( !(lpNew = Tcl_NewObj()) )
			{
				lpArgList = NULL;
				break;
			}
			tszResult=GetStringIndex(Arguments, n);
			Tcl_IoSetStringObj(lpNew, tszResult, -1);
			Tcl_ListObjAppendElement(NULL, lpArgList, lpNew);
		}
	}

  } else {
    lpArguments=NULL;
	lpArgList=NULL;
  }
    
  lpUniqueId = Tcl_NewObj();
  if (lpUniqueId) Tcl_SetIntObj(lpUniqueId, lpTclInterpreter->dwUniqueId);

  if (lpEventData->tszPrefix)
  {
	  lpPrefix = Tcl_NewObj();
	  if (lpPrefix) Tcl_SetStringObj(lpPrefix, lpEventData->tszPrefix, lpEventData->dwPrefix);
  }
  else
  {
	  lpPrefix = NULL;
  }

  if (DataOffset.lpUserFile)
  {
    //  Get speed
    if (DataOffset.lpDataChannel &&	DataOffset.lpDataChannel->Size > 0 && DataOffset.lpDataChannel->dwDuration)
	{
      dSpeed  = DataOffset.lpDataChannel->Size / 1024. / (((DOUBLE) DataOffset.lpDataChannel->dwDuration) / 1000.0);
    }
    else dSpeed  = 0.;

    //  User ids
	lpUid  = Tcl_NewObj();
	if (lpUid) Tcl_SetIntObj(lpUid, DataOffset.lpUserFile->Uid);
    lpGid  = Tcl_NewObj();
    if (lpGid) Tcl_SetIntObj(lpGid, DataOffset.lpUserFile->Gid);
    //  Speed
    lpSpeed  = Tcl_NewObj();
    if (lpSpeed) Tcl_SetDoubleObj(lpSpeed, dSpeed);

	//  Sections
    lpCreditSection  = Tcl_NewObj();
    if (lpCreditSection) Tcl_SetIntObj(lpCreditSection, DataOffset.iCreditSection);
    lpStatsSection  = Tcl_NewObj();
    if (lpStatsSection) Tcl_SetIntObj(lpStatsSection, DataOffset.iStatsSection);
    //  Flags
    lpFlags  = Tcl_NewObj();
    if (lpFlags) Tcl_SetStringObj(lpFlags, DataOffset.lpUserFile->Flags, _tcslen(DataOffset.lpUserFile->Flags));
    //  Group list
    lpGroupList  = Tcl_NewObj();
    if (lpGroupList)
    {
      for (n = 0;n < MAX_GROUPS && DataOffset.lpUserFile->Groups[n] != -1;n++)
      {
        tszResult  = Gid2Group(DataOffset.lpUserFile->Groups[n]);
        if (tszResult)
        {
          lpGroupName  = Tcl_NewObj();
          if (lpGroupName)
          {
            Tcl_IoSetStringObj(lpGroupName, tszResult, -1);
            Tcl_ListObjAppendElement(NULL, lpGroupList, lpGroupName);
          }
        }
      }
    }
    //  Primary group
    tszResult  = Gid2Group(DataOffset.lpUserFile->Gid);
    if (tszResult)
    {
      lpGroupName  = Tcl_NewObj();
      if (lpGroupName) Tcl_IoSetStringObj(lpGroupName, tszResult, -1);
    }
    else lpGroupName  = NULL;
    //  Username
    tszResult  = Uid2User(DataOffset.lpUserFile->Uid);
    if (tszResult)
    {
      lpUserName  = Tcl_NewObj();
      if (lpUserName) Tcl_IoSetStringObj(lpUserName, tszResult, -1);
    }
    else lpUserName  = NULL;

    //  Paths
    if (DataOffset.lpCommandChannel)
    {
      lpPath  = Tcl_NewObj();
      if (lpPath) Tcl_IoSetStringObj(lpPath, DataOffset.lpCommandChannel->Path.RealPath, DataOffset.lpCommandChannel->Path.l_RealPath);
      lpVirtualPath  = Tcl_NewObj();
      if (lpVirtualPath) Tcl_IoSetStringObj(lpVirtualPath, DataOffset.lpCommandChannel->Path.pwd, DataOffset.lpCommandChannel->Path.len);
	  lpSymbolicPath  = Tcl_NewObj();
	  if (lpSymbolicPath)
	  {
		  if (DataOffset.hMountFile && DataOffset.hMountFile->lpFtpUser && DataOffset.hMountFile->lpFtpUser->FtpVariables.bKeepLinksInPath)
		  {
			  Tcl_IoSetStringObj(lpSymbolicPath, DataOffset.lpCommandChannel->Path.Symbolic, DataOffset.lpCommandChannel->Path.Symlen);
		  }
		  else
		  {
			  Tcl_IoSetStringObj(lpSymbolicPath, DataOffset.lpCommandChannel->Path.pwd, DataOffset.lpCommandChannel->Path.len);
		  }
	  }
	  else
	  {
		  lpSymbolicPath = NULL;
	  }
    }
    else
    {
      lpPath  = NULL;
      lpVirtualPath  = NULL;
	  lpSymbolicPath = NULL;
    }
  }
  else
  {
    lpUid  = NULL;
    lpGid  = NULL;
    lpSpeed  = NULL;
    lpFlags  = NULL;
    lpPath  = NULL;
    lpCreditSection  = NULL;
    lpStatsSection  = NULL;
    lpVirtualPath  = NULL;
	lpSymbolicPath = NULL;
    lpGroupList  = NULL;
    lpUserName  = NULL;
    lpGroupName  = NULL;
  }

  //  Client id
  if (DataOffset.pConnectionInfo)
  {
    lpCid  = Tcl_NewObj();
    if (lpCid) Tcl_SetIntObj(lpCid, DataOffset.pConnectionInfo->dwUniqueId);
  }
  else lpCid  = NULL;

  //  Set variables
  Tcl_SetVariable(lpInterp, "args", lpArguments);
  Tcl_SetVariable(lpInterp, "cid", lpCid);
  Tcl_SetVariable(lpInterp, "creditsection", lpCreditSection);
  Tcl_SetVariable(lpInterp, "cwd", lpSymbolicPath);
  Tcl_SetVariable(lpInterp, "flags", lpFlags);
  Tcl_SetVariable(lpInterp, "gid", lpGid);
  Tcl_SetVariable(lpInterp, "groups", lpGroupList);
  Tcl_SetVariable(lpInterp, "group", lpGroupName);
  Tcl_SetVariable(lpInterp, "ioArgs", lpArgList);
  Tcl_SetVariable(lpInterp, "ioPrefix", lpPrefix);
  Tcl_SetVariable(lpInterp, "path", lpPath);
  Tcl_SetVariable(lpInterp, "pwd", lpVirtualPath);
  Tcl_SetVariable(lpInterp, "speed", lpSpeed);
  Tcl_SetVariable(lpInterp, "statssection", lpStatsSection);
  Tcl_SetVariable(lpInterp, "uid", lpUid);
  Tcl_SetVariable(lpInterp, "uniqueid", lpUniqueId);
  Tcl_SetVariable(lpInterp, "user", lpUserName);

  if (GetFileAttributes(tszFileName) == INVALID_FILE_ATTRIBUTES)
  {
	  bReturn = TRUE;
	  dwErrorCode = ERROR_SCRIPT_MISSING;
	  Putlog(LOG_SYSTEM, _TEXT("Missing TCL script '%s'.\r\n"), tszFileName);
  }
  else if (Tcl_EvalFile(lpInterp, tszFileName) != TCL_OK)    //  Evaluate file
  {
	  bReturn = TRUE;
	  dwErrorCode = IO_SCRIPT_FAILURE;

	  //  Allocate memory for data buffer
	  DataBuffer.dwType  = TYPE_CHAR;
	  DataBuffer.len  = 0;
	  DataBuffer.size  = 1024;
	  DataBuffer.buf  = (PCHAR)Allocate("TCL:DataBuffer", 1024);

	  szErrorCode  = (LPSTR)Tcl_GetVar(lpInterp, "errorCode", 0);
	  lpArguments  = Tcl_GetVar2Ex(lpInterp, "errorInfo", NULL, 0);

	  //  Format error
	  if (lpArguments && DataBuffer.buf)
	  {
		  FormatString(&DataBuffer, _TEXT("%s"), _TEXT("--- ErrorInfo ---\r\n"));
		  if (tszResult = szErrorInfo = Tcl_IoGetString(lpArguments, 0, 0, 0))
		  {
			  for (;pNewline = strchr(szErrorInfo, '\n');szErrorInfo = &pNewline[1])
			  {
				  FormatString(&DataBuffer, _TEXT("%.*s\r\n"), pNewline - szErrorInfo, szErrorInfo);
			  }
			  if (szErrorInfo[0] != '\0') FormatString(&DataBuffer, _TEXT("%s\r\n"), szErrorInfo);
			  Free(tszResult);
		  }
		  FormatString(&DataBuffer, _TEXT("%s"), _TEXT("----\r\n"));
	  }

	  //  Output error to log
	  Putlog(LOG_SYSTEM, _T("\"%s\" terminated abnormally\r\n%.*s"), tszFileName, DataBuffer.len, DataBuffer.buf);
	  Free(DataBuffer.buf);
  }
  else
  {
	  dwErrorCode = NO_ERROR;
	  if ((szErrorCode = (LPSTR)Tcl_GetVar(lpInterp, "ioerror", 0)))
	  {
		  bReturn  = (atoi(szErrorCode) ? TRUE : FALSE);
		  //  Unset error
	  }
	  else bReturn  = FALSE;

	  if (!bReturn && lpiResult)
	  {
		  if (lpResult = Tcl_GetObjResult(lpInterp))
		  {
			  if (Tcl_GetIntFromObj(lpInterp, lpResult, lpiResult) != TCL_OK)
			  {
				  *lpiResult = 0;
				  bReturn = TRUE;
			  }
		  }
	  }
  }
  Tcl_UnsetVar(lpInterp, "ioerror", 0);

  //  Free resources
  if (TclData.dwFlags & TCL_USERFILE_LOCK) UserFile_Unlock(&TclData.lpUserFile, 0);
  if (TclData.dwFlags & TCL_GROUPFILE_LOCK) GroupFile_Unlock(&TclData.lpGroupFile, 0);
  if (TclData.dwFlags & TCL_USERFILE_OPEN) UserFile_Close(&TclData.lpUserFile, 0);
  if (TclData.dwFlags & TCL_GROUPFILE_OPEN) GroupFile_Close(&TclData.lpGroupFile, 0);
  if (TclData.dwFlags & TCL_MOUNTFILE_OPEN) MountFile_Close(TclData.hMountFile);
  if (TclData.dwFlags & TCL_THEME_CHANGED) SetTheme(TclData.lpTheme);
  if (TclData.dwFlags & TCL_HANDLE_LOCK) ReleaseHandleLock();
  if (TclData.dwWaitObject)
  {
	  EnterCriticalSection(&csTclWaitObjectList);
	  for (n = TclData.dwWaitObject;n--;)
	  {
		  Tcl_CloseWaitObject(TclData.lpWaitObject[n]);
	  }
	  LeaveCriticalSection(&csTclWaitObjectList);
  }

  if (! bKeepInterpreter) Tcl_DeleteInterpreter(lpTclInterpreter);

  SetLastError(dwErrorCode);
  return bReturn;
}


BOOL TclExecute(LPEVENT_DATA lpEventData, IO_STRING *Arguments)
{
	return TclExecute2(lpEventData, Arguments, NULL, NULL);
}


BOOL Tcl_ModuleInit(VOID)
{
	Tcl_Interp  *lpInterp;
	CHAR         szBuffer[_MAX_PATH + 1];
	int          iMajor, iMinor, iPatch;

	//  Initialize tcl library
	lpListType  = NULL;
	lpTclWaitObjectList  = NULL;
	lpGlobalVariables  = NULL;
	dwGlobalVariables  = 0;
	dwGlobalVariablesSize  = 0;
	dwTclUniqueId = 0;
	if (! InitializeCriticalSectionAndSpinCount(&csGlobalVariables, 100)) return TRUE;
	if (! InitializeCriticalSectionAndSpinCount(&csTclWaitObjectList, 100)) return TRUE;

	Tcl_RegisterHandleLockFunctions(AcquireHandleLock, ReleaseHandleLock);
	if (! GetModuleFileNameA(NULL, szBuffer, _MAX_PATH)) return TRUE;
	Tcl_FindExecutable(szBuffer);
	//  Allocate Tls index
	dwTclInterpreterTlsIndex  = TlsAlloc();
	if (dwTclInterpreterTlsIndex == TLS_OUT_OF_INDEXES) return TRUE;

	Tcl_GetVersion(&iMajor, &iMinor, &iPatch, NULL);
	if ((iMajor < IO_TCL_MAJOR_VER) || (iMinor != IO_TCL_MINOR_VER) || (iPatch != IO_TCL_PATCH_VER))
	{
		// this won't ever get printed since it would be queued as a job...
		Putlog(LOG_ERROR, _T("TCL Version mismatch: found %d.%d.%d expected %d.%d.%d\r\n"),
			iMajor, iMinor, iPatch, IO_TCL_MAJOR_VER, IO_TCL_MINOR_VER, IO_TCL_PATCH_VER);
		SetLastError(ERROR_TCL_VERSION);
		return TRUE;
	}

	lpInterp  = Tcl_CreateInterp();
	if (! lpInterp)
	{
		SetLastError(ERROR_TCL_VERSION);
		return TRUE;
	}
	if (Tcl_Init(lpInterp))
	{
		Putlog(LOG_ERROR, _T("TCL Library (/lib dir) error - Need version %d.%d.%d.\r\n"), IO_TCL_MAJOR_VER, IO_TCL_MINOR_VER, IO_TCL_PATCH_VER);
		Tcl_DeleteInterp(lpInterp);
		Tcl_Finalize();
		SetLastError(ERROR_TCL_VERSION);
		return TRUE;
	}

	Tcl_DeleteInterp(lpInterp);
	Tcl_FinalizeThread();

	if (Config_Get_Bool(&IniConfigFile, _T("Threads"), _T("Debug_Tcl_Interpreters"), &bDebugTclInterpreters))
	{
		bDebugTclInterpreters = FALSE;
	}

	return FALSE;
}








VOID Tcl_ModuleDeInit(VOID)
{
	LPTCL_WAITOBJECT  lpWaitObject;
	DWORD n;

	//  Free resources
	TlsFree(dwTclInterpreterTlsIndex);
	DeleteCriticalSection(&csTclWaitObjectList);

	for (;lpWaitObject = lpTclWaitObjectList;)
	{
		lpTclWaitObjectList  = lpTclWaitObjectList->lpNext;
		switch (lpWaitObject->dwType)
		{
		case EVENT:
		case SEMAPHORE:
			CloseHandle((HANDLE)lpWaitObject->lpData);
		}
		Free(lpWaitObject);
	}

	for (n = 0; lTclInterps && (n < 5) ; n++)
	{
		if (n)
		{
			// don't print this the first time through as it's likely to be true...
			Putlog(LOG_DEBUG, "Waiting for %d non-finalized interpreters.\r\n", lTclInterps);
		}
		Sleep(1000);
	}

	Sleep(10);

	if (lTclInterps)
	{
		Putlog(LOG_ERROR, "Wait timeout for non-finalized interpreters: %d\r\n", lTclInterps);
	}
	else
	{
		Tcl_Finalize();
	}
}
