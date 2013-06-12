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
extern int rtRegisterComediDataIn( int, void * );
extern struct {
	int ID ;
	void *comdev ;
	} ComediDataIn[MAX_COMEDI_COMDEV];
extern int N_DATAIN;

struct ADCOMDev{
	int channel;
	char devName[20];
	unsigned int range;
	unsigned int aref;
	void * dev;
	int subdev;
	double range_min;
	double range_max;
};

void rtai_comedi_datain(
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

		struct ADCOMDev * comdev = (struct ADCOMDev *) malloc(sizeof(struct ADCOMDev));
		int n_channels;
		char board[50];
		int index;
		comedi_krange krange;


		comdev->channel = IP[0];
		sprintf(comdev->devName,"/dev/comedi%li",IP[3]);
		comdev->range = (unsigned int) IP[1];
		comdev->aref = IP[2];

		if( rtRegisterComediDataIn( 1000*(IP[3]+1)+IP[0] ,  (void *)comdev ) ){
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
			if ((comdev->subdev = comedi_find_subdevice_by_type(comdev->dev, COMEDI_SUBD_AI, 0)) < 0) {
				fprintf(stderr, "Comedi find_subdevice failed (No analog input)\n");
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
			comdev->subdev = comedi_find_subdevice_by_type(comdev->dev, COMEDI_SUBD_AI, 0);
		}
		//rt_sched_unlock();

		if ((n_channels = comedi_get_n_channels(comdev->dev, comdev->subdev)) < 0) {
			fprintf(stderr, "Comedi get_n_channels failed for subdevice %d\n", comdev->subdev);
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
		if ((comedi_get_krange(comdev->dev, comdev->subdev, comdev->channel, comdev->range, &krange)) < 0) {
			fprintf(stderr, "Comedi get range failed for subdevice %d\n", comdev->subdev);
			comedi_unlock(comdev->dev, comdev->subdev);
			comedi_close(comdev->dev);
			exit_on_error();
		}

		ComediDev_InUse[index]++;
		ComediDev_AIInUse[index]++;
		comdev->range_min = (double)(krange.min)*1.e-6;
		comdev->range_max = (double)(krange.max)*1.e-6;
		printf("AI Channel %d - Range : %1.2f [V] - %1.2f [V]\n", comdev->channel, comdev->range_min, comdev->range_max);
	}

	if(INFO->OUTPUTS)  {

		struct ADCOMDev * comdev;
		lsampl_t data, maxdata;
		double x;
		int i;


		for ( i=0 ; i<N_DATAIN; i++ ){
			if ( ComediDataIn[i].ID == 1000*(IP[3]+1)+IP[0] )
				break;
		}

		comdev = (struct ADCOMDev *) ComediDataIn[i].comdev;
		maxdata = comedi_get_maxdata(comdev->dev, comdev->subdev, comdev->channel);
		comedi_data_read(comdev->dev, comdev->subdev, comdev->channel, comdev->range, comdev->aref, &data);

		x = data;
		x /= maxdata;
		x *= (comdev->range_max - comdev->range_min);
		x += comdev->range_min;
		Y[0] = x;
	}

	INFO->INIT = FALSE;

	return;
}
