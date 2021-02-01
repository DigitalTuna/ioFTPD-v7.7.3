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

typedef INT (__cdecl * QUICKCOMPAREPROC)(LPCVOID *, LPCVOID *);
typedef INT (__cdecl * QUICKCHECKPROC)(LPCVOID *);

LPVOID QuickDelete(LPVOID *List, INT Items, LPVOID Item, QUICKCOMPAREPROC Comp, QUICKCHECKPROC Check);
INT QuickInsert(LPVOID *List, INT Items, LPVOID Item, QUICKCOMPAREPROC Comp);
INT QuickInsert2(register LPVOID *List, INT Items, LPVOID Item, QUICKCOMPAREPROC Comp);
INT QuickFind(LPVOID *List, INT Items, LPVOID Item, QUICKCOMPAREPROC Comp);
LPVOID QuickDeleteIndex(LPVOID *List, INT Items, INT dwPos);
