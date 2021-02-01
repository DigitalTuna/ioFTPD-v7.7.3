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

#include <Windows.h>
#include <TChar.h>
#include <IoString.h>
#include <IoMemory.h>




BOOL SplitString(LPTSTR tszStringIn, LPIO_STRING lpStringOut)
{
	TCHAR	*tpQuote, *tpLast, *tpSeeker;
	LPVOID	lpMemory;
	DWORD	dwStringIn, dwMarks, dwMarksAllocated;

	dwStringIn	= _tcslen(tszStringIn) + 1;
	if (dwStringIn == 1) return TRUE;

	dwMarks				= 0;
	dwMarksAllocated	= 32;

	if (! (lpStringOut->pMarks = (TCHAR **)Allocate("SplitString:Marks", dwMarksAllocated * sizeof(TCHAR *)))) return TRUE;
	if (! (lpMemory = Allocate("SplitString:Buffer", (dwStringIn * sizeof(TCHAR)) * 2))) return TRUE;

	lpStringOut->dwShift	= 0;
	lpStringOut->pString	= (LPTSTR)lpMemory;
	lpStringOut->pBuffer	= &((LPTSTR)lpMemory)[dwStringIn];

	CopyMemory(lpStringOut->pString, tszStringIn, dwStringIn);

	for (tpSeeker = tpLast = lpStringOut->pString;;tpSeeker++)
	{
		switch (tpSeeker[0])
		{
		case _TEXT('\\'):
			if (tpSeeker[1] != _TEXT('\0')) tpSeeker++;
			break;

		case _TEXT('"'):
			if ((tpQuote = _tcschr(&tpSeeker[1], _TEXT('"')))) tpSeeker	= tpQuote;
			break;

		case _TEXT(' '):
		case _TEXT('\0'):
			if (tpSeeker != tpLast)
			{
				if ((dwMarks + 1) >= dwMarksAllocated)
				{
					dwMarksAllocated	*= 2;

					if (! (lpMemory = ReAllocate(lpStringOut->pMarks, NULL, dwMarksAllocated * sizeof(TCHAR *))))
					{
						Free(lpStringOut->pString);
						Free(lpStringOut->pMarks);
						return TRUE;
					}
					lpStringOut->pMarks	= (TCHAR **)lpMemory;
				}

				lpStringOut->pMarks[dwMarks++]	= tpLast;
				lpStringOut->pMarks[dwMarks++]	= tpSeeker;

				if (tpSeeker[0] == _TEXT('\0'))
				{
					lpStringOut->dwMarks	= dwMarks >> 1;
					return (dwMarks ? FALSE : TRUE);
				}
				tpSeeker[0]	= _TEXT('\0');
			}
			else if (tpSeeker[0] == _TEXT('\0'))
			{
				lpStringOut->dwMarks	= dwMarks >> 1;
				return (dwMarks ? FALSE : TRUE);
			}
			tpLast	= &tpSeeker[1];
			break;
		}
	}
}





