#include <string.h>
#include <math.h>

#include "fold.h"
#include "ast.h"
#include "ir.h"

#include "parser.h"

#define FOLD_STRING_UNTRANSLATE_HTSIZE 1024
#define FOLD_STRING_DOTRANSLATE_HTSIZE 1024

/* The options to use for inexact and arithmetic exceptions */
#define FOLD_ROUNDING SFLOAT_ROUND_NEAREST_EVEN
#define FOLD_TINYNESS SFLOAT_TBEFORE

/*
 * Comparing float values is an unsafe operation when the operands to the
 * comparison are floating point values that are inexact. For instance 1/3 is an
 * inexact value. The FPU is meant to raise exceptions when these sorts of things
 * happen, including division by zero, underflows and overflows. The C standard
 * library provides us with the <fenv.h> header to gain access to the floating-
 * point environment and lets us set the rounding mode and check for these exceptions.
 * The problem is the standard C library allows an implementation to leave these
 * stubbed out and does not require they be implemented. Furthermore, depending
 * on implementations there is no control over the FPU. This is an IEE 754
 * conforming implementation in software to compensate.
 */
typedef uint32_t sfloat_t;

union sfloat_cast_t {
    qcfloat_t f;
    sfloat_t s;
};

/* Exception flags */
enum sfloat_exceptionflags_t {
    SFLOAT_NOEXCEPT  = 0,
    SFLOAT_INVALID   = 1,
    SFLOAT_DIVBYZERO = 4,
    SFLOAT_OVERFLOW  = 8,
    SFLOAT_UNDERFLOW = 16,
    SFLOAT_INEXACT   = 32
};

/* Rounding modes */
enum sfloat_roundingmode_t {
    SFLOAT_ROUND_NEAREST_EVEN,
    SFLOAT_ROUND_DOWN,
    SFLOAT_ROUND_UP,
    SFLOAT_ROUND_TO_ZERO
};

/* Underflow tininess-detection mode */
enum sfloat_tdetect_t {
    SFLOAT_TAFTER,
    SFLOAT_TBEFORE
};

struct sfloat_state_t {
    sfloat_roundingmode_t roundingmode;
    sfloat_exceptionflags_t exceptionflags;
    sfloat_tdetect_t tiny;
};

/* Counts the number of leading zero bits before the most-significand one bit. */
#ifdef _MSC_VER
/* MSVC has an intrinsic for this */
    static GMQCC_INLINE uint32_t sfloat_clz(uint32_t x) {
        int r = 0;
        _BitScanForward(&r, x);
        return r;
    }
#   define SFLOAT_CLZ(X, SUB) \
        (sfloat_clz((X)) - (SUB))
#elif defined(__GNUC__) || defined(__CLANG__)
/* Clang and GCC have a builtin for this */
#   define SFLOAT_CLZ(X, SUB) \
        (__builtin_clz((X)) - (SUB))
#else
/* Native fallback */
    static GMQCC_INLINE uint32_t sfloat_popcnt(uint32_t x) {
        x -= ((x >> 1) & 0x55555555);
        x  = (((x >> 2) & 0x33333333) + (x & 0x33333333));
        x  = (((x >> 4) + x) & 0x0F0F0F0F);
        x += x >> 8;
        x += x >> 16;
        return x & 0x0000003F;
    }
    static GMQCC_INLINE uint32_t sfloat_clz(uint32_t x) {
        x |= (x >> 1);
        x |= (x >> 2);
        x |= (x >> 4);
        x |= (x >> 8);
        x |= (x >> 16);
        return 32 - sfloat_popcnt(x);
    }
#   define SFLOAT_CLZ(X, SUB) \
        (sfloat_clz((X) - (SUB)))
#endif

/* The value of a NaN */
#define SFLOAT_NAN 0xFFFFFFFF
/* Test if NaN */
#define SFLOAT_ISNAN(A) \
    (0xFF000000 < (uint32_t)((A) << 1))
/* Test if signaling NaN */
#define SFLOAT_ISSNAN(A) \
    (((((A) >> 22) & 0x1FF) == 0x1FE) && ((A) & 0x003FFFFF))
/* Raise exception */
#define SFLOAT_RAISE(STATE, FLAGS) \
    ((STATE)->exceptionflags = (sfloat_exceptionflags_t)((STATE)->exceptionflags | (FLAGS)))
/*
 * Shifts `A' right by the number of bits given in `COUNT'. If any non-zero bits
 * are shifted off they are forced into the least significand bit of the result
 * by setting it to one. As a result of this, the value of `COUNT' can be
 * arbitrarily large; if `COUNT' is greater than 32, the result will be either
 * zero or one, depending on whether `A' is a zero or non-zero. The result is
 * stored into the value pointed by `Z'.
 */
#define SFLOAT_SHIFT(SIZE, A, COUNT, Z)                                      \
    *(Z) = ((COUNT) == 0)                                                    \
        ? 1                                                                  \
        : (((COUNT) < (SIZE))                                                \
            ? ((A) >> (COUNT)) | (((A) << ((-(COUNT)) & ((SIZE) - 1))) != 0) \
            : ((A) != 0))

/* Extract fractional component */
#define SFLOAT_EXTRACT_FRAC(X) \
    ((uint32_t)((X) & 0x007FFFFF))
/* Extract exponent component */
#define SFLOAT_EXTRACT_EXP(X) \
    ((int16_t)((X) >> 23) & 0xFF)
/* Extract sign bit */
#define SFLOAT_EXTRACT_SIGN(X) \
    ((X) >> 31)
/*
 * Normalizes the subnormal value represented by the denormalized significand
 * `SA'. The normalized exponent and significand are stored at the locations
 * pointed by `Z' and `SZ' respectively.
 */
#define SFLOAT_SUBNORMALIZE(SA, Z, SZ) \
    (void)(*(SZ) = (SA) << SFLOAT_CLZ((SA), 8), *(Z) = 1 - SFLOAT_CLZ((SA), 8))
/*
 * Packs the sign `SIGN', exponent `EXP' and significand `SIG' into the value
 * giving the result.
 *
 * After the shifting into their proper positions, the fields are added together
 * to form the result. This means any integer portion of `SIG' will be added
 * to the exponent. Similarly, because a properly normalized significand will
 * always have an integer portion equal to one, the exponent input `EXP' should
 * be one less than the desired result exponent whenever the significant input
 * `SIG' is a complete, normalized significand.
 */
#define SFLOAT_PACK(SIGN, EXP, SIG) \
    (sfloat_t)((((uint32_t)(SIGN)) << 31) + (((uint32_t)(EXP)) << 23) + (SIG))

/*
 * Takes two values `a' and `b', one of which is a NaN, and returns the appropriate
 * NaN result. If either `a' or `b' is a signaling NaN than an invalid exception is
 * raised.
 */
static sfloat_t sfloat_propagate_nan(sfloat_state_t *state, sfloat_t a, sfloat_t b) {
    bool isnan_a  = SFLOAT_ISNAN(a);
    bool issnan_a = SFLOAT_ISSNAN(a);
    bool isnan_b  = SFLOAT_ISNAN(b);
    bool issnan_b = SFLOAT_ISSNAN(b);

    a |= 0x00400000;
    b |= 0x00400000;

    if (issnan_a | issnan_b)
        SFLOAT_RAISE(state, SFLOAT_INVALID);
    if (isnan_a)
        return (issnan_a & isnan_b) ? b : a;
    return b;
}

/*
 * Takes an abstract value having sign `sign_z', exponent `exp_z', and significand
 * `sig_z' and returns the appropriate value corresponding to the abstract input.
 *
 * The abstract value is simply rounded and packed into the format. If the abstract
 * input cannot be represented exactly an inexact exception is raised. If the
 * abstract input is too large, the overflow and inexact exceptions are both raised
 * and an infinity or maximal finite value is returned. If the abstract value is
 * too small, the value is rounded to a subnormal and the underflow and inexact
 * exceptions are only raised if the value cannot be represented exactly with
 * a subnormal.
 *
 * The input significand `sig_z' has it's binary point between bits 30 and 29,
 * this is seven bits to the left of its usual location. The shifted significand
 * must be normalized or smaller than this. If it's not normalized then the exponent
 * `exp_z' must be zero; in that case, the result returned is a subnormal number
 * which must not require rounding. In the more usual case where the significand
 * is normalized, the exponent must be one less than the *true* exponent.
 *
 * The handling of underflow and overflow is otherwise in alignment with IEC/IEEE.
 */
