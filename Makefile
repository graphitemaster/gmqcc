DESTDIR :=
PREFIX  := /usr/local
BINDIR  := $(PREFIX)/bin
DATADIR := $(PREFIX)/share
MANDIR  := $(DATADIR)/man

UNAME  ?= $(shell uname)
CYGWIN  = $(findstring CYGWIN,  $(UNAME))
MINGW   = $(findstring MINGW32, $(UNAME))

CC     ?= clang
CFLAGS += -Wall -Wextra -I. -fno-strict-aliasing -fsigned-char
CFLAGS += -DGMQCC_GITINFO="`git describe`"
#turn on tons of warnings if clang is present
# but also turn off the STUPID ONES
ifeq ($(CC), clang)
	CFLAGS +=                         \
		-Weverything                  \
		-Wno-padded                   \
		-Wno-format-nonliteral        \
		-Wno-disabled-macro-expansion \
		-Wno-conversion               \
		-Wno-missing-prototypes       \
		-Wno-float-equal              \
		-Wno-cast-align
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

OBJ_D = util.o code.o ast.o ir.o conout.o ftepp.o opts.o file.o utf8.o correct.o
OBJ_T = test.o util.o conout.o file.o
OBJ_C = main.o lexer.o parser.o file.o
OBJ_X = exec-standalone.o util.o conout.o file.o

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
else
	#arm support for linux .. we need to allow unaligned accesses
	#to memory otherwise we just segfault everywhere
	ifneq (, $(findstring arm, $(shell uname -m)))
		CFLAGS += -munaligned-access
	endif

	QCVM      = qcvm
	GMQCC     = gmqcc
	TESTSUITE = testsuite
endif
endif

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
	$(CC) -o $@ $^ $(CFLAGS)

all: $(GMQCC) $(QCVM) $(TESTSUITE)

check: all
	@ ./$(TESTSUITE)

clean:
	rm -f *.o $(GMQCC) $(QCVM) $(TESTSUITE) *.dat

# deps
$(OBJ_D) $(OBJ_C) $(OBJ_X): gmqcc.h opts.def
main.o:   lexer.h
parser.o: ast.h lexer.h
ftepp.o:  lexer.h
lexer.o:  lexer.h
ast.o:    ast.h ir.h
ir.o:     ir.h

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
	install    -m755  doc/gmqcc.1  $(DESTDIR)$(MANDIR)/man1/
	install    -m755  doc/qcvm.1   $(DESTDIR)$(MANDIR)/man1/
