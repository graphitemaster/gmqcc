CC ?= clang
CFLAGS = -MD -Wall -Wextra -pedantic-errors
LDFLAGS = -lm

CSRCS = ansi.c ast.c code.c conout.c fold.c fs.c ftepp.c hash.c intrin.c ir.c lexer.c main.c opts.c parser.c stat.c utf8.c util.c
TSRCS = ansi.c conout.c fs.c hash.c opts.c stat.c test.c util.c

COBJS = $(CSRCS:.c=.o)
TOBJS = $(TSRCS:.c=.o)

CDEPS = $(CSRCS:.c=.d)
TDEPS = $(TSRCS:.c=.d)

CBIN = gmqcc
TBIN = testsuite

all: $(CBIN) $(TBIN)

$(CBIN): $(COBJS)
	$(CC) $(LDFLAGS) $(COBJS) -o $@

$(TBIN): $(TOBJS)
	$(CC) $(LDFLAGS) $(TOBJS) -o $@

.c.o:
	$(CC) -c $(CFLAGS) $< -o $@

test: $(CBIN) $(TBIN)
	@./$(TBIN)

clean:
	rm -f *.d
	rm -f $(COBJS) $(CDEPS) $(CBIN)
	rm -f $(TOBJS) $(TDEPS) $(TBIN)
