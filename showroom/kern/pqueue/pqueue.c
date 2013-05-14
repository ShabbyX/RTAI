//////////////////////////////////////////////////////////////////////////////
//
//      Copyright (©) 1999 Zentropic Computing, All rights reserved
//
// Authors:             Trevor Woolven (trevw@zentropix.com)
// Original date:       Thu 15 Jul 1999
// Id:                  @(#)$Id: pqueue.c,v 1.3 2005/12/08 17:35:02 mante Exp $
//
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2 of the License, or (at your option) any later version.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License along with this library; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA.
//
// Examples of the pqueues interface for Real Time Linux.
//
///////////////////////////////////////////////////////////////////////////////
#include <linux/module.h>
#include <linux/errno.h>

#include <asm/fcntl.h>

#include <rtai_mq.h>

#include "mod_stuff.h"

char *test_name = __FILE__;

//Declare task structures and initialise them!
RT_TASK parent_task = {0};
static RT_TASK child_task  = {0};

const int PARENT_PRIORITY = (RT_SCHED_LOWEST_PRIORITY - 2);
const int CHILD_PRIORITY  = (RT_SCHED_LOWEST_PRIORITY - 1);

SEM my_sem;

#define N_QUEUES 2
mqd_t qId[N_QUEUES] = {INVALID_PQUEUE};

struct Msg {
  const char *str;
  int str_size;
  uint prio;
};

//-----------------------------------------------------------------------------
void child_func(long arg) {

int i;
size_t n = 0;
char inBuf[50];
uint priority = 0;
int nMsgs = 0;
struct mq_attr my_attrs ={0,0,0,0};
int my_oflags;
mode_t my_mode = 0;
struct Msg myMsg[] = { {"Send parent message on queue!", 29, 4} };
mqd_t rx_q = INVALID_PQUEUE, tx_q = INVALID_PQUEUE;


  rt_printk("Starting child task %d\n", arg);
  for(i = 0; i < 3; i++) {

    rt_printk("child task %d, loop count %d\n", arg, i);

  }
  //Open a queue for reading
  rx_q = mq_open("my_queue1", O_RDONLY, my_mode, &my_attrs);
  if (rx_q <= 0) {
    rt_printk("ERROR: child cannot open my_queue1\n");
  } else {

    //Get the message(s) off the pQueue
    n = mq_getattr(rx_q, &my_attrs);
    nMsgs = my_attrs.mq_curmsgs;
    rt_printk("There are %d messages on the queue\n", nMsgs);

    while(nMsgs-- > 0) {
      n = mq_receive(rx_q, inBuf, sizeof(inBuf), &priority);
      inBuf[n] = '\0';

      //Report what happened
      rt_printk("Child got >%s<, %d bytes at priority %d\n", inBuf,n, priority);
    }
  }

  //Create another queue for comms back to parent
  my_oflags = O_RDWR | O_CREAT | O_NONBLOCK;
  my_mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP;
  my_attrs.mq_maxmsg = 10;
  my_attrs.mq_msgsize = 50;

  qId[1] = tx_q = mq_open("my_queue2", my_oflags, my_mode, &my_attrs);

  //Now send parent a message on this queue
  if (tx_q > 0) {
    n = mq_send(tx_q, myMsg[0].str, strlen(myMsg[0].str), myMsg[0].prio);
  } else {
    rt_printk("ERROR: child could not create my_queue2\n");
  }
  rt_printk("child: mq_send returned %d\n", n);

  //...and a semaphore to wake the lazy git up!
  rt_sem_signal(&my_sem);

  //Close queue (at least my access to it)
  rt_printk("Child closing queues\n");
  mq_close(rx_q);
  mq_close(tx_q);

  //Unlink the queue I own
  mq_unlink("mq_queue2");

//  free_z_apps(rt_whoami());
  rt_task_delete(rt_whoami());
}

//------------------------------------------------------------------------------
volatile int zdbg = 0;

void parent_func(long arg) {
int i;
int my_oflags, n = 0;
mode_t my_mode;
struct mq_attr my_attrs;
char inBuf[50];
uint priority = 0;

static const char str1[23] = "Test string number one\0";
static const char str2[28] = "This is test string num 2!!\0";
static const char str3[22] = "Test string number 3!\0";
static const char str4[34] = "This is test string number four!!\0";

#define nMsgs 4

static const struct Msg myMsgs[nMsgs] =
{
    {str1, 23, 5},
    {str2, 28, 1},
    {str3, 22, 6},
    {str4, 34, 7},
};

mqd_t tx_q = INVALID_PQUEUE, rx_q = INVALID_PQUEUE;

#ifdef STEP_TRACE
  //Wait for the debugger to say GO!
  while(!zdbg) {
	rt_task_wait_period();
  }
#endif

  rt_printk("Starting parent task %d\n", arg);
  for(i = 0; i < 5; i++) {
    rt_printk("parent task %d, loop count %d\n", arg, i);
  }
  //Create a queue
  my_oflags = O_RDWR | O_CREAT | O_NONBLOCK;
  my_mode = S_IRUSR | S_IWUSR | S_IRGRP;
  my_attrs.mq_maxmsg = 10;
  my_attrs.mq_msgsize = 50;

  qId[0] = tx_q = mq_open("my_queue1", my_oflags, my_mode, &my_attrs);

  if (tx_q > 0) {
    rt_printk("Parent sending %d messages\n", nMsgs);
    for(i = 0; i <  nMsgs ; i++) {

      //Put message on queue
      n = mq_send(tx_q, myMsgs[i].str, myMsgs[i].str_size, myMsgs[i].prio);

      //Report how much was sent
      rt_printk("Parent sent message %d of %d bytes to child at priority %d\n",
					i, n, myMsgs[i].prio);
    }
  } else {
      rt_printk("ERROR: parent could not create my_queue1\n");
  }

  rt_task_init(&child_task, child_func, 2, STACK_SIZE, CHILD_PRIORITY, 1, 0);
  rt_set_runnable_on_cpus(&child_task, RUN_ON_CPUS);
  rt_task_resume(&child_task);

  //Wait for comms back from kiddy
  rt_sem_init(&my_sem, 0);
  rt_sem_wait(&my_sem);

  rt_printk("Parent waking up\n");

  //Now open the receive queue to read what kiddy has to say
  rx_q = mq_open("my_queue2", my_oflags, 0, 0);
  rt_printk("parent: mq_open returned %ld\n", rx_q);

  n = mq_receive(rx_q, inBuf, sizeof(inBuf), &priority);
  if (n > 0) {
    inBuf[n] = '\0';
    rt_printk("Parent got >%s< from child on queue %d\n", inBuf, (int)rx_q);
  }

  //Tidy-up...
  mq_close(tx_q);
  mq_close(rx_q);
  mq_unlink("my_queue1");
  mq_unlink("my_queue2");

  rt_task_delete(rt_whoami());
}

EXPORT_SYMBOL(parent_task);
EXPORT_SYMBOL(test_name);
EXPORT_SYMBOL(parent_func);
EXPORT_SYMBOL(PARENT_PRIORITY);
//------------------------------------eof---------------------------------------
