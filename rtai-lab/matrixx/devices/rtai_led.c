/*
  COPYRIGHT (C) 2002-2006      Mattia Mattaboni (mattaboni@aero.polimi.it)

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


#include "sa_sys.h"
#include "sa_defn.h"
#include "sa_types.h"

#include <stdio.h>
#include <stdlib.h>
#include <rtai_netrpc.h>
#include <rtai_msg.h>
#include <rtai_mbx.h>
#include <string.h>

#include <stdio.h>
#include "rtmain.h"

#include "devices.h"


extern void exit_on_error(void);

extern int rtRegisterLed(const char *, int , int );
extern struct {
	char name[MAX_NAME_SIZE];
	int nleds;
	int ID;
	MBX *mbx;
	char MBXname[MAX_NAME_SIZE];
	} rtaiLed[MAX_LEDS];
extern int NLEDS;

void rtai_led(
#if (ANSI_PROTOTYPES)
	struct STATUS_RECORD *INFO,
	RT_DURATION  	      T,
	RT_FLOAT  	      U[],
	RT_INTEGER            NU,
	RT_FLOAT              X[],
	RT_FLOAT              XDOT[],
	RT_INTEGER            NX,
	RT_FLOAT              Y[],
	RT_INTEGER            NY,
	RT_FLOAT              RP[],
	RT_INTEGER            IP[]  )
#else
INFO,T,U,NU,X,XDOT,NX,Y,NY,RP,IP)
	struct STATUS_RECORD       *INFO   ;
	RT_DURATION  		T       ;
	RT_FLOAT  			U[]     ;
	RT_INTEGER			NU      ;
	RT_FLOAT  			X[]     ;
	RT_FLOAT  	       		XDOT[]  ;
	RT_INTEGER	       		NX      ;
	RT_FLOAT  	       		Y[]     ;
	RT_INTEGER	       		NY      ;
	RT_FLOAT                   RP[]    ;
	RT_INTEGER                 IP[]    ;
#endif
{
	if(INFO->INIT) {
		char ledName[10];
		int nleds = NU;

		sprintf(ledName,"LED-%ld",IP[0]);
		if ( rtRegisterLed(ledName,nleds,IP[0]) ){
			exit_on_error();
		}
	}
	if(INFO->OUTPUTS)  {
		unsigned int led_mask = 0;
		int nleds = NU;
		int i;

		for (i = 0; i < nleds; i++) {
		    if ( U[i] > 0.) {
		      led_mask += (1 << i);
		    } else {
		      led_mask += (0 << i);
		    }
		}

		for ( i=0 ; i<NLEDS; i++ ){
			if ( rtaiLed[i].ID == IP[0] )
				break;
		}

		RT_mbx_send_if(0, 0, rtaiLed[i].mbx, &led_mask, sizeof(led_mask));
	}
	INFO->INIT = FALSE;

	return;
}
