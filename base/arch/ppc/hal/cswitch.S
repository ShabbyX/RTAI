/*
COPYRIGHT (C) 2000  Paolo Mantegazza (mantegazza@aero.polimi.it)

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA.
*/

#include <rtai_config.h> 

/*
 * Saving: a series of push (stw reg,-ofst(1)) of the general purpose registers, followed by push of some machine registers
 *         and the saving of the stack pointer (general register 1).
 * Restoring: recovery of the stack pointer, followed by a series of pop (lwz reg,-ofst(1)) of the same machine registers
 *            and of the general purpose registers.
 * Note that they are not really push/pop as we do not update the stack pointer, it is not needed and saves an instruction :-).
 */

#ifndef __ASSEMBLY__
#define __ASSEMBLY__
#endif
#include <asm/processor.h>

	.globl up_task_sw
up_task_sw:

	stw	31, -1*4(1)
	stw	30, -2*4(1)
	stw	29, -3*4(1)
	stw	28, -4*4(1)
	stw	27, -5*4(1)
	stw	26, -6*4(1)
	stw	25, -7*4(1)
	stw	24, -8*4(1)
	stw	23, -9*4(1)
	stw	22,-10*4(1)
	stw	21,-11*4(1)
	stw	20,-12*4(1)
	stw	19,-13*4(1)
	stw	18,-14*4(1)
	stw	17,-15*4(1)
	stw	16,-16*4(1)
	stw	15,-17*4(1)
	stw	14,-18*4(1)
	stw	13,-19*4(1)
	stw	12,-20*4(1)
	stw	11,-21*4(1)
	stw	10,-22*4(1)
	stw	 9,-23*4(1)
	stw	 8,-24*4(1)
	stw	 7,-25*4(1)
	stw	 6,-26*4(1)
	stw	 5,-27*4(1)
	stw	 4,-28*4(1)
	stw	 3,-29*4(1)
	stw	 2,-30*4(1)
	stw	 0,-31*4(1)
	mfctr	 5
	stw	 5,-32*4(1)
	mfcr	 5
	stw	 5,-33*4(1)
	mfxer	 5
	stw	 5,-34*4(1)
	mflr	 5
	stw	 5,-35*4(1)
	mfmsr	 5
	stw	 5,-36*4(1)

	lwz	 5,0(3)
	stw	 1,0(5)

	stw	 4,0(3)

	lwz	 1,0(4)

	lwz 	 2,-36*4(1)
	mtmsr	 2
	lwz	 2,-35*4(1)
	mtlr	 2
	lwz	 2,-34*4(1)
	mtxer	 2
	lwz	 2,-33*4(1)
	mtcr	 2
	lwz	 2,-32*4(1)
	mtctr	 2
	lwz	 0,-31*4(1)
	lwz	 2,-30*4(1)
	lwz	 3,-29*4(1)
	lwz	 4,-28*4(1)
	lwz	 5,-27*4(1)
	lwz	 6,-26*4(1)
	lwz	 7,-25*4(1)
	lwz	 8,-24*4(1)
	lwz	 9,-23*4(1)
	lwz	10,-22*4(1)
	lwz	11,-21*4(1)
	lwz	12,-20*4(1)
	lwz	13,-19*4(1)
	lwz	14,-18*4(1)
	lwz	15,-17*4(1)
	lwz	16,-16*4(1)
	lwz	17,-15*4(1)
	lwz	18,-14*4(1)
	lwz	19,-13*4(1)
	lwz	20,-12*4(1)
	lwz	21,-11*4(1)
	lwz	22,-10*4(1)
	lwz	23, -9*4(1)
	lwz	24, -8*4(1)
	lwz	25, -7*4(1)
	lwz	26, -6*4(1)
	lwz	27, -5*4(1)
	lwz	28, -4*4(1)
	lwz	29, -3*4(1)
	lwz	30, -2*4(1)
	lwz	31, -1*4(1)

	blr

#ifdef CONFIG_RTAI_FPU_SUPPORT
/*
 * Saving/restoring the FPU environment in PPC is like eating a cake, very simple. Just save/restore all of the floating 
 * point registers, recall they are always 64 bits long, and the floating point state register. Remark: at task init we
 * always enable FP, i.e. MSR flag FP set to 1, for real time tasks and accept default actions for faulty FLOPs, i.e. MSR 
 * flags FE0 and FE1 are set to zero.
 */

	.globl __save_fpenv
__save_fpenv:

	stw	 0,  -4(1)
	mfmsr    0
	ori      0, 0,MSR_FP
	mtmsr    0
	lwz      0,  -4(1)
	isync

	stfd	 0, 0*8(3)
	stfd	 1, 1*8(3)
	stfd	 2, 2*8(3)
	stfd	 3, 3*8(3)
	stfd	 4, 4*8(3)
	stfd	 5, 5*8(3)
	stfd	 6, 6*8(3)
	stfd	 7, 7*8(3)
	stfd	 8, 8*8(3)
	stfd	 9, 9*8(3)
	stfd	10,10*8(3)
	stfd	11,11*8(3)
	stfd	12,12*8(3)
	stfd	13,13*8(3)
	stfd	14,14*8(3)
	stfd	15,15*8(3)
	stfd	16,16*8(3)
	stfd	17,17*8(3)
	stfd	18,18*8(3)
	stfd	19,19*8(3)
	stfd	20,20*8(3)
	stfd	21,21*8(3)
	stfd	22,22*8(3)
	stfd	23,23*8(3)
	stfd	24,24*8(3)
	stfd	25,25*8(3)
	stfd	26,26*8(3)
	stfd	27,27*8(3)
	stfd	28,28*8(3)
	stfd	29,29*8(3)
	stfd	30,30*8(3)
	stfd	31,31*8(3)
	mffs	 0
	stfd	 0,32*8(3)

	blr

	.globl __restore_fpenv
__restore_fpenv:

	lfd	 0, 32*8(3)
	mtfsf	 0xFF,0
	lfd	 0, 0*8(3)
	lfd	 1, 1*8(3)
	lfd	 2, 2*8(3)
	lfd	 3, 3*8(3)
	lfd	 4, 4*8(3)
	lfd	 5, 5*8(3)
	lfd	 6, 6*8(3)
	lfd	 7, 7*8(3)
	lfd	 8, 8*8(3)
	lfd	 9, 9*8(3)
	lfd	10,10*8(3)
	lfd	11,11*8(3)
	lfd	12,12*8(3)
	lfd	13,13*8(3)
	lfd	14,14*8(3)
	lfd	15,15*8(3)
	lfd	16,16*8(3)
	lfd	17,17*8(3)
	lfd	18,18*8(3)
	lfd	19,19*8(3)
	lfd	20,20*8(3)
	lfd	21,21*8(3)
	lfd	22,22*8(3)
	lfd	23,23*8(3)
	lfd	24,24*8(3)
	lfd	25,25*8(3)
	lfd	26,26*8(3)
	lfd	27,27*8(3)
	lfd	28,28*8(3)
	lfd	29,29*8(3)
	lfd	30,30*8(3)
	lfd	31,31*8(3)

	blr
#endif

