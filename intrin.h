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
    ast_expression *(*intrin)(parser_t *);
    const char       *name;
    const char       *alias;
} intrin_t;

static ht intrin_intrinsics(void) {
    static ht intrinsics = NULL;
    if (!intrinsics)
        intrinsics = util_htnew(PARSER_HT_SIZE);

    return intrinsics;
}

#define INTRIN_VAL(VALUE, NAME, FUNC, STYPE, VTYPE)                   \
    do {                                                              \
        (VALUE) = ast_value_new (                                     \
            parser_ctx(parser),                                       \
            "__builtin_" NAME,                                        \
            TYPE_FUNCTION                                             \
        );                                                            \
        (VALUE)->expression.next = (ast_expression*)ast_value_new (   \
            parser_ctx(parser),                                       \
            STYPE,                                                    \
            VTYPE                                                     \
        );                                                            \
        (FUNC) = ast_function_new (                                   \
            parser_ctx(parser),                                       \
            "__builtin_" NAME,                                        \
            (VALUE)                                                   \
        );                                                            \
    } while (0)

#define INTRIN_REG(FUNC, VALUE)                                       \
    do {                                                              \
        vec_push(parser->functions, (FUNC));                          \
        vec_push(parser->globals,   (ast_expression*)(VALUE));        \
    } while (0)

#define QC_M_E 2.71828182845905

static ast_expression *intrin_func(parser_t *parser, const char *name);
static ast_expression *intrin_pow (parser_t *parser) {
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
    static ast_value *value = NULL;

    if (!value) {
        ast_value    *arg1  = ast_value_new(parser_ctx(parser), "x",     TYPE_FLOAT);
        ast_value    *arg2  = ast_value_new(parser_ctx(parser), "y",     TYPE_FLOAT);
        ast_value    *local = ast_value_new(parser_ctx(parser), "local", TYPE_FLOAT);
        ast_block    *body  = ast_block_new(parser_ctx(parser));
        ast_block    *l1b   = ast_block_new(parser_ctx(parser)); /* loop 1 body */
        ast_block    *l2b   = ast_block_new(parser_ctx(parser)); /* looo 2 body */
        ast_loop     *loop1 = NULL;
        ast_loop     *loop2 = NULL;
        ast_function *func  = NULL;

        INTRIN_VAL(value, "pow", func, "<float>", TYPE_FLOAT);

        /* arguments */
        vec_push(value->expression.params, arg1);
        vec_push(value->expression.params, arg2);

        /* local */
        vec_push(body->locals, local);

        /* assignment to local of value 1.0f */
        vec_push(body->exprs,
            (ast_expression*)ast_store_new (
                parser_ctx(parser),
                INSTR_STORE_F,
                (ast_expression*)local,
                (ast_expression*)parser->fold->imm_float[1] /* 1 == 1.0f */
            )
        );

        /* y >>= 2 */
        vec_push(l2b->exprs,
            (ast_expression*)ast_binstore_new (
                parser_ctx(parser),
                INSTR_STORE_F,
                INSTR_MUL_F,
                (ast_expression*)arg2,
                (ast_expression*)fold_constgen_float(parser->fold, 0.25f)
            )
        );

        /* x *= x */
        vec_push(l2b->exprs,
            (ast_expression*)ast_binstore_new (
                parser_ctx(parser),
                INSTR_STORE_F,
                INSTR_MUL_F,
                (ast_expression*)arg1,
                (ast_expression*)arg1
            )
        );

        /* while (!(y&1)) */
        loop2 = ast_loop_new (
            parser_ctx(parser),
            NULL,
            (ast_expression*)ast_binary_new (
                parser_ctx(parser),
                INSTR_AND,
                (ast_expression*)arg2,
                (ast_expression*)parser->fold->imm_float[1] /* 1 == 1.0f */
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
                parser_ctx(parser),
                INSTR_STORE_F,
                INSTR_SUB_F,
                (ast_expression*)arg2,
                (ast_expression*)parser->fold->imm_float[1] /* 1 == 1.0f */
            )
        );
        /* local *= x */
        vec_push(l1b->exprs,
            (ast_expression*)ast_binstore_new (
                parser_ctx(parser),
                INSTR_STORE_F,
                INSTR_MUL_F,
                (ast_expression*)local,
                (ast_expression*)arg1
            )
        );

        /* while (y > 0) */
        loop1 = ast_loop_new (
            parser_ctx(parser),
            NULL,
            (ast_expression*)ast_binary_new (
                parser_ctx(parser),
                INSTR_GT,
                (ast_expression*)arg2,
                (ast_expression*)parser->fold->imm_float[0] /* 0 == 0.0f */
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
                parser_ctx(parser),
                (ast_expression*)local
            )
        );

        /* push block and register intrin for codegen */
        vec_push(func->blocks, body);

        INTRIN_REG(func, value);
    }

    return (ast_expression*)value;
}

