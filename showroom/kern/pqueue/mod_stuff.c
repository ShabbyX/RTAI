//////////////////////////////////////////////////////////////////////////////
//
//      Copyright (©) 1999 Zentropic Computing, All rights reserved
//
// Authors:             Trevor Woolven (trevw@zentropix.com)
// Original date:       Thu 15 Jul 1999
// Id:                  @(#)$Id: mod_stuff.c,v 1.3 2005/12/09 17:28:21 mante Exp $
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
// Module control file for the pqueues interface for Real Time Linux.
//
///////////////////////////////////////////////////////////////////////////////
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>

#include <rtai_mq.h>
#include "mod_stuff.h"

//------------------------------------------------------------------------------
//Stuff defined/declared in the test program file
extern int PARENT_PRIORITY;
extern char *test_name;
extern RT_TASK parent_task;
void parent_func(long arg);

//------------------------------------------------------------------------------
#include <linux/ctype.h>
void upc(char *str)
{
//Convert a string from lower to upper case
int len = strlen(str);
int i = 0;

    while(i < len ) {
	if(isalpha(str[i])) {
	    str[i] = toupper(str[i]);
	}
	i++;
    }
}

//------------------------------------------------------------------------------
int init_module(void) {

  RTIME now, tick_period;
  upc(test_name);
  printk("\n==== Posix Queues test program %s ====\n", test_name);
#ifdef PERIODIC
    printk("==== Periodic mode selected ====\n");
    rt_set_periodic_mode();
    rt_task_init(&parent_task, parent_func, 1, STACK_SIZE,
                                                PARENT_PRIORITY, 1, 0);
//    init_z_apps(&parent_task);
    rt_set_runnable_on_cpus(&parent_task, RUN_ON_CPUS);
    tick_period = start_rt_timer((int)nano2count(TICK_PERIOD));
    rt_assign_irq_to_cpu(TIMER_8254_IRQ, TIMER_TO_CPU);
    rt_linux_use_fpu(1);
    now = rt_get_time();
    rt_task_make_periodic(&parent_task, now + 2*tick_period, tick_period);
#else
  rt_task_init(&parent_task, parent_func, 1, STACK_SIZE,
                                                PARENT_PRIORITY, 1, 0);
//  init_z_apps(&parent_task);
  rt_set_runnable_on_cpus(&parent_task, RUN_ON_CPUS);
  tick_period = start_rt_timer((int)nano2count(TICK_PERIOD));
  rt_assign_irq_to_cpu(TIMER_8254_IRQ, TIMER_TO_CPU);
  rt_linux_use_fpu(1);
  rt_task_resume(&parent_task);
#endif
	while (parent_task.magic) {
		current->state = TASK_INTERRUPTIBLE;
		schedule_timeout(HZ/10);
	}
  return 0;

}

//------------------------------------------------------------------------------
void cleanup_module(void) {

  rt_reset_irq_to_sym_mode(TIMER_8254_IRQ);
  stop_rt_timer();
  printk("\n==== Posix Queues test program %s removed====\n", test_name);

}
//------------------------------------eof---------------------------------------
