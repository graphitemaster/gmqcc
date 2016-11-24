#ifndef GMQCC_AST_HDR
#define GMQCC_AST_HDR
#include <vector>
#include "ir.h"

typedef uint16_t ast_flag_t;

/* Note: I will not be using a _t suffix for the
 * "main" ast node types for now.
 */

struct ast_node;
struct ast_expression;
struct ast_value;
struct ast_function;
struct ast_block;
struct ast_binary;
struct ast_store;
struct ast_binstore;
struct ast_entfield;
struct ast_ifthen;
struct ast_ternary;
struct ast_loop;
struct ast_call;
struct ast_unary;
struct ast_return;
struct ast_member;
struct ast_array_index;
struct ast_breakcont;
struct ast_switch;
struct ast_label;
struct ast_goto;
struct ast_argpipe;
struct ast_state;

enum {
    AST_FLAG_VARIADIC       = 1 << 0,
    AST_FLAG_NORETURN       = 1 << 1,
    AST_FLAG_INLINE         = 1 << 2,
    AST_FLAG_INITIALIZED    = 1 << 3,
    AST_FLAG_DEPRECATED     = 1 << 4,
    AST_FLAG_INCLUDE_DEF    = 1 << 5,
    AST_FLAG_IS_VARARG      = 1 << 6,
    AST_FLAG_ALIAS          = 1 << 7,
    AST_FLAG_ERASEABLE      = 1 << 8,
    AST_FLAG_ACCUMULATE     = 1 << 9,

    /* An array declared as []
     * so that the size is taken from the initializer
     */
    AST_FLAG_ARRAY_INIT     = 1 << 10,

    AST_FLAG_FINAL_DECL     = 1 << 11,

    /* Several coverage options
     * AST_FLAG_COVERAGE means there was an explicit [[coverage]] attribute,
     * which will overwrite the default set via the commandline switches.
     * BLOCK_COVERAGE inserts coverage() calls into every basic block.
     * In the future there might be more options like tracking variable access
     * by creating get/set wrapper functions.
     */
    AST_FLAG_COVERAGE       = 1 << 12,
    AST_FLAG_BLOCK_COVERAGE = 1 << 13,

    /*
     * Propagates norefness to the IR so the unused (read/write) check can be
     * more intelligently done.
     */
    AST_FLAG_NOREF          = 1 << 14,

    AST_FLAG_LAST,
    AST_FLAG_TYPE_MASK      = (AST_FLAG_VARIADIC | AST_FLAG_NORETURN),
    AST_FLAG_COVERAGE_MASK  = (AST_FLAG_BLOCK_COVERAGE)
};

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
    TYPE_ast_goto,        /* 20 */
    TYPE_ast_argpipe,     /* 21 */
    TYPE_ast_state        /* 22 */
};

#define ast_istype(x, t) ( (x)->m_node_type == (TYPE_##t) )

struct ast_node
{
    ast_node() = delete;
    ast_node(lex_ctx_t, int nodetype);
    virtual ~ast_node();

    lex_ctx_t m_context;
    /* I don't feel comfortable using keywords like 'delete' as names... */
    int              m_node_type;
    /* keep_node: if a node contains this node, 'keep_node'
     * prevents its dtor from destroying this node as well.
     */
    bool             m_keep_node;
    bool             m_side_effects;

    void propagateSideEffects(const ast_node *other);
};

#define ast_unref(x) do        \
{                              \
    if (! (x)->m_keep_node ) { \
        delete (x);            \
    }                          \
} while(0)

enum class ast_copy_type_t { value };
static const ast_copy_type_t ast_copy_type = ast_copy_type_t::value;

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
struct ast_expression : ast_node {
    ast_expression() = delete;
    ast_expression(lex_ctx_t ctx, int nodetype, qc_type vtype);
    ast_expression(lex_ctx_t ctx, int nodetype);
    ~ast_expression();

    ast_expression(ast_copy_type_t, const ast_expression&);
    ast_expression(ast_copy_type_t, lex_ctx_t ctx, const ast_expression&);
    ast_expression(ast_copy_type_t, int nodetype, const ast_expression&);
    ast_expression(ast_copy_type_t, int nodetype, lex_ctx_t ctx, const ast_expression&);

