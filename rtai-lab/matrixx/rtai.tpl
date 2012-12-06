@*
Mattia Mattaboni (mattaboni@aero.polimi.it) reworked this file for use within
RTAI-Lab.
@*

@* AutoCode_LicensedWork -----------------------------------------------------

This product is protected by the National Instruments License Agreement
which is located at: http://www.ni.com/legal/license.

(c) 1987-2004 National Instruments Corporation. All rights reserved.

----------------------------------------------------------------------------*@
@*
------------------------------------------------------------------------------
--
--  File       : rtai.tpl
--
--  Abstract:
--
--  This template is designed to produce C executive code for real-time
--  execution in a RTAI environment.
--
******************************************************************************
*@

@INT i,j@  @/ Global variables, mainly used for loop counters.

@INT pe_num_i@@/ Processor number ( = Processor scope id + 1 )
@INC "c_intgr.tpl"@
@INC "c_api.tpl"@@



@*

  This main segment defines the control flow of code generation. It invokes
  library tpl functions and other tpl functions defined below. User can
  customize any of the segments defined in this template or implement new
  segments (tpl functions) thereby controlling the output of the code
  generator.

*@

@SEGMENT MAIN()@@
@ASSERT STRCMP("C",language_s)@@
@*  @ASSERT not multiprocessor_b@@  *@
@gen_info()@
@SCOPE PROCESSOR 0@@pe_num_i = pe_id_i plus 1@@
@gen_includes(1)@
@IFF not procs_only_b@@*Scheduled system only*@
@/ Uncomment the following tpl statement if structure map needs to be output.
@/@struct_map()@
@ENDIFF@@

@gen_systemdata()@
@gen_procs_decl()@
@gen_procs_defn()@
@IFF not procs_only_b@@
@gen_tasks_decl()@
@gen_tasks_defn()@
@ENDIFF@@/Scheduled system only
@gen_systemdata_init()@
@IFF not procs_only_b@@
@gen_scheduler()@
@ENDIFF@@/Scheduled system only
@
@bestapi_create_api()@
@gen_objlist()@
@
@IFF procs_only_b@
@gen_proc_ucbwrapper_decl()@@
@gen_proc_ucbwrapper_defn()@@
#ifndef SBUSER

@gen_proc_subsyswrapper_decl()@@
@gen_proc_subsyswrapper_defn()@@

#endif /* SBUSER */
@ENDIFF@@
@
@FILEOPEN("stdout", "append")@@
Output generated in @output_fname_s@.

@FILECLOSE@@
@gen_best_code()@@
@gen_applDOTh()@@
@FILEOPEN("stdout", "append")@@
generating appl.h ...............

@
@ENDSEGMENT@@

@////////////////////////////////////////////////////////////////////

@*
  This segment generates the banner in the generated code.
*@

@SEGMENT gen_info() INT k@@
/****************************************************************************
|                   AutoCode/C (TM) Code Generator V7.x
|                NATIONAL INSTRUMENTS CORP.,  AUSTIN, TEXAS
*****************************************************************************
rtf filename           : @rtf_fname_s@
Filename               : @currentfname_s@
Dac filename           : @dac_fname_s@
Generated on           : @currentdate_s@
Dac file created on    : @dac_createdate_s@
@IFF not procs_only_b@@/Scheduled system only
*****************************************************************************
** The APPLICATION module has a fixed interface as shown here
** and is provided in the header file appl.h
**
** extern void APPLICATION_Get_Params (RT_FLOAT    *TimerInterruptFrequency,
**                                     RT_FLOAT    **ext_in,
**                                     RT_INTEGER  *num_in,
**                                     RT_FLOAT    **ext_out,
**                                     RT_INTEGER  *num_out,
**                                     unsigned long *max_taskprio,
**                                     unsigned long *min_taskprio);
**
** extern RT_INTEGER       APPLICATION_Start(void);
**
** extern RT_INTEGER       APPLICATION_Stop(void);
**
** extern RT_INTEGER       APPLICATION_Restart(void);
**
** extern RT_INTEGER       APPLICATION_Shut_Down(void);
**
** enum event_type { INTERNAL_EV, ERROR_EV, TIME_EV, name_EV,
**                   NUM_EVENTS };
** extern RT_INTEGER       APPLICATION_Process_Event(enum event_type);
**
** extern RT_INTEGER       APPLICATION_response_state[NUM_EVENTS];
**      NON_PREEMPTABLE : event is being processed (set by event originator ISR)
**      PREEMPTABLE: event processing completed (set by the application)
**
** extern RT_INTEGER       APPLICATION_manager_status;
*****************************************************************************
--
--   Number of External Inputs : @numin_i@
--   Number of External Outputs: @numout_i@
--
--   Scheduler Frequency:    @schedulerfreq_r@
--
--   SUBSYSTEM  FREQUENCY  TIME_SKEW  OUTPUT_TIME  TASK_TYPE  PROCESSOR NUM
--   ---------  ---------  ---------  -----------  ---------  -------------
@LOOPP i=0, i lt ntasks_i, i=i plus 1@@
@j = i plus 1@@k = processorid_li[i] plus 1@@
--@5@ @j@ @16@ @ssfreq_lr[i]@ @27@ @ssskew_lr[i]@ @38@ @ssoptime_lr[i]@ @51@ @sstypes_ls[i]@ @71@ @k@
@ENDLOOPP@
@ENDIFF@
*/
@ENDSEGMENT@

@////////////////////////////////////////////////////////////////////

@*
  This segment generates the #includes in the generated code.
*@

@SEGMENT gen_includes(incall)@
#include <vxWorks.h>
#include <taskLib.h>
#include <msgQLib.h>
#include <semLib.h>
#include <sysLib.h>
#include <kernelLib.h>

#include <stdio.h>
#include <math.h>

#include "sa_sys.h"
#include "sa_defn.h"
#include "sa_types.h"
#include "sa_math.h"
#include "sa_utils.h"
#include "sa_fuzzy.h"

#include "appl.h"

#include "mxp_data.h"
#include "mxp.h"

#include "devices.h"

@IFF fixpt_math_b@@
#include "sa_fxr.h"
#include "sa_fxm.h"
#include "sa_fxmp.h"
@ENDIFF@@


@ENDSEGMENT@

@////////////////////////////////////////////////////////////////////

@*
  This segment generates the global data declarations and definitions.
*@

@SEGMENT gen_systemdata() INT k@
/* {{Frag Header Info Begin}} */
@/generate BEST charts header file includes
@IFF nsystem_best_charts_i@@
/*** BEST Chart header files ***/
@  LOOPP k=0, k lt nsystem_best_charts_i, k=k plus 1@@
@*   *@#include "@system_best_filebasenames_ls[k]@.h"
@  ENDLOOPP@@
@ENDIFF@@


/*** System Data ***/

@declare_dczeros()@
@define_dczeros()@
@declare_globals()@

const RT_INTEGER               MAX_PROCESSOR    = @nprocessors_i@;
const RT_INTEGER               MY_PROCESSOR     = @pe_num_i@;

#ifdef ABSTOL
#undef ABSTOL
#endif
#define ABSTOL                        @epsilon_r@

#ifdef XREMAP
#undef XREMAP
#endif
#define XREMAP                        @noicmap_b@

#ifdef EPSILON
#undef EPSILON
#endif
#define EPSILON                       @epsilon_r@

#ifdef EPS
#undef EPS
#endif
const RT_FLOAT                        isi_EPS            = 4.0f*EPSILON;
#define EPS                           isi_EPS

#ifdef PI
#undef PI
#endif
const RT_FLOAT                        isi_PI             = 3.141592653589793;
#define                               PI                 isi_PI

#ifdef E1
#undef E1
#endif
const RT_FLOAT                        isi_E1             = 2.718281828;
#define                               E1                 isi_E1

#ifdef OK
#undef OK
#endif
const RT_INTEGER                      isi_OK             = 0;
#define                               OK                 isi_OK

/* Error Codes from Real-Time AutoCode */
#ifdef STOP_BLOCK
#undef STOP_BLOCK
#endif
const RT_INTEGER                      isi_STOP_BLOCK     = 1;
#define                               STOP_BLOCK         isi_STOP_BLOCK

#ifdef MATH_ERROR
#undef MATH_ERROR
#endif
const RT_INTEGER                      isi_MATH_ERROR     = 2;
#define                               MATH_ERROR         isi_MATH_ERROR

#ifdef UCB_ERROR
#undef UCB_ERROR
#endif
const RT_INTEGER                      isi_UCB_ERROR      = -2;
#define                               UCB_ERROR          isi_UCB_ERROR

#ifdef INIT_ERROR
#undef INIT_ERROR
#endif
const RT_INTEGER                      isi_INIT_ERROR     = 8;
#define                               INIT_ERROR         isi_INIT_ERROR

#ifdef EVENT_OVERFLOW
#undef EVENT_OVERFLOW
#endif
const RT_INTEGER                      isi_EVENT_OVERFLOW  = -1;
#define                               EVENT_OVERFLOW   isi_EVENT_OVERFLOW

#ifdef TASK_OVERRUN
#undef TASK_OVERRUN
#endif
const RT_INTEGER                      isi_TASK_OVERRUN  = 16;
#define                               TASK_OVERRUN   isi_TASK_OVERRUN

#ifdef TASK_ERROR
#undef TASK_ERROR
#endif
const RT_INTEGER                      isi_TASK_ERROR  = 32;
#define                               TASK_ERROR   isi_TASK_ERROR

#ifdef INVALID_EVENT
#undef INVALID_EVENT
#endif
const RT_INTEGER                      isi_INVALID_EVENT  = 48;
#define                               INVALID_EVENT   isi_INVALID_EVENT

#ifdef IO_ERROR
#undef IO_ERROR
#endif
const RT_INTEGER                      isi_IO_ERROR  = 64;
#define                               IO_ERROR   isi_IO_ERROR

#ifdef UNKNOWN_ERROR
#undef UNKNOWN_ERROR
#endif
const RT_INTEGER                      isi_UNKNOWN_ERROR  = 100;
#define                               UNKNOWN_ERROR      isi_UNKNOWN_ERROR

/* Exception codes for internal error handling */
#define EXIT_CONDITION                2
#define USERCODE_ERROR                3

char modelname[30]="@rtf_fname_s@";
extern int endex;
extern int OVERFLOW_LIMIT;

