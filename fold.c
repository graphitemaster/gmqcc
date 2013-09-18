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

#define isfloat(X)      (((ast_expression*)(X))->vtype == TYPE_FLOAT)
#define isvector(X)     (((ast_expression*)(X))->vtype == TYPE_VECTOR)
#define isstring(X)     (((ast_expression*)(X))->vtype == TYPE_STRING)
#define isfloats(X,Y)   (isfloat  (X) && isfloat (Y))

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

static GMQCC_INLINE vec3_t vec3_neg(vec3_t a) {
    vec3_t out;
    out.x = -a.x;
    out.y = -a.y;
    out.z = -a.z;
    return out;
}

static GMQCC_INLINE vec3_t vec3_or(vec3_t a, vec3_t b) {
    vec3_t out;
    out.x = (qcfloat_t)(((qcint_t)a.x) | ((qcint_t)b.x));
    out.y = (qcfloat_t)(((qcint_t)a.y) | ((qcint_t)b.y));
    out.z = (qcfloat_t)(((qcint_t)a.z) | ((qcint_t)b.z));
    return out;
}

static GMQCC_INLINE vec3_t vec3_orvf(vec3_t a, qcfloat_t b) {
    vec3_t out;
    out.x = (qcfloat_t)(((qcint_t)a.x) | ((qcint_t)b));
    out.y = (qcfloat_t)(((qcint_t)a.y) | ((qcint_t)b));
    out.z = (qcfloat_t)(((qcint_t)a.z) | ((qcint_t)b));
    return out;
}

static GMQCC_INLINE vec3_t vec3_and(vec3_t a, vec3_t b) {
    vec3_t out;
    out.x = (qcfloat_t)(((qcint_t)a.x) & ((qcint_t)b.x));
    out.y = (qcfloat_t)(((qcint_t)a.y) & ((qcint_t)b.y));
    out.z = (qcfloat_t)(((qcint_t)a.z) & ((qcint_t)b.z));
    return out;
}

static GMQCC_INLINE vec3_t vec3_andvf(vec3_t a, qcfloat_t b) {
    vec3_t out;
    out.x = (qcfloat_t)(((qcint_t)a.x) & ((qcint_t)b));
    out.y = (qcfloat_t)(((qcint_t)a.y) & ((qcint_t)b));
    out.z = (qcfloat_t)(((qcint_t)a.z) & ((qcint_t)b));
    return out;
}

static GMQCC_INLINE vec3_t vec3_xor(vec3_t a, vec3_t b) {
    vec3_t out;
    out.x = (qcfloat_t)(((qcint_t)a.x) ^ ((qcint_t)b.x));
    out.y = (qcfloat_t)(((qcint_t)a.y) ^ ((qcint_t)b.y));
    out.z = (qcfloat_t)(((qcint_t)a.z) ^ ((qcint_t)b.z));
    return out;
}

static GMQCC_INLINE vec3_t vec3_xorvf(vec3_t a, qcfloat_t b) {
    vec3_t out;
    out.x = (qcfloat_t)(((qcint_t)a.x) ^ ((qcint_t)b));
    out.y = (qcfloat_t)(((qcint_t)a.y) ^ ((qcint_t)b));
    out.z = (qcfloat_t)(((qcint_t)a.z) ^ ((qcint_t)b));
    return out;
}

