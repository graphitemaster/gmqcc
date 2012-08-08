#include "gmqcc.h"
#include "ast.h"

/* NOTE: it's a test - I'll abort() on epic-failure */

#ifdef assert
#   undef assert
#endif
/* (note: 'do {} while(0)' forces the need for a semicolon after assert() */
#define assert(x) do { if ( !(x) ) { printf("Assertion failed: %s\n", #x); abort(); } } while(0)

VECTOR_MAKE(ast_value*, globals);
VECTOR_MAKE(ast_function*, functions);

uint32_t    opts_flags[1 + (COUNT_FLAGS / 32)];
uint32_t    opts_warn [1 + (COUNT_WARNINGS / 32)];

uint32_t    opts_O        = 1;
const char *opts_output   = "progs.dat";
int         opts_standard = COMPILER_GMQCC;
bool        opts_debug    = false;
bool        opts_memchk   = false;

#include "ast-macros.h"

int main()
{
    size_t i;

    ir_builder     *ir;

    TESTVARS();

    DEFVAR(vi);
    DEFVAR(vx);
    DEFVAR(f0);
    DEFVAR(f1);
    DEFVAR(f5);
    DEFVAR(sHello);
    DEFVAR(print);

    /* opts_debug = true; */

BUILTIN(print, TYPE_VOID, -1);
PARAM(TYPE_STRING, text);
ENDBUILTIN();

    TESTINIT();
VAR(TYPE_FLOAT, f0);
VAR(TYPE_FLOAT, f1);
VAR(TYPE_FLOAT, f5);
VAR(TYPE_STRING, sHello);
MKCONSTFLOAT(f0, 0.0);
MKCONSTFLOAT(f1, 1.0);
MKCONSTFLOAT(f5, 5.0);
MKCONSTSTRING(sHello, "Hello, World\n");

FUNCTION(foo, TYPE_VOID);
ENDFUNCTION(foo);

FUNCTION(main, TYPE_VOID);

    VAR(TYPE_FLOAT, vi);
    VAR(TYPE_FLOAT, vx);

    MKLOCAL(vi);
    MKLOCAL(vx);

    STATE(ASSIGN(STORE_F, vi, f0));
    WHILE(BIN(LT, vi, f5));
        STATE(ASSIGN(STORE_F, vx, BIN(MUL_F, vi, f5)));
        STATE(ASSIGN(STORE_F, vi, BIN(ADD_F, vi, f1)));
    ENDWHILE();

    CALL(print)
    CALLPARAM(sHello)
    ENDCALL();

ENDFUNCTION(main);

    ir = ir_builder_new("ast_test");
    assert(ir);

    /* gen globals */
    for (i = 0; i < globals_elements; ++i) {
        if (!ast_global_codegen(globals_data[i], ir)) {
            assert(!"failed to generate global");
        }
    }

    /* gen functions */
    for (i = 0; i < functions_elements; ++i) {
        if (!ast_function_codegen(functions_data[i], ir)) {
            assert(!"failed to generate function");
        }
        if (!ir_function_finalize(functions_data[i]->ir_func))
            assert(!"finalize on function failed...");
    }


    /* dump */
    ir_builder_dump(ir, printf);

    /* Now create a file */
    if (!ir_builder_generate(ir, "test_ast.dat"))
        printf("*** failed to generate code\n");

    /* ir cleanup */
    ir_builder_delete(ir);

    /* cleanup */
    /* Functions must be deleted FIRST since their expressions
     * reference global variables.
     */
    for (i = 0; i < functions_elements; ++i) {
        ast_function_delete(functions_data[i]);
    }
    if (functions_data)
        mem_d(functions_data);

    /* We must delete not only globals, but also the functions'
     * ast_values (their type and name), that's why we added them to the globals vector.
     */
    for (i = 0; i < globals_elements; ++i) {
        ast_value_delete(globals_data[i]);
    }
    if (globals_data)
        mem_d(globals_data);
    return 0;
}
