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

extern int rtRegisterScope(const char *, int , int  );
extern struct {
	char name[MAX_NAME_SIZE];
	int ntraces;
	int ID;
	MBX *mbx;
	char MBXname[MAX_NAME_SIZE];
	} rtaiScope[MAX_SCOPES];
extern int NSCOPE;

void rtai_scope(
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
		char scopeName[10];
		int nch = NU;

		sprintf(scopeName,"SCOPE-%ld",IP[0]);
		if ( rtRegisterScope(scopeName,nch,IP[0])) {
			exit_on_error();
		}
	}
	if(INFO->OUTPUTS)  {
		int ntraces=NU;
		struct {
			float t;
			float u[ntraces];
		} data;
		int i;

		data.t=(float) T;
		for (i = 0; i < ntraces; i++) {
			data.u[i] = U[i];
		}
		for ( i=0 ; i<NSCOPE; i++ ){
			if ( rtaiScope[i].ID == IP[0] )
				break;
		}
		RT_mbx_send_if(0, 0, rtaiScope[i].mbx, &data, sizeof(data));
	}
	INFO->INIT = FALSE;

	return;
}
