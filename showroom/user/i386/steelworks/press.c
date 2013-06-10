/*
COPYRIGHT (C) 2000  Andrew Hooper (andrew@best.net.nz)
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
#include <linux/module.h>
*/


#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/poll.h>
#include <sys/io.h>
#include <fcntl.h>
#include <signal.h>
#include <curses.h>

#include <rtai_lxrt.h>
#include <rtai_mbx.h>

#define LPT_PORT 0x378
#define LPT_DATA 0x379
#define LPT_CTRL 0x37a

static volatile int end;

static SEM *sem;

static int maxcnt, MaxCount;

#define PI 3.141592

static volatile int encoder_count, old_enc_cnt;
static int encoder_bit, encoder_bit_old;

static float Encoder_Res, Encoder_Dia, Encoder;
static float Item_Length, Steel_Width, Steel_Thick;
static int Item_Qty, Pressed_Items;

/*  something to the printer port */
#define pport_out(byte) outb((byte), LPT_PORT)

/* Get the interupt, increment the encoder count, and play with parport */
static void *encoder_irq(void *args)
{
	RT_TASK *mytask;
	int mycount = 0;

	rt_allow_nonroot_hrt();
	iopl(3);
 	if (!(mytask = rt_thread_init(nam2num("ENCTSK"), 1, 0, SCHED_FIFO, 0xF))) {
		printf("CANNOT INIT PROCESS TASK\n");
		exit(1);
	}
	mlockall(MCL_CURRENT | MCL_FUTURE);

	rt_make_hard_real_time();
	while (end != 2) {
		rt_sem_wait(sem);
		encoder_count++;
		mycount++;
		if (mycount >= MaxCount) {
			if (mycount >= maxcnt) {
				maxcnt = mycount;
			}
			mycount = 0;
		}
		encoder_bit = inb(LPT_PORT + 1) & 0x80;
		pport_out(inb(LPT_PORT) == 0x01 ? 0x00 : 0x01);
		encoder_bit_old = encoder_bit;
	}
	rt_make_soft_real_time();
	rt_task_delete(mytask);

	return NULL;
}

static WINDOW *screen, *title, *mywin1, *mywin2, *foot;

void screen_init(void)
{
	const int x = 0, y = 0, width = 80, len = 25;
        initscr();
        noecho();
        crmode();

        screen = newwin(len, width, y, x);
        box(screen, ACS_VLINE, ACS_HLINE);

        title = newwin(len - 22, width - 4, y + 1, x + 2);
        box(title, ACS_VLINE, ACS_HLINE);

        mywin1 = newwin(len - 8, width - 43, y + 4, x + 2);
        box(mywin1, ACS_VLINE, ACS_HLINE);

        mywin2 = newwin(len - 8, width - 43, y + 4, x + 41);
        box(mywin2, ACS_VLINE, ACS_HLINE);

        foot = newwin(len - 22, width - 4, y + 21, x + 2);
        box(foot, ACS_VLINE, ACS_HLINE);

        wrefresh(screen);
        wrefresh(title);
        wrefresh(mywin1);
        wrefresh(mywin2);
        wrefresh(foot);
}

static void get_data(void)
{
	printf("Please enter encoder's data.\n");
	printf("Pulses/Round : ");
	scanf("%f",&Encoder_Res);
	printf("Encoder Diameter : ");
	scanf("%f",&Encoder_Dia);

	printf("\nPlease enter job details\n");
	printf("Steel Width : ");
	scanf("%f",&Steel_Width);
	printf("Steel Thickness : ");
	scanf("%f",&Steel_Thick);
	printf("Item Length : ");
	scanf("%f",&Item_Length);
	printf("Quantity to press : ");
	scanf("%i",&Item_Qty);
	Encoder = PI*Encoder_Dia/Encoder_Res;
}

static void paint_screen(void)
{
	curs_set(0);
	mvwprintw(title,1,32,"- PRESSA -");

	mvwprintw(mywin1,1,12,"ENCODER CONFIG");
	mvwprintw(mywin1,3,2,"Pusles/Round     : %i",(int)Encoder_Res);
	mvwprintw(mywin1,4,2,"Wheel Diameter   : %f",Encoder_Dia);
	mvwprintw(mywin1,5,2,"Length per pulse : %f",Encoder);

	mvwprintw(mywin1,8,14,"JOB SPECS");
	mvwprintw(mywin1,10,2,"Steel Width     : %f",Steel_Width);
	mvwprintw(mywin1,11,2,"Steel Thickness : %f",Steel_Thick);
	mvwprintw(mywin1,12,2,"Item Length     : %f",Item_Length);
	mvwprintw(mywin1,13,2,"Items Required  : %i",Item_Qty);

	mvwprintw(mywin2,4,10,"WORK IN PROGRESS");
	mvwprintw(mywin2,6,2,"Encoder count :");
	mvwprintw(mywin2,7,2,"Item Length   :");
	mvwprintw(mywin2,8,2,"Items Pressed :");
	mvwprintw(mywin2,10,2,"(MAX_ENC_CNT (%i) :", (int)(Item_Length/Encoder));

	mvwprintw(foot,1,32,"- PRESSA -");

	wrefresh(title);
	wrefresh(mywin1);
	wrefresh(mywin2);
	wrefresh(foot);
}

static void update_display(void)
{
	old_enc_cnt = encoder_count;
	mvwprintw(mywin2,6,18,"         ");
	mvwprintw(mywin2,6,18,"%i", old_enc_cnt);
	mvwprintw(mywin2,7,18,"%f", Item_Length*Encoder);
	mvwprintw(mywin2,8,18,"%i", Pressed_Items);
	mvwprintw(mywin2,10,25,"%i)", maxcnt);
	wrefresh(mywin2);
}

void screen_end(void)
{
	endwin();
	endwin();
	endwin();
	endwin();
	endwin();
}

#define msleep(x)  do { poll(0, 0, x); } while (0)

static void endme(int dummy) { end = 1; }

/* main routine */
int main(void)
{
        MBX *mbx;
        RT_TASK *mytask;
	char start;
	pthread_t thread;

	signal(SIGINT, endme);
        if (!(mytask = rt_thread_init(nam2num("MAIN"), 1, 0, SCHED_FIFO, 0xF))) {
                printf("CANNOT INIT PROCESS TASK\n");
                exit(1);
        }
        if (!(mbx = rt_get_adr(nam2num("RESMBX")))) {
                printf("CANNOT FIND MAILBOX\n");
                exit(1);
        }
	if (!(sem = rt_get_adr(nam2num("RESEM")))) {
		printf("CANNOT FIND SEMAPHORE\n");
		exit(1);
	}
	thread = rt_thread_create(encoder_irq, NULL, 0);
	get_data();
	screen_init();
	paint_screen();
	msleep(10);
        start = 1;
        rt_mbx_send(mbx, &start, 1);

	MaxCount = Item_Length/Encoder;
	while (Pressed_Items < Item_Qty && !end) {
		if (encoder_count >= Item_Length/Encoder) {
			encoder_count = 0;
			Pressed_Items++;
		}
		update_display();
	}
	end = 2;
	while (rt_get_adr(nam2num("ENCTSK"))) {
		msleep(10);
	}
        rt_mbx_send(mbx, &start, 1);
	rt_task_delete(mytask);
	screen_end();
	printf("\n");
        rt_thread_join(thread);

	return 0;
}
