/* diskacc3.c - direct disk access library for Win32.
 *
 * Author:  Kai Uwe Rommel <rommel@ars.de>
 * Created: Fri Jul 08 1994
 */

static char *rcsid =
"$Id: diskacc3.c,v 1.1 1998/07/05 07:44:17 rommel Exp rommel $";
static char *rcsrev = "$Revision: 1.1 $";

/*
 * $Log: diskacc3.c,v $
 * Revision 1.1  1998/07/05 07:44:17  rommel
 * Initial revision
 *
 */

#define _INTEGRAL_MAX_BITS 64
#include <windows.h>
#include <winioctl.h>

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "diskacc.h"

/* memory allocation for disk buffers */

void *DskAlloc(unsigned sectors, unsigned bytespersector)
{
  return VirtualAlloc(NULL, sectors * bytespersector, MEM_COMMIT, PAGE_READWRITE);
}

void DskFree(void *ptr)
{
  VirtualFree(ptr, 0, MEM_RELEASE);
}

/* logical/physical hard disk and floppy disk access code */

int DskCount(void)
{
  char device[1024];
  int devices = 0;
  HANDLE file;

  strcpy(device, "\\\\.\\PhysicalDrive0");

  for (;;)
  {
    device[17] = '0' + devices;
    file = CreateFile(device, 0, FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);

    if (file == INVALID_HANDLE_VALUE)
      break;

    CloseHandle(file);
    devices++;
  }

  return devices;
}

int DskOpen(char *drv, int logical, int lock, unsigned *sector,
	    unsigned *sides, unsigned *tracks, unsigned *sectors)
{
  char device[1024];
  HANDLE handle;
  DISK_GEOMETRY geo;
  DWORD bytes;

  if (isalpha(drv[0]) && drv[1] == ':' && drv[2] == 0)
  {
    strcpy(device, "\\\\.\\A:");
    device[4] = drv[0];

    handle = CreateFile(device, GENERIC_READ|GENERIC_WRITE, 
			FILE_SHARE_READ|FILE_SHARE_WRITE, NULL,
			OPEN_EXISTING, FILE_FLAG_NO_BUFFERING, NULL);

    if (handle == INVALID_HANDLE_VALUE)
      return -1;
  }
  else if (drv[0] == '$' && isdigit(drv[1]) && drv[2] == ':' && drv[3] == 0)
  {
    strcpy(device, "\\\\.\\PhysicalDrive0");
    device[17] = drv[1] - 1;

    handle = CreateFile(device, GENERIC_READ|GENERIC_WRITE, 
			FILE_SHARE_READ|FILE_SHARE_WRITE, NULL,
			OPEN_EXISTING, FILE_FLAG_NO_BUFFERING, NULL);

    if (handle == INVALID_HANDLE_VALUE)
      return -1;
  }
  else
    return -1;

  /* do locking here, when implemented */

  if (!DeviceIoControl(handle, IOCTL_DISK_GET_DRIVE_GEOMETRY, 
		       NULL, 0, &geo, sizeof(geo), &bytes, NULL))
    return CloseHandle(handle), -1;

  *sectors = geo.SectorsPerTrack;
  *tracks = geo.Cylinders.LowPart;
  *sides = geo.TracksPerCylinder;
  *sector = geo.BytesPerSector;

  if (geo.Cylinders.HighPart != 0)
  {
    printf("Error: more than 2^32 tracks not supported\n");
    CloseHandle(handle);
    return -1;
  }

  return (int) handle;
}

int DskClose(int handle)
{
  return !CloseHandle((HANDLE) handle);
}

int DskRead(int handle, unsigned side, unsigned track,
            unsigned sector, unsigned nsects, void *buf)
{
  DISK_GEOMETRY geo;
  LARGE_INTEGER pos;
  DWORD bytes;

  if (!DeviceIoControl((HANDLE) handle, IOCTL_DISK_GET_DRIVE_GEOMETRY, 
		       NULL, 0, &geo, sizeof(geo), &bytes, NULL))
    return -1;

  pos.QuadPart = ((track * geo.TracksPerCylinder + side) 
		  * geo.SectorsPerTrack + sector) * geo.BytesPerSector;

  if (SetFilePointer((HANDLE) handle, pos.LowPart, &pos.HighPart, 
		     FILE_BEGIN) == 0xFFFFFFFF)
    return -1;
  
  if (!ReadFile((HANDLE) handle, buf, nsects * geo.BytesPerSector, &bytes, NULL))
    return -1;

  return bytes != nsects * geo.BytesPerSector;
}

int DskWrite(int handle, unsigned side, unsigned  track,
             unsigned sector, unsigned nsects, void *buf)
{
  DISK_GEOMETRY geo;
  LARGE_INTEGER pos;
  DWORD bytes;

  if (!DeviceIoControl((HANDLE) handle, IOCTL_DISK_GET_DRIVE_GEOMETRY, 
		       NULL, 0, &geo, sizeof(geo), &bytes, NULL))
    return -1;

  pos.QuadPart = ((track * geo.TracksPerCylinder + side) 
		  * geo.SectorsPerTrack + sector) * geo.BytesPerSector;

  if (SetFilePointer((HANDLE) handle, pos.LowPart, &pos.HighPart, 
		     FILE_BEGIN) == 0xFFFFFFFF)
    return -1;
  
  if (!WriteFile((HANDLE) handle, buf, nsects * geo.BytesPerSector, &bytes, NULL))
    return -1;

  return bytes != nsects * geo.BytesPerSector;
}

/* CD-ROM access code */

int CDFind(int number)
{
  char buffer[5120], *ptr;
  int devices = 0;

  if (GetLogicalDriveStrings(sizeof(buffer), buffer) == 0)
    return -1;

  for (ptr = buffer; *ptr; ptr += strlen(ptr) + 1)
    if (GetDriveType(ptr) == DRIVE_CDROM)
    {
      devices++;

      if (devices == number)
	return *ptr;
    }

  return devices;
}

int CDOpen(char *drv, int lock, char *upc, unsigned *sectors)
{
  char device[1024];
  HANDLE handle;
  DWORD Cluster, Sector, FreeClusters, TotalClusters;

  strcpy(device, "A:\\");
  device[0] = drv[0];

  if (!GetDiskFreeSpace(device, &Cluster, &Sector, &FreeClusters, &TotalClusters))
    return -1;

  *sectors = TotalClusters * Cluster;

  strcpy(device, "\\\\.\\A:");
  device[4] = drv[0];

  handle = CreateFile(device, GENERIC_READ, 
		      FILE_SHARE_READ|FILE_SHARE_WRITE, NULL,
		      OPEN_EXISTING, FILE_FLAG_NO_BUFFERING, NULL);

  if (handle == INVALID_HANDLE_VALUE)
    return -1;

  return (int) handle;
}

int CDClose(int handle)
{
  return !CloseHandle((HANDLE) handle);
}

int CDRead(int handle, unsigned sector, unsigned nsects, void *buf)
{
  LARGE_INTEGER pos;
  DWORD bytes;

  pos.QuadPart = sector * 2048;

  if (SetFilePointer((HANDLE) handle, pos.LowPart, &pos.HighPart, 
		     FILE_BEGIN) == 0xFFFFFFFF)
    return -1;
  
  if (!ReadFile((HANDLE) handle, buf, nsects * 2048, &bytes, NULL))
    return -1;

  return bytes / 2048;
}

/* end of diskacc3.c */
