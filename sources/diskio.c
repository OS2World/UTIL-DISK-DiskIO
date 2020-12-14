/* diskio.c - disk benchmark
 *
 * Author:  Kai Uwe Rommel <rommel@ars.muc.de>
 * Created: Fri Jul 08 1994
 */
 
static char *rcsid =
"$Id: diskio.c,v 1.18 2004/04/24 madded2 Exp rommel $";
static char *rcsrev = "$Revision: 1.18z $";

/*
 * $Log: diskio.c,v $
 * Revision 1.18  2004/04/24  madded2
 * many changes in OS/2 branch, many bugs fixed in HDD measurement
 * CAUTION: NEVER USE THIS BAG OF CRAP AS EXAMPLES OF DISK PROGRAMMING
 * now using native OS/2 CPU load measuring mechanism, DosPerfSysCall API
 * using MS timer counter on SMP systems to avoid bugs in DosTmr* API
 * changes in CD-ROM benchmarks for tiny CD-disks (c) by x-kot
 * added DVD disks support via OS/2 64-bit file API
 *
 * $Log: diskio.c,v $
 * Revision 1.17  1999/10/28 17:33:33  rommel
 * corrected OS/2 timer code
 *
 * Revision 1.16  1999/10/25 08:36:28  rommel
 * corrected timer code for NT
 *
 * Revision 1.15  1998/07/05 07:44:17  rommel
 * added Windows NT version
 * added multi disk benchmark
 * added preliminary multi thread I/O benchmark
 * many fixes
 * general cleanup
 *
 * Revision 1.14  1998/01/05 18:04:04  rommel
 * add fix for CD-ROM size detection problem
 *
 * Revision 1.13  1997/03/01 20:36:55  rommel
 * handle media access errors more gracefully
 *
 * Revision 1.12  1997/03/01 20:21:35  rommel
 * catch a few bugs and obscure situations,
 * increase accuracy, too
 *
 * Revision 1.11  1997/02/09 15:06:38  rommel
 * changed command line interface
 *
 * Revision 1.10  1997/01/12 21:15:10  rommel
 * added CD-ROM benchmarks
 *
 * Revision 1.9  1995/12/31 20:24:09  rommel
 * Changed CPU load calculation
 * General cleanup
 *
 * Revision 1.8  1995/12/28 11:28:07  rommel
 * Fixed async timer problem.
 *
 * Revision 1.7  1995/12/28 10:04:15  rommel
 * Added CPU benchmark (concurrently to disk I/O)
 *
 * Revision 1.6  1995/11/24 16:02:10  rommel
 * Added bus/drive cache speed test by 
 * repeatedly reading a small amount of data
 *
 * Revision 1.5  1995/08/09 13:07:02  rommel
 * Changes for new diskacc2 library, minor corrections, arguments.
 *
 * Revision 1.4  1994/07/11 14:23:00  rommel
 * Changed latency timing
 *
 * Revision 1.3  1994/07/09 13:07:20  rommel
 * Changed transfer speed test
 *
 * Revision 1.2  1994/07/08 21:53:05  rommel
 * Cleanup
 *
 * Revision 1.1  1994/07/08 21:29:41  rommel
 * Initial revision
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "diskacc.h"

#define INTERVAL     10
#define MAXTHREADS   4
#define THREADSTACK  65536

char *pBuffer;

int time_over;
long dhry_result, dhry_stones;

extern unsigned long Number_Of_Runs;
extern long dhry_stone(void);

#ifdef OS2

// always use OS/2 DosPerfSysCall API by default
#define OS2CPUPERF

#define INCL_DOS
#define INCL_DOSDEVICES
#define INCL_DOSDEVIOCTL
#define INCL_DOSERRORS
#define INCL_NOPM
#include <os2.h>

#ifdef OS2CPUPERF
#include <cpuperf.h>

int flPerfOk = 0;
CPUPERF_INFO CpuPerf;
#endif // of OS2CPUPERF

#define idle_priority() \
  DosSetPriority(PRTYS_THREAD, PRTYC_IDLETIME, PRTYD_MAXIMUM, 0)
#define normal_priority() \
  DosSetPriority(PRTYS_THREAD, PRTYC_REGULAR, PRTYD_MINIMUM, 0)

typedef HEV semaphore;
static ULONG nSemCount;
#define create_sem(phsem) \
  DosCreateEventSem(0, phsem, 0, FALSE)
#define destroy_sem(hsem) \
  DosCloseEventSem(hsem)
#define post_sem(hsem) \
  DosPostEventSem(hsem)
#define reset_sem(hsem) \
  DosResetEventSem(hsem, nSemCount)
#define wait_sem(hsem) \
  DosWaitEventSem(hsem, SEM_INDEFINITE_WAIT)

typedef QWORD TIMER;

// HiRes IRQ0 (DosTmr) timer usage flag, must be disabled on SMP systems
int flHiresTmr = 1;

HEV hSemTimer;

VOID APIENTRY timer_thread(ULONG nArg)
{
  HTIMER hTimer;

  DosCreateEventSem(0, &hSemTimer, DC_SEM_SHARED, 0);
  DosAsyncTimer(nArg * 1000, (HSEM) hSemTimer, &hTimer);
  DosWaitEventSem(hSemTimer, SEM_INDEFINITE_WAIT);
  DosStopTimer(hTimer);
  DosCloseEventSem(hSemTimer);

  time_over = 1;
  Number_Of_Runs = 0;

  DosExit(EXIT_THREAD, 0);
}

int start_alarm(ULONG nSeconds)
{ 
  TID ttid;

  time_over = 0;
  Number_Of_Runs = -1;

  if (DosCreateThread(&ttid, timer_thread, nSeconds, 0, THREADSTACK))
    return printf("Cannot create timer thread.\n"), -1;

  return 0;
}

void stop_alarm(void)
{
  DosPostEventSem(hSemTimer);
}

// init flHiresTmr flag
// note: DosTmrQueryTime() API not works on OS/2 SMP as designed (thanks scott),
// so on SMP systems we must use other timer - standart MS counter.
void init_timer (void)
{
    int nCPUs = 0;

    if (DosQuerySysInfo (QSV_NUMPROCESSORS,
                         QSV_NUMPROCESSORS,
                         &nCPUs,
                         sizeof(nCPUs)) == NO_ERROR)
    {
        if (nCPUs > 1) flHiresTmr = 0;
    }
}

// use MS counter timer for SMP systems and HiRes Timer for any other
int start_timer (TIMER *nStart)
{
    if (flHiresTmr)
    {
        if (DosTmrQueryTime(nStart) != NO_ERROR)
        {
            printf ("Timer error.\n");
            return (-1);
        }
    } else
    {
        nStart->ulHi = 0;
        if (DosQuerySysInfo (QSV_MS_COUNT,
                             QSV_MS_COUNT,
                             nStart,
                             sizeof(nStart)) != NO_ERROR)
        {
            printf ("Timer error.\n");
            return (-1);
        }
    }

    return (0);
}

int stop_timer(TIMER *nStart, int accuracy)
{
    TIMER nStop;

    if (flHiresTmr)
    {
        ULONG nFreq;

        if (DosTmrQueryTime (&nStop) != NO_ERROR)
        {
            printf ("Timer error.\n");
            return (-1);
        }
        if (DosTmrQueryFreq (&nFreq) != NO_ERROR)
        {
            printf ("Timer error.\n");
            return (-1);
        }

        nFreq = (nFreq + accuracy / 2) / accuracy;

        return (* (__int64*) &nStop - * (__int64*) nStart) / nFreq;
    } else
    {
        nStop.ulHi = 0;
        if (DosQuerySysInfo (QSV_MS_COUNT,
                             QSV_MS_COUNT,
                             &nStop,
                             sizeof(nStop)) != NO_ERROR)
        {
            printf("Timer error.\n");
            return (-1);
        }

        // check timer overflow (49 days boundary)
        if (nStop.ulLo < nStart->ulLo) nStop.ulHi++;

        return (((* (__int64*) &nStop - * (__int64*) nStart) * accuracy) / 1000);
    }
}

#endif

#ifdef WIN32

/* the following perhaps only works with IBM's compiler */
#define _INTEGRAL_MAX_BITS 64
#include <windows.h>

