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


INT64 tcstoi64(LPTSTR tszString, TCHAR **tpOffset, DWORD dwBase)
{
  INT64  lResult;
  DWORD  dwValid;
  BOOL   bNegative;

  dwValid  = 0;
  lResult  = 0;
  if(tszString[0]==_TEXT('-')) {
    tszString++;
    bNegative  = TRUE;
  } else {
    bNegative  = FALSE;
  }

  for(;;dwValid++) {
    if(!_istdigit(tszString[0])) {
      if(tpOffset)
        tpOffset[0]=(bNegative && ! dwValid ? &tszString[-1] : tszString);
      return (bNegative ? lResult * -1 : lResult);
    }
    lResult  *= dwBase;
    lResult  += tszString++[0] - _TEXT('0');
  }
}

VOID
DataRow_ParseBuffer(PCHAR pBuffer,
                    DWORD dwBufferSize,
                    LPVOID lpStorage,
                    LPDATAROW lpDataRowArray,
                    DWORD dwDataRowArray)
{
  LPDATAROW  lpDataRow;
  PCHAR      pNewline, pBufferEnd, pField, pSpace, pCheck;
  LPVOID     lpBuffer;
  LPSTR      szData;
  DWORD      dwData, dwField, dwItem;
  CHAR       pHexData[3];
  INT64      i64;
  INT32      i32;
  UINT8      u8;
  DWORD      i, j;
  INT        n;


  //  Get pointer to end of read buffer
  pBufferEnd  = &pBuffer[dwBufferSize];
  pField    = pBuffer;

  for (;pNewline = (PCHAR)memchr(pField, '\n', pBufferEnd -  pField);pField = &pNewline[1])
  {
    //  Find first field
    if (! (pSpace = (PCHAR)memchr(pField, ' ', pNewline - pField)))
	{
		if (pField == pNewline || ( pField == pNewline-1 && pNewline[-1] == '\r'))
		{
			// it was an empty line, will happen on last line
			continue;
		}
		// it could be a line with just a field name like "admingroups" but no data...
		dwField  = (pNewline[-1] == '\r' ? &pNewline[-1] : pNewline) - pField;
		pField[dwField] = 0;

		for (n = dwDataRowArray;n--;)
		{
			//  Get pointer to userfield
			lpDataRow  = &lpDataRowArray[n];
			//  Compare field names
			if (memicmp(pField, lpDataRow->szName, dwField)) continue;
			if (lpDataRow->szName[dwField] == 0)
			{
				//  Known line
				break;
			}
		}
		//  Only process known lines
		if (n < 0)
		{
			Putlog(LOG_ERROR, _T("Unknown field name on line '%s'.\r\n"), pField);
		}
		continue;
	}
    //  Calculate length of name and data
    dwField  = pSpace - pField;
    szData  = &pSpace[1];
    dwData  = (pNewline[-1] == '\r' ? &pNewline[-1] : pNewline) - szData;
    //  Skip zero length
    if (dwData <= 0 ||
      dwField <= 0)
	{
		// need to terminate the line
		szData[dwData] = 0;
		Putlog(LOG_ERROR, _T("Zero length field or data on line '%s'.\r\n"), pField);
		continue;
	}
    //  Terminate with zeros
    szData[dwData]  = 0;
	pSpace[0] = 0;

    for (n = dwDataRowArray;n--;)
    {
      //  Get pointer to userfield
      lpDataRow  = &lpDataRowArray[n];
      //  Compare field names
      if (memicmp(pField, lpDataRow->szName, dwField)) continue;
	  if (lpDataRow->szName[dwField] == 0)
	  {
		  //  Known line
		  break;
	  }
    }
    //  Only process known lines
    if (n < 0)
	{
		Putlog(LOG_ERROR, _T("Unknown field name '%s'.\r\n"), pField);
		continue;
	}
    //  Get buffer offset
    lpBuffer  = (LPVOID)((ULONG)lpStorage + lpDataRow->dwOffset);

    switch (lpDataRow->dwType)
    {
    case DT_STRING:
      if (lpDataRow->dwMaxArgs == 1)
      {
        //  Copy single string
        if (dwData > lpDataRow->dwMaxLength) dwData  = lpDataRow->dwMaxLength;
        CopyMemory(lpBuffer, szData, dwData + 1);
        break;
      }

      for (i = 0U;i < lpDataRow->dwMaxArgs;i++)
      {
        //  Find next string
        if (! (pCheck = (LPSTR)memchr(szData, ' ', dwData)))
        {
          //  End of string
          pCheck  = &szData[dwData];
        }
        //  Determinate length for item
        if ((dwItem = pCheck - szData) > lpDataRow->dwMaxLength) dwItem  = lpDataRow->dwMaxLength;
        //  Copy string
        CopyMemory(lpBuffer, szData, dwItem);
        ((LPSTR)lpBuffer)[dwItem]  = '\0';
        //  Check for end of string
        if (pCheck == &szData[dwData]) break;
        //  Move buffer to next entry
        lpBuffer  = (LPVOID)((ULONG)lpBuffer + lpDataRow->dwMaxLength + 1);
        //  Reduce length of available data
        dwData  -= &pCheck[1] - szData;
        szData  = &pCheck[1];
      }
      break;
    case DT_GROUPID:
      for (i = j = 0U;j < lpDataRow->dwMaxArgs;i++)
      {
        i32  = strtol(szData, &pCheck, 10);
        if (! pCheck || pCheck == szData) break;
        if (i32 >= 0 && i32 < MAX_GID && Gid2Group(i32))
        {
          ((PINT32)lpBuffer)[0]  = i32;
          lpBuffer  = (LPVOID)((ULONG)lpBuffer + sizeof(INT32));
          j++;
        }
        if (pCheck[0] == '\0') break;
        szData  = &pCheck[1];
      }
      if (j < lpDataRow->dwMaxArgs) ((PINT)lpBuffer)[0]  = -1;
      break;
    case DT_INT32:
      for (i = 0U;i < lpDataRow->dwMaxArgs;i++)
      {
        i32  = strtol(szData, &pCheck, 10);
        if (! pCheck || pCheck == szData) break;
        ((PINT32)lpBuffer)[0]  = i32;
        if (pCheck[0] == '\0') break;
        szData  = &pCheck[1];
        lpBuffer  = (LPVOID)((ULONG)lpBuffer + sizeof(INT32));
      }
      break;
    case DT_INT64:
      for (i = 0U;i < lpDataRow->dwMaxArgs;i++)
      {
        i64  = tcstoi64(szData, &pCheck, 10);
        if (! pCheck || pCheck == szData) break;
        ((PINT64)lpBuffer)[0]  = i64;
        if (pCheck[0] == '\0') break;
        //  Reduce length
        szData  = &pCheck[1];
        lpBuffer  = (LPVOID)((ULONG)lpBuffer + sizeof(INT64));
      }
      break;
    case DT_PASSWORD:
      //  Password hash
      ZeroMemory(pHexData, sizeof(pHexData));

      for (i = 0U;i < lpDataRow->dwMaxLength && dwData >= 2;i++)
      {
        //  Copy to work buffer
        pHexData[0]  = szData[i << 1];
        pHexData[1]  = szData[(i << 1) + 1];
        //  Decrease length
        dwData  -= 2;
        //  Convert hex to dec
        u8  = (UINT8)strtoul(pHexData, &pCheck, 16);
        //  Sanity check
        if (pCheck != &pHexData[2]) break;
        //  Store to memory
        ((PUINT8)lpBuffer)[i]  = u8;
      }
      break;
    }
  }
}


