#include <string.h>
#include <math.h>

#include "intrin.h"
#include "fold.h"
#include "ast.h"
#include "parser.h"

#define PARSER_HT_LOCALS  2
#define PARSER_HT_SIZE    512
#define TYPEDEF_HT_SIZE   512

static void parser_enterblock(parser_t *parser);
static bool parser_leaveblock(parser_t *parser);
static void parser_addlocal(parser_t *parser, const char *name, ast_expression *e);
static void parser_addlocal(parser_t *parser, const std::string &name, ast_expression *e);
static void parser_addglobal(parser_t *parser, const char *name, ast_expression *e);
static void parser_addglobal(parser_t *parser, const std::string &name, ast_expression *e);
static bool parse_typedef(parser_t *parser);
static bool parse_variable(parser_t *parser, ast_block *localblock, bool nofields, int qualifier, ast_value *cached_typedef, bool noref, bool is_static, uint32_t qflags, char *vstring);
static ast_block* parse_block(parser_t *parser);
static bool parse_block_into(parser_t *parser, ast_block *block);
static bool parse_statement_or_block(parser_t *parser, ast_block* parent_block, ast_expression **out);
static bool parse_statement(parser_t *parser, ast_block *block, ast_expression **out, bool allow_cases);
static ast_expression* parse_expression_leave(parser_t *parser, bool stopatcomma, bool truthvalue, bool with_labels);
static ast_expression* parse_expression(parser_t *parser, bool stopatcomma, bool with_labels);
static ast_value* parser_create_array_setter_proto(parser_t *parser, ast_value *array, const char *funcname);
static ast_value* parser_create_array_getter_proto(parser_t *parser, ast_value *array, const ast_expression *elemtype, const char *funcname);
static ast_value *parse_typename(parser_t *parser, ast_value **storebase, ast_value *cached_typedef, bool *is_vararg);

static void parseerror_(parser_t *parser, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vcompile_error(parser->lex->tok.ctx, fmt, ap);
    va_end(ap);
}

template<typename... Ts>
static inline void parseerror(parser_t *parser, const char *fmt, const Ts&... ts) {
    return parseerror_(parser, fmt, formatNormalize(ts)...);
}

// returns true if it counts as an error
static bool GMQCC_WARN parsewarning_(parser_t *parser, int warntype, const char *fmt, ...)
{
    bool    r;
    va_list ap;
    va_start(ap, fmt);
    r = vcompile_warning(parser->lex->tok.ctx, warntype, fmt, ap);
    va_end(ap);
    return r;
}

template<typename... Ts>
static inline bool GMQCC_WARN parsewarning(parser_t *parser, int warntype, const char *fmt, const Ts&... ts) {
    return parsewarning_(parser, warntype, fmt, formatNormalize(ts)...);
}

/**********************************************************************
 * parsing
 */

static bool parser_next(parser_t *parser)
{
    /* lex_do kills the previous token */
    parser->tok = lex_do(parser->lex);
    if (parser->tok == TOKEN_EOF)
        return true;
    if (parser->tok >= TOKEN_ERROR) {
        parseerror(parser, "lex error");
        return false;
    }
    return true;
}

#define parser_tokval(p) ((p)->lex->tok.value)
#define parser_token(p)  (&((p)->lex->tok))

char *parser_strdup(const char *str)
{
    if (str && !*str) {
        /* actually dup empty strings */
        char *out = (char*)mem_a(1);
        *out = 0;
        return out;
    }
    return util_strdup(str);
}

static ast_expression* parser_find_field(parser_t *parser, const char *name) {
    return (ast_expression*)util_htget(parser->htfields, name);
}
static ast_expression* parser_find_field(parser_t *parser, const std::string &name) {
    return parser_find_field(parser, name.c_str());
}

static ast_expression* parser_find_label(parser_t *parser, const char *name)
{
    for (auto &it : parser->labels)
        if (it->m_name == name)
            return it;
    return nullptr;
}
static inline ast_expression* parser_find_label(parser_t *parser, const std::string &name) {
    return parser_find_label(parser, name.c_str());
}

ast_expression* parser_find_global(parser_t *parser, const char *name)
{
    ast_expression *var = (ast_expression*)util_htget(parser->aliases, parser_tokval(parser));
    if (var)
        return var;
    return (ast_expression*)util_htget(parser->htglobals, name);
}

ast_expression* parser_find_global(parser_t *parser, const std::string &name) {
    return parser_find_global(parser, name.c_str());
}

static ast_expression* parser_find_param(parser_t *parser, const char *name)
{
    ast_value *fun;
    if (!parser->function)
        return nullptr;
    fun = parser->function->m_function_type;
    for (auto &it : fun->m_type_params) {
        if (it->m_name == name)
            return it.get();
    }
    return nullptr;
}

static ast_expression* parser_find_local(parser_t *parser, const char *name, size_t upto, bool *isparam)
{
    size_t          i, hash;
    ast_expression *e;

    hash = util_hthash(parser->htglobals, name);

    *isparam = false;
    for (i = parser->variables.size(); i > upto;) {
        --i;
        if ( (e = (ast_expression*)util_htgeth(parser->variables[i], name, hash)) )
            return e;
    }
    *isparam = true;
    return parser_find_param(parser, name);
}

static ast_expression* parser_find_local(parser_t *parser, const std::string &name, size_t upto, bool *isparam) {
    return parser_find_local(parser, name.c_str(), upto, isparam);
}

static ast_expression* parser_find_var(parser_t *parser, const char *name)
{
    bool dummy;
    ast_expression *v;
    v         = parser_find_local(parser, name, PARSER_HT_LOCALS, &dummy);
    if (!v) v = parser_find_global(parser, name);
    return v;
}

static inline ast_expression* parser_find_var(parser_t *parser, const std::string &name) {
    return parser_find_var(parser, name.c_str());
}

static ast_value* parser_find_typedef(parser_t *parser, const char *name, size_t upto)
{
    size_t     i, hash;
    ast_value *e;
    hash = util_hthash(parser->typedefs[0], name);

    for (i = parser->typedefs.size(); i > upto;) {
        --i;
        if ( (e = (ast_value*)util_htgeth(parser->typedefs[i], name, hash)) )
            return e;
    }
    return nullptr;
}

static ast_value* parser_find_typedef(parser_t *parser, const std::string &name, size_t upto) {
    return parser_find_typedef(parser, name.c_str(), upto);
}

struct sy_elem {
    size_t etype; /* 0 = expression, others are operators */
    bool isparen;
    size_t off;
    ast_expression *out;
    ast_block *block; /* for commas and function calls */
    lex_ctx_t ctx;
};

enum {
    PAREN_EXPR,
    PAREN_FUNC,
    PAREN_INDEX,
    PAREN_TERNARY1,
    PAREN_TERNARY2
};

struct shunt {
    std::vector<sy_elem> out;
    std::vector<sy_elem> ops;
    std::vector<size_t> argc;
    std::vector<unsigned int> paren;
};

static sy_elem syexp(lex_ctx_t ctx, ast_expression *v) {
    sy_elem e;
    e.etype = 0;
    e.off   = 0;
    e.out   = v;
    e.block = nullptr;
    e.ctx   = ctx;
    e.isparen = false;
    return e;
}

static sy_elem syblock(lex_ctx_t ctx, ast_block *v) {
    sy_elem e;
    e.etype = 0;
    e.off   = 0;
    e.out   = v;
    e.block = v;
    e.ctx   = ctx;
    e.isparen = false;
    return e;
}

static sy_elem syop(lex_ctx_t ctx, const oper_info *op) {
    sy_elem e;
    e.etype = 1 + (op - operators);
    e.off   = 0;
    e.out   = nullptr;
    e.block = nullptr;
    e.ctx   = ctx;
    e.isparen = false;
    return e;
}

static sy_elem syparen(lex_ctx_t ctx, size_t off) {
    sy_elem e;
    e.etype = 0;
    e.off   = off;
    e.out   = nullptr;
    e.block = nullptr;
    e.ctx   = ctx;
    e.isparen = true;
    return e;
}

/* With regular precedence rules, ent.foo[n] is the same as (ent.foo)[n],
 * so we need to rotate it to become ent.(foo[n]).
 */
static bool rotate_entfield_array_index_nodes(ast_expression **out)
{
    ast_array_index *index, *oldindex;
    ast_entfield    *entfield;

    ast_value       *field;
    ast_expression  *sub;
    ast_expression  *entity;

    lex_ctx_t ctx = (*out)->m_context;

    if (!ast_istype(*out, ast_array_index))
        return false;
    index = (ast_array_index*)*out;

    if (!ast_istype(index->m_array, ast_entfield))
        return false;
    entfield = (ast_entfield*)index->m_array;

    if (!ast_istype(entfield->m_field, ast_value))
        return false;
    field = (ast_value*)entfield->m_field;

    sub    = index->m_index;
    entity = entfield->m_entity;

    oldindex = index;

    index = ast_array_index::make(ctx, field, sub);
    entfield = new ast_entfield(ctx, entity, index);
    *out = entfield;

    oldindex->m_array = nullptr;
    oldindex->m_index = nullptr;
    delete oldindex;

    return true;
}

static int store_op_for(ast_expression* expr)
{
    if (OPTS_FLAG(ADJUST_VECTOR_FIELDS) && expr->m_vtype == TYPE_FIELD && expr->m_next->m_vtype == TYPE_VECTOR) {
        if (ast_istype(expr, ast_entfield)) {
            return type_storep_instr[TYPE_VECTOR];
        } else {
            return type_store_instr[TYPE_VECTOR];
        }
    }

    if (ast_istype(expr, ast_member) && ast_istype(((ast_member*)expr)->m_owner, ast_entfield)) {
        return type_storep_instr[expr->m_vtype];
    }

    if (ast_istype(expr, ast_entfield)) {
        return type_storep_instr[expr->m_vtype];
    }

    return type_store_instr[expr->m_vtype];
}

static bool check_write_to(lex_ctx_t ctx, ast_expression *expr)
{
    if (ast_istype(expr, ast_value)) {
        ast_value *val = (ast_value*)expr;
        if (val->m_cvq == CV_CONST) {
            if (val->m_name[0] == '#') {
                compile_error(ctx, "invalid assignment to a literal constant");
                return false;
            }
            /*
             * To work around quakeworld we must elide the error and make it
             * a warning instead.
             */
            if (OPTS_OPTION_U32(OPTION_STANDARD) != COMPILER_QCC)
                compile_error(ctx, "assignment to constant `%s`", val->m_name);
            else
                (void)!compile_warning(ctx, WARN_CONST_OVERWRITE, "assignment to constant `%s`", val->m_name);
            return false;
        }
    }
    return true;
}

static bool parser_sy_apply_operator(parser_t *parser, shunt *sy)
{
    const oper_info *op;
    lex_ctx_t ctx;
    ast_expression *out = nullptr;
    ast_expression *exprs[3] = { 0, 0, 0 };
    ast_block      *blocks[3];
    ast_binstore   *asbinstore;
    size_t i, assignop, addop, subop;
    qcint_t  generated_op = 0;

    char ty1[1024];
    char ty2[1024];

    if (sy->ops.empty()) {
        parseerror(parser, "internal error: missing operator");
        return false;
    }

    if (sy->ops.back().isparen) {
        parseerror(parser, "unmatched parenthesis");
        return false;
    }

    op = &operators[sy->ops.back().etype - 1];
    ctx = sy->ops.back().ctx;

    if (sy->out.size() < op->operands) {
        if (op->flags & OP_PREFIX)
            compile_error(ctx, "expected expression after unary operator `%s`", op->op, (int)op->id);
        else /* this should have errored previously already */
            compile_error(ctx, "expected expression after operator `%s`", op->op, (int)op->id);
        return false;
    }

    sy->ops.pop_back();

    /* op(:?) has no input and no output */
    if (!op->operands)
        return true;

    sy->out.erase(sy->out.end() - op->operands, sy->out.end());
    for (i = 0; i < op->operands; ++i) {
        exprs[i]  = sy->out[sy->out.size()+i].out;
        blocks[i] = sy->out[sy->out.size()+i].block;

        if (exprs[i]->m_vtype == TYPE_NOEXPR &&
            !(i != 0 && op->id == opid2('?',':')) &&
            !(i == 1 && op->id == opid1('.')))
        {
            if (ast_istype(exprs[i], ast_label))
                compile_error(exprs[i]->m_context, "expected expression, got an unknown identifier");
            else
                compile_error(exprs[i]->m_context, "not an expression");
            (void)!compile_warning(exprs[i]->m_context, WARN_DEBUG, "expression %u\n", (unsigned int)i);
        }
    }

    if (blocks[0] && blocks[0]->m_exprs.empty() && op->id != opid1(',')) {
        compile_error(ctx, "internal error: operator cannot be applied on empty blocks");
        return false;
    }

#define NotSameType(T) \
             (exprs[0]->m_vtype != exprs[1]->m_vtype || \
              exprs[0]->m_vtype != T)

    switch (op->id)
    {
        default:
            compile_error(ctx, "internal error: unhandled operator: %s (%i)", op->op, (int)op->id);
            return false;

        case opid1('.'):
            if (exprs[0]->m_vtype == TYPE_VECTOR &&
                exprs[1]->m_vtype == TYPE_NOEXPR)
            {
                if      (exprs[1] == parser->const_vec[0])
                    out = ast_member::make(ctx, exprs[0], 0, "");
                else if (exprs[1] == parser->const_vec[1])
                    out = ast_member::make(ctx, exprs[0], 1, "");
                else if (exprs[1] == parser->const_vec[2])
                    out = ast_member::make(ctx, exprs[0], 2, "");
                else {
                    compile_error(ctx, "access to invalid vector component");
                    return false;
                }
            }
            else if (exprs[0]->m_vtype == TYPE_ENTITY) {
                if (exprs[1]->m_vtype != TYPE_FIELD) {
                    compile_error(exprs[1]->m_context, "type error: right hand of member-operand should be an entity-field");
                    return false;
                }
                out = new ast_entfield(ctx, exprs[0], exprs[1]);
            }
            else if (exprs[0]->m_vtype == TYPE_VECTOR) {
                compile_error(exprs[1]->m_context, "vectors cannot be accessed this way");
                return false;
            }
            else {
                compile_error(exprs[1]->m_context, "type error: member-of operator on something that is not an entity or vector");
                return false;
            }
            break;

        case opid1('['):
            if (exprs[0]->m_vtype != TYPE_ARRAY &&
                !(exprs[0]->m_vtype == TYPE_FIELD &&
                  exprs[0]->m_next->m_vtype == TYPE_ARRAY))
            {
                ast_type_to_string(exprs[0], ty1, sizeof(ty1));
                compile_error(exprs[0]->m_context, "cannot index value of type %s", ty1);
                return false;
            }
            if (exprs[1]->m_vtype != TYPE_FLOAT) {
                ast_type_to_string(exprs[0], ty1, sizeof(ty1));
                compile_error(exprs[1]->m_context, "index must be of type float, not %s", ty1);
                return false;
            }
            out = ast_array_index::make(ctx, exprs[0], exprs[1]);
            rotate_entfield_array_index_nodes(&out);
            break;

        case opid1(','):
            if (sy->paren.size() && sy->paren.back() == PAREN_FUNC) {
                sy->out.push_back(syexp(ctx, exprs[0]));
                sy->out.push_back(syexp(ctx, exprs[1]));
                sy->argc.back()++;
                return true;
            }
            if (blocks[0]) {
                if (!blocks[0]->addExpr(exprs[1]))
                    return false;
            } else {
                blocks[0] = new ast_block(ctx);
                if (!blocks[0]->addExpr(exprs[0]) ||
                    !blocks[0]->addExpr(exprs[1]))
                {
                    return false;
                }
            }
            blocks[0]->setType(*exprs[1]);

            sy->out.push_back(syblock(ctx, blocks[0]));
            return true;

        case opid2('+','P'):
            out = exprs[0];
            break;
        case opid2('-','P'):
            if ((out = parser->m_fold.op(op, exprs)))
                break;

            if (exprs[0]->m_vtype != TYPE_FLOAT &&
                exprs[0]->m_vtype != TYPE_VECTOR) {
                    compile_error(ctx, "invalid types used in unary expression: cannot negate type %s",
                                  type_name[exprs[0]->m_vtype]);
                return false;
            }
            if (exprs[0]->m_vtype == TYPE_FLOAT)
                out = ast_unary::make(ctx, VINSTR_NEG_F, exprs[0]);
            else
                out = ast_unary::make(ctx, VINSTR_NEG_V, exprs[0]);
            break;

        case opid2('!','P'):
            if (!(out = parser->m_fold.op(op, exprs))) {
                switch (exprs[0]->m_vtype) {
                    case TYPE_FLOAT:
                        out = ast_unary::make(ctx, INSTR_NOT_F, exprs[0]);
                        break;
                    case TYPE_VECTOR:
                        out = ast_unary::make(ctx, INSTR_NOT_V, exprs[0]);
                        break;
                    case TYPE_STRING:
                        if (OPTS_FLAG(TRUE_EMPTY_STRINGS))
                            out = ast_unary::make(ctx, INSTR_NOT_F, exprs[0]);
                        else
                            out = ast_unary::make(ctx, INSTR_NOT_S, exprs[0]);
                        break;
                    /* we don't constant-fold NOT for these types */
                    case TYPE_ENTITY:
                        out = ast_unary::make(ctx, INSTR_NOT_ENT, exprs[0]);
                        break;
                    case TYPE_FUNCTION:
                        out = ast_unary::make(ctx, INSTR_NOT_FNC, exprs[0]);
                        break;
                    default:
                    compile_error(ctx, "invalid types used in expression: cannot logically negate type %s",
                                  type_name[exprs[0]->m_vtype]);
                    return false;
                }
            }
            break;

        case opid1('+'):
            if (exprs[0]->m_vtype != exprs[1]->m_vtype ||
               (exprs[0]->m_vtype != TYPE_VECTOR && exprs[0]->m_vtype != TYPE_FLOAT) )
            {
                compile_error(ctx, "invalid types used in expression: cannot add type %s and %s",
                              type_name[exprs[0]->m_vtype],
                              type_name[exprs[1]->m_vtype]);
                return false;
            }
            if (!(out = parser->m_fold.op(op, exprs))) {
                switch (exprs[0]->m_vtype) {
                    case TYPE_FLOAT:
                        out = fold::binary(ctx, INSTR_ADD_F, exprs[0], exprs[1]);
                        break;
                    case TYPE_VECTOR:
                        out = fold::binary(ctx, INSTR_ADD_V, exprs[0], exprs[1]);
                        break;
                    default:
                        compile_error(ctx, "invalid types used in expression: cannot add type %s and %s",
                                      type_name[exprs[0]->m_vtype],
                                      type_name[exprs[1]->m_vtype]);
                        return false;
                }
            }
            break;
        case opid1('-'):
            if  (exprs[0]->m_vtype != exprs[1]->m_vtype ||
                (exprs[0]->m_vtype != TYPE_VECTOR && exprs[0]->m_vtype != TYPE_FLOAT))
            {
                compile_error(ctx, "invalid types used in expression: cannot subtract type %s from %s",
                              type_name[exprs[1]->m_vtype],
                              type_name[exprs[0]->m_vtype]);
                return false;
            }
            if (!(out = parser->m_fold.op(op, exprs))) {
                switch (exprs[0]->m_vtype) {
                    case TYPE_FLOAT:
                        out = fold::binary(ctx, INSTR_SUB_F, exprs[0], exprs[1]);
                        break;
                    case TYPE_VECTOR:
                        out = fold::binary(ctx, INSTR_SUB_V, exprs[0], exprs[1]);
                        break;
                    default:
                        compile_error(ctx, "invalid types used in expression: cannot subtract type %s from %s",
                                      type_name[exprs[1]->m_vtype],
                                      type_name[exprs[0]->m_vtype]);
                        return false;
                }
            }
            break;
        case opid1('*'):
            if (exprs[0]->m_vtype != exprs[1]->m_vtype &&
                !(exprs[0]->m_vtype == TYPE_VECTOR &&
                  exprs[1]->m_vtype == TYPE_FLOAT) &&
                !(exprs[1]->m_vtype == TYPE_VECTOR &&
                  exprs[0]->m_vtype == TYPE_FLOAT)
                )
            {
                compile_error(ctx, "invalid types used in expression: cannot multiply types %s and %s",
                              type_name[exprs[1]->m_vtype],
                              type_name[exprs[0]->m_vtype]);
                return false;
            }
            if (!(out = parser->m_fold.op(op, exprs))) {
                switch (exprs[0]->m_vtype) {
                    case TYPE_FLOAT:
                        if (exprs[1]->m_vtype == TYPE_VECTOR)
                            out = fold::binary(ctx, INSTR_MUL_FV, exprs[0], exprs[1]);
                        else
                            out = fold::binary(ctx, INSTR_MUL_F, exprs[0], exprs[1]);
                        break;
                    case TYPE_VECTOR:
                        if (exprs[1]->m_vtype == TYPE_FLOAT)
                            out = fold::binary(ctx, INSTR_MUL_VF, exprs[0], exprs[1]);
                        else
                            out = fold::binary(ctx, INSTR_MUL_V, exprs[0], exprs[1]);
                        break;
                    default:
                        compile_error(ctx, "invalid types used in expression: cannot multiply types %s and %s",
                                      type_name[exprs[1]->m_vtype],
                                      type_name[exprs[0]->m_vtype]);
                        return false;
                }
            }
            break;

        case opid1('/'):
            if (exprs[1]->m_vtype != TYPE_FLOAT) {
                ast_type_to_string(exprs[0], ty1, sizeof(ty1));
                ast_type_to_string(exprs[1], ty2, sizeof(ty2));
                compile_error(ctx, "invalid types used in expression: cannot divide types %s and %s", ty1, ty2);
                return false;
            }
            if (!(out = parser->m_fold.op(op, exprs))) {
                if (exprs[0]->m_vtype == TYPE_FLOAT)
                    out = fold::binary(ctx, INSTR_DIV_F, exprs[0], exprs[1]);
                else {
                    ast_type_to_string(exprs[0], ty1, sizeof(ty1));
                    ast_type_to_string(exprs[1], ty2, sizeof(ty2));
                    compile_error(ctx, "invalid types used in expression: cannot divide types %s and %s", ty1, ty2);
                    return false;
                }
            }
            break;

        case opid1('%'):
            if (NotSameType(TYPE_FLOAT)) {
                compile_error(ctx, "invalid types used in expression: cannot perform modulo operation between types %s and %s",
                    type_name[exprs[0]->m_vtype],
                    type_name[exprs[1]->m_vtype]);
                return false;
            } else if (!(out = parser->m_fold.op(op, exprs))) {
                /* generate a call to __builtin_mod */
                ast_expression *mod  = parser->m_intrin.func("mod");
                ast_call       *call = nullptr;
                if (!mod) return false; /* can return null for missing floor */

                call = ast_call::make(parser_ctx(parser), mod);
                call->m_params.push_back(exprs[0]);
                call->m_params.push_back(exprs[1]);

                out = call;
            }
            break;

        case opid2('%','='):
            compile_error(ctx, "%= is unimplemented");
            return false;

        case opid1('|'):
        case opid1('&'):
        case opid1('^'):
            if ( !(exprs[0]->m_vtype == TYPE_FLOAT  && exprs[1]->m_vtype == TYPE_FLOAT) &&
                 !(exprs[0]->m_vtype == TYPE_VECTOR && exprs[1]->m_vtype == TYPE_FLOAT) &&
                 !(exprs[0]->m_vtype == TYPE_VECTOR && exprs[1]->m_vtype == TYPE_VECTOR))
            {
                compile_error(ctx, "invalid types used in expression: cannot perform bit operations between types %s and %s",
                              type_name[exprs[0]->m_vtype],
                              type_name[exprs[1]->m_vtype]);
                return false;
            }

            if (!(out = parser->m_fold.op(op, exprs))) {
                /*
                 * IF the first expression is float, the following will be too
                 * since scalar ^ vector is not allowed.
                 */
                if (exprs[0]->m_vtype == TYPE_FLOAT) {
                    out = fold::binary(ctx,
                        (op->id == opid1('^') ? VINSTR_BITXOR : op->id == opid1('|') ? INSTR_BITOR : INSTR_BITAND),
                        exprs[0], exprs[1]);
                } else {
                    /*
                     * The first is a vector: vector is allowed to bitop with vector and
                     * with scalar, branch here for the second operand.
                     */
                    if (exprs[1]->m_vtype == TYPE_VECTOR) {
                        /*
                         * Bitop all the values of the vector components against the
                         * vectors components in question.
                         */
                        out = fold::binary(ctx,
                            (op->id == opid1('^') ? VINSTR_BITXOR_V : op->id == opid1('|') ? VINSTR_BITOR_V : VINSTR_BITAND_V),
                            exprs[0], exprs[1]);
                    } else {
                        out = fold::binary(ctx,
                            (op->id == opid1('^') ? VINSTR_BITXOR_VF : op->id == opid1('|') ? VINSTR_BITOR_VF : VINSTR_BITAND_VF),
                            exprs[0], exprs[1]);
                    }
                }
            }
            break;

        case opid2('<','<'):
        case opid2('>','>'):
            if (NotSameType(TYPE_FLOAT)) {
                compile_error(ctx, "invalid types used in expression: cannot perform shift between types %s and %s",
                    type_name[exprs[0]->m_vtype],
                    type_name[exprs[1]->m_vtype]);
                return false;
            }

            if (!(out = parser->m_fold.op(op, exprs))) {
                ast_expression *shift = parser->m_intrin.func((op->id == opid2('<','<')) ? "__builtin_lshift" : "__builtin_rshift");
                ast_call *call  = ast_call::make(parser_ctx(parser), shift);
                call->m_params.push_back(exprs[0]);
                call->m_params.push_back(exprs[1]);
                out = call;
            }
            break;

        case opid3('<','<','='):
        case opid3('>','>','='):
            if (NotSameType(TYPE_FLOAT)) {
                compile_error(ctx, "invalid types used in expression: cannot perform shift operation between types %s and %s",
                    type_name[exprs[0]->m_vtype],
                    type_name[exprs[1]->m_vtype]);
                return false;
            }

            if(!(out = parser->m_fold.op(op, exprs))) {
                ast_expression *shift = parser->m_intrin.func((op->id == opid3('<','<','=')) ? "__builtin_lshift" : "__builtin_rshift");
                ast_call *call  = ast_call::make(parser_ctx(parser), shift);
                call->m_params.push_back(exprs[0]);
                call->m_params.push_back(exprs[1]);
                out = new ast_store(
                    parser_ctx(parser),
                    INSTR_STORE_F,
                    exprs[0],
                    call
                );
            }

            break;

        case opid2('|','|'):
            generated_op += 1; /* INSTR_OR */
            [[fallthrough]];
        case opid2('&','&'):
            generated_op += INSTR_AND;
            if (!(out = parser->m_fold.op(op, exprs))) {
                if (OPTS_FLAG(PERL_LOGIC) && !exprs[0]->compareType(*exprs[1])) {
                    ast_type_to_string(exprs[0], ty1, sizeof(ty1));
                    ast_type_to_string(exprs[1], ty2, sizeof(ty2));
                    compile_error(ctx, "invalid types for logical operation with -fperl-logic: %s and %s", ty1, ty2);
                    return false;
                }
                for (i = 0; i < 2; ++i) {
                    if (OPTS_FLAG(CORRECT_LOGIC) && exprs[i]->m_vtype == TYPE_VECTOR) {
                        out = ast_unary::make(ctx, INSTR_NOT_V, exprs[i]);
                        if (!out) break;
                        out = ast_unary::make(ctx, INSTR_NOT_F, out);
                        if (!out) break;
                        exprs[i] = out; out = nullptr;
                        if (OPTS_FLAG(PERL_LOGIC)) {
                            /* here we want to keep the right expressions' type */
                            break;
                        }
                    }
                    else if (OPTS_FLAG(FALSE_EMPTY_STRINGS) && exprs[i]->m_vtype == TYPE_STRING) {
                        out = ast_unary::make(ctx, INSTR_NOT_S, exprs[i]);
                        if (!out) break;
                        out = ast_unary::make(ctx, INSTR_NOT_F, out);
                        if (!out) break;
                        exprs[i] = out; out = nullptr;
                        if (OPTS_FLAG(PERL_LOGIC)) {
                            /* here we want to keep the right expressions' type */
                            break;
                        }
                    }
                }
                out = fold::binary(ctx, generated_op, exprs[0], exprs[1]);
            }
            break;

        case opid2('?',':'):
            if (sy->paren.back() != PAREN_TERNARY2) {
                compile_error(ctx, "mismatched parenthesis/ternary");
                return false;
            }
            sy->paren.pop_back();
            if (!exprs[1]->compareType(*exprs[2])) {
                ast_type_to_string(exprs[1], ty1, sizeof(ty1));
                ast_type_to_string(exprs[2], ty2, sizeof(ty2));
                compile_error(ctx, "operands of ternary expression must have the same type, got %s and %s", ty1, ty2);
                return false;
            }
            if (!(out = parser->m_fold.op(op, exprs)))
                out = new ast_ternary(ctx, exprs[0], exprs[1], exprs[2]);
            break;

        case opid2('*', '*'):
            if (NotSameType(TYPE_FLOAT)) {
                ast_type_to_string(exprs[0], ty1, sizeof(ty1));
                ast_type_to_string(exprs[1], ty2, sizeof(ty2));
                compile_error(ctx, "invalid types used in exponentiation: %s and %s",
                    ty1, ty2);
                return false;
            }

            if (!(out = parser->m_fold.op(op, exprs))) {
                ast_call *gencall = ast_call::make(parser_ctx(parser), parser->m_intrin.func("pow"));
                gencall->m_params.push_back(exprs[0]);
                gencall->m_params.push_back(exprs[1]);
                out = gencall;
            }
            break;

        case opid2('>', '<'):
            if (NotSameType(TYPE_VECTOR)) {
                ast_type_to_string(exprs[0], ty1, sizeof(ty1));
                ast_type_to_string(exprs[1], ty2, sizeof(ty2));
                compile_error(ctx, "invalid types used in cross product: %s and %s",
                    ty1, ty2);
                return false;
            }

            if (!(out = parser->m_fold.op(op, exprs))) {
                out = fold::binary(
                    parser_ctx(parser),
                    VINSTR_CROSS,
                    exprs[0],
                    exprs[1]
                );
            }

            break;

        case opid3('<','=','>'): /* -1, 0, or 1 */
            if (NotSameType(TYPE_FLOAT)) {
                ast_type_to_string(exprs[0], ty1, sizeof(ty1));
                ast_type_to_string(exprs[1], ty2, sizeof(ty2));
                compile_error(ctx, "invalid types used in comparision: %s and %s",
                    ty1, ty2);

                return false;
            }

            if (!(out = parser->m_fold.op(op, exprs))) {
                /* This whole block is NOT fold_binary safe */
                ast_binary *eq = new ast_binary(ctx, INSTR_EQ_F, exprs[0], exprs[1]);

                eq->m_refs = AST_REF_NONE;

                    /* if (lt) { */
                out = new ast_ternary(ctx,
                        new ast_binary(ctx, INSTR_LT, exprs[0], exprs[1]),
                        /* out = -1 */
                        parser->m_fold.imm_float(2),
                    /* } else { */
                        /* if (eq) { */
                        new ast_ternary(ctx, eq,
                            /* out = 0 */
                            parser->m_fold.imm_float(0),
                        /* } else { */
                            /* out = 1 */
                            parser->m_fold.imm_float(1)
                        /* } */
                        )
                    /* } */
                    );

            }
            break;

        case opid1('>'):
            generated_op += 1; /* INSTR_GT */
            [[fallthrough]];
        case opid1('<'):
            generated_op += 1; /* INSTR_LT */
            [[fallthrough]];
        case opid2('>', '='):
            generated_op += 1; /* INSTR_GE */
            [[fallthrough]];
        case opid2('<', '='):
            generated_op += INSTR_LE;
            if (NotSameType(TYPE_FLOAT)) {
                compile_error(ctx, "invalid types used in expression: cannot perform comparison between types %s and %s",
                              type_name[exprs[0]->m_vtype],
                              type_name[exprs[1]->m_vtype]);
                return false;
            }
            if (!(out = parser->m_fold.op(op, exprs)))
                out = fold::binary(ctx, generated_op, exprs[0], exprs[1]);
            break;
        case opid2('!', '='):
            if (exprs[0]->m_vtype != exprs[1]->m_vtype) {
                compile_error(ctx, "invalid types used in expression: cannot perform comparison between types %s and %s",
                              type_name[exprs[0]->m_vtype],
                              type_name[exprs[1]->m_vtype]);
                return false;
            }
            if (!(out = parser->m_fold.op(op, exprs)))
                out = fold::binary(ctx, type_ne_instr[exprs[0]->m_vtype], exprs[0], exprs[1]);
            break;
        case opid2('=', '='):
            if (exprs[0]->m_vtype != exprs[1]->m_vtype) {
                compile_error(ctx, "invalid types used in expression: cannot perform comparison between types %s and %s",
                              type_name[exprs[0]->m_vtype],
                              type_name[exprs[1]->m_vtype]);
                return false;
            }
            if (!(out = parser->m_fold.op(op, exprs)))
                out = fold::binary(ctx, type_eq_instr[exprs[0]->m_vtype], exprs[0], exprs[1]);
            break;

        case opid1('='):
            if (ast_istype(exprs[0], ast_entfield)) {
                ast_expression *field = ((ast_entfield*)exprs[0])->m_field;
                assignop = store_op_for(exprs[0]);
                if (assignop == VINSTR_END || !field->m_next->compareType(*exprs[1]))
                {
                    ast_type_to_string(field->m_next, ty1, sizeof(ty1));
                    ast_type_to_string(exprs[1], ty2, sizeof(ty2));
                    if (OPTS_FLAG(ASSIGN_FUNCTION_TYPES) &&
                        field->m_next->m_vtype == TYPE_FUNCTION &&
                        exprs[1]->m_vtype == TYPE_FUNCTION)
                    {
                        (void)!compile_warning(ctx, WARN_ASSIGN_FUNCTION_TYPES,
                                               "invalid types in assignment: cannot assign %s to %s", ty2, ty1);
                    }
                    else
                        compile_error(ctx, "invalid types in assignment: cannot assign %s to %s", ty2, ty1);
                }
            }
            else
            {
                assignop = store_op_for(exprs[0]);

                if (assignop == VINSTR_END) {
                    ast_type_to_string(exprs[0], ty1, sizeof(ty1));
                    ast_type_to_string(exprs[1], ty2, sizeof(ty2));
                    compile_error(ctx, "invalid types in assignment: cannot assign %s to %s", ty2, ty1);
                }
                else if (!exprs[0]->compareType(*exprs[1]))
                {
                    ast_type_to_string(exprs[0], ty1, sizeof(ty1));
                    ast_type_to_string(exprs[1], ty2, sizeof(ty2));
                    if (OPTS_FLAG(ASSIGN_FUNCTION_TYPES) &&
                        exprs[0]->m_vtype == TYPE_FUNCTION &&
                        exprs[1]->m_vtype == TYPE_FUNCTION)
                    {
                        (void)!compile_warning(ctx, WARN_ASSIGN_FUNCTION_TYPES,
                                               "invalid types in assignment: cannot assign %s to %s", ty2, ty1);
                    }
                    else
                        compile_error(ctx, "invalid types in assignment: cannot assign %s to %s", ty2, ty1);
                }
            }
            (void)check_write_to(ctx, exprs[0]);
            /* When we're a vector of part of an entity field we use STOREP */
            if (ast_istype(exprs[0], ast_member) && ast_istype(((ast_member*)exprs[0])->m_owner, ast_entfield))
                assignop = INSTR_STOREP_F;
            out = new ast_store(ctx, assignop, exprs[0], exprs[1]);
            break;
        case opid3('+','+','P'):
        case opid3('-','-','P'):
            /* prefix ++ */
            if (exprs[0]->m_vtype != TYPE_FLOAT) {
                ast_type_to_string(exprs[0], ty1, sizeof(ty1));
                compile_error(exprs[0]->m_context, "invalid type for prefix increment: %s", ty1);
                return false;
            }
            if (op->id == opid3('+','+','P'))
                addop = INSTR_ADD_F;
            else
                addop = INSTR_SUB_F;
            (void)check_write_to(exprs[0]->m_context, exprs[0]);
            if (ast_istype(exprs[0], ast_entfield)) {
                out = new ast_binstore(ctx, INSTR_STOREP_F, addop,
                                       exprs[0],
                                       parser->m_fold.imm_float(1));
            } else {
                out = new ast_binstore(ctx, INSTR_STORE_F, addop,
                                       exprs[0],
                                       parser->m_fold.imm_float(1));
            }
            break;
        case opid3('S','+','+'):
        case opid3('S','-','-'):
            /* prefix ++ */
            if (exprs[0]->m_vtype != TYPE_FLOAT) {
                ast_type_to_string(exprs[0], ty1, sizeof(ty1));
                compile_error(exprs[0]->m_context, "invalid type for suffix increment: %s", ty1);
                return false;
            }
            if (op->id == opid3('S','+','+')) {
                addop = INSTR_ADD_F;
                subop = INSTR_SUB_F;
            } else {
                addop = INSTR_SUB_F;
                subop = INSTR_ADD_F;
            }
            (void)check_write_to(exprs[0]->m_context, exprs[0]);
            if (ast_istype(exprs[0], ast_entfield)) {
                out = new ast_binstore(ctx, INSTR_STOREP_F, addop,
                                       exprs[0],
                                       parser->m_fold.imm_float(1));
            } else {
                out = new ast_binstore(ctx, INSTR_STORE_F, addop,
                                       exprs[0],
                                       parser->m_fold.imm_float(1));
            }
            if (!out)
                return false;
            out = fold::binary(ctx, subop,
                              out,
                              parser->m_fold.imm_float(1));

            break;
        case opid2('+','='):
        case opid2('-','='):
            if (exprs[0]->m_vtype != exprs[1]->m_vtype ||
                (exprs[0]->m_vtype != TYPE_VECTOR && exprs[0]->m_vtype != TYPE_FLOAT) )
            {
                ast_type_to_string(exprs[0], ty1, sizeof(ty1));
                ast_type_to_string(exprs[1], ty2, sizeof(ty2));
                compile_error(ctx, "invalid types used in expression: cannot add or subtract type %s and %s",
                              ty1, ty2);
                return false;
            }
            (void)check_write_to(ctx, exprs[0]);
            assignop = store_op_for(exprs[0]);
            switch (exprs[0]->m_vtype) {
                case TYPE_FLOAT:
                    out = new ast_binstore(ctx, assignop,
                                           (op->id == opid2('+','=') ? INSTR_ADD_F : INSTR_SUB_F),
                                           exprs[0], exprs[1]);
                    break;
                case TYPE_VECTOR:
                    out = new ast_binstore(ctx, assignop,
                                           (op->id == opid2('+','=') ? INSTR_ADD_V : INSTR_SUB_V),
                                           exprs[0], exprs[1]);
                    break;
                default:
                    compile_error(ctx, "invalid types used in expression: cannot add or subtract type %s and %s",
                                  type_name[exprs[0]->m_vtype],
                                  type_name[exprs[1]->m_vtype]);
                    return false;
            };
            break;
        case opid2('*','='):
        case opid2('/','='):
            if (exprs[1]->m_vtype != TYPE_FLOAT ||
                !(exprs[0]->m_vtype == TYPE_FLOAT ||
                  exprs[0]->m_vtype == TYPE_VECTOR))
            {
                ast_type_to_string(exprs[0], ty1, sizeof(ty1));
                ast_type_to_string(exprs[1], ty2, sizeof(ty2));
                compile_error(ctx, "invalid types used in expression: %s and %s",
                              ty1, ty2);
                return false;
            }
            (void)check_write_to(ctx, exprs[0]);
            assignop = store_op_for(exprs[0]);
            switch (exprs[0]->m_vtype) {
                case TYPE_FLOAT:
                    out = new ast_binstore(ctx, assignop,
                                           (op->id == opid2('*','=') ? INSTR_MUL_F : INSTR_DIV_F),
                                           exprs[0], exprs[1]);
                    break;
                case TYPE_VECTOR:
                    if (op->id == opid2('*','=')) {
                        out = new ast_binstore(ctx, assignop, INSTR_MUL_VF,
                                               exprs[0], exprs[1]);
                    } else {
                        out = fold::binary(ctx, INSTR_DIV_F,
                                         parser->m_fold.imm_float(1),
                                         exprs[1]);
                        if (!out) {
                            compile_error(ctx, "internal error: failed to generate division");
                            return false;
                        }
                        out = new ast_binstore(ctx, assignop, INSTR_MUL_VF,
                                               exprs[0], out);
                    }
                    break;
                default:
                    compile_error(ctx, "invalid types used in expression: cannot add or subtract type %s and %s",
                                  type_name[exprs[0]->m_vtype],
                                  type_name[exprs[1]->m_vtype]);
                    return false;
            };
            break;
        case opid2('&','='):
        case opid2('|','='):
        case opid2('^','='):
            if (NotSameType(TYPE_FLOAT) && NotSameType(TYPE_VECTOR)) {
                ast_type_to_string(exprs[0], ty1, sizeof(ty1));
                ast_type_to_string(exprs[1], ty2, sizeof(ty2));
                compile_error(ctx, "invalid types used in expression: %s and %s",
                              ty1, ty2);
                return false;
            }
            (void)check_write_to(ctx, exprs[0]);
            assignop = store_op_for(exprs[0]);
            if (exprs[0]->m_vtype == TYPE_FLOAT)
                out = new ast_binstore(ctx, assignop,
                                       (op->id == opid2('^','=') ? VINSTR_BITXOR : op->id == opid2('&','=') ? INSTR_BITAND : INSTR_BITOR),
                                       exprs[0], exprs[1]);
            else
                out = new ast_binstore(ctx, assignop,
                                       (op->id == opid2('^','=') ? VINSTR_BITXOR_V : op->id == opid2('&','=') ? VINSTR_BITAND_V : VINSTR_BITOR_V),
                                       exprs[0], exprs[1]);
            break;
        case opid3('&','~','='):
            /* This is like: a &= ~(b);
             * But QC has no bitwise-not, so we implement it as
             * a -= a & (b);
             */
            if (NotSameType(TYPE_FLOAT) && NotSameType(TYPE_VECTOR)) {
                ast_type_to_string(exprs[0], ty1, sizeof(ty1));
                ast_type_to_string(exprs[1], ty2, sizeof(ty2));
                compile_error(ctx, "invalid types used in expression: %s and %s",
                              ty1, ty2);
                return false;
            }
            assignop = store_op_for(exprs[0]);
            if (exprs[0]->m_vtype == TYPE_FLOAT)
                out = fold::binary(ctx, INSTR_BITAND, exprs[0], exprs[1]);
            else
                out = fold::binary(ctx, VINSTR_BITAND_V, exprs[0], exprs[1]);
            if (!out)
                return false;
            (void)check_write_to(ctx, exprs[0]);
            if (exprs[0]->m_vtype == TYPE_FLOAT)
                asbinstore = new ast_binstore(ctx, assignop, INSTR_SUB_F, exprs[0], out);
            else
                asbinstore = new ast_binstore(ctx, assignop, INSTR_SUB_V, exprs[0], out);
            asbinstore->m_keep_dest = true;
            out = asbinstore;
            break;

        case opid3('l', 'e', 'n'):
            if (exprs[0]->m_vtype != TYPE_STRING && exprs[0]->m_vtype != TYPE_ARRAY) {
                ast_type_to_string(exprs[0], ty1, sizeof(ty1));
                compile_error(exprs[0]->m_context, "invalid type for length operator: %s", ty1);
                return false;
            }
            /* strings must be const, arrays are statically sized */
            if (exprs[0]->m_vtype == TYPE_STRING &&
                !(((ast_value*)exprs[0])->m_hasvalue && ((ast_value*)exprs[0])->m_cvq == CV_CONST))
            {
                compile_error(exprs[0]->m_context, "operand of length operator not a valid constant expression");
                return false;
            }
            out = parser->m_fold.op(op, exprs);
            break;

        case opid2('~', 'P'):
            if (exprs[0]->m_vtype != TYPE_FLOAT && exprs[0]->m_vtype != TYPE_VECTOR) {
                ast_type_to_string(exprs[0], ty1, sizeof(ty1));
                compile_error(exprs[0]->m_context, "invalid type for bit not: %s", ty1);
                return false;
            }
            if (!(out = parser->m_fold.op(op, exprs))) {
                if (exprs[0]->m_vtype == TYPE_FLOAT) {
                    out = fold::binary(ctx, INSTR_SUB_F, parser->m_fold.imm_float(2), exprs[0]);
                } else {
                    out = fold::binary(ctx, INSTR_SUB_V, parser->m_fold.imm_vector(1), exprs[0]);
                }
            }
            break;
    }
#undef NotSameType
    if (!out) {
        compile_error(ctx, "failed to apply operator %s", op->op);
        return false;
    }

    sy->out.push_back(syexp(ctx, out));
    return true;
}

static bool parser_close_call(parser_t *parser, shunt *sy)
{
    if (!parser->function)
    {
        parseerror(parser, "cannot call functions from global scope");
        return false;
    }

    /* was a function call */
    ast_expression *fun;
    ast_value      *funval = nullptr;
    ast_call       *call;

    size_t          fid;
    size_t          paramcount, i;
    bool            fold = true;

    fid = sy->ops.back().off;
    sy->ops.pop_back();

    /* out[fid] is the function
     * everything above is parameters...
     */
    if (sy->argc.empty()) {
        parseerror(parser, "internal error: no argument counter available");
        return false;
    }

    paramcount = sy->argc.back();
    sy->argc.pop_back();

    if (sy->out.size() < fid) {
        parseerror(parser, "internal error: broken function call %zu < %zu+%zu\n",
                   sy->out.size(),
                   fid,
                   paramcount);
        return false;
    }

    /*
     * TODO handle this at the intrinsic level with an ast_intrinsic
     * node and codegen.
     */
    if ((fun = sy->out[fid].out) == parser->m_intrin.debug_typestring()) {
        char ty[1024];
        if (fid+2 != sy->out.size() || sy->out.back().block) {
            parseerror(parser, "intrinsic __builtin_debug_typestring requires exactly 1 parameter");
            return false;
        }
        ast_type_to_string(sy->out.back().out, ty, sizeof(ty));
        ast_unref(sy->out.back().out);
        sy->out[fid] = syexp(sy->out.back().out->m_context,
                             parser->m_fold.constgen_string(ty, false));
        sy->out.pop_back();
        return true;
    }

    /*
     * Now we need to determine if the function that is being called is
     * an intrinsic so we can evaluate if the arguments to it are constant
     * and than fruitfully fold them.
     */
#define fold_can_1(X)  \
    (ast_istype(((X)), ast_value) && (X)->m_hasvalue && ((X)->m_cvq == CV_CONST) && \
                ((X))->m_vtype != TYPE_FUNCTION)

    if (fid + 1 < sy->out.size())
        ++paramcount;

    for (i = 0; i < paramcount; ++i) {
        if (!fold_can_1((ast_value*)sy->out[fid + 1 + i].out)) {
            fold = false;
            break;
        }
    }

    /*
     * All is well which ends well, if we make it into here we can ignore the
     * intrinsic call and just evaluate it i.e constant fold it.
     */
    if (fold && ast_istype(fun, ast_value) && ((ast_value*)fun)->m_intrinsic) {
        std::vector<ast_expression*> exprs;
        ast_expression *foldval = nullptr;

        exprs.reserve(paramcount);
        for (i = 0; i < paramcount; i++)
            exprs.push_back(sy->out[fid+1 + i].out);

        if (!(foldval = parser->m_intrin.do_fold((ast_value*)fun, exprs.data()))) {
            goto fold_leave;
        }

        /*
         * Blub: what sorts of unreffing and resizing of
         * sy->out should I be doing here?
         */
        sy->out[fid] = syexp(foldval->m_context, foldval);
        sy->out.erase(sy->out.end() - paramcount, sy->out.end());

        return true;
    }

    fold_leave:
    call = ast_call::make(sy->ops[sy->ops.size()].ctx, fun);

    if (!call)
        return false;

    if (fid+1 + paramcount != sy->out.size()) {
        parseerror(parser, "internal error: parameter count mismatch: (%zu+1+%zu), %zu",
                   fid,
                   paramcount,
                   sy->out.size());
        return false;
    }

    for (i = 0; i < paramcount; ++i)
        call->m_params.push_back(sy->out[fid+1 + i].out);
    sy->out.erase(sy->out.end() - paramcount, sy->out.end());
    (void)!call->checkTypes(parser->function->m_function_type->m_varparam);
    if (parser->max_param_count < paramcount)
        parser->max_param_count = paramcount;

    if (ast_istype(fun, ast_value)) {
        funval = (ast_value*)fun;
        if ((fun->m_flags & AST_FLAG_VARIADIC) &&
            !(/*funval->m_cvq == CV_CONST && */ funval->m_hasvalue && funval->m_constval.vfunc->m_builtin))
        {
            call->m_va_count = parser->m_fold.constgen_float((qcfloat_t)paramcount, false);
        }
    }

    /* overwrite fid, the function, with a call */
    sy->out[fid] = syexp(call->m_context, call);

    if (fun->m_vtype != TYPE_FUNCTION) {
        parseerror(parser, "not a function (%s)", type_name[fun->m_vtype]);
        return false;
    }

    if (!fun->m_next) {
        parseerror(parser, "could not determine function return type");
        return false;
    } else {
        ast_value *fval = (ast_istype(fun, ast_value) ? ((ast_value*)fun) : nullptr);

        if (fun->m_flags & AST_FLAG_DEPRECATED) {
            if (!fval) {
                return !parsewarning(parser, WARN_DEPRECATED,
                        "call to function (which is marked deprecated)\n",
                        "-> it has been declared here: %s:%i",
                        fun->m_context.file, fun->m_context.line);
            }
            if (!fval->m_desc.length()) {
                return !parsewarning(parser, WARN_DEPRECATED,
                        "call to `%s` (which is marked deprecated)\n"
                        "-> `%s` declared here: %s:%i",
                        fval->m_name, fval->m_name, fun->m_context.file, fun->m_context.line);
            }
            return !parsewarning(parser, WARN_DEPRECATED,
                    "call to `%s` (deprecated: %s)\n"
                    "-> `%s` declared here: %s:%i",
                    fval->m_name, fval->m_desc, fval->m_name, fun->m_context.file,
                    fun->m_context.line);
        }

        if (fun->m_type_params.size() != paramcount &&
            !((fun->m_flags & AST_FLAG_VARIADIC) &&
              fun->m_type_params.size() < paramcount))
        {
            const char *fewmany = (fun->m_type_params.size() > paramcount) ? "few" : "many";
            if (fval)
                return !parsewarning(parser, WARN_INVALID_PARAMETER_COUNT,
                                     "too %s parameters for call to %s: expected %i, got %i\n"
                                     " -> `%s` has been declared here: %s:%i",
                                     fewmany, fval->m_name, (int)fun->m_type_params.size(), (int)paramcount,
                                     fval->m_name, fun->m_context.file, (int)fun->m_context.line);
            else
                return !parsewarning(parser, WARN_INVALID_PARAMETER_COUNT,
                                     "too %s parameters for function call: expected %i, got %i\n"
                                     " -> it has been declared here: %s:%i",
                                     fewmany, (int)fun->m_type_params.size(), (int)paramcount,
                                     fun->m_context.file, (int)fun->m_context.line);
        }
    }

    return true;
}