#define idle_priority() \
  SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_IDLE)
#define normal_priority() \
  SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL)

typedef HANDLE semaphore;
#define create_sem(phsem) \
  (*(phsem) = CreateEvent(0, TRUE, FALSE, 0))
#define destroy_sem(hsem) \
  CloseHandle(hsem)
#define post_sem(hsem) \
  SetEvent(hsem)
#define reset_sem(hsem) \
  ResetEvent(hsem)
#define wait_sem(hsem) \
  WaitForSingleObject(hsem, INFINITE)

typedef LARGE_INTEGER TIMER;

HANDLE hTimerThread;

DWORD CALLBACK timer_thread(void * pArg)
{
  Sleep((UINT) pArg * 1000);

  time_over = 1;
  Number_Of_Runs = 0;

  return 0;
}

int start_alarm(long nSeconds)
{ 
  DWORD ttid;

  time_over = 0;
  Number_Of_Runs = -1;

  hTimerThread = CreateThread(0, THREADSTACK, timer_thread, (void *) nSeconds, 0, &ttid);

  if (hTimerThread == NULL)
    return printf("Cannot create timer thread.\n"), -1;

  return 0;
}

void stop_alarm(void)
{
  TerminateThread(hTimerThread, 0);

  time_over = 1;
  Number_Of_Runs = 0;
}

int start_timer(TIMER *nStart)
{
  if (!QueryPerformanceCounter(nStart))
    return printf("Timer error.\n"), -1;

  return 0;
}