BOOL ConcatString(LPIO_STRING lpDestinationString, LPIO_STRING lpSourceString)
{
	TCHAR	*tpBuffer, *tpString, **tpMarks, **tpOldMarks, *tpOldString;
	DWORD	dwStringMarks[2], dwStringLength[2];
	DWORD	i;

	dwStringMarks[0]	= lpDestinationString->dwMarks << 1;
	dwStringMarks[1]	= lpSourceString->dwMarks << 1;
	//	Calculate string lengths
	dwStringLength[0]	= (dwStringMarks[0] ?
		(ULONG)lpDestinationString->pMarks[dwStringMarks[0] - 1] - (ULONG)lpDestinationString->pMarks[0] + 1 : 0);
	dwStringLength[1]	= (dwStringMarks[1] ?
		(ULONG)lpSourceString->pMarks[dwStringMarks[1] - 1] - (ULONG)lpSourceString->pMarks[0] + 1 : 0);

	if (! (tpString = (TCHAR *)Allocate("ConcatString:Buffer", (dwStringLength[0] + dwStringLength[1]) * 2))) return TRUE;
	if (! (tpMarks = (TCHAR **)Allocate("ConcatString:Marks", (dwStringMarks[0] + dwStringMarks[1]) * sizeof(TCHAR *))))
	{
		//	Out of memory
		Free(tpString);
		return TRUE;
	}

	tpOldMarks	= lpDestinationString->pMarks;
	tpOldString	= lpDestinationString->pString;
	tpBuffer	= &tpString[dwStringLength[0] + dwStringLength[1]];
	//	Update destination string's contents
	lpDestinationString->dwMarks	+= (dwStringMarks[1] >> 1);
	lpDestinationString->pMarks		= tpMarks;
	lpDestinationString->pBuffer	= tpBuffer;
	lpDestinationString->pString	= tpString;

	CopyMemory(tpString, tpOldMarks[0], dwStringLength[0]);
	tpString	= (TCHAR *)((ULONG)tpString - (ULONG)tpOldMarks[0]);

	for (i = 0 ;i < dwStringMarks[0];i++)
	{
		(tpMarks++)[0]	= (TCHAR *)((ULONG)tpString + (ULONG)tpOldMarks[i]);
	}

	Free(tpOldString);
	Free(tpOldMarks);
	tpOldMarks	= lpSourceString->pMarks;

	if (dwStringLength[1])
	{
		CopyMemory(&lpDestinationString->pString[dwStringLength[0] / sizeof(TCHAR)], tpOldMarks[0], dwStringLength[1]);
		tpString	= (TCHAR *)((ULONG)lpDestinationString->pString + dwStringLength[0] - (ULONG)tpOldMarks[0]);
	}

	for (i = 0;i < dwStringMarks[1];i++)
	{
		(tpMarks++)[0]	= (TCHAR *)((ULONG)tpString + (ULONG)tpOldMarks[i]);
	}

	return FALSE;
}



BOOL AppendArgToString(LPIO_STRING lpStringOut, LPTSTR tszIn)
{
	TCHAR	*tpString, **tpMarks, **tpOldMarks, *tpOldString;
	DWORD	dwLen, dwStringMarks, dwStringLength;
	DWORD	i;

	dwLen = _tcslen(tszIn);
	if (!dwLen) return FALSE;

	dwStringMarks = lpStringOut->dwMarks << 1;
	dwStringLength = (dwStringMarks ? (ULONG)lpStringOut->pMarks[dwStringMarks - 1] - (ULONG)lpStringOut->pMarks[0] + 1 : 0);

	if (! (tpString = (TCHAR *)Allocate("ConcatString:Buffer", (dwLen + dwStringLength + 1) * 2))) return TRUE;
	if (! (tpMarks = (TCHAR **)Allocate("ConcatString:Marks", (dwStringMarks+2) * sizeof(TCHAR *))))
	{
		//	Out of memory
		Free(tpString);
		return TRUE;
	}

	tpOldString	= lpStringOut->pString;
	tpOldMarks  = lpStringOut->pMarks;

	lpStringOut->pString = tpString;
	lpStringOut->pMarks  = tpMarks;
	lpStringOut->pBuffer = &tpString[dwStringLength + dwLen];
	lpStringOut->dwMarks++;

	CopyMemory(tpString, tpOldMarks[0], dwStringLength);
	CopyMemory(tpString + dwStringLength, tszIn, dwLen+1);

	tpString = (TCHAR *)((ULONG)tpString - (ULONG)tpOldMarks[0]);

	for (i = 0 ;i < dwStringMarks;i++)
	{
		(tpMarks++)[0]	= (TCHAR *)((ULONG)tpString + (ULONG)tpOldMarks[i]);
	}

	Free(tpOldString);
	Free(tpOldMarks);

	*tpMarks++	= &lpStringOut->pString[dwStringLength];
	*tpMarks    = tpMarks[-1] + dwLen;

	return FALSE;
}


