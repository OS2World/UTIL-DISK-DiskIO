/* diskacc.h - direct disk access library 
 *
 * Author:  Kai Uwe Rommel <rommel@ars.de>
 * Created: Fri Jul 08 1994
 */

/* $Id: diskacc.h,v 1.4 1998/07/05 07:44:17 rommel Exp rommel $ */

/*
 * $Log: diskacc.h,v $
 * Revision 1.4  1998/07/05 07:44:17  rommel
 * added Win32 version
 * added buffer allocation functions
 *
 * Revision 1.3  1997/01/12 21:15:44  rommel
 * added CD-ROM routines
 *
 * Revision 1.2  1994/07/08 21:35:50  rommel
 * bug fix
 *
 * Revision 1.1  1994/07/08 21:34:12  rommel
 * Initial revision
 * 
 */

void *DskAlloc(unsigned sectors, unsigned bytespersector);
void DskFree(void *ptr);

int DskCount(void);
int DskOpen(char *drv, int logical, int lock, unsigned *sector,
	    unsigned *sides, unsigned *tracks, unsigned *sectors);
int DskClose(int handle);
int DskRead(int handle, unsigned side, unsigned  track,
	    unsigned sector, unsigned nsects, void *buf);
int DskWrite(int handle, unsigned side, unsigned  track,
	     unsigned sector, unsigned nsects, void *buf);

int CDFind(int number);
int CDOpen(char *drv, int lock, char *upc, unsigned *sectors);
int CDClose(int handle);
int CDRead(int handle, unsigned sector, unsigned nsects, void *buf);

#ifdef OS2
void CDInit (void);
#endif

/* end of diskacc.h */
