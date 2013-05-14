#ifndef _MOD_STUFF_H_
#define _MOD_STUFF_H_
//////////////////////////////////////////////////////////////////////////////
//
//      Copyright (©) 1999 Zentropic Computing, All rights reserved
//
// Authors:             Trevor Woolven (trevw@zentropix.com)
// Original date:       Thu 15 Jul 1999
// Id:                  @(#)$Id: mod_stuff.h,v 1.2 2004/09/01 15:44:48 mante Exp $
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
#define RUNNABLE_ON_CPUS 3
#define RUN_ON_CPUS (num_online_cpus() > 1 ? RUNNABLE_ON_CPUS : 1)

#define TIMER_TO_CPU 3
// < 0 || > 1 to maintain a symmetrically processed timer

#define STACK_SIZE 2000
#define PERIODIC                // for debugging on a 486
#ifdef PERIODIC
#define TICK_PERIOD             (1000000000/1000)       // 1 millisecond
#else
#define TICK_PERIOD 20000
#endif

//------------------------------------eof---------------------------------------
#endif
