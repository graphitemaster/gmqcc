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
typedef struct ast_binstore_s   ast_binstore;
typedef struct ast_entfield_s   ast_entfield;
typedef struct ast_ifthen_s     ast_ifthen;
typedef struct ast_ternary_s    ast_ternary;
typedef struct ast_loop_s       ast_loop;
typedef struct ast_call_s       ast_call;
typedef struct ast_unary_s      ast_unary;
typedef struct ast_return_s     ast_return;
typedef struct ast_member_s     ast_member;

enum {
    TYPE_ast_node,
    TYPE_ast_expression,
    TYPE_ast_value,
    TYPE_ast_function,
    TYPE_ast_block,
    TYPE_ast_binary,
    TYPE_ast_store,
    TYPE_ast_binstore,
    TYPE_ast_entfield,
    TYPE_ast_ifthen,
    TYPE_ast_ternary,
    TYPE_ast_loop,
    TYPE_ast_call,
    TYPE_ast_unary,
    TYPE_ast_return,
    TYPE_ast_member
};

#define ast_istype(x, t) ( ((ast_node_common*)x)->nodetype == (TYPE_##t) )
#define ast_ctx(node) (((ast_node_common*)(node))->context)

/* Node interface with common components
 */
typedef void ast_node_delete(ast_node*);
typedef struct
{
    lex_ctx          context;
    /* I don't feel comfortable using keywords like 'delete' as names... */
    ast_node_delete *destroy;
    int              nodetype;
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
    int                     vtype;
    ast_expression         *next;
    MEM_VECTOR_MAKE(ast_value*, params);
    bool                    variadic;
    /* The codegen functions should store their output values
     * so we can call it multiple times without re-evaluating.
     * Store lvalue and rvalue seperately though. So that
     * ast_entfield for example can generate both if required.
     */
    ir_value               *outl;
    ir_value               *outr;
} ast_expression_common;
MEM_VECTOR_PROTO(ast_expression_common, ast_value*, params);

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

    /*
    int         vtype;
    ast_value  *next;
    */

    bool isconst;
    union {
        double        vfloat;
        int           vint;
        vector        vvec;
        const char   *vstring;
        int           ventity;
        ast_function *vfunc;
    } constval;

    /* usecount for the parser */
    size_t uses;

    ir_value *ir_v;
};

ast_value* ast_value_new(lex_ctx ctx, const char *name, int qctype);
ast_value* ast_value_copy(const ast_value *self);
/* This will NOT delete an underlying ast_function */
void ast_value_delete(ast_value*);

bool ast_value_set_name(ast_value*, const char *name);

bool ast_value_codegen(ast_value*, ast_function*, bool lvalue, ir_value**);
bool ast_local_codegen(ast_value *self, ir_function *func, bool isparam);
bool ast_global_codegen(ast_value *self, ir_builder *ir);

bool GMQCC_WARN ast_value_params_add(ast_value*, ast_value*);

bool ast_compare_type(ast_expression *a, ast_expression *b);
ast_expression* ast_type_copy(lex_ctx ctx, const ast_expression *ex);
#define ast_type_adopt(a, b) ast_type_adopt_impl((ast_expression*)(a), (ast_expression*)(b))
bool ast_type_adopt_impl(ast_expression *self, const ast_expression *other);

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

/* Binstore
 *
 * An assignment including a binary expression with the source as left operand.
 * Eg. a += b; is a binstore { INSTR_STORE, INSTR_ADD, a, b }
 */
struct ast_binstore_s
{
    ast_expression_common expression;

    int             opstore;
    int             opbin;
    ast_expression *dest;
    ast_expression *source;
};
ast_binstore* ast_binstore_new(lex_ctx    ctx,
                               int        storeop,
                               int        op,
                               ast_expression *left,
                               ast_expression *right);
void ast_binstore_delete(ast_binstore*);

bool ast_binstore_codegen(ast_binstore*, ast_function*, bool lvalue, ir_value**);

/* Unary
 *
 * Regular unary expressions: not,neg
 */
struct ast_unary_s
{
    ast_expression_common expression;

    int             op;
    ast_expression *operand;
};
ast_unary* ast_unary_new(lex_ctx    ctx,
                         int        op,
                         ast_expression *expr);
void ast_unary_delete(ast_unary*);

bool ast_unary_codegen(ast_unary*, ast_function*, bool lvalue, ir_value**);

/* Return
 *
 * Make sure 'return' only happens at the end of a block, otherwise the IR
 * will refuse to create further instructions.
 * This should be honored by the parser.
 */
struct ast_return_s
{
    ast_expression_common expression;
    ast_expression *operand;
};
ast_return* ast_return_new(lex_ctx    ctx,
                           ast_expression *expr);
void ast_return_delete(ast_return*);

bool ast_return_codegen(ast_return*, ast_function*, bool lvalue, ir_value**);

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

