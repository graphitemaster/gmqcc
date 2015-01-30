#include <new>

#include <stdlib.h>
#include <string.h>

#include "gmqcc.h"
#include "ast.h"
#include "fold.h"
//#include "parser.h"

#include "algo.h"

#define ast_instantiate(T, ctx, destroyfn) \
    T* self = new T;                       \
    if (!self) return nullptr;             \
    ast_node_init(self, ctx, TYPE_##T);    \
    self->m_destroy = (ast_node_delete*)destroyfn

/*
 * forward declarations, these need not be in ast.h for obvious
 * static reasons.
 */
static bool ast_member_codegen(ast_member*, ast_function*, bool lvalue, ir_value**);
static void ast_array_index_delete(ast_array_index*);
static bool ast_array_index_codegen(ast_array_index*, ast_function*, bool lvalue, ir_value**);
static void ast_argpipe_delete(ast_argpipe*);
static bool ast_argpipe_codegen(ast_argpipe*, ast_function*, bool lvalue, ir_value**);
static void ast_store_delete(ast_store*);
static bool ast_store_codegen(ast_store*, ast_function*, bool lvalue, ir_value**);
static void ast_ifthen_delete(ast_ifthen*);
static bool ast_ifthen_codegen(ast_ifthen*, ast_function*, bool lvalue, ir_value**);
static void ast_ternary_delete(ast_ternary*);
static bool ast_ternary_codegen(ast_ternary*, ast_function*, bool lvalue, ir_value**);
static void ast_loop_delete(ast_loop*);
static bool ast_loop_codegen(ast_loop*, ast_function*, bool lvalue, ir_value**);
static void ast_breakcont_delete(ast_breakcont*);
static bool ast_breakcont_codegen(ast_breakcont*, ast_function*, bool lvalue, ir_value**);
static void ast_switch_delete(ast_switch*);
static bool ast_switch_codegen(ast_switch*, ast_function*, bool lvalue, ir_value**);
static void ast_label_delete(ast_label*);
static void ast_label_register_goto(ast_label*, ast_goto*);
static bool ast_label_codegen(ast_label*, ast_function*, bool lvalue, ir_value**);
static bool ast_goto_codegen(ast_goto*, ast_function*, bool lvalue, ir_value**);
static void ast_goto_delete(ast_goto*);
static void ast_call_delete(ast_call*);
static bool ast_call_codegen(ast_call*, ast_function*, bool lvalue, ir_value**);
static bool ast_block_codegen(ast_block*, ast_function*, bool lvalue, ir_value**);
static void ast_unary_delete(ast_unary*);
static bool ast_unary_codegen(ast_unary*, ast_function*, bool lvalue, ir_value**);
static void ast_entfield_delete(ast_entfield*);
static bool ast_entfield_codegen(ast_entfield*, ast_function*, bool lvalue, ir_value**);
static void ast_return_delete(ast_return*);
static bool ast_return_codegen(ast_return*, ast_function*, bool lvalue, ir_value**);
static void ast_binstore_delete(ast_binstore*);
static bool ast_binstore_codegen(ast_binstore*, ast_function*, bool lvalue, ir_value**);
static void ast_binary_delete(ast_binary*);
static bool ast_binary_codegen(ast_binary*, ast_function*, bool lvalue, ir_value**);
static bool ast_state_codegen(ast_state*, ast_function*, bool lvalue, ir_value**);

/* Initialize main ast node aprts */
ast_node::ast_node(lex_ctx_t ctx, int node_type)
    : m_context(ctx)
    , m_node_type(node_type)
    , m_keep_node(false)
    , m_side_effects(false)
{
}

ast_node::~ast_node()
{
}

/* weight and side effects */
void ast_node::propagate_side_effects(ast_node *other) const
{
    other->m_side_effects = m_side_effects;
}

/* General expression initialization */
ast_expression::ast_expression(lex_ctx_t ctx, int nodetype, qc_type type)
    : ast_node(ctx, nodetype)
    , m_vtype(type)
{
    if (OPTS_OPTION_BOOL(OPTION_COVERAGE))
        m_flags |= AST_FLAG_BLOCK_COVERAGE;
}
ast_expression::ast_expression(lex_ctx_t ctx, int nodetype)
    : ast_expression(ctx, nodetype, TYPE_VOID)
{}

ast_expression::~ast_expression()
{
    if (m_next)
        delete m_next;
    if (m_varparam)
        delete m_varparam;
}

ast_expression::ast_expression(ast_copy_type_t, int nodetype, const ast_expression &other)
    : ast_expression(other.m_context, nodetype)
{
    m_vtype = other.m_vtype;
    m_count = other.m_count;
    m_flags = other.m_flags;
    if (other.m_next)
        m_next = new ast_expression(ast_copy_type, TYPE_ast_expression, *other.m_next);
    m_type_params.reserve(other.m_type_params.size());
    for (auto &it : other.m_type_params)
        m_type_params.emplace_back(new ast_value(ast_copy_type, *it));
}

ast_expression::ast_expression(ast_copy_type_t, const ast_expression &other)
    : ast_expression(other.m_context, TYPE_ast_expression)
{}

ast_expression *ast_expression::shallow_type(lex_ctx_t ctx, qc_type vtype) {
    auto expr = new ast_expression(ctx, TYPE_ast_expression);
    expr->m_vtype = vtype;
    return expr;
}

void ast_expression::adopt_type(const ast_expression &other)
{
    m_vtype = other.m_vtype;
    if (other.m_next)
        m_next = new ast_expression(ast_copy_type, TYPE_ast_expression, *other.m_next);
    m_count = other.m_count;
    m_flags = other.m_flags;
    m_type_params.clear();
    m_type_params.reserve(other.m_type_params.size());
    for (auto &it : other.m_type_params)
        m_type_params.emplace_back(new ast_value(ast_copy_type, *it));
}

bool ast_expression::compare_type(const ast_expression &other) const
{
    if (m_vtype == TYPE_NIL ||
        other.m_vtype == TYPE_NIL)
        return true;
    if (m_vtype != other.m_vtype)
        return false;
    if (!m_next != !other.m_next)
        return false;
    if (m_type_params.size() != other.m_type_params.size())
        return false;
    if ((m_flags & AST_FLAG_TYPE_MASK) !=
        (other.m_flags & AST_FLAG_TYPE_MASK) )
    {
        return false;
    }
    if (m_type_params.size()) {
        size_t i;
        for (i = 0; i < m_type_params.size(); ++i) {
            if (!m_type_params[i]->compare_type(*other.m_type_params[i]))
                return false;
        }
    }
    if (m_next)
        return m_next->compare_type(*other.m_next);
    return true;
}

ast_value::ast_value(ast_copy_type_t, const ast_value &other, const std::string &name)
    : ast_value(ast_copy_type, static_cast<const ast_expression&>(other), name)
{}

ast_value::ast_value(ast_copy_type_t, const ast_value &other)
    : ast_value(ast_copy_type, static_cast<const ast_expression&>(other), other.m_name)
{}

ast_value::ast_value(ast_copy_type_t, const ast_expression &other, const std::string &name)
    : ast_expression(ast_copy_type, other)
    , m_name(name)
{}

ast_value::ast_value(lex_ctx_t ctx, const std::string &name, qc_type t)
    : ast_expression(ctx, TYPE_ast_value, t)
    , m_name(name)
{
    m_keep_node = true; // keep values, always
    memset(&m_constval, 0, sizeof(m_constval));
}

ast_value::~ast_value()
{
    if (m_argcounter)
        mem_d((void*)m_argcounter);
    if (m_hasvalue) {
        switch (m_vtype)
        {
        case TYPE_STRING:
            mem_d((void*)m_constval.vstring);
            break;
        case TYPE_FUNCTION:
            // unlink us from the function node
            m_constval.vfunc->m_function_type = nullptr;
            break;
        // NOTE: delete function? currently collected in
        // the parser structure
        default:
            break;
        }
    }
    if (m_ir_values)
        mem_d(m_ir_values);

    // initlist imples an array which implies .next in the expression exists.
    if (m_initlist.size() && m_next->m_vtype == TYPE_STRING) {
        for (auto &it : m_initlist)
            if (it.vstring)
                mem_d(it.vstring);
    }
}

static size_t ast_type_to_string_impl(const ast_expression *e, char *buf, size_t bufsize, size_t pos)
{
    const char *typestr;
    size_t typelen;
    size_t i;

    if (!e) {
        if (pos + 6 >= bufsize)
            goto full;
        util_strncpy(buf + pos, "(null)", 6);
        return pos + 6;
    }

    if (pos + 1 >= bufsize)
        goto full;

    switch (e->m_vtype) {
        case TYPE_VARIANT:
            util_strncpy(buf + pos, "(variant)", 9);
            return pos + 9;

        case TYPE_FIELD:
            buf[pos++] = '.';
            return ast_type_to_string_impl(e->m_next, buf, bufsize, pos);

        case TYPE_POINTER:
            if (pos + 3 >= bufsize)
                goto full;
            buf[pos++] = '*';
            buf[pos++] = '(';
            pos = ast_type_to_string_impl(e->m_next, buf, bufsize, pos);
            if (pos + 1 >= bufsize)
                goto full;
            buf[pos++] = ')';
            return pos;

        case TYPE_FUNCTION:
            pos = ast_type_to_string_impl(e->m_next, buf, bufsize, pos);
            if (pos + 2 >= bufsize)
                goto full;
            if (e->m_type_params.empty()) {
                buf[pos++] = '(';
                buf[pos++] = ')';
                return pos;
            }
            buf[pos++] = '(';
            pos = ast_type_to_string_impl(e->m_type_params[0].get(), buf, bufsize, pos);
            for (i = 1; i < e->m_type_params.size(); ++i) {
                if (pos + 2 >= bufsize)
                    goto full;
                buf[pos++] = ',';
                buf[pos++] = ' ';
                pos = ast_type_to_string_impl(e->m_type_params[i].get(), buf, bufsize, pos);
            }
            if (pos + 1 >= bufsize)
                goto full;
            buf[pos++] = ')';
            return pos;

        case TYPE_ARRAY:
            pos = ast_type_to_string_impl(e->m_next, buf, bufsize, pos);
            if (pos + 1 >= bufsize)
                goto full;
            buf[pos++] = '[';
            pos += util_snprintf(buf + pos, bufsize - pos - 1, "%i", (int)e->m_count);
            if (pos + 1 >= bufsize)
                goto full;
            buf[pos++] = ']';
            return pos;

        default:
            typestr = type_name[e->m_vtype];
            typelen = strlen(typestr);
            if (pos + typelen >= bufsize)
                goto full;
            util_strncpy(buf + pos, typestr, typelen);
            return pos + typelen;
    }

full:
    buf[bufsize-3] = '.';
    buf[bufsize-2] = '.';
    buf[bufsize-1] = '.';
    return bufsize;
}

void ast_type_to_string(const ast_expression *e, char *buf, size_t bufsize)
{
    size_t pos = ast_type_to_string_impl(e, buf, bufsize-1, 0);
    buf[pos] = 0;
}

void ast_value::add_param(ast_value *p)
{
    m_type_params.emplace_back(p);
}

ast_binary::ast_binary(lex_ctx_t ctx, int op,
                       ast_expression* left, ast_expression* right)
    : ast_expression(ctx, TYPE_ast_binary)
    , m_op(op)
    // m_left/m_right happen after the peephole step right below
    , m_right_first(false)
{
    if (ast_istype(right, ast_unary) && OPTS_OPTIMIZATION(OPTIM_PEEPHOLE)) {
        ast_unary      *unary  = ((ast_unary*)right);
        ast_expression *normal = unary->m_operand;

        /* make a-(-b) => a + b */
        if (unary->m_op == VINSTR_NEG_F || unary->m_op == VINSTR_NEG_V) {
            if (op == INSTR_SUB_F) {
                op = INSTR_ADD_F;
                right = normal;
                ++opts_optimizationcount[OPTIM_PEEPHOLE];
            } else if (op == INSTR_SUB_V) {
                op = INSTR_ADD_V;
                right = normal;
                ++opts_optimizationcount[OPTIM_PEEPHOLE];
            }
        }
    }

    m_left = left;
    m_right = right;

    propagate_side_effects(left);
    propagate_side_effects(right);

    if (op >= INSTR_EQ_F && op <= INSTR_GT)
        m_vtype = TYPE_FLOAT;
    else if (op == INSTR_AND || op == INSTR_OR) {
        if (OPTS_FLAG(PERL_LOGIC))
            adopt_type(*right);
        else
            m_vtype = TYPE_FLOAT;
    }
    else if (op == INSTR_BITAND || op == INSTR_BITOR)
        m_vtype = TYPE_FLOAT;
    else if (op == INSTR_MUL_VF || op == INSTR_MUL_FV)
        m_vtype = TYPE_VECTOR;
    else if (op == INSTR_MUL_V)
        m_vtype = TYPE_FLOAT;
    else
        m_vtype = left->m_vtype;

    // references all
    m_refs = AST_REF_ALL;
}

ast_binary::~ast_binary()
{
    if (m_refs & AST_REF_LEFT)  ast_unref(m_left);
    if (m_refs & AST_REF_RIGHT) ast_unref(m_right);
}

ast_binstore::ast_binstore(lex_ctx_t ctx, int storop, int mathop,
                           ast_expression* left, ast_expression* right)
    : ast_expression(ctx, TYPE_ast_binstore)
    , m_opstore(storop)
    , m_opbin(mathop)
    , m_dest(left)
    , m_source(right)
    , m_keep_dest(false)
{
    m_side_effects = true;
    adopt_type(*left);
}

ast_binstore::~ast_binstore()
{
    if (!m_keep_dest)
        ast_unref(m_dest);
    ast_unref(m_source);
}

ast_unary* ast_unary::make(lex_ctx_t ctx, int op, ast_expression *expr)
{
    if (ast_istype(expr, ast_unary) && OPTS_OPTIMIZATION(OPTIM_PEEPHOLE)) {
        ast_unary *prev = (ast_unary*)((ast_unary*)expr)->m_operand;

        /* Handle for double negation */
        if (((ast_unary*)expr)->m_op == op)
            prev = (ast_unary*)((ast_unary*)expr)->m_operand;

        if (ast_istype(prev, ast_unary)) {
            ++opts_optimizationcount[OPTIM_PEEPHOLE];
            return prev;
        }
    }

    return new ast_unary(ctx, op, expr);
}

ast_unary::ast_unary(lex_ctx_t ctx, int op, ast_expression *expr)
    : ast_expression(ctx, TYPE_ast_unary)
    , m_op(op)
    , m_operand(expr)
{
    propagate_side_effects(expr);
    if ((op >= INSTR_NOT_F && op <= INSTR_NOT_FNC) || op == VINSTR_NEG_F) {
        m_vtype = TYPE_FLOAT;
    } else if (op == VINSTR_NEG_V) {
        m_vtype = TYPE_VECTOR;
    } else {
        compile_error(ctx, "cannot determine type of unary operation %s", util_instr_str[op]);
    }
}

ast_unary::~ast_unary()
{
    if (m_operand)
        ast_unref(m_operand);
}

ast_return::ast_return(lex_ctx_t ctx, ast_expression *expr)
    : ast_expression(ctx, TYPE_ast_return)
    , m_operand(expr)
{
    if (expr)
        propagate_side_effects(expr);
}

ast_return::~ast_return()
{
    if (m_operand)
        ast_unref(m_operand);
}

ast_entfield::ast_entfield(lex_ctx_t ctx, ast_expression *entity, ast_expression *field)
    : ast_entfield(ctx, entity, field, field->m_next)
{
    if (field->m_vtype != TYPE_FIELD)
        compile_error(ctx, "ast_entfield with expression not of type field");
}

ast_entfield::ast_entfield(lex_ctx_t ctx, ast_expression *entity, ast_expression *field, const ast_expression *outtype)
    : ast_expression(ctx, TYPE_ast_entfield)
    , m_entity(entity)
    , m_field(field)
{
    propagate_side_effects(m_entity);
    propagate_side_effects(m_field);

    if (!outtype) {
        compile_error(ctx, "ast_entfield: field has no type");
        m_vtype = TYPE_VOID;
    }
    else
        adopt_type(*outtype);
}

ast_entfield::~ast_entfield()
{
    ast_unref(m_entity);
    ast_unref(m_field);
}

ast_member *ast_member::make(lex_ctx_t ctx, ast_expression *owner, unsigned int field, const std::string &name)
{
    if (field >= 3) {
        compile_error(ctx, "ast_member: invalid field (>=3): %u", field);
        return nullptr;
    }
    if (owner->m_vtype != TYPE_VECTOR &&
        owner->m_vtype != TYPE_FIELD)
    {
        compile_error(ctx, "member-access on an invalid owner of type %s", type_name[owner->m_vtype]);
        return nullptr;
    }
    return new ast_member(ctx, owner, field, name);
}

ast_member::ast_member(lex_ctx_t ctx, ast_expression *owner, unsigned int field, const std::string &name)
    : ast_expression(ctx, TYPE_ast_member)
    , m_owner(owner)
    , m_field(field)
    , m_name(name)
    , m_rvalue(false)
{
    m_keep_node = true;

    if (m_owner->m_vtype == TYPE_VECTOR) {
        m_vtype = TYPE_FLOAT;
        m_next  = nullptr;
    } else {
        m_vtype = TYPE_FIELD;
        m_next = ast_shallow_type(ctx, TYPE_FLOAT);
    }

    propagate_side_effects(owner);
}

ast_member::~ast_member()
{
    // The owner is always an ast_value, which has .keep_node=true,
    // also: ast_members are usually deleted after the owner, thus
    // this will cause invalid access
        //ast_unref(self->m_owner);
    // once we allow (expression).x to access a vector-member, we need
    // to change this: preferably by creating an alternate ast node for this
    // purpose that is not garbage-collected.
}

ast_array_index* ast_array_index::make(lex_ctx_t ctx, ast_expression *array, ast_expression *index)
{
    ast_expression *outtype = array->m_next;
    if (!outtype) {
        // field has no type
        return nullptr;
    }

    return new ast_array_index(ctx, array, index);
}

ast_array_index::ast_array_index(lex_ctx_t ctx, ast_expression *array, ast_expression *index)
    : ast_expression(ctx, TYPE_ast_array_index)
    , m_array(array)
    , m_index(index)
{
    propagate_side_effects(array);
    propagate_side_effects(index);

    ast_expression *outtype = m_array->m_next;
    adopt_type(*outtype);

    if (array->m_vtype == TYPE_FIELD && outtype->m_vtype == TYPE_ARRAY) {
        // FIXME: investigate - this is not possible after adopt_type
        //if (m_vtype != TYPE_ARRAY) {
        //    compile_error(self->m_context, "array_index node on type");
        //    ast_array_index_delete(self);
        //    return nullptr;
        //}

        m_array = outtype;
        m_vtype = TYPE_FIELD;
    }
}

ast_array_index::~ast_array_index()
{
    if (m_array)
        ast_unref(m_array);
    if (m_index)
        ast_unref(m_index);
}

ast_argpipe::ast_argpipe(lex_ctx_t ctx, ast_expression *index)
    : ast_expression(ctx, TYPE_ast_argpipe)
    , m_index(index)
{
    m_vtype = TYPE_NOEXPR;
}

ast_argpipe::~ast_argpipe()
{
    if (m_index)
        ast_unref(m_index);
}

ast_store::ast_store(lex_ctx_t ctx, int op, ast_expression *dest, ast_expression *source)
    : ast_expression(ctx, TYPE_ast_store)
    , m_op(op)
    , m_dest(dest)
    , m_source(source)
{
    m_side_effects = true;
    adopt_type(*dest);
}

ast_store::~ast_store()
{
    ast_unref(m_dest);
    ast_unref(m_source);
}

ast_ifthen::ast_ifthen(lex_ctx_t ctx, ast_expression *cond, ast_expression *ontrue, ast_expression *onfalse)
    : ast_expression(ctx, TYPE_ast_ifthen)
    , m_cond(cond)
    , m_on_true(ontrue)
    , m_on_false(onfalse)
{
    propagate_side_effects(cond);
    if (ontrue)
        propagate_side_effects(ontrue);
    if (onfalse)
        propagate_side_effects(onfalse);
}

ast_ifthen::~ast_ifthen()
{
    ast_unref(m_cond);
    if (m_on_true)
        ast_unref(m_on_true);
    if (m_on_false)
        ast_unref(m_on_false);
}

ast_ternary::ast_ternary(lex_ctx_t ctx, ast_expression *cond, ast_expression *ontrue, ast_expression *onfalse)
    : ast_expression(ctx, TYPE_ast_ternary)
    , m_cond(cond)
    , m_on_true(ontrue)
    , m_on_false(onfalse)
{
    propagate_side_effects(cond);
    propagate_side_effects(ontrue);
    propagate_side_effects(onfalse);

    if (ontrue->m_vtype == TYPE_NIL)
        adopt_type(onfalse);
    else
        adopt_type(ontrue);
}

ast_ternary::~ast_ternary()
{
    /* the if()s are only there because computed-gotos can set them
     * to nullptr
     */
    if (m_cond)     ast_unref(m_cond);
    if (m_on_true)  ast_unref(m_on_true);
    if (m_on_false) ast_unref(m_on_false);
}

ast_loop::ast_loop(lex_ctx_t ctx,
                   ast_expression *initexpr,
                   ast_expression *precond, bool pre_not,
                   ast_expression *postcond, bool post_not,
                   ast_expression *increment,
                   ast_expression *body)
    , ast_expression(ctx, TYPE_ast_loop)
    , m_initexpr(initexpr)
    , m_precond(precond)
    , m_postcond(postcond)
    , m_increment(increment)
    , m_body(body)
    , m_pre_not(pre_not)
    , m_post_not(post_not)
{
    if (initexpr)
        propagate_side_effects(initexpr);
    if (precond)
        propagate_side_effects(precond);
    if (postcond)
        propagate_side_effects(postcond);
    if (increment)
        propagate_side_effects(increment);
    if (body)
        propagate_side_effects(body);
}

ast_loop::~ast_loop()
{
    if (m_initexpr)
        ast_unref(m_initexpr);
    if (m_precond)
        ast_unref(m_precond);
    if (m_postcond)
        ast_unref(m_postcond);
    if (m_increment)
        ast_unref(m_increment);
    if (m_body)
        ast_unref(m_body);
}

ast_breakcont* ast_breakcont_new(lex_ctx_t ctx, bool iscont, unsigned int levels)
{
    ast_instantiate(ast_breakcont, ctx, ast_breakcont_delete);
    ast_expression_init((ast_expression*)self, (ast_expression_codegen*)&ast_breakcont_codegen);

    self->m_is_continue = iscont;
    self->m_levels      = levels;

    return self;
}

void ast_breakcont_delete(ast_breakcont *self)
{
    ast_expression_delete((ast_expression*)self);
    self->~ast_breakcont();
    mem_d(self);
}

ast_switch* ast_switch_new(lex_ctx_t ctx, ast_expression *op)
{
    ast_instantiate(ast_switch, ctx, ast_switch_delete);
    ast_expression_init((ast_expression*)self, (ast_expression_codegen*)&ast_switch_codegen);

    self->m_operand = op;

    self->propagate_side_effects(op);

    return self;
}

void ast_switch_delete(ast_switch *self)
{
    ast_unref(self->m_operand);

    for (auto &it : self->m_cases) {
        if (it.m_value)
            ast_unref(it.m_value);
        ast_unref(it.m_code);
    }

    ast_expression_delete((ast_expression*)self);
    self->~ast_switch();
    mem_d(self);
}

ast_label* ast_label_new(lex_ctx_t ctx, const char *name, bool undefined)
{
    ast_instantiate(ast_label, ctx, ast_label_delete);
    ast_expression_init((ast_expression*)self, (ast_expression_codegen*)&ast_label_codegen);

    self->m_vtype = TYPE_NOEXPR;

    self->m_name      = util_strdup(name);
    self->m_irblock   = nullptr;
    self->m_undefined = undefined;

    return self;
}

void ast_label_delete(ast_label *self)
{
    mem_d((void*)self->m_name);
    ast_expression_delete((ast_expression*)self);
    self->~ast_label();
    mem_d(self);
}

static void ast_label_register_goto(ast_label *self, ast_goto *g)
{
   self->m_gotos.push_back(g);
}

ast_goto* ast_goto_new(lex_ctx_t ctx, const char *name)
{
    ast_instantiate(ast_goto, ctx, ast_goto_delete);
    ast_expression_init((ast_expression*)self, (ast_expression_codegen*)&ast_goto_codegen);

    self->m_name    = util_strdup(name);
    self->m_target  = nullptr;
    self->m_irblock_from = nullptr;

    return self;
}

void ast_goto_delete(ast_goto *self)
{
    mem_d((void*)self->m_name);
    ast_expression_delete((ast_expression*)self);
    self->~ast_goto();
    mem_d(self);
}

void ast_goto_set_label(ast_goto *self, ast_label *label)
{
    self->m_target = label;
}

ast_state* ast_state_new(lex_ctx_t ctx, ast_expression *frame, ast_expression *think)
{
    ast_instantiate(ast_state, ctx, ast_state_delete);
    ast_expression_init((ast_expression*)self, (ast_expression_codegen*)&ast_state_codegen);
    self->m_framenum  = frame;
    self->m_nextthink = think;
    return self;
}

void ast_state_delete(ast_state *self)
{
    if (self->m_framenum)
        ast_unref(self->m_framenum);
    if (self->m_nextthink)
        ast_unref(self->m_nextthink);

    ast_expression_delete((ast_expression*)self);
    self->~ast_state();
    mem_d(self);
}

ast_call* ast_call_new(lex_ctx_t ctx,
                       ast_expression *funcexpr)
{
    ast_instantiate(ast_call, ctx, ast_call_delete);
    if (!funcexpr->m_next) {
        compile_error(ctx, "not a function");
        mem_d(self);
        return nullptr;
    }
    ast_expression_init((ast_expression*)self, (ast_expression_codegen*)&ast_call_codegen);

    self->m_side_effects = true;

    self->m_func     = funcexpr;
    self->m_va_count = nullptr;

    ast_type_adopt(self, funcexpr->m_next);

    return self;
}

void ast_call_delete(ast_call *self)
{
    for (auto &it : self->m_params)
        ast_unref(it);

    if (self->m_func)
        ast_unref(self->m_func);

    if (self->m_va_count)
        ast_unref(self->m_va_count);

    ast_expression_delete((ast_expression*)self);
    self->~ast_call();
    mem_d(self);
}

static bool ast_call_check_vararg(ast_call *self, ast_expression *va_type, ast_expression *exp_type)
{
    char texp[1024];
    char tgot[1024];
    if (!exp_type)
        return true;
    if (!va_type || !ast_compare_type(va_type, exp_type))
    {
        if (va_type && exp_type)
        {
            ast_type_to_string(va_type,  tgot, sizeof(tgot));
            ast_type_to_string(exp_type, texp, sizeof(texp));
            if (OPTS_FLAG(UNSAFE_VARARGS)) {
                if (compile_warning(self->m_context, WARN_UNSAFE_TYPES,
                                    "piped variadic argument differs in type: constrained to type %s, expected type %s",
                                    tgot, texp))
                    return false;
            } else {
                compile_error(self->m_context,
                              "piped variadic argument differs in type: constrained to type %s, expected type %s",
                              tgot, texp);
                return false;
            }
        }
        else
        {
            ast_type_to_string(exp_type, texp, sizeof(texp));
            if (OPTS_FLAG(UNSAFE_VARARGS)) {
                if (compile_warning(self->m_context, WARN_UNSAFE_TYPES,
                                    "piped variadic argument may differ in type: expected type %s",
                                    texp))
                    return false;
            } else {
                compile_error(self->m_context,
                              "piped variadic argument may differ in type: expected type %s",
                              texp);
                return false;
            }
        }
    }
    return true;
}

bool ast_call_check_types(ast_call *self, ast_expression *va_type)
{
    char texp[1024];
    char tgot[1024];
    size_t i;
    bool retval = true;
    const ast_expression *func = self->m_func;
    size_t count = self->m_params.size();
    if (count > func->m_type_params.size())
        count = func->m_type_params.size();

    for (i = 0; i < count; ++i) {
        if (ast_istype(self->m_params[i], ast_argpipe)) {
            /* warn about type safety instead */
            if (i+1 != count) {
                compile_error(self->m_context, "argpipe must be the last parameter to a function call");
                return false;
            }
            if (!ast_call_check_vararg(self, va_type, (ast_expression*)func->m_type_params[i]))
                retval = false;
        }
        else if (!ast_compare_type(self->m_params[i], (ast_expression*)(func->m_type_params[i])))
        {
            ast_type_to_string(self->m_params[i], tgot, sizeof(tgot));
            ast_type_to_string((ast_expression*)func->m_type_params[i], texp, sizeof(texp));
            compile_error(self->m_context, "invalid type for parameter %u in function call: expected %s, got %s",
                     (unsigned int)(i+1), texp, tgot);
            /* we don't immediately return */
            retval = false;
        }
    }
    count = self->m_params.size();
    if (count > func->m_type_params.size() && func->m_varparam) {
        for (; i < count; ++i) {
            if (ast_istype(self->m_params[i], ast_argpipe)) {
                /* warn about type safety instead */
                if (i+1 != count) {
                    compile_error(self->m_context, "argpipe must be the last parameter to a function call");
                    return false;
                }
                if (!ast_call_check_vararg(self, va_type, func->m_varparam))
                    retval = false;
            }
            else if (!ast_compare_type(self->m_params[i], func->m_varparam))
            {
                ast_type_to_string(self->m_params[i], tgot, sizeof(tgot));
                ast_type_to_string(func->m_varparam, texp, sizeof(texp));
                compile_error(self->m_context, "invalid type for variadic parameter %u in function call: expected %s, got %s",
                         (unsigned int)(i+1), texp, tgot);
                /* we don't immediately return */
                retval = false;
            }
        }
    }
    return retval;
}

ast_block* ast_block_new(lex_ctx_t ctx)
{
    ast_instantiate(ast_block, ctx, ast_block_delete);
    ast_expression_init((ast_expression*)self,
                        (ast_expression_codegen*)&ast_block_codegen);
    return self;
}

bool ast_block_add_expr(ast_block *self, ast_expression *e)
{
    self->propagate_side_effects(e);
    self->m_exprs.push_back(e);
    if (self->m_next) {
        ast_delete(self->m_next);
        self->m_next = nullptr;
    }
    ast_type_adopt(self, e);
    return true;
}

void ast_block_collect(ast_block *self, ast_expression *expr)
{
    self->m_collect.push_back(expr);
    expr->m_keep_node = true;
}

void ast_block_delete(ast_block *self)
{
    for (auto &it : self->m_exprs) ast_unref(it);
    for (auto &it : self->m_locals) ast_delete(it);
    for (auto &it : self->m_collect) ast_delete(it);
    ast_expression_delete((ast_expression*)self);
    self->~ast_block();
    mem_d(self);
}

void ast_block_set_type(ast_block *self, ast_expression *from)
{
    if (self->m_next)
        ast_delete(self->m_next);
    ast_type_adopt(self, from);
}

ast_function* ast_function_new(lex_ctx_t ctx, const char *name, ast_value *vtype)
{
    ast_instantiate(ast_function, ctx, ast_function_delete);

    if (!vtype) {
        compile_error(self->m_context, "internal error: ast_function_new condition 0");
        goto cleanup;
    } else if (vtype->m_hasvalue || vtype->m_vtype != TYPE_FUNCTION) {
        compile_error(self->m_context, "internal error: ast_function_new condition %i %i type=%i (probably 2 bodies?)",
                 (int)!vtype,
                 (int)vtype->m_hasvalue,
                 vtype->m_vtype);
        goto cleanup;
    }

    self->m_function_type = vtype;
    self->m_name          = name ? util_strdup(name) : nullptr;

    self->m_labelcount = 0;
    self->m_builtin = 0;

    self->m_ir_func = nullptr;
    self->m_curblock = nullptr;

    vtype->m_hasvalue = true;
    vtype->m_constval.vfunc = self;

    self->m_varargs          = nullptr;
    self->m_argc             = nullptr;
    self->m_fixedparams      = nullptr;
    self->m_return_value     = nullptr;
    self->m_static_count     = 0;

    return self;

cleanup:
    mem_d(self);
    return nullptr;
}

void ast_function_delete(ast_function *self)
{
    if (self->m_name)
        mem_d((void*)self->m_name);
    if (self->m_function_type) {
        /* ast_value_delete(self->m_function_type); */
        self->m_function_type->m_hasvalue = false;
        self->m_function_type->m_constval.vfunc = nullptr;
        /* We use unref - if it was stored in a global table it is supposed
         * to be deleted from *there*
         */
        ast_unref(self->m_function_type);
    }
    for (auto &it : self->m_static_names)
        mem_d(it);
    // FIXME::DELME:: unique_ptr used on ast_block
    //for (auto &it : self->m_blocks)
    //    ast_delete(it);
    if (self->m_varargs)
        ast_delete(self->m_varargs);
    if (self->m_argc)
        ast_delete(self->m_argc);
    if (self->m_fixedparams)
        ast_unref(self->m_fixedparams);
    if (self->m_return_value)
        ast_unref(self->m_return_value);
    self->~ast_function();
    mem_d(self);
}

const char* ast_function_label(ast_function *self, const char *prefix)
{
    size_t id;
    size_t len;
    char  *from;

    if (!OPTS_OPTION_BOOL(OPTION_DUMP)    &&
        !OPTS_OPTION_BOOL(OPTION_DUMPFIN) &&
        !OPTS_OPTION_BOOL(OPTION_DEBUG))
    {
        return nullptr;
    }

    id  = (self->m_labelcount++);
    len = strlen(prefix);

    from = self->m_labelbuf + sizeof(self->m_labelbuf)-1;
    *from-- = 0;
    do {
        *from-- = (id%10) + '0';
        id /= 10;
    } while (id);
    ++from;
    memcpy(from - len, prefix, len);
    return from - len;
}

/*********************************************************************/
/* AST codegen part
 * by convention you must never pass nullptr to the 'ir_value **out'
 * parameter. If you really don't care about the output, pass a dummy.
 * But I can't imagine a pituation where the output is truly unnecessary.
 */

static void _ast_codegen_output_type(ast_expression *self, ir_value *out)
{
    if (out->m_vtype == TYPE_FIELD)
        out->m_fieldtype = self->m_next->m_vtype;
    if (out->m_vtype == TYPE_FUNCTION)
        out->m_outtype = self->m_next->m_vtype;
}

#define codegen_output_type(a,o) (_ast_codegen_output_type(static_cast<ast_expression*>((a)),(o)))

bool ast_value_codegen(ast_value *self, ast_function *func, bool lvalue, ir_value **out)
{
    (void)func;
    (void)lvalue;
    if (self->m_vtype == TYPE_NIL) {
        *out = func->m_ir_func->m_owner->m_nil;
        return true;
    }
    /* NOTE: This is the codegen for a variable used in an 
     * It is not the codegen to generate the value. For this purpose,
     * ast_local_codegen and ast_global_codegen are to be used before this
     * is executed. ast_function_codegen should take care of its locals,
     * and the ast-user should take care of ast_global_codegen to be used
     * on all the globals.
     */
    if (!self->m_ir_v) {
        char tname[1024]; /* typename is reserved in C++ */
        ast_type_to_string((ast_expression*)self, tname, sizeof(tname));
        compile_error(self->m_context, "ast_value used before generated %s %s", tname, self->m_name);
        return false;
    }
    *out = self->m_ir_v;
    return true;
}

static bool ast_global_array_set(ast_value *self)
{
    size_t count = self->m_initlist.size();
    size_t i;

    if (count > self->m_count) {
        compile_error(self->m_context, "too many elements in initializer");
        count = self->m_count;
    }
    else if (count < self->m_count) {
        /* add this?
        compile_warning(self->m_context, "not all elements are initialized");
        */
    }

    for (i = 0; i != count; ++i) {
        switch (self->m_next->m_vtype) {
            case TYPE_FLOAT:
                if (!ir_value_set_float(self->m_ir_values[i], self->m_initlist[i].vfloat))
                    return false;
                break;
            case TYPE_VECTOR:
                if (!ir_value_set_vector(self->m_ir_values[i], self->m_initlist[i].vvec))
                    return false;
                break;
            case TYPE_STRING:
                if (!ir_value_set_string(self->m_ir_values[i], self->m_initlist[i].vstring))
                    return false;
                break;
            case TYPE_ARRAY:
                /* we don't support them in any other place yet either */
                compile_error(self->m_context, "TODO: nested arrays");
                return false;
            case TYPE_FUNCTION:
                /* this requiers a bit more work - similar to the fields I suppose */
                compile_error(self->m_context, "global of type function not properly generated");
                return false;
            case TYPE_FIELD:
                if (!self->m_initlist[i].vfield) {
                    compile_error(self->m_context, "field constant without vfield set");
                    return false;
                }
                if (!self->m_initlist[i].vfield->m_ir_v) {
                    compile_error(self->m_context, "field constant generated before its field");
                    return false;
                }
                if (!ir_value_set_field(self->m_ir_values[i], self->m_initlist[i].vfield->m_ir_v))
                    return false;
                break;
            default:
                compile_error(self->m_context, "TODO: global constant type %i", self->m_vtype);
                break;
        }
    }
    return true;
}

static bool check_array(ast_value *self, ast_value *array)
{
    if (array->m_flags & AST_FLAG_ARRAY_INIT && array->m_initlist.empty()) {
        compile_error(self->m_context, "array without size: %s", self->m_name);
        return false;
    }
    /* we are lame now - considering the way QC works we won't tolerate arrays > 1024 elements */
    if (!array->m_count || array->m_count > OPTS_OPTION_U32(OPTION_MAX_ARRAY_SIZE)) {
        compile_error(self->m_context, "Invalid array of size %lu", (unsigned long)array->m_count);
        return false;
    }
    return true;
}

bool ast_global_codegen(ast_value *self, ir_builder *ir, bool isfield)
{
    ir_value *v = nullptr;

    if (self->m_vtype == TYPE_NIL) {
        compile_error(self->m_context, "internal error: trying to generate a variable of TYPE_NIL");
        return false;
    }

    if (self->m_hasvalue && self->m_vtype == TYPE_FUNCTION)
    {
        ir_function *func = ir_builder_create_function(ir, self->m_name, self->m_next->m_vtype);
        if (!func)
            return false;
        func->m_context = self->m_context;
        func->m_value->m_context = self->m_context;

        self->m_constval.vfunc->m_ir_func = func;
        self->m_ir_v = func->m_value;
        if (self->m_flags & AST_FLAG_INCLUDE_DEF)
            self->m_ir_v->m_flags |= IR_FLAG_INCLUDE_DEF;
        if (self->m_flags & AST_FLAG_ERASEABLE)
            self->m_ir_v->m_flags |= IR_FLAG_ERASABLE;
        if (self->m_flags & AST_FLAG_BLOCK_COVERAGE)
            func->m_flags |= IR_FLAG_BLOCK_COVERAGE;
        /* The function is filled later on ast_function_codegen... */
        return true;
    }

    if (isfield && self->m_vtype == TYPE_FIELD) {
        ast_expression *fieldtype = self->m_next;

        if (self->m_hasvalue) {
            compile_error(self->m_context, "TODO: constant field pointers with value");
            goto error;
        }

        if (fieldtype->m_vtype == TYPE_ARRAY) {
            size_t ai;
            char   *name;
            size_t  namelen;

            ast_expression *elemtype;
            qc_type         vtype;
            ast_value      *array = (ast_value*)fieldtype;

            if (!ast_istype(fieldtype, ast_value)) {
                compile_error(self->m_context, "internal error: ast_value required");
                return false;
            }

            if (!check_array(self, array))
                return false;

            elemtype = array->m_next;
            vtype = elemtype->m_vtype;

            v = ir_builder_create_field(ir, self->m_name, vtype);
            if (!v) {
                compile_error(self->m_context, "ir_builder_create_global failed on `%s`", self->m_name);
                return false;
            }
            v->m_context = self->m_context;
            v->m_unique_life = true;
            v->m_locked      = true;
            array->m_ir_v = self->m_ir_v = v;

            if (self->m_flags & AST_FLAG_INCLUDE_DEF)
                self->m_ir_v->m_flags |= IR_FLAG_INCLUDE_DEF;
            if (self->m_flags & AST_FLAG_ERASEABLE)
                self->m_ir_v->m_flags |= IR_FLAG_ERASABLE;

            namelen = strlen(self->m_name);
            name    = (char*)mem_a(namelen + 16);
            util_strncpy(name, self->m_name, namelen);

            array->m_ir_values = (ir_value**)mem_a(sizeof(array->m_ir_values[0]) * array->m_count);
            array->m_ir_values[0] = v;
            for (ai = 1; ai < array->m_count; ++ai) {
                util_snprintf(name + namelen, 16, "[%u]", (unsigned int)ai);
                array->m_ir_values[ai] = ir_builder_create_field(ir, name, vtype);
                if (!array->m_ir_values[ai]) {
                    mem_d(name);
                    compile_error(self->m_context, "ir_builder_create_global failed on `%s`", name);
                    return false;
                }
                array->m_ir_values[ai]->m_context = self->m_context;
                array->m_ir_values[ai]->m_unique_life = true;
                array->m_ir_values[ai]->m_locked      = true;
                if (self->m_flags & AST_FLAG_INCLUDE_DEF)
                    self->m_ir_values[ai]->m_flags |= IR_FLAG_INCLUDE_DEF;
            }
            mem_d(name);
        }
        else
        {
            v = ir_builder_create_field(ir, self->m_name, self->m_next->m_vtype);
            if (!v)
                return false;
            v->m_context = self->m_context;
            self->m_ir_v = v;
            if (self->m_flags & AST_FLAG_INCLUDE_DEF)
                self->m_ir_v->m_flags |= IR_FLAG_INCLUDE_DEF;

            if (self->m_flags & AST_FLAG_ERASEABLE)
                self->m_ir_v->m_flags |= IR_FLAG_ERASABLE;
        }
        return true;
    }

    if (self->m_vtype == TYPE_ARRAY) {
        size_t ai;
        char   *name;
        size_t  namelen;

        ast_expression *elemtype = self->m_next;
        qc_type vtype = elemtype->m_vtype;

        if (self->m_flags & AST_FLAG_ARRAY_INIT && !self->m_count) {
            compile_error(self->m_context, "array `%s' has no size", self->m_name);
            return false;
        }

        /* same as with field arrays */
        if (!check_array(self, self))
            return false;

        v = ir_builder_create_global(ir, self->m_name, vtype);
        if (!v) {
            compile_error(self->m_context, "ir_builder_create_global failed `%s`", self->m_name);
            return false;
        }
        v->m_context = self->m_context;
        v->m_unique_life = true;
        v->m_locked      = true;

        if (self->m_flags & AST_FLAG_INCLUDE_DEF)
            v->m_flags |= IR_FLAG_INCLUDE_DEF;
        if (self->m_flags & AST_FLAG_ERASEABLE)
            self->m_ir_v->m_flags |= IR_FLAG_ERASABLE;

        namelen = strlen(self->m_name);
        name    = (char*)mem_a(namelen + 16);
        util_strncpy(name, self->m_name, namelen);

        self->m_ir_values = (ir_value**)mem_a(sizeof(self->m_ir_values[0]) * self->m_count);
        self->m_ir_values[0] = v;
        for (ai = 1; ai < self->m_count; ++ai) {
            util_snprintf(name + namelen, 16, "[%u]", (unsigned int)ai);
            self->m_ir_values[ai] = ir_builder_create_global(ir, name, vtype);
            if (!self->m_ir_values[ai]) {
                mem_d(name);
                compile_error(self->m_context, "ir_builder_create_global failed `%s`", name);
                return false;
            }
            self->m_ir_values[ai]->m_context = self->m_context;
            self->m_ir_values[ai]->m_unique_life = true;
            self->m_ir_values[ai]->m_locked      = true;
            if (self->m_flags & AST_FLAG_INCLUDE_DEF)
                self->m_ir_values[ai]->m_flags |= IR_FLAG_INCLUDE_DEF;
        }
        mem_d(name);
    }
    else
    {
        /* Arrays don't do this since there's no "array" value which spans across the
         * whole thing.
         */
        v = ir_builder_create_global(ir, self->m_name, self->m_vtype);
        if (!v) {
            compile_error(self->m_context, "ir_builder_create_global failed on `%s`", self->m_name);
            return false;
        }
        codegen_output_type(self, v);
        v->m_context = self->m_context;
    }

    /* link us to the ir_value */
    v->m_cvq = self->m_cvq;
    self->m_ir_v = v;

    if (self->m_flags & AST_FLAG_INCLUDE_DEF)
        self->m_ir_v->m_flags |= IR_FLAG_INCLUDE_DEF;
    if (self->m_flags & AST_FLAG_ERASEABLE)
        self->m_ir_v->m_flags |= IR_FLAG_ERASABLE;

    /* initialize */
    if (self->m_hasvalue) {
        switch (self->m_vtype)
        {
            case TYPE_FLOAT:
                if (!ir_value_set_float(v, self->m_constval.vfloat))
                    goto error;
                break;
            case TYPE_VECTOR:
                if (!ir_value_set_vector(v, self->m_constval.vvec))
                    goto error;
                break;
            case TYPE_STRING:
                if (!ir_value_set_string(v, self->m_constval.vstring))
                    goto error;
                break;
            case TYPE_ARRAY:
                ast_global_array_set(self);
                break;
            case TYPE_FUNCTION:
                compile_error(self->m_context, "global of type function not properly generated");
                goto error;
                /* Cannot generate an IR value for a function,
                 * need a pointer pointing to a function rather.
                 */
            case TYPE_FIELD:
                if (!self->m_constval.vfield) {
                    compile_error(self->m_context, "field constant without vfield set");
                    goto error;
                }
                if (!self->m_constval.vfield->m_ir_v) {
                    compile_error(self->m_context, "field constant generated before its field");
                    goto error;
                }
                if (!ir_value_set_field(v, self->m_constval.vfield->m_ir_v))
                    goto error;
                break;
            default:
                compile_error(self->m_context, "TODO: global constant type %i", self->m_vtype);
                break;
        }
    }
    return true;

error: /* clean up */
    if (v) delete v;
    return false;
}

static bool ast_local_codegen(ast_value *self, ir_function *func, bool param)
{
    ir_value *v = nullptr;

    if (self->m_vtype == TYPE_NIL) {
        compile_error(self->m_context, "internal error: trying to generate a variable of TYPE_NIL");
        return false;
    }

    if (self->m_hasvalue && self->m_vtype == TYPE_FUNCTION)
    {
        /* Do we allow local functions? I think not...
         * this is NOT a function pointer atm.
         */
        return false;
    }

    if (self->m_vtype == TYPE_ARRAY) {
        size_t ai;
        char   *name;
        size_t  namelen;

        ast_expression *elemtype = self->m_next;
        qc_type vtype = elemtype->m_vtype;

        func->m_flags |= IR_FLAG_HAS_ARRAYS;

        if (param && !(self->m_flags & AST_FLAG_IS_VARARG)) {
            compile_error(self->m_context, "array-parameters are not supported");
            return false;
        }

        /* we are lame now - considering the way QC works we won't tolerate arrays > 1024 elements */
        if (!check_array(self, self))
            return false;

        self->m_ir_values = (ir_value**)mem_a(sizeof(self->m_ir_values[0]) * self->m_count);
        if (!self->m_ir_values) {
            compile_error(self->m_context, "failed to allocate array values");
            return false;
        }

        v = ir_function_create_local(func, self->m_name, vtype, param);
        if (!v) {
            compile_error(self->m_context, "internal error: ir_function_create_local failed");
            return false;
        }
        v->m_context = self->m_context;
        v->m_unique_life = true;
        v->m_locked      = true;

        namelen = strlen(self->m_name);
        name    = (char*)mem_a(namelen + 16);
        util_strncpy(name, self->m_name, namelen);

        self->m_ir_values[0] = v;
        for (ai = 1; ai < self->m_count; ++ai) {
            util_snprintf(name + namelen, 16, "[%u]", (unsigned int)ai);
            self->m_ir_values[ai] = ir_function_create_local(func, name, vtype, param);
            if (!self->m_ir_values[ai]) {
                compile_error(self->m_context, "internal_error: ir_builder_create_global failed on `%s`", name);
                return false;
            }
            self->m_ir_values[ai]->m_context = self->m_context;
            self->m_ir_values[ai]->m_unique_life = true;
            self->m_ir_values[ai]->m_locked      = true;
        }
        mem_d(name);
    }
    else
    {
        v = ir_function_create_local(func, self->m_name, self->m_vtype, param);
        if (!v)
            return false;
        codegen_output_type(self, v);
        v->m_context = self->m_context;
    }

    /* A constant local... hmmm...
     * I suppose the IR will have to deal with this
     */
    if (self->m_hasvalue) {
        switch (self->m_vtype)
        {
            case TYPE_FLOAT:
                if (!ir_value_set_float(v, self->m_constval.vfloat))
                    goto error;
                break;
            case TYPE_VECTOR:
                if (!ir_value_set_vector(v, self->m_constval.vvec))
                    goto error;
                break;
            case TYPE_STRING:
                if (!ir_value_set_string(v, self->m_constval.vstring))
                    goto error;
                break;
            default:
                compile_error(self->m_context, "TODO: global constant type %i", self->m_vtype);
                break;
        }
    }

    /* link us to the ir_value */
    v->m_cvq = self->m_cvq;
    self->m_ir_v = v;

    if (!ast_generate_accessors(self, func->m_owner))
        return false;
    return true;

error: /* clean up */
    delete v;
    return false;
}

bool ast_generate_accessors(ast_value *self, ir_builder *ir)
{
    size_t i;
    bool warn = OPTS_WARN(WARN_USED_UNINITIALIZED);
    if (!self->m_setter || !self->m_getter)
        return true;
    for (i = 0; i < self->m_count; ++i) {
        if (!self->m_ir_values) {
            compile_error(self->m_context, "internal error: no array values generated for `%s`", self->m_name);
            return false;
        }
        if (!self->m_ir_values[i]) {
            compile_error(self->m_context, "internal error: not all array values have been generated for `%s`", self->m_name);
            return false;
        }
        if (!self->m_ir_values[i]->m_life.empty()) {
            compile_error(self->m_context, "internal error: function containing `%s` already generated", self->m_name);
            return false;
        }
    }

    opts_set(opts.warn, WARN_USED_UNINITIALIZED, false);
    if (self->m_setter) {
        if (!ast_global_codegen  (self->m_setter, ir, false) ||
            !ast_function_codegen(self->m_setter->m_constval.vfunc, ir) ||
            !ir_function_finalize(self->m_setter->m_constval.vfunc->m_ir_func))
        {
            compile_error(self->m_context, "internal error: failed to generate setter for `%s`", self->m_name);
            opts_set(opts.warn, WARN_USED_UNINITIALIZED, warn);
            return false;
        }
    }
    if (self->m_getter) {
        if (!ast_global_codegen  (self->m_getter, ir, false) ||
            !ast_function_codegen(self->m_getter->m_constval.vfunc, ir) ||
            !ir_function_finalize(self->m_getter->m_constval.vfunc->m_ir_func))
        {
            compile_error(self->m_context, "internal error: failed to generate getter for `%s`", self->m_name);
            opts_set(opts.warn, WARN_USED_UNINITIALIZED, warn);
            return false;
        }
    }
    for (i = 0; i < self->m_count; ++i)
        self->m_ir_values[i]->m_life.clear();
    opts_set(opts.warn, WARN_USED_UNINITIALIZED, warn);
    return true;
}

bool ast_function_codegen(ast_function *self, ir_builder *ir)
{
    ir_function *irf;
    ir_value    *dummy;
    ast_expression         *ec;
    ast_expression_codegen *cgen;

    (void)ir;

    irf = self->m_ir_func;
    if (!irf) {
        compile_error(self->m_context, "internal error: ast_function's related ast_value was not generated yet");
        return false;
    }

    /* fill the parameter list */
    ec = self->m_function_type;
    for (auto &it : ec->m_type_params) {
        if (it->m_vtype == TYPE_FIELD)
            vec_push(irf->m_params, it->m_next->m_vtype);
        else
            vec_push(irf->m_params, it->m_vtype);
        if (!self->m_builtin) {
            if (!ast_local_codegen(it, self->m_ir_func, true))
                return false;
        }
    }

    if (self->m_varargs) {
        if (!ast_local_codegen(self->m_varargs, self->m_ir_func, true))
            return false;
        irf->m_max_varargs = self->m_varargs->m_count;
    }

    if (self->m_builtin) {
        irf->m_builtin = self->m_builtin;
        return true;
    }

    /* have a local return value variable? */
    if (self->m_return_value) {
        if (!ast_local_codegen(self->m_return_value, self->m_ir_func, false))
            return false;
    }

    if (self->m_blocks.empty()) {
        compile_error(self->m_context, "function `%s` has no body", self->m_name);
        return false;
    }

    irf->m_first = self->m_curblock = ir_function_create_block(self->m_context, irf, "entry");
    if (!self->m_curblock) {
        compile_error(self->m_context, "failed to allocate entry block for `%s`", self->m_name);
        return false;
    }

    if (self->m_argc) {
        ir_value *va_count;
        ir_value *fixed;
        ir_value *sub;
        if (!ast_local_codegen(self->m_argc, self->m_ir_func, true))
            return false;
        cgen = self->m_argc->m_codegen;
        if (!(*cgen)((ast_expression*)(self->m_argc), self, false, &va_count))
            return false;
        cgen = self->m_fixedparams->m_codegen;
        if (!(*cgen)((ast_expression*)(self->m_fixedparams), self, false, &fixed))
            return false;
        sub = ir_block_create_binop(self->m_curblock, self->m_context,
                                    ast_function_label(self, "va_count"), INSTR_SUB_F,
                                    ir_builder_get_va_count(ir), fixed);
        if (!sub)
            return false;
        if (!ir_block_create_store_op(self->m_curblock, self->m_context, INSTR_STORE_F,
                                      va_count, sub))
        {
            return false;
        }
    }

    for (auto &it : self->m_blocks) {
        cgen = it->m_codegen;
        if (!(*cgen)(it.get(), self, false, &dummy))
            return false;
    }

    /* TODO: check return types */
    if (!self->m_curblock->m_final)
    {
        if (!self->m_function_type->m_next ||
            self->m_function_type->m_next->m_vtype == TYPE_VOID)
        {
            return ir_block_create_return(self->m_curblock, self->m_context, nullptr);
        }
        else if (vec_size(self->m_curblock->m_entries) || self->m_curblock == irf->m_first)
        {
            if (self->m_return_value) {
                cgen = self->m_return_value->m_codegen;
                if (!(*cgen)((ast_expression*)(self->m_return_value), self, false, &dummy))
                    return false;
                return ir_block_create_return(self->m_curblock, self->m_context, dummy);
            }
            else if (compile_warning(self->m_context, WARN_MISSING_RETURN_VALUES,
                                "control reaches end of non-void function (`%s`) via %s",
                                self->m_name, self->m_curblock->m_label.c_str()))
            {
                return false;
            }
            return ir_block_create_return(self->m_curblock, self->m_context, nullptr);
        }
    }
    return true;
}

static bool starts_a_label(ast_expression *ex)
{
    while (ex && ast_istype(ex, ast_block)) {
        ast_block *b = (ast_block*)ex;
        ex = b->m_exprs[0];
    }
    if (!ex)
        return false;
    return ast_istype(ex, ast_label);
}

/* Note, you will not see ast_block_codegen generate ir_blocks.
 * To the AST and the IR, blocks are 2 different things.
 * In the AST it represents a block of code, usually enclosed in
 * curly braces {...}.
 * While in the IR it represents a block in terms of control-flow.
 */
bool ast_block_codegen(ast_block *self, ast_function *func, bool lvalue, ir_value **out)
{
    /* We don't use this
     * Note: an ast-representation using the comma-operator
     * of the form: (a, b, c) = x should not assign to c...
     */
    if (lvalue) {
        compile_error(self->m_context, "not an l-value (code-block)");
        return false;
    }

    if (self->m_outr) {
        *out = self->m_outr;
        return true;
    }

    /* output is nullptr at first, we'll have each expression
     * assign to out output, thus, a comma-operator represention
     * using an ast_block will return the last generated value,
     * so: (b, c) + a  executed both b and c, and returns c,
     * which is then added to a.
     */
    *out = nullptr;

    /* generate locals */
    for (auto &it : self->m_locals) {
        if (!ast_local_codegen(it, func->m_ir_func, false)) {
            if (OPTS_OPTION_BOOL(OPTION_DEBUG))
                compile_error(self->m_context, "failed to generate local `%s`", it->m_name);
            return false;
        }
    }

    for (auto &it : self->m_exprs) {
        ast_expression_codegen *gen;
        if (func->m_curblock->m_final && !starts_a_label(it)) {
            if (compile_warning(it->m_context, WARN_UNREACHABLE_CODE, "unreachable statement"))
                return false;
            continue;
        }
        gen = it->m_codegen;
        if (!(*gen)(it, func, false, out))
            return false;
    }

    self->m_outr = *out;

    return true;
}

bool ast_store_codegen(ast_store *self, ast_function *func, bool lvalue, ir_value **out)
{
    ast_expression_codegen *cgen;
    ir_value *left  = nullptr;
    ir_value *right = nullptr;

    ast_value       *arr;
    ast_value       *idx = 0;
    ast_array_index *ai = nullptr;

    if (lvalue && self->m_outl) {
        *out = self->m_outl;
        return true;
    }

    if (!lvalue && self->m_outr) {
        *out = self->m_outr;
        return true;
    }

    if (ast_istype(self->m_dest, ast_array_index))
    {

        ai = (ast_array_index*)self->m_dest;
        idx = (ast_value*)ai->m_index;

        if (ast_istype(ai->m_index, ast_value) && idx->m_hasvalue && idx->m_cvq == CV_CONST)
            ai = nullptr;
    }

    if (ai) {
        /* we need to call the setter */
        ir_value  *iridx, *funval;
        ir_instr  *call;

        if (lvalue) {
            compile_error(self->m_context, "array-subscript assignment cannot produce lvalues");
            return false;
        }

        arr = (ast_value*)ai->m_array;
        if (!ast_istype(ai->m_array, ast_value) || !arr->m_setter) {
            compile_error(self->m_context, "value has no setter (%s)", arr->m_name);
            return false;
        }

        cgen = idx->m_codegen;
        if (!(*cgen)((ast_expression*)(idx), func, false, &iridx))
            return false;

        cgen = arr->m_setter->m_codegen;
        if (!(*cgen)((ast_expression*)(arr->m_setter), func, true, &funval))
            return false;

        cgen = self->m_source->m_codegen;
        if (!(*cgen)((ast_expression*)(self->m_source), func, false, &right))
            return false;

        call = ir_block_create_call(func->m_curblock, self->m_context, ast_function_label(func, "store"), funval, false);
        if (!call)
            return false;
        ir_call_param(call, iridx);
        ir_call_param(call, right);
        self->m_outr = right;
    }
    else
    {
        /* regular code */

        cgen = self->m_dest->m_codegen;
        /* lvalue! */
        if (!(*cgen)((ast_expression*)(self->m_dest), func, true, &left))
            return false;
        self->m_outl = left;

        cgen = self->m_source->m_codegen;
        /* rvalue! */
        if (!(*cgen)((ast_expression*)(self->m_source), func, false, &right))
            return false;

        if (!ir_block_create_store_op(func->m_curblock, self->m_context, self->m_op, left, right))
            return false;
        self->m_outr = right;
    }

    /* Theoretically, an assinment returns its left side as an
     * lvalue, if we don't need an lvalue though, we return
     * the right side as an rvalue, otherwise we have to
     * somehow know whether or not we need to dereference the pointer
     * on the left side - that is: OP_LOAD if it was an address.
     * Also: in original QC we cannot OP_LOADP *anyway*.
     */
    *out = (lvalue ? left : right);

    return true;
}

bool ast_binary_codegen(ast_binary *self, ast_function *func, bool lvalue, ir_value **out)
{
    ast_expression_codegen *cgen;
    ir_value *left, *right;

    /* A binary operation cannot yield an l-value */
    if (lvalue) {
        compile_error(self->m_context, "not an l-value (binop)");
        return false;
    }

    if (self->m_outr) {
        *out = self->m_outr;
        return true;
    }

    if ((OPTS_FLAG(SHORT_LOGIC) || OPTS_FLAG(PERL_LOGIC)) &&
        (self->m_op == INSTR_AND || self->m_op == INSTR_OR))
    {
        /* NOTE: The short-logic path will ignore right_first */

        /* short circuit evaluation */
        ir_block *other, *merge;
        ir_block *from_left, *from_right;
        ir_instr *phi;
        size_t    merge_id;

        /* prepare end-block */
        merge_id = func->m_ir_func->m_blocks.size();
        merge    = ir_function_create_block(self->m_context, func->m_ir_func, ast_function_label(func, "sce_merge"));

        /* generate the left expression */
        cgen = self->m_left->m_codegen;
        if (!(*cgen)((ast_expression*)(self->m_left), func, false, &left))
            return false;
        /* remember the block */
        from_left = func->m_curblock;

        /* create a new block for the right expression */
        other = ir_function_create_block(self->m_context, func->m_ir_func, ast_function_label(func, "sce_other"));
        if (self->m_op == INSTR_AND) {
            /* on AND: left==true -> other */
            if (!ir_block_create_if(func->m_curblock, self->m_context, left, other, merge))
                return false;
        } else {
            /* on OR: left==false -> other */
            if (!ir_block_create_if(func->m_curblock, self->m_context, left, merge, other))
                return false;
        }
        /* use the likely flag */
        vec_last(func->m_curblock->m_instr)->m_likely = true;

        /* enter the right-expression's block */
        func->m_curblock = other;
        /* generate */
        cgen = self->m_right->m_codegen;
        if (!(*cgen)((ast_expression*)(self->m_right), func, false, &right))
            return false;
        /* remember block */
        from_right = func->m_curblock;

        /* jump to the merge block */
        if (!ir_block_create_jump(func->m_curblock, self->m_context, merge))
            return false;

        algo::shiftback(func->m_ir_func->m_blocks.begin() + merge_id,
                        func->m_ir_func->m_blocks.end());
        // FIXME::DELME::
        //func->m_ir_func->m_blocks[merge_id].release();
        //func->m_ir_func->m_blocks.erase(func->m_ir_func->m_blocks.begin() + merge_id);
        //func->m_ir_func->m_blocks.emplace_back(merge);

        func->m_curblock = merge;
        phi = ir_block_create_phi(func->m_curblock, self->m_context,
                                  ast_function_label(func, "sce_value"),
                                  self->m_vtype);
        ir_phi_add(phi, from_left, left);
        ir_phi_add(phi, from_right, right);
        *out = ir_phi_value(phi);
        if (!*out)
            return false;

        if (!OPTS_FLAG(PERL_LOGIC)) {
            /* cast-to-bool */
            if (OPTS_FLAG(CORRECT_LOGIC) && (*out)->m_vtype == TYPE_VECTOR) {
                *out = ir_block_create_unary(func->m_curblock, self->m_context,
                                             ast_function_label(func, "sce_bool_v"),
                                             INSTR_NOT_V, *out);
                if (!*out)
                    return false;
                *out = ir_block_create_unary(func->m_curblock, self->m_context,
                                             ast_function_label(func, "sce_bool"),
                                             INSTR_NOT_F, *out);
                if (!*out)
                    return false;
            }
            else if (OPTS_FLAG(FALSE_EMPTY_STRINGS) && (*out)->m_vtype == TYPE_STRING) {
                *out = ir_block_create_unary(func->m_curblock, self->m_context,
                                             ast_function_label(func, "sce_bool_s"),
                                             INSTR_NOT_S, *out);
                if (!*out)
                    return false;
                *out = ir_block_create_unary(func->m_curblock, self->m_context,
                                             ast_function_label(func, "sce_bool"),
                                             INSTR_NOT_F, *out);
                if (!*out)
                    return false;
            }
            else {
                *out = ir_block_create_binop(func->m_curblock, self->m_context,
                                             ast_function_label(func, "sce_bool"),
                                             INSTR_AND, *out, *out);
                if (!*out)
                    return false;
            }
        }

        self->m_outr = *out;
        codegen_output_type(self, *out);
        return true;
    }

    if (self->m_right_first) {
        cgen = self->m_right->m_codegen;
        if (!(*cgen)((ast_expression*)(self->m_right), func, false, &right))
            return false;
        cgen = self->m_left->m_codegen;
        if (!(*cgen)((ast_expression*)(self->m_left), func, false, &left))
            return false;
    } else {
        cgen = self->m_left->m_codegen;
        if (!(*cgen)((ast_expression*)(self->m_left), func, false, &left))
            return false;
        cgen = self->m_right->m_codegen;
        if (!(*cgen)((ast_expression*)(self->m_right), func, false, &right))
            return false;
    }

    *out = ir_block_create_binop(func->m_curblock, self->m_context, ast_function_label(func, "bin"),
                                 self->m_op, left, right);
    if (!*out)
        return false;
    self->m_outr = *out;
    codegen_output_type(self, *out);

    return true;
}

bool ast_binstore_codegen(ast_binstore *self, ast_function *func, bool lvalue, ir_value **out)
{
    ast_expression_codegen *cgen;
    ir_value *leftl = nullptr, *leftr, *right, *bin;

    ast_value       *arr;
    ast_value       *idx = 0;
    ast_array_index *ai = nullptr;
    ir_value        *iridx = nullptr;

    if (lvalue && self->m_outl) {
        *out = self->m_outl;
        return true;
    }

    if (!lvalue && self->m_outr) {
        *out = self->m_outr;
        return true;
    }

    if (ast_istype(self->m_dest, ast_array_index))
    {

        ai = (ast_array_index*)self->m_dest;
        idx = (ast_value*)ai->m_index;

        if (ast_istype(ai->m_index, ast_value) && idx->m_hasvalue && idx->m_cvq == CV_CONST)
            ai = nullptr;
    }

    /* for a binstore we need both an lvalue and an rvalue for the left side */
    /* rvalue of destination! */
    if (ai) {
        cgen = idx->m_codegen;
        if (!(*cgen)((ast_expression*)(idx), func, false, &iridx))
            return false;
    }
    cgen = self->m_dest->m_codegen;
    if (!(*cgen)((ast_expression*)(self->m_dest), func, false, &leftr))
        return false;

    /* source as rvalue only */
    cgen = self->m_source->m_codegen;
    if (!(*cgen)((ast_expression*)(self->m_source), func, false, &right))
        return false;

    /* now the binary */
    bin = ir_block_create_binop(func->m_curblock, self->m_context, ast_function_label(func, "binst"),
                                self->m_opbin, leftr, right);
    self->m_outr = bin;

    if (ai) {
        /* we need to call the setter */
        ir_value  *funval;
        ir_instr  *call;

        if (lvalue) {
            compile_error(self->m_context, "array-subscript assignment cannot produce lvalues");
            return false;
        }

        arr = (ast_value*)ai->m_array;
        if (!ast_istype(ai->m_array, ast_value) || !arr->m_setter) {
            compile_error(self->m_context, "value has no setter (%s)", arr->m_name);
            return false;
        }

        cgen = arr->m_setter->m_codegen;
        if (!(*cgen)((ast_expression*)(arr->m_setter), func, true, &funval))
            return false;

        call = ir_block_create_call(func->m_curblock, self->m_context, ast_function_label(func, "store"), funval, false);
        if (!call)
            return false;
        ir_call_param(call, iridx);
        ir_call_param(call, bin);
        self->m_outr = bin;
    } else {
        /* now store them */
        cgen = self->m_dest->m_codegen;
        /* lvalue of destination */
        if (!(*cgen)((ast_expression*)(self->m_dest), func, true, &leftl))
            return false;
        self->m_outl = leftl;

        if (!ir_block_create_store_op(func->m_curblock, self->m_context, self->m_opstore, leftl, bin))
            return false;
        self->m_outr = bin;
    }

    /* Theoretically, an assinment returns its left side as an
     * lvalue, if we don't need an lvalue though, we return
     * the right side as an rvalue, otherwise we have to
     * somehow know whether or not we need to dereference the pointer
     * on the left side - that is: OP_LOAD if it was an address.
     * Also: in original QC we cannot OP_LOADP *anyway*.
     */
    *out = (lvalue ? leftl : bin);

    return true;
}

bool ast_unary_codegen(ast_unary *self, ast_function *func, bool lvalue, ir_value **out)
{
    ast_expression_codegen *cgen;
    ir_value *operand;

    /* An unary operation cannot yield an l-value */
    if (lvalue) {
        compile_error(self->m_context, "not an l-value (binop)");
        return false;
    }

    if (self->m_outr) {
        *out = self->m_outr;
        return true;
    }

    cgen = self->m_operand->m_codegen;
    /* lvalue! */
    if (!(*cgen)((ast_expression*)(self->m_operand), func, false, &operand))
        return false;

    *out = ir_block_create_unary(func->m_curblock, self->m_context, ast_function_label(func, "unary"),
                                 self->m_op, operand);
    if (!*out)
        return false;
    self->m_outr = *out;

    return true;
}

bool ast_return_codegen(ast_return *self, ast_function *func, bool lvalue, ir_value **out)
{
    ast_expression_codegen *cgen;
    ir_value *operand;

    *out = nullptr;

    /* In the context of a return operation, we don't actually return
     * anything...
     */
    if (lvalue) {
        compile_error(self->m_context, "return-expression is not an l-value");
        return false;
    }

    if (self->m_outr) {
        compile_error(self->m_context, "internal error: ast_return cannot be reused, it bears no result!");
        return false;
    }
    self->m_outr = (ir_value*)1;

    if (self->m_operand) {
        cgen = self->m_operand->m_codegen;
        /* lvalue! */
        if (!(*cgen)((ast_expression*)(self->m_operand), func, false, &operand))
            return false;

        if (!ir_block_create_return(func->m_curblock, self->m_context, operand))
            return false;
    } else {
        if (!ir_block_create_return(func->m_curblock, self->m_context, nullptr))
            return false;
    }

    return true;
}

bool ast_entfield_codegen(ast_entfield *self, ast_function *func, bool lvalue, ir_value **out)
{
    ast_expression_codegen *cgen;
    ir_value *ent, *field;

    /* This function needs to take the 'lvalue' flag into account!
     * As lvalue we provide a field-pointer, as rvalue we provide the
     * value in a temp.
     */

    if (lvalue && self->m_outl) {
        *out = self->m_outl;
        return true;
    }

    if (!lvalue && self->m_outr) {
        *out = self->m_outr;
        return true;
    }

    cgen = self->m_entity->m_codegen;
    if (!(*cgen)((ast_expression*)(self->m_entity), func, false, &ent))
        return false;

    cgen = self->m_field->m_codegen;
    if (!(*cgen)((ast_expression*)(self->m_field), func, false, &field))
        return false;

    if (lvalue) {
        /* address! */
        *out = ir_block_create_fieldaddress(func->m_curblock, self->m_context, ast_function_label(func, "efa"),
                                            ent, field);
    } else {
        *out = ir_block_create_load_from_ent(func->m_curblock, self->m_context, ast_function_label(func, "efv"),
                                             ent, field, self->m_vtype);
        /* Done AFTER error checking:
        codegen_output_type(self, *out);
        */
    }
    if (!*out) {
        compile_error(self->m_context, "failed to create %s instruction (output type %s)",
                 (lvalue ? "ADDRESS" : "FIELD"),
                 type_name[self->m_vtype]);
        return false;
    }
    if (!lvalue)
        codegen_output_type(self, *out);

    if (lvalue)
        self->m_outl = *out;
    else
        self->m_outr = *out;

    /* Hm that should be it... */
    return true;
}

bool ast_member_codegen(ast_member *self, ast_function *func, bool lvalue, ir_value **out)
{
    ast_expression_codegen *cgen;
    ir_value *vec;

    /* in QC this is always an lvalue */
    if (lvalue && self->m_rvalue) {
        compile_error(self->m_context, "not an l-value (member access)");
        return false;
    }
    if (self->m_outl) {
        *out = self->m_outl;
        return true;
    }

    cgen = self->m_owner->m_codegen;
    if (!(*cgen)((ast_expression*)(self->m_owner), func, false, &vec))
        return false;

    if (vec->m_vtype != TYPE_VECTOR &&
        !(vec->m_vtype == TYPE_FIELD && self->m_owner->m_next->m_vtype == TYPE_VECTOR))
    {
        return false;
    }

    *out = ir_value_vector_member(vec, self->m_field);
    self->m_outl = *out;

    return (*out != nullptr);
}

bool ast_array_index_codegen(ast_array_index *self, ast_function *func, bool lvalue, ir_value **out)
{
    ast_value *arr;
    ast_value *idx;

    if (!lvalue && self->m_outr) {
        *out = self->m_outr;
        return true;
    }
    if (lvalue && self->m_outl) {
        *out = self->m_outl;
        return true;
    }

    if (!ast_istype(self->m_array, ast_value)) {
        compile_error(self->m_context, "array indexing this way is not supported");
        /* note this would actually be pointer indexing because the left side is
         * not an actual array but (hopefully) an indexable expression.
         * Once we get integer arithmetic, and GADDRESS/GSTORE/GLOAD instruction
         * support this path will be filled.
         */
        return false;
    }

    arr = (ast_value*)self->m_array;
    idx = (ast_value*)self->m_index;

    if (!ast_istype(self->m_index, ast_value) || !idx->m_hasvalue || idx->m_cvq != CV_CONST) {
        /* Time to use accessor functions */
        ast_expression_codegen *cgen;
        ir_value               *iridx, *funval;
        ir_instr               *call;

        if (lvalue) {
            compile_error(self->m_context, "(.2) array indexing here needs a compile-time constant");
            return false;
        }

        if (!arr->m_getter) {
            compile_error(self->m_context, "value has no getter, don't know how to index it");
            return false;
        }

        cgen = self->m_index->m_codegen;
        if (!(*cgen)((ast_expression*)(self->m_index), func, false, &iridx))
            return false;

        cgen = arr->m_getter->m_codegen;
        if (!(*cgen)((ast_expression*)(arr->m_getter), func, true, &funval))
            return false;

        call = ir_block_create_call(func->m_curblock, self->m_context, ast_function_label(func, "fetch"), funval, false);
        if (!call)
            return false;
        ir_call_param(call, iridx);

        *out = ir_call_value(call);
        self->m_outr = *out;
        (*out)->m_vtype = self->m_vtype;
        codegen_output_type(self, *out);
        return true;
    }

    if (idx->m_vtype == TYPE_FLOAT) {
        unsigned int arridx = idx->m_constval.vfloat;
        if (arridx >= self->m_array->m_count)
        {
            compile_error(self->m_context, "array index out of bounds: %i", arridx);
            return false;
        }
        *out = arr->m_ir_values[arridx];
    }
    else if (idx->m_vtype == TYPE_INTEGER) {
        unsigned int arridx = idx->m_constval.vint;
        if (arridx >= self->m_array->m_count)
        {
            compile_error(self->m_context, "array index out of bounds: %i", arridx);
            return false;
        }
        *out = arr->m_ir_values[arridx];
    }
    else {
        compile_error(self->m_context, "array indexing here needs an integer constant");
        return false;
    }
    (*out)->m_vtype = self->m_vtype;
    codegen_output_type(self, *out);
    return true;
}

bool ast_argpipe_codegen(ast_argpipe *self, ast_function *func, bool lvalue, ir_value **out)
{
    *out = nullptr;
    if (lvalue) {
        compile_error(self->m_context, "argpipe node: not an lvalue");
        return false;
    }
    (void)func;
    (void)out;
    compile_error(self->m_context, "TODO: argpipe codegen not implemented");
    return false;
}

bool ast_ifthen_codegen(ast_ifthen *self, ast_function *func, bool lvalue, ir_value **out)
{
    ast_expression_codegen *cgen;

    ir_value *condval;
    ir_value *dummy;

    ir_block *cond;
    ir_block *ontrue;
    ir_block *onfalse;
    ir_block *ontrue_endblock = nullptr;
    ir_block *onfalse_endblock = nullptr;
    ir_block *merge = nullptr;
    int folded = 0;

    /* We don't output any value, thus also don't care about r/lvalue */
    (void)out;
    (void)lvalue;

    if (self->m_outr) {
        compile_error(self->m_context, "internal error: ast_ifthen cannot be reused, it bears no result!");
        return false;
    }
    self->m_outr = (ir_value*)1;

    /* generate the condition */
    cgen = self->m_cond->m_codegen;
    if (!(*cgen)((ast_expression*)(self->m_cond), func, false, &condval))
        return false;
    /* update the block which will get the jump - because short-logic or ternaries may have changed this */
    cond = func->m_curblock;

    /* try constant folding away the condition */
    if ((folded = fold::cond_ifthen(condval, func, self)) != -1)
        return folded;

    if (self->m_on_true) {
        /* create on-true block */
        ontrue = ir_function_create_block(self->m_context, func->m_ir_func, ast_function_label(func, "ontrue"));
        if (!ontrue)
            return false;

        /* enter the block */
        func->m_curblock = ontrue;

        /* generate */
        cgen = self->m_on_true->m_codegen;
        if (!(*cgen)((ast_expression*)(self->m_on_true), func, false, &dummy))
            return false;

        /* we now need to work from the current endpoint */
        ontrue_endblock = func->m_curblock;
    } else
        ontrue = nullptr;

    /* on-false path */
    if (self->m_on_false) {
        /* create on-false block */
        onfalse = ir_function_create_block(self->m_context, func->m_ir_func, ast_function_label(func, "onfalse"));
        if (!onfalse)
            return false;

        /* enter the block */
        func->m_curblock = onfalse;

        /* generate */
        cgen = self->m_on_false->m_codegen;
        if (!(*cgen)((ast_expression*)(self->m_on_false), func, false, &dummy))
            return false;

        /* we now need to work from the current endpoint */
        onfalse_endblock = func->m_curblock;
    } else
        onfalse = nullptr;

    /* Merge block were they all merge in to */
    if (!ontrue || !onfalse || !ontrue_endblock->m_final || !onfalse_endblock->m_final)
    {
        merge = ir_function_create_block(self->m_context, func->m_ir_func, ast_function_label(func, "endif"));
        if (!merge)
            return false;
        /* add jumps ot the merge block */
        if (ontrue && !ontrue_endblock->m_final && !ir_block_create_jump(ontrue_endblock, self->m_context, merge))
            return false;
        if (onfalse && !onfalse_endblock->m_final && !ir_block_create_jump(onfalse_endblock, self->m_context, merge))
            return false;

        /* Now enter the merge block */
        func->m_curblock = merge;
    }

    /* we create the if here, that way all blocks are ordered :)
     */
    if (!ir_block_create_if(cond, self->m_context, condval,
                            (ontrue  ? ontrue  : merge),
                            (onfalse ? onfalse : merge)))
    {
        return false;
    }

    return true;
}

bool ast_ternary_codegen(ast_ternary *self, ast_function *func, bool lvalue, ir_value **out)
{
    ast_expression_codegen *cgen;

    ir_value *condval;
    ir_value *trueval, *falseval;
    ir_instr *phi;

    ir_block *cond = func->m_curblock;
    ir_block *cond_out = nullptr;
    ir_block *ontrue, *ontrue_out = nullptr;
    ir_block *onfalse, *onfalse_out = nullptr;
    ir_block *merge;
    int folded = 0;

    /* Ternary can never create an lvalue... */
    if (lvalue)
        return false;

    /* In theory it shouldn't be possible to pass through a node twice, but
     * in case we add any kind of optimization pass for the AST itself, it
     * may still happen, thus we remember a created ir_value and simply return one
     * if it already exists.
     */
    if (self->m_outr) {
        *out = self->m_outr;
        return true;
    }

    /* In the following, contraty to ast_ifthen, we assume both paths exist. */

    /* generate the condition */
    func->m_curblock = cond;
    cgen = self->m_cond->m_codegen;
    if (!(*cgen)((ast_expression*)(self->m_cond), func, false, &condval))
        return false;
    cond_out = func->m_curblock;

    /* try constant folding away the condition */
    if ((folded = fold::cond_ternary(condval, func, self)) != -1)
        return folded;

    /* create on-true block */
    ontrue = ir_function_create_block(self->m_context, func->m_ir_func, ast_function_label(func, "tern_T"));
    if (!ontrue)
        return false;
    else
    {
        /* enter the block */
        func->m_curblock = ontrue;

        /* generate */
        cgen = self->m_on_true->m_codegen;
        if (!(*cgen)((ast_expression*)(self->m_on_true), func, false, &trueval))
            return false;

        ontrue_out = func->m_curblock;
    }

    /* create on-false block */
    onfalse = ir_function_create_block(self->m_context, func->m_ir_func, ast_function_label(func, "tern_F"));
    if (!onfalse)
        return false;
    else
    {
        /* enter the block */
        func->m_curblock = onfalse;

        /* generate */
        cgen = self->m_on_false->m_codegen;
        if (!(*cgen)((ast_expression*)(self->m_on_false), func, false, &falseval))
            return false;

        onfalse_out = func->m_curblock;
    }

    /* create merge block */
    merge = ir_function_create_block(self->m_context, func->m_ir_func, ast_function_label(func, "tern_out"));
    if (!merge)
        return false;
    /* jump to merge block */
    if (!ir_block_create_jump(ontrue_out, self->m_context, merge))
        return false;
    if (!ir_block_create_jump(onfalse_out, self->m_context, merge))
        return false;

    /* create if instruction */
    if (!ir_block_create_if(cond_out, self->m_context, condval, ontrue, onfalse))
        return false;

    /* Now enter the merge block */
    func->m_curblock = merge;

    /* Here, now, we need a PHI node
     * but first some sanity checking...
     */
    if (trueval->m_vtype != falseval->m_vtype && trueval->m_vtype != TYPE_NIL && falseval->m_vtype != TYPE_NIL) {
        /* error("ternary with different types on the two sides"); */
        compile_error(self->m_context, "internal error: ternary operand types invalid");
        return false;
    }

    /* create PHI */
    phi = ir_block_create_phi(merge, self->m_context, ast_function_label(func, "phi"), self->m_vtype);
    if (!phi) {
        compile_error(self->m_context, "internal error: failed to generate phi node");
        return false;
    }
    ir_phi_add(phi, ontrue_out,  trueval);
    ir_phi_add(phi, onfalse_out, falseval);

    self->m_outr = ir_phi_value(phi);
    *out = self->m_outr;

    codegen_output_type(self, *out);

    return true;
}

bool ast_loop_codegen(ast_loop *self, ast_function *func, bool lvalue, ir_value **out)
{
    ast_expression_codegen *cgen;

    ir_value *dummy      = nullptr;
    ir_value *precond    = nullptr;
    ir_value *postcond   = nullptr;

    /* Since we insert some jumps "late" so we have blocks
     * ordered "nicely", we need to keep track of the actual end-blocks
     * of expressions to add the jumps to.
     */
    ir_block *bbody      = nullptr, *end_bbody      = nullptr;
    ir_block *bprecond   = nullptr, *end_bprecond   = nullptr;
    ir_block *bpostcond  = nullptr, *end_bpostcond  = nullptr;
    ir_block *bincrement = nullptr, *end_bincrement = nullptr;
    ir_block *bout       = nullptr, *bin            = nullptr;

    /* let's at least move the outgoing block to the end */
    size_t    bout_id;

    /* 'break' and 'continue' need to be able to find the right blocks */
    ir_block *bcontinue     = nullptr;
    ir_block *bbreak        = nullptr;

    ir_block *tmpblock      = nullptr;

    (void)lvalue;
    (void)out;

    if (self->m_outr) {
        compile_error(self->m_context, "internal error: ast_loop cannot be reused, it bears no result!");
        return false;
    }
    self->m_outr = (ir_value*)1;

    /* NOTE:
     * Should we ever need some kind of block ordering, better make this function
     * move blocks around than write a block ordering algorithm later... after all
     * the ast and ir should work together, not against each other.
     */

    /* initexpr doesn't get its own block, it's pointless, it could create more blocks
     * anyway if for example it contains a ternary.
     */
    if (self->m_initexpr)
    {
        cgen = self->m_initexpr->m_codegen;
        if (!(*cgen)((ast_expression*)(self->m_initexpr), func, false, &dummy))
            return false;
    }

    /* Store the block from which we enter this chaos */
    bin = func->m_curblock;

    /* The pre-loop condition needs its own block since we
     * need to be able to jump to the start of that expression.
     */
    if (self->m_precond)
    {
        bprecond = ir_function_create_block(self->m_context, func->m_ir_func, ast_function_label(func, "pre_loop_cond"));
        if (!bprecond)
            return false;

        /* the pre-loop-condition the least important place to 'continue' at */
        bcontinue = bprecond;

        /* enter */
        func->m_curblock = bprecond;

        /* generate */
        cgen = self->m_precond->m_codegen;
        if (!(*cgen)((ast_expression*)(self->m_precond), func, false, &precond))
            return false;

        end_bprecond = func->m_curblock;
    } else {
        bprecond = end_bprecond = nullptr;
    }

    /* Now the next blocks won't be ordered nicely, but we need to
     * generate them this early for 'break' and 'continue'.
     */
    if (self->m_increment) {
        bincrement = ir_function_create_block(self->m_context, func->m_ir_func, ast_function_label(func, "loop_increment"));
        if (!bincrement)
            return false;
        bcontinue = bincrement; /* increment comes before the pre-loop-condition */
    } else {
        bincrement = end_bincrement = nullptr;
    }

    if (self->m_postcond) {
        bpostcond = ir_function_create_block(self->m_context, func->m_ir_func, ast_function_label(func, "post_loop_cond"));
        if (!bpostcond)
            return false;
        bcontinue = bpostcond; /* postcond comes before the increment */
    } else {
        bpostcond = end_bpostcond = nullptr;
    }

    bout_id = func->m_ir_func->m_blocks.size();
    bout = ir_function_create_block(self->m_context, func->m_ir_func, ast_function_label(func, "after_loop"));
    if (!bout)
        return false;
    bbreak = bout;

    /* The loop body... */
    /* if (self->m_body) */
    {
        bbody = ir_function_create_block(self->m_context, func->m_ir_func, ast_function_label(func, "loop_body"));
        if (!bbody)
            return false;

        /* enter */
        func->m_curblock = bbody;

        func->m_breakblocks.push_back(bbreak);
        if (bcontinue)
            func->m_continueblocks.push_back(bcontinue);
        else
            func->m_continueblocks.push_back(bbody);

        /* generate */
        if (self->m_body) {
            cgen = self->m_body->m_codegen;
            if (!(*cgen)((ast_expression*)(self->m_body), func, false, &dummy))
                return false;
        }

        end_bbody = func->m_curblock;
        func->m_breakblocks.pop_back();
        func->m_continueblocks.pop_back();
    }

    /* post-loop-condition */
    if (self->m_postcond)
    {
        /* enter */
        func->m_curblock = bpostcond;

        /* generate */
        cgen = self->m_postcond->m_codegen;
        if (!(*cgen)((ast_expression*)(self->m_postcond), func, false, &postcond))
            return false;

        end_bpostcond = func->m_curblock;
    }

    /* The incrementor */
    if (self->m_increment)
    {
        /* enter */
        func->m_curblock = bincrement;

        /* generate */
        cgen = self->m_increment->m_codegen;
        if (!(*cgen)((ast_expression*)(self->m_increment), func, false, &dummy))
            return false;

        end_bincrement = func->m_curblock;
    }

    /* In any case now, we continue from the outgoing block */
    func->m_curblock = bout;

    /* Now all blocks are in place */
    /* From 'bin' we jump to whatever comes first */
    if      (bprecond)   tmpblock = bprecond;
    else                 tmpblock = bbody;    /* can never be null */

    /* DEAD CODE
    else if (bpostcond)  tmpblock = bpostcond;
    else                 tmpblock = bout;
    */

    if (!ir_block_create_jump(bin, self->m_context, tmpblock))
        return false;

    /* From precond */
    if (bprecond)
    {
        ir_block *ontrue, *onfalse;
        ontrue = bbody; /* can never be null */

        /* all of this is dead code
        else if (bincrement) ontrue = bincrement;
        else                 ontrue = bpostcond;
        */

        onfalse = bout;
        if (self->m_pre_not) {
            tmpblock = ontrue;
            ontrue   = onfalse;
            onfalse  = tmpblock;
        }
        if (!ir_block_create_if(end_bprecond, self->m_context, precond, ontrue, onfalse))
            return false;
    }

    /* from body */
    if (bbody)
    {
        if      (bincrement) tmpblock = bincrement;
        else if (bpostcond)  tmpblock = bpostcond;
        else if (bprecond)   tmpblock = bprecond;
        else                 tmpblock = bbody;
        if (!end_bbody->m_final && !ir_block_create_jump(end_bbody, self->m_context, tmpblock))
            return false;
    }

    /* from increment */
    if (bincrement)
    {
        if      (bpostcond)  tmpblock = bpostcond;
        else if (bprecond)   tmpblock = bprecond;
        else if (bbody)      tmpblock = bbody;
        else                 tmpblock = bout;
        if (!ir_block_create_jump(end_bincrement, self->m_context, tmpblock))
            return false;
    }

    /* from postcond */
    if (bpostcond)
    {
        ir_block *ontrue, *onfalse;
        if      (bprecond)   ontrue = bprecond;
        else                 ontrue = bbody; /* can never be null */

        /* all of this is dead code
        else if (bincrement) ontrue = bincrement;
        else                 ontrue = bpostcond;
        */

        onfalse = bout;
        if (self->m_post_not) {
            tmpblock = ontrue;
            ontrue   = onfalse;
            onfalse  = tmpblock;
        }
        if (!ir_block_create_if(end_bpostcond, self->m_context, postcond, ontrue, onfalse))
            return false;
    }

    /* Move 'bout' to the end */
    algo::shiftback(func->m_ir_func->m_blocks.begin() + bout_id,
                    func->m_ir_func->m_blocks.end());
    // FIXME::DELME::
    //func->m_ir_func->m_blocks[bout_id].release(); // it's a vector<unique_ptr<>>
    //func->m_ir_func->m_blocks.erase(func->m_ir_func->m_blocks.begin() + bout_id);
    //func->m_ir_func->m_blocks.emplace_back(bout);

    return true;
}

bool ast_breakcont_codegen(ast_breakcont *self, ast_function *func, bool lvalue, ir_value **out)
{
    ir_block *target;

    *out = nullptr;

    if (lvalue) {
        compile_error(self->m_context, "break/continue expression is not an l-value");
        return false;
    }

    if (self->m_outr) {
        compile_error(self->m_context, "internal error: ast_breakcont cannot be reused!");
        return false;
    }
    self->m_outr = (ir_value*)1;

    if (self->m_is_continue)
        target = func->m_continueblocks[func->m_continueblocks.size()-1-self->m_levels];
    else
        target = func->m_breakblocks[func->m_breakblocks.size()-1-self->m_levels];

    if (!target) {
        compile_error(self->m_context, "%s is lacking a target block", (self->m_is_continue ? "continue" : "break"));
        return false;
    }

    if (!ir_block_create_jump(func->m_curblock, self->m_context, target))
        return false;
    return true;
}

bool ast_switch_codegen(ast_switch *self, ast_function *func, bool lvalue, ir_value **out)
{
    ast_expression_codegen *cgen;

    ast_switch_case *def_case     = nullptr;
    ir_block        *def_bfall    = nullptr;
    ir_block        *def_bfall_to = nullptr;
    bool set_def_bfall_to = false;

    ir_value *dummy     = nullptr;
    ir_value *irop      = nullptr;
    ir_block *bout      = nullptr;
    ir_block *bfall     = nullptr;
    size_t    bout_id;

    char      typestr[1024];
    uint16_t  cmpinstr;

    if (lvalue) {
        compile_error(self->m_context, "switch expression is not an l-value");
        return false;
    }

    if (self->m_outr) {
        compile_error(self->m_context, "internal error: ast_switch cannot be reused!");
        return false;
    }
    self->m_outr = (ir_value*)1;

    (void)lvalue;
    (void)out;

    cgen = self->m_operand->m_codegen;
    if (!(*cgen)((ast_expression*)(self->m_operand), func, false, &irop))
        return false;

    if (self->m_cases.empty())
        return true;

    cmpinstr = type_eq_instr[irop->m_vtype];
    if (cmpinstr >= VINSTR_END) {
        ast_type_to_string(self->m_operand, typestr, sizeof(typestr));
        compile_error(self->m_context, "invalid type to perform a switch on: %s", typestr);
        return false;
    }

    bout_id = func->m_ir_func->m_blocks.size();
    bout = ir_function_create_block(self->m_context, func->m_ir_func, ast_function_label(func, "after_switch"));
    if (!bout)
        return false;

    /* setup the break block */
    func->m_breakblocks.push_back(bout);

    /* Now create all cases */
    for (auto &it : self->m_cases) {
        ir_value *cond, *val;
        ir_block *bcase, *bnot;
        size_t bnot_id;

        ast_switch_case *swcase = &it;

        if (swcase->m_value) {
            /* A regular case */
            /* generate the condition operand */
            cgen = swcase->m_value->m_codegen;
            if (!(*cgen)((ast_expression*)(swcase->m_value), func, false, &val))
                return false;
            /* generate the condition */
            cond = ir_block_create_binop(func->m_curblock, self->m_context, ast_function_label(func, "switch_eq"), cmpinstr, irop, val);
            if (!cond)
                return false;

            bcase = ir_function_create_block(self->m_context, func->m_ir_func, ast_function_label(func, "case"));
            bnot_id = func->m_ir_func->m_blocks.size();
            bnot = ir_function_create_block(self->m_context, func->m_ir_func, ast_function_label(func, "not_case"));
            if (!bcase || !bnot)
                return false;
            if (set_def_bfall_to) {
                set_def_bfall_to = false;
                def_bfall_to = bcase;
            }
            if (!ir_block_create_if(func->m_curblock, self->m_context, cond, bcase, bnot))
                return false;

            /* Make the previous case-end fall through */
            if (bfall && !bfall->m_final) {
                if (!ir_block_create_jump(bfall, self->m_context, bcase))
                    return false;
            }

            /* enter the case */
            func->m_curblock = bcase;
            cgen = swcase->m_code->m_codegen;
            if (!(*cgen)((ast_expression*)swcase->m_code, func, false, &dummy))
                return false;

            /* remember this block to fall through from */
            bfall = func->m_curblock;

            /* enter the else and move it down */
            func->m_curblock = bnot;
            algo::shiftback(func->m_ir_func->m_blocks.begin() + bnot_id,
                            func->m_ir_func->m_blocks.end());
            // FIXME::DELME::
            //func->m_ir_func->m_blocks[bnot_id].release();
            //func->m_ir_func->m_blocks.erase(func->m_ir_func->m_blocks.begin() + bnot_id);
            //func->m_ir_func->m_blocks.emplace_back(bnot);
        } else {
            /* The default case */
            /* Remember where to fall through from: */
            def_bfall = bfall;
            bfall     = nullptr;
            /* remember which case it was */
            def_case  = swcase;
            /* And the next case will be remembered */
            set_def_bfall_to = true;
        }
    }

    /* Jump from the last bnot to bout */
    if (bfall && !bfall->m_final && !ir_block_create_jump(bfall, self->m_context, bout)) {
        /*
        astwarning(bfall->m_context, WARN_???, "missing break after last case");
        */
        return false;
    }

    /* If there was a default case, put it down here */
    if (def_case) {
        ir_block *bcase;

        /* No need to create an extra block */
        bcase = func->m_curblock;

        /* Insert the fallthrough jump */
        if (def_bfall && !def_bfall->m_final) {
            if (!ir_block_create_jump(def_bfall, self->m_context, bcase))
                return false;
        }

        /* Now generate the default code */
        cgen = def_case->m_code->m_codegen;
        if (!(*cgen)((ast_expression*)def_case->m_code, func, false, &dummy))
            return false;

        /* see if we need to fall through */
        if (def_bfall_to && !func->m_curblock->m_final)
        {
            if (!ir_block_create_jump(func->m_curblock, self->m_context, def_bfall_to))
                return false;
        }
    }

    /* Jump from the last bnot to bout */
    if (!func->m_curblock->m_final && !ir_block_create_jump(func->m_curblock, self->m_context, bout))
        return false;
    /* enter the outgoing block */
    func->m_curblock = bout;

    /* restore the break block */
    func->m_breakblocks.pop_back();

    /* Move 'bout' to the end, it's nicer */
    algo::shiftback(func->m_ir_func->m_blocks.begin() + bout_id,
                    func->m_ir_func->m_blocks.end());
    // FIXME::DELME::
    //func->m_ir_func->m_blocks[bout_id].release();
    //func->m_ir_func->m_blocks.erase(func->m_ir_func->m_blocks.begin() + bout_id);
    //func->m_ir_func->m_blocks.emplace_back(bout);

    return true;
}

bool ast_label_codegen(ast_label *self, ast_function *func, bool lvalue, ir_value **out)
{
    ir_value *dummy;

    if (self->m_undefined) {
        compile_error(self->m_context, "internal error: ast_label never defined");
        return false;
    }

    *out = nullptr;
    if (lvalue) {
        compile_error(self->m_context, "internal error: ast_label cannot be an lvalue");
        return false;
    }

    /* simply create a new block and jump to it */
    self->m_irblock = ir_function_create_block(self->m_context, func->m_ir_func, self->m_name);
    if (!self->m_irblock) {
        compile_error(self->m_context, "failed to allocate label block `%s`", self->m_name);
        return false;
    }
    if (!func->m_curblock->m_final) {
        if (!ir_block_create_jump(func->m_curblock, self->m_context, self->m_irblock))
            return false;
    }

    /* enter the new block */
    func->m_curblock = self->m_irblock;

    /* Generate all the leftover gotos */
    for (auto &it : self->m_gotos) {
        if (!ast_goto_codegen(it, func, false, &dummy))
            return false;
    }

    return true;
}

bool ast_goto_codegen(ast_goto *self, ast_function *func, bool lvalue, ir_value **out)
{
    *out = nullptr;
    if (lvalue) {
        compile_error(self->m_context, "internal error: ast_goto cannot be an lvalue");
        return false;
    }

    if (self->m_target->m_irblock) {
        if (self->m_irblock_from) {
            /* we already tried once, this is the callback */
            self->m_irblock_from->m_final = false;
            if (!ir_block_create_goto(self->m_irblock_from, self->m_context, self->m_target->m_irblock)) {
                compile_error(self->m_context, "failed to generate goto to `%s`", self->m_name);
                return false;
            }
        }
        else
        {
            if (!ir_block_create_goto(func->m_curblock, self->m_context, self->m_target->m_irblock)) {
                compile_error(self->m_context, "failed to generate goto to `%s`", self->m_name);
                return false;
            }
        }
    }
    else
    {
        /* the target has not yet been created...
         * close this block in a sneaky way:
         */
        func->m_curblock->m_final = true;
        self->m_irblock_from = func->m_curblock;
        ast_label_register_goto(self->m_target, self);
    }

    return true;
}

#include <stdio.h>
bool ast_state_codegen(ast_state *self, ast_function *func, bool lvalue, ir_value **out)
{
    ast_expression_codegen *cgen;

    ir_value *frameval, *thinkval;

    if (lvalue) {
        compile_error(self->m_context, "not an l-value (state operation)");
        return false;
    }
    if (self->m_outr) {
        compile_error(self->m_context, "internal error: ast_state cannot be reused!");
        return false;
    }
    *out = nullptr;

    cgen = self->m_framenum->m_codegen;
    if (!(*cgen)((ast_expression*)(self->m_framenum), func, false, &frameval))
        return false;
    if (!frameval)
        return false;

    cgen = self->m_nextthink->m_codegen;
    if (!(*cgen)((ast_expression*)(self->m_nextthink), func, false, &thinkval))
        return false;
    if (!frameval)
        return false;

    if (!ir_block_create_state_op(func->m_curblock, self->m_context, frameval, thinkval)) {
        compile_error(self->m_context, "failed to create STATE instruction");
        return false;
    }

    self->m_outr = (ir_value*)1;
    return true;
}

bool ast_call_codegen(ast_call *self, ast_function *func, bool lvalue, ir_value **out)
{
    ast_expression_codegen *cgen;
    std::vector<ir_value*> params;
    ir_instr *callinstr;

    ir_value *funval = nullptr;

    /* return values are never lvalues */
    if (lvalue) {
        compile_error(self->m_context, "not an l-value (function call)");
        return false;
    }

    if (self->m_outr) {
        *out = self->m_outr;
        return true;
    }

    cgen = self->m_func->m_codegen;
    if (!(*cgen)((ast_expression*)(self->m_func), func, false, &funval))
        return false;
    if (!funval)
        return false;

    /* parameters */
    for (auto &it : self->m_params) {
        ir_value *param;
        cgen = it->m_codegen;
        if (!(*cgen)(it, func, false, &param))
            return false;
        if (!param)
            return false;
        params.push_back(param);
    }

    /* varargs counter */
    if (self->m_va_count) {
        ir_value   *va_count;
        ir_builder *builder = func->m_curblock->m_owner->m_owner;
        cgen = self->m_va_count->m_codegen;
        if (!(*cgen)((ast_expression*)(self->m_va_count), func, false, &va_count))
            return false;
        if (!ir_block_create_store_op(func->m_curblock, self->m_context, INSTR_STORE_F,
                                      ir_builder_get_va_count(builder), va_count))
        {
            return false;
        }
    }

    callinstr = ir_block_create_call(func->m_curblock, self->m_context,
                                     ast_function_label(func, "call"),
                                     funval, !!(self->m_func->m_flags & AST_FLAG_NORETURN));
    if (!callinstr)
        return false;

    for (auto &it : params)
        ir_call_param(callinstr, it);

    *out = ir_call_value(callinstr);
    self->m_outr = *out;

    codegen_output_type(self, *out);

    return true;
}