static ast_expression *intrin_mod(parser_t *parser) {
    /*
     * float mod(float x, float y) {
     *   return x - y * floor(x / y);
     * }
     */
    static ast_value *value = NULL;

    if (!value) {
        ast_call     *call  = ast_call_new (parser_ctx(parser), intrin_func(parser, "floor"));
        ast_value    *arg1  = ast_value_new(parser_ctx(parser), "x", TYPE_FLOAT);
        ast_value    *arg2  = ast_value_new(parser_ctx(parser), "y", TYPE_FLOAT);
        ast_block    *body  = ast_block_new(parser_ctx(parser));
        ast_function *func  = NULL;

        INTRIN_VAL(value, "mod", func, "<float>", TYPE_FLOAT);

        /* floor(x/y) */
        vec_push(call->params,
            (ast_expression*)ast_binary_new (
                parser_ctx(parser),
                INSTR_DIV_F,
                (ast_expression*)arg1,
                (ast_expression*)arg2
            )
        );

        vec_push(body->exprs,
            (ast_expression*)ast_return_new(
                parser_ctx(parser),
                (ast_expression*)ast_binary_new(
                    parser_ctx(parser),
                    INSTR_SUB_F,
                    (ast_expression*)arg1,
                    (ast_expression*)ast_binary_new(
                        parser_ctx(parser),
                        INSTR_MUL_F,
                        (ast_expression*)arg2,
                        (ast_expression*)call
                    )
                )
            )
        );

        vec_push(value->expression.params, arg1); /* float x (for param) */
        vec_push(value->expression.params, arg2); /* float y (for param) */

        vec_push(func->blocks,            body); /* {{{ body }}} */

        INTRIN_REG(func, value);
    }

    return (ast_expression*)value;
}

static ast_expression *intrin_exp(parser_t *parser) {
    /*
     * float exp(float x) {
     *     return pow(QC_M_E, x);
     * }
     */
    static ast_value *value = NULL;

    if (!value) {
        ast_call     *call = ast_call_new    (parser_ctx(parser), intrin_func(parser, "pow"));
        ast_value    *arg1 = ast_value_new   (parser_ctx(parser), "x", TYPE_FLOAT);
        ast_block    *body = ast_block_new   (parser_ctx(parser));
        ast_function *func = NULL;

        INTRIN_VAL(value, "exp", func, "<float>", TYPE_FLOAT);

        /* push arguments for params to call */
        vec_push(call->params, (ast_expression*)fold_constgen_float(parser->fold, QC_M_E));
        vec_push(call->params, (ast_expression*)arg1);

        /* return pow(QC_M_E, x) */
        vec_push(body->exprs,
            (ast_expression*)ast_return_new(
                parser_ctx(parser),
                (ast_expression*)call
            )
        );

        vec_push(value->expression.params, arg1); /* float x (for param) */

        vec_push(func->blocks,             body); /* {{{ body }}} */

        INTRIN_REG(func, value);
    }

    return (ast_expression*)value;
}

static ast_expression *intrin_isnan(parser_t *parser) {
    /*
     * float isnan(float x) {
     *   float local;
     *   local = x;
     *
     *   return (x != local);
     * }
     */
    static ast_value *value = NULL;

    if (!value) {
        ast_value    *arg1   = ast_value_new (parser_ctx(parser), "x", TYPE_FLOAT);
        ast_value    *local  = ast_value_new (parser_ctx(parser), "local", TYPE_FLOAT);
        ast_block    *body   = ast_block_new (parser_ctx(parser));
        ast_function *func   = NULL;

        INTRIN_VAL(value, "isnan", func, "<float>", TYPE_FLOAT);

        vec_push(body->locals, local);
        vec_push(body->exprs,
            (ast_expression*)ast_store_new(
                parser_ctx(parser),
                INSTR_STORE_F,
                (ast_expression*)local,
                (ast_expression*)arg1
            )
        );

        vec_push(body->exprs,
            (ast_expression*)ast_return_new(
                parser_ctx(parser),
                (ast_expression*)ast_binary_new(
                    parser_ctx(parser),
                    INSTR_NE_F,
                    (ast_expression*)arg1,
                    (ast_expression*)local
                )
            )
        );

        vec_push(value->expression.params, arg1);

        vec_push(func->blocks, body);

        INTRIN_REG(func, value);
    }

    return (ast_expression*)value;
}

static intrin_t intrinsics[] = {
    {&intrin_exp,   "__builtin_exp",   "exp"},
    {&intrin_mod,   "__builtin_mod",   "mod"},
    {&intrin_pow,   "__builtin_pow",   "pow"},
    {&intrin_isnan, "__builtin_isnan", "isnan"}
};

void intrin_intrinsics_destroy(parser_t *parser) {
    /*size_t i;*/
    (void)parser;
    util_htdel(intrin_intrinsics());
#if 0
    for (i = 0; i < sizeof(intrinsics)/sizeof(intrin_t); i++)
        ast_value_delete( (ast_value*) intrinsics[i].intrin(parser));
#endif
}


static ast_expression *intrin_func(parser_t *parser, const char *name) {
    static bool  init = false;
    size_t       i    = 0;
    void        *find;

    /* register the intrinsics in the hashtable for O(1) lookup */
    if (!init) {
        for (i = 0; i < sizeof(intrinsics)/sizeof(*intrinsics); i++)
            util_htset(intrin_intrinsics(), intrinsics[i].alias, &intrinsics[i]);

        init = true; /* only once */
    }

    /*
     * jesus fucking christ, Blub design something less fucking
     * impossible to use, like a ast_is_builtin(ast_expression *), also
     * use a hashtable :P
     */
    if ((find = (void*)parser_find_global(parser, name)) && ((ast_value*)find)->expression.vtype == TYPE_FUNCTION)
        for (i = 0; i < vec_size(parser->functions); ++i)
            if (((ast_value*)find)->name && !strcmp(parser->functions[i]->name, ((ast_value*)find)->name) && parser->functions[i]->builtin < 0)
                return (ast_expression*)find;

    if ((find = util_htget(intrin_intrinsics(), name))) {
        /* intrinsic is in table. This will "generate the function" so
         * to speak (if it's not already generated).
         */
        return ((intrin_t*)find)->intrin(parser);
    }

    parseerror(parser, "need function: `%s` compiler depends on it", name);
    return NULL;
}