unsigned long stop_timer(TIMER *nStart, int nAccuracy)
{
  TIMER nStop, nFreq;

  if (!QueryPerformanceCounter(&nStop))
    return printf("Timer error.\n"), -1;
  if (!QueryPerformanceFrequency(&nFreq))
    return printf("Timer error.\n"), -1;

  nFreq.QuadPart = (nFreq.QuadPart + nAccuracy / 2) / nAccuracy;

  return (nStop.QuadPart - nStart->QuadPart) / nFreq.QuadPart;
}

#endif

typedef struct 
{
  int nHandle, nData, nCnt;
  unsigned nSector, nSides, nSectors, nTracks;
  void *pBuffer;
  semaphore sDone;
}
THREADPARMS;

void run_dhrystone(void)
{
  long dhry_time = dhry_stone();

  /* originally, time is measured in ms */
  dhry_time = (dhry_time + 5) / 10;
  /* now it is in units of 10 ms */

  if (dhry_time == 0)
    dhry_time = 1; /* if less than 10 ms, then assume at least 10 ms */

  /* now calculate runs per second */
  dhry_stones = Number_Of_Runs * 100 / dhry_time;
  /* by the time we cross 20 million dhrystones per second with a CPU, 
     we will hopefully have only 64-bit machines to run this on ... */
}

void dhry_thread(void *nArg)
{
  idle_priority();
  run_dhrystone();

  _endthread();
}

int bench_hd_bus(int nHandle, unsigned nSides, unsigned nSectors, unsigned nSector)
{
  int nCnt = 0, nData = 0, nTime, nTrack;
  TIMER nLocal;

  printf("Drive cache/bus transfer rate: ");
  fflush(stdout);

  // first read, fill cache
  DskRead(nHandle, 0, 0, 1, nSectors, pBuffer);

  if (start_alarm(INTERVAL))
    return -1;

  if (start_timer(&nLocal))
    return -1;

  while (!time_over)
  {
    if (DskRead(nHandle, 0, 0, 1, nSectors, pBuffer))
    {
      stop_alarm();
      printf("Disk read error.\n");
      return -1;
    }

    nData += nSectors * nSector;
  }

  if ((nTime = stop_timer(&nLocal, 1024)) == -1)
    return -1;

  printf("%d k/sec\n", nData / nTime);

  return 0;
}

int bench_hd_transfer(int nHandle, int nTrack, int nDirection, 
		      unsigned nSides, unsigned nSectors, unsigned nSector)
{
  int nCnt, nData = 0, nTime;
  TIMER nLocal;

  printf("Data transfer rate on cylinder %-4d: ", nTrack);
  fflush(stdout);

  if (start_alarm(INTERVAL))
    return -1;

  if (start_timer(&nLocal))
    return -1;

  for (nCnt = 0; !time_over; nCnt++)
  {
    if (DskRead(nHandle, nCnt % nSides, 
                nTrack + (nCnt / nSides) * nDirection, 1, nSectors, pBuffer))
    {
      stop_alarm();
      printf("Disk read error.\n");
      return -1;
    }

    nData += nSectors * nSector;
  }

  if ((nTime = stop_timer(&nLocal, 1024)) == -1)
    return -1;

  printf("%d k/sec\n", nData / nTime);

  return 0;
}

int bench_hd_cpuusage(int nHandle, unsigned nSides, 
		      unsigned nSectors, unsigned nSector)
{
  int nCnt, nData = 0, nTime, nPercent;
  TIMER nLocal;

  printf("CPU usage by full speed disk transfers: ");
  fflush(stdout);

  if (start_alarm(INTERVAL))
    return -1;

#ifndef OS2CPUPERF
  if (_beginthread(dhry_thread, 0, THREADSTACK, 0) == -1)
    return -1;
#else
  if (flPerfOk)
  {
    if (CpuPerfStart() != NO_ERROR)
      return -1;
  } else
  {
    if (_beginthread(dhry_thread, 0, THREADSTACK, 0) == -1)
      return -1;
  }
#endif

  if (start_timer(&nLocal))
    return -1;

  for (nCnt = 0; !time_over; nCnt++)
  {
    if (DskRead(nHandle, nCnt % nSides, nCnt / nSides, 1, nSectors, pBuffer))
    {
      stop_alarm();
      printf("Disk read error.\n");
      return -1;
    }

    nData += nSectors * nSector;
  }

  if ((nTime = stop_timer(&nLocal, 1024)) == -1)
    return -1;

#ifndef OS2CPUPERF
  nPercent = (dhry_result - dhry_stones) * 100 / dhry_result;
#else
  if (flPerfOk)
  {
    if (CpuPerfStop(&CpuPerf) != NO_ERROR)
      return -1;
    nPercent = 100 - CpuPerf.ulIdle;
  } else
  {
    nPercent = (dhry_result - dhry_stones) * 100 / dhry_result;
  }
#endif

  printf("%d%%\n", nPercent);

  return 0;
}