@IFF not procs_only_b@@/Scheduled system only


@define_varblocks()@@
@declare_varblocks()@@
@define_percentvars()@@
@declare_percentvars()@@


@IFF nvars_i@@
enum variable_id {@LOOPP i=0, i lt nvars_i, i=i plus 1@@IFF i@, @ENDIFF@varid_@vars_ls[i]@@ENDLOOPP@};
@ENDIFF@@


/* Use floating point co-processor in taskSpawn iff VX_FP is defined */
#if (VX_FP)
int tsOptions = VX_FP_TASK;
#else
int tsOptions = 0;
#endif

/* TSK_PARAM is for future use */
#if defined(Enter_Local_Varblk_Section)
#undef Enter_Local_Varblk_Section
#endif
#define Enter_Local_Varblk_Section(VARID)

/* TSK_PARAM must contain ID of the calling task */
#if defined(Leave_Local_Varblk_Section)
#undef Leave_Local_Varblk_Section
#endif
#define Leave_Local_Varblk_Section(VARID)

/* prio gap between last subsys task and background task */
#define PRIORITY_GAP                  2

/* appl tasks must have priorities lower than the WindShell launching task=100 */
#define PRIORITY_RAISE                120

#define MANAGER_PRIORITY              @scheduler_priority_i@ + PRIORITY_RAISE
#define MANAGER_TIMER_FREQ            @schedulerfreq_r@
#define NTASKS                        @ntasks_i@
#define NUMIN                         @numin_i@
#define NUMOUT                        @numout_i@
#define MANAGER_ID                    0
#define PREEMPTABLE                   0
#define NON_PREEMPTABLE               1

#define TICKS2SLICE                   10


/*
enum event_type { INTERNAL_EV, ERROR_EV, TIME_EV, @LOOPP j=0,j lt ninterrupt_procs_i,j=j plus 1@@i=interrupt_procs_li[j]@@SCOPE PROCEDURE i@@
@procedurename_s@_EV, @ENDLOOPP@@
NUM_EVENTS };
*/

enum TASK_STATE_TYPE { IDLE, RUNNING, BLOCKED, UNALLOCATED };

MSG_Q_ID qid_ovflow1;
unsigned long tid_ovflow1;
SEM_ID semid_mgr1;
static unsigned long tid_mgr1;

@LOOPP i=1, i le ntasks_i, i=i plus 1@@
SEM_ID semid_sub@i@;
static unsigned long tid_sub@i@;
@ENDLOOPP@@

@LOOPP j=0,j lt ninterrupt_procs_i,j=j plus 1@@i=interrupt_procs_li[j]@@
unsigned long tid_int@i@;
@ENDLOOPP@@

unsigned long tid_Background;

@IFF nbackgnd_procs_i ge 1@@
@LOOPP j=0,j lt nbackgnd_procs_i,j=j plus 1@@i=backgnd_procs_li[j]@@
unsigned long tid_bgd@i@;
@ENDLOOPP@@
@ENDIFF@@

unsigned long delete_tid_ovflow1;
static unsigned long delete_tid_mgr1;

@LOOPP i=1, i le ntasks_i, i=i plus 1@@
unsigned long delete_tid_sub@i@;
@ENDLOOPP@@

@LOOPP j=0,j lt ninterrupt_procs_i,j=j plus 1@@i=interrupt_procs_li[j]@@
unsigned long delete_tid_int@i@;
@ENDLOOPP@@

unsigned long delete_tid_Background;


@IFF nbackgnd_procs_i ge 1@@
@LOOPP j=0,j lt nbackgnd_procs_i,j=j plus 1@@i=backgnd_procs_li[j]@@
unsigned long delete_tid_bgd@i@;
@ENDLOOPP@@
@ENDIFF@@

/* Priorities of all application tasks. */

@IFF rtos_option_b@@
unsigned long TASK_PRIORITY[NTASKS+@nstartup_procs_i@+@nbackgnd_procs_i@+@ninterrupt_procs_i@+1+1] =
{
  MANAGER_PRIORITY       /* Application Manager */
@LOOPP i=0, i lt ntasks_i, i=i plus 1@@
@j= i plus 1@@
, @sspriority_li[i]@            /* Subsystem @j@ */
@ENDLOOPP@@
@LOOPP i=0, i lt nstartup_procs_i, i=i plus 1@@
@j = startup_procs_li[i]@@
@j = j minus nstandard_procs_i@@
, @proc_priority_li[j]@         /* Startup proc @i@ */
@ENDLOOPP@@
@LOOPP i=0, i lt nbackgnd_procs_i, i=i plus 1@@
@j = backgnd_procs_li[i]@@
@j = j minus nstandard_procs_i@@
, @proc_priority_li[j]@         /* Background Proc @i@ */
@ENDLOOPP@@
@LOOPP i=0, i lt ninterrupt_procs_i, i=i plus 1@@
@j = interrupt_procs_li[i]@@
@j = j minus nstandard_procs_i@@
, @proc_priority_li[j]@         /* Interrupt Proc @i@ */
@ENDLOOPP@@
, MANAGER_PRIORITY+1+@ninterrupt_procs_i@+NTASKS+1+PRIORITY_GAP /* SA_Background */
};
@ELSE@@
unsigned long TASK_PRIORITY[NTASKS+@nstartup_procs_i@+@nbackgnd_procs_i@+@ninterrupt_procs_i@+1+1] =
{
  MANAGER_PRIORITY               /* Application Manager */
@LOOPP i=1, i le ntasks_i, i=i plus 1@@
, MANAGER_PRIORITY+1+@ninterrupt_procs_i@+@i@ /* Subsystem @i@ */
@ENDLOOPP@@
@LOOPP i=1, i le nstartup_procs_i, i=i plus 1@@
, MANAGER_PRIORITY+1+@ninterrupt_procs_i@+NTASKS-1 /* Startup proc @i@ */
@ENDLOOPP@@
@LOOPP i=1, i le nbackgnd_procs_i, i=i plus 1@@
, MANAGER_PRIORITY+1+@ninterrupt_procs_i@+NTASKS+1 /* Background Proc @i@ */
@ENDLOOPP@@
@LOOPP i=1, i le ninterrupt_procs_i, i=i plus 1@@
, MANAGER_PRIORITY+1+@i@ /* Interrupt Proc @i@ */
@ENDLOOPP@@
, MANAGER_PRIORITY+1+@ninterrupt_procs_i@+NTASKS+1+PRIORITY_GAP /* SA_Background */
};
@ENDIFF@@

static unsigned long MAX_TASKPRIO = MANAGER_PRIORITY-1;
static unsigned long MIN_TASKPRIO = MANAGER_PRIORITY+1+@ninterrupt_procs_i@+NTASKS+1+PRIORITY_GAP;

@IFF nvars_i@@
unsigned long VARS_PRIO[@nvars_i@] = { @LOOPP i=0, i lt nvars_i, i=i plus 1@@
@IFF i@, @ENDIFF@@
@j = get_vars_prio(i)@@
@k = j plus 1@@
@k@@
@ENDLOOPP@
};
@ENDIFF@@


static RT_INTEGER                     isi_ERROR_FLAG  [NTASKS+1];
#define                               ERROR_FLAG      isi_ERROR_FLAG
@IFF timerequired_b@@
static RT_DURATION                    isi_SUBSYS_TIME [NTASKS+1];
#define                               SUBSYS_TIME     isi_SUBSYS_TIME
static long int                       TIME_COUNT;
@ENDIFF@@
static RT_BOOLEAN                     SUBSYS_PREINIT [NTASKS+1];

@declare_structs()@
@define_structs()@

RT_FLOAT                              ExtIn       [NUMIN+1];
RT_FLOAT                              ExtOut      [NUMOUT+1];
static RT_BOOLEAN                     SUBSYS_INIT [NTASKS+1];
static enum TASK_STATE_TYPE           TASK_STATE  [NTASKS+1];
RT_INTEGER                            APPLICATION_manager_state[2];
RT_INTEGER                            APPLICATION_response_state[NUM_EVENTS];

RT_INTEGER                            TASK_OVERRUN_CNT[NTASKS+1];
RT_INTEGER                            schedulerOverflowCnt;

const  RT_BOOLEAN                     Requires_MultiProcSupp = FALSE;


SEM_ID errorSemId;
RT_INTEGER APPLICATION_current_state = INIT_STATE; /* set to EXIT_STATE when shutdown is done */
enum event_type applEventType;


@IFF continuous_b@@
RT_BOOLEAN RESETC;

/******* Continuous Subsystem states and info type declarations. *******/
@SCOPE SUBSYSTEM continuous_id_i@@
@declare_cont_states()@@
@declare_cont_info()@@

/******* Continuous Subsystem states and info definitions.       *******/
@define_cont_states()@@
@define_cont_info()@@
RT_INTEGER *ss@continuous_id_i@_iinfo = &subsys_@continuous_id_i@_info.iinfo[0];
RT_FLOAT   *ss@continuous_id_i@_rinfo = &subsys_@continuous_id_i@_info.rinfo[0];

@ENDIFF@@
@ENDIFF@@/Scheduled system only

@ENDSEGMENT@


@*
 This segment generates the init function to initialize the application data.
*@

@SEGMENT gen_systemdata_init()@
void Init_Application_Data (void);
@IFF not procs_only_b and continuous_b@@
@SCOPE SUBSYSTEM continuous_id_i@@
@define_cont_init()@
@ENDIFF@@
void Init_Application_Data ()
{
   RT_INTEGER cnt;
@system_data_init()@@
@IFF not procs_only_b@@
   for(  cnt=0; cnt<NUMOUT; cnt++ ){
       ExtOut[cnt] = -EPSILON;
   }
@ENDIFF@@

}
@ENDSEGMENT@


@*
  This segment generates the SCHEDULER() and its support functions.
*@

@SEGMENT gen_scheduler() INT k, INIT_OFCHECK@
@FILECLOSE@@
@FILEOPEN("stdout","append")@@
             Generating the scheduler ...
@FILECLOSE@@
@*
   Set this INIT_OFCHECK flag to 0 if you want the scheduler not to check for
   subsystems overflow during their first cycle so that the subsystems can take
   their own time to complete their initialization cycles.
   ******** WARNING: Resetting this flag will improve the performance ********
            but will introduce non-determinacy in the system.
