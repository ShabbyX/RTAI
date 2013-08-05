/*
 * Copyright (C) 2005 Joerg Langenberg <joerg.langenberg@gmx.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.

 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <rtdm/syscall.h>

int __rtdm_muxid = -1;

static __attribute__((constructor)) void __init_rtdm_interface(void)

{
    int muxid;

    muxid = XENOMAI_SYSCALL2(__xn_sys_bind, RTDM_SKIN_MAGIC, NULL);

    if (muxid < 0) {
	fprintf(stderr,"RTAI/fusion: RTDM skin or user-space support unavailable.\n");
	fprintf(stderr,"(Did you load the rtai_rtdm.ko module?)\n");
	exit(1);
    }

    __rtdm_muxid = muxid;
}
