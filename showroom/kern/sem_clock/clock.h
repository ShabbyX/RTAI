/*
COPYRIGHT (C) 1999  Paolo Mantegazza <mantegazza@aero.polimi.it>

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


/* DEFINITIONS */

#define ONE_SHOT

#define SEM_TYPE CNT_SEM

#define BOOLEAN int
#define TRUE 1
#define FALSE 0

/* DEFINITION CMDCLK */

extern void CommandClock_Put(char command);
extern void CommandClock_Get(char *command);

/* DEFINITION CMDCRN */

extern void CommandChrono_Put(char command);
extern void CommandChrono_Get(char *command);

/* DEFINITION DISPCLK */

typedef struct {char chain[12];} MenageHmsh_tChain11;
typedef struct {int hours, minutes, seconds, hundredthes;} MenageHmsh_tHour;

extern void  MenageHmsh_Initialise(MenageHmsh_tHour *h);
extern void  MenageHmsh_InitialiseHundredthes(MenageHmsh_tHour *h);
extern void  MenageHmsh_AdvanceHours(MenageHmsh_tHour *h);
extern void  MenageHmsh_AdvanceMinutes(MenageHmsh_tHour *h);
extern void  MenageHmsh_AdvanceSeconds(MenageHmsh_tHour *h);
extern void  MenageHmsh_PlusNSeconds(int n, MenageHmsh_tHour *h);
extern void  MenageHmsh_PlusOneUnit(MenageHmsh_tHour *h, BOOLEAN *CarrySeconds);
extern void  MenageHmsh_Convert(MenageHmsh_tHour h, BOOLEAN hundredthes, MenageHmsh_tChain11 *chain);
extern BOOLEAN MenageHmsh_Equal(MenageHmsh_tHour h1, MenageHmsh_tHour h2);

typedef enum {destClock, destChrono} Display_tDest;

extern void Display_PutHour(MenageHmsh_tChain11 chain);
extern void Display_PutTimes(MenageHmsh_tChain11 chain);
extern void Display_Get(MenageHmsh_tChain11 *chain, Display_tDest *receiver);
