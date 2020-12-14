DISKIO - Fixed Disk Benchmark, Version 1.18z
(C) 1994-1998 Kai Uwe Rommel
(C) 2004 madded2

changes.
--------

version 1.18z (2004-04-24)

 - General bug fixes and improvements;

 - [OS2] improved physical disk access functions, now working ok with
   large hard disks (> 502 Gb);

 - [OS2] CPU load measuring mechanism changed to native OS/2 DosPerfSysCall()
   API, SMP-aware, may be incompatible with oldest Warp3 systems or
   pre-Pentium CPUs;

 - [OS2] now CPU usage, multithreaded disk I/O and concurrent disks I/O
   measurings working correctly (surprise!);

 - [OS2] DVD disks support added in CD-ROM benchmarks;

 - [OS2] due to problems in DosTmrQueryTime() API call in OS/2 SMP,
   32-bit MS counter is used as timer info when running on SMP system.

notes.
------

This tiny system utility is the beautiful example of unique linux-way
programming technique (thanks to original author). Please, never use its
sources as examples.
However, i haven't enough motivation to rewrite it from the ground, so i did
only major bugfixes/improvements in some parts of code.

The only new binary is binos2\diskio.exe.
All other binaries are included from 1.17 version.

Enjoy.

Send bug reports please.
mailto: "Stepan Kazakov" <madded@mitm.ru>
