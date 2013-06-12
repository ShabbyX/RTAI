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

#ifndef _SA_USER
#define _SA_USER

#include "sa_sys.h"

EXTERN_FUNC(void C_BLK_1, ( ) );
EXTERN_FUNC(void rtai_scope, (struct STATUS_RECORD *INFO,
			    RT_DURATION           T,
			    RT_FLOAT              U[],
			    RT_INTEGER            NU,
			    RT_FLOAT              X[],
			    RT_FLOAT              XDOT[],
			    RT_INTEGER            NX,
			    RT_FLOAT              Y[],
			    RT_INTEGER            NY,
			    RT_FLOAT              RP[],
			    RT_INTEGER            IP[]) );

EXTERN_FUNC(void rtai_log, (struct STATUS_RECORD *INFO,
			    RT_DURATION           T,
			    RT_FLOAT              U[],
			    RT_INTEGER            NU,
			    RT_FLOAT              X[],
			    RT_FLOAT              XDOT[],
			    RT_INTEGER            NX,
			    RT_FLOAT              Y[],
			    RT_INTEGER            NY,
			    RT_FLOAT              RP[],
			    RT_INTEGER            IP[]) );

EXTERN_FUNC(void rtai_meter, (struct STATUS_RECORD *INFO,
			    RT_DURATION           T,
			    RT_FLOAT              U[],
			    RT_INTEGER            NU,
			    RT_FLOAT              X[],
			    RT_FLOAT              XDOT[],
			    RT_INTEGER            NX,
			    RT_FLOAT              Y[],
			    RT_INTEGER            NY,
			    RT_FLOAT              RP[],
			    RT_INTEGER            IP[]) );

EXTERN_FUNC(void rtai_led, (struct STATUS_RECORD *INFO,
			    RT_DURATION           T,
			    RT_FLOAT              U[],
			    RT_INTEGER            NU,
			    RT_FLOAT              X[],
			    RT_FLOAT              XDOT[],
			    RT_INTEGER            NX,
			    RT_FLOAT              Y[],
			    RT_INTEGER            NY,
			    RT_FLOAT              RP[],
			    RT_INTEGER            IP[]) );
#endif
