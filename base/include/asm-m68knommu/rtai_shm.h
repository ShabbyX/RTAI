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

#ifndef _RTAI_ASM_M68KNOMMU_SHM_H
#define _RTAI_ASM_M68KNOMMU_SHM_H

#include <asm/pgtable.h>
#include <asm/io.h>
#include <asm/rtai_vectors.h>
#include <rtai_wrappers.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
#define VMALLOC_VMADDR(x) ((unsigned long)(x))
#endif /* >= 2.6.0 */

#ifndef __KERNEL__

static inline long long rtai_shmrq(int srq, unsigned long args)
{
	long long retval;
	RTAI_DO_TRAP_SYS(&retval, srq, args);
	return retval;
}

#endif /* __KERNEL__ */

#ifndef CONFIG_MMU

/* convert virtual user memory address to physical address */
/* (virt_to_phys only works for kmalloced kernel memory) */

static inline unsigned long uvirt_to_kva(pgd_t *pgd, unsigned long adr)
{
	return adr;
}

static inline unsigned long uvirt_to_bus(unsigned long adr)
{
	return virt_to_bus((void *)adr);
}

static inline unsigned long kvirt_to_bus(unsigned long adr)
{
	unsigned long va;

	va = VMALLOC_VMADDR(adr);
	return virt_to_bus((void *)va);
}

static inline unsigned long kvirt_to_pa(unsigned long adr)
{
	unsigned long va;

	va = VMALLOC_VMADDR(adr);
	return __pa(va);
}

#endif /* !CONFIG_MMU */

#endif  /* !_RTAI_ASM_M68KNOMMU_SHM_H */
