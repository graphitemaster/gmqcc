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
typedef struct ast_entfield_s   ast_entfield;
typedef struct ast_ifthen_s     ast_ifthen;
typedef struct ast_ternary_s    ast_ternary;

/* Node interface with common components
 */
typedef void ast_node_delete(ast_node*);
typedef struct
{
    lex_ctx          context;
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
                                    bool lvalue,
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
        vector        vvec;
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
ast_value* ast_value_new(lex_ctx ctx, const char *name, int qctype);
/* This will NOT delete an underlying ast_function */
void ast_value_delete(ast_value*);

bool ast_value_set_name(ast_value*, const char *name);

bool ast_value_codegen(ast_value*, ast_function*, bool lvalue, ir_value**);

/* Binary
 *
 * A value-returning binary expression.
 */
struct ast_binary_s
{
    ast_expression_common expression;

    int             op;
    ast_expression *left;
    ast_expression *right;
};
ast_binary* ast_binary_new(lex_ctx    ctx,
                           int        op,
                           ast_expression *left,
                           ast_expression *right);
void ast_binary_delete(ast_binary*);

bool ast_binary_codegen(ast_binary*, ast_function*, bool lvalue, ir_value**);

/* Entity-field
 *
 * This must do 2 things:
 * -) Provide a way to fetch an entity field value. (Rvalue)
 * -) Provide a pointer to an entity field. (Lvalue)
 * The problem:
 * In original QC, there's only a STORE via pointer, but
 * no LOAD via pointer.
 * So we must know beforehand if we are going to read or assign
 * the field.
 * For this we will have to extend the codegen() functions with
 * a flag saying whether or not we need an L or an R-value.
 */
struct ast_entfield_s
{
    ast_expression_common expression;
    /* The entity can come from an expression of course. */
    ast_expression *entity;
    /* As can the field, it just must result in a value of TYPE_FIELD */
    ast_expression *field;
};
ast_entfield* ast_entfield_new(lex_ctx ctx, ast_expression *entity, ast_expression *field);
void ast_entfield_delete(ast_entfield*);

bool ast_entfield_codegen(ast_entfield*, ast_function*, bool lvalue, ir_value**);

/* Store
 *
 * Stores left<-right and returns left.
 * Specialized binary expression node
 */
struct ast_store_s
{
    ast_expression_common expression;
    int             op;
    ast_value      *dest; /* When we add pointers this might have to change to expression */
    ast_expression *source;
};
ast_store* ast_store_new(lex_ctx ctx, int op,
                         ast_value *d, ast_expression *s);
void ast_store_delete(ast_store*);

bool ast_store_codegen(ast_store*, ast_function*, bool lvalue, ir_value**);

/* If
 *
 * A general 'if then else' statement, either side can be NULL and will
 * thus be omitted. It is an error for *both* cases to be NULL at once.
 *
 * During its 'codegen' it'll be changing the ast_function's block.
 *
 * An if is also an "expression". Its codegen will put NULL into the
 * output field though. For ternary expressions an ast_ternary will be
 * added.
 */
struct ast_ifthen_s
{
    ast_expression_common expression;
    ast_expression *cond;
    /* It's all just 'expressions', since an ast_block is one too. */
    ast_expression *on_true;
    ast_expression *on_false;
};
ast_ifthen* ast_ifthen_new(lex_ctx ctx, ast_expression *cond, ast_expression *ontrue, ast_expression *onfalse);
void ast_ifthen_delete(ast_ifthen*);

bool ast_ifthen_codegen(ast_ifthen*, ast_function*, bool lvalue, ir_value**);

/* Ternary expressions...
 *
 * Contrary to 'if-then-else' nodes, ternary expressions actually
 * return a value, otherwise they behave the very same way.
 * The difference in 'codegen' is that it'll return the value of
 * a PHI node.
 *
 * The other difference is that in an ast_ternary, NEITHER side
 * must be NULL, there's ALWAYS an else branch.
 *
 * This is the only ast_node beside ast_value which contains
 * an ir_value. Theoretically we don't need to remember it though.
 */
struct ast_ternary_s
{
    ast_expression_common expression;
    ast_expression *cond;
    /* It's all just 'expressions', since an ast_block is one too. */
    ast_expression *on_true;
    ast_expression *on_false;
    /* After a ternary expression we find ourselves in a new IR block
     * and start with a PHI node */
    ir_value       *phi_out;
};
ast_ternary* ast_ternary_new(lex_ctx ctx, ast_expression *cond, ast_expression *ontrue, ast_expression *onfalse);
void ast_ternary_delete(ast_ternary*);

bool ast_ternary_codegen(ast_ternary*, ast_function*, bool lvalue, ir_value**);


/* Blocks
 *
 */
struct ast_block_s
{
    ast_expression_common expression;

    MEM_VECTOR_MAKE(ast_value*,      locals);
    MEM_VECTOR_MAKE(ast_expression*, exprs);
};
ast_block* ast_block_new(lex_ctx ctx);
void ast_block_delete(ast_block*);

MEM_VECTOR_PROTO(ast_block, ast_value*, locals);
MEM_VECTOR_PROTO(ast_block, ast_expression*, exprs);

bool ast_block_codegen(ast_block*, ast_function*, bool lvalue, ir_value**);

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

    ir_function *ir_func;

    MEM_VECTOR_MAKE(ast_block*, blocks);
};
ast_function* ast_function_new(lex_ctx ctx, const char *name, ast_value *vtype);
/* This will NOT delete the underlying ast_value */
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
