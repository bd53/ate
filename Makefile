# makefile for ate, updated Tue Nov 25 17:59:07 UTC 2025

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

SRC=$(shell find . -name '*.c' ! -name 'test.c')

OBJ=$(SRC:.c=.o)

CC=gcc
WARNINGS=-Wall -Wstrict-prototypes
CFLAGS=-O2 $(WARNINGS) -g
ifeq ($(uname_S),Linux)
	DEFINES := -DAUTOCONF -DPOSIX -DUSG -D_XOPEN_SOURCE=600 -D_GNU_SOURCE
endif
ifeq ($(uname_S),FreeBSD)
	DEFINES := -DAUTOCONF -DPOSIX -DSYSV -D_FREEBSD_C_SOURCE -D_BSD_SOURCE -D_SVID_SOURCE -D_XOPEN_SOURCE=600
endif
ifeq ($(uname_S),Darwin)
	DEFINES := -DAUTOCONF -DPOSIX -DSYSV -D_DARWIN_C_SOURCE -D_BSD_SOURCE -D_SVID_SOURCE -D_XOPEN_SOURCE=600
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
	$(Q) rm -f $(PROGRAM) tags
	$(Q) find . -name '*.o' -type f -delete

install: $(PROGRAM)
	$(Q) install -d $(BINDIR)
	$(Q) install -d $(LIBDIR)
	$(Q) install -m 755 $(PROGRAM) $(BINDIR)/$(PROGRAM)
	$(Q) install -m 644 help.txt $(LIBDIR)/help.txt

tags: $(SRC)
	$(Q) rm -f tags
	$(Q) ctags $(SRC)

.c.o:
	$(E) "  CC      " $@
	$(Q) $(CC) $(CFLAGS) $(DEFINES) -c $*.c

run: $(PROGRAM)
	$(Q) ./$(PROGRAM)