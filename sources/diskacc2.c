/* diskacc2.c - direct disk access library for OS/2 2.x.
 *
 * Author:  Kai Uwe Rommel <rommel@ars.de>
 * Created: Fri Jul 08 1994
 */

static char *rcsid =
"$Id: diskacc2.c,v 1.8 2004/04/24 madded2 Exp rommel $";
static char *rcsrev = "$Revision: 1.8 $";

/*
 * $Log: diskacc2.c,v $
 * Revision 1.8  2004/04/24  madded2
 * DskOpen changed to handle 64K buffer bounds cross to avoid regression
 *   in DosDevIOCtl() when buffers crosses 64K boundary
 * DskRead changed to read >63 sectors per track for huge disks > 502 Gb
 * Added 64-bit file API support for DVD disks in CDRead() and CDInit()
 *
 * Revision 1.7  1998/07/05 07:44:17  rommel
 * added buffer allocation functions
 *
 * Revision 1.6  1998/01/05 18:03:18  rommel
 * use different method to determine number of sectors
 * because IOCTL seems to be wrong
 *
 * Revision 1.5  1997/03/01 20:21:35  rommel
 * fixed CDOpen bugs
 *
 * Revision 1.4  1997/02/09 15:05:57  rommel
 * added a few comments
 *
 * Revision 1.3  1997/01/12 21:15:26  rommel
 * added CD-ROM routines
 *
 * Revision 1.2  1995/12/31 21:50:58  rommel
 * added physical/logical mode parameter
 *
 * Revision 1.1  1994/07/08 21:34:12  rommel
 * Initial revision
 * 
 */

#define INCL_DOSDEVICES
#define INCL_DOSDEVIOCTL
#define INCL_DOSMODULEMGR
#define INCL_NOPM
#include <os2.h>

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "diskacc.h"

#define PHYSICAL     0x1000
#define CDROM        0x2000

#define CATEGORY(x)  (((x) & CDROM) ? IOCTL_CDROMDISK : ((x) & PHYSICAL) ? IOCTL_PHYSICALDISK : IOCTL_DISK)
#define HANDLE(x)    ((x) & ~(PHYSICAL|CDROM))

#pragma pack(1)

/* memory allocation for disk buffers */

// max sectors to read in one DosDevIOCtl call
#define DSK_MAX_READ_SECTORS (127)

// currently only supported sector size
#define DSK_DEFAULT_SECTOR_SIZE (512)

// max avail buffer space for OS/2 IOCtl
#define DSK_MAX_BUFFER_LENGTH (DSK_MAX_READ_SECTORS * DSK_DEFAULT_SECTOR_SIZE)

// returns TRUE if memory block crosses 64K boundary
#define CHECK_64K(_p,_len) (((((unsigned long)_p) & 0x0000ffff) + _len) >= 0x00010000)

// corrects address if needed
#define CORRECT_64K(_p,_len) ((CHECK_64K(_p,_len)) ? ((void *)((((unsigned long)_p) & 0xffff0000) + 0x00010000)) : _p)

void *DskAlloc (unsigned sectors, unsigned bytespersector)
{
    unsigned nBytes = (sectors * bytespersector);
    void *pBuf;

    if (bytespersector != 512)
    {
        // XXX: normal alloc for CD-ROMs
        return (malloc (nBytes));
    }

    if (nBytes > DSK_MAX_BUFFER_LENGTH) nBytes = DSK_MAX_BUFFER_LENGTH;

    pBuf = malloc (nBytes);
    if (CHECK_64K(pBuf, nBytes))
    {
        // object crossess 64k boundary, reallocate new buf
        free (pBuf);
        pBuf = malloc (nBytes * 2);
    }

    return (pBuf);
}

void DskFree (void *ptr)
{
    free (ptr);
}

/* logical/physical hard disk and floppy disk access code */

typedef struct
{
  BYTE   bCommand;
  USHORT usHead;
  USHORT usCylinder;
  USHORT usFirstSector;
  USHORT cSectors;
  struct
  {
    USHORT usSectorNumber;
    USHORT usSectorSize;
  }
  TrackTable[DSK_MAX_READ_SECTORS];
}
TRACK;

