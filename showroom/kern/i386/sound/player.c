#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>

#define BUFSIZE 512

static int end;

static void endme(int dummy) { end = 1; }

int main(void)
{
	unsigned int player, fifo;
	char data[BUFSIZE];

	signal(SIGINT, endme);

	if ((fifo = open("/dev/rtf0", O_WRONLY)) < 0) {
		printf("ERROR OPENING FIFO /dev/rtf0\n");
		exit(1);
	}

	if ((player = open("../../../share/linux.au", O_RDONLY)) < 0) {
		printf("ERROR OPENING SOUND FILE (linux.au)\n");
		exit(1);
	}

	while (!end) {	
		lseek(player, 0, SEEK_SET);
		while(!end && read(player, data, BUFSIZE)) {
			write(fifo, data, BUFSIZE);
		}
	}

	return 0;
}