int bench_hd_latency(int nHandle, unsigned nTracks, unsigned nSides, unsigned nSectors)
{
  int nCnt, nSector, nTime;
  TIMER nLocal;

  printf("Average latency time: ");
  fflush(stdout);

  srand(1);

  if (start_alarm(INTERVAL))
    return -1;

  if (start_timer(&nLocal))
    return -1;

  for (nCnt = 0; !time_over; nCnt++)
  {
    nSector = rand() * nSectors / RAND_MAX + 1;

    if (DskRead(nHandle, 0, 0, nSector, 1, pBuffer))
    {
      stop_alarm();
      printf("Disk read error.\n");
      return -1;
    }
  }

  if ((nTime = stop_timer(&nLocal, 1000)) == -1)
    return -1;

  nTime = nTime * 10 / nCnt;

  printf("%d.%d ms\n", nTime / 10, nTime % 10);

  return nTime;
}

int bench_hd_seek(int nHandle, unsigned nTracks, unsigned nSides, unsigned nSectors)
{
  int nCnt, nSide, nTrack, nSector, nTime;
  TIMER nLocal;

  printf("Average data access time: ");
  fflush(stdout);

  srand(1);

  if (start_alarm(INTERVAL))
    return -1;

  if (start_timer(&nLocal))
    return -1;

  for (nCnt = 0; !time_over; nCnt++)
  {
    nSide   = rand() * nSides   / RAND_MAX;
    nSector = rand() * nSectors / RAND_MAX;
    nTrack  = rand() * nTracks  / RAND_MAX;

    if (DskRead(nHandle, nSide, nTrack, nSector, 1, pBuffer))
    {
      stop_alarm();
      printf("Disk read error.\n");
      return -1;
    }
  }

  if ((nTime = stop_timer(&nLocal, 1000)) == -1)
    return -1;

  nTime = nTime * 10 / nCnt;

  printf("%d.%d ms\n", nTime / 10, nTime % 10);

  return 0;
}

void bench_hd_thread(void *pArg)
{
  THREADPARMS *ptr = (THREADPARMS *) pArg;
  int nTrack, nSide;

#if 0
  srand(1);
#endif

  while (!time_over)
  {
#if 0
    nTrack = rand() * ptr->nTracks / RAND_MAX;
    nSide = rand() * ptr->nSides / RAND_MAX;
#else
    nTrack = ptr->nCnt / ptr->nSides;
    nSide = ptr->nCnt % ptr->nSides;
    ptr->nCnt += MAXTHREADS;
#endif

    if (DskRead(ptr->nHandle, nSide, nTrack, 1, ptr->nSectors, ptr->pBuffer))
    {
      stop_alarm();
      printf("Disk read error.\n");
      break;
    }

    ptr->nData += ptr->nSectors * ptr->nSector;
  }

  post_sem(ptr->sDone);

  _endthread();
}

int bench_hd_mthread(int nHandle, unsigned nTracks, unsigned nSides, 
		     unsigned nSectors, unsigned nSector)
{
  int nTime, nPercent, nCnt, nTotal;
  THREADPARMS tHD[MAXTHREADS];
  TIMER nLocal;

  printf("Multithreaded disk I/O (%d threads): ", MAXTHREADS);
  fflush(stdout);

  for (nCnt = 0; nCnt < MAXTHREADS; nCnt++)
  {
    tHD[nCnt].nHandle  = nHandle;
    tHD[nCnt].nTracks  = nTracks;
    tHD[nCnt].nSides   = nSides;
    tHD[nCnt].nSectors = nSectors;
    tHD[nCnt].nSector  = nSector;
    tHD[nCnt].nData    = 0;
    tHD[nCnt].nCnt     = nCnt;

    if ((tHD[nCnt].pBuffer = DskAlloc(nSectors, nSector)) == NULL)
      return printf("\nNot enough memory.\n"), -1;

    create_sem(&(tHD[nCnt].sDone));
  }

  if (start_alarm(INTERVAL))
    return -1;

  if (start_timer(&nLocal))
    return -1;

  for (nCnt = 0; nCnt < MAXTHREADS; nCnt++)
    if (_beginthread(bench_hd_thread, 0, THREADSTACK, &tHD[nCnt]) == -1)
      return -1;

#ifndef OS2CPUPERF
  idle_priority();
  run_dhrystone();
  normal_priority();
  nPercent = (dhry_result - dhry_stones) * 100 / dhry_result;
#else
  if (flPerfOk)
  {
    if (CpuPerfStart() != NO_ERROR)
      return -1;

    for (nCnt = 0; nCnt < MAXTHREADS; nCnt++) wait_sem(tHD[nCnt].sDone);

    if (CpuPerfStop(&CpuPerf) != NO_ERROR)
      return -1;

    nPercent = 100 - CpuPerf.ulIdle;
  } else
  {
    idle_priority();
    run_dhrystone();
    normal_priority();
    nPercent = (dhry_result - dhry_stones) * 100 / dhry_result;
  }
#endif

  if ((nTime = stop_timer(&nLocal, 1024)) == -1)
    return -1;

  nTotal = 0;

  for (nCnt = 0; nCnt < MAXTHREADS; nCnt++)
  {
    wait_sem(tHD[nCnt].sDone);
    destroy_sem(tHD[nCnt].sDone);

    DskFree(tHD[nCnt].pBuffer);

    nTotal += tHD[nCnt].nData;
  }

  printf("%d k/sec, %d%% CPU usage\n", nTotal / nTime, nPercent);

  return 0;
}

