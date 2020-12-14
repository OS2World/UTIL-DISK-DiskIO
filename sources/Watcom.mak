# Watcom C compiler makefile, OS/2 target.
# look at Makefile for VAC compiler.
#===================================================================
#
#   Setup the environment properly
#
#===================================================================

INC_C	= .\;

SOURCE_PATH = .\;

#===================================================================
#
#   Auto-dependency information
#
#===================================================================
.ERASE
.SUFFIXES:
.SUFFIXES: .lst .obj .c .lib .def

CFLAGS	 = -bm -bt=os2v2 -e60 -5r -i$(INC_C) -oabhiklmnrt -s -w5 -ze -zdp -zq -zfp -zgp -zp1 -dOS2TMR -dOS2
CC	 = WCC386 $(CFLAGS)

LFLAGS	= form os2 lx option int, dosseg, map, eliminate, mang, tog sort global
LINK	= WLINK $(LFLAGS)

.c: $(SOURCE_PATH)
.c.obj: .AUTODEPEND
	$(CC) -fo$@ $*.c

#===================================================================
#
#   List of source files
#
#===================================================================
FILES	  = diskio.obj diskacc2.obj dhry_1.obj dhry_2.obj cpuperf.obj
TARGET	  = diskio

#===================================================================
#
#   Specific dependencies
#
#===================================================================
all: $(TARGET).exe

$(TARGET).def: makefile
    @%write $^@ name $(TARGET).exe
    @for %f in ($(FILES)) do @%append $^@ file %f

$(TARGET).exe: $(TARGET).def $(FILES)
     $(LINK) @$(TARGET).def
#     wmap $(TARGET) $(TARGET).map $(TARGET)1.map
#     copy $(TARGET).map $(TARGET).wat
#     del $(TARGET).map
#     move $(TARGET)1.map $(TARGET).map
#     mapsym $(TARGET).map

