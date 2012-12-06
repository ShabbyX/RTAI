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


#ifndef _RTAI_ASM_PPC_SHM_H
#define _RTAI_ASM_PPC_SHM_H

#if 0

#undef __SHM_USE_VECTOR

#ifndef __KERNEL__

#include <asm/rtai_vectors.h>

#ifdef __SHM_USE_VECTOR
// the following function is adapted from Linux PPC unistd.h 
static inline long long rtai_shmrq(unsigned long srq, unsigned long whatever)
{
	long long retval;
	register unsigned long __sc_0 __asm__ ("r0");
	register unsigned long __sc_3 __asm__ ("r3");
	register unsigned long __sc_4 __asm__ ("r4");

	__sc_0 = (__sc_3 = srq | (RTAI_SHM_VECTOR << 24)) + (__sc_4 = whatever);
	__asm__ __volatile__
		("trap         \n\t"
		: "=&r" (__sc_3), "=&r" (__sc_4)
		: "0"   (__sc_3), "1"   (__sc_4),
	          "r"   (__sc_0)
		: "r0", "r3", "r4" );
	((unsigned long *)&retval)[0] = __sc_3;
	((unsigned long *)&retval)[1] = __sc_4;
	return retval;
}

#else  /* __KERNEL__ */

#define RTAI_SHM_HANDLER shm_handler

#define DEFINE_SHM_HANDLER

#endif /* __SHM_USE_VECTOR */

#endif /* __KERNEL__ */

#include <asm/pgtable.h>
#include <asm/io.h>

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

#ifndef VMALLOC_VMADDR
#define VMALLOC_VMADDR(x) ((unsigned long)(x))
#endif

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

#endif /* !_RTAI_ASM_PPC_SHM_H */
