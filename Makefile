CC     ?= clang
CFLAGS += -Wall
OBJ     = main.o      \
          lex.o       \
          error.o     \
          parse.o     \
          typedef.o   \
          util.o      \
          code.o      \
          asm.c       \
          ast.c       \
          ir.c

%.o: %.c
	$(CC) -c $< -o $@ $(CFLAGS)

gmqcc: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS)
	
clean:
	rm -f *.o gmqcc
