/* The algorithm used here is based on the crc32.c file from zlib.
   It's copyright follows although the code has been modified.

   * Copyright (C) 1995-2005 Mark Adler

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.

  Jean-loup Gailly        Mark Adler
  jloup@gzip.org          madler@alumni.caltech.edu
*/
#include <ioFTPD.h>


static DWORD gf2_matrix_times(DWORD *mat, DWORD vec)
{
	DWORD sum;

    sum = 0;
    while (vec) {
        if (vec & 1)
            sum ^= *mat;
        vec >>= 1;
        mat++;
    }
    return sum;
}

static void gf2_matrix_square(DWORD *square, DWORD *mat)
{
    int n;

    for (n = 0; n < 32; n++)
	{
		square[n] = gf2_matrix_times(mat, mat[n]);
	}
}


DWORD crc32_combine(DWORD crc1, DWORD crc2, UINT64 u64Len2)
{
	DWORD n, row;
	DWORD even[32];    /* even-power-of-two zeros operator */
	DWORD odd[32];     /* odd-power-of-two zeros operator */

    /* degenerate case */
    if (u64Len2 == 0)
        return crc1;

    /* put operator for one zero bit in odd */
    odd[0] = 0xedb88320L;           /* CRC-32 polynomial */
    row = 1;
    for (n = 1; n < 32; n++) {
        odd[n] = row;
        row <<= 1;
    }

    /* put operator for two zero bits in even */
    gf2_matrix_square(even, odd);

    /* put operator for four zero bits in odd */
    gf2_matrix_square(odd, even);

    /* apply len2 zeros to crc1 (first square will put the operator for one
       zero byte, eight zero bits, in even) */
    do {
        /* apply zeros operator for this bit of len2 */
        gf2_matrix_square(even, odd);
        if (u64Len2 & 1)
            crc1 = gf2_matrix_times(even, crc1);
        u64Len2 >>= 1;

        /* if no more bits set, then done */
        if (u64Len2 == 0)
            break;

        /* another iteration of the loop with odd and even swapped */
        gf2_matrix_square(odd, even);
        if (u64Len2 & 1)
            crc1 = gf2_matrix_times(odd, crc1);
        u64Len2 >>= 1;

        /* if no more bits set, then done */
    } while (u64Len2 != 0);

    /* return combined crc */
    crc1 ^= crc2;
    return crc1;
}