static sfloat_t SFLOAT_PACK_round(sfloat_state_t *state, bool sign_z, int16_t exp_z, uint32_t sig_z) {
    sfloat_roundingmode_t mode      = state->roundingmode;
    bool                  even      = !!(mode == SFLOAT_ROUND_NEAREST_EVEN);
    unsigned char         increment = 0x40;
    unsigned char         bits      = sig_z & 0x7F;

    if (!even) {
        if (mode == SFLOAT_ROUND_TO_ZERO)
            increment = 0;
        else {
            increment = 0x7F;
            if (sign_z) {
                if (mode == SFLOAT_ROUND_UP)
                    increment = 0;
            } else {
                if (mode == SFLOAT_ROUND_DOWN)
                    increment = 0;
            }
        }
    }

    if (0xFD <= (uint16_t)exp_z) {
        if ((0xFD < exp_z) || ((exp_z == 0xFD) && ((int32_t)(sig_z + increment) < 0))) {
            SFLOAT_RAISE(state, SFLOAT_OVERFLOW | SFLOAT_INEXACT);
            return SFLOAT_PACK(sign_z, 0xFF, 0) - (increment == 0);
        }
        if (exp_z < 0) {
            /* Check for underflow */
            bool tiny = (state->tiny == SFLOAT_TBEFORE) || (exp_z < -1) || (sig_z + increment < 0x80000000);
            SFLOAT_SHIFT(32, sig_z, -exp_z, &sig_z);
            exp_z = 0;
            bits = sig_z & 0x7F;
            if (tiny && bits)
                SFLOAT_RAISE(state, SFLOAT_UNDERFLOW);
        }
    }
    if (bits)
        SFLOAT_RAISE(state, SFLOAT_INEXACT);
    sig_z = (sig_z + increment) >> 7;
    sig_z &= ~(((bits ^ 0x40) == 0) & even);
    if (sig_z == 0)
        exp_z = 0;
    return SFLOAT_PACK(sign_z, exp_z, sig_z);
}

/*
 * Takes an abstract value having sign `sign_z', exponent `exp_z' and significand
 * `sig_z' and returns the appropriate value corresponding to the abstract input.
 * This function is exactly like `PACK_round' except the significand does not have
 * to be normalized.
 *
 * Bit 31 of the significand must be zero and the exponent must be one less than
 * the *true* exponent.
 */
static sfloat_t SFLOAT_PACK_normal(sfloat_state_t *state, bool sign_z, int16_t exp_z, uint32_t sig_z) {
    unsigned char c = SFLOAT_CLZ(sig_z, 1);
    return SFLOAT_PACK_round(state, sign_z, exp_z - c, sig_z << c);
}

/*
 * Returns the result of adding the absolute values of `a' and `b'. The sign
 * `sign_z' is ignored if the result is a NaN.
 */
static sfloat_t sfloat_add_impl(sfloat_state_t *state, sfloat_t a, sfloat_t b, bool sign_z) {
    int16_t  exp_a = SFLOAT_EXTRACT_EXP(a);
    int16_t  exp_b = SFLOAT_EXTRACT_EXP(b);
    int16_t  exp_z = 0;
    int16_t  exp_d = exp_a - exp_b;
    uint32_t sig_a = SFLOAT_EXTRACT_FRAC(a) << 6;
    uint32_t sig_b = SFLOAT_EXTRACT_FRAC(b) << 6;
    uint32_t sig_z = 0;

    if (0 < exp_d) {
        if (exp_a == 0xFF)
            return sig_a ? sfloat_propagate_nan(state, a, b) : a;
        if (exp_b == 0)
            --exp_d;
        else
            sig_b |= 0x20000000;
        SFLOAT_SHIFT(32, sig_b, exp_d, &sig_b);
        exp_z = exp_a;
    } else if (exp_d < 0) {
        if (exp_b == 0xFF)
            return sig_b ? sfloat_propagate_nan(state, a, b) : SFLOAT_PACK(sign_z, 0xFF, 0);
        if (exp_a == 0)
            ++exp_d;
        else
            sig_a |= 0x20000000;
        SFLOAT_SHIFT(32, sig_a, -exp_d, &sig_a);
        exp_z = exp_b;
    } else {
        if (exp_a == 0xFF)
            return (sig_a | sig_b) ? sfloat_propagate_nan(state, a, b) : a;
        if (exp_a == 0)
            return SFLOAT_PACK(sign_z, 0, (sig_a + sig_b) >> 6);
        sig_z = 0x40000000 + sig_a + sig_b;
        exp_z = exp_a;
        goto end;
    }
    sig_a |= 0x20000000;
    sig_z = (sig_a + sig_b) << 1;
    --exp_z;
    if ((int32_t)sig_z < 0) {
        sig_z = sig_a + sig_b;
        ++exp_z;
    }
end:
    return SFLOAT_PACK_round(state, sign_z, exp_z, sig_z);
}

/*
 * Returns the result of subtracting the absolute values of `a' and `b'. If the
 * sign `sign_z' is one, the difference is negated before being returned. The
 * sign is ignored if the result is a NaN.
 */
static sfloat_t sfloat_sub_impl(sfloat_state_t *state, sfloat_t a, sfloat_t b, bool sign_z) {
    int16_t  exp_a = SFLOAT_EXTRACT_EXP(a);
    int16_t  exp_b = SFLOAT_EXTRACT_EXP(b);
    int16_t  exp_z = 0;
    int16_t  exp_d = exp_a - exp_b;
    uint32_t sig_a = SFLOAT_EXTRACT_FRAC(a) << 7;
    uint32_t sig_b = SFLOAT_EXTRACT_FRAC(b) << 7;
    uint32_t sig_z = 0;

    if (0 < exp_d) goto exp_greater_a;
    if (exp_d < 0) goto exp_greater_b;

    if (exp_a == 0xFF) {
        if (sig_a | sig_b)
            return sfloat_propagate_nan(state, a, b);
        SFLOAT_RAISE(state, SFLOAT_INVALID);
        return SFLOAT_NAN;
    }

    if (exp_a == 0)
        exp_a = exp_b = 1;

    if (sig_b < sig_a) goto greater_a;
    if (sig_a < sig_b) goto greater_b;

    return SFLOAT_PACK(state->roundingmode == SFLOAT_ROUND_DOWN, 0, 0);

exp_greater_b:
    if (exp_b == 0xFF)
        return (sig_b) ? sfloat_propagate_nan(state, a, b) : SFLOAT_PACK(sign_z ^ 1, 0xFF, 0);
    if (exp_a == 0)
        ++exp_d;
    else
        sig_a |= 0x40000000;
    SFLOAT_SHIFT(32, sig_a, -exp_d, &sig_a);
    sig_b |= 0x40000000;
greater_b:
    sig_z = sig_b - sig_a;
    exp_z = exp_b;
    sign_z ^= 1;
    goto end;

exp_greater_a:
    if (exp_a == 0xFF)
        return (sig_a) ? sfloat_propagate_nan(state, a, b) : a;
    if (exp_b == 0)
        --exp_d;
    else
        sig_b |= 0x40000000;
    SFLOAT_SHIFT(32, sig_b, exp_d, &sig_b);
    sig_a |= 0x40000000;
greater_a:
    sig_z = sig_a - sig_b;
    exp_z = exp_a;

end:
    --exp_z;
    return SFLOAT_PACK_normal(state, sign_z, exp_z, sig_z);
}

static GMQCC_INLINE sfloat_t sfloat_add(sfloat_state_t *state, sfloat_t a, sfloat_t b) {
    bool sign_a = SFLOAT_EXTRACT_SIGN(a);
    bool sign_b = SFLOAT_EXTRACT_SIGN(b);
    return (sign_a == sign_b) ? sfloat_add_impl(state, a, b, sign_a)
                              : sfloat_sub_impl(state, a, b, sign_a);
}

