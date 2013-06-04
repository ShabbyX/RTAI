/*
 * FPU support.
 *
 * Original RTAI/x86 layer implementation:
 *   Copyright (c) 2000 Paolo Mantegazza (mantegazza@aero.polimi.it)
 *   Copyright (c) 2000 Steve Papacharalambous (stevep@zentropix.com)
 *   Copyright (c) 2000 Stuart Hughes
 *   and others.
 *
 * RTAI/x86 rewrite over Adeos:
 *   Copyright (c) 2002 Philippe Gerum (rpm@xenomai.org)
 *
 * Original RTAI/ARM RTHAL implementation:
 *   Copyright (c) 2000 Pierre Cloutier (pcloutier@poseidoncontrols.com)
 *   Copyright (c) 2001 Alex Züpke, SYSGO RTS GmbH (azu@sysgo.de)
 *   Copyright (c) 2002 Guennadi Liakhovetski DSA GmbH (gl@dsa-ac.de)
 *   Copyright (c) 2002 Steve Papacharalambous (stevep@zentropix.com)
 *   Copyright (c) 2002 Wolfgang Müller (wolfgang.mueller@dsa-ac.de)
 *   Copyright (c) 2003 Bernard Haible, Marconi Communications
 *   Copyright (c) 2003 Thomas Gleixner (tglx@linutronix.de)
 *   Copyright (c) 2003 Philippe Gerum (rpm@xenomai.org)
 *
 * RTAI/ARM over Adeos rewrite:
 *   Copyright (c) 2004-2005 Michael Neuhauser, Firmix Software GmbH (mike@firmix.at)
 *
 * RTAI/ARM over Adeos rewrite for PXA255_2.6.7:
 *   Copyright (c) 2005 Stefano Gafforelli (stefano.gafforelli@tiscali.it)
 *   Copyright (c) 2005 Luca Pizzi (lucapizzi@hotmail.com)
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge MA 02139, USA; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place - Suite 330, Boston, MA 02111-1307, USA.
 */
#ifndef _RTAI_ASM_M68KNOMMU_FPU_H
#define _RTAI_ASM_M68KNOMMU_FPU_H

#ifdef CONFIG_RTAI_FPU_SUPPORT
#error "Sorry, there is no FPU support in RTAI for M68KNOMMU (you don't need it for soft-float or FPU-emulation)"
#endif

/* All the work is done by the soft-float library or the kernel FPU emulator. */


#define init_fpu(tsk)		do { /* nop */ } while (0)
#define restore_fpu(tsk)	do { /* nop */ } while (0)
#define save_cr0_and_clts(x)	do { /* nop */ } while (0)
#define restore_cr0(x)		do { /* nop */ } while (0)
#define enable_fpu()		do { /* nop */ } while (0)
#define load_mxcsr(val)		do { /* nop */ } while (0)
#define init_xfpu()		do { /* nop */ } while (0)
#define save_fpenv(x)		do { /* nop */ } while (0)
#define restore_fpenv(x)	do { /* nop */ } while (0)
#define restore_task_fpenv(t)	do { /* nop */ } while (0)
#define restore_fpenv_lxrt(t)	do { /* nop */ } while (0)

/* Nothing to do in m68knomu */
#define save_fpcr_and_enable_fpu(fpcr) do { /* nop */ } while (0)
#define restore_fpcr(fpcr) do { /* nop */ } while (0)
#define init_hard_fpenv() do { /* nop */ } while (0)
#define init_fpenv(fpenv) do { /* nop */ } while (0)
#define init_hard_fpu(lnxtsk) do { /* nop */ } while (0)

typedef struct m68k_fpu_env { unsigned long fpu_reg[1]; } FPU_ENV;

//Looks like we don't need it
/*#define set_tsk_used_fpu(t) \
    do { (t)->flags |= TIF_USED_FPU; } while (0)*/

//We do nothing
#define set_lnxtsk_uses_fpu(lnxtsk) do { /* nop */ } while (0)
#define clear_lnxtsk_uses_fpu(lnxtsk) do { /* nop */ } while (0)
#define lnxtsk_uses_fpu(lnxtsk) (0)

#endif /* _RTAI_ASM_M68KNOMMU_FPU_H */
