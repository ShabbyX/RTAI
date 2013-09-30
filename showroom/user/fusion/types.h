/*
 * Copyright (C) 2005 Paolo Mantegazza <mantegazza@aero.polimi.it>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef _RTAI_FUSION_TYPES_H
#define _RTAI_FUSION_TYPES_H

#include <rtai_config.h>
#include <errno.h>

#define SEM_ERR     (0xFFFF)
#define SEM_TIMOUT  (0xFFFE)
#define MSG_ERR     ((void *)0x3FFFFFFF)

#define PRIO_Q    0
#define FIFO_Q    4
#define RES_Q     3

#define BIN_SEM   1
#define CNT_SEM   2
#define RES_SEM   3

#define RT_SCHED_FIFO  0
#define RT_SCHED_RR    1

typedef unsigned long long RTIME;

#define TM_INFINITE (0)
#define TM_NONBLOCK (0)
#define TM_NOW      (0)

#endif /* !_RTAI_FUSION_TYPES_H */
