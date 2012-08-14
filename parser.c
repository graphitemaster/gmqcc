#include <stdio.h>
#include <stdarg.h>

#include "gmqcc.h"
#include "lexer.h"

typedef struct {
    char *name;
    ast_expression *var;
} varentry_t;

typedef struct {
    lex_file *lex;
    int      tok;

    MEM_VECTOR_MAKE(varentry_t, globals);
    MEM_VECTOR_MAKE(varentry_t, fields);
    MEM_VECTOR_MAKE(ast_function*, functions);
    MEM_VECTOR_MAKE(ast_value*, imm_float);
    MEM_VECTOR_MAKE(ast_value*, imm_string);
    MEM_VECTOR_MAKE(ast_value*, imm_vector);

    ast_function *function;
    MEM_VECTOR_MAKE(varentry_t, locals);
    size_t blocklocal;

    size_t errors;

    /* TYPE_FIELD -> parser_find_fields is used instead of find_var
     * TODO: TYPE_VECTOR -> x, y and z are accepted in the gmqcc standard
     * anything else: type error
     */
    qcint  memberof;
} parser_t;

MEM_VEC_FUNCTIONS(parser_t, varentry_t, globals)
MEM_VEC_FUNCTIONS(parser_t, varentry_t, fields)
MEM_VEC_FUNCTIONS(parser_t, ast_value*, imm_float)
MEM_VEC_FUNCTIONS(parser_t, ast_value*, imm_string)
MEM_VEC_FUNCTIONS(parser_t, ast_value*, imm_vector)
MEM_VEC_FUNCTIONS(parser_t, varentry_t, locals)
MEM_VEC_FUNCTIONS(parser_t, ast_function*, functions)

static void parser_pop_local(parser_t *parser);
static bool parser_variable(parser_t *parser, ast_block *localblock);
static ast_block* parser_parse_block(parser_t *parser);
static ast_expression* parser_parse_statement_or_block(parser_t *parser);
static ast_expression* parser_expression_leave(parser_t *parser);
static ast_expression* parser_expression(parser_t *parser);

void parseerror(parser_t *parser, const char *fmt, ...)
{
	va_list ap;

	parser->errors++;

	va_start(ap, fmt);
    vprintmsg(LVL_ERROR, parser->lex->tok->ctx.file, parser->lex->tok->ctx.line, "parse error", fmt, ap);
	va_end(ap);

	printf("\n");
}

/* returns true if it counts as an error */
bool GMQCC_WARN parsewarning(parser_t *parser, int warntype, const char *fmt, ...)
{
	va_list ap;
	int lvl = LVL_WARNING;

    if (!OPTS_WARN(warntype))
        return false;

    if (OPTS_WARN(WARN_ERROR)) {
	    parser->errors++;
	    lvl = LVL_ERROR;
	}

	va_start(ap, fmt);
    vprintmsg(lvl, parser->lex->tok->ctx.file, parser->lex->tok->ctx.line, "warning", fmt, ap);
	va_end(ap);

	return OPTS_WARN(WARN_ERROR);
}

bool parser_next(parser_t *parser)
{
    /* lex_do kills the previous token */
    parser->tok = lex_do(parser->lex);
    if (parser->tok == TOKEN_EOF || parser->tok >= TOKEN_ERROR)
        return false;
    return true;
}

/* lift a token out of the parser so it's not destroyed by parser_next */
token *parser_lift(parser_t *parser)
{
    token *tok = parser->lex->tok;
    parser->lex->tok = NULL;
    return tok;
}

#define parser_tokval(p) (p->lex->tok->value)
#define parser_token(p)  (p->lex->tok)
#define parser_ctx(p)    (p->lex->tok->ctx)

ast_value* parser_const_float(parser_t *parser, double d)
{
    size_t i;
    ast_value *out;
    for (i = 0; i < parser->imm_float_count; ++i) {
        if (parser->imm_float[i]->constval.vfloat == d)
            return parser->imm_float[i];
    }
    out = ast_value_new(parser_ctx(parser), "#IMMEDIATE", TYPE_FLOAT);
    out->isconst = true;
    out->constval.vfloat = d;
    if (!parser_t_imm_float_add(parser, out)) {
        ast_value_delete(out);
        return NULL;
    }
    return out;
}

ast_value* parser_const_string(parser_t *parser, const char *str)
{
    size_t i;
    ast_value *out;
    for (i = 0; i < parser->imm_string_count; ++i) {
        if (!strcmp(parser->imm_string[i]->constval.vstring, str))
            return parser->imm_string[i];
    }
    out = ast_value_new(parser_ctx(parser), "#IMMEDIATE", TYPE_STRING);
    out->isconst = true;
    out->constval.vstring = util_strdup(str);
    if (!parser_t_imm_string_add(parser, out)) {
        ast_value_delete(out);
        return NULL;
    }
    return out;
}

ast_value* parser_const_vector(parser_t *parser, vector v)
{
    size_t i;
    ast_value *out;
    for (i = 0; i < parser->imm_vector_count; ++i) {
        if (!memcmp(&parser->imm_vector[i]->constval.vvec, &v, sizeof(v)))
            return parser->imm_vector[i];
    }
    out = ast_value_new(parser_ctx(parser), "#IMMEDIATE", TYPE_VECTOR);
    out->isconst = true;
    out->constval.vvec = v;
    if (!parser_t_imm_vector_add(parser, out)) {
        ast_value_delete(out);
        return NULL;
    }
    return out;
}

ast_expression* parser_find_field(parser_t *parser, const char *name)
{
    size_t i;
    for (i = 0; i < parser->fields_count; ++i) {
        if (!strcmp(parser->fields[i].name, name))
            return parser->fields[i].var;
    }
    return NULL;
}

ast_expression* parser_find_global(parser_t *parser, const char *name)
{
    size_t i;
    for (i = 0; i < parser->globals_count; ++i) {
        if (!strcmp(parser->globals[i].name, name))
            return parser->globals[i].var;
    }
    return NULL;
}

ast_expression* parser_find_local(parser_t *parser, const char *name, size_t upto)
{
    size_t i;
    ast_value *fun;
    for (i = parser->locals_count; i > upto;) {
        --i;
        if (!strcmp(parser->locals[i].name, name))
            return parser->locals[i].var;
    }
    fun = parser->function->vtype;
    for (i = 0; i < fun->expression.params_count; ++i) {
        if (!strcmp(fun->expression.params[i]->name, name))
            return (ast_expression*)(fun->expression.params[i]);
    }
    return NULL;
}

ast_expression* parser_find_var(parser_t *parser, const char *name)
{
    ast_expression *v;
    v         = parser_find_local(parser, name, 0);
    if (!v) v = parser_find_global(parser, name);
    return v;
}

typedef struct {
    MEM_VECTOR_MAKE(ast_value*, p);
} paramlist_t;
MEM_VEC_FUNCTIONS(paramlist_t, ast_value*, p)

static ast_value *parser_parse_type(parser_t *parser, int basetype, bool *isfunc)
{
    paramlist_t params;
    ast_value *var;
    lex_ctx   ctx = parser_ctx(parser);
    int vtype = basetype;
    int temptype;
    size_t i;

    MEM_VECTOR_INIT(&params, p);

    *isfunc = false;

    if (parser->tok == '(') {
        *isfunc = true;
        while (true) {
            ast_value *param;
            bool dummy;

            if (!parser_next(parser))
                goto on_error;

            if (parser->tok == ')')
                break;

            temptype = parser_token(parser)->constval.t;
            if (!parser_next(parser))
                goto on_error;

            param = parser_parse_type(parser, temptype, &dummy);
            (void)dummy;

            if (!param)
                goto on_error;

            if (parser->tok == TOKEN_IDENT) {
                /* named parameter */
                if (!ast_value_set_name(param, parser_tokval(parser)))
                    goto on_error;
                if (!parser_next(parser))
                    goto on_error;
            }

            if (!paramlist_t_p_add(&params, param)) {
                parseerror(parser, "Out of memory while parsing typename");
                goto on_error;
            }

            if (parser->tok == ',')
                continue;
            if (parser->tok == ')')
                break;
            parseerror(parser, "Unexpected token");
            goto on_error;
        }
        if (!parser_next(parser))
            goto on_error;
    }

    var = ast_value_new(ctx, "<unnamed>", vtype);
    if (!var)
        goto on_error;
    MEM_VECTOR_MOVE(&params, p, &var->expression, params);
    return var;
on_error:
    for (i = 0; i < params.p_count; ++i)
        ast_value_delete(params.p[i]);
    MEM_VECTOR_CLEAR(&params, p);
    return NULL;
}

