DESTDIR :=
PREFIX := /usr/local
BINDIR := $(PREFIX)/bin
DATADIR := $(PREFIX)/share
MANDIR := $(DATADIR)/man

CC     ?= clang
CFLAGS += -Wall -Wextra -I. -pedantic-errors
#turn on tons of warnings if clang is present
ifeq ($(CC), clang)
	CFLAGS +=                         \
		-Weverything                  \
		-Wno-padded                   \
		-Wno-format-nonliteral        \
		-Wno-disabled-macro-expansion \
		-Wno-conversion               \
		-Wno-missing-prototypes

endif
ifeq ($(track), no)
    CFLAGS += -DNOTRACK
endif

OBJ     =             \
          util.o      \
          code.o      \
          ast.o       \
          ir.o        \
          con.o       \
          ftepp.o     \
          opts.o

OBJ_T = test.o util.o con.o
OBJ_C = main.o lexer.o parser.o
OBJ_X = exec-standalone.o util.o con.o


default: gmqcc
%.o: %.c
	$(CC) -c $< -o $@ $(CFLAGS)

exec-standalone.o: exec.c
	$(CC) -c $< -o $@ $(CFLAGS) -DQCVM_EXECUTOR=1

qcvm: $(OBJ_X)
	$(CC) -o $@ $^ $(CFLAGS) -lm

gmqcc: $(OBJ_C) $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS)

testsuite: $(OBJ_T)
	$(CC) -o $@ $^ $(CFLAGS)

all: gmqcc qcvm testsuite

check: all
	@ ./testsuite

clean:
	rm -f *.o gmqcc qcvm testsuite *.dat


$(OBJ) $(OBJ_C) $(OBJ_X): gmqcc.h opts.def
main.o: lexer.h
parser.o: ast.h lexer.h
ast.o: ast.h ir.h
ir.o: ir.h

install: install-gmqcc install-qcvm install-doc
install-gmqcc: gmqcc
	install -d -m755               $(DESTDIR)$(BINDIR)
	install    -m755  gmqcc        $(DESTDIR)$(BINDIR)/gmqcc
install-qcvm: qcvm
	install -d -m755               $(DESTDIR)$(BINDIR)
	install    -m755  qcvm         $(DESTDIR)$(BINDIR)/qcvm
install-doc:
	install -d -m755               $(DESTDIR)$(MANDIR)/man1
	install    -m755  doc/gmqcc.1  $(DESTDIR)$(MANDIR)/man1/
