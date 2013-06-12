#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>

#include <rtai_scb.h>

#define BUFSIZE 512

static int end;

static void endme(int dummy) { end = 1; }

int main(void)
{
	unsigned int player, cnt;
	char data[BUFSIZE];
	void *scb;

	signal(SIGINT, endme);

	scb = rt_scb_init(nam2num("SCB"), 0, 0);
	if ((player = open("../../../share/linux.au", O_RDONLY)) < 0) {
		printf("ERROR OPENING SOUND FILE (linux.au)\n");
		exit(1);
	}

	while (!end) {
		lseek(player, 0, SEEK_SET);
		while(!end && (cnt = read(player, data, BUFSIZE)) > 0) {
			while (!end && rt_scb_put(scb, data, cnt));
		}
	}

//	rt_scb_delete(nam2num("SCB"));
	close(player);
	return 0;
}
