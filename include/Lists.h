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

#define DELETELIST(lpItem, lpList)				\
	if (lpItem->lpPrev)					\
	{							\
		lpItem->lpPrev->lpNext	= lpItem->lpNext;	\
	}							\
	else lpList[HEAD]	= lpItem->lpNext;		\
	if (lpItem->lpNext)					\
	{							\
		lpItem->lpNext->lpPrev	= lpItem->lpPrev;	\
	}							\
	else lpList[TAIL]	= lpItem->lpPrev;

#define	APPENDLIST(lpItem, lpList)				\
	if (! lpList[HEAD])					\
	{							\
		lpList[HEAD]	= lpItem;			\
		lpList[TAIL]	= NULL;				\
	}							\
	else lpList[TAIL]->lpNext	= lpItem;		\
	lpItem->lpPrev	= lpList[TAIL];				\
	lpList[TAIL]	= lpItem;				\
	lpItem->lpNext	= NULL;

#define	INSERTLIST(lpItem, lpList)				\
	if (! (lpItem->lpNext = lpList[HEAD]))			\
	{							\
		lpList[TAIL]	= lpItem;			\
	}							\
	else lpList[HEAD]->lpPrev	= lpItem;		\
	lpList[HEAD]	= lpItem;				\
	lpItem->lpPrev	= NULL;
