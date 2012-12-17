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
#include <stdio.h>
#include <stdarg.h>

#include "gmqcc.h"
#include "lexer.h"

#define PARSER_HT_FIELDS  0
#define PARSER_HT_GLOBALS 1
/* beginning of locals */
#define PARSER_HT_LOCALS  2

#define PARSER_HT_SIZE    1024
#define TYPEDEF_HT_SIZE   16

typedef struct {
    lex_file *lex;
    int      tok;

    ast_expression **globals;
    ast_expression **fields;
    ast_function **functions;
    ast_value    **imm_float;
    ast_value    **imm_string;
    ast_value    **imm_vector;
    size_t         translated;

    /* must be deleted first, they reference immediates and values */
    ast_value    **accessors;

    ast_value *imm_float_zero;
    ast_value *imm_float_one;
    ast_value *imm_vector_zero;

    size_t crc_globals;
    size_t crc_fields;

    ast_function *function;

    /* All the labels the function defined...
     * Should they be in ast_function instead?
     */
    ast_label **labels;
    ast_goto  **gotos;

    /* A list of hashtables for each scope */
    ht *variables;
    ht htfields;
    ht htglobals;
    ht *typedefs;

    /* not to be used directly, we use the hash table */
    ast_expression **_locals;
    size_t          *_blocklocals;
    ast_value      **_typedefs;
    size_t          *_blocktypedefs;
    lex_ctx         *_block_ctx;

    size_t errors;

    /* we store the '=' operator info */
    const oper_info *assign_op;

    /* TYPE_FIELD -> parser_find_fields is used instead of find_var
     * TODO: TYPE_VECTOR -> x, y and z are accepted in the gmqcc standard
     * anything else: type error
     */
    qcint  memberof;

    /* Keep track of our ternary vs parenthesis nesting state.
     * If we reach a 'comma' operator in a ternary without a paren,
     * we shall trigger -Wternary-precedence.
     */
    enum { POT_PAREN, POT_TERNARY1, POT_TERNARY2 } *pot;

    /* pragma flags */
    bool noref;
} parser_t;

static void parser_enterblock(parser_t *parser);
static bool parser_leaveblock(parser_t *parser);
static void parser_addlocal(parser_t *parser, const char *name, ast_expression *e);
static bool parse_typedef(parser_t *parser);
static bool parse_variable(parser_t *parser, ast_block *localblock, bool nofields, int qualifier, ast_value *cached_typedef, bool noref);
static ast_block* parse_block(parser_t *parser);
static bool parse_block_into(parser_t *parser, ast_block *block);
static bool parse_statement_or_block(parser_t *parser, ast_expression **out);
static bool parse_statement(parser_t *parser, ast_block *block, ast_expression **out, bool allow_cases);
static ast_expression* parse_expression_leave(parser_t *parser, bool stopatcomma);
static ast_expression* parse_expression(parser_t *parser, bool stopatcomma);

static void parseerror(parser_t *parser, const char *fmt, ...)
{
	va_list ap;

	parser->errors++;

	va_start(ap, fmt);
    con_vprintmsg(LVL_ERROR, parser->lex->tok.ctx.file, parser->lex->tok.ctx.line, "parse error", fmt, ap);
	va_end(ap);
}

/* returns true if it counts as an error */
static bool GMQCC_WARN parsewarning(parser_t *parser, int warntype, const char *fmt, ...)
{
	va_list ap;
	int lvl = LVL_WARNING;

    if (!OPTS_WARN(warntype))
        return false;

    if (opts.werror) {
	    parser->errors++;
	    lvl = LVL_ERROR;
	}

	va_start(ap, fmt);
    con_vprintmsg(lvl, parser->lex->tok.ctx.file, parser->lex->tok.ctx.line, (opts.werror ? "error" : "warning"), fmt, ap);
	va_end(ap);

	return opts.werror;
}

static bool GMQCC_WARN genwarning(lex_ctx ctx, int warntype, const char *fmt, ...)
{
	va_list ap;
	int lvl = LVL_WARNING;

    if (!OPTS_WARN(warntype))
        return false;

    if (opts.werror)
	    lvl = LVL_ERROR;

	va_start(ap, fmt);
    con_vprintmsg(lvl, ctx.file, ctx.line, (opts.werror ? "error" : "warning"), fmt, ap);
	va_end(ap);

	return opts.werror;
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
    for (i = 0; i < vec_size(parser->imm_float); ++i) {
        const double compare = parser->imm_float[i]->constval.vfloat;
        if (memcmp((const void*)&compare, (const void *)&d, sizeof(double)) == 0)
            return parser->imm_float[i];
    }
    out = ast_value_new(parser_ctx(parser), "#IMMEDIATE", TYPE_FLOAT);
    out->cvq      = CV_CONST;
    out->hasvalue = true;
    out->constval.vfloat = d;
    vec_push(parser->imm_float, out);
    return out;
}

static ast_value* parser_const_float_0(parser_t *parser)
{
    if (!parser->imm_float_zero)
        parser->imm_float_zero = parser_const_float(parser, 0);
    return parser->imm_float_zero;
}

