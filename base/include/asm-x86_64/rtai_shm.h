/*
 * Copyright (C) 1999-2003 Paolo Mantegazza <mantegazza@aero.polimi.it>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef _RTAI_ASM_X8664_SHM_H
#define _RTAI_ASM_X8664_SHM_H

#if 0

#ifndef __KERNEL__

#include <asm/rtai_vectors.h>

static inline long long rtai_shmrq(int srq, unsigned long args)
{
	long long retval;
	RTAI_DO_TRAP(RTAI_SYS_VECTOR, retval, srq, args);
	return retval;
}

#endif /* __KERNEL__ */

#include <asm/pgtable.h>
#include <asm/io.h>
#include <rtai_wrappers.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
#define VMALLOC_VMADDR(x) ((unsigned long)(x))
#endif /* >= 2.6.0 */

/* convert virtual user memory address to physical address */
/* (virt_to_phys only works for kmalloced kernel memory) */

static inline unsigned long uvirt_to_kva(pgd_t *pgd, unsigned long adr)
{
	unsigned long ret = 0UL;
	pmd_t *pmd;
	pte_t *ptep, pte;

	if(!pgd_none(*pgd)) {
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,11)
                pmd = pmd_offset(pgd, adr);
#else /* >= 2.6.11 */
                pmd = pmd_offset(pud_offset(pgd, adr), adr);
#endif /* < 2.6.11 */
		if (!pmd_none(*pmd)) {
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
			ptep = pte_offset(pmd, adr);
#else /* >= 2.6.0 */
			ptep = pte_offset_kernel(pmd, adr);
#endif /* < 2.6.0 */
			pte = *ptep;
			if(pte_present(pte)){
				ret = (unsigned long) page_address(pte_page(pte));
				ret |= (adr&(PAGE_SIZE-1));
			}
		}
	}
	return ret;
}

static inline unsigned long uvirt_to_bus(unsigned long adr)
{
	unsigned long kva, ret;

	kva = uvirt_to_kva(pgd_offset(current->mm, adr), adr);
	ret = virt_to_bus((void *)kva);

	return ret;
}

static inline unsigned long kvirt_to_bus(unsigned long adr)
{
	unsigned long va, kva, ret;

	va = VMALLOC_VMADDR(adr);
	kva = uvirt_to_kva(pgd_offset_k(va), va);
	ret = virt_to_bus((void *)kva);

	return ret;
}

static inline unsigned long kvirt_to_pa(unsigned long adr)
{
	unsigned long va, kva, ret;

	va = VMALLOC_VMADDR(adr);
	kva = uvirt_to_kva(pgd_offset_k(va), va);
	ret = __pa(kva);

	return ret;
}

#endif

#endif  /* !_RTAI_ASM_X8664_SHM_H */
