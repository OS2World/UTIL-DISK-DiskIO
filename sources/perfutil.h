
/*
    perfutil.h
    DosPerfSysCall() API call header.
*/

// shit against multiple inclusion
#ifndef PERFCALL_INCLUDED
#define PERFCALL_INCLUDED

#define INCL_BASE
#include <os2.h>

#ifdef __cplusplus
    extern "C" {
#endif

#pragma pack (1)


/*
    DosPerfSysCall Function Prototype
*/
 
/* The  ordinal for DosPerfSysCall (in BSEORD.H) */
/* is defined as ORD_DOS32PERFSYSCALL         */

#define ORD_DOS32PERFSYSCALL (976)
 
APIRET APIENTRY DosPerfSysCall (ULONG ulCommand,
                                ULONG ulParm1,
                                ULONG ulParm2,
                                ULONG ulParm3);
 
/*
  *
  * Software Tracing
  * ----------------
  *
*/
 
#define   CMD_SOFTTRACE_LOG (0x14)
 
typedef struct _HOOKDATA
{
    ULONG ulLength;
    PBYTE pData;
} HOOKDATA;

typedef HOOKDATA * PHOOKDATA;
 
 
/*
  *
  * CPU Utilization
  * ---------------
  *
*/
 
#define   CMD_KI_RDCNT    (0x63)

// note: on SMP machine DosPerfSysCall() will return an array of
// CPUUTIL structures, by number of processors.
 
typedef struct _CPUUTIL
{
    ULONG ulTimeLow;     /* Low 32 bits of time stamp      */
    ULONG ulTimeHigh;    /* High 32 bits of time stamp     */
    ULONG ulIdleLow;     /* Low 32 bits of idle time       */
    ULONG ulIdleHigh;    /* High 32 bits of idle time      */
    ULONG ulBusyLow;     /* Low 32 bits of busy time       */
    ULONG ulBusyHigh;    /* High 32 bits of busy time      */
    ULONG ulIntrLow;     /* Low 32 bits of interrupt time  */
    ULONG ulIntrHigh;    /* High 32 bits of interrupt time */
} CPUUTIL;
 
typedef CPUUTIL *PCPUUTIL;

typedef unsigned __int64 TIMESTAMP64;

typedef struct _CPUUTIL64
{
    TIMESTAMP64 qwTime;  // time stamp
    TIMESTAMP64 qwIdle;  // idle time
    TIMESTAMP64 qwBusy;  // busy time
    TIMESTAMP64 qwIntr;  // interrupt time
} CPUUTIL64;

typedef CPUUTIL64 *PCPUUTIL64;
 

#pragma pack ()

#ifdef __cplusplus
    }
#endif

#endif // of PERFCALL_INCLUDED

