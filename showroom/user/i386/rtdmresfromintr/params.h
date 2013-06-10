/*
 * Copyright (C) 2008 Paolo Mantegazza <mantegazza@aero.polimi.it>.
 *
 * RTAI/fusion is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * RTAI/fusion is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with RTAI/fusion; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#define RTC_IRQ  8
#define MAX_RTC_FREQ  8192
#define RTC_FREQ      MAX_RTC_FREQ
#define PERIOD  (1000000000 + RTC_FREQ/2)/RTC_FREQ // nanos
#define TASK_CPU  (1 << 1)
#define IRQ_CPU   (1 << 0)
