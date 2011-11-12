# Makefile
# Copyright (C) 2003 by Salvatore Sanfilippo
# all rights reserved

# see the LICENSE file for COPYRIGHT and PERMISSION notice.

.SUFFIXES:
.SUFFIXES: .c .o

CC=gcc
LD=ld
CFLAGS= -fPIC -Wall -O2 -g
SHAREDFLAGS= -nostartfiles -shared -Wl,-soname,libtclsbignum.so.0
INCLUDES= -I/usr/include/tcl8.4
DEFS=

RANLIB=/usr/bin/ranlib
AR=/usr/bin/ar
SHELL= /bin/sh

INSTALL= /usr/bin/install
INSTALL_PROGRAM= $(INSTALL) -m 755
INSTALL_DATA= $(INSTALL) -m 644

LIBPATH=/usr/local/lib
BINPATH=/usr/local/bin
COMPILE_TIME=

all: .depend tclsbignum.so

.depend:
	@$(CC) $(INCLUDES) -MM *.c > .depend
	@echo Making dependences

.c.o:
	$(CC) $(INCLUDES) $(CFLAGS) $(DEFS) -c $< -o $@

tclsbignum.so: tclsbignum.o sbignum.o sbignum-tables.o
	rm -f tclsbignum.so
	$(LD) -G -z text -o tclsbignum.so tclsbignum.o sbignum.o sbignum-tables.o -ldl -lm -lc

clean:
	rm -f *.o tclsbignum.so .depend

ifeq (.depend,$(wildcard .depend))
include .depend
endif