*@
@INIT_OFCHECK = 1@@
/*---------------*
 *-- MANAGER --*
 *---------------*/

/*** Manager Data ***/

enum SUBSYSTEM_TYPE  { CONTINUOUS, PERIODIC, ENABLED_PERIODIC, TRIGGERED_ANT,
                          TRIGGERED_ATR, TRIGGERED_SAF, isi_NONE };
@IFF timerequired_b@@
const RT_DURATION              MANAGER_TIMER_INTERVAL              =
                                      (RT_DURATION) (1.0/MANAGER_TIMER_FREQ);
@ENDIFF@@
static const enum SUBSYSTEM_TYPE      TASK_TYPE            [NTASKS+1] =
  {isi_NONE@LOOPP i=0, i lt ntasks_i, i=i plus 1@, @
@IFF i and i div 4 times 4 eq i@
   @ENDIFF@@sstypes_ls[i]@@ENDLOOPP@};
static const enum TASK_STATE_TYPE     INITIAL_TASK_STATE   [NTASKS+1] =
  {UNALLOCATED@LOOPP i=0, i lt ntasks_i, i=i plus 1@, @
@IFF i and i div 6 times 6 eq i@
   @ENDIFF@@initialtaskstate_ls[i]@@ENDLOOPP@};
static const RT_INTEGER               START_COUNT          [NTASKS+1] =
  {0@LOOPP i=0, i lt ntasks_i, i=i plus 1@, @
@IFF i and i div 11 times 11 eq i@
   @ENDIFF@@startcount_li[i]@@ENDLOOPP@};
static const RT_INTEGER               SCHEDULING_COUNT     [NTASKS+1] =
  {0@LOOPP i=0, i lt ntasks_i, i=i plus 1@, @
@IFF i and i div 11 times 11 eq i@
   @ENDIFF@@schedulingcount_li[i]@@ENDLOOPP@};
static const RT_INTEGER               OUTPUT_COUNT         [NTASKS+1] =
  {0@LOOPP i=0, i lt ntasks_i, i=i plus 1@, @
@IFF i and i div 11 times 11 eq i@
   @ENDIFF@@outputcount_li[i]@@ENDLOOPP@};

@IFF timerequired_b@@
static RT_DURATION                    ELAPSED_TIME;
@ENDIFF@@
static RT_INTEGER                     READY_COUNT;
static RT_INTEGER                     READY_QUEUE [NTASKS+1];
static RT_INTEGER                     DISPATCH_QUEUE [NTASKS+1];
static volatile RT_INTEGER            DISPATCH_COUNT;

struct TCB_TYPE
   {
     enum  SUBSYSTEM_TYPE             TASK_TYPE;
     RT_BOOLEAN                       ENABLED;
     RT_INTEGER                       START;
     RT_INTEGER                       START_COUNT;
     RT_INTEGER                       SCHEDULING_COUNT;
     RT_INTEGER                       OUTPUT;
     RT_INTEGER                       OUTPUT_COUNT;
     RT_BOOLEAN                       DS_UPDATE;
   };
static struct TCB_TYPE                TCB [NTASKS+1];
RT_BOOLEAN                            manager_current_events[NUM_EVENTS];
RT_INTEGER                            APPLICATION_manager_status;


/* Work area side indices for subsystems. */
static RT_INTEGER                     SSWORKSIDE  [NTASKS+1];
static RT_INTEGER                     SSREADSIDE;

#define  Queue_Task(NTSK)                                         \
   READY_COUNT++;                                                 \
   READY_QUEUE[READY_COUNT] = NTSK;                               \
   TASK_STATE[NTSK]         = RUNNING

void Update_Outputs(RT_INTEGER NTSK);
void Update_DS_With_Externals(void);
void Init_Manager(void);
void event_ovflow_handler(unsigned long delete_tid_ovflow1);
void APPLICATION_Manager(unsigned long delete_tid_mgr1);

void Update_Outputs(NTSK)
  RT_INTEGER NTSK;
{
   SSREADSIDE = SSWORKSIDE[NTSK];
   SSWORKSIDE[NTSK] = 1 - SSREADSIDE;
   switch(NTSK) {
     @toggle_rwbuffers()@
     default:
        break;
   }
   return;
}

void  Update_DS_With_Externals() {
   @update_dsext()@
}

void Init_Manager()
{
   RT_INTEGER NTSK;
   RT_INTEGER EV;
   for(  NTSK=1; NTSK<=NTASKS; NTSK++  ) {
      TCB[NTSK].TASK_TYPE        = TASK_TYPE[NTSK];
      TCB[NTSK].ENABLED          = FALSE;
      TCB[NTSK].START            = START_COUNT[NTSK];
      TCB[NTSK].START_COUNT      = START_COUNT[NTSK];
      TCB[NTSK].SCHEDULING_COUNT = SCHEDULING_COUNT[NTSK];
      TCB[NTSK].OUTPUT           = OUTPUT_COUNT[NTSK];
      TCB[NTSK].OUTPUT_COUNT     = OUTPUT_COUNT[NTSK];
      ERROR_FLAG[NTSK]           = 0;
      SUBSYS_INIT[NTSK]          = TRUE;
@IFF timerequired_b@@
      SUBSYS_TIME[NTSK]          = 0.0;
@ENDIFF@@
      TASK_STATE[NTSK]           = INITIAL_TASK_STATE[NTSK];
      if(TASK_TYPE[NTSK]==TRIGGERED_ATR || TASK_TYPE[NTSK]==TRIGGERED_SAF){
         SSWORKSIDE[NTSK] = 0;
         TCB[NTSK].DS_UPDATE = TRUE;
      }
      else {
         SSWORKSIDE[NTSK] = 1;
         TCB[NTSK].DS_UPDATE = FALSE;
      }

      TASK_OVERRUN_CNT[NTSK] = 0;
   }
   schedulerOverflowCnt = 0;
   DISPATCH_COUNT   = 0;
   DISPATCH_QUEUE[0]= 0;
   READY_COUNT      = 0;
   READY_QUEUE[0]   = 0;
   READY_QUEUE[1]   = 0;
   SSWORKSIDE[0]    = 0;
   ERROR_FLAG[0]    = 0;
   SUBSYS_INIT[0]   = FALSE;
@IFF timerequired_b@@
   ELAPSED_TIME     = 0.0;
   TIME_COUNT       = 0;
@ENDIFF@@
   APPLICATION_manager_status = OK;
   APPLICATION_manager_state[1] = PREEMPTABLE;
   for(  EV=0; EV<NUM_EVENTS; EV++  ) {
      manager_current_events[EV] = FALSE;
      APPLICATION_response_state[EV] = PREEMPTABLE;
   }
}


RT_INTEGER APPLICATION_Process_Event(eventtype)
enum event_type eventtype;
{
  char msgbuf[4];

  msgbuf[0] = applEventType = eventtype;

  switch(eventtype) {
    case TIME_EV:
     if(APPLICATION_manager_state[eventtype] == NON_PREEMPTABLE)
     {
        if (schedulerOverflowCnt >= OVERFLOW_LIMIT)
        {
           msgbuf[1] = EVENT_OVERFLOW;
           msgbuf[2] = taskIdSelf();
           msgbuf[3] = 1;
           msgQSend(qid_ovflow1, &msgbuf[0], sizeof (msgbuf), NO_WAIT, MSG_PRI_URGENT);
           return EVENT_OVERFLOW;
        }
        else schedulerOverflowCnt++;
     }
     else schedulerOverflowCnt = 0;

     semGive(semid_mgr1);
     break;

@LOOPP j=0,j lt ninterrupt_procs_i,j=j plus 1@@i=interrupt_procs_li[j]@@
@SCOPE PROCEDURE i@@
    case @procedurename_s@_EV:
     if (taskResume(tid_int@i@)) {
        msgbuf[1] = EVENT_OVERFLOW;
        msgbuf[2] = taskIdSelf();
        msgbuf[3] = 1;
        msgQSend(qid_ovflow1, &msgbuf[0], sizeof (msgbuf), NO_WAIT, MSG_PRI_URGENT);
        return EVENT_OVERFLOW;
     }
     break;
@ENDLOOPP@@
    default:
        semGive(semid_mgr1);
        return INVALID_EVENT;
     break;
  }

  return OK;
}

void event_ovflow_handler(unsigned long delete_tid_ovflow1) {
  char msgbuf[4];
  enum event_type eventtype;
  RT_INTEGER errorCode, taskId;

  if(delete_tid_ovflow1) {
         /* Clean Up */
   taskDelete(tid_ovflow1);
  }
  else { /* Initialize */
  }

  for(;;) {
   if ((msgQReceive(qid_ovflow1, &msgbuf[0], 16, WAIT_FOREVER)) == ERROR)
                         SA_Error(taskIdSelf(), errno, VXWORKS_ERROR, 1);
   eventtype = msgbuf[0];
   errorCode = msgbuf[1];
   taskId    = msgbuf[2];
   SA_Error(taskId, eventtype, errorCode, 1);

   semGive(errorSemId);
   APPLICATION_current_state = EXIT_STATE;
   printf("Application is being shut down.\n");
   APPLICATION_Shut_Down();

  }
}