int bench_hd(int nDisk)
{
  int nHandle;
  unsigned nSector, nSides, nTracks, nSectors;
  char szName[8];

  sprintf(szName, "$%d:", nDisk);

  if ((nHandle = DskOpen(szName, 0, 0, &nSector, &nSides, &nTracks, &nSectors)) < 0)
    return printf("\nCannot access disk %d.\n", nDisk), -1;

  printf("\nHard disk %d: %d sides, %d cylinders, %d sectors per track = %d MB\n", 
	 nDisk, nSides, nTracks, nSectors,
	 nSides * nTracks * nSectors / 2048);

  if ((pBuffer = DskAlloc(nSectors, nSector)) == NULL)
    return printf("\nNot enough memory.\n"), -1;

  bench_hd_bus(nHandle, nSides, nSectors, nSector);
  bench_hd_transfer(nHandle, 0, 1, nSides, nSectors, nSector);
  bench_hd_transfer(nHandle, nTracks - 2, -1, nSides, nSectors, nSector);
  bench_hd_cpuusage(nHandle, nSides, nSectors, nSector);
  /* bench_hd_latency(nHandle, nTracks, nSides, nSectors); */
  bench_hd_seek(nHandle, nTracks, nSides, nSectors);
  bench_hd_mthread(nHandle, nTracks, nSides, nSectors, nSector);

  DskFree(pBuffer);
  DskClose(nHandle);

  return 0;
}

int bench_cd_transfer(int nHandle, int nStart, int nSectors)
{
  int nCnt, nData = 0, nTime, nRate;
  TIMER nLocal;

  printf("Data transfer rate at sector %-8d: ", nStart);
  fflush(stdout);
  
  if (start_alarm(INTERVAL))
    return -1;

  if (start_timer(&nLocal))
    return -1;

  for (nCnt = 0; !time_over; nCnt++)
  {
    if (nStart + nCnt * 32 + 32 > nSectors)
      nCnt = 0;

    if (CDRead(nHandle, nStart + nCnt * 32, 32, pBuffer) == -1)
    {
      stop_alarm();
      printf("CD-ROM read error.\n");
      return -1;
    }

    nData += 32 * 2048;
  }

  if ((nTime = stop_timer(&nLocal, 1024)) == -1)
    return -1;

  nRate = nData / nTime;
  printf("%d k/sec (~%.1fx)\n", nRate, (double) nRate / 150.0);

  return 0;
}

int bench_cd_cpuusage(int nHandle, int nStart, int nSectors)
{
  int nCnt, nData = 0, nTime, nPercent;
  TIMER nLocal;

  printf("CPU usage by full speed reads (middle of medium): ");
  fflush(stdout);

  if (start_alarm(INTERVAL))
    return -1;

#ifndef OS2CPUPERF
  if (_beginthread(dhry_thread, 0, THREADSTACK, 0) == -1)
    return -1;
#else
  if (flPerfOk)
  {
    if (CpuPerfStart() != NO_ERROR)
      return -1;
  } else
  {
    if (_beginthread(dhry_thread, 0, THREADSTACK, 0) == -1)
      return -1;
  }
#endif

  if (start_timer(&nLocal))
    return -1;

  for (nCnt = 0; !time_over; nCnt++)
  {
    if (nStart + nCnt * 32 + 32 > nSectors)
      nCnt = 0;

    if (CDRead(nHandle, nStart + nCnt * 32, 32, pBuffer) == -1)
    {
      stop_alarm();
      printf("CD-ROM read error.\n");
      return -1;
    }

    nData += 32 * 2048;
  }

  if ((nTime = stop_timer(&nLocal, 1024)) == -1)
    return -1;

#ifndef OS2CPUPERF
  nPercent = (dhry_result - dhry_stones) * 100 / dhry_result;
#else
  if (flPerfOk)
  {
    if (CpuPerfStop(&CpuPerf) != NO_ERROR)
      return -1;
    nPercent = 100 - CpuPerf.ulIdle;
  } else
  {
    nPercent = (dhry_result - dhry_stones) * 100 / dhry_result;
  }
#endif

  printf("%d%%\n", nPercent);

  return 0;
}

int bench_cd_seek(int nHandle, unsigned nSectors)
{
  int nCnt, nTime;
  TIMER nLocal;

  printf("Average data access time: ");
  fflush(stdout);

  srand(1);

  if (start_alarm(INTERVAL))
    return -1;

  if (start_timer(&nLocal))
    return -1;

  for (nCnt = 0; !time_over; nCnt++)
  {
    double dblSector = nSectors;
    dblSector *= rand();
    dblSector /= RAND_MAX;

    if (CDRead(nHandle, dblSector, 1, pBuffer) != 1)
    {
      stop_alarm();
      printf("CD-ROM read error.\n");
      return -1;
    }
  }

  if ((nTime = stop_timer(&nLocal, 1000)) == -1)
    return -1;

  nTime = nTime * 10 / nCnt;

  printf("%d.%d ms\n", nTime / 10, nTime % 10);

  return 0;
}

