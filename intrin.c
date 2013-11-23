/*
 * Copyright (C) 2012, 2013
 *     Dale Weiler
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is furnished to do
 * so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include <string.h>
#include "parser.h"

/*
 * Provides all the "intrinsics" / "builtins" for GMQCC. These can do
 * a few things, they can provide fall back implementations for math
 * functions if the definitions don't exist for some given engine. Or
 * then can determine definitions for existing builtins, and simply
 * wrap back to them instead.  This is like a "portable" intrface that
 * is entered when -fintrin is used (causing all existing builtins to
 * be ignored by the compiler and instead interface through here.
 */
#define intrin_ctx(I) parser_ctx((I)->parser)

static GMQCC_INLINE ast_function *intrin_value(intrin_t *intrin, ast_value **out, const char *name, qcint_t vtype) {
    ast_value    *value = NULL;
    ast_function *func  = NULL;
    char          buffer[1024];
    char          stype [1024];

    util_snprintf(buffer, sizeof(buffer), "__builtin_%s", name);
    util_snprintf(stype,  sizeof(stype),   "<%s>",        type_name[vtype]);

    value                    = ast_value_new(intrin_ctx(intrin), buffer, TYPE_FUNCTION);
    value->intrinsic         = true;
    value->expression.next   = (ast_expression*)ast_value_new(intrin_ctx(intrin), stype, vtype);
    func                     = ast_function_new(intrin_ctx(intrin), buffer, value);
    value->expression.flags |= AST_FLAG_ERASEABLE;

    *out = value;
    return func;
}

static GMQCC_INLINE void intrin_reg(intrin_t *intrin, ast_value *const value, ast_function *const func) {
    vec_push(intrin->parser->functions, func);
    vec_push(intrin->parser->globals,   (ast_expression*)value);
}

#define QC_M_E 2.71828182845905f

static ast_expression *intrin_pow (intrin_t *intrin) {
    /*
     * float pow(float x, float y) {
     *   float local = 1.0f;
     *   while (y > 0) {
     *     while (!(y & 1)) {
     *       y >>= 2;
     *       x *=  x;
     *     }
     *     y--;
     *     local *= x;
     *   }
     *   return local;
     * }
     */
    ast_value    *value = NULL;
    ast_value    *arg1  = ast_value_new(intrin_ctx(intrin), "x",     TYPE_FLOAT);
    ast_value    *arg2  = ast_value_new(intrin_ctx(intrin), "y",     TYPE_FLOAT);
    ast_value    *local = ast_value_new(intrin_ctx(intrin), "local", TYPE_FLOAT);
    ast_block    *body  = ast_block_new(intrin_ctx(intrin));
    ast_block    *l1b   = ast_block_new(intrin_ctx(intrin)); /* loop 1 body */
    ast_block    *l2b   = ast_block_new(intrin_ctx(intrin)); /* loop 2 body */
    ast_loop     *loop1 = NULL;
    ast_loop     *loop2 = NULL;
    ast_function *func  = intrin_value(intrin, &value, "pow", TYPE_FLOAT);

    /* arguments */
    vec_push(value->expression.params, arg1);
    vec_push(value->expression.params, arg2);

    /* local */
    vec_push(body->locals, local);

    /* assignment to local of value 1.0f */
    vec_push(body->exprs,
        (ast_expression*)ast_store_new (
            intrin_ctx(intrin),
            INSTR_STORE_F,
            (ast_expression*)local,
            (ast_expression*)intrin->fold->imm_float[1] /* 1 == 1.0f */
        )
    );

    /* y >>= 2 */
    vec_push(l2b->exprs,
        (ast_expression*)ast_binstore_new (
            intrin_ctx(intrin),
            INSTR_STORE_F,
            INSTR_MUL_F,
            (ast_expression*)arg2,
            (ast_expression*)fold_constgen_float(intrin->parser->fold, 0.25f)
        )
    );

    /* x *= x */
    vec_push(l2b->exprs,
        (ast_expression*)ast_binstore_new (
            intrin_ctx(intrin),
            INSTR_STORE_F,
            INSTR_MUL_F,
            (ast_expression*)arg1,
            (ast_expression*)arg1
        )
    );

    /* while (!(y&1)) */
    loop2 = ast_loop_new (
        intrin_ctx(intrin),
        NULL,
        (ast_expression*)ast_binary_new (
            intrin_ctx(intrin),
            INSTR_AND,
            (ast_expression*)arg2,
            (ast_expression*)intrin->fold->imm_float[1] /* 1 == 1.0f */
        ),
        true, /* ! not */
        NULL,
        false,
        NULL,
        (ast_expression*)l2b
    );

    /* push nested loop into loop expressions */
    vec_push(l1b->exprs, (ast_expression*)loop2);

    /* y-- */
    vec_push(l1b->exprs,
        (ast_expression*)ast_binstore_new (
            intrin_ctx(intrin),
            INSTR_STORE_F,
            INSTR_SUB_F,
            (ast_expression*)arg2,
            (ast_expression*)intrin->fold->imm_float[1] /* 1 == 1.0f */
        )
    );
    /* local *= x */
    vec_push(l1b->exprs,
        (ast_expression*)ast_binstore_new (
            intrin_ctx(intrin),
            INSTR_STORE_F,
            INSTR_MUL_F,
            (ast_expression*)local,
            (ast_expression*)arg1
        )
    );

    /* while (y > 0) */
    loop1 = ast_loop_new (
        intrin_ctx(intrin),
        NULL,
        (ast_expression*)ast_binary_new (
            intrin_ctx(intrin),
            INSTR_GT,
            (ast_expression*)arg2,
            (ast_expression*)intrin->fold->imm_float[0] /* 0 == 0.0f */
        ),
        false,
        NULL,
        false,
        NULL,
        (ast_expression*)l1b
    );

    /* push the loop1 into the body for the function */
    vec_push(body->exprs, (ast_expression*)loop1);

    /* return local; */
    vec_push(body->exprs,
        (ast_expression*)ast_return_new (
            intrin_ctx(intrin),
            (ast_expression*)local
        )
    );

    /* push block and register intrin for codegen */
    vec_push(func->blocks, body);

    intrin_reg(intrin, value, func);

    return (ast_expression*)value;
}

