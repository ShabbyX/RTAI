/*
COPYRIGHT (C) 2002  Wolfgang Grandegger (wg@denx)

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

/*
Simple SHM demo program: The application program "display.c"
lists a counter incremented by the RTAI kernel modue "shm.c".
*/

#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>

#include <rtai_shm.h>
#include <rtai_nam2num.h>


#define SHMNAM "MYSHM"
#define SHMSIZ 0

static int end = 0;
static void endme (int dummy) { end = 1; }

int main (int argc, char* argv[])
{
    int *p, *shm, value = 0, count = 0;

    printf("nam2num(%s) = 0x%lx\n", SHMNAM, nam2num(SHMNAM));
    shm = (int *)rtai_malloc(nam2num(SHMNAM), SHMSIZ);
    shm = (int *)rtai_malloc(nam2num(SHMNAM), SHMSIZ);
    shm = (int *)rtai_malloc(nam2num(SHMNAM), SHMSIZ);
    p = shm = (int *)rtai_malloc(nam2num(SHMNAM), SHMSIZ);
    if (shm == NULL) {
	printf("rtai_malloc() failed (maybe /dev/rtai_shm is missing)!\n");
	return -1;
    }

    signal(SIGINT, endme);

    while (!end) {
	value = *p++;
	printf("%d\n", value);
	if (value != ++count) {
		count = 0;
		p = shm;
	}
    }
    rtai_free (nam2num(SHMNAM), shm);
    rtai_free (nam2num(SHMNAM), shm);
    rtai_free (nam2num(SHMNAM), shm);
    rtai_free (nam2num(SHMNAM), shm);

   return 0;
}

