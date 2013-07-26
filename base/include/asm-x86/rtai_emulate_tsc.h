/*
 *   Copyright (C) 2004 Paolo Mantegazza (mantegazza@aero.polimi.it)
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, Inc., 675 Mass Ave, Cambridge MA 02139,
 *   USA; either version 2 of the License, or (at your option) any later
 *   version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */


//#define EMULATE_TSC  /* to force emulation even if CONFIG_X86_TSC */

#ifndef _RTAI_ASM_EMULATE_TSC_H
#define _RTAI_ASM_EMULATE_TSC_H

#if defined(EMULATE_TSC) || !defined(CONFIG_X86_TSC)

#undef RTAI_CPU_FREQ
#undef RTAI_CALIBRATED_CPU_FREQ
#undef rdtsc
#undef rtai_rdtsc
#undef DECLR_8254_TSC_EMULATION
#undef TICK_8254_TSC_EMULATION
#undef SETUP_8254_TSC_EMULATION
#undef CLEAR_8254_TSC_EMULATION

#define RTAI_CPU_FREQ             RTAI_FREQ_8254
#define RTAI_CALIBRATED_CPU_FREQ  RTAI_FREQ_8254
#define rtai_rdtsc()              rd_8254_ts()
#define rdtsc()                   rd_8254_ts()

#define TICK_8254_TSC_EMULATION()  rd_8254_ts()

#include <linux/version.h>

#define DECLR_8254_TSC_EMULATION \
const int TSC_EMULATION_GUARD_FREQ = 20; \
static struct timer_list timer;

#define SETUP_8254_TSC_EMULATION \
	do { \
		rt_setup_8254_tsc(); \
		init_timer(&timer); \
		timer.function = timer_fun; \
		timer_fun(0); \
	} while (0)

#define CLEAR_8254_TSC_EMULATION del_timer(&timer)

#endif

#endif /* !_RTAI_ASM_EMULATE_TSC_H */