void APPLICATION_Manager(unsigned long delete_tid_mgr1)
{
              RT_INTEGER io_status;
   register   RT_INTEGER    NTSK;
   register   RT_INTEGER    I,J;
              char msgbuf[4], errMsgBuf[4];
              unsigned long sendbuf[4];
              enum event_type eventtype;

  if(delete_tid_mgr1) {
         /* Clean Up */
    taskDelete(tid_mgr1);
  }
  else { /* Initialize */
    Init_Manager();
  }

  for(;;) {

MANAGER_LOOP:

   if ((semTake(semid_mgr1, WAIT_FOREVER)) == ERROR)
                           SA_Error(taskIdSelf(), errno, VXWORKS_ERROR, 1);

INNER_LOOP:
   do {
      if (applEventType == TIME_EV)
      {
            manager_current_events[TIME_EV] = TRUE;
            /*** Update elapsed time ***/
@IFF timerequired_b@@
            ELAPSED_TIME = ((RT_DURATION)TIME_COUNT)*MANAGER_TIMER_INTERVAL;
            TIME_COUNT++;
@ENDIFF@@
      }
      else
      {
            SA_Error(taskIdSelf(), applEventType, INVALID_EVENT, 1);
            goto MANAGER_LOOP;
      }
   } while((semTake(semid_mgr1, NO_WAIT)) != ERROR);


   APPLICATION_manager_state[1] = NON_PREEMPTABLE;

   /*** System Input for the current event(s) ***/

    io_status = SA_External_Input();
    if( io_status != OK ){
      if (outOfInputData)
      {
		  endex = 1;
      }
      else
      {
         SA_Error(taskIdSelf(),ERROR_EV,io_status,1);
         goto MANAGER_LOOP;
      }
    }
   /*** System Input ***/

   @sys_extin_copy()@

   @setenables()@
   @settriggers()@

   /*** Clear Ready Queue ***/

   READY_COUNT    = 0;
   READY_QUEUE[1] = 0;

   /*** Task Scheduling ***/

   if (manager_current_events[TIME_EV]) {
    for( NTSK=NTASKS; NTSK>=1; NTSK--  ){

      switch( TASK_STATE[NTSK] ){
         case IDLE :

            switch( TCB[NTSK].TASK_TYPE ){
               case CONTINUOUS :
               case PERIODIC :
                  if( TCB[NTSK].START == 0 ){
                     Queue_Task(NTSK);
                     Update_Outputs(NTSK);
                     TCB[NTSK].START  = TCB[NTSK].SCHEDULING_COUNT;
                  }else{
                     TCB[NTSK].START  = TCB[NTSK].START - 1;
                  }
                  break;

               case ENABLED_PERIODIC :
                  if( !TCB[NTSK].ENABLED ){
                     TASK_STATE[NTSK] = BLOCKED;
                  }else if( TCB[NTSK].START == 0 ){
                     Queue_Task(NTSK);
                     Update_Outputs(NTSK);
                     TCB[NTSK].START  = TCB[NTSK].SCHEDULING_COUNT;
                  }else{
                     TCB[NTSK].START  = TCB[NTSK].START - 1;
                  }
                  break;

               case TRIGGERED_ANT :
                  if( TCB[NTSK].START == 0 ){
                     Queue_Task(NTSK);
                     Update_Outputs(NTSK);
                     TCB[NTSK].START  = 1;
                  }
                  break;

               case TRIGGERED_ATR :
                  if( TCB[NTSK].OUTPUT == 0 ){
                     Update_Outputs(NTSK);
                     TASK_STATE[NTSK] = BLOCKED;
                     if( TCB[NTSK].START == 0 ){
                        Queue_Task(NTSK);
                        TCB[NTSK].OUTPUT = TCB[NTSK].OUTPUT_COUNT;
                        TCB[NTSK].START  = 1;
                     }
                  }else{
                     TCB[NTSK].OUTPUT = TCB[NTSK].OUTPUT - 1;
                  }
                  break;

               case TRIGGERED_SAF :
                  if( TCB[NTSK].OUTPUT == 0 ){
                     Update_Outputs(NTSK);
                     TASK_STATE[NTSK] = BLOCKED;
                     if( TCB[NTSK].START == 0 ){
                        Queue_Task(NTSK);
                        TCB[NTSK].OUTPUT = 0;
                        TCB[NTSK].START  = 1;
                     }
                  }
                  break;

               default :
                  break;
            }
            break;

         case RUNNING :

            switch( TCB[NTSK].TASK_TYPE ){
               case CONTINUOUS :
               case PERIODIC :
               case ENABLED_PERIODIC :
                  if( TCB[NTSK].START > 0 ){
                     TCB[NTSK].START  = TCB[NTSK].START - 1;
@IFF INIT_OFCHECK@@
                  } else {
                     if (TASK_OVERRUN_CNT[NTSK] >= OVERFLOW_LIMIT)
                     {
                        errMsgBuf[0] = ERROR_EV;
                        errMsgBuf[1] = TASK_OVERRUN;
                        errMsgBuf[2] = NTSK;
                        errMsgBuf[3] = 1;
                        msgQSend(qid_ovflow1, &errMsgBuf[0], sizeof(errMsgBuf),
                                 NO_WAIT, MSG_PRI_URGENT);
                        goto MANAGER_LOOP;
                     }
                     else TASK_OVERRUN_CNT[NTSK]++;
                  }
@ELSE@@
                  }else if(ERROR_FLAG[NTSK] != OK || !SUBSYS_INIT[NTSK]) {
                     SA_Error(NTSK, ERROR_EV, TASK_OVERRUN, 1);
                     goto MANAGER_LOOP;
                  }
@ENDIFF@@
                  break;

               case TRIGGERED_ANT :
                  if( TCB[NTSK].START == 0 ){
@IFF INIT_OFCHECK@@
                     if (TASK_OVERRUN_CNT[NTSK] >= OVERFLOW_LIMIT)
                     {
                        errMsgBuf[0] = ERROR_EV;
                        errMsgBuf[1] = TASK_OVERRUN;
                        errMsgBuf[2] = NTSK;
                        errMsgBuf[3] = 1;
                        msgQSend(qid_ovflow1, &errMsgBuf[0], sizeof (errMsgBuf),
                                 NO_WAIT, MSG_PRI_URGENT);
                        goto MANAGER_LOOP;
                     }
                     else TASK_OVERRUN_CNT[NTSK]++;
@ELSE@@
                   if(ERROR_FLAG[NTSK] != OK || !SUBSYS_INIT[NTSK]) {
                     SA_Error(NTSK, ERROR_EV, TASK_OVERRUN, 1);
                     goto MANAGER_LOOP;
                   }
@ENDIFF@@
                  }
                  break;

               case TRIGGERED_ATR :
                  if( TCB[NTSK].OUTPUT > 0 ){
                     TCB[NTSK].OUTPUT = TCB[NTSK].OUTPUT - 1;
@IFF INIT_OFCHECK@@
                  } else {
                     if (TASK_OVERRUN_CNT[NTSK] >= OVERFLOW_LIMIT)
                     {
                        errMsgBuf[0] = ERROR_EV;
                        errMsgBuf[1] = TASK_OVERRUN;
                        errMsgBuf[2] = NTSK;
                        errMsgBuf[3] = 1;
                        msgQSend(qid_ovflow1, &errMsgBuf[0], sizeof (errMsgBuf),
                                 NO_WAIT, MSG_PRI_URGENT);
                        goto MANAGER_LOOP;
                     }
                     else TASK_OVERRUN_CNT[NTSK]++;
                  }
@ELSE@@
                  }else if(ERROR_FLAG[NTSK] != OK || !SUBSYS_INIT[NTSK]) {
                     SA_Error(NTSK, ERROR_EV, TASK_OVERRUN, 1);
                     goto MANAGER_LOOP;
                  }
@ENDIFF@@
                  break;

               case TRIGGERED_SAF :
                  break;

               default :
                  break;
            }
            break;

         case BLOCKED :

            switch( TCB[NTSK].TASK_TYPE ){
               case ENABLED_PERIODIC :
                  if( TCB[NTSK].ENABLED ){
                     Queue_Task(NTSK);
                     Update_Outputs(NTSK);
                     TCB[NTSK].START  = TCB[NTSK].SCHEDULING_COUNT;
                  }
                  break;

               case TRIGGERED_ATR :
                  if( TCB[NTSK].START == 0 ){
                     Queue_Task(NTSK);
                     TCB[NTSK].OUTPUT = TCB[NTSK].OUTPUT_COUNT;
                     TCB[NTSK].START  = 1;
                  }
                  break;

               case TRIGGERED_SAF :
                  if( TCB[NTSK].START == 0 ){
                     Queue_Task(NTSK);
                     TCB[NTSK].OUTPUT = 0;
                     TCB[NTSK].START  = 1;
                  }
                  break;

               default :
                  break;
            }
          default :
               break;
      }
    }
   }

   /*** System Output ***/

   Update_DS_With_Externals();
@/   @system_output()@
   @sys_extout_copy()@

   io_status = SA_External_Output ();


   if(io_status != OK) {
    SA_Error(taskIdSelf(), ERROR_EV,io_status,1);
    goto MANAGER_LOOP;
   }

  for(I=TIME_EV; I<NUM_EVENTS; I++) {
     if(manager_current_events[I]) {
       APPLICATION_response_state[I] = PREEMPTABLE;
       manager_current_events[I] = FALSE;
     }
  }

   /*** Task Input Sample and Hold ***/

   for( I=READY_COUNT; I>=1; I-- ){
      NTSK   = READY_QUEUE[I];
      switch (NTSK){
         @sample_hold()@
         default:
           break;
      }
@IFF timerequired_b@@
      SUBSYS_TIME[NTSK] = ELAPSED_TIME;
@ENDIFF@@
      DISPATCH_COUNT++;
      DISPATCH_QUEUE[DISPATCH_COUNT] = NTSK;
   }

   /*** Signal End of Critical Section ***/


  APPLICATION_manager_state[1] = PREEMPTABLE;

  /*** Task Dispatching ***/

  if(DISPATCH_COUNT) {

  if((semTake(semid_mgr1, NO_WAIT)) != ERROR)
     goto INNER_LOOP;

   do {
     NTSK = DISPATCH_QUEUE[DISPATCH_COUNT];
     switch(NTSK){
@LOOPP i=1, i le ntasks_i, i=i plus 1@@
@SCOPE SUBSYSTEM i@@
         case @i@ :
                if ((semGive(semid_sub@i@)) == ERROR)
                    SA_Error(taskIdSelf(), errno, VXWORKS_ERROR, 1);
                break;
@ENDLOOPP@@
         default :  break;
     }
   } while(--DISPATCH_COUNT);
  }

 } /* infinite loop */


}


/*--------------------*
 *-- Initialization --*
 *--------------------*/

void APPLICATION_Get_Params (RT_FLOAT *TimerInterruptFrequency,
       RT_FLOAT **ExternalInputs,  RT_INTEGER *NumIn,
       RT_FLOAT **ExternalOutputs, RT_INTEGER *NumOut,
       unsigned long *max_taskprio,  unsigned long *min_taskprio)
/*
  Get the Real-Time parameters for this generated code application
  including the application timer frequency.
*/
{
    *TimerInterruptFrequency = MANAGER_TIMER_FREQ;
    *NumIn              = NUMIN;
    *NumOut             = NUMOUT;
    *ExternalInputs     = &ExtIn[0];
    *ExternalOutputs    = &ExtOut[0];
    *max_taskprio       = MAX_TASKPRIO;
    *min_taskprio       = MIN_TASKPRIO;
}