ULONG DosDevIOCtl32(PVOID pData, ULONG cbData, PVOID pParms, ULONG cbParms,
		    ULONG usFunction, HFILE hDevice)
{
  ULONG ulParmLengthInOut = cbParms, ulDataLengthInOut = cbData;
  return DosDevIOCtl(HANDLE(hDevice), CATEGORY(hDevice), usFunction,
		     pParms, cbParms, &ulParmLengthInOut, 
		     pData, cbData, &ulDataLengthInOut);
}

static int test_sector(int handle, int side, int track, int sector)
{
  char buffer[1024];
  TRACK trk;

  trk.bCommand      = 0;
  trk.usHead        = side;
  trk.usCylinder    = track;
  trk.usFirstSector = 0;
  trk.cSectors      = 1;

  trk.TrackTable[0].usSectorNumber = sector;
  trk.TrackTable[0].usSectorSize   = DSK_DEFAULT_SECTOR_SIZE;

  return DosDevIOCtl32(buffer, sizeof(buffer), &trk, sizeof(trk), 
		       DSK_READTRACK, handle) == 0;
}

int DskCount(void)
{
  USHORT nDisks;

  if (DosPhysicalDisk(INFO_COUNT_PARTITIONABLE_DISKS, &nDisks, sizeof(nDisks), 0, 0))
    return 0;

  return nDisks;
}

int DskOpen(char *drv, int logical, int lock, unsigned *sector,
	    unsigned *sides, unsigned *tracks, unsigned *sectors)
{
  BIOSPARAMETERBLOCK bpb;
  DEVICEPARAMETERBLOCK dpb;
  HFILE handle;
  USHORT physical;
  ULONG action;
  BYTE cmd = logical;

  if (isalpha(drv[0]) && drv[1] == ':' && drv[2] == 0)
  {
    if (DosOpen(drv, &handle, &action, 0, FILE_NORMAL, FILE_OPEN,
		OPEN_FLAGS_DASD | OPEN_FLAGS_FAIL_ON_ERROR |
		OPEN_ACCESS_READWRITE | OPEN_SHARE_DENYREADWRITE, 0))
      return -1;
  }
  else if (drv[0] == '$' && isdigit(drv[1]) && drv[2] == ':' && drv[3] == 0)
  {
    if (DosPhysicalDisk(INFO_GETIOCTLHANDLE, &physical, sizeof(physical), 
			drv + 1, strlen(drv + 1) + 1))
      return -1;
    handle = physical | PHYSICAL;
  }
  else
    return -1;

  if (handle & PHYSICAL)
  {
    if (DosDevIOCtl32(&dpb, sizeof(dpb), &cmd, sizeof(cmd), 
		      DSK_GETDEVICEPARAMS, handle))
    {
      DosPhysicalDisk(INFO_FREEIOCTLHANDLE, NULL, 0, &physical, sizeof(physical));
      return -1;
    }

    *sectors = dpb.cSectorsPerTrack;
    *tracks  = dpb.cCylinders;
    *sides   = dpb.cHeads;
    *sector  = DSK_DEFAULT_SECTOR_SIZE;
  }
  else
  {
    if (DosDevIOCtl32(&bpb, sizeof(bpb), &cmd, sizeof(cmd), 
		      DSK_GETDEVICEPARAMS, handle))
    {
      DosClose(handle);
      return -1;
    }

    *sectors = bpb.usSectorsPerTrack;
    *tracks  = bpb.cCylinders;
    *sides   = bpb.cHeads;
    *sector  = bpb.usBytesPerSector;
  }


  if (lock && DosDevIOCtl32(0, 0, &cmd, sizeof(cmd), DSK_LOCKDRIVE, handle))
  {
    if (handle & PHYSICAL)
      DosPhysicalDisk(INFO_FREEIOCTLHANDLE, NULL, 0, &physical, sizeof(physical));
    else
      DosClose(handle);
    return -1;
  }

  if (*sectors >= 15) /* 360k floppies ... */
    if (!test_sector(handle, 0, 0, 15))
    {
      if (*sectors == 15)
        *tracks = 40;

      *sectors = 9;
    }

  return handle;
}

