
/*
    cpuperf.h
    Calc CPU usage for diskio/os2. SMP-aware.
    (c) 2004 by madded2.

    note: no multithread aware version !
*/

// shit against multiple inclusion (__COOL__H nameing (c) sunlover)
#ifndef __CPUPERF__H
#define __CPUPERF__H


// result data
typedef struct _CPUPERF_INFO
{
    ULONG ulIdle;   // *
    ULONG ulBusy;   // * - common summary info in % for all CPUs
    ULONG ulIntr;   // *
} CPUPERF_INFO;

typedef CPUPERF_INFO *PCPUPERF_INFO;


ULONG CpuPerfInit (void);
ULONG CpuPerfClose (void);

ULONG CpuPerfStart (void);
ULONG CpuPerfStop (PCPUPERF_INFO pInfo);


#endif /* __CPUPERF__H */