/* Initialize the data structures for the Real-Time application. */

RT_INTEGER APPLICATION_Start (void)
{
   unsigned long buf[4];

    /*** Create and initialize events overflow handler task and its queue
***/

    if ((qid_ovflow1 = msgQCreate(NUM_EVENTS, 16, MSG_Q_PRIORITY))
                           == NULL)  SA_Error(taskIdSelf(), errno, VXWORKS_ERROR, 1);
    if ((tid_ovflow1 = taskSpawn("OF01", MANAGER_PRIORITY - 1, tsOptions, 4096,
               (void *)event_ovflow_handler, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0))
                           == ERROR) SA_Error(taskIdSelf(), errno, VXWORKS_ERROR, 1);

    /*** Create and initialize Manager task and its queue ***/

    if ((semid_mgr1 = semCCreate(SEM_Q_PRIORITY, 0))
                           == NULL)  SA_Error(taskIdSelf(), errno, VXWORKS_ERROR, 1);
    if ((tid_mgr1 = taskSpawn("AM01", MANAGER_PRIORITY, tsOptions, 4096,
               (void *)APPLICATION_Manager, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0))
                           == ERROR) SA_Error(taskIdSelf(), errno, VXWORKS_ERROR, 1);

    /* Initialize Application data structures */

    Init_Application_Data();

    /******* Call startup procedures for this PE *******/
@LOOPP i=0, i lt nstartup_procs_i, i=i plus 1@@
@SCOPE PROCEDURE startup_procs_li[i]@
    @procedurename_s@();
@ENDLOOPP@@

    /******* Create and initialize subsystem tasks and their queues ******
*/

@LOOPP i=1, i le ntasks_i, i=i plus 1@@
@SCOPE SUBSYSTEM i@@
    if ((semid_sub@i@ = semBCreate(SEM_Q_PRIORITY, SEM_EMPTY))
                           == NULL)  SA_Error(taskIdSelf(), errno, VXWORKS_ERROR, 1);
    if ((tid_sub@i@ = taskSpawn("SS@IFF i le 9 @0@ENDIFF@@i@", TASK_PRIORITY[@i@], tsOptions, 16384,
                 (void *)@procedurename_s@_task, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0))
                           == ERROR) SA_Error(taskIdSelf(), errno, VXWORKS_ERROR, 1);

    if ((semGive(semid_sub@i@)) == ERROR)
       SA_Error(taskIdSelf(), errno, VXWORKS_ERROR, 1);
@ENDLOOPP@@

    /******* Create and start Interrupt tasks for this PE. *******/
@LOOPP i=0, i lt ninterrupt_procs_i, i=i plus 1@@j=interrupt_procs_li[i]@@
@SCOPE PROCEDURE j@@
@k = j minus nstandard_procs_i plus 1@@
    if ((tid_int@j@ = taskSpawn("IT@IFF i le 9 @0@ENDIFF@@i@", TASK_PRIORITY[NTASKS+@k@], tsOptions, 4096,
                 (void *)@procedurename_s@_task, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0))
                           == ERROR) SA_Error(taskIdSelf(), errno, VXWORKS_ERROR, 1);
@ENDLOOPP@@

@IFF nbackgnd_procs_i@@
    /******* Create and start model background tasks for this PE. *****/
@LOOPP i=0, i lt nbackgnd_procs_i, i=i plus 1@@j = backgnd_procs_li[i]@@
@SCOPE PROCEDURE j@@
@k = j minus nstandard_procs_i plus 1@@
    if ((tid_bgd@j@ = taskSpawn("BG@IFF j le 9 @0@ENDIFF@@j@", TASK_PRIORITY[NTASKS+@k@], tsOptions, 4096,
                 (void *)@procedurename_s@_task, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0))
                           == ERROR) SA_Error(taskIdSelf(), errno, VXWORKS_ERROR, 1);
@ENDLOOPP@@
@ENDIFF@@


@IFF nbackgnd_procs_i@@
@LOOPP i=0, i lt nbackgnd_procs_i, i=i plus 1@@j = backgnd_procs_li[i]@@
    if ((taskResume(tid_bgd@j@)) == ERROR)
           SA_Error(taskIdSelf(), errno, VXWORKS_ERROR, 1);
@ENDLOOPP@@
@ENDIFF@@

    SA_Initialize();

    return OK;
}

/*********************************
* Exported Update Procedure *
*********************************/
RT_INTEGER APPLICATION_Initialize (void)
{
   Init_Application_Data();
   return OK;
}

/*********************************
* Exported Update Procedure *
*********************************/
void       APPLICATION_Update (void)
{
   APPLICATION_Process_Event(TIME_EV);
}


/*********************************
* Exported Termination Procedure *
*********************************/

RT_INTEGER APPLICATION_Shut_Down(void)
{
    RT_INTEGER errorCode = 0;

@IFF nbackgnd_procs_i@@
    /******* Delete model background tasks *******/
@LOOPP i=0, i lt nbackgnd_procs_i, i=i plus 1@@j = backgnd_procs_li[i]@@
    delete_tid_bgd@j@ = 1;
    if ((taskDelete(tid_bgd@j@)) == ERROR)
             SA_Error(taskIdSelf(), errno, VXWORKS_ERROR, 1);
@ENDLOOPP@@
@ENDIFF@@

    /******* Delete subsystem tasks *******/

@LOOPP i=ntasks_i, i gt 0, i=i minus 1@@
    delete_tid_sub@i@ = 1;
    if ((taskDelete(tid_sub@i@)) == ERROR)
             SA_Error(taskIdSelf(), errno, VXWORKS_ERROR, 1);
    if ((semDelete(semid_sub@i@)) == ERROR)
             SA_Error(taskIdSelf(), errno, VXWORKS_ERROR, 1);
@ENDLOOPP@@

    delete_tid_mgr1 = 1;
    if ((taskDelete(tid_mgr1)) == ERROR)
             SA_Error(taskIdSelf(), errno, VXWORKS_ERROR, 1);
    if ((semDelete(semid_mgr1)) == ERROR)
             SA_Error(taskIdSelf(), errno, VXWORKS_ERROR, 1);


    /******* Delete Interrupt tasks *******/
@LOOPP i=0, i lt ninterrupt_procs_i, i=i plus 1@@j = interrupt_procs_li[i]@@
    delete_tid_int@j@ = 1;
    if ((taskDelete(tid_int@j@)) == ERROR)
             SA_Error(taskIdSelf(), errno, VXWORKS_ERROR, 1);
@ENDLOOPP@@


    /* write output data to file */
    SA_Output_To_File();

    /* Delete the overflow task last coz' in case of critical errors */
    /* APPLICATION_Shut_Down happens from the context of overflow task */
    if ((msgQDelete(qid_ovflow1)) == ERROR)
             SA_Error(taskIdSelf(), errno, VXWORKS_ERROR, 1);
    delete_tid_ovflow1 = 1;
    if ((taskDelete(tid_ovflow1)) == ERROR)
             SA_Error(taskIdSelf(), errno, VXWORKS_ERROR, 1);

    return (OK);
}


void auxClkISR()
{
      APPLICATION_Process_Event(TIME_EV);
}

/*********************************
*           MAIN                 *
*********************************/

int MXmain (void)
{

   RT_FLOAT timerInterruptFreq, *externalInputs, *externalOutputs;
   RT_INTEGER numIn, numOut;
   unsigned long highestTaskPrio, lowestTaskPrio;


   APPLICATION_Get_Params(&timerInterruptFreq, &externalInputs,
                          &numIn, &externalOutputs, &numOut,
                          &highestTaskPrio, &lowestTaskPrio);

   Implementation_Initialize(externalInputs, numIn, externalOutputs,
                             numOut, timerInterruptFreq, auxClkISR);

   if ((errorSemId = semBCreate(SEM_Q_FIFO, SEM_EMPTY)) == NULL)
      SA_Error(taskIdSelf(), VXWORKS_ERROR, errno, 1);

   APPLICATION_Start();
   APPLICATION_Process_Event(TIME_EV);

   MXP_REGISTER;
   return(0);


}


@ENDSEGMENT@

@////////////////////////////////////////////////////////////////////

@*
 This segment generates the tasks(subsystems) functions declarations.
*@

@SEGMENT gen_tasks_decl()@
void Background_task(unsigned long delete_tid_Background);
@IFF ntasks_i le 0@@RETURN 0@@ENDIFF@@
@FILECLOSE@@
@FILEOPEN("stdout","append")@@
             Generating subsystems declarations ...
@FILECLOSE@@
@LOOPP i=1, i le ntasks_i, i=i plus 1@@
@SCOPE SUBSYSTEM i@@
void @procedurename_s@_task(unsigned long delete_tid_sub@i@);
@ENDLOOPP@
/******** Tasks declarations ********/
@LOOPP i=1, i le ntasks_i, i=i plus 1@@
@SCOPE SUBSYSTEM i@@
@IFF continuous_b@@
@IFF i eq continuous_id_i@
/******* Continuous Subsystem  *******/
@ENDIFF@@
@ENDIFF@
/******* Subsystem @i@  *******/
extern void @procedurename_s@();
@IFF continuous_b@@
@IFF i eq continuous_id_i@
@declare_integrator()@

@declare_cont_init()@
@ENDIFF@@
@ENDIFF@@
@ENDLOOPP@
@ENDSEGMENT@

@////////////////////////////////////////////////////////////////////

@*
 This segment generates the tasks(subsystems) functions definitions.
*@

@SEGMENT gen_tasks_defn()@
@IFF ntasks_i le 0@@RETURN 0@@ENDIFF@@
@FILECLOSE@@
@FILEOPEN("stdout","append")@@
             Generating subsystems definitions ...
@FILECLOSE@@
/******** Tasks code ********/
@LOOPP i=1, i le ntasks_i, i=i plus 1@@
@SCOPE SUBSYSTEM i@
@IFF continuous_b@@
@IFF i eq continuous_id_i@
@define_integrator()@
/******* Continuous Subsystem  *******/
@ENDIFF@@
@ENDIFF@
/******* Subsystem @i@  *******/

/* ERROR is a #define in VxWorks                */
/* UCBs have ERROR as part of the status record */
#ifdef ERROR
#undef ERROR
#endif
@define_subsystem()@
      if(iinfo[1]) {
         SUBSYS_INIT[@i@] = FALSE;
         iinfo[1] = 0;
      }
      return;
