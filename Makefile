CC     = gcc
CFLAGS = -O3 -Wall
OBJ    = main.o    \
         lex.o     \
         error.o   \
         parse.o   \
         typedef.o \
         alloc.o

%.o: %.c
	$(CC) -c $< -o $@ $(CFLAGS)

gmqcc: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS)
	
clean:
	rm -f *.o gmqcc
