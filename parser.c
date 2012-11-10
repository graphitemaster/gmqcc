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

    ast_value *imm_float_zero;
    ast_value *imm_vector_zero;

    size_t crc_globals;
    size_t crc_fields;

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

static bool GMQCC_WARN parser_pop_local(parser_t *parser);
static bool parse_variable(parser_t *parser, ast_block *localblock, bool nofields);
static ast_block* parse_block(parser_t *parser, bool warnreturn);
static bool parse_block_into(parser_t *parser, ast_block *block, bool warnreturn);
static ast_expression* parse_statement_or_block(parser_t *parser);
static ast_expression* parse_expression_leave(parser_t *parser, bool stopatcomma);
static ast_expression* parse_expression(parser_t *parser, bool stopatcomma);

static void parseerror(parser_t *parser, const char *fmt, ...)
{
	va_list ap;

	parser->errors++;

	va_start(ap, fmt);
    vprintmsg(LVL_ERROR, parser->lex->tok.ctx.file, parser->lex->tok.ctx.line, "parse error", fmt, ap);
	va_end(ap);
}

/* returns true if it counts as an error */
static bool GMQCC_WARN parsewarning(parser_t *parser, int warntype, const char *fmt, ...)
{
	va_list ap;
	int lvl = LVL_WARNING;

    if (!OPTS_WARN(warntype))
        return false;

    if (opts_werror) {
	    parser->errors++;
	    lvl = LVL_ERROR;
	}

	va_start(ap, fmt);
    vprintmsg(lvl, parser->lex->tok.ctx.file, parser->lex->tok.ctx.line, "warning", fmt, ap);
	va_end(ap);

	return opts_werror;
}

static bool GMQCC_WARN genwarning(lex_ctx ctx, int warntype, const char *fmt, ...)
{
	va_list ap;
	int lvl = LVL_WARNING;

    if (!OPTS_WARN(warntype))
        return false;

    if (opts_werror)
	    lvl = LVL_ERROR;

	va_start(ap, fmt);
    vprintmsg(lvl, ctx.file, ctx.line, "warning", fmt, ap);
	va_end(ap);

	return opts_werror;
}

/**********************************************************************
 * some maths used for constant folding
 */

vector vec3_add(vector a, vector b)
{
    vector out;
    out.x = a.x + b.x;
    out.y = a.y + b.y;
    out.z = a.z + b.z;
    return out;
}

vector vec3_sub(vector a, vector b)
{
    vector out;
    out.x = a.x - b.x;
    out.y = a.y - b.y;
    out.z = a.z - b.z;
    return out;
}

qcfloat vec3_mulvv(vector a, vector b)
{
    return (a.x * b.x + a.y * b.y + a.z * b.z);
}

vector vec3_mulvf(vector a, float b)
{
    vector out;
    out.x = a.x * b;
    out.y = a.y * b;
    out.z = a.z * b;
    return out;
}

/**********************************************************************
 * parsing
 */

bool parser_next(parser_t *parser)
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
#define parser_ctx(p)    ((p)->lex->tok.ctx)

static ast_value* parser_const_float(parser_t *parser, double d)
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

static ast_value* parser_const_float_0(parser_t *parser)
{
    if (!parser->imm_float_zero)
        parser->imm_float_zero = parser_const_float(parser, 0);
    return parser->imm_float_zero;
}

static char *parser_strdup(const char *str)
{
    if (str && !*str) {
        /* actually dup empty strings */
        char *out = mem_a(1);
        *out = 0;
        return out;
    }
    return util_strdup(str);
}

static ast_value* parser_const_string(parser_t *parser, const char *str)
{
    size_t i;
    ast_value *out;
    for (i = 0; i < parser->imm_string_count; ++i) {
        if (!strcmp(parser->imm_string[i]->constval.vstring, str))
            return parser->imm_string[i];
    }
    out = ast_value_new(parser_ctx(parser), "#IMMEDIATE", TYPE_STRING);
    out->isconst = true;
    out->constval.vstring = parser_strdup(str);
    if (!parser_t_imm_string_add(parser, out)) {
        ast_value_delete(out);
        return NULL;
    }
    return out;
}

static ast_value* parser_const_vector(parser_t *parser, vector v)
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

static ast_value* parser_const_vector_f(parser_t *parser, float x, float y, float z)
{
    vector v;
    v.x = x;
    v.y = y;
    v.z = z;
    return parser_const_vector(parser, v);
}

static ast_value* parser_const_vector_0(parser_t *parser)
{
    if (!parser->imm_vector_zero)
        parser->imm_vector_zero = parser_const_vector_f(parser, 0, 0, 0);
    return parser->imm_vector_zero;
}

static ast_expression* parser_find_field(parser_t *parser, const char *name)
{
    size_t i;
    for (i = 0; i < parser->fields_count; ++i) {
        if (!strcmp(parser->fields[i].name, name))
            return parser->fields[i].var;
    }
    return NULL;
}

static ast_expression* parser_find_global(parser_t *parser, const char *name)
{
    size_t i;
    for (i = 0; i < parser->globals_count; ++i) {
        if (!strcmp(parser->globals[i].name, name))
            return parser->globals[i].var;
    }
    return NULL;
}

static ast_expression* parser_find_param(parser_t *parser, const char *name)
{
    size_t i;
    ast_value *fun;
    if (!parser->function)
        return NULL;
    fun = parser->function->vtype;
    for (i = 0; i < fun->expression.params_count; ++i) {
        if (!strcmp(fun->expression.params[i]->name, name))
            return (ast_expression*)(fun->expression.params[i]);
    }
    return NULL;
}

static ast_expression* parser_find_local(parser_t *parser, const char *name, size_t upto, bool *isparam)
{
    size_t i;
    *isparam = false;
    for (i = parser->locals_count; i > upto;) {
        --i;
        if (!strcmp(parser->locals[i].name, name))
            return parser->locals[i].var;
    }
    *isparam = true;
    return parser_find_param(parser, name);
}

static ast_expression* parser_find_var(parser_t *parser, const char *name)
{
    bool dummy;
    ast_expression *v;
    v         = parser_find_local(parser, name, 0, &dummy);
    if (!v) v = parser_find_global(parser, name);
    return v;
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
    e.off   = 0;
    e.out   = v;
    e.block = NULL;
    e.ctx   = ctx;
    e.paren = 0;
    return e;
}

static sy_elem syblock(lex_ctx ctx, ast_block *v) {
    sy_elem e;
    e.etype = 0;
    e.off   = 0;
    e.out   = (ast_expression*)v;
    e.block = v;
    e.ctx   = ctx;
    e.paren = 0;
    return e;
}

static sy_elem syop(lex_ctx ctx, const oper_info *op) {
    sy_elem e;
    e.etype = 1 + (op - operators);
    e.off   = 0;
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
    ast_value      *asvalue[3];
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
        parseerror(parser, "internal error: not enough operands: %i (operator %s (%i))", sy->out_count,
                   op->op, (int)op->id);
        return false;
    }

    sy->ops_count--;

    sy->out_count -= op->operands;
    for (i = 0; i < op->operands; ++i) {
        exprs[i]  = sy->out[sy->out_count+i].out;
        blocks[i] = sy->out[sy->out_count+i].block;
        asvalue[i] = (ast_value*)exprs[i];
    }

    if (blocks[0] && !blocks[0]->exprs_count && op->id != opid1(',')) {
        parseerror(parser, "internal error: operator cannot be applied on empty blocks");
        return false;
    }

#define NotSameType(T) \
             (exprs[0]->expression.vtype != exprs[1]->expression.vtype || \
              exprs[0]->expression.vtype != T)
#define CanConstFold1(A) \
             (ast_istype((A), ast_value) && ((ast_value*)(A))->isconst)
#define CanConstFold(A, B) \
             (CanConstFold1(A) && CanConstFold1(B))