typedef struct
{
    size_t etype; /* 0 = expression, others are operators */
    int             paren;
    size_t          off;
    ast_expression *out;
    ast_block      *block; /* for commas and function calls */
    lex_ctx ctx;
} sy_elem;
typedef struct
{
    MEM_VECTOR_MAKE(sy_elem, out);
    MEM_VECTOR_MAKE(sy_elem, ops);
} shunt;
MEM_VEC_FUNCTIONS(shunt, sy_elem, out)
MEM_VEC_FUNCTIONS(shunt, sy_elem, ops)

static sy_elem syexp(lex_ctx ctx, ast_expression *v) {
    sy_elem e;
    e.etype = 0;
    e.out   = v;
    e.block = NULL;
    e.ctx   = ctx;
    e.paren = 0;
    return e;
}

static sy_elem syblock(lex_ctx ctx, ast_block *v) {
    sy_elem e;
    e.etype = 0;
    e.out   = (ast_expression*)v;
    e.block = v;
    e.ctx   = ctx;
    e.paren = 0;
    return e;
}

static sy_elem syop(lex_ctx ctx, const oper_info *op) {
    sy_elem e;
    e.etype = 1 + (op - operators);
    e.out   = NULL;
    e.block = NULL;
    e.ctx   = ctx;
    e.paren = 0;
    return e;
}

static sy_elem syparen(lex_ctx ctx, int p, size_t off) {
    sy_elem e;
    e.etype = 0;
    e.off   = off;
    e.out   = NULL;
    e.block = NULL;
    e.ctx   = ctx;
    e.paren = p;
    return e;
}

#ifdef DEBUGSHUNT
# define DEBUGSHUNTDO(x) x
#else
# define DEBUGSHUNTDO(x)
#endif

