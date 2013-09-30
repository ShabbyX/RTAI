/*
COPYRIGHT (C) 2001  Paolo Mantegazza <mantegazza@aero.polimi.it>

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
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/mman.h>
#include <signal.h>
#include <sys/types.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <rtai_netrpc.h>

#define BUFSIZE  1000

static volatile int end;

static void *endme(void *args)
{
	getchar();
	end = 1;
	return 0;
}

static pthread_t thread;

int main(int argc, char *argv[])
{
	unsigned int player, msg, spknode, spkport, i;
	RT_TASK *plrtsk, *spktsk;
	struct sockaddr_in addr;
	MBX *mbx;
	char data[BUFSIZE];

	thread = rt_thread_create(endme, NULL, 2000);
	if ((player = open("../../../share/linux.au", O_RDONLY)) < 0) {
		printf("ERROR OPENING SOUND FILE (linux.au)\n");
		exit(1);
	}

 	if (!(plrtsk = rt_task_init_schmod(nam2num("PLRTSK"), 2, 0, 0, SCHED_FIFO, 0xF))) {
		printf("CANNOT INIT PLAYER TASK\n");
		exit(1);
	}

	spknode = 0;
	if (argc == 2 && strstr(argv[1], "SpkNode=")) {
		inet_aton(argv[1] + 8, &addr.sin_addr);
		spknode = addr.sin_addr.s_addr;
	}
	if (!spknode) {
		inet_aton("127.0.0.1", &addr.sin_addr);
		spknode = addr.sin_addr.s_addr;
	}

	while ((spkport = rt_request_port(spknode)) <= 0 && spkport != -EINVAL);
	spktsk = RT_get_adr(spknode, spkport, "SPKTSK");
	mbx = RT_get_adr(spknode, spkport, "SNDMBX");

	for (i = 0; i < 100; i++) {
		RT_rpc(spknode, spkport, spktsk, msg, &msg);
	}
	mlockall(MCL_CURRENT | MCL_FUTURE);
	printf("\nPLAYER TASK RUNNING\n\n(TYPE ENTER TO END EVERYTHING)\n");

	while (!end) {
		lseek(player, 0, SEEK_SET);
		while(!end && (msg = read(player, data, BUFSIZE)) > 0) {
			RT_mbx_send(spknode, spkport, mbx, data, msg);
		}
	}

	RT_rpc(spknode, spkport, spktsk, msg, &msg);
	rt_release_port(spknode, spkport);
	rt_task_delete(plrtsk);
	close(player);
	printf("\nPLAYER TASK STOPS\n");
	return 0;
}