static GMQCC_INLINE sfloat_t sfloat_sub(sfloat_state_t *state, sfloat_t a, sfloat_t b) {
    bool sign_a = SFLOAT_EXTRACT_SIGN(a);
    bool sign_b = SFLOAT_EXTRACT_SIGN(b);
    return (sign_a == sign_b) ? sfloat_sub_impl(state, a, b, sign_a)
                              : sfloat_add_impl(state, a, b, sign_a);
}

static sfloat_t sfloat_mul(sfloat_state_t *state, sfloat_t a, sfloat_t b) {
    int16_t  exp_a   = SFLOAT_EXTRACT_EXP(a);
    int16_t  exp_b   = SFLOAT_EXTRACT_EXP(b);
    int16_t  exp_z   = 0;
    uint32_t sig_a   = SFLOAT_EXTRACT_FRAC(a);
    uint32_t sig_b   = SFLOAT_EXTRACT_FRAC(b);
    uint32_t sig_z   = 0;
    uint64_t sig_z64 = 0;
    bool     sign_a  = SFLOAT_EXTRACT_SIGN(a);
    bool     sign_b  = SFLOAT_EXTRACT_SIGN(b);
    bool     sign_z  = sign_a ^ sign_b;

    if (exp_a == 0xFF) {
        if (sig_a || ((exp_b == 0xFF) && sig_b))
            return sfloat_propagate_nan(state, a, b);
        if ((exp_b | sig_b) == 0) {
            SFLOAT_RAISE(state, SFLOAT_INVALID);
            return SFLOAT_NAN;
        }
        return SFLOAT_PACK(sign_z, 0xFF, 0);
    }
    if (exp_b == 0xFF) {
        if (sig_b)
            return sfloat_propagate_nan(state, a, b);
        if ((exp_a | sig_a) == 0) {
            SFLOAT_RAISE(state, SFLOAT_INVALID);
            return SFLOAT_NAN;
        }
        return SFLOAT_PACK(sign_z, 0xFF, 0);
    }
    if (exp_a == 0) {
        if (sig_a == 0)
            return SFLOAT_PACK(sign_z, 0, 0);
        SFLOAT_SUBNORMALIZE(sig_a, &exp_a, &sig_a);
    }
    if (exp_b == 0) {
        if (sig_b == 0)
            return SFLOAT_PACK(sign_z, 0, 0);
        SFLOAT_SUBNORMALIZE(sig_b, &exp_b, &sig_b);
    }
    exp_z = exp_a + exp_b - 0x7F;
    sig_a = (sig_a | 0x00800000) << 7;
    sig_b = (sig_b | 0x00800000) << 8;
    SFLOAT_SHIFT(64, ((uint64_t)sig_a) * sig_b, 32, &sig_z64);
    sig_z = sig_z64;
    if (0 <= (int32_t)(sig_z << 1)) {
        sig_z <<= 1;
        --exp_z;
    }
    return SFLOAT_PACK_round(state, sign_z, exp_z, sig_z);
}

static sfloat_t sfloat_div(sfloat_state_t *state, sfloat_t a, sfloat_t b) {
    int16_t  exp_a   = SFLOAT_EXTRACT_EXP(a);
    int16_t  exp_b   = SFLOAT_EXTRACT_EXP(b);
    int16_t  exp_z   = 0;
    uint32_t sig_a   = SFLOAT_EXTRACT_FRAC(a);
    uint32_t sig_b   = SFLOAT_EXTRACT_FRAC(b);
    uint32_t sig_z   = 0;
    bool     sign_a  = SFLOAT_EXTRACT_SIGN(a);
    bool     sign_b  = SFLOAT_EXTRACT_SIGN(b);
    bool     sign_z  = sign_a ^ sign_b;

    if (exp_a == 0xFF) {
        if (sig_a)
            return sfloat_propagate_nan(state, a, b);
        if (exp_b == 0xFF) {
            if (sig_b)
                return sfloat_propagate_nan(state, a, b);
            SFLOAT_RAISE(state, SFLOAT_INVALID);
            return SFLOAT_NAN;
        }
        return SFLOAT_PACK(sign_z, 0xFF, 0);
    }
    if (exp_b == 0xFF)
        return (sig_b) ? sfloat_propagate_nan(state, a, b) : SFLOAT_PACK(sign_z, 0, 0);
    if (exp_b == 0) {
        if (sig_b == 0) {
            if ((exp_a | sig_a) == 0) {
                SFLOAT_RAISE(state, SFLOAT_INVALID);
                return SFLOAT_NAN;
            }
            SFLOAT_RAISE(state, SFLOAT_DIVBYZERO);
            return SFLOAT_PACK(sign_z, 0xFF, 0);
        }
        SFLOAT_SUBNORMALIZE(sig_b, &exp_b, &sig_b);
    }
    if (exp_a == 0) {
        if (sig_a == 0)
            return SFLOAT_PACK(sign_z, 0, 0);
        SFLOAT_SUBNORMALIZE(sig_a, &exp_a, &sig_a);
    }
    exp_z = exp_a - exp_b + 0x7D;
    sig_a = (sig_a | 0x00800000) << 7;
    sig_b = (sig_b | 0x00800000) << 8;
    if (sig_b <= (sig_a + sig_a)) {
        sig_a >>= 1;
        ++exp_z;
    }
    sig_z = (((uint64_t)sig_a) << 32) / sig_b;
    if ((sig_z & 0x3F) == 0)
        sig_z |= ((uint64_t)sig_b * sig_z != ((uint64_t)sig_a) << 32);
    return SFLOAT_PACK_round(state, sign_z, exp_z, sig_z);
}

static sfloat_t sfloat_neg(sfloat_state_t *state, sfloat_t a) {
    sfloat_cast_t neg;
    neg.f = -1;
    return sfloat_mul(state, a, neg.s);
}

static GMQCC_INLINE void sfloat_check(lex_ctx_t ctx, sfloat_state_t *state, const char *vec) {
    /* Exception comes from vector component */
    if (vec) {
        if (state->exceptionflags & SFLOAT_DIVBYZERO)
            compile_error(ctx, "division by zero in `%s' component", vec);
        if (state->exceptionflags & SFLOAT_INVALID)
            compile_error(ctx, "undefined (inf) in `%s' component", vec);
        if (state->exceptionflags & SFLOAT_OVERFLOW)
            compile_error(ctx, "arithmetic overflow in `%s' component", vec);
        if (state->exceptionflags & SFLOAT_UNDERFLOW)
            compile_error(ctx, "arithmetic underflow in `%s' component", vec);
            return;
    }
    if (state->exceptionflags & SFLOAT_DIVBYZERO)
        compile_error(ctx, "division by zero");
    if (state->exceptionflags & SFLOAT_INVALID)
        compile_error(ctx, "undefined (inf)");
    if (state->exceptionflags & SFLOAT_OVERFLOW)
        compile_error(ctx, "arithmetic overflow");
    if (state->exceptionflags & SFLOAT_UNDERFLOW)
        compile_error(ctx, "arithmetic underflow");
}

static GMQCC_INLINE void sfloat_init(sfloat_state_t *state) {
    state->exceptionflags = SFLOAT_NOEXCEPT;
    state->roundingmode   = FOLD_ROUNDING;
    state->tiny           = FOLD_TINYNESS;
}

/*
 * There is two stages to constant folding in GMQCC: there is the parse
 * stage constant folding, where, with the help of the AST, operator
 * usages can be constant folded. Then there is the constant folding
 * in the IR for things like eliding if statements, can occur.
 *
 * This file is thus, split into two parts.
 */

#define isfloat(X)      (((ast_expression*)(X))->m_vtype == TYPE_FLOAT)
#define isvector(X)     (((ast_expression*)(X))->m_vtype == TYPE_VECTOR)
#define isstring(X)     (((ast_expression*)(X))->m_vtype == TYPE_STRING)
#define isarray(X)      (((ast_expression*)(X))->m_vtype == TYPE_ARRAY)
#define isfloats(X,Y)   (isfloat  (X) && isfloat (Y))

/*
 * Implementation of basic vector math for vec3_t, for trivial constant
 * folding.
 *
 * TODO: gcc/clang hinting for autovectorization
 */
enum vec3_comp_t {
    VEC_COMP_X = 1 << 0,
    VEC_COMP_Y = 1 << 1,
    VEC_COMP_Z = 1 << 2
};

struct vec3_soft_t {
    sfloat_cast_t x;
    sfloat_cast_t y;
    sfloat_cast_t z;
};

struct vec3_soft_state_t {
    vec3_comp_t faults;
    sfloat_state_t state[3];
};

static GMQCC_INLINE vec3_soft_t vec3_soft_convert(vec3_t vec) {
    vec3_soft_t soft;
    soft.x.f = vec.x;
    soft.y.f = vec.y;
    soft.z.f = vec.z;
    return soft;
}

static GMQCC_INLINE bool vec3_soft_exception(vec3_soft_state_t *vstate, size_t index) {
    sfloat_exceptionflags_t flags = vstate->state[index].exceptionflags;
    if (flags & SFLOAT_DIVBYZERO) return true;
    if (flags & SFLOAT_INVALID)   return true;
    if (flags & SFLOAT_OVERFLOW)  return true;
    if (flags & SFLOAT_UNDERFLOW) return true;
    return false;
}

static GMQCC_INLINE void vec3_soft_eval(vec3_soft_state_t *state,
                                        sfloat_t         (*callback)(sfloat_state_t *, sfloat_t, sfloat_t),
                                        vec3_t             a,
                                        vec3_t             b)
{
    vec3_soft_t sa = vec3_soft_convert(a);
    vec3_soft_t sb = vec3_soft_convert(b);
    callback(&state->state[0], sa.x.s, sb.x.s);
    if (vec3_soft_exception(state, 0)) state->faults = (vec3_comp_t)(state->faults | VEC_COMP_X);
    callback(&state->state[1], sa.y.s, sb.y.s);
    if (vec3_soft_exception(state, 1)) state->faults = (vec3_comp_t)(state->faults | VEC_COMP_Y);
    callback(&state->state[2], sa.z.s, sb.z.s);
    if (vec3_soft_exception(state, 2)) state->faults = (vec3_comp_t)(state->faults | VEC_COMP_Z);
}

static GMQCC_INLINE void vec3_check_except(vec3_t     a,
                                           vec3_t     b,
                                           lex_ctx_t  ctx,
                                           sfloat_t (*callback)(sfloat_state_t *, sfloat_t, sfloat_t))
{
    vec3_soft_state_t state;

    if (!OPTS_FLAG(ARITHMETIC_EXCEPTIONS))
        return;

    sfloat_init(&state.state[0]);
    sfloat_init(&state.state[1]);
    sfloat_init(&state.state[2]);

    vec3_soft_eval(&state, callback, a, b);
    if (state.faults & VEC_COMP_X) sfloat_check(ctx, &state.state[0], "x");
    if (state.faults & VEC_COMP_Y) sfloat_check(ctx, &state.state[1], "y");
    if (state.faults & VEC_COMP_Z) sfloat_check(ctx, &state.state[2], "z");
}

static GMQCC_INLINE vec3_t vec3_add(lex_ctx_t ctx, vec3_t a, vec3_t b) {
    vec3_t out;
    vec3_check_except(a, b, ctx, &sfloat_add);
    out.x = a.x + b.x;
    out.y = a.y + b.y;
    out.z = a.z + b.z;
    return out;
}

static GMQCC_INLINE vec3_t vec3_sub(lex_ctx_t ctx, vec3_t a, vec3_t b) {
    vec3_t out;
    vec3_check_except(a, b, ctx, &sfloat_sub);
    out.x = a.x - b.x;
    out.y = a.y - b.y;
    out.z = a.z - b.z;
    return out;
}

static GMQCC_INLINE vec3_t vec3_neg(lex_ctx_t ctx, vec3_t a) {
    vec3_t         out;
    sfloat_cast_t  v[3];
    sfloat_state_t s[3];

    if (!OPTS_FLAG(ARITHMETIC_EXCEPTIONS))
        goto end;

    v[0].f = a.x;
    v[1].f = a.y;
    v[2].f = a.z;

    sfloat_init(&s[0]);
    sfloat_init(&s[1]);
    sfloat_init(&s[2]);

    sfloat_neg(&s[0], v[0].s);
    sfloat_neg(&s[1], v[1].s);
    sfloat_neg(&s[2], v[2].s);

    sfloat_check(ctx, &s[0], nullptr);
    sfloat_check(ctx, &s[1], nullptr);
    sfloat_check(ctx, &s[2], nullptr);

end:
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
    out.x = -1-a.x;
    out.y = -1-a.y;
    out.z = -1-a.z;
    return out;
}

static GMQCC_INLINE qcfloat_t vec3_mulvv(lex_ctx_t ctx, vec3_t a, vec3_t b) {
    vec3_soft_t    sa;
    vec3_soft_t    sb;
    sfloat_state_t s[5];
    sfloat_t       r[5];

    if (!OPTS_FLAG(ARITHMETIC_EXCEPTIONS))
        goto end;

    sa = vec3_soft_convert(a);
    sb = vec3_soft_convert(b);

    sfloat_init(&s[0]);
    sfloat_init(&s[1]);
    sfloat_init(&s[2]);
    sfloat_init(&s[3]);
    sfloat_init(&s[4]);

    r[0] = sfloat_mul(&s[0], sa.x.s, sb.x.s);
    r[1] = sfloat_mul(&s[1], sa.y.s, sb.y.s);
    r[2] = sfloat_mul(&s[2], sa.z.s, sb.z.s);
    r[3] = sfloat_add(&s[3], r[0],   r[1]);
    r[4] = sfloat_add(&s[4], r[3],   r[2]);

    sfloat_check(ctx, &s[0], nullptr);
    sfloat_check(ctx, &s[1], nullptr);
    sfloat_check(ctx, &s[2], nullptr);
    sfloat_check(ctx, &s[3], nullptr);
    sfloat_check(ctx, &s[4], nullptr);

end:
    return (a.x * b.x + a.y * b.y + a.z * b.z);
}

static GMQCC_INLINE vec3_t vec3_mulvf(lex_ctx_t ctx, vec3_t a, qcfloat_t b) {
    vec3_t         out;
    vec3_soft_t    sa;
    sfloat_cast_t  sb;
    sfloat_state_t s[3];

    if (!OPTS_FLAG(ARITHMETIC_EXCEPTIONS))
        goto end;

    sa   = vec3_soft_convert(a);
    sb.f = b;
    sfloat_init(&s[0]);
    sfloat_init(&s[1]);
    sfloat_init(&s[2]);

    sfloat_mul(&s[0], sa.x.s, sb.s);
    sfloat_mul(&s[1], sa.y.s, sb.s);
    sfloat_mul(&s[2], sa.z.s, sb.s);

    sfloat_check(ctx, &s[0], "x");
    sfloat_check(ctx, &s[1], "y");
    sfloat_check(ctx, &s[2], "z");

end:
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
    return (a.x || a.y || a.z);
}

static GMQCC_INLINE vec3_t vec3_cross(lex_ctx_t ctx, vec3_t a, vec3_t b) {
    vec3_t         out;
    vec3_soft_t    sa;
    vec3_soft_t    sb;
    sfloat_t       r[9];
    sfloat_state_t s[9];

    if (!OPTS_FLAG(ARITHMETIC_EXCEPTIONS))
        goto end;

    sa = vec3_soft_convert(a);
    sb = vec3_soft_convert(b);

    sfloat_init(&s[0]);
    sfloat_init(&s[1]);
    sfloat_init(&s[2]);
    sfloat_init(&s[3]);
    sfloat_init(&s[4]);
    sfloat_init(&s[5]);
    sfloat_init(&s[6]);
    sfloat_init(&s[7]);
    sfloat_init(&s[8]);

    r[0] = sfloat_mul(&s[0], sa.y.s, sb.z.s);
    r[1] = sfloat_mul(&s[1], sa.z.s, sb.y.s);
    r[2] = sfloat_mul(&s[2], sa.z.s, sb.x.s);
    r[3] = sfloat_mul(&s[3], sa.x.s, sb.z.s);
    r[4] = sfloat_mul(&s[4], sa.x.s, sb.y.s);
    r[5] = sfloat_mul(&s[5], sa.y.s, sb.x.s);
    r[6] = sfloat_sub(&s[6], r[0],   r[1]);
    r[7] = sfloat_sub(&s[7], r[2],   r[3]);
    r[8] = sfloat_sub(&s[8], r[4],   r[5]);

    sfloat_check(ctx, &s[0], nullptr);
    sfloat_check(ctx, &s[1], nullptr);
    sfloat_check(ctx, &s[2], nullptr);
    sfloat_check(ctx, &s[3], nullptr);
    sfloat_check(ctx, &s[4], nullptr);
    sfloat_check(ctx, &s[5], nullptr);
    sfloat_check(ctx, &s[6], "x");
    sfloat_check(ctx, &s[7], "y");
    sfloat_check(ctx, &s[8], "z");

end:
    out.x = a.y * b.z - a.z * b.y;
    out.y = a.z * b.x - a.x * b.z;
    out.z = a.x * b.y - a.y * b.x;
    return out;
}

qcfloat_t fold::immvalue_float(ast_value *value) {
    return value->m_constval.vfloat;
}

vec3_t fold::immvalue_vector(ast_value *value) {
    return value->m_constval.vvec;
}

const char *fold::immvalue_string(ast_value *value) {
    return value->m_constval.vstring;
}

lex_ctx_t fold::ctx() {
    lex_ctx_t ctx;
    if (m_parser->lex)
        return parser_ctx(m_parser);
    memset(&ctx, 0, sizeof(ctx));
    return ctx;
}

bool fold::immediate_true(ast_value *v) {
    switch (v->m_vtype) {
        case TYPE_FLOAT:
            return !!v->m_constval.vfloat;
        case TYPE_INTEGER:
            return !!v->m_constval.vint;
        case TYPE_VECTOR:
            if (OPTS_FLAG(CORRECT_LOGIC))
                return vec3_pbool(v->m_constval.vvec);
            return !!(v->m_constval.vvec.x);
        case TYPE_STRING:
            if (!v->m_constval.vstring)
                return false;
            if (OPTS_FLAG(TRUE_EMPTY_STRINGS))
                return true;
            return !!v->m_constval.vstring[0];
        default:
            compile_error(ctx(), "internal error: fold_immediate_true on invalid type");
            break;
    }
    return !!v->m_constval.vfunc;
}

/* Handy macros to determine if an ast_value can be constant folded. */
#define fold_can_1(X)  \
    (ast_istype(((ast_expression*)(X)), ast_value) && (X)->m_hasvalue && ((X)->m_cvq == CV_CONST) && \
                ((ast_expression*)(X))->m_vtype != TYPE_FUNCTION)

#define fold_can_2(X, Y) (fold_can_1(X) && fold_can_1(Y))

fold::fold()
    : m_parser(nullptr)
{
}

fold::fold(parser_t *parser)
    : m_parser(parser)
{
    m_imm_string_untranslate = util_htnew(FOLD_STRING_UNTRANSLATE_HTSIZE);
    m_imm_string_dotranslate = util_htnew(FOLD_STRING_DOTRANSLATE_HTSIZE);

    constgen_float(0.0f, false);
    constgen_float(1.0f, false);
    constgen_float(-1.0f, false);
    constgen_float(2.0f, false);

    constgen_vector(vec3_create(0.0f, 0.0f, 0.0f));
    constgen_vector(vec3_create(-1.0f, -1.0f, -1.0f));
}

bool fold::generate(ir_builder *ir) {
    // generate globals for immediate folded values
    ast_value *cur;
    for (auto &it : m_imm_float)
        if (!ast_global_codegen((cur = it), ir, false)) goto err;
    for (auto &it : m_imm_vector)
        if (!ast_global_codegen((cur = it), ir, false)) goto err;
    for (auto &it : m_imm_string)
        if (!ast_global_codegen((cur = it), ir, false)) goto err;
    return true;
err:
    con_out("failed to generate global %s\n", cur->m_name);
    delete ir;
    return false;
}

fold::~fold() {
// TODO: parser lifetime so this is called when it should be
#if 0
    for (auto &it : m_imm_float) ast_delete(it);
    for (auto &it : m_imm_vector) ast_delete(it);
    for (auto &it : m_imm_string) ast_delete(it);

    util_htdel(m_imm_string_untranslate);
    util_htdel(m_imm_string_dotranslate);
#endif
}

ast_expression *fold::constgen_float(qcfloat_t value, bool inexact) {
    for (auto &it : m_imm_float)
        if (!memcmp(&it->m_constval.vfloat, &value, sizeof(qcfloat_t)))
            return (ast_expression*)it;

    ast_value *out  = ast_value_new(ctx(), "#IMMEDIATE", TYPE_FLOAT);
    out->m_cvq = CV_CONST;
    out->m_hasvalue = true;
    out->m_inexact = inexact;
    out->m_constval.vfloat = value;

    m_imm_float.push_back(out);

    return (ast_expression*)out;
}

ast_expression *fold::constgen_vector(vec3_t value) {
    for (auto &it : m_imm_vector)
        if (vec3_cmp(it->m_constval.vvec, value))
            return (ast_expression*)it;

    ast_value *out = ast_value_new(ctx(), "#IMMEDIATE", TYPE_VECTOR);
    out->m_cvq = CV_CONST;
    out->m_hasvalue = true;
    out->m_constval.vvec = value;

    m_imm_vector.push_back(out);

    return (ast_expression*)out;
}

ast_expression *fold::constgen_string(const char *str, bool translate) {
    hash_table_t *table = translate ? m_imm_string_untranslate : m_imm_string_dotranslate;
    ast_value *out = nullptr;
    size_t hash = util_hthash(table, str);

    if ((out = (ast_value*)util_htgeth(table, str, hash)))
        return (ast_expression*)out;

    if (translate) {
        char name[32];
        util_snprintf(name, sizeof(name), "dotranslate_%zu", m_parser->translated++);
        out = ast_value_new(ctx(), name, TYPE_STRING);
        out->m_flags |= AST_FLAG_INCLUDE_DEF; /* def needs to be included for translatables */
    } else {
        out = ast_value_new(ctx(), "#IMMEDIATE", TYPE_STRING);
    }

    out->m_cvq = CV_CONST;
    out->m_hasvalue = true;
    out->m_isimm = true;
    out->m_constval.vstring = parser_strdup(str);

    m_imm_string.push_back(out);
    util_htseth(table, str, hash, out);

    return (ast_expression*)out;
}

typedef union {
    void (*callback)(void);
    sfloat_t (*binary)(sfloat_state_t *, sfloat_t, sfloat_t);
    sfloat_t (*unary)(sfloat_state_t *, sfloat_t);
} float_check_callback_t;

bool fold::check_except_float_impl(void (*callback)(void), ast_value *a, ast_value *b) {
    float_check_callback_t call;
    sfloat_state_t s;
    sfloat_cast_t ca;

    if (!OPTS_FLAG(ARITHMETIC_EXCEPTIONS) && !OPTS_WARN(WARN_INEXACT_COMPARES))
        return false;

    call.callback = callback;
    sfloat_init(&s);
    ca.f = immvalue_float(a);
    if (b) {
        sfloat_cast_t cb;
        cb.f = immvalue_float(b);
        call.binary(&s, ca.s, cb.s);
    } else {
        call.unary(&s, ca.s);
    }

    if (s.exceptionflags == 0)
        return false;

    if (!OPTS_FLAG(ARITHMETIC_EXCEPTIONS))
        goto inexact_possible;

    sfloat_check(ctx(), &s, nullptr);

inexact_possible:
    return s.exceptionflags & SFLOAT_INEXACT;
}

#define check_except_float(CALLBACK, A, B) \
    check_except_float_impl(((void (*)(void))(CALLBACK)), (A), (B))

