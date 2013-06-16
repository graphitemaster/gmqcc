#
# This is the Makefile for the BSD flavor
#

DESTDIR  :=
OPTIONAL :=
PREFIX   := /usr/local
BINDIR   := $(PREFRIX)/bin
DATADIR  := $(PREFIX)/share
MANDIR   := $(DATADIR)/man

GITTEST  != git describe --always 2>/dev/null
GITINFO  :=

.if $(GITTEST)
    GITINFO != git describe --always
.endif

CC       ?= clang

# linker flags and optional additional libraries if required
LDFLAGS  +=
LIBS     += -lm

CFLAGS   +=  -Wall -Wextra -Werror -fno-strict-aliasing -DGMQCC_GITINFO=\"$(GITINFO)\"$(OPTIONAL)

.if $(CC) == clang
    CFLAGS +=   -Weverything\
                -Wno-padded\
                -Wno-format-nonliteral\
                -Wno-disabled-macro-expansion\
                -Wno-conversion\
                -Wno-missing-prototypes\
                -Wno-float-equal\
                -Wno-unknown-warning-option\
                -Wstrict-prototypes
.else
.    if $(CC) == tcc
       CFLAGS += -Wstrict-prototypes -pedantic-errors
.    else
       CFLAGS += -Wno-pointer-sign -fno-common
.    endif
.endif

OBJ_C = main.o lexer.o parser.o fs.o stat.o util.o code.o ast.o ir.o conout.o ftepp.o opts.o utf8.o correct.o
OBJ_P = util.o fs.o conout.o opts.o pak.o stat.o
OBJ_T = test.o util.o conout.o fs.o stat.o
OBJ_X = exec-standalone.o util.o conout.o fs.o stat.o

QCVM      = qcvm
GMQCC     = gmqcc
TESTSUITE = testsuite
PAK       = gmqpak

#gource flags
GOURCEFLAGS =                 \
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
SPLINTFLAGS =                 \
    -redef                    \
    -noeffect                 \
    -nullderef                \
    -usedef                   \
    -type                     \
    -mustfreeonly             \
    -nullstate                \
    -varuse                   \
    -mustfreefresh            \
    -compdestroy              \
    -compmempass              \
    -nullpass                 \
    -onlytrans                \
    -predboolint              \
    -boolops                  \
    -incondefs                \
    -macroredef               \
    -retvalint                \
    -nullret                  \
    -predboolothers           \
    -globstate                \
    -dependenttrans           \
    -branchstate              \
    -compdef                  \
    -temptrans                \
    -usereleased              \
    -warnposix                \
    +charindex                \
    -kepttrans                \
    -unqualifiedtrans         \
    +matchanyintegral         \
    +voidabstract             \
    -nullassign               \
    -unrecog                  \
    -casebreak                \
    -retvalbool               \
    -retvalother              \
    -mayaliasunique           \
    -realcompare              \
    -observertrans            \
    -abstract                 \
    -statictrans              \
    -castfcnptr

#standard rules
default: all

c.o:
	$(CC) -c ${.IMPSRC} -o ${.TARGET} $(CPPFLAGS) $(CFLAGS)

exec-standalone.o: exec.c
	$(CC) -c ${.ALLSRC} -o ${.TARGET} $(CPPFLAGS) $(CFLAGS) -DQCVM_EXECUTOR=1

$(QCVM): $(OBJ_X)
	$(CC) -o ${.TARGET} ${.IMPSRC} $(LDFLAGS) $(LIBS) $(OBJ_X)

$(GMQCC): $(OBJ_C)
	$(CC) -o ${.TARGET} ${.IMPSRC} $(LDFLAGS) $(LIBS) $(OBJ_C)

$(TESTSUITE): $(OBJ_T)
	$(CC) -o ${.TARGET} ${.IMPSRC} $(LDFLAGS) $(LIBS) $(OBJ_T)

$(PAK): $(OBJ_P)
	$(CC) -o ${.TARGET} ${.IMPSRC} $(LDFLAGS) $(OBJ_P)

all: $(GMQCC) $(QCVM) $(TESTSUITE) $(PAK)

check: all
	@ ./$(TESTSUITE)
test: all
	@ ./$(TESTSUITE)

clean:
	rm -f *.o $(GMQCC) $(QCVM) $(TESTSUITE) $(PAK) *.dat gource.mp4 *.exe

splint:
	@ splint $(SPLINTFLAGS) *.c *.h

gource:
	@ gource $(GOURCEFLAGS)

gource-record:
	@ gource $(GOURCEFLAGS) -o - | ffmpeg $(FFMPEGFLAGS) gource.mp4

#install rules
install: install-gmqcc install-qcvm install-gmqpak install-doc
install-gmqcc: $(GMQCC)
	install -d -m755              $(DESTDIR)$(BINDIR)
	install    -m755 $(GMQCC)     $(DESTDIR)$(BINDIR)/$(GMQCC)
install-qcvm: $(QCVM)
	install -d -m755              $(DESTDIR)$(BINDIR)
	install    -m755 $(QCVM)      $(DESTDIR)$(BINDIR)/$(QCVM)
install-gmqpak: $(PAK)
	install -d -m755              $(DESTDIR)$(BINDIR)
	install    -m755 $(PAK)       $(DESTDIR)$(BINDIR)/$(PAK)
install-doc:
	install -d -m755              $(DESTDIR)$(MANDIR)/man1
	install    -m644 doc/gmqcc.1  $(DESTDIR)$(MANDIR)/man1/
	install    -m644 doc/qcvm.1   $(DESTDIR)$(MANDIR)/man1/
	install    -m644 doc/gmqpak.1 $(DESTDIR)$(MANDIR)/man1/

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/gmqcc
	rm -f $(DESTDIR)$(BINDIR)/qcvm
	rm -f $(DESTDIR)$(BINDIR)/gmqpak
	rm -f $(DESTDIR)$(MANDIR)/man1/doc/gmqcc.1
	rm -f $(DESTDIR)$(MANDIR)/man1/doc/qcvm.1
	rm -f $(DESTDIR)$(MANDIR)/man1/doc/gmqpak.1

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