static bool parser_close_paren(parser_t *parser, shunt *sy)
{
    if (sy->ops.empty()) {
        parseerror(parser, "unmatched closing paren");
        return false;
    }

    while (sy->ops.size()) {
        if (sy->ops.back().isparen) {
            if (sy->paren.back() == PAREN_FUNC) {
                sy->paren.pop_back();
                if (!parser_close_call(parser, sy))
                    return false;
                break;
            }
            if (sy->paren.back() == PAREN_EXPR) {
                sy->paren.pop_back();
                if (sy->out.empty()) {
                    compile_error(sy->ops.back().ctx, "empty paren expression");
                    sy->ops.pop_back();
                    return false;
                }
                sy->ops.pop_back();
                break;
            }
            if (sy->paren.back() == PAREN_INDEX) {
                sy->paren.pop_back();
                // pop off the parenthesis
                sy->ops.pop_back();
                /* then apply the index operator */
                if (!parser_sy_apply_operator(parser, sy))
                    return false;
                break;
            }
            if (sy->paren.back() == PAREN_TERNARY1) {
                sy->paren.back() = PAREN_TERNARY2;
                // pop off the parenthesis
                sy->ops.pop_back();
                break;
            }
            compile_error(sy->ops.back().ctx, "invalid parenthesis");
            return false;
        }
        if (!parser_sy_apply_operator(parser, sy))
            return false;
    }
    return true;
}

static void parser_reclassify_token(parser_t *parser)
{
    size_t i;
    if (parser->tok >= TOKEN_START)
        return;
    for (i = 0; i < operator_count; ++i) {
        if (!strcmp(parser_tokval(parser), operators[i].op)) {
            parser->tok = TOKEN_OPERATOR;
            return;
        }
    }
}

static ast_expression* parse_vararg_do(parser_t *parser)
{
    ast_expression *idx, *out;
    ast_value      *typevar;
    ast_value      *funtype = parser->function->m_function_type;
    lex_ctx_t         ctx     = parser_ctx(parser);

    if (!parser->function->m_varargs) {
        parseerror(parser, "function has no variable argument list");
        return nullptr;
    }

    if (!parser_next(parser) || parser->tok != '(') {
        parseerror(parser, "expected parameter index and type in parenthesis");
        return nullptr;
    }
    if (!parser_next(parser)) {
        parseerror(parser, "error parsing parameter index");
        return nullptr;
    }

    idx = parse_expression_leave(parser, true, false, false);
    if (!idx)
        return nullptr;

    if (parser->tok != ',') {
        if (parser->tok != ')') {
            ast_unref(idx);
            parseerror(parser, "expected comma after parameter index");
            return nullptr;
        }
        // vararg piping: ...(start)
        out = new ast_argpipe(ctx, idx);
        return out;
    }

    if (!parser_next(parser) || (parser->tok != TOKEN_IDENT && parser->tok != TOKEN_TYPENAME)) {
        ast_unref(idx);
        parseerror(parser, "expected typename for vararg");
        return nullptr;
    }

    typevar = parse_typename(parser, nullptr, nullptr, nullptr);
    if (!typevar) {
        ast_unref(idx);
        return nullptr;
    }

    if (parser->tok != ')') {
        ast_unref(idx);
        delete typevar;
        parseerror(parser, "expected closing paren");
        return nullptr;
    }

    if (funtype->m_varparam &&
        !typevar->compareType(*funtype->m_varparam))
    {
        char ty1[1024];
        char ty2[1024];
        ast_type_to_string(typevar, ty1, sizeof(ty1));
        ast_type_to_string(funtype->m_varparam, ty2, sizeof(ty2));
        compile_error(typevar->m_context,
                      "function was declared to take varargs of type `%s`, requested type is: %s",
                      ty2, ty1);
    }

    out = ast_array_index::make(ctx, parser->function->m_varargs.get(), idx);
    out->adoptType(*typevar);
    delete typevar;
    return out;
}

static ast_expression* parse_vararg(parser_t *parser)
{
    bool           old_noops = parser->lex->flags.noops;

    ast_expression *out;

    parser->lex->flags.noops = true;
    out = parse_vararg_do(parser);

    parser->lex->flags.noops = old_noops;
    return out;
}

/* not to be exposed */
bool ftepp_predef_exists(const char *name);
static bool parse_sya_operand(parser_t *parser, shunt *sy, bool with_labels)
{
    if (OPTS_FLAG(TRANSLATABLE_STRINGS) &&
        parser->tok == TOKEN_IDENT &&
        !strcmp(parser_tokval(parser), "_"))
    {
        /* a translatable string */
        ast_value *val;

        parser->lex->flags.noops = true;
        if (!parser_next(parser) || parser->tok != '(') {
            parseerror(parser, "use _(\"string\") to create a translatable string constant");
            return false;
        }
        parser->lex->flags.noops = false;
        if (!parser_next(parser) || parser->tok != TOKEN_STRINGCONST) {
            parseerror(parser, "expected a constant string in translatable-string extension");
            return false;
        }
        val = (ast_value*)parser->m_fold.constgen_string(parser_tokval(parser), true);
        if (!val)
            return false;
        sy->out.push_back(syexp(parser_ctx(parser), val));

        if (!parser_next(parser) || parser->tok != ')') {
            parseerror(parser, "expected closing paren after translatable string");
            return false;
        }
        return true;
    }
    else if (parser->tok == TOKEN_DOTS)
    {
        ast_expression *va;
        if (!OPTS_FLAG(VARIADIC_ARGS)) {
            parseerror(parser, "cannot access varargs (try -fvariadic-args)");
            return false;
        }
        va = parse_vararg(parser);
        if (!va)
            return false;
        sy->out.push_back(syexp(parser_ctx(parser), va));
        return true;
    }
    else if (parser->tok == TOKEN_FLOATCONST) {
        ast_expression *val = parser->m_fold.constgen_float((parser_token(parser)->constval.f), false);
        if (!val)
            return false;
        sy->out.push_back(syexp(parser_ctx(parser), val));
        return true;
    }
    else if (parser->tok == TOKEN_INTCONST || parser->tok == TOKEN_CHARCONST) {
        ast_expression *val = parser->m_fold.constgen_float((qcfloat_t)(parser_token(parser)->constval.i), false);
        if (!val)
            return false;
        sy->out.push_back(syexp(parser_ctx(parser), val));
        return true;
    }
    else if (parser->tok == TOKEN_STRINGCONST) {
        ast_expression *val = parser->m_fold.constgen_string(parser_tokval(parser), false);
        if (!val)
            return false;
        sy->out.push_back(syexp(parser_ctx(parser), val));
        return true;
    }
    else if (parser->tok == TOKEN_VECTORCONST) {
        ast_expression *val = parser->m_fold.constgen_vector(parser_token(parser)->constval.v);
        if (!val)
            return false;
        sy->out.push_back(syexp(parser_ctx(parser), val));
        return true;
    }
    else if (parser->tok == TOKEN_IDENT)
    {
        const char     *ctoken = parser_tokval(parser);
        ast_expression *prev = sy->out.size() ? sy->out.back().out : nullptr;
        ast_expression *var;
        /* a_vector.{x,y,z} */
        if (sy->ops.empty() ||
            !sy->ops.back().etype ||
            operators[sy->ops.back().etype-1].id != opid1('.'))
        {
            /* When adding more intrinsics, fix the above condition */
            prev = nullptr;
        }
        if (prev && prev->m_vtype == TYPE_VECTOR && ctoken[0] >= 'x' && ctoken[0] <= 'z' && !ctoken[1])
        {
            var = parser->const_vec[ctoken[0]-'x'];
        } else {
            var = parser_find_var(parser, parser_tokval(parser));
            if (!var)
                var = parser_find_field(parser, parser_tokval(parser));
        }
        if (!var && with_labels) {
            var = parser_find_label(parser, parser_tokval(parser));
            if (!with_labels) {
                ast_label *lbl = new ast_label(parser_ctx(parser), parser_tokval(parser), true);
                var = lbl;
                parser->labels.push_back(lbl);
            }
        }
        if (!var && !strcmp(parser_tokval(parser), "__FUNC__"))
            var = parser->m_fold.constgen_string(parser->function->m_name, false);
        if (!var) {
            /*
             * now we try for the real intrinsic hashtable. If the string
             * begins with __builtin, we simply skip past it, otherwise we
             * use the identifier as is.
             */
            if (!strncmp(parser_tokval(parser), "__builtin_", 10)) {
                var = parser->m_intrin.func(parser_tokval(parser));
            }

            /*
             * Try it again, intrin_func deals with the alias method as well
             * the first one masks for __builtin though, we emit warning here.
             */
            if (!var) {
                if ((var = parser->m_intrin.func(parser_tokval(parser)))) {
                    (void)!compile_warning(
                        parser_ctx(parser),
                        WARN_BUILTINS,
                        "using implicitly defined builtin `__builtin_%s' for `%s'",
                        parser_tokval(parser),
                        parser_tokval(parser)
                    );
                }
            }


            if (!var) {
                /*
                 * sometimes people use preprocessing predefs without enabling them
                 * i've done this thousands of times already myself.  Lets check for
                 * it in the predef table.  And diagnose it better :)
                 */
                if (!OPTS_FLAG(FTEPP_PREDEFS) && ftepp_predef_exists(parser_tokval(parser))) {
                    parseerror(parser, "unexpected identifier: %s (use -fftepp-predef to enable pre-defined macros)", parser_tokval(parser));
                    return false;
                }

                parseerror(parser, "unexpected identifier: %s", parser_tokval(parser));
                return false;
            }
        }
        else
        {
            // promote these to norefs
            if (ast_istype(var, ast_value))
            {
                ((ast_value *)var)->m_flags |= AST_FLAG_NOREF;
            }
            else if (ast_istype(var, ast_member))
            {
                ast_member *mem = (ast_member *)var;
                if (ast_istype(mem->m_owner, ast_value))
                    ((ast_value *)mem->m_owner)->m_flags |= AST_FLAG_NOREF;
            }
        }
        sy->out.push_back(syexp(parser_ctx(parser), var));
        return true;
    }
    parseerror(parser, "unexpected token `%s`", parser_tokval(parser));
    return false;
}

static ast_expression* parse_expression_leave(parser_t *parser, bool stopatcomma, bool truthvalue, bool with_labels)
{
    ast_expression *expr = nullptr;
    shunt sy;
    bool wantop = false;
    /* only warn once about an assignment in a truth value because the current code
     * would trigger twice on: if(a = b && ...), once for the if-truth-value, once for the && part
     */
    bool warn_parenthesis = true;

    /* count the parens because an if starts with one, so the
     * end of a condition is an unmatched closing paren
     */
    int ternaries = 0;

    memset(&sy, 0, sizeof(sy));

    parser->lex->flags.noops = false;

    parser_reclassify_token(parser);

    while (true)
    {
        if (parser->tok == TOKEN_TYPENAME) {
            parseerror(parser, "unexpected typename `%s`", parser_tokval(parser));
            goto onerr;
        }

        if (parser->tok == TOKEN_OPERATOR)
        {
            /* classify the operator */
            const oper_info *op;
            const oper_info *olast = nullptr;
            size_t o;
            for (o = 0; o < operator_count; ++o) {
                if (((!(operators[o].flags & OP_PREFIX) == !!wantop)) &&
                    /* !(operators[o].flags & OP_SUFFIX) && / * remove this */
                    !strcmp(parser_tokval(parser), operators[o].op))
                {
                    break;
                }
            }
            if (o == operator_count) {
                compile_error(parser_ctx(parser), "unexpected operator: %s", parser_tokval(parser));
                goto onerr;
            }
            /* found an operator */
            op = &operators[o];

            /* when declaring variables, a comma starts a new variable */
            if (op->id == opid1(',') && sy.paren.empty() && stopatcomma) {
                /* fixup the token */
                parser->tok = ',';
                break;
            }

            /* a colon without a pervious question mark cannot be a ternary */
            if (!ternaries && op->id == opid2(':','?')) {
                parser->tok = ':';
                break;
            }

            if (op->id == opid1(',')) {
                if (sy.paren.size() && sy.paren.back() == PAREN_TERNARY2) {
                    (void)!parsewarning(parser, WARN_TERNARY_PRECEDENCE, "suggesting parenthesis around ternary expression");
                }
            }

            if (sy.ops.size() && !sy.ops.back().isparen)
                olast = &operators[sy.ops.back().etype-1];

            /* first only apply higher precedences, assoc_left+equal comes after we warn about precedence rules */
            while (olast && op->prec < olast->prec)
            {
                if (!parser_sy_apply_operator(parser, &sy))
                    goto onerr;
                if (sy.ops.size() && !sy.ops.back().isparen)
                    olast = &operators[sy.ops.back().etype-1];
                else
                    olast = nullptr;
            }

#define IsAssignOp(x) (\
                (x) == opid1('=') || \
                (x) == opid2('+','=') || \
                (x) == opid2('-','=') || \
                (x) == opid2('*','=') || \
                (x) == opid2('/','=') || \
                (x) == opid2('%','=') || \
                (x) == opid2('&','=') || \
                (x) == opid2('|','=') || \
                (x) == opid3('&','~','=') \
                )
            if (warn_parenthesis) {
                if ( (olast && IsAssignOp(olast->id) && (op->id == opid2('&','&') || op->id == opid2('|','|'))) ||
                     (olast && IsAssignOp(op->id) && (olast->id == opid2('&','&') || olast->id == opid2('|','|'))) ||
                     (truthvalue && sy.paren.empty() && IsAssignOp(op->id))
                   )
                {
                    (void)!parsewarning(parser, WARN_PARENTHESIS, "suggesting parenthesis around assignment used as truth value");
                    warn_parenthesis = false;
                }

                if (olast && olast->id != op->id) {
                    if ((op->id    == opid1('&') || op->id    == opid1('|') || op->id    == opid1('^')) &&
                        (olast->id == opid1('&') || olast->id == opid1('|') || olast->id == opid1('^')))
                    {
                        (void)!parsewarning(parser, WARN_PARENTHESIS, "suggesting parenthesis around bitwise operations");
                        warn_parenthesis = false;
                    }
                    else if ((op->id    == opid2('&','&') || op->id    == opid2('|','|')) &&
                             (olast->id == opid2('&','&') || olast->id == opid2('|','|')))
                    {
                        (void)!parsewarning(parser, WARN_PARENTHESIS, "suggesting parenthesis around logical operations");
                        warn_parenthesis = false;
                    }
                }
            }

            while (olast && (
                    (op->prec < olast->prec) ||
                    (op->assoc == ASSOC_LEFT && op->prec <= olast->prec) ) )
            {
                if (!parser_sy_apply_operator(parser, &sy))
                    goto onerr;
                if (sy.ops.size() && !sy.ops.back().isparen)
                    olast = &operators[sy.ops.back().etype-1];
                else
                    olast = nullptr;
            }

            if (op->id == opid1('(')) {
                if (wantop) {
                    size_t sycount = sy.out.size();
                    /* we expected an operator, this is the function-call operator */
                    sy.paren.push_back(PAREN_FUNC);
                    sy.ops.push_back(syparen(parser_ctx(parser), sycount-1));
                    sy.argc.push_back(0);
                } else {
                    sy.paren.push_back(PAREN_EXPR);
                    sy.ops.push_back(syparen(parser_ctx(parser), 0));
                }
                wantop = false;
            } else if (op->id == opid1('[')) {
                if (!wantop) {
                    parseerror(parser, "unexpected array subscript");
                    goto onerr;
                }
                sy.paren.push_back(PAREN_INDEX);
                /* push both the operator and the paren, this makes life easier */
                sy.ops.push_back(syop(parser_ctx(parser), op));
                sy.ops.push_back(syparen(parser_ctx(parser), 0));
                wantop = false;
            } else if (op->id == opid2('?',':')) {
                sy.ops.push_back(syop(parser_ctx(parser), op));
                sy.ops.push_back(syparen(parser_ctx(parser), 0));
                wantop = false;
                ++ternaries;
                sy.paren.push_back(PAREN_TERNARY1);
            } else if (op->id == opid2(':','?')) {
                if (sy.paren.empty()) {
                    parseerror(parser, "unexpected colon outside ternary expression (missing parenthesis?)");
                    goto onerr;
                }
                if (sy.paren.back() != PAREN_TERNARY1) {
                    parseerror(parser, "unexpected colon outside ternary expression (missing parenthesis?)");
                    goto onerr;
                }
                if (!parser_close_paren(parser, &sy))
                    goto onerr;
                sy.ops.push_back(syop(parser_ctx(parser), op));
                wantop = false;
                --ternaries;
            } else {
                sy.ops.push_back(syop(parser_ctx(parser), op));
                wantop = !!(op->flags & OP_SUFFIX);
            }
        }
        else if (parser->tok == ')') {
            while (sy.paren.size() && sy.paren.back() == PAREN_TERNARY2) {
                if (!parser_sy_apply_operator(parser, &sy))
                    goto onerr;
            }
            if (sy.paren.empty())
                break;
            if (wantop) {
                if (sy.paren.back() == PAREN_TERNARY1) {
                    parseerror(parser, "mismatched parentheses (closing paren in ternary expression?)");
                    goto onerr;
                }
                if (!parser_close_paren(parser, &sy))
                    goto onerr;
            } else {
                /* must be a function call without parameters */
                if (sy.paren.back() != PAREN_FUNC) {
                    parseerror(parser, "closing paren in invalid position");
                    goto onerr;
                }
                if (!parser_close_paren(parser, &sy))
                    goto onerr;
            }
            wantop = true;
        }
        else if (parser->tok == '(') {
            parseerror(parser, "internal error: '(' should be classified as operator");
            goto onerr;
        }
        else if (parser->tok == '[') {
            parseerror(parser, "internal error: '[' should be classified as operator");
            goto onerr;
        }
        else if (parser->tok == ']') {
            while (sy.paren.size() && sy.paren.back() == PAREN_TERNARY2) {
                if (!parser_sy_apply_operator(parser, &sy))
                    goto onerr;
            }
            if (sy.paren.empty())
                break;
            if (sy.paren.back() != PAREN_INDEX) {
                parseerror(parser, "mismatched parentheses, unexpected ']'");
                goto onerr;
            }
            if (!parser_close_paren(parser, &sy))
                goto onerr;
            wantop = true;
        }
        else if (!wantop) {
            if (!parse_sya_operand(parser, &sy, with_labels))
                goto onerr;
            wantop = true;
        }
        else {
            /* in this case we might want to allow constant string concatenation */
            bool concatenated = false;
            if (parser->tok == TOKEN_STRINGCONST && sy.out.size()) {
                ast_expression *lexpr = sy.out.back().out;
                if (ast_istype(lexpr, ast_value)) {
                    ast_value *last = (ast_value*)lexpr;
                    if (last->m_isimm == true && last->m_cvq == CV_CONST &&
                        last->m_hasvalue && last->m_vtype == TYPE_STRING)
                    {
                        char *newstr = nullptr;
                        util_asprintf(&newstr, "%s%s", last->m_constval.vstring, parser_tokval(parser));
                        sy.out.back().out = parser->m_fold.constgen_string(newstr, false);
                        mem_d(newstr);
                        concatenated = true;
                    }
                }
            }
            if (!concatenated) {
                parseerror(parser, "expected operator or end of statement");
                goto onerr;
            }
        }

        if (!parser_next(parser)) {
            goto onerr;
        }
        if (parser->tok == ';' ||
            ((sy.paren.empty() || (sy.paren.size() == 1 && sy.paren.back() == PAREN_TERNARY2)) &&
            (parser->tok == ']' || parser->tok == ')' || parser->tok == '}')))
        {
            break;
        }
    }

    while (sy.ops.size()) {
        if (!parser_sy_apply_operator(parser, &sy))
            goto onerr;
    }

    parser->lex->flags.noops = true;
    if (sy.out.size() != 1) {
        parseerror(parser, "expression expected");
        expr = nullptr;
    } else
        expr = sy.out[0].out;
    if (sy.paren.size()) {
        parseerror(parser, "internal error: sy.paren.size() = %zu", sy.paren.size());
        return nullptr;
    }
    return expr;

onerr:
    parser->lex->flags.noops = true;
    for (auto &it : sy.out)
        if (it.out) ast_unref(it.out);
    return nullptr;
}

static ast_expression* parse_expression(parser_t *parser, bool stopatcomma, bool with_labels)
{
    ast_expression *e = parse_expression_leave(parser, stopatcomma, false, with_labels);
    if (!e)
        return nullptr;
    if (parser->tok != ';') {
        parseerror(parser, "semicolon expected after expression");
        ast_unref(e);
        return nullptr;
    }
    if (!parser_next(parser)) {
        ast_unref(e);
        return nullptr;
    }
    return e;
}

static void parser_enterblock(parser_t *parser)
{
    parser->variables.push_back(util_htnew(PARSER_HT_SIZE));
    parser->_blocklocals.push_back(parser->_locals.size());
    parser->typedefs.push_back(util_htnew(TYPEDEF_HT_SIZE));
    parser->_blocktypedefs.push_back(parser->_typedefs.size());
    parser->_block_ctx.push_back(parser_ctx(parser));
}

static bool parser_leaveblock(parser_t *parser)
{
    bool   rv = true;
    size_t locals, typedefs;

    if (parser->variables.size() <= PARSER_HT_LOCALS) {
        parseerror(parser, "internal error: parser_leaveblock with no block");
        return false;
    }

    util_htdel(parser->variables.back());

    parser->variables.pop_back();
    if (!parser->_blocklocals.size()) {
        parseerror(parser, "internal error: parser_leaveblock with no block (2)");
        return false;
    }

    locals = parser->_blocklocals.back();
    parser->_blocklocals.pop_back();
    parser->_locals.resize(locals);

    typedefs = parser->_blocktypedefs.back();
    parser->_typedefs.resize(typedefs);
    util_htdel(parser->typedefs.back());
    parser->typedefs.pop_back();

    parser->_block_ctx.pop_back();

    return rv;
}

static void parser_addlocal(parser_t *parser, const char *name, ast_expression *e)
{
    parser->_locals.push_back(e);
    util_htset(parser->variables.back(), name, (void*)e);
}
static void parser_addlocal(parser_t *parser, const std::string &name, ast_expression *e) {
    return parser_addlocal(parser, name.c_str(), e);
}

static void parser_addglobal(parser_t *parser, const char *name, ast_expression *e)
{
    parser->globals.push_back(e);
    util_htset(parser->htglobals, name, e);
}
static void parser_addglobal(parser_t *parser, const std::string &name, ast_expression *e) {
    return parser_addglobal(parser, name.c_str(), e);
}

static ast_expression* process_condition(parser_t *parser, ast_expression *cond, bool *_ifnot)
{
    bool       ifnot = false;
    ast_unary *unary;
    ast_expression *prev;

    if (cond->m_vtype == TYPE_VOID || cond->m_vtype >= TYPE_VARIANT) {
        char ty[1024];
        ast_type_to_string(cond, ty, sizeof(ty));
        compile_error(cond->m_context, "invalid type for if() condition: %s", ty);
    }

    if (OPTS_FLAG(FALSE_EMPTY_STRINGS) && cond->m_vtype == TYPE_STRING)
    {
        prev = cond;
        cond = ast_unary::make(cond->m_context, INSTR_NOT_S, cond);
        if (!cond) {
            ast_unref(prev);
            parseerror(parser, "internal error: failed to process condition");
            return nullptr;
        }
        ifnot = !ifnot;
    }
    else if (OPTS_FLAG(CORRECT_LOGIC) && cond->m_vtype == TYPE_VECTOR)
    {
        /* vector types need to be cast to true booleans */
        ast_binary *bin = (ast_binary*)cond;
        if (!OPTS_FLAG(PERL_LOGIC) || !ast_istype(cond, ast_binary) || !(bin->m_op == INSTR_AND || bin->m_op == INSTR_OR))
        {
            /* in perl-logic, AND and OR take care of the -fcorrect-logic */
            prev = cond;
            cond = ast_unary::make(cond->m_context, INSTR_NOT_V, cond);
            if (!cond) {
                ast_unref(prev);
                parseerror(parser, "internal error: failed to process condition");
                return nullptr;
            }
            ifnot = !ifnot;
        }
    }

    unary = (ast_unary*)cond;
    /* ast_istype dereferences cond, should test here for safety */
    while (cond && ast_istype(cond, ast_unary) && unary->m_op == INSTR_NOT_F)
    {
        cond = unary->m_operand;
        unary->m_operand = nullptr;
        delete unary;
        ifnot = !ifnot;
        unary = (ast_unary*)cond;
    }

    if (!cond)
        parseerror(parser, "internal error: failed to process condition");

    if (ifnot) *_ifnot = !*_ifnot;
    return cond;
}

