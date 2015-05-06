/*
 *  linux/arch/arm/lib/pagecmp.S
 *
 *  Author: Indal Choi
 *  Created: Mar 27, 2014
 *  Copyright: LG Electronics, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/linkage.h>
#include <asm/assembler.h>
#include <asm/asm-offsets.h>
#include <asm/cache.h>

        .text

ENTRY(cmp_page)
                stmfd sp!, {r4 - r10, lr}
        PLD(    pld [r0] )
        PLD(    pld [r1] )
                mov r3, r0
                mov r2, #PAGE_SZ
1:
        PLD(    pld [r3, #L1_CACHE_BYTES] )
        PLD(    pld [r1, #L1_CACHE_BYTES] )
        .rept ( L1_CACHE_BYTES / 16)
                ldm r3!, {r4, r5, r6, r7}
                ldm r1!, {r8, r9, r10, lr}
                subs r0, r4, r8
                subeqs r0, r5, r9
                subeqs r0, r6, r10
                subeqs r0, r7, lr
                bne 2f
        .endr
        subs r2, r2, #L1_CACHE_BYTES
        bne 1b
2:
        ldmia sp!, {r4 - r10, pc}
ENDPROC(cmp_page)