int bench_cd(int nDrive)
{
  int nHandle, nDriveLetter, nCnt;
  unsigned nSectors;
  char szName[8], szUPC[8];

  if ((nDriveLetter = CDFind(nDrive)) == -1)
    return printf("\nCannot access CD-ROM drive %d.\n", nDrive), -1;

  sprintf(szName, "%c:", nDriveLetter);

  if ((nHandle = CDOpen(szName, 1, szUPC, &nSectors)) == -1)
    return printf("\nCannot access CD-ROM drive %d.\n", nDrive), -1;

  printf("\nCD-ROM drive %s medium has %d sectors = %d MB\n", 
	 szName, nSectors, nSectors / 512);

  if ((pBuffer = DskAlloc(32, 2048)) == NULL)
    return printf("\nNot enough memory.\n"), -1;

  /* spin up and seek to first sector */
  if (CDRead(nHandle, nSectors - 64, 32, pBuffer) == -1)
    return printf("CD-ROM read error.\n"), -1;
  for (nCnt = 0; nCnt < 4096; nCnt += 32)
    if (CDRead(nHandle, nCnt % (nSectors - 32), 32, pBuffer) == -1)
      return printf("CD-ROM read error.\n"), -1;
  if (CDRead(nHandle, 0, 32, pBuffer) == -1)
    return printf("CD-ROM read error.\n"), -1;

  bench_cd_transfer(nHandle, 0, nSectors);
  if ((nSectors - 32768) < (nSectors / 2))
    bench_cd_transfer(nHandle, nSectors / 2, nSectors);
  else
    bench_cd_transfer(nHandle, nSectors - 32768, nSectors);
  bench_cd_cpuusage(nHandle, nSectors / 2, nSectors);
  bench_cd_seek(nHandle, nSectors);

  DskFree(pBuffer);
  CDClose(nHandle);

  return 0;
}

void bench_concurrent_hd(void *pArg)
{
  THREADPARMS *ptr = (THREADPARMS *) pArg;
  int nCnt;

  for (nCnt = 0; !time_over; nCnt++)
  {
    if (DskRead(ptr->nHandle,
                nCnt % ptr->nSides, nCnt / ptr->nSides, 1,
                ptr->nSectors, ptr->pBuffer))
    {
      stop_alarm();
      printf("Disk read error.\n");
      break;
    }

    ptr->nData += ptr->nSectors * ptr->nSector;
  }

  post_sem(ptr->sDone);

  _endthread();
}

int bench_concurrent_allhds(int *nHD, int nDisks)
{
  int nTime, nPercent, nTotal, nCnt;
  char szName[8];
  THREADPARMS tHD[256];
  TIMER nLocal;

  printf("\nConcurrent full speed reads on %d hard disks: ", nDisks);
  fflush(stdout);

  for (nCnt = 0; nCnt < nDisks; nCnt++)
  {
    sprintf(szName, "$%d:", nHD[nCnt]);

    if ((tHD[nCnt].nHandle = DskOpen(szName, 0, 0, &tHD[nCnt].nSector,
				     &tHD[nCnt].nSides, &tHD[nCnt].nTracks, 
				     &tHD[nCnt].nSectors)) < 0)
      return printf("\nCannot access disk %d.\n", nHD[nCnt]), -1;

    if ((tHD[nCnt].pBuffer = DskAlloc(tHD[nCnt].nSectors, 
				      tHD[nCnt].nSector)) == NULL)
      return printf("\nNot enough memory.\n"), -1;

    create_sem(&tHD[nCnt].sDone);
    tHD[nCnt].nData = 0;
    tHD[nCnt].nCnt = nCnt;
  }

  if (start_alarm(INTERVAL))
    return -1;

  if (start_timer(&nLocal))
    return -1;

  for (nCnt = 0; nCnt < nDisks; nCnt++)
    if (_beginthread(bench_concurrent_hd, 0, THREADSTACK, &tHD[nCnt]) == -1)
      return -1;

#ifndef OS2CPUPERF
  idle_priority();
  run_dhrystone();
  normal_priority();
  nPercent = (dhry_result - dhry_stones) * 100 / dhry_result;
#else
  if (flPerfOk)
  {
    if (CpuPerfStart() != NO_ERROR)
      return -1;

    for (nCnt = 0; nCnt < nDisks; nCnt++) wait_sem(tHD[nCnt].sDone);

    if (CpuPerfStop(&CpuPerf) != NO_ERROR)
      return -1;

    nPercent = 100 - CpuPerf.ulIdle;
  } else
  {
    idle_priority();
    run_dhrystone();
    normal_priority();
    nPercent = (dhry_result - dhry_stones) * 100 / dhry_result;
  }
#endif

  if ((nTime = stop_timer(&nLocal, 1024)) == -1)
    return -1;

  nTotal = 0;

  for (nCnt = 0; nCnt < nDisks; nCnt++)
  {
    wait_sem(tHD[nCnt].sDone);
    destroy_sem(tHD[nCnt].sDone);

    DskFree(tHD[nCnt].pBuffer);
    DskClose(tHD[nCnt].nHandle);

    nTotal += tHD[nCnt].nData;
  }
  
  printf("\nCombined throughput: %d k/sec\n", nTotal / nTime);
  printf("CPU usage: %d%%\n", nPercent);

  return 0;
}

