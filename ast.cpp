#include <new>

#include <stdlib.h>
#include <string.h>

#include "gmqcc.h"
#include "ast.h"
#include "fold.h"
//#include "parser.h"

#include "algo.h"

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
void ast_node::propagateSideEffects(const ast_node *other)
{
    if (other->m_side_effects)
        m_side_effects = true;
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

ast_expression::ast_expression(ast_copy_type_t, const ast_expression &other)
    : ast_expression(ast_copy_type, other.m_context, other)
{}

ast_expression::ast_expression(ast_copy_type_t, lex_ctx_t ctx, const ast_expression &other)
    : ast_expression(ast_copy_type, TYPE_ast_expression, ctx, other)
{}

ast_expression::ast_expression(ast_copy_type_t, int nodetype, const ast_expression &other)
    : ast_expression(ast_copy_type, nodetype, other.m_context, other)
{}

ast_expression::ast_expression(ast_copy_type_t, int nodetype, lex_ctx_t ctx, const ast_expression &other)
    : ast_expression(ctx, nodetype)
{
    m_vtype = other.m_vtype;
    m_count = other.m_count;
    m_flags = other.m_flags;
    if (other.m_next)
        m_next = new ast_expression(ast_copy_type, *other.m_next);
    m_type_params.reserve(other.m_type_params.size());
    for (auto &it : other.m_type_params)
        m_type_params.emplace_back(new ast_value(ast_copy_type, *it));
}


ast_expression *ast_expression::shallowType(lex_ctx_t ctx, qc_type vtype) {
    auto expr = new ast_expression(ctx, TYPE_ast_expression);
    expr->m_vtype = vtype;
    return expr;
}

void ast_expression::adoptType(const ast_expression &other)
{
    m_vtype = other.m_vtype;
    if (other.m_next)
        m_next = new ast_expression(ast_copy_type, *other.m_next);
    m_count = other.m_count;
    m_flags = other.m_flags;
    m_type_params.clear();
    m_type_params.reserve(other.m_type_params.size());
    for (auto &it : other.m_type_params)
        m_type_params.emplace_back(new ast_value(ast_copy_type, *it));
}

bool ast_expression::compareType(const ast_expression &other) const
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
            if (!m_type_params[i]->compareType(*other.m_type_params[i]))
                return false;
        }
    }
    if (m_next)
        return m_next->compareType(*other.m_next);
    return true;
}

bool ast_expression::codegen(ast_function*, bool, ir_value**) {
    compile_error(m_context, "ast_expression::codegen called!");
    abort();
    return false;
}

ast_value::ast_value(ast_copy_type_t, const ast_value &other, const std::string &name)
    : ast_value(ast_copy_type, static_cast<const ast_expression&>(other), name)
{
    m_keep_node = true; // keep values, always
    memset(&m_constval, 0, sizeof(m_constval));
}

ast_value::ast_value(ast_copy_type_t, const ast_value &other)
    : ast_value(ast_copy_type, static_cast<const ast_expression&>(other), other.m_name)
{
    m_keep_node = true; // keep values, always
    memset(&m_constval, 0, sizeof(m_constval));
}

ast_value::ast_value(ast_copy_type_t, const ast_expression &other, const std::string &name)
    : ast_expression(ast_copy_type, TYPE_ast_value, other)
    , m_name(name)
{
    m_keep_node = true; // keep values, always
    memset(&m_constval, 0, sizeof(m_constval));
}

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

void ast_value::addParam(ast_value *p)
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

    propagateSideEffects(left);
    propagateSideEffects(right);

    if (op >= INSTR_EQ_F && op <= INSTR_GT)
        m_vtype = TYPE_FLOAT;
    else if (op == INSTR_AND || op == INSTR_OR) {
        if (OPTS_FLAG(PERL_LOGIC))
            adoptType(*right);
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
    adoptType(*left);
}

ast_binstore::~ast_binstore()
{
    if (!m_keep_dest)
        ast_unref(m_dest);
    ast_unref(m_source);
}

ast_unary* ast_unary::make(lex_ctx_t ctx, int op, ast_expression *expr)
{
    // handle double negation, double bitwise or logical not
    if (op == opid2('!','P') ||
        op == opid2('~','P') ||
        op == opid2('-','P'))
    {
        if (ast_istype(expr, ast_unary) && OPTS_OPTIMIZATION(OPTIM_PEEPHOLE)) {
            ast_unary *unary = reinterpret_cast<ast_unary*>(expr);
            if (unary->m_op == op) {
                auto out = reinterpret_cast<ast_unary*>(unary->m_operand);
                unary->m_operand = nullptr;
                delete unary;
                ++opts_optimizationcount[OPTIM_PEEPHOLE];
                return out;
            }
        }
    }

    return new ast_unary(ctx, op, expr);
}

