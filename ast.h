/*
 * Copyright (C) 2012, 2013
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

typedef struct ast_value_s       ast_value;
typedef struct ast_function_s    ast_function;
typedef struct ast_block_s       ast_block;
typedef struct ast_binary_s      ast_binary;
typedef struct ast_store_s       ast_store;
typedef struct ast_binstore_s    ast_binstore;
typedef struct ast_entfield_s    ast_entfield;
typedef struct ast_ifthen_s      ast_ifthen;
typedef struct ast_ternary_s     ast_ternary;
typedef struct ast_loop_s        ast_loop;
typedef struct ast_call_s        ast_call;
typedef struct ast_unary_s       ast_unary;
typedef struct ast_return_s      ast_return;
typedef struct ast_member_s      ast_member;
typedef struct ast_array_index_s ast_array_index;
typedef struct ast_breakcont_s   ast_breakcont;
typedef struct ast_switch_s      ast_switch;
typedef struct ast_label_s       ast_label;
typedef struct ast_goto_s        ast_goto;

enum {
    TYPE_ast_node,        /*  0 */
    TYPE_ast_expression,  /*  1 */
    TYPE_ast_value,       /*  2 */
    TYPE_ast_function,    /*  3 */
    TYPE_ast_block,       /*  4 */
    TYPE_ast_binary,      /*  5 */
    TYPE_ast_store,       /*  6 */
    TYPE_ast_binstore,    /*  7 */
    TYPE_ast_entfield,    /*  8 */
    TYPE_ast_ifthen,      /*  9 */
    TYPE_ast_ternary,     /* 10 */
    TYPE_ast_loop,        /* 11 */
    TYPE_ast_call,        /* 12 */
    TYPE_ast_unary,       /* 13 */
    TYPE_ast_return,      /* 14 */
    TYPE_ast_member,      /* 15 */
    TYPE_ast_array_index, /* 16 */
    TYPE_ast_breakcont,   /* 17 */
    TYPE_ast_switch,      /* 18 */
    TYPE_ast_label,       /* 19 */
    TYPE_ast_goto         /* 20 */
};

#define ast_istype(x, t) ( ((ast_node_common*)x)->nodetype == (TYPE_##t) )
#define ast_ctx(node) (((ast_node_common*)(node))->context)
#define ast_side_effects(node) (((ast_node_common*)(node))->side_effects)

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
    bool             side_effects;
} ast_node_common;

#define ast_delete(x) (*( ((ast_node*)(x))->node.destroy ))((ast_node*)(x))
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
/* TODO: the codegen function should take an output-type parameter
 * indicating whether a variable, type, label etc. is expected, and
 * an environment!
 * Then later an ast_ident could have a codegen using this to figure
 * out what to look for.
 * eg. in code which uses a not-yet defined variable, the expression
 * would take an ast_ident, and the codegen would be called with
 * type `expression`, so the ast_ident's codegen would search for
 * variables through the environment (or functions, constants...).
 */
typedef struct
{
    ast_node_common         node;
    ast_expression_codegen *codegen;
    int                     vtype;
    ast_expression         *next;
    /* arrays get a member-count */
    size_t                  count;
    ast_value*             *params;
    uint32_t                flags;
    /* void foo(string...) gets varparam set as a restriction
     * for variadic parameters
     */
    ast_expression         *varparam;
    /* The codegen functions should store their output values
     * so we can call it multiple times without re-evaluating.
     * Store lvalue and rvalue seperately though. So that
     * ast_entfield for example can generate both if required.
     */
    ir_value               *outl;
    ir_value               *outr;
} ast_expression_common;
#define AST_FLAG_VARIADIC     (1<<0)
#define AST_FLAG_NORETURN     (1<<1)
#define AST_FLAG_INLINE       (1<<2)
#define AST_FLAG_INITIALIZED  (1<<3)
#define AST_FLAG_DEPRECATED   (1<<4)
#define AST_FLAG_INCLUDE_DEF  (1<<5)
#define AST_FLAG_IS_VARARG    (1<<6)
#define AST_FLAG_ALIAS        (1<<7)
#define AST_FLAG_TYPE_MASK (AST_FLAG_VARIADIC | AST_FLAG_NORETURN)

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
    const char *desc;

    const char *argcounter;

    /*
    int         vtype;
    ast_value  *next;
    */

    int  cvq;     /* const/var qualifier */
    bool isfield; /* this declares a field */
    bool isimm;   /* an immediate, not just const */
    bool hasvalue;
    union {
        double        vfloat;
        int           vint;
        vector        vvec;
        const char   *vstring;
        int           ventity;
        ast_function *vfunc;
        ast_value    *vfield;
    } constval;

    /* usecount for the parser */
    size_t uses;

    ir_value *ir_v;
    ir_value **ir_values;
    size_t   ir_value_count;

    /* ONLY for arrays in progs version up to 6 */
    ast_value *setter;
    ast_value *getter;
};

