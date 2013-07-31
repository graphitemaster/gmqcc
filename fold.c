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
static GMQCC_INLINE bool fold_possible(const ast_value *val) {
    return  ast_istype((ast_expression*)val, ast_value) &&
            val->hasvalue && (val->cvq == CV_CONST)     &&
            ((ast_expression*)val)->vtype != TYPE_FUNCTION; /* why not for functions? */
}

#define isfloatonly(X)  (((ast_expression*)(X))->vtype == TYPE_FLOAT)
#define isvectoronly(X) (((ast_expression*)(X))->vtype == TYPE_VECTOR)
#define isstringonly(X) (((ast_expression*)(X))->vtype == TYPE_STRING)
#define isfloat(X)      (isfloatonly (X) && fold_possible(X))
#define isvector(X)     (isvectoronly(X) && fold_possible(X))
#define isstring(X)     (isstringonly(X) && fold_possible(X))
#define isfloats(X,Y)   (isfloat     (X) && isfloat (Y))
#define isvectors(X,Y)  (isvector    (X) && isvector(Y))
/*#define isstrings(X,Y)  (isstring    (X) && isstring(Y))*/

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

static GMQCC_INLINE bool vec3_pbool(vec3_t a) {
    return (a.x && a.y && a.z);
}

#define fold_immvalue_float(E)  ((E)->constval.vfloat)
#define fold_immvalue_vector(E) ((E)->constval.vvec)
#define fold_immvalue_string(E) ((E)->constval.vstring)

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

static GMQCC_INLINE ast_expression *fold_op_mul_vec(fold_t *fold, vec3_t *vec, ast_value *sel, const char *set) {
    /*
     * vector-component constant folding works by matching the component sets
     * to eliminate expensive operations on whole-vectors (3 components at runtime).
     * to achive this effect in a clean manner this function generalizes the 
     * values through the use of a set paramater, which is used as an indexing method
     * for creating the elided ast binary expression.
     *
     * Consider 'n 0 0' where y, and z need to be tested for 0, and x is
     * used as the value in a binary operation generating an INSTR_MUL instruction
     * to acomplish the indexing of the correct component value we use set[0], set[1], set[2]
     * as x, y, z, where the values of those operations return 'x', 'y', 'z'. Because
     * of how ASCII works we can easily deliniate:
     * vec.z is the same as set[2]-'x' for when set[2] is 'z', 'z'-'x' results in a
     * literal value of 2, using this 2, we know that taking the address of vec->x (float)
     * and indxing it with this literal will yeild the immediate address of that component
     * 
     * Of course more work needs to be done to generate the correct index for the ast_member_new
     * call, which is no problem: set[0]-'x' suffices that job.
     */
    qcfloat_t x = (&vec->x)[set[0]-'x'];
    qcfloat_t y = (&vec->x)[set[1]-'x'];
    qcfloat_t z = (&vec->x)[set[2]-'x'];

    if (!y && !z) {
        ast_expression *out;
        ++opts_optimizationcount[OPTIM_VECTOR_COMPONENTS];
        out                        = (ast_expression*)ast_member_new(fold_ctx(fold), (ast_expression*)sel, set[0]-'x', NULL);
        out->node.keep             = false;
        ((ast_member*)out)->rvalue = true;
        if (!x != -1)
            return (ast_expression*)ast_binary_new(fold_ctx(fold), INSTR_MUL_F, fold_constgen_float(fold, x), out);
    }

    return NULL;
}