static bool parser_sy_pop(parser_t *parser, shunt *sy)
{
    const oper_info *op;
    lex_ctx ctx;
    ast_expression *out = NULL;
    ast_expression *exprs[3];
    ast_block      *blocks[3];
    size_t i, assignop;
    qcint  generated_op = 0;

    if (!sy->ops_count) {
        parseerror(parser, "internal error: missing operator");
        return false;
    }

    if (sy->ops[sy->ops_count-1].paren) {
        parseerror(parser, "unmatched parenthesis");
        return false;
    }

    op = &operators[sy->ops[sy->ops_count-1].etype - 1];
    ctx = sy->ops[sy->ops_count-1].ctx;

    DEBUGSHUNTDO(printf("apply %s\n", op->op));

    if (sy->out_count < op->operands) {
        parseerror(parser, "internal error: not enough operands: %i", sy->out_count);
        return false;
    }

    sy->ops_count--;

    sy->out_count -= op->operands;
    for (i = 0; i < op->operands; ++i) {
        exprs[i]  = sy->out[sy->out_count+i].out;
        blocks[i] = sy->out[sy->out_count+i].block;
    }

    if (blocks[0] && !blocks[0]->exprs_count && op->id != opid1(',')) {
        parseerror(parser, "internal error: operator cannot be applied on empty blocks");
        return false;
    }

#define NotSameType(T) \
             (exprs[0]->expression.vtype != exprs[1]->expression.vtype || \
              exprs[0]->expression.vtype != T)
    switch (op->id)
    {
        default:
            parseerror(parser, "internal error: unhandled operand");
            return false;

        case opid1('.'):
            if (exprs[0]->expression.vtype == TYPE_ENTITY) {
                if (exprs[1]->expression.vtype != TYPE_FIELD) {
                    parseerror(parser, "type error: right hand of member-operand should be an entity-field");
                    return false;
                }
                out = (ast_expression*)ast_entfield_new(ctx, exprs[0], exprs[1]);
            }
            else if (exprs[0]->expression.vtype == TYPE_VECTOR) {
                parseerror(parser, "internal error: vector access is not supposed to be handled at this point");
                return false;
            }
            else {
                parseerror(parser, "type error: member-of operator on something that is not an entity or vector");
                return false;
            }
            break;

        case opid1(','):
            if (blocks[0]) {
                if (!ast_block_exprs_add(blocks[0], exprs[1]))
                    return false;
            } else {
                blocks[0] = ast_block_new(ctx);
                if (!ast_block_exprs_add(blocks[0], exprs[0]) ||
                    !ast_block_exprs_add(blocks[0], exprs[1]))
                {
                    return false;
                }
            }
            if (!ast_block_set_type(blocks[0], exprs[1]))
                return false;

            sy->out[sy->out_count++] = syblock(ctx, blocks[0]);
            return true;

        case opid1('+'):
            if (exprs[0]->expression.vtype != exprs[1]->expression.vtype) {
                parseerror(parser, "invalid types used in expression: cannot add type %s and %s",
                           type_name[exprs[0]->expression.vtype],
                           type_name[exprs[1]->expression.vtype]);
                return false;
            }
            if (exprs[0]->expression.vtype != TYPE_VECTOR && exprs[0]->expression.vtype != TYPE_FLOAT) {
                parseerror(parser, "invalid types used in expression: cannot add type %s and %s",
                           type_name[exprs[0]->expression.vtype],
                           type_name[exprs[1]->expression.vtype]);
                return false;
            }
            switch (exprs[0]->expression.vtype) {
                case TYPE_FLOAT:
                    out = (ast_expression*)ast_binary_new(ctx, INSTR_ADD_F, exprs[0], exprs[1]);
                    break;
                case TYPE_VECTOR:
                    out = (ast_expression*)ast_binary_new(ctx, INSTR_ADD_V, exprs[0], exprs[1]);
                    break;
                default:
                    parseerror(parser, "invalid types used in expression: cannot add type %s and %s",
                               type_name[exprs[0]->expression.vtype],
                               type_name[exprs[1]->expression.vtype]);
                    return false;
            };
            break;
        case opid1('-'):
            if (exprs[0]->expression.vtype != exprs[1]->expression.vtype) {
                parseerror(parser, "invalid types used in expression: cannot subtract type %s from %s",
                           type_name[exprs[1]->expression.vtype],
                           type_name[exprs[0]->expression.vtype]);
                return false;
            }
            if (exprs[0]->expression.vtype != TYPE_VECTOR && exprs[0]->expression.vtype != TYPE_FLOAT) {
                parseerror(parser, "invalid types used in expression: cannot subtract type %s from %s",
                           type_name[exprs[0]->expression.vtype],
                           type_name[exprs[1]->expression.vtype]);
                return false;
            }
            switch (exprs[0]->expression.vtype) {
                case TYPE_FLOAT:
                    out = (ast_expression*)ast_binary_new(ctx, INSTR_SUB_F, exprs[0], exprs[1]);
                    break;
                case TYPE_VECTOR:
                    out = (ast_expression*)ast_binary_new(ctx, INSTR_SUB_V, exprs[0], exprs[1]);
                    break;
                default:
                    parseerror(parser, "invalid types used in expression: cannot subtract type %s from %s",
                               type_name[exprs[1]->expression.vtype],
                               type_name[exprs[0]->expression.vtype]);
                    return false;
            };
            break;
        case opid1('*'):
            if (exprs[0]->expression.vtype != exprs[1]->expression.vtype &&
                exprs[0]->expression.vtype != TYPE_VECTOR &&
                exprs[0]->expression.vtype != TYPE_FLOAT &&
                exprs[1]->expression.vtype != TYPE_VECTOR &&
                exprs[1]->expression.vtype != TYPE_FLOAT)
            {
                parseerror(parser, "invalid types used in expression: cannot multiply types %s and %s",
                           type_name[exprs[1]->expression.vtype],
                           type_name[exprs[0]->expression.vtype]);
                return false;
            }
            switch (exprs[0]->expression.vtype) {
                case TYPE_FLOAT:
                    if (exprs[1]->expression.vtype == TYPE_VECTOR)
                        out = (ast_expression*)ast_binary_new(ctx, INSTR_MUL_FV, exprs[0], exprs[1]);
                    else
                        out = (ast_expression*)ast_binary_new(ctx, INSTR_MUL_F, exprs[0], exprs[1]);
                    break;
                case TYPE_VECTOR:
                    if (exprs[1]->expression.vtype == TYPE_FLOAT)
                        out = (ast_expression*)ast_binary_new(ctx, INSTR_MUL_VF, exprs[0], exprs[1]);
                    else
                        out = (ast_expression*)ast_binary_new(ctx, INSTR_MUL_V, exprs[0], exprs[1]);
                    break;
                default:
                    parseerror(parser, "invalid types used in expression: cannot multiply types %s and %s",
                               type_name[exprs[1]->expression.vtype],
                               type_name[exprs[0]->expression.vtype]);
                    return false;
            };
            break;
        case opid1('/'):
            if (NotSameType(TYPE_FLOAT)) {
                parseerror(parser, "invalid types used in expression: cannot divide types %s and %s",
                           type_name[exprs[0]->expression.vtype],
                           type_name[exprs[1]->expression.vtype]);
                return false;
            }
            out = (ast_expression*)ast_binary_new(ctx, INSTR_DIV_F, exprs[0], exprs[1]);
            break;
        case opid1('%'):
        case opid2('%','='):
            parseerror(parser, "qc does not have a modulo operator");
            return false;
        case opid1('|'):
        case opid1('&'):
            if (NotSameType(TYPE_FLOAT)) {
                parseerror(parser, "invalid types used in expression: cannot perform bit operations between types %s and %s",
                           type_name[exprs[0]->expression.vtype],
                           type_name[exprs[1]->expression.vtype]);
                return false;
            }
            out = (ast_expression*)ast_binary_new(ctx,
                                                  (op->id == opid1('|') ? INSTR_BITOR : INSTR_BITAND),
                                                  exprs[0], exprs[1]);
            break;
        case opid1('^'):
            parseerror(parser, "TODO: bitxor");
            return false;

        case opid2('<','<'):
        case opid2('>','>'):
        case opid3('<','<','='):
        case opid3('>','>','='):
            parseerror(parser, "TODO: shifts");
            return false;

        case opid2('|','|'):
            generated_op += 1; /* INSTR_OR */
        case opid2('&','&'):
            generated_op += INSTR_AND;
            if (NotSameType(TYPE_FLOAT)) {
                parseerror(parser, "invalid types used in expression: cannot perform logical operations between types %s and %s",
                           type_name[exprs[0]->expression.vtype],
                           type_name[exprs[1]->expression.vtype]);
                parseerror(parser, "TODO: logical ops for arbitrary types using INSTR_NOT");
                parseerror(parser, "TODO: optional early out");
                return false;
            }
            if (opts_standard == COMPILER_GMQCC)
                printf("TODO: early out logic\n");
            out = (ast_expression*)ast_binary_new(ctx, generated_op, exprs[0], exprs[1]);
            break;

        case opid1('>'):
            generated_op += 1; /* INSTR_GT */
        case opid1('<'):
            generated_op += 1; /* INSTR_LT */
        case opid2('>', '='):
            generated_op += 1; /* INSTR_GE */
        case opid2('<', '='):
            generated_op += INSTR_LE;
            if (NotSameType(TYPE_FLOAT)) {
                parseerror(parser, "invalid types used in expression: cannot perform comparison between types %s and %s",
                           type_name[exprs[0]->expression.vtype],
                           type_name[exprs[1]->expression.vtype]);
                return false;
            }
            out = (ast_expression*)ast_binary_new(ctx, generated_op, exprs[0], exprs[1]);
            break;
        case opid2('!', '='):
            if (exprs[0]->expression.vtype != exprs[1]->expression.vtype) {
                parseerror(parser, "invalid types used in expression: cannot perform comparison between types %s and %s",
                           type_name[exprs[0]->expression.vtype],
                           type_name[exprs[1]->expression.vtype]);
                return false;
            }
            out = (ast_expression*)ast_binary_new(ctx, type_ne_instr[exprs[0]->expression.vtype], exprs[0], exprs[1]);
            break;
        case opid2('=', '='):
            if (exprs[0]->expression.vtype != exprs[1]->expression.vtype) {
                parseerror(parser, "invalid types used in expression: cannot perform comparison between types %s and %s",
                           type_name[exprs[0]->expression.vtype],
                           type_name[exprs[1]->expression.vtype]);
                return false;
            }
            out = (ast_expression*)ast_binary_new(ctx, type_eq_instr[exprs[0]->expression.vtype], exprs[0], exprs[1]);
            break;

        case opid1('='):
            if (ast_istype(exprs[0], ast_entfield))
                assignop = type_storep_instr[exprs[0]->expression.vtype];
            else
                assignop = type_store_instr[exprs[0]->expression.vtype];
            out = (ast_expression*)ast_store_new(ctx, assignop, exprs[0], exprs[1]);
            break;
        case opid2('+','='):
            if (exprs[0]->expression.vtype != exprs[1]->expression.vtype) {
                parseerror(parser, "invalid types used in expression: cannot add type %s and %s",
                           type_name[exprs[0]->expression.vtype],
                           type_name[exprs[1]->expression.vtype]);
                return false;
            }
            if (exprs[0]->expression.vtype != TYPE_VECTOR && exprs[0]->expression.vtype != TYPE_FLOAT) {
                parseerror(parser, "invalid types used in expression: cannot add type %s and %s",
                           type_name[exprs[0]->expression.vtype],
                           type_name[exprs[1]->expression.vtype]);
                return false;
            }
            if (ast_istype(exprs[0], ast_entfield))
                assignop = type_storep_instr[exprs[0]->expression.vtype];
            else
                assignop = type_store_instr[exprs[0]->expression.vtype];
            switch (exprs[0]->expression.vtype) {
                case TYPE_FLOAT:
                    out = (ast_expression*)ast_binstore_new(ctx, assignop, INSTR_ADD_F, exprs[0], exprs[1]);
                    break;
                case TYPE_VECTOR:
                    out = (ast_expression*)ast_binstore_new(ctx, assignop, INSTR_ADD_V, exprs[0], exprs[1]);
                    break;
                default:
                    parseerror(parser, "invalid types used in expression: cannot add type %s and %s",
                               type_name[exprs[0]->expression.vtype],
                               type_name[exprs[1]->expression.vtype]);
                    return false;
            };
            break;
    }
#undef NotSameType

    if (!out) {
        parseerror(parser, "failed to apply operand %s", op->op);
        return false;
    }

    DEBUGSHUNTDO(printf("applied %s\n", op->op));
    sy->out[sy->out_count++] = syexp(ctx, out);
    return true;
}

