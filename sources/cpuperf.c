
/*
    cpuperf.c
    Calc CPU usage for diskio/os2. SMP-aware.
    (c) 2004 by madded2.

    note: no multithread aware version !
*/

#define INCL_DOS
#define INCL_DOSERRORS
#define INCL_NOPM
#include <os2.h>

#include <perfutil.h>
#include <cpuperf.h>

#include <stdlib.h>


typedef struct _CPUPERF_GLOBAL
{
    ULONG ulInited;                    // 0 - not inited
    ULONG ulCPUs;                      // number of CPUs detected
    PCPUUTIL64 pCpuPerf[2];            // start/stop measure data
} CPUPERF_GLOBAL;


// global data
CPUPERF_GLOBAL gCpuPerf = { 0 };


ULONG CpuPerfInit (void)
{
    ULONG rc;

    if (gCpuPerf.ulInited != 0)
    {
        // already inited
        return (ERROR_ACCESS_DENIED);
    }

    // get number of CPUs in system
    rc = DosQuerySysInfo (QSV_NUMPROCESSORS,
                          QSV_NUMPROCESSORS,
                          &gCpuPerf.ulCPUs,
                          sizeof(gCpuPerf.ulCPUs));
    if (rc != NO_ERROR)
    {
        // error, assume we got only 1 CPU
        gCpuPerf.ulCPUs = 1;
    }

    gCpuPerf.pCpuPerf[0] = (PCPUUTIL64)calloc (1, gCpuPerf.ulCPUs * sizeof(CPUUTIL64));
    if (gCpuPerf.pCpuPerf[0] == NULL)
    {
        return (ERROR_NOT_ENOUGH_MEMORY);
    }
    gCpuPerf.pCpuPerf[1] = (PCPUUTIL64)calloc (1, gCpuPerf.ulCPUs * sizeof(CPUUTIL64));
    if (gCpuPerf.pCpuPerf[1] == NULL)
    {
        free (gCpuPerf.pCpuPerf[0]);
        return (ERROR_NOT_ENOUGH_MEMORY);
    }

    // check if Perf api available
    rc = DosPerfSysCall (CMD_KI_RDCNT,
                         (ULONG)gCpuPerf.pCpuPerf[0],
                         0,
                         0);
    if (rc != NO_ERROR)
    {
        // error
        free (gCpuPerf.pCpuPerf[0]);
        free (gCpuPerf.pCpuPerf[1]);
        return (rc);
    }

    // all ok
    gCpuPerf.ulInited = 1;

    return (NO_ERROR);
}


ULONG CpuPerfClose (void)
{
    if (gCpuPerf.ulInited != 1)
    {
        // not inited
        return (ERROR_ACCESS_DENIED);
    }

    free (gCpuPerf.pCpuPerf[0]);
    free (gCpuPerf.pCpuPerf[1]);
    
    gCpuPerf.ulInited = 0;

    return (NO_ERROR);
}


ULONG CpuPerfStart (void)
{
    ULONG rc;

    if (gCpuPerf.ulInited != 1)
    {
        // not inited
        return (ERROR_ACCESS_DENIED);
    }

    // get current timestamps    
    rc = DosPerfSysCall (CMD_KI_RDCNT,
                         (ULONG)gCpuPerf.pCpuPerf[0],
                         0,
                         0);

    return (rc);
}


ULONG CpuPerfStop (PCPUPERF_INFO pInfo)
{
    ULONG rc;
    ULONG ulIdx;

    double dblIdle = 0;
    double dblBusy = 0;
    double dblIntr = 0;

    PCPUUTIL64 pCpu0 = gCpuPerf.pCpuPerf[0];
    PCPUUTIL64 pCpu1 = gCpuPerf.pCpuPerf[1];

    if ((gCpuPerf.ulInited != 1) ||
        (pInfo == NULL))
    {
        // not inited
        return (ERROR_ACCESS_DENIED);
    }

    // get current timestamps    
    rc = DosPerfSysCall (CMD_KI_RDCNT,
                         (ULONG)gCpuPerf.pCpuPerf[1],
                         0,
                         0);
    if (rc != NO_ERROR)
    {
        // error
        return (rc);
    }

    // calc load for every CPU
    for (ulIdx = 0; ulIdx < gCpuPerf.ulCPUs; ulIdx++)
    {
        double dblTmp, dblTime;

        // delta total time
        dblTime = pCpu1->qwTime - pCpu0->qwTime;

        // calc idle %
        dblTmp = pCpu1->qwIdle - pCpu0->qwIdle;
        dblTmp = dblTmp / dblTime;
        dblIdle += dblTmp;

        // calc busy %
        dblTmp = pCpu1->qwBusy - pCpu0->qwBusy;
        dblTmp = dblTmp / dblTime;
        dblBusy += dblTmp;

        // calc interrupt %
        dblTmp = pCpu1->qwIntr - pCpu0->qwIntr;
        dblTmp = dblTmp / dblTime;
        dblIntr += dblTmp;

        // next CPU
        pCpu0++;
        pCpu1++;
    }

    // calc common load
    dblIdle /= gCpuPerf.ulCPUs;
    dblBusy /= gCpuPerf.ulCPUs;
    dblIntr /= gCpuPerf.ulCPUs;

    // make real %
    dblIdle *= 100;
    dblBusy *= 100;
    dblIntr *= 100;

    // save result, dirty trick with 0.5 ;)
    pInfo->ulIdle = dblIdle + 0.5;
    pInfo->ulBusy = dblBusy + 0.5;
    pInfo->ulIntr = dblIntr + 0.5;

    // final correction
    if ((pInfo->ulIdle + pInfo->ulBusy + pInfo->ulIntr) != 100)
    {
        pInfo->ulIdle = 100 - (pInfo->ulBusy + pInfo->ulIntr);
    }

    return (NO_ERROR);
}