static ast_expression *intrin_mod(intrin_t *intrin) {
    /*
     * float mod(float a, float b) {
     *     float div = a / b;
     *     float sign = (div < 0.0f) ? -1 : 1;
     *     return a - b * sign * floor(sign * div);
     * }
     */
    ast_value    *value = NULL;
    ast_call     *call  = ast_call_new (intrin_ctx(intrin), intrin_func(intrin, "floor"));
    ast_value    *a     = ast_value_new(intrin_ctx(intrin), "a",    TYPE_FLOAT);
    ast_value    *b     = ast_value_new(intrin_ctx(intrin), "b",    TYPE_FLOAT);
    ast_value    *div   = ast_value_new(intrin_ctx(intrin), "div",  TYPE_FLOAT);
    ast_value    *sign  = ast_value_new(intrin_ctx(intrin), "sign", TYPE_FLOAT);
    ast_block    *body  = ast_block_new(intrin_ctx(intrin));
    ast_function *func  = intrin_value(intrin, &value, "mod", TYPE_FLOAT);

    vec_push(value->expression.params, a);
    vec_push(value->expression.params, b);

    vec_push(body->locals, div);
    vec_push(body->locals, sign);

    /* div = a / b; */
    vec_push(body->exprs,
        (ast_expression*)ast_store_new(
            intrin_ctx(intrin),
            INSTR_STORE_F,
            (ast_expression*)div,
            (ast_expression*)ast_binary_new(
                intrin_ctx(intrin),
                INSTR_DIV_F,
                (ast_expression*)a,
                (ast_expression*)b
            )
        )
    );

    /* sign = (div < 0.0f) ? -1 : 1; */
    vec_push(body->exprs,
        (ast_expression*)ast_store_new(
            intrin_ctx(intrin),
            INSTR_STORE_F,
            (ast_expression*)sign,
            (ast_expression*)ast_ternary_new(
                intrin_ctx(intrin),
                (ast_expression*)ast_binary_new(
                    intrin_ctx(intrin),
                    INSTR_LT,
                    (ast_expression*)div,
                    (ast_expression*)intrin->fold->imm_float[0]
                ),
                (ast_expression*)intrin->fold->imm_float[2],
                (ast_expression*)intrin->fold->imm_float[1]
            )
        )
    );

    /* floor(sign * div) */
    vec_push(call->params,
        (ast_expression*)ast_binary_new(
            intrin_ctx(intrin),
            INSTR_MUL_F,
            (ast_expression*)sign,
            (ast_expression*)div
        )
    );

    /* return a - b * sign * <call> */
    vec_push(body->exprs,
        (ast_expression*)ast_return_new(
            intrin_ctx(intrin),
            (ast_expression*)ast_binary_new(
                intrin_ctx(intrin),
                INSTR_SUB_F,
                (ast_expression*)a,
                (ast_expression*)ast_binary_new(
                    intrin_ctx(intrin),
                    INSTR_MUL_F,
                    (ast_expression*)b,
                    (ast_expression*)ast_binary_new(
                        intrin_ctx(intrin),
                        INSTR_MUL_F,
                        (ast_expression*)sign,
                        (ast_expression*)call
                    )
                )
            )
        )
    );

    vec_push(func->blocks, body); /* {{{ body }}} */
    intrin_reg(intrin, value, func);

    return (ast_expression*)value;
}

static ast_expression *intrin_exp(intrin_t *intrin) {
    /*
     * float exp(float x) {
     *     return pow(QC_M_E, x);
     * }
     */
    ast_value    *value = NULL;
    ast_call     *call  = ast_call_new (intrin_ctx(intrin), intrin_func(intrin, "pow"));
    ast_value    *arg1  = ast_value_new(intrin_ctx(intrin), "x", TYPE_FLOAT);
    ast_block    *body  = ast_block_new(intrin_ctx(intrin));
    ast_function *func  = intrin_value(intrin, &value, "exp", TYPE_FLOAT);

    /* push arguments for params to call */
    vec_push(call->params, (ast_expression*)fold_constgen_float(intrin->fold, QC_M_E));
    vec_push(call->params, (ast_expression*)arg1);

    /* return pow(QC_M_E, x) */
    vec_push(body->exprs,
        (ast_expression*)ast_return_new(
            intrin_ctx(intrin),
            (ast_expression*)call
        )
    );

    vec_push(value->expression.params, arg1); /* float x (for param) */

    vec_push(func->blocks,             body); /* {{{ body }}} */

    intrin_reg(intrin, value, func);

    return (ast_expression*)value;
}

static ast_expression *intrin_isnan(intrin_t *intrin) {
    /*
     * float isnan(float x) {
     *   float local;
     *   local = x;
     *
     *   return (x != local);
     * }
     */
    ast_value    *value  = NULL;
    ast_value    *arg1   = ast_value_new(intrin_ctx(intrin), "x",     TYPE_FLOAT);
    ast_value    *local  = ast_value_new(intrin_ctx(intrin), "local", TYPE_FLOAT);
    ast_block    *body   = ast_block_new(intrin_ctx(intrin));
    ast_function *func   = intrin_value(intrin, &value, "isnan", TYPE_FLOAT);

    vec_push(body->locals, local);
    vec_push(body->exprs,
        (ast_expression*)ast_store_new(
            intrin_ctx(intrin),
            INSTR_STORE_F,
            (ast_expression*)local,
            (ast_expression*)arg1
        )
    );

    vec_push(body->exprs,
        (ast_expression*)ast_return_new(
            intrin_ctx(intrin),
            (ast_expression*)ast_binary_new(
                intrin_ctx(intrin),
                INSTR_NE_F,
                (ast_expression*)arg1,
                (ast_expression*)local
            )
        )
    );

    vec_push(value->expression.params, arg1);
    vec_push(func->blocks, body);

    intrin_reg(intrin, value, func);

    return (ast_expression*)value;
}

static ast_expression *intrin_fabs(intrin_t *intrin) {
    /*
     * float fabs(float x) {
     *     return x < 0 ? -x : x;
     * }
     */
    ast_value    *value  = NULL;
    ast_value    *arg1   = ast_value_new(intrin_ctx(intrin), "x", TYPE_FLOAT);
    ast_block    *body   = ast_block_new(intrin_ctx(intrin));
    ast_function *func   = intrin_value(intrin, &value, "fabs", TYPE_FLOAT);

    vec_push(body->exprs,
        (ast_expression*)ast_return_new(
            intrin_ctx(intrin),
            (ast_expression*)ast_ternary_new(
                intrin_ctx(intrin),
                (ast_expression*)ast_binary_new(
                    intrin_ctx(intrin),
                    INSTR_LE,
                    (ast_expression*)arg1,
                    (ast_expression*)intrin->fold->imm_float[0]
                ),
                (ast_expression*)ast_binary_new(
                    intrin_ctx(intrin),
                    INSTR_SUB_F,
                    (ast_expression*)intrin->fold->imm_float[0],
                    (ast_expression*)arg1
                ),
                (ast_expression*)arg1
            )
        )
    );

    vec_push(value->expression.params, arg1);
    vec_push(func->blocks, body);

    intrin_reg(intrin, value, func);

    return (ast_expression*)value;
}