static bool parser_close_call(parser_t *parser, shunt *sy)
{
    /* was a function call */
    ast_expression *fun;
    ast_call       *call;

    size_t          fid;
    size_t          paramcount;

    sy->ops_count--;
    fid = sy->ops[sy->ops_count].off;

    /* out[fid] is the function
     * everything above is parameters...
     * 0 params = nothing
     * 1 params = ast_expression
     * more = ast_block
     */

    if (sy->out_count < 1 || sy->out_count <= fid) {
        parseerror(parser, "internal error: function call needs function and parameter list...");
        return false;
    }

    fun = sy->out[fid].out;

    call = ast_call_new(sy->ops[sy->ops_count].ctx, fun);
    if (!call) {
        parseerror(parser, "out of memory");
        return false;
    }

    if (fid+1 == sy->out_count) {
        /* no arguments */
        paramcount = 0;
    } else if (fid+2 == sy->out_count) {
        ast_block *params;
        sy->out_count--;
        params = sy->out[sy->out_count].block;
        if (!params) {
            /* 1 param */
            paramcount = 1;
            if (!ast_call_params_add(call, sy->out[sy->out_count].out)) {
                ast_delete(sy->out[sy->out_count].out);
                parseerror(parser, "out of memory");
                return false;
            }
        } else {
            paramcount = params->exprs_count;
            MEM_VECTOR_MOVE(params, exprs, call, params);
            ast_delete(params);
        }
    } else {
        parseerror(parser, "invalid function call");
        return false;
    }

    /* overwrite fid, the function, with a call */
    sy->out[fid] = syexp(call->expression.node.context, (ast_expression*)call);

    if (fun->expression.vtype != TYPE_FUNCTION) {
        parseerror(parser, "not a function");
        return false;
    }

    if (!fun->expression.next) {
        parseerror(parser, "could not determine function return type");
        return false;
    } else {
        if (fun->expression.params_count != paramcount) {
            parseerror(parser, "expected %i parameters, got %i", (int)fun->expression.params_count, paramcount);
            return false;
        }
    }

    return true;
}

static bool parser_close_paren(parser_t *parser, shunt *sy, bool functions_only)
{
    if (!sy->ops_count) {
        parseerror(parser, "unmatched closing paren");
        return false;
    }
    if (sy->ops[sy->ops_count-1].paren == 1) {
        parseerror(parser, "empty parenthesis expression");
        return false;
    }
    while (sy->ops_count) {
        if (sy->ops[sy->ops_count-1].paren == 'f') {
            if (!parser_close_call(parser, sy))
                return false;
            break;
        }
        if (sy->ops[sy->ops_count-1].paren == 1) {
            sy->ops_count--;
            return !functions_only;
        }
        if (!parser_sy_pop(parser, sy))
            return false;
    }
    return true;
}

static ast_expression* parser_expression_leave(parser_t *parser)
{
    ast_expression *expr = NULL;
    shunt sy;
    bool wantop = false;
    bool gotmemberof = false;

    /* count the parens because an if starts with one, so the
     * end of a condition is an unmatched closing paren
     */
    int parens = 0;

    MEM_VECTOR_INIT(&sy, out);
    MEM_VECTOR_INIT(&sy, ops);

    while (true)
    {
        if (gotmemberof)
            gotmemberof = false;
        else
            parser->memberof = 0;
        if (!wantop)
        {
            bool nextwant = true;
            if (parser->tok == TOKEN_IDENT)
            {
                /* variable */
                ast_expression *var;
                if (opts_standard == COMPILER_GMQCC)
                {
                    if (parser->memberof == TYPE_ENTITY)
                        var = parser_find_field(parser, parser_tokval(parser));
                    else if (parser->memberof == TYPE_VECTOR)
                    {
                        parseerror(parser, "TODO: implement effective vector member access");
                        goto onerr;
                    }
                    else if (parser->memberof) {
                        parseerror(parser, "namespace for member not found");
                        goto onerr;
                    }
                    else
                        var = parser_find_var(parser, parser_tokval(parser));
                } else {
                    var = parser_find_var(parser, parser_tokval(parser));
                    if (!var)
                        var = parser_find_field(parser, parser_tokval(parser));
                }
                if (!var) {
                    parseerror(parser, "unexpected ident: %s", parser_tokval(parser));
                    goto onerr;
                }
                if (!shunt_out_add(&sy, syexp(parser_ctx(parser), var))) {
                    parseerror(parser, "out of memory");
                    goto onerr;
                }
                DEBUGSHUNTDO(printf("push %s\n", parser_tokval(parser)));
            }
            else if (parser->tok == TOKEN_FLOATCONST) {
                ast_value *val = parser_const_float(parser, (parser_token(parser)->constval.f));
                if (!val)
                    return false;
                if (!shunt_out_add(&sy, syexp(parser_ctx(parser), (ast_expression*)val))) {
                    parseerror(parser, "out of memory");
                    goto onerr;
                }
                DEBUGSHUNTDO(printf("push %g\n", parser_token(parser)->constval.f));
            }
            else if (parser->tok == TOKEN_INTCONST) {
                ast_value *val = parser_const_float(parser, (double)(parser_token(parser)->constval.i));
                if (!val)
                    return false;
                if (!shunt_out_add(&sy, syexp(parser_ctx(parser), (ast_expression*)val))) {
                    parseerror(parser, "out of memory");
                    goto onerr;
                }
                DEBUGSHUNTDO(printf("push %i\n", parser_token(parser)->constval.i));
            }
            else if (parser->tok == TOKEN_STRINGCONST) {
                ast_value *val = parser_const_string(parser, parser_tokval(parser));
                if (!val)
                    return false;
                if (!shunt_out_add(&sy, syexp(parser_ctx(parser), (ast_expression*)val))) {
                    parseerror(parser, "out of memory");
                    goto onerr;
                }
                DEBUGSHUNTDO(printf("push string\n"));
            }
            else if (parser->tok == TOKEN_VECTORCONST) {
                ast_value *val = parser_const_vector(parser, parser_token(parser)->constval.v);
                if (!val)
                    return false;
                if (!shunt_out_add(&sy, syexp(parser_ctx(parser), (ast_expression*)val))) {
                    parseerror(parser, "out of memory");
                    goto onerr;
                }
                DEBUGSHUNTDO(printf("push '%g %g %g'\n",
                                    parser_token(parser)->constval.v.x,
                                    parser_token(parser)->constval.v.y,
                                    parser_token(parser)->constval.v.z));
            }
            else if (parser->tok == '(') {
                ++parens;
                nextwant = false; /* not expecting an operator next */
                if (!shunt_ops_add(&sy, syparen(parser_ctx(parser), 1, 0))) {
                    parseerror(parser, "out of memory");
                    goto onerr;
                }
                DEBUGSHUNTDO(printf("push (\n"));
            }
            else if (parser->tok == ')') {
                DEBUGSHUNTDO(printf("do[nop] )\n"));
                --parens;
                if (parens < 0)
                    break;
                /* allowed for function calls */
                if (!parser_close_paren(parser, &sy, true))
                    goto onerr;
            }
            else {
                /* TODO: prefix operators */
                parseerror(parser, "expected statement");
                goto onerr;
            }
            wantop = nextwant;
            parser->lex->flags.noops = !wantop;
        } else {
            bool nextwant = false;
            if (parser->tok == '(') {
                DEBUGSHUNTDO(printf("push (\n"));
                ++parens;
                /* we expected an operator, this is the function-call operator */
                if (!shunt_ops_add(&sy, syparen(parser_ctx(parser), 'f', sy.out_count-1))) {
                    parseerror(parser, "out of memory");
                    goto onerr;
                }
            }
            else if (parser->tok == ')') {
                DEBUGSHUNTDO(printf("do[op] )\n"));
                --parens;
                if (parens < 0)
                    break;
                /* we do expect an operator next */
                /* closing an opening paren */
                if (!parser_close_paren(parser, &sy, false))
                    goto onerr;
                nextwant = true;
            }
            else if (parser->tok != TOKEN_OPERATOR) {
                parseerror(parser, "expected operator or end of statement");
                goto onerr;
            }
            else {
                /* classify the operator */
                /* TODO: suffix operators */
                const oper_info *op;
                const oper_info *olast = NULL;
                size_t o;
                for (o = 0; o < operator_count; ++o) {
                    if (!(operators[o].flags & OP_PREFIX) &&
                        !(operators[o].flags & OP_SUFFIX) && /* remove this */
                        !strcmp(parser_tokval(parser), operators[o].op))
                    {
                        break;
                    }
                }
                if (o == operator_count) {
                    /* no operator found... must be the end of the statement */
                    break;
                }
                /* found an operator */
                op = &operators[o];
                if (op->id == opid1('.')) {
                    /* for gmqcc standard: open up the namespace of the previous type */
                    ast_expression *prevex = sy.out[sy.out_count-1].out;
                    if (!prevex) {
                        parseerror(parser, "unexpected member operator");
                        goto onerr;
                    }
                    if (prevex->expression.vtype == TYPE_ENTITY)
                        parser->memberof = TYPE_ENTITY;
                    else if (prevex->expression.vtype == TYPE_VECTOR)
                        parser->memberof = TYPE_VECTOR;
                    else {
                        parseerror(parser, "type error: type has no members");
                        goto onerr;
                    }
                    gotmemberof = true;
                }

                if (sy.ops_count && !sy.ops[sy.ops_count-1].paren)
                    olast = &operators[sy.ops[sy.ops_count-1].etype-1];

                while (olast && (
                        (op->prec < olast->prec) ||
                        (op->assoc == ASSOC_LEFT && op->prec <= olast->prec) ) )
                {
                    if (!parser_sy_pop(parser, &sy))
                        goto onerr;
                    if (sy.ops_count && !sy.ops[sy.ops_count-1].paren)
                        olast = &operators[sy.ops[sy.ops_count-1].etype-1];
                    else
                        olast = NULL;
                }

                DEBUGSHUNTDO(printf("push operator %s\n", op->op));
                if (!shunt_ops_add(&sy, syop(parser_ctx(parser), op)))
                    goto onerr;
            }
            wantop = nextwant;
            parser->lex->flags.noops = !wantop;
        }
        if (!parser_next(parser)) {
            goto onerr;
        }
        if (parser->tok == ';') {
            break;
        }
    }

    while (sy.ops_count) {
        if (!parser_sy_pop(parser, &sy))
            goto onerr;
    }

    parser->lex->flags.noops = true;
    if (!sy.out_count) {
        parseerror(parser, "empty expression");
        expr = NULL;
    } else
        expr = sy.out[0].out;
    MEM_VECTOR_CLEAR(&sy, out);
    MEM_VECTOR_CLEAR(&sy, ops);
    DEBUGSHUNTDO(printf("shut done\n"));
    return expr;

onerr:
    parser->lex->flags.noops = true;
    MEM_VECTOR_CLEAR(&sy, out);
    MEM_VECTOR_CLEAR(&sy, ops);
    return NULL;
}

