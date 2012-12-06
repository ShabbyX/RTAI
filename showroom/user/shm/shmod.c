/*
COPYRIGHT (C) 2008  Paolo Mantegazza (mantegazza@aero.polimi.it)

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


#include <rtai_nam2num.h>
#include <rtai_shm.h>

#define SIZE 100000

int *gh;	

int init_module(void)
{
	int *vm, *km, *ka, *kd, i;	
	unsigned long s;
	vm = rt_shm_alloc(nam2num("VM"), SIZE*sizeof(int), USE_VMALLOC);
	km = rt_shm_alloc(nam2num("KM"), SIZE*sizeof(int), USE_GFP_KERNEL);
	ka = rt_shm_alloc(nam2num("KA"), SIZE*sizeof(int), USE_GFP_ATOMIC);
	kd = rt_shm_alloc(nam2num("KD"), SIZE*sizeof(int), USE_GFP_DMA);
	gh = rt_named_malloc(nam2num("GH"), SIZE*sizeof(int));
	vm[0] = km[0] = ka[0] = kd[0] = gh[0] = SIZE;
	s = 0;
	for (i = 1; i < SIZE; i++) {
		vm[i] = km[i] = ka[i] = kd[i] = gh[i] = s += i;
	}
	printk("< SIZEs in KERNEL %d >\n", SIZE);
	return 0;
}

void cleanup_module(void)
{
	rt_shm_free(nam2num("VM"));
	rt_shm_free(nam2num("KM"));
	rt_shm_free(nam2num("KA"));
	rt_shm_free(nam2num("KD"));
	rt_named_free(gh);
}