static ast_value* parser_const_float_1(parser_t *parser)
{
    if (!parser->imm_float_one)
        parser->imm_float_one = parser_const_float(parser, 1);
    return parser->imm_float_one;
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

static ast_value* parser_const_string(parser_t *parser, const char *str, bool dotranslate)
{
    size_t i;
    ast_value *out;
    for (i = 0; i < vec_size(parser->imm_string); ++i) {
        if (!strcmp(parser->imm_string[i]->constval.vstring, str))
            return parser->imm_string[i];
    }
    if (dotranslate) {
        char name[32];
        snprintf(name, sizeof(name), "dotranslate_%lu", (unsigned long)(parser->translated++));
        out = ast_value_new(parser_ctx(parser), name, TYPE_STRING);
    } else
        out = ast_value_new(parser_ctx(parser), "#IMMEDIATE", TYPE_STRING);
    out->cvq      = CV_CONST;
    out->hasvalue = true;
    out->constval.vstring = parser_strdup(str);
    vec_push(parser->imm_string, out);
    return out;
}

static ast_value* parser_const_vector(parser_t *parser, vector v)
{
    size_t i;
    ast_value *out;
    for (i = 0; i < vec_size(parser->imm_vector); ++i) {
        if (!memcmp(&parser->imm_vector[i]->constval.vvec, &v, sizeof(v)))
            return parser->imm_vector[i];
    }
    out = ast_value_new(parser_ctx(parser), "#IMMEDIATE", TYPE_VECTOR);
    out->cvq      = CV_CONST;
    out->hasvalue = true;
    out->constval.vvec = v;
    vec_push(parser->imm_vector, out);
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
    return util_htget(parser->htfields, name);
}

static ast_expression* parser_find_global(parser_t *parser, const char *name)
{
    return util_htget(parser->htglobals, name);
}

static ast_expression* parser_find_param(parser_t *parser, const char *name)
{
    size_t i;
    ast_value *fun;
    if (!parser->function)
        return NULL;
    fun = parser->function->vtype;
    for (i = 0; i < vec_size(fun->expression.params); ++i) {
        if (!strcmp(fun->expression.params[i]->name, name))
            return (ast_expression*)(fun->expression.params[i]);
    }
    return NULL;
}

static ast_expression* parser_find_local(parser_t *parser, const char *name, size_t upto, bool *isparam)
{
    size_t          i, hash;
    ast_expression *e;

    hash = util_hthash(parser->htglobals, name);

    *isparam = false;
    for (i = vec_size(parser->variables); i > upto;) {
        --i;
        if ( (e = util_htgeth(parser->variables[i], name, hash)) )
            return e;
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

static ast_value* parser_find_typedef(parser_t *parser, const char *name, size_t upto)
{
    size_t     i, hash;
    ast_value *e;
    hash = util_hthash(parser->typedefs[0], name);

    for (i = vec_size(parser->typedefs); i > upto;) {
        --i;
        if ( (e = (ast_value*)util_htgeth(parser->typedefs[i], name, hash)) )
            return e;
    }
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
    sy_elem *out;
    sy_elem *ops;
} shunt;

#define SY_PAREN_EXPR '('
#define SY_PAREN_FUNC 'f'
#define SY_PAREN_INDEX '['
#define SY_PAREN_TERNARY '?'

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

/* With regular precedence rules, ent.foo[n] is the same as (ent.foo)[n],
 * so we need to rotate it to become ent.(foo[n]).
 */
static bool rotate_entfield_array_index_nodes(ast_expression **out)
{
    ast_array_index *index;
    ast_entfield    *entfield;

    ast_value       *field;
    ast_expression  *sub;
    ast_expression  *entity;

    lex_ctx ctx = ast_ctx(*out);

    if (!ast_istype(*out, ast_array_index))
        return false;
    index = (ast_array_index*)*out;

    if (!ast_istype(index->array, ast_entfield))
        return false;
    entfield = (ast_entfield*)index->array;

    if (!ast_istype(entfield->field, ast_value))
        return false;
    field = (ast_value*)entfield->field;

    sub    = index->index;
    entity = entfield->entity;

    ast_delete(index);

    index = ast_array_index_new(ctx, (ast_expression*)field, sub);
    entfield = ast_entfield_new(ctx, entity, (ast_expression*)index);
    *out = (ast_expression*)entfield;

    return true;
}

static bool parser_sy_apply_operator(parser_t *parser, shunt *sy)
{
    const oper_info *op;
    lex_ctx ctx;
    ast_expression *out = NULL;
    ast_expression *exprs[3];
    ast_block      *blocks[3];
    ast_value      *asvalue[3];
    ast_binstore   *asbinstore;
    size_t i, assignop, addop, subop;
    qcint  generated_op = 0;

    char ty1[1024];
    char ty2[1024];

    if (!vec_size(sy->ops)) {
        parseerror(parser, "internal error: missing operator");
        return false;
    }

    if (vec_last(sy->ops).paren) {
        parseerror(parser, "unmatched parenthesis");
        return false;
    }

    op = &operators[vec_last(sy->ops).etype - 1];
    ctx = vec_last(sy->ops).ctx;

    DEBUGSHUNTDO(con_out("apply %s\n", op->op));

    if (vec_size(sy->out) < op->operands) {
        parseerror(parser, "internal error: not enough operands: %i (operator %s (%i))", vec_size(sy->out),
                   op->op, (int)op->id);
        return false;
    }

    vec_shrinkby(sy->ops, 1);

    /* op(:?) has no input and no output */
    if (!op->operands)
        return true;

    vec_shrinkby(sy->out, op->operands);
    for (i = 0; i < op->operands; ++i) {
        exprs[i]  = sy->out[vec_size(sy->out)+i].out;
        blocks[i] = sy->out[vec_size(sy->out)+i].block;
        asvalue[i] = (ast_value*)exprs[i];
    }

    if (blocks[0] && !vec_size(blocks[0]->exprs) && op->id != opid1(',')) {
        parseerror(parser, "internal error: operator cannot be applied on empty blocks");
        return false;
    }

#define NotSameType(T) \
             (exprs[0]->expression.vtype != exprs[1]->expression.vtype || \
              exprs[0]->expression.vtype != T)
#define CanConstFold1(A) \
             (ast_istype((A), ast_value) && ((ast_value*)(A))->hasvalue && (((ast_value*)(A))->cvq == CV_CONST))
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

        case opid1('['):
            if (exprs[0]->expression.vtype != TYPE_ARRAY &&
                !(exprs[0]->expression.vtype == TYPE_FIELD &&
                  exprs[0]->expression.next->expression.vtype == TYPE_ARRAY))
            {
                ast_type_to_string(exprs[0], ty1, sizeof(ty1));
                parseerror(parser, "cannot index value of type %s", ty1);
                return false;
            }
            if (exprs[1]->expression.vtype != TYPE_FLOAT) {
                ast_type_to_string(exprs[0], ty1, sizeof(ty1));
                parseerror(parser, "index must be of type float, not %s", ty1);
                return false;
            }
            out = (ast_expression*)ast_array_index_new(ctx, exprs[0], exprs[1]);
            if (rotate_entfield_array_index_nodes(&out))
            {
#if 0
                /* This is not broken in fteqcc anymore */
                if (opts.standard != COMPILER_GMQCC) {
                    /* this error doesn't need to make us bail out */
                    (void)!parsewarning(parser, WARN_EXTENSIONS,
                                        "accessing array-field members of an entity without parenthesis\n"
                                        " -> this is an extension from -std=gmqcc");
                }
#endif
            }
            break;

        case opid1(','):
            if (blocks[0]) {
                if (!ast_block_add_expr(blocks[0], exprs[1]))
                    return false;
            } else {
                blocks[0] = ast_block_new(ctx);
                if (!ast_block_add_expr(blocks[0], exprs[0]) ||
                    !ast_block_add_expr(blocks[0], exprs[1]))
                {
                    return false;
                }
            }
            if (!ast_block_set_type(blocks[0], exprs[1]))
                return false;

            vec_push(sy->out, syblock(ctx, blocks[0]));
            return true;

        case opid2('+','P'):
            out = exprs[0];
            break;
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
#if 0
            if (NotSameType(TYPE_FLOAT)) {
                parseerror(parser, "invalid types used in expression: cannot perform logical operations between types %s and %s",
                           type_name[exprs[0]->expression.vtype],
                           type_name[exprs[1]->expression.vtype]);
                parseerror(parser, "TODO: logical ops for arbitrary types using INSTR_NOT");
                parseerror(parser, "TODO: optional early out");
                return false;
            }
#endif
            if (opts.standard == COMPILER_GMQCC)
                con_out("TODO: early out logic\n");
            if (CanConstFold(exprs[0], exprs[1]))
                out = (ast_expression*)parser_const_float(parser,
                    (generated_op == INSTR_OR ? (ConstF(0) || ConstF(1)) : (ConstF(0) && ConstF(1))));
            else
                out = (ast_expression*)ast_binary_new(ctx, generated_op, exprs[0], exprs[1]);
            break;

        case opid2('?',':'):
            if (vec_last(parser->pot) != POT_TERNARY2) {
                parseerror(parser, "mismatched parenthesis/ternary");
                return false;
            }
            vec_pop(parser->pot);
            if (exprs[1]->expression.vtype != exprs[2]->expression.vtype) {
                ast_type_to_string(exprs[1], ty1, sizeof(ty1));
                ast_type_to_string(exprs[2], ty2, sizeof(ty2));
                parseerror(parser, "operands of ternary expression must have the same type, got %s and %s", ty1, ty2);
                return false;
            }
            if (CanConstFold1(exprs[0]))
                out = (ConstF(0) ? exprs[1] : exprs[2]);
            else
                out = (ast_expression*)ast_ternary_new(ctx, exprs[0], exprs[1], exprs[2]);
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
                if (OPTS_FLAG(ADJUST_VECTOR_FIELDS) &&
                    exprs[0]->expression.vtype == TYPE_FIELD &&
                    exprs[0]->expression.next->expression.vtype == TYPE_VECTOR)
                {
                    assignop = type_storep_instr[TYPE_VECTOR];
                }
                else
                    assignop = type_storep_instr[exprs[0]->expression.vtype];
                if (assignop == AINSTR_END ||
                    !ast_compare_type(field->expression.next, exprs[1]))
                {
                    ast_type_to_string(field->expression.next, ty1, sizeof(ty1));
                    ast_type_to_string(exprs[1], ty2, sizeof(ty2));
                    if (OPTS_FLAG(ASSIGN_FUNCTION_TYPES) &&
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
                if (OPTS_FLAG(ADJUST_VECTOR_FIELDS) &&
                    exprs[0]->expression.vtype == TYPE_FIELD &&
                    exprs[0]->expression.next->expression.vtype == TYPE_VECTOR)
                {
                    assignop = type_store_instr[TYPE_VECTOR];
                }
                else {
                    assignop = type_store_instr[exprs[0]->expression.vtype];
                }

                if (assignop == AINSTR_END) {
                    ast_type_to_string(exprs[0], ty1, sizeof(ty1));
                    ast_type_to_string(exprs[1], ty2, sizeof(ty2));
                    parseerror(parser, "invalid types in assignment: cannot assign %s to %s", ty2, ty1);
                }
                else if (!ast_compare_type(exprs[0], exprs[1])) {
                    ast_type_to_string(exprs[0], ty1, sizeof(ty1));
                    ast_type_to_string(exprs[1], ty2, sizeof(ty2));
                    if (OPTS_FLAG(ASSIGN_FUNCTION_TYPES) &&
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
            if (ast_istype(exprs[0], ast_value) && asvalue[0]->cvq == CV_CONST) {
                parseerror(parser, "assignment to constant `%s`", asvalue[0]->name);
            }
            out = (ast_expression*)ast_store_new(ctx, assignop, exprs[0], exprs[1]);
            break;
        case opid3('+','+','P'):
        case opid3('-','-','P'):
            /* prefix ++ */
            if (exprs[0]->expression.vtype != TYPE_FLOAT) {
                ast_type_to_string(exprs[0], ty1, sizeof(ty1));
                parseerror(parser, "invalid type for prefix increment: %s", ty1);
                return false;
            }
            if (op->id == opid3('+','+','P'))
                addop = INSTR_ADD_F;
            else
                addop = INSTR_SUB_F;
            if (ast_istype(exprs[0], ast_value) && asvalue[0]->cvq == CV_CONST) {
                parseerror(parser, "assignment to constant `%s`", asvalue[0]->name);
            }
            if (ast_istype(exprs[0], ast_entfield)) {
                out = (ast_expression*)ast_binstore_new(ctx, INSTR_STOREP_F, addop,
                                                        exprs[0],
                                                        (ast_expression*)parser_const_float_1(parser));
            } else {
                out = (ast_expression*)ast_binstore_new(ctx, INSTR_STORE_F, addop,
                                                        exprs[0],
                                                        (ast_expression*)parser_const_float_1(parser));
            }
            break;
        case opid3('S','+','+'):
        case opid3('S','-','-'):
            /* prefix ++ */
            if (exprs[0]->expression.vtype != TYPE_FLOAT) {
                ast_type_to_string(exprs[0], ty1, sizeof(ty1));
                parseerror(parser, "invalid type for suffix increment: %s", ty1);
                return false;
            }
            if (op->id == opid3('S','+','+')) {
                addop = INSTR_ADD_F;
                subop = INSTR_SUB_F;
            } else {
                addop = INSTR_SUB_F;
                subop = INSTR_ADD_F;
            }
            if (ast_istype(exprs[0], ast_value) && asvalue[0]->cvq == CV_CONST) {
                parseerror(parser, "assignment to constant `%s`", asvalue[0]->name);
            }
            if (ast_istype(exprs[0], ast_entfield)) {
                out = (ast_expression*)ast_binstore_new(ctx, INSTR_STOREP_F, addop,
                                                        exprs[0],
                                                        (ast_expression*)parser_const_float_1(parser));
            } else {
                out = (ast_expression*)ast_binstore_new(ctx, INSTR_STORE_F, addop,
                                                        exprs[0],
                                                        (ast_expression*)parser_const_float_1(parser));
            }
            if (!out)
                return false;
            out = (ast_expression*)ast_binary_new(ctx, subop,
                                                  out,
                                                  (ast_expression*)parser_const_float_1(parser));
            break;
        case opid2('+','='):
        case opid2('-','='):
            if (exprs[0]->expression.vtype != exprs[1]->expression.vtype ||
                (exprs[0]->expression.vtype != TYPE_VECTOR && exprs[0]->expression.vtype != TYPE_FLOAT) )
            {
                ast_type_to_string(exprs[0], ty1, sizeof(ty1));
                ast_type_to_string(exprs[1], ty2, sizeof(ty2));
                parseerror(parser, "invalid types used in expression: cannot add or subtract type %s and %s",
                           ty1, ty2);
                return false;
            }
            if (ast_istype(exprs[0], ast_value) && asvalue[0]->cvq == CV_CONST) {
                parseerror(parser, "assignment to constant `%s`", asvalue[0]->name);
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
        case opid2('*','='):
        case opid2('/','='):
            if (exprs[1]->expression.vtype != TYPE_FLOAT ||
                !(exprs[0]->expression.vtype == TYPE_FLOAT ||
                  exprs[0]->expression.vtype == TYPE_VECTOR))
            {
                ast_type_to_string(exprs[0], ty1, sizeof(ty1));
                ast_type_to_string(exprs[1], ty2, sizeof(ty2));
                parseerror(parser, "invalid types used in expression: %s and %s",
                           ty1, ty2);
                return false;
            }
            if (ast_istype(exprs[0], ast_value) && asvalue[0]->cvq == CV_CONST) {
                parseerror(parser, "assignment to constant `%s`", asvalue[0]->name);
            }
            if (ast_istype(exprs[0], ast_entfield))
                assignop = type_storep_instr[exprs[0]->expression.vtype];
            else
                assignop = type_store_instr[exprs[0]->expression.vtype];
            switch (exprs[0]->expression.vtype) {
                case TYPE_FLOAT:
                    out = (ast_expression*)ast_binstore_new(ctx, assignop,
                                                            (op->id == opid2('*','=') ? INSTR_MUL_F : INSTR_DIV_F),
                                                            exprs[0], exprs[1]);
                    break;
                case TYPE_VECTOR:
                    if (op->id == opid2('*','=')) {
                        out = (ast_expression*)ast_binstore_new(ctx, assignop, INSTR_MUL_VF,
                                                                exprs[0], exprs[1]);
                    } else {
                        /* there's no DIV_VF */
                        out = (ast_expression*)ast_binary_new(ctx, INSTR_DIV_F,
                                                              (ast_expression*)parser_const_float_1(parser),
                                                              exprs[1]);
                        if (!out)
                            return false;
                        out = (ast_expression*)ast_binstore_new(ctx, assignop, INSTR_MUL_VF,
                                                                exprs[0], out);
                    }
                    break;
                default:
                    parseerror(parser, "invalid types used in expression: cannot add or subtract type %s and %s",
                               type_name[exprs[0]->expression.vtype],
                               type_name[exprs[1]->expression.vtype]);
                    return false;
            };
            break;
        case opid2('&','='):
        case opid2('|','='):
            if (NotSameType(TYPE_FLOAT)) {
                ast_type_to_string(exprs[0], ty1, sizeof(ty1));
                ast_type_to_string(exprs[1], ty2, sizeof(ty2));
                parseerror(parser, "invalid types used in expression: %s and %s",
                           ty1, ty2);
                return false;
            }
            if (ast_istype(exprs[0], ast_value) && asvalue[0]->cvq == CV_CONST) {
                parseerror(parser, "assignment to constant `%s`", asvalue[0]->name);
            }
            if (ast_istype(exprs[0], ast_entfield))
                assignop = type_storep_instr[exprs[0]->expression.vtype];
            else
                assignop = type_store_instr[exprs[0]->expression.vtype];
            out = (ast_expression*)ast_binstore_new(ctx, assignop,
                                                    (op->id == opid2('&','=') ? INSTR_BITAND : INSTR_BITOR),
                                                    exprs[0], exprs[1]);
            break;
        case opid3('&','~','='):
            /* This is like: a &= ~(b);
             * But QC has no bitwise-not, so we implement it as
             * a -= a & (b);
             */
            if (NotSameType(TYPE_FLOAT)) {
                ast_type_to_string(exprs[0], ty1, sizeof(ty1));
                ast_type_to_string(exprs[1], ty2, sizeof(ty2));
                parseerror(parser, "invalid types used in expression: %s and %s",
                           ty1, ty2);
                return false;
            }
            if (ast_istype(exprs[0], ast_entfield))
                assignop = type_storep_instr[exprs[0]->expression.vtype];
            else
                assignop = type_store_instr[exprs[0]->expression.vtype];
            out = (ast_expression*)ast_binary_new(ctx, INSTR_BITAND, exprs[0], exprs[1]);
            if (!out)
                return false;
            if (ast_istype(exprs[0], ast_value) && asvalue[0]->cvq == CV_CONST) {
                parseerror(parser, "assignment to constant `%s`", asvalue[0]->name);
            }
            asbinstore = ast_binstore_new(ctx, assignop, INSTR_SUB_F, exprs[0], out);
            asbinstore->keep_dest = true;
            out = (ast_expression*)asbinstore;
            break;
    }
#undef NotSameType

    if (!out) {
        parseerror(parser, "failed to apply operand %s", op->op);
        return false;
    }

    DEBUGSHUNTDO(con_out("applied %s\n", op->op));
    vec_push(sy->out, syexp(ctx, out));
    return true;
}

static bool parser_close_call(parser_t *parser, shunt *sy)
{
    /* was a function call */
    ast_expression *fun;
    ast_call       *call;

    size_t          fid;
    size_t          paramcount;

    vec_shrinkby(sy->ops, 1);
    fid = sy->ops[vec_size(sy->ops)].off;

    /* out[fid] is the function
     * everything above is parameters...
     * 0 params = nothing
     * 1 params = ast_expression
     * more = ast_block
     */

    if (vec_size(sy->out) < 1 || vec_size(sy->out) <= fid) {
        parseerror(parser, "internal error: function call needs function and parameter list...");
        return false;
    }

    fun = sy->out[fid].out;

    call = ast_call_new(sy->ops[vec_size(sy->ops)].ctx, fun);
    if (!call) {
        parseerror(parser, "out of memory");
        return false;
    }

    if (fid+1 == vec_size(sy->out)) {
        /* no arguments */
        paramcount = 0;
    } else if (fid+2 == vec_size(sy->out)) {
        ast_block *params;
        vec_shrinkby(sy->out, 1);
        params = sy->out[vec_size(sy->out)].block;
        if (!params) {
            /* 1 param */
            paramcount = 1;
            vec_push(call->params, sy->out[vec_size(sy->out)].out);
        } else {
            paramcount = vec_size(params->exprs);
            call->params = params->exprs;
            params->exprs = NULL;
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
        if (vec_size(fun->expression.params) != paramcount &&
            !(fun->expression.variadic &&
              vec_size(fun->expression.params) < paramcount))
        {
            ast_value *fval;
            const char *fewmany = (vec_size(fun->expression.params) > paramcount) ? "few" : "many";

            fval = (ast_istype(fun, ast_value) ? ((ast_value*)fun) : NULL);
            if (opts.standard == COMPILER_GMQCC)
            {
                if (fval)
                    parseerror(parser, "too %s parameters for call to %s: expected %i, got %i\n"
                               " -> `%s` has been declared here: %s:%i",
                               fewmany, fval->name, (int)vec_size(fun->expression.params), (int)paramcount,
                               fval->name, ast_ctx(fun).file, (int)ast_ctx(fun).line);
                else
                    parseerror(parser, "too %s parameters for function call: expected %i, got %i\n"
                               " -> `%s` has been declared here: %s:%i",
                               fewmany, fval->name, (int)vec_size(fun->expression.params), (int)paramcount,
                               fval->name, ast_ctx(fun).file, (int)ast_ctx(fun).line);
                return false;
            }
            else
            {
                if (fval)
                    return !parsewarning(parser, WARN_TOO_FEW_PARAMETERS,
                                         "too %s parameters for call to %s: expected %i, got %i\n"
                                         " -> `%s` has been declared here: %s:%i",
                                         fewmany, fval->name, (int)vec_size(fun->expression.params), (int)paramcount,
                                         fval->name, ast_ctx(fun).file, (int)ast_ctx(fun).line);
                else
                    return !parsewarning(parser, WARN_TOO_FEW_PARAMETERS,
                                         "too %s parameters for function call: expected %i, got %i\n"
                                         " -> `%s` has been declared here: %s:%i",
                                         fewmany, fval->name, (int)vec_size(fun->expression.params), (int)paramcount,
                                         fval->name, ast_ctx(fun).file, (int)ast_ctx(fun).line);
            }
        }
    }

    return true;
}

static bool parser_close_paren(parser_t *parser, shunt *sy, bool functions_only)
{
    if (!vec_size(sy->ops)) {
        parseerror(parser, "unmatched closing paren");
        return false;
    }
    /* this would for bit a + (x) because there are no operators inside (x)
    if (sy->ops[vec_size(sy->ops)-1].paren == 1) {
        parseerror(parser, "empty parenthesis expression");
        return false;
    }
    */
    while (vec_size(sy->ops)) {
        if (sy->ops[vec_size(sy->ops)-1].paren == SY_PAREN_FUNC) {
            if (!parser_close_call(parser, sy))
                return false;
            break;
        }
        if (sy->ops[vec_size(sy->ops)-1].paren == SY_PAREN_EXPR) {
            vec_shrinkby(sy->ops, 1);
            return !functions_only;
        }
        if (sy->ops[vec_size(sy->ops)-1].paren == SY_PAREN_INDEX) {
            if (functions_only)
                return false;
            /* pop off the parenthesis */
            vec_shrinkby(sy->ops, 1);
            /* then apply the index operator */
            if (!parser_sy_apply_operator(parser, sy))
                return false;
            return true;
        }
        if (sy->ops[vec_size(sy->ops)-1].paren == SY_PAREN_TERNARY) {
            if (functions_only)
                return false;
            if (vec_last(parser->pot) != POT_TERNARY1) {
                parseerror(parser, "mismatched colon in ternary expression (missing closing paren?)");
                return false;
            }
            vec_last(parser->pot) = POT_TERNARY2;
            /* pop off the parenthesis */
            vec_shrinkby(sy->ops, 1);
            return true;
        }
        if (!parser_sy_apply_operator(parser, sy))
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
    int ternaries = 0;

    sy.out = NULL;
    sy.ops = NULL;

    parser->lex->flags.noops = false;

    parser_reclassify_token(parser);

    while (true)
    {
        if (gotmemberof)
            gotmemberof = false;
        else
            parser->memberof = 0;

        if (OPTS_FLAG(TRANSLATABLE_STRINGS) &&
            parser->tok == TOKEN_IDENT && !strcmp(parser_tokval(parser), "_"))
        {
            /* a translatable string */
            ast_value *val;

            if (wantop) {
                parseerror(parser, "expected operator or end of statement, got constant");
                goto onerr;
            }

            parser->lex->flags.noops = true;
            if (!parser_next(parser) || parser->tok != '(') {
                parseerror(parser, "use _(\"string\") to create a translatable string constant");
                goto onerr;
            }
            parser->lex->flags.noops = false;
            if (!parser_next(parser) || parser->tok != TOKEN_STRINGCONST) {
                parseerror(parser, "expected a constant string in translatable-string extension");
                goto onerr;
            }
            val = parser_const_string(parser, parser_tokval(parser), true);
            wantop = true;
            if (!val)
                return false;
            vec_push(sy.out, syexp(parser_ctx(parser), (ast_expression*)val));
            DEBUGSHUNTDO(con_out("push string\n"));

            if (!parser_next(parser) || parser->tok != ')') {
                parseerror(parser, "expected closing paren after translatable string");
                goto onerr;
            }
        }
        else if (parser->tok == TOKEN_IDENT)
        {
            ast_expression *var;
            if (wantop) {
                parseerror(parser, "expected operator or end of statement");
                goto onerr;
            }
            wantop = true;
            /* variable */
            if (opts.standard == COMPILER_GMQCC)
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
            if (ast_istype(var, ast_value)) {
                ((ast_value*)var)->uses++;
            }
            else if (ast_istype(var, ast_member)) {
                ast_member *mem = (ast_member*)var;
                if (ast_istype(mem->owner, ast_value))
                    ((ast_value*)(mem->owner))->uses++;
            }
            vec_push(sy.out, syexp(parser_ctx(parser), var));
            DEBUGSHUNTDO(con_out("push %s\n", parser_tokval(parser)));
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
            vec_push(sy.out, syexp(parser_ctx(parser), (ast_expression*)val));
            DEBUGSHUNTDO(con_out("push %g\n", parser_token(parser)->constval.f));
        }
        else if (parser->tok == TOKEN_INTCONST || parser->tok == TOKEN_CHARCONST) {
            ast_value *val;
            if (wantop) {
                parseerror(parser, "expected operator or end of statement, got constant");
                goto onerr;
            }
            wantop = true;
            val = parser_const_float(parser, (double)(parser_token(parser)->constval.i));
            if (!val)
                return false;
            vec_push(sy.out, syexp(parser_ctx(parser), (ast_expression*)val));
            DEBUGSHUNTDO(con_out("push %i\n", parser_token(parser)->constval.i));
        }
        else if (parser->tok == TOKEN_STRINGCONST) {
            ast_value *val;
            if (wantop) {
                parseerror(parser, "expected operator or end of statement, got constant");
                goto onerr;
            }
            wantop = true;
            val = parser_const_string(parser, parser_tokval(parser), false);
            if (!val)
                return false;
            vec_push(sy.out, syexp(parser_ctx(parser), (ast_expression*)val));
            DEBUGSHUNTDO(con_out("push string\n"));
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
            vec_push(sy.out, syexp(parser_ctx(parser), (ast_expression*)val));
            DEBUGSHUNTDO(con_out("push '%g %g %g'\n",
                                parser_token(parser)->constval.v.x,
                                parser_token(parser)->constval.v.y,
                                parser_token(parser)->constval.v.z));
        }
        else if (parser->tok == '(') {
            parseerror(parser, "internal error: '(' should be classified as operator");
            goto onerr;
        }
        else if (parser->tok == '[') {
            parseerror(parser, "internal error: '[' should be classified as operator");
            goto onerr;
        }
        else if (parser->tok == ')') {
            if (wantop) {
                DEBUGSHUNTDO(con_out("do[op] )\n"));
                --parens;
                if (parens < 0)
                    break;
                /* we do expect an operator next */
                /* closing an opening paren */
                if (!parser_close_paren(parser, &sy, false))
                    goto onerr;
                if (vec_last(parser->pot) != POT_PAREN) {
                    parseerror(parser, "mismatched parentheses (closing paren during ternary expression?)");
                    goto onerr;
                }
                vec_pop(parser->pot);
            } else {
                DEBUGSHUNTDO(con_out("do[nop] )\n"));
                --parens;
                if (parens < 0)
                    break;
                /* allowed for function calls */
                if (!parser_close_paren(parser, &sy, true))
                    goto onerr;
                if (vec_last(parser->pot) != POT_PAREN) {
                    parseerror(parser, "mismatched parentheses (closing paren during ternary expression?)");
                    goto onerr;
                }
                vec_pop(parser->pot);
            }
            wantop = true;
        }
        else if (parser->tok == ']') {
            if (!wantop)
                parseerror(parser, "operand expected");
            --parens;
            if (parens < 0)
                break;
            if (!parser_close_paren(parser, &sy, false))
                goto onerr;
            if (vec_last(parser->pot) != POT_PAREN) {
                parseerror(parser, "mismatched parentheses (closing paren during ternary expression?)");
                goto onerr;
            }
            vec_pop(parser->pot);
            wantop = true;
        }
        else if (parser->tok == TOKEN_TYPENAME) {
            parseerror(parser, "unexpected typename");
            goto onerr;
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
            const oper_info *op;
            const oper_info *olast = NULL;
            size_t o;
            for (o = 0; o < operator_count; ++o) {
                if ((!(operators[o].flags & OP_PREFIX) == wantop) &&
                    /* !(operators[o].flags & OP_SUFFIX) && / * remove this */
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

            /* a colon without a pervious question mark cannot be a ternary */
            if (!ternaries && op->id == opid2(':','?')) {
                parser->tok = ':';
                break;
            }

            if (op->id == opid1(',')) {
                if (vec_size(parser->pot) && vec_last(parser->pot) == POT_TERNARY2) {
                    (void)!parsewarning(parser, WARN_TERNARY_PRECEDENCE, "suggesting parenthesis around ternary expression");
                }
            }

            if (vec_size(sy.ops) && !vec_last(sy.ops).paren)
                olast = &operators[vec_last(sy.ops).etype-1];

            while (olast && (
                    (op->prec < olast->prec) ||
                    (op->assoc == ASSOC_LEFT && op->prec <= olast->prec) ) )
            {
                if (!parser_sy_apply_operator(parser, &sy))
                    goto onerr;
                if (vec_size(sy.ops) && !vec_last(sy.ops).paren)
                    olast = &operators[vec_last(sy.ops).etype-1];
                else
                    olast = NULL;
            }

            if (op->id == opid1('.') && opts.standard == COMPILER_GMQCC) {
                /* for gmqcc standard: open up the namespace of the previous type */
                ast_expression *prevex = vec_last(sy.out).out;
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
                    size_t sycount = vec_size(sy.out);
                    DEBUGSHUNTDO(con_out("push [op] (\n"));
                    ++parens; vec_push(parser->pot, POT_PAREN);
                    /* we expected an operator, this is the function-call operator */
                    vec_push(sy.ops, syparen(parser_ctx(parser), SY_PAREN_FUNC, sycount-1));
                } else {
                    ++parens; vec_push(parser->pot, POT_PAREN);
                    vec_push(sy.ops, syparen(parser_ctx(parser), SY_PAREN_EXPR, 0));
                    DEBUGSHUNTDO(con_out("push [nop] (\n"));
                }
                wantop = false;
            } else if (op->id == opid1('[')) {
                if (!wantop) {
                    parseerror(parser, "unexpected array subscript");
                    goto onerr;
                }
                ++parens; vec_push(parser->pot, POT_PAREN);
                /* push both the operator and the paren, this makes life easier */
                vec_push(sy.ops, syop(parser_ctx(parser), op));
                vec_push(sy.ops, syparen(parser_ctx(parser), SY_PAREN_INDEX, 0));
                wantop = false;
            } else if (op->id == opid2('?',':')) {
                wantop = false;
                vec_push(sy.ops, syop(parser_ctx(parser), op));
                vec_push(sy.ops, syparen(parser_ctx(parser), SY_PAREN_TERNARY, 0));
                wantop = false;
                ++ternaries;
                vec_push(parser->pot, POT_TERNARY1);
            } else if (op->id == opid2(':','?')) {
                if (!vec_size(parser->pot)) {
                    parseerror(parser, "unexpected colon outside ternary expression (missing parenthesis?)");
                    goto onerr;
                }
                if (vec_last(parser->pot) != POT_TERNARY1) {
                    parseerror(parser, "unexpected colon outside ternary expression (missing parenthesis?)");
                    goto onerr;
                }
                if (!parser_close_paren(parser, &sy, false))
                    goto onerr;
                vec_push(sy.ops, syop(parser_ctx(parser), op));
                wantop = false;
                --ternaries;
            } else {
                DEBUGSHUNTDO(con_out("push operator %s\n", op->op));
                vec_push(sy.ops, syop(parser_ctx(parser), op));
                wantop = !!(op->flags & OP_SUFFIX);
            }
        }
        if (!parser_next(parser)) {
            goto onerr;
        }
        if (parser->tok == ';' ||
            (!parens && parser->tok == ']'))
        {
            break;
        }
    }

    while (vec_size(sy.ops)) {
        if (!parser_sy_apply_operator(parser, &sy))
            goto onerr;
    }

    parser->lex->flags.noops = true;
    if (!vec_size(sy.out)) {
        parseerror(parser, "empty expression");
        expr = NULL;
    } else
        expr = sy.out[0].out;
    vec_free(sy.out);
    vec_free(sy.ops);
    DEBUGSHUNTDO(con_out("shunt done\n"));
    if (vec_size(parser->pot)) {
        parseerror(parser, "internal error: vec_size(parser->pot) = %lu", (unsigned long)vec_size(parser->pot));
        return NULL;
    }
    vec_free(parser->pot);
    return expr;

onerr:
    parser->lex->flags.noops = true;
    vec_free(sy.out);
    vec_free(sy.ops);
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

static void parser_enterblock(parser_t *parser)
{
    vec_push(parser->variables, util_htnew(PARSER_HT_SIZE));
    vec_push(parser->_blocklocals, vec_size(parser->_locals));
    vec_push(parser->typedefs, util_htnew(TYPEDEF_HT_SIZE));
    vec_push(parser->_blocktypedefs, vec_size(parser->_typedefs));
    vec_push(parser->_block_ctx, parser_ctx(parser));
}

static bool parser_leaveblock(parser_t *parser)
{
    bool   rv = true;
    size_t locals, typedefs;

    if (vec_size(parser->variables) <= PARSER_HT_LOCALS) {
        parseerror(parser, "internal error: parser_leaveblock with no block");
        return false;
    }

    util_htdel(vec_last(parser->variables));
    vec_pop(parser->variables);
    if (!vec_size(parser->_blocklocals)) {
        parseerror(parser, "internal error: parser_leaveblock with no block (2)");
        return false;
    }

    locals = vec_last(parser->_blocklocals);
    vec_pop(parser->_blocklocals);
    while (vec_size(parser->_locals) != locals) {
        ast_expression *e = vec_last(parser->_locals);
        ast_value      *v = (ast_value*)e;
        vec_pop(parser->_locals);
        if (ast_istype(e, ast_value) && !v->uses) {
            if (compile_warning(ast_ctx(v), WARN_UNUSED_VARIABLE, "unused variable: `%s`", v->name)) {
                parser->errors++;
                rv = false;
            }
        }
    }

    typedefs = vec_last(parser->_blocktypedefs);
    while (vec_size(parser->_typedefs) != typedefs) {
        ast_delete(vec_last(parser->_typedefs));
        vec_pop(parser->_typedefs);
    }
    util_htdel(vec_last(parser->typedefs));
    vec_pop(parser->typedefs);

    vec_pop(parser->_block_ctx);
    return rv;
}

static void parser_addlocal(parser_t *parser, const char *name, ast_expression *e)
{
    vec_push(parser->_locals, e);
    util_htset(vec_last(parser->variables), name, (void*)e);
}

static bool parse_if(parser_t *parser, ast_block *block, ast_expression **out)
{
    ast_ifthen *ifthen;
    ast_expression *cond, *ontrue, *onfalse = NULL;
    bool ifnot = false;

    lex_ctx ctx = parser_ctx(parser);

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
    if (!parse_statement_or_block(parser, &ontrue)) {
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
        if (!parse_statement_or_block(parser, &onfalse)) {
            ast_delete(ontrue);
            ast_delete(cond);
            return false;
        }
    }

    if (ifnot)
        ifthen = ast_ifthen_new(ctx, cond, onfalse, ontrue);
    else
        ifthen = ast_ifthen_new(ctx, cond, ontrue, onfalse);
    *out = (ast_expression*)ifthen;
    return true;
}

static bool parse_while(parser_t *parser, ast_block *block, ast_expression **out)
{
    ast_loop *aloop;
    ast_expression *cond, *ontrue;

    lex_ctx ctx = parser_ctx(parser);

    (void)block; /* not touching */

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
    if (!parse_statement_or_block(parser, &ontrue)) {
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

    (void)block; /* not touching */

    /* skip the 'do' and get the body */
    if (!parser_next(parser)) {
        parseerror(parser, "expected loop body");
        return false;
    }
    if (!parse_statement_or_block(parser, &ontrue))
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
    ast_loop       *aloop;
    ast_expression *initexpr, *cond, *increment, *ontrue;
    ast_value      *typevar;
    bool   retval = true;

    lex_ctx ctx = parser_ctx(parser);

    parser_enterblock(parser);

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

    typevar = NULL;
    if (parser->tok == TOKEN_IDENT)
        typevar = parser_find_typedef(parser, parser_tokval(parser), 0);

    if (typevar || parser->tok == TOKEN_TYPENAME) {
        if (opts.standard != COMPILER_GMQCC) {
            if (parsewarning(parser, WARN_EXTENSIONS,
                             "current standard does not allow variable declarations in for-loop initializers"))
                goto onerr;
        }
        if (!parse_variable(parser, block, true, CV_VAR, typevar, false))
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
        if (!ast_side_effects(increment)) {
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
    if (!parse_statement_or_block(parser, &ontrue))
        goto onerr;

    aloop = ast_loop_new(ctx, initexpr, cond, NULL, increment, ontrue);
    *out = (ast_expression*)aloop;

    if (!parser_leaveblock(parser))
        retval = false;
    return retval;
onerr:
    if (initexpr)  ast_delete(initexpr);
    if (cond)      ast_delete(cond);
    if (increment) ast_delete(increment);
    (void)!parser_leaveblock(parser);
    return false;
}

static bool parse_return(parser_t *parser, ast_block *block, ast_expression **out)
{
    ast_expression *exp = NULL;
    ast_return     *ret = NULL;
    ast_value      *expected = parser->function->vtype;

    lex_ctx ctx = parser_ctx(parser);

    (void)block; /* not touching */

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
            if (opts.standard != COMPILER_GMQCC)
                (void)!parsewarning(parser, WARN_MISSING_RETURN_VALUES, "return without value");
            else
                parseerror(parser, "return without value");
        }
        ret = ast_return_new(ctx, NULL);
    }
    *out = (ast_expression*)ret;
    return true;
}

static bool parse_break_continue(parser_t *parser, ast_block *block, ast_expression **out, bool is_continue)
{
    lex_ctx ctx = parser_ctx(parser);

    (void)block; /* not touching */

    if (!parser_next(parser) || parser->tok != ';') {
        parseerror(parser, "expected semicolon");
        return false;
    }

    if (!parser_next(parser))
        parseerror(parser, "parse error");

    *out = (ast_expression*)ast_breakcont_new(ctx, is_continue);
    return true;
}

/* returns true when it was a variable qualifier, false otherwise!
 * on error, cvq is set to CV_WRONG
 */
static bool parse_var_qualifiers(parser_t *parser, bool with_local, int *cvq, bool *noref)
{
    bool had_const = false;
    bool had_var   = false;
    bool had_noref = false;

    for (;;) {
        if (!strcmp(parser_tokval(parser), "const"))
            had_const = true;
        else if (!strcmp(parser_tokval(parser), "var"))
            had_var = true;
        else if (with_local && !strcmp(parser_tokval(parser), "local"))
            had_var = true;
        else if (!strcmp(parser_tokval(parser), "noref"))
            had_noref = true;
        else if (!had_const && !had_var && !had_noref) {
            return false;
        }
        else
            break;
        if (!parser_next(parser))
            goto onerr;
    }
    if (had_const)
        *cvq = CV_CONST;
    else if (had_var)
        *cvq = CV_VAR;
    else
        *cvq = CV_NONE;
    *noref = had_noref;
    return true;
onerr:
    parseerror(parser, "parse error after variable qualifier");
    *cvq = CV_WRONG;
    return true;
}

static bool parse_switch(parser_t *parser, ast_block *block, ast_expression **out)
{
    ast_expression *operand;
    ast_value      *opval;
    ast_value      *typevar;
    ast_switch     *switchnode;
    ast_switch_case swcase;

    int  cvq;
    bool noref;

    lex_ctx ctx = parser_ctx(parser);

    (void)block; /* not touching */
    (void)opval;

    /* parse over the opening paren */
    if (!parser_next(parser) || parser->tok != '(') {
        parseerror(parser, "expected switch operand in parenthesis");
        return false;
    }

    /* parse into the expression */
    if (!parser_next(parser)) {
        parseerror(parser, "expected switch operand");
        return false;
    }
    /* parse the operand */
    operand = parse_expression_leave(parser, false);
    if (!operand)
        return false;

    switchnode = ast_switch_new(ctx, operand);

    /* closing paren */
    if (parser->tok != ')') {
        ast_delete(switchnode);
        parseerror(parser, "expected closing paren after 'switch' operand");
        return false;
    }

    /* parse over the opening paren */
    if (!parser_next(parser) || parser->tok != '{') {
        ast_delete(switchnode);
        parseerror(parser, "expected list of cases");
        return false;
    }

    if (!parser_next(parser)) {
        ast_delete(switchnode);
        parseerror(parser, "expected 'case' or 'default'");
        return false;
    }

    /* new block; allow some variables to be declared here */
    parser_enterblock(parser);
    while (true) {
        typevar = NULL;
        if (parser->tok == TOKEN_IDENT)
            typevar = parser_find_typedef(parser, parser_tokval(parser), 0);
        if (typevar || parser->tok == TOKEN_TYPENAME) {
            if (!parse_variable(parser, block, false, CV_NONE, typevar, false)) {
                ast_delete(switchnode);
                return false;
            }
            continue;
        }
        if (parse_var_qualifiers(parser, true, &cvq, &noref))
        {
            if (cvq == CV_WRONG) {
                ast_delete(switchnode);
                return false;
            }
            if (!parse_variable(parser, block, false, cvq, NULL, noref)) {
                ast_delete(switchnode);
                return false;
            }
            continue;
        }
        break;
    }

    /* case list! */
    while (parser->tok != '}') {
        ast_block *caseblock;

        if (parser->tok != TOKEN_KEYWORD) {
            ast_delete(switchnode);
            parseerror(parser, "expected 'case' or 'default'");
            return false;
        }
        if (!strcmp(parser_tokval(parser), "case")) {
            if (!parser_next(parser)) {
                ast_delete(switchnode);
                parseerror(parser, "expected expression for case");
                return false;
            }
            swcase.value = parse_expression_leave(parser, false);
            if (!swcase.value) {
                ast_delete(switchnode);
                parseerror(parser, "expected expression for case");
                return false;
            }
            if (!OPTS_FLAG(RELAXED_SWITCH)) {
                opval = (ast_value*)swcase.value;
                if (!ast_istype(swcase.value, ast_value)) { /* || opval->cvq != CV_CONST) { */
                    parseerror(parser, "case on non-constant values need to be explicitly enabled via -frelaxed-switch");
                    ast_unref(operand);
                    return false;
                }
            }
        }
        else if (!strcmp(parser_tokval(parser), "default")) {
            swcase.value = NULL;
            if (!parser_next(parser)) {
                ast_delete(switchnode);
                parseerror(parser, "expected colon");
                return false;
            }
        }

        /* Now the colon and body */
        if (parser->tok != ':') {
            if (swcase.value) ast_unref(swcase.value);
            ast_delete(switchnode);
            parseerror(parser, "expected colon");
            return false;
        }

        if (!parser_next(parser)) {
            if (swcase.value) ast_unref(swcase.value);
            ast_delete(switchnode);
            parseerror(parser, "expected statements or case");
            return false;
        }
        caseblock = ast_block_new(parser_ctx(parser));
        if (!caseblock) {
            if (swcase.value) ast_unref(swcase.value);
            ast_delete(switchnode);
            return false;
        }
        swcase.code = (ast_expression*)caseblock;
        vec_push(switchnode->cases, swcase);
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
                ast_delete(switchnode);
                return false;
            }
            if (!expr)
                continue;
            if (!ast_block_add_expr(caseblock, expr)) {
                ast_delete(switchnode);
                return false;
            }
        }
    }

    parser_leaveblock(parser);

    /* closing paren */
    if (parser->tok != '}') {
        ast_delete(switchnode);
        parseerror(parser, "expected closing paren of case list");
        return false;
    }
    if (!parser_next(parser)) {
        ast_delete(switchnode);
        parseerror(parser, "parse error after switch");
        return false;
    }
    *out = (ast_expression*)switchnode;
    return true;
}

static bool parse_goto(parser_t *parser, ast_expression **out)
{
    size_t    i;
    ast_goto *gt;

    if (!parser_next(parser) || parser->tok != TOKEN_IDENT) {
        parseerror(parser, "expected label name after `goto`");
        return false;
    }

    gt = ast_goto_new(parser_ctx(parser), parser_tokval(parser));

    for (i = 0; i < vec_size(parser->labels); ++i) {
        if (!strcmp(parser->labels[i]->name, parser_tokval(parser))) {
            ast_goto_set_label(gt, parser->labels[i]);
            break;
        }
    }
    if (i == vec_size(parser->labels))
        vec_push(parser->gotos, gt);

    if (!parser_next(parser) || parser->tok != ';') {
        parseerror(parser, "semicolon expected after goto label");
        return false;
    }
    if (!parser_next(parser)) {
        parseerror(parser, "parse error after goto");
        return false;
    }

    *out = (ast_expression*)gt;
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
        parseerror(parser, "unrecognized hash-keyword: `%s`", parser_tokval(parser));
        return false;
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
    bool       noref;
    int        cvq = CV_NONE;
    ast_value *typevar = NULL;

    *out = NULL;

    if (parser->tok == TOKEN_IDENT)
        typevar = parser_find_typedef(parser, parser_tokval(parser), 0);

    if (typevar || parser->tok == TOKEN_TYPENAME || parser->tok == '.')
    {
        /* local variable */
        if (!block) {
            parseerror(parser, "cannot declare a variable from here");
            return false;
        }
        if (opts.standard == COMPILER_QCC) {
            if (parsewarning(parser, WARN_EXTENSIONS, "missing 'local' keyword when declaring a local variable"))
                return false;
        }
        if (!parse_variable(parser, block, false, CV_NONE, typevar, false))
            return false;
        return true;
    }
    else if (parse_var_qualifiers(parser, !!block, &cvq, &noref))
    {
        if (cvq == CV_WRONG)
            return false;
        return parse_variable(parser, block, true, cvq, NULL, noref);
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
                ast_type_to_string((ast_expression*)tdef, ty, sizeof(ty));
                con_out("__builtin_debug_printtype: `%s`=`%s`\n", tdef->name, ty);
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
            if (opts.standard == COMPILER_QCC) {
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
        parseerror(parser, "Unexpected keyword");
        return false;
    }
    else if (parser->tok == '{')
    {
        ast_block *inner;
        inner = parse_block(parser);
        if (!inner)
            return false;
        *out = (ast_expression*)inner;
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
        label = ast_label_new(parser_ctx(parser), parser_tokval(parser));
        if (!label)
            return false;
        vec_push(parser->labels, label);
        *out = (ast_expression*)label;
        if (!parser_next(parser)) {
            parseerror(parser, "parse error after label");
            return false;
        }
        for (i = 0; i < vec_size(parser->gotos); ++i) {
            if (!strcmp(parser->gotos[i]->name, label->name)) {
                ast_goto_set_label(parser->gotos[i], label);
                vec_remove(parser->gotos, i, 1);
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
        ast_expression *exp = parse_expression(parser, false);
        if (!exp)
            return false;
        *out = exp;
        if (!ast_side_effects(exp)) {
            if (genwarning(ast_ctx(exp), WARN_EFFECTLESS_STATEMENT, "statement has no effect"))
                return false;
        }
        return true;
    }
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
        ast_expression *expr = NULL;
        if (parser->tok == '}')
            break;

        if (!parse_statement(parser, block, &expr, false)) {
            /* parseerror(parser, "parse error"); */
            block = NULL;
            goto cleanup;
        }
        if (!expr)
            continue;
        if (!ast_block_add_expr(block, expr)) {
            ast_delete(block);
            block = NULL;
            goto cleanup;
        }
    }

    if (parser->tok != '}') {
        block = NULL;
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
    block = ast_block_new(parser_ctx(parser));
    if (!block)
        return NULL;
    if (!parse_block_into(parser, block)) {
        ast_block_delete(block);
        return NULL;
    }
    return block;
}

static bool parse_statement_or_block(parser_t *parser, ast_expression **out)
{
    if (parser->tok == '{') {
        *out = (ast_expression*)parse_block(parser);
        return !!*out;
    }
    return parse_statement(parser, NULL, out, false);
}

static bool create_vector_members(ast_value *var, ast_member **me)
{
    size_t i;
    size_t len = strlen(var->name);

    for (i = 0; i < 3; ++i) {
        char *name = mem_a(len+3);
        memcpy(name, var->name, len);
        name[len+0] = '_';
        name[len+1] = 'x'+i;
        name[len+2] = 0;
        me[i] = ast_member_new(ast_ctx(var), (ast_expression*)var, i, name);
        mem_d(name);
        if (!me[i])
            break;
    }
    if (i == 3)
        return true;

    /* unroll */
    do { ast_member_delete(me[--i]); } while(i);
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

    if (vec_size(parser->gotos) || vec_size(parser->labels)) {
        parseerror(parser, "gotos/labels leaking");
        return false;
    }

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
        if (!ast_istype(framenum, ast_value) || !( (ast_value*)framenum )->hasvalue) {
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

            vec_push(parser->globals, (ast_expression*)thinkfunc);
            util_htset(parser->htglobals, thinkfunc->name, thinkfunc);
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
            if (!ast_block_add_expr(block, (ast_expression*)store_frame) ||
                !ast_block_add_expr(block, (ast_expression*)store_nextthink) ||
                !ast_block_add_expr(block, (ast_expression*)store_think))
            {
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

    parser_enterblock(parser);

    for (parami = 0; parami < vec_size(var->expression.params); ++parami) {
        size_t     e;
        ast_value *param = var->expression.params[parami];
        ast_member *me[3];

        if (param->expression.vtype != TYPE_VECTOR &&
            (param->expression.vtype != TYPE_FIELD ||
             param->expression.next->expression.vtype != TYPE_VECTOR))
        {
            continue;
        }

        if (!create_vector_members(param, me)) {
            ast_block_delete(block);
            return false;
        }

        for (e = 0; e < 3; ++e) {
            parser_addlocal(parser, me[e]->name, (ast_expression*)me[e]);
            ast_block_collect(block, (ast_expression*)me[e]);
        }
    }

    func = ast_function_new(ast_ctx(var), var->name, var);
    if (!func) {
        parseerror(parser, "failed to allocate function for `%s`", var->name);
        ast_block_delete(block);
        goto enderr;
    }
    vec_push(parser->functions, func);

    parser->function = func;
    if (!parse_block_into(parser, block)) {
        ast_block_delete(block);
        goto enderrfn;
    }

    vec_push(func->blocks, block);

    parser->function = old;
    if (!parser_leaveblock(parser))
        retval = false;
    if (vec_size(parser->variables) != PARSER_HT_LOCALS) {
        parseerror(parser, "internal error: local scopes left");
        retval = false;
    }

    if (parser->tok == ';')
        return parser_next(parser);
    else if (opts.standard == COMPILER_QCC)
        parseerror(parser, "missing semicolon after function body (mandatory with -std=qcc)");
    return retval;

enderrfn:
    vec_pop(parser->functions);
    ast_function_delete(func);
    var->constval.vfunc = NULL;

enderr:
    (void)!parser_leaveblock(parser);
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

    lex_ctx ctx = ast_ctx(array);

    if (!left || !right) {
        if (left)  ast_delete(left);
        if (right) ast_delete(right);
        return NULL;
    }

    cmp = ast_binary_new(ctx, INSTR_LT,
                         (ast_expression*)index,
                         (ast_expression*)parser_const_float(parser, middle));
    if (!cmp) {
        ast_delete(left);
        ast_delete(right);
        parseerror(parser, "internal error: failed to create comparison for array setter");
        return NULL;
    }

    ifthen = ast_ifthen_new(ctx, (ast_expression*)cmp, left, right);
    if (!ifthen) {
        ast_delete(cmp); /* will delete left and right */
        parseerror(parser, "internal error: failed to create conditional jump for array setter");
        return NULL;
    }

    return (ast_expression*)ifthen;
}

static ast_expression *array_setter_node(parser_t *parser, ast_value *array, ast_value *index, ast_value *value, size_t from, size_t afterend)
{
    lex_ctx ctx = ast_ctx(array);

    if (from+1 == afterend) {
        /* set this value */
        ast_block       *block;
        ast_return      *ret;
        ast_array_index *subscript;
        ast_store       *st;
        int assignop = type_store_instr[value->expression.vtype];

        if (value->expression.vtype == TYPE_FIELD && value->expression.next->expression.vtype == TYPE_VECTOR)
            assignop = INSTR_STORE_V;

        subscript = ast_array_index_new(ctx, (ast_expression*)array, (ast_expression*)parser_const_float(parser, from));
        if (!subscript)
            return NULL;

        st = ast_store_new(ctx, assignop, (ast_expression*)subscript, (ast_expression*)value);
        if (!st) {
            ast_delete(subscript);
            return NULL;
        }

        block = ast_block_new(ctx);
        if (!block) {
            ast_delete(st);
            return NULL;
        }

        if (!ast_block_add_expr(block, (ast_expression*)st)) {
            ast_delete(block);
            return NULL;
        }

        ret = ast_return_new(ctx, NULL);
        if (!ret) {
            ast_delete(block);
            return NULL;
        }

        if (!ast_block_add_expr(block, (ast_expression*)ret)) {
            ast_delete(block);
            return NULL;
        }

        return (ast_expression*)block;
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
    lex_ctx ctx = ast_ctx(array);

    if (from+1 == afterend) {
        /* set this value */
        ast_block       *block;
        ast_return      *ret;
        ast_entfield    *entfield;
        ast_array_index *subscript;
        ast_store       *st;
        int assignop = type_storep_instr[value->expression.vtype];

        if (value->expression.vtype == TYPE_FIELD && value->expression.next->expression.vtype == TYPE_VECTOR)
            assignop = INSTR_STOREP_V;

        subscript = ast_array_index_new(ctx, (ast_expression*)array, (ast_expression*)parser_const_float(parser, from));
        if (!subscript)
            return NULL;

        entfield = ast_entfield_new_force(ctx,
                                          (ast_expression*)entity,
                                          (ast_expression*)subscript,
                                          (ast_expression*)subscript);
        if (!entfield) {
            ast_delete(subscript);
            return NULL;
        }

        st = ast_store_new(ctx, assignop, (ast_expression*)entfield, (ast_expression*)value);
        if (!st) {
            ast_delete(entfield);
            return NULL;
        }

        block = ast_block_new(ctx);
        if (!block) {
            ast_delete(st);
            return NULL;
        }

        if (!ast_block_add_expr(block, (ast_expression*)st)) {
            ast_delete(block);
            return NULL;
        }

        ret = ast_return_new(ctx, NULL);
        if (!ret) {
            ast_delete(block);
            return NULL;
        }

        if (!ast_block_add_expr(block, (ast_expression*)ret)) {
            ast_delete(block);
            return NULL;
        }

        return (ast_expression*)block;
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
    lex_ctx ctx = ast_ctx(array);

    if (from+1 == afterend) {
        ast_return      *ret;
        ast_array_index *subscript;

        subscript = ast_array_index_new(ctx, (ast_expression*)array, (ast_expression*)parser_const_float(parser, from));
        if (!subscript)
            return NULL;

        ret = ast_return_new(ctx, (ast_expression*)subscript);
        if (!ret) {
            ast_delete(subscript);
            return NULL;
        }

        return (ast_expression*)ret;
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
    ast_function   *func = NULL;
    ast_value      *fval = NULL;
    ast_block      *body = NULL;

    fval = ast_value_new(ast_ctx(array), funcname, TYPE_FUNCTION);
    if (!fval) {
        parseerror(parser, "failed to create accessor function value");
        return false;
    }

    func = ast_function_new(ast_ctx(array), funcname, fval);
    if (!func) {
        ast_delete(fval);
        parseerror(parser, "failed to create accessor function node");
        return false;
    }

    body = ast_block_new(ast_ctx(array));
    if (!body) {
        parseerror(parser, "failed to create block for array accessor");
        ast_delete(fval);
        ast_delete(func);
        return false;
    }

    vec_push(func->blocks, body);
    *out = fval;

    vec_push(parser->accessors, fval);

    return true;
}

static bool parser_create_array_setter(parser_t *parser, ast_value *array, const char *funcname)
{
    ast_expression *root = NULL;
    ast_value      *index = NULL;
    ast_value      *value = NULL;
    ast_function   *func;
    ast_value      *fval;

    if (!ast_istype(array->expression.next, ast_value)) {
        parseerror(parser, "internal error: array accessor needs to build an ast_value with a copy of the element type");
        return false;
    }

    if (!parser_create_array_accessor(parser, array, funcname, &fval))
        return false;
    func = fval->constval.vfunc;
    fval->expression.next = (ast_expression*)ast_value_new(ast_ctx(array), "<void>", TYPE_VOID);

    index = ast_value_new(ast_ctx(array), "index", TYPE_FLOAT);
    value = ast_value_copy((ast_value*)array->expression.next);

    if (!index || !value) {
        parseerror(parser, "failed to create locals for array accessor");
        goto cleanup;
    }
    (void)!ast_value_set_name(value, "value"); /* not important */
    vec_push(fval->expression.params, index);
    vec_push(fval->expression.params, value);

    root = array_setter_node(parser, array, index, value, 0, array->expression.count);
    if (!root) {
        parseerror(parser, "failed to build accessor search tree");
        goto cleanup;
    }

    array->setter = fval;
    return ast_block_add_expr(func->blocks[0], root);
cleanup:
    if (index) ast_delete(index);
    if (value) ast_delete(value);
    if (root)  ast_delete(root);
    ast_delete(func);
    ast_delete(fval);
    return false;
}

static bool parser_create_array_field_setter(parser_t *parser, ast_value *array, const char *funcname)
{
    ast_expression *root = NULL;
    ast_value      *entity = NULL;
    ast_value      *index = NULL;
    ast_value      *value = NULL;
    ast_function   *func;
    ast_value      *fval;

    if (!ast_istype(array->expression.next, ast_value)) {
        parseerror(parser, "internal error: array accessor needs to build an ast_value with a copy of the element type");
        return false;
    }

    if (!parser_create_array_accessor(parser, array, funcname, &fval))
        return false;
    func = fval->constval.vfunc;
    fval->expression.next = (ast_expression*)ast_value_new(ast_ctx(array), "<void>", TYPE_VOID);

    entity = ast_value_new(ast_ctx(array), "entity", TYPE_ENTITY);
    index  = ast_value_new(ast_ctx(array), "index",  TYPE_FLOAT);
    value  = ast_value_copy((ast_value*)array->expression.next);
    if (!entity || !index || !value) {
        parseerror(parser, "failed to create locals for array accessor");
        goto cleanup;
    }
    (void)!ast_value_set_name(value, "value"); /* not important */
    vec_push(fval->expression.params, entity);
    vec_push(fval->expression.params, index);
    vec_push(fval->expression.params, value);

    root = array_field_setter_node(parser, array, entity, index, value, 0, array->expression.count);
    if (!root) {
        parseerror(parser, "failed to build accessor search tree");
        goto cleanup;
    }

    array->setter = fval;
    return ast_block_add_expr(func->blocks[0], root);
cleanup:
    if (entity) ast_delete(entity);
    if (index)  ast_delete(index);
    if (value)  ast_delete(value);
    if (root)   ast_delete(root);
    ast_delete(func);
    ast_delete(fval);
    return false;
}

static bool parser_create_array_getter(parser_t *parser, ast_value *array, const ast_expression *elemtype, const char *funcname)
{
    ast_expression *root = NULL;
    ast_value      *index = NULL;
    ast_value      *fval;
    ast_function   *func;

    /* NOTE: checking array->expression.next rather than elemtype since
     * for fields elemtype is a temporary fieldtype.
     */
    if (!ast_istype(array->expression.next, ast_value)) {
        parseerror(parser, "internal error: array accessor needs to build an ast_value with a copy of the element type");
        return false;
    }

    if (!parser_create_array_accessor(parser, array, funcname, &fval))
        return false;
    func = fval->constval.vfunc;
    fval->expression.next = ast_type_copy(ast_ctx(array), elemtype);

    index = ast_value_new(ast_ctx(array), "index", TYPE_FLOAT);

    if (!index) {
        parseerror(parser, "failed to create locals for array accessor");
        goto cleanup;
    }
    vec_push(fval->expression.params, index);

    root = array_getter_node(parser, array, index, 0, array->expression.count);
    if (!root) {
        parseerror(parser, "failed to build accessor search tree");
        goto cleanup;
    }

    array->getter = fval;
    return ast_block_add_expr(func->blocks[0], root);
cleanup:
    if (index) ast_delete(index);
    if (root)  ast_delete(root);
    ast_delete(func);
    ast_delete(fval);
    return false;
}

static ast_value *parse_typename(parser_t *parser, ast_value **storebase, ast_value *cached_typedef);
static ast_value *parse_parameter_list(parser_t *parser, ast_value *var)
{
    lex_ctx     ctx;
    size_t      i;
    ast_value **params;
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

    params = NULL;

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
            param = parse_typename(parser, NULL, NULL);
            if (!param)
                goto on_error;
            vec_push(params, param);
            if (param->expression.vtype >= TYPE_VARIANT) {
                char typename[1024];
                ast_type_to_string((ast_expression*)param, typename, sizeof(typename));
                parseerror(parser, "type not supported as part of a parameter list: %s", typename);
                goto on_error;
            }
        }
    }

    if (vec_size(params) == 1 && params[0]->expression.vtype == TYPE_VOID)
        vec_free(params);

    /* sanity check */
    if (vec_size(params) > 8 && opts.standard == COMPILER_QCC)
        (void)!parsewarning(parser, WARN_EXTENSIONS, "more than 8 parameters are not supported by this standard");

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

    var->expression.params = params;
    params = NULL;

    return var;

on_error:
    ast_delete(var);
    for (i = 0; i < vec_size(params); ++i)
        ast_delete(params[i]);
    vec_free(params);
    return NULL;
}

static ast_value *parse_arraysize(parser_t *parser, ast_value *var)
{
    ast_expression *cexp;
    ast_value      *cval, *tmp;
    lex_ctx ctx;

    ctx = parser_ctx(parser);

    if (!parser_next(parser)) {
        ast_delete(var);
        parseerror(parser, "expected array-size");
        return NULL;
    }

    cexp = parse_expression_leave(parser, true);

    if (!cexp || !ast_istype(cexp, ast_value)) {
        if (cexp)
            ast_unref(cexp);
        ast_delete(var);
        parseerror(parser, "expected array-size as constant positive integer");
        return NULL;
    }
    cval = (ast_value*)cexp;

    tmp = ast_value_new(ctx, "<type[]>", TYPE_ARRAY);
    tmp->expression.next = (ast_expression*)var;
    var = tmp;

    if (cval->expression.vtype == TYPE_INTEGER)
        tmp->expression.count = cval->constval.vint;
    else if (cval->expression.vtype == TYPE_FLOAT)
        tmp->expression.count = cval->constval.vfloat;
    else {
        ast_unref(cexp);
        ast_delete(var);
        parseerror(parser, "array-size must be a positive integer constant");
        return NULL;
    }
    ast_unref(cexp);

    if (parser->tok != ']') {
        ast_delete(var);
        parseerror(parser, "expected ']' after array-size");
        return NULL;
    }
    if (!parser_next(parser)) {
        ast_delete(var);
        parseerror(parser, "error after parsing array size");
        return NULL;
    }
    return var;
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
static ast_value *parse_typename(parser_t *parser, ast_value **storebase, ast_value *cached_typedef)
{
    ast_value *var, *tmp;
    lex_ctx    ctx;

    const char *name = NULL;
    bool        isfield  = false;
    bool        wasarray = false;
    size_t      morefields = 0;

    ctx = parser_ctx(parser);

    /* types may start with a dot */
    if (parser->tok == '.') {
        isfield = true;
        /* if we parsed a dot we need a typename now */
        if (!parser_next(parser)) {
            parseerror(parser, "expected typename for field definition");
            return NULL;
        }

        /* Further dots are handled seperately because they won't be part of the
         * basetype
         */
        while (parser->tok == '.') {
            ++morefields;
            if (!parser_next(parser)) {
                parseerror(parser, "expected typename for field definition");
                return NULL;
            }
        }
    }
    if (parser->tok == TOKEN_IDENT)
        cached_typedef = parser_find_typedef(parser, parser_tokval(parser), 0);
    if (!cached_typedef && parser->tok != TOKEN_TYPENAME) {
        parseerror(parser, "expected typename");
        return NULL;
    }

    /* generate the basic type value */
    if (cached_typedef) {
        var = ast_value_copy(cached_typedef);
        ast_value_set_name(var, "<type(from_def)>");
    } else
        var = ast_value_new(ctx, "<type>", parser_token(parser)->constval.t);

    for (; morefields; --morefields) {
        tmp = ast_value_new(ctx, "<.type>", TYPE_FIELD);
        tmp->expression.next = (ast_expression*)var;
        var = tmp;
    }

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

    /* an opening paren now starts the parameter-list of a function
     * this is where original-QC has parameter lists.
     * We allow a single parameter list here.
     * Much like fteqcc we don't allow `float()() x`
     */
    if (parser->tok == '(') {
        var = parse_parameter_list(parser, var);
        if (!var)
            return NULL;
    }

    /* store the base if requested */
    if (storebase) {
        *storebase = ast_value_copy(var);
        if (isfield) {
            tmp = ast_value_new(ctx, "<type:f>", TYPE_FIELD);
            tmp->expression.next = (ast_expression*)*storebase;
            *storebase = tmp;
        }
    }

    /* there may be a name now */
    if (parser->tok == TOKEN_IDENT) {
        name = util_strdup(parser_tokval(parser));
        /* parse on */
        if (!parser_next(parser)) {
            ast_delete(var);
            parseerror(parser, "error after variable or field declaration");
            return NULL;
        }
    }

    /* now this may be an array */
    if (parser->tok == '[') {
        wasarray = true;
        var = parse_arraysize(parser, var);
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

    /* now there may be function parens again */
    if (parser->tok == '(' && opts.standard == COMPILER_QCC)
        parseerror(parser, "C-style function syntax is not allowed in -std=qcc");
    if (parser->tok == '(' && wasarray)
        parseerror(parser, "arrays as part of a return type is not supported");
    while (parser->tok == '(') {
        var = parse_parameter_list(parser, var);
        if (!var) {
            if (name)
                mem_d((void*)name);
            ast_delete(var);
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

static bool parse_typedef(parser_t *parser)
{
    ast_value      *typevar, *oldtype;
    ast_expression *old;

    typevar = parse_typename(parser, NULL, NULL);

    if (!typevar)
        return false;

    if ( (old = parser_find_var(parser, typevar->name)) ) {
        parseerror(parser, "cannot define a type with the same name as a variable: %s\n"
                   " -> `%s` has been declared here: %s:%i",
                   typevar->name, ast_ctx(old).file, ast_ctx(old).line);
        ast_delete(typevar);
        return false;
    }

    if ( (oldtype = parser_find_typedef(parser, typevar->name, vec_last(parser->_blocktypedefs))) ) {
        parseerror(parser, "type `%s` has already been declared here: %s:%i",
                   typevar->name, ast_ctx(oldtype).file, ast_ctx(oldtype).line);
        ast_delete(typevar);
        return false;
    }

    vec_push(parser->_typedefs, typevar);
    util_htset(vec_last(parser->typedefs), typevar->name, typevar);

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

static bool parse_variable(parser_t *parser, ast_block *localblock, bool nofields, int qualifier, ast_value *cached_typedef, bool noref)
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
    bool      wasarray  = false;

    ast_member *me[3];

    /* get the first complete variable */
    var = parse_typename(parser, &basetype, cached_typedef);
    if (!var) {
        if (basetype)
            ast_delete(basetype);
        return false;
    }

    while (true) {
        proto = NULL;
        wasarray = false;

        /* Part 0: finish the type */
        if (parser->tok == '(') {
            if (opts.standard == COMPILER_QCC)
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
            if (opts.standard == COMPILER_QCC)
                parseerror(parser, "C-style function syntax is not allowed in -std=qcc");
            var = parse_parameter_list(parser, var);
            if (!var) {
                retval = false;
                goto cleanup;
            }
        }

        var->cvq = qualifier;
        /* in a noref section we simply bump the usecount */
        if (noref || parser->noref)
            var->uses++;

        /* Part 1:
         * check for validity: (end_sys_..., multiple-definitions, prototypes, ...)
         * Also: if there was a prototype, `var` will be deleted and set to `proto` which
         * is then filled with the previous definition and the parameter-names replaced.
         */
        if (!localblock) {
            /* Deal with end_sys_ vars */
            was_end = false;
            if (!strcmp(var->name, "end_sys_globals")) {
                var->uses++;
                parser->crc_globals = vec_size(parser->globals);
                was_end = true;
            }
            else if (!strcmp(var->name, "end_sys_fields")) {
                var->uses++;
                parser->crc_fields = vec_size(parser->fields);
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
                if (opts.standard == COMPILER_QCC &&
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
                    for (i = 0; i < vec_size(proto->expression.params); ++i)
                        ast_value_set_name(proto->expression.params[i], var->expression.params[i]->name);
                    ast_delete(var);
                    var = proto;
                }
                else
                {
                    /* other globals */
                    if (old) {
                        if (opts.standard == COMPILER_GMQCC) {
                            parseerror(parser, "global `%s` already declared here: %s:%i",
                                       var->name, ast_ctx(old).file, ast_ctx(old).line);
                            retval = false;
                            goto cleanup;
                        } else {
                            if (parsewarning(parser, WARN_DOUBLE_DECLARATION,
                                             "global `%s` already declared here: %s:%i",
                                             var->name, ast_ctx(old).file, ast_ctx(old).line))
                            {
                                retval = false;
                                goto cleanup;
                            }
                            proto = (ast_value*)old;
                            if (!ast_istype(old, ast_value)) {
                                parseerror(parser, "internal error: not an ast_value");
                                retval = false;
                                proto = NULL;
                                goto cleanup;
                            }
                            ast_delete(var);
                            var = proto;
                        }
                    }
                    if (opts.standard == COMPILER_QCC &&
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
            old = parser_find_local(parser, var->name, vec_size(parser->variables)-1, &isparam);
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
                if (opts.standard != COMPILER_GMQCC) {
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
                if (!create_vector_members(var, me)) {
                    retval = false;
                    goto cleanup;
                }
            }

            if (!localblock) {
                /* deal with global variables, fields, functions */
                if (!nofields && var->expression.vtype == TYPE_FIELD && parser->tok != '=') {
                    var->isfield = true;
                    vec_push(parser->fields, (ast_expression*)var);
                    util_htset(parser->htfields, var->name, var);
                    if (isvector) {
                        for (i = 0; i < 3; ++i) {
                            vec_push(parser->fields, (ast_expression*)me[i]);
                            util_htset(parser->htfields, me[i]->name, me[i]);
                        }
                    }
                }
                else {
                    vec_push(parser->globals, (ast_expression*)var);
                    util_htset(parser->htglobals, var->name, var);
                    if (isvector) {
                        for (i = 0; i < 3; ++i) {
                            vec_push(parser->globals, (ast_expression*)me[i]);
                            util_htset(parser->htglobals, me[i]->name, me[i]);
                        }
                    }
                }
            } else {
                vec_push(localblock->locals, var);
                parser_addlocal(parser, var->name, (ast_expression*)var);
                if (isvector) {
                    for (i = 0; i < 3; ++i) {
                        parser_addlocal(parser, me[i]->name, (ast_expression*)me[i]);
                        ast_block_collect(localblock, (ast_expression*)me[i]);
                    }
                }
            }

        }
        me[0] = me[1] = me[2] = NULL;
        cleanvar = false;
        /* Part 2.2
         * deal with arrays
         */
        if (var->expression.vtype == TYPE_ARRAY) {
            char name[1024];
            snprintf(name, sizeof(name), "%s##SET", var->name);
            if (!parser_create_array_setter(parser, var, name))
                goto cleanup;
            snprintf(name, sizeof(name), "%s##GET", var->name);
            if (!parser_create_array_getter(parser, var, var->expression.next, name))
                goto cleanup;
        }
        else if (!localblock && !nofields &&
                 var->expression.vtype == TYPE_FIELD &&
                 var->expression.next->expression.vtype == TYPE_ARRAY)
        {
            char name[1024];
            ast_expression *telem;
            ast_value      *tfield;
            ast_value      *array = (ast_value*)var->expression.next;

            if (!ast_istype(var->expression.next, ast_value)) {
                parseerror(parser, "internal error: field element type must be an ast_value");
                goto cleanup;
            }

            snprintf(name, sizeof(name), "%s##SETF", var->name);
            if (!parser_create_array_field_setter(parser, array, name))
                goto cleanup;

            telem = ast_type_copy(ast_ctx(var), array->expression.next);
            tfield = ast_value_new(ast_ctx(var), "<.type>", TYPE_FIELD);
            tfield->expression.next = telem;
            snprintf(name, sizeof(name), "%s##GETFP", var->name);
            if (!parser_create_array_getter(parser, array, (ast_expression*)tfield, name)) {
                ast_delete(tfield);
                goto cleanup;
            }
            ast_delete(tfield);
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

        /*
        if (!var || (!localblock && !nofields && basetype->expression.vtype == TYPE_FIELD)) {
        */
        if (!var) {
            parseerror(parser, "missing comma or semicolon while parsing variables");
            break;
        }

        if (localblock && opts.standard == COMPILER_QCC) {
            if (parsewarning(parser, WARN_LOCAL_CONSTANTS,
                             "initializing expression turns variable `%s` into a constant in this standard",
                             var->name) )
            {
                break;
            }
        }

        if (parser->tok != '{') {
            if (parser->tok != '=') {
                parseerror(parser, "missing semicolon or initializer, got: `%s`", parser_tokval(parser));
                break;
            }

            if (!parser_next(parser)) {
                parseerror(parser, "error parsing initializer");
                break;
            }
        }
        else if (opts.standard == COMPILER_QCC) {
            parseerror(parser, "expected '=' before function body in this standard");
        }

        if (parser->tok == '#') {
            ast_function *func = NULL;

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
            if (parser_token(parser)->constval.i < 0) {
                parseerror(parser, "builtin number must be an integer greater than zero");
                break;
            }

            if (var->hasvalue) {
                (void)!parsewarning(parser, WARN_DOUBLE_DECLARATION,
                                    "builtin `%s` has already been defined\n"
                                    " -> previous declaration here: %s:%i",
                                    var->name, ast_ctx(var).file, (int)ast_ctx(var).line);
            }
            else
            {
                func = ast_function_new(ast_ctx(var), var->name, var);
                if (!func) {
                    parseerror(parser, "failed to allocate function for `%s`", var->name);
                    break;
                }
                vec_push(parser->functions, func);

                func->builtin = -parser_token(parser)->constval.i-1;
            }

            if (!parser_next(parser)) {
                parseerror(parser, "expected comma or semicolon");
                if (func)
                    ast_function_delete(func);
                var->constval.vfunc = NULL;
                break;
            }
        }
        else if (parser->tok == '{' || parser->tok == '[')
        {
            size_t i;
            if (localblock) {
                parseerror(parser, "cannot declare functions within functions");
                break;
            }

            if (proto)
                ast_ctx(proto) = parser_ctx(parser);

            if (!parse_function_body(parser, var))
                break;
            ast_delete(basetype);
            for (i = 0; i < vec_size(parser->gotos); ++i)
                parseerror(parser, "undefined label: `%s`", parser->gotos[i]->name);
            vec_free(parser->gotos);
            vec_free(parser->labels);
            return true;
        } else {
            ast_expression *cexp;
            ast_value      *cval;

            cexp = parse_expression_leave(parser, true);
            if (!cexp)
                break;

            if (!localblock) {
                cval = (ast_value*)cexp;
                if (!ast_istype(cval, ast_value) || ((!cval->hasvalue || cval->cvq != CV_CONST) && !cval->isfield))
                    parseerror(parser, "cannot initialize a global constant variable with a non-constant expression");
                else
                {
                    if (opts.standard != COMPILER_GMQCC &&
                        !OPTS_FLAG(INITIALIZED_NONCONSTANTS) &&
                        qualifier != CV_VAR)
                    {
                        var->cvq = CV_CONST;
                    }
                    var->hasvalue = true;
                    if (cval->expression.vtype == TYPE_STRING)
                        var->constval.vstring = parser_strdup(cval->constval.vstring);
                    else if (cval->expression.vtype == TYPE_FIELD)
                        var->constval.vfield = cval;
                    else
                        memcpy(&var->constval, &cval->constval, sizeof(var->constval));
                    ast_unref(cval);
                }
            } else {
                bool cvq;
                shunt sy = { NULL, NULL };
                cvq = var->cvq;
                var->cvq = CV_NONE;
                vec_push(sy.out, syexp(ast_ctx(var), (ast_expression*)var));
                vec_push(sy.out, syexp(ast_ctx(cexp), (ast_expression*)cexp));
                vec_push(sy.ops, syop(ast_ctx(var), parser->assign_op));
                if (!parser_sy_apply_operator(parser, &sy))
                    ast_unref(cexp);
                else {
                    if (vec_size(sy.out) != 1 && vec_size(sy.ops) != 0)
                        parseerror(parser, "internal error: leaked operands");
                    if (!ast_block_add_expr(localblock, (ast_expression*)sy.out[0].out))
                        break;
                }
                vec_free(sy.out);
                vec_free(sy.ops);
                var->cvq = cvq;
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
    if (me[0]) ast_member_delete(me[0]);
    if (me[1]) ast_member_delete(me[1]);
    if (me[2]) ast_member_delete(me[2]);
    return retval;
}

static bool parser_global_statement(parser_t *parser)
{
    int        cvq = CV_WRONG;
    bool       noref = false;
    ast_value *istype = NULL;

    if (parser->tok == TOKEN_IDENT)
        istype = parser_find_typedef(parser, parser_tokval(parser), 0);

    if (istype || parser->tok == TOKEN_TYPENAME || parser->tok == '.')
    {
        return parse_variable(parser, NULL, false, CV_NONE, istype, false);
    }
    else if (parse_var_qualifiers(parser, false, &cvq, &noref))
    {
        if (cvq == CV_WRONG)
            return false;
        return parse_variable(parser, NULL, true, cvq, NULL, noref);
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
        parseerror(parser, "unexpected token: %s", parser->lex->tok.value);
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

static void generate_checksum(parser_t *parser)
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
	    switch (value->expression.vtype) {
	        case TYPE_FLOAT:    crc = progdefs_crc_both(crc, "\tfloat\t"); break;
	        case TYPE_VECTOR:   crc = progdefs_crc_both(crc, "\tvec3_t\t"); break;
	        case TYPE_STRING:   crc = progdefs_crc_both(crc, "\tstring_t\t"); break;
	        case TYPE_FUNCTION: crc = progdefs_crc_both(crc, "\tfunc_t\t"); break;
	        default:
	            crc = progdefs_crc_both(crc, "\tint\t");
	            break;
	    }
	    crc = progdefs_crc_both(crc, value->name);
	    crc = progdefs_crc_both(crc, ";\n");
	}
	crc = progdefs_crc_both(crc, "} globalvars_t;\n\ntypedef struct\n{\n");
	for (i = 0; i < parser->crc_fields; ++i) {
	    if (!ast_istype(parser->fields[i], ast_value))
	        continue;
	    value = (ast_value*)(parser->fields[i]);
	    switch (value->expression.next->expression.vtype) {
	        case TYPE_FLOAT:    crc = progdefs_crc_both(crc, "\tfloat\t"); break;
	        case TYPE_VECTOR:   crc = progdefs_crc_both(crc, "\tvec3_t\t"); break;
	        case TYPE_STRING:   crc = progdefs_crc_both(crc, "\tstring_t\t"); break;
	        case TYPE_FUNCTION: crc = progdefs_crc_both(crc, "\tfunc_t\t"); break;
	        default:
	            crc = progdefs_crc_both(crc, "\tint\t");
	            break;
	    }
	    crc = progdefs_crc_both(crc, value->name);
	    crc = progdefs_crc_both(crc, ";\n");
	}
	crc = progdefs_crc_both(crc, "} entvars_t;\n\n");

	code_crc = crc;
}

static parser_t *parser;

bool parser_init()
{
    size_t i;

    parser = (parser_t*)mem_a(sizeof(parser_t));
    if (!parser)
        return false;

    memset(parser, 0, sizeof(*parser));

    for (i = 0; i < operator_count; ++i) {
        if (operators[i].id == opid1('=')) {
            parser->assign_op = operators+i;
            break;
        }
    }
    if (!parser->assign_op) {
        printf("internal error: initializing parser: failed to find assign operator\n");
        mem_d(parser);
        return false;
    }

    vec_push(parser->variables, parser->htfields  = util_htnew(PARSER_HT_SIZE));
    vec_push(parser->variables, parser->htglobals = util_htnew(PARSER_HT_SIZE));
    vec_push(parser->typedefs, util_htnew(TYPEDEF_HT_SIZE));
    vec_push(parser->_blocktypedefs, 0);
    return true;
}

bool parser_compile()
{
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

bool parser_compile_file(const char *filename)
{
    parser->lex = lex_open(filename);
    if (!parser->lex) {
        con_err("failed to open file \"%s\"\n", filename);
        return false;
    }
    return parser_compile();
}

bool parser_compile_string_len(const char *name, const char *str, size_t len)
{
    parser->lex = lex_open_string(str, len, name);
    if (!parser->lex) {
        con_err("failed to create lexer for string \"%s\"\n", name);
        return false;
    }
    return parser_compile();
}

bool parser_compile_string(const char *name, const char *str)
{
    parser->lex = lex_open_string(str, strlen(str), name);
    if (!parser->lex) {
        con_err("failed to create lexer for string \"%s\"\n", name);
        return false;
    }
    return parser_compile();
}

void parser_cleanup()
{
    size_t i;
    for (i = 0; i < vec_size(parser->accessors); ++i) {
        ast_delete(parser->accessors[i]->constval.vfunc);
        parser->accessors[i]->constval.vfunc = NULL;
        ast_delete(parser->accessors[i]);
    }
    for (i = 0; i < vec_size(parser->functions); ++i) {
        ast_delete(parser->functions[i]);
    }
    for (i = 0; i < vec_size(parser->imm_vector); ++i) {
        ast_delete(parser->imm_vector[i]);
    }
    for (i = 0; i < vec_size(parser->imm_string); ++i) {
        ast_delete(parser->imm_string[i]);
    }
    for (i = 0; i < vec_size(parser->imm_float); ++i) {
        ast_delete(parser->imm_float[i]);
    }
    for (i = 0; i < vec_size(parser->fields); ++i) {
        ast_delete(parser->fields[i]);
    }
    for (i = 0; i < vec_size(parser->globals); ++i) {
        ast_delete(parser->globals[i]);
    }
    vec_free(parser->accessors);
    vec_free(parser->functions);
    vec_free(parser->imm_vector);
    vec_free(parser->imm_string);
    vec_free(parser->imm_float);
    vec_free(parser->globals);
    vec_free(parser->fields);

    for (i = 0; i < vec_size(parser->variables); ++i)
        util_htdel(parser->variables[i]);
    vec_free(parser->variables);
    vec_free(parser->_blocklocals);
    vec_free(parser->_locals);

    for (i = 0; i < vec_size(parser->_typedefs); ++i)
        ast_delete(parser->_typedefs[i]);
    vec_free(parser->_typedefs);
    for (i = 0; i < vec_size(parser->typedefs); ++i)
        util_htdel(parser->typedefs[i]);
    vec_free(parser->typedefs);
    vec_free(parser->_blocktypedefs);

    vec_free(parser->_block_ctx);

    vec_free(parser->labels);
    vec_free(parser->gotos);

    mem_d(parser);
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
            con_out("failed to allocate builder\n");
            return false;
        }

        for (i = 0; i < vec_size(parser->fields); ++i) {
            ast_value *field;
            bool hasvalue;
            if (!ast_istype(parser->fields[i], ast_value))
                continue;
            field = (ast_value*)parser->fields[i];
            hasvalue = field->hasvalue;
            field->hasvalue = false;
            if (!ast_global_codegen((ast_value*)field, ir, true)) {
                con_out("failed to generate field %s\n", field->name);
                ir_builder_delete(ir);
                return false;
            }
            if (hasvalue) {
                ir_value *ifld;
                ast_expression *subtype;
                field->hasvalue = true;
                subtype = field->expression.next;
                ifld = ir_builder_create_field(ir, field->name, subtype->expression.vtype);
                if (subtype->expression.vtype == TYPE_FIELD)
                    ifld->fieldtype = subtype->expression.next->expression.vtype;
                else if (subtype->expression.vtype == TYPE_FUNCTION)
                    ifld->outtype = subtype->expression.next->expression.vtype;
                (void)!ir_value_set_field(field->ir_v, ifld);
            }
        }
        for (i = 0; i < vec_size(parser->globals); ++i) {
            ast_value *asvalue;
            if (!ast_istype(parser->globals[i], ast_value))
                continue;
            asvalue = (ast_value*)(parser->globals[i]);
            if (!asvalue->uses && !asvalue->hasvalue && asvalue->expression.vtype != TYPE_FUNCTION) {
                retval = retval && !genwarning(ast_ctx(asvalue), WARN_UNUSED_VARIABLE,
                                               "unused global: `%s`", asvalue->name);
            }
            if (!ast_global_codegen(asvalue, ir, false)) {
                con_out("failed to generate global %s\n", asvalue->name);
                ir_builder_delete(ir);
                return false;
            }
        }
        for (i = 0; i < vec_size(parser->imm_float); ++i) {
            if (!ast_global_codegen(parser->imm_float[i], ir, false)) {
                con_out("failed to generate global %s\n", parser->imm_float[i]->name);
                ir_builder_delete(ir);
                return false;
            }
        }
        for (i = 0; i < vec_size(parser->imm_string); ++i) {
            if (!ast_global_codegen(parser->imm_string[i], ir, false)) {
                con_out("failed to generate global %s\n", parser->imm_string[i]->name);
                ir_builder_delete(ir);
                return false;
            }
        }
        for (i = 0; i < vec_size(parser->imm_vector); ++i) {
            if (!ast_global_codegen(parser->imm_vector[i], ir, false)) {
                con_out("failed to generate global %s\n", parser->imm_vector[i]->name);
                ir_builder_delete(ir);
                return false;
            }
        }
        for (i = 0; i < vec_size(parser->globals); ++i) {
            ast_value *asvalue;
            if (!ast_istype(parser->globals[i], ast_value))
                continue;
            asvalue = (ast_value*)(parser->globals[i]);
            if (!ast_generate_accessors(asvalue, ir)) {
                ir_builder_delete(ir);
                return false;
            }
        }
        for (i = 0; i < vec_size(parser->fields); ++i) {
            ast_value *asvalue;
            asvalue = (ast_value*)(parser->fields[i]->expression.next);

            if (!ast_istype((ast_expression*)asvalue, ast_value))
                continue;
            if (asvalue->expression.vtype != TYPE_ARRAY)
                continue;
            if (!ast_generate_accessors(asvalue, ir)) {
                ir_builder_delete(ir);
                return false;
            }
        }
        for (i = 0; i < vec_size(parser->functions); ++i) {
            if (!ast_function_codegen(parser->functions[i], ir)) {
                con_out("failed to generate function %s\n", parser->functions[i]->name);
                ir_builder_delete(ir);
                return false;
            }
        }
        if (opts.dump)
            ir_builder_dump(ir, con_out);
        for (i = 0; i < vec_size(parser->functions); ++i) {
            if (!ir_function_finalize(parser->functions[i]->ir_func)) {
                con_out("failed to finalize function %s\n", parser->functions[i]->name);
                ir_builder_delete(ir);
                return false;
            }
        }

        if (retval) {
            if (opts.dumpfin)
                ir_builder_dump(ir, con_out);

            generate_checksum(parser);

            if (!ir_builder_generate(ir, output)) {
                con_out("*** failed to generate output file\n");
                ir_builder_delete(ir);
                return false;
            }
        }

        ir_builder_delete(ir);
        return retval;
    }

    con_out("*** there were compile errors\n");
    return false;
}
