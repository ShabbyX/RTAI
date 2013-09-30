/*
COPYRIGHT (C) 2008  Paolo Mantegazza <mantegazza@aero.polimi.it>

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

#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sched.h>

#include <rtai_shm.h>

int main(void)
{
	int *vm, *km, *ka, *kd, *gh, i;
	unsigned long s;
	rt_thread_init(nam2num("MAIN"), 0, 0, SCHED_FIFO, 0xF);
	vm = rt_shm_alloc(nam2num("VM"), 0, USE_VMALLOC);
	km = rt_shm_alloc(nam2num("KM"), 0, USE_GFP_KERNEL);
	ka = rt_shm_alloc(nam2num("KA"), 0, USE_GFP_ATOMIC);
	kd = rt_shm_alloc(nam2num("KD"), 0, USE_GFP_DMA);
	rt_global_heap_open();
	gh = rt_named_malloc(nam2num("GH"), 0);
	printf("SIZEs in USER: VM = %d, KM = %d, KA = %d, KD = %d, GH = %d.\n", vm[0], km[0], ka[0], kd[0], gh[0]);
	printf("< ALL OF THE ABOVE SIZEs MUST BE THE SAME AS THAT DISPLAYED FOR KERNEL SPACE >\n");
	s = 0;
	for (i = 1; i < vm[0]; i++) {
		if ( vm[i] != km[i] || km[i] != ka[i] || ka[i] != kd[i] || kd[i] != gh[i] || gh[i] != (s += i)) {
			printf("wrong at index %i\n", i);
		}
	}
	rt_shm_free(nam2num("VM"));
	rt_shm_free(nam2num("KM"));
	rt_shm_free(nam2num("KA"));
	rt_shm_free(nam2num("KD"));
	rt_named_free(gh);
	rt_global_heap_close();
	return 0;
}
