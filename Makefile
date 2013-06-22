include include.mk

UNAME  ?= $(shell uname)
CYGWIN  = $(findstring CYGWIN,  $(UNAME))
MINGW   = $(findstring MINGW32, $(UNAME))

CFLAGS  += -Wall -Wextra -Werror -fno-strict-aliasing $(OPTIONAL)
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
	    -Wno-unknown-warning-option        \
	    -Wno-cast-align                    \
	    -Wstrict-prototypes
else
	#Tiny C Compiler doesn't know what -pedantic-errors is
	# and instead of ignoring .. just errors.
	ifneq ($(CC), tcc)
		CFLAGS += -pedantic-errors
	else
		CFLAGS += -Wno-pointer-sign -fno-common
	endif
	
	#-Wstrict-prototypes is not valid in g++
	ifneq ($(CC), g++)
		CFLAGS += -Wstrict-prototypes
	endif
endif

#we have duplicate object files when dealing with creating a simple list
#for dependinces. To combat this we use some clever recrusive-make to
#filter the list and remove duplicates which we use for make depend
RMDUP = $(if $1,$(firstword $1) $(call RMDUP,$(filter-out $(firstword $1),$1)))
DEPS := $(call RMDUP, $(OBJ_P) $(OBJ_T) $(OBJ_C) $(OBJ_X))

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
	PAK       = gmqpak.exe
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
	PAK       = gmqpak.exe
else
	QCVM      = qcvm
	GMQCC     = gmqcc
	TESTSUITE = testsuite
	PAK       = gmqpak
endif
endif

#standard rules
%.o: %.c
	$(CC) -c $< -o $@ $(CPPFLAGS) $(CFLAGS)

exec-standalone.o: exec.c
	$(CC) -c $< -o $@ $(CPPFLAGS) $(CFLAGS) -DQCVM_EXECUTOR=1

$(QCVM): $(OBJ_X)
	$(CC) -o $@ $^ $(LDFLAGS) $(LIBS)

$(GMQCC): $(OBJ_C) $(OBJ_D)
	$(CC) -o $@ $^ $(LDFLAGS) $(LIBS)

$(TESTSUITE): $(OBJ_T)
	$(CC) -o $@ $^ $(LDFLAGS) $(LIBS)

$(PAK): $(OBJ_P)
	$(CC) -o $@ $^ $(LDFLAGS)

all: $(GMQCC) $(QCVM) $(TESTSUITE) $(PAK)

check: all
	@ ./$(TESTSUITE)
test: all
	@ ./$(TESTSUITE)

clean:
	rm -rf *.o $(GMQCC) $(QCVM) $(TESTSUITE) $(PAK) *.dat gource.mp4 *.exe gm-qcc.tgz ./cov-int

splint:
	@  splint $(SPLINTFLAGS) *.c *.h

gource:
	@ gource $(GOURCEFLAGS)

gource-record:
	@ gource $(GOURCEFLAGS) -o - | ffmpeg $(FFMPEGFLAGS) gource.mp4

depend:
	@ makedepend -Y -w 65536 2> /dev/null $(subst .o,.c,$(DEPS))


coverity:
	@cov-build --dir cov-int $(MAKE)
	@tar czf gm-qcc.tgz cov-int
	@rm -rf cov-int
	@echo gm-qcc.tgz generated, submit for analysis

#install rules
install: install-gmqcc install-qcvm install-gmqpak install-doc
install-gmqcc: $(GMQCC)
	install -d -m755               $(DESTDIR)$(BINDIR)
	install    -m755  $(GMQCC)     $(DESTDIR)$(BINDIR)/$(GMQCC)
install-qcvm: $(QCVM)
	install -d -m755               $(DESTDIR)$(BINDIR)
	install    -m755  $(QCVM)      $(DESTDIR)$(BINDIR)/$(QCVM)
install-gmqpak: $(PAK)
	install -d -m755               $(DESTDIR)$(BINDIR)
	install    -m755  $(PAK)       $(DESTDIR)$(BINDIR)/$(PAK)
install-doc:
	install -d -m755               $(DESTDIR)$(MANDIR)/man1
	install    -m644  doc/gmqcc.1  $(DESTDIR)$(MANDIR)/man1/
	install    -m644  doc/qcvm.1   $(DESTDIR)$(MANDIR)/man1/
	install    -m644  doc/gmqpak.1 $(DESTDIR)$(MANDIR)/man1/

# DO NOT DELETE

util.o: gmqcc.h opts.def
fs.o: gmqcc.h opts.def
conout.o: gmqcc.h opts.def
opts.o: gmqcc.h opts.def
pak.o: gmqcc.h opts.def
stat.o: gmqcc.h opts.def
test.o: gmqcc.h opts.def
main.o: gmqcc.h opts.def lexer.h
lexer.o: gmqcc.h opts.def lexer.h
parser.o: gmqcc.h opts.def lexer.h ast.h ir.h intrin.h
code.o: gmqcc.h opts.def
ast.o: gmqcc.h opts.def ast.h ir.h
ir.o: gmqcc.h opts.def ir.h
ftepp.o: gmqcc.h opts.def lexer.h
utf8.o: gmqcc.h opts.def
correct.o: gmqcc.h opts.def
