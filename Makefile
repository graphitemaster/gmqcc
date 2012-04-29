CC     ?= clang
CFLAGS += -Wall -I. -pedantic-errors -std=c90
OBJ     = main.o      \
          lex.o       \
          error.o     \
          parse.o     \
          typedef.o   \
          util.o      \
          code.o      \
          asm.o       \
          ast.o       \
          ir.o
# ast and ir test
TEST_AST = test/ast-test.o
TEST_IR  = test/ir-test.o

#default is compiler only
default: gmqcc
%.o: %.c
	$(CC) -c $< -o $@ $(CFLAGS)

# test targets
test_ast: $(TEST_AST) $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS)	
test_ir:  $(TEST_IR) $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS)
test: test_ast test_ir

# compiler target	
gmqcc: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS)

#all target is test and all
all: test gmqcc
	
clean:
	rm -f *.o gmqcc test_ast test_ir test/*.o
