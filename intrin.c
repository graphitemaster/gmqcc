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

#define QC_M_E         2.718281828459045f
#define QC_POW_EPSILON 0.00001f

/*
 * since some intrinsics depend on each other there is the possibility
 * that an intrinsic will fail to get a 'depended' function that a
 * builtin needs, causing some dependency in the chain to have a NULL
 * function. This will cause a segmentation fault at code generation,
 * even though an error was raised. To contiue to allow it (instead
 * of stopping compilation right away). We need to return from the
 * parser, before compilation stops after all the collected errors.
 */
static ast_expression *intrin_func_self(intrin_t *intrin, const char *name, const char *from);
static ast_expression *intrin_nullfunc(intrin_t *intrin) {
    ast_value    *value = NULL;
    ast_function *func  = intrin_value(intrin, &value, NULL, TYPE_VOID);
    intrin_reg(intrin, value, func);
    return (ast_expression*)value;
}

static ast_expression *intrin_pow(intrin_t *intrin) {
    /*
     *
     * float pow(float base, float exp) {
     *     float result;
     *     float low;
     *     float high;
     *     float mid;
     *     float square;
     *     float accumulate;
     *
     *     if (exp == 0.0)
     *         return 1;
     *     if (exp == 1.0)
     *         return base;
     *     if (exp < 0)
     *         return 1.0 / pow(base, -exp);
     *     if (exp >= 1) {
     *         result = pow(base, exp / 2);
     *         return result * result;
     *     }
     *
     *     low        = 0.0f;
     *     high       = 1.0f;
     *     square     = sqrt(base);
     *     accumulate = square;
     *     mid        = high / 2.0f
     *
     *     while (fabs(mid - exp) > QC_POW_EPSILON) {
     *         square = sqrt(square);
     *         if (mid < exp) {
     *             low         = mid;
     *             accumulate *= square;
     *         } else {
     *             high        = mid;
     *             accumulate *= (1.0f / square);
     *         }
     *         mid = (low + high) / 2;
     *     }
     *     return accumulate;
     * }
     */
    ast_value    *value = NULL;
    ast_function *func = intrin_value(intrin, &value, "pow", TYPE_FLOAT);

    /* prepare some calls for later */
    ast_call *callpow1  = ast_call_new(intrin_ctx(intrin), (ast_expression*)value);                  /* for pow(base, -exp)    */
    ast_call *callpow2  = ast_call_new(intrin_ctx(intrin), (ast_expression*)value);                  /* for pow(vase, exp / 2) */
    ast_call *callsqrt1 = ast_call_new(intrin_ctx(intrin), intrin_func_self(intrin, "sqrt", "pow")); /* for sqrt(base)         */
    ast_call *callsqrt2 = ast_call_new(intrin_ctx(intrin), intrin_func_self(intrin, "sqrt", "pow")); /* for sqrt(square)       */
    ast_call *callfabs  = ast_call_new(intrin_ctx(intrin), intrin_func_self(intrin, "fabs", "pow")); /* for fabs(mid - exp)    */

    /* prepare some blocks for later */
    ast_block *expgt1       = ast_block_new(intrin_ctx(intrin));
    ast_block *midltexp     = ast_block_new(intrin_ctx(intrin));
    ast_block *midltexpelse = ast_block_new(intrin_ctx(intrin));
    ast_block *whileblock   = ast_block_new(intrin_ctx(intrin));

    /* float pow(float base, float exp) */
    ast_value    *base = ast_value_new(intrin_ctx(intrin), "base", TYPE_FLOAT);
    ast_value    *exp  = ast_value_new(intrin_ctx(intrin), "exp",  TYPE_FLOAT);
    /* { */
    ast_block    *body = ast_block_new(intrin_ctx(intrin));

    /*
     * float result;
     * float low;
     * float high;
     * float square;
     * float accumulate;
     * float mid;
     */
    ast_value *result     = ast_value_new(intrin_ctx(intrin), "result",     TYPE_FLOAT);
    ast_value *low        = ast_value_new(intrin_ctx(intrin), "low",        TYPE_FLOAT);
    ast_value *high       = ast_value_new(intrin_ctx(intrin), "high",       TYPE_FLOAT);
    ast_value *square     = ast_value_new(intrin_ctx(intrin), "square",     TYPE_FLOAT);
    ast_value *accumulate = ast_value_new(intrin_ctx(intrin), "accumulate", TYPE_FLOAT);
    ast_value *mid        = ast_value_new(intrin_ctx(intrin), "mid",        TYPE_FLOAT);
    vec_push(body->locals, result);
    vec_push(body->locals, low);
    vec_push(body->locals, high);
    vec_push(body->locals, square);
    vec_push(body->locals, accumulate);
    vec_push(body->locals, mid);

    vec_push(value->expression.params, base);
    vec_push(value->expression.params, exp);

    /*
     * if (exp == 0.0)
     *     return 1;
     */
    vec_push(body->exprs,
        (ast_expression*)ast_ifthen_new(
            intrin_ctx(intrin),
            (ast_expression*)ast_binary_new(
                intrin_ctx(intrin),
                INSTR_EQ_F,
                (ast_expression*)exp,
                (ast_expression*)intrin->fold->imm_float[0]
            ),
            (ast_expression*)ast_return_new(
                intrin_ctx(intrin),
                (ast_expression*)intrin->fold->imm_float[1]
            ),
            NULL
        )
    );

    /*
     * if (exp == 1.0)
     *     return base;
     */
    vec_push(body->exprs,
        (ast_expression*)ast_ifthen_new(
            intrin_ctx(intrin),
            (ast_expression*)ast_binary_new(
                intrin_ctx(intrin),
                INSTR_EQ_F,
                (ast_expression*)exp,
                (ast_expression*)intrin->fold->imm_float[1]
            ),
            (ast_expression*)ast_return_new(
                intrin_ctx(intrin),
                (ast_expression*)base
            ),
            NULL
        )
    );

    /* <callpow1> = pow(base, -exp) */
    vec_push(callpow1->params, (ast_expression*)base);
    vec_push(callpow1->params,
        (ast_expression*)ast_unary_new(
            intrin_ctx(intrin),
            VINSTR_NEG_F,
            (ast_expression*)exp
        )
    );

    /*
     * if (exp < 0)
     *     return 1.0 / <callpow1>;
     */
    vec_push(body->exprs,
        (ast_expression*)ast_ifthen_new(
            intrin_ctx(intrin),
            (ast_expression*)ast_binary_new(
                intrin_ctx(intrin),
                INSTR_LT,
                (ast_expression*)exp,
                (ast_expression*)intrin->fold->imm_float[0]
            ),
            (ast_expression*)ast_return_new(
                intrin_ctx(intrin),
                (ast_expression*)ast_binary_new(
                    intrin_ctx(intrin),
                    INSTR_DIV_F,
                    (ast_expression*)intrin->fold->imm_float[1],
                    (ast_expression*)callpow1
                )
            ),
            NULL
        )
    );

    /* <callpow2> = pow(base, exp / 2) */
    vec_push(callpow2->params, (ast_expression*)base);
    vec_push(callpow2->params,
        (ast_expression*)ast_binary_new(
            intrin_ctx(intrin),
            INSTR_DIV_F,
            (ast_expression*)exp,
            (ast_expression*)fold_constgen_float(intrin->fold, 2.0f)
        )
    );

    /*
     * <expgt1> = {
     *     result = <callpow2>;
     *     return result * result;
     * }
     */
    vec_push(expgt1->exprs,
        (ast_expression*)ast_store_new(
            intrin_ctx(intrin),
            INSTR_STORE_F,
            (ast_expression*)result,
            (ast_expression*)callpow2
        )
    );
    vec_push(expgt1->exprs,
        (ast_expression*)ast_return_new(
            intrin_ctx(intrin),
            (ast_expression*)ast_binary_new(
                intrin_ctx(intrin),
                INSTR_MUL_F,
                (ast_expression*)result,
                (ast_expression*)result
            )
        )
    );

    /*
     * if (exp >= 1) {
     *     <expgt1>
     * }
     */
    vec_push(body->exprs,
        (ast_expression*)ast_ifthen_new(
            intrin_ctx(intrin),
            (ast_expression*)ast_binary_new(
                intrin_ctx(intrin),
                INSTR_GE,
                (ast_expression*)exp,
                (ast_expression*)intrin->fold->imm_float[1]
            ),
            (ast_expression*)expgt1,
            NULL
        )
    );

    /*
     * <callsqrt1> = sqrt(base)
     */
    vec_push(callsqrt1->params, (ast_expression*)base);

    /*
     * low        = 0.0f;
     * high       = 1.0f;
     * square     = sqrt(base);
     * accumulate = square;
     * mid        = high / 2.0f;
     */
    vec_push(body->exprs,
        (ast_expression*)ast_store_new(intrin_ctx(intrin),
            INSTR_STORE_F,
            (ast_expression*)low,
            (ast_expression*)intrin->fold->imm_float[0]
        )
    );
    vec_push(body->exprs,
        (ast_expression*)ast_store_new(
            intrin_ctx(intrin),
            INSTR_STORE_F,
            (ast_expression*)high,
            (ast_expression*)intrin->fold->imm_float[1]
        )
    );
    vec_push(body->exprs,
        (ast_expression*)ast_store_new(
            intrin_ctx(intrin),
            INSTR_STORE_F,
            (ast_expression*)square,
            (ast_expression*)callsqrt1
        )
    );
    vec_push(body->exprs,
        (ast_expression*)ast_store_new(
            intrin_ctx(intrin),
            INSTR_STORE_F,
            (ast_expression*)accumulate,
            (ast_expression*)square
        )
    );
    vec_push(body->exprs,
        (ast_expression*)ast_store_new(
            intrin_ctx(intrin),
            INSTR_STORE_F,
            (ast_expression*)mid,
            (ast_expression*)ast_binary_new(
                intrin_ctx(intrin),
                INSTR_DIV_F,
                (ast_expression*)high,
                (ast_expression*)fold_constgen_float(intrin->fold, 2.0f)
            )
        )
    );

    /*
     * <midltexp> = {
     *     low         = mid;
     *     accumulate *= square;
     * }
     */
    vec_push(midltexp->exprs,
        (ast_expression*)ast_store_new(
            intrin_ctx(intrin),
            INSTR_STORE_F,
            (ast_expression*)low,
            (ast_expression*)mid
        )
    );
    vec_push(midltexp->exprs,
        (ast_expression*)ast_binstore_new(
            intrin_ctx(intrin),
            INSTR_STORE_F,
            INSTR_MUL_F,
            (ast_expression*)accumulate,
            (ast_expression*)square
        )
    );

    /*
     * <midltexpelse> = {
     *     high        = mid;
     *     accumulate *= (1.0 / square);
     * }
     */
    vec_push(midltexpelse->exprs,
        (ast_expression*)ast_store_new(
            intrin_ctx(intrin),
            INSTR_STORE_F,
            (ast_expression*)high,
            (ast_expression*)mid
        )
    );
    vec_push(midltexpelse->exprs,
        (ast_expression*)ast_binstore_new(
            intrin_ctx(intrin),
            INSTR_STORE_F,
            INSTR_MUL_F,
            (ast_expression*)accumulate,
            (ast_expression*)ast_binary_new(
                intrin_ctx(intrin),
                INSTR_DIV_F,
                (ast_expression*)intrin->fold->imm_float[1],
                (ast_expression*)square
            )
        )
    );

    /*
     * <callsqrt2> = sqrt(square)
     */
    vec_push(callsqrt2->params, (ast_expression*)square);

    /*
     * <whileblock> = {
     *     square = <callsqrt2>;
     *     if (mid < exp)
     *          <midltexp>;
     *     else
     *          <midltexpelse>;
     *
     *     mid = (low + high) / 2;
     * }
     */
    vec_push(whileblock->exprs,
        (ast_expression*)ast_store_new(
            intrin_ctx(intrin),
            INSTR_STORE_F,
            (ast_expression*)square,
            (ast_expression*)callsqrt2
        )
    );
    vec_push(whileblock->exprs,
        (ast_expression*)ast_ifthen_new(
            intrin_ctx(intrin),
            (ast_expression*)ast_binary_new(
                intrin_ctx(intrin),
                INSTR_LT,
                (ast_expression*)mid,
                (ast_expression*)exp
            ),
            (ast_expression*)midltexp,
            (ast_expression*)midltexpelse
        )
    );
    vec_push(whileblock->exprs,
        (ast_expression*)ast_store_new(
            intrin_ctx(intrin),
            INSTR_STORE_F,
            (ast_expression*)mid,
            (ast_expression*)ast_binary_new(
                intrin_ctx(intrin),
                INSTR_DIV_F,
                (ast_expression*)ast_binary_new(
                    intrin_ctx(intrin),
                    INSTR_ADD_F,
                    (ast_expression*)low,
                    (ast_expression*)high
                ),
                (ast_expression*)fold_constgen_float(intrin->fold, 2.0f)
            )
        )
    );

    /*
     * <callabs> = fabs(mid - exp)
     */
    vec_push(callfabs->params,
        (ast_expression*)ast_binary_new(
            intrin_ctx(intrin),
            INSTR_SUB_F,
            (ast_expression*)mid,
            (ast_expression*)exp
        )
    );

    /*
     * while (<callfabs>  > epsilon)
     *     <whileblock>
     */
    vec_push(body->exprs,
        (ast_expression*)ast_loop_new(
            intrin_ctx(intrin),
            /* init */
            NULL,
            /* pre condition */
            (ast_expression*)ast_binary_new(
                intrin_ctx(intrin),
                INSTR_GT,
                (ast_expression*)callfabs,
                (ast_expression*)fold_constgen_float(intrin->fold, QC_POW_EPSILON)
            ),
            /* pre not */
            false,
            /* post condition */
            NULL,
            /* post not */
            false,
            /* increment expression */
            NULL,
            /* code block */
            (ast_expression*)whileblock
        )
    );

    /* return accumulate */
    vec_push(body->exprs,
        (ast_expression*)ast_return_new(
            intrin_ctx(intrin),
            (ast_expression*)accumulate
        )
    );

    /* } */
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
    ast_call     *call  = ast_call_new (intrin_ctx(intrin), intrin_func_self(intrin, "floor", "mod"));
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
     *     // mul 10 to round increments of 0.1f
     *     return floor((pow(QC_M_E, x) * 10) + 0.5) / 10;
     * }
     */
    ast_value    *value     = NULL;
    ast_call     *callpow   = ast_call_new (intrin_ctx(intrin), intrin_func_self(intrin, "pow", "exp"));
    ast_call     *callfloor = ast_call_new (intrin_ctx(intrin), intrin_func_self(intrin, "floor", "exp"));
    ast_value    *arg1      = ast_value_new(intrin_ctx(intrin), "x", TYPE_FLOAT);
    ast_block    *body      = ast_block_new(intrin_ctx(intrin));
    ast_function *func      = intrin_value(intrin, &value, "exp", TYPE_FLOAT);

    vec_push(value->expression.params, arg1);

    vec_push(callpow->params, (ast_expression*)fold_constgen_float(intrin->fold, QC_M_E));
    vec_push(callpow->params, (ast_expression*)arg1);
    vec_push(callfloor->params,
        (ast_expression*)ast_binary_new(
            intrin_ctx(intrin),
            INSTR_ADD_F,
            (ast_expression*)ast_binary_new(
                intrin_ctx(intrin),
                INSTR_MUL_F,
                (ast_expression*)callpow,
                (ast_expression*)fold_constgen_float(intrin->fold, 10.0f)
            ),
            (ast_expression*)fold_constgen_float(intrin->fold, 0.5f)
        )
    );

    /* return <callfloor> / 10.0f */
    vec_push(body->exprs,
        (ast_expression*)ast_return_new(
            intrin_ctx(intrin),
            (ast_expression*)ast_binary_new(
                intrin_ctx(intrin),
                INSTR_DIV_F,
                (ast_expression*)callfloor,
                (ast_expression*)fold_constgen_float(intrin->fold, 10.0f)
            )
        )
    );

    vec_push(func->blocks, body); /* {{{ body }}} */

    intrin_reg(intrin, value, func);
    return (ast_expression*)value;
}

static ast_expression *intrin_exp2(intrin_t *intrin) {
    /*
     * float exp2(float x) {
     *     return pow(2, x);
     * }
     */
    ast_value    *value     = NULL;
    ast_call     *callpow   = ast_call_new (intrin_ctx(intrin), intrin_func_self(intrin, "pow", "exp2"));
    ast_value    *arg1      = ast_value_new(intrin_ctx(intrin), "x", TYPE_FLOAT);
    ast_block    *body      = ast_block_new(intrin_ctx(intrin));
    ast_function *func      = intrin_value(intrin, &value, "exp2", TYPE_FLOAT);

    vec_push(value->expression.params, arg1);

    vec_push(callpow->params, (ast_expression*)fold_constgen_float(intrin->fold, 2.0f));
    vec_push(callpow->params, (ast_expression*)arg1);

    /* return <callpow> */
    vec_push(body->exprs,
        (ast_expression*)ast_return_new(
            intrin_ctx(intrin),
            (ast_expression*)callpow
        )
    );

    vec_push(func->blocks, body);

    intrin_reg(intrin, value, func);
    return (ast_expression*)value;
}

static ast_expression *intrin_isinf(intrin_t *intrin) {
    /*
     * float isinf(float x) {
     *     return (x != 0.0) && (x + x == x);
     * }
     */
    ast_value    *value = NULL;
    ast_value    *x     = ast_value_new(intrin_ctx(intrin), "x", TYPE_FLOAT);
    ast_block    *body  = ast_block_new(intrin_ctx(intrin));
    ast_function *func  = intrin_value(intrin, &value, "isinf", TYPE_FLOAT);

    vec_push(body->exprs,
        (ast_expression*)ast_return_new(
            intrin_ctx(intrin),
            (ast_expression*)ast_binary_new(
                intrin_ctx(intrin),
                INSTR_AND,
                (ast_expression*)ast_binary_new(
                    intrin_ctx(intrin),
                    INSTR_NE_F,
                    (ast_expression*)x,
                    (ast_expression*)intrin->fold->imm_float[0]
                ),
                (ast_expression*)ast_binary_new(
                    intrin_ctx(intrin),
                    INSTR_EQ_F,
                    (ast_expression*)ast_binary_new(
                        intrin_ctx(intrin),
                        INSTR_ADD_F,
                        (ast_expression*)x,
                        (ast_expression*)x
                    ),
                    (ast_expression*)x
                )
            )
        )
    );

    vec_push(value->expression.params, x);
    vec_push(func->blocks, body);

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
                (ast_expression*)ast_unary_new(
                    intrin_ctx(intrin),
                    VINSTR_NEG_F,
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
    {&intrin_exp,              "__builtin_exp",              "exp",      1},
    {&intrin_exp2,             "__builtin_exp2",             "exp2",     1},
    {&intrin_mod,              "__builtin_mod",              "mod",      2},
    {&intrin_pow,              "__builtin_pow",              "pow",      2},
    {&intrin_isnan,            "__builtin_isnan",            "isnan",    1},
    {&intrin_isinf,            "__builtin_isinf",            "isinf",    1},
    {&intrin_fabs,             "__builtin_fabs",             "fabs",     1},
    {&intrin_debug_typestring, "__builtin_debug_typestring", "",         0},
    {&intrin_nullfunc,         "#nullfunc",                  "",         0}
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

static ast_expression *intrin_func_self(intrin_t *intrin, const char *name, const char *from) {
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

    if (from) {
        intrin_error(intrin, "need function `%s', compiler depends on it for `__builtin_%s'", name, from);
        return intrin_func_self(intrin, "#nullfunc", NULL);
    }
    return NULL;
}

ast_expression *intrin_func(intrin_t *intrin, const char *name) {
    return intrin_func_self(intrin, name, NULL);
}
