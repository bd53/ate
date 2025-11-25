# makefile for ate, updated Tue Nov 25 07:56:29 AM EST 2025

# Make the build silent by default
V =

ifeq ($(strip $(V)),)
	E = @echo
	Q = @
else
	E = @\#
	Q =
endif
export E Q

uname_S := $(shell sh -c 'uname -s 2>/dev/null || echo not')

PROGRAM=ate

SRC=bind.c buffer.c display.c eval.c file.c input.c main.c posix.c search.c tree.c utf8.c util.c version.c

OBJ=bind.o buffer.o display.o eval.o file.o input.o main.o posix.o search.o tree.o utf8.o util.o version.o

HDR=ebind.h edef.h efunc.h estruct.h utf8.h util.h version.h

# DO NOT ADD OR MODIFY ANY LINES ABOVE THIS -- make source creates them

CC=gcc
WARNINGS=-Wall -Wstrict-prototypes
CFLAGS=-O2 $(WARNINGS) -g
ifeq ($(uname_S),Linux)
 DEFINES=-DAUTOCONF -DPOSIX -DUSG -D_XOPEN_SOURCE=600 -D_GNU_SOURCE
endif
ifeq ($(uname_S),FreeBSD)
 DEFINES=-DAUTOCONF -DPOSIX -DSYSV -D_FREEBSD_C_SOURCE -D_BSD_SOURCE -D_SVID_SOURCE -D_XOPEN_SOURCE=600
endif
ifeq ($(uname_S),Darwin)
 DEFINES=-DAUTOCONF -DPOSIX -DSYSV -D_DARWIN_C_SOURCE -D_BSD_SOURCE -D_SVID_SOURCE -D_XOPEN_SOURCE=600
endif
#DEFINES=-DAUTOCONF
LIBS=-lncurses
BINDIR=/usr/local/bin
LIBDIR=/usr/local/lib

$(PROGRAM): $(OBJ)
	$(E) "  LINK    " $@
	$(Q) $(CC) $(LDFLAGS) $(DEFINES) -o $@ $(OBJ) $(LIBS)

SPARSE=sparse
SPARSE_FLAGS=-D__LITTLE_ENDIAN__ -D__x86_64__ -D__linux__ -D__unix__

sparse:
	$(SPARSE) $(SPARSE_FLAGS) $(DEFINES) $(SRC)

clean:
	$(E) "  CLEAN"
	$(Q) rm -f $(PROGRAM) tags Makefile.bak *.o

install: $(PROGRAM)
	$(Q) install -d $(BINDIR)
	$(Q) install -d $(LIBDIR)
	$(Q) install -m 755 $(PROGRAM) $(BINDIR)/$(PROGRAM)
	$(Q) install -m 644 help.txt $(LIBDIR)/help.txt

tags: $(SRC)
	$(Q) rm -f tags
	$(Q) ctags $(SRC)

source:
	@mv Makefile Makefile.bak
	@echo "# makefile for ate, updated `date`" >Makefile
	@echo '' >>Makefile
	@sed -n -e '2,/^SRC=/p' <Makefile.bak | sed '1d' | sed '$$ d' >>Makefile
	@echo SRC=`ls *.c` >>Makefile
	@echo '' >>Makefile
	@echo OBJ=`ls *.c | sed s/c$$/o/` >>Makefile
	@echo '' >>Makefile
	@echo HDR=`ls *.h` >>Makefile
	@sed -n -e '/^HDR=/,$$p' <Makefile.bak | sed '1d' >>Makefile

.c.o:
	$(E) "  CC      " $@
	$(Q) $(CC) $(CFLAGS) $(DEFINES) -c $*.c

run: $(PROGRAM)
	$(Q) ./$(PROGRAM)