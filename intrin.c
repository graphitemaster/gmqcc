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
#include "ast.h"


/*
 * Provides all the "intrinsics" / "builtins" for GMQCC. These can do
 * a few things, they can provide fall back implementations for math
 * functions if the definitions don't exist for some given engine. Or
 * then can determine definitions for existing builtins, and simply
 * wrap back to them instead.  This is like a "portable" intrface that
 * is entered when -fintrin is used (causing all existing builtins to
 * be ignored by the compiler and instead interface through here.
 */
typedef struct {
    ast_expression (*intrin)(parser_t *);
    const char      *name;
    const char      *alias;
} intrin_t;


/*
 * Some helper macros for generating def, doing ast_value from func, with
 * assignment, and a nice aggregate builder for generating the intrinsic
 * table at the bottom of this file, and for registering functions and
 * globals with the parser (so that they can be codegen'ed) 
 */   
#define INTRIN_IMP(NAME) \
    ast_expression intrin_##NAME (parser_t *parser)

/*
 * For intrinsics that are not to take precedence over a builtin, leave
 * ALIAS as an empty string.
 */  
#define INTRIN_DEF(NAME, ALIAS) \
    { &intrin_##NAME, #NAME, ALIAS }


#define INTRIN_VAL(NAME, FUNC, STYPE, VTYPE)                          \
    do {                                                              \
        (NAME) = ast_value_new (                                      \
            parser_ctx(parser),                                       \
            "__builtin_" #NAME,                                       \
            TYPE_FUNCTION                                             \
        );                                                            \
        (NAME)->expression.next = (ast_expression*)ast_value_new (    \
            parser_ctx(parser),                                       \
            STYPE,                                                    \
            VTYPE                                                     \
        );                                                            \
        (FUNC) = ast_function_new (                                   \
            parser_ctx(parser),                                       \
            "__builtin_" #NAME,                                       \
            (NAME)                                                    \
        );                                                            \
    } while (0)

#define INTRIN_REG(FUNC, VALUE)                                       \
    do {                                                              \
        vec_push(parser->functions, (FUNC));                          \
        vec_push(parser->globals,   (ast_expression*)(VALUE));        \
    } while (0)


typedef enum {
    QC_FP_NAN,
    QC_FP_INFINITE,
    QC_FP_ZERO,
    QC_FP_SUBNORMAL,
    QC_FP_NORMAL
} intrin_fp_t;


#if 0
/*
 * Implementation of intrinsics.  Each new intrinsic needs to be added
 * to the intrinsic table below.
 */  
INTRIN_IMP(isnan) {
    /*
     * float isnan(float x) {
     *     float y;
     *
     *     y = x;
     *     return (x != y);
     */      
}

INTRIN_IMP(isinf) {
    /*
     * float isinf(float x) {
     *   return (x != 0) && (x + x == x);
     * }
     */    
}

INTRIN_IMP(fpclassify) {
    /*
     * float __builtin_fpclassify(float x) {
     *     if (isnan(x)) {
     *         return QC_FP_NAN;
     *     }
     *     if (isinf(x)) {
     *         return QC_FP_INFINITE;
     *     }
     *     if (x == 0.0f) {
     *         return QC_FP_ZERO;
     *     }
     *     return QC_FP_NORMAL;
     * }
     */             
}
#endif


INTRIN_IMP(exp) {
    /*
     * float __builtin_exp(float x) {
     *     return __builtin_pow(QC_M_E, x);
     * }
     */
    static ast_value *value = NULL;

    if (!value) {
        ast_call     *call = ast_call_new    (parser_ctx(parser), intrin_func(parser, "pow"));
        ast_value    *arg1 = ast_value_new   (parser_ctx(parser), "x", TYPE_FLOAT);
        ast_block    *body = ast_block_new   (parser_ctx(parser));
        ast_function *func = NULL;

        INTRIN_VAL("exp", func, "<float>", TYPE_FLOAT);

        /* push arguments for params to call */
        vec_push(call->params, (ast_expression*)parser_const_float(parser, QC_M_E));
        vec_push(call->params, (ast_expression*)arg1);

        /* return pow(QC_M_E, x) */
        vec_push(body->exprs,
            (ast_expression*)ast_return_new(
                parser_ctx(parser),
                (ast_expression*)call
            )
        );

        vec_push(value->expressions.param, arg1); /* float x (for param) */
        vec_push(func ->blocks,            body); /* {{{ body }}} */

        INTRIN_REG(func, value);
    }

    return (ast_expression*)value;
}

#if 0
INTRIN_IMP(exp2) {
    /*
     * float __builtin_exp2(float x) {
     *     return __builin_pow(2, x);
     * }
     */ 
}

INTRIN_IMP(expm1) {
    /*
     * float __builtin_expm1(float x) {
     *     return __builtin_exp(x) - 1;
     * }
     */    
}
#endif

intrin_t intrin_intrinsics[] = {
    INTRIN_DEF(exp, "exp")
};