static bool parse_if(parser_t *parser, ast_block *block, ast_expression **out)
{
    ast_ifthen *ifthen;
    ast_expression *cond, *ontrue = nullptr, *onfalse = nullptr;
    bool ifnot = false;

    lex_ctx_t ctx = parser_ctx(parser);

    (void)block; /* not touching */

    /* skip the 'if', parse an optional 'not' and check for an opening paren */
    if (!parser_next(parser)) {
        parseerror(parser, "expected condition or 'not'");
        return false;
    }
    if (parser->tok == TOKEN_IDENT && !strcmp(parser_tokval(parser), "not")) {
        ifnot = true;
        if (!parser_next(parser)) {
            parseerror(parser, "expected condition in parenthesis");
            return false;
        }
    }
    if (parser->tok != '(') {
        parseerror(parser, "expected 'if' condition in parenthesis");
        return false;
    }
    /* parse into the expression */
    if (!parser_next(parser)) {
        parseerror(parser, "expected 'if' condition after opening paren");
        return false;
    }
    /* parse the condition */
    cond = parse_expression_leave(parser, false, true, false);
    if (!cond)
        return false;
    /* closing paren */
    if (parser->tok != ')') {
        parseerror(parser, "expected closing paren after 'if' condition");
        ast_unref(cond);
        return false;
    }
    /* parse into the 'then' branch */
    if (!parser_next(parser)) {
        parseerror(parser, "expected statement for on-true branch of 'if'");
        ast_unref(cond);
        return false;
    }
    if (!parse_statement_or_block(parser, block, &ontrue)) {
        ast_unref(cond);
        return false;
    }
    if (!ontrue)
        ontrue = new ast_block(parser_ctx(parser));
    /* check for an else */
    if (!strcmp(parser_tokval(parser), "else")) {
        /* parse into the 'else' branch */
        if (!parser_next(parser)) {
            parseerror(parser, "expected on-false branch after 'else'");
            delete ontrue;
            ast_unref(cond);
            return false;
        }
        if (!parse_statement_or_block(parser, block, &onfalse)) {
            delete ontrue;
            ast_unref(cond);
            return false;
        }
    }

    cond = process_condition(parser, cond, &ifnot);
    if (!cond) {
        if (ontrue)  delete ontrue;
        if (onfalse) delete onfalse;
        return false;
    }

    if (ifnot)
        ifthen = new ast_ifthen(ctx, cond, onfalse, ontrue);
    else
        ifthen = new ast_ifthen(ctx, cond, ontrue, onfalse);
    *out = ifthen;
    return true;
}

static bool parse_while_go(parser_t *parser, ast_block *block, ast_expression **out);
static bool parse_while(parser_t *parser, ast_block *block, ast_expression **out)
{
    bool rv;
    char *label = nullptr;

    /* skip the 'while' and get the body */
    if (!parser_next(parser)) {
        if (OPTS_FLAG(LOOP_LABELS))
            parseerror(parser, "expected loop label or 'while' condition in parenthesis");
        else
            parseerror(parser, "expected 'while' condition in parenthesis");
        return false;
    }

    if (parser->tok == ':') {
        if (!OPTS_FLAG(LOOP_LABELS))
            parseerror(parser, "labeled loops not activated, try using -floop-labels");
        if (!parser_next(parser) || parser->tok != TOKEN_IDENT) {
            parseerror(parser, "expected loop label");
            return false;
        }
        label = util_strdup(parser_tokval(parser));
        if (!parser_next(parser)) {
            mem_d(label);
            parseerror(parser, "expected 'while' condition in parenthesis");
            return false;
        }
    }

    if (parser->tok != '(') {
        parseerror(parser, "expected 'while' condition in parenthesis");
        return false;
    }

    parser->breaks.push_back(label);
    parser->continues.push_back(label);

    rv = parse_while_go(parser, block, out);
    if (label)
        mem_d(label);
    if (parser->breaks.back() != label || parser->continues.back() != label) {
        parseerror(parser, "internal error: label stack corrupted");
        rv = false;
        delete *out;
        *out = nullptr;
    }
    else {
        parser->breaks.pop_back();
        parser->continues.pop_back();
    }
    return rv;
}

static bool parse_while_go(parser_t *parser, ast_block *block, ast_expression **out)
{
    ast_loop *aloop;
    ast_expression *cond, *ontrue;

    bool ifnot = false;

    lex_ctx_t ctx = parser_ctx(parser);

    (void)block; /* not touching */

    /* parse into the expression */
    if (!parser_next(parser)) {
        parseerror(parser, "expected 'while' condition after opening paren");
        return false;
    }
    /* parse the condition */
    cond = parse_expression_leave(parser, false, true, false);
    if (!cond)
        return false;
    /* closing paren */
    if (parser->tok != ')') {
        parseerror(parser, "expected closing paren after 'while' condition");
        ast_unref(cond);
        return false;
    }
    /* parse into the 'then' branch */
    if (!parser_next(parser)) {
        parseerror(parser, "expected while-loop body");
        ast_unref(cond);
        return false;
    }
    if (!parse_statement_or_block(parser, block, &ontrue)) {
        ast_unref(cond);
        return false;
    }

    cond = process_condition(parser, cond, &ifnot);
    if (!cond) {
        ast_unref(ontrue);
        return false;
    }
    aloop = new ast_loop(ctx, nullptr, cond, ifnot, nullptr, false, nullptr, ontrue);
    *out = aloop;
    return true;
}

static bool parse_dowhile_go(parser_t *parser, ast_block *block, ast_expression **out);
static bool parse_dowhile(parser_t *parser, ast_block *block, ast_expression **out)
{
    bool rv;
    char *label = nullptr;

    /* skip the 'do' and get the body */
    if (!parser_next(parser)) {
        if (OPTS_FLAG(LOOP_LABELS))
            parseerror(parser, "expected loop label or body");
        else
            parseerror(parser, "expected loop body");
        return false;
    }

    if (parser->tok == ':') {
        if (!OPTS_FLAG(LOOP_LABELS))
            parseerror(parser, "labeled loops not activated, try using -floop-labels");
        if (!parser_next(parser) || parser->tok != TOKEN_IDENT) {
            parseerror(parser, "expected loop label");
            return false;
        }
        label = util_strdup(parser_tokval(parser));
        if (!parser_next(parser)) {
            mem_d(label);
            parseerror(parser, "expected loop body");
            return false;
        }
    }

    parser->breaks.push_back(label);
    parser->continues.push_back(label);

    rv = parse_dowhile_go(parser, block, out);
    if (label)
        mem_d(label);
    if (parser->breaks.back() != label || parser->continues.back() != label) {
        parseerror(parser, "internal error: label stack corrupted");
        rv = false;
        delete *out;
        *out = nullptr;
    }
    else {
        parser->breaks.pop_back();
        parser->continues.pop_back();
    }
    return rv;
}

static bool parse_dowhile_go(parser_t *parser, ast_block *block, ast_expression **out)
{
    ast_loop *aloop;
    ast_expression *cond, *ontrue;

    bool ifnot = false;

    lex_ctx_t ctx = parser_ctx(parser);

    (void)block; /* not touching */

    if (!parse_statement_or_block(parser, block, &ontrue))
        return false;

    /* expect the "while" */
    if (parser->tok != TOKEN_KEYWORD ||
        strcmp(parser_tokval(parser), "while"))
    {
        parseerror(parser, "expected 'while' and condition");
        delete ontrue;
        return false;
    }

    /* skip the 'while' and check for opening paren */
    if (!parser_next(parser) || parser->tok != '(') {
        parseerror(parser, "expected 'while' condition in parenthesis");
        delete ontrue;
        return false;
    }
    /* parse into the expression */
    if (!parser_next(parser)) {
        parseerror(parser, "expected 'while' condition after opening paren");
        delete ontrue;
        return false;
    }
    /* parse the condition */
    cond = parse_expression_leave(parser, false, true, false);
    if (!cond)
        return false;
    /* closing paren */
    if (parser->tok != ')') {
        parseerror(parser, "expected closing paren after 'while' condition");
        delete ontrue;
        ast_unref(cond);
        return false;
    }
    /* parse on */
    if (!parser_next(parser) || parser->tok != ';') {
        parseerror(parser, "expected semicolon after condition");
        delete ontrue;
        ast_unref(cond);
        return false;
    }

    if (!parser_next(parser)) {
        parseerror(parser, "parse error");
        delete ontrue;
        ast_unref(cond);
        return false;
    }

    cond = process_condition(parser, cond, &ifnot);
    if (!cond) {
        delete ontrue;
        return false;
    }
    aloop = new ast_loop(ctx, nullptr, nullptr, false, cond, ifnot, nullptr, ontrue);
    *out = aloop;
    return true;
}

static bool parse_for_go(parser_t *parser, ast_block *block, ast_expression **out);
static bool parse_for(parser_t *parser, ast_block *block, ast_expression **out)
{
    bool rv;
    char *label = nullptr;

    /* skip the 'for' and check for opening paren */
    if (!parser_next(parser)) {
        if (OPTS_FLAG(LOOP_LABELS))
            parseerror(parser, "expected loop label or 'for' expressions in parenthesis");
        else
            parseerror(parser, "expected 'for' expressions in parenthesis");
        return false;
    }

    if (parser->tok == ':') {
        if (!OPTS_FLAG(LOOP_LABELS))
            parseerror(parser, "labeled loops not activated, try using -floop-labels");
        if (!parser_next(parser) || parser->tok != TOKEN_IDENT) {
            parseerror(parser, "expected loop label");
            return false;
        }
        label = util_strdup(parser_tokval(parser));
        if (!parser_next(parser)) {
            mem_d(label);
            parseerror(parser, "expected 'for' expressions in parenthesis");
            return false;
        }
    }

    if (parser->tok != '(') {
        parseerror(parser, "expected 'for' expressions in parenthesis");
        return false;
    }

    parser->breaks.push_back(label);
    parser->continues.push_back(label);

    rv = parse_for_go(parser, block, out);
    if (label)
        mem_d(label);
    if (parser->breaks.back() != label || parser->continues.back() != label) {
        parseerror(parser, "internal error: label stack corrupted");
        rv = false;
        delete *out;
        *out = nullptr;
    }
    else {
        parser->breaks.pop_back();
        parser->continues.pop_back();
    }
    return rv;
}
static bool parse_for_go(parser_t *parser, ast_block *block, ast_expression **out)
{
    ast_loop       *aloop;
    ast_expression *initexpr, *cond, *increment, *ontrue;
    ast_value      *typevar;

    bool ifnot  = false;

    lex_ctx_t ctx = parser_ctx(parser);

    parser_enterblock(parser);

    initexpr  = nullptr;
    cond      = nullptr;
    increment = nullptr;
    ontrue    = nullptr;

    /* parse into the expression */
    if (!parser_next(parser)) {
        parseerror(parser, "expected 'for' initializer after opening paren");
        goto onerr;
    }

    typevar = nullptr;
    if (parser->tok == TOKEN_IDENT)
        typevar = parser_find_typedef(parser, parser_tokval(parser), 0);

    if (typevar || parser->tok == TOKEN_TYPENAME) {
        if (!parse_variable(parser, block, true, CV_VAR, typevar, false, false, 0, nullptr))
            goto onerr;
    }
    else if (parser->tok != ';')
    {
        initexpr = parse_expression_leave(parser, false, false, false);
        if (!initexpr)
            goto onerr;
        /* move on to condition */
        if (parser->tok != ';') {
            parseerror(parser, "expected semicolon after for-loop initializer");
            goto onerr;
        }
        if (!parser_next(parser)) {
            parseerror(parser, "expected for-loop condition");
            goto onerr;
        }
    } else if (!parser_next(parser)) {
        parseerror(parser, "expected for-loop condition");
        goto onerr;
    }

    /* parse the condition */
    if (parser->tok != ';') {
        cond = parse_expression_leave(parser, false, true, false);
        if (!cond)
            goto onerr;
    }
    /* move on to incrementor */
    if (parser->tok != ';') {
        parseerror(parser, "expected semicolon after for-loop initializer");
        goto onerr;
    }
    if (!parser_next(parser)) {
        parseerror(parser, "expected for-loop condition");
        goto onerr;
    }

    /* parse the incrementor */
    if (parser->tok != ')') {
        lex_ctx_t condctx = parser_ctx(parser);
        increment = parse_expression_leave(parser, false, false, false);
        if (!increment)
            goto onerr;
        if (!increment->m_side_effects) {
            if (compile_warning(condctx, WARN_EFFECTLESS_STATEMENT, "statement has no effect"))
                goto onerr;
        }
    }

    /* closing paren */
    if (parser->tok != ')') {
        parseerror(parser, "expected closing paren after 'for-loop' incrementor");
        goto onerr;
    }
    /* parse into the 'then' branch */
    if (!parser_next(parser)) {
        parseerror(parser, "expected for-loop body");
        goto onerr;
    }
    if (!parse_statement_or_block(parser, block, &ontrue))
        goto onerr;

    if (cond) {
        cond = process_condition(parser, cond, &ifnot);
        if (!cond)
            goto onerr;
    }
    aloop = new ast_loop(ctx, initexpr, cond, ifnot, nullptr, false, increment, ontrue);
    *out = aloop;

    if (!parser_leaveblock(parser)) {
        delete aloop;
        return false;
    }
    return true;
onerr:
    if (initexpr)  ast_unref(initexpr);
    if (cond)      ast_unref(cond);
    if (increment) ast_unref(increment);
    (void)!parser_leaveblock(parser);
    return false;
}

static bool parse_return(parser_t *parser, ast_block *block, ast_expression **out)
{
    ast_expression *exp      = nullptr;
    ast_expression *var      = nullptr;
    ast_return     *ret      = nullptr;
    ast_value      *retval   = parser->function->m_return_value;
    ast_value      *expected = parser->function->m_function_type;

    lex_ctx_t ctx = parser_ctx(parser);

    (void)block; /* not touching */

    if (!parser_next(parser)) {
        parseerror(parser, "expected return expression");
        return false;
    }

    /* return assignments */
    if (parser->tok == '=') {
        if (!OPTS_FLAG(RETURN_ASSIGNMENTS)) {
            parseerror(parser, "return assignments not activated, try using -freturn-assigments");
            return false;
        }

        if (type_store_instr[expected->m_next->m_vtype] == VINSTR_END) {
            char ty1[1024];
            ast_type_to_string(expected->m_next, ty1, sizeof(ty1));
            parseerror(parser, "invalid return type: `%s'", ty1);
            return false;
        }

        if (!parser_next(parser)) {
            parseerror(parser, "expected return assignment expression");
            return false;
        }

        if (!(exp = parse_expression_leave(parser, false, false, false)))
            return false;

        /* prepare the return value */
        if (!retval) {
            retval = new ast_value(ctx, "#LOCAL_RETURN", TYPE_VOID);
            retval->adoptType(*expected->m_next);
            parser->function->m_return_value = retval;
            parser->function->m_return_value->m_flags |= AST_FLAG_NOREF;
        }

        if (!exp->compareType(*retval)) {
            char ty1[1024], ty2[1024];
            ast_type_to_string(exp, ty1, sizeof(ty1));
            ast_type_to_string(retval, ty2, sizeof(ty2));
            parseerror(parser, "invalid type for return value: `%s', expected `%s'", ty1, ty2);
        }

        /* store to 'return' local variable */
        var = new ast_store(
            ctx,
            type_store_instr[expected->m_next->m_vtype],
            retval, exp);

        if (!var) {
            ast_unref(exp);
            return false;
        }

        if (parser->tok != ';')
            parseerror(parser, "missing semicolon after return assignment");
        else if (!parser_next(parser))
            parseerror(parser, "parse error after return assignment");

        *out = var;
        return true;
    }

    if (parser->tok != ';') {
        exp = parse_expression(parser, false, false);
        if (!exp)
            return false;

        if (exp->m_vtype != TYPE_NIL &&
            exp->m_vtype != (expected)->m_next->m_vtype)
        {
            parseerror(parser, "return with invalid expression");
        }

        ret = new ast_return(ctx, exp);
        if (!ret) {
            ast_unref(exp);
            return false;
        }
    } else {
        if (!parser_next(parser))
            parseerror(parser, "parse error");

        if (!retval && expected->m_next->m_vtype != TYPE_VOID)
        {
            (void)!parsewarning(parser, WARN_MISSING_RETURN_VALUES, "return without value");
        }
        ret = new ast_return(ctx, retval);
    }
    *out = ret;
    return true;
}

static bool parse_break_continue(parser_t *parser, ast_block *block, ast_expression **out, bool is_continue)
{
    size_t i;
    unsigned int levels = 0;
    lex_ctx_t ctx = parser_ctx(parser);
    auto &loops = (is_continue ? parser->continues : parser->breaks);

    (void)block; /* not touching */
    if (!parser_next(parser)) {
        parseerror(parser, "expected semicolon or loop label");
        return false;
    }

    if (loops.empty()) {
        if (is_continue)
            parseerror(parser, "`continue` can only be used inside loops");
        else
            parseerror(parser, "`break` can only be used inside loops or switches");
    }

    if (parser->tok == TOKEN_IDENT) {
        if (!OPTS_FLAG(LOOP_LABELS))
            parseerror(parser, "labeled loops not activated, try using -floop-labels");
        i = loops.size();
        while (i--) {
            if (loops[i] && !strcmp(loops[i], parser_tokval(parser)))
                break;
            if (!i) {
                parseerror(parser, "no such loop to %s: `%s`",
                           (is_continue ? "continue" : "break out of"),
                           parser_tokval(parser));
                return false;
            }
            ++levels;
        }
        if (!parser_next(parser)) {
            parseerror(parser, "expected semicolon");
            return false;
        }
    }

    if (parser->tok != ';') {
        parseerror(parser, "expected semicolon");
        return false;
    }

    if (!parser_next(parser))
        parseerror(parser, "parse error");

    *out = new ast_breakcont(ctx, is_continue, levels);
    return true;
}

/* returns true when it was a variable qualifier, false otherwise!
 * on error, cvq is set to CV_WRONG
 */
struct attribute_t {
    const char *name;
    size_t      flag;
};

static bool parse_qualifiers(parser_t *parser, bool with_local, int *cvq, bool *noref, bool *is_static, uint32_t *_flags, char **message)
{
    bool had_const    = false;
    bool had_var      = false;
    bool had_noref    = false;
    bool had_attrib   = false;
    bool had_static   = false;
    uint32_t flags    = 0;

    static attribute_t attributes[] = {
        { "noreturn",   AST_FLAG_NORETURN   },
        { "inline",     AST_FLAG_INLINE     },
        { "eraseable",  AST_FLAG_ERASEABLE  },
        { "noerase",    AST_FLAG_NOERASE    },
        { "accumulate", AST_FLAG_ACCUMULATE },
        { "last",       AST_FLAG_FINAL_DECL }
    };

   *cvq = CV_NONE;

    for (;;) {
        size_t i;
        if (parser->tok == TOKEN_ATTRIBUTE_OPEN) {
            had_attrib = true;
            /* parse an attribute */
            if (!parser_next(parser)) {
                parseerror(parser, "expected attribute after `[[`");
                *cvq = CV_WRONG;
                return false;
            }

            for (i = 0; i < GMQCC_ARRAY_COUNT(attributes); i++) {
                if (!strcmp(parser_tokval(parser), attributes[i].name)) {
                    flags |= attributes[i].flag;
                    if (!parser_next(parser) || parser->tok != TOKEN_ATTRIBUTE_CLOSE) {
                        parseerror(parser, "`%s` attribute has no parameters, expected `]]`",
                            attributes[i].name);
                        *cvq = CV_WRONG;
                        return false;
                    }
                    break;
                }
            }

            if (i != GMQCC_ARRAY_COUNT(attributes))
                goto leave;

            if (!strcmp(parser_tokval(parser), "noref")) {
                had_noref = true;
                if (!parser_next(parser) || parser->tok != TOKEN_ATTRIBUTE_CLOSE) {
                    parseerror(parser, "`noref` attribute has no parameters, expected `]]`");
                    *cvq = CV_WRONG;
                    return false;
                }
            }
            else if (!strcmp(parser_tokval(parser), "alias") && !(flags & AST_FLAG_ALIAS)) {
                flags   |= AST_FLAG_ALIAS;
                *message = nullptr;

                if (!parser_next(parser)) {
                    parseerror(parser, "parse error in attribute");
                    goto argerr;
                }

                if (parser->tok == '(') {
                    if (!parser_next(parser) || parser->tok != TOKEN_STRINGCONST) {
                        parseerror(parser, "`alias` attribute missing parameter");
                        goto argerr;
                    }

                    *message = util_strdup(parser_tokval(parser));

                    if (!parser_next(parser)) {
                        parseerror(parser, "parse error in attribute");
                        goto argerr;
                    }

                    if (parser->tok != ')') {
                        parseerror(parser, "`alias` attribute expected `)` after parameter");
                        goto argerr;
                    }

                    if (!parser_next(parser)) {
                        parseerror(parser, "parse error in attribute");
                        goto argerr;
                    }
                }

                if (parser->tok != TOKEN_ATTRIBUTE_CLOSE) {
                    parseerror(parser, "`alias` attribute expected `]]`");
                    goto argerr;
                }
            }
            else if (!strcmp(parser_tokval(parser), "deprecated") && !(flags & AST_FLAG_DEPRECATED)) {
                flags   |= AST_FLAG_DEPRECATED;
                *message = nullptr;

                if (!parser_next(parser)) {
                    parseerror(parser, "parse error in attribute");
                    goto argerr;
                }

                if (parser->tok == '(') {
                    if (!parser_next(parser) || parser->tok != TOKEN_STRINGCONST) {
                        parseerror(parser, "`deprecated` attribute missing parameter");
                        goto argerr;
                    }

                    *message = util_strdup(parser_tokval(parser));

                    if (!parser_next(parser)) {
                        parseerror(parser, "parse error in attribute");
                        goto argerr;
                    }

                    if(parser->tok != ')') {
                        parseerror(parser, "`deprecated` attribute expected `)` after parameter");
                        goto argerr;
                    }

                    if (!parser_next(parser)) {
                        parseerror(parser, "parse error in attribute");
                        goto argerr;
                    }
                }
                /* no message */
                if (parser->tok != TOKEN_ATTRIBUTE_CLOSE) {
                    parseerror(parser, "`deprecated` attribute expected `]]`");

                    argerr: /* ugly */
                    if (*message) mem_d(*message);
                    *message = nullptr;
                    *cvq     = CV_WRONG;
                    return false;
                }
            }
            else if (!strcmp(parser_tokval(parser), "coverage") && !(flags & AST_FLAG_COVERAGE)) {
                flags |= AST_FLAG_COVERAGE;
                if (!parser_next(parser)) {
                    error_in_coverage:
                    parseerror(parser, "parse error in coverage attribute");
                    *cvq = CV_WRONG;
                    return false;
                }
                if (parser->tok == '(') {
                    if (!parser_next(parser)) {
                        bad_coverage_arg:
                        parseerror(parser, "invalid parameter for coverage() attribute\n"
                                           "valid are: block");
                        *cvq = CV_WRONG;
                        return false;
                    }
                    if (parser->tok != ')') {
                        do {
                            if (parser->tok != TOKEN_IDENT)
                                goto bad_coverage_arg;
                            if (!strcmp(parser_tokval(parser), "block"))
                                flags |= AST_FLAG_BLOCK_COVERAGE;
                            else if (!strcmp(parser_tokval(parser), "none"))
                                flags &= ~(AST_FLAG_COVERAGE_MASK);
                            else
                                goto bad_coverage_arg;
                            if (!parser_next(parser))
                                goto error_in_coverage;
                            if (parser->tok == ',') {
                                if (!parser_next(parser))
                                    goto error_in_coverage;
                            }
                        } while (parser->tok != ')');
                    }
                    if (parser->tok != ')' || !parser_next(parser))
                        goto error_in_coverage;
                } else {
                    /* without parameter [[coverage]] equals [[coverage(block)]] */
                    flags |= AST_FLAG_BLOCK_COVERAGE;
                }
            }
            else
            {
                /* Skip tokens until we hit a ]] */
                (void)!parsewarning(parser, WARN_UNKNOWN_ATTRIBUTE, "unknown attribute starting with `%s`", parser_tokval(parser));
                while (parser->tok != TOKEN_ATTRIBUTE_CLOSE) {
                    if (!parser_next(parser)) {
                        parseerror(parser, "error inside attribute");
                        *cvq = CV_WRONG;
                        return false;
                    }
                }
            }
        }
        else if (with_local && !strcmp(parser_tokval(parser), "static"))
            had_static = true;
        else if (!strcmp(parser_tokval(parser), "const"))
            had_const = true;
        else if (!strcmp(parser_tokval(parser), "var"))
            had_var = true;
        else if (with_local && !strcmp(parser_tokval(parser), "local"))
            had_var = true;
        else if (!strcmp(parser_tokval(parser), "noref"))
            had_noref = true;
        else if (!had_const && !had_var && !had_noref && !had_attrib && !had_static && !flags) {
            return false;
        }
        else
            break;

        leave:
        if (!parser_next(parser))
            goto onerr;
    }
    if (had_const)
        *cvq = CV_CONST;
    else if (had_var)
        *cvq = CV_VAR;
    else
        *cvq = CV_NONE;
    *noref     = had_noref;
    *is_static = had_static;
    *_flags    = flags;
    return true;
onerr:
    parseerror(parser, "parse error after variable qualifier");
    *cvq = CV_WRONG;
    return true;
}

static bool parse_switch_go(parser_t *parser, ast_block *block, ast_expression **out);
static bool parse_switch(parser_t *parser, ast_block *block, ast_expression **out)
{
    bool rv;
    char *label = nullptr;

    /* skip the 'while' and get the body */
    if (!parser_next(parser)) {
        if (OPTS_FLAG(LOOP_LABELS))
            parseerror(parser, "expected loop label or 'switch' operand in parenthesis");
        else
            parseerror(parser, "expected 'switch' operand in parenthesis");
        return false;
    }

    if (parser->tok == ':') {
        if (!OPTS_FLAG(LOOP_LABELS))
            parseerror(parser, "labeled loops not activated, try using -floop-labels");
        if (!parser_next(parser) || parser->tok != TOKEN_IDENT) {
            parseerror(parser, "expected loop label");
            return false;
        }
        label = util_strdup(parser_tokval(parser));
        if (!parser_next(parser)) {
            mem_d(label);
            parseerror(parser, "expected 'switch' operand in parenthesis");
            return false;
        }
    }

    if (parser->tok != '(') {
        parseerror(parser, "expected 'switch' operand in parenthesis");
        return false;
    }

    parser->breaks.push_back(label);

    rv = parse_switch_go(parser, block, out);
    if (label)
        mem_d(label);
    if (parser->breaks.back() != label) {
        parseerror(parser, "internal error: label stack corrupted");
        rv = false;
        delete *out;
        *out = nullptr;
    }
    else {
        parser->breaks.pop_back();
    }
    return rv;
}