bool fold::check_inexact_float(ast_value *a, ast_value *b) {
    if (!OPTS_WARN(WARN_INEXACT_COMPARES))
        return false;
    if (!a->m_inexact && !b->m_inexact)
        return false;
    return compile_warning(ctx(), WARN_INEXACT_COMPARES, "inexact value in comparison");
}

ast_expression *fold::op_mul_vec(vec3_t vec, ast_value *sel, const char *set) {
    qcfloat_t x = (&vec.x)[set[0]-'x'];
    qcfloat_t y = (&vec.x)[set[1]-'x'];
    qcfloat_t z = (&vec.x)[set[2]-'x'];
    if (!y && !z) {
        ast_expression *out;
        ++opts_optimizationcount[OPTIM_VECTOR_COMPONENTS];
        out = (ast_expression*)ast_member_new(ctx(), (ast_expression*)sel, set[0]-'x', nullptr);
        out->m_keep_node = false;
        ((ast_member*)out)->m_rvalue = true;
        if (x != -1.0f)
            return (ast_expression*)ast_binary_new(ctx(), INSTR_MUL_F, constgen_float(x, false), out);
    }
    return nullptr;
}


ast_expression *fold::op_neg(ast_value *a) {
    if (isfloat(a)) {
        if (fold_can_1(a)) {
            /* Negation can produce inexact as well */
            bool inexact = check_except_float(&sfloat_neg, a, nullptr);
            return constgen_float(-immvalue_float(a), inexact);
        }
    } else if (isvector(a)) {
        if (fold_can_1(a))
            return constgen_vector(vec3_neg(ctx(), immvalue_vector(a)));
    }
    return nullptr;
}

ast_expression *fold::op_not(ast_value *a) {
    if (isfloat(a)) {
        if (fold_can_1(a))
            return constgen_float(!immvalue_float(a), false);
    } else if (isvector(a)) {
        if (fold_can_1(a))
            return constgen_float(vec3_notf(immvalue_vector(a)), false);
    } else if (isstring(a)) {
        if (fold_can_1(a)) {
            if (OPTS_FLAG(TRUE_EMPTY_STRINGS))
                return constgen_float(!immvalue_string(a), false);
            else
                return constgen_float(!immvalue_string(a) || !*immvalue_string(a), false);
        }
    }
    return nullptr;
}

ast_expression *fold::op_add(ast_value *a, ast_value *b) {
    if (isfloat(a)) {
        if (fold_can_2(a, b)) {
            bool inexact = check_except_float(&sfloat_add, a, b);
            return constgen_float(immvalue_float(a) + immvalue_float(b), inexact);
        }
    } else if (isvector(a)) {
        if (fold_can_2(a, b))
            return constgen_vector(vec3_add(ctx(),
                                                       immvalue_vector(a),
                                                       immvalue_vector(b)));
    }
    return nullptr;
}

ast_expression *fold::op_sub(ast_value *a, ast_value *b) {
    if (isfloat(a)) {
        if (fold_can_2(a, b)) {
            bool inexact = check_except_float(&sfloat_sub, a, b);
            return constgen_float(immvalue_float(a) - immvalue_float(b), inexact);
        }
    } else if (isvector(a)) {
        if (fold_can_2(a, b))
            return constgen_vector(vec3_sub(ctx(),
                                                       immvalue_vector(a),
                                                       immvalue_vector(b)));
    }
    return nullptr;
}

ast_expression *fold::op_mul(ast_value *a, ast_value *b) {
    if (isfloat(a)) {
        if (isvector(b)) {
            if (fold_can_2(a, b))
                return constgen_vector(vec3_mulvf(ctx(), immvalue_vector(b), immvalue_float(a)));
        } else {
            if (fold_can_2(a, b)) {
                bool inexact = check_except_float(&sfloat_mul, a, b);
                return constgen_float(immvalue_float(a) * immvalue_float(b), inexact);
            }
        }
    } else if (isvector(a)) {
        if (isfloat(b)) {
            if (fold_can_2(a, b))
                return constgen_vector(vec3_mulvf(ctx(), immvalue_vector(a), immvalue_float(b)));
        } else {
            if (fold_can_2(a, b)) {
                return constgen_float(vec3_mulvv(ctx(), immvalue_vector(a), immvalue_vector(b)), false);
            } else if (OPTS_OPTIMIZATION(OPTIM_VECTOR_COMPONENTS) && fold_can_1(a)) {
                ast_expression *out;
                if ((out = op_mul_vec(immvalue_vector(a), b, "xyz"))) return out;
                if ((out = op_mul_vec(immvalue_vector(a), b, "yxz"))) return out;
                if ((out = op_mul_vec(immvalue_vector(a), b, "zxy"))) return out;
            } else if (OPTS_OPTIMIZATION(OPTIM_VECTOR_COMPONENTS) && fold_can_1(b)) {
                ast_expression *out;
                if ((out = op_mul_vec(immvalue_vector(b), a, "xyz"))) return out;
                if ((out = op_mul_vec(immvalue_vector(b), a, "yxz"))) return out;
                if ((out = op_mul_vec(immvalue_vector(b), a, "zxy"))) return out;
            }
        }
    }
    return nullptr;
}

ast_expression *fold::op_div(ast_value *a, ast_value *b) {
    if (isfloat(a)) {
        if (fold_can_2(a, b)) {
            bool inexact = check_except_float(&sfloat_div, a, b);
            return constgen_float(immvalue_float(a) / immvalue_float(b), inexact);
        } else if (fold_can_1(b)) {
            return (ast_expression*)ast_binary_new(
                ctx(),
                INSTR_MUL_F,
                (ast_expression*)a,
                constgen_float(1.0f / immvalue_float(b), false)
            );
        }
    } else if (isvector(a)) {
        if (fold_can_2(a, b)) {
            return constgen_vector(vec3_mulvf(ctx(), immvalue_vector(a), 1.0f / immvalue_float(b)));
        } else {
            return (ast_expression*)ast_binary_new(
                ctx(),
                INSTR_MUL_VF,
                (ast_expression*)a,
                (fold_can_1(b))
                    ? (ast_expression*)constgen_float(1.0f / immvalue_float(b), false)
                    : (ast_expression*)ast_binary_new(
                                            ctx(),
                                            INSTR_DIV_F,
                                            (ast_expression*)m_imm_float[1],
                                            (ast_expression*)b
                    )
            );
        }
    }
    return nullptr;
}

ast_expression *fold::op_mod(ast_value *a, ast_value *b) {
    return (fold_can_2(a, b))
                ? constgen_float(fmod(immvalue_float(a), immvalue_float(b)), false)
                : nullptr;
}

ast_expression *fold::op_bor(ast_value *a, ast_value *b) {
    if (isfloat(a)) {
        if (fold_can_2(a, b))
            return constgen_float((qcfloat_t)(((qcint_t)immvalue_float(a)) | ((qcint_t)immvalue_float(b))), false);
    } else {
        if (isvector(b)) {
            if (fold_can_2(a, b))
                return constgen_vector(vec3_or(immvalue_vector(a), immvalue_vector(b)));
        } else {
            if (fold_can_2(a, b))
                return constgen_vector(vec3_orvf(immvalue_vector(a), immvalue_float(b)));
        }
    }
    return nullptr;
}

ast_expression *fold::op_band(ast_value *a, ast_value *b) {
    if (isfloat(a)) {
        if (fold_can_2(a, b))
            return constgen_float((qcfloat_t)(((qcint_t)immvalue_float(a)) & ((qcint_t)immvalue_float(b))), false);
    } else {
        if (isvector(b)) {
            if (fold_can_2(a, b))
                return constgen_vector(vec3_and(immvalue_vector(a), immvalue_vector(b)));
        } else {
            if (fold_can_2(a, b))
                return constgen_vector(vec3_andvf(immvalue_vector(a), immvalue_float(b)));
        }
    }
    return nullptr;
}