ast_value* ast_value_new(lex_ctx ctx, const char *name, int qctype);
ast_value* ast_value_copy(const ast_value *self);
/* This will NOT delete an underlying ast_function */
void ast_value_delete(ast_value*);

bool ast_value_set_name(ast_value*, const char *name);

bool ast_value_codegen(ast_value*, ast_function*, bool lvalue, ir_value**);
bool ast_local_codegen(ast_value *self, ir_function *func, bool isparam);
bool ast_global_codegen(ast_value *self, ir_builder *ir, bool isfield);

void ast_value_params_add(ast_value*, ast_value*);

bool ast_compare_type(ast_expression *a, ast_expression *b);
ast_expression* ast_type_copy(lex_ctx ctx, const ast_expression *ex);
#define ast_type_adopt(a, b) ast_type_adopt_impl((ast_expression*)(a), (ast_expression*)(b))
void ast_type_adopt_impl(ast_expression *self, const ast_expression *other);
void ast_type_to_string(ast_expression *e, char *buf, size_t bufsize);

typedef enum ast_binary_ref_s {
    AST_REF_LEFT  = 1 << 1,
    AST_REF_RIGHT = 1 << 2,
    AST_REF_ALL   = (AST_REF_LEFT | AST_REF_RIGHT)
} ast_binary_ref;


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
    ast_binary_ref  refs;

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
    /* for &~= which uses the destination in a binary in source we can use this */
    bool            keep_dest;
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
ast_entfield* ast_entfield_new_force(lex_ctx ctx, ast_expression *entity, ast_expression *field, const ast_expression *outtype);
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
    const char     *name;
    bool            rvalue;
};
ast_member* ast_member_new(lex_ctx ctx, ast_expression *owner, unsigned int field, const char *name);
void ast_member_delete(ast_member*);
bool ast_member_set_name(ast_member*, const char *name);

bool ast_member_codegen(ast_member*, ast_function*, bool lvalue, ir_value**);

/* Array index access:
 *
 * QC forces us to take special action on arrays:
 * an ast_store on an ast_array_index must not codegen the index,
 * but call its setter - unless we have an instruction set which supports
 * what we need.
 * Any other array index access will be codegened to a call to the getter.
 * In any case, accessing an element via a compiletime-constant index will
 * result in quick access to that variable.
 */
struct ast_array_index_s
{
    ast_expression_common expression;
    ast_expression *array;
    ast_expression *index;
};
ast_array_index* ast_array_index_new(lex_ctx ctx, ast_expression *array, ast_expression *index);
void ast_array_index_delete(ast_array_index*);

bool ast_array_index_codegen(ast_array_index*, ast_function*, bool lvalue, ir_value**);

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
    /* For now we allow a seperate flag on whether or not the condition
     * is supposed to be true or false.
     * That way, the parser can generate a 'while not(!x)' for `while(x)`
     * if desired, which is useful for the new -f{true,false}-empty-strings
     * flag.
     */
    bool pre_not;
    bool post_not;
};
ast_loop* ast_loop_new(lex_ctx ctx,
                       ast_expression *initexpr,
                       ast_expression *precond, bool pre_not,
                       ast_expression *postcond, bool post_not,
                       ast_expression *increment,
                       ast_expression *body);
void ast_loop_delete(ast_loop*);

bool ast_loop_codegen(ast_loop*, ast_function*, bool lvalue, ir_value**);

/* Break/Continue
 */
struct ast_breakcont_s
{
    ast_expression_common expression;
    bool         is_continue;
    unsigned int levels;
};
ast_breakcont* ast_breakcont_new(lex_ctx ctx, bool iscont, unsigned int levels);
void ast_breakcont_delete(ast_breakcont*);

bool ast_breakcont_codegen(ast_breakcont*, ast_function*, bool lvalue, ir_value**);

/* Switch Statements
 *
 * A few notes about this: with the original QCVM, no real optimization
 * is possible. The SWITCH instruction set isn't really helping a lot, since
 * it only collapes the EQ and IF instructions into one.
 * Note: Declaring local variables inside caseblocks is normal.
 * Since we don't have to deal with a stack there's no unnatural behaviour to
 * be expected from it.
 * TODO: Ticket #20
 */
typedef struct {
    ast_expression *value; /* #20 will replace this */
    ast_expression *code;
} ast_switch_case;
struct ast_switch_s
{
    ast_expression_common expression;

    ast_expression  *operand;
    ast_switch_case *cases;
};

ast_switch* ast_switch_new(lex_ctx ctx, ast_expression *op);
void ast_switch_delete(ast_switch*);

bool ast_switch_codegen(ast_switch*, ast_function*, bool lvalue, ir_value**);

/* Label nodes
 *
 * Introduce a label which can be used together with 'goto'
 */
struct ast_label_s
{
    ast_expression_common expression;
    const char *name;
    ir_block   *irblock;
    ast_goto  **gotos;
    /* means it has not yet been defined */
    bool        undefined;
};

ast_label* ast_label_new(lex_ctx ctx, const char *name, bool undefined);
void ast_label_delete(ast_label*);
void ast_label_register_goto(ast_label*, ast_goto*);

bool ast_label_codegen(ast_label*, ast_function*, bool lvalue, ir_value**);

/* GOTO nodes
 *
 * Go to a label, the label node is filled in at a later point!
 */
struct ast_goto_s
{
    ast_expression_common expression;
    const char *name;
    ast_label  *target;
    ir_block   *irblock_from;
};

ast_goto* ast_goto_new(lex_ctx ctx, const char *name);
void ast_goto_delete(ast_goto*);
void ast_goto_set_label(ast_goto*, ast_label*);

bool ast_goto_codegen(ast_goto*, ast_function*, bool lvalue, ir_value**);

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
    ast_expression* *params;
    ast_expression *va_count;
};
ast_call* ast_call_new(lex_ctx ctx,
                       ast_expression *funcexpr);
void ast_call_delete(ast_call*);
bool ast_call_codegen(ast_call*, ast_function*, bool lvalue, ir_value**);
bool ast_call_check_types(ast_call*);

/* Blocks
 *
 */
struct ast_block_s
{
    ast_expression_common expression;

    ast_value*      *locals;
    ast_expression* *exprs;
    ast_expression* *collect;
};
ast_block* ast_block_new(lex_ctx ctx);
void ast_block_delete(ast_block*);
void ast_block_set_type(ast_block*, ast_expression *from);

bool ast_block_codegen(ast_block*, ast_function*, bool lvalue, ir_value**);
void ast_block_collect(ast_block*, ast_expression*);

bool GMQCC_WARN ast_block_add_expr(ast_block*, ast_expression*);

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
    ir_block    **breakblocks;
    ir_block    **continueblocks;

#if 0
    /* In order for early-out logic not to go over
     * excessive jumps, we remember their target
     * blocks...
     */
    ir_block    *iftrue;
    ir_block    *iffalse;
#endif

    size_t       labelcount;
    /* in order for thread safety - for the optional
     * channel abesed multithreading... keeping a buffer
     * here to use in ast_function_label.
     */
    char         labelbuf[64];

    ast_block* *blocks;

    ast_value   *varargs;
    ast_value   *argc;
    ast_value   *fixedparams;
};
ast_function* ast_function_new(lex_ctx ctx, const char *name, ast_value *vtype);
/* This will NOT delete the underlying ast_value */
void ast_function_delete(ast_function*);
/* For "optimized" builds this can just keep returning "foo"...
 * or whatever...
 */
const char* ast_function_label(ast_function*, const char *prefix);

bool ast_function_codegen(ast_function *self, ir_builder *builder);
bool ast_generate_accessors(ast_value *asvalue, ir_builder *ir);

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
