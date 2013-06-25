/* Copyright (C) 2004 Free Software Foundation, Inc.
   This file is part of the GNU C Library.
   Contributed by Jakub Jelinek <jakub@redhat.com>, 2004.

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
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <rtai_posix.h>

pthread_barrier_t br;
pthread_cond_t cv; // = PTHREAD_COND_INITIALIZER;
pthread_mutex_t lock; // = PTHREAD_MUTEX_INITIALIZER;
bool n, exiting;
FILE *f;
int count;

void *
tf (void *dummy)
{
  bool loop = true;

  pthread_setschedparam_np(0, SCHED_FIFO, 0, 0xF, PTHREAD_HARD_REAL_TIME);
  pthread_barrier_wait(&br);

  while (loop)
    {
      pthread_mutex_lock (&lock);
      while (n && !exiting)
	pthread_cond_wait (&cv, &lock);
      n = true;
      pthread_mutex_unlock (&lock);

      fputs (".", f);

      pthread_mutex_lock (&lock);
      n = false;
      if (exiting)
	loop = false;
#ifdef UNLOCK_AFTER_BROADCAST
      pthread_cond_broadcast (&cv);
      pthread_mutex_unlock (&lock);
#else
      pthread_mutex_unlock (&lock);
      pthread_cond_broadcast (&cv);
#endif
    }

  return NULL;
}

int
do_test (void)
{
  f = fopen ("/dev/null", "w");
  if (f == NULL)
    {
      printf ("couldn't open /dev/null, %m\n");
      return 1;
    }

  count = sysconf (_SC_NPROCESSORS_ONLN);
  if (count <= 0)
    count = 1;
  count *= 4;
  pthread_barrier_init(&br, NULL, count);

  pthread_t th[count];
  int i, ret;
  for (i = 0; i < count; ++i)
    if ((ret = pthread_create (&th[i], NULL, tf, NULL)) != 0)
      {
	errno = ret;
	printf ("pthread_create %d failed: %m\n", i);
	return 1;
      }

  struct timespec ts = { .tv_sec = 5, .tv_nsec = 0 };
  while (nanosleep (&ts, &ts) != 0);

  pthread_mutex_lock (&lock);
  exiting = true;
  pthread_mutex_unlock (&lock);

  for (i = 0; i < count; ++i)
    pthread_join (th[i], NULL);

  fclose (f);
  return 0;
}

#define TIMEOUT 40
int main(void)
{
	pthread_setschedparam_np(0, SCHED_FIFO, 0, 0xF, PTHREAD_HARD_REAL_TIME);
	start_rt_timer(0);
	pthread_cond_init(&cv, NULL);
	pthread_mutex_init(&lock, NULL);
	do_test();
	return 0;
}