static ast_expression* parser_expression(parser_t *parser)
{
    ast_expression *e = parser_expression_leave(parser);
    if (!e)
        return NULL;
    if (!parser_next(parser)) {
        ast_delete(e);
        return NULL;
    }
    return e;
}

static bool parser_parse_if(parser_t *parser, ast_block *block, ast_expression **out)
{
    ast_ifthen *ifthen;
    ast_expression *cond, *ontrue, *onfalse = NULL;

    lex_ctx ctx = parser_ctx(parser);

    /* skip the 'if' and check for opening paren */
    if (!parser_next(parser) || parser->tok != '(') {
        parseerror(parser, "expected 'if' condition in parenthesis");
        return false;
    }
    /* parse into the expression */
    if (!parser_next(parser)) {
        parseerror(parser, "expected 'if' condition after opening paren");
        return false;
    }
    /* parse the condition */
    cond = parser_expression_leave(parser);
    if (!cond)
        return false;
    /* closing paren */
    if (parser->tok != ')') {
        parseerror(parser, "expected closing paren after 'if' condition");
        ast_delete(cond);
        return false;
    }
    /* parse into the 'then' branch */
    if (!parser_next(parser)) {
        parseerror(parser, "expected statement for on-true branch of 'if'");
        ast_delete(cond);
        return false;
    }
    ontrue = parser_parse_statement_or_block(parser);
    if (!ontrue) {
        ast_delete(cond);
        return false;
    }
    /* check for an else */
    if (!strcmp(parser_tokval(parser), "else")) {
        /* parse into the 'else' branch */
        if (!parser_next(parser)) {
            parseerror(parser, "expected on-false branch after 'else'");
            ast_delete(ontrue);
            ast_delete(cond);
            return false;
        }
        onfalse = parser_parse_statement_or_block(parser);
        if (!onfalse) {
            ast_delete(ontrue);
            ast_delete(cond);
            return false;
        }
    }

    ifthen = ast_ifthen_new(ctx, cond, ontrue, onfalse);
    *out = (ast_expression*)ifthen;
    return true;
}

static bool parser_parse_while(parser_t *parser, ast_block *block, ast_expression **out)
{
    ast_loop *aloop;
    ast_expression *cond, *ontrue;

    lex_ctx ctx = parser_ctx(parser);

    /* skip the 'while' and check for opening paren */
    if (!parser_next(parser) || parser->tok != '(') {
        parseerror(parser, "expected 'while' condition in parenthesis");
        return false;
    }
    /* parse into the expression */
    if (!parser_next(parser)) {
        parseerror(parser, "expected 'while' condition after opening paren");
        return false;
    }
    /* parse the condition */
    cond = parser_expression_leave(parser);
    if (!cond)
        return false;
    /* closing paren */
    if (parser->tok != ')') {
        parseerror(parser, "expected closing paren after 'while' condition");
        ast_delete(cond);
        return false;
    }
    /* parse into the 'then' branch */
    if (!parser_next(parser)) {
        parseerror(parser, "expected while-loop body");
        ast_delete(cond);
        return false;
    }
    ontrue = parser_parse_statement_or_block(parser);
    if (!ontrue) {
        ast_delete(cond);
        return false;
    }

    aloop = ast_loop_new(ctx, NULL, cond, NULL, NULL, ontrue);
    *out = (ast_expression*)aloop;
    return true;
}

static bool parser_parse_dowhile(parser_t *parser, ast_block *block, ast_expression **out)
{
    ast_loop *aloop;
    ast_expression *cond, *ontrue;

    lex_ctx ctx = parser_ctx(parser);

    /* skip the 'do' and get the body */
    if (!parser_next(parser)) {
        parseerror(parser, "expected loop body");
        return false;
    }
    ontrue = parser_parse_statement_or_block(parser);
    if (!ontrue)
        return false;

    /* expect the "while" */
    if (parser->tok != TOKEN_KEYWORD ||
        strcmp(parser_tokval(parser), "while"))
    {
        parseerror(parser, "expected 'while' and condition");
        ast_delete(ontrue);
        return false;
    }

    /* skip the 'while' and check for opening paren */
    if (!parser_next(parser) || parser->tok != '(') {
        parseerror(parser, "expected 'while' condition in parenthesis");
        ast_delete(ontrue);
        return false;
    }
    /* parse into the expression */
    if (!parser_next(parser)) {
        parseerror(parser, "expected 'while' condition after opening paren");
        ast_delete(ontrue);
        return false;
    }
    /* parse the condition */
    cond = parser_expression_leave(parser);
    if (!cond)
        return false;
    /* closing paren */
    if (parser->tok != ')') {
        parseerror(parser, "expected closing paren after 'while' condition");
        ast_delete(ontrue);
        ast_delete(cond);
        return false;
    }
    /* parse on */
    if (!parser_next(parser) || parser->tok != ';') {
        parseerror(parser, "expected semicolon after condition");
        ast_delete(ontrue);
        ast_delete(cond);
        return false;
    }

    if (!parser_next(parser)) {
        parseerror(parser, "parse error");
        ast_delete(ontrue);
        ast_delete(cond);
        return false;
    }

    aloop = ast_loop_new(ctx, NULL, NULL, cond, NULL, ontrue);
    *out = (ast_expression*)aloop;
    return true;
}

