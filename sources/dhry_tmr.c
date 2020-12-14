/* dhry_tmr.c
 *
 * Author:  Kai Uwe Rommel <rommel@ars.de>
 * Created: Thu Dec 21 1995
 */
 
static char *rcsid =
"$Id: dhry_tmr.c,v 1.2 1999/10/28 17:35:29 rommel Exp rommel $";
static char *rcsrev = "$Revision: 1.2 $";

/*
 * $Log: dhry_tmr.c,v $
 * Revision 1.2  1999/10/28 17:35:29  rommel
 * fixed timer code
 *
 * Revision 1.1  1999/10/25 08:36:19  rommel
 * Initial revision
 * 
 */

#include <stdio.h>

extern unsigned long Number_Of_Runs;

#define THREADSTACK 65536

#ifdef OS2

#define INCL_DOS
#define INCL_NOPM
#include <os2.h>

typedef QWORD TIMER;

VOID APIENTRY timer_thread(ULONG nArg)
{
  HEV hSem;
  HTIMER hTimer;

  DosCreateEventSem(0, &hSem, DC_SEM_SHARED, 0);
  DosAsyncTimer(nArg, (HSEM) hSem, &hTimer);
  DosWaitEventSem(hSem, SEM_INDEFINITE_WAIT);
  DosStopTimer(hTimer);
  DosCloseEventSem(hSem);

  Number_Of_Runs = 0;

  DosExit(EXIT_THREAD, 0);
}

int start_alarm(int nSeconds)
{
  TID tid;

  Number_Of_Runs = -1;

  return DosCreateThread(&tid, timer_thread, nSeconds * 1000, 0, THREADSTACK);
}

int start_timer(TIMER *nStart)
{
  if (DosTmrQueryTime(nStart))
    return printf("Timer error.\n"), -1;

  return 0;
}

unsigned long stop_timer(TIMER *nStart, int accuracy)
{
  ULONG nFreq;
  TIMER nStop;

  if (DosTmrQueryTime(&nStop))
    return printf("Timer error.\n"), -1;
  if (DosTmrQueryFreq(&nFreq))
    return printf("Timer error.\n"), -1;

  nFreq = (nFreq + accuracy / 2) / accuracy;

  return (* (__int64*) &nStop - * (__int64*) nStart) / nFreq;
}

#endif

#ifdef WIN32

/* the following perhaps only works with IBM's compiler */
#define _INTEGRAL_MAX_BITS 64
#include <windows.h>

typedef LARGE_INTEGER TIMER;

DWORD CALLBACK timer_thread(void * pArg)
{
  Sleep((long) pArg * 1000);

  Number_Of_Runs = 0;

  return 0;
}

int start_alarm(long nSeconds)
{ 
  DWORD ttid;

  Number_Of_Runs = -1;

  if (CreateThread(0, THREADSTACK, timer_thread, (void *) nSeconds, 0, &ttid) == NULL)
    return printf("Cannot create timer thread.\n"), -1;

  return 0;
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

void main(int argc, char **argv)
{
  extern void dhry_main(int argc, char **argv);
  dhry_main(argc, argv);
}

/* end of dhry_tmr.c */
