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


BOOL HasFlag(LPUSERFILE lpUserFile, LPSTR szFlagList)
{
	if (! lpUserFile ||	strpbrk(lpUserFile->Flags, szFlagList))
	{
		return FALSE;
	}
	//	Set error
	SetLastError(IO_NO_ACCESS);
	return TRUE;
}




/*

  HavePermission() checks whether user has permission to use command
  against the access list located in Offset

	'!' = Deny access
	'=' = Group
	'-' = User
	'*' = Any
	Other characters are custom flags


*/
INT HavePermission(LPUSERFILE lpUserFile, LPSTR szAccessList)
{
	LPSTR		szUserName;

	//	Cannot compare NULL
	if (! szAccessList || ! lpUserFile)
	{
		return FALSE;
	}

	szUserName	= Uid2User(lpUserFile->Uid);

	return CheckPermissions(szUserName, lpUserFile->Groups, lpUserFile->Flags, szAccessList);
}


INT CheckPermissions(LPSTR szUserName, PINT32 lpGroups,
					 LPSTR szUserFlags, LPSTR szAccessList)
{
	LPSTR		szGroupName;
	PCHAR		pOffset, pNewOffset;
	BOOL		bDeny, bMatch, bLoop;
	DWORD		dwLength, dwUserName, dwGroupName;
	INT			i;

	pOffset		= szAccessList;
	bLoop		= TRUE;
	bMatch		= FALSE;
	dwUserName  = 0;
	
	for (;bLoop;)
	{
		//	Search for next blank
		if (! (pNewOffset = (LPSTR)strchr(pOffset, ' ')))
		{
			//	End of string, no more looping
			bLoop		= FALSE;
			pNewOffset	= &pOffset[strlen(pOffset)];
		}

		if (pOffset[0] == '!')
		{
			//	Reject matching user
			bDeny	= TRUE;
			pOffset++;
		}
		else
		{
			//	Accept matching user
			bDeny	= FALSE;
		}

		switch (pOffset[0])
		{
		case '*':
			//	Any (always matches)
			bMatch	= TRUE;
			break;

		case '=':
			//	Group
			dwLength	= pNewOffset - ++pOffset;

			for (i = 0;i < MAX_GROUPS && lpGroups[i] != -1;i++)
			{
				if (szGroupName = Gid2Group(lpGroups[i]))
				{
					dwGroupName = strlen(szGroupName);
					// changed from !memcmp(pOffset, szGroupName, dwLength) && szGroupName[dwLength] == 0
					// to make purify happy
					if (dwGroupName == dwLength && ! memcmp(pOffset, szGroupName, dwLength))
					{
						bMatch	= TRUE;
						break;
					}
				}
			}
			break;

		case '-':
			//	User
			dwLength	= pNewOffset - ++pOffset;
			if (!dwUserName)
			{
				if (!szUserName) break;
				dwUserName = strlen(szUserName);
			}

			if (dwUserName == dwLength && ! memcmp(szUserName, pOffset, dwLength))
			{
				bMatch	= TRUE;
			}
			break;

		default:
			dwLength	= pNewOffset - pOffset;
			i			= strlen(szUserFlags);

			for (;dwLength--;)
			{
				if (memchr(szUserFlags, (pOffset++)[0], i))
				{
					bMatch	= TRUE;
					break;
				}
			}
			break; 
		}

		if (bMatch)
		{
			//	Rule matches
			if (bDeny) break;
			return FALSE;
		}
		pOffset	= &pNewOffset[1];
	}

	SetLastError(IO_NO_ACCESS);
	return -1;
}