#define ConstV(i) (asvalue[(i)]->constval.vvec)
#define ConstF(i) (asvalue[(i)]->constval.vfloat)
#define ConstS(i) (asvalue[(i)]->constval.vstring)
    switch (op->id)
    {
        default:
            parseerror(parser, "internal error: unhandled operator: %s (%i)", op->op, (int)op->id);
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

        case opid2('-','P'):
            switch (exprs[0]->expression.vtype) {
                case TYPE_FLOAT:
                    if (CanConstFold1(exprs[0]))
                        out = (ast_expression*)parser_const_float(parser, -ConstF(0));
                    else
                        out = (ast_expression*)ast_binary_new(ctx, INSTR_SUB_F,
                                                              (ast_expression*)parser_const_float_0(parser),
                                                              exprs[0]);
                    break;
                case TYPE_VECTOR:
                    if (CanConstFold1(exprs[0]))
                        out = (ast_expression*)parser_const_vector_f(parser,
                            -ConstV(0).x, -ConstV(0).y, -ConstV(0).z);
                    else
                        out = (ast_expression*)ast_binary_new(ctx, INSTR_SUB_V,
                                                              (ast_expression*)parser_const_vector_0(parser),
                                                              exprs[0]);
                    break;
                default:
                parseerror(parser, "invalid types used in expression: cannot negate type %s",
                           type_name[exprs[0]->expression.vtype]);
                return false;
            }
            break;

        case opid2('!','P'):
            switch (exprs[0]->expression.vtype) {
                case TYPE_FLOAT:
                    if (CanConstFold1(exprs[0]))
                        out = (ast_expression*)parser_const_float(parser, !ConstF(0));
                    else
                        out = (ast_expression*)ast_unary_new(ctx, INSTR_NOT_F, exprs[0]);
                    break;
                case TYPE_VECTOR:
                    if (CanConstFold1(exprs[0]))
                        out = (ast_expression*)parser_const_float(parser,
                            (!ConstV(0).x && !ConstV(0).y && !ConstV(0).z));
                    else
                        out = (ast_expression*)ast_unary_new(ctx, INSTR_NOT_V, exprs[0]);
                    break;
                case TYPE_STRING:
                    if (CanConstFold1(exprs[0]))
                        out = (ast_expression*)parser_const_float(parser, !ConstS(0) || !*ConstS(0));
                    else
                        out = (ast_expression*)ast_unary_new(ctx, INSTR_NOT_S, exprs[0]);
                    break;
                /* we don't constant-fold NOT for these types */
                case TYPE_ENTITY:
                    out = (ast_expression*)ast_unary_new(ctx, INSTR_NOT_ENT, exprs[0]);
                    break;
                case TYPE_FUNCTION:
                    out = (ast_expression*)ast_unary_new(ctx, INSTR_NOT_FNC, exprs[0]);
                    break;
                default:
                parseerror(parser, "invalid types used in expression: cannot logically negate type %s",
                           type_name[exprs[0]->expression.vtype]);
                return false;
            }
            break;

        case opid1('+'):
            if (exprs[0]->expression.vtype != exprs[1]->expression.vtype ||
                (exprs[0]->expression.vtype != TYPE_VECTOR && exprs[0]->expression.vtype != TYPE_FLOAT) )
            {
                parseerror(parser, "invalid types used in expression: cannot add type %s and %s",
                           type_name[exprs[0]->expression.vtype],
                           type_name[exprs[1]->expression.vtype]);
                return false;
            }
            switch (exprs[0]->expression.vtype) {
                case TYPE_FLOAT:
                    if (CanConstFold(exprs[0], exprs[1]))
                    {
                        out = (ast_expression*)parser_const_float(parser, ConstF(0) + ConstF(1));
                    }
                    else
                        out = (ast_expression*)ast_binary_new(ctx, INSTR_ADD_F, exprs[0], exprs[1]);
                    break;
                case TYPE_VECTOR:
                    if (CanConstFold(exprs[0], exprs[1]))
                        out = (ast_expression*)parser_const_vector(parser, vec3_add(ConstV(0), ConstV(1)));
                    else
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
            if (exprs[0]->expression.vtype != exprs[1]->expression.vtype ||
                (exprs[0]->expression.vtype != TYPE_VECTOR && exprs[0]->expression.vtype != TYPE_FLOAT) )
            {
                parseerror(parser, "invalid types used in expression: cannot subtract type %s from %s",
                           type_name[exprs[1]->expression.vtype],
                           type_name[exprs[0]->expression.vtype]);
                return false;
            }
            switch (exprs[0]->expression.vtype) {
                case TYPE_FLOAT:
                    if (CanConstFold(exprs[0], exprs[1]))
                        out = (ast_expression*)parser_const_float(parser, ConstF(0) - ConstF(1));
                    else
                        out = (ast_expression*)ast_binary_new(ctx, INSTR_SUB_F, exprs[0], exprs[1]);
                    break;
                case TYPE_VECTOR:
                    if (CanConstFold(exprs[0], exprs[1]))
                        out = (ast_expression*)parser_const_vector(parser, vec3_sub(ConstV(0), ConstV(1)));
                    else
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
                    {
                        if (CanConstFold(exprs[0], exprs[1]))
                            out = (ast_expression*)parser_const_vector(parser, vec3_mulvf(ConstV(1), ConstF(0)));
                        else
                            out = (ast_expression*)ast_binary_new(ctx, INSTR_MUL_FV, exprs[0], exprs[1]);
                    }
                    else
                    {
                        if (CanConstFold(exprs[0], exprs[1]))
                            out = (ast_expression*)parser_const_float(parser, ConstF(0) * ConstF(1));
                        else
                            out = (ast_expression*)ast_binary_new(ctx, INSTR_MUL_F, exprs[0], exprs[1]);
                    }
                    break;
                case TYPE_VECTOR:
                    if (exprs[1]->expression.vtype == TYPE_FLOAT)
                    {
                        if (CanConstFold(exprs[0], exprs[1]))
                            out = (ast_expression*)parser_const_vector(parser, vec3_mulvf(ConstV(0), ConstF(1)));
                        else
                            out = (ast_expression*)ast_binary_new(ctx, INSTR_MUL_VF, exprs[0], exprs[1]);
                    }
                    else
                    {
                        if (CanConstFold(exprs[0], exprs[1]))
                            out = (ast_expression*)parser_const_float(parser, vec3_mulvv(ConstV(0), ConstV(1)));
                        else
                            out = (ast_expression*)ast_binary_new(ctx, INSTR_MUL_V, exprs[0], exprs[1]);
                    }
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
            if (CanConstFold(exprs[0], exprs[1]))
                out = (ast_expression*)parser_const_float(parser, ConstF(0) / ConstF(1));
            else
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
            if (CanConstFold(exprs[0], exprs[1]))
                out = (ast_expression*)parser_const_float(parser,
                    (op->id == opid1('|') ? (float)( ((qcint)ConstF(0)) | ((qcint)ConstF(1)) ) :
                                            (float)( ((qcint)ConstF(0)) & ((qcint)ConstF(1)) ) ));
            else
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
            if (CanConstFold(exprs[0], exprs[1]))
                out = (ast_expression*)parser_const_float(parser,
                    (generated_op == INSTR_OR ? (ConstF(0) || ConstF(1)) : (ConstF(0) && ConstF(1))));
            else
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
            if (ast_istype(exprs[0], ast_entfield)) {
                ast_expression *field = ((ast_entfield*)exprs[0])->field;
                assignop = type_storep_instr[exprs[0]->expression.vtype];
                if (!ast_compare_type(field->expression.next, exprs[1])) {
                    char ty1[1024];
                    char ty2[1024];
                    ast_type_to_string(field->expression.next, ty1, sizeof(ty1));
                    ast_type_to_string(exprs[1], ty2, sizeof(ty2));
                    if (opts_standard == COMPILER_QCC &&
                        field->expression.next->expression.vtype == TYPE_FUNCTION &&
                        exprs[1]->expression.vtype == TYPE_FUNCTION)
                    {
                        if (parsewarning(parser, WARN_ASSIGN_FUNCTION_TYPES,
                                         "invalid types in assignment: cannot assign %s to %s", ty2, ty1))
                        {
                            parser->errors++;
                        }
                    }
                    else
                        parseerror(parser, "invalid types in assignment: cannot assign %s to %s", ty2, ty1);
                }
            }
            else
            {
                assignop = type_store_instr[exprs[0]->expression.vtype];
                if (!ast_compare_type(exprs[0], exprs[1])) {
                    char ty1[1024];
                    char ty2[1024];
                    ast_type_to_string(exprs[0], ty1, sizeof(ty1));
                    ast_type_to_string(exprs[1], ty2, sizeof(ty2));
                    if (opts_standard == COMPILER_QCC &&
                        exprs[0]->expression.vtype == TYPE_FUNCTION &&
                        exprs[1]->expression.vtype == TYPE_FUNCTION)
                    {
                        if (parsewarning(parser, WARN_ASSIGN_FUNCTION_TYPES,
                                         "invalid types in assignment: cannot assign %s to %s", ty2, ty1))
                        {
                            parser->errors++;
                        }
                    }
                    else
                        parseerror(parser, "invalid types in assignment: cannot assign %s to %s", ty2, ty1);
                }
            }
            out = (ast_expression*)ast_store_new(ctx, assignop, exprs[0], exprs[1]);
            break;
        case opid2('+','='):
        case opid2('-','='):
            if (exprs[0]->expression.vtype != exprs[1]->expression.vtype ||
                (exprs[0]->expression.vtype != TYPE_VECTOR && exprs[0]->expression.vtype != TYPE_FLOAT) )
            {
                parseerror(parser, "invalid types used in expression: cannot add or subtract type %s and %s",
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
                    out = (ast_expression*)ast_binstore_new(ctx, assignop,
                                                            (op->id == opid2('+','=') ? INSTR_ADD_F : INSTR_SUB_F),
                                                            exprs[0], exprs[1]);
                    break;
                case TYPE_VECTOR:
                    out = (ast_expression*)ast_binstore_new(ctx, assignop,
                                                            (op->id == opid2('+','=') ? INSTR_ADD_V : INSTR_SUB_V),
                                                            exprs[0], exprs[1]);
                    break;
                default:
                    parseerror(parser, "invalid types used in expression: cannot add or subtract type %s and %s",
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
        if (!ast_call_check_types(call))
            parser->errors++;
    } else {
        parseerror(parser, "invalid function call");
        return false;
    }

    /* overwrite fid, the function, with a call */
    sy->out[fid] = syexp(call->expression.node.context, (ast_expression*)call);

    if (fun->expression.vtype != TYPE_FUNCTION) {
        parseerror(parser, "not a function (%s)", type_name[fun->expression.vtype]);
        return false;
    }

    if (!fun->expression.next) {
        parseerror(parser, "could not determine function return type");
        return false;
    } else {
        if (fun->expression.params_count != paramcount &&
            !(fun->expression.variadic &&
              fun->expression.params_count < paramcount))
        {
            ast_value *fval;
            const char *fewmany = (fun->expression.params_count > paramcount) ? "few" : "many";

            fval = (ast_istype(fun, ast_value) ? ((ast_value*)fun) : NULL);
            if (opts_standard == COMPILER_GMQCC)
            {
                if (fval)
                    parseerror(parser, "too %s parameters for call to %s: expected %i, got %i\n"
                               " -> `%s` has been declared here: %s:%i",
                               fewmany, fval->name, (int)fun->expression.params_count, (int)paramcount,
                               fval->name, ast_ctx(fun).file, (int)ast_ctx(fun).line);
                else
                    parseerror(parser, "too %s parameters for function call: expected %i, got %i\n"
                               " -> `%s` has been declared here: %s:%i",
                               fewmany, fval->name, (int)fun->expression.params_count, (int)paramcount,
                               fval->name, ast_ctx(fun).file, (int)ast_ctx(fun).line);
                return false;
            }
            else
            {
                if (fval)
                    return !parsewarning(parser, WARN_TOO_FEW_PARAMETERS,
                                         "too %s parameters for call to %s: expected %i, got %i\n"
                                         " -> `%s` has been declared here: %s:%i",
                                         fewmany, fval->name, (int)fun->expression.params_count, (int)paramcount,
                                         fval->name, ast_ctx(fun).file, (int)ast_ctx(fun).line);
                else
                    return !parsewarning(parser, WARN_TOO_FEW_PARAMETERS,
                                         "too %s parameters for function call: expected %i, got %i\n"
                                         " -> `%s` has been declared here: %s:%i",
                                         fewmany, fval->name, (int)fun->expression.params_count, (int)paramcount,
                                         fval->name, ast_ctx(fun).file, (int)ast_ctx(fun).line);
            }
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
    /* this would for bit a + (x) because there are no operators inside (x)
    if (sy->ops[sy->ops_count-1].paren == 1) {
        parseerror(parser, "empty parenthesis expression");
        return false;
    }
    */
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

static void parser_reclassify_token(parser_t *parser)
{
    size_t i;
    for (i = 0; i < operator_count; ++i) {
        if (!strcmp(parser_tokval(parser), operators[i].op)) {
            parser->tok = TOKEN_OPERATOR;
            return;
        }
    }
}

static ast_expression* parse_expression_leave(parser_t *parser, bool stopatcomma)
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

    parser->lex->flags.noops = false;

    parser_reclassify_token(parser);

    while (true)
    {
        if (gotmemberof)
            gotmemberof = false;
        else
            parser->memberof = 0;

        if (parser->tok == TOKEN_IDENT)
        {
            ast_expression *var;
            if (wantop) {
                parseerror(parser, "expected operator or end of statement");
                goto onerr;
            }
            wantop = true;
            /* variable */
            if (opts_standard == COMPILER_GMQCC)
            {
                if (parser->memberof == TYPE_ENTITY) {
                    /* still get vars first since there could be a fieldpointer */
                    var = parser_find_var(parser, parser_tokval(parser));
                    if (!var)
                        var = parser_find_field(parser, parser_tokval(parser));
                }
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
            if (ast_istype(var, ast_value))
                ((ast_value*)var)->uses++;
            if (!shunt_out_add(&sy, syexp(parser_ctx(parser), var))) {
                parseerror(parser, "out of memory");
                goto onerr;
            }
            DEBUGSHUNTDO(printf("push %s\n", parser_tokval(parser)));
        }
        else if (parser->tok == TOKEN_FLOATCONST) {
            ast_value *val;
            if (wantop) {
                parseerror(parser, "expected operator or end of statement, got constant");
                goto onerr;
            }
            wantop = true;
            val = parser_const_float(parser, (parser_token(parser)->constval.f));
            if (!val)
                return false;
            if (!shunt_out_add(&sy, syexp(parser_ctx(parser), (ast_expression*)val))) {
                parseerror(parser, "out of memory");
                goto onerr;
            }
            DEBUGSHUNTDO(printf("push %g\n", parser_token(parser)->constval.f));
        }
        else if (parser->tok == TOKEN_INTCONST) {
            ast_value *val;
            if (wantop) {
                parseerror(parser, "expected operator or end of statement, got constant");
                goto onerr;
            }
            wantop = true;
            val = parser_const_float(parser, (double)(parser_token(parser)->constval.i));
            if (!val)
                return false;
            if (!shunt_out_add(&sy, syexp(parser_ctx(parser), (ast_expression*)val))) {
                parseerror(parser, "out of memory");
                goto onerr;
            }
            DEBUGSHUNTDO(printf("push %i\n", parser_token(parser)->constval.i));
        }
        else if (parser->tok == TOKEN_STRINGCONST) {
            ast_value *val;
            if (wantop) {
                parseerror(parser, "expected operator or end of statement, got constant");
                goto onerr;
            }
            wantop = true;
            val = parser_const_string(parser, parser_tokval(parser));
            if (!val)
                return false;
            if (!shunt_out_add(&sy, syexp(parser_ctx(parser), (ast_expression*)val))) {
                parseerror(parser, "out of memory");
                goto onerr;
            }
            DEBUGSHUNTDO(printf("push string\n"));
        }
        else if (parser->tok == TOKEN_VECTORCONST) {
            ast_value *val;
            if (wantop) {
                parseerror(parser, "expected operator or end of statement, got constant");
                goto onerr;
            }
            wantop = true;
            val = parser_const_vector(parser, parser_token(parser)->constval.v);
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
            parseerror(parser, "internal error: '(' should be classified as operator");
            goto onerr;
        }
        else if (parser->tok == ')') {
            if (wantop) {
                DEBUGSHUNTDO(printf("do[op] )\n"));
                --parens;
                if (parens < 0)
                    break;
                /* we do expect an operator next */
                /* closing an opening paren */
                if (!parser_close_paren(parser, &sy, false))
                    goto onerr;
            } else {
                DEBUGSHUNTDO(printf("do[nop] )\n"));
                --parens;
                if (parens < 0)
                    break;
                /* allowed for function calls */
                if (!parser_close_paren(parser, &sy, true))
                    goto onerr;
            }
            wantop = true;
        }
        else if (parser->tok != TOKEN_OPERATOR) {
            if (wantop) {
                parseerror(parser, "expected operator or end of statement");
                goto onerr;
            }
            break;
        }
        else
        {
            /* classify the operator */
            /* TODO: suffix operators */
            const oper_info *op;
            const oper_info *olast = NULL;
            size_t o;
            for (o = 0; o < operator_count; ++o) {
                if ((!(operators[o].flags & OP_PREFIX) == wantop) &&
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

            /* when declaring variables, a comma starts a new variable */
            if (op->id == opid1(',') && !parens && stopatcomma) {
                /* fixup the token */
                parser->tok = ',';
                break;
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

            if (op->id == opid1('.') && opts_standard == COMPILER_GMQCC) {
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

            if (op->id == opid1('(')) {
                if (wantop) {
                    DEBUGSHUNTDO(printf("push [op] (\n"));
                    ++parens;
                    /* we expected an operator, this is the function-call operator */
                    if (!shunt_ops_add(&sy, syparen(parser_ctx(parser), 'f', sy.out_count-1))) {
                        parseerror(parser, "out of memory");
                        goto onerr;
                    }
                } else {
                    ++parens;
                    if (!shunt_ops_add(&sy, syparen(parser_ctx(parser), 1, 0))) {
                        parseerror(parser, "out of memory");
                        goto onerr;
                    }
                    DEBUGSHUNTDO(printf("push [nop] (\n"));
                }
                wantop = false;
            } else {
                DEBUGSHUNTDO(printf("push operator %s\n", op->op));
                if (!shunt_ops_add(&sy, syop(parser_ctx(parser), op)))
                    goto onerr;
                wantop = false;
            }
        }
        if (!parser_next(parser)) {
            goto onerr;
        }
        if (parser->tok == ';' || parser->tok == ']') {
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
    DEBUGSHUNTDO(printf("shunt done\n"));
    return expr;

onerr:
    parser->lex->flags.noops = true;
    MEM_VECTOR_CLEAR(&sy, out);
    MEM_VECTOR_CLEAR(&sy, ops);
    return NULL;
}

static ast_expression* parse_expression(parser_t *parser, bool stopatcomma)
{
    ast_expression *e = parse_expression_leave(parser, stopatcomma);
    if (!e)
        return NULL;
    if (!parser_next(parser)) {
        ast_delete(e);
        return NULL;
    }
    return e;
}

static bool parse_if(parser_t *parser, ast_block *block, ast_expression **out)
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
    cond = parse_expression_leave(parser, false);
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
    ontrue = parse_statement_or_block(parser);
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
        onfalse = parse_statement_or_block(parser);
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

static bool parse_while(parser_t *parser, ast_block *block, ast_expression **out)
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
    cond = parse_expression_leave(parser, false);
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
    ontrue = parse_statement_or_block(parser);
    if (!ontrue) {
        ast_delete(cond);
        return false;
    }

    aloop = ast_loop_new(ctx, NULL, cond, NULL, NULL, ontrue);
    *out = (ast_expression*)aloop;
    return true;
}

static bool parse_dowhile(parser_t *parser, ast_block *block, ast_expression **out)
{
    ast_loop *aloop;
    ast_expression *cond, *ontrue;

    lex_ctx ctx = parser_ctx(parser);

    /* skip the 'do' and get the body */
    if (!parser_next(parser)) {
        parseerror(parser, "expected loop body");
        return false;
    }
    ontrue = parse_statement_or_block(parser);
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
    cond = parse_expression_leave(parser, false);
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

static bool parse_for(parser_t *parser, ast_block *block, ast_expression **out)
{
    ast_loop *aloop;
    ast_expression *initexpr, *cond, *increment, *ontrue;
    size_t oldblocklocal;
    bool   retval = true;

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
        if (!parse_variable(parser, block, true))
            goto onerr;
    }
    else if (parser->tok != ';')
    {
        initexpr = parse_expression_leave(parser, false);
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
        cond = parse_expression_leave(parser, false);
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
        increment = parse_expression_leave(parser, false);
        if (!increment)
            goto onerr;
        if (!ast_istype(increment, ast_store) &&
            !ast_istype(increment, ast_call) &&
            !ast_istype(increment, ast_binstore))
        {
            if (genwarning(ast_ctx(increment), WARN_EFFECTLESS_STATEMENT, "statement has no effect"))
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
    ontrue = parse_statement_or_block(parser);
    if (!ontrue) {
        goto onerr;
    }

    aloop = ast_loop_new(ctx, initexpr, cond, NULL, increment, ontrue);
    *out = (ast_expression*)aloop;

    while (parser->locals_count > parser->blocklocal)
        retval = retval && parser_pop_local(parser);
    parser->blocklocal = oldblocklocal;
    return retval;
onerr:
    if (initexpr)  ast_delete(initexpr);
    if (cond)      ast_delete(cond);
    if (increment) ast_delete(increment);
    while (parser->locals_count > parser->blocklocal)
        (void)!parser_pop_local(parser);
    parser->blocklocal = oldblocklocal;
    return false;
}

static bool parse_statement(parser_t *parser, ast_block *block, ast_expression **out)
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
        if (!parse_variable(parser, block, false))
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
            if (!parse_variable(parser, block, true))
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
                exp = parse_expression(parser, false);
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
            } else {
                if (!parser_next(parser))
                    parseerror(parser, "parse error");
                if (expected->expression.next->expression.vtype != TYPE_VOID) {
                    if (opts_standard != COMPILER_GMQCC)
                        (void)!parsewarning(parser, WARN_MISSING_RETURN_VALUES, "return without value");
                    else
                        parseerror(parser, "return without value");
                }
                ret = ast_return_new(parser_ctx(parser), NULL);
            }
            *out = (ast_expression*)ret;
            return true;
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
            if (opts_standard == COMPILER_QCC) {
                if (parsewarning(parser, WARN_EXTENSIONS, "for loops are not recognized in the original Quake C standard, to enable try an alternate standard --std=?"))
                    return false;
            }
            return parse_for(parser, block, out);
        }
        parseerror(parser, "Unexpected keyword");
        return false;
    }
    else if (parser->tok == '{')
    {
        ast_block *inner;
        inner = parse_block(parser, false);
        if (!inner)
            return false;
        *out = (ast_expression*)inner;
        return true;
    }
    else
    {
        ast_expression *exp = parse_expression(parser, false);
        if (!exp)
            return false;
        *out = exp;
        if (!ast_istype(exp, ast_store) &&
            !ast_istype(exp, ast_call) &&
            !ast_istype(exp, ast_binstore))
        {
            if (genwarning(ast_ctx(exp), WARN_EFFECTLESS_STATEMENT, "statement has no effect"))
                return false;
        }
        return true;
    }
}

static bool GMQCC_WARN parser_pop_local(parser_t *parser)
{
    varentry_t *ve;
    parser->locals_count--;

    ve = &parser->locals[parser->locals_count];
    if (ast_istype(ve->var, ast_value) && !(((ast_value*)(ve->var))->uses)) {
        if (parsewarning(parser, WARN_UNUSED_VARIABLE, "unused variable: `%s`", ve->name))
            return false;
    }
    mem_d(parser->locals[parser->locals_count].name);
    return true;
}

static bool parse_block_into(parser_t *parser, ast_block *block, bool warnreturn)
{
    size_t oldblocklocal;
    bool   retval = true;

    oldblocklocal = parser->blocklocal;
    parser->blocklocal = parser->locals_count;

    if (!parser_next(parser)) { /* skip the '{' */
        parseerror(parser, "expected function body");
        goto cleanup;
    }

    while (parser->tok != TOKEN_EOF && parser->tok < TOKEN_ERROR)
    {
        ast_expression *expr;
        if (parser->tok == '}')
            break;

        if (!parse_statement(parser, block, &expr)) {
            /* parseerror(parser, "parse error"); */
            block = NULL;
            goto cleanup;
        }
        if (!expr)
            continue;
        if (!ast_block_exprs_add(block, expr)) {
            ast_delete(expr);
            block = NULL;
            goto cleanup;
        }
    }

    if (parser->tok != '}') {
        block = NULL;
    } else {
        if (warnreturn && parser->function->vtype->expression.next->expression.vtype != TYPE_VOID)
        {
            if (!block->exprs_count ||
                !ast_istype(block->exprs[block->exprs_count-1], ast_return))
            {
                if (parsewarning(parser, WARN_MISSING_RETURN_VALUES, "control reaches end of non-void function")) {
                    block = NULL;
                    goto cleanup;
                }
            }
        }
        (void)parser_next(parser);
    }

cleanup:
    while (parser->locals_count > parser->blocklocal)
        retval = retval && parser_pop_local(parser);
    parser->blocklocal = oldblocklocal;
    return !!block;
}

static ast_block* parse_block(parser_t *parser, bool warnreturn)
{
    ast_block *block;
    block = ast_block_new(parser_ctx(parser));
    if (!block)
        return NULL;
    if (!parse_block_into(parser, block, warnreturn)) {
        ast_block_delete(block);
        return NULL;
    }
    return block;
}

static ast_expression* parse_statement_or_block(parser_t *parser)
{
    ast_expression *expr = NULL;
    if (parser->tok == '{')
        return (ast_expression*)parse_block(parser, false);
    if (!parse_statement(parser, NULL, &expr))
        return NULL;
    return expr;
}

/* loop method */
static bool create_vector_members(parser_t *parser, ast_value *var, varentry_t *ve)
{
    size_t i;
    size_t len = strlen(var->name);

    for (i = 0; i < 3; ++i) {
        ve[i].var = (ast_expression*)ast_member_new(ast_ctx(var), (ast_expression*)var, i);
        if (!ve[i].var)
            break;

        ve[i].name = (char*)mem_a(len+3);
        if (!ve[i].name) {
            ast_delete(ve[i].var);
            break;
        }

        memcpy(ve[i].name, var->name, len);
        ve[i].name[len]   = '_';
        ve[i].name[len+1] = 'x'+i;
        ve[i].name[len+2] = 0;
    }
    if (i == 3)
        return true;

    /* unroll */
    do {
        --i;
        mem_d(ve[i].name);
        ast_delete(ve[i].var);
        ve[i].name = NULL;
        ve[i].var  = NULL;
    } while (i);
    return false;
}

static bool parse_function_body(parser_t *parser, ast_value *var)
{
    ast_block      *block = NULL;
    ast_function   *func;
    ast_function   *old;
    size_t          parami;

    ast_expression *framenum  = NULL;
    ast_expression *nextthink = NULL;
    /* None of the following have to be deleted */
    ast_expression *fld_think = NULL, *fld_nextthink = NULL, *fld_frame = NULL;
    ast_expression *gbl_time = NULL, *gbl_self = NULL;
    bool            has_frame_think;

    bool retval = true;

    has_frame_think = false;
    old = parser->function;

    if (var->expression.variadic) {
        if (parsewarning(parser, WARN_VARIADIC_FUNCTION,
                         "variadic function with implementation will not be able to access additional parameters"))
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
        nextthink = NULL;

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

        framenum = parse_expression_leave(parser, true);
        if (!framenum) {
            parseerror(parser, "expected a framenumber constant in[frame,think] notation");
            return false;
        }
        if (!ast_istype(framenum, ast_value) || !( (ast_value*)framenum )->isconst) {
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
            varentry_t      varent;
            ast_value      *thinkfunc;
            ast_expression *functype = fld_think->expression.next;

            thinkfunc = ast_value_new(parser_ctx(parser), parser_tokval(parser), functype->expression.vtype);
            if (!thinkfunc || !ast_type_adopt(thinkfunc, functype)) {
                ast_unref(framenum);
                parseerror(parser, "failed to create implicit prototype for `%s`", parser_tokval(parser));
                return false;
            }

            if (!parser_next(parser)) {
                ast_unref(framenum);
                ast_delete(thinkfunc);
                return false;
            }

            varent.var = (ast_expression*)thinkfunc;
            varent.name = util_strdup(thinkfunc->name);
            if (!parser_t_globals_add(parser, varent)) {
                ast_unref(framenum);
                ast_delete(thinkfunc);
                return false;
            }
            nextthink = (ast_expression*)thinkfunc;

        } else {
            nextthink = parse_expression_leave(parser, true);
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

    block = ast_block_new(parser_ctx(parser));
    if (!block) {
        parseerror(parser, "failed to allocate block");
        if (has_frame_think) {
            ast_unref(nextthink);
            ast_unref(framenum);
        }
        return false;
    }

    if (has_frame_think) {
        lex_ctx ctx;
        ast_expression *self_frame;
        ast_expression *self_nextthink;
        ast_expression *self_think;
        ast_expression *time_plus_1;
        ast_store *store_frame;
        ast_store *store_nextthink;
        ast_store *store_think;

        ctx = parser_ctx(parser);
        self_frame     = (ast_expression*)ast_entfield_new(ctx, gbl_self, fld_frame);
        self_nextthink = (ast_expression*)ast_entfield_new(ctx, gbl_self, fld_nextthink);
        self_think     = (ast_expression*)ast_entfield_new(ctx, gbl_self, fld_think);

        time_plus_1    = (ast_expression*)ast_binary_new(ctx, INSTR_ADD_F,
                         gbl_time, (ast_expression*)parser_const_float(parser, 0.1));

        if (!self_frame || !self_nextthink || !self_think || !time_plus_1) {
            if (self_frame)     ast_delete(self_frame);
            if (self_nextthink) ast_delete(self_nextthink);
            if (self_think)     ast_delete(self_think);
            if (time_plus_1)    ast_delete(time_plus_1);
            retval = false;
        }

        if (retval)
        {
            store_frame     = ast_store_new(ctx, INSTR_STOREP_F,   self_frame,     framenum);
            store_nextthink = ast_store_new(ctx, INSTR_STOREP_F,   self_nextthink, time_plus_1);
            store_think     = ast_store_new(ctx, INSTR_STOREP_FNC, self_think,     nextthink);

            if (!store_frame) {
                ast_delete(self_frame);
                retval = false;
            }
            if (!store_nextthink) {
                ast_delete(self_nextthink);
                retval = false;
            }
            if (!store_think) {
                ast_delete(self_think);
                retval = false;
            }
            if (!retval) {
                if (store_frame)     ast_delete(store_frame);
                if (store_nextthink) ast_delete(store_nextthink);
                if (store_think)     ast_delete(store_think);
                retval = false;
            }
            if (retval && !ast_block_exprs_add(block, (ast_expression*)store_frame)) {
                ast_delete(store_frame);
                ast_delete(store_nextthink);
                ast_delete(store_think);
                retval = false;
            }

            if (retval && !ast_block_exprs_add(block, (ast_expression*)store_nextthink)) {
                ast_delete(store_nextthink);
                ast_delete(store_think);
                retval = false;
            }

            if (retval && !ast_block_exprs_add(block, (ast_expression*)store_think) )
            {
                ast_delete(store_think);
                retval = false;
            }
        }

        if (!retval) {
            parseerror(parser, "failed to generate code for [frame,think]");
            ast_unref(nextthink);
            ast_unref(framenum);
            ast_delete(block);
            return false;
        }
    }

    for (parami = 0; parami < var->expression.params_count; ++parami) {
        size_t     e;
        varentry_t ve[3];
        ast_value *param = var->expression.params[parami];

        if (param->expression.vtype != TYPE_VECTOR &&
            (param->expression.vtype != TYPE_FIELD ||
             param->expression.next->expression.vtype != TYPE_VECTOR))
        {
            continue;
        }

        if (!create_vector_members(parser, param, ve)) {
            ast_block_delete(block);
            return false;
        }

        for (e = 0; e < 3; ++e) {
            if (!parser_t_locals_add(parser, ve[e]))
                break;
            if (!ast_block_collect(block, ve[e].var)) {
                parser->locals_count--;
                break;
            }
            ve[e].var = NULL; /* collected */
        }
        if (e != 3) {
            parser->locals -= e;
            do {
                mem_d(ve[e].name);
                --e;
            } while (e);
            ast_block_delete(block);
            return false;
        }
    }

    func = ast_function_new(ast_ctx(var), var->name, var);
    if (!func) {
        parseerror(parser, "failed to allocate function for `%s`", var->name);
        ast_block_delete(block);
        goto enderr;
    }
    if (!parser_t_functions_add(parser, func)) {
        parseerror(parser, "failed to allocate slot for function `%s`", var->name);
        ast_block_delete(block);
        goto enderrfn;
    }

    parser->function = func;
    if (!parse_block_into(parser, block, true)) {
        ast_block_delete(block);
        goto enderrfn2;
    }

    if (!ast_function_blocks_add(func, block)) {
        ast_block_delete(block);
        goto enderrfn2;
    }

    parser->function = old;
    while (parser->locals_count)
        retval = retval && parser_pop_local(parser);

    if (parser->tok == ';')
        return parser_next(parser);
    else if (opts_standard == COMPILER_QCC)
        parseerror(parser, "missing semicolon after function body (mandatory with -std=qcc)");
    return retval;

enderrfn2:
    parser->functions_count--;
enderrfn:
    ast_function_delete(func);
    var->constval.vfunc = NULL;

enderr:
    while (parser->locals_count) {
        parser->locals_count--;
        mem_d(parser->locals[parser->locals_count].name);
    }
    parser->function = old;
    return false;
}

typedef struct {
    MEM_VECTOR_MAKE(ast_value*, p);
} paramlist_t;
MEM_VEC_FUNCTIONS(paramlist_t, ast_value*, p)

static ast_value *parse_typename(parser_t *parser, ast_value **storebase);
static ast_value *parse_parameter_list(parser_t *parser, ast_value *var)
{
    lex_ctx     ctx;
    size_t      i;
    paramlist_t params;
    ast_value  *param;
    ast_value  *fval;
    bool        first = true;
    bool        variadic = false;

    ctx = parser_ctx(parser);

    /* for the sake of less code we parse-in in this function */
    if (!parser_next(parser)) {
        parseerror(parser, "expected parameter list");
        return NULL;
    }

    MEM_VECTOR_INIT(&params, p);

    /* parse variables until we hit a closing paren */
    while (parser->tok != ')') {
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

        if (parser->tok == TOKEN_DOTS) {
            /* '...' indicates a varargs function */
            variadic = true;
            if (!parser_next(parser)) {
                parseerror(parser, "expected parameter");
                return NULL;
            }
            if (parser->tok != ')') {
                parseerror(parser, "`...` must be the last parameter of a variadic function declaration");
                goto on_error;
            }
        }
        else
        {
            /* for anything else just parse a typename */
            param = parse_typename(parser, NULL);
            if (!param)
                goto on_error;
            if (!paramlist_t_p_add(&params, param))
                goto on_error;
        }
    }

    /* sanity check */
    if (params.p_count > 8)
        parseerror(parser, "more than 8 parameters are currently not supported");

    /* parse-out */
    if (!parser_next(parser)) {
        parseerror(parser, "parse error after typename");
        goto on_error;
    }

    /* now turn 'var' into a function type */
    fval = ast_value_new(ctx, "<type()>", TYPE_FUNCTION);
    fval->expression.next     = (ast_expression*)var;
    fval->expression.variadic = variadic;
    var = fval;

    MEM_VECTOR_MOVE(&params, p, &var->expression, params);

    return var;

on_error:
    ast_delete(var);
    for (i = 0; i < params.p_count; ++i)
        ast_delete(params.p[i]);
    MEM_VECTOR_CLEAR(&params, p);
    return NULL;
}

/* Parse a complete typename.
 * for single-variables (ie. function parameters or typedefs) storebase should be NULL
 * but when parsing variables separated by comma
 * 'storebase' should point to where the base-type should be kept.
 * The base type makes up every bit of type information which comes *before* the
 * variable name.
 *
 * The following will be parsed in its entirety:
 *     void() foo()
 * The 'basetype' in this case is 'void()'
 * and if there's a comma after it, say:
 *     void() foo(), bar
 * then the type-information 'void()' can be stored in 'storebase'
 */
static ast_value *parse_typename(parser_t *parser, ast_value **storebase)
{
    ast_value *var, *tmp;
    lex_ctx    ctx;

    const char *name = NULL;
    bool        isfield = false;

    ctx = parser_ctx(parser);

    /* types may start with a dot */
    if (parser->tok == '.') {
        isfield = true;
        /* if we parsed a dot we need a typename now */
        if (!parser_next(parser)) {
            parseerror(parser, "expected typename for field definition");
            return NULL;
        }
        if (parser->tok != TOKEN_TYPENAME) {
            parseerror(parser, "expected typename");
            return NULL;
        }
    }

    /* generate the basic type value */
    var = ast_value_new(ctx, "<type>", parser_token(parser)->constval.t);
    /* do not yet turn into a field - remember:
     * .void() foo; is a field too
     * .void()() foo; is a function
     */

    /* parse on */
    if (!parser_next(parser)) {
        ast_delete(var);
        parseerror(parser, "parse error after typename");
        return NULL;
    }

    /* an opening paren now starts the parameter-list of a function */
    if (parser->tok == '(') {
        var = parse_parameter_list(parser, var);
        if (!var)
            return NULL;
    }
    /* This is the point where we can turn it into a field */
    if (isfield) {
        /* turn it into a field if desired */
        tmp = ast_value_new(ctx, "<type:f>", TYPE_FIELD);
        tmp->expression.next = (ast_expression*)var;
        var = tmp;
    }

    while (parser->tok == '(') {
        var = parse_parameter_list(parser, var);
        if (!var)
            return NULL;
    }

    /* store the base if requested */
    if (storebase) {
        *storebase = ast_value_copy(var);
    }

    /* there may be a name now */
    if (parser->tok == TOKEN_IDENT) {
        name = util_strdup(parser_tokval(parser));
        /* parse on */
        if (!parser_next(parser)) {
            parseerror(parser, "error after variable or field declaration");
            return NULL;
        }
    }

    /* now there may be function parens again */
    if (parser->tok == '(' && opts_standard == COMPILER_QCC)
        parseerror(parser, "C-style function syntax is not allowed in -std=qcc");
    while (parser->tok == '(') {
        var = parse_parameter_list(parser, var);
        if (!var) {
            if (name)
                mem_d((void*)name);
            return NULL;
        }
    }

    /* finally name it */
    if (name) {
        if (!ast_value_set_name(var, name)) {
            ast_delete(var);
            parseerror(parser, "internal error: failed to set name");
            return NULL;
        }
        /* free the name, ast_value_set_name duplicates */
        mem_d((void*)name);
    }

    return var;
}

static bool parse_variable(parser_t *parser, ast_block *localblock, bool nofields)
{
    ast_value *var;
    ast_value *proto;
    ast_expression *old;
    bool       was_end;
    size_t     i;

    ast_value *basetype = NULL;
    bool      retval    = true;
    bool      isparam   = false;
    bool      isvector  = false;
    bool      cleanvar  = true;

    varentry_t varent, ve[3];

    /* get the first complete variable */
    var = parse_typename(parser, &basetype);
    if (!var) {
        if (basetype)
            ast_delete(basetype);
        return false;
    }

    memset(&varent, 0, sizeof(varent));
    memset(&ve, 0, sizeof(ve));

    while (true) {
        proto = NULL;

        /* Part 0: finish the type */
        while (parser->tok == '(') {
            if (opts_standard == COMPILER_QCC)
                parseerror(parser, "C-style function syntax is not allowed in -std=qcc");
            var = parse_parameter_list(parser, var);
            if (!var) {
                retval = false;
                goto cleanup;
            }
        }

        /* Part 1:
         * check for validity: (end_sys_..., multiple-definitions, prototypes, ...)
         * Also: if there was a prototype, `var` will be deleted and set to `proto` which
         * is then filled with the previous definition and the parameter-names replaced.
         */
        if (!localblock) {
            /* Deal with end_sys_ vars */
            was_end = false;
            if (!strcmp(var->name, "end_sys_globals")) {
                parser->crc_globals = parser->globals_count;
                was_end = true;
            }
            else if (!strcmp(var->name, "end_sys_fields")) {
                parser->crc_fields = parser->fields_count;
                was_end = true;
            }
            if (was_end && var->expression.vtype == TYPE_FIELD) {
                if (parsewarning(parser, WARN_END_SYS_FIELDS,
                                 "global '%s' hint should not be a field",
                                 parser_tokval(parser)))
                {
                    retval = false;
                    goto cleanup;
                }
            }

            if (!nofields && var->expression.vtype == TYPE_FIELD)
            {
                /* deal with field declarations */
                old = parser_find_field(parser, var->name);
                if (old) {
                    if (parsewarning(parser, WARN_FIELD_REDECLARED, "field `%s` already declared here: %s:%i",
                                     var->name, ast_ctx(old).file, (int)ast_ctx(old).line))
                    {
                        retval = false;
                        goto cleanup;
                    }
                    ast_delete(var);
                    var = NULL;
                    goto skipvar;
                    /*
                    parseerror(parser, "field `%s` already declared here: %s:%i",
                               var->name, ast_ctx(old).file, ast_ctx(old).line);
                    retval = false;
                    goto cleanup;
                    */
                }
                if (opts_standard == COMPILER_QCC &&
                    (old = parser_find_global(parser, var->name)))
                {
                    parseerror(parser, "cannot declare a field and a global of the same name with -std=qcc");
                    parseerror(parser, "field `%s` already declared here: %s:%i",
                               var->name, ast_ctx(old).file, ast_ctx(old).line);
                    retval = false;
                    goto cleanup;
                }
            }
            else
            {
                /* deal with other globals */
                old = parser_find_global(parser, var->name);
                if (old && var->expression.vtype == TYPE_FUNCTION && old->expression.vtype == TYPE_FUNCTION)
                {
                    /* This is a function which had a prototype */
                    if (!ast_istype(old, ast_value)) {
                        parseerror(parser, "internal error: prototype is not an ast_value");
                        retval = false;
                        goto cleanup;
                    }
                    proto = (ast_value*)old;
                    if (!ast_compare_type((ast_expression*)proto, (ast_expression*)var)) {
                        parseerror(parser, "conflicting types for `%s`, previous declaration was here: %s:%i",
                                   proto->name,
                                   ast_ctx(proto).file, ast_ctx(proto).line);
                        retval = false;
                        goto cleanup;
                    }
                    /* we need the new parameter-names */
                    for (i = 0; i < proto->expression.params_count; ++i)
                        ast_value_set_name(proto->expression.params[i], var->expression.params[i]->name);
                    ast_delete(var);
                    var = proto;
                }
                else
                {
                    /* other globals */
                    if (old) {
                        parseerror(parser, "global `%s` already declared here: %s:%i",
                                   var->name, ast_ctx(old).file, ast_ctx(old).line);
                        retval = false;
                        goto cleanup;
                    }
                    if (opts_standard == COMPILER_QCC &&
                        (old = parser_find_field(parser, var->name)))
                    {
                        parseerror(parser, "cannot declare a field and a global of the same name with -std=qcc");
                        parseerror(parser, "global `%s` already declared here: %s:%i",
                                   var->name, ast_ctx(old).file, ast_ctx(old).line);
                        retval = false;
                        goto cleanup;
                    }
                }
            }
        }
        else /* it's not a global */
        {
            old = parser_find_local(parser, var->name, parser->blocklocal, &isparam);
            if (old && !isparam) {
                parseerror(parser, "local `%s` already declared here: %s:%i",
                           var->name, ast_ctx(old).file, (int)ast_ctx(old).line);
                retval = false;
                goto cleanup;
            }
            old = parser_find_local(parser, var->name, 0, &isparam);
            if (old && isparam) {
                if (parsewarning(parser, WARN_LOCAL_SHADOWS,
                                 "local `%s` is shadowing a parameter", var->name))
                {
                    parseerror(parser, "local `%s` already declared here: %s:%i",
                               var->name, ast_ctx(old).file, (int)ast_ctx(old).line);
                    retval = false;
                    goto cleanup;
                }
                if (opts_standard != COMPILER_GMQCC) {
                    ast_delete(var);
                    var = NULL;
                    goto skipvar;
                }
            }
        }

        /* Part 2:
         * Create the global/local, and deal with vector types.
         */
        if (!proto) {
            if (var->expression.vtype == TYPE_VECTOR)
                isvector = true;
            else if (var->expression.vtype == TYPE_FIELD &&
                     var->expression.next->expression.vtype == TYPE_VECTOR)
                isvector = true;

            if (isvector) {
                if (!create_vector_members(parser, var, ve)) {
                    retval = false;
                    goto cleanup;
                }
            }

            varent.name = util_strdup(var->name);
            varent.var  = (ast_expression*)var;

            if (!localblock) {
                /* deal with global variables, fields, functions */
                if (!nofields && var->expression.vtype == TYPE_FIELD) {
                    if (!(retval = parser_t_fields_add(parser, varent)))
                        goto cleanup;
                    if (isvector) {
                        for (i = 0; i < 3; ++i) {
                            if (!(retval = parser_t_fields_add(parser, ve[i])))
                                break;
                        }
                        if (!retval) {
                            parser->fields_count -= i+1;
                            goto cleanup;
                        }
                    }
                }
                else {
                    if (!(retval = parser_t_globals_add(parser, varent)))
                        goto cleanup;
                    if (isvector) {
                        for (i = 0; i < 3; ++i) {
                            if (!(retval = parser_t_globals_add(parser, ve[i])))
                                break;
                        }
                        if (!retval) {
                            parser->globals_count -= i+1;
                            goto cleanup;
                        }
                    }
                }
            } else {
                if (!(retval = parser_t_locals_add(parser, varent)))
                    goto cleanup;
                if (!(retval = ast_block_locals_add(localblock, var))) {
                    parser->locals_count--;
                    goto cleanup;
                }
                if (isvector) {
                    for (i = 0; i < 3; ++i) {
                        if (!(retval = parser_t_locals_add(parser, ve[i])))
                            break;
                        if (!(retval = ast_block_collect(localblock, ve[i].var)))
                            break;
                        ve[i].var = NULL; /* from here it's being collected in the block */
                    }
                    if (!retval) {
                        parser->locals_count -= i+1;
                        localblock->locals_count--;
                        goto cleanup;
                    }
                }
            }

            varent.name = NULL;
            ve[0].name = ve[1].name = ve[2].name = NULL;
            ve[0].var  = ve[1].var  = ve[2].var  = NULL;
            cleanvar = false;
        }

skipvar:
        if (parser->tok == ';') {
            ast_delete(basetype);
            if (!parser_next(parser)) {
                parseerror(parser, "error after variable declaration");
                return false;
            }
            return true;
        }

        if (parser->tok == ',')
            goto another;

        if (!var || (!localblock && !nofields && basetype->expression.vtype == TYPE_FIELD)) {
            parseerror(parser, "missing comma or semicolon while parsing variables");
            break;
        }

        if (localblock && opts_standard == COMPILER_QCC) {
            if (parsewarning(parser, WARN_LOCAL_CONSTANTS,
                             "initializing expression turns variable `%s` into a constant in this standard",
                             var->name) )
            {
                break;
            }
        }

        if (parser->tok != '{') {
            if (parser->tok != '=') {
                parseerror(parser, "missing semicolon or initializer");
                break;
            }

            if (!parser_next(parser)) {
                parseerror(parser, "error parsing initializer");
                break;
            }
        }
        else if (opts_standard == COMPILER_QCC) {
            parseerror(parser, "expected '=' before function body in this standard");
        }

        if (parser->tok == '#') {
            ast_function *func;

            if (localblock) {
                parseerror(parser, "cannot declare builtins within functions");
                break;
            }
            if (var->expression.vtype != TYPE_FUNCTION) {
                parseerror(parser, "unexpected builtin number, '%s' is not a function", var->name);
                break;
            }
            if (!parser_next(parser)) {
                parseerror(parser, "expected builtin number");
                break;
            }
            if (parser->tok != TOKEN_INTCONST) {
                parseerror(parser, "builtin number must be an integer constant");
                break;
            }
            if (parser_token(parser)->constval.i <= 0) {
                parseerror(parser, "builtin number must be an integer greater than zero");
                break;
            }

            func = ast_function_new(ast_ctx(var), var->name, var);
            if (!func) {
                parseerror(parser, "failed to allocate function for `%s`", var->name);
                break;
            }
            if (!parser_t_functions_add(parser, func)) {
                parseerror(parser, "failed to allocate slot for function `%s`", var->name);
                ast_function_delete(func);
                var->constval.vfunc = NULL;
                break;
            }

            func->builtin = -parser_token(parser)->constval.i;

            if (!parser_next(parser)) {
                parseerror(parser, "expected comma or semicolon");
                ast_function_delete(func);
                var->constval.vfunc = NULL;
                break;
            }
        }
        else if (parser->tok == '{' || parser->tok == '[')
        {
            if (localblock) {
                parseerror(parser, "cannot declare functions within functions");
                break;
            }

            if (!parse_function_body(parser, var))
                break;
            ast_delete(basetype);
            return true;
        } else {
            ast_expression *cexp;
            ast_value      *cval;

            cexp = parse_expression_leave(parser, true);
            if (!cexp)
                break;

            cval = (ast_value*)cexp;
            if (!ast_istype(cval, ast_value) || !cval->isconst)
                parseerror(parser, "cannot initialize a global constant variable with a non-constant expression");
            else
            {
                var->isconst = true;
                if (cval->expression.vtype == TYPE_STRING)
                    var->constval.vstring = parser_strdup(cval->constval.vstring);
                else
                    memcpy(&var->constval, &cval->constval, sizeof(var->constval));
                ast_unref(cval);
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
            var = ast_value_copy(basetype);
            cleanvar = true;
            ast_value_set_name(var, parser_tokval(parser));
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

        ast_delete(basetype);
        return true;
    }

    if (cleanvar && var)
        ast_delete(var);
    ast_delete(basetype);
    return false;

cleanup:
    ast_delete(basetype);
    if (cleanvar && var)
        ast_delete(var);
    if (varent.name) mem_d(varent.name);
    if (ve[0].name)  mem_d(ve[0].name);
    if (ve[1].name)  mem_d(ve[1].name);
    if (ve[2].name)  mem_d(ve[2].name);
    if (ve[0].var)   mem_d(ve[0].var);
    if (ve[1].var)   mem_d(ve[1].var);
    if (ve[2].var)   mem_d(ve[2].var);
    return retval;
}

static bool parser_global_statement(parser_t *parser)
{
    if (parser->tok == TOKEN_TYPENAME || parser->tok == '.')
    {
        return parse_variable(parser, NULL, false);
    }
    else if (parser->tok == TOKEN_KEYWORD)
    {
        /* handle 'var' and 'const' */
        if (!strcmp(parser_tokval(parser), "var")) {
            if (!parser_next(parser)) {
                parseerror(parser, "expected variable declaration after 'var'");
                return false;
            }
            return parse_variable(parser, NULL, true);
        }
        return false;
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
        parseerror(parser, "unexpected token: %s", parser->lex->tok.value);
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

    memset(parser, 0, sizeof(*parser));
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
            if (!parser_global_statement(parser)) {
                if (parser->tok == TOKEN_EOF)
                    parseerror(parser, "unexpected eof");
                else if (!parser->errors)
                    parseerror(parser, "there have been errors, bailing out");
                lex_close(parser->lex);
                parser->lex = NULL;
                return false;
            }
        }
    } else {
        parseerror(parser, "parse error");
        lex_close(parser->lex);
        parser->lex = NULL;
        return false;
    }

    lex_close(parser->lex);
    parser->lex = NULL;

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
    for (i = 0; i < parser->fields_count; ++i) {
        ast_delete(parser->fields[i].var);
        mem_d(parser->fields[i].name);
    }
    for (i = 0; i < parser->globals_count; ++i) {
        ast_delete(parser->globals[i].var);
        mem_d(parser->globals[i].name);
    }
    MEM_VECTOR_CLEAR(parser, functions);
    MEM_VECTOR_CLEAR(parser, imm_vector);
    MEM_VECTOR_CLEAR(parser, imm_string);
    MEM_VECTOR_CLEAR(parser, imm_float);
    MEM_VECTOR_CLEAR(parser, globals);
    MEM_VECTOR_CLEAR(parser, fields);
    MEM_VECTOR_CLEAR(parser, locals);

    mem_d(parser);
}

static uint16_t progdefs_crc_sum(uint16_t old, const char *str)
{
    return util_crc16(old, str, strlen(str));
}

static void progdefs_crc_file(const char *str)
{
    /* write to progdefs.h here */
}

static uint16_t progdefs_crc_both(uint16_t old, const char *str)
{
    old = progdefs_crc_sum(old, str);
    progdefs_crc_file(str);
    return old;
}

static void generate_checksum(parser_t *parser)
{
    uint16_t crc = 0xFFFF;
    size_t i;

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
	    if (!ast_istype(parser->globals[i].var, ast_value))
	        continue;
	    switch (parser->globals[i].var->expression.vtype) {
	        case TYPE_FLOAT:    crc = progdefs_crc_both(crc, "\tfloat\t"); break;
	        case TYPE_VECTOR:   crc = progdefs_crc_both(crc, "\tvec3_t\t"); break;
	        case TYPE_STRING:   crc = progdefs_crc_both(crc, "\tstring_t\t"); break;
	        case TYPE_FUNCTION: crc = progdefs_crc_both(crc, "\tfunc_t\t"); break;
	        default:
	            crc = progdefs_crc_both(crc, "\tint\t");
	            break;
	    }
	    crc = progdefs_crc_both(crc, parser->globals[i].name);
	    crc = progdefs_crc_both(crc, ";\n");
	}
	crc = progdefs_crc_both(crc, "} globalvars_t;\n\ntypedef struct\n{\n");
	for (i = 0; i < parser->crc_fields; ++i) {
	    if (!ast_istype(parser->fields[i].var, ast_value))
	        continue;
	    switch (parser->fields[i].var->expression.next->expression.vtype) {
	        case TYPE_FLOAT:    crc = progdefs_crc_both(crc, "\tfloat\t"); break;
	        case TYPE_VECTOR:   crc = progdefs_crc_both(crc, "\tvec3_t\t"); break;
	        case TYPE_STRING:   crc = progdefs_crc_both(crc, "\tstring_t\t"); break;
	        case TYPE_FUNCTION: crc = progdefs_crc_both(crc, "\tfunc_t\t"); break;
	        default:
	            crc = progdefs_crc_both(crc, "\tint\t");
	            break;
	    }
	    crc = progdefs_crc_both(crc, parser->fields[i].name);
	    crc = progdefs_crc_both(crc, ";\n");
	}
	crc = progdefs_crc_both(crc, "} entvars_t;\n\n");

	code_crc = crc;
}

bool parser_finish(const char *output)
{
    size_t i;
    ir_builder *ir;
    bool retval = true;

    if (!parser->errors)
    {
        ir = ir_builder_new("gmqcc_out");
        if (!ir) {
            printf("failed to allocate builder\n");
            return false;
        }

        for (i = 0; i < parser->fields_count; ++i) {
            ast_value *field;
            bool isconst;
            if (!ast_istype(parser->fields[i].var, ast_value))
                continue;
            field = (ast_value*)parser->fields[i].var;
            isconst = field->isconst;
            field->isconst = false;
            if (!ast_global_codegen((ast_value*)field, ir, true)) {
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
            ast_value *asvalue;
            if (!ast_istype(parser->globals[i].var, ast_value))
                continue;
            asvalue = (ast_value*)(parser->globals[i].var);
            if (!asvalue->uses && !asvalue->isconst && asvalue->expression.vtype != TYPE_FUNCTION) {
                if (strcmp(asvalue->name, "end_sys_globals") &&
                    strcmp(asvalue->name, "end_sys_fields"))
                {
                    retval = retval && !genwarning(ast_ctx(asvalue), WARN_UNUSED_VARIABLE,
                                                   "unused global: `%s`", asvalue->name);
                }
            }
            if (!ast_global_codegen(asvalue, ir, false)) {
                printf("failed to generate global %s\n", parser->globals[i].name);
                ir_builder_delete(ir);
                return false;
            }
        }
        for (i = 0; i < parser->imm_float_count; ++i) {
            if (!ast_global_codegen(parser->imm_float[i], ir, false)) {
                printf("failed to generate global %s\n", parser->imm_float[i]->name);
                ir_builder_delete(ir);
                return false;
            }
        }
        for (i = 0; i < parser->imm_string_count; ++i) {
            if (!ast_global_codegen(parser->imm_string[i], ir, false)) {
                printf("failed to generate global %s\n", parser->imm_string[i]->name);
                ir_builder_delete(ir);
                return false;
            }
        }
        for (i = 0; i < parser->imm_vector_count; ++i) {
            if (!ast_global_codegen(parser->imm_vector[i], ir, false)) {
                printf("failed to generate global %s\n", parser->imm_vector[i]->name);
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

        if (retval) {
            if (opts_dump)
                ir_builder_dump(ir, printf);

            generate_checksum(parser);

            if (!ir_builder_generate(ir, output)) {
                printf("*** failed to generate output file\n");
                ir_builder_delete(ir);
                return false;
            }
        }

        ir_builder_delete(ir);
        return retval;
    }

    printf("*** there were compile errors\n");
    return false;
}