static bool parse_switch_go(parser_t *parser, ast_block *block, ast_expression **out)
{
    ast_expression *operand;
    ast_value      *opval;
    ast_value      *typevar;
    ast_switch     *switchnode;
    ast_switch_case swcase;

    int  cvq;
    bool noref, is_static;
    uint32_t qflags = 0;

    lex_ctx_t ctx = parser_ctx(parser);

    (void)block; /* not touching */
    (void)opval;

    /* parse into the expression */
    if (!parser_next(parser)) {
        parseerror(parser, "expected switch operand");
        return false;
    }
    /* parse the operand */
    operand = parse_expression_leave(parser, false, false, false);
    if (!operand)
        return false;

    switchnode = new ast_switch(ctx, operand);

    /* closing paren */
    if (parser->tok != ')') {
        delete switchnode;
        parseerror(parser, "expected closing paren after 'switch' operand");
        return false;
    }

    /* parse over the opening paren */
    if (!parser_next(parser) || parser->tok != '{') {
        delete switchnode;
        parseerror(parser, "expected list of cases");
        return false;
    }

    if (!parser_next(parser)) {
        delete switchnode;
        parseerror(parser, "expected 'case' or 'default'");
        return false;
    }

    /* new block; allow some variables to be declared here */
    parser_enterblock(parser);
    while (true) {
        typevar = nullptr;
        if (parser->tok == TOKEN_IDENT)
            typevar = parser_find_typedef(parser, parser_tokval(parser), 0);
        if (typevar || parser->tok == TOKEN_TYPENAME) {
            if (!parse_variable(parser, block, true, CV_NONE, typevar, false, false, 0, nullptr)) {
                delete switchnode;
                return false;
            }
            continue;
        }
        if (parse_qualifiers(parser, true, &cvq, &noref, &is_static, &qflags, nullptr))
        {
            if (cvq == CV_WRONG) {
                delete switchnode;
                return false;
            }
            if (!parse_variable(parser, block, true, cvq, nullptr, noref, is_static, qflags, nullptr)) {
                delete switchnode;
                return false;
            }
            continue;
        }
        break;
    }

    /* case list! */
    while (parser->tok != '}') {
        ast_block *caseblock;

        if (!strcmp(parser_tokval(parser), "case")) {
            if (!parser_next(parser)) {
                delete switchnode;
                parseerror(parser, "expected expression for case");
                return false;
            }
            swcase.m_value = parse_expression_leave(parser, false, false, false);

            if (!operand->compareType(*swcase.m_value)) {
                char ty1[1024];
                char ty2[1024];

                ast_type_to_string(swcase.m_value, ty1, sizeof ty1);
                ast_type_to_string(operand, ty2, sizeof ty2);

                auto fnLiteral = [](ast_expression *expression) -> char* {
                    if (!ast_istype(expression, ast_value))
                        return nullptr;
                    ast_value *value = (ast_value *)expression;
                    if (!value->m_hasvalue)
                        return nullptr;
                    char *string = nullptr;
                    basic_value_t *constval = &value->m_constval;
                    switch (value->m_vtype)
                    {
                    case TYPE_FLOAT:
                        util_asprintf(&string, "%.2f", constval->vfloat);
                        return string;
                    case TYPE_VECTOR:
                        util_asprintf(&string, "'%.2f %.2f %.2f'",
                            constval->vvec.x,
                            constval->vvec.y,
                            constval->vvec.z);
                        return string;
                    case TYPE_STRING:
                        util_asprintf(&string, "\"%s\"", constval->vstring);
                        return string;
                    default:
                        break;
                    }
                    return nullptr;
                };

                char *literal = fnLiteral(swcase.m_value);
                if (literal)
                    compile_error(parser_ctx(parser), "incompatible type `%s` for switch case `%s` expected `%s`", ty1, literal, ty2);
                else
                    compile_error(parser_ctx(parser), "incompatible type `%s` for switch case expected `%s`", ty1, ty2);
                mem_d(literal);
                delete switchnode;
                return false;
            }

            if (!swcase.m_value) {
                delete switchnode;
                parseerror(parser, "expected expression for case");
                return false;
            }
            if (!OPTS_FLAG(RELAXED_SWITCH)) {
                if (!ast_istype(swcase.m_value, ast_value)) { /* || ((ast_value*)swcase.m_value)->m_cvq != CV_CONST) { */
                    delete switchnode;
                    parseerror(parser, "case on non-constant values need to be explicitly enabled via -frelaxed-switch");
                    ast_unref(operand);
                    return false;
                }
            }
        }
        else if (!strcmp(parser_tokval(parser), "default")) {
            swcase.m_value = nullptr;
            if (!parser_next(parser)) {
                delete switchnode;
                parseerror(parser, "expected colon");
                return false;
            }
        }
        else {
            delete switchnode;
            parseerror(parser, "expected 'case' or 'default'");
            return false;
        }

        /* Now the colon and body */
        if (parser->tok != ':') {
            if (swcase.m_value) ast_unref(swcase.m_value);
            delete switchnode;
            parseerror(parser, "expected colon");
            return false;
        }

        if (!parser_next(parser)) {
            if (swcase.m_value) ast_unref(swcase.m_value);
            delete switchnode;
            parseerror(parser, "expected statements or case");
            return false;
        }
        caseblock = new ast_block(parser_ctx(parser));
        if (!caseblock) {
            if (swcase.m_value) ast_unref(swcase.m_value);
            delete switchnode;
            return false;
        }
        swcase.m_code = caseblock;
        switchnode->m_cases.push_back(swcase);
        while (true) {
            ast_expression *expr;
            if (parser->tok == '}')
                break;
            if (parser->tok == TOKEN_KEYWORD) {
                if (!strcmp(parser_tokval(parser), "case") ||
                    !strcmp(parser_tokval(parser), "default"))
                {
                    break;
                }
            }
            if (!parse_statement(parser, caseblock, &expr, true)) {
                delete switchnode;
                return false;
            }
            if (!expr)
                continue;
            if (!caseblock->addExpr(expr)) {
                delete switchnode;
                return false;
            }
        }
    }

    parser_leaveblock(parser);

    /* closing paren */
    if (parser->tok != '}') {
        delete switchnode;
        parseerror(parser, "expected closing paren of case list");
        return false;
    }
    if (!parser_next(parser)) {
        delete switchnode;
        parseerror(parser, "parse error after switch");
        return false;
    }
    *out = switchnode;
    return true;
}

/* parse computed goto sides */
static ast_expression *parse_goto_computed(parser_t *parser, ast_expression **side) {
    ast_expression *on_true;
    ast_expression *on_false;
    ast_expression *cond;

    if (!*side)
        return nullptr;

    if (ast_istype(*side, ast_ternary)) {
        ast_ternary *tern = (ast_ternary*)*side;
        on_true  = parse_goto_computed(parser, &tern->m_on_true);
        on_false = parse_goto_computed(parser, &tern->m_on_false);

        if (!on_true || !on_false) {
            parseerror(parser, "expected label or expression in ternary");
            if (on_true) ast_unref(on_true);
            if (on_false) ast_unref(on_false);
            return nullptr;
        }

        cond = tern->m_cond;
        tern->m_cond = nullptr;
        delete tern;
        *side = nullptr;
        return new ast_ifthen(parser_ctx(parser), cond, on_true, on_false);
    } else if (ast_istype(*side, ast_label)) {
        ast_goto *gt = new ast_goto(parser_ctx(parser), ((ast_label*)*side)->m_name);
        gt->setLabel(reinterpret_cast<ast_label*>(*side));
        *side = nullptr;
        return gt;
    }
    return nullptr;
}

static bool parse_goto(parser_t *parser, ast_expression **out)
{
    ast_goto       *gt = nullptr;
    ast_expression *lbl;

    if (!parser_next(parser))
        return false;

    if (parser->tok != TOKEN_IDENT) {
        ast_expression *expression;

        /* could be an expression i.e computed goto :-) */
        if (parser->tok != '(') {
            parseerror(parser, "expected label name after `goto`");
            return false;
        }

        /* failed to parse expression for goto */
        if (!(expression = parse_expression(parser, false, true)) ||
            !(*out = parse_goto_computed(parser, &expression))) {
            parseerror(parser, "invalid goto expression");
            if(expression)
                ast_unref(expression);
            return false;
        }

        return true;
    }

    /* not computed goto */
    gt = new ast_goto(parser_ctx(parser), parser_tokval(parser));
    lbl = parser_find_label(parser, gt->m_name);
    if (lbl) {
        if (!ast_istype(lbl, ast_label)) {
            parseerror(parser, "internal error: label is not an ast_label");
            delete gt;
            return false;
        }
        gt->setLabel(reinterpret_cast<ast_label*>(lbl));
    }
    else
        parser->gotos.push_back(gt);

    if (!parser_next(parser) || parser->tok != ';') {
        parseerror(parser, "semicolon expected after goto label");
        return false;
    }
    if (!parser_next(parser)) {
        parseerror(parser, "parse error after goto");
        return false;
    }

    *out = gt;
    return true;
}

static bool parse_skipwhite(parser_t *parser)
{
    do {
        if (!parser_next(parser))
            return false;
    } while (parser->tok == TOKEN_WHITE && parser->tok < TOKEN_ERROR);
    return parser->tok < TOKEN_ERROR;
}

static bool parse_eol(parser_t *parser)
{
    if (!parse_skipwhite(parser))
        return false;
    return parser->tok == TOKEN_EOL;
}

static bool parse_pragma_do(parser_t *parser)
{
    if (!parser_next(parser) ||
        parser->tok != TOKEN_IDENT ||
        strcmp(parser_tokval(parser), "pragma"))
    {
        parseerror(parser, "expected `pragma` keyword after `#`, got `%s`", parser_tokval(parser));
        return false;
    }
    if (!parse_skipwhite(parser) || parser->tok != TOKEN_IDENT) {
        parseerror(parser, "expected pragma, got `%s`", parser_tokval(parser));
        return false;
    }

    if (!strcmp(parser_tokval(parser), "noref")) {
        if (!parse_skipwhite(parser) || parser->tok != TOKEN_INTCONST) {
            parseerror(parser, "`noref` pragma requires an argument: 0 or 1");
            return false;
        }
        parser->noref = !!parser_token(parser)->constval.i;
        if (!parse_eol(parser)) {
            parseerror(parser, "parse error after `noref` pragma");
            return false;
        }
    }
    else
    {
        (void)!parsewarning(parser, WARN_UNKNOWN_PRAGMAS, "ignoring #pragma %s", parser_tokval(parser));

        /* skip to eol */
        while (!parse_eol(parser)) {
            parser_next(parser);
        }

        return true;
    }

    return true;
}

static bool parse_pragma(parser_t *parser)
{
    bool rv;
    parser->lex->flags.preprocessing = true;
    parser->lex->flags.mergelines = true;
    rv = parse_pragma_do(parser);
    if (parser->tok != TOKEN_EOL) {
        parseerror(parser, "junk after pragma");
        rv = false;
    }
    parser->lex->flags.preprocessing = false;
    parser->lex->flags.mergelines = false;
    if (!parser_next(parser)) {
        parseerror(parser, "parse error after pragma");
        rv = false;
    }
    return rv;
}

static bool parse_statement(parser_t *parser, ast_block *block, ast_expression **out, bool allow_cases)
{
    bool       noref, is_static;
    int        cvq     = CV_NONE;
    uint32_t   qflags  = 0;
    ast_value *typevar = nullptr;
    char      *vstring = nullptr;

    *out = nullptr;

    if (parser->tok == TOKEN_IDENT)
        typevar = parser_find_typedef(parser, parser_tokval(parser), 0);

    if (typevar || parser->tok == TOKEN_TYPENAME || parser->tok == '.' || parser->tok == TOKEN_DOTS)
    {
        /* local variable */
        if (!block) {
            parseerror(parser, "cannot declare a variable from here");
            return false;
        }
        if (OPTS_OPTION_U32(OPTION_STANDARD) == COMPILER_QCC) {
            if (parsewarning(parser, WARN_EXTENSIONS, "missing 'local' keyword when declaring a local variable"))
                return false;
        }
        if (!parse_variable(parser, block, false, CV_NONE, typevar, false, false, 0, nullptr))
            return false;
        return true;
    }
    else if (parse_qualifiers(parser, !!block, &cvq, &noref, &is_static, &qflags, &vstring))
    {
        if (cvq == CV_WRONG)
            return false;
        return parse_variable(parser, block, false, cvq, nullptr, noref, is_static, qflags, vstring);
    }
    else if (parser->tok == TOKEN_KEYWORD)
    {
        if (!strcmp(parser_tokval(parser), "__builtin_debug_printtype"))
        {
            char ty[1024];
            ast_value *tdef;

            if (!parser_next(parser)) {
                parseerror(parser, "parse error after __builtin_debug_printtype");
                return false;
            }

            if (parser->tok == TOKEN_IDENT && (tdef = parser_find_typedef(parser, parser_tokval(parser), 0)))
            {
                ast_type_to_string(tdef, ty, sizeof(ty));
                con_out("__builtin_debug_printtype: `%s`=`%s`\n", tdef->m_name.c_str(), ty);
                if (!parser_next(parser)) {
                    parseerror(parser, "parse error after __builtin_debug_printtype typename argument");
                    return false;
                }
            }
            else
            {
                if (!parse_statement(parser, block, out, allow_cases))
                    return false;
                if (!*out)
                    con_out("__builtin_debug_printtype: got no output node\n");
                else
                {
                    ast_type_to_string(*out, ty, sizeof(ty));
                    con_out("__builtin_debug_printtype: `%s`\n", ty);
                }
            }
            return true;
        }
        else if (!strcmp(parser_tokval(parser), "return"))
        {
            return parse_return(parser, block, out);
        }
        else if (!strcmp(parser_tokval(parser), "if"))
        {
            return parse_if(parser, block, out);
        }
        else if (!strcmp(parser_tokval(parser), "while"))
        {
            return parse_while(parser, block, out);
        }
        else if (!strcmp(parser_tokval(parser), "do"))
        {
            return parse_dowhile(parser, block, out);
        }
        else if (!strcmp(parser_tokval(parser), "for"))
        {
            if (OPTS_OPTION_U32(OPTION_STANDARD) == COMPILER_QCC) {
                if (parsewarning(parser, WARN_EXTENSIONS, "for loops are not recognized in the original Quake C standard, to enable try an alternate standard --std=?"))
                    return false;
            }
            return parse_for(parser, block, out);
        }
        else if (!strcmp(parser_tokval(parser), "break"))
        {
            return parse_break_continue(parser, block, out, false);
        }
        else if (!strcmp(parser_tokval(parser), "continue"))
        {
            return parse_break_continue(parser, block, out, true);
        }
        else if (!strcmp(parser_tokval(parser), "switch"))
        {
            return parse_switch(parser, block, out);
        }
        else if (!strcmp(parser_tokval(parser), "case") ||
                 !strcmp(parser_tokval(parser), "default"))
        {
            if (!allow_cases) {
                parseerror(parser, "unexpected 'case' label");
                return false;
            }
            return true;
        }
        else if (!strcmp(parser_tokval(parser), "goto"))
        {
            return parse_goto(parser, out);
        }
        else if (!strcmp(parser_tokval(parser), "typedef"))
        {
            if (!parser_next(parser)) {
                parseerror(parser, "expected type definition after 'typedef'");
                return false;
            }
            return parse_typedef(parser);
        }
        parseerror(parser, "Unexpected keyword: `%s'", parser_tokval(parser));
        return false;
    }
    else if (parser->tok == '{')
    {
        ast_block *inner;
        inner = parse_block(parser);
        if (!inner)
            return false;
        *out = inner;
        return true;
    }
    else if (parser->tok == ':')
    {
        size_t i;
        ast_label *label;
        if (!parser_next(parser)) {
            parseerror(parser, "expected label name");
            return false;
        }
        if (parser->tok != TOKEN_IDENT) {
            parseerror(parser, "label must be an identifier");
            return false;
        }
        label = (ast_label*)parser_find_label(parser, parser_tokval(parser));
        if (label) {
            if (!label->m_undefined) {
                parseerror(parser, "label `%s` already defined", label->m_name);
                return false;
            }
            label->m_undefined = false;
        }
        else {
            label = new ast_label(parser_ctx(parser), parser_tokval(parser), false);
            parser->labels.push_back(label);
        }
        *out = label;
        if (!parser_next(parser)) {
            parseerror(parser, "parse error after label");
            return false;
        }
        for (i = 0; i < parser->gotos.size(); ++i) {
            if (parser->gotos[i]->m_name == label->m_name) {
                parser->gotos[i]->setLabel(label);
                parser->gotos.erase(parser->gotos.begin() + i);
                --i;
            }
        }
        return true;
    }
    else if (parser->tok == ';')
    {
        if (!parser_next(parser)) {
            parseerror(parser, "parse error after empty statement");
            return false;
        }
        return true;
    }
    else
    {
        lex_ctx_t ctx = parser_ctx(parser);
        ast_expression *exp = parse_expression(parser, false, false);
        if (!exp)
            return false;
        *out = exp;
        if (!exp->m_side_effects) {
            if (compile_warning(ctx, WARN_EFFECTLESS_STATEMENT, "statement has no effect"))
                return false;
        }
        return true;
    }
}

static bool parse_enum(parser_t *parser)
{
    bool        flag = false;
    bool        reverse = false;
    qcfloat_t     num = 0;
    ast_value  *var = nullptr;
    ast_value  *asvalue;
    std::vector<ast_value*> values;

    ast_expression *old;

    if (!parser_next(parser) || (parser->tok != '{' && parser->tok != ':')) {
        parseerror(parser, "expected `{` or `:` after `enum` keyword");
        return false;
    }

    /* enumeration attributes (can add more later) */
    if (parser->tok == ':') {
        if (!parser_next(parser) || parser->tok != TOKEN_IDENT){
            parseerror(parser, "expected `flag` or `reverse` for enumeration attribute");
            return false;
        }

        /* attributes? */
        if (!strcmp(parser_tokval(parser), "flag")) {
            num  = 1;
            flag = true;
        }
        else if (!strcmp(parser_tokval(parser), "reverse")) {
            reverse = true;
        }
        else {
            parseerror(parser, "invalid attribute `%s` for enumeration", parser_tokval(parser));
            return false;
        }

        if (!parser_next(parser) || parser->tok != '{') {
            parseerror(parser, "expected `{` after enum attribute ");
            return false;
        }
    }

    while (true) {
        if (!parser_next(parser) || parser->tok != TOKEN_IDENT) {
            if (parser->tok == '}') {
                /* allow an empty enum */
                break;
            }
            parseerror(parser, "expected identifier or `}`");
            return false;
        }

        old = parser_find_field(parser, parser_tokval(parser));
        if (!old)
            old = parser_find_global(parser, parser_tokval(parser));
        if (old) {
            parseerror(parser, "value `%s` has already been declared here: %s:%i",
                       parser_tokval(parser), old->m_context.file, old->m_context.line);
            return false;
        }

        var = new ast_value(parser_ctx(parser), parser_tokval(parser), TYPE_FLOAT);
        values.push_back(var);
        var->m_cvq             = CV_CONST;
        var->m_hasvalue        = true;

        /* for flagged enumerations increment in POTs of TWO */
        var->m_constval.vfloat = (flag) ? (num *= 2) : (num ++);
        parser_addglobal(parser, var->m_name, var);

        if (!parser_next(parser)) {
            parseerror(parser, "expected `=`, `}` or comma after identifier");
            return false;
        }

        if (parser->tok == ',')
            continue;
        if (parser->tok == '}')
            break;
        if (parser->tok != '=') {
            parseerror(parser, "expected `=`, `}` or comma after identifier");
            return false;
        }

        if (!parser_next(parser)) {
            parseerror(parser, "expected expression after `=`");
            return false;
        }

        /* We got a value! */
        old = parse_expression_leave(parser, true, false, false);
        asvalue = (ast_value*)old;
        if (!ast_istype(old, ast_value) || asvalue->m_cvq != CV_CONST || !asvalue->m_hasvalue) {
            compile_error(var->m_context, "constant value or expression expected");
            return false;
        }
        num = (var->m_constval.vfloat = asvalue->m_constval.vfloat) + 1;

        if (parser->tok == '}')
            break;
        if (parser->tok != ',') {
            parseerror(parser, "expected `}` or comma after expression");
            return false;
        }
    }

    /* patch them all (for reversed attribute) */
    if (reverse) {
        size_t i;
        for (i = 0; i < values.size(); i++)
            values[i]->m_constval.vfloat = values.size() - i - 1;
    }

    if (parser->tok != '}') {
        parseerror(parser, "internal error: breaking without `}`");
        return false;
    }

    if (!parser_next(parser) || parser->tok != ';') {
        parseerror(parser, "expected semicolon after enumeration");
        return false;
    }

    if (!parser_next(parser)) {
        parseerror(parser, "parse error after enumeration");
        return false;
    }

    return true;
}

static bool parse_block_into(parser_t *parser, ast_block *block)
{
    bool   retval = true;

    parser_enterblock(parser);

    if (!parser_next(parser)) { /* skip the '{' */
        parseerror(parser, "expected function body");
        goto cleanup;
    }

    while (parser->tok != TOKEN_EOF && parser->tok < TOKEN_ERROR)
    {
        ast_expression *expr = nullptr;
        if (parser->tok == '}')
            break;

        if (!parse_statement(parser, block, &expr, false)) {
            /* parseerror(parser, "parse error"); */
            block = nullptr;
            goto cleanup;
        }
        if (!expr)
            continue;
        if (!block->addExpr(expr)) {
            delete block;
            block = nullptr;
            goto cleanup;
        }
    }

    if (parser->tok != '}') {
        block = nullptr;
    } else {
        (void)parser_next(parser);
    }

cleanup:
    if (!parser_leaveblock(parser))
        retval = false;
    return retval && !!block;
}

static ast_block* parse_block(parser_t *parser)
{
    ast_block *block;
    block = new ast_block(parser_ctx(parser));
    if (!block)
        return nullptr;
    if (!parse_block_into(parser, block)) {
        delete block;
        return nullptr;
    }
    return block;
}

static bool parse_statement_or_block(parser_t *parser, ast_block* parent_block, ast_expression **out)
{
    if (parser->tok == '{') {
        *out = parse_block(parser);
        return !!*out;
    }
    return parse_statement(parser, parent_block, out, false);
}

static bool create_vector_members(ast_value *var, ast_member **me)
{
    size_t i;
    size_t len = var->m_name.length();

    for (i = 0; i < 3; ++i) {
        char *name = (char*)mem_a(len+3);
        memcpy(name, var->m_name.c_str(), len);
        name[len+0] = '_';
        name[len+1] = 'x'+i;
        name[len+2] = 0;
        me[i] = ast_member::make(var->m_context, var, i, name);
        mem_d(name);
        if (!me[i])
            break;
    }
    if (i == 3)
        return true;

    /* unroll */
    do { delete me[--i]; } while(i);
    return false;
}