    static ast_expression *shallowType(lex_ctx_t ctx, qc_type vtype);

    bool compareType(const ast_expression &other) const;
    void adoptType(const ast_expression &other);

    qc_type                 m_vtype = TYPE_VOID;
    ast_expression         *m_next = nullptr;
    /* arrays get a member-count */
    size_t                  m_count = 0;
    std::vector<std::unique_ptr<ast_value>> m_type_params;

    ast_flag_t              m_flags = 0;
    /* void foo(string...) gets varparam set as a restriction
     * for variadic parameters
     */
    ast_expression         *m_varparam = nullptr;
    /* The codegen functions should store their output values
     * so we can call it multiple times without re-evaluating.
     * Store lvalue and rvalue seperately though. So that
     * ast_entfield for example can generate both if required.
     */
    ir_value               *m_outl = nullptr;
    ir_value               *m_outr = nullptr;

    virtual bool codegen(ast_function *current, bool lvalue, ir_value **out);
};

/* Value
 *
 * Types are also values, both have a type and a name.
 * especially considering possible constructs like typedefs.
 * typedef float foo;
 * is like creating a 'float foo', foo serving as the type's name.
 */
union basic_value_t {
    qcfloat_t     vfloat;
    int           vint;
    vec3_t        vvec;
    const char   *vstring;
    int           ventity;
    ast_function *vfunc;
    ast_value    *vfield;
};

struct ast_value : ast_expression
{
    ast_value() = delete;
    ast_value(lex_ctx_t ctx, const std::string &name, qc_type qctype);
    ~ast_value();

    ast_value(ast_copy_type_t, const ast_expression&, const std::string&);
    ast_value(ast_copy_type_t, const ast_value&);
    ast_value(ast_copy_type_t, const ast_value&, const std::string&);

    bool codegen(ast_function *current, bool lvalue, ir_value **out) override;

    void addParam(ast_value*);

    bool generateGlobal(ir_builder*, bool isfield);
    bool generateLocal(ir_function*, bool param);
    bool generateAccessors(ir_builder*);

    std::string m_name;
    std::string m_desc;

    const char *m_argcounter = nullptr;

    int m_cvq = CV_NONE;     /* const/var qualifier */
    bool m_isfield = false; /* this declares a field */
    bool m_isimm = false;   /* an immediate, not just const */
    bool m_hasvalue = false;
    bool m_inexact = false; /* inexact coming from folded expression */
    basic_value_t m_constval;
    /* for TYPE_ARRAY we have an optional vector
     * of constants when an initializer list
     * was provided.
     */
    std::vector<basic_value_t> m_initlist;

    ir_value *m_ir_v = nullptr;
    std::vector<ir_value*> m_ir_values;
    size_t m_ir_value_count = 0;

    /* ONLY for arrays in progs version up to 6 */
    ast_value *m_setter = nullptr;
    ast_value *m_getter = nullptr;

    bool m_intrinsic = false; /* true if associated with intrinsic */

private:
    bool generateGlobalFunction(ir_builder*);
    bool generateGlobalField(ir_builder*);
    ir_value *prepareGlobalArray(ir_builder*);
    bool setGlobalArray();
    bool checkArray(const ast_value &array) const;
};

void ast_type_to_string(const ast_expression *e, char *buf, size_t bufsize);

enum ast_binary_ref {
    AST_REF_NONE  = 0,
    AST_REF_LEFT  = 1 << 1,
    AST_REF_RIGHT = 1 << 2,
    AST_REF_ALL   = (AST_REF_LEFT | AST_REF_RIGHT)
};


/* Binary
 *
 * A value-returning binary expression.
 */
struct ast_binary : ast_expression
{
    ast_binary() = delete;
    ast_binary(lex_ctx_t ctx, int op, ast_expression *l, ast_expression *r);
    ~ast_binary();

    bool codegen(ast_function *current, bool lvalue, ir_value **out) override;

    int m_op;
    ast_expression *m_left;
    ast_expression *m_right;
    ast_binary_ref m_refs;
    bool m_right_first;
};

