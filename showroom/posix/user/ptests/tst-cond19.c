/* Copyright (C) 2004 Free Software Foundation, Inc.
   This file is part of the GNU C Library.
   Contributed by Ulrich Drepper <drepper@redhat.com>, 2004.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307 USA.  */

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <rtai_posix.h>

static pthread_cond_t cond; // = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t mut; // = PTHREAD_MUTEX_INITIALIZER;


static int
do_test (void)
{
  int result = 0;
  struct timespec ts;

  if (clock_gettime (CLOCK_MONOTONIC, &ts) != 0)
    {
      puts ("clock_gettime failed");
      return 1;
    }

  ts.tv_nsec = -1;

  int e = pthread_cond_timedwait (&cond, &mut, &ts);
  if (e == 0)
    {
      puts ("first cond_timedwait did not fail");
      result = 1;
    }
  else if (e != EINVAL)
    {
      puts ("first cond_timedwait did not return EINVAL");
      result = 1;
    }

  ts.tv_nsec = 2000000000;

  e = pthread_cond_timedwait (&cond, &mut, &ts);
  if (e == 0)
    {
      puts ("second cond_timedwait did not fail");
      result = 1;
    }
  else if (e != EINVAL)
    {
      puts ("second cond_timedwait did not return EINVAL");
      result = 1;
    }

  return result;
}

int main(void)
{
	pthread_init_real_time_np("TASKA", 0, SCHED_FIFO, 0xF, PTHREAD_HARD_REAL_TIME);
	start_rt_timer(0);
	pthread_cond_init(&cond, NULL);
	pthread_mutex_init(&mut, NULL);
	do_test();
	pthread_cond_destroy(&cond);
	pthread_mutex_destroy(&mut);
	return 0;
}