static bool parse_function_body(parser_t *parser, ast_value *var)
{
    ast_block *block = nullptr;
    ast_function *func;
    ast_function *old;

    ast_expression *framenum  = nullptr;
    ast_expression *nextthink = nullptr;
    /* None of the following have to be deleted */
    ast_expression *fld_think = nullptr, *fld_nextthink = nullptr, *fld_frame = nullptr;
    ast_expression *gbl_time = nullptr, *gbl_self = nullptr;
    bool has_frame_think;

    bool retval = true;

    has_frame_think = false;
    old = parser->function;

    if (var->m_flags & AST_FLAG_ALIAS) {
        parseerror(parser, "function aliases cannot have bodies");
        return false;
    }

    if (parser->gotos.size() || parser->labels.size()) {
        parseerror(parser, "gotos/labels leaking");
        return false;
    }

    if (!OPTS_FLAG(VARIADIC_ARGS) && var->m_flags & AST_FLAG_VARIADIC) {
        if (parsewarning(parser, WARN_VARIADIC_FUNCTION,
                         "variadic function with implementation will not be able to access additional parameters (try -fvariadic-args)"))
        {
            return false;
        }
    }

    if (parser->tok == '[') {
        /* got a frame definition: [ framenum, nextthink ]
         * this translates to:
         * self.frame = framenum;
         * self.nextthink = time + 0.1;
         * self.think = nextthink;
         */
        nextthink = nullptr;

        fld_think     = parser_find_field(parser, "think");
        fld_nextthink = parser_find_field(parser, "nextthink");
        fld_frame     = parser_find_field(parser, "frame");
        if (!fld_think || !fld_nextthink || !fld_frame) {
            parseerror(parser, "cannot use [frame,think] notation without the required fields");
            parseerror(parser, "please declare the following entityfields: `frame`, `think`, `nextthink`");
            return false;
        }
        gbl_time      = parser_find_global(parser, "time");
        gbl_self      = parser_find_global(parser, "self");
        if (!gbl_time || !gbl_self) {
            parseerror(parser, "cannot use [frame,think] notation without the required globals");
            parseerror(parser, "please declare the following globals: `time`, `self`");
            return false;
        }

        if (!parser_next(parser))
            return false;

        framenum = parse_expression_leave(parser, true, false, false);
        if (!framenum) {
            parseerror(parser, "expected a framenumber constant in[frame,think] notation");
            return false;
        }
        if (!ast_istype(framenum, ast_value) || !( (ast_value*)framenum )->m_hasvalue) {
            ast_unref(framenum);
            parseerror(parser, "framenumber in [frame,think] notation must be a constant");
            return false;
        }

        if (parser->tok != ',') {
            ast_unref(framenum);
            parseerror(parser, "expected comma after frame number in [frame,think] notation");
            parseerror(parser, "Got a %i\n", parser->tok);
            return false;
        }

        if (!parser_next(parser)) {
            ast_unref(framenum);
            return false;
        }

        if (parser->tok == TOKEN_IDENT && !parser_find_var(parser, parser_tokval(parser)))
        {
            /* qc allows the use of not-yet-declared functions here
             * - this automatically creates a prototype */
            ast_value      *thinkfunc;
            ast_expression *functype = fld_think->m_next;

            thinkfunc = new ast_value(parser_ctx(parser), parser_tokval(parser), functype->m_vtype);
            if (!thinkfunc) { /* || !thinkfunc->adoptType(*functype)*/
                ast_unref(framenum);
                parseerror(parser, "failed to create implicit prototype for `%s`", parser_tokval(parser));
                return false;
            }
            thinkfunc->adoptType(*functype);

            if (!parser_next(parser)) {
                ast_unref(framenum);
                delete thinkfunc;
                return false;
            }

            parser_addglobal(parser, thinkfunc->m_name, thinkfunc);

            nextthink = thinkfunc;

        } else {
            nextthink = parse_expression_leave(parser, true, false, false);
            if (!nextthink) {
                ast_unref(framenum);
                parseerror(parser, "expected a think-function in [frame,think] notation");
                return false;
            }
        }

        if (!ast_istype(nextthink, ast_value)) {
            parseerror(parser, "think-function in [frame,think] notation must be a constant");
            retval = false;
        }

        if (retval && parser->tok != ']') {
            parseerror(parser, "expected closing `]` for [frame,think] notation");
            retval = false;
        }

        if (retval && !parser_next(parser)) {
            retval = false;
        }

        if (retval && parser->tok != '{') {
            parseerror(parser, "a function body has to be declared after a [frame,think] declaration");
            retval = false;
        }

        if (!retval) {
            ast_unref(nextthink);
            ast_unref(framenum);
            return false;
        }

        has_frame_think = true;
    }

    block = new ast_block(parser_ctx(parser));
    if (!block) {
        parseerror(parser, "failed to allocate block");
        if (has_frame_think) {
            ast_unref(nextthink);
            ast_unref(framenum);
        }
        return false;
    }

    if (has_frame_think) {
        if (!OPTS_FLAG(EMULATE_STATE)) {
            ast_state *state_op = new ast_state(parser_ctx(parser), framenum, nextthink);
            if (!block->addExpr(state_op)) {
                parseerror(parser, "failed to generate state op for [frame,think]");
                ast_unref(nextthink);
                ast_unref(framenum);
                delete block;
                return false;
            }
        } else {
            /* emulate OP_STATE in code: */
            lex_ctx_t ctx;
            ast_expression *self_frame;
            ast_expression *self_nextthink;
            ast_expression *self_think;
            ast_expression *time_plus_1;
            ast_store *store_frame;
            ast_store *store_nextthink;
            ast_store *store_think;

            float frame_delta = 1.0f / (float)OPTS_OPTION_U32(OPTION_STATE_FPS);

            ctx = parser_ctx(parser);
            self_frame     = new ast_entfield(ctx, gbl_self, fld_frame);
            self_nextthink = new ast_entfield(ctx, gbl_self, fld_nextthink);
            self_think     = new ast_entfield(ctx, gbl_self, fld_think);

            time_plus_1    = new ast_binary(ctx, INSTR_ADD_F,
                             gbl_time, parser->m_fold.constgen_float(frame_delta, false));

            if (!self_frame || !self_nextthink || !self_think || !time_plus_1) {
                if (self_frame)     delete self_frame;
                if (self_nextthink) delete self_nextthink;
                if (self_think)     delete self_think;
                if (time_plus_1)    delete time_plus_1;
                retval = false;
            }

            if (retval)
            {
                store_frame     = new ast_store(ctx, INSTR_STOREP_F,   self_frame,     framenum);
                store_nextthink = new ast_store(ctx, INSTR_STOREP_F,   self_nextthink, time_plus_1);
                store_think     = new ast_store(ctx, INSTR_STOREP_FNC, self_think,     nextthink);

                if (!store_frame) {
                    delete self_frame;
                    retval = false;
                }
                if (!store_nextthink) {
                    delete self_nextthink;
                    retval = false;
                }
                if (!store_think) {
                    delete self_think;
                    retval = false;
                }
                if (!retval) {
                    if (store_frame)     delete store_frame;
                    if (store_nextthink) delete store_nextthink;
                    if (store_think)     delete store_think;
                    retval = false;
                }
                if (!block->addExpr(store_frame) ||
                    !block->addExpr(store_nextthink) ||
                    !block->addExpr(store_think))
                {
                    retval = false;
                }
            }

            if (!retval) {
                parseerror(parser, "failed to generate code for [frame,think]");
                ast_unref(nextthink);
                ast_unref(framenum);
                delete block;
                return false;
            }
        }
    }

    if (var->m_hasvalue) {
        if (!(var->m_flags & AST_FLAG_ACCUMULATE)) {
            parseerror(parser, "function `%s` declared with multiple bodies", var->m_name);
            delete block;
            goto enderr;
        }
        func = var->m_constval.vfunc;

        if (!func) {
            parseerror(parser, "internal error: nullptr function: `%s`", var->m_name);
            delete block;
            goto enderr;
        }
    } else {
        func = ast_function::make(var->m_context, var->m_name, var);

        if (!func) {
            parseerror(parser, "failed to allocate function for `%s`", var->m_name);
            delete block;
            goto enderr;
        }
        parser->functions.push_back(func);
    }

    parser_enterblock(parser);

    for (auto &it : var->m_type_params) {
        size_t e;
        ast_member *me[3];

        if (it->m_vtype != TYPE_VECTOR &&
            (it->m_vtype != TYPE_FIELD ||
             it->m_next->m_vtype != TYPE_VECTOR))
        {
            continue;
        }

        if (!create_vector_members(it.get(), me)) {
            delete block;
            goto enderrfn;
        }

        for (e = 0; e < 3; ++e) {
            parser_addlocal(parser, me[e]->m_name, me[e]);
            block->collect(me[e]);
        }
    }

    if (var->m_argcounter && !func->m_argc) {
        ast_value *argc = new ast_value(var->m_context, var->m_argcounter, TYPE_FLOAT);
        parser_addlocal(parser, argc->m_name, argc);
        func->m_argc.reset(argc);
    }

    if (OPTS_FLAG(VARIADIC_ARGS) && var->m_flags & AST_FLAG_VARIADIC && !func->m_varargs) {
        char name[1024];
        ast_value *varargs = new ast_value(var->m_context, "reserved:va_args", TYPE_ARRAY);
        varargs->m_flags |= AST_FLAG_IS_VARARG;
        varargs->m_next = new ast_value(var->m_context, "", TYPE_VECTOR);
        varargs->m_count = 0;
        util_snprintf(name, sizeof(name), "%s##va##SET", var->m_name.c_str());
        if (!parser_create_array_setter_proto(parser, varargs, name)) {
            delete varargs;
            delete block;
            goto enderrfn;
        }
        util_snprintf(name, sizeof(name), "%s##va##GET", var->m_name.c_str());
        if (!parser_create_array_getter_proto(parser, varargs, varargs->m_next, name)) {
            delete varargs;
            delete block;
            goto enderrfn;
        }
        func->m_varargs.reset(varargs);
        func->m_fixedparams = (ast_value*)parser->m_fold.constgen_float(var->m_type_params.size(), false);
    }

    parser->function = func;
    if (!parse_block_into(parser, block)) {
        delete block;
        goto enderrfn;
    }

    func->m_blocks.emplace_back(block);

    parser->function = old;
    if (!parser_leaveblock(parser))
        retval = false;
    if (parser->variables.size() != PARSER_HT_LOCALS) {
        parseerror(parser, "internal error: local scopes left");
        retval = false;
    }

    if (parser->tok == ';')
        return parser_next(parser);
    else if (OPTS_OPTION_U32(OPTION_STANDARD) == COMPILER_QCC)
        parseerror(parser, "missing semicolon after function body (mandatory with -std=qcc)");
    return retval;

enderrfn:
    (void)!parser_leaveblock(parser);

    delete func;

    // Remove |func| from |parser->functions|. It may not actually be at the
    // back of the vector for accumulated functions.
    for (auto it = parser->functions.begin(); it != parser->functions.end(); it++) {
        if (*it == func) {
            parser->functions.erase(it, it + 1);
            break;
        }
    }

    var->m_constval.vfunc = nullptr;

enderr:
    parser->function = old;
    return false;
}

static ast_expression *array_accessor_split(
    parser_t  *parser,
    ast_value *array,
    ast_value *index,
    size_t     middle,
    ast_expression *left,
    ast_expression *right
    )
{
    ast_ifthen *ifthen;
    ast_binary *cmp;

    lex_ctx_t ctx = array->m_context;

    if (!left || !right) {
        if (left)  delete left;
        if (right) delete right;
        return nullptr;
    }

    cmp = new ast_binary(ctx, INSTR_LT,
                         index,
                         parser->m_fold.constgen_float(middle, false));
    if (!cmp) {
        delete left;
        delete right;
        parseerror(parser, "internal error: failed to create comparison for array setter");
        return nullptr;
    }

    ifthen = new ast_ifthen(ctx, cmp, left, right);
    if (!ifthen) {
        delete cmp; /* will delete left and right */
        parseerror(parser, "internal error: failed to create conditional jump for array setter");
        return nullptr;
    }

    return ifthen;
}

static ast_expression *array_setter_node(parser_t *parser, ast_value *array, ast_value *index, ast_value *value, size_t from, size_t afterend)
{
    lex_ctx_t ctx = array->m_context;

    if (from+1 == afterend) {
        /* set this value */
        ast_block       *block;
        ast_return      *ret;
        ast_array_index *subscript;
        ast_store       *st;
        int assignop = type_store_instr[value->m_vtype];

        if (value->m_vtype == TYPE_FIELD && value->m_next->m_vtype == TYPE_VECTOR)
            assignop = INSTR_STORE_V;

        subscript = ast_array_index::make(ctx, array, parser->m_fold.constgen_float(from, false));
        if (!subscript)
            return nullptr;

        st = new ast_store(ctx, assignop, subscript, value);
        if (!st) {
            delete subscript;
            return nullptr;
        }

        block = new ast_block(ctx);
        if (!block) {
            delete st;
            return nullptr;
        }

        if (!block->addExpr(st)) {
            delete block;
            return nullptr;
        }

        ret = new ast_return(ctx, nullptr);
        if (!ret) {
            delete block;
            return nullptr;
        }

        if (!block->addExpr(ret)) {
            delete block;
            return nullptr;
        }

        return block;
    } else {
        ast_expression *left, *right;
        size_t diff = afterend - from;
        size_t middle = from + diff/2;
        left  = array_setter_node(parser, array, index, value, from, middle);
        right = array_setter_node(parser, array, index, value, middle, afterend);
        return array_accessor_split(parser, array, index, middle, left, right);
    }
}

static ast_expression *array_field_setter_node(
    parser_t  *parser,
    ast_value *array,
    ast_value *entity,
    ast_value *index,
    ast_value *value,
    size_t     from,
    size_t     afterend)
{
    lex_ctx_t ctx = array->m_context;

    if (from+1 == afterend) {
        /* set this value */
        ast_block       *block;
        ast_return      *ret;
        ast_entfield    *entfield;
        ast_array_index *subscript;
        ast_store       *st;
        int assignop = type_storep_instr[value->m_vtype];

        if (value->m_vtype == TYPE_FIELD && value->m_next->m_vtype == TYPE_VECTOR)
            assignop = INSTR_STOREP_V;

        subscript = ast_array_index::make(ctx, array, parser->m_fold.constgen_float(from, false));
        if (!subscript)
            return nullptr;

        subscript->m_next = new ast_expression(ast_copy_type, subscript->m_context, *subscript);
        subscript->m_vtype = TYPE_FIELD;

        entfield = new ast_entfield(ctx, entity, subscript, subscript);
        if (!entfield) {
            delete subscript;
            return nullptr;
        }

        st = new ast_store(ctx, assignop, entfield, value);
        if (!st) {
            delete entfield;
            return nullptr;
        }

        block = new ast_block(ctx);
        if (!block) {
            delete st;
            return nullptr;
        }

        if (!block->addExpr(st)) {
            delete block;
            return nullptr;
        }

        ret = new ast_return(ctx, nullptr);
        if (!ret) {
            delete block;
            return nullptr;
        }

        if (!block->addExpr(ret)) {
            delete block;
            return nullptr;
        }

        return block;
    } else {
        ast_expression *left, *right;
        size_t diff = afterend - from;
        size_t middle = from + diff/2;
        left  = array_field_setter_node(parser, array, entity, index, value, from, middle);
        right = array_field_setter_node(parser, array, entity, index, value, middle, afterend);
        return array_accessor_split(parser, array, index, middle, left, right);
    }
}

static ast_expression *array_getter_node(parser_t *parser, ast_value *array, ast_value *index, size_t from, size_t afterend)
{
    lex_ctx_t ctx = array->m_context;

    if (from+1 == afterend) {
        ast_return      *ret;
        ast_array_index *subscript;

        subscript = ast_array_index::make(ctx, array, parser->m_fold.constgen_float(from, false));
        if (!subscript)
            return nullptr;

        ret = new ast_return(ctx, subscript);
        if (!ret) {
            delete subscript;
            return nullptr;
        }

        return ret;
    } else {
        ast_expression *left, *right;
        size_t diff = afterend - from;
        size_t middle = from + diff/2;
        left  = array_getter_node(parser, array, index, from, middle);
        right = array_getter_node(parser, array, index, middle, afterend);
        return array_accessor_split(parser, array, index, middle, left, right);
    }
}

static bool parser_create_array_accessor(parser_t *parser, ast_value *array, const char *funcname, ast_value **out)
{
    ast_function   *func = nullptr;
    ast_value      *fval = nullptr;
    ast_block      *body = nullptr;

    fval = new ast_value(array->m_context, funcname, TYPE_FUNCTION);
    if (!fval) {
        parseerror(parser, "failed to create accessor function value");
        return false;
    }
    fval->m_flags &= ~(AST_FLAG_COVERAGE_MASK);

    func = ast_function::make(array->m_context, funcname, fval);
    if (!func) {
        delete fval;
        parseerror(parser, "failed to create accessor function node");
        return false;
    }

    body = new ast_block(array->m_context);
    if (!body) {
        parseerror(parser, "failed to create block for array accessor");
        delete fval;
        delete func;
        return false;
    }

    func->m_blocks.emplace_back(body);
    *out = fval;

    parser->accessors.push_back(fval);

    return true;
}

static ast_value* parser_create_array_setter_proto(parser_t *parser, ast_value *array, const char *funcname)
{
    ast_value      *index = nullptr;
    ast_value      *value = nullptr;
    ast_function   *func;
    ast_value      *fval;

    if (!ast_istype(array->m_next, ast_value)) {
        parseerror(parser, "internal error: array accessor needs to build an ast_value with a copy of the element type");
        return nullptr;
    }

    if (!parser_create_array_accessor(parser, array, funcname, &fval))
        return nullptr;
    func = fval->m_constval.vfunc;
    fval->m_next = new ast_value(array->m_context, "<void>", TYPE_VOID);

    index = new ast_value(array->m_context, "index", TYPE_FLOAT);
    value = new ast_value(ast_copy_type, *(ast_value*)array->m_next);

    if (!index || !value) {
        parseerror(parser, "failed to create locals for array accessor");
        goto cleanup;
    }
    value->m_name = "value"; // not important
    fval->m_type_params.emplace_back(index);
    fval->m_type_params.emplace_back(value);

    array->m_setter = fval;
    return fval;
cleanup:
    if (index) delete index;
    if (value) delete value;
    delete func;
    delete fval;
    return nullptr;
}

static bool parser_create_array_setter_impl(parser_t *parser, ast_value *array)
{
    ast_expression *root = nullptr;
    root = array_setter_node(parser, array,
                             array->m_setter->m_type_params[0].get(),
                             array->m_setter->m_type_params[1].get(),
                             0, array->m_count);
    if (!root) {
        parseerror(parser, "failed to build accessor search tree");
        return false;
    }
    if (!array->m_setter->m_constval.vfunc->m_blocks[0].get()->addExpr(root)) {
        delete root;
        return false;
    }
    return true;
}

static bool parser_create_array_setter(parser_t *parser, ast_value *array, const char *funcname)
{
    if (!parser_create_array_setter_proto(parser, array, funcname))
        return false;
    return parser_create_array_setter_impl(parser, array);
}

static bool parser_create_array_field_setter(parser_t *parser, ast_value *array, const char *funcname)
{
    ast_expression *root = nullptr;
    ast_value      *entity = nullptr;
    ast_value      *index = nullptr;
    ast_value      *value = nullptr;
    ast_function   *func;
    ast_value      *fval;

    if (!ast_istype(array->m_next, ast_value)) {
        parseerror(parser, "internal error: array accessor needs to build an ast_value with a copy of the element type");
        return false;
    }

    if (!parser_create_array_accessor(parser, array, funcname, &fval))
        return false;
    func = fval->m_constval.vfunc;
    fval->m_next = new ast_value(array->m_context, "<void>", TYPE_VOID);

    entity = new ast_value(array->m_context, "entity", TYPE_ENTITY);
    index  = new ast_value(array->m_context, "index",  TYPE_FLOAT);
    value  = new ast_value(ast_copy_type, *(ast_value*)array->m_next);
    if (!entity || !index || !value) {
        parseerror(parser, "failed to create locals for array accessor");
        goto cleanup;
    }
    value->m_name = "value"; // not important
    fval->m_type_params.emplace_back(entity);
    fval->m_type_params.emplace_back(index);
    fval->m_type_params.emplace_back(value);

    root = array_field_setter_node(parser, array, entity, index, value, 0, array->m_count);
    if (!root) {
        parseerror(parser, "failed to build accessor search tree");
        goto cleanup;
    }

    array->m_setter = fval;
    return func->m_blocks[0].get()->addExpr(root);
cleanup:
    if (entity) delete entity;
    if (index)  delete index;
    if (value)  delete value;
    if (root)   delete root;
    delete func;
    delete fval;
    return false;
}

static ast_value* parser_create_array_getter_proto(parser_t *parser, ast_value *array, const ast_expression *elemtype, const char *funcname)
{
    ast_value      *index = nullptr;
    ast_value      *fval;
    ast_function   *func;

    /* NOTE: checking array->m_next rather than elemtype since
     * for fields elemtype is a temporary fieldtype.
     */
    if (!ast_istype(array->m_next, ast_value)) {
        parseerror(parser, "internal error: array accessor needs to build an ast_value with a copy of the element type");
        return nullptr;
    }

    if (!parser_create_array_accessor(parser, array, funcname, &fval))
        return nullptr;
    func = fval->m_constval.vfunc;
    fval->m_next = new ast_expression(ast_copy_type, array->m_context, *elemtype);

    index = new ast_value(array->m_context, "index", TYPE_FLOAT);

    if (!index) {
        parseerror(parser, "failed to create locals for array accessor");
        goto cleanup;
    }
    fval->m_type_params.emplace_back(index);

    array->m_getter = fval;
    return fval;
cleanup:
    if (index) delete index;
    delete func;
    delete fval;
    return nullptr;
}

static bool parser_create_array_getter_impl(parser_t *parser, ast_value *array)
{
    ast_expression *root = nullptr;

    root = array_getter_node(parser, array, array->m_getter->m_type_params[0].get(), 0, array->m_count);
    if (!root) {
        parseerror(parser, "failed to build accessor search tree");
        return false;
    }
    if (!array->m_getter->m_constval.vfunc->m_blocks[0].get()->addExpr(root)) {
        delete root;
        return false;
    }
    return true;
}

static bool parser_create_array_getter(parser_t *parser, ast_value *array, const ast_expression *elemtype, const char *funcname)
{
    if (!parser_create_array_getter_proto(parser, array, elemtype, funcname))
        return false;
    return parser_create_array_getter_impl(parser, array);
}

static ast_value *parse_parameter_list(parser_t *parser, ast_value *var)
{
    lex_ctx_t ctx = parser_ctx(parser);
    std::vector<std::unique_ptr<ast_value>> params;
    ast_value *fval;
    bool first = true;
    bool variadic = false;
    ast_value *varparam = nullptr;
    char *argcounter = nullptr;

    /* for the sake of less code we parse-in in this function */
    if (!parser_next(parser)) {
        delete var;
        parseerror(parser, "expected parameter list");
        return nullptr;
    }

    /* parse variables until we hit a closing paren */
    while (parser->tok != ')') {
        bool is_varargs = false;

        if (!first) {
            /* there must be commas between them */
            if (parser->tok != ',') {
                parseerror(parser, "expected comma or end of parameter list");
                goto on_error;
            }
            if (!parser_next(parser)) {
                parseerror(parser, "expected parameter");
                goto on_error;
            }
        }
        first = false;

        ast_value *param = parse_typename(parser, nullptr, nullptr, &is_varargs);
        if (!param && !is_varargs)
            goto on_error;
        if (is_varargs) {
            /* '...' indicates a varargs function */
            variadic = true;
            if (parser->tok != ')' && parser->tok != TOKEN_IDENT) {
                parseerror(parser, "`...` must be the last parameter of a variadic function declaration");
                goto on_error;
            }
            if (parser->tok == TOKEN_IDENT) {
                argcounter = util_strdup(parser_tokval(parser));
                if (!parser_next(parser) || parser->tok != ')') {
                    parseerror(parser, "`...` must be the last parameter of a variadic function declaration");
                    goto on_error;
                }
            }
        } else {
            params.emplace_back(param);
            if (param->m_vtype >= TYPE_VARIANT) {
                char tname[1024]; /* typename is reserved in C++ */
                ast_type_to_string(param, tname, sizeof(tname));
                parseerror(parser, "type not supported as part of a parameter list: %s", tname);
                goto on_error;
            }
            /* type-restricted varargs */
            if (parser->tok == TOKEN_DOTS) {
                variadic = true;
                varparam = params.back().release();
                params.pop_back();
                if (!parser_next(parser) || (parser->tok != ')' && parser->tok != TOKEN_IDENT)) {
                    parseerror(parser, "`...` must be the last parameter of a variadic function declaration");
                    goto on_error;
                }
                if (parser->tok == TOKEN_IDENT) {
                    argcounter = util_strdup(parser_tokval(parser));
                    param->m_name = argcounter;
                    if (!parser_next(parser) || parser->tok != ')') {
                        parseerror(parser, "`...` must be the last parameter of a variadic function declaration");
                        goto on_error;
                    }
                }
            }
            if (OPTS_OPTION_U32(OPTION_STANDARD) == COMPILER_FTEQCC && param->m_name[0] == '<') {
                parseerror(parser, "parameter name omitted");
                goto on_error;
            }
        }
    }

    if (params.size() == 1 && params[0]->m_vtype == TYPE_VOID)
        params.clear();

    /* sanity check */
    if (params.size() > 8 && OPTS_OPTION_U32(OPTION_STANDARD) == COMPILER_QCC)
        (void)!parsewarning(parser, WARN_EXTENSIONS, "more than 8 parameters are not supported by this standard");

    /* parse-out */
    if (!parser_next(parser)) {
        parseerror(parser, "parse error after typename");
        goto on_error;
    }

    /* now turn 'var' into a function type */
    fval = new ast_value(ctx, "<type()>", TYPE_FUNCTION);
    fval->m_next = var;
    if (variadic)
        fval->m_flags |= AST_FLAG_VARIADIC;
    var = fval;

    var->m_type_params = move(params);
    var->m_varparam = varparam;
    var->m_argcounter = argcounter;

    return var;

on_error:
    if (argcounter)
        mem_d(argcounter);
    if (varparam)
        delete varparam;
    delete var;
    return nullptr;
}

static ast_value *parse_arraysize(parser_t *parser, ast_value *var)
{
    ast_expression *cexp;
    ast_value      *cval, *tmp;
    lex_ctx_t ctx;

    ctx = parser_ctx(parser);

    if (!parser_next(parser)) {
        delete var;
        parseerror(parser, "expected array-size");
        return nullptr;
    }

    if (parser->tok != ']') {
        cexp = parse_expression_leave(parser, true, false, false);

        if (!cexp || !ast_istype(cexp, ast_value)) {
            if (cexp)
                ast_unref(cexp);
            delete var;
            parseerror(parser, "expected array-size as constant positive integer");
            return nullptr;
        }
        cval = (ast_value*)cexp;
    }
    else {
        cexp = nullptr;
        cval = nullptr;
    }

    tmp = new ast_value(ctx, "<type[]>", TYPE_ARRAY);
    tmp->m_next = var;
    var = tmp;

    if (cval) {
        if (cval->m_vtype == TYPE_INTEGER)
            tmp->m_count = cval->m_constval.vint;
        else if (cval->m_vtype == TYPE_FLOAT)
            tmp->m_count = cval->m_constval.vfloat;
        else {
            ast_unref(cexp);
            delete var;
            parseerror(parser, "array-size must be a positive integer constant");
            return nullptr;
        }

        ast_unref(cexp);
    } else {
        var->m_count = -1;
        var->m_flags |= AST_FLAG_ARRAY_INIT;
    }

    if (parser->tok != ']') {
        delete var;
        parseerror(parser, "expected ']' after array-size");
        return nullptr;
    }
    if (!parser_next(parser)) {
        delete var;
        parseerror(parser, "error after parsing array size");
        return nullptr;
    }
    return var;
}

