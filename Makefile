# makefile for ate, updated Thu Nov 27 07:55:39 AM EST 2025

PROGRAM=ate

SRC=bind.c buffer.c file.c globals.c help.c main.c posix.c search.c tree.c util.c version.c

OBJ=bind.o buffer.o file.o globals.o help.o main.o posix.o search.o tree.o util.o version.o

HDR=ebind.h edef.h efunc.h estruct.h util.h version.h

# DO NOT ADD OR MODIFY ANY LINES ABOVE THIS -- make source creates them

CC=gcc
WARNINGS=-Wall -Wstrict-prototypes
CFLAGS=-O2 $(WARNINGS) -g
DEFINES=-DAUTOCONF -DPOSIX -DUSG -D_XOPEN_SOURCE=600 -D_GNU_SOURCE
LIBS=-lcurses
BINDIR=/usr/local/bin

$(PROGRAM): $(OBJ)
	@echo "  LINK    " $@
	$(CC) $(LDFLAGS) $(DEFINES) -o $@ $(OBJ) $(LIBS)

SPARSE=sparse
SPARSE_FLAGS=-D__LITTLE_ENDIAN__ -D__x86_64__ -D__linux__ -D__unix__

sparse:
	$(SPARSE) $(SPARSE_FLAGS) $(DEFINES) $(SRC)

clean:
	@echo "  CLEAN"
	rm -f $(PROGRAM) lintout tags Makefile.bak *.o *~

install: $(PROGRAM)
	cp $(PROGRAM) ${BINDIR}/
	chmod 755 ${BINDIR}/$(PROGRAM)

lint: $(SRC)
	@rm -f lintout
	splint $(DEFINES) $(SRC) >lintout
	cat lintout

tags: $(SRC)
	rm -f tags
	ctags $(SRC)

format:
	@indent -kr -i8 -ts8 -sob -l80 -lc80 -nut $(SRC) $(HDR)

source:
	@mv Makefile Makefile.bak
	@echo "# makefile for ate, updated `date`" >Makefile
	@echo '' >>Makefile
	@echo PROGRAM=ate  >>Makefile
	@echo SRC=`ls *.c` >>Makefile
	@echo OBJ=`ls *.c | sed s/c$$/o/` >>Makefile
	@echo HDR=`ls *.h` >>Makefile
	@echo '' >>Makefile
	@sed -n -e '/^# DO NOT ADD OR MODIFY/,$$p' <Makefile.bak >>Makefile

.c.o:
	@echo "  CC      " $@
	$(CC) $(CFLAGS) $(DEFINES) -c $*.c

run: $(PROGRAM)
	./$(PROGRAM)