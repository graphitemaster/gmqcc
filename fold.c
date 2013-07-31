/*
 * Copyright (C) 2012, 2013
 *     Dale Weiler
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
#include <string.h>
#include <math.h>

#include "ast.h"
#include "parser.h"

#define FOLD_STRING_UNTRANSLATE_HTSIZE 1024
#define FOLD_STRING_DOTRANSLATE_HTSIZE 1024

/*
 * There is two stages to constant folding in GMQCC: there is the parse
 * stage constant folding, where, witht he help of the AST, operator
 * usages can be constant folded. Then there is the constant folding
 * in the IR for things like eliding if statements, can occur.
 * 
 * This file is thus, split into two parts.
 */
ast_expression **fold_const_values = NULL;

static GMQCC_INLINE bool fold_possible(const ast_value *val) {
    return  ast_istype((ast_expression*)val, ast_value) &&
            val->hasvalue && (val->cvq == CV_CONST)     &&
            ((ast_expression*)val)->vtype != TYPE_FUNCTION; /* why not for functions? */
}

#define isfloat(X)     (((ast_expression*)(X))->vtype == TYPE_FLOAT  && fold_possible(X))
#define isvector(X)    (((ast_expression*)(X))->vtype == TYPE_VECTOR && fold_possible(X))
#define isstring(X)    (((ast_expression*)(X))->vtype == TYPE_STRING && fold_possible(X))
#define isfloats(X,Y)  (isfloat (X) && isfloat(Y))
#define isvectors(X,Y) (isvector(X) && isvector(Y))
#define isstrings(X,Y) (isstring(X) && isstring(Y))

/*
 * Implementation of basic vector math for vec3_t, for trivial constant
 * folding.
 * 
 * TODO: gcc/clang hinting for autovectorization
 */
static GMQCC_INLINE vec3_t vec3_add(vec3_t a, vec3_t b) {
    vec3_t out;
    out.x = a.x + b.x;
    out.y = a.y + b.y;
    out.z = a.z + b.z;
    return out;
}

static GMQCC_INLINE vec3_t vec3_sub(vec3_t a, vec3_t b) {
    vec3_t out;
    out.x = a.x + b.x;
    out.y = a.y + b.y;
    out.z = a.z + b.z;
    return out;
}

static GMQCC_INLINE vec3_t vec3_not(vec3_t a) {
    vec3_t out;
    out.x = !a.x;
    out.y = !a.y;
    out.z = !a.z;
    return out;
}

static GMQCC_INLINE vec3_t vec3_neg(vec3_t a) {
    vec3_t out;
    out.x = -a.x;
    out.y = -a.y;
    out.z = -a.z;
    return out;
}

static GMQCC_INLINE vec3_t vec3_xor(vec3_t a, vec3_t b) {
    vec3_t out;
    out.x = (qcfloat_t)((qcint_t)a.x ^ (qcint_t)b.x);
    out.y = (qcfloat_t)((qcint_t)a.y ^ (qcint_t)b.y);
    out.z = (qcfloat_t)((qcint_t)a.z ^ (qcint_t)b.z);
    return out;
}

static GMQCC_INLINE vec3_t vec3_xorvf(vec3_t a, qcfloat_t b) {
    vec3_t out;
    out.x = (qcfloat_t)((qcint_t)a.x ^ (qcint_t)b);
    out.y = (qcfloat_t)((qcint_t)a.y ^ (qcint_t)b);
    out.z = (qcfloat_t)((qcint_t)a.z ^ (qcint_t)b);
    return out;
}

#if 0
static GMQCC_INLINE qcfloat_t vec3_mulvv(vec3_t a, vec3_t b) {
    return (a.x * b.x + a.y * b.y + a.z * b.z);
}


static GMQCC_INLINE vec3_t vec3_mulvf(vec3_t a, qcfloat_t b) {
    vec3_t out;
    out.x = a.x * b;
    out.y = a.y * b;
    out.z = a.z * b;
    return out;
}
#endif

static GMQCC_INLINE bool vec3_cmp(vec3_t a, vec3_t b) {
    return a.x == b.x &&
           a.y == b.y &&
           a.z == b.z;
}

static GMQCC_INLINE vec3_t vec3_create(float x, float y, float z) {
    vec3_t out;
    out.x = x;
    out.y = y;
    out.z = z;
    return out;
}


static GMQCC_INLINE float fold_immvalue_float(ast_value *expr) {
    return expr->constval.vfloat;
}
static GMQCC_INLINE vec3_t fold_immvalue_vector(ast_value *expr) {
    return expr->constval.vvec;
}
static GMQCC_INLINE const char *fold_immvalue_string(ast_value *expr) {
    return expr->constval.vstring;
}


fold_t *fold_init(parser_t *parser) {
    fold_t *fold                 = (fold_t*)mem_a(sizeof(fold_t));
    fold->parser                 = parser;
    fold->imm_float              = NULL;
    fold->imm_vector             = NULL;
    fold->imm_string             = NULL;
    fold->imm_string_untranslate = util_htnew(FOLD_STRING_UNTRANSLATE_HTSIZE);
    fold->imm_string_dotranslate = util_htnew(FOLD_STRING_DOTRANSLATE_HTSIZE);

    /*
     * prime the tables with common constant values at constant
     * locations.
     */
    (void)fold_constgen_float (fold,  0.0f);
    (void)fold_constgen_float (fold,  1.0f);
    (void)fold_constgen_float (fold, -1.0f);

    (void)fold_constgen_vector(fold, vec3_create(0.0f, 0.0f, 0.0f));

    return fold;
}

bool fold_generate(fold_t *fold, ir_builder *ir) {
    /* generate globals for immediate folded values */
    size_t     i;
    ast_value *cur;

    for (i = 0; i < vec_size(fold->imm_float);   ++i)
        if (!ast_global_codegen ((cur = fold->imm_float[i]), ir, false)) goto err;
    for (i = 0; i < vec_size(fold->imm_vector);  ++i)
        if (!ast_global_codegen((cur = fold->imm_vector[i]), ir, false)) goto err;
    for (i = 0; i < vec_size(fold->imm_string);  ++i)
        if (!ast_global_codegen((cur = fold->imm_string[i]), ir, false)) goto err;

    return true;

err:
    con_out("failed to generate global %s\n", cur->name);
    ir_builder_delete(ir);
    return false;
}

void fold_cleanup(fold_t *fold) {
    size_t i;

    for (i = 0; i < vec_size(fold->imm_float);  ++i) ast_delete(fold->imm_float[i]);
    for (i = 0; i < vec_size(fold->imm_vector); ++i) ast_delete(fold->imm_vector[i]);
    for (i = 0; i < vec_size(fold->imm_string); ++i) ast_delete(fold->imm_string[i]);

    vec_free(fold->imm_float);
    vec_free(fold->imm_vector);
    vec_free(fold->imm_string);

    util_htdel(fold->imm_string_untranslate);
    util_htdel(fold->imm_string_dotranslate);

    mem_d(fold);
}

static lex_ctx_t fold_ctx(fold_t *fold) {
    lex_ctx_t ctx;
    if (fold->parser->lex)
        return parser_ctx(fold->parser);

    memset(&ctx, 0, sizeof(ctx));
    return ctx;
}

ast_expression *fold_constgen_float(fold_t *fold, qcfloat_t value) {
    ast_value  *out = NULL;
    size_t      i;

    for (i = 0; i < vec_size(fold->imm_float); i++) {
        if (fold->imm_float[i]->constval.vfloat == value)
            return (ast_expression*)fold->imm_float[i];
    }

    out                  = ast_value_new(fold_ctx(fold), "#IMMEDIATE", TYPE_FLOAT);
    out->cvq             = CV_CONST;
    out->hasvalue        = true;
    out->constval.vfloat = value;

    vec_push(fold->imm_float, out);

    return (ast_expression*)out;
}

ast_expression *fold_constgen_vector(fold_t *fold, vec3_t value) {
    ast_value *out;
    size_t     i;

    for (i = 0; i < vec_size(fold->imm_vector); i++) {
        if (vec3_cmp(fold->imm_vector[i]->constval.vvec, value))
            return (ast_expression*)fold->imm_vector[i];
    }

    out                = ast_value_new(fold_ctx(fold), "#IMMEDIATE", TYPE_VECTOR);
    out->cvq           = CV_CONST;
    out->hasvalue      = true;
    out->constval.vvec = value;

    vec_push(fold->imm_vector, out);

    return (ast_expression*)out;
}

ast_expression *fold_constgen_string(fold_t *fold, const char *str, bool translate) {
    hash_table_t *table = (translate) ? fold->imm_string_untranslate : fold->imm_string_dotranslate;
    ast_value    *out   = NULL;
    size_t        hash  = util_hthash(table, str);

    if ((out = (ast_value*)util_htgeth(table, str, hash)))
        return (ast_expression*)out;

    if (translate) {
        char name[32];
        util_snprintf(name, sizeof(name), "dotranslate_%lu", (unsigned long)(fold->parser->translated++));
        out                    = ast_value_new(parser_ctx(fold->parser), name, TYPE_STRING);
        out->expression.flags |= AST_FLAG_INCLUDE_DEF; /* def needs to be included for translatables */
    } else
        out                    = ast_value_new(fold_ctx(fold), "#IMMEDIATE", TYPE_STRING);

    out->cvq              = CV_CONST;
    out->hasvalue         = true;
    out->isimm            = true;
    out->constval.vstring = parser_strdup(str);

    vec_push(fold->imm_string, out);
    util_htseth(table, str, hash, out);

    return (ast_expression*)out;
}