static GMQCC_INLINE ast_expression *fold_op_mul(fold_t *fold, ast_value *a, ast_value *b) {
    if (isfloatonly(a)) {
        return (fold_possible(a) && fold_possible(b))
                    ? fold_constgen_vector(fold, vec3_mulvf(fold_immvalue_vector(b), fold_immvalue_float(a))) /* a=float,  b=vector */
                    : NULL;                                                                                   /* cannot fold them   */
    } else if (isfloats(a, b)) {
        return fold_constgen_float(fold, fold_immvalue_float(a) * fold_immvalue_float(b));                    /* a=float,  b=float  */
    } else if (isvectoronly(a)) {
        if (isfloat(b) && fold_possible(a))
            return fold_constgen_vector(fold, vec3_mulvf(fold_immvalue_vector(a), fold_immvalue_float(b)));   /* a=vector, b=float  */
        else if (isvector(b)) {
            /*
             * if we made it here the two ast values are both vectors. However because vectors are represented as
             * three float values, constant folding can still occur within reason of the individual const-qualification
             * of the components the vector is composed of.
             */
            if (fold_possible(a) && fold_possible(b))
                return fold_constgen_float(fold, vec3_mulvv(fold_immvalue_vector(a), fold_immvalue_vector(b)));
            else if (OPTS_OPTIMIZATION(OPTIM_VECTOR_COMPONENTS) && fold_possible(a)) {
                vec3_t          vec = fold_immvalue_vector(a);
                ast_expression *out;
                if ((out = fold_op_mul_vec(fold, &vec, b, "xyz"))) return out;
                if ((out = fold_op_mul_vec(fold, &vec, b, "yxz"))) return out;
                if ((out = fold_op_mul_vec(fold, &vec, b, "zxy"))) return out;
                return NULL;
            } else if (OPTS_OPTIMIZATION(OPTIM_VECTOR_COMPONENTS) && fold_possible(b)) {
                vec3_t          vec = fold_immvalue_vector(b);
                ast_expression *out;
                if ((out = fold_op_mul_vec(fold, &vec, a, "xyz"))) return out;
                if ((out = fold_op_mul_vec(fold, &vec, a, "yxz"))) return out;
                if ((out = fold_op_mul_vec(fold, &vec, a, "zxy"))) return out;
                return NULL;
            }
        }
    }
    return NULL;
}

static GMQCC_INLINE bool fold_immediate_true(fold_t *fold, ast_value *v) {
    switch (v->expression.vtype) {
        case TYPE_FLOAT:   return !!v->constval.vfloat;
        case TYPE_INTEGER: return !!v->constval.vint;
        case TYPE_VECTOR:  return OPTS_FLAG(CORRECT_LOGIC) ? vec3_pbool(v->constval.vvec) : !!v->constval.vvec.x;
        case TYPE_STRING:
            if (!v->constval.vstring)
                return false;
            if (OPTS_FLAG(TRUE_EMPTY_STRINGS))
                return true;
            return !!v->constval.vstring[0];
        default:
            compile_error(fold_ctx(fold), "internal error: fold_immediate_true on invalid type");
            break;
    }
    return !!v->constval.vfunc;
}

static GMQCC_INLINE ast_expression *fold_op_div(fold_t *fold, ast_value *a, ast_value *b) {
    if (isfloatonly(a)) {
        return (fold_possible(a) && fold_possible(b))
                    ? fold_constgen_float(fold, fold_immvalue_float(a) / fold_immvalue_float(b))
                    : NULL;
    }

    if (isvectoronly(a)) {
        if (fold_possible(a) && fold_possible(b))
            return fold_constgen_vector(fold, vec3_mulvf(fold_immvalue_vector(a), 1.0f / fold_immvalue_float(b)));
        else if (fold_possible(b))
            return fold_constgen_float (fold, 1.0f / fold_immvalue_float(b));
    }
    return NULL;
}

static GMQCC_INLINE ast_expression *fold_op_andor(fold_t *fold, ast_value *a, ast_value *b, bool isor) {
    if (fold_possible(a) && fold_possible(b)) {
        if (OPTS_FLAG(PERL_LOGIC)) {
            if (fold_immediate_true(fold, b))
                return (ast_expression*)b;
        } else {
            return ((isor) ? (fold_immediate_true(fold, a) || fold_immediate_true(fold, b))
                           : (fold_immediate_true(fold, a) && fold_immediate_true(fold, b)))
                                 ? (ast_expression*)fold->imm_float[1]  /* 1.0f */
                                 : (ast_expression*)fold->imm_float[0]; /* 0.0f */
        }
    }
    return NULL;
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

        case opid1('*'):     return fold_op_mul  (fold, a, b);
        case opid1('/'):     return fold_op_div  (fold, a, b);
        case opid2('|','|'): return fold_op_andor(fold, a, b, true);
        case opid2('&','&'): return fold_op_andor(fold, a, b, false);
        case opid2('?',':'):
            /* TODO: seperate function for this case */
            return NULL;
        case opid3('<','=','>'):
            /* TODO: seperate function for this case */
            return NULL;
    }
    return NULL;
}
