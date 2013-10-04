/*
	COPYRIGHT (C) 2002-2006 Lorenzo Dozio <dozio@aero.polimi.it>
				Paolo Mantegazza <mantegazza@aero.polimi.it>
				Roberto Bucher <roberto.bucher@supsi.ch>
				Mattia Mattaboni <mattaboni@aero.polimi.it>

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
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <signal.h>
#include <getopt.h>
#include <float.h>
#include <math.h>
#include <pthread.h>
#include <semaphore.h>

#include <sys/mman.h>
#include <sys/poll.h>

#include "rtai_sched.h"
#include <vxWorks.h>
#include <taskLib.h>
#include <msgQLib.h>
#include <semLib.h>
#include <sysLib.h>
#include <kernelLib.h>

#include "sa_types.h"
#include "appl.h"

#include <mxp_data.h>
#include <mxp.h>
#include <rtai_netrpc.h>
#include <rtai_msg.h>
#include <rtai_mbx.h>
#include <rtai_fifos.h>

#include <rtai_comedi.h>

#include <asm/rtai_atomic.h>

#include <rtmain.h>

#define rt_HostInterfaceTaskPriority	96
#define rt_MainTaskPriority	      97

#define MAX_DATA_SIZE           30

typedef struct rtTargetParamInfo {
	char modelName[MAX_NAME_SIZE];
	char blockName[MAX_NAME_SIZE];
	char paramName[MAX_NAME_SIZE];
	unsigned int nRows;
	unsigned int nCols;
	unsigned int dataType;
	unsigned int dataClass;
	double dataValue[MAX_DATA_SIZE];
} rtTargetParamInfo;

static sem_t err_sem;
static pthread_t  rt_HostInterfaceThread, rt_BaseRateThread;
static RT_TASK    *rt_MainTask, *rt_HostInterfaceTask, *rt_BaseRateTask;

static char *HostInterfaceTaskName = "IFTASK";
char *TargetMbxID                  = "RTS";
char *TargetLogMbxID               = "RTL";
char *TargetALogMbxID              = "RAL";
char *TargetLedMbxID               = "RTE";
char *TargetMeterMbxID	           = "RTM";
char *TargetSynchronoscopeMbxID    = "RTY";

static volatile int CpuMap        = 0xF;
static volatile int UseHRT        = 1;
static volatile int WaitToStart   = 0;
static volatile int isRunning     = 0;
static volatile int verbose       = 0;
static volatile int endBaseRate   = 0;
static volatile int endInterface  = 0;
static volatile int stackinc      = 100000;
static volatile int ClockTick     = 1;
static volatile int InternTimer   = 1;
static RTIME rt_BaseRateTick;
static float FinalTime            = 0.0;

volatile int endex;

double SIM_TIME;

struct {
	char name[MAX_NAME_SIZE];
	int ntraces;
	int ID; MBX *mbx;
	char MBXname[MAX_NAME_SIZE];
	} rtaiScope[MAX_SCOPES];

struct {
	char name[MAX_NAME_SIZE];
	int nrow, ncol;
	int ID ;
	MBX *mbx ;
	char MBXname[MAX_NAME_SIZE];
	} rtaiLogData[MAX_LOGS];

static struct {
	char name[MAX_NAME_SIZE];
	int nrow, ncol;
	} rtaiALogData[MAX_LOGS];

struct {
	char name[MAX_NAME_SIZE];
	int nleds;
	int ID;
	MBX *mbx;
	char MBXname[MAX_NAME_SIZE];
	} rtaiLed[MAX_LEDS];

struct {
	char name[MAX_NAME_SIZE];
	int ntraces;
	int ID;
	MBX *mbx;
	char MBXname[MAX_NAME_SIZE];
	} rtaiMeter[MAX_METERS];

#ifdef TASKDURATION
RTIME RTTSKinit=0, RTTSKper;
#endif

#define SS_DOUBLE  0
#define rt_SCALAR  0
#define rt_VECTOR  1
#define rt_MATRIX_ROW_MAJOR 2

#define msleep(t)  do { poll(0, 0, t); } while (0)

void *ComediDev[MAX_COMEDI_DEVICES];
int ComediDev_InUse[MAX_COMEDI_DEVICES] = {0};
int ComediDev_AIInUse[MAX_COMEDI_DEVICES] = {0};
int ComediDev_AOInUse[MAX_COMEDI_DEVICES] = {0};
int ComediDev_DIOInUse[MAX_COMEDI_DEVICES] = {0};

struct {
	int ID ;
	void *comdev ;
	} ComediDataIn[MAX_COMEDI_COMDEV];

struct {
	int ID ;
	void *comdev ;
	} ComediDataOut[MAX_COMEDI_COMDEV];

struct {
	int ID ;
	void *comdev ;
	} ComediDioIn[MAX_COMEDI_COMDEV];

struct {
	int ID ;
	void *comdev ;
	} ComediDioOut[MAX_COMEDI_COMDEV];

static void DummyWait(void) { }
static void DummySend(void) { }

extern const RT_DURATION MANAGER_TIMER_INTERVAL;
RT_DURATION get_tsamp(){
	return MANAGER_TIMER_INTERVAL;
}

extern int MXmain (void);
extern void SA_Output_To_File(void);
extern RT_INTEGER APPLICATION_Process_Event( enum event_type eventtype );
extern MSG_Q_ID qid_ovflow1;
extern char modelname[30];

int NSCOPE;
int NLOGS;
int NMETERS;
int NLEDS;
int N_DATAIN;
int N_DATAOUT;
int N_DIOIN;
int N_DIOOUT;
int OVERFLOW_LIMIT = 5;
char *DATA_FILE_LOCATION = "./";

static inline void strncpyz(char *dest, const char *src, int n)
{
	strncpy(dest, src, n);
	dest[n - 1] = '\0';
}

// the following 2 functions are unsafe, in theory, but can be used anyhow
// since it is very unlikely that two controllers will be started in parallel,
// moreover it is also possible to avoid using them

char *get_a_name(const char *root, char *name)
{
	unsigned long i;

	for (i = 0; i < MAX_ADR_SRCH; i++) {
		sprintf(name, "%s%ld", root, i);
		if (!rt_get_adr(nam2num(name))) {
			return name;
		}
	}
	return 0;
}

static void endme(int dummy)
{
	signal(SIGINT, endme);
	signal(SIGTERM, endme);
	endex = 1;
}

void exit_on_error()
{
	endme(0);
}

int rtRegisterScope(const char *name, int n, int ID )
{
	int i;
	int nscope;

	rt_sched_lock();
	nscope = NSCOPE++;
	rt_sched_unlock();

	if( nscope>= MAX_SCOPES ){
		fprintf(stderr,"Error: Scopes exceed %d \n",MAX_SCOPES);
		return -1;
	}
	if ( ID <= 0  )
		fprintf(stderr,"Warning: scopes ID must be positive\n");

	for (i = 0; i < MAX_SCOPES; i++) {
		if (rtaiScope[i].ID == ID)
			fprintf(stderr,"Warning: two or more scopes have the same ID(%d)\n",ID);
	}

	sprintf(rtaiScope[nscope].MBXname, "%s%d", TargetMbxID, nscope);
	rtaiScope[nscope].ntraces = n;
	rtaiScope[nscope].ID = ID;
	rtaiScope[nscope].mbx=  (MBX *) RT_typed_named_mbx_init(0,0,rtaiScope[nscope].MBXname,(MBX_RTAI_SCOPE_SIZE/((n+1)*sizeof(float)))*((n+1)*sizeof(float)),FIFO_Q);
	if(rtaiScope[nscope].mbx == NULL) {
		fprintf(stderr, "Cannot init mailbox\n");
		exit_on_error();
	}
	strncpyz(rtaiScope[nscope].name, name, MAX_NAME_SIZE);

	return 0;
}

int rtRegisterLed(const char *name, int n, int ID )
{
	int i;
	int nled;

	rt_sched_lock();
	nled = NLEDS++;
	rt_sched_unlock();

	if( nled >= MAX_LEDS ){
		fprintf(stderr,"Error: Leds exceed %d \n",MAX_LEDS);
		return -1;
	}

	if ( ID <= 0  )
		fprintf(stderr,"Warning: leds ID must be positive\n");

	for (i = 0; i < MAX_LEDS; i++) {
		if (rtaiLed[i].ID == ID)
			fprintf(stderr,"Warning: two or more leds have the same ID(%d)\n",ID);
	}

	sprintf(rtaiLed[nled].MBXname, "%s%d", TargetLedMbxID, nled);
	rtaiLed[nled].nleds= n;
	rtaiLed[nled].ID = ID;
	rtaiLed[nled].mbx=  (MBX *) RT_typed_named_mbx_init(0,0,rtaiLed[nled].MBXname,(MBX_RTAI_LED_SIZE/((n+1)*sizeof(float)))*((n+1)*sizeof(float)),FIFO_Q);
	if(rtaiLed[nled].mbx == NULL) {
		fprintf(stderr, "Cannot init mailbox\n");
		exit_on_error();
	}
	strncpyz(rtaiLed[nled].name, name, MAX_NAME_SIZE);

	return 0;
}

int rtRegisterMeter(const char *name, int n, int ID )
{
	int i;
	int nmeter;

	rt_sched_lock();
	nmeter = NMETERS++;
	rt_sched_unlock();

	if( nmeter >= MAX_METERS ){
		fprintf(stderr,"Error: Meters exceed %d \n",MAX_METERS);
		return -1;
	}

	if ( ID <= 0  )
		fprintf(stderr,"Warning: meters ID must be positive\n");

	for (i = 0; i < MAX_METERS; i++) {
		if (rtaiMeter[i].ID == ID)
			fprintf(stderr,"Warning: two or more meters have the same ID(%d)\n",ID);
	}

	sprintf(rtaiMeter[nmeter].MBXname, "%s%d", TargetMeterMbxID, nmeter);
	rtaiMeter[nmeter].ntraces = n;
	rtaiMeter[nmeter].ID = ID;
	rtaiMeter[nmeter].mbx=  (MBX *) RT_typed_named_mbx_init(0,0,rtaiMeter[nmeter].MBXname,(MBX_RTAI_METER_SIZE/((n+1)*sizeof(float)))*((n+1)*sizeof(float)),FIFO_Q);
	if(rtaiMeter[nmeter].mbx == NULL) {
		fprintf(stderr, "Cannot init mailbox\n");
		exit_on_error();
	}
	strncpyz(rtaiMeter[nmeter].name, name, MAX_NAME_SIZE);

	return 0;
}

int rtRegisterLogData(const char *name, int nrow, int ncol, int ID)
{
	int i;
	int nlogs;

	rt_sched_lock();
	nlogs = NLOGS++;
	rt_sched_unlock();

	if( nlogs >= MAX_LOGS ){
		fprintf(stderr,"Error: Logs exceed %d \n",MAX_LOGS);
		return -1;
	}

	if ( ID <= 0  )
		fprintf(stderr,"Warning: logs ID must be positive\n");

	for (i = 0; i < MAX_LOGS; i++) {
		if (rtaiLogData[i].ID == ID)
			fprintf(stderr,"Warning: two or more logs have the same ID(%d)\n",ID);
	}

	sprintf(rtaiLogData[nlogs].MBXname, "%s%d", TargetLogMbxID, nlogs);
	rtaiLogData[nlogs].nrow = nrow;
	rtaiLogData[nlogs].ncol = ncol;
	rtaiLogData[nlogs].ID = ID;
	rtaiLogData[nlogs].mbx=  (MBX *) RT_typed_named_mbx_init(0,0,rtaiLogData[nlogs].MBXname,(MBX_RTAI_LOG_SIZE/((ncol*nrow)*sizeof(float)))*((ncol*nrow)*sizeof(float)),FIFO_Q);
	if(rtaiLogData[nlogs].mbx == NULL) {
		fprintf(stderr, "Cannot init mailbox\n");
		exit_on_error();
	}
	strncpyz(rtaiLogData[nlogs].name, name, MAX_NAME_SIZE);

	return 0;
}

int rtRegisterComediDataIn( int ID , void *comdev )
{
	int i;
	int n_datain;

	rt_sched_lock();
	n_datain = N_DATAIN++;
	rt_sched_unlock();

	if( n_datain >= MAX_COMEDI_COMDEV ){
		fprintf(stderr,"Error: ComediDataIn exceed %d \n",MAX_COMEDI_COMDEV);
		return -1;
	}

	if ( ID <= 0  )
		fprintf(stderr,"Warning: ComediDataIn ID must be positive\n");

	for (i = 0; i < MAX_COMEDI_COMDEV; i++) {
		if (ComediDataIn[i].ID == ID){
			fprintf(stderr,"Warning: two or more ComediDataIn have the same ID(%d)\n",ID);
		}
	}

	ComediDataIn[n_datain].ID = ID;
	ComediDataIn[n_datain].comdev = comdev;

	return 0;
}

int rtRegisterComediDataOut( int ID , void *comdev )
{
	int i;
	int n_dataout;

	rt_sched_lock();
	n_dataout = N_DATAOUT++;
	rt_sched_unlock();

	if( n_dataout >= MAX_COMEDI_COMDEV ){
		fprintf(stderr,"Error: ComediDataOut exceed %d \n",MAX_COMEDI_COMDEV);
		return -1;
	}

	if ( ID <= 0  )
		fprintf(stderr,"Warning: ComediDataOut ID must be positive\n");

	for (i = 0; i < MAX_COMEDI_COMDEV; i++) {
		if (ComediDataOut[i].ID == ID){
			fprintf(stderr,"Warning: two or more ComediDataOut have the same ID(%d)\n",ID);
		}
	}

	ComediDataOut[n_dataout].ID = ID;
	ComediDataOut[n_dataout].comdev = comdev;

	return 0;
}

int rtRegisterComediDioIn( int ID , void *comdev )
{
	int i;
	int n_dioin;

	rt_sched_lock();
	n_dioin = N_DIOIN++;
	rt_sched_unlock();

	if( n_dioin >= MAX_COMEDI_COMDEV ){
		fprintf(stderr,"Error: ComediDioIn exceed %d \n",MAX_COMEDI_COMDEV);
		return -1;
	}

	if ( ID <= 0  )
		fprintf(stderr,"Warning: ComediDioIn ID must be positive\n");

	for (i = 0; i < MAX_COMEDI_COMDEV; i++) {
		if (ComediDioIn[i].ID == ID){
			fprintf(stderr,"Warning: two or more ComediDioIn have the same ID(%d)\n",ID);
		}
	}

	ComediDioIn[n_dioin].ID = ID;
	ComediDioIn[n_dioin].comdev = comdev;

	return 0;
}

int rtRegisterComediDioOut( int ID , void *comdev )
{
	int i;
	int n_dioout;

	rt_sched_lock();
	n_dioout = N_DIOOUT++;
	rt_sched_unlock();

	if( n_dioout >= MAX_COMEDI_COMDEV ){
		fprintf(stderr,"Error: ComediDioOut exceed %d \n",MAX_COMEDI_COMDEV);
		return -1;
	}

	if ( ID <= 0  )
		fprintf(stderr,"Warning: ComediDioOut ID must be positive\n");

	for (i = 0; i < MAX_COMEDI_COMDEV; i++) {
		if (ComediDioOut[i].ID == ID){
			fprintf(stderr,"Warning: two or more ComediDioOut have the same ID(%d)\n",ID);
		}
	}

	ComediDioOut[n_dioout].ID = ID;
	ComediDioOut[n_dioout].comdev = comdev;

	return 0;
}

static void grow_and_lock_stack(int inc)
{
	char c[inc];

	memset(c, 0, inc);
	mlockall(MCL_CURRENT | MCL_FUTURE);
}

static void (*WaitTimingEvent)(unsigned long);
static void (*SendTimingEvent)(unsigned long);
static unsigned long TimingEventArg;

#define XNAME(x,y)  x##y
#define NAME(x,y)   XNAME(x,y)

#define XSTR(x)    #x
#define STR(x)     XSTR(x)

double get_scicos_time()
{
	return(SIM_TIME);
}

static void *rt_BaseRate(void *args)
{
	char name[7];
	int i;
	static RTIME t0;

	for (i = 0; i < MAX_NTARGETS; i++) {
		sprintf(name,"BRT%d",i);
		if (!rt_get_adr(nam2num(name))) break;
	}
	if (!(rt_BaseRateTask = rt_task_init_schmod(nam2num(name), *((int *)args), 0, 0, SCHED_FIFO, CpuMap))) {
		fprintf(stderr,"Cannot init rt_BaseRateTask.\n");
		return (void *)1;
	}

	sem_post(&err_sem);

	iopl(3);
	rt_task_use_fpu(rt_BaseRateTask, 1);

	MXmain();
	grow_and_lock_stack(stackinc);
	if (UseHRT) {
		rt_make_hard_real_time();
	}

	rt_rpc(rt_MainTask, 0, (void *)name);
	t0 = rt_get_cpu_time_ns();
	rt_task_make_periodic(rt_BaseRateTask, rt_get_time() + rt_BaseRateTick, rt_BaseRateTick);
	while (!endBaseRate) {
#ifdef TASKDURATION
		RTTSKper=rt_get_cpu_time_ns()-RTTSKinit;
#endif
		WaitTimingEvent(TimingEventArg);

		if (endBaseRate) break;
		APPLICATION_Process_Event(TIME_EV);

		SIM_TIME = (rt_get_cpu_time_ns() - t0)*1.0E-9;
#ifdef TASKDURATION
		RTTSKinit=rt_get_cpu_time_ns();
#endif

	}
	if (UseHRT) {
		rt_make_soft_real_time();
	}

	rt_task_delete(rt_BaseRateTask);

	return 0;
}

static void *rt_HostInterface(void *args)
{
	RT_TASK *task;
	unsigned int Request;
	int Reply, len;
	char nome[8];

	if (!(rt_HostInterfaceTask = rt_task_init_schmod(nam2num(HostInterfaceTaskName), rt_HostInterfaceTaskPriority, 0, 0, SCHED_RR, 0xFF))) {
		fprintf(stderr,"Cannot init rt_HostInterfaceTask.\n");
		return (void *)1;
	}

	sem_post(&err_sem);

	while (!endInterface) {
		task = rt_receive(0, &Request);
		num2nam(rt_get_name(task),nome);
		if (endInterface) break;
		switch (Request & 0xFF) {
			case 'c': {
						int i ,Idx, idx[2];
						rtTargetParamInfo rtParam;
						float samplingTime;
						int NPAR,i1,i2;
						mxp_t pardata;
						double value;

						strncpyz(rtParam.modelName, modelname, MAX_NAME_SIZE);
						rtParam.dataType  = 0;

						NPAR = mxp_getnvars();

						rt_return(task, (isRunning << 16) | ( NPAR & 0xFFFF ));
						rt_receivex(task, &Request, 1, &len);
						rt_returnx(task, &rtParam, sizeof(rtParam));

						for (i = 0; i < NPAR; i++) {
							sprintf(rtParam.blockName,"%s/%s",rtParam.modelName,"Tunable Parameters");
							mxp_getvarbyidx(i, &pardata);
							if ( pardata.ndim == 0 ){
								rt_receivex(task,&Request,1,&len);
								sprintf(rtParam.paramName, pardata.name);
								mxp_getparam(i,idx, &value);
								rtParam.dataValue[0] = value;
								rtParam.dataClass = rt_SCALAR;
								rtParam.nRows = 1;
								rtParam.nCols = 1;
								rt_returnx(task, &rtParam, sizeof(rtParam));
							}
							if ( pardata.ndim == 1 ){
								rt_receivex(task,&Request,1,&len);
								sprintf(rtParam.paramName, pardata.name);
								rtParam.dataClass = rt_VECTOR;
								rtParam.nRows = 1;
								rtParam.nCols = pardata.dim[0];
								for (i1 = 0; i1 < pardata.dim[0] ; i1++){
									idx[0] = i1;
									mxp_getparam(i,idx, &value);
									rtParam.dataValue[i1] = value;
								}
								rt_returnx(task, &rtParam, sizeof(rtParam));
							}
							if ( pardata.ndim == 2 ){
								rt_receivex(task,&Request,1,&len);
								sprintf(rtParam.paramName, pardata.name);
								rtParam.dataClass = rt_MATRIX_ROW_MAJOR;
								rtParam.nRows = pardata.dim[0];
								rtParam.nCols = pardata.dim[1];
								for (i1 = 0; i1 < pardata.dim[0] ; i1++){
									for (i2 = 0; i2 < pardata.dim[1] ; i2++){
										idx[0] = i1;
										idx[1] = i2;
										mxp_getparam(i,idx, &value);
										rtParam.dataValue[i1*rtParam.nCols+i2] = value;
									}
								}
								rt_returnx(task, &rtParam, sizeof(rtParam));
							}
							if (pardata.ndim > 2){
								fprintf(stderr,"MAX PARAMETER DIMESION = 2........\n");
							}
						}

						while (1) {
							rt_receivex(task, &Idx, sizeof(int), &len);
							if (Idx < 0) {
								rt_returnx(task, &Idx, sizeof(int));
								break;
							} else {
								rt_returnx(task, &rtaiScope[Idx].ntraces, sizeof(int));
								rt_receivex(task, &Idx, sizeof(int), &len);
								rt_returnx(task, rtaiScope[Idx].name, MAX_NAME_SIZE);
								rt_receivex(task, &Idx, sizeof(int), &len);
								samplingTime = get_tsamp();
								rt_returnx(task, &samplingTime, sizeof(float));
							}
						}
						while (1) {
							rt_receivex(task, &Idx, sizeof(int), &len);
							if (Idx < 0) {
								rt_returnx(task, &Idx, sizeof(int));
								break;
							} else {
								rt_returnx(task, &rtaiLogData[Idx].nrow, sizeof(int));
								rt_receivex(task, &Idx, sizeof(int), &len);
								rt_returnx(task, &rtaiLogData[Idx].ncol, sizeof(int));
								rt_receivex(task, &Idx, sizeof(int), &len);
								rt_returnx(task, rtaiLogData[Idx].name, MAX_NAME_SIZE);
								rt_receivex(task, &Idx, sizeof(int), &len);
								samplingTime = get_tsamp();
								rt_returnx(task, &samplingTime, sizeof(float));
							}
						}
						while (1) {
							rt_receivex(task, &Idx, sizeof(int), &len);
							if (Idx < 0) {
								rt_returnx(task, &Idx, sizeof(int));
								break;
							} else {
								rt_returnx(task, &rtaiALogData[Idx].nrow, sizeof(int));
								rt_receivex(task, &Idx, sizeof(int), &len);
								rt_returnx(task, &rtaiALogData[Idx].ncol, sizeof(int));
								rt_receivex(task, &Idx, sizeof(int), &len);
								rt_returnx(task, rtaiALogData[Idx].name, MAX_NAME_SIZE);
								rt_receivex(task, &Idx, sizeof(int), &len);
								samplingTime = get_tsamp();
								rt_returnx(task, &samplingTime, sizeof(float));
							}
						}
						while (1) {
							rt_receivex(task, &Idx, sizeof(int), &len);
							if (Idx < 0) {
								rt_returnx(task, &Idx, sizeof(int));
								break;
							} else {
								rt_returnx(task, &rtaiLed[Idx].nleds, sizeof(int));
								rt_receivex(task, &Idx, sizeof(int), &len);
								rt_returnx(task, rtaiLed[Idx].name, MAX_NAME_SIZE);
								rt_receivex(task, &Idx, sizeof(int), &len);
								samplingTime = get_tsamp();
								rt_returnx(task, &samplingTime, sizeof(float));
							}
						}
						while (1) {
							rt_receivex(task, &Idx, sizeof(int), &len);
							if (Idx < 0) {
								rt_returnx(task, &Idx, sizeof(int));
								break;
							} else {
								rt_returnx(task, rtaiMeter[Idx].name, MAX_NAME_SIZE);
								rt_receivex(task, &Idx, sizeof(int), &len);
								samplingTime = get_tsamp();
								rt_returnx(task, &samplingTime, sizeof(float));
							}
						}
						while (1) {
							rt_receivex(task, &Idx, sizeof(int), &len);
							if (Idx < 0) {
								rt_returnx(task, &Idx, sizeof(int));
								break;
							} else {
								rt_returnx(task, "", MAX_NAME_SIZE);
								rt_receivex(task, &Idx, sizeof(int), &len);
								samplingTime = get_tsamp();
								rt_returnx(task, &samplingTime, sizeof(float));
							}
						}
						break;
					}
			case 's': {
						rt_task_resume(rt_MainTask);
						rt_return(task, 1);
						break;
					}
			case 't': {
						endex = 1;
						rt_return(task, 0);
						break;
					}
			case 'p': {
						int index;
						double param;
						int mat_ind,Idx[2];
						mxp_t pardata;

						rt_return(task, isRunning);
						rt_receivex(task, &index, sizeof(int), &len);
						Reply = 0;
						rt_returnx(task, &Reply, sizeof(int));
						rt_receivex(task, &param, sizeof(double), &len);
						Reply = 1;
						rt_returnx(task, &Reply, sizeof(int));
						rt_receivex(task, &mat_ind, sizeof(int), &len);
						mxp_getvarbyidx(index, &pardata);
						if ( pardata.ndim == 1 ) Idx[0] = mat_ind;
						if ( pardata.ndim == 2 ){
								Idx[0] = mat_ind/pardata.dim[1];
								Idx[1] = mat_ind - Idx[0]*pardata.dim[1];
						}
						mxp_setparam(index, Idx, param);
						rt_returnx(task, &Reply, sizeof(int));
						break;
					}
			case 'g': {
						int i, idx[2], i1,i2;
						rtTargetParamInfo rtParam;
						int NPAR;
						mxp_t pardata;
						double value;

						strncpyz(rtParam.modelName, modelname, MAX_NAME_SIZE);
						rtParam.dataType  = 0;

						NPAR = mxp_getnvars();
						rt_return(task, isRunning);
						for (i = 0; i < NPAR; i++) {
							sprintf(rtParam.blockName,"%s/%s",rtParam.modelName,"Tunable Parameters");
							mxp_getvarbyidx(i, &pardata);
							if ( pardata.ndim == 0 ){
								rt_receivex(task,&Request,1,&len);
								sprintf(rtParam.paramName, pardata.name);
								mxp_getparam(i,idx, &value);
								rtParam.dataValue[0] = value;
								rtParam.dataClass = rt_SCALAR;
								rtParam.nRows = 1;
								rtParam.nCols = 1;
								rt_returnx(task, &rtParam, sizeof(rtParam));
							}
							if ( pardata.ndim == 1 ){
								rt_receivex(task,&Request,1,&len);
								sprintf(rtParam.paramName, pardata.name);
								rtParam.dataClass = rt_VECTOR;
								rtParam.nRows = 1;
								rtParam.nCols = pardata.dim[0];
								for (i1 = 0; i1 < pardata.dim[0] ; i1++){
									idx[0] = i1;
									mxp_getparam(i,idx, &value);
									rtParam.dataValue[i1] = value;
								}
								rt_returnx(task, &rtParam, sizeof(rtParam));
							}
							if ( pardata.ndim == 2 ){
								rt_receivex(task,&Request,1,&len);
								sprintf(rtParam.paramName, pardata.name);
								rtParam.dataClass = rt_MATRIX_ROW_MAJOR;
								rtParam.nRows = pardata.dim[0];
								rtParam.nCols = pardata.dim[1];
								for (i1 = 0; i1 < pardata.dim[0] ; i1++){
									for (i2 = 0; i2 < pardata.dim[1] ; i2++){
										idx[0] = i1;
										idx[1] = i2;
										mxp_getparam(i,idx, &value);
										rtParam.dataValue[i1*rtParam.nCols+i2] = value;
									}
								}
								rt_returnx(task, &rtParam, sizeof(rtParam));
							}
						}
						break;
					}
			case 'd': {
						int ParamCnt;
						int Idx[2];
						mxp_t pardata;

						rt_return(task, isRunning);
						rt_receivex(task, &ParamCnt, sizeof(int), &len);
						Reply = 0;
						rt_returnx(task, &Reply, sizeof(int));
						{
							struct {
									int index;
									int mat_ind;
									double value;
									} Params[ParamCnt];
							int i;

							rt_receivex(task, &Params, sizeof(Params), &len);
							for (i = 0; i < ParamCnt; i++) {
								mxp_getvarbyidx(Params[i].index, &pardata);
								if ( pardata.ndim == 1 ) Idx[0] = Params[i].mat_ind;
								if ( pardata.ndim == 2 ){
									Idx[0] = Params[i].mat_ind/pardata.dim[1];
									Idx[1] = Params[i].mat_ind - Idx[0]*pardata.dim[1];
								}
								mxp_setparam(Params[i].index, Idx, Params[i].value);
							}
						}
						Reply = 1;
						rt_returnx(task, &Reply, sizeof(int));
						break;
					}
			case 'm': {
						float time = SIM_TIME;
						rt_return(task, isRunning);
						rt_receivex(task, &Reply, sizeof(int), &len);
						rt_returnx(task, &time, sizeof(float));
						break;
					}
			case 'b': {
						rt_return(task, (unsigned int)rt_BaseRateTask);
						break;
					}
			default : {
						break;
					}
		}
	}
	rt_task_delete(rt_HostInterfaceTask);
	return 0;
}

static int rt_Main(int priority)
{
	SEM *hard_timers_cnt;
	char name[7];
	RTIME rt_BaseTaskPeriod;
	struct timespec err_timeout;
	int i;

	rt_allow_nonroot_hrt();

	for (i = 0; i < MAX_NTARGETS; i++) {
		sprintf(name,"MNT%d",i);
		if (!rt_get_adr(nam2num(name))) break;
	}

	if (!(rt_MainTask = rt_task_init_schmod(nam2num(name), rt_MainTaskPriority, 0, 0, SCHED_RR, 0xFF))) {
		fprintf(stderr,"Cannot init rt_MainTask.\n");
		return 1;
	}
	sem_init(&err_sem, 0, 0);

	printf("TARGET STARTS.\n");
	pthread_create(&rt_HostInterfaceThread, NULL, rt_HostInterface, NULL);
	err_timeout.tv_sec = (long int)(time(NULL)) + 1;
	err_timeout.tv_nsec = 0;
	if ((sem_timedwait(&err_sem, &err_timeout)) != 0) {
		fprintf(stderr, "Target is terminated.\n");
		goto finish;
	}

	pthread_create(&rt_BaseRateThread, NULL, rt_BaseRate, &priority);
	err_timeout.tv_sec = (long int)(time(NULL)) + 1;
	err_timeout.tv_nsec = 0;
	if ((sem_timedwait(&err_sem, &err_timeout)) != 0) {
		endInterface = 1;
		rt_send(rt_HostInterfaceTask, 0);
		pthread_join(rt_HostInterfaceThread, NULL);
		fprintf(stderr, "Target is terminated.\n");
		goto finish;
	}

	rt_BaseTaskPeriod = (RTIME) (1e9*get_tsamp());
	if (InternTimer) {
		WaitTimingEvent = (void *)rt_task_wait_period;
		if (!(hard_timers_cnt = rt_get_adr(nam2num("HTMRCN")))) {
			if (!ClockTick) {
				rt_set_oneshot_mode();
				start_rt_timer(0);
				rt_BaseRateTick = nano2count(rt_BaseTaskPeriod);
			}
			else {
				rt_set_periodic_mode();
				rt_BaseRateTick = start_rt_timer(nano2count(rt_BaseTaskPeriod));
			}
			hard_timers_cnt = rt_sem_init(nam2num("HTMRCN"), 0);
		}
		else {
			rt_BaseRateTick = nano2count(rt_BaseTaskPeriod);
			rt_sem_signal(hard_timers_cnt);
		}
	}
	else {
		WaitTimingEvent = (void *)DummyWait;
		SendTimingEvent = (void *)DummySend;
	}

	if (verbose) {
		printf("Model : %s .\n", modelname);
		printf("Executes on CPU map : %x.\n", CpuMap);
		printf("Sampling time : %e (s).\n", get_tsamp());
	}
	{
	int msg;

	rt_receive(0, &msg);
	}

	if (WaitToStart) {
		if (verbose) {
			printf("Target is waiting to start ... ");
			fflush(stdout);
		}
		rt_task_suspend(rt_MainTask);
	}

	if (verbose) {
		printf("Target is running.\n");
	}
	rt_return(rt_BaseRateTask, 0);
	isRunning = 1;

	while (!endex && (!FinalTime || SIM_TIME < FinalTime)) {
		msleep(POLL_PERIOD);
	}
	endBaseRate = 1;
	if (!InternTimer) {
		SendTimingEvent(TimingEventArg);
	}
	pthread_join(rt_BaseRateThread, NULL);
	isRunning = 0;
	endInterface = 1;
	rt_send(rt_HostInterfaceTask, 0);
	if (verbose) {
		printf("Target has been stopped.\n");
	}
	pthread_join(rt_HostInterfaceThread, NULL);
	if (InternTimer) {
		if (!rt_sem_wait_if(hard_timers_cnt)) {
			rt_sem_delete(hard_timers_cnt);
		}
	}

	finish:
    		for (i=0 ; i<NSCOPE ; i++)
				RT_named_mbx_delete(0, 0, rtaiScope[i].mbx);
    		for (i=0 ; i<NLOGS ; i++)
				RT_named_mbx_delete(0, 0, rtaiLogData[i].mbx);
    		for (i=0 ; i<NLEDS ; i++)
				RT_named_mbx_delete(0, 0, rtaiLed[i].mbx);
    		for (i=0 ; i<NMETERS ; i++)
				RT_named_mbx_delete(0, 0, rtaiMeter[i].mbx);

			for ( i=0 ; i<MAX_COMEDI_DEVICES ; i++ ){
				if ( ComediDev[i] != NULL ){
					comedi_close(ComediDev[i]);
				}
			}

			for ( i=0 ; i<N_DATAIN ; i++){
				free( ComediDataIn[i].comdev );
			}
			for ( i=0 ; i<N_DATAOUT ; i++){
				free( ComediDataOut[i].comdev );
			}
			for ( i=0 ; i<N_DIOIN ; i++){
				free( ComediDioIn[i].comdev );
			}
			for ( i=0 ; i<N_DIOOUT ; i++){
				free( ComediDioOut[i].comdev );
			}

			SA_Output_To_File();
			rt_task_delete(rt_MainTask);
			printf("TARGET ENDS.\n");

	return 0;

}

struct option options[] = {
	{ "usage",      0, 0, 'u' },
	{ "verbose",    0, 0, 'v' },
	{ "soft",       0, 0, 's' },
	{ "wait",       0, 0, 'w' },
	{ "priority",   1, 0, 'p' },
	{ "finaltime",  1, 0, 'f' },
	{ "name",       1, 0, 'n' },
	{ "idscope",    1, 0, 'i' },
	{ "idlog",      1, 0, 'l' },
	{ "idalog",     1, 0, 'a' },
	{ "idmeter",    1, 0, 't' },
	{ "idled",      1, 0, 'd' },
	{ "idsynch",    1, 0, 'y' },
	{ "cpumap",     1, 0, 'c' },
	{ "external",   0, 0, 'e' },
	{ "oneshot",    0, 0, 'o' },
	{ "stack",      1, 0, 'm' }
};

void print_usage(void)
{
	fputs(
	("\nUsage:  'RT-model-name' [OPTIONS]\n"
	 "\n"
	 "OPTIONS:\n"
	 "  -u, --usage\n"
	 "      print usage\n"
	 "  -v, --verbose\n"
	 "      verbose output\n"
	 "  -V, --version\n"
	 "      print rtmain version\n"
	 "  -s, --soft\n"
	 "      run RT-model in soft real time (default hard RT)\n"
	 "  -w, --wait\n"
	 "      wait to start\n"
	 "  -p <priority>, --priority <priority>\n"
	 "      set the priority at which the RT-model's highest priority task will run (default 0)\n"
	 "  -f <finaltime>, --finaltime <finaltime>\n"
	 "      set the final time (default infinite)\n"
	 "  -n <ifname>, --name <ifname>\n"
	 "      set the name of the host interface task (default IFTASK)\n"
	 "  -i <scopeid>, --idscope <scopeid>\n"
	 "      set the scope mailboxes identifier (default RTS)\n"
	 "  -l <logid>, --idlog <logid>\n"
	 "      set the log mailboxes identifier (default RTL)\n"
	 "  -a <alogid>, --idalog <logid>\n"
	 "      set the alog mailboxes identifier (default RAL)\n"
	 "  -t <meterid>, --idmeter <meterid>\n"
	 "      set the meter mailboxes identifier (default RTM)\n"
	 "  -d <ledid>, --idled <ledid>\n"
	 "      set the led mailboxes identifier (default RTE)\n"
	 "  -y <synchid>, --idsynch <synchid>\n"
	 "      set the synchronoscope mailboxes identifier (default RTY)\n"
	 "  -c <cpumap>, --cpumap <cpumap>\n"
	 "      (1 << cpunum) on which the RT-model runs (default: let RTAI choose)\n"
	 "  -e, --external\n"
	 "      RT-model timed by an external resume (default internal)\n"
	 "  -o, --oneshot\n"
	 "      the hard timer will run in oneshot mode (default periodic)\n"
	 "  -m <stack>, --stack <stack>\n"
	 "      set a guaranteed stack size extension (default 30000)\n"
	 "  -O <OverFlowLimit>, --oflimit <OverFlowLimit>\n"
	 "      set the number of clock cycle to be not exceeded (default 5)\n"
	 "  -P <PathName>, --path <PathName>\n"
	 "      set the path for the file IO (default ""./"")\n"
	 "\n")
	,stderr);
	exit(0);
}

int main(int argc, char *argv[])
{
	extern char *optarg;
	int c, donotrun = 0, priority = 0;

	signal(SIGINT, endme);
	signal(SIGTERM, endme);

	modelname[strlen(modelname)-4] = '\0';

	do {
		c = getopt_long(argc, argv, "euvVswop:f:m:n:i:l:a:t:d:y:c:O:P:", options, NULL);
		switch (c) {
			case 'c':
					if ((CpuMap = atoi(optarg)) <= 0) {
						fprintf(stderr, "-> Invalid CPU map.\n");
						donotrun = 1;
					}
					break;
			case 'e':
					InternTimer = 0;
					break;
			case 'f':
					if (strstr(optarg, "inf")) {
						FinalTime = 0.0;
					} else if ((FinalTime = atof(optarg)) <= 0.0) {
						fprintf(stderr, "-> Invalid final time.\n");
						donotrun = 1;
					}
					break;
			case 'i':
					TargetMbxID = strdup(optarg);
					break;
			case 'l':
					TargetLogMbxID = strdup(optarg);
					break;
			case 'a':
					TargetALogMbxID = strdup(optarg);
					break;
			case 't':
					TargetMeterMbxID = strdup(optarg);
					break;
			case 'd':
					TargetLedMbxID = strdup(optarg);
					break;
			case 'y':
					TargetSynchronoscopeMbxID = strdup(optarg);
					break;
			case 'm':
					if ((stackinc = atoi(optarg)) < 0 ) {
						fprintf(stderr, "-> Invalid stack expansion.\n");
						donotrun = 1;
					}
					break;
			case 'n':
					HostInterfaceTaskName = strdup(optarg);
					break;
			case 'p':
					if ((priority = atoi(optarg)) < 0) {
						fprintf(stderr, "-> Invalid priority value.\n");
						donotrun = 1;
					}
					break;
			case 's':
					UseHRT = 0;
					break;
			case 'o':
					ClockTick = 0;
					break;
			case 'u':
					print_usage();
					break;
			case 'V':
					fprintf(stderr, "Version %s.\n", RTAILAB_VERSION);
					return 0;
					break;
			case 'v':
					verbose = 1;
					break;
			case 'w':
					WaitToStart = 1;
					break;
			case 'O':
					if ((OVERFLOW_LIMIT = atoi(optarg)) <= 0) {
						fprintf(stderr, "-> Invalid OverFlow limit.\n");
						donotrun = 1;
					}
					break;
			case 'P':
					DATA_FILE_LOCATION = strdup(optarg);
					break;
			default:
					if (c >= 0) {
						donotrun = 1;
					}
					break;
		}
	} while (c >= 0);

	if (verbose) {
		printf("\nTarget settings\n");
		printf("===============\n");
		printf("  Real-time : %s\n", UseHRT ? "HARD" : "SOFT");
		printf("  Timing    : %s / ", InternTimer ? "internal" : "external");
		printf("%s\n", ClockTick ? "periodic" : "oneshot");
		printf("  Priority  : %d\n", priority);
		if (FinalTime > 0) {
			printf("  Finaltime : %f [s]\n", FinalTime);
		} else {
			printf("  Finaltime : RUN FOREVER\n");
		}
		printf("  CPU map   : %x\n\n", CpuMap);
	}
	if (donotrun) {
		printf("ABORTED BECAUSE OF EXECUTION OPTIONS ERRORS.\n");
		return 1;
	}
	return rt_Main(priority);
}