/* Binstore
 *
 * An assignment including a binary expression with the source as left operand.
 * Eg. a += b; is a binstore { INSTR_STORE, INSTR_ADD, a, b }
 */
struct ast_binstore : ast_expression
{
    ast_binstore() = delete;
    ast_binstore(lex_ctx_t ctx, int storeop, int mathop, ast_expression *l, ast_expression *r);
    ~ast_binstore();

    bool codegen(ast_function *current, bool lvalue, ir_value **out) override;

    int m_opstore;
    int m_opbin;
    ast_expression *m_dest;
    ast_expression *m_source;
    /* for &~= which uses the destination in a binary in source we can use this */
    bool m_keep_dest;
};

/* Unary
 *
 * Regular unary expressions: not,neg
 */
struct ast_unary : ast_expression
{
    ast_unary() = delete;
    ~ast_unary();

    static ast_unary* make(lex_ctx_t ctx, int op, ast_expression *expr);

    bool codegen(ast_function *current, bool lvalue, ir_value **out) override;

    int m_op;
    ast_expression *m_operand;

private:
    ast_unary(lex_ctx_t ctx, int op, ast_expression *expr);
};

/* Return
 *
 * Make sure 'return' only happens at the end of a block, otherwise the IR
 * will refuse to create further instructions.
 * This should be honored by the parser.
 */
struct ast_return : ast_expression
{
    ast_return() = delete;
    ast_return(lex_ctx_t ctx, ast_expression *expr);
    ~ast_return();

    bool codegen(ast_function *current, bool lvalue, ir_value **out) override;

    ast_expression *m_operand;
};

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
struct ast_entfield : ast_expression
{
    ast_entfield() = delete;
    ast_entfield(lex_ctx_t ctx, ast_expression *entity, ast_expression *field);
    ast_entfield(lex_ctx_t ctx, ast_expression *entity, ast_expression *field, const ast_expression *outtype);
    ~ast_entfield();

    bool codegen(ast_function *current, bool lvalue, ir_value **out) override;

    // The entity can come from an expression of course.
    ast_expression *m_entity;
    // As can the field, it just must result in a value of TYPE_FIELD
    ast_expression *m_field;
};

/* Member access:
 *
 * For now used for vectors. If we get structs or unions
 * we can have them handled here as well.
 */
struct ast_member : ast_expression
{
    static ast_member *make(lex_ctx_t ctx, ast_expression *owner, unsigned int field, const std::string &name);
    ~ast_member();

    bool codegen(ast_function *current, bool lvalue, ir_value **out) override;

    ast_expression *m_owner;
    unsigned int m_field;
    std::string m_name;
    bool m_rvalue;

private:
    ast_member() = delete;
    ast_member(lex_ctx_t ctx, ast_expression *owner, unsigned int field, const std::string &name);
};

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
struct ast_array_index : ast_expression
{
    static ast_array_index* make(lex_ctx_t ctx, ast_expression *array, ast_expression *index);
    ~ast_array_index();

    bool codegen(ast_function *current, bool lvalue, ir_value **out) override;

    ast_expression *m_array;
    ast_expression *m_index;
private:
    ast_array_index() = delete;
    ast_array_index(lex_ctx_t ctx, ast_expression *array, ast_expression *index);
};

/* Vararg pipe node:
 *
 * copy all varargs starting from a specific index
 */
struct ast_argpipe : ast_expression
{
    ast_argpipe() = delete;
    ast_argpipe(lex_ctx_t ctx, ast_expression *index);

    bool codegen(ast_function *current, bool lvalue, ir_value **out) override;

    ~ast_argpipe();
    ast_expression *m_index;
};

/* Store
 *
 * Stores left<-right and returns left.
 * Specialized binary expression node
 */
struct ast_store : ast_expression
{
    ast_store() = delete;
    ast_store(lex_ctx_t ctx, int op, ast_expression *d, ast_expression *s);
    ~ast_store();

    bool codegen(ast_function *current, bool lvalue, ir_value **out) override;

    int m_op;
    ast_expression *m_dest;
    ast_expression *m_source;
};

