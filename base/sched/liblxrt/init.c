/*
 * Copyright (C) Pierre Cloutier <pcloutier@PoseidonControls.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the version 2 of the GNU Lesser
 * General Public License as published by the Free Software
 * Foundation.
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
#include <unistd.h>
#include <sys/types.h>
#include <sys/user.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sched.h>

void init_linux_scheduler(int sched, int pri)
{
	struct sched_param mysched;

	if(sched != SCHED_RR && sched != SCHED_FIFO) {
		puts("Invalid scheduling scheme");
		exit(1);
	}

	if((pri < sched_get_priority_min(sched)) || (pri > sched_get_priority_max(sched))) {
		puts("Invalid priority");
		exit(2);
	}

	mysched.sched_priority = pri ;
	if( sched_setscheduler( 0, SCHED_FIFO, &mysched ) == -1 ) {
		puts("Error in setting the Linux scheduler");
		perror("errno");
		exit(3);
	}
}
