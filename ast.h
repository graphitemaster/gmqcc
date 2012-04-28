/*
 * Copyright (C) 2012 
 *     Wolfgang Bumiller
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
#ifndef GMQCC_AST_HDR
#define GMQCC_AST_HDR

#include "astir.h"
#include "ir.h"

/* Note: I will not be using a _t suffix for the
 * "main" ast node types for now.
 */

typedef union ast_node_u ast_node;
typedef union ast_expression_u ast_expression;

typedef struct ast_value_s      ast_value;
typedef struct ast_function_s   ast_function;
typedef struct ast_block_s      ast_block;
typedef struct ast_binary_s     ast_binary;
typedef struct ast_store_s      ast_store;

/* Node interface with common components
 */
typedef void ast_node_delete(ast_node*);
typedef struct
{
    lex_ctx_t        context;
    /* I don't feel comfortable using keywords like 'delete' as names... */
    ast_node_delete *destroy;
    /* keep: if a node contains this node, 'keep'
     * prevents its dtor from destroying this node as well.
     */
    bool             keep;
} ast_node_common;

#define ast_delete(x) ( ( (ast_node*)(x) ) -> node.destroy )((ast_node*)(x))
#define ast_unref(x) do                     \
{                                           \
    if (! (((ast_node*)(x))->node.keep) ) { \
        ast_delete(x);                      \
    }                                       \
} while(0)

/* Expression interface
 *
 * Any expression or block returns an ir_value, and needs
 * to know the current function.
 */
typedef bool ast_expression_codegen(ast_expression*,
                                    ast_function*,
                                    ir_value**);
typedef struct
{
    ast_node_common         node;
    ast_expression_codegen *codegen;
} ast_expression_common;

/* Value
 *
 * Types are also values, both have a type and a name.
 * especially considering possible constructs like typedefs.
 * typedef float foo;
 * is like creating a 'float foo', foo serving as the type's name.
 */
struct ast_value_s
{
    ast_expression_common expression;

    const char *name;

    int         vtype;
    ast_value  *next;

    bool isconst;
    union {
        double        vfloat;
        int           vint;
        vector_t      vvec;
        const char   *vstring;
        int           ventity;
        ast_function *vfunc;
    } constval;

    ir_value *ir_v;

    /* if vtype is qc_function, params contain parameters, and
     * 'next' the return type.
     */
    MEM_VECTOR_MAKE(ast_value*, params);
};
ast_value* ast_value_new(lex_ctx_t ctx, const char *name, int qctype, bool keep);
void ast_value_delete(ast_value*);

bool ast_value_set_name(ast_value*, const char *name);

bool ast_value_codegen(ast_value*, ast_function*, ir_value**);

/* Binary
 *
 * A value-returning binary expression.
 */
struct ast_binary_s
{
    ast_expression_common expression;

    int       op;
    ast_value *left;
    ast_value *right;
};
ast_binary* ast_binary_new(lex_ctx_t  ctx,
                           int        op,
                           ast_value *left,
                           ast_value *right);
void ast_binary_delete(ast_binary*);

/* hmm, seperate functions?
bool ast_block_codegen(ast_block*, ast_function*, ir_value**);
 */
bool ast_binary_codegen(ast_binary*, ast_function*, ir_value**);

/* Store
 *
 * Stores left<-right and returns left.
 * Specialized binary expression node
 */
struct ast_store_s
{
    ast_expression_common expression;
    int       op;
    ast_value *dest;
    ast_value *source;
};
ast_store* ast_store_new(lex_ctx_t ctx, int op,
                         ast_value *d, ast_value *s);
void ast_store_delete(ast_store*);

bool ast_store_codegen(ast_store*, ast_function*, ir_value**);

/* Blocks
 *
 */
struct ast_block_s
{
    ast_expression_common expression;

    MEM_VECTOR_MAKE(ast_value*,      locals);
    MEM_VECTOR_MAKE(ast_expression*, exprs);
};
ast_block* ast_block_new(lex_ctx_t ctx);
void ast_block_delete(ast_block*);

MEM_VECTOR_PROTO(ast_block, ast_value*, locals);
MEM_VECTOR_PROTO(ast_block, ast_expression*, exprs);

bool ast_block_codegen(ast_block*, ast_function*, ir_value**);

/* Function
 *
 * Contains a list of blocks... at least in theory.
 * Usually there's just the main block, other blocks are inside that.
 *
 * Technically, functions don't need to be an AST node, since we have
 * neither functions inside functions, nor lambdas, and function
 * pointers could just work with a name. However, this way could be
 * more flexible, and adds no real complexity.
 */
struct ast_function_s
{
    ast_node_common node;

    ast_value  *vtype;
    const char *name;

    MEM_VECTOR_MAKE(ast_block*, blocks);
};
ast_function* ast_function_new(lex_ctx_t ctx, const char *name, ast_value *vtype);
void ast_function_delete(ast_function*);

MEM_VECTOR_PROTO(ast_function, ast_block*, blocks);

bool ast_function_codegen(ast_function *self, ir_builder *builder);

/* Expression union
 */
union ast_expression_u
{
    ast_expression_common expression;

    ast_binary binary;
    ast_block  block;
};

/* Node union
 */
union ast_node_u
{
    ast_node_common node;
    ast_expression  expression;
};

#endif
