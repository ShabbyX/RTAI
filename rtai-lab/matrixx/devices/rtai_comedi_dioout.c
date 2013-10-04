/*
	COPYRIGHT (C) 2002-2006      Mattia Mattaboni <mattaboni@aero.polimi.it>

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
extern int ComediDev_DIOInUse[];
extern int rtRegisterComediDioOut( int, void * );
extern struct {
	int ID ;
	void *comdev ;
	} ComediDioOut[MAX_COMEDI_COMDEV];
extern int N_DIOOUT;

struct DOCOMDev{
  int channel;
  char devName[20];
  void * dev;
  int subdev;
  int subdev_type;
  double threshold;
};


void rtai_comedi_dioout(
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

		struct DOCOMDev * comdev = (struct DOCOMDev *) malloc(sizeof(struct DOCOMDev));
		comdev->subdev_type = -1;
		int n_channels;
		char board[50];
		int  index;


		comdev->channel = IP[0];
		sprintf(comdev->devName,"/dev/comedi%li",IP[1]);
		comdev->threshold = RP[0];

		if ( rtRegisterComediDioOut( 1000*(IP[1]+1)+IP[0] , (void *)comdev ) ) {
			exit_on_error();
		}
		index = IP[1];

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

			if ((comdev->subdev = comedi_find_subdevice_by_type(comdev->dev, COMEDI_SUBD_DO, 0)) < 0) {
			//      fprintf(stderr, "Comedi find_subdevice failed (No digital I/O)\n");
			}else {
				comdev->subdev_type = COMEDI_SUBD_DO;
			}
			if(comdev->subdev == -1){
				if ((comdev->subdev = comedi_find_subdevice_by_type(comdev->dev, COMEDI_SUBD_DIO, 0)) < 0) {
					fprintf(stderr, "Comedi find_subdevice failed (No digital Output)\n");
					comedi_close(comdev->dev);
					exit_on_error();
				}else{
				comdev->subdev_type = COMEDI_SUBD_DIO;
				}
			}

		//	if ((comedi_lock(comdev->dev, comdev->subdev)) < 0) {
		//		fprintf(stderr, "Comedi lock failed for subdevice %d\n",comdev-> subdev);
				//comedi_close(comdev->dev);
				//exit_on_error();
		//	}
		} else {
			comdev->dev = ComediDev[index];
			if((comdev->subdev = comedi_find_subdevice_by_type(comdev->dev, COMEDI_SUBD_DO, 0)) < 0){
				comdev->subdev = comedi_find_subdevice_by_type(comdev->dev, COMEDI_SUBD_DIO, 0);
				comdev->subdev_type =COMEDI_SUBD_DIO;
			}else comdev->subdev_type =COMEDI_SUBD_DO;
		}
		//rt_sched_unlock();

		if ((n_channels = comedi_get_n_channels(comdev->dev, comdev->subdev)) < 0) {
			fprintf(stderr, "Comedi get_n_channels failed for subdevice %d\n", comdev->subdev);
			comedi_unlock(comdev->dev, comdev->subdev);
			comedi_close(comdev->dev);
			exit_on_error();
		}
		if (comdev->channel >= n_channels) {
			fprintf(stderr, "Comedi channel not available for subdevice %d\n",comdev-> subdev);
			comedi_unlock(comdev->dev, comdev->subdev);
			comedi_close(comdev->dev);
			exit_on_error();
		}

		if(comdev->subdev_type == COMEDI_SUBD_DIO){
			if ((comedi_dio_config(comdev->dev,comdev->subdev, comdev->channel, COMEDI_OUTPUT)) < 0) {
				fprintf(stderr, "Comedi DIO config failed for subdevice %d\n", comdev->subdev);
				comedi_unlock(comdev->dev, comdev->subdev);
				comedi_close(comdev->dev);
				exit_on_error();
			}
		}

		ComediDev_InUse[index]++;
		ComediDev_DIOInUse[index]++;
		comedi_dio_write(comdev->dev, comdev->subdev, comdev->channel, 0);
	}
	if(INFO->OUTPUTS)  {

		int i;
		struct DOCOMDev *comdev;
		unsigned int bit = 0;


		for ( i=0 ; i<N_DIOOUT; i++ ){
			if ( ComediDioOut[i].ID == 1000*(IP[1]+1)+IP[0] )
				break;
		}

		comdev = (struct DOCOMDev *) ComediDioOut[i].comdev;
		if (U[0] >= comdev->threshold) {
			bit = 1;
		}

		comedi_dio_write(comdev->dev,comdev->subdev, comdev->channel, bit);
	}

	INFO->INIT = FALSE;

	return;
}

