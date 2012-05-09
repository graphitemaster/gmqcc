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

#if 0
int main()
{
    /* AST */
    ast_expression *exp;
    ast_value      *gfoo    = NULL;
    ast_value      *gbar    = NULL;
    ast_value      *const_3 = NULL;
    ast_value      *const_4 = NULL;
    ast_value      *vmain   = NULL;
    ast_function   *fmain   = NULL;
    ast_block      *ba      = NULL;
    ast_value      *li      = NULL;

    /* IR */
    ir_builder     *ir = NULL;

    /* common stuff */

    size_t i;
    lex_ctx ctx;
    ctx.file = NULL;
    ctx.line = 1;

    /* globals */
    gfoo = ast_value_new(ctx, "foo", TYPE_FLOAT);
        assert(gfoo);
    assert(globals_add(gfoo) >= 0);

    gbar = ast_value_new(ctx, "bar", TYPE_FLOAT);
        assert(gbar);
    assert(globals_add(gbar) >= 0);

    const_3 = ast_value_new(ctx, "3", TYPE_FLOAT);
        assert(const_3);
    assert(globals_add(const_3) >= 0);
    const_3->isconst = true;
    const_3->constval.vfloat = 3.0;

    const_4 = ast_value_new(ctx, "4", TYPE_FLOAT);
        assert(const_4);
    assert(globals_add(const_4) >= 0);
    const_4->isconst = true;
    const_4->constval.vfloat = 4.0;

    /* defining a function */
    vmain = ast_value_new(ctx, "main", TYPE_FUNCTION);
        assert(vmain);
    assert(globals_add(vmain) >= 0);
    /* This happens in ast_function_new:
        vmain->isconst = true;
        vmain->constval.vfunc = fmain;
       Creating a function node connects the global to the function.
       You still have to delete *BOTH*, deleting one will NOT delete the other,
       it will only unlink them.
     */

    /* creating a function body */
    fmain = ast_function_new(ctx, "main", vmain);
        assert(fmain);
    assert(functions_add(fmain) >= 0);

    /* { block */
    ba = ast_block_new(ctx);
        assert(ba);
    assert(ast_function_blocks_add(fmain, ba));

    /* local variable i */
    li = ast_value_new(ctx, "i", TYPE_FLOAT);
        assert(li);
    assert(ast_block_locals_add(ba, li));

    /* I realize having to provide the opcode isn't the best way to go, but,
     * urgh... */
    /* foo = 3; */
    exp = (ast_expression*)ast_store_new(ctx, INSTR_STORE_F, gfoo, (ast_expression*)const_3);
        assert(exp);
    assert(ast_block_exprs_add(ba, exp));
    /* bar = 4; */
    exp = (ast_expression*)ast_store_new(ctx, INSTR_STORE_F, gbar, (ast_expression*)const_4);
        assert(exp);
    assert(ast_block_exprs_add(ba, exp));

    /* i = foo * bar */
    exp = (ast_expression*)ast_binary_new(ctx,  INSTR_MUL_F, (ast_expression*)gbar, (ast_expression*)gfoo);
        assert(exp);
    exp = (ast_expression*)ast_store_new(ctx, INSTR_STORE_F, li, exp);
        assert(exp);
    assert(ast_block_exprs_add(ba, exp));

    /* } block */

    /* Next up: Having the AST generate IR */

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
#endif

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
    
    TESTINIT();
VAR(TYPE_FLOAT, f0);
VAR(TYPE_FLOAT, f1);
VAR(TYPE_FLOAT, f5);
MKCONSTFLOAT(f0, 0.0);
MKCONSTFLOAT(f1, 1.0);
MKCONSTFLOAT(f5, 5.0);

FUNCTION(main);

VAR(TYPE_FLOAT, vi);
VAR(TYPE_FLOAT, vx);

MKLOCAL(vi);
MKLOCAL(vx);

STATE(ASSIGN(STORE_F, vi, f0));
WHILE(BIN(LT, vi, f5));
STATE(ASSIGN(STORE_F, vx, BIN(MUL_F, vi, f5)));
STATE(ASSIGN(STORE_F, vi, BIN(ADD_F, vi, f1)));
ENDWHILE();

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
