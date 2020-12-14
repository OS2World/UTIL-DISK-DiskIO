# Makefile for DHRY
#
# Created: Fri Dec 15 1995
#
# $Id$
# $Revision$
#
# $Log$

os2:
	$(MAKE) -f dhry.mak all CFLAGS="-DOS2TMR -DOS2"
win32:
	$(MAKE) -f dhry.mak all CFLAGS="-DOS2TMR -DWIN32"

CC = icc -q -O -Gl -Gs -Wpro-
O = .obj
OUT = -Fe

LFLAGS = -B/ST:0x100000

OBJS = dhry_1$O dhry_2$O dhry_tmr$O

.SUFFIXES: .c $O

.c$O:
	$(CC) -c $(CFLAGS) $<

all: dhry.exe

dhry.exe: $(OBJS)
	$(CC) $(OUT) $@ $(OBJS) $(LFLAGS)