BOOL AppendQuotedArgToString(LPIO_STRING lpStringOut, LPTSTR tszIn)
{
	TCHAR	*tpString, **tpMarks, **tpOldMarks, *tpOldString;
	DWORD	dwLen, dwStringMarks, dwStringLength;
	DWORD	i;

	dwLen = _tcslen(tszIn);

	dwStringMarks = lpStringOut->dwMarks << 1;
	dwStringLength = (dwStringMarks ? (ULONG)lpStringOut->pMarks[dwStringMarks - 1] - (ULONG)lpStringOut->pMarks[0] + 1 : 0);

	if (! (tpString = (TCHAR *)Allocate("ConcatString:Buffer", (dwLen + dwStringLength + 3) * 2))) return TRUE;
	if (! (tpMarks = (TCHAR **)Allocate("ConcatString:Marks", (dwStringMarks+2) * sizeof(TCHAR *))))
	{
		//	Out of memory
		Free(tpString);
		return TRUE;
	}

	tpOldString	= lpStringOut->pString;
	tpOldMarks  = lpStringOut->pMarks;

	lpStringOut->pString = tpString;
	lpStringOut->pMarks  = tpMarks;
	lpStringOut->pBuffer = &tpString[dwStringLength + dwLen + 2];
	lpStringOut->dwMarks++;

	CopyMemory(tpString, tpOldMarks[0], dwStringLength);
	tpString[dwStringLength] = '"';
	CopyMemory(tpString + dwStringLength + 1, tszIn, dwLen);
	tpString[dwStringLength+dwLen+1] = '"';
	tpString[dwStringLength+dwLen+2] = 0;

	tpString = (TCHAR *)((ULONG)tpString - (ULONG)tpOldMarks[0]);

	for (i = 0 ;i < dwStringMarks;i++)
	{
		(tpMarks++)[0]	= (TCHAR *)((ULONG)tpString + (ULONG)tpOldMarks[i]);
	}

	Free(tpOldString);
	Free(tpOldMarks);

	*tpMarks++	= &lpStringOut->pString[dwStringLength];
	*tpMarks    = tpMarks[-1] + dwLen + 2;

	return FALSE;
}




BOOL PushString(LPIO_STRING lpString, DWORD dwShift)
{
	//	Sanity check
	if (lpString->dwMarks < dwShift) return TRUE;
	//	Shift to right
	lpString->dwShift	+= dwShift;
	lpString->dwMarks	-= dwShift;
	lpString->pMarks	= &lpString->pMarks[dwShift << 1];

	return FALSE;
}



VOID PullString(LPIO_STRING lpString, DWORD dwShift)
{
	//	Shift to left
	dwShift	= min(lpString->dwShift, dwShift);
	//	Shift to left
	lpString->dwShift	-= dwShift;
	lpString->dwMarks	+= dwShift;
	lpString->pMarks	= (TCHAR **)((ULONG)lpString->pMarks - ((dwShift << 1) * sizeof(TCHAR *)));

}





VOID FreeString(LPIO_STRING lpString)
{
	lpString->pMarks	= (PCHAR *)((ULONG)lpString->pMarks - (lpString->dwShift * 2 * sizeof(PCHAR)));
	//	Free memory
	Free(lpString->pString);
	Free(lpString->pMarks);
}






LPTSTR GetStringRange(LPIO_STRING lpString, DWORD dwBeginIndex, DWORD dwEndIndex)
{
	DWORD	dwBuffer;
	TCHAR	*tpBuffer;

	if (dwEndIndex == STR_END)
	{
		dwEndIndex = lpString->dwMarks - 1;
		if (dwBeginIndex >= lpString->dwMarks) return NULL;
	}
	else
	{
		if (dwBeginIndex > dwEndIndex ||
			dwEndIndex >= lpString->dwMarks) return NULL;
	}

	//	Calculate string length
	dwBuffer	= lpString->pMarks[(dwEndIndex << 1) + 1] - lpString->pMarks[dwBeginIndex << 1];
	tpBuffer	= (TCHAR *)((ULONG)lpString->pBuffer - (ULONG)lpString->pMarks[dwBeginIndex << 1]);
	//	Apply zero padding
	lpString->pBuffer[dwBuffer]	= _TEXT('\0');
	//	Copy buffer
	CopyMemory(lpString->pBuffer, lpString->pMarks[dwBeginIndex << 1], dwBuffer * sizeof(TCHAR));

	for (;dwEndIndex > dwBeginIndex;dwEndIndex--)
	{
		((TCHAR *)((ULONG)tpBuffer + (ULONG)lpString->pMarks[(dwEndIndex << 1) - 1]))[0]	= _TEXT(' ');
	}
	return lpString->pBuffer;
}




LPTSTR GetStringIndexStatic(LPIO_STRING lpString, DWORD dwIndex)
{
	return (dwIndex >= lpString->dwMarks ? NULL : lpString->pMarks[dwIndex << 1]);
}




LPSTR GetStringIndex(LPIO_STRING lpString, DWORD dwIndex)
{
	return (dwIndex == STR_ALL ?
		GetStringRange(lpString, STR_BEGIN, STR_END) : GetStringRange(lpString, dwIndex, dwIndex));
}
