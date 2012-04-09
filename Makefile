CC     = gcc
CFLAGS = -O3 -Wall
OBJ    = main.o lex.o error.o parse.o cpp.o

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

gmqcc: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS)
	
clean:
	rm -f *.o dpqcc