/* Parse a complete typename.
 * for single-variables (ie. function parameters or typedefs) storebase should be nullptr
 * but when parsing variables separated by comma
 * 'storebase' should point to where the base-type should be kept.
 * The base type makes up every bit of type information which comes *before* the
 * variable name.
 *
 * NOTE: The value must either be named, have a nullptr name, or a name starting
 *       with '<'. In the first case, this will be the actual variable or type
 *       name, in the other cases it is assumed that the name will appear
 *       later, and an error is generated otherwise.
 *
 * The following will be parsed in its entirety:
 *     void() foo()
 * The 'basetype' in this case is 'void()'
 * and if there's a comma after it, say:
 *     void() foo(), bar
 * then the type-information 'void()' can be stored in 'storebase'
 */
static ast_value *parse_typename(parser_t *parser, ast_value **storebase, ast_value *cached_typedef, bool *is_vararg)
{
    ast_value *var, *tmp;
    lex_ctx_t    ctx;

    const char *name = nullptr;
    bool        isfield  = false;
    bool        wasarray = false;
    size_t      morefields = 0;

    bool        vararg = (parser->tok == TOKEN_DOTS);

    ctx = parser_ctx(parser);

    /* types may start with a dot */
    if (parser->tok == '.' || parser->tok == TOKEN_DOTS) {
        isfield = true;
        if (parser->tok == TOKEN_DOTS)
            morefields += 2;
        /* if we parsed a dot we need a typename now */
        if (!parser_next(parser)) {
            parseerror(parser, "expected typename for field definition");
            return nullptr;
        }

        /* Further dots are handled seperately because they won't be part of the
         * basetype
         */
        while (true) {
            if (parser->tok == '.')
                ++morefields;
            else if (parser->tok == TOKEN_DOTS)
                morefields += 3;
            else
                break;
            vararg = false;
            if (!parser_next(parser)) {
                parseerror(parser, "expected typename for field definition");
                return nullptr;
            }
        }
    }
    if (parser->tok == TOKEN_IDENT)
        cached_typedef = parser_find_typedef(parser, parser_tokval(parser), 0);
    if (!cached_typedef && parser->tok != TOKEN_TYPENAME) {
        if (vararg && is_vararg) {
            *is_vararg = true;
            return nullptr;
        }
        parseerror(parser, "expected typename");
        return nullptr;
    }

    /* generate the basic type value */
    if (cached_typedef) {
        var = new ast_value(ast_copy_type, *cached_typedef);
        var->m_name = "<type(from_def)>";
    } else
        var = new ast_value(ctx, "<type>", parser_token(parser)->constval.t);

    for (; morefields; --morefields) {
        tmp = new ast_value(ctx, "<.type>", TYPE_FIELD);
        tmp->m_next = var;
        var = tmp;
    }

    /* do not yet turn into a field - remember:
     * .void() foo; is a field too
     * .void()() foo; is a function
     */

    /* parse on */
    if (!parser_next(parser)) {
        delete var;
        parseerror(parser, "parse error after typename");
        return nullptr;
    }

    /* an opening paren now starts the parameter-list of a function
     * this is where original-QC has parameter lists.
     * We allow a single parameter list here.
     * Much like fteqcc we don't allow `float()() x`
     */
    if (parser->tok == '(') {
        var = parse_parameter_list(parser, var);
        if (!var)
            return nullptr;
    }

    /* store the base if requested */
    if (storebase) {
        *storebase = new ast_value(ast_copy_type, *var);
        if (isfield) {
            tmp = new ast_value(ctx, "<type:f>", TYPE_FIELD);
            tmp->m_next = *storebase;
            *storebase = tmp;
        }
    }

    /* there may be a name now */
    if (parser->tok == TOKEN_IDENT || parser->tok == TOKEN_KEYWORD) {
        if (!strcmp(parser_tokval(parser), "break"))
            (void)!parsewarning(parser, WARN_BREAKDEF, "break definition ignored (suggest removing it)");
        else if (parser->tok == TOKEN_KEYWORD)
            goto leave;

        name = util_strdup(parser_tokval(parser));

        /* parse on */
        if (!parser_next(parser)) {
            delete var;
            mem_d(name);
            parseerror(parser, "error after variable or field declaration");
            return nullptr;
        }
    }

    leave:
    /* now this may be an array */
    if (parser->tok == '[') {
        wasarray = true;
        var = parse_arraysize(parser, var);
        if (!var) {
            if (name) mem_d(name);
            return nullptr;
        }
    }

    /* This is the point where we can turn it into a field */
    if (isfield) {
        /* turn it into a field if desired */
        tmp = new ast_value(ctx, "<type:f>", TYPE_FIELD);
        tmp->m_next = var;
        var = tmp;
    }

    /* now there may be function parens again */
    if (parser->tok == '(' && OPTS_OPTION_U32(OPTION_STANDARD) == COMPILER_QCC)
        parseerror(parser, "C-style function syntax is not allowed in -std=qcc");
    if (parser->tok == '(' && wasarray)
        parseerror(parser, "arrays as part of a return type is not supported");
    while (parser->tok == '(') {
        var = parse_parameter_list(parser, var);
        if (!var) {
            if (name) mem_d(name);
            return nullptr;
        }
    }

    /* finally name it */
    if (name) {
        var->m_name = name;
        // free the name, ast_value_set_name duplicates
        mem_d(name);
    }

    return var;
}

static bool parse_typedef(parser_t *parser)
{
    ast_value      *typevar, *oldtype;
    ast_expression *old;

    typevar = parse_typename(parser, nullptr, nullptr, nullptr);

    if (!typevar)
        return false;

    // while parsing types, the ast_value's get named '<something>'
    if (!typevar->m_name.length() || typevar->m_name[0] == '<') {
        parseerror(parser, "missing name in typedef");
        delete typevar;
        return false;
    }

    if ( (old = parser_find_var(parser, typevar->m_name)) ) {
        parseerror(parser, "cannot define a type with the same name as a variable: %s\n"
                   " -> `%s` has been declared here: %s:%i",
                   typevar->m_name, old->m_context.file, old->m_context.line);
        delete typevar;
        return false;
    }

    if ( (oldtype = parser_find_typedef(parser, typevar->m_name, parser->_blocktypedefs.back())) ) {
        parseerror(parser, "type `%s` has already been declared here: %s:%i",
                   typevar->m_name, oldtype->m_context.file, oldtype->m_context.line);
        delete typevar;
        return false;
    }

    parser->_typedefs.emplace_back(typevar);
    util_htset(parser->typedefs.back(), typevar->m_name.c_str(), typevar);

    if (parser->tok != ';') {
        parseerror(parser, "expected semicolon after typedef");
        return false;
    }
    if (!parser_next(parser)) {
        parseerror(parser, "parse error after typedef");
        return false;
    }

    return true;
}

static const char *cvq_to_str(int cvq) {
    switch (cvq) {
        case CV_NONE:  return "none";
        case CV_VAR:   return "`var`";
        case CV_CONST: return "`const`";
        default:       return "<INVALID>";
    }
}

static bool parser_check_qualifiers(parser_t *parser, const ast_value *var, const ast_value *proto)
{
    bool av, ao;
    if (proto->m_cvq != var->m_cvq) {
        if (!(proto->m_cvq == CV_CONST && var->m_cvq == CV_NONE &&
              !OPTS_FLAG(INITIALIZED_NONCONSTANTS) &&
              parser->tok == '='))
        {
            return !parsewarning(parser, WARN_DIFFERENT_QUALIFIERS,
                                 "`%s` declared with different qualifiers: %s\n"
                                 " -> previous declaration here: %s:%i uses %s",
                                 var->m_name, cvq_to_str(var->m_cvq),
                                 proto->m_context.file, proto->m_context.line,
                                 cvq_to_str(proto->m_cvq));
        }
    }
    av = (var  ->m_flags & AST_FLAG_NORETURN);
    ao = (proto->m_flags & AST_FLAG_NORETURN);
    if (!av != !ao) {
        return !parsewarning(parser, WARN_DIFFERENT_ATTRIBUTES,
                             "`%s` declared with different attributes%s\n"
                             " -> previous declaration here: %s:%i",
                             var->m_name, (av ? ": noreturn" : ""),
                             proto->m_context.file, proto->m_context.line,
                             (ao ? ": noreturn" : ""));
    }
    return true;
}

static bool create_array_accessors(parser_t *parser, ast_value *var)
{
    char name[1024];
    util_snprintf(name, sizeof(name), "%s##SET", var->m_name.c_str());
    if (!parser_create_array_setter(parser, var, name))
        return false;
    util_snprintf(name, sizeof(name), "%s##GET", var->m_name.c_str());
    if (!parser_create_array_getter(parser, var, var->m_next, name))
        return false;
    return true;
}

static bool parse_array(parser_t *parser, ast_value *array)
{
    size_t i;
    if (array->m_initlist.size()) {
        parseerror(parser, "array already initialized elsewhere");
        return false;
    }
    if (!parser_next(parser)) {
        parseerror(parser, "parse error in array initializer");
        return false;
    }
    i = 0;
    while (parser->tok != '}') {
        ast_value *v = (ast_value*)parse_expression_leave(parser, true, false, false);
        if (!v)
            return false;
        if (!ast_istype(v, ast_value) || !v->m_hasvalue || v->m_cvq != CV_CONST) {
            ast_unref(v);
            parseerror(parser, "initializing element must be a compile time constant");
            return false;
        }
        array->m_initlist.push_back(v->m_constval);
        if (v->m_vtype == TYPE_STRING) {
            array->m_initlist[i].vstring = util_strdupe(array->m_initlist[i].vstring);
            ++i;
        }
        ast_unref(v);
        if (parser->tok == '}')
            break;
        if (parser->tok != ',' || !parser_next(parser)) {
            parseerror(parser, "expected comma or '}' in element list");
            return false;
        }
    }
    if (!parser_next(parser) || parser->tok != ';') {
        parseerror(parser, "expected semicolon after initializer, got %s");
        return false;
    }
    /*
    if (!parser_next(parser)) {
        parseerror(parser, "parse error after initializer");
        return false;
    }
    */

    if (array->m_flags & AST_FLAG_ARRAY_INIT) {
        if (array->m_count != (size_t)-1) {
            parseerror(parser, "array `%s' has already been initialized with %u elements",
                       array->m_name, (unsigned)array->m_count);
        }
        array->m_count = array->m_initlist.size();
        if (!create_array_accessors(parser, array))
            return false;
    }
    return true;
}

static bool parse_variable(parser_t *parser, ast_block *localblock, bool nofields, int qualifier, ast_value *cached_typedef, bool noref, bool is_static, uint32_t qflags, char *vstring)
{
    ast_value *var;
    ast_value *proto;
    ast_expression *old;
    bool       was_end;
    size_t     i;

    ast_value *basetype = nullptr;
    bool      retval    = true;
    bool      isparam   = false;
    bool      isvector  = false;
    bool      cleanvar  = true;
    bool      wasarray  = false;

    ast_member *me[3] = { nullptr, nullptr, nullptr };
    ast_member *last_me[3] = { nullptr, nullptr, nullptr };

    if (!localblock && is_static)
        parseerror(parser, "`static` qualifier is not supported in global scope");

    /* get the first complete variable */
    var = parse_typename(parser, &basetype, cached_typedef, nullptr);
    if (!var) {
        if (basetype)
            delete basetype;
        return false;
    }

    /* while parsing types, the ast_value's get named '<something>' */
    if (!var->m_name.length() || var->m_name[0] == '<') {
        parseerror(parser, "declaration does not declare anything");
        if (basetype)
            delete basetype;
        return false;
    }

    while (true) {
        proto = nullptr;
        wasarray = false;

        /* Part 0: finish the type */
        if (parser->tok == '(') {
            if (OPTS_OPTION_U32(OPTION_STANDARD) == COMPILER_QCC)
                parseerror(parser, "C-style function syntax is not allowed in -std=qcc");
            var = parse_parameter_list(parser, var);
            if (!var) {
                retval = false;
                goto cleanup;
            }
        }
        /* we only allow 1-dimensional arrays */
        if (parser->tok == '[') {
            wasarray = true;
            var = parse_arraysize(parser, var);
            if (!var) {
                retval = false;
                goto cleanup;
            }
        }
        if (parser->tok == '(' && wasarray) {
            parseerror(parser, "arrays as part of a return type is not supported");
            /* we'll still parse the type completely for now */
        }
        /* for functions returning functions */
        while (parser->tok == '(') {
            if (OPTS_OPTION_U32(OPTION_STANDARD) == COMPILER_QCC)
                parseerror(parser, "C-style function syntax is not allowed in -std=qcc");
            var = parse_parameter_list(parser, var);
            if (!var) {
                retval = false;
                goto cleanup;
            }
        }

        var->m_cvq = qualifier;
        if (qflags & AST_FLAG_COVERAGE) /* specified in QC, drop our default */
            var->m_flags &= ~(AST_FLAG_COVERAGE_MASK);
        var->m_flags |= qflags;

        /*
         * store the vstring back to var for alias and
         * deprecation messages.
         */
        if (var->m_flags & AST_FLAG_DEPRECATED || var->m_flags & AST_FLAG_ALIAS)
            var->m_desc = vstring;

        if (parser_find_global(parser, var->m_name) && var->m_flags & AST_FLAG_ALIAS) {
            parseerror(parser, "function aliases cannot be forward declared");
            retval = false;
            goto cleanup;
        }


        /* Part 1:
         * check for validity: (end_sys_..., multiple-definitions, prototypes, ...)
         * Also: if there was a prototype, `var` will be deleted and set to `proto` which
         * is then filled with the previous definition and the parameter-names replaced.
         */
        if (var->m_name == "nil") {
            if (OPTS_FLAG(UNTYPED_NIL)) {
                if (!localblock || !OPTS_FLAG(PERMISSIVE))
                    parseerror(parser, "name `nil` not allowed (try -fpermissive)");
            } else
                (void)!parsewarning(parser, WARN_RESERVED_NAMES, "variable name `nil` is reserved");
        }
        if (!localblock) {
            /* Deal with end_sys_ vars */
            was_end = false;
            if (var->m_name == "end_sys_globals") {
                var->m_flags |= AST_FLAG_NOREF;
                parser->crc_globals = parser->globals.size();
                was_end = true;
            }
            else if (var->m_name == "end_sys_fields") {
                var->m_flags |= AST_FLAG_NOREF;
                parser->crc_fields = parser->fields.size();
                was_end = true;
            }
            if (was_end && var->m_vtype == TYPE_FIELD) {
                if (parsewarning(parser, WARN_END_SYS_FIELDS,
                                 "global '%s' hint should not be a field",
                                 parser_tokval(parser)))
                {
                    retval = false;
                    goto cleanup;
                }
            }

            if (!nofields && var->m_vtype == TYPE_FIELD)
            {
                /* deal with field declarations */
                old = parser_find_field(parser, var->m_name);
                if (old) {
                    if (parsewarning(parser, WARN_FIELD_REDECLARED, "field `%s` already declared here: %s:%i",
                                     var->m_name, old->m_context.file, (int)old->m_context.line))
                    {
                        retval = false;
                        goto cleanup;
                    }
                    delete var;
                    var = nullptr;
                    goto skipvar;
                    /*
                    parseerror(parser, "field `%s` already declared here: %s:%i",
                               var->m_name, old->m_context.file, old->m_context.line);
                    retval = false;
                    goto cleanup;
                    */
                }
                if ((OPTS_OPTION_U32(OPTION_STANDARD) == COMPILER_QCC || OPTS_OPTION_U32(OPTION_STANDARD) == COMPILER_FTEQCC) &&
                    (old = parser_find_global(parser, var->m_name)))
                {
                    parseerror(parser, "cannot declare a field and a global of the same name with -std=qcc");
                    parseerror(parser, "field `%s` already declared here: %s:%i",
                               var->m_name, old->m_context.file, old->m_context.line);
                    retval = false;
                    goto cleanup;
                }
            }
            else
            {
                /* deal with other globals */
                old = parser_find_global(parser, var->m_name);
                if (old && var->m_vtype == TYPE_FUNCTION && old->m_vtype == TYPE_FUNCTION)
                {
                    /* This is a function which had a prototype */
                    if (!ast_istype(old, ast_value)) {
                        parseerror(parser, "internal error: prototype is not an ast_value");
                        retval = false;
                        goto cleanup;
                    }
                    proto = (ast_value*)old;
                    proto->m_desc = var->m_desc;
                    if (!proto->compareType(*var)) {
                        parseerror(parser, "conflicting types for `%s`, previous declaration was here: %s:%i",
                                   proto->m_name,
                                   proto->m_context.file, proto->m_context.line);
                        retval = false;
                        goto cleanup;
                    }
                    /* we need the new parameter-names */
                    for (i = 0; i < proto->m_type_params.size(); ++i)
                        proto->m_type_params[i]->m_name = var->m_type_params[i]->m_name;
                    if (!parser_check_qualifiers(parser, var, proto)) {
                        retval = false;
                        proto = nullptr;
                        goto cleanup;
                    }
                    proto->m_flags |= var->m_flags;
                    delete var;
                    var = proto;
                }
                else
                {
                    /* other globals */
                    if (old) {
                        if (parsewarning(parser, WARN_DOUBLE_DECLARATION,
                                         "global `%s` already declared here: %s:%i",
                                         var->m_name, old->m_context.file, old->m_context.line))
                        {
                            retval = false;
                            goto cleanup;
                        }
                        if (old->m_flags & AST_FLAG_FINAL_DECL) {
                            parseerror(parser, "cannot redeclare variable `%s`, declared final here: %s:%i",
                                       var->m_name, old->m_context.file, old->m_context.line);
                            retval = false;
                            goto cleanup;
                        }
                        proto = (ast_value*)old;
                        if (!ast_istype(old, ast_value)) {
                            parseerror(parser, "internal error: not an ast_value");
                            retval = false;
                            proto = nullptr;
                            goto cleanup;
                        }
                        if (!parser_check_qualifiers(parser, var, proto)) {
                            retval = false;
                            proto = nullptr;
                            goto cleanup;
                        }
                        proto->m_flags |= var->m_flags;
                        /* copy the context for finals,
                         * so the error can show where it was actually made 'final'
                         */
                        if (proto->m_flags & AST_FLAG_FINAL_DECL)
                            old->m_context = var->m_context;
                        delete var;
                        var = proto;
                    }
                    if (OPTS_OPTION_U32(OPTION_STANDARD) == COMPILER_QCC &&
                        (old = parser_find_field(parser, var->m_name)))
                    {
                        parseerror(parser, "cannot declare a field and a global of the same name with -std=qcc");
                        parseerror(parser, "global `%s` already declared here: %s:%i",
                                   var->m_name, old->m_context.file, old->m_context.line);
                        retval = false;
                        goto cleanup;
                    }
                }
            }
        }
        else /* it's not a global */
        {
            old = parser_find_local(parser, var->m_name, parser->variables.size()-1, &isparam);
            if (old && !isparam) {
                parseerror(parser, "local `%s` already declared here: %s:%i",
                           var->m_name, old->m_context.file, (int)old->m_context.line);
                retval = false;
                goto cleanup;
            }
            /* doing this here as the above is just for a single scope */
            old = parser_find_local(parser, var->m_name, 0, &isparam);
            if (old && isparam) {
                if (parsewarning(parser, WARN_LOCAL_SHADOWS,
                                 "local `%s` is shadowing a parameter", var->m_name))
                {
                    parseerror(parser, "local `%s` already declared here: %s:%i",
                               var->m_name, old->m_context.file, (int)old->m_context.line);
                    retval = false;
                    goto cleanup;
                }
                if (OPTS_OPTION_U32(OPTION_STANDARD) != COMPILER_GMQCC) {
                    delete var;
                    if (ast_istype(old, ast_value))
                        var = proto = (ast_value*)old;
                    else {
                        var = nullptr;
                        goto skipvar;
                    }
                }
            }
        }

        if (noref || parser->noref)
            var->m_flags |= AST_FLAG_NOREF;

        /* Part 2:
         * Create the global/local, and deal with vector types.
         */
        if (!proto) {
            if (var->m_vtype == TYPE_VECTOR)
                isvector = true;
            else if (var->m_vtype == TYPE_FIELD &&
                     var->m_next->m_vtype == TYPE_VECTOR)
                isvector = true;

            if (isvector) {
                if (!create_vector_members(var, me)) {
                    retval = false;
                    goto cleanup;
                }
            }

            if (!localblock) {
                /* deal with global variables, fields, functions */
                if (!nofields && var->m_vtype == TYPE_FIELD && parser->tok != '=') {
                    var->m_isfield = true;
                    parser->fields.push_back(var);
                    util_htset(parser->htfields, var->m_name.c_str(), var);
                    if (isvector) {
                        for (i = 0; i < 3; ++i) {
                            parser->fields.push_back(me[i]);
                            util_htset(parser->htfields, me[i]->m_name.c_str(), me[i]);
                        }
                    }
                }
                else {
                    if (!(var->m_flags & AST_FLAG_ALIAS)) {
                        parser_addglobal(parser, var->m_name, var);
                        if (isvector) {
                            for (i = 0; i < 3; ++i) {
                                parser_addglobal(parser, me[i]->m_name.c_str(), me[i]);
                            }
                        }
                    } else {
                        ast_expression *find  = parser_find_global(parser, var->m_desc);

                        if (!find) {
                            compile_error(parser_ctx(parser), "undeclared variable `%s` for alias `%s`", var->m_desc, var->m_name);
                            return false;
                        }

                        if (!var->compareType(*find)) {
                            char ty1[1024];
                            char ty2[1024];

                            ast_type_to_string(find, ty1, sizeof(ty1));
                            ast_type_to_string(var,  ty2, sizeof(ty2));

                            compile_error(parser_ctx(parser), "incompatible types `%s` and `%s` for alias `%s`",
                                ty1, ty2, var->m_name
                            );
                            return false;
                        }

                        util_htset(parser->aliases, var->m_name.c_str(), find);

                        /* generate aliases for vector components */
                        if (isvector) {
                            char *buffer[3];

                            util_asprintf(&buffer[0], "%s_x", var->m_desc.c_str());
                            util_asprintf(&buffer[1], "%s_y", var->m_desc.c_str());
                            util_asprintf(&buffer[2], "%s_z", var->m_desc.c_str());

                            util_htset(parser->aliases, me[0]->m_name.c_str(), parser_find_global(parser, buffer[0]));
                            util_htset(parser->aliases, me[1]->m_name.c_str(), parser_find_global(parser, buffer[1]));
                            util_htset(parser->aliases, me[2]->m_name.c_str(), parser_find_global(parser, buffer[2]));

                            mem_d(buffer[0]);
                            mem_d(buffer[1]);
                            mem_d(buffer[2]);
                        }
                    }
                }
            } else {
                if (is_static) {
                    // a static adds itself to be generated like any other global
                    // but is added to the local namespace instead
                    std::string defname;
                    size_t  prefix_len;
                    size_t  sn, sn_size;

                    defname = parser->function->m_name;
                    defname.append(2, ':');

                    // remember the length up to here
                    prefix_len = defname.length();

                    // Add it to the local scope
                    util_htset(parser->variables.back(), var->m_name.c_str(), (void*)var);

                    // now rename the global
                    defname.append(var->m_name);
                    // if a variable of that name already existed, add the
                    // counter value.
                    // The counter is incremented either way.
                    sn_size = parser->function->m_static_names.size();
                    for (sn = 0; sn != sn_size; ++sn) {
                        if (parser->function->m_static_names[sn] == var->m_name.c_str())
                            break;
                    }
                    if (sn != sn_size) {
                        char *num = nullptr;
                        int   len = util_asprintf(&num, "#%u", parser->function->m_static_count);
                        defname.append(num, 0, len);
                        mem_d(num);
                    }
                    else
                        parser->function->m_static_names.emplace_back(var->m_name);
                    parser->function->m_static_count++;
                    var->m_name = defname;

                    // push it to the to-be-generated globals
                    parser->globals.push_back(var);

                    // same game for the vector members
                    if (isvector) {
                        defname.erase(prefix_len);
                        for (i = 0; i < 3; ++i) {
                            util_htset(parser->variables.back(), me[i]->m_name.c_str(), (void*)(me[i]));
                            me[i]->m_name = defname + me[i]->m_name;
                            parser->globals.push_back(me[i]);
                        }
                    }
                } else {
                    localblock->m_locals.push_back(var);
                    parser_addlocal(parser, var->m_name, var);
                    if (isvector) {
                        for (i = 0; i < 3; ++i) {
                            parser_addlocal(parser, me[i]->m_name, me[i]);
                            localblock->collect(me[i]);
                        }
                    }
                }
            }
        }
        memcpy(last_me, me, sizeof(me));
        me[0] = me[1] = me[2] = nullptr;
        cleanvar = false;
        /* Part 2.2
         * deal with arrays
         */
        if (var->m_vtype == TYPE_ARRAY) {
            if (var->m_count != (size_t)-1) {
                if (!create_array_accessors(parser, var))
                    goto cleanup;
            }
        }
        else if (!localblock && !nofields &&
                 var->m_vtype == TYPE_FIELD &&
                 var->m_next->m_vtype == TYPE_ARRAY)
        {
            char name[1024];
            ast_expression *telem;
            ast_value      *tfield;
            ast_value      *array = (ast_value*)var->m_next;

            if (!ast_istype(var->m_next, ast_value)) {
                parseerror(parser, "internal error: field element type must be an ast_value");
                goto cleanup;
            }

            util_snprintf(name, sizeof(name), "%s##SETF", var->m_name.c_str());
            if (!parser_create_array_field_setter(parser, array, name))
                goto cleanup;

            telem = new ast_expression(ast_copy_type, var->m_context, *array->m_next);
            tfield = new ast_value(var->m_context, "<.type>", TYPE_FIELD);
            tfield->m_next = telem;
            util_snprintf(name, sizeof(name), "%s##GETFP", var->m_name.c_str());
            if (!parser_create_array_getter(parser, array, tfield, name)) {
                delete tfield;
                goto cleanup;
            }
            delete tfield;
        }

skipvar:
        if (parser->tok == ';') {
            delete basetype;
            if (!parser_next(parser)) {
                parseerror(parser, "error after variable declaration");
                return false;
            }
            return true;
        }

        if (parser->tok == ',')
            goto another;

        /*
        if (!var || (!localblock && !nofields && basetype->m_vtype == TYPE_FIELD)) {
        */
        if (!var) {
            parseerror(parser, "missing comma or semicolon while parsing variables");
            break;
        }

        if (localblock && OPTS_OPTION_U32(OPTION_STANDARD) == COMPILER_QCC) {
            if (parsewarning(parser, WARN_LOCAL_CONSTANTS,
                             "initializing expression turns variable `%s` into a constant in this standard",
                             var->m_name) )
            {
                break;
            }
        }

        if (parser->tok != '{' || var->m_vtype != TYPE_FUNCTION) {
            if (parser->tok != '=') {
                parseerror(parser, "missing semicolon or initializer, got: `%s`", parser_tokval(parser));
                break;
            }

            if (!parser_next(parser)) {
                parseerror(parser, "error parsing initializer");
                break;
            }
        }
        else if (OPTS_OPTION_U32(OPTION_STANDARD) == COMPILER_QCC) {
            parseerror(parser, "expected '=' before function body in this standard");
        }

        if (parser->tok == '#') {
            ast_function *func   = nullptr;
            ast_value    *number = nullptr;
            float         fractional;
            float         integral;
            int           builtin_num;

            if (localblock) {
                parseerror(parser, "cannot declare builtins within functions");
                break;
            }
            if (var->m_vtype != TYPE_FUNCTION) {
                parseerror(parser, "unexpected builtin number, '%s' is not a function", var->m_name);
                break;
            }
            if (!parser_next(parser)) {
                parseerror(parser, "expected builtin number");
                break;
            }

            if (OPTS_FLAG(EXPRESSIONS_FOR_BUILTINS)) {
                number = (ast_value*)parse_expression_leave(parser, true, false, false);
                if (!number) {
                    parseerror(parser, "builtin number expected");
                    break;
                }
                if (!ast_istype(number, ast_value) || !number->m_hasvalue || number->m_cvq != CV_CONST)
                {
                    ast_unref(number);
                    parseerror(parser, "builtin number must be a compile time constant");
                    break;
                }
                if (number->m_vtype == TYPE_INTEGER)
                    builtin_num = number->m_constval.vint;
                else if (number->m_vtype == TYPE_FLOAT)
                    builtin_num = number->m_constval.vfloat;
                else {
                    ast_unref(number);
                    parseerror(parser, "builtin number must be an integer constant");
                    break;
                }
                ast_unref(number);

                fractional = modff(builtin_num, &integral);
                if (builtin_num < 0 || fractional != 0) {
                    parseerror(parser, "builtin number must be an integer greater than zero");
                    break;
                }

                /* we only want the integral part anyways */
                builtin_num = integral;
            } else if (parser->tok == TOKEN_INTCONST) {
                builtin_num = parser_token(parser)->constval.i;
            } else {
                parseerror(parser, "builtin number must be a compile time constant");
                break;
            }

            if (var->m_hasvalue) {
                (void)!parsewarning(parser, WARN_DOUBLE_DECLARATION,
                                    "builtin `%s` has already been defined\n"
                                    " -> previous declaration here: %s:%i",
                                    var->m_name, var->m_context.file, (int)var->m_context.line);
            }
            else
            {
                func = ast_function::make(var->m_context, var->m_name, var);
                if (!func) {
                    parseerror(parser, "failed to allocate function for `%s`", var->m_name);
                    break;
                }
                parser->functions.push_back(func);

                func->m_builtin = -builtin_num-1;
            }

            if (OPTS_FLAG(EXPRESSIONS_FOR_BUILTINS)
                    ? (parser->tok != ',' && parser->tok != ';')
                    : (!parser_next(parser)))
            {
                parseerror(parser, "expected comma or semicolon");
                delete func;
                var->m_constval.vfunc = nullptr;
                break;
            }
        }
        else if (var->m_vtype == TYPE_ARRAY && parser->tok == '{')
        {
            if (localblock) {
                /* Note that fteqcc and most others don't even *have*
                 * local arrays, so this is not a high priority.
                 */
                parseerror(parser, "TODO: initializers for local arrays");
                break;
            }

            var->m_hasvalue = true;
            if (!parse_array(parser, var))
                break;
        }
        else if (var->m_vtype == TYPE_FUNCTION && (parser->tok == '{' || parser->tok == '['))
        {
            if (localblock) {
                parseerror(parser, "cannot declare functions within functions");
                break;
            }

            if (proto)
                proto->m_context = parser_ctx(parser);

            if (!parse_function_body(parser, var))
                break;
            delete basetype;
            for (auto &it : parser->gotos)
                parseerror(parser, "undefined label: `%s`", it->m_name);
            parser->gotos.clear();
            parser->labels.clear();
            return true;
        } else {
            ast_expression *cexp;
            ast_value      *cval;
            bool            folded_const = false;

            cexp = parse_expression_leave(parser, true, false, false);
            if (!cexp)
                break;
            cval = ast_istype(cexp, ast_value) ? (ast_value*)cexp : nullptr;

            /* deal with foldable constants: */
            if (localblock &&
                var->m_cvq == CV_CONST && cval && cval->m_hasvalue && cval->m_cvq == CV_CONST && !cval->m_isfield)
            {
                /* remove it from the current locals */
                if (isvector) {
                    for (i = 0; i < 3; ++i) {
                        parser->_locals.pop_back();
                        localblock->m_collect.pop_back();
                    }
                }
                /* do sanity checking, this function really needs refactoring */
                if (parser->_locals.back() != var)
                    parseerror(parser, "internal error: unexpected change in local variable handling");
                else
                    parser->_locals.pop_back();
                if (localblock->m_locals.back() != var)
                    parseerror(parser, "internal error: unexpected change in local variable handling (2)");
                else
                    localblock->m_locals.pop_back();
                /* push it to the to-be-generated globals */
                parser->globals.push_back(var);
                if (isvector)
                    for (i = 0; i < 3; ++i)
                        parser->globals.push_back(last_me[i]);
                folded_const = true;
            }

            if (folded_const || !localblock || is_static) {
                if (cval != parser->nil &&
                    (!cval || ((!cval->m_hasvalue || cval->m_cvq != CV_CONST) && !cval->m_isfield))
                   )
                {
                    parseerror(parser, "initializer is non constant");
                }
                else
                {
                    if (!is_static &&
                        !OPTS_FLAG(INITIALIZED_NONCONSTANTS) &&
                        qualifier != CV_VAR)
                    {
                        var->m_cvq = CV_CONST;
                    }
                    if (cval == parser->nil)
                    {
                        var->m_flags |= AST_FLAG_INITIALIZED;
                        var->m_flags |= AST_FLAG_NOREF;
                    }
                    else
                    {
                        var->m_hasvalue = true;
                        if (cval->m_vtype == TYPE_STRING)
                            var->m_constval.vstring = parser_strdup(cval->m_constval.vstring);
                        else if (cval->m_vtype == TYPE_FIELD)
                            var->m_constval.vfield = cval;
                        else
                            memcpy(&var->m_constval, &cval->m_constval, sizeof(var->m_constval));
                        ast_unref(cval);
                    }
                }
            } else {
                int cvq;
                shunt sy;
                cvq = var->m_cvq;
                var->m_cvq = CV_NONE;
                sy.out.push_back(syexp(var->m_context, var));
                sy.out.push_back(syexp(cexp->m_context, cexp));
                sy.ops.push_back(syop(var->m_context, parser->assign_op));
                if (!parser_sy_apply_operator(parser, &sy))
                    ast_unref(cexp);
                else {
                    if (sy.out.size() != 1 && sy.ops.size() != 0)
                        parseerror(parser, "internal error: leaked operands");
                    if (!localblock->addExpr(sy.out[0].out))
                        break;
                }
                var->m_cvq = cvq;
            }
            /* a constant initialized to an inexact value should be marked inexact:
             * const float x = <inexact>; should propagate the inexact flag
             */
            if (var->m_cvq == CV_CONST && var->m_vtype == TYPE_FLOAT) {
                if (cval && cval->m_hasvalue && cval->m_cvq == CV_CONST)
                    var->m_inexact = cval->m_inexact;
            }
        }

another:
        if (parser->tok == ',') {
            if (!parser_next(parser)) {
                parseerror(parser, "expected another variable");
                break;
            }

            if (parser->tok != TOKEN_IDENT) {
                parseerror(parser, "expected another variable");
                break;
            }
            var = new ast_value(ast_copy_type, *basetype);
            cleanvar = true;
            var->m_name = parser_tokval(parser);
            if (!parser_next(parser)) {
                parseerror(parser, "error parsing variable declaration");
                break;
            }
            continue;
        }

        if (parser->tok != ';') {
            parseerror(parser, "missing semicolon after variables");
            break;
        }

        if (!parser_next(parser)) {
            parseerror(parser, "parse error after variable declaration");
            break;
        }

        delete basetype;
        return true;
    }

    if (cleanvar && var)
        delete var;
    delete basetype;
    return false;

cleanup:
    delete basetype;
    if (cleanvar && var)
        delete var;
    delete me[0];
    delete me[1];
    delete me[2];
    return retval;
}

