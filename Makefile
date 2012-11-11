CC     ?= clang
CFLAGS += -Wall -I. -fomit-frame-pointer -fno-stack-protector
#turn on tons of warnings if clang is present
ifeq ($(CC), clang)
	CFLAGS +=                  \
		-Weverything                  \
		-Wno-missing-prototypes       \
		-Wno-unused-parameter         \
		-Wno-sign-compare             \
		-Wno-implicit-fallthrough     \
		-Wno-sign-conversion          \
		-Wno-conversion               \
		-Wno-disabled-macro-expansion \
		-Wno-padded                   \
		-Wno-format-nonliteral

endif
ifeq ($(track), no)
    CFLAGS += -DNOTRACK
endif

OBJ     = \
          util.o      \
          code.o      \
          ast.o       \
          ir.o        \
          error.o
OBJ_A = test/ast-test.o
OBJ_I = test/ir-test.o
OBJ_C = main.o lexer.o parser.o
OBJ_X = exec-standalone.o util.o

#default is compiler only
default: gmqcc
%.o: %.c
	$(CC) -c $< -o $@ $(CFLAGS)

exec-standalone.o: exec.c
	$(CC) -c $< -o $@ $(CFLAGS) -DQCVM_EXECUTOR=1

# test targets
test_ast: $(OBJ_A) $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS)
test_ir:  $(OBJ_I) $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS)
qcvm:     $(OBJ_X)
	$(CC) -o $@ $^ $(CFLAGS) -lm
test: test_ast test_ir

# compiler target
gmqcc: $(OBJ_C) $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS)

#all target is test and all
all: test gmqcc

clean:
	rm -f *.o gmqcc qcvm test_ast test_ir test/*.o
	

