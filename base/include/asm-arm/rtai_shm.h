/*
 * Shared memory.
 *
 * Original RTAI/x86 layer implementation:
 *   Copyright (C) 2000 Paolo Mantegazza <mantegazza@aero.polimi.it>
 *   Copyright (C) 2000 Steve Papacharalambous <stevep@freescale.com>
 *   Copyright (C) 2000 Stuart Hughes <stuarth@lineo.com>
 *   and others.
 *
 * RTAI/x86 rewrite over Adeos:
 *   Copyright (C) 2002 Philippe Gerum <rpm@xenomai.org>
 *
 * Original RTAI/ARM RTHAL implementation:
 *   Copyright (C) 2000 Pierre Cloutier <pcloutier@poseidoncontrols.com>
 *   Copyright (C) 2001 Alex Züpke, SYSGO RTS GmbH <azu@sysgo.de>
 *   Copyright (C) 2002 Guennadi Liakhovetski DSA GmbH <gl@dsa-ac.de>
 *   Copyright (C) 2002 Steve Papacharalambous <stevep@freescale.com>
 *   Copyright (C) 2002 Wolfgang Müller <wolfgang.mueller@dsa-ac.de>
 *   Copyright (C) 2003 Bernard Haible, Marconi Communications
 *   Copyright (C) 2003 Thomas Gleixner <tglx@linutronix.de>
 *   Copyright (C) 2003 Philippe Gerum <rpm@xenomai.org>
 *
 * RTAI/ARM over Adeos rewrite:
 *   Copyright (C) 2004-2005 Michael Neuhauser, Firmix Software GmbH <mike@firmix.at>
 *
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
#ifndef _RTAI_ASM_ARM_SHM_H
#define _RTAI_ASM_ARM_SHM_H

#undef __SHM_USE_VECTOR /* not yet implemented for arm ... */

#ifdef __KERNEL__

#define RTAI_SHM_HANDLER		shm_handler
#define DEFINE_SHM_HANDLER

#else /* !__KERNEL__ */

#ifdef __SHM_USE_VECTOR
#include <asm/rtai_vectors.h>
#define rtai_shmrq(srq, whatever)	rtai_do_swi((srq)|(RTAI_SHM_VECTOR << 24), whatever)
#endif

#endif  /* __KERNEL__ */

/* convert virtual user memory address to physical address */
/* (virt_to_phys only works for kmalloced kernel memory) */

static inline unsigned long uvirt_to_kva(pgd_t *pgd, unsigned long adr)
{
	unsigned long ret = 0UL;
	pmd_t *pmd;
	pte_t *ptep, pte;

	if(!pgd_none(*pgd)) {
		pmd = pmd_offset(pgd, adr);
		if (!pmd_none(*pmd)) {
			ptep = pte_offset_kernel(pmd, adr);
			pte = *ptep;
			if(pte_present(pte)){
				ret = (unsigned long) page_address(pte_page(pte));
				ret |= (adr&(PAGE_SIZE-1));
			}
		}
	}
	return ret;
}

#ifndef VMALLOC_VMADDR
#define VMALLOC_VMADDR(x) ((unsigned long)(x))
#endif

static inline unsigned long kvirt_to_pa(unsigned long adr)
{
	unsigned long va, kva, ret;

	va = VMALLOC_VMADDR(adr);
	kva = uvirt_to_kva(pgd_offset_k(va), va);
	ret = __pa(kva);

	return ret;
}

#endif  /* _RTAI_ASM_ARM_SHM_H */