static bool parser_parse_for(parser_t *parser, ast_block *block, ast_expression **out)
{
    ast_loop *aloop;
    ast_expression *initexpr, *cond, *increment, *ontrue;
    size_t oldblocklocal;

    lex_ctx ctx = parser_ctx(parser);

    oldblocklocal = parser->blocklocal;
    parser->blocklocal = parser->locals_count;

    initexpr  = NULL;
    cond      = NULL;
    increment = NULL;
    ontrue    = NULL;

    /* skip the 'while' and check for opening paren */
    if (!parser_next(parser) || parser->tok != '(') {
        parseerror(parser, "expected 'for' expressions in parenthesis");
        goto onerr;
    }
    /* parse into the expression */
    if (!parser_next(parser)) {
        parseerror(parser, "expected 'for' initializer after opening paren");
        goto onerr;
    }

    if (parser->tok == TOKEN_TYPENAME) {
        if (opts_standard != COMPILER_GMQCC) {
            if (parsewarning(parser, WARN_EXTENSIONS,
                             "current standard does not allow variable declarations in for-loop initializers"))
                goto onerr;
        }

        parseerror(parser, "TODO: assignment of new variables to be non-const");
        goto onerr;
        if (!parser_variable(parser, block))
            goto onerr;
    }
    else if (parser->tok != ';')
    {
        initexpr = parser_expression_leave(parser);
        if (!initexpr)
            goto onerr;
    }

    /* move on to condition */
    if (parser->tok != ';') {
        parseerror(parser, "expected semicolon after for-loop initializer");
        goto onerr;
    }
    if (!parser_next(parser)) {
        parseerror(parser, "expected for-loop condition");
        goto onerr;
    }

    /* parse the condition */
    if (parser->tok != ';') {
        cond = parser_expression_leave(parser);
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
        increment = parser_expression_leave(parser);
        if (!increment)
            goto onerr;
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
    ontrue = parser_parse_statement_or_block(parser);
    if (!ontrue) {
        goto onerr;
    }

    aloop = ast_loop_new(ctx, initexpr, cond, NULL, increment, ontrue);
    *out = (ast_expression*)aloop;

    while (parser->locals_count > parser->blocklocal)
        parser_pop_local(parser);
    parser->blocklocal = oldblocklocal;
    return true;
onerr:
    if (initexpr)  ast_delete(initexpr);
    if (cond)      ast_delete(cond);
    if (increment) ast_delete(increment);
    while (parser->locals_count > parser->blocklocal)
        parser_pop_local(parser);
    parser->blocklocal = oldblocklocal;
    return false;
}

static bool parser_parse_statement(parser_t *parser, ast_block *block, ast_expression **out)
{
    if (parser->tok == TOKEN_TYPENAME)
    {
        /* local variable */
        if (!block) {
            parseerror(parser, "cannot declare a variable from here");
            return false;
        }
        if (opts_standard == COMPILER_QCC) {
            if (parsewarning(parser, WARN_EXTENSIONS, "missing 'local' keyword when declaring a local variable"))
                return false;
        }
        if (!parser_variable(parser, block))
            return false;
        *out = NULL;
        return true;
    }
    else if (parser->tok == TOKEN_KEYWORD)
    {
        if (!strcmp(parser_tokval(parser), "local"))
        {
            if (!block) {
                parseerror(parser, "cannot declare a local variable here");
                return false;
            }
            if (!parser_next(parser)) {
                parseerror(parser, "expected variable declaration");
                return false;
            }
            if (!parser_variable(parser, block))
                return false;
            *out = NULL;
            return true;
        }
        else if (!strcmp(parser_tokval(parser), "return"))
        {
            ast_expression *exp = NULL;
            ast_return     *ret = NULL;
            ast_value      *expected = parser->function->vtype;

            if (!parser_next(parser)) {
                parseerror(parser, "expected return expression");
                return false;
            }

            if (parser->tok != ';') {
                exp = parser_expression(parser);
                if (!exp)
                    return false;

                if (exp->expression.vtype != expected->expression.next->expression.vtype) {
                    parseerror(parser, "return with invalid expression");
                }

                ret = ast_return_new(exp->expression.node.context, exp);
                if (!ret) {
                    ast_delete(exp);
                    return false;
                }

                *out = (ast_expression*)ret;
            } else if (!parser_next(parser)) {
                parseerror(parser, "expected semicolon");
                if (expected->expression.next->expression.vtype != TYPE_VOID) {
                    parseerror(parser, "return without value");
                }
            }
            return true;
        }
        else if (!strcmp(parser_tokval(parser), "if"))
        {
            return parser_parse_if(parser, block, out);
        }
        else if (!strcmp(parser_tokval(parser), "while"))
        {
            return parser_parse_while(parser, block, out);
        }
        else if (!strcmp(parser_tokval(parser), "do"))
        {
            return parser_parse_dowhile(parser, block, out);
        }
        else if (!strcmp(parser_tokval(parser), "for"))
        {
            if (opts_standard == COMPILER_QCC) {
                if (parsewarning(parser, WARN_EXTENSIONS, "for loops are not recognized in the original Quake C standard, to enable try an alternate standard --std=?"))
                    return false;
            }
            return parser_parse_for(parser, block, out);
        }
        parseerror(parser, "Unexpected keyword");
        return false;
    }
    else if (parser->tok == '{')
    {
        ast_block *inner;
        inner = parser_parse_block(parser);
        if (!inner)
            return false;
        *out = (ast_expression*)inner;
        return true;
    }
    else
    {
        ast_expression *exp = parser_expression(parser);
        if (!exp)
            return false;
        *out = exp;
        return true;
    }
}

static void parser_pop_local(parser_t *parser)
{
    parser->locals_count--;
    mem_d(parser->locals[parser->locals_count].name);
}

static ast_block* parser_parse_block(parser_t *parser)
{
    size_t oldblocklocal;
    ast_block *block = NULL;

    oldblocklocal = parser->blocklocal;
    parser->blocklocal = parser->locals_count;

    if (!parser_next(parser)) { /* skip the '{' */
        parseerror(parser, "expected function body");
        goto cleanup;
    }

    block = ast_block_new(parser_ctx(parser));

    while (parser->tok != TOKEN_EOF && parser->tok < TOKEN_ERROR)
    {
        ast_expression *expr;
        if (parser->tok == '}')
            break;

        if (!parser_parse_statement(parser, block, &expr)) {
            ast_block_delete(block);
            block = NULL;
            goto cleanup;
        }
        if (!expr)
            continue;
        if (!ast_block_exprs_add(block, expr)) {
            ast_delete(expr);
            ast_block_delete(block);
            block = NULL;
            goto cleanup;
        }
    }

    if (parser->tok != '}') {
        ast_block_delete(block);
        block = NULL;
    } else {
        (void)parser_next(parser);
    }

cleanup:
    while (parser->locals_count > parser->blocklocal)
        parser_pop_local(parser);
    parser->blocklocal = oldblocklocal;
    /* unroll the local vector */
    return block;
}

static ast_expression* parser_parse_statement_or_block(parser_t *parser)
{
    ast_expression *expr;
    if (parser->tok == '{')
        return (ast_expression*)parser_parse_block(parser);
    if (!parser_parse_statement(parser, NULL, &expr))
        return NULL;
    return expr;
}

static bool parser_variable(parser_t *parser, ast_block *localblock)
{
    bool          isfunc = false;
    ast_function *func = NULL;
    lex_ctx       ctx;
    ast_value    *var;
    varentry_t    varent;
    ast_expression *olddecl;

    int basetype = parser_token(parser)->constval.t;

    while (true)
    {
        if (!parser_next(parser)) { /* skip basetype or comma */
            parseerror(parser, "expected variable declaration");
            return false;
        }

        olddecl = NULL;
        isfunc  = false;
        func    = NULL;
        ctx = parser_ctx(parser);
        var = parser_parse_type(parser, basetype, &isfunc);

        if (!var)
            return false;

        if (parser->tok != TOKEN_IDENT) {
            parseerror(parser, "expected variable name\n");
            return false;
        }

        if (!isfunc) {
            if (!localblock && (olddecl = parser_find_global(parser, parser_tokval(parser)))) {
                ast_value_delete(var);
                parseerror(parser, "global %s already declared here: %s:%i\n",
                           parser_tokval(parser), ast_ctx(olddecl).file, (int)ast_ctx(olddecl).line);
                return false;
            }

            if (localblock && parser_find_local(parser, parser_tokval(parser), parser->blocklocal)) {
                ast_value_delete(var);
                parseerror(parser, "local %s already declared here: %s:%i\n",
                           parser_tokval(parser), ast_ctx(olddecl).file, (int)ast_ctx(olddecl).line);
                return false;
            }
        }

        if (!ast_value_set_name(var, parser_tokval(parser))) {
            parseerror(parser, "failed to set variable name\n");
            ast_value_delete(var);
            return false;
        }

        if (isfunc) {
            /* a function was defined */
            ast_value *fval;
            ast_value *proto = NULL;

            if (!localblock)
                olddecl = parser_find_global(parser, parser_tokval(parser));
            else
                olddecl = parser_find_local(parser, parser_tokval(parser), parser->blocklocal);

            if (olddecl) {
                /* we had a prototype */
                if (!ast_istype(olddecl, ast_value)) {
                    /* theoretically not possible you think?
                     * well:
                     * vector v;
                     * void() v_x = {}
                     * got it?
                     */
                    parseerror(parser, "cannot declare a function with the same name as a vector's member: %s",
                               parser_tokval(parser));
                    ast_value_delete(var);
                    return false;
                }

                proto = (ast_value*)olddecl;
            }

            /* turn var into a value of TYPE_FUNCTION, with the old var
             * as return type
             */
            fval = ast_value_new(ctx, var->name, TYPE_FUNCTION);
            func = ast_function_new(ctx, var->name, fval);
            if (!fval || !func) {
                ast_value_delete(var);
                if (fval) ast_value_delete(fval);
                if (func) ast_function_delete(func);
                return false;
            }

            fval->expression.next = (ast_expression*)var;
            MEM_VECTOR_MOVE(&var->expression, params, &fval->expression, params);

            /* we compare the type late here, but it's easier than
             * messing with the parameter-vector etc. earlier
             */
            if (proto) {
                if (!ast_compare_type((ast_expression*)proto, (ast_expression*)fval)) {
                    parseerror(parser, "prototype declared at %s:%i had a different type",
                               ast_ctx(proto).file, ast_ctx(proto).line);
                    ast_function_delete(func);
                    ast_value_delete(fval);
                    return false;
                }
                ast_function_delete(func);
                ast_value_delete(fval);
                var = proto;
                func = var->constval.vfunc;
            }
            else
            {
                if (!parser_t_functions_add(parser, func)) {
                    ast_function_delete(func);
                    ast_value_delete(fval);
                    return false;
                }
            }

            var = fval;
        }

        varent.name = util_strdup(var->name);
        varent.var = (ast_expression*)var;
        if (var->expression.vtype == TYPE_VECTOR)
        {
            size_t len = strlen(varent.name);
            varentry_t vx, vy, vz;
            vx.var = (ast_expression*)ast_member_new(var->expression.node.context, (ast_expression*)var, 0);
            vy.var = (ast_expression*)ast_member_new(var->expression.node.context, (ast_expression*)var, 1);
            vz.var = (ast_expression*)ast_member_new(var->expression.node.context, (ast_expression*)var, 2);
            vx.name = mem_a(len+3);
            vy.name = mem_a(len+3);
            vz.name = mem_a(len+3);
            strcpy(vx.name, varent.name);
            strcpy(vy.name, varent.name);
            strcpy(vz.name, varent.name);
            vx.name[len] = vy.name[len] = vz.name[len] = '_';
            vx.name[len+1] = 'x';
            vy.name[len+1] = 'y';
            vz.name[len+1] = 'z';
            vx.name[len+2] = vy.name[len+2] = vz.name[len+2] = 0;

            if (!localblock) {
                (void)!parser_t_globals_add(parser, varent);
                (void)!parser_t_globals_add(parser, vx);
                (void)!parser_t_globals_add(parser, vy);
                (void)!parser_t_globals_add(parser, vz);
            } else {
                (void)!parser_t_locals_add(parser, varent);
                (void)!parser_t_locals_add(parser, vx);
                (void)!parser_t_locals_add(parser, vy);
                (void)!parser_t_locals_add(parser, vz);
            }
        }
        else
        {
            if ( (!localblock && !parser_t_globals_add(parser, varent)) ||
                 ( localblock && !parser_t_locals_add(parser, varent)) )
            {
                ast_value_delete(var);
                return false;
            }
        }
        if (localblock && !ast_block_locals_add(localblock, var))
        {
            parser_pop_local(parser);
            ast_value_delete(var);
            return false;
        }

        if (!parser_next(parser)) {
            ast_value_delete(var);
            return false;
        }

        if (parser->tok == ';') {
            if (!parser_next(parser))
                return parser->tok == TOKEN_EOF;
            return true;
        }

        if (parser->tok == ',') {
            /* another var */
            continue;
        }

        if (parser->tok != '=') {
            parseerror(parser, "expected '=' or ';'");
            return false;
        }

        if (!parser_next(parser))
            return false;

        if (parser->tok == '#') {
            if (localblock) {
                parseerror(parser, "cannot declare builtins within functions");
                return false;
            }
            if (!isfunc || !func) {
                parseerror(parser, "unexpected builtin number, '%s' is not a function", var->name);
                return false;
            }
            if (!parser_next(parser)) {
                parseerror(parser, "expected builtin number");
                return false;
            }
            if (parser->tok != TOKEN_INTCONST) {
                parseerror(parser, "builtin number must be an integer constant");
                return false;
            }
            if (parser_token(parser)->constval.i <= 0) {
                parseerror(parser, "builtin number must be positive integer greater than zero");
                return false;
            }

            func->builtin = -parser_token(parser)->constval.i;
        } else if (parser->tok == '{') {
            /* function body */
            ast_block *block;
            ast_function *old = parser->function;

            if (localblock) {
                parseerror(parser, "cannot declare functions within functions");
                return false;
            }

            parser->function = func;
            block = parser_parse_block(parser);
            parser->function = old;

            if (!block)
                return false;

            if (!ast_function_blocks_add(func, block)) {
                ast_block_delete(block);
                return false;
            }

            if (parser->tok == ';')
                return parser_next(parser) || parser->tok == TOKEN_EOF;
            else if (opts_standard == COMPILER_QCC)
                parseerror(parser, "missing semicolon after function body (mandatory with -std=qcc)");
            return true;
        } else {
            parseerror(parser, "TODO, const assignment");
        }

        if (!parser_next(parser))
            return false;

        if (parser->tok == ',') {
            /* another */
            continue;
        }

        if (parser->tok != ';') {
            parseerror(parser, "expected semicolon");
            return false;
        }

        (void)parser_next(parser);

        return true;
    }
}

static bool parser_do(parser_t *parser)
{
    if (parser->tok == TOKEN_TYPENAME)
    {
        return parser_variable(parser, NULL);
    }
    else if (parser->tok == TOKEN_KEYWORD)
    {
        /* handle 'var' and 'const' */
        return false;
    }
    else if (parser->tok == '.')
    {
        ast_value *var;
        ast_value *fld;
        bool       isfunc = false;
        int        basetype;
        lex_ctx    ctx = parser_ctx(parser);
        varentry_t varent;

        /* entity-member declaration */
        if (!parser_next(parser) || parser->tok != TOKEN_TYPENAME) {
            parseerror(parser, "expected member variable definition");
            return false;
        }

        /* remember the base/return type */
        basetype = parser_token(parser)->constval.t;

        /* parse into the declaration */
        if (!parser_next(parser)) {
            parseerror(parser, "expected field def");
            return false;
        }

        /* parse the field type fully */
        var = parser_parse_type(parser, basetype, &isfunc);
        if (!var)
            return false;

        /* now the field name */
        if (parser->tok != TOKEN_IDENT) {
            parseerror(parser, "expected field name");
            ast_delete(var);
            return false;
        }

        /* check for an existing field
         * in original qc we also have to check for an existing
         * global named like the field
         */
        if (opts_standard == COMPILER_QCC) {
            if (parser_find_global(parser, parser_tokval(parser))) {
                parseerror(parser, "cannot declare a field and a global of the same name with -std=qcc");
                ast_delete(var);
                return false;
            }
        }
        if (parser_find_field(parser, parser_tokval(parser))) {
            parseerror(parser, "field %s already exists", parser_tokval(parser));
            ast_delete(var);
            return false;
        }

        /* if it was a function, turn it into a function */
        if (isfunc) {
            ast_value *fval;
            /* turn var into a value of TYPE_FUNCTION, with the old var
             * as return type
             */
            fval = ast_value_new(ctx, var->name, TYPE_FUNCTION);
            if (!fval) {
                ast_value_delete(var);
                ast_value_delete(fval);
                return false;
            }

            fval->expression.next = (ast_expression*)var;
            MEM_VECTOR_MOVE(&var->expression, params, &fval->expression, params);

            var = fval;
        }

        /* turn it into a field */
        fld = ast_value_new(ctx, parser_tokval(parser), TYPE_FIELD);
        fld->expression.next = (ast_expression*)var;

        varent.var = (ast_expression*)fld;
        if (var->expression.vtype == TYPE_VECTOR)
        {
            /* create _x, _y and _z fields as well */
            parseerror(parser, "TODO: vector field members (_x,_y,_z)");
            ast_delete(fld);
            return false;
        }

        varent.name = util_strdup(fld->name);
        (void)!parser_t_fields_add(parser, varent);

        /* end with a semicolon */
        if (!parser_next(parser) || parser->tok != ';') {
            parseerror(parser, "semicolon expected");
            return false;
        }

        /* skip the semicolon */
        if (!parser_next(parser))
            return parser->tok == TOKEN_EOF;

        return true;
    }
    else
    {
        parseerror(parser, "unexpected token: %s", parser->lex->tok->value);
        return false;
    }
    return true;
}

static parser_t *parser;

bool parser_init()
{
    parser = (parser_t*)mem_a(sizeof(parser_t));
    if (!parser)
        return false;

    memset(parser, 0, sizeof(parser));

    MEM_VECTOR_INIT(parser, globals);
    MEM_VECTOR_INIT(parser, locals);
    return true;
}

bool parser_compile(const char *filename)
{
    parser->lex = lex_open(filename);
    if (!parser->lex) {
        printf("failed to open file \"%s\"\n", filename);
        return false;
    }

    /* initial lexer/parser state */
    parser->lex->flags.noops = true;

    if (parser_next(parser))
    {
        while (parser->tok != TOKEN_EOF && parser->tok < TOKEN_ERROR)
        {
            if (!parser_do(parser)) {
                if (parser->tok == TOKEN_EOF)
                    parseerror(parser, "unexpected eof");
                else if (!parser->errors)
                    parseerror(parser, "parse error\n");
                lex_close(parser->lex);
                mem_d(parser);
                return false;
            }
        }
    }

    lex_close(parser->lex);

    return !parser->errors;
}

void parser_cleanup()
{
    size_t i;
    for (i = 0; i < parser->functions_count; ++i) {
        ast_delete(parser->functions[i]);
    }
    for (i = 0; i < parser->imm_vector_count; ++i) {
        ast_delete(parser->imm_vector[i]);
    }
    for (i = 0; i < parser->imm_string_count; ++i) {
        ast_delete(parser->imm_string[i]);
    }
    for (i = 0; i < parser->imm_float_count; ++i) {
        ast_delete(parser->imm_float[i]);
    }
    for (i = 0; i < parser->globals_count; ++i) {
        ast_delete(parser->globals[i].var);
        mem_d(parser->globals[i].name);
    }
    MEM_VECTOR_CLEAR(parser, globals);

    mem_d(parser);
}

bool parser_finish(const char *output)
{
    size_t i;
    ir_builder *ir;

    if (!parser->errors)
    {
        ir = ir_builder_new("gmqcc_out");
        if (!ir) {
            printf("failed to allocate builder\n");
            return false;
        }

        for (i = 0; i < parser->imm_float_count; ++i) {
            if (!ast_global_codegen(parser->imm_float[i], ir)) {
                printf("failed to generate global %s\n", parser->imm_float[i]->name);
                ir_builder_delete(ir);
                return false;
            }
        }
        for (i = 0; i < parser->imm_string_count; ++i) {
            if (!ast_global_codegen(parser->imm_string[i], ir)) {
                printf("failed to generate global %s\n", parser->imm_string[i]->name);
                ir_builder_delete(ir);
                return false;
            }
        }
        for (i = 0; i < parser->imm_vector_count; ++i) {
            if (!ast_global_codegen(parser->imm_vector[i], ir)) {
                printf("failed to generate global %s\n", parser->imm_vector[i]->name);
                ir_builder_delete(ir);
                return false;
            }
        }
        for (i = 0; i < parser->fields_count; ++i) {
            ast_value *field;
            bool isconst;
            if (!ast_istype(parser->fields[i].var, ast_value))
                continue;
            field = (ast_value*)parser->fields[i].var;
            isconst = field->isconst;
            field->isconst = false;
            if (!ast_global_codegen((ast_value*)field, ir)) {
                printf("failed to generate field %s\n", field->name);
                ir_builder_delete(ir);
                return false;
            }
            if (isconst) {
                ir_value *ifld;
                ast_expression *subtype;
                field->isconst = true;
                subtype = field->expression.next;
                ifld = ir_builder_create_field(ir, field->name, subtype->expression.vtype);
                if (subtype->expression.vtype == TYPE_FIELD)
                    ifld->fieldtype = subtype->expression.next->expression.vtype;
                else if (subtype->expression.vtype == TYPE_FUNCTION)
                    ifld->outtype = subtype->expression.next->expression.vtype;
                (void)!ir_value_set_field(field->ir_v, ifld);
            }
        }
        for (i = 0; i < parser->globals_count; ++i) {
            if (!ast_istype(parser->globals[i].var, ast_value))
                continue;
            if (!ast_global_codegen((ast_value*)(parser->globals[i].var), ir)) {
                printf("failed to generate global %s\n", parser->globals[i].name);
                ir_builder_delete(ir);
                return false;
            }
        }
        for (i = 0; i < parser->functions_count; ++i) {
            if (!ast_function_codegen(parser->functions[i], ir)) {
                printf("failed to generate function %s\n", parser->functions[i]->name);
                ir_builder_delete(ir);
                return false;
            }
            if (!ir_function_finalize(parser->functions[i]->ir_func)) {
                printf("failed to finalize function %s\n", parser->functions[i]->name);
                ir_builder_delete(ir);
                return false;
            }
        }

        if (opts_dump)
            ir_builder_dump(ir, printf);

        if (!ir_builder_generate(ir, output)) {
            printf("*** failed to generate output file\n");
            ir_builder_delete(ir);
            return false;
        }

        ir_builder_delete(ir);
        return true;
    }

    printf("*** there were compile errors\n");
    return false;
}