static bool parser_global_statement(parser_t *parser)
{
    int        cvq       = CV_WRONG;
    bool       noref     = false;
    bool       is_static = false;
    uint32_t   qflags    = 0;
    ast_value *istype    = nullptr;
    char      *vstring   = nullptr;

    if (parser->tok == TOKEN_IDENT)
        istype = parser_find_typedef(parser, parser_tokval(parser), 0);

    if (istype || parser->tok == TOKEN_TYPENAME || parser->tok == '.' || parser->tok == TOKEN_DOTS)
    {
        return parse_variable(parser, nullptr, false, CV_NONE, istype, false, false, 0, nullptr);
    }
    else if (parse_qualifiers(parser, false, &cvq, &noref, &is_static, &qflags, &vstring))
    {
        if (cvq == CV_WRONG)
            return false;
        return parse_variable(parser, nullptr, false, cvq, nullptr, noref, is_static, qflags, vstring);
    }
    else if (parser->tok == TOKEN_IDENT && !strcmp(parser_tokval(parser), "enum"))
    {
        return parse_enum(parser);
    }
    else if (parser->tok == TOKEN_KEYWORD)
    {
        if (!strcmp(parser_tokval(parser), "typedef")) {
            if (!parser_next(parser)) {
                parseerror(parser, "expected type definition after 'typedef'");
                return false;
            }
            return parse_typedef(parser);
        }
        parseerror(parser, "unrecognized keyword `%s`", parser_tokval(parser));
        return false;
    }
    else if (parser->tok == '#')
    {
        return parse_pragma(parser);
    }
    else if (parser->tok == '$')
    {
        if (!parser_next(parser)) {
            parseerror(parser, "parse error");
            return false;
        }
    }
    else
    {
        parseerror(parser, "unexpected token: `%s`", parser->lex->tok.value);
        return false;
    }
    return true;
}

static uint16_t progdefs_crc_sum(uint16_t old, const char *str)
{
    return util_crc16(old, str, strlen(str));
}

static void progdefs_crc_file(const char *str)
{
    /* write to progdefs.h here */
    (void)str;
}

static uint16_t progdefs_crc_both(uint16_t old, const char *str)
{
    old = progdefs_crc_sum(old, str);
    progdefs_crc_file(str);
    return old;
}

static void generate_checksum(parser_t *parser, ir_builder *ir)
{
    uint16_t   crc = 0xFFFF;
    size_t     i;
    ast_value *value;

    crc = progdefs_crc_both(crc, "\n/* file generated by qcc, do not modify */\n\ntypedef struct\n{");
    crc = progdefs_crc_sum(crc, "\tint\tpad[28];\n");
    /*
    progdefs_crc_file("\tint\tpad;\n");
    progdefs_crc_file("\tint\tofs_return[3];\n");
    progdefs_crc_file("\tint\tofs_parm0[3];\n");
    progdefs_crc_file("\tint\tofs_parm1[3];\n");
    progdefs_crc_file("\tint\tofs_parm2[3];\n");
    progdefs_crc_file("\tint\tofs_parm3[3];\n");
    progdefs_crc_file("\tint\tofs_parm4[3];\n");
    progdefs_crc_file("\tint\tofs_parm5[3];\n");
    progdefs_crc_file("\tint\tofs_parm6[3];\n");
    progdefs_crc_file("\tint\tofs_parm7[3];\n");
    */
    for (i = 0; i < parser->crc_globals; ++i) {
        if (!ast_istype(parser->globals[i], ast_value))
            continue;
        value = (ast_value*)(parser->globals[i]);
        switch (value->m_vtype) {
            case TYPE_FLOAT:    crc = progdefs_crc_both(crc, "\tfloat\t"); break;
            case TYPE_VECTOR:   crc = progdefs_crc_both(crc, "\tvec3_t\t"); break;
            case TYPE_STRING:   crc = progdefs_crc_both(crc, "\tstring_t\t"); break;
            case TYPE_FUNCTION: crc = progdefs_crc_both(crc, "\tfunc_t\t"); break;
            default:
                crc = progdefs_crc_both(crc, "\tint\t");
                break;
        }
        crc = progdefs_crc_both(crc, value->m_name.c_str());
        crc = progdefs_crc_both(crc, ";\n");
    }
    crc = progdefs_crc_both(crc, "} globalvars_t;\n\ntypedef struct\n{\n");
    for (i = 0; i < parser->crc_fields; ++i) {
        if (!ast_istype(parser->fields[i], ast_value))
            continue;
        value = (ast_value*)(parser->fields[i]);
        switch (value->m_next->m_vtype) {
            case TYPE_FLOAT:    crc = progdefs_crc_both(crc, "\tfloat\t"); break;
            case TYPE_VECTOR:   crc = progdefs_crc_both(crc, "\tvec3_t\t"); break;
            case TYPE_STRING:   crc = progdefs_crc_both(crc, "\tstring_t\t"); break;
            case TYPE_FUNCTION: crc = progdefs_crc_both(crc, "\tfunc_t\t"); break;
            default:
                crc = progdefs_crc_both(crc, "\tint\t");
                break;
        }
        crc = progdefs_crc_both(crc, value->m_name.c_str());
        crc = progdefs_crc_both(crc, ";\n");
    }
    crc = progdefs_crc_both(crc, "} entvars_t;\n\n");
    ir->m_code->crc = crc;
}

parser_t::parser_t()
    : lex(nullptr)
    , tok(0)
    , ast_cleaned(false)
    , translated(0)
    , crc_globals(0)
    , crc_fields(0)
    , function(nullptr)
    , aliases(util_htnew(PARSER_HT_SIZE))
    , htfields(util_htnew(PARSER_HT_SIZE))
    , htglobals(util_htnew(PARSER_HT_SIZE))
    , assign_op(nullptr)
    , noref(false)
    , max_param_count(1)
    // finish initializing the rest of the parser before initializing
    // m_fold and m_intrin with the parser passed along
    , m_fold()
    , m_intrin()
{
    variables.push_back(htfields);
    variables.push_back(htglobals);
    typedefs.push_back(util_htnew(TYPEDEF_HT_SIZE));
    _blocktypedefs.push_back(0);

    lex_ctx_t empty_ctx;
    empty_ctx.file   = "<internal>";
    empty_ctx.line   = 0;
    empty_ctx.column = 0;
    nil = new ast_value(empty_ctx, "nil", TYPE_NIL);
    nil->m_cvq = CV_CONST;
    if (OPTS_FLAG(UNTYPED_NIL))
        util_htset(htglobals, "nil", (void*)nil);

    const_vec[0] = new ast_value(empty_ctx, "<vector.x>", TYPE_NOEXPR);
    const_vec[1] = new ast_value(empty_ctx, "<vector.y>", TYPE_NOEXPR);
    const_vec[2] = new ast_value(empty_ctx, "<vector.z>", TYPE_NOEXPR);

    if (OPTS_OPTION_BOOL(OPTION_ADD_INFO)) {
        reserved_version = new ast_value(empty_ctx, "reserved:version", TYPE_STRING);
        reserved_version->m_cvq = CV_CONST;
        reserved_version->m_hasvalue = true;
        reserved_version->m_flags |= AST_FLAG_INCLUDE_DEF;
        reserved_version->m_flags |= AST_FLAG_NOREF;
        reserved_version->m_constval.vstring = util_strdup(GMQCC_FULL_VERSION_STRING);
    } else {
        reserved_version = nullptr;
    }

    m_fold = fold(this);
    m_intrin = intrin(this);
}

parser_t::~parser_t()
{
    remove_ast();
}

parser_t *parser_create()
{
    parser_t *parser;
    size_t i;

    parser = new parser_t;
    if (!parser)
        return nullptr;

    for (i = 0; i < operator_count; ++i) {
        if (operators[i].id == opid1('=')) {
            parser->assign_op = operators+i;
            break;
        }
    }
    if (!parser->assign_op) {
        con_err("internal error: initializing parser: failed to find assign operator\n");
        delete parser;
        return nullptr;
    }

    return parser;
}

static bool parser_compile(parser_t *parser)
{
    /* initial lexer/parser state */
    parser->lex->flags.noops = true;

    if (parser_next(parser))
    {
        while (parser->tok != TOKEN_EOF && parser->tok < TOKEN_ERROR)
        {
            if (!parser_global_statement(parser)) {
                if (parser->tok == TOKEN_EOF)
                    parseerror(parser, "unexpected end of file");
                else if (compile_errors)
                    parseerror(parser, "there have been errors, bailing out");
                lex_close(parser->lex);
                parser->lex = nullptr;
                return false;
            }
        }
    } else {
        parseerror(parser, "parse error");
        lex_close(parser->lex);
        parser->lex = nullptr;
        return false;
    }

    lex_close(parser->lex);
    parser->lex = nullptr;

    return !compile_errors;
}

bool parser_compile_file(parser_t *parser, const char *filename)
{
    parser->lex = lex_open(filename);
    if (!parser->lex) {
        con_err("failed to open file \"%s\"\n", filename);
        return false;
    }
    return parser_compile(parser);
}

bool parser_compile_string(parser_t *parser, const char *name, const char *str, size_t len)
{
    parser->lex = lex_open_string(str, len, name);
    if (!parser->lex) {
        con_err("failed to create lexer for string \"%s\"\n", name);
        return false;
    }
    return parser_compile(parser);
}

void parser_t::remove_ast()
{
    if (ast_cleaned)
        return;
    ast_cleaned = true;
    for (auto &it : accessors) {
        delete it->m_constval.vfunc;
        it->m_constval.vfunc = nullptr;
        delete it;
    }
    for (auto &it : functions) delete it;
    for (auto &it : globals) delete it;
    for (auto &it : fields) delete it;

    for (auto &it : variables) util_htdel(it);
    variables.clear();
    _blocklocals.clear();
    _locals.clear();

    _typedefs.clear();
    for (auto &it : typedefs) util_htdel(it);
    typedefs.clear();
    _blocktypedefs.clear();

    _block_ctx.clear();

    delete nil;

    delete const_vec[0];
    delete const_vec[1];
    delete const_vec[2];

    if (reserved_version)
        delete reserved_version;

    util_htdel(aliases);
}

static bool parser_set_coverage_func(parser_t *parser, ir_builder *ir) {
    ast_expression *expr;
    ast_value      *cov;
    ast_function   *func;

    if (!OPTS_OPTION_BOOL(OPTION_COVERAGE))
        return true;

    func = nullptr;
    for (auto &it : parser->functions) {
        if (it->m_name == "coverage") {
            func = it;
            break;
        }
    }
    if (!func) {
        if (OPTS_OPTION_BOOL(OPTION_COVERAGE)) {
            con_out("coverage support requested but no coverage() builtin declared\n");
            delete ir;
            return false;
        }
        return true;
    }

    cov  = func->m_function_type;
    expr = cov;

    if (expr->m_vtype != TYPE_FUNCTION || expr->m_type_params.size()) {
        char ty[1024];
        ast_type_to_string(expr, ty, sizeof(ty));
        con_out("invalid type for coverage(): %s\n", ty);
        delete ir;
        return false;
    }

    ir->m_coverage_func = func->m_ir_func->m_value;
    return true;
}

bool parser_finish(parser_t *parser, const char *output)
{
    ir_builder *ir;
    bool retval = true;

    if (compile_errors) {
        con_out("*** there were compile errors\n");
        return false;
    }

    ir = new ir_builder("gmqcc_out");
    if (!ir) {
        con_out("failed to allocate builder\n");
        return false;
    }

    for (auto &it : parser->fields) {
        bool hasvalue;
        if (!ast_istype(it, ast_value))
            continue;
        ast_value *field = (ast_value*)it;
        hasvalue = field->m_hasvalue;
        field->m_hasvalue = false;
        if (!reinterpret_cast<ast_value*>(field)->generateGlobal(ir, true)) {
            con_out("failed to generate field %s\n", field->m_name.c_str());
            delete ir;
            return false;
        }
        if (hasvalue) {
            ir_value *ifld;
            ast_expression *subtype;
            field->m_hasvalue = true;
            subtype = field->m_next;
            ifld = ir->createField(field->m_name, subtype->m_vtype);
            if (subtype->m_vtype == TYPE_FIELD)
                ifld->m_fieldtype = subtype->m_next->m_vtype;
            else if (subtype->m_vtype == TYPE_FUNCTION)
                ifld->m_outtype = subtype->m_next->m_vtype;
            (void)!field->m_ir_v->setField(ifld);
        }
    }
    for (auto &it : parser->globals) {
        ast_value *asvalue;
        if (!ast_istype(it, ast_value))
            continue;
        asvalue = (ast_value*)it;
        if (!(asvalue->m_flags & AST_FLAG_NOREF) && asvalue->m_cvq != CV_CONST && asvalue->m_vtype != TYPE_FUNCTION) {
            retval = retval && !compile_warning(asvalue->m_context, WARN_UNUSED_VARIABLE,
                                                "unused global: `%s`", asvalue->m_name);
        }
        if (!asvalue->generateGlobal(ir, false)) {
            con_out("failed to generate global %s\n", asvalue->m_name.c_str());
            delete ir;
            return false;
        }
    }
    /* Build function vararg accessor ast tree now before generating
     * immediates, because the accessors may add new immediates
     */
    for (auto &f : parser->functions) {
        if (f->m_varargs) {
            if (parser->max_param_count > f->m_function_type->m_type_params.size()) {
                f->m_varargs->m_count = parser->max_param_count - f->m_function_type->m_type_params.size();
                if (!parser_create_array_setter_impl(parser, f->m_varargs.get())) {
                    con_out("failed to generate vararg setter for %s\n", f->m_name.c_str());
                    delete ir;
                    return false;
                }
                if (!parser_create_array_getter_impl(parser, f->m_varargs.get())) {
                    con_out("failed to generate vararg getter for %s\n", f->m_name.c_str());
                    delete ir;
                    return false;
                }
            } else {
                f->m_varargs = nullptr;
            }
        }
    }
    /* Now we can generate immediates */
    if (!parser->m_fold.generate(ir))
        return false;

    /* before generating any functions we need to set the coverage_func */
    if (!parser_set_coverage_func(parser, ir))
        return false;
    for (auto &it : parser->globals) {
        if (!ast_istype(it, ast_value))
            continue;
        ast_value *asvalue = (ast_value*)it;
        if (!(asvalue->m_flags & AST_FLAG_INITIALIZED))
        {
            if (asvalue->m_cvq == CV_CONST && !asvalue->m_hasvalue)
                (void)!compile_warning(asvalue->m_context, WARN_UNINITIALIZED_CONSTANT,
                                       "uninitialized constant: `%s`",
                                       asvalue->m_name);
            else if ((asvalue->m_cvq == CV_NONE || asvalue->m_cvq == CV_CONST) && !asvalue->m_hasvalue)
                (void)!compile_warning(asvalue->m_context, WARN_UNINITIALIZED_GLOBAL,
                                       "uninitialized global: `%s`",
                                       asvalue->m_name);
        }
        if (!asvalue->generateAccessors(ir)) {
            delete ir;
            return false;
        }
    }
    for (auto &it : parser->fields) {
        ast_value *asvalue = (ast_value*)it->m_next;
        if (!ast_istype(asvalue, ast_value))
            continue;
        if (asvalue->m_vtype != TYPE_ARRAY)
            continue;
        if (!asvalue->generateAccessors(ir)) {
            delete ir;
            return false;
        }
    }
    if (parser->reserved_version &&
        !parser->reserved_version->generateGlobal(ir, false))
    {
        con_out("failed to generate reserved::version");
        delete ir;
        return false;
    }
    for (auto &f : parser->functions) {
        if (!f->generateFunction(ir)) {
            con_out("failed to generate function %s\n", f->m_name.c_str());
            delete ir;
            return false;
        }
    }

    generate_checksum(parser, ir);

    if (OPTS_OPTION_BOOL(OPTION_DUMP))
        ir->dump(con_out);
    for (auto &it : parser->functions) {
        if (!ir_function_finalize(it->m_ir_func)) {
            con_out("failed to finalize function %s\n", it->m_name.c_str());
            delete ir;
            return false;
        }
    }
    parser->remove_ast();

    auto fnCheckWErrors = [&retval]() {
        if (compile_Werrors) {
            con_out("*** there were warnings treated as errors\n");
            compile_show_werrors();
            retval = false;
        }
    };

    fnCheckWErrors();

    if (retval) {
        if (OPTS_OPTION_BOOL(OPTION_DUMPFIN))
            ir->dump(con_out);

        if (!ir->generate(output)) {
            con_out("*** failed to generate output file\n");
            delete ir;
            return false;
        }

        // ir->generate can generate compiler warnings
        fnCheckWErrors();
    }
    delete ir;
    return retval;
}
