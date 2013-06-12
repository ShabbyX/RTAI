/*
COPYRIGHT (C) 2001-2006  Paolo Mantegazza  (mantegazza@aero.polimi.it)
			 Giuseppe Quaranta (quaranta@aero.polimi.it)

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
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA.
*/


#ifndef _VXW_WXWORKS_H_
#define _VXW_WXWORKS_H_

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#ifndef  OK
#define OK      (0)
#endif

#ifndef ERROR
#define ERROR  (-1)
#endif

typedef void (*FUNCPTR)(long, ...);
typedef long STATUS;

#define NO_WAIT          (0)
#define WAIT_FOREVER     (-1)

#endif
