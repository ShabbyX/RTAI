/*
	COPYRIGHT (C) 2006      Mattia Mattaboni (mattaboni@aero.polimi.it)

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
	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
*/


#ifndef _RTMAIN_H_
#define _RTMAIN_H_

#define RTAILAB_VERSION   "3.4.6"
#define MAX_ADR_SRCH      500
#define MAX_NAME_SIZE     256
#define MAX_SCOPES        100
#define MAX_LOGS          100
#define MAX_LEDS          100
#define MAX_METERS        100
#define HZ                100
#define POLL_PERIOD       100 // millisecs
#define MAX_NTARGETS      1000

#define MBX_RTAI_SCOPE_SIZE  5000
#define MBX_RTAI_LOG_SIZE    5000
#define MBX_RTAI_METER_SIZE  5000
#define MBX_RTAI_LED_SIZE    5000

#define MAX_COMEDI_DEVICES     11
#define MAX_COMEDI_COMDEV     256

#endif /* _RTMAIN_H_ */
