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

#include <windows.h>


// Simple path globbing.. only * and ? allowed
// returns 0 on match, else 1
INT spCompare(LPSTR String1, LPSTR String2)
{
	PCHAR	pStore[2];

	pStore[0]	= NULL;

	while (String2[0])
	{
		switch (String1[0])
		{
		case '\0':
			return -1;

		case '*':
			if ((++String1)[0] == '\0') return 0;
			//	Store new offsets
			pStore[0]	= String1;
			pStore[1]	= String2;
			break;

		case '?':
			String1++;
			String2++;
			break;

		default:
			if (tolower(String1[0]) != tolower(String2[0]))
			{
				//	Get resume offset
				if (! pStore[0]) return String1[0] - String2[0];

				String1	= pStore[0];
				String2	= ++(pStore[1]);
			}
			else
			{
				String1++;
				String2++;
			}
		}
	}

	if (String1[0] != '\0' &&
		(String1[0] != '*' || String1[1] != '\0')) return 1;

	return 0;
}



// returns 0 on match, else 1
INT iCompare(LPSTR String1, LPSTR String2)
{
	PCHAR	pStore[2];
	BOOL	bMatch, bReject, bLoop;

	pStore[0]	= NULL;

	while (String2[0])
	{
		switch (String1[0])
		{
		case '\0':
			return -1;

		case '*':
			if ((++String1)[0] == '\0') return 0;
			//	Store new offsets
			pStore[0]	= String1;
			pStore[1]	= String2;
			break;

		case '?':
			String1++;
			String2++;
			break;

		case '[':
			bMatch	= FALSE;
			bReject	= FALSE;
			bLoop   = TRUE;

			while (bLoop)
			{
				switch ((++String1)[0])
				{
				case '\0':
					//	Broken comparison set
					return 1;
				case ']':
					//	No match
					bLoop = FALSE;
					String1++;
					break;
				case '^':
					// toggles matching / not matching
					bReject = TRUE;
					continue;
				case '\\':
					//	Sanity check
					if (String1[1] != '\0') String1++;
				default:
					if (String1[1] == '-' && String1[-1] != '\\')
					{
						//	Range 'x0' - 'x1'
						if (tolower(String2[0]) >= tolower(String1[0]) &&
							tolower(String2[0]) <= tolower(String1[2]))
						{
							String1	+= 2;
							bMatch = TRUE;
						}
					}
					else if (tolower(String1[0]) == tolower(String2[0]))
					{
						bMatch = TRUE;
					}
				}
			}

			// rejecting and found a match, or not rejecting and didn't find a match
			if ((bReject && bMatch) || (!bReject && !bMatch))
			{
				//	Get resume offset
				if (! pStore[0]) return 1;

				String1	= pStore[0];
				String2	= ++(pStore[1]);
			}
			else
			{
				String2++;
			}
			break;

		default:
			if (tolower(String1[0]) != tolower(String2[0]))
			{
				//	Get resume offset
				if (! pStore[0]) return String1[0] - String2[0];

				String1	= pStore[0];
				String2	= ++(pStore[1]);
			}
			else
			{
				String1++;
				String2++;
			}
		}
	}

	if (String1[0] != '\0' &&
		(String1[0] != '*' || String1[1] != '\0')) return 1;

	return 0;
}




INT PathCompare(LPSTR String1, LPSTR String2)
{
	PCHAR	pStore[2];
	BOOL	bMatch, bReject, bLoop;

	pStore[0]	= NULL;

	while (String2[0])
	{
		switch (String1[0])
		{
		case '\0':
			return -1;

		case '*':
			if ((++String1)[0] == '\0') return 0;
			//	Store new offsets
			pStore[0]	= String1;
			pStore[1]	= String2;
			break;

		case '?':
			String1++;
			String2++;
			break;

		case '[':
			bMatch	= FALSE;
			bReject	= FALSE;
			bLoop   = TRUE;

			while (bLoop)
			{
				switch ((++String1)[0])
				{
				case '\0':
					//	Broken comparison set
					return 1;
				case ']':
					//	No match
					bLoop = FALSE;
					String1++;
					break;
				case '^':
					// toggles matching / not matching
					bReject = TRUE;
					continue;
				case '\\':
					//	Sanity check
					if (String1[1] != '\0') String1++;
				default:
					if (String1[1] == '-' && String1[-1] != '\\')
					{
						//	Range 'x0' - 'x1'
						if (tolower(String2[0]) >= tolower(String1[0]) &&
							tolower(String2[0]) <= tolower(String1[2]))
						{
							String1	+= 2;
							bMatch = TRUE;
						}
					}
					else if (tolower(String1[0]) == tolower(String2[0]))
					{
						bMatch = TRUE;
					}
				}
			}

			// rejecting and found a match, or not rejecting and didn't find a match
			if ((bReject && bMatch) || (!bReject && !bMatch))
			{
				//	Get resume offset
				if (! pStore[0]) return 1;

				String1	= pStore[0];
				String2	= ++(pStore[1]);
			}
			else
			{
				String2++;
			}
			break;

		case '\\':
			if (String1[0] != '\0') String1++;
		default:
			if (tolower(String1[0]) != tolower(String2[0]))
			{
				//	Get resume offset
				if (! pStore[0]) return String1[0] - String2[0];

				String1	= pStore[0];
				String2	= ++(pStore[1]);
			}
			else
			{
				String1++;
				String2++;
			}
		}
	}

	if (String1[0] == '/') String1++;
	if (String1[0] != '\0' &&
		(String1[0] != '*' || String1[1] != '\0')) return 1;

	return 0;
}
