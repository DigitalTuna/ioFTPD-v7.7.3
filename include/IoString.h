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

typedef struct _IO_STRING
{
	TCHAR		*pBuffer;
	TCHAR		*pString;
	TCHAR		**pMarks;
	
	DWORD	dwMarks;
	DWORD	dwShift;

} IO_STRING, * LPIO_STRING;


#define ARG_NOERROR	0
#define ARG_IGNORE	(DWORD)-1


#define STR_BEGIN	0
#define STR_END		(DWORD)-1
#define STR_ALL		(DWORD)-1

#define GetStringItems(x)			((x)->dwMarks)
#define GetStringIndexLength(x, y)	((x)->pMarks[(y << 1) + 1] - (x)->pMarks[y << 1])


VOID FreeString(LPIO_STRING lpString);
LPTSTR GetStringRange(LPIO_STRING lpString, DWORD dwBeginIndex, DWORD dwEndIndex);
LPTSTR GetStringIndexStatic(LPIO_STRING lpString, DWORD dwIndex);
LPTSTR GetStringIndex(LPIO_STRING lpString, DWORD dwIndex);
VOID PullString(LPIO_STRING lpString, DWORD dwShift);
BOOL PushString(LPIO_STRING lpString, DWORD dwShift);
BOOL ConcatString(LPIO_STRING lpDestinationString, LPIO_STRING lpSourceString);
BOOL SplitString(LPTSTR tszStringIn, LPIO_STRING lpStringOut);
BOOL AppendArgToString(LPIO_STRING lpStringOut, LPTSTR tszIn);
BOOL AppendQuotedArgToString(LPIO_STRING lpStringOut, LPTSTR tszIn);
