#
# This is the Makefile for the BSD flavor
#
.include "include.mk"

GITTEST  != git describe --always 2>/dev/null
GITINFO  :=

.if $(GITTEST)
    GITINFO != git describe --always
.endif

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

DEPS != for i in $(OBJ_C) $(OBJ_P) $(OBJ_T) $(OBJ_X); do echo $$i; done | sort | uniq

QCVM      = qcvm
GMQCC     = gmqcc
TESTSUITE = testsuite
PAK       = gmqpak

#standard rules
c.o: ${.IMPSRC} 
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

depend:
	@makedepend -Y -f BSDmakefile -w 65536 2> /dev/null ${DEPS:C/\.o/.c/g}

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

# DO NOT DELETE

ast.o: gmqcc.h opts.def ast.h ir.h
code.o: gmqcc.h opts.def
conout.o: gmqcc.h opts.def
correct.o: gmqcc.h opts.def
fs.o: gmqcc.h opts.def
ftepp.o: gmqcc.h opts.def lexer.h
ir.o: gmqcc.h opts.def ir.h
lexer.o: gmqcc.h opts.def lexer.h
main.o: gmqcc.h opts.def lexer.h
opts.o: gmqcc.h opts.def
pak.o: gmqcc.h opts.def
parser.o: gmqcc.h opts.def lexer.h ast.h ir.h intrin.h
stat.o: gmqcc.h opts.def
test.o: gmqcc.h opts.def
utf8.o: gmqcc.h opts.def
util.o: gmqcc.h opts.def
