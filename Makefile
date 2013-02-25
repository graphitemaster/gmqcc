DESTDIR :=
PREFIX  := /usr/local
BINDIR  := $(PREFIX)/bin
DATADIR := $(PREFIX)/share
MANDIR  := $(DATADIR)/man

UNAME  ?= $(shell uname)
CYGWIN  = $(findstring CYGWIN,  $(UNAME))
MINGW   = $(findstring MINGW32, $(UNAME))

CC     ?= clang
CFLAGS += -Wall -Wextra -Werror -I. -fno-strict-aliasing -fsigned-char
ifneq ($(shell git describe --always 2>/dev/null),)
    CFLAGS += -DGMQCC_GITINFO="\"$(shell git describe --always)\""
endif
#turn on tons of warnings if clang is present
# but also turn off the STUPID ONES
ifeq ($(CC), clang)
	CFLAGS +=                              \
		-Weverything                       \
		-Wno-padded                        \
		-Wno-format-nonliteral             \
		-Wno-disabled-macro-expansion      \
		-Wno-conversion                    \
		-Wno-missing-prototypes            \
		-Wno-float-equal                   \
		-Wno-cast-align                    \
		-Wno-missing-variable-declarations \
		-Wno-unknown-warning-option
else
	#Tiny C Compiler doesn't know what -pedantic-errors is
	# and instead of ignoring .. just errors.
	ifneq ($(CC), tcc)
		CFLAGS +=-pedantic-errors
	else
		CFLAGS += -Wno-pointer-sign -fno-common
	endif
endif

ifeq ($(track), no)
    CFLAGS += -DNOTRACK
endif

OBJ_D = util.o code.o ast.o ir.o conout.o ftepp.o opts.o fs.o utf8.o correct.o
OBJ_P = util.o fs.o conout.o opts.o pak.o
OBJ_T = test.o util.o conout.o fs.o
OBJ_C = main.o lexer.o parser.o fs.o
OBJ_X = exec-standalone.o util.o conout.o fs.o

ifneq ("$(CYGWIN)", "")
	#nullify the common variables that
	#most *nix systems have (for windows)
	PREFIX   :=
	BINDIR   :=
	DATADIR  :=
	MANDIR   :=
	QCVM      = qcvm.exe
	GMQCC     = gmqcc.exe
	TESTSUITE = testsuite.exe
	PAK       = pak.exe
else
ifneq ("$(MINGW)", "")
	#nullify the common variables that
	#most *nix systems have (for windows)
	PREFIX   :=
	BINDIR   :=
	DATADIR  :=
	MANDIR   :=
	QCVM      = qcvm.exe
	GMQCC     = gmqcc.exe
	TESTSUITE = testsuite.exe
	PAK       = pak.exe
else
	#arm support for linux .. we need to allow unaligned accesses
	#to memory otherwise we just segfault everywhere
	ifneq (, $(findstring arm, $(shell uname -m)))
		CFLAGS += -munaligned-access
	endif

	QCVM      = qcvm
	GMQCC     = gmqcc
	TESTSUITE = testsuite
	PAK       = pak
endif
endif

#splint flags
SPLINTFLAGS =            \
    -redef               \
    -noeffect            \
    -nullderef           \
    -usedef              \
    -type                \
    -mustfreeonly        \
    -nullstate           \
    -varuse              \
    -mustfreefresh       \
    -compdestroy         \
    -compmempass         \
    -nullpass            \
    -onlytrans           \
    -predboolint         \
    -boolops             \
    -exportlocal         \
    -incondefs           \
    -macroredef          \
    -retvalint           \
    -nullret             \
    -predboolothers      \
    -globstate           \
    -dependenttrans      \
    -branchstate         \
    -compdef             \
    -temptrans           \
    -usereleased         \
    -warnposix           \
    -shiftimplementation \
    +charindex           \
    -kepttrans           \
    -unqualifiedtrans    \
    +matchanyintegral    \
    -bufferoverflowhigh  \
    +voidabstract        \
    -nullassign          \
    -unrecog             \
    -casebreak           \
    -retvalbool          \
    -retvalother         \
    -mayaliasunique      \
    -realcompare         \
    -observertrans       \
    -shiftnegative       \
    -freshtrans          \
    -abstract            \
    -statictrans         \
    -castfcnptr

