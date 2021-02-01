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

// return 0 if item not already existing and it added to array, else index (from 1) of the
// existing matching item.
INT QuickInsert(LPVOID *List, INT Items, LPVOID Item, QUICKCOMPAREPROC Comp)
{
	register ULONG	Best, Left, Shift;
	register LPVOID	lpItem;
	register INT	i;

	// Handle adding an item to the end of the list as a special case because
	// on NTFS the FindNextFile() system call returns items already in
	// alphabetical order although not separated via dir/file, it
	// is still a big savings.
	if (!Items || Comp(&Item, &List[Items-1]) > 0)
	{
		List[Items]	= Item;
		return 0;
	}

	Best	= 0;
	lpItem	= &Item;

	for (Left = Shift = Items;Shift;Left -= Shift)
	{
		Shift	= Left >> 1;

		if ((i = Comp(lpItem, &List[Shift + Best])) > 0)
		{
			//	Higher
			Best	+= (Shift ? Shift : 1);
		}
		else if (! i) return Shift + Best + 1;
	}
	//	Cycle memory
	if ((Items -= Best) > 0)
	{
		MoveMemory(&List[Best + 1], &List[Best], Items * sizeof(LPVOID));
	}

	List[Best]	= Item;

	return 0;
}


// return index (from 1) of an already existing matching item in list, or else
// a negative number whose absolute value indicates the position the new item was
// inserted into.
INT QuickInsert2(register LPVOID *List, INT Items, LPVOID Item, QUICKCOMPAREPROC Comp)
{
	register INT	Best, Left, Shift;
	register LPVOID	lpItem;
	register INT	i;

	// Handle adding an item to the end of the list as a special case because
	// on NTFS the FindNextFile() system call returns items already in
	// alphabetical order although not separated via dir/file, it
	// is still a big savings.
	if (!Items || Comp(&Item, &List[Items-1]) > 0)
	{
		List[Items]	= Item;
		return -Items - 1;
	}

	Best	= 0;
	lpItem	= &Item;

	for (Left = Shift = Items;Shift;Left -= Shift)
	{
		Shift	= Left >> 1;

		if ((i = Comp(lpItem, &List[Shift + Best])) > 0)
		{
			//	Higher
			Best	+= (Shift ? Shift : 1);
		}
		else if (! i) return Shift + Best + 1;
	}
	//	Cycle memory
	if ((Items -= Best) > 0)
	{
		MoveMemory(&List[Best + 1], &List[Best], Items * sizeof(LPVOID));
	}

	List[Best]	= Item;

	return -Best-1;
}


// return 0 if no item found, else index (from 1) of the existing matching item.
INT QuickFind(register LPVOID *List, INT Items, LPVOID Item, QUICKCOMPAREPROC Comp)
{
	register ULONG	Best, Left, Shift;
	register LPVOID	lpItem;
	register INT	i;

	// Handle adding an item to the end of the list as a special case because
	// on NTFS the FindNextFile() system call returns items already in
	// alphabetical order although not separated via dir/file, it
	// is still a big savings.
	if (!Items || Comp(&Item, &List[Items-1]) > 0)
	{
		return 0;
	}

	Best	= 0;
	lpItem	= &Item;

	for (Left = Shift = Items;Shift;Left -= Shift)
	{
		Shift	= Left >> 1;

		if ((i = Comp(lpItem, &List[Shift + Best])) > 0)
		{
			//	Higher
			Best	+= (Shift ? Shift : 1);
		}
		else if (! i) return Shift + Best + 1;
	}
	return 0;
}





LPVOID QuickDelete(LPVOID *List, INT Items, LPVOID Item, QUICKCOMPAREPROC Comp, QUICKCHECKPROC Check)
{
	register LPVOID	*Result, Return;
	register INT	iSize;


	Result	= (LPVOID *)bsearch(&Item, List, Items, sizeof(LPVOID), Comp);

	if (Result)
	{
		Return	= Result[0];

		if (! Check || ! Check(Return))
		{
			iSize	= &List[Items] - &Result[1];
			MoveMemory(&Result[0], &Result[1], iSize * sizeof(LPVOID));
		}
		return Return;
	}
	return NULL;
}


// dwPos is from 1
LPVOID QuickDeleteIndex(LPVOID *List, INT Items, INT dwPos)
{
	LPVOID *Result, Return;
	int iSize;

	if (dwPos > Items)
	{
		return NULL;
	}
	Result = &List[dwPos-1];
	Return = Result[0];

	iSize	= &List[Items] - &Result[1];
	MoveMemory(&Result[0], &Result[1], iSize * sizeof(LPVOID));
	return Return;
}