EXEC_ERROR: ERROR_FLAG[@i@] = iinfo[0];
      iinfo[0]=0;
}
#undef ERROR
#define ERROR (-1)

void @procedurename_s@_task(unsigned long delete_tid_sub@i@)
{
  char errMsgBuf[4];

@IFF utype_b@
  struct @utype_tag_s@ *U;
@ENDIFF@@
@IFF ytype_b@
  struct @ytype_tag_s@ *Y;
@ENDIFF@@
@IFF continuous_b@@
@IFF stype_b@
  struct @stype_tag_s@ *S;
@ENDIFF@@
@IFF itype_b@
  struct @itype_tag_s@ *I;
@ENDIFF@@
@ENDIFF@@

    if(delete_tid_sub@i@) {
           /* Clean Up */
      taskDelete(tid_sub@i@);
    }
    else { /* Initialize */
       SUBSYS_PREINIT[@i@] = TRUE;
       if ((semTake(semid_sub@i@, WAIT_FOREVER)) == ERROR)
                SA_Error(taskIdSelf(), errno, VXWORKS_ERROR, 1);

@IFF continuous_b@@
@IFF i eq continuous_id_i@@
       @IFF utype_b@U = &@utype_obj_s@;@ENDIFF@
@IFF initop_b@@
@IFF doublebuf_b@@
       @IFF ytype_b@Y = ss@i@_outw;@ENDIFF@
@ELSE@@
       @IFF ytype_b@Y = &@ytype_obj_s@;@ENDIFF@
@ENDIFF@@
@ENDIFF@@
       @IFF stype_b@S = @stype_obj_s@;@ENDIFF@
       @IFF itype_b@I = &@itype_obj_s@;@ENDIFF@
       RESETC = FALSE;
       @procedurename_s@_init(@IFF initop_b and ytype_b@Y@ENDIFF@@
@IFF stype_b@@IFF (initop_b and ytype_b)@,@ENDIFF@S@ENDIFF@@
@IFF itype_b@@IFF (initop_b and ytype_b) or stype_b@,@ENDIFF@I@ENDIFF@);
@ELSE@@
       @IFF utype_b@U = &@utype_obj_s@;@ENDIFF@
@IFF doublebuf_b@@
       @IFF ytype_b@Y = ss@i@_outw;@ENDIFF@
@ELSE@@
       @IFF ytype_b@Y = &@ytype_obj_s@;@ENDIFF@
@ENDIFF@@
       @procedurename_s@(@IFF utype_b@U@ENDIFF@@IFF ytype_b@@IFF utype_b@,@ENDIFF@Y@ENDIFF@);
@ENDIFF@@
@ELSE@@
       @IFF utype_b@U = &@utype_obj_s@;@ENDIFF@
@IFF doublebuf_b@@
       @IFF ytype_b@Y = ss@i@_outw;@ENDIFF@
@ELSE@@
       @IFF ytype_b@Y = &@ytype_obj_s@;@ENDIFF@
@ENDIFF@@
       @procedurename_s@(@IFF utype_b@U@ENDIFF@@IFF ytype_b@@IFF utype_b@,@ENDIFF@Y@ENDIFF@);
@ENDIFF@@/ continuous_b?

       if( ERROR_FLAG[@i@] == OK ){
           TASK_OVERRUN_CNT[@i@] = 0;
           TASK_STATE[@i@] =  IDLE;
       }
       else {
           errMsgBuf[0] = ERROR_EV;      /* type of event */
           errMsgBuf[1] = ERROR_FLAG[@i@]; /* type of error */
           errMsgBuf[2] = @i@;           /* task id */
           errMsgBuf[3] = 1;             /* processor number */
           ERROR_FLAG[@i@] = OK;
           if ((msgQSend(qid_ovflow1, errMsgBuf, sizeof (errMsgBuf), NO_WAIT, MSG_PRI_URGENT)) == ERROR)
                    SA_Error(taskIdSelf(), errno, VXWORKS_ERROR, 1);
       }
    }
    for(;;) {
       if ((semTake(semid_sub@i@, WAIT_FOREVER)) == ERROR)
                SA_Error(taskIdSelf(), errno, VXWORKS_ERROR, 1);

@IFF continuous_b@@
@IFF i eq continuous_id_i@@
       @IFF utype_b@U = &@utype_obj_s@;@ENDIFF@
@IFF doublebuf_b@@
       @IFF ytype_b@Y = ss@i@_outw;@ENDIFF@
@ELSE@@
       @IFF ytype_b@Y = &@ytype_obj_s@;@ENDIFF@
@ENDIFF@@
       @IFF stype_b@S = @stype_obj_s@;@ENDIFF@
       @IFF itype_b@I = &@itype_obj_s@;@ENDIFF@
@IFF stype_b@@/ should generate update?
       if( TIME_COUNT == 0) {
           ss@i@_rinfo[0] = (RT_FLOAT)SUBSYS_TIME[@i@];
           @procedurename_s@(@IFF utype_b@U@ENDIFF@@
@IFF ytype_b@@IFF utype_b@,@ENDIFF@Y@ENDIFF@@
@IFF stype_b@@IFF utype_b or ytype_b@,@ENDIFF@S@ENDIFF@@
@IFF itype_b@@IFF utype_b or ytype_b or stype_b@,@ENDIFF@I@ENDIFF@);
       }
@IFF integrator_i eq 0 and numxs_i neq 0@@
       usrintegrator(@numxs_i@,
                     (RT_FLOAT *)(&@stype_obj_s@[0]),
                     (RT_FLOAT *)(&@stype_obj_s@[1]),
                     (RT_FLOAT )SUBSYS_TIME[@i@],
                     @subsys_sampletime_r@);
@ENDIFF@@
@IFF integrator_i eq 1 and numxs_i neq 0@@
       rungekutta1(@numxs_i@,
                     (RT_FLOAT *)(&@stype_obj_s@[0]),
                     (RT_FLOAT *)(&@stype_obj_s@[1]),
                     (RT_FLOAT )SUBSYS_TIME[@i@],
                     @subsys_sampletime_r@);
@ENDIFF@@
@IFF integrator_i eq 2 and numxs_i neq 0@@
       rungekutta2(@numxs_i@,
                     (RT_FLOAT *)(&@stype_obj_s@[0]),
                     (RT_FLOAT *)(&@stype_obj_s@[1]),
                     (RT_FLOAT )SUBSYS_TIME[@i@],
                     @subsys_sampletime_r@);
@ENDIFF@@
@IFF integrator_i eq 3 and numxs_i neq 0@@
       rungekutta4(@numxs_i@,
                     (RT_FLOAT *)(&@stype_obj_s@[0]),
                     (RT_FLOAT *)(&@stype_obj_s@[1]),
                     (RT_FLOAT )SUBSYS_TIME[@i@],
                     @subsys_sampletime_r@);
@ENDIFF@@
@IFF integrator_i eq 4 and numxs_i neq 0@@
       kuttamerson(@numxs_i@,
                     (RT_FLOAT *)(&@stype_obj_s@[0]),
                     (RT_FLOAT *)(&@stype_obj_s@[1]),
                     (RT_FLOAT )SUBSYS_TIME[@i@],
                     @subsys_sampletime_r@);
@ENDIFF@@
       ss@i@_iinfo[1] = 0;
       ss@i@_iinfo[2] = 0;
       ss@i@_iinfo[3] = 1;
       ss@i@_iinfo[4] = 7;
       ss@i@_rinfo[0] = (RT_FLOAT )SUBSYS_TIME[@i@];
       @procedurename_s@(@IFF utype_b@U@ENDIFF@@
@IFF ytype_b@@IFF utype_b@,@ENDIFF@Y@ENDIFF@@
@IFF stype_b@@IFF utype_b or ytype_b@,@ENDIFF@S@ENDIFF@@
@IFF itype_b@@IFF utype_b or ytype_b or stype_b@,@ENDIFF@I@ENDIFF@);
@IFF reset_b@@/ should generate reset? Autocode doesnot support this
       if( RESETC ) {
           RESETC = 0;
       }
@ENDIFF@@/ should generate reset?
@ELSE@@/ should generate update?
@IFF not stype_b@@/ if not generate update also?
       ss@i@_rinfo[0] = (RT_FLOAT )SUBSYS_TIME[@i@];
       @procedurename_s@(@IFF utype_b@U@ENDIFF@@
@IFF ytype_b@@IFF utype_b@,@ENDIFF@Y@ENDIFF@@
@IFF stype_b@@IFF utype_b or ytype_b@,@ENDIFF@S@ENDIFF@@
@IFF itype_b@@IFF utype_b or ytype_b or stype_b@,@ENDIFF@I@ENDIFF@);
@ENDIFF@@
@ENDIFF@@
@ELSE@@
       @IFF utype_b@U = &@utype_obj_s@;@ENDIFF@
@IFF doublebuf_b@@
       @IFF ytype_b@Y = ss@i@_outw;@ENDIFF@
@ELSE@@
       @IFF ytype_b@Y = &@ytype_obj_s@;@ENDIFF@
@ENDIFF@@
       @procedurename_s@(@IFF utype_b@U@ENDIFF@@IFF ytype_b@@IFF utype_b@,@ENDIFF@Y@ENDIFF@);
@ENDIFF@@
@ELSE@@
       @IFF utype_b@U = &@utype_obj_s@;@ENDIFF@
@IFF doublebuf_b@@
       @IFF ytype_b@Y = ss@i@_outw;@ENDIFF@
@ELSE@@
       @IFF ytype_b@Y = &@ytype_obj_s@;@ENDIFF@
@ENDIFF@@
       @procedurename_s@(@IFF utype_b@U@ENDIFF@@IFF ytype_b@@IFF utype_b@,@ENDIFF@Y@ENDIFF@);
@ENDIFF@@/ continuous_b?

       if( ERROR_FLAG[@i@] == OK ){
           TASK_OVERRUN_CNT[@i@] = 0;
           TASK_STATE[@i@] =  IDLE;
       }
       else {
           errMsgBuf[0] = ERROR_EV;      /* type of event */
           errMsgBuf[1] = ERROR_FLAG[@i@]; /* type of error */
           errMsgBuf[2] = @i@;           /* task id */
           errMsgBuf[3] = 1;             /* processor number */
           ERROR_FLAG[@i@] = OK;
           if ((msgQSend(qid_ovflow1, errMsgBuf, sizeof (errMsgBuf), NO_WAIT, MSG_PRI_URGENT)) == ERROR)
                    SA_Error(taskIdSelf(), errno, VXWORKS_ERROR, 1);
       }
    }

}