/*
 * TODO: make static (and handle ast_type_string) here for the builtin
 * instead of in SYA parse close.
 */
ast_expression *intrin_debug_typestring(intrin_t *intrin) {
    (void)intrin;
    return (ast_expression*)0x1;
}

static const intrin_func_t intrinsics[] = {
    {&intrin_exp,              "__builtin_exp",              "exp",   1},
    {&intrin_mod,              "__builtin_mod",              "mod",   2},
    {&intrin_pow,              "__builtin_pow",              "pow",   2},
    {&intrin_isnan,            "__builtin_isnan",            "isnan", 1},
    {&intrin_fabs,             "__builtin_fabs",             "fabs",  1},
    {&intrin_debug_typestring, "__builtin_debug_typestring", "",      0}
};

static void intrin_error(intrin_t *intrin, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vcompile_error(intrin->parser->lex->tok.ctx, fmt, ap);
    va_end(ap);
}

/* exposed */
intrin_t *intrin_init(parser_t *parser) {
    intrin_t *intrin = (intrin_t*)mem_a(sizeof(intrin_t));
    size_t    i;

    intrin->parser     = parser;
    intrin->fold       = parser->fold;
    intrin->intrinsics = NULL;
    intrin->generated  = NULL;

    vec_append(intrin->intrinsics, GMQCC_ARRAY_COUNT(intrinsics), intrinsics);

    /* populate with null pointers for tracking generation */
    for (i = 0; i < GMQCC_ARRAY_COUNT(intrinsics); i++)
        vec_push(intrin->generated, NULL);

    return intrin;
}

void intrin_cleanup(intrin_t *intrin) {
    vec_free(intrin->intrinsics);
    vec_free(intrin->generated);
    mem_d(intrin);
}

ast_expression *intrin_fold(intrin_t *intrin, ast_value *value, ast_expression **exprs) {
    size_t i;
    if (!value || !value->name)
        return NULL;
    for (i = 0; i < vec_size(intrin->intrinsics); i++)
        if (!strcmp(value->name, intrin->intrinsics[i].name))
            return (vec_size(exprs) != intrin->intrinsics[i].args)
                        ? NULL
                        : fold_intrin(intrin->fold, value->name + 10, exprs);
    return NULL;
}

static GMQCC_INLINE ast_expression *intrin_func_try(intrin_t *intrin, size_t offset, const char *compare) {
    size_t i;
    for (i = 0; i < vec_size(intrin->intrinsics); i++) {
        if (strcmp(*(char **)((char *)&intrin->intrinsics[i] + offset), compare))
            continue;
        if (intrin->generated[i])
            return intrin->generated[i];
        return intrin->generated[i] = intrin->intrinsics[i].intrin(intrin);
    }
    return NULL;
}

ast_expression *intrin_func(intrin_t *intrin, const char *name) {
    size_t           i;
    ast_expression  *find;

    /* try current first */
    if ((find = parser_find_global(intrin->parser, name)) && ((ast_value*)find)->expression.vtype == TYPE_FUNCTION)
        for (i = 0; i < vec_size(intrin->parser->functions); ++i)
            if (((ast_value*)find)->name && !strcmp(intrin->parser->functions[i]->name, ((ast_value*)find)->name) && intrin->parser->functions[i]->builtin < 0)
                return find;
    /* try name second */
    if ((find = intrin_func_try(intrin, offsetof(intrin_func_t, name),  name)))
        return find;
    /* try alias third */
    if ((find = intrin_func_try(intrin, offsetof(intrin_func_t, alias), name)))
        return find;

    intrin_error(intrin, "need function: `%s` compiler depends on it", name);
    return NULL;
}