/* If
 *
 * A general 'if then else' statement, either side can be nullptr and will
 * thus be omitted. It is an error for *both* cases to be nullptr at once.
 *
 * During its 'codegen' it'll be changing the ast_function's block.
 *
 * An if is also an "expression". Its codegen will put nullptr into the
 * output field though. For ternary expressions an ast_ternary will be
 * added.
 */
struct ast_ifthen : ast_expression
{
    ast_ifthen() = delete;
    ast_ifthen(lex_ctx_t ctx, ast_expression *cond, ast_expression *ontrue, ast_expression *onfalse);
    ~ast_ifthen();

    bool codegen(ast_function *current, bool lvalue, ir_value **out) override;

    ast_expression *m_cond;
    /* It's all just 'expressions', since an ast_block is one too. */
    ast_expression *m_on_true;
    ast_expression *m_on_false;
};

/* Ternary expressions...
 *
 * Contrary to 'if-then-else' nodes, ternary expressions actually
 * return a value, otherwise they behave the very same way.
 * The difference in 'codegen' is that it'll return the value of
 * a PHI node.
 *
 * The other difference is that in an ast_ternary, NEITHER side
 * must be nullptr, there's ALWAYS an else branch.
 *
 * This is the only ast_node beside ast_value which contains
 * an ir_value. Theoretically we don't need to remember it though.
 */
struct ast_ternary : ast_expression
{
    ast_ternary() = delete;
    ast_ternary(lex_ctx_t ctx, ast_expression *cond, ast_expression *ontrue, ast_expression *onfalse);
    ~ast_ternary();

    bool codegen(ast_function *current, bool lvalue, ir_value **out) override;

    ast_expression *m_cond;
    /* It's all just 'expressions', since an ast_block is one too. */
    ast_expression *m_on_true;
    ast_expression *m_on_false;
};

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
struct ast_loop : ast_expression
{
    ast_loop() = delete;
    ast_loop(lex_ctx_t ctx,
             ast_expression *initexpr,
             ast_expression *precond, bool pre_not,
             ast_expression *postcond, bool post_not,
             ast_expression *increment,
             ast_expression *body);
    ~ast_loop();

    bool codegen(ast_function *current, bool lvalue, ir_value **out) override;

    ast_expression *m_initexpr;
    ast_expression *m_precond;
    ast_expression *m_postcond;
    ast_expression *m_increment;
    ast_expression *m_body;
    /* For now we allow a seperate flag on whether or not the condition
     * is supposed to be true or false.
     * That way, the parser can generate a 'while not(!x)' for `while(x)`
     * if desired, which is useful for the new -f{true,false}-empty-strings
     * flag.
     */
    bool m_pre_not;
    bool m_post_not;
};

/* Break/Continue
 */
struct ast_breakcont : ast_expression
{
    ast_breakcont() = delete;
    ast_breakcont(lex_ctx_t ctx, bool iscont, unsigned int levels);
    ~ast_breakcont();

    bool codegen(ast_function *current, bool lvalue, ir_value **out) override;


    bool         m_is_continue;
    unsigned int m_levels;
};

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
struct ast_switch_case {
    ast_expression *m_value; /* #20 will replace this */
    ast_expression *m_code;
};

struct ast_switch : ast_expression
{
    ast_switch() = delete;
    ast_switch(lex_ctx_t ctx, ast_expression *op);
    ~ast_switch();

    bool codegen(ast_function *current, bool lvalue, ir_value **out) override;

    ast_expression *m_operand;
    std::vector<ast_switch_case> m_cases;
};


/* Label nodes
 *
 * Introduce a label which can be used together with 'goto'
 */
struct ast_label : ast_expression
{
    ast_label() = delete;
    ast_label(lex_ctx_t ctx, const std::string &name, bool undefined);
    ~ast_label();

    bool codegen(ast_function *current, bool lvalue, ir_value **out) override;

    std::string m_name;
    ir_block *m_irblock;
    std::vector<ast_goto*> m_gotos;

    /* means it has not yet been defined */
    bool m_undefined;

private:
    void registerGoto(ast_goto*);
    friend struct ast_goto;
};

/* GOTO nodes
 *
 * Go to a label, the label node is filled in at a later point!
 */
struct ast_goto : ast_expression
{
    ast_goto() = delete;
    ast_goto(lex_ctx_t ctx, const std::string &name);
    ~ast_goto();