ast_expression *fold_op(fold_t *fold, const oper_info *info, ast_expression **opexprs) {
    ast_value *a = (ast_value*)opexprs[0];
    ast_value *b = (ast_value*)opexprs[1];
    ast_value *c = (ast_value*)opexprs[2];

    /* can a fold operation be applied to this operator usage? */
    if (!info->folds)
        return NULL;

    switch(info->operands) {
        case 3: if(!c) return NULL;
        case 2: if(!b) return NULL;
    }

    switch(info->id) {
        case opid2('-', 'P'):
            return isfloat (a)             ? fold_constgen_float (fold, fold_immvalue_float(a))
                 : isvector(a)             ? fold_constgen_vector(fold, vec3_neg(fold_immvalue_vector(a)))
                 : NULL;
        case opid2('!', 'P'):
            return isfloat (a)             ? fold_constgen_float (fold, !fold_immvalue_float(a))
                 : isvector(a)             ? fold_constgen_vector(fold, vec3_not(fold_immvalue_vector(a)))
                 : isstring(a)             ? fold_constgen_float (fold, !fold_immvalue_string(a) || OPTS_FLAG(TRUE_EMPTY_STRINGS) ? 0 : !*fold_immvalue_string(a))
                 : NULL;
        case opid1('+'):
            return isfloats(a,b)           ? fold_constgen_float (fold, fold_immvalue_float(a) + fold_immvalue_float(b))
                 : isvectors(a,b)          ? fold_constgen_vector(fold, vec3_add(fold_immvalue_vector(a), fold_immvalue_vector(b)))
                 : NULL;
        case opid1('-'):
            return isfloats(a,b)           ? fold_constgen_float (fold, fold_immvalue_float(a) - fold_immvalue_float(b))
                 : isvectors(a,b)          ? fold_constgen_vector(fold, vec3_sub(fold_immvalue_vector(a), fold_immvalue_vector(b)))
                 : NULL;
        case opid1('%'):
            return isfloats(a,b)           ? fold_constgen_float (fold, (qcfloat_t)(((qcint_t)fold_immvalue_float(a)) % ((qcint_t)fold_immvalue_float(b))))
                 : NULL;
        case opid1('|'):
            return isfloats(a,b)           ? fold_constgen_float (fold, (qcfloat_t)(((qcint_t)fold_immvalue_float(a)) | ((qcint_t)fold_immvalue_float(b))))
                 : NULL;
        case opid1('&'):
            return isfloats(a,b)           ? fold_constgen_float (fold, (qcfloat_t)(((qcint_t)fold_immvalue_float(a)) & ((qcint_t)fold_immvalue_float(b))))
                 : NULL;
        case opid1('^'):
            return isfloats(a,b)           ? fold_constgen_float (fold, (qcfloat_t)(((qcint_t)fold_immvalue_float(a)) ^ ((qcint_t)fold_immvalue_float(b))))
                 : isvectors(a,b)          ? fold_constgen_vector(fold, vec3_xor  (fold_immvalue_vector(a), fold_immvalue_vector(b)))
                 : isvector(a)&&isfloat(b) ? fold_constgen_vector(fold, vec3_xorvf(fold_immvalue_vector(a), fold_immvalue_float (b)))
                 : NULL;
        case opid2('<','<'):
            return isfloats(a,b)           ? fold_constgen_float (fold, (qcfloat_t)(((qcuint_t)(fold_immvalue_float(a)) << ((qcuint_t)fold_immvalue_float(b)))))
                 : NULL;
        case opid2('>','>'):
            return isfloats(a,b)           ? fold_constgen_float (fold, (qcfloat_t)(((qcuint_t)(fold_immvalue_float(a)) >> ((qcuint_t)fold_immvalue_float(b)))))
                 : NULL;
        case opid2('*','*'):
            return isfloats(a,b)           ? fold_constgen_float (fold, (qcfloat_t)powf(fold_immvalue_float(a), fold_immvalue_float(b)))
                 : NULL;
        case opid2('!','='):
            return isfloats(a,b)           ? fold_constgen_float (fold, fold_immvalue_float(a) != fold_immvalue_float(b))
                 : NULL;
        case opid2('=','='):
            return isfloats(a,b)           ? fold_constgen_float (fold, fold_immvalue_float(a) == fold_immvalue_float(b))
                 : NULL;
        case opid2('~','P'):
            return isfloat(a)              ? fold_constgen_float (fold, ~(qcint_t)fold_immvalue_float(a))
                 : NULL;

        case opid1('*'):
            /* TODO: seperate function for this case */
            return NULL;
        case opid1('/'):
            /* TODO: seperate function for this case */
            return NULL;
        case opid2('|','|'):
            /* TODO: seperate function for this case */
            return NULL;
        case opid2('&','&'):
            /* TODO: seperate function for this case */
            return NULL;
        case opid2('?',':'):
            /* TODO: seperate function for this case */
            return NULL;
        case opid3('<','=','>'):
            /* TODO: seperate function for this case */
            return NULL;
    }
    return NULL;
}
