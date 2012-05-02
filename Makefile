CC     ?= clang
CFLAGS += -Wall -I. -pedantic-errors -std=c90 -Wno-attributes
OBJ     = lex.o       \
          error.o     \
          parse.o     \
          typedef.o   \
          util.o      \
          code.o      \
          asm.o       \
          ast.o       \
          ir.o 
OBJ_A = test/ast-test.o
OBJ_I = test/ir-test.o
OBJ_C = main.o

#default is compiler only
default: gmqcc
%.o: %.c
	$(CC) -c $< -o $@ $(CFLAGS)

# test targets
test_ast: $(OBJ_A) $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS)	
test_ir:  $(OBJ_I) $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS)
test: test_ast test_ir

# compiler target	
gmqcc: $(OBJ_C) $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS)

#all target is test and all
all: test gmqcc
	
clean:
	rm -f *.o gmqcc test_ast test_ir test/*.o