/* Member access:
 *
 * For now used for vectors. If we get structs or unions
 * we can have them handled here as well.
 */
struct ast_member_s
{
    ast_expression_common expression;
    ast_expression *owner;
    unsigned int    field;
};
ast_member* ast_member_new(lex_ctx ctx, ast_expression *owner, unsigned int field);
void ast_member_delete(ast_member*);

bool ast_member_codegen(ast_member*, ast_function*, bool lvalue, ir_value**);

/* Store
 *
 * Stores left<-right and returns left.
 * Specialized binary expression node
 */
struct ast_store_s
{
    ast_expression_common expression;
    int             op;
    ast_expression *dest;
    ast_expression *source;
};
ast_store* ast_store_new(lex_ctx ctx, int op,
                         ast_expression *d, ast_expression *s);
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

/* A general loop node
 *
 * For convenience it contains 4 parts:
 * -) (ini) = initializing expression
 * -) (pre) = pre-loop condition
 * -) (pst) = post-loop condition
 * -) (inc) = "increment" expression
 * The following is a psudo-representation of this loop
 * note that '=>' bears the logical meaning of "implies".
 * (a => b) equals (!a || b)

{ini};
while (has_pre => {pre})
{
    {body};

continue:      // a 'continue' will jump here
    if (has_pst => {pst})
        break;

    {inc};
}
 */
struct ast_loop_s
{
    ast_expression_common expression;
    ast_expression *initexpr;
    ast_expression *precond;
    ast_expression *postcond;
    ast_expression *increment;
    ast_expression *body;
};
ast_loop* ast_loop_new(lex_ctx ctx,
                       ast_expression *initexpr,
                       ast_expression *precond,
                       ast_expression *postcond,
                       ast_expression *increment,
                       ast_expression *body);
void ast_loop_delete(ast_loop*);

bool ast_loop_codegen(ast_loop*, ast_function*, bool lvalue, ir_value**);

/* CALL node
 *
 * Contains an ast_expression as target, rather than an ast_function/value.
 * Since it's how QC works, every ast_function has an ast_value
 * associated anyway - in other words, the VM contains function
 * pointers for every function anyway. Thus, this node will call
 * expression.
 * Additionally it contains a list of ast_expressions as parameters.
 * Since calls can return values, an ast_call is also an ast_expression.
 */
struct ast_call_s
{
    ast_expression_common expression;
    ast_expression *func;
    MEM_VECTOR_MAKE(ast_expression*, params);
};
ast_call* ast_call_new(lex_ctx ctx,
                       ast_expression *funcexpr);
void ast_call_delete(ast_call*);
bool ast_call_codegen(ast_call*, ast_function*, bool lvalue, ir_value**);

MEM_VECTOR_PROTO(ast_call, ast_expression*, params);

/* Blocks
 *
 */
struct ast_block_s
{
    ast_expression_common expression;

    MEM_VECTOR_MAKE(ast_value*,      locals);
    MEM_VECTOR_MAKE(ast_expression*, exprs);
    MEM_VECTOR_MAKE(ast_expression*, collect);
};
ast_block* ast_block_new(lex_ctx ctx);
void ast_block_delete(ast_block*);
bool ast_block_set_type(ast_block*, ast_expression *from);

MEM_VECTOR_PROTO(ast_block, ast_value*, locals);
MEM_VECTOR_PROTO(ast_block, ast_expression*, exprs);
MEM_VECTOR_PROTO(ast_block, ast_expression*, collect);

bool ast_block_codegen(ast_block*, ast_function*, bool lvalue, ir_value**);
bool ast_block_collect(ast_block*, ast_expression*);

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

    int builtin;

    ir_function *ir_func;
    ir_block    *curblock;
    ir_block    *breakblock;
    ir_block    *continueblock;

    size_t       labelcount;
    /* in order for thread safety - for the optional
     * channel abesed multithreading... keeping a buffer
     * here to use in ast_function_label.
     */
    char         labelbuf[64];

    MEM_VECTOR_MAKE(ast_block*, blocks);
};
ast_function* ast_function_new(lex_ctx ctx, const char *name, ast_value *vtype);
/* This will NOT delete the underlying ast_value */
void ast_function_delete(ast_function*);
/* For "optimized" builds this can just keep returning "foo"...
 * or whatever...
 */
const char* ast_function_label(ast_function*, const char *prefix);

MEM_VECTOR_PROTO(ast_function, ast_block*, blocks);

bool ast_function_codegen(ast_function *self, ir_builder *builder);

/* Expression union
 */
union ast_expression_u
{
    ast_expression_common expression;

    ast_value    value;
    ast_binary   binary;
    ast_block    block;
    ast_ternary  ternary;
    ast_ifthen   ifthen;
    ast_store    store;
    ast_entfield entfield;
};

/* Node union
 */
union ast_node_u
{
    ast_node_common node;
    ast_expression  expression;
};

#endif