@ENDLOOPP@
@ENDSEGMENT@

@////////////////////////////////////////////////////////////////////

@*
 This segment generates the procedures declarations in an orderly fashion.
*@

@SEGMENT gen_procs_decl()@
@IFF nprocedures_i le 0@@RETURN 0@@ENDIFF@@
@FILECLOSE@@
@FILEOPEN("stdout","append")@@
             Generating procedures declarations ...
@FILECLOSE@@
/******** Procedures' declarations ********/
@LOOPP i=0, i lt nprocedures_i, i=i plus 1@@
@SCOPE PROCEDURE ordered_procs_li[i]@@
@declare_procs()@
extern void @procedurename_s@();
@ENDLOOPP@@
@ENDSEGMENT@


@////////////////////////////////////////////////////////////////////

@*
 This segment generates the procedures definitions in an orderly fashion.
*@

@SEGMENT gen_procs_defn()@
@IFF nprocedures_i le 0@@RETURN 0@@ENDIFF@@
@FILECLOSE@@
@FILEOPEN("stdout","append")@@
             Generating procedures definitions ...
@FILECLOSE@@
/* ERROR is a #define in VxWorks                */
/* UCBs have ERROR as part of the status record */
#ifdef ERROR
#undef ERROR
#endif
/******** Procedures' definitions ********/
@LOOPP i=0, i lt nprocedures_i, i=i plus 1@@
@SCOPE PROCEDURE ordered_procs_li[i]@@
@define_procs()@
      iinfo[1] = 0;
EXEC_ERROR: return;
}
@ENDLOOPP@
#undef ERROR
#define ERROR (-1)
@IFF ninterrupt_procs_i gt 0@@
   /*** Prototypes for Task wrappers ***/
@LOOPP i=0, i lt ninterrupt_procs_i, i = i plus 1@@j=interrupt_procs_li[i]@@
@SCOPE PROCEDURE interrupt_procs_li[i]@@
void @procedurename_s@_task(unsigned long delete_tid_int@j@);
@ENDLOOPP@@
@ENDIFF@@
@IFF nbackgnd_procs_i gt 0@@
@LOOPP i=0, i lt nbackgnd_procs_i, i = i plus 1@@j=backgnd_procs_li[i]@@
@SCOPE PROCEDURE backgnd_procs_li[i]@@
void @procedurename_s@_task(unsigned long delete_tid_bgd@j@);
@ENDLOOPP@@
@ENDIFF@@

@IFF ninterrupt_procs_i gt 0@@
   /*** Task wrapper for Interrupt Procedure super-blocks ***/
@LOOPP i=0, i lt ninterrupt_procs_i, i = i plus 1@@j=interrupt_procs_li[i]@@
@SCOPE PROCEDURE interrupt_procs_li[i]@@
void @procedurename_s@_task(unsigned long delete_tid_int@j@)
{
    if(delete_tid_int@j@) {
           /* Clean Up */
      taskDelete(0);
    }
    else { /* Initialize */
    }
    for(;;) {
      taskSuspend(0);
      @procedurename_s@();
    }
}
@ENDLOOPP@@
@ENDIFF@@

@IFF nbackgnd_procs_i gt 0@@
   /*** Task wrapper for Background Procedure super-blocks ***/
@LOOPP i=0, i lt nbackgnd_procs_i, i = i plus 1@@j=backgnd_procs_li[i]@@
@SCOPE PROCEDURE backgnd_procs_li[i]@@
void @procedurename_s@_task(unsigned long delete_tid_bgd@j@)
{

    if(delete_tid_bgd@j@) {
           /* Clean Up */
      taskDelete(0);
    }
    else { /* Initialize */
      taskSuspend(0);
    }
    for(;;) {
      taskDelay(1000);
      @procedurename_s@();
    }
}
@ENDLOOPP@
@ENDIFF@@
@ENDSEGMENT@


@////////////////////////////////////////////////////////////////////

@*
 This segment generates declarations of UCB-style wrappers to the procedures.
*@

@SEGMENT gen_proc_ucbwrapper_decl()@@

@* Generate declaration of UCB-style wrapper to first procedure found
   in top-level superblock.
*@
@gen_proc_ucb_hook_decl(0)@@

@/ Un-comment the following statements if you want to loop through *all*
@/ procedures and generate declarations for UCB-style wrappers.

@/@LOOPP i=0, i lt nprocedures_i, i=i plus 1@@
@/@gen_proc_ucb_hook_decl(ordered_procs_li[i])@@
@/@ENDLOOPP@@

@ENDSEGMENT@@


@////////////////////////////////////////////////////////////////////

@*
 This segment generates definitions of (UCB-style) wrappers to the procedures.
*@

@SEGMENT gen_proc_ucbwrapper_defn()@@
@FILEOPEN("stdout","append")@@
             Generating UCB-style wrapper(s) around procedure(s) ...
@FILECLOSE@@
@gen_proc_ucb_hook_defn(0)@@

@/ Un-comment the following statements if you want to loop through *all*
@/ procedures and generate definitions for (UCB-style) wrappers.

@/@LOOPP i=0, i lt nprocedures_i, i=i plus 1@@
@/@gen_proc_ucb_hook_defn(ordered_procs_li[i])@@
@/@ENDLOOPP@@

@ENDSEGMENT@@

@////////////////////////////////////////////////////////////////////

@*
 This segment generates declarations of (subsystem-style) wrappers to the
 procedures.
*@
@SEGMENT gen_proc_subsyswrapper_decl()@@
#define NTASKS                        @ntasks_i@
static RT_INTEGER                     ERROR_FLAG  [NTASKS+1];
static RT_BOOLEAN                     SUBSYS_PREINIT [NTASKS+1];
static RT_BOOLEAN                     SUBSYS_INIT [NTASKS+1];
@IFF timerequired_b@@
static RT_DURATION                    SUBSYS_TIME [NTASKS+1];
@ENDIFF@@

@declare_structs()@@

@ENDSEGMENT@@

@////////////////////////////////////////////////////////////////////

@*
 This segment generates definitions of (subsystem-style) wrappers to the
 procedures.
*@
@SEGMENT gen_proc_subsyswrapper_defn()
INT subsys_i, proc_i, nchildprocs_i, ip@@

@LOOPP i=0, i lt ntasks_i, i=i plus 1@@
@proc_i = i plus nprocedures_i@@
@nchildprocs_i=nchildprocs_li[proc_i]@@
@subsys_i = i plus 1@@
@IFF nchildprocs_i ge 1@@
@SCOPE PROCEDURE childprocs_li[childprocs_off_li[proc_i]]@@

@IFF nchildprocs_i eq 1@@
@FILEOPEN("stdout","append")@@
             Generating subsystem-style wrapper (subsys_@subsys_i@)
             around procedure @procedurename_s@ ...
@FILECLOSE@@
@ELSE@@
@FILEOPEN("stdout","append")@@
             Generating subsystem-style wrapper (subsys_@subsys_i@) around procedure(s):
@FILECLOSE@@
@LOOPP ip=0, ip lt nchildprocs_i, ip=ip plus 1@@
@SCOPE PROCEDURE childprocs_li[childprocs_off_li[proc_i] plus ip]@@
@IFF ip eq nchildprocs_i minus 1@@
@FILEOPEN("stdout","append")@@
                 @procedurename_s@() ...
@FILECLOSE@@
@ELSE@@
@FILEOPEN("stdout","append")@@
                 @procedurename_s@(),
@FILECLOSE@@
@ENDIFF@@
@ENDLOOPP@@
@ENDIFF@@

/*****************************************************************************/
/* (Subsystem-style) Wrapper (subsys_@subsys_i@) around procedure(s):    @78@*/
@LOOPP ip=0, ip lt nchildprocs_i, ip=ip plus 1@@
@SCOPE PROCEDURE childprocs_li[childprocs_off_li[proc_i] plus ip]@@
/*                    @procedurename_s@()                                @78@*/
@ENDLOOPP@@
/* to support invoking it from a user application.                           */
/*****************************************************************************/
@gen_subsys_decl(subsys_i)@@

@gen_subsys_defn(subsys_i)@@
@ELSE@@
@FILEOPEN("stdout","append")@@
             Warning: no procedures in subsystem @subsys_i@ ...
@FILECLOSE@@
@ENDIFF@@
@ENDLOOPP@@
@ENDSEGMENT@@

@////////////////////////////////////////////////////////////////////

@SEGMENT gen_proc_ucb_hook_decl(proc_i)@@
@SCOPE PROCEDURE proc_i@@

/* Required for linking generated procedures with SystemBuild simulator. */

#if (FC_)
#ifdef SBUSER
#define @procedurename_s@_ucbhook @procedurename_s@_ucbhook_
#endif
#endif

extern void @procedurename_s@_ucbhook();

@ENDSEGMENT@@

@////////////////////////////////////////////////////////////////////

@SEGMENT gen_proc_ucb_hook_defn(proc_i)@@
@SCOPE PROCEDURE proc_i@@
@proc_ucb_hook()@@

@ENDSEGMENT@@

@////////////////////////////////////////////////////////////////////

@*
 This segment generates a subsystem declaration.
*@
@SEGMENT gen_subsys_decl(subsys_i)@@

extern void subsys_@subsys_i@();

@ENDSEGMENT@@

@////////////////////////////////////////////////////////////////////

@*
 This segment generates a subsystem definition.
*@
@SEGMENT gen_subsys_defn(subsys_i)@@

/******* Subsystem @subsys_i@  *******/

@SCOPE SUBSYSTEM subsys_i@@

@define_subsystem()@@
      iinfo[1] = 0;
      return;
EXEC_ERROR: ERROR_FLAG[@subsys_i@] = iinfo[0];
      iinfo[0]=0;
}
@ENDSEGMENT@@

@////////////////////////////////////////////////////////////////////

@*
 generates var block structure
*@
@SEGMENT gen_var_blk_structure() INT k,m,shared_count,offset@@
@*
   ** handle the situation where we need to generate a
   ** a structure that contains ONLY shared variable blocks
   ** This is a globally known structure and the rest
   ** of the generated code ASSUMES that it is defined.
   ** This only applied to non-RVE multiprocessor code.