VOID
DataRow_Dump(LPBUFFER lpOutBuffer,
             LPVOID lpStorage,
             LPDATAROW lpDataRowArray,
             DWORD dwDataRowArray)
{
  LPDATAROW  lpDataRow;
  LPVOID    lpBuffer;
  INT64    i64;
  INT32    i32;
  UINT     i;

  //  Copy data to output buffer
  for (;dwDataRowArray--;)
  {
    lpDataRow  = &lpDataRowArray[dwDataRowArray];
    //  Get offset where-to read data
    lpBuffer  = (LPVOID)((ULONG)lpStorage + lpDataRow->dwOffset);

    switch (lpDataRow->dwType)
    {
    case DT_STRING:
      if (((LPSTR)lpBuffer)[0] == '\0') break;
      FormatString(lpOutBuffer, _TEXT("%s"), lpDataRow->szName);
      for (i = 0U;i < lpDataRow->dwMaxArgs;i++)
      {
        if (((LPSTR)lpBuffer)[0] == '\0') break;
        FormatString(lpOutBuffer, _TEXT(" %s"),  (LPSTR)lpBuffer);
        lpBuffer  = (LPVOID)((ULONG)lpBuffer + lpDataRow->dwMaxLength + 1);
      }
      Put_Buffer(lpOutBuffer, "\r\n", 2);
      break;
    case DT_GROUPID:
      FormatString(lpOutBuffer, _TEXT("%s"), lpDataRow->szName);
      for (i = 0U;i < lpDataRow->dwMaxArgs;i++)
      {
        i32  = ((PINT32)lpBuffer)[0];
        if (i32 < 0) break;
        FormatString(lpOutBuffer, _TEXT(" %i"), i32);
        lpBuffer  = (LPVOID)((ULONG)lpBuffer + sizeof(INT32));
      }
      Put_Buffer(lpOutBuffer, "\r\n", 2);
      break;
    case DT_INT32:
      FormatString(lpOutBuffer, _TEXT("%s"), lpDataRow->szName);
      for (i = 0U;i < lpDataRow->dwMaxArgs;i++)
      {
        i32  = ((PINT32)lpBuffer)[0];
        FormatString(lpOutBuffer, _TEXT(" %i"), i32);
        lpBuffer  = (LPVOID)((ULONG)lpBuffer + sizeof(INT32));
      }
      Put_Buffer(lpOutBuffer, "\r\n", 2);
      break;
    case DT_INT64:
      FormatString(lpOutBuffer, _TEXT("%s"), lpDataRow->szName);
      for (i = 0U;i < lpDataRow->dwMaxArgs;i++)
      {
        i64  = ((PINT64)lpBuffer)[0];
        FormatString(lpOutBuffer, _TEXT(" %I64i"), i64);
        lpBuffer  = (LPVOID)((ULONG)lpBuffer + sizeof(INT64));
      }
      Put_Buffer(lpOutBuffer, "\r\n", 2);
      break;
    case DT_PASSWORD:
      //  Password hash
      FormatString(lpOutBuffer, _TEXT("%s "), lpDataRow->szName);
      for (i = 0U;i < lpDataRow->dwMaxLength;i++)
      {
        //  Store to memory
        i32  = (INT32)((PUINT8)lpBuffer)[i];
        //  Convert dec to hex
        FormatString(lpOutBuffer, _TEXT("%02x"), i32);
      }
      Put_Buffer(lpOutBuffer, "\r\n", 2);
      break;
    }
  }
}
