CC      = gcc
CFLAGS += -Wall
OBJ     = main.o    \
          lex.o     \
          error.o   \
          parse.o   \
          typedef.o \
          util.o    \
          code.o

%.o: %.c
	$(CC) -c $< -o $@ $(CFLAGS)

gmqcc: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS)
	
clean:
	rm -f *.o gmqcc
