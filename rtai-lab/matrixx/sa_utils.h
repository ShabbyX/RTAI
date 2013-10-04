/*
Mattia Mattaboni <mattaboni@aero.polimi.it> reworked this file for use within
RTAI-Lab.
*/

/* AutoCode_LicensedWork --------------------------------------------------

This product is protected by the National Instruments License Agreement
which is located at: http://www.ni.com/legal/license.

(c) 1987-2004 National Instruments Corporation. All rights reserved.

-------------------------------------------------------------------------*/
/*
**  File       : sa_utils.h
**  Project    : AutoCode/C
**
**  Abstract:
**
**      Interface to AutoCode/C stand-alone support utilities.
*/

#ifndef  _SA_UTILS
#define  _SA_UTILS

#include "sa_sys.h"
#include "sa_types.h"


/*================================================================
		O T H E R   C O N V E R S I O N S              
 ================================================================*/
#define I_Ipr(a)     (a)
#define F_Fpr(a)     (a)
#define I_Fpr(a)     ((RT_INTEGER)ROUND(a))
#define F_Ipr(a)     ((RT_FLOAT)a)

/*================================================================
		G E T  A N D  S E T  M A C R O S              
 ================================================================*/

#define GET_EXTIN(type, b)   \
		  JUXTAPOSE(type,_Fpr)(b)  
		  
#define SET_EXTOUT(type, b)   \
		  JUXTAPOSE(F_,type)(b)




  /* Function: SA_External_Input ++++++++++++++++++++++++++++++++++++++++++++++
  **
  ** Purpose:
  **   Called by the application scheduler on every minor tick to request
  **   that external inputs be updated in the external input block.
  **   Returns (0) if no error.
  */
  EXTERN_FUNC( RT_INTEGER  SA_External_Input, (void));



  /* Function: SA_External_Output +++++++++++++++++++++++++++++++++++++++++++++
  **
  ** Purpose:
  **   Called by the application scheduler on every minor tick to request
  **   that new external outputs in the external output block be sent to
  **   the I/O devices.  Returns (0) if no error. 
  */
  EXTERN_FUNC( RT_INTEGER  SA_External_Output, (void));



  /* Function: SA_Implementation_Terminate ++++++++++++++++++++++++++++++++++++
  **
  ** Purpose:
  **   This routine will be called only once when the system wants to
  **   terminate.  An errcode of zero specifies a normal exit, non-zero
  **   specifies an abort exit. 
  */
  EXTERN_FUNC( void  SA_Implementation_Terminate, 
	(
	 RT_INTEGER   errcode         /* specifies the type of exit */
	));

  /* Function: SA_Output_To_File +++++++++++++++++++++++++++++++++++++++++++++
  **
  ** Purpose:
  **   Called by the application scheduler when it has finished using all the
  **   input data from the input file. This will write the computed outputs
  **   to the given file if FILE_IO is defined. 
  */
  EXTERN_FUNC( void  SA_Output_To_File, (void));



  /* Function: SA_Background ++++++++++++++++++++++++++++++++++++++++++++++++++
  **
  ** Purpose:
  **   Starts the application and waits for it to complete.
  **   Returns (0) if no error.
  */
  EXTERN_FUNC( RT_INTEGER  SA_Background, (void));



  /* Function: SA_Error +++++++++++++++++++++++++++++++++++++++++++++++++++++++
  **
  ** Purpose:
  **   Called by the application scheduler to invoke the message reporting
  **   services and signal that the application needs to stop.
  */
  EXTERN_FUNC( void  SA_Error, 
	(
	 RT_INTEGER   n_task,         /* Task_id of the reporting task  */
	 RT_INTEGER   error_num,      /* Error code                     */
	RT_INTEGER   perror_code,    /* RTOS error code                */
	 RT_INTEGER   pnum            /* Processor_id of reporting task */
	));

extern RT_BOOLEAN outOfInputData;
extern RT_INTEGER numInputData;

  /* Function: SA_Initialize ++++++++++++++++++++++++++++++++++++++++++++++++++
  **
  ** Purpose:
  **   Called by the application start routine to initialize low level services
  **   like the aux clock related data.
  */
   extern void SA_Initialize(void);

   extern void Implementation_Initialize(RT_FLOAT BUS_IN[],  
						 RT_INTEGER NI, 
						 RT_FLOAT BUS_OUT[], 
						 RT_INTEGER NO,
						 RT_FLOAT SCHEDULER_FREQ,
						 void *funcPtr);

  /********************
  * Macro definitions *
  ********************/

  /**************************************************************************
  ** SA_Disable
  **
  ** Purpose:
  **   Called by the application scheduler to disable (mask) scheduler
  **   interrupts that protect critical code sections.  (This is only
  **   required in certain procedural forms of the generated code.)
  **************************************************************************/

  /**************************************************************************
  ** SA_Enable
  **
  ** Purpose:
  **   Called by the application scheduler to enable (unmask) scheduler
  **   interrupts that protect critical code sections.  (This is only
  **   required in certain procedural forms of the generated code.)
  **************************************************************************/

#   define SA_Disable              TSKLock();
#   define SA_Enable               TSKUnlock();




#endif /*_SA_UTILS*/

