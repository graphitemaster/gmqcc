DESTDIR :=
OPTIONAL:=
PREFIX  := /usr/local
BINDIR  := $(PREFIX)/bin
DATADIR := $(PREFIX)/share
MANDIR  := $(DATADIR)/man

UNAME  ?= $(shell uname)
CYGWIN  = $(findstring CYGWIN,  $(UNAME))
MINGW   = $(findstring MINGW32, $(UNAME))

CC      ?= clang
# linker flags and optional additional libraries if required
LDFLAGS +=
LIBS    += -lm

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
	    -Wno-unknown-warning-option
else
	#Tiny C Compiler doesn't know what -pedantic-errors is
	# and instead of ignoring .. just errors.
	ifneq ($(CC), tcc)
		CFLAGS += -pedantic-errors
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

#we have duplicate object files when dealing with creating a simple list
#for dependinces. To combat this we use some clever recrusive-make to
#filter the list and remove duplicates which we use for make depend
RMDUP = $(if $1,$(firstword $1) $(call RMDUP,$(filter-out $(firstword $1),$1)))
DEPS := $(call RMDUP, $(OBJ_D) $(OBJ_P) $(OBJ_T) $(OBJ_C) $(OBJ_X))

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
	PAK       = gmqpak.exe
else
	QCVM      = qcvm
	GMQCC     = gmqcc
	TESTSUITE = testsuite
	PAK       = gmqpak
endif
endif

#gource flags
GOURCEFLAGS=                  \
    --date-format "%d %B, %Y" \
    --seconds-per-day 0.01    \
    --auto-skip-seconds 1     \
    --title "GMQCC"           \
    --key                     \
    --camera-mode overview    \
    --highlight-all-users     \
    --file-idle-time 0        \
    --hide progress,mouse     \
    --stop-at-end             \
    --max-files 99999999999   \
    --max-file-lag 0.000001   \
    --bloom-multiplier 1.3    \
    --logo doc/html/gmqcc.png \
    -1280x720

#ffmpeg flags for gource
FFMPEGFLAGS=                  \
    -y                        \
    -r 60                     \
    -f image2pipe             \
    -vcodec ppm               \
    -i -                      \
    -vcodec libx264           \
    -preset ultrafast         \
    -crf 1                    \
    -threads 0                \
    -bf 0

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
    -compmempass         \
    -nullpass            \
    -onlytrans           \
    -predboolint         \
    -boolops             \
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
    +charindex           \
    -kepttrans           \
    -unqualifiedtrans    \
    +matchanyintegral    \
    +voidabstract        \
    -nullassign          \
    -unrecog             \
    -casebreak           \
    -retvalbool          \
    -retvalother         \
    -mayaliasunique      \
    -realcompare         \
    -observertrans       \
    -abstract            \
    -statictrans         \
    -castfcnptr

#standard rules
default: all
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
	rm -f *.o $(GMQCC) $(QCVM) $(TESTSUITE) $(PAK) *.dat gource.mp4 *.exe

splint:
	@  splint $(SPLINTFLAGS) *.c *.h

gource:
	@ gource $(GOURCEFLAGS)

gource-record:
	@ gource $(GOURCEFLAGS) -o - | ffmpeg $(FFMPEGFLAGS) gource.mp4

depend:
	@makedepend    -Y -w 65536 2> /dev/null \
		$(subst .o,.c,$(DEPS))

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

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/gmqcc
	rm -f $(DESTDIR)$(BINDIR)/qcvm
	rm -f $(DESTDIR)$(BINDIR)/gmqpak
	rm -f $(DESTDIR)$(MANDIR)/man1/doc/gmqcc.1
	rm -f $(DESTDIR)$(MANDIR)/man1/doc/qcvm.1
	rm -f $(DESTDIR)$(MANDIR)/man1/doc/gmqpak.1

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
pak.o: gmqcc.h opts.def
test.o: gmqcc.h opts.def
main.o: gmqcc.h opts.def lexer.h
lexer.o: gmqcc.h opts.def lexer.h
parser.o: gmqcc.h opts.def lexer.h ast.h ir.h intrin.h