void bench_concurrent_cd(void *pArg)
{
  THREADPARMS *ptr = (THREADPARMS *) pArg;
  int nCnt, nStart = ptr->nSectors / 2;

  for (nCnt = 0; !time_over; nCnt++)
  {
    if (CDRead(ptr->nHandle, nStart + nCnt * 32, 32, ptr->pBuffer) == -1)
    {
      stop_alarm();
      printf("CD-ROM read error.\n");
      break;
    }

    ptr->nData += 32 * 2048;
  }

  post_sem(ptr->sDone);

  _endthread();
}

int bench_concurrent_hdcd(int nDisk, int nCD)
{
  int nDriveLetter, nTime, nPercent, nCnt;
  char szName[8], szUPC[8];
  THREADPARMS tHD, tCD;
  TIMER nLocal;

  printf("\nConcurrent hard disk %d and CD-ROM %d reads at full speed: ", nDisk, nCD);
  fflush(stdout);

  sprintf(szName, "$%d:", nDisk);

  if ((tHD.nHandle = DskOpen(szName, 0, 0, &tHD.nSector,
			     &tHD.nSides, &tHD.nTracks, &tHD.nSectors)) < 0)
    return printf("\nCannot access disk %d.\n", nDisk), -1;

  if ((tHD.pBuffer = DskAlloc(tHD.nSectors, tHD.nSector)) == NULL)
    return printf("\nNot enough memory.\n"), -1;

  create_sem(&tHD.sDone);
  tHD.nData = tHD.nCnt = 0;

  if ((nDriveLetter = CDFind(nCD)) == -1)
    return printf("\nCannot access CD-ROM drive %d.\n", nCD), -1;

  sprintf(szName, "%c:", nDriveLetter);

  if ((tCD.nHandle = CDOpen(szName, 1, szUPC, &tCD.nSectors)) == -1)
    return printf("\nCannot access CD-ROM drive %d.\n", nCD), -1;

  if ((tCD.pBuffer = DskAlloc(32, 2048)) == NULL)
    return printf("\nNot enough memory.\n"), -1;

  /* spin up and seek to first sector */
  if (CDRead(tCD.nHandle, tCD.nSectors - 64, 32, tCD.pBuffer) == -1)
    return printf("CD-ROM read error.\n"), -1;
  for (nCnt = 0; nCnt < 512; nCnt += 32)
    if (CDRead(tCD.nHandle, tCD.nSectors / 2 + nCnt, 32, tCD.pBuffer) == -1)
      return printf("CD-ROM read error.\n"), -1;
  if (CDRead(tCD.nHandle, 0, 32, tCD.pBuffer) == -1)
    return printf("CD-ROM read error.\n"), -1;

  create_sem(&tCD.sDone);
  tCD.nData = tCD.nCnt = 0;

  if (start_alarm(INTERVAL))
    return -1;

  if (start_timer(&nLocal))
    return -1;

  if (_beginthread(bench_concurrent_hd, 0, THREADSTACK, &tHD) == -1)
    return -1;
  if (_beginthread(bench_concurrent_cd, 0, THREADSTACK, &tCD) == -1)
    return -1;

#ifndef OS2CPUPERF
  idle_priority();
  run_dhrystone();
  normal_priority();
  nPercent = (dhry_result - dhry_stones) * 100 / dhry_result;
#else
  if (flPerfOk)
  {
    if (CpuPerfStart() != NO_ERROR)
      return -1;

    wait_sem(tHD.sDone);
    wait_sem(tCD.sDone);

    if (CpuPerfStop(&CpuPerf) != NO_ERROR)
      return -1;
    nPercent = 100 - CpuPerf.ulIdle;
  } else
  {
    idle_priority();
    run_dhrystone();
    normal_priority();
    nPercent = (dhry_result - dhry_stones) * 100 / dhry_result;
  }
#endif

  if ((nTime = stop_timer(&nLocal, 1024)) == -1)
    return -1;

  wait_sem(tHD.sDone);
  destroy_sem(tHD.sDone);

  DskFree(tHD.pBuffer);
  DskClose(tHD.nHandle);

  wait_sem(tCD.sDone);
  destroy_sem(tCD.sDone);

  DskFree(tCD.pBuffer);
  CDClose(tCD.nHandle);

  printf("\nHard disk throughput (cylinder 0): %d k/sec\n", tHD.nData / nTime);
  printf("CD-ROM throughput (middle of medium): %d k/sec\n", tCD.nData / nTime);

  printf("CPU usage: %d%%\n", nPercent);

  return 0;
}