#standard rules
default: all
%.o: %.c
	$(CC) -c $< -o $@ $(CFLAGS)

exec-standalone.o: exec.c
	$(CC) -c $< -o $@ $(CFLAGS) -DQCVM_EXECUTOR=1

$(QCVM): $(OBJ_X)
	$(CC) -o $@ $^ $(CFLAGS) -lm

$(GMQCC): $(OBJ_C) $(OBJ_D)
	$(CC) -o $@ $^ $(CFLAGS)

$(TESTSUITE): $(OBJ_T)
	$(CC) -o $@ $^ $(CFLAGS) -lm

$(PAK): $(OBJ_P)
	$(CC) -o $@ $^ $(CFLAGS)

all: $(GMQCC) $(QCVM) $(TESTSUITE) $(PAK)

check: all
	@ ./$(TESTSUITE)
test: all
	@ ./$(TESTSUITE)

clean:
	rm -f *.o $(GMQCC) $(QCVM) $(TESTSUITE) $(PAK) *.dat

splint:
	@  splint $(SPLINTFLAGS) *.c *.h

depend:
	@makedepend    -Y -w 65536 2> /dev/null \
		$(subst .o,.c,$(OBJ_D))
	@makedepend -a -Y -w 65536 2> /dev/null \
		$(subst .o,.c,$(OBJ_T))
	@makedepend -a -Y -w 65536 2> /dev/null \
		$(subst .o,.c,$(OBJ_C))
	@makedepend -a -Y -w 65536 2> /dev/null \
		$(subst .o,.c,$(OBJ_X))
	@makedepend -a -Y -w 65536 2> /dev/null \
		$(subst .o,.c,$(OBJ_P))

#install rules
install: install-gmqcc install-qcvm install-doc
install-gmqcc: $(GMQCC)
	install -d -m755               $(DESTDIR)$(BINDIR)
	install    -m755  $(GMQCC)     $(DESTDIR)$(BINDIR)/gmqcc
install-qcvm: $(QCVM)
	install -d -m755               $(DESTDIR)$(BINDIR)
	install    -m755  $(QCVM)      $(DESTDIR)$(BINDIR)/qcvm
install-doc:
	install -d -m755               $(DESTDIR)$(MANDIR)/man1
	install    -m644  doc/gmqcc.1  $(DESTDIR)$(MANDIR)/man1/
	install    -m644  doc/qcvm.1   $(DESTDIR)$(MANDIR)/man1/

uninstall:
	rm $(DESTDIR)$(BINDIR)/gmqcc
	rm $(DESTDIR)$(BINDIR)/qcvm
	rm $(DESTDIR)$(MANDIR)/man1/doc/gmqcc.1
	rm $(DESTDIR)$(MANDIR)/man1/doc/qcvm.1

# DO NOT DELETE

util.o: gmqcc.h opts.def
code.o: gmqcc.h opts.def
ast.o: gmqcc.h opts.def ast.h ir.h
ir.o: gmqcc.h opts.def ir.h
conout.o: gmqcc.h opts.def
ftepp.o: gmqcc.h opts.def lexer.h
opts.o: gmqcc.h opts.def
fs.o: gmqcc.h opts.def
utf8.o: gmqcc.h opts.def
correct.o: gmqcc.h opts.def

test.o: gmqcc.h opts.def
util.o: gmqcc.h opts.def
conout.o: gmqcc.h opts.def
fs.o: gmqcc.h opts.def

main.o: gmqcc.h opts.def lexer.h
lexer.o: gmqcc.h opts.def lexer.h
parser.o: gmqcc.h opts.def lexer.h ast.h ir.h
fs.o: gmqcc.h opts.def

util.o: gmqcc.h opts.def
conout.o: gmqcc.h opts.def
fs.o: gmqcc.h opts.def