int DskClose(int handle)
{
  BYTE cmd = 0;
  USHORT physical = handle & ~PHYSICAL;

  DosDevIOCtl32(0, 0, &cmd, sizeof(cmd), DSK_UNLOCKDRIVE, handle);

  if (handle & PHYSICAL)
    return DosPhysicalDisk(INFO_FREEIOCTLHANDLE, NULL, 0, 
			   &physical, sizeof(physical));
  else
    return DosClose(handle);
}

/* old DskRead by rommel
int DskRead(int handle, unsigned side, unsigned  track,
            unsigned sector, unsigned nsects, void *buf)
{
  TRACK trk;
  unsigned cnt;

  trk.bCommand      = 0;
  trk.usHead        = side;
  trk.usCylinder    = track;
  trk.usFirstSector = 0;
  trk.cSectors      = nsects;

  for (cnt = 0; cnt < nsects; cnt++)
  {
    trk.TrackTable[cnt].usSectorNumber = sector + cnt;
    trk.TrackTable[cnt].usSectorSize   = DSK_DEFAULT_SECTOR_SIZE;
  }

  return DosDevIOCtl32(buf,
                       nsects * DSK_DEFAULT_SECTOR_SIZE,
                       &trk, sizeof(trk), 
                       DSK_READTRACK, handle);
} */

// modified DskRead by madded2.
int DskRead (int handle,
             unsigned side, unsigned  track, unsigned sector,
             unsigned nsects, void *buf)
{
    int rc;
    TRACK trk;
    unsigned to_read = nsects;
    unsigned cnt;

    while (nsects > 0)
    {
        to_read = nsects;
        if (to_read > DSK_MAX_READ_SECTORS) to_read = DSK_MAX_READ_SECTORS;

        buf = CORRECT_64K(buf, to_read * DSK_DEFAULT_SECTOR_SIZE);

        trk.bCommand      = 0;
        trk.usHead        = side;
        trk.usCylinder    = track;
        trk.usFirstSector = 0;
        trk.cSectors      = to_read;

        for (cnt = 0; cnt < to_read; cnt++)
        {
            trk.TrackTable[cnt].usSectorNumber = sector + cnt;
            trk.TrackTable[cnt].usSectorSize   = DSK_DEFAULT_SECTOR_SIZE;
        }

        rc = DosDevIOCtl32 (buf,
                            to_read * DSK_DEFAULT_SECTOR_SIZE,
                            &trk, sizeof(trk), 
                            DSK_READTRACK, handle);
        if (rc != 0) break;

        nsects -= to_read;
        sector += to_read;
    }

    return (rc);
}

int DskWrite(int handle, unsigned side, unsigned  track,
             unsigned sector, unsigned nsects, void *buf)
{
  TRACK trk;
  unsigned cnt;

  trk.bCommand      = 0;
  trk.usHead        = side;
  trk.usCylinder    = track;
  trk.usFirstSector = 0;
  trk.cSectors      = nsects;

  for (cnt = 0; cnt < nsects; cnt++)
  {
    trk.TrackTable[cnt].usSectorNumber = sector + cnt;
    trk.TrackTable[cnt].usSectorSize   = DSK_DEFAULT_SECTOR_SIZE;
  }

  return DosDevIOCtl32(buf, nsects * DSK_DEFAULT_SECTOR_SIZE, &trk, sizeof(trk), 
                       DSK_WRITETRACK, handle);
}

/* CD-ROM access code */

// 2Gb+ file API support for DVD access (c) 2004 by madded2.

typedef APIRET (APIENTRY *PDOSSETFILEPTRL)(HFILE hFile,
                                           __int64 ib,
                                           ULONG method,
                                           __int64 *pibActual);

#define ORD_DOS32SETFILEPTRL (988)

// Dos32SetFilePtrL API entry point address
PDOSSETFILEPTRL pDosSetFilePtrL = NULL;

// init CD-ROM support - 64-bit file API
void CDInit (void)
{
  HMODULE hmod;

  if (DosLoadModule (NULL,
                     0,
                     "DOSCALLS",
                     &hmod)) return;

  DosQueryProcAddr (hmod,
                    ORD_DOS32SETFILEPTRL,
                    NULL,
                    (PFN *)&pDosSetFilePtrL);
}

static struct
{
  char sig[4];
  char cmd;
}
cdparams;

