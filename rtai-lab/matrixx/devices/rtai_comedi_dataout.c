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

#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>

#include "sa_sys.h"
#include "sa_defn.h"
#include "sa_types.h"

#include <rtai_lxrt.h>
#include <rtai_comedi.h>

#include "rtmain.h"
#include "devices.h"


extern void exit_on_error(void);

extern void *ComediDev[];
extern int ComediDev_InUse[];
extern int ComediDev_AOInUse[];
extern int ComediDev_AIInUse[];
extern int rtRegisterComediDataOut( int, void * );
extern struct {
	int ID ;
	void *comdev ;
	} ComediDataOut[MAX_COMEDI_COMDEV];
extern int N_DATAOUT;

struct DACOMDev{
  int channel;
  char devName[20];
  unsigned int range;
  unsigned int aref;
  void * dev;
  int subdev;
  double range_min;
  double range_max;
};

void rtai_comedi_dataout(
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

		struct DACOMDev * comdev = (struct DACOMDev *) malloc(sizeof(struct DACOMDev));
		int n_channels;
		int index;
		char board[50];
		lsampl_t data, maxdata;
		comedi_krange krange;
		double s,u;


		comdev->channel = IP[0];
		sprintf(comdev->devName,"/dev/comedi%li",IP[3]);
		comdev->range = (unsigned int) IP[1];
		comdev->aref = IP[2];

		if ( rtRegisterComediDataOut( 1000*(IP[3]+1)+IP[0] , (void *)comdev ) ){
			exit_on_error();
		}
		index = IP[3];

		//rt_sched_lock();
		if (!ComediDev[index]) {
			comdev->dev = comedi_open(comdev->devName);
			if (!(comdev->dev)) {
				fprintf(stderr, "Comedi open failed\n");
				exit_on_error();
			}
			rt_comedi_get_board_name(comdev->dev, board);
			printf("COMEDI %s (%s) opened.\n\n", comdev->devName, board);
			ComediDev[index] = comdev->dev;
			if ((comdev->subdev = comedi_find_subdevice_by_type(comdev->dev, COMEDI_SUBD_AO, 0)) < 0) {
				fprintf(stderr, "Comedi find_subdevice failed (No analog output)\n");
				comedi_close(comdev->dev);
				exit_on_error();
			}
			//if ((comedi_lock(comdev->dev, comdev->subdev)) < 0) {
			//	fprintf(stderr, "Comedi lock failed for subdevice %d\n", comdev->subdev);
				//comedi_close(comdev->dev);
				//exit_on_error();
			//}
		} else {
			comdev->dev = ComediDev[index];
			comdev->subdev = comedi_find_subdevice_by_type(comdev->dev, COMEDI_SUBD_AO, 0);
		}

		//rt_sched_unlock();
		if ((n_channels = comedi_get_n_channels(comdev->dev, comdev->subdev)) < 0) {
			printf("Comedi get_n_channels failed for subdevice %d\n", comdev->subdev);
			comedi_unlock(comdev->dev, comdev->subdev);
			comedi_close(comdev->dev);
			exit_on_error();
		}
		if (comdev->channel >= n_channels) {
			fprintf(stderr, "Comedi channel not available for subdevice %d\n", comdev->subdev);
			comedi_unlock(comdev->dev, comdev->subdev);
			comedi_close(comdev->dev);
			exit_on_error();
		}
		maxdata = comedi_get_maxdata(comdev->dev, comdev->subdev, comdev->channel);
		if ((comedi_get_krange(comdev->dev, comdev->subdev, comdev->channel, comdev->range, &krange)) < 0) {
			fprintf(stderr, "Comedi get range failed for subdevice %d\n", comdev->subdev);
			comedi_unlock(comdev->dev, comdev->subdev);
			comedi_close(comdev->dev);
			exit_on_error();
		}

		ComediDev_InUse[index]++;
		ComediDev_AOInUse[index]++;
		comdev->range_min = (double)(krange.min)*1.e-6;
		comdev->range_max = (double)(krange.max)*1.e-6;
		printf("AO Channel %d - Range : %1.2f [V] - %1.2f [V]\n", comdev->channel, comdev->range_min, comdev->range_max);
		u = 0.;
		s = (u - comdev->range_min)/(comdev->range_max - comdev->range_min)*maxdata;
		data = (lsampl_t)(floor(s+0.5));
		comedi_data_write(comdev->dev, comdev->subdev, comdev->channel, comdev->range, comdev->aref, data);
	}
	if(INFO->OUTPUTS)  {

		struct DACOMDev *comdev;
		lsampl_t data, maxdata;
		double s;
		int i;


		for ( i=0 ; i<N_DATAOUT; i++ ){
			if ( ComediDataOut[i].ID == 1000*(IP[3]+1)+IP[0] )
				break;
		}

		comdev = (struct DACOMDev *) ComediDataOut[i].comdev;
		maxdata = comedi_get_maxdata(comdev->dev, comdev->subdev, comdev->channel);

		s = (U[0] - comdev->range_min)/(comdev->range_max - comdev->range_min)*maxdata;
		if (s < 0) {
			data = 0;
		} else if (s > maxdata) {
			data = maxdata;
		} else {
			data = (lsampl_t)(floor(s+0.5));
		}
		comedi_data_write(comdev->dev, comdev->subdev, comdev->channel, comdev->range, comdev->aref, data);

	}

	INFO->INIT = FALSE;

	return;
}