ast_unary::ast_unary(lex_ctx_t ctx, int op, ast_expression *expr)
    : ast_expression(ctx, TYPE_ast_unary)
    , m_op(op)
    , m_operand(expr)
{
    propagateSideEffects(expr);
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
        propagateSideEffects(expr);
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
    propagateSideEffects(m_entity);
    propagateSideEffects(m_field);

    if (!outtype) {
        compile_error(ctx, "ast_entfield: field has no type");
        m_vtype = TYPE_VOID;
    }
    else
        adoptType(*outtype);
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
        m_next = ast_expression::shallowType(ctx, TYPE_FLOAT);
    }

    propagateSideEffects(owner);
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
    propagateSideEffects(array);
    propagateSideEffects(index);

    ast_expression *outtype = m_array->m_next;
    adoptType(*outtype);

    if (array->m_vtype == TYPE_FIELD && outtype->m_vtype == TYPE_ARRAY) {
        // FIXME: investigate - this is not possible after adoptType
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
    adoptType(*dest);
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
    propagateSideEffects(cond);
    if (ontrue)
        propagateSideEffects(ontrue);
    if (onfalse)
        propagateSideEffects(onfalse);
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
    propagateSideEffects(cond);
    propagateSideEffects(ontrue);
    propagateSideEffects(onfalse);

    if (ontrue->m_vtype == TYPE_NIL)
        adoptType(*onfalse);
    else
        adoptType(*ontrue);
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
    : ast_expression(ctx, TYPE_ast_loop)
    , m_initexpr(initexpr)
    , m_precond(precond)
    , m_postcond(postcond)
    , m_increment(increment)
    , m_body(body)
    , m_pre_not(pre_not)
    , m_post_not(post_not)
{
    if (initexpr)
        propagateSideEffects(initexpr);
    if (precond)
        propagateSideEffects(precond);
    if (postcond)
        propagateSideEffects(postcond);
    if (increment)
        propagateSideEffects(increment);
    if (body)
        propagateSideEffects(body);
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

ast_breakcont::ast_breakcont(lex_ctx_t ctx, bool iscont, unsigned int levels)
    : ast_expression(ctx, TYPE_ast_breakcont)
    , m_is_continue(iscont)
    , m_levels(levels)
{
}

ast_breakcont::~ast_breakcont()
{
}

ast_switch::ast_switch(lex_ctx_t ctx, ast_expression *op)
    : ast_expression(ctx, TYPE_ast_switch)
    , m_operand(op)
{
    propagateSideEffects(op);
}

ast_switch::~ast_switch()
{
    ast_unref(m_operand);

    for (auto &it : m_cases) {
        if (it.m_value)
            ast_unref(it.m_value);
        ast_unref(it.m_code);
    }
}

ast_label::ast_label(lex_ctx_t ctx, const std::string &name, bool undefined)
    : ast_expression(ctx, TYPE_ast_label)
    , m_name(name)
    , m_irblock(nullptr)
    , m_undefined(undefined)
{
    m_vtype = TYPE_NOEXPR;
}

ast_label::~ast_label()
{
}

void ast_label::registerGoto(ast_goto *g)
{
   m_gotos.push_back(g);
}

ast_goto::ast_goto(lex_ctx_t ctx, const std::string &name)
    : ast_expression(ctx, TYPE_ast_goto)
    , m_name(name)
    , m_target(nullptr)
    , m_irblock_from(nullptr)
{
}

ast_goto::~ast_goto()
{
}

void ast_goto::setLabel(ast_label *label)
{
    m_target = label;
}

ast_state::ast_state(lex_ctx_t ctx, ast_expression *frame, ast_expression *think)
    : ast_expression(ctx, TYPE_ast_expression)
    , m_framenum(frame)
    , m_nextthink(think)
{
}

ast_state::~ast_state()
{
    if (m_framenum)
        ast_unref(m_framenum);
    if (m_nextthink)
        ast_unref(m_nextthink);
}

ast_call *ast_call::make(lex_ctx_t ctx, ast_expression *funcexpr)
{
    if (!funcexpr->m_next) {
        compile_error(ctx, "not a function");
        return nullptr;
    }
    return new ast_call(ctx, funcexpr);
}

ast_call::ast_call(lex_ctx_t ctx, ast_expression *funcexpr)
    : ast_expression(ctx, TYPE_ast_call)
    , m_func(funcexpr)
    , m_va_count(nullptr)
{
    m_side_effects = true;
    adoptType(*funcexpr->m_next);
}

ast_call::~ast_call()
{
    for (auto &it : m_params)
        ast_unref(it);

    if (m_func)
        ast_unref(m_func);

    if (m_va_count)
        ast_unref(m_va_count);
}

bool ast_call::checkVararg(ast_expression *va_type, ast_expression *exp_type) const
{
    char texp[1024];
    char tgot[1024];
    if (!exp_type)
        return true;
    if (!va_type || !va_type->compareType(*exp_type))
    {
        if (va_type && exp_type)
        {
            ast_type_to_string(va_type,  tgot, sizeof(tgot));
            ast_type_to_string(exp_type, texp, sizeof(texp));
            if (OPTS_FLAG(UNSAFE_VARARGS)) {
                if (compile_warning(m_context, WARN_UNSAFE_TYPES,
                                    "piped variadic argument differs in type: constrained to type %s, expected type %s",
                                    tgot, texp))
                    return false;
            } else {
                compile_error(m_context,
                              "piped variadic argument differs in type: constrained to type %s, expected type %s",
                              tgot, texp);
                return false;
            }
        }
        else
        {
            ast_type_to_string(exp_type, texp, sizeof(texp));
            if (OPTS_FLAG(UNSAFE_VARARGS)) {
                if (compile_warning(m_context, WARN_UNSAFE_TYPES,
                                    "piped variadic argument may differ in type: expected type %s",
                                    texp))
                    return false;
            } else {
                compile_error(m_context,
                              "piped variadic argument may differ in type: expected type %s",
                              texp);
                return false;
            }
        }
    }
    return true;
}

bool ast_call::checkTypes(ast_expression *va_type) const
{
    char texp[1024];
    char tgot[1024];
    size_t i;
    bool retval = true;

    size_t count = m_params.size();
    if (count > m_func->m_type_params.size())
        count = m_func->m_type_params.size();

    for (i = 0; i < count; ++i) {
        if (ast_istype(m_params[i], ast_argpipe)) {
            /* warn about type safety instead */
            if (i+1 != count) {
                compile_error(m_context, "argpipe must be the last parameter to a function call");
                return false;
            }
            if (!checkVararg(va_type, m_func->m_type_params[i].get()))
                retval = false;
        }
        else if (!m_params[i]->compareType(*m_func->m_type_params[i]))
        {
            ast_type_to_string(m_params[i], tgot, sizeof(tgot));
            ast_type_to_string(m_func->m_type_params[i].get(), texp, sizeof(texp));
            compile_error(m_context, "invalid type for parameter %u in function call: expected %s, got %s",
                     (unsigned int)(i+1), texp, tgot);
            /* we don't immediately return */
            retval = false;
        }
    }
    count = m_params.size();
    if (count > m_func->m_type_params.size() && m_func->m_varparam) {
        for (; i < count; ++i) {
            if (ast_istype(m_params[i], ast_argpipe)) {
                /* warn about type safety instead */
                if (i+1 != count) {
                    compile_error(m_context, "argpipe must be the last parameter to a function call");
                    return false;
                }
                if (!checkVararg(va_type, m_func->m_varparam))
                    retval = false;
            }
            else if (!m_params[i]->compareType(*m_func->m_varparam))
            {
                ast_type_to_string(m_params[i], tgot, sizeof(tgot));
                ast_type_to_string(m_func->m_varparam, texp, sizeof(texp));
                compile_error(m_context, "invalid type for variadic parameter %u in function call: expected %s, got %s",
                         (unsigned int)(i+1), texp, tgot);
                /* we don't immediately return */
                retval = false;
            }
        }
    }
    return retval;
}

ast_block::ast_block(lex_ctx_t ctx)
    : ast_expression(ctx, TYPE_ast_block)
{
}

ast_block::~ast_block()
{
    for (auto &it : m_exprs) ast_unref(it);
    for (auto &it : m_locals) delete it;
    for (auto &it : m_collect) delete it;
}

void ast_block::setType(const ast_expression &from)
{
    if (m_next)
        delete m_next;
    adoptType(from);
}


bool ast_block::addExpr(ast_expression *e)
{
    propagateSideEffects(e);
    m_exprs.push_back(e);
    if (m_next) {
        delete m_next;
        m_next = nullptr;
    }
    adoptType(*e);
    return true;
}

void ast_block::collect(ast_expression *expr)
{
    m_collect.push_back(expr);
    expr->m_keep_node = true;
}

ast_function *ast_function::make(lex_ctx_t ctx, const std::string &name, ast_value *vtype)
{
    if (!vtype) {
        compile_error(ctx, "internal error: ast_function_new condition 0");
        return nullptr;
    } else if (vtype->m_hasvalue || vtype->m_vtype != TYPE_FUNCTION) {
        compile_error(ctx, "internal error: ast_function_new condition %i %i type=%i (probably 2 bodies?)",
                 (int)!vtype,
                 (int)vtype->m_hasvalue,
                 vtype->m_vtype);
        return nullptr;
    }
    return new ast_function(ctx, name, vtype);
}

ast_function::ast_function(lex_ctx_t ctx, const std::string &name, ast_value *vtype)
    : ast_node(ctx, TYPE_ast_function)
    , m_function_type(vtype)
    , m_name(name)
    , m_builtin(0)
    , m_static_count(0)
    , m_ir_func(nullptr)
    , m_curblock(nullptr)
    , m_labelcount(0)
    , m_varargs(nullptr)
    , m_argc(nullptr)
    , m_fixedparams(nullptr)
    , m_return_value(nullptr)
{
    vtype->m_hasvalue = true;
    vtype->m_constval.vfunc = this;
}

ast_function::~ast_function()
{
    if (m_function_type) {
        // ast_value_delete(m_function_type);
        m_function_type->m_hasvalue = false;
        m_function_type->m_constval.vfunc = nullptr;
        // We use unref - if it was stored in a global table it is supposed
        // to be deleted from *there*
        ast_unref(m_function_type);
    }

    if (m_fixedparams)
        ast_unref(m_fixedparams);
    if (m_return_value)
        ast_unref(m_return_value);

    // force this to be cleared before m_varargs/m_argc as blocks might
    // try to access them via ast_unref()
    m_blocks.clear();
}

const char* ast_function::makeLabel(const char *prefix)
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

    id  = (m_labelcount++);
    len = strlen(prefix);

    from = m_labelbuf + sizeof(m_labelbuf)-1;
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

static void codegen_output_type(ast_expression *self, ir_value *out)
{
    if (out->m_vtype == TYPE_FIELD)
        out->m_fieldtype = self->m_next->m_vtype;
    if (out->m_vtype == TYPE_FUNCTION)
        out->m_outtype = self->m_next->m_vtype;
}

bool ast_value::codegen(ast_function *func, bool lvalue, ir_value **out)
{
    (void)func;
    (void)lvalue;
    if (m_vtype == TYPE_NIL) {
        *out = func->m_ir_func->m_owner->m_nil;
        return true;
    }
    // NOTE: This is the codegen for a variable used in an expression.
    // It is not the codegen to generate the value storage. For this purpose,
    // generateLocal and generateGlobal are to be used before this
    // is executed. ast_function::generateFunction should take care of its
    // locals, and the ast-user should take care of generateGlobal to be used
    // on all the globals.
    if (!m_ir_v) {
        char tname[1024]; /* typename is reserved in C++ */
        ast_type_to_string(this, tname, sizeof(tname));
        compile_error(m_context, "ast_value used before generated %s %s", tname, m_name);
        return false;
    }
    *out = m_ir_v;
    return true;
}

bool ast_value::setGlobalArray()
{
    size_t count = m_initlist.size();
    size_t i;

    if (count > m_count) {
        compile_error(m_context, "too many elements in initializer");
        count = m_count;
    }
    else if (count < m_count) {
        /* add this?
        compile_warning(m_context, "not all elements are initialized");
        */
    }

    for (i = 0; i != count; ++i) {
        switch (m_next->m_vtype) {
            case TYPE_FLOAT:
                if (!m_ir_values[i]->setFloat(m_initlist[i].vfloat))
                    return false;
                break;
            case TYPE_VECTOR:
                if (!m_ir_values[i]->setVector(m_initlist[i].vvec))
                    return false;
                break;
            case TYPE_STRING:
                if (!m_ir_values[i]->setString(m_initlist[i].vstring))
                    return false;
                break;
            case TYPE_ARRAY:
                /* we don't support them in any other place yet either */
                compile_error(m_context, "TODO: nested arrays");
                return false;
            case TYPE_FUNCTION:
                /* this requiers a bit more work - similar to the fields I suppose */
                compile_error(m_context, "global of type function not properly generated");
                return false;
            case TYPE_FIELD:
                if (!m_initlist[i].vfield) {
                    compile_error(m_context, "field constant without vfield set");
                    return false;
                }
                if (!m_initlist[i].vfield->m_ir_v) {
                    compile_error(m_context, "field constant generated before its field");
                    return false;
                }
                if (!m_ir_values[i]->setField(m_initlist[i].vfield->m_ir_v))
                    return false;
                break;
            default:
                compile_error(m_context, "TODO: global constant type %i", m_vtype);
                break;
        }
    }
    return true;
}

bool ast_value::checkArray(const ast_value &array) const
{
    if (array.m_flags & AST_FLAG_ARRAY_INIT && array.m_initlist.empty()) {
        compile_error(m_context, "array without size: %s", m_name);
        return false;
    }
    // we are lame now - considering the way QC works we won't tolerate arrays > 1024 elements
    if (!array.m_count || array.m_count > OPTS_OPTION_U32(OPTION_MAX_ARRAY_SIZE)) {
        compile_error(m_context, "Invalid array of size %lu", (unsigned long)array.m_count);
        return false;
    }
    return true;
}

bool ast_value::generateGlobal(ir_builder *ir, bool isfield)
{
    if (m_vtype == TYPE_NIL) {
        compile_error(m_context, "internal error: trying to generate a variable of TYPE_NIL");
        return false;
    }

    if (m_hasvalue && m_vtype == TYPE_FUNCTION)
        return generateGlobalFunction(ir);

    if (isfield && m_vtype == TYPE_FIELD)
        return generateGlobalField(ir);

    ir_value *v = nullptr;
    if (m_vtype == TYPE_ARRAY) {
        v = prepareGlobalArray(ir);
        if (!v)
            return false;
    } else {
        // Arrays don't do this since there's no "array" value which spans across the
        // whole thing.
        v = ir->createGlobal(m_name, m_vtype);
        if (!v) {
            compile_error(m_context, "ir_builder::createGlobal failed on `%s`", m_name);
            return false;
        }
        codegen_output_type(this, v);
        v->m_context = m_context;
    }

    /* link us to the ir_value */
    v->m_cvq = m_cvq;
    m_ir_v = v;

    if (m_flags & AST_FLAG_INCLUDE_DEF)
        m_ir_v->m_flags |= IR_FLAG_INCLUDE_DEF;
    if (m_flags & AST_FLAG_ERASEABLE)
        m_ir_v->m_flags |= IR_FLAG_ERASABLE;
    if (m_flags & AST_FLAG_NOREF)
        m_ir_v->m_flags |= IR_FLAG_NOREF;

    /* initialize */
    if (m_hasvalue) {
        switch (m_vtype)
        {
            case TYPE_FLOAT:
                if (!v->setFloat(m_constval.vfloat))
                    return false;
                break;
            case TYPE_VECTOR:
                if (!v->setVector(m_constval.vvec))
                    return false;
                break;
            case TYPE_STRING:
                if (!v->setString(m_constval.vstring))
                    return false;
                break;
            case TYPE_ARRAY:
                if (!setGlobalArray())
                    return false;
                break;
            case TYPE_FUNCTION:
                compile_error(m_context, "global of type function not properly generated");
                return false;
                /* Cannot generate an IR value for a function,
                 * need a pointer pointing to a function rather.
                 */
            case TYPE_FIELD:
                if (!m_constval.vfield) {
                    compile_error(m_context, "field constant without vfield set");
                    return false;
                }
                if (!m_constval.vfield->m_ir_v) {
                    compile_error(m_context, "field constant generated before its field");
                    return false;
                }
                if (!v->setField(m_constval.vfield->m_ir_v))
                    return false;
                break;
            default:
                compile_error(m_context, "TODO: global constant type %i", m_vtype);
                break;
        }
    }

    return true;
}

bool ast_value::generateGlobalFunction(ir_builder *ir)
{
    ir_function *func = ir->createFunction(m_name, m_next->m_vtype);
    if (!func)
        return false;
    func->m_context = m_context;
    func->m_value->m_context = m_context;

    m_constval.vfunc->m_ir_func = func;
    m_ir_v = func->m_value;
    if (m_flags & AST_FLAG_INCLUDE_DEF)
        m_ir_v->m_flags |= IR_FLAG_INCLUDE_DEF;
    if (m_flags & AST_FLAG_ERASEABLE)
        m_ir_v->m_flags |= IR_FLAG_ERASABLE;
    if (m_flags & AST_FLAG_BLOCK_COVERAGE)
        func->m_flags |= IR_FLAG_BLOCK_COVERAGE;
    // The function is filled later on ast_function::generateFunction...
    return true;
}

bool ast_value::generateGlobalField(ir_builder *ir)
{
    ast_expression *fieldtype = m_next;

    if (m_hasvalue) {
        compile_error(m_context, "TODO: constant field pointers with value");
        return false;
    }

    if (fieldtype->m_vtype == TYPE_ARRAY) {
        if (!ast_istype(fieldtype, ast_value)) {
            compile_error(m_context, "internal error: ast_value required");
            return false;
        }
        ast_value      *array = reinterpret_cast<ast_value*>(fieldtype);

        if (!checkArray(*array))
            return false;

        ast_expression *elemtype = array->m_next;
        qc_type vtype = elemtype->m_vtype;

        ir_value *v = ir->createField(m_name, vtype);
        if (!v) {
            compile_error(m_context, "ir_builder::createGlobal failed on `%s`", m_name);
            return false;
        }
        v->m_context = m_context;
        v->m_unique_life = true;
        v->m_locked      = true;
        array->m_ir_v = m_ir_v = v;

        if (m_flags & AST_FLAG_INCLUDE_DEF)
            m_ir_v->m_flags |= IR_FLAG_INCLUDE_DEF;
        if (m_flags & AST_FLAG_ERASEABLE)
            m_ir_v->m_flags |= IR_FLAG_ERASABLE;
        if (m_flags & AST_FLAG_NOREF)
            m_ir_v->m_flags |= IR_FLAG_NOREF;

        const size_t namelen = m_name.length();
        std::unique_ptr<char[]> name(new char[namelen+16]);
        util_strncpy(name.get(), m_name.c_str(), namelen);

        array->m_ir_values.resize(array->m_count);
        array->m_ir_values[0] = v;
        for (size_t ai = 1; ai < array->m_count; ++ai) {
            util_snprintf(name.get() + namelen, 16, "[%u]", (unsigned int)ai);
            array->m_ir_values[ai] = ir->createField(name.get(), vtype);
            if (!array->m_ir_values[ai]) {
                compile_error(m_context, "ir_builder::createGlobal failed on `%s`", name.get());
                return false;
            }
            array->m_ir_values[ai]->m_context = m_context;
            array->m_ir_values[ai]->m_unique_life = true;
            array->m_ir_values[ai]->m_locked      = true;
            if (m_flags & AST_FLAG_INCLUDE_DEF)
                array->m_ir_values[ai]->m_flags |= IR_FLAG_INCLUDE_DEF;
            if (m_flags & AST_FLAG_NOREF)
                array->m_ir_values[ai]->m_flags |= IR_FLAG_NOREF;
        }
    }
    else
    {
        ir_value *v = ir->createField(m_name, m_next->m_vtype);
        if (!v)
            return false;
        v->m_context = m_context;
        m_ir_v = v;
        if (m_flags & AST_FLAG_INCLUDE_DEF)
            m_ir_v->m_flags |= IR_FLAG_INCLUDE_DEF;
        if (m_flags & AST_FLAG_ERASEABLE)
            m_ir_v->m_flags |= IR_FLAG_ERASABLE;
        if (m_flags & AST_FLAG_NOREF)
            m_ir_v->m_flags |= IR_FLAG_NOREF;
    }
    return true;
}

ir_value *ast_value::prepareGlobalArray(ir_builder *ir)
{
    ast_expression *elemtype = m_next;
    qc_type vtype = elemtype->m_vtype;

    if (m_flags & AST_FLAG_ARRAY_INIT && !m_count) {
        compile_error(m_context, "array `%s' has no size", m_name);
        return nullptr;
    }

    /* same as with field arrays */
    if (!checkArray(*this))
        return nullptr;

    ir_value *v = ir->createGlobal(m_name, vtype);
    if (!v) {
        compile_error(m_context, "ir_builder::createGlobal failed `%s`", m_name);
        return nullptr;
    }
    v->m_context = m_context;
    v->m_unique_life = true;
    v->m_locked      = true;

    if (m_flags & AST_FLAG_INCLUDE_DEF)
        v->m_flags |= IR_FLAG_INCLUDE_DEF;
    if (m_flags & AST_FLAG_ERASEABLE)
        v->m_flags |= IR_FLAG_ERASABLE;
    if (m_flags & AST_FLAG_NOREF)
        v->m_flags |= IR_FLAG_NOREF;

    const size_t namelen = m_name.length();
    std::unique_ptr<char[]> name(new char[namelen+16]);
    util_strncpy(name.get(), m_name.c_str(), namelen);

    m_ir_values.resize(m_count);
    m_ir_values[0] = v;
    for (size_t ai = 1; ai < m_count; ++ai) {
        util_snprintf(name.get() + namelen, 16, "[%u]", (unsigned int)ai);
        m_ir_values[ai] = ir->createGlobal(name.get(), vtype);
        if (!m_ir_values[ai]) {
            compile_error(m_context, "ir_builder::createGlobal failed `%s`", name.get());
            return nullptr;
        }
        m_ir_values[ai]->m_context = m_context;
        m_ir_values[ai]->m_unique_life = true;
        m_ir_values[ai]->m_locked      = true;
        if (m_flags & AST_FLAG_INCLUDE_DEF)
            m_ir_values[ai]->m_flags |= IR_FLAG_INCLUDE_DEF;
        if (m_flags & AST_FLAG_NOREF)
            m_ir_values[ai]->m_flags |= IR_FLAG_NOREF;
    }

    return v;
}

bool ast_value::generateLocal(ir_function *func, bool param)
{
    if (m_vtype == TYPE_NIL) {
        compile_error(m_context, "internal error: trying to generate a variable of TYPE_NIL");
        return false;
    }

    if (m_hasvalue && m_vtype == TYPE_FUNCTION)
    {
        /* Do we allow local functions? I think not...
         * this is NOT a function pointer atm.
         */
        return false;
    }

    ir_value *v = nullptr;
    if (m_vtype == TYPE_ARRAY) {
        ast_expression *elemtype = m_next;
        qc_type vtype = elemtype->m_vtype;

        func->m_flags |= IR_FLAG_HAS_ARRAYS;

        if (param && !(m_flags & AST_FLAG_IS_VARARG)) {
            compile_error(m_context, "array-parameters are not supported");
            return false;
        }

        /* we are lame now - considering the way QC works we won't tolerate arrays > 1024 elements */
        if (!checkArray(*this))
            return false;

        m_ir_values.resize(m_count);
        v = ir_function_create_local(func, m_name, vtype, param);
        if (!v) {
            compile_error(m_context, "internal error: ir_function_create_local failed");
            return false;
        }
        v->m_context = m_context;
        v->m_unique_life = true;
        v->m_locked      = true;

        if (m_flags & AST_FLAG_NOREF)
            v->m_flags |= IR_FLAG_NOREF;

        const size_t namelen = m_name.length();
        std::unique_ptr<char[]> name(new char[namelen+16]);
        util_strncpy(name.get(), m_name.c_str(), namelen);

        m_ir_values[0] = v;
        for (size_t ai = 1; ai < m_count; ++ai) {
            util_snprintf(name.get() + namelen, 16, "[%u]", (unsigned int)ai);
            m_ir_values[ai] = ir_function_create_local(func, name.get(), vtype, param);
            if (!m_ir_values[ai]) {
                compile_error(m_context, "internal_error: ir_builder::createGlobal failed on `%s`", name.get());
                return false;
            }
            m_ir_values[ai]->m_context = m_context;
            m_ir_values[ai]->m_unique_life = true;
            m_ir_values[ai]->m_locked = true;

            if (m_flags & AST_FLAG_NOREF)
                m_ir_values[ai]->m_flags |= IR_FLAG_NOREF;
        }
    }
    else
    {
        v = ir_function_create_local(func, m_name, m_vtype, param);
        if (!v)
            return false;
        codegen_output_type(this, v);
        v->m_context = m_context;
    }

    // A constant local... hmmm...
    // I suppose the IR will have to deal with this
    if (m_hasvalue) {
        switch (m_vtype)
        {
            case TYPE_FLOAT:
                if (!v->setFloat(m_constval.vfloat))
                    goto error;
                break;
            case TYPE_VECTOR:
                if (!v->setVector(m_constval.vvec))
                    goto error;
                break;
            case TYPE_STRING:
                if (!v->setString(m_constval.vstring))
                    goto error;
                break;
            default:
                compile_error(m_context, "TODO: global constant type %i", m_vtype);
                break;
        }
    }

    // link us to the ir_value
    v->m_cvq = m_cvq;
    m_ir_v = v;

    if (m_flags & AST_FLAG_NOREF)
        m_ir_v->m_flags |= IR_FLAG_NOREF;

    if (!generateAccessors(func->m_owner))
        return false;
    return true;

error: /* clean up */
    delete v;
    return false;
}

bool ast_value::generateAccessors(ir_builder *ir)
{
    size_t i;
    bool warn = OPTS_WARN(WARN_USED_UNINITIALIZED);
    if (!m_setter || !m_getter)
        return true;
    if (m_count && m_ir_values.empty()) {
        compile_error(m_context, "internal error: no array values generated for `%s`", m_name);
        return false;
    }
    for (i = 0; i < m_count; ++i) {
        if (!m_ir_values[i]) {
            compile_error(m_context, "internal error: not all array values have been generated for `%s`", m_name);
            return false;
        }
        if (!m_ir_values[i]->m_life.empty()) {
            compile_error(m_context, "internal error: function containing `%s` already generated", m_name);
            return false;
        }
    }

    opts_set(opts.warn, WARN_USED_UNINITIALIZED, false);
    if (m_setter) {
        if (!m_setter->generateGlobal(ir, false) ||
            !m_setter->m_constval.vfunc->generateFunction(ir) ||
            !ir_function_finalize(m_setter->m_constval.vfunc->m_ir_func))
        {
            compile_error(m_context, "internal error: failed to generate setter for `%s`", m_name);
            opts_set(opts.warn, WARN_USED_UNINITIALIZED, warn);
            return false;
        }
    }
    if (m_getter) {
        if (!m_getter->generateGlobal(ir, false) ||
            !m_getter->m_constval.vfunc->generateFunction(ir) ||
            !ir_function_finalize(m_getter->m_constval.vfunc->m_ir_func))
        {
            compile_error(m_context, "internal error: failed to generate getter for `%s`", m_name);
            opts_set(opts.warn, WARN_USED_UNINITIALIZED, warn);
            return false;
        }
    }
    for (i = 0; i < m_count; ++i)
        m_ir_values[i]->m_life.clear();
    opts_set(opts.warn, WARN_USED_UNINITIALIZED, warn);
    return true;
}

bool ast_function::generateFunction(ir_builder *ir)
{
    (void)ir;

    ir_value *dummy;

    ir_function *irf = m_ir_func;
    if (!irf) {
        compile_error(m_context, "internal error: ast_function's related ast_value was not generated yet");
        return false;
    }

    /* fill the parameter list */
    for (auto &it : m_function_type->m_type_params) {
        if (it->m_vtype == TYPE_FIELD)
            irf->m_params.push_back(it->m_next->m_vtype);
        else
            irf->m_params.push_back(it->m_vtype);
        if (!m_builtin) {
            if (!it->generateLocal(m_ir_func, true))
                return false;
        }
    }

    if (m_varargs) {
        if (!m_varargs->generateLocal(m_ir_func, true))
            return false;
        irf->m_max_varargs = m_varargs->m_count;
    }

    if (m_builtin) {
        irf->m_builtin = m_builtin;
        return true;
    }

    /* have a local return value variable? */
    if (m_return_value) {
        if (!m_return_value->generateLocal(m_ir_func, false))
            return false;
    }

    if (m_blocks.empty()) {
        compile_error(m_context, "function `%s` has no body", m_name);
        return false;
    }

    irf->m_first = m_curblock = ir_function_create_block(m_context, irf, "entry");
    if (!m_curblock) {
        compile_error(m_context, "failed to allocate entry block for `%s`", m_name);
        return false;
    }

    if (m_argc) {
        ir_value *va_count;
        ir_value *fixed;
        ir_value *sub;
        if (!m_argc->generateLocal(m_ir_func, true))
            return false;
        if (!m_argc->codegen(this, false, &va_count))
            return false;
        if (!m_fixedparams->codegen(this, false, &fixed))
            return false;
        sub = ir_block_create_binop(m_curblock, m_context,
                                    makeLabel("va_count"), INSTR_SUB_F,
                                    ir->get_va_count(), fixed);
        if (!sub)
            return false;
        if (!ir_block_create_store_op(m_curblock, m_context, INSTR_STORE_F,
                                      va_count, sub))
        {
            return false;
        }
    }

    for (auto &it : m_blocks) {
        if (!it->codegen(this, false, &dummy))
          return false;
    }

    /* TODO: check return types */
    if (!m_curblock->m_final)
    {
        if (!m_function_type->m_next ||
            m_function_type->m_next->m_vtype == TYPE_VOID)
        {
            return ir_block_create_return(m_curblock, m_context, nullptr);
        }
        else if (m_curblock->m_entries.size() || m_curblock == irf->m_first)
        {
            if (m_return_value) {
                if (!m_return_value->codegen(this, false, &dummy))
                    return false;
                return ir_block_create_return(m_curblock, m_context, dummy);
            }
            else if (compile_warning(m_context, WARN_MISSING_RETURN_VALUES,
                                "control reaches end of non-void function (`%s`) via %s",
                                m_name.c_str(), m_curblock->m_label.c_str()))
            {
                return false;
            }
            return ir_block_create_return(m_curblock, m_context, nullptr);
        }
    }
    return true;
}

static bool starts_a_label(const ast_expression *ex)
{
    while (ex && ast_istype(ex, ast_block)) {
        auto b = reinterpret_cast<const ast_block*>(ex);
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
bool ast_block::codegen(ast_function *func, bool lvalue, ir_value **out)
{
    /* We don't use this
     * Note: an ast-representation using the comma-operator
     * of the form: (a, b, c) = x should not assign to c...
     */
    if (lvalue) {
        compile_error(m_context, "not an l-value (code-block)");
        return false;
    }

    if (m_outr) {
        *out = m_outr;
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
    for (auto &it : m_locals) {
        if (!it->generateLocal(func->m_ir_func, false)) {
            if (OPTS_OPTION_BOOL(OPTION_DEBUG))
                compile_error(m_context, "failed to generate local `%s`", it->m_name);
            return false;
        }
    }

    for (auto &it : m_exprs) {
        if (func->m_curblock->m_final && !starts_a_label(it)) {
            if (compile_warning(it->m_context, WARN_UNREACHABLE_CODE, "unreachable statement"))
                return false;
            continue;
        }
        if (!it->codegen(func, false, out))
            return false;
    }

    m_outr = *out;

    return true;
}

bool ast_store::codegen(ast_function *func, bool lvalue, ir_value **out)
{
    ir_value *left  = nullptr;
    ir_value *right = nullptr;

    ast_value       *idx = 0;
    ast_array_index *ai = nullptr;

    if (lvalue && m_outl) {
        *out = m_outl;
        return true;
    }

    if (!lvalue && m_outr) {
        *out = m_outr;
        return true;
    }

    if (ast_istype(m_dest, ast_array_index))
    {

        ai = (ast_array_index*)m_dest;
        idx = (ast_value*)ai->m_index;

        if (ast_istype(ai->m_index, ast_value) && idx->m_hasvalue && idx->m_cvq == CV_CONST)
            ai = nullptr;
    }

    if (ai) {
        /* we need to call the setter */
        ir_value  *iridx, *funval;
        ir_instr  *call;

        if (lvalue) {
            compile_error(m_context, "array-subscript assignment cannot produce lvalues");
            return false;
        }

        auto arr = reinterpret_cast<ast_value*>(ai->m_array);
        if (!ast_istype(ai->m_array, ast_value) || !arr->m_setter) {
            compile_error(m_context, "value has no setter (%s)", arr->m_name);
            return false;
        }

        if (!idx->codegen(func, false, &iridx))
            return false;

        if (!arr->m_setter->codegen(func, true, &funval))
            return false;

        if (!m_source->codegen(func, false, &right))
            return false;

        call = ir_block_create_call(func->m_curblock, m_context, func->makeLabel("store"), funval, false);
        if (!call)
            return false;
        ir_call_param(call, iridx);
        ir_call_param(call, right);
        m_outr = right;
    }
    else
    {
        // regular code

        // lvalue!
        if (!m_dest->codegen(func, true, &left))
            return false;
        m_outl = left;

        /* rvalue! */
        if (!m_source->codegen(func, false, &right))
            return false;

        if (!ir_block_create_store_op(func->m_curblock, m_context, m_op, left, right))
            return false;
        m_outr = right;
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

bool ast_binary::codegen(ast_function *func, bool lvalue, ir_value **out)
{
    ir_value *left, *right;

    /* A binary operation cannot yield an l-value */
    if (lvalue) {
        compile_error(m_context, "not an l-value (binop)");
        return false;
    }

    if (m_outr) {
        *out = m_outr;
        return true;
    }

    if ((OPTS_FLAG(SHORT_LOGIC) || OPTS_FLAG(PERL_LOGIC)) &&
        (m_op == INSTR_AND || m_op == INSTR_OR))
    {
        /* NOTE: The short-logic path will ignore right_first */

        /* short circuit evaluation */
        ir_block *other, *merge;
        ir_block *from_left, *from_right;
        ir_instr *phi;
        size_t    merge_id;

        /* prepare end-block */
        merge_id = func->m_ir_func->m_blocks.size();
        merge    = ir_function_create_block(m_context, func->m_ir_func, func->makeLabel("sce_merge"));

        /* generate the left expression */
        if (!m_left->codegen(func, false, &left))
            return false;
        /* remember the block */
        from_left = func->m_curblock;

        /* create a new block for the right expression */
        other = ir_function_create_block(m_context, func->m_ir_func, func->makeLabel("sce_other"));
        if (m_op == INSTR_AND) {
            /* on AND: left==true -> other */
            if (!ir_block_create_if(func->m_curblock, m_context, left, other, merge))
                return false;
        } else {
            /* on OR: left==false -> other */
            if (!ir_block_create_if(func->m_curblock, m_context, left, merge, other))
                return false;
        }
        /* use the likely flag */
        func->m_curblock->m_instr.back()->m_likely = true;

        /* enter the right-expression's block */
        func->m_curblock = other;
        /* generate */
        if (!m_right->codegen(func, false, &right))
            return false;
        /* remember block */
        from_right = func->m_curblock;

        /* jump to the merge block */
        if (!ir_block_create_jump(func->m_curblock, m_context, merge))
            return false;

        algo::shiftback(func->m_ir_func->m_blocks.begin() + merge_id,
                        func->m_ir_func->m_blocks.end());
        // FIXME::DELME::
        //func->m_ir_func->m_blocks[merge_id].release();
        //func->m_ir_func->m_blocks.erase(func->m_ir_func->m_blocks.begin() + merge_id);
        //func->m_ir_func->m_blocks.emplace_back(merge);

        func->m_curblock = merge;
        phi = ir_block_create_phi(func->m_curblock, m_context,
                                  func->makeLabel("sce_value"),
                                  m_vtype);
        ir_phi_add(phi, from_left, left);
        ir_phi_add(phi, from_right, right);
        *out = ir_phi_value(phi);
        if (!*out)
            return false;

        if (!OPTS_FLAG(PERL_LOGIC)) {
            /* cast-to-bool */
            if (OPTS_FLAG(CORRECT_LOGIC) && (*out)->m_vtype == TYPE_VECTOR) {
                *out = ir_block_create_unary(func->m_curblock, m_context,
                                             func->makeLabel("sce_bool_v"),
                                             INSTR_NOT_V, *out);
                if (!*out)
                    return false;
                *out = ir_block_create_unary(func->m_curblock, m_context,
                                             func->makeLabel("sce_bool"),
                                             INSTR_NOT_F, *out);
                if (!*out)
                    return false;
            }
            else if (OPTS_FLAG(FALSE_EMPTY_STRINGS) && (*out)->m_vtype == TYPE_STRING) {
                *out = ir_block_create_unary(func->m_curblock, m_context,
                                             func->makeLabel("sce_bool_s"),
                                             INSTR_NOT_S, *out);
                if (!*out)
                    return false;
                *out = ir_block_create_unary(func->m_curblock, m_context,
                                             func->makeLabel("sce_bool"),
                                             INSTR_NOT_F, *out);
                if (!*out)
                    return false;
            }
            else {
                *out = ir_block_create_binop(func->m_curblock, m_context,
                                             func->makeLabel("sce_bool"),
                                             INSTR_AND, *out, *out);
                if (!*out)
                    return false;
            }
        }

        m_outr = *out;
        codegen_output_type(this, *out);
        return true;
    }

    if (m_right_first) {
        if (!m_right->codegen(func, false, &right))
            return false;
        if (!m_left->codegen(func, false, &left))
            return false;
    } else {
        if (!m_left->codegen(func, false, &left))
            return false;
        if (!m_right->codegen(func, false, &right))
            return false;
    }

    *out = ir_block_create_binop(func->m_curblock, m_context, func->makeLabel("bin"),
                                 m_op, left, right);
    if (!*out)
        return false;
    m_outr = *out;
    codegen_output_type(this, *out);

    return true;
}

bool ast_binstore::codegen(ast_function *func, bool lvalue, ir_value **out)
{
    ir_value *leftl = nullptr, *leftr, *right, *bin;

    ast_value       *arr;
    ast_value       *idx = 0;
    ast_array_index *ai = nullptr;
    ir_value        *iridx = nullptr;

    if (lvalue && m_outl) {
        *out = m_outl;
        return true;
    }

    if (!lvalue && m_outr) {
        *out = m_outr;
        return true;
    }

    if (ast_istype(m_dest, ast_array_index))
    {

        ai = (ast_array_index*)m_dest;
        idx = (ast_value*)ai->m_index;

        if (ast_istype(ai->m_index, ast_value) && idx->m_hasvalue && idx->m_cvq == CV_CONST)
            ai = nullptr;
    }

    /* for a binstore we need both an lvalue and an rvalue for the left side */
    /* rvalue of destination! */
    if (ai) {
        if (!idx->codegen(func, false, &iridx))
            return false;
    }
    if (!m_dest->codegen(func, false, &leftr))
        return false;

    /* source as rvalue only */
    if (!m_source->codegen(func, false, &right))
        return false;

    /* now the binary */
    bin = ir_block_create_binop(func->m_curblock, m_context, func->makeLabel("binst"),
                                m_opbin, leftr, right);
    m_outr = bin;

    if (ai) {
        /* we need to call the setter */
        ir_value  *funval;
        ir_instr  *call;

        if (lvalue) {
            compile_error(m_context, "array-subscript assignment cannot produce lvalues");
            return false;
        }

        arr = (ast_value*)ai->m_array;
        if (!ast_istype(ai->m_array, ast_value) || !arr->m_setter) {
            compile_error(m_context, "value has no setter (%s)", arr->m_name);
            return false;
        }

        if (!arr->m_setter->codegen(func, true, &funval))
            return false;

        call = ir_block_create_call(func->m_curblock, m_context, func->makeLabel("store"), funval, false);
        if (!call)
            return false;
        ir_call_param(call, iridx);
        ir_call_param(call, bin);
        m_outr = bin;
    } else {
        // now store them
        // lvalue of destination
        if (!m_dest->codegen(func, true, &leftl))
            return false;
        m_outl = leftl;

        if (!ir_block_create_store_op(func->m_curblock, m_context, m_opstore, leftl, bin))
            return false;
        m_outr = bin;
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

bool ast_unary::codegen(ast_function *func, bool lvalue, ir_value **out)
{
    ir_value *operand;

    /* An unary operation cannot yield an l-value */
    if (lvalue) {
        compile_error(m_context, "not an l-value (binop)");
        return false;
    }

    if (m_outr) {
        *out = m_outr;
        return true;
    }

    /* lvalue! */
    if (!m_operand->codegen(func, false, &operand))
        return false;

    *out = ir_block_create_unary(func->m_curblock, m_context, func->makeLabel("unary"),
                                 m_op, operand);
    if (!*out)
        return false;
    m_outr = *out;

    return true;
}

bool ast_return::codegen(ast_function *func, bool lvalue, ir_value **out)
{
    ir_value *operand;

    *out = nullptr;

    /* In the context of a return operation, we don't actually return
     * anything...
     */
    if (lvalue) {
        compile_error(m_context, "return-expression is not an l-value");
        return false;
    }

    if (m_outr) {
        compile_error(m_context, "internal error: ast_return cannot be reused, it bears no result!");
        return false;
    }
    m_outr = (ir_value*)1;

    if (m_operand) {
        /* lvalue! */
        if (!m_operand->codegen(func, false, &operand))
            return false;

        if (!ir_block_create_return(func->m_curblock, m_context, operand))
            return false;
    } else {
        if (!ir_block_create_return(func->m_curblock, m_context, nullptr))
            return false;
    }

    return true;
}

bool ast_entfield::codegen(ast_function *func, bool lvalue, ir_value **out)
{
    ir_value *ent, *field;

    // This function needs to take the 'lvalue' flag into account!
    // As lvalue we provide a field-pointer, as rvalue we provide the
    // value in a temp.

    if (lvalue && m_outl) {
        *out = m_outl;
        return true;
    }

    if (!lvalue && m_outr) {
        *out = m_outr;
        return true;
    }

    if (!m_entity->codegen(func, false, &ent))
        return false;

    if (!m_field->codegen(func, false, &field))
        return false;

    if (lvalue) {
        /* address! */
        *out = ir_block_create_fieldaddress(func->m_curblock, m_context, func->makeLabel("efa"),
                                            ent, field);
    } else {
        *out = ir_block_create_load_from_ent(func->m_curblock, m_context, func->makeLabel("efv"),
                                             ent, field, m_vtype);
        /* Done AFTER error checking:
        codegen_output_type(this, *out);
        */
    }
    if (!*out) {
        compile_error(m_context, "failed to create %s instruction (output type %s)",
                 (lvalue ? "ADDRESS" : "FIELD"),
                 type_name[m_vtype]);
        return false;
    }
    if (!lvalue)
        codegen_output_type(this, *out);

    if (lvalue)
        m_outl = *out;
    else
        m_outr = *out;

    // Hm that should be it...
    return true;
}

bool ast_member::codegen(ast_function *func, bool lvalue, ir_value **out)
{
    ir_value *vec;

    /* in QC this is always an lvalue */
    if (lvalue && m_rvalue) {
        compile_error(m_context, "not an l-value (member access)");
        return false;
    }
    if (lvalue && m_outl) {
        *out = m_outl;
        return true;
    }
    if (!lvalue && m_outr) {
        *out = m_outr;
        return true;
    }

    if (ast_istype(m_owner, ast_entfield)) {
        ir_value *ent, *field;
        auto entfield = reinterpret_cast<ast_entfield*>(m_owner);
        if (!entfield->m_entity->codegen(func, false, &ent))
            return false;
        if (!entfield->m_field->codegen(func, false, &vec))
            return false;
        field = vec->vectorMember(m_field);
        if (lvalue) {
            *out = ir_block_create_fieldaddress(func->m_curblock, m_context, func->makeLabel("mefa"),
                                                ent, field);
        } else {
            *out = ir_block_create_load_from_ent(func->m_curblock, m_context, func->makeLabel("mefv"),
                                                 ent, field, m_vtype);
        }
        if (!*out) {
            compile_error(m_context, "failed to create %s instruction (output type %s)",
                     (lvalue ? "ADDRESS" : "FIELD"),
                     type_name[m_vtype]);
            return false;
        }
        if (lvalue)
            m_outl = *out;
        else
            m_outr = *out;
        return (*out != nullptr);
    } else {
        if (!m_owner->codegen(func, false, &vec))
            return false;
    }

    if (vec->m_vtype != TYPE_VECTOR &&
        !(vec->m_vtype == TYPE_FIELD && m_owner->m_next->m_vtype == TYPE_VECTOR))
    {
        return false;
    }

    *out = vec->vectorMember(m_field);
    m_outl = *out;

    return (*out != nullptr);
}

bool ast_array_index::codegen(ast_function *func, bool lvalue, ir_value **out)
{
    ast_value *arr;
    ast_value *idx;

    if (!lvalue && m_outr) {
        *out = m_outr;
        return true;
    }
    if (lvalue && m_outl) {
        *out = m_outl;
        return true;
    }

    if (!ast_istype(m_array, ast_value)) {
        compile_error(m_context, "array indexing this way is not supported");
        /* note this would actually be pointer indexing because the left side is
         * not an actual array but (hopefully) an indexable expression.
         * Once we get integer arithmetic, and GADDRESS/GSTORE/GLOAD instruction
         * support this path will be filled.
         */
        return false;
    }

    arr = reinterpret_cast<ast_value*>(m_array);
    idx = reinterpret_cast<ast_value*>(m_index);

    if (!ast_istype(m_index, ast_value) || !idx->m_hasvalue || idx->m_cvq != CV_CONST) {
        /* Time to use accessor functions */
        ir_value               *iridx, *funval;
        ir_instr               *call;

        if (lvalue) {
            compile_error(m_context, "(.2) array indexing here needs a compile-time constant");
            return false;
        }

        if (!arr->m_getter) {
            compile_error(m_context, "value has no getter, don't know how to index it");
            return false;
        }

        if (!m_index->codegen(func, false, &iridx))
            return false;

        if (!arr->m_getter->codegen(func, true, &funval))
            return false;

        call = ir_block_create_call(func->m_curblock, m_context, func->makeLabel("fetch"), funval, false);
        if (!call)
            return false;
        ir_call_param(call, iridx);

        *out = ir_call_value(call);
        m_outr = *out;
        (*out)->m_vtype = m_vtype;
        codegen_output_type(this, *out);
        return true;
    }

    if (idx->m_vtype == TYPE_FLOAT) {
        unsigned int arridx = idx->m_constval.vfloat;
        if (arridx >= m_array->m_count)
        {
            compile_error(m_context, "array index out of bounds: %i", arridx);
            return false;
        }
        *out = arr->m_ir_values[arridx];
    }
    else if (idx->m_vtype == TYPE_INTEGER) {
        unsigned int arridx = idx->m_constval.vint;
        if (arridx >= m_array->m_count)
        {
            compile_error(m_context, "array index out of bounds: %i", arridx);
            return false;
        }
        *out = arr->m_ir_values[arridx];
    }
    else {
        compile_error(m_context, "array indexing here needs an integer constant");
        return false;
    }
    (*out)->m_vtype = m_vtype;
    codegen_output_type(this, *out);
    return true;
}

bool ast_argpipe::codegen(ast_function *func, bool lvalue, ir_value **out)
{
    *out = nullptr;
    if (lvalue) {
        compile_error(m_context, "argpipe node: not an lvalue");
        return false;
    }
    (void)func;
    (void)out;
    compile_error(m_context, "TODO: argpipe codegen not implemented");
    return false;
}

bool ast_ifthen::codegen(ast_function *func, bool lvalue, ir_value **out)
{
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

    if (m_outr) {
        compile_error(m_context, "internal error: ast_ifthen cannot be reused, it bears no result!");
        return false;
    }
    m_outr = (ir_value*)1;

    /* generate the condition */
    if (!m_cond->codegen(func, false, &condval))
        return false;
    /* update the block which will get the jump - because short-logic or ternaries may have changed this */
    cond = func->m_curblock;

    /* try constant folding away the condition */
    if ((folded = fold::cond_ifthen(condval, func, this)) != -1)
        return folded;

    if (m_on_true) {
        /* create on-true block */
        ontrue = ir_function_create_block(m_context, func->m_ir_func, func->makeLabel("ontrue"));
        if (!ontrue)
            return false;

        /* enter the block */
        func->m_curblock = ontrue;

        /* generate */
        if (!m_on_true->codegen(func, false, &dummy))
            return false;

        /* we now need to work from the current endpoint */
        ontrue_endblock = func->m_curblock;
    } else
        ontrue = nullptr;

    /* on-false path */
    if (m_on_false) {
        /* create on-false block */
        onfalse = ir_function_create_block(m_context, func->m_ir_func, func->makeLabel("onfalse"));
        if (!onfalse)
            return false;

        /* enter the block */
        func->m_curblock = onfalse;

        /* generate */
        if (!m_on_false->codegen(func, false, &dummy))
            return false;

        /* we now need to work from the current endpoint */
        onfalse_endblock = func->m_curblock;
    } else
        onfalse = nullptr;

    /* Merge block were they all merge in to */
    if (!ontrue || !onfalse || !ontrue_endblock->m_final || !onfalse_endblock->m_final)
    {
        merge = ir_function_create_block(m_context, func->m_ir_func, func->makeLabel("endif"));
        if (!merge)
            return false;
        /* add jumps ot the merge block */
        if (ontrue && !ontrue_endblock->m_final && !ir_block_create_jump(ontrue_endblock, m_context, merge))
            return false;
        if (onfalse && !onfalse_endblock->m_final && !ir_block_create_jump(onfalse_endblock, m_context, merge))
            return false;

        /* Now enter the merge block */
        func->m_curblock = merge;
    }

    /* we create the if here, that way all blocks are ordered :)
     */
    if (!ir_block_create_if(cond, m_context, condval,
                            (ontrue  ? ontrue  : merge),
                            (onfalse ? onfalse : merge)))
    {
        return false;
    }

    return true;
}

bool ast_ternary::codegen(ast_function *func, bool lvalue, ir_value **out)
{
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
    if (m_outr) {
        *out = m_outr;
        return true;
    }

    /* In the following, contraty to ast_ifthen, we assume both paths exist. */

    /* generate the condition */
    func->m_curblock = cond;
    if (!m_cond->codegen(func, false, &condval))
        return false;
    cond_out = func->m_curblock;

    /* try constant folding away the condition */
    if ((folded = fold::cond_ternary(condval, func, this)) != -1)
        return folded;

    /* create on-true block */
    ontrue = ir_function_create_block(m_context, func->m_ir_func, func->makeLabel("tern_T"));
    if (!ontrue)
        return false;
    else
    {
        /* enter the block */
        func->m_curblock = ontrue;

        /* generate */
        if (!m_on_true->codegen(func, false, &trueval))
            return false;

        ontrue_out = func->m_curblock;
    }

    /* create on-false block */
    onfalse = ir_function_create_block(m_context, func->m_ir_func, func->makeLabel("tern_F"));
    if (!onfalse)
        return false;
    else
    {
        /* enter the block */
        func->m_curblock = onfalse;

        /* generate */
        if (!m_on_false->codegen(func, false, &falseval))
            return false;

        onfalse_out = func->m_curblock;
    }

    /* create merge block */
    merge = ir_function_create_block(m_context, func->m_ir_func, func->makeLabel("tern_out"));
    if (!merge)
        return false;
    /* jump to merge block */
    if (!ir_block_create_jump(ontrue_out, m_context, merge))
        return false;
    if (!ir_block_create_jump(onfalse_out, m_context, merge))
        return false;

    /* create if instruction */
    if (!ir_block_create_if(cond_out, m_context, condval, ontrue, onfalse))
        return false;

    /* Now enter the merge block */
    func->m_curblock = merge;

    /* Here, now, we need a PHI node
     * but first some sanity checking...
     */
    if (trueval->m_vtype != falseval->m_vtype && trueval->m_vtype != TYPE_NIL && falseval->m_vtype != TYPE_NIL) {
        /* error("ternary with different types on the two sides"); */
        compile_error(m_context, "internal error: ternary operand types invalid");
        return false;
    }

    /* create PHI */
    phi = ir_block_create_phi(merge, m_context, func->makeLabel("phi"), m_vtype);
    if (!phi) {
        compile_error(m_context, "internal error: failed to generate phi node");
        return false;
    }
    ir_phi_add(phi, ontrue_out,  trueval);
    ir_phi_add(phi, onfalse_out, falseval);

    m_outr = ir_phi_value(phi);
    *out = m_outr;

    codegen_output_type(this, *out);

    return true;
}

bool ast_loop::codegen(ast_function *func, bool lvalue, ir_value **out)
{
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

    if (m_outr) {
        compile_error(m_context, "internal error: ast_loop cannot be reused, it bears no result!");
        return false;
    }
    m_outr = (ir_value*)1;

    /* NOTE:
     * Should we ever need some kind of block ordering, better make this function
     * move blocks around than write a block ordering algorithm later... after all
     * the ast and ir should work together, not against each other.
     */

    /* initexpr doesn't get its own block, it's pointless, it could create more blocks
     * anyway if for example it contains a ternary.
     */
    if (m_initexpr)
    {
        if (!m_initexpr->codegen(func, false, &dummy))
            return false;
    }

    /* Store the block from which we enter this chaos */
    bin = func->m_curblock;

    /* The pre-loop condition needs its own block since we
     * need to be able to jump to the start of that expression.
     */
    if (m_precond)
    {
        bprecond = ir_function_create_block(m_context, func->m_ir_func, func->makeLabel("pre_loop_cond"));
        if (!bprecond)
            return false;

        /* the pre-loop-condition the least important place to 'continue' at */
        bcontinue = bprecond;

        /* enter */
        func->m_curblock = bprecond;

        /* generate */
        if (!m_precond->codegen(func, false, &precond))
            return false;

        end_bprecond = func->m_curblock;
    } else {
        bprecond = end_bprecond = nullptr;
    }

    /* Now the next blocks won't be ordered nicely, but we need to
     * generate them this early for 'break' and 'continue'.
     */
    if (m_increment) {
        bincrement = ir_function_create_block(m_context, func->m_ir_func, func->makeLabel("loop_increment"));
        if (!bincrement)
            return false;
        bcontinue = bincrement; /* increment comes before the pre-loop-condition */
    } else {
        bincrement = end_bincrement = nullptr;
    }

    if (m_postcond) {
        bpostcond = ir_function_create_block(m_context, func->m_ir_func, func->makeLabel("post_loop_cond"));
        if (!bpostcond)
            return false;
        bcontinue = bpostcond; /* postcond comes before the increment */
    } else {
        bpostcond = end_bpostcond = nullptr;
    }

    bout_id = func->m_ir_func->m_blocks.size();
    bout = ir_function_create_block(m_context, func->m_ir_func, func->makeLabel("after_loop"));
    if (!bout)
        return false;
    bbreak = bout;

    /* The loop body... */
    /* if (m_body) */
    {
        bbody = ir_function_create_block(m_context, func->m_ir_func, func->makeLabel("loop_body"));
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
        if (m_body) {
            if (!m_body->codegen(func, false, &dummy))
                return false;
        }

        end_bbody = func->m_curblock;
        func->m_breakblocks.pop_back();
        func->m_continueblocks.pop_back();
    }

    /* post-loop-condition */
    if (m_postcond)
    {
        /* enter */
        func->m_curblock = bpostcond;

        /* generate */
        if (!m_postcond->codegen(func, false, &postcond))
            return false;

        end_bpostcond = func->m_curblock;
    }

    /* The incrementor */
    if (m_increment)
    {
        /* enter */
        func->m_curblock = bincrement;

        /* generate */
        if (!m_increment->codegen(func, false, &dummy))
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

    if (!ir_block_create_jump(bin, m_context, tmpblock))
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
        if (m_pre_not) {
            tmpblock = ontrue;
            ontrue   = onfalse;
            onfalse  = tmpblock;
        }
        if (!ir_block_create_if(end_bprecond, m_context, precond, ontrue, onfalse))
            return false;
    }

    /* from body */
    if (bbody)
    {
        if      (bincrement) tmpblock = bincrement;
        else if (bpostcond)  tmpblock = bpostcond;
        else if (bprecond)   tmpblock = bprecond;
        else                 tmpblock = bbody;
        if (!end_bbody->m_final && !ir_block_create_jump(end_bbody, m_context, tmpblock))
            return false;
    }

    /* from increment */
    if (bincrement)
    {
        if      (bpostcond)  tmpblock = bpostcond;
        else if (bprecond)   tmpblock = bprecond;
        else if (bbody)      tmpblock = bbody;
        else                 tmpblock = bout;
        if (!ir_block_create_jump(end_bincrement, m_context, tmpblock))
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
        if (m_post_not) {
            tmpblock = ontrue;
            ontrue   = onfalse;
            onfalse  = tmpblock;
        }
        if (!ir_block_create_if(end_bpostcond, m_context, postcond, ontrue, onfalse))
            return false;
    }

    /* Move 'bout' to the end */
    algo::shiftback(func->m_ir_func->m_blocks.begin() + bout_id,
                    func->m_ir_func->m_blocks.end());
    // FIXME::DELME::
    //func->m_ir_func->m_blocks[bout_id].release(); // it's a vector<std::unique_ptr<>>
    //func->m_ir_func->m_blocks.erase(func->m_ir_func->m_blocks.begin() + bout_id);
    //func->m_ir_func->m_blocks.emplace_back(bout);

    return true;
}

bool ast_breakcont::codegen(ast_function *func, bool lvalue, ir_value **out)
{
    ir_block *target;

    *out = nullptr;

    if (lvalue) {
        compile_error(m_context, "break/continue expression is not an l-value");
        return false;
    }

    if (m_outr) {
        compile_error(m_context, "internal error: ast_breakcont cannot be reused!");
        return false;
    }
    m_outr = (ir_value*)1;

    if (m_is_continue)
        target = func->m_continueblocks[func->m_continueblocks.size()-1-m_levels];
    else
        target = func->m_breakblocks[func->m_breakblocks.size()-1-m_levels];

    if (!target) {
        compile_error(m_context, "%s is lacking a target block", (m_is_continue ? "continue" : "break"));
        return false;
    }

    if (!ir_block_create_jump(func->m_curblock, m_context, target))
        return false;
    return true;
}

bool ast_switch::codegen(ast_function *func, bool lvalue, ir_value **out)
{
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
        compile_error(m_context, "switch expression is not an l-value");
        return false;
    }

    if (m_outr) {
        compile_error(m_context, "internal error: ast_switch cannot be reused!");
        return false;
    }
    m_outr = (ir_value*)1;

    (void)lvalue;
    (void)out;

    if (!m_operand->codegen(func, false, &irop))
        return false;

    if (m_cases.empty())
        return true;

    cmpinstr = type_eq_instr[irop->m_vtype];
    if (cmpinstr >= VINSTR_END) {
        ast_type_to_string(m_operand, typestr, sizeof(typestr));
        compile_error(m_context, "invalid type to perform a switch on: %s", typestr);
        return false;
    }

    bout_id = func->m_ir_func->m_blocks.size();
    bout = ir_function_create_block(m_context, func->m_ir_func, func->makeLabel("after_switch"));
    if (!bout)
        return false;

    /* setup the break block */
    func->m_breakblocks.push_back(bout);

    /* Now create all cases */
    for (auto &it : m_cases) {
        ir_value *cond, *val;
        ir_block *bcase, *bnot;
        size_t bnot_id;

        ast_switch_case *swcase = &it;

        if (swcase->m_value) {
            /* A regular case */
            /* generate the condition operand */
            if (!swcase->m_value->codegen(func, false, &val))
                return false;
            /* generate the condition */
            cond = ir_block_create_binop(func->m_curblock, m_context, func->makeLabel("switch_eq"), cmpinstr, irop, val);
            if (!cond)
                return false;

            bcase = ir_function_create_block(m_context, func->m_ir_func, func->makeLabel("case"));
            bnot_id = func->m_ir_func->m_blocks.size();
            bnot = ir_function_create_block(m_context, func->m_ir_func, func->makeLabel("not_case"));
            if (!bcase || !bnot)
                return false;
            if (set_def_bfall_to) {
                set_def_bfall_to = false;
                def_bfall_to = bcase;
            }
            if (!ir_block_create_if(func->m_curblock, m_context, cond, bcase, bnot))
                return false;

            /* Make the previous case-end fall through */
            if (bfall && !bfall->m_final) {
                if (!ir_block_create_jump(bfall, m_context, bcase))
                    return false;
            }

            /* enter the case */
            func->m_curblock = bcase;
            if (!swcase->m_code->codegen(func, false, &dummy))
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
    if (bfall && !bfall->m_final && !ir_block_create_jump(bfall, m_context, bout)) {
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
            if (!ir_block_create_jump(def_bfall, m_context, bcase))
                return false;
        }

        /* Now generate the default code */
        if (!def_case->m_code->codegen(func, false, &dummy))
            return false;

        /* see if we need to fall through */
        if (def_bfall_to && !func->m_curblock->m_final)
        {
            if (!ir_block_create_jump(func->m_curblock, m_context, def_bfall_to))
                return false;
        }
    }

    /* Jump from the last bnot to bout */
    if (!func->m_curblock->m_final && !ir_block_create_jump(func->m_curblock, m_context, bout))
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

bool ast_label::codegen(ast_function *func, bool lvalue, ir_value **out)
{
    ir_value *dummy;

    if (m_undefined) {
        compile_error(m_context, "internal error: ast_label never defined");
        return false;
    }

    *out = nullptr;
    if (lvalue) {
        compile_error(m_context, "internal error: ast_label cannot be an lvalue");
        return false;
    }

    /* simply create a new block and jump to it */
    m_irblock = ir_function_create_block(m_context, func->m_ir_func, m_name.c_str());
    if (!m_irblock) {
        compile_error(m_context, "failed to allocate label block `%s`", m_name);
        return false;
    }
    if (!func->m_curblock->m_final) {
        if (!ir_block_create_jump(func->m_curblock, m_context, m_irblock))
            return false;
    }

    /* enter the new block */
    func->m_curblock = m_irblock;

    /* Generate all the leftover gotos */
    for (auto &it : m_gotos) {
        if (!it->codegen(func, false, &dummy))
            return false;
    }

    return true;
}

bool ast_goto::codegen(ast_function *func, bool lvalue, ir_value **out)
{
    *out = nullptr;
    if (lvalue) {
        compile_error(m_context, "internal error: ast_goto cannot be an lvalue");
        return false;
    }

    if (m_target->m_irblock) {
        if (m_irblock_from) {
            /* we already tried once, this is the callback */
            m_irblock_from->m_final = false;
            if (!ir_block_create_goto(m_irblock_from, m_context, m_target->m_irblock)) {
                compile_error(m_context, "failed to generate goto to `%s`", m_name);
                return false;
            }
        }
        else
        {
            if (!ir_block_create_goto(func->m_curblock, m_context, m_target->m_irblock)) {
                compile_error(m_context, "failed to generate goto to `%s`", m_name);
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
        m_irblock_from = func->m_curblock;
        m_target->registerGoto(this);
    }

    return true;
}

bool ast_state::codegen(ast_function *func, bool lvalue, ir_value **out)
{
    ir_value *frameval, *thinkval;

    if (lvalue) {
        compile_error(m_context, "not an l-value (state operation)");
        return false;
    }
    if (m_outr) {
        compile_error(m_context, "internal error: ast_state cannot be reused!");
        return false;
    }
    *out = nullptr;

    if (!m_framenum->codegen(func, false, &frameval))
        return false;
    if (!frameval)
        return false;

    if (!m_nextthink->codegen(func, false, &thinkval))
        return false;
    if (!frameval)
        return false;

    if (!ir_block_create_state_op(func->m_curblock, m_context, frameval, thinkval)) {
        compile_error(m_context, "failed to create STATE instruction");
        return false;
    }

    m_outr = (ir_value*)1;
    return true;
}

bool ast_call::codegen(ast_function *func, bool lvalue, ir_value **out)
{
    std::vector<ir_value*> params;
    ir_instr *callinstr;

    ir_value *funval = nullptr;

    /* return values are never lvalues */
    if (lvalue) {
        compile_error(m_context, "not an l-value (function call)");
        return false;
    }

    if (m_outr) {
        *out = m_outr;
        return true;
    }

    if (!m_func->codegen(func, false, &funval))
        return false;
    if (!funval)
        return false;

    /* parameters */
    for (auto &it : m_params) {
        ir_value *param;
        if (!it->codegen(func, false, &param))
            return false;
        if (!param)
            return false;
        params.push_back(param);
    }

    /* varargs counter */
    if (m_va_count) {
        ir_value   *va_count;
        ir_builder *builder = func->m_curblock->m_owner->m_owner;
        if (!m_va_count->codegen(func, false, &va_count))
            return false;
        if (!ir_block_create_store_op(func->m_curblock, m_context, INSTR_STORE_F,
                                      builder->get_va_count(), va_count))
        {
            return false;
        }
    }

    callinstr = ir_block_create_call(func->m_curblock, m_context,
                                     func->makeLabel("call"),
                                     funval, !!(m_func->m_flags & AST_FLAG_NORETURN));
    if (!callinstr)
        return false;

    for (auto &it : params)
        ir_call_param(callinstr, it);

    *out = ir_call_value(callinstr);
    m_outr = *out;

    codegen_output_type(this, *out);

    return true;
}