static GMQCC_INLINE vec3_t vec3_not(vec3_t a) {
    vec3_t out;
    out.x = (qcfloat_t)(~((qcint_t)a.x));
    out.y = (qcfloat_t)(~((qcint_t)a.y));
    out.z = (qcfloat_t)(~((qcint_t)a.z));
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

static GMQCC_INLINE qcfloat_t vec3_notf(vec3_t a) {
    return (!a.x && !a.y && !a.z);
}

static GMQCC_INLINE bool vec3_pbool(vec3_t a) {
    return (a.x && a.y && a.z);
}

static GMQCC_INLINE vec3_t vec3_cross(vec3_t a, vec3_t b) {
    vec3_t out;
    out.x = a.y * b.z - a.z * b.y;
    out.y = a.z * b.x - a.x * b.z;
    out.z = a.x * b.y - a.y * b.x;
    return out;
}

static lex_ctx_t fold_ctx(fold_t *fold) {
    lex_ctx_t ctx;
    if (fold->parser->lex)
        return parser_ctx(fold->parser);

    memset(&ctx, 0, sizeof(ctx));
    return ctx;
}

static GMQCC_INLINE bool fold_immediate_true(fold_t *fold, ast_value *v) {
    switch (v->expression.vtype) {
        case TYPE_FLOAT:
            return !!v->constval.vfloat;
        case TYPE_INTEGER:
            return !!v->constval.vint;
        case TYPE_VECTOR:
            if (OPTS_FLAG(CORRECT_LOGIC))
                return vec3_pbool(v->constval.vvec);
            return !!(v->constval.vvec.x);
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

/* Handy macros to determine if an ast_value can be constant folded. */
#define fold_can_1(X)  \
    (ast_istype(((ast_expression*)(X)), ast_value) && (X)->hasvalue && ((X)->cvq == CV_CONST) && \
                ((ast_expression*)(X))->vtype != TYPE_FUNCTION)

#define fold_can_2(X, Y) (fold_can_1(X) && fold_can_1(Y))
#define fold_can_div(X) (fold_immvalue_float(X) != 0.0f)

#define fold_immvalue_float(E)  ((E)->constval.vfloat)
#define fold_immvalue_vector(E) ((E)->constval.vvec)
#define fold_immvalue_string(E) ((E)->constval.vstring)

#ifdef INFINITY
#   define fold_infinity_float  INFINITY
#else
#   define fold_infinity_float  (1.0 / 0.0)
#endif /*! INFINITY */

#define fold_infinity_vector \
    vec3_create(             \
        fold_infinity_float, \
        fold_infinity_float, \
        fold_infinity_float  \
    )

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
    (void)fold_constgen_float (fold,  fold_infinity_float); /* +inf */

    (void)fold_constgen_vector(fold, vec3_create(0.0f, 0.0f, 0.0f));
    (void)fold_constgen_vector(fold, vec3_create(-1.0f, -1.0f, -1.0f));
    (void)fold_constgen_vector(fold, fold_infinity_vector); /* +inf */

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


static GMQCC_INLINE ast_expression *fold_op_mul_vec(fold_t *fold, vec3_t vec, ast_value *sel, const char *set) {
    /*
     * vector-component constant folding works by matching the component sets
     * to eliminate expensive operations on whole-vectors (3 components at runtime).
     * to achive this effect in a clean manner this function generalizes the
     * values through the use of a set paramater, which is used as an indexing method
     * for creating the elided ast binary expression.
     *
     * Consider 'n 0 0' where y, and z need to be tested for 0, and x is
     * used as the value in a binary operation generating an INSTR_MUL instruction,
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
    qcfloat_t x = (&vec.x)[set[0]-'x'];
    qcfloat_t y = (&vec.x)[set[1]-'x'];
    qcfloat_t z = (&vec.x)[set[2]-'x'];

    if (!y && !z) {
        ast_expression *out;
        ++opts_optimizationcount[OPTIM_VECTOR_COMPONENTS];
        out                        = (ast_expression*)ast_member_new(fold_ctx(fold), (ast_expression*)sel, set[0]-'x', NULL);
        out->node.keep             = false;
        ((ast_member*)out)->rvalue = true;
        if (x != -1.0f)
            return (ast_expression*)ast_binary_new(fold_ctx(fold), INSTR_MUL_F, fold_constgen_float(fold, x), out);
    }
    return NULL;
}


static GMQCC_INLINE ast_expression *fold_op_neg(fold_t *fold, ast_value *a) {
    if (isfloat(a)) {
        if (fold_can_1(a))
            return fold_constgen_float(fold, -fold_immvalue_float(a));
    } else if (isvector(a)) {
        if (fold_can_1(a))
            return fold_constgen_vector(fold, vec3_neg(fold_immvalue_vector(a)));
    }
    return NULL;
}

static GMQCC_INLINE ast_expression *fold_op_not(fold_t *fold, ast_value *a) {
    if (isfloat(a)) {
        if (fold_can_1(a))
            return fold_constgen_float(fold, !fold_immvalue_float(a));
    } else if (isvector(a)) {
        if (fold_can_1(a))
            return fold_constgen_float(fold, vec3_notf(fold_immvalue_vector(a)));
    } else if (isstring(a)) {
        if (fold_can_1(a)) {
            if (OPTS_FLAG(TRUE_EMPTY_STRINGS))
                return fold_constgen_float(fold, !fold_immvalue_string(a));
            else
                return fold_constgen_float(fold, !fold_immvalue_string(a) || !*fold_immvalue_string(a));
        }
    }
    return NULL;
}

static GMQCC_INLINE ast_expression *fold_op_add(fold_t *fold, ast_value *a, ast_value *b) {
    if (isfloat(a)) {
        if (fold_can_2(a, b))
            return fold_constgen_float(fold, fold_immvalue_float(a) + fold_immvalue_float(b));
    } else if (isvector(a)) {
        if (fold_can_2(a, b))
            return fold_constgen_vector(fold, vec3_add(fold_immvalue_vector(a), fold_immvalue_vector(b)));
    }
    return NULL;
}

static GMQCC_INLINE ast_expression *fold_op_sub(fold_t *fold, ast_value *a, ast_value *b) {
    if (isfloat(a)) {
        if (fold_can_2(a, b))
            return fold_constgen_float(fold, fold_immvalue_float(a) - fold_immvalue_float(b));
    } else if (isvector(a)) {
        if (fold_can_2(a, b))
            return fold_constgen_vector(fold, vec3_sub(fold_immvalue_vector(a), fold_immvalue_vector(b)));
    }
    return NULL;
}

static GMQCC_INLINE ast_expression *fold_op_mul(fold_t *fold, ast_value *a, ast_value *b) {
    if (isfloat(a)) {
        if (isvector(b)) {
            if (fold_can_2(a, b))
                return fold_constgen_vector(fold, vec3_mulvf(fold_immvalue_vector(b), fold_immvalue_float(a)));
        } else {
            if (fold_can_2(a, b))
                return fold_constgen_float(fold, fold_immvalue_float(a) * fold_immvalue_float(b));
        }
    } else if (isvector(a)) {
        if (isfloat(b)) {
            if (fold_can_2(a, b))
                return fold_constgen_vector(fold, vec3_mulvf(fold_immvalue_vector(a), fold_immvalue_float(b)));
        } else {
            if (fold_can_2(a, b)) {
                return fold_constgen_float(fold, vec3_mulvv(fold_immvalue_vector(a), fold_immvalue_vector(b)));
            } else if (OPTS_OPTIMIZATION(OPTIM_VECTOR_COMPONENTS) && fold_can_1(a)) {
                ast_expression *out;
                if ((out = fold_op_mul_vec(fold, fold_immvalue_vector(a), b, "xyz"))) return out;
                if ((out = fold_op_mul_vec(fold, fold_immvalue_vector(a), b, "yxz"))) return out;
                if ((out = fold_op_mul_vec(fold, fold_immvalue_vector(a), b, "zxy"))) return out;
            } else if (OPTS_OPTIMIZATION(OPTIM_VECTOR_COMPONENTS) && fold_can_1(b)) {
                ast_expression *out;
                if ((out = fold_op_mul_vec(fold, fold_immvalue_vector(b), a, "xyz"))) return out;
                if ((out = fold_op_mul_vec(fold, fold_immvalue_vector(b), a, "yxz"))) return out;
                if ((out = fold_op_mul_vec(fold, fold_immvalue_vector(b), a, "zxy"))) return out;
            }
        }
    }
    return NULL;
}

static GMQCC_INLINE ast_expression *fold_op_div(fold_t *fold, ast_value *a, ast_value *b) {
    if (isfloat(a)) {
        if (fold_can_2(a, b)) {
            if (fold_can_div(b))
                return fold_constgen_float(fold, fold_immvalue_float(a) / fold_immvalue_float(b));
            else
                return (ast_expression*)fold->imm_float[3]; /* inf */
        } else if (fold_can_1(b)) {
            return (ast_expression*)ast_binary_new(
                fold_ctx(fold),
                INSTR_MUL_F,
                (ast_expression*)a,
                fold_constgen_float(fold, 1.0f / fold_immvalue_float(b))
            );
        }
    } else if (isvector(a)) {
        if (fold_can_2(a, b)) {
            if (fold_can_div(b)) {
                printf("hit wrong logic\n");
                return fold_constgen_vector(fold, vec3_mulvf(fold_immvalue_vector(a), 1.0f / fold_immvalue_float(b)));
            }
            else {
                printf("hit logic\n");
                return (ast_expression*)fold->imm_vector[2]; /* inf */
            }
        } else {
            return (ast_expression*)ast_binary_new(
                fold_ctx(fold),
                INSTR_MUL_VF,
                (ast_expression*)a,
                (fold_can_1(b))
                    ? (ast_expression*)fold_constgen_float(fold, 1.0f / fold_immvalue_float(b))
                    : (ast_expression*)ast_binary_new(
                                            fold_ctx(fold),
                                            INSTR_DIV_F,
                                            (ast_expression*)fold->imm_float[1],
                                            (ast_expression*)b
                    )
            );
        }
    }
    return NULL;
}

static GMQCC_INLINE ast_expression *fold_op_mod(fold_t *fold, ast_value *a, ast_value *b) {
    if (fold_can_2(a, b)) {
        if (fold_can_div(b))
            return fold_constgen_float(fold, (qcfloat_t)(((qcint_t)fold_immvalue_float(a)) % ((qcint_t)fold_immvalue_float(b))));
        else
            return (ast_expression*)fold->imm_float[3]; /* inf */
    }
    return NULL;
}

static GMQCC_INLINE ast_expression *fold_op_bor(fold_t *fold, ast_value *a, ast_value *b) {
    if (isfloat(a)) {
        if (fold_can_2(a, b))
            return fold_constgen_float(fold, (qcfloat_t)(((qcint_t)fold_immvalue_float(a)) | ((qcint_t)fold_immvalue_float(b))));
    } else {
        if (isvector(b)) {
            if (fold_can_2(a, b))
                return fold_constgen_vector(fold, vec3_or(fold_immvalue_vector(a), fold_immvalue_vector(b)));
        } else {
            if (fold_can_2(a, b))
                return fold_constgen_vector(fold, vec3_orvf(fold_immvalue_vector(a), fold_immvalue_float(b)));
        }
    }
    return NULL;
}

static GMQCC_INLINE ast_expression *fold_op_band(fold_t *fold, ast_value *a, ast_value *b) {
    if (isfloat(a)) {
        if (fold_can_2(a, b))
            return fold_constgen_float(fold, (qcfloat_t)(((qcint_t)fold_immvalue_float(a)) & ((qcint_t)fold_immvalue_float(b))));
    } else {
        if (isvector(b)) {
            if (fold_can_2(a, b))
                return fold_constgen_vector(fold, vec3_and(fold_immvalue_vector(a), fold_immvalue_vector(b)));
        } else {
            if (fold_can_2(a, b))
                return fold_constgen_vector(fold, vec3_andvf(fold_immvalue_vector(a), fold_immvalue_float(b)));
        }
    }
    return NULL;
}

static GMQCC_INLINE ast_expression *fold_op_xor(fold_t *fold, ast_value *a, ast_value *b) {
    if (isfloat(a)) {
        if (fold_can_2(a, b))
            return fold_constgen_float(fold, (qcfloat_t)(((qcint_t)fold_immvalue_float(a)) ^ ((qcint_t)fold_immvalue_float(b))));
    } else {
        if (isvector(b)) {
            if (fold_can_2(a, b))
                return fold_constgen_vector(fold, vec3_xor(fold_immvalue_vector(a), fold_immvalue_vector(b)));
        } else {
            if (fold_can_2(a, b))
                return fold_constgen_vector(fold, vec3_xorvf(fold_immvalue_vector(a), fold_immvalue_float(b)));
        }
    }
    return NULL;
}

static GMQCC_INLINE ast_expression *fold_op_lshift(fold_t *fold, ast_value *a, ast_value *b) {
    if (fold_can_2(a, b) && isfloats(a, b))
        return fold_constgen_float(fold, (qcfloat_t)((qcuint_t)(fold_immvalue_float(a)) << (qcuint_t)(fold_immvalue_float(b))));
    return NULL;
}

static GMQCC_INLINE ast_expression *fold_op_rshift(fold_t *fold, ast_value *a, ast_value *b) {
    if (fold_can_2(a, b) && isfloats(a, b))
        return fold_constgen_float(fold, (qcfloat_t)((qcuint_t)(fold_immvalue_float(a)) >> (qcuint_t)(fold_immvalue_float(b))));
    return NULL;
}

static GMQCC_INLINE ast_expression *fold_op_andor(fold_t *fold, ast_value *a, ast_value *b, float expr) {
    if (fold_can_2(a, b)) {
        if (OPTS_FLAG(PERL_LOGIC)) {
            if (fold_immediate_true(fold, a))
                return (ast_expression*)b;
        } else {
            return fold_constgen_float (
                fold,
                ((expr) ? (fold_immediate_true(fold, a) || fold_immediate_true(fold, b))
                        : (fold_immediate_true(fold, a) && fold_immediate_true(fold, b)))
                            ? 1
                            : 0
            );
        }
    }
    return NULL;
}

static GMQCC_INLINE ast_expression *fold_op_tern(fold_t *fold, ast_value *a, ast_value *b, ast_value *c) {
    if (fold_can_1(a)) {
        return fold_immediate_true(fold, a)
                    ? (ast_expression*)b
                    : (ast_expression*)c;
    }
    return NULL;
}

static GMQCC_INLINE ast_expression *fold_op_exp(fold_t *fold, ast_value *a, ast_value *b) {
    if (fold_can_2(a, b))
        return fold_constgen_float(fold, (qcfloat_t)powf(fold_immvalue_float(a), fold_immvalue_float(b)));
    return NULL;
}

static GMQCC_INLINE ast_expression *fold_op_lteqgt(fold_t *fold, ast_value *a, ast_value *b) {
    if (fold_can_2(a,b)) {
        if (fold_immvalue_float(a) <  fold_immvalue_float(b)) return (ast_expression*)fold->imm_float[2];
        if (fold_immvalue_float(a) == fold_immvalue_float(b)) return (ast_expression*)fold->imm_float[0];
        if (fold_immvalue_float(a) >  fold_immvalue_float(b)) return (ast_expression*)fold->imm_float[1];
    }
    return NULL;
}

static GMQCC_INLINE ast_expression *fold_op_cmp(fold_t *fold, ast_value *a, ast_value *b, bool ne) {
    if (fold_can_2(a, b)) {
        return fold_constgen_float(
                    fold,
                    (ne) ? (fold_immvalue_float(a) != fold_immvalue_float(b))
                         : (fold_immvalue_float(a) == fold_immvalue_float(b))
                );
    }
    return NULL;
}

static GMQCC_INLINE ast_expression *fold_op_bnot(fold_t *fold, ast_value *a) {
    if (isfloat(a)) {
        if (fold_can_1(a))
            return fold_constgen_float(fold, ~((qcint_t)fold_immvalue_float(a)));
    } else {
        if (isvector(a)) {
            if (fold_can_1(a))
                return fold_constgen_vector(fold, vec3_not(fold_immvalue_vector(a)));
        }
    }
    return NULL;
}

static GMQCC_INLINE ast_expression *fold_op_cross(fold_t *fold, ast_value *a, ast_value *b) {
    if (fold_can_2(a, b))
        return fold_constgen_vector(fold, vec3_cross(fold_immvalue_vector(a), fold_immvalue_vector(b)));
    return NULL;
}

ast_expression *fold_op(fold_t *fold, const oper_info *info, ast_expression **opexprs) {
    ast_value      *a = (ast_value*)opexprs[0];
    ast_value      *b = (ast_value*)opexprs[1];
    ast_value      *c = (ast_value*)opexprs[2];
    ast_expression *e = NULL;

    /* can a fold operation be applied to this operator usage? */
    if (!info->folds)
        return NULL;

    switch(info->operands) {
        case 3: if(!c) return NULL;
        case 2: if(!b) return NULL;
        case 1:
        if(!a) {
            compile_error(fold_ctx(fold), "internal error: fold_op no operands to fold\n");
            return NULL;
        }
    }

    /*
     * we could use a boolean and default case but ironically gcc produces
     * invalid broken assembly from that operation. clang/tcc get it right,
     * but interestingly ignore compiling this to a jump-table when I do that,
     * this happens to be the most efficent method, since you have per-level
     * granularity on the pointer check happening only for the case you check
     * it in. Opposed to the default method which would involve a boolean and
     * pointer check after wards.
     */
    #define fold_op_case(ARGS, ARGS_OPID, OP, ARGS_FOLD)    \
        case opid##ARGS ARGS_OPID:                          \
            if ((e = fold_op_##OP ARGS_FOLD)) {             \
                ++opts_optimizationcount[OPTIM_CONST_FOLD]; \
            }                                               \
            return e

    switch(info->id) {
        fold_op_case(2, ('-', 'P'),    neg,    (fold, a));
        fold_op_case(2, ('!', 'P'),    not,    (fold, a));
        fold_op_case(1, ('+'),         add,    (fold, a, b));
        fold_op_case(1, ('-'),         sub,    (fold, a, b));
        fold_op_case(1, ('*'),         mul,    (fold, a, b));
        fold_op_case(1, ('/'),         div,    (fold, a, b));
        fold_op_case(1, ('%'),         mod,    (fold, a, b));
        fold_op_case(1, ('|'),         bor,    (fold, a, b));
        fold_op_case(1, ('&'),         band,   (fold, a, b));
        fold_op_case(1, ('^'),         xor,    (fold, a, b));
        fold_op_case(2, ('<', '<'),    lshift, (fold, a, b));
        fold_op_case(2, ('>', '>'),    rshift, (fold, a, b));
        fold_op_case(2, ('|', '|'),    andor,  (fold, a, b, true));
        fold_op_case(2, ('&', '&'),    andor,  (fold, a, b, false));
        fold_op_case(2, ('?', ':'),    tern,   (fold, a, b, c));
        fold_op_case(2, ('*', '*'),    exp,    (fold, a, b));
        fold_op_case(3, ('<','=','>'), lteqgt, (fold, a, b));
        fold_op_case(2, ('!', '='),    cmp,    (fold, a, b, true));
        fold_op_case(2, ('=', '='),    cmp,    (fold, a, b, false));
        fold_op_case(2, ('~', 'P'),    bnot,   (fold, a));
        fold_op_case(2, ('>', '<'),    cross,  (fold, a, b));
    }
    #undef fold_op_case
    compile_error(fold_ctx(fold), "internal error: attempted to constant-fold for unsupported operator");
    return NULL;
}

/*
 * Constant folding for compiler intrinsics, simaler approach to operator
 * folding, primarly: individual functions for each intrinsics to fold,
 * and a generic selection function.
 */
static GMQCC_INLINE ast_expression *fold_intrin_mod(fold_t *fold, ast_value *lhs, ast_value *rhs) {
    return fold_constgen_float(
                fold,
                fmodf(
                    fold_immvalue_float(lhs),
                    fold_immvalue_float(rhs)
                )
            );
}

static GMQCC_INLINE ast_expression *fold_intrin_pow(fold_t *fold, ast_value *lhs, ast_value *rhs) {
    return fold_constgen_float(
                fold,
                powf(
                    fold_immvalue_float(lhs),
                    fold_immvalue_float(rhs)
                )
            );
}

static GMQCC_INLINE ast_expression *fold_intrin_exp(fold_t *fold, ast_value *value) {
    return fold_constgen_float(fold, exp(fold_immvalue_float(value)));
}

static GMQCC_INLINE ast_expression *fold_intrin_isnan(fold_t *fold, ast_value *value) {
    return fold_constgen_float(fold, isnan(fold_immvalue_float(value)) != 0.0f);
}

static GMQCC_INLINE ast_expression *fold_intrin_fabs(fold_t *fold, ast_value *value) {
    return fold_constgen_float(fold, fabs(fold_immvalue_float(value)));
}

ast_expression *fold_intrin(fold_t *fold, const char *intrin, ast_expression **arg) {
    if (!strcmp(intrin, "mod"))   return fold_intrin_mod  (fold, (ast_value*)arg[0], (ast_value*)arg[1]);
    if (!strcmp(intrin, "pow"))   return fold_intrin_pow  (fold, (ast_value*)arg[0], (ast_value*)arg[1]);
    if (!strcmp(intrin, "exp"))   return fold_intrin_exp  (fold, (ast_value*)arg[0]);
    if (!strcmp(intrin, "isnan")) return fold_intrin_isnan(fold, (ast_value*)arg[0]);
    if (!strcmp(intrin, "fabs"))  return fold_intrin_fabs (fold, (ast_value*)arg[0]);

    return NULL;
}

/*
 * These are all the actual constant folding methods that happen in between
 * the AST/IR stage of the compiler , i.e eliminating branches for const
 * expressions, which is the only supported thing so far. We undefine the
 * testing macros here because an ir_value is differant than an ast_value.
 */
#undef expect
#undef isfloat
#undef isstring
#undef isvector
#undef fold_immvalue_float
#undef fold_immvalue_string
#undef fold_immvalue_vector
#undef fold_can_1
#undef fold_can_2

#define isfloat(X)              ((X)->vtype == TYPE_FLOAT)
/*#define isstring(X)             ((X)->vtype == TYPE_STRING)*/
/*#define isvector(X)             ((X)->vtype == TYPE_VECTOR)*/
#define fold_immvalue_float(X)  ((X)->constval.vfloat)
/*#define fold_immvalue_vector(X) ((X)->constval.vvec)*/
/*#define fold_immvalue_string(X) ((X)->constval.vstring)*/
#define fold_can_1(X)           ((X)->hasvalue && (X)->cvq == CV_CONST)
/*#define fold_can_2(X,Y)         (fold_can_1(X) && fold_can_1(Y))*/


int fold_cond(ir_value *condval, ast_function *func, ast_ifthen *branch) {
    if (isfloat(condval) && fold_can_1(condval) && OPTS_OPTIMIZATION(OPTIM_CONST_FOLD_DCE)) {
        ast_expression_codegen *cgen;
        ir_block               *elide;
        ir_value               *dummy;
        bool                    istrue  = (fold_immvalue_float(condval) != 0.0f && branch->on_true);
        bool                    isfalse = (fold_immvalue_float(condval) == 0.0f && branch->on_false);
        ast_expression         *path    = (istrue)  ? branch->on_true  :
                                          (isfalse) ? branch->on_false : NULL;
        if (!path) {
            /*
             * no path to take implies that the evaluation is if(0) and there
             * is no else block. so eliminate all the code.
             */
            ++opts_optimizationcount[OPTIM_CONST_FOLD_DCE];
            return true;
        }

        if (!(elide = ir_function_create_block(ast_ctx(branch), func->ir_func, ast_function_label(func, ((istrue) ? "ontrue" : "onfalse")))))
            return false;
        if (!(*(cgen = path->codegen))((ast_expression*)path, func, false, &dummy))
            return false;
        if (!ir_block_create_jump(func->curblock, ast_ctx(branch), elide))
            return false;
        /*
         * now the branch has been eliminated and the correct block for the constant evaluation
         * is expanded into the current block for the function.
         */
        func->curblock = elide;
        ++opts_optimizationcount[OPTIM_CONST_FOLD_DCE];
        return true;
    }
    return -1; /* nothing done */
}
