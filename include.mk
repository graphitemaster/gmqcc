# default directories and paths
DESTDIR :=
OPTIONAL:=
PREFIX  := /usr/local
BINDIR  := $(PREFIX)/bin
DATADIR := $(PREFIX)/share
MANDIR  := $(DATADIR)/man

# compiler
CC      ?= clang

# linker flags and optional additional libraries if required
LDFLAGS +=
LIBS    += -lm

#objects
OBJ_C = main.o ansi.o util.o stat.o fs.o lexer.o parser.o code.o ast.o ir.o conout.o ftepp.o opts.o utf8.o correct.o fold.o intrin.o
OBJ_P = pak.o ansi.o util.o stat.o fs.o conout.o opts.o pak.o
OBJ_T = test.o ansi.o util.o stat.o fs.o opts.o conout.o
OBJ_X = exec-standalone.o ansi.o util.o stat.o fs.o opts.o conout.o

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

#always the right rule
default: all

#uninstall rule
uninstall:
	rm -f $(DESTDIR)$(BINDIR)/gmqcc
	rm -f $(DESTDIR)$(BINDIR)/qcvm
	rm -f $(DESTDIR)$(BINDIR)/gmqpak
	rm -f $(DESTDIR)$(MANDIR)/man1/doc/gmqcc.1
	rm -f $(DESTDIR)$(MANDIR)/man1/doc/qcvm.1
	rm -f $(DESTDIR)$(MANDIR)/man1/doc/gmqpak.1

STYLE_MATCH = \( -name '*.[ch]' -or -name '*.def' -or -name '*.qc' \)

whitespace:
	find . -type f $(STYLE_MATCH) -exec sed -i 's/ *$$//' '{}' ';'
newline:
	find . -type f $(STYLE_MATCH) -exec sed -i -e '$$a\' '{}' ';'
indent:
	find . -type f $(STYLE_MATCH) -exec sed -i 's/\t/    /g' '{}' ';'

style: whitespace newline indent