ast_expression *fold::op_xor(ast_value *a, ast_value *b) {
    if (isfloat(a)) {
        if (fold_can_2(a, b))
            return constgen_float((qcfloat_t)(((qcint_t)immvalue_float(a)) ^ ((qcint_t)immvalue_float(b))), false);
    } else {
        if (fold_can_2(a, b)) {
            if (isvector(b))
                return constgen_vector(vec3_xor(immvalue_vector(a), immvalue_vector(b)));
            else
                return constgen_vector(vec3_xorvf(immvalue_vector(a), immvalue_float(b)));
        }
    }
    return nullptr;
}

ast_expression *fold::op_lshift(ast_value *a, ast_value *b) {
    if (fold_can_2(a, b) && isfloats(a, b))
        return constgen_float((qcfloat_t)floorf(immvalue_float(a) * powf(2.0f, immvalue_float(b))), false);
    return nullptr;
}

ast_expression *fold::op_rshift(ast_value *a, ast_value *b) {
    if (fold_can_2(a, b) && isfloats(a, b))
        return constgen_float((qcfloat_t)floorf(immvalue_float(a) / powf(2.0f, immvalue_float(b))), false);
    return nullptr;
}

ast_expression *fold::op_andor(ast_value *a, ast_value *b, float expr) {
    if (fold_can_2(a, b)) {
        if (OPTS_FLAG(PERL_LOGIC)) {
            if (expr)
                return immediate_true(a) ? (ast_expression*)a : (ast_expression*)b;
            else
                return immediate_true(a) ? (ast_expression*)b : (ast_expression*)a;
        } else {
            return constgen_float(
                ((expr) ? (immediate_true(a) || immediate_true(b))
                        : (immediate_true(a) && immediate_true(b)))
                            ? 1
                            : 0,
                false
            );
        }
    }
    return nullptr;
}

ast_expression *fold::op_tern(ast_value *a, ast_value *b, ast_value *c) {
    if (fold_can_1(a)) {
        return immediate_true(a)
                    ? (ast_expression*)b
                    : (ast_expression*)c;
    }
    return nullptr;
}

ast_expression *fold::op_exp(ast_value *a, ast_value *b) {
    if (fold_can_2(a, b))
        return constgen_float((qcfloat_t)powf(immvalue_float(a), immvalue_float(b)), false);
    return nullptr;
}

ast_expression *fold::op_lteqgt(ast_value *a, ast_value *b) {
    if (fold_can_2(a,b)) {
        check_inexact_float(a, b);
        if (immvalue_float(a) <  immvalue_float(b)) return (ast_expression*)m_imm_float[2];
        if (immvalue_float(a) == immvalue_float(b)) return (ast_expression*)m_imm_float[0];
        if (immvalue_float(a) >  immvalue_float(b)) return (ast_expression*)m_imm_float[1];
    }
    return nullptr;
}

ast_expression *fold::op_ltgt(ast_value *a, ast_value *b, bool lt) {
    if (fold_can_2(a, b)) {
        check_inexact_float(a, b);
        return (lt) ? (ast_expression*)m_imm_float[!!(immvalue_float(a) < immvalue_float(b))]
                    : (ast_expression*)m_imm_float[!!(immvalue_float(a) > immvalue_float(b))];
    }
    return nullptr;
}

ast_expression *fold::op_cmp(ast_value *a, ast_value *b, bool ne) {
    if (fold_can_2(a, b)) {
        if (isfloat(a) && isfloat(b)) {
            float la = immvalue_float(a);
            float lb = immvalue_float(b);
            check_inexact_float(a, b);
            return (ast_expression*)m_imm_float[ne ? la != lb : la == lb];
        } else if (isvector(a) && isvector(b)) {
            vec3_t la = immvalue_vector(a);
            vec3_t lb = immvalue_vector(b);
            bool compare = vec3_cmp(la, lb);
            return (ast_expression*)m_imm_float[ne ? !compare : compare];
        } else if (isstring(a) && isstring(b)) {
            bool compare = !strcmp(immvalue_string(a), immvalue_string(b));
            return (ast_expression*)m_imm_float[ne ? !compare : compare];
        }
    }
    return nullptr;
}

ast_expression *fold::op_bnot(ast_value *a) {
    if (isfloat(a)) {
        if (fold_can_1(a))
            return constgen_float(-1-immvalue_float(a), false);
    } else {
        if (isvector(a)) {
            if (fold_can_1(a))
                return constgen_vector(vec3_not(immvalue_vector(a)));
        }
    }
    return nullptr;
}

ast_expression *fold::op_cross(ast_value *a, ast_value *b) {
    if (fold_can_2(a, b))
        return constgen_vector(vec3_cross(ctx(),
                                          immvalue_vector(a),
                                          immvalue_vector(b)));
    return nullptr;
}

ast_expression *fold::op_length(ast_value *a) {
    if (fold_can_1(a) && isstring(a))
        return constgen_float(strlen(immvalue_string(a)), false);
    if (isarray(a))
        return constgen_float(a->m_initlist.size(), false);
    return nullptr;
}

