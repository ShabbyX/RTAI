/*
 *   ARTI -- RTAI-compatible Adeos-based Real-Time Interface. Based on
 *   the original RTAI layer for x86.
 *
 *   Original RTAI/x86 layer implementation:
 *   Copyright (C) 2000 Paolo Mantegazza,
 *   Copyright (C) 2000 Steve Papacharalambous,
 *   Copyright (C) 2000 Stuart Hughes,
 *   and others.
 *
 *   RTAI/x86 rewrite over Adeos:
 *   Copyright (C) 2002 Philippe Gerum.
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, Inc., 675 Mass Ave, Cambridge MA 02139,
 *   USA; either version 2 of the License, or (at your option) any later
 *   version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef _RTAI_ASM_M68KNOMMU_VECTORS_H
#define _RTAI_ASM_M68KNOMMU_VECTORS_H

#define RTAI_SYS_VECTOR     43
#define RTAI_CMPXCHG_TRAP_SYS_VECTOR     44
#define RTAI_XCHG_TRAP_SYS_VECTOR     45

#ifdef __KERNEL__

#include <linux/version.h>

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,19)
#include <asm/ipipe.h>
#endif

#include <rtai_hal_names.h>
#include <rtai_config.h>

#ifdef CONFIG_X86_LOCAL_APIC
#define RTAI_APIC_HIGH_VECTOR  HAL_APIC_HIGH_VECTOR
#define RTAI_APIC_LOW_VECTOR   HAL_APIC_LOW_VECTOR
#else 
#define RTAI_APIC_HIGH_VECTOR  0xff
#define RTAI_APIC_LOW_VECTOR   0xff
#endif

#ifdef ipipe_apic_vector_irq /* LINUX_VERSION_CODE > KERNEL_VERSION(2,6,19) */
#define RTAI_APIC_HIGH_IPI     ipipe_apic_vector_irq(RTAI_APIC_HIGH_VECTOR)
#define RTAI_APIC_LOW_IPI      ipipe_apic_vector_irq(RTAI_APIC_LOW_VECTOR)
#define RTAI_SYS_IRQ           ipipe_apic_vector_irq(RTAI_SYS_VECTOR)
#define LOCAL_TIMER_IPI        ipipe_apic_vector_irq(LOCAL_TIMER_VECTOR)
#else
#define RTAI_APIC_HIGH_IPI     (RTAI_APIC_HIGH_VECTOR - FIRST_EXTERNAL_VECTOR)
#define RTAI_APIC_LOW_IPI      (RTAI_APIC_LOW_VECTOR - FIRST_EXTERNAL_VECTOR)
#define RTAI_SYS_IRQ           (RTAI_SYS_VECTOR - FIRST_EXTERNAL_VECTOR)
#define LOCAL_TIMER_IPI        (LOCAL_TIMER_VECTOR - FIRST_EXTERNAL_VECTOR)
#endif

#if RTAI_APIC_HIGH_VECTOR == RTAI_SYS_VECTOR || RTAI_APIC_LOW_VECTOR == RTAI_SYS_VECTOR
#error *** RTAI_SYS_VECTOR CONFLICTS WITH APIC VECTORS USED BY RTAI ***
#endif

#endif

static inline void RTAI_DO_TRAP_SYS(long long *ret, int srq, unsigned long args)
{
	register int __srq __asm__ ("%d0") = srq;
        register unsigned long __args __asm__ ("%d1") = args;
        register unsigned long __ret1 __asm__ ("%d2");
        register unsigned long __ret2 __asm__ ("%d3");
	__asm__ __volatile__ ( "trap #11\n\t" : "+d" (__ret1),"+d" (__ret2) : "d" (__srq), "d" (__args) : "memory" );
	*ret = __ret1 + ((long long)__ret2 << 32);
}

#endif /* !_RTAI_ASM_M68KNOMMU_VECTORS_H */