int bench_dhry(void)
{
  printf("Dhrystone benchmark for this CPU: ");
  fflush(stdout);

  if (start_alarm(INTERVAL / 2))
    return -1;

  run_dhrystone();

  dhry_result = dhry_stones;

  if (dhry_result == 0)
    dhry_result = 1; /* to avoid dividing by zero later on */

  printf("%d runs/sec\n", dhry_result);

  return 0;
}

int main(int argc, char **argv)
{
  char szVersion[32];
  int nDisks, nCDROMs, nCount, xHD[256], xCD[256], cHD = 0, cCD = 0;
  int rc;

  strcpy(szVersion, rcsrev + sizeof("$Revision: ") - 1);
  *strchr(szVersion, ' ') = 0;

  printf("DISKIO - Fixed Disk Benchmark, Version %s"
	 "\n(C) 1994-1998 Kai Uwe Rommel"
         "\n(C) 2004 madded2\n", szVersion);

#ifdef OS2
  init_timer ();
  CDInit ();
#endif

#ifdef OS2CPUPERF
  rc = CpuPerfInit ();
  if (rc == NO_ERROR)
  {
    flPerfOk = 1;
  } else
  {
    flPerfOk = 0;
    printf("\nOS/2 CPU load measuring mechanism disabled, rc=%d\n", rc);
  }
#endif

  nDisks = DskCount();
  printf("\nNumber of fixed disks: %d\n", nDisks);

  nCDROMs = CDFind(0);
  if (nCDROMs == -1) /* not even the driver is loaded */
    nCDROMs = 0;
  printf("Number of CD-ROM drives: %d\n", nCDROMs);

  if (argc > 1)
  {
    nDisks = nCDROMs = 0;

    for (nCount = 1; nCount < argc; )
      if (stricmp(argv[nCount], "-hd") == 0)
      {
        for (nCount++; argv[nCount] && argv[nCount][0] != '-'; nCount++)    /*PLF Wed  97-02-26 22:31:38*/
	  xHD[nDisks++] = atoi(argv[nCount]);
      }
      else if (stricmp(argv[nCount], "-cd") == 0)
      {
        for (nCount++; argv[nCount] && argv[nCount][0] != '-'; nCount++)    /*PLF Wed  97-02-26 22:31:47*/
	  xCD[nCDROMs++] = atoi(argv[nCount]);
      }      
      else if (stricmp(argv[nCount], "-c") == 0)
      {
	if(argv[nCount+1] && argv[nCount+2])
	  if (argv[nCount + 1][0] != '-' && argv[nCount + 2][0] != '-')
	  {
	    cHD = atoi(argv[nCount + 1]);
	    cCD = atoi(argv[nCount + 2]);
	    nCount += 3;
	  }
      }
      else if (stricmp(argv[nCount], "-?") == 0)
      {
	printf("\nUsage:\tdiskio [options]\n"
	       "\n\t-hd <list of hard disk numbers>"
	       "\n\t-cd <list of CD-ROM drive numbers>"
      	       "\n\t-c  <pair of hard disk and CD-ROM drive numbers>\n"
	       "\nHard disk and CD-ROM drive numbers are physical ones and start at 1."
	       "\nThe drive number lists must be blank separated.\n"
	       "\nWith -cd, -hd and -c you can explicitly select the drives to be used"
	       "\nfor hard disk, CD-ROM drive and concurrent hard disk and CD-ROM benchmarks.\n"
	       "\nIf none of them is used, all drives are used for hard disk and CD-ROM"
	       "\ndrive benchmarks and the first hard disk and CD-ROM drives are used"
	       "\nfor the concurrent hard disk and CD-ROM benchmark\n"
	       "\nExample: diskio -hd 2 3 -cd 1 -c 3 1\n");
	exit(1);
      }

      else
	nCount++;
  }
  else
  {
    for (nCount = 1; nCount <= nDisks; nCount++)
      xHD[nCount - 1] = nCount;

    for (nCount = 1; nCount <= nCDROMs; nCount++)
      xCD[nCount - 1] = nCount;

    if (nDisks > 0 && nCDROMs > 0)
      cHD = cCD = 1;
  }

  printf("\nDhrystone 2.1 C benchmark routines (C) 1988 Reinhold P. Weicker\n");
  bench_dhry();

  for (nCount = 0; nCount < nDisks; nCount++)
    bench_hd(xHD[nCount]);

  if (nDisks > 1)
    bench_concurrent_allhds(xHD, nDisks);

  for (nCount = 0; nCount < nCDROMs; nCount++)
    bench_cd(xCD[nCount]);

  if (cHD != 0 && cCD != 0)
    bench_concurrent_hdcd(cHD, cCD);

  return 0;
}

/* end of diskio.c */