    bool codegen(ast_function *current, bool lvalue, ir_value **out) override;

    void setLabel(ast_label*);

    std::string m_name;
    ast_label *m_target;
    ir_block *m_irblock_from;
};

/* STATE node
 *
 * For frame/think state updates: void foo() [framenum, nextthink] {}
 */
struct ast_state : ast_expression
{
    ast_state() = delete;
    ast_state(lex_ctx_t ctx, ast_expression *frame, ast_expression *think);
    ~ast_state();

    bool codegen(ast_function *current, bool lvalue, ir_value **out) override;

    ast_expression *m_framenum;
    ast_expression *m_nextthink;
};

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
struct ast_call : ast_expression
{
    ast_call() = delete;
    static ast_call *make(lex_ctx_t, ast_expression*);
    ~ast_call();

    bool codegen(ast_function *current, bool lvalue, ir_value **out) override;

    bool checkTypes(ast_expression *this_func_va_type) const;

    ast_expression *m_func;
    std::vector<ast_expression *> m_params;
    ast_expression *m_va_count;

private:
    ast_call(lex_ctx_t ctx, ast_expression *funcexpr);
    bool checkVararg(ast_expression *va_type, ast_expression *exp_type) const;
};

/* Blocks
 *
 */
struct ast_block : ast_expression
{
    ast_block() = delete;
    ast_block(lex_ctx_t ctx);
    ~ast_block();

    bool codegen(ast_function *current, bool lvalue, ir_value **out) override;

    std::vector<ast_value*>      m_locals;
    std::vector<ast_expression*> m_exprs;
    std::vector<ast_expression*> m_collect;

    void setType(const ast_expression &from);
    bool GMQCC_WARN addExpr(ast_expression*);
    void collect(ast_expression*);
};

/* Function
 *
 * Contains a list of blocks... at least in theory.
 * Usually there's just the main block, other blocks are inside that.
 *
 * Technically, functions don't need to be an AST node, since we have
 * neither functions inside functions, nor lambdas, and function
 * pointers could just work with a name. However, this way could be
 * more flexible, and adds no real complexity.
 *
 * The destructor will NOT delete the underlying ast_value
 *
 */
struct ast_function : ast_node
{
    ast_function() = delete;
    static ast_function *make(lex_ctx_t ctx, const std::string &name, ast_value *vtype);
    ~ast_function();

    const char* makeLabel(const char *prefix);
    virtual bool generateFunction(ir_builder*);

    ast_value  *m_function_type = nullptr;
    std::string m_name;

    int m_builtin = 0;

    /* list of used-up names for statics without the count suffix */
    std::vector<std::string> m_static_names;
    /* number of static variables, by convention this includes the
     * ones without the count-suffix - remember this when dealing
     * with savegames. uint instead of size_t as %zu in printf is
     * C99, so no windows support. */
    unsigned int m_static_count = 0;

    ir_function *m_ir_func = nullptr;
    ir_block *m_curblock = nullptr;
    std::vector<ir_block*> m_breakblocks;
    std::vector<ir_block*> m_continueblocks;

    size_t m_labelcount = 0;
    /* in order for thread safety - for the optional
     * channel abesed multithreading... keeping a buffer
     * here to use in ast_function_label.
     */
    std::vector<std::unique_ptr<ast_block>> m_blocks;
    std::unique_ptr<ast_value> m_varargs;
    std::unique_ptr<ast_value> m_argc;
    ast_value *m_fixedparams = nullptr; // these use unref()
    ast_value *m_return_value = nullptr;

private:
    ast_function(lex_ctx_t ctx, const std::string &name, ast_value *vtype);

    char m_labelbuf[64];
};

/*
 * If the condition creates a situation where this becomes -1 size it means there are
 * more AST_FLAGs than the type ast_flag_t is capable of holding. So either eliminate
 * the AST flag count or change the ast_flag_t typedef to a type large enough to accomodate
 * all the flags.
 */
typedef int static_assert_is_ast_flag_safe [((AST_FLAG_LAST) <= (ast_flag_t)(-1)) ? 1 : -1];
#endif