int CDFind(int number)
{
  int i;
  HFILE handle;
  ULONG action, status, datasize;
  APIRET rc;
  struct 
  {
    USHORT count;
    USHORT first;
  } 
  cdinfo;

  if (DosOpen("\\DEV\\CD-ROM2$", &handle, &action, 0, FILE_NORMAL,
              OPEN_ACTION_OPEN_IF_EXISTS,
              OPEN_SHARE_DENYNONE | OPEN_ACCESS_READONLY, NULL))
    return -1;

  datasize = sizeof(cdinfo);
  rc = DosDevIOCtl(handle, 0x82, 0x60, NULL, 0, NULL,
		  (PVOID)&cdinfo, sizeof(cdinfo), &datasize);

  DosClose(handle);

  if (rc)
    return DosClose(handle), -1;

  return number == 0 ? cdinfo.count : 'A' + cdinfo.first + number - 1;
}

int CDOpen(char *drv, int lock, char *upc, unsigned *sectors)
{
  HFILE handle;
  ULONG action, lockrc;
  char upcdata[10];
  FSALLOCATE fa;

  if (isalpha(drv[0]) && drv[1] == ':' && drv[2] == 0)
  {
    if (DosOpen(drv, &handle, &action, 0, FILE_NORMAL, FILE_OPEN,
		OPEN_FLAGS_DASD | OPEN_FLAGS_FAIL_ON_ERROR |
		OPEN_ACCESS_READONLY | OPEN_SHARE_DENYREADWRITE, 0))
      return -1;
  }
  else
    return -1;

  handle |= CDROM;
  memcpy(cdparams.sig, "CD01", 4);

  cdparams.cmd = 1;
  lockrc = DosDevIOCtl32(0, 0, &cdparams, sizeof(cdparams),
	  	         CDROMDISK_LOCKUNLOCKDOOR, handle);

  if (lock && lockrc != 0 && lockrc != 0xff13)
  {
    DosClose(HANDLE(handle));
    return -1;
  }

  if (DosQueryFSInfo(drv[0] - '@', FSIL_ALLOC, &fa, sizeof(fa)))
  {
    cdparams.cmd = 0;
    DosDevIOCtl32(0, 0, &cdparams, sizeof(cdparams.sig), 
  		  CDROMDISK_LOCKUNLOCKDOOR, handle);
    DosClose(HANDLE(handle));
    return -1;
  }
  else
    *sectors = fa.cUnit;

  memset(upcdata, 0, sizeof(upcdata));
  if (DosDevIOCtl32(upcdata, sizeof(upcdata), &cdparams, sizeof(cdparams.sig),
		    CDROMDISK_GETUPC, handle))
  {
    /* ignore possible errors but ... */
    *upc = 0;
  }
  else
  {
    memcpy(upc, upcdata + 1, 7);
    upc[7] = 0;
  }

  return handle;
}

int CDClose(int handle)
{
  cdparams.cmd = 0;
  DosDevIOCtl32(0, 0, &cdparams, sizeof(cdparams.sig), 
		CDROMDISK_LOCKUNLOCKDOOR, handle);

  return DosClose(HANDLE(handle));
}

int CDRead(int handle, unsigned sector, unsigned nsects, void *buf)
{
  ULONG nActual;
  __int64 nActual2;

  // note: 0x7fffffff/2048 = 1048575 - last possible sector number for
  // DosSetFilePtr() 32-bit API call

  if (sector <= 1048575)
  {
      // always use 32-bit file API if possible

      if (DosSetFilePtr (HANDLE(handle),
                         sector * 2048,
                         FILE_BEGIN,
                         &nActual)) return (-1);
  } else
  {
      // use 64-bit file API for 2Gb+ file access

      if (pDosSetFilePtrL != NULL)
      {
          nActual2 = sector;
          nActual2 *= 2048;

          if (pDosSetFilePtrL (HANDLE(handle),
                               nActual2,
                               FILE_BEGIN,
                               &nActual2)) return (-1);
      } else
      {
          // 64-bit file API not supported
          return (-1);
      }
  }

  if (DosRead(HANDLE(handle), buf, nsects * 2048, &nActual))
    return (-1);

  return (nActual / 2048);
}

/* end of diskacc2.c */