ast_expression *fold::op(const oper_info *info, ast_expression **opexprs) {
    ast_value *a = (ast_value*)opexprs[0];
    ast_value *b = (ast_value*)opexprs[1];
    ast_value *c = (ast_value*)opexprs[2];
    ast_expression *e = nullptr;

    /* can a fold operation be applied to this operator usage? */
    if (!info->folds)
        return nullptr;

    switch(info->operands) {
        case 3: if(!c) return nullptr;
        case 2: if(!b) return nullptr;
        case 1:
        if(!a) {
            compile_error(ctx(), "internal error: fold_op no operands to fold\n");
            return nullptr;
        }
    }

    #define fold_op_case(ARGS, ARGS_OPID, OP, ARGS_FOLD)    \
        case opid##ARGS ARGS_OPID:                          \
            if ((e = op_##OP ARGS_FOLD)) {                  \
                ++opts_optimizationcount[OPTIM_CONST_FOLD]; \
            }                                               \
            return e

    switch(info->id) {
        fold_op_case(2, ('-', 'P'),      neg,    (a));
        fold_op_case(2, ('!', 'P'),      not,    (a));
        fold_op_case(1, ('+'),           add,    (a, b));
        fold_op_case(1, ('-'),           sub,    (a, b));
        fold_op_case(1, ('*'),           mul,    (a, b));
        fold_op_case(1, ('/'),           div,    (a, b));
        fold_op_case(1, ('%'),           mod,    (a, b));
        fold_op_case(1, ('|'),           bor,    (a, b));
        fold_op_case(1, ('&'),           band,   (a, b));
        fold_op_case(1, ('^'),           xor,    (a, b));
        fold_op_case(1, ('<'),           ltgt,   (a, b, true));
        fold_op_case(1, ('>'),           ltgt,   (a, b, false));
        fold_op_case(2, ('<', '<'),      lshift, (a, b));
        fold_op_case(2, ('>', '>'),      rshift, (a, b));
        fold_op_case(2, ('|', '|'),      andor,  (a, b, true));
        fold_op_case(2, ('&', '&'),      andor,  (a, b, false));
        fold_op_case(2, ('?', ':'),      tern,   (a, b, c));
        fold_op_case(2, ('*', '*'),      exp,    (a, b));
        fold_op_case(3, ('<','=','>'),   lteqgt, (a, b));
        fold_op_case(2, ('!', '='),      cmp,    (a, b, true));
        fold_op_case(2, ('=', '='),      cmp,    (a, b, false));
        fold_op_case(2, ('~', 'P'),      bnot,   (a));
        fold_op_case(2, ('>', '<'),      cross,  (a, b));
        fold_op_case(3, ('l', 'e', 'n'), length, (a));
    }
    #undef fold_op_case
    compile_error(ctx(), "internal error: attempted to constant-fold for unsupported operator");
    return nullptr;
}

/*
 * Constant folding for compiler intrinsics, similar approach to operator
 * folding, primarily: individual functions for each intrinsics to fold,
 * and a generic selection function.
 */
ast_expression *fold::intrinsic_isfinite(ast_value *a) {
    return constgen_float(isfinite(immvalue_float(a)), false);
}
ast_expression *fold::intrinsic_isinf(ast_value *a) {
    return constgen_float(isinf(immvalue_float(a)), false);
}
ast_expression *fold::intrinsic_isnan(ast_value *a) {
    return constgen_float(isnan(immvalue_float(a)), false);
}
ast_expression *fold::intrinsic_isnormal(ast_value *a) {
    return constgen_float(isnormal(immvalue_float(a)), false);
}
ast_expression *fold::intrinsic_signbit(ast_value *a) {
    return constgen_float(signbit(immvalue_float(a)), false);
}
ast_expression *fold::intrinsic_acosh(ast_value *a) {
    return constgen_float(acoshf(immvalue_float(a)), false);
}
ast_expression *fold::intrinsic_asinh(ast_value *a) {
    return constgen_float(asinhf(immvalue_float(a)), false);
}
ast_expression *fold::intrinsic_atanh(ast_value *a) {
    return constgen_float((float)atanh(immvalue_float(a)), false);
}
ast_expression *fold::intrinsic_exp(ast_value *a) {
    return constgen_float(expf(immvalue_float(a)), false);
}
ast_expression *fold::intrinsic_exp2(ast_value *a) {
    return constgen_float(exp2f(immvalue_float(a)), false);
}
ast_expression *fold::intrinsic_expm1(ast_value *a) {
    return constgen_float(expm1f(immvalue_float(a)), false);
}
ast_expression *fold::intrinsic_mod(ast_value *lhs, ast_value *rhs) {
    return constgen_float(fmodf(immvalue_float(lhs), immvalue_float(rhs)), false);
}
ast_expression *fold::intrinsic_pow(ast_value *lhs, ast_value *rhs) {
    return constgen_float(powf(immvalue_float(lhs), immvalue_float(rhs)), false);
}
ast_expression *fold::intrinsic_fabs(ast_value *a) {
    return constgen_float(fabsf(immvalue_float(a)), false);
}

ast_expression *fold::intrinsic(const char *intrinsic, ast_expression **arg) {
    ast_expression *ret = nullptr;
    ast_value *a = (ast_value*)arg[0];
    ast_value *b = (ast_value*)arg[1];

    if (!strcmp(intrinsic, "isfinite")) ret = intrinsic_isfinite(a);
    if (!strcmp(intrinsic, "isinf"))    ret = intrinsic_isinf(a);
    if (!strcmp(intrinsic, "isnan"))    ret = intrinsic_isnan(a);
    if (!strcmp(intrinsic, "isnormal")) ret = intrinsic_isnormal(a);
    if (!strcmp(intrinsic, "signbit"))  ret = intrinsic_signbit(a);
    if (!strcmp(intrinsic, "acosh"))    ret = intrinsic_acosh(a);
    if (!strcmp(intrinsic, "asinh"))    ret = intrinsic_asinh(a);
    if (!strcmp(intrinsic, "atanh"))    ret = intrinsic_atanh(a);
    if (!strcmp(intrinsic, "exp"))      ret = intrinsic_exp(a);
    if (!strcmp(intrinsic, "exp2"))     ret = intrinsic_exp2(a);
    if (!strcmp(intrinsic, "expm1"))    ret = intrinsic_expm1(a);
    if (!strcmp(intrinsic, "mod"))      ret = intrinsic_mod(a, b);
    if (!strcmp(intrinsic, "pow"))      ret = intrinsic_pow(a, b);
    if (!strcmp(intrinsic, "fabs"))     ret = intrinsic_fabs(a);

    if (ret)
        ++opts_optimizationcount[OPTIM_CONST_FOLD];

    return ret;
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
#undef fold__immvalue_float
#undef fold__immvalue_string
#undef fold__immvalue_vector
#undef fold_can_1
#undef fold_can_2

#define isfloat(X)              ((X)->m_vtype == TYPE_FLOAT)
/*#define isstring(X)             ((X)->m_vtype == TYPE_STRING)*/
/*#define isvector(X)             ((X)->m_vtype == TYPE_VECTOR)*/
#define fold_can_1(X)           ((X)->m_hasvalue && (X)->m_cvq == CV_CONST)
/*#define fold_can_2(X,Y)         (fold_can_1(X) && fold_can_1(Y))*/

qcfloat_t fold::immvalue_float(ir_value *value) {
    return value->m_constval.vfloat;
}

vec3_t fold::immvalue_vector(ir_value *value) {
    return value->m_constval.vvec;
}

ast_expression *fold::superfluous(ast_expression *left, ast_expression *right, int op) {
    ast_expression *swapped = nullptr; /* using this as bool */
    ast_value *load;

    if (!ast_istype(right, ast_value) || !fold_can_1((load = (ast_value*)right))) {
        swapped = left;
        left    = right;
        right   = swapped;
    }

    if (!ast_istype(right, ast_value) || !fold_can_1((load = (ast_value*)right)))
        return nullptr;

    switch (op) {
        case INSTR_DIV_F:
            if (swapped)
                return nullptr;
        case INSTR_MUL_F:
            if (immvalue_float(load) == 1.0f) {
                ++opts_optimizationcount[OPTIM_PEEPHOLE];
                ast_unref(right);
                return left;
            }
            break;


        case INSTR_SUB_F:
            if (swapped)
                return nullptr;
        case INSTR_ADD_F:
            if (immvalue_float(load) == 0.0f) {
                ++opts_optimizationcount[OPTIM_PEEPHOLE];
                ast_unref(right);
                return left;
            }
            break;

        case INSTR_MUL_V:
            if (vec3_cmp(immvalue_vector(load), vec3_create(1, 1, 1))) {
                ++opts_optimizationcount[OPTIM_PEEPHOLE];
                ast_unref(right);
                return left;
            }
            break;

        case INSTR_SUB_V:
            if (swapped)
                return nullptr;
        case INSTR_ADD_V:
            if (vec3_cmp(immvalue_vector(load), vec3_create(0, 0, 0))) {
                ++opts_optimizationcount[OPTIM_PEEPHOLE];
                ast_unref(right);
                return left;
            }
            break;
    }

    return nullptr;
}

ast_expression *fold::binary(lex_ctx_t ctx, int op, ast_expression *left, ast_expression *right) {
    ast_expression *ret = superfluous(left, right, op);
    if (ret)
        return ret;
    return (ast_expression*)ast_binary_new(ctx, op, left, right);
}

int fold::cond(ir_value *condval, ast_function *func, ast_ifthen *branch) {
    if (isfloat(condval) && fold_can_1(condval) && OPTS_OPTIMIZATION(OPTIM_CONST_FOLD_DCE)) {
        ast_expression_codegen *cgen;
        ir_block               *elide;
        ir_value               *dummy;
        bool                    istrue  = (immvalue_float(condval) != 0.0f && branch->m_on_true);
        bool                    isfalse = (immvalue_float(condval) == 0.0f && branch->m_on_false);
        ast_expression         *path    = (istrue)  ? branch->m_on_true  :
                                          (isfalse) ? branch->m_on_false : nullptr;
        if (!path) {
            /*
             * no path to take implies that the evaluation is if(0) and there
             * is no else block. so eliminate all the code.
             */
            ++opts_optimizationcount[OPTIM_CONST_FOLD_DCE];
            return true;
        }

        if (!(elide = ir_function_create_block(branch->m_context, func->m_ir_func, ast_function_label(func, ((istrue) ? "ontrue" : "onfalse")))))
            return false;
        if (!(*(cgen = path->m_codegen))((ast_expression*)path, func, false, &dummy))
            return false;
        if (!ir_block_create_jump(func->m_curblock, branch->m_context, elide))
            return false;
        /*
         * now the branch has been eliminated and the correct block for the constant evaluation
         * is expanded into the current block for the function.
         */
        func->m_curblock = elide;
        ++opts_optimizationcount[OPTIM_CONST_FOLD_DCE];
        return true;
    }
    return -1; /* nothing done */
}

int fold::cond_ternary(ir_value *condval, ast_function *func, ast_ternary *branch) {
    return cond(condval, func, (ast_ifthen*)branch);
}

int fold::cond_ifthen(ir_value *condval, ast_function *func, ast_ifthen *branch) {
    return cond(condval, func, branch);
}