*@
@shared_count = 0@@
@IFF multiprocessor_b@@
/**** shared variable block structure (for shared memory) ****/
static struct shared_varblk_type {
@offset = 0@@
@shared_count = 0@@
@LOOPP k=0, k lt nvars_i, k=k plus 1@@
@IFF vars_prsr_scope_li[k] eq 2 or vars_prsr_scope_li[k] eq 3 @@
@
@/Test to see if we support the given type of the shared variable.
@/Currently, all types are support except: fixed point byte and
@/fixed point short, (signed and unsigned)  This is
@/due to the 32 bit hardware requirement.
@
@ASSERT(vars_type_li[k] lt fixed_unsigned_byte_type_i or
        vars_type_li[k] gt fixed_signed_short_type_i)@@
@
@
    @vars_typ_pfix_ls[k]@ @vars_ls[k]@@
@
@shared_count = shared_count plus 1@@
@LOOPP m=0, m lt vars_typ_sfix_dim_li[k], m=m plus 1@@
[@vars_typ_sfix_li[offset]@]@
@offset = offset plus 1@@
@ENDLOOPP@@
@
@IFF vars_typ_sfix_dim_li[k] eq 0 @@
@offset = offset plus 1@@
@ENDIFF@@
;
@
@ELSE@@
@IFF vars_typ_sfix_dim_li[k] eq 0 @@
@offset = offset plus 1@@
@ELSE@@
@offset = offset plus vars_typ_sfix_dim_li[k]@@
@ENDIFF@@
@ENDIFF@@
@ENDLOOPP@@
@
@/ if no shared varblk, must still declare atleast one
@/ element in the structure
@
@IFF shared_count eq 0 @@
    RT_INTEGER    ignore;
@ENDIFF@@
};
@ENDIFF@ @/ multiprocessor_b
@ENDSEGMENT@@

@////////////////////////////////////////////////////////////////////

@SEGMENT get_vars_subsysid(var_id) INT cnt, freq, max, subsys_id, start, end@@
@start = vars_subsys_idx_li[var_id]@@
@end   = vars_subsys_idx_li[var_id plus 1]@@
@subsys_id = 0@@
@max = 0@@
@* This is where we are doing the load balancing. By default only
   subsystems are taken into account, because we dont know the freq. of
   interrupt and background subsystems. A subsystem with higher freq. will
   get that var block generated along with. For different algorithm
   this is the place to change.
*@
@LOOPP cnt=start, cnt lt end, cnt=cnt plus 1@@
@IFF vars_subsys_li[cnt] lt 0 or vars_subsys_li[cnt] gt ntasks_i@@
@CONTINUE@@
@ENDIFF@@
@freq = ssfreq_lr[vars_subsys_li[cnt] minus 1] plus 1@
@IFF (vars_subsys_freq_li[cnt] times freq) gt max@@
@subsys_id = vars_subsys_li[cnt]@@
@max = vars_subsys_freq_li[cnt] times freq@@
@ENDIFF@@
@ENDLOOPP@@
@RETURN subsys_id@@
@ENDSEGMENT@@

@SEGMENT get_vars_prio(var_id) INT proc_id, cnt, temp_prio, prio, start, end@@
@start = vars_subsys_idx_li[var_id]@@
@end   = vars_subsys_idx_li[var_id plus 1]@@
@prio = 0@
@LOOPP cnt=start, cnt lt end, cnt=cnt plus 1@@
@IFF vars_subsys_li[cnt] lt 0@@/ Startup procedure
@CONTINUE@@
@ENDIFF@@
@IFF vars_subsys_li[cnt] le ntasks_i@@
@temp_prio = sspriority_li[vars_subsys_li[cnt] minus 1]@@/ Real Subsystem
@ELSE@@
@proc_id = vars_subsys_li[cnt] minus ntasks_i minus 1 @@/ Procedure
@temp_prio = proc_priority_li[proc_id]@@/ Real Procedure
@ENDIFF@@
@IFF temp_prio gt prio@@
@prio = temp_prio@@
@ENDIFF@@
@ENDLOOPP@@
@RETURN prio@@
@ENDSEGMENT@@

@SEGMENT gen_applDOTh()@@
@FILEOPEN(outputfile_dir_s cat "appl.h","new")@@
/* AutoCode_LicensedWork -----------------------------------------------------

This product is protected by the National Instruments License Agreement
which is located at: http://www.ni.com/legal/license.

(c) 1987-2003 National Instruments Corporation. All rights reserved.

----------------------------------------------------------------------------*/

#include "sa_defn.h"

/* Exit conditions */
#define NORMAL_EXIT     0
#define SYSINIT_EXIT    1
#define APPINIT_EXIT    2
#define UNKNOWN_EXIT    3

/* Application states */
#define INIT_STATE      0
#define IDLE_STATE      1
#define RUN_STATE       2
#define EXIT_STATE      3

extern RT_INTEGER APPLICATION_current_state;
extern SEM_ID errorSemId;


/* Returns useful application parameters */
extern void APPLICATION_Get_Params(
        RT_FLOAT    *timer_frequency,     /* application timer freq.   */
        RT_FLOAT    **u_ptr,              /* pointer to external in vector */
        RT_INTEGER  *nu,                  /* number of external inputs     */
        RT_FLOAT    **y_ptr,              /* pointer to external out vector*/
        RT_INTEGER  *ny,                  /* number of external outputs    */
        unsigned long *max_taskprio,      /* Max. priority used */
        unsigned long *min_taxprio        /* Min. priority used */
       );

/* Initializes the application and returns OK (=0) if no init errors */
extern RT_INTEGER APPLICATION_Start(void);

/* Stops the application and returns OK (=0) if no stop errors */
extern RT_INTEGER APPLICATION_Stop(void);

/* Restarts the application and returns OK (=0) if no init errors */
extern RT_INTEGER APPLICATION_Restart(void);

/* Shuts down the application and returns OK (=0) if no shut down errors */
extern RT_INTEGER APPLICATION_Shut_Down(void);

enum event_type { INTERNAL_EV, ERROR_EV, TIME_EV, @LOOPP j=0,j lt ninterrupt_procs_i,j=j plus 1@@i=interrupt_procs_li[j]@@SCOPE PROCEDURE i@@
@procedurename_s@_EV, @ENDLOOPP@@
NUM_EVENTS };

/* Invokes the application to process a given event (called from ISR). Returns
   OK if no error. */
extern RT_INTEGER APPLICATION_Process_Event(enum event_type eventtype);

/*
** APPLICATION_response_state[EVENT] is set to "PREEMPTABLE" (=0) by the
** application at the end of processing the event. It is checked by the
** interrupt handling routine (or executive software) as part of detecting
** event overflow, and if no overflow, APPLICATION_response_state[EVENT]
** is set to "NOT_PREEMPTABLE" (=1) before sending the event to the
** application. Initialized to PREEMPTABLE by the generated code.
*/
extern RT_INTEGER APPLICATION_response_state[NUM_EVENTS];

/* APPLICATION_manager_status denotes the error status. */
extern RT_INTEGER APPLICATION_manager_status;

/* APPLICATION_Update provides a time event to the application manager. */
extern void APPLICATION_Update (void);

/* APPLICATION_Initialize calls the application initialization routine. */
extern RT_INTEGER APPLICATION_Initialize (void);

@ENDSEGMENT@@

@SEGMENT gen_sharedmem_files()@@
#include "appl.h"

  /*
  ** Procedure needed to declare absolute locations for
  ** the 68K shared memory in multiprocessor systems.
  */

void define_shared_mem()
{
    asm("       ORG.L $00200000");
    asm("       XDEF _APPL_SHRMEM_START");
    asm("_APPL_SHRMEM_START DS.L 1");
    asm("       XDEF _APPL_SHRMEM_END");
    asm("_APPL_SHRMEM_END DS.L 1");
    asm("       SECTION code,4,C" );
}
@ENDSEGMENT@@

@SEGMENT gen_objlist() INT ii@@
@IFF ((nucb_filenames_i plus nsystem_best_charts_i) gt 0)@
@FILEOPEN("stdout","append")@@
             Generating list of UCB and BetterState files ...
@FILECLOSE@@
@FILECLOSE@@
@FILEOPEN(modelname_s cat ".cmd","new")@@
@*
#!-------------------------------------------------------------------
#! Generated list of UCB and BetterState files referenced in:
#!
#! Filename      : @currentfname_s@
#! Mtx-Model     : @rtf_fname_s@
#! Creation Date : @currentdate_s@
#!
#! Automatically generated: do not delete this file
#!-------------------------------------------------------------------
*@@
@/
@/  UCB filenames
@/
@IFF nucb_filenames_i@
#!UCB Filenames -----------------------------------------------------
@LOOPP ii = 0, ii lt nucb_filenames_i, ii = ii plus 1@@
compile @ucb_filenames_ls[ii]@
@ENDLOOPP@@
@ENDIFF@
@/
@/  BetterState filenames
@/
@IFF nsystem_best_charts_i@
#!BetterState Filenames ---------------------------------------------
@LOOPP ii = 0, ii lt nsystem_best_charts_i, ii = ii plus 1@@
compile @system_best_filebasenames_ls[ii]@
@ENDLOOPP@@
@ENDIFF@
@/
@FILECLOSE@@
@ENDIFF@
@ENDSEGMENT@@

@****************************************************************************

  The following routine taken directly from c_core.tpl:
          gen_banner()
****************************************************************************@

@*
  This segment generates the banner in the generated code.

  Parameter: endcmt : 0=don't end the comment block, 1 = end the comment
*@

@SEGMENT gen_banner(endcmt)@@
/****************************************************************************
|                   AutoCode/C (TM) Code Generator V7.x
|                NATIONAL INSTRUMENTS CORP.,  AUSTIN, TEXAS
*****************************************************************************
RTF filename           : @rtf_fname_s@
Filename               : @currentfname_s@
Generated on           : @currentdate_s@
Dac filename           : @dac_fname_s@
Dac file created on    : @dac_createdate_s@
@/Stamp file with command-line options used to generate it
@emit_options()@
****************************************************************************@
@IFF endcmt@*/@ELSE@*@ENDIFF@
@ENDSEGMENT@@


