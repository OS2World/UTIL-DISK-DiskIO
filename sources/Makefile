# IBM VAC compiler makefile.
# look at Watcom.mak for Watcom compiler.
#===================================================================
# Makefile for diskio - disk benchmark
# Copyright (C) 1994-1995 Kai Uwe Rommel <rommel@ars.muc.de>
#
# $Id: Makefile,v 1.5 1998/07/05 07:44:17 rommel Exp rommel $
#
# $Log: Makefile,v $
# Revision 1.5  1998/07/05 07:44:17  rommel
# added Windows NT version
#
# Revision 1.4  1997/02/09 15:07:09  rommel
# *** empty log message ***
#
# Revision 1.3  1995/12/31 20:36:04  rommel
# added dhrystone
#
# Revision 1.2  1994/08/07 17:29:53  rommel
# added dependencies
#
# Revision 1.1  1994/07/08 21:55:24  rommel
# Initial revision
#

os2:
	$(MAKE) all CFLAGS="-DOS2TMR -DOS2" LIBS="" P=2
win32:
	$(MAKE) all CFLAGS="-DOS2TMR -DWIN32" LIBS="user32.lib" P=3

CC = icc -q -Wpro -O -qtune=pentium2 -Gl -Gm -Gs
O = .obj
OUT = -Fe

LFLAGS = -B/ST:0x50000 -B/E:2

OBJSD = diskio$O diskacc$P$O dhry_1$O dhry_2$O cpuperf$O
OBJSH = dhry_1$O dhry_2$O dhry_tmr$O

.SUFFIXES: .c $O
.c$O:
	$(CC) $(CFLAGS) -c $<

all: diskio.exe dhry.exe

diskio.exe: $(OBJSD)
	$(CC) $(OUT) $@ $(OBJSD) $(LFLAGS) $(LIBS)

dhry.exe: $(OBJSH)
	$(CC) $(OUT) $@ $(OBJSH) $(LFLAGS) $(LIBS)

diskio$O diskacc$P$O: diskacc.h
dhry_1$O dhry_2$O: dhry.h

# end of Makefile
