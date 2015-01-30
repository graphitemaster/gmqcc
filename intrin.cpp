#include <string.h>

#include "ast.h"
#include "fold.h"
#include "parser.h"

lex_ctx_t intrin::ctx() const {
    return parser_ctx(m_parser);
}

ast_function *intrin::value(ast_value **out, const char *name, qc_type vtype) {
    ast_value *value = nullptr;
    ast_function *func  = nullptr;
    char buffer[1024];
    char stype [1024];

    util_snprintf(buffer, sizeof(buffer), "__builtin_%s", name);
    util_snprintf(stype,  sizeof(stype),   "<%s>",        type_name[vtype]);

    value = new ast_value(ctx(), buffer, TYPE_FUNCTION);
    value->m_intrinsic = true;
    value->m_next = new ast_value(ctx(), stype, vtype);
    func = ast_function::make(ctx(), buffer, value);
    value->m_flags |= AST_FLAG_ERASEABLE;

    *out = value;
    return func;
}

void intrin::reg(ast_value *const value, ast_function *const func) {
    m_parser->functions.push_back(func);
    m_parser->globals.push_back(value);
}

#define QC_POW_EPSILON 0.00001f

ast_expression *intrin::nullfunc() {
    ast_value *val = nullptr;
    ast_function *func = value(&val, nullptr, TYPE_VOID);
    reg(val, func);
    return val;
}

ast_expression *intrin::isfinite_() {
    /*
     * float isfinite(float x) {
     *     return !(isnan(x) || isinf(x));
     * }
     */
    ast_value    *val     = nullptr;
    ast_value    *x         = new ast_value(ctx(), "x", TYPE_FLOAT);
    ast_function *func      = value(&val, "isfinite", TYPE_FLOAT);
    ast_call     *callisnan = ast_call::make(ctx(), func_self("isnan", "isfinite"));
    ast_call     *callisinf = ast_call::make(ctx(), func_self("isinf", "isfinite"));
    ast_block    *block     = new ast_block(ctx());

    /* float x; */
    val->m_type_params.emplace_back(x);

    /* <callisnan> = isnan(x); */
    callisnan->m_params.push_back(x);

    /* <callisinf> = isinf(x); */
    callisinf->m_params.push_back(x);

    /* return (!<callisnan> || <callisinf>); */
    block->m_exprs.push_back(
        new ast_return(
            ctx(),
            ast_unary::make(
                ctx(),
                INSTR_NOT_F,
                new ast_binary(
                    ctx(),
                    INSTR_OR,
                    callisnan,
                    callisinf
                )
            )
        )
    );

    func->m_blocks.emplace_back(block);
    reg(val, func);

    return val;;
}

ast_expression *intrin::isinf_() {
    /*
     * float isinf(float x) {
     *     return (x != 0.0) && (x + x == x);
     * }
     */
    ast_value *val = nullptr;
    ast_value *x = new ast_value(ctx(), "x", TYPE_FLOAT);
    ast_block *body = new ast_block(ctx());
    ast_function *func = value(&val, "isinf", TYPE_FLOAT);

    body->m_exprs.push_back(
        new ast_return(
            ctx(),
            new ast_binary(
                ctx(),
                INSTR_AND,
                new ast_binary(
                    ctx(),
                    INSTR_NE_F,
                    x,
                    m_fold->m_imm_float[0]
                ),
                new ast_binary(
                    ctx(),
                    INSTR_EQ_F,
                    new ast_binary(
                        ctx(),
                        INSTR_ADD_F,
                        x,
                        x
                    ),
                    x
                )
            )
        )
    );

    val->m_type_params.emplace_back(x);
    func->m_blocks.emplace_back(body);

    reg(val, func);

    return val;
}

ast_expression *intrin::isnan_() {
    /*
     * float isnan(float x) {
     *   float local;
     *   local = x;
     *
     *   return (x != local);
     * }
     */
    ast_value *val = nullptr;
    ast_value *arg1 = new ast_value(ctx(), "x",TYPE_FLOAT);
    ast_value *local = new ast_value(ctx(), "local", TYPE_FLOAT);
    ast_block *body  = new ast_block(ctx());
    ast_function *func = value(&val, "isnan", TYPE_FLOAT);

    body->m_locals.push_back(local);
    body->m_exprs.push_back(
        new ast_store(
            ctx(),
            INSTR_STORE_F,
            local,
            arg1
        )
    );

    body->m_exprs.push_back(
        new ast_return(
            ctx(),
            new ast_binary(
                ctx(),
                INSTR_NE_F,
                arg1,
                local
            )
        )
    );

    val->m_type_params.emplace_back(arg1);
    func->m_blocks.emplace_back(body);

    reg(val, func);

    return val;
}

ast_expression *intrin::isnormal_() {
    /*
     * float isnormal(float x) {
     *     return isfinite(x);
     * }
     */
    ast_value *val = nullptr;
    ast_call *callisfinite = ast_call::make(ctx(), func_self("isfinite", "isnormal"));
    ast_value *x = new ast_value(ctx(), "x", TYPE_FLOAT);
    ast_block *body = new ast_block(ctx());
    ast_function *func = value(&val, "isnormal", TYPE_FLOAT);

    val->m_type_params.emplace_back(x);
    callisfinite->m_params.push_back(x);

    /* return <callisfinite> */
    body->m_exprs.push_back(
        new ast_return(
            ctx(),
            callisfinite
        )
    );

    func->m_blocks.emplace_back(body);
    reg(val, func);
    return val;
}

ast_expression *intrin::signbit_() {
    /*
     * float signbit(float x) {
     *     return (x < 0);
     * }
     */
    ast_value *val = nullptr;
    ast_value *x = new ast_value(ctx(), "x", TYPE_FLOAT);
    ast_block *body = new ast_block(ctx());
    ast_function *func = value(&val, "signbit", TYPE_FLOAT);

    val->m_type_params.emplace_back(x);

    /* return (x < 0); */
    body->m_exprs.push_back(
        new ast_return(
            ctx(),
            new ast_ternary(
                ctx(),
                new ast_binary(
                    ctx(),
                    INSTR_LT,
                    x,
                    m_fold->m_imm_float[0]
                ),
                m_fold->m_imm_float[1],
                m_fold->m_imm_float[0]
            )
        )
    );

    func->m_blocks.emplace_back(body);
    reg(val, func);
    return val;
}

ast_expression *intrin::acosh_() {
    /*
     * float acosh(float x) {
     *     return log(x + sqrt((x * x) - 1));
     * }
     */
    ast_value    *val    = nullptr;
    ast_value    *x        = new ast_value(ctx(), "x", TYPE_FLOAT);
    ast_call     *calllog  = ast_call::make(ctx(), func_self("log", "acosh"));
    ast_call     *callsqrt = ast_call::make(ctx(), func_self("sqrt", "acosh"));
    ast_block    *body     = new ast_block(ctx());
    ast_function *func     = value(&val, "acosh", TYPE_FLOAT);

    val->m_type_params.emplace_back(x);

    /* <callsqrt> = sqrt((x * x) - 1); */
    callsqrt->m_params.push_back(
        new ast_binary(
            ctx(),
            INSTR_SUB_F,
            new ast_binary(
                ctx(),
                INSTR_MUL_F,
                x,
                x
            ),
            m_fold->m_imm_float[1]
        )
    );

    /* <calllog> = log(x + <callsqrt>); */
    calllog->m_params.push_back(
        new ast_binary(
            ctx(),
            INSTR_ADD_F,
            x,
            callsqrt
        )
    );

    /* return <calllog>; */
    body->m_exprs.push_back(
        new ast_return(
            ctx(),
            calllog
        )
    );

    func->m_blocks.emplace_back(body);
    reg(val, func);
    return val;
}

ast_expression *intrin::asinh_() {
    /*
     * float asinh(float x) {
     *     return log(x + sqrt((x * x) + 1));
     * }
     */
    ast_value *val = nullptr;
    ast_value *x = new ast_value(ctx(), "x", TYPE_FLOAT);
    ast_call *calllog = ast_call::make(ctx(), func_self("log", "asinh"));
    ast_call *callsqrt = ast_call::make(ctx(), func_self("sqrt", "asinh"));
    ast_block *body = new ast_block(ctx());
    ast_function *func = value(&val, "asinh", TYPE_FLOAT);

    val->m_type_params.emplace_back(x);

    /* <callsqrt> = sqrt((x * x) + 1); */
    callsqrt->m_params.push_back(
        new ast_binary(
            ctx(),
            INSTR_ADD_F,
            new ast_binary(
                ctx(),
                INSTR_MUL_F,
                x,
                x
            ),
            m_fold->m_imm_float[1]
        )
    );

    /* <calllog> = log(x + <callsqrt>); */
    calllog->m_params.push_back(
        new ast_binary(
            ctx(),
            INSTR_ADD_F,
            x,
            callsqrt
        )
    );

    /* return <calllog>; */
    body->m_exprs.push_back(
        new ast_return(
            ctx(),
            calllog
        )
    );

    func->m_blocks.emplace_back(body);
    reg(val, func);
    return val;
}

ast_expression *intrin::atanh_() {
    /*
     * float atanh(float x) {
     *     return 0.5 * log((1 + x) / (1 - x))
     * }
     */
    ast_value    *val   = nullptr;
    ast_value    *x       = new ast_value(ctx(), "x", TYPE_FLOAT);
    ast_call     *calllog = ast_call::make(ctx(), func_self("log", "atanh"));
    ast_block    *body    = new ast_block(ctx());
    ast_function *func    = value(&val, "atanh", TYPE_FLOAT);

    val->m_type_params.emplace_back(x);

    /* <callog> = log((1 + x) / (1 - x)); */
    calllog->m_params.push_back(
        new ast_binary(
            ctx(),
            INSTR_DIV_F,
            new ast_binary(
                ctx(),
                INSTR_ADD_F,
                m_fold->m_imm_float[1],
                x
            ),
            new ast_binary(
                ctx(),
                INSTR_SUB_F,
                m_fold->m_imm_float[1],
                x
            )
        )
    );

    /* return 0.5 * <calllog>; */
    body->m_exprs.push_back(
        new ast_binary(
            ctx(),
            INSTR_MUL_F,
            m_fold->constgen_float(0.5, false),
            calllog
        )
    );

    func->m_blocks.emplace_back(body);
    reg(val, func);
    return val;
}

ast_expression *intrin::exp_() {
    /*
     * float exp(float x) {
     *     float sum = 1.0;
     *     float acc = 1.0;
     *     float i;
     *     for (i = 1; i < 200; ++i)
     *         sum += (acc *= x / i);
     *
     *     return sum;
     * }
     */
    ast_value *val = nullptr;
    ast_value *x = new ast_value(ctx(), "x", TYPE_FLOAT);
    ast_value *sum = new ast_value(ctx(), "sum", TYPE_FLOAT);
    ast_value *acc = new ast_value(ctx(), "acc", TYPE_FLOAT);
    ast_value *i = new ast_value(ctx(), "i",   TYPE_FLOAT);
    ast_block *body = new ast_block(ctx());
    ast_function *func = value(&val, "exp", TYPE_FLOAT);

    val->m_type_params.emplace_back(x);

    body->m_locals.push_back(sum);
    body->m_locals.push_back(acc);
    body->m_locals.push_back(i);

    /* sum = 1.0; */
    body->m_exprs.push_back(
        new ast_store(
            ctx(),
            INSTR_STORE_F,
            sum,
            m_fold->m_imm_float[1]
        )
    );

    /* acc = 1.0; */
    body->m_exprs.push_back(
        new ast_store(
            ctx(),
            INSTR_STORE_F,
            acc,
            m_fold->m_imm_float[1]
        )
    );

    /*
     * for (i = 1; i < 200; ++i)
     *     sum += (acc *= x / i);
     */
    body->m_exprs.push_back(
        new ast_loop(
            ctx(),
            /* i = 1; */
            new ast_store(
                ctx(),
                INSTR_STORE_F,
                i,
                m_fold->m_imm_float[1]
            ),
            /* i < 200; */
            new ast_binary(
                ctx(),
                INSTR_LT,
                i,
                m_fold->constgen_float(200.0f, false)
            ),
            false,
            nullptr,
            false,
            /* ++i; */
            new ast_binstore(
                ctx(),
                INSTR_STORE_F,
                INSTR_ADD_F,
                i,
                m_fold->m_imm_float[1]
            ),
            /* sum += (acc *= (x / i)) */
            new ast_binstore(
                ctx(),
                INSTR_STORE_F,
                INSTR_ADD_F,
                sum,
                new ast_binstore(
                    ctx(),
                    INSTR_STORE_F,
                    INSTR_MUL_F,
                    acc,
                    new ast_binary(
                        ctx(),
                        INSTR_DIV_F,
                        x,
                        i
                    )
                )
            )
        )
    );

    /* return sum; */
    body->m_exprs.push_back(
        new ast_return(
            ctx(),
            sum
        )
    );

    func->m_blocks.emplace_back(body);
    reg(val, func);
    return val;
}

ast_expression *intrin::exp2_() {
    /*
     * float exp2(float x) {
     *     return pow(2, x);
     * }
     */
    ast_value *val = nullptr;
    ast_call  *callpow = ast_call::make(ctx(), func_self("pow", "exp2"));
    ast_value *arg1 = new ast_value(ctx(), "x", TYPE_FLOAT);
    ast_block *body = new ast_block(ctx());
    ast_function *func = value(&val, "exp2", TYPE_FLOAT);

    val->m_type_params.emplace_back(arg1);

    callpow->m_params.push_back(m_fold->m_imm_float[3]);
    callpow->m_params.push_back(arg1);

    /* return <callpow> */
    body->m_exprs.push_back(
        new ast_return(
            ctx(),
            callpow
        )
    );

    func->m_blocks.emplace_back(body);
    reg(val, func);
    return val;
}

ast_expression *intrin::expm1_() {
    /*
     * float expm1(float x) {
     *     return exp(x) - 1;
     * }
     */
    ast_value *val = nullptr;
    ast_call *callexp = ast_call::make(ctx(), func_self("exp", "expm1"));
    ast_value *x = new ast_value(ctx(), "x", TYPE_FLOAT);
    ast_block *body = new ast_block(ctx());
    ast_function *func = value(&val, "expm1", TYPE_FLOAT);

    val->m_type_params.emplace_back(x);

    /* <callexp> = exp(x); */
    callexp->m_params.push_back(x);

    /* return <callexp> - 1; */
    body->m_exprs.push_back(
        new ast_return(
            ctx(),
            new ast_binary(
                ctx(),
                INSTR_SUB_F,
                callexp,
                m_fold->m_imm_float[1]
            )
        )
    );

    func->m_blocks.emplace_back(body);
    reg(val, func);
    return val;
}

ast_expression *intrin::pow_() {
    /*
     *
     * float pow(float base, float exp) {
     *     float result;
     *     float low;
     *     float high;
     *     float mid;
     *     float square;
     *     float accumulate;
     *
     *     if (exp == 0.0)
     *         return 1;
     *     if (exp == 1.0)
     *         return base;
     *     if (exp < 0)
     *         return 1.0 / pow(base, -exp);
     *     if (exp >= 1) {
     *         result = pow(base, exp / 2);
     *         return result * result;
     *     }
     *
     *     low        = 0.0f;
     *     high       = 1.0f;
     *     square     = sqrt(base);
     *     accumulate = square;
     *     mid        = high / 2.0f
     *
     *     while (fabs(mid - exp) > QC_POW_EPSILON) {
     *         square = sqrt(square);
     *         if (mid < exp) {
     *             low         = mid;
     *             accumulate *= square;
     *         } else {
     *             high        = mid;
     *             accumulate *= (1.0f / square);
     *         }
     *         mid = (low + high) / 2;
     *     }
     *     return accumulate;
     * }
     */
    ast_value    *val = nullptr;
    ast_function *func = value(&val, "pow", TYPE_FLOAT);

    /* prepare some calls for later */
    ast_call *callpow1  = ast_call::make(ctx(), val);                  /* for pow(base, -exp)    */
    ast_call *callpow2  = ast_call::make(ctx(), val);                  /* for pow(vase, exp / 2) */
    ast_call *callsqrt1 = ast_call::make(ctx(), func_self("sqrt", "pow")); /* for sqrt(base)         */
    ast_call *callsqrt2 = ast_call::make(ctx(), func_self("sqrt", "pow")); /* for sqrt(square)       */
    ast_call *callfabs  = ast_call::make(ctx(), func_self("fabs", "pow")); /* for fabs(mid - exp)    */

    /* prepare some blocks for later */
    ast_block *expgt1       = new ast_block(ctx());
    ast_block *midltexp     = new ast_block(ctx());
    ast_block *midltexpelse = new ast_block(ctx());
    ast_block *whileblock   = new ast_block(ctx());

    /* float pow(float base, float exp) */
    ast_value    *base = new ast_value(ctx(), "base", TYPE_FLOAT);
    ast_value    *exp  = new ast_value(ctx(), "exp",  TYPE_FLOAT);
    /* { */
    ast_block    *body = new ast_block(ctx());

    /*
     * float result;
     * float low;
     * float high;
     * float square;
     * float accumulate;
     * float mid;
     */
    ast_value *result     = new ast_value(ctx(), "result",     TYPE_FLOAT);
    ast_value *low        = new ast_value(ctx(), "low",        TYPE_FLOAT);
    ast_value *high       = new ast_value(ctx(), "high",       TYPE_FLOAT);
    ast_value *square     = new ast_value(ctx(), "square",     TYPE_FLOAT);
    ast_value *accumulate = new ast_value(ctx(), "accumulate", TYPE_FLOAT);
    ast_value *mid        = new ast_value(ctx(), "mid",        TYPE_FLOAT);
    body->m_locals.push_back(result);
    body->m_locals.push_back(low);
    body->m_locals.push_back(high);
    body->m_locals.push_back(square);
    body->m_locals.push_back(accumulate);
    body->m_locals.push_back(mid);

    val->m_type_params.emplace_back(base);
    val->m_type_params.emplace_back(exp);

    /*
     * if (exp == 0.0)
     *     return 1;
     */
    body->m_exprs.push_back(
        new ast_ifthen(
            ctx(),
            new ast_binary(
                ctx(),
                INSTR_EQ_F,
                exp,
                m_fold->m_imm_float[0]
            ),
            new ast_return(
                ctx(),
                m_fold->m_imm_float[1]
            ),
            nullptr
        )
    );

    /*
     * if (exp == 1.0)
     *     return base;
     */
    body->m_exprs.push_back(
        new ast_ifthen(
            ctx(),
            new ast_binary(
                ctx(),
                INSTR_EQ_F,
                exp,
                m_fold->m_imm_float[1]
            ),
            new ast_return(
                ctx(),
                base
            ),
            nullptr
        )
    );

    /* <callpow1> = pow(base, -exp) */
    callpow1->m_params.push_back(base);
    callpow1->m_params.push_back(
        ast_unary::make(
            ctx(),
            VINSTR_NEG_F,
            exp
        )
    );

    /*
     * if (exp < 0)
     *     return 1.0 / <callpow1>;
     */
    body->m_exprs.push_back(
        new ast_ifthen(
            ctx(),
            new ast_binary(
                ctx(),
                INSTR_LT,
                exp,
                m_fold->m_imm_float[0]
            ),
            new ast_return(
                ctx(),
                new ast_binary(
                    ctx(),
                    INSTR_DIV_F,
                    m_fold->m_imm_float[1],
                    callpow1
                )
            ),
            nullptr
        )
    );

    /* <callpow2> = pow(base, exp / 2) */
    callpow2->m_params.push_back(base);
    callpow2->m_params.push_back(
        new ast_binary(
            ctx(),
            INSTR_DIV_F,
            exp,
            m_fold->m_imm_float[3] /* 2.0f */
        )
    );

    /*
     * <expgt1> = {
     *     result = <callpow2>;
     *     return result * result;
     * }
     */
    expgt1->m_exprs.push_back(
        new ast_store(
            ctx(),
            INSTR_STORE_F,
            result,
            callpow2
        )
    );
    expgt1->m_exprs.push_back(
        new ast_return(
            ctx(),
            new ast_binary(
                ctx(),
                INSTR_MUL_F,
                result,
                result
            )
        )
    );

    /*
     * if (exp >= 1) {
     *     <expgt1>
     * }
     */
    body->m_exprs.push_back(
        new ast_ifthen(
            ctx(),
            new ast_binary(
                ctx(),
                INSTR_GE,
                exp,
                m_fold->m_imm_float[1]
            ),
            expgt1,
            nullptr
        )
    );

    /*
     * <callsqrt1> = sqrt(base)
     */
    callsqrt1->m_params.push_back(base);

    /*
     * low        = 0.0f;
     * high       = 1.0f;
     * square     = sqrt(base);
     * accumulate = square;
     * mid        = high / 2.0f;
     */
    body->m_exprs.push_back(
        new ast_store(ctx(),
            INSTR_STORE_F,
            low,
            m_fold->m_imm_float[0]
        )
    );
    body->m_exprs.push_back(
        new ast_store(
            ctx(),
            INSTR_STORE_F,
            high,
            m_fold->m_imm_float[1]
        )
    );

    body->m_exprs.push_back(
        new ast_store(
            ctx(),
            INSTR_STORE_F,
            square,
            callsqrt1
        )
    );

    body->m_exprs.push_back(
        new ast_store(
            ctx(),
            INSTR_STORE_F,
            accumulate,
            square
        )
    );
    body->m_exprs.push_back(
        new ast_store(
            ctx(),
            INSTR_STORE_F,
            mid,
            new ast_binary(
                ctx(),
                INSTR_DIV_F,
                high,
                m_fold->m_imm_float[3] /* 2.0f */
            )
        )
    );

    /*
     * <midltexp> = {
     *     low         = mid;
     *     accumulate *= square;
     * }
     */
    midltexp->m_exprs.push_back(
        new ast_store(
            ctx(),
            INSTR_STORE_F,
            low,
            mid
        )
    );
    midltexp->m_exprs.push_back(
        new ast_binstore(
            ctx(),
            INSTR_STORE_F,
            INSTR_MUL_F,
            accumulate,
            square
        )
    );

    /*
     * <midltexpelse> = {
     *     high        = mid;
     *     accumulate *= (1.0 / square);
     * }
     */
    midltexpelse->m_exprs.push_back(
        new ast_store(
            ctx(),
            INSTR_STORE_F,
            high,
            mid
        )
    );
    midltexpelse->m_exprs.push_back(
        new ast_binstore(
            ctx(),
            INSTR_STORE_F,
            INSTR_MUL_F,
            accumulate,
            new ast_binary(
                ctx(),
                INSTR_DIV_F,
                m_fold->m_imm_float[1],
                square
            )
        )
    );

    /*
     * <callsqrt2> = sqrt(square)
     */
    callsqrt2->m_params.push_back(square);

    /*
     * <whileblock> = {
     *     square = <callsqrt2>;
     *     if (mid < exp)
     *          <midltexp>;
     *     else
     *          <midltexpelse>;
     *
     *     mid = (low + high) / 2;
     * }
     */
    whileblock->m_exprs.push_back(
        new ast_store(
            ctx(),
            INSTR_STORE_F,
            square,
            callsqrt2
        )
    );
    whileblock->m_exprs.push_back(
        new ast_ifthen(
            ctx(),
            new ast_binary(
                ctx(),
                INSTR_LT,
                mid,
                exp
            ),
            midltexp,
            midltexpelse
        )
    );
    whileblock->m_exprs.push_back(
        new ast_store(
            ctx(),
            INSTR_STORE_F,
            mid,
            new ast_binary(
                ctx(),
                INSTR_DIV_F,
                new ast_binary(
                    ctx(),
                    INSTR_ADD_F,
                    low,
                    high
                ),
                m_fold->m_imm_float[3] /* 2.0f */
            )
        )
    );

    /*
     * <callabs> = fabs(mid - exp)
     */
    callfabs->m_params.push_back(
        new ast_binary(
            ctx(),
            INSTR_SUB_F,
            mid,
            exp
        )
    );

    /*
     * while (<callfabs>  > epsilon)
     *     <whileblock>
     */
    body->m_exprs.push_back(
        new ast_loop(
            ctx(),
            /* init */
            nullptr,
            /* pre condition */
            new ast_binary(
                ctx(),
                INSTR_GT,
                callfabs,
                m_fold->constgen_float(QC_POW_EPSILON, false)
            ),
            /* pre not */
            false,
            /* post condition */
            nullptr,
            /* post not */
            false,
            /* increment expression */
            nullptr,
            /* code block */
            whileblock
        )
    );

    /* return accumulate */
    body->m_exprs.push_back(
        new ast_return(
            ctx(),
            accumulate
        )
    );

    /* } */
    func->m_blocks.emplace_back(body);
    reg(val, func);
    return val;
}

ast_expression *intrin::mod_() {
    /*
     * float mod(float a, float b) {
     *     float div = a / b;
     *     float sign = (div < 0.0f) ? -1 : 1;
     *     return a - b * sign * floor(sign * div);
     * }
     */
    ast_value    *val = nullptr;
    ast_call     *call  = ast_call::make(ctx(), func_self("floor", "mod"));
    ast_value    *a     = new ast_value(ctx(), "a",    TYPE_FLOAT);
    ast_value    *b     = new ast_value(ctx(), "b",    TYPE_FLOAT);
    ast_value    *div   = new ast_value(ctx(), "div",  TYPE_FLOAT);
    ast_value    *sign  = new ast_value(ctx(), "sign", TYPE_FLOAT);
    ast_block    *body  = new ast_block(ctx());
    ast_function *func  = value(&val, "mod", TYPE_FLOAT);

    val->m_type_params.emplace_back(a);
    val->m_type_params.emplace_back(b);

    body->m_locals.push_back(div);
    body->m_locals.push_back(sign);

    /* div = a / b; */
    body->m_exprs.push_back(
        new ast_store(
            ctx(),
            INSTR_STORE_F,
            div,
            new ast_binary(
                ctx(),
                INSTR_DIV_F,
                a,
                b
            )
        )
    );

    /* sign = (div < 0.0f) ? -1 : 1; */
    body->m_exprs.push_back(
        new ast_store(
            ctx(),
            INSTR_STORE_F,
            sign,
            new ast_ternary(
                ctx(),
                new ast_binary(
                    ctx(),
                    INSTR_LT,
                    div,
                    m_fold->m_imm_float[0]
                ),
                m_fold->m_imm_float[2],
                m_fold->m_imm_float[1]
            )
        )
    );

    /* floor(sign * div) */
    call->m_params.push_back(
        new ast_binary(
            ctx(),
            INSTR_MUL_F,
            sign,
            div
        )
    );

    /* return a - b * sign * <call> */
    body->m_exprs.push_back(
        new ast_return(
            ctx(),
            new ast_binary(
                ctx(),
                INSTR_SUB_F,
                a,
                new ast_binary(
                    ctx(),
                    INSTR_MUL_F,
                    b,
                    new ast_binary(
                        ctx(),
                        INSTR_MUL_F,
                        sign,
                        call
                    )
                )
            )
        )
    );

    func->m_blocks.emplace_back(body);
    reg(val, func);
    return val;
}

ast_expression *intrin::fabs_() {
    /*
     * float fabs(float x) {
     *     return x < 0 ? -x : x;
     * }
     */
    ast_value    *val  = nullptr;
    ast_value    *arg1   = new ast_value(ctx(), "x", TYPE_FLOAT);
    ast_block    *body   = new ast_block(ctx());
    ast_function *func   = value(&val, "fabs", TYPE_FLOAT);

    body->m_exprs.push_back(
        new ast_return(
            ctx(),
            new ast_ternary(
                ctx(),
                new ast_binary(
                    ctx(),
                    INSTR_LE,
                    arg1,
                    m_fold->m_imm_float[0]
                ),
                ast_unary::make(
                    ctx(),
                    VINSTR_NEG_F,
                    arg1
                ),
                arg1
            )
        )
    );

    val->m_type_params.emplace_back(arg1);

    func->m_blocks.emplace_back(body);
    reg(val, func);
    return val;
}

ast_expression *intrin::epsilon_() {
    /*
     * float epsilon(void) {
     *     float eps = 1.0f;
     *     do { eps /= 2.0f; } while ((1.0f + (eps / 2.0f)) != 1.0f);
     *     return eps;
     * }
     */
    ast_value    *val  = nullptr;
    ast_value    *eps    = new ast_value(ctx(), "eps", TYPE_FLOAT);
    ast_block    *body   = new ast_block(ctx());
    ast_function *func   = value(&val, "epsilon", TYPE_FLOAT);

    body->m_locals.push_back(eps);

    /* eps = 1.0f; */
    body->m_exprs.push_back(
        new ast_store(
            ctx(),
            INSTR_STORE_F,
            eps,
            m_fold->m_imm_float[0]
        )
    );

    body->m_exprs.push_back(
        new ast_loop(
            ctx(),
            nullptr,
            nullptr,
            false,
            new ast_binary(
                ctx(),
                INSTR_NE_F,
                new ast_binary(
                    ctx(),
                    INSTR_ADD_F,
                    m_fold->m_imm_float[1],
                    new ast_binary(
                        ctx(),
                        INSTR_MUL_F,
                        eps,
                        m_fold->m_imm_float[3] /* 2.0f */
                    )
                ),
                m_fold->m_imm_float[1]
            ),
            false,
            nullptr,
            new ast_binstore(
                ctx(),
                INSTR_STORE_F,
                INSTR_DIV_F,
                eps,
                m_fold->m_imm_float[3] /* 2.0f */
            )
        )
    );

    /* return eps; */
    body->m_exprs.push_back(
        new ast_return(
            ctx(),
            eps
        )
    );

    func->m_blocks.emplace_back(body);
    reg(val, func);
    return val;
}

ast_expression *intrin::nan_() {
    /*
     * float nan(void) {
     *     float x = 0.0f;
     *     return x / x;
     * }
     */
    ast_value    *val  = nullptr;
    ast_value    *x      = new ast_value(ctx(), "x", TYPE_FLOAT);
    ast_function *func   = value(&val, "nan", TYPE_FLOAT);
    ast_block    *block  = new ast_block(ctx());

    block->m_locals.push_back(x);

    block->m_exprs.push_back(
        new ast_store(
            ctx(),
            INSTR_STORE_F,
            x,
            m_fold->m_imm_float[0]
        )
    );

    block->m_exprs.push_back(
        new ast_return(
            ctx(),
            new ast_binary(
                ctx(),
                INSTR_DIV_F,
                x,
                x
            )
        )
    );

    func->m_blocks.emplace_back(block);
    reg(val, func);
    return val;
}

ast_expression *intrin::inf_() {
    /*
     * float inf(void) {
     *     float x = 1.0f;
     *     float y = 0.0f;
     *     return x / y;
     * }
     */
    ast_value    *val  = nullptr;
    ast_value    *x      = new ast_value(ctx(), "x", TYPE_FLOAT);
    ast_value    *y      = new ast_value(ctx(), "y", TYPE_FLOAT);
    ast_function *func   = value(&val, "inf", TYPE_FLOAT);
    ast_block    *block  = new ast_block(ctx());
    size_t        i;

    block->m_locals.push_back(x);
    block->m_locals.push_back(y);

    /* to keep code size down */
    for (i = 0; i <= 1; i++) {
        block->m_exprs.push_back(
            new ast_store(
                ctx(),
                INSTR_STORE_F,
                ((i == 0) ? x : y),
                m_fold->m_imm_float[i]
            )
        );
    }

    block->m_exprs.push_back(
        new ast_return(
            ctx(),
            new ast_binary(
                ctx(),
                INSTR_DIV_F,
                x,
                y
            )
        )
    );

    func->m_blocks.emplace_back(block);
    reg(val, func);
    return val;
}

ast_expression *intrin::ln_() {
    /*
     * float log(float power, float base) {
     *   float whole;
     *   float nth
     *   float sign = 1.0f;
     *   float eps  = epsilon();
     *
     *   if (power <= 1.0f || bbase <= 1.0) {
     *       if (power <= 0.0f || base <= 0.0f)
     *           return nan();
     *
     *       if (power < 1.0f) {
     *           power = 1.0f / power;
     *           sign *= -1.0f;
     *       }
     *
     *       if (base < 1.0f) {
     *           sign *= -1.0f;
     *           base  = 1.0f / base;
     *       }
     *   }
     *
     *   float A_i       = 1;
     *   float B_i       = 0;
     *   float A_iminus1 = 0;
     *   float B_iminus1 = 1;
     *
     *   for (;;) {
     *       whole = power;
     *       nth   = 0.0f;
     *
     *       while (whole >= base) {
     *           float base2    = base;
     *           float n2       = 1.0f;
     *           float newbase2 = base2 * base2;
     *
     *           while (whole >= newbase2) {
     *               base2     = newbase2;
     *               n2       *= 2;
     *               newbase2 *= newbase2;
     *           }
     *
     *           whole /= base2;
     *           nth += n2;
     *       }
     *
     *       float b_iplus1 = n;
     *       float A_iplus1 = b_iplus1 * A_i + A_iminus1;
     *       float B_iplus1 = b_iplus1 * B_i + B_iminus1;
     *
     *       A_iminus1 = A_i;
     *       B_iminus1 = B_i;
     *       A_i       = A_iplus1;
     *       B_i       = B_iplus1;
     *
     *       if (whole <= 1.0f + eps)
     *           break;
     *
     *       power = base;
     *       bower = whole;
     *   }
     *   return sign * A_i / B_i;
     * }
     */

    ast_value *val = nullptr;
    ast_value *power = new ast_value(ctx(), "power", TYPE_FLOAT);
    ast_value *base = new ast_value(ctx(), "base",TYPE_FLOAT);
    ast_value *whole= new ast_value(ctx(), "whole", TYPE_FLOAT);
    ast_value *nth = new ast_value(ctx(), "nth", TYPE_FLOAT);
    ast_value *sign = new ast_value(ctx(), "sign", TYPE_FLOAT);
    ast_value *A_i = new ast_value(ctx(), "A_i", TYPE_FLOAT);
    ast_value *B_i = new ast_value(ctx(), "B_i", TYPE_FLOAT);
    ast_value *A_iminus1 = new ast_value(ctx(), "A_iminus1", TYPE_FLOAT);
    ast_value *B_iminus1 = new ast_value(ctx(), "B_iminus1", TYPE_FLOAT);
    ast_value *b_iplus1 = new ast_value(ctx(), "b_iplus1", TYPE_FLOAT);
    ast_value *A_iplus1 = new ast_value(ctx(), "A_iplus1", TYPE_FLOAT);
    ast_value *B_iplus1 = new ast_value(ctx(), "B_iplus1", TYPE_FLOAT);
    ast_value *eps = new ast_value(ctx(), "eps", TYPE_FLOAT);
    ast_value *base2 = new ast_value(ctx(), "base2", TYPE_FLOAT);
    ast_value *n2 = new ast_value(ctx(), "n2",TYPE_FLOAT);
    ast_value *newbase2 = new ast_value(ctx(), "newbase2", TYPE_FLOAT);
    ast_block *block = new ast_block(ctx());
    ast_block *plt1orblt1 = new ast_block(ctx()); // (power <= 1.0f || base <= 1.0f)
    ast_block *plt1 = new ast_block(ctx()); // (power < 1.0f)
    ast_block *blt1 = new ast_block(ctx()); // (base< 1.0f)
    ast_block *forloop = new ast_block(ctx()); // for(;;)
    ast_block *whileloop = new ast_block(ctx()); // while (whole >= base)
    ast_block *nestwhile= new ast_block(ctx()); // while (whole >= newbase2)
    ast_function *func = value(&val, "ln", TYPE_FLOAT);
    size_t i;

    val->m_type_params.emplace_back(power);
    val->m_type_params.emplace_back(base);

    block->m_locals.push_back(whole);
    block->m_locals.push_back(nth);
    block->m_locals.push_back(sign);
    block->m_locals.push_back(eps);
    block->m_locals.push_back(A_i);
    block->m_locals.push_back(B_i);
    block->m_locals.push_back(A_iminus1);
    block->m_locals.push_back(B_iminus1);

    /* sign = 1.0f; */
    block->m_exprs.push_back(
        new ast_store(
            ctx(),
            INSTR_STORE_F,
            sign,
            m_fold->m_imm_float[1]
        )
    );

    /* eps = __builtin_epsilon(); */
    block->m_exprs.push_back(
        new ast_store(
            ctx(),
            INSTR_STORE_F,
            eps,
            ast_call::make(
                ctx(),
                func_self("__builtin_epsilon", "ln")
            )
        )
    );

    /*
     * A_i       = 1;
     * B_i       = 0;
     * A_iminus1 = 0;
     * B_iminus1 = 1;
     */
    for (i = 0; i <= 1; i++) {
        int j;
        for (j = 1; j >= 0; j--) {
            block->m_exprs.push_back(
                new ast_store(
                    ctx(),
                    INSTR_STORE_F,
                    ((j) ? ((i) ? B_iminus1 : A_i)
                                          : ((i) ? A_iminus1 : B_i)),
                    m_fold->m_imm_float[j]
                )
            );
        }
    }

    /*
     * <plt1> = {
     *     power = 1.0f / power;
     *     sign *= -1.0f;
     * }
     * <blt1> = {
     *     base  = 1.0f / base;
     *     sign *= -1.0f;
     * }
     */
    for (i = 0; i <= 1; i++) {
        ((i) ? blt1 : plt1)->m_exprs.push_back(
            new ast_store(
                ctx(),
                INSTR_STORE_F,
                ((i) ? base : power),
                new ast_binary(
                    ctx(),
                    INSTR_DIV_F,
                    m_fold->m_imm_float[1],
                    ((i) ? base : power)
                )
            )
        );
        plt1->m_exprs.push_back(
            new ast_binstore(
                ctx(),
                INSTR_STORE_F,
                INSTR_MUL_F,
                sign,
                m_fold->m_imm_float[2]
            )
        );
    }

    /*
     * <plt1orblt1> = {
     *     if (power <= 0.0 || base <= 0.0f)
     *         return __builtin_nan();
     *     if (power < 1.0f)
     *         <plt1>
     *     if (base < 1.0f)
     *         <blt1>
     * }
     */
    plt1orblt1->m_exprs.push_back(
        new ast_ifthen(
            ctx(),
            new ast_binary(
                ctx(),
                INSTR_OR,
                new ast_binary(
                    ctx(),
                    INSTR_LE,
                    power,
                    m_fold->m_imm_float[0]
                ),
                new ast_binary(
                    ctx(),
                    INSTR_LE,
                    base,
                    m_fold->m_imm_float[0]
                )
            ),
            new ast_return(
                ctx(),
                ast_call::make(
                    ctx(),
                    func_self("__builtin_nan", "ln")
                )
            ),
            nullptr
        )
    );

    for (i = 0; i <= 1; i++) {
        plt1orblt1->m_exprs.push_back(
            new ast_ifthen(
                ctx(),
                new ast_binary(
                    ctx(),
                    INSTR_LT,
                    ((i) ? base : power),
                    m_fold->m_imm_float[1]
                ),
                ((i) ? blt1 : plt1),
                nullptr
            )
        );
    }

    block->m_exprs.push_back(plt1orblt1);


    /* whole = power; */
    forloop->m_exprs.push_back(
        new ast_store(
            ctx(),
            INSTR_STORE_F,
            whole,
            power
        )
    );

    /* nth = 0.0f; */
    forloop->m_exprs.push_back(
        new ast_store(
            ctx(),
            INSTR_STORE_F,
            nth,
            m_fold->m_imm_float[0]
        )
    );

    /* base2 = base; */
    whileloop->m_exprs.push_back(
        new ast_store(
            ctx(),
            INSTR_STORE_F,
            base2,
            base
        )
    );

    /* n2 = 1.0f; */
    whileloop->m_exprs.push_back(
        new ast_store(
            ctx(),
            INSTR_STORE_F,
            n2,
            m_fold->m_imm_float[1]
        )
    );

    /* newbase2 = base2 * base2; */
    whileloop->m_exprs.push_back(
        new ast_store(
            ctx(),
            INSTR_STORE_F,
            newbase2,
            new ast_binary(
                ctx(),
                INSTR_MUL_F,
                base2,
                base2
            )
        )
    );

    /* while loop locals */
    whileloop->m_locals.push_back(base2);
    whileloop->m_locals.push_back(n2);
    whileloop->m_locals.push_back(newbase2);

    /* base2 = newbase2; */
    nestwhile->m_exprs.push_back(
        new ast_store(
            ctx(),
            INSTR_STORE_F,
            base2,
            newbase2
        )
    );

    /* n2 *= 2; */
    nestwhile->m_exprs.push_back(
        new ast_binstore(
            ctx(),
            INSTR_STORE_F,
            INSTR_MUL_F,
            n2,
            m_fold->m_imm_float[3] /* 2.0f */
        )
    );

    /* newbase2 *= newbase2; */
    nestwhile->m_exprs.push_back(
        new ast_binstore(
            ctx(),
            INSTR_STORE_F,
            INSTR_MUL_F,
            newbase2,
            newbase2
        )
    );

    /* while (whole >= newbase2) */
    whileloop->m_exprs.push_back(
        new ast_loop(
            ctx(),
            nullptr,
            new ast_binary(
                ctx(),
                INSTR_GE,
                whole,
                newbase2
            ),
            false,
            nullptr,
            false,
            nullptr,
            nestwhile
        )
    );

    /* whole /= base2; */
    whileloop->m_exprs.push_back(
        new ast_binstore(
            ctx(),
            INSTR_STORE_F,
            INSTR_DIV_F,
            whole,
            base2
        )
    );

    /* nth += n2; */
    whileloop->m_exprs.push_back(
        new ast_binstore(
            ctx(),
            INSTR_STORE_F,
            INSTR_ADD_F,
            nth,
            n2
        )
    );

    /* while (whole >= base) */
    forloop->m_exprs.push_back(
        new ast_loop(
            ctx(),
            nullptr,
            new ast_binary(
                ctx(),
                INSTR_GE,
                whole,
                base
            ),
            false,
            nullptr,
            false,
            nullptr,
            whileloop
        )
    );

    forloop->m_locals.push_back(b_iplus1);
    forloop->m_locals.push_back(A_iplus1);
    forloop->m_locals.push_back(B_iplus1);

    /* b_iplus1 = nth; */
    forloop->m_exprs.push_back(
        new ast_store(
            ctx(),
            INSTR_STORE_F,
            b_iplus1,
            nth
        )
    );

    /*
     * A_iplus1 = b_iplus1 * A_i + A_iminus1;
     * B_iplus1 = b_iplus1 * B_i + B_iminus1;
     */
    for (i = 0; i <= 1; i++) {
        forloop->m_exprs.push_back(
            new ast_store(
                ctx(),
                INSTR_STORE_F,
                ((i) ? B_iplus1 : A_iplus1),
                new ast_binary(
                    ctx(),
                    INSTR_ADD_F,
                    new ast_binary(
                        ctx(),
                        INSTR_MUL_F,
                        b_iplus1,
                         ((i) ? B_i : A_i)
                    ),
                    ((i) ? B_iminus1 : A_iminus1)
                )
            )
        );
    }

    /*
     * A_iminus1 = A_i;
     * B_iminus1 = B_i;
     */
    for (i = 0; i <= 1; i++) {
        forloop->m_exprs.push_back(
            new ast_store(
                ctx(),
                INSTR_STORE_F,
                ((i) ? B_iminus1 : A_iminus1),
                ((i) ? B_i       : A_i)
            )
        );
    }

    /*
     * A_i = A_iplus1;
     * B_i = B_iplus1;
     */
    for (i = 0; i <= 1; i++) {
        forloop->m_exprs.push_back(
            new ast_store(
                ctx(),
                INSTR_STORE_F,
                ((i) ? B_i      : A_i),
                ((i) ? B_iplus1 : A_iplus1)
            )
        );
    }

    /*
     * if (whole <= 1.0f + eps)
     *     break;
     */
    forloop->m_exprs.push_back(
        new ast_ifthen(
            ctx(),
            new ast_binary(
                ctx(),
                INSTR_LE,
                whole,
                new ast_binary(
                    ctx(),
                    INSTR_ADD_F,
                    m_fold->m_imm_float[1],
                    eps
                )
            ),
            new ast_breakcont(
                ctx(),
                false,
                0
            ),
            nullptr
        )
    );

    /*
     * power = base;
     * base  = whole;
     */
    for (i = 0; i <= 1; i++) {
        forloop->m_exprs.push_back(
            new ast_store(
                ctx(),
                INSTR_STORE_F,
                ((i) ? base  : power),
                ((i) ? whole : base)
            )
        );
    }

    /* add the for loop block */
    block->m_exprs.push_back(
        new ast_loop(
            ctx(),
            nullptr,
            /* for(; 1; ) ?? (can this be nullptr too?) */
            m_fold->m_imm_float[1],
            false,
            nullptr,
            false,
            nullptr,
            forloop
        )
    );

    /* return sign * A_i / B_il */
    block->m_exprs.push_back(
        new ast_return(
            ctx(),
            new ast_binary(
                ctx(),
                INSTR_MUL_F,
                sign,
                new ast_binary(
                    ctx(),
                    INSTR_DIV_F,
                    A_i,
                    B_i
                )
            )
        )
    );

    func->m_blocks.emplace_back(block);
    reg(val, func);
    return val;
}

ast_expression *intrin::log_variant(const char *name, float base) {
    ast_value *val = nullptr;
    ast_call *callln = ast_call::make(ctx(), func_self("__builtin_ln", name));
    ast_value *arg1 = new ast_value(ctx(), "x", TYPE_FLOAT);
    ast_block *body = new ast_block(ctx());
    ast_function *func = value(&val, name, TYPE_FLOAT);

    val->m_type_params.emplace_back(arg1);

    callln->m_params.push_back(arg1);
    callln->m_params.push_back(m_fold->constgen_float(base, false));

    body->m_exprs.push_back(
        new ast_return(
            ctx(),
            callln
        )
    );

    func->m_blocks.emplace_back(body);
    reg(val, func);
    return val;
}

ast_expression *intrin::log_() {
    return log_variant("log", 2.7182818284590452354);
}
ast_expression *intrin::log10_() {
    return log_variant("log10", 10);
}
ast_expression *intrin::log2_() {
    return log_variant("log2", 2);
}
ast_expression *intrin::logb_() {
    /* FLT_RADIX == 2 for now */
    return log_variant("log2", 2);
}

ast_expression *intrin::shift_variant(const char *name, size_t instr) {
    /*
     * float [shift] (float a, float b) {
     *   return floor(a [instr] pow(2, b));
     */
    ast_value *val = nullptr;
    ast_call *callpow = ast_call::make(ctx(), func_self("pow", name));
    ast_call *callfloor = ast_call::make(ctx(), func_self("floor", name));
    ast_value *a = new ast_value(ctx(), "a", TYPE_FLOAT);
    ast_value *b = new ast_value(ctx(), "b", TYPE_FLOAT);
    ast_block *body = new ast_block(ctx());
    ast_function *func = value(&val, name, TYPE_FLOAT);

    val->m_type_params.emplace_back(a);
    val->m_type_params.emplace_back(b);

    /* <callpow> = pow(2, b) */
    callpow->m_params.push_back(m_fold->m_imm_float[3]);
    callpow->m_params.push_back(b);

    /* <callfloor> = floor(a [instr] <callpow>) */
    callfloor->m_params.push_back(
        new ast_binary(
            ctx(),
            instr,
            a,
            callpow
        )
    );

    /* return <callfloor> */
    body->m_exprs.push_back(
        new ast_return(
            ctx(),
            callfloor
        )
    );

    func->m_blocks.emplace_back(body);
    reg(val, func);
    return val;
}

ast_expression *intrin::lshift() {
    return shift_variant("lshift", INSTR_MUL_F);
}

ast_expression *intrin::rshift() {
    return shift_variant("rshift", INSTR_DIV_F);
}

void intrin::error(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vcompile_error(ctx(), fmt, ap);
    va_end(ap);
}

/* exposed */
ast_expression *intrin::debug_typestring() {
    return (ast_expression*)0x1;
}

intrin::intrin(parser_t *parser)
    : m_parser(parser)
    , m_fold(&parser->m_fold)
{
    static const intrin_func_t intrinsics[] = {
        {&intrin::isfinite_,        "__builtin_isfinite",         "isfinite", 1},
        {&intrin::isinf_,           "__builtin_isinf",            "isinf",    1},
        {&intrin::isnan_,           "__builtin_isnan",            "isnan",    1},
        {&intrin::isnormal_,        "__builtin_isnormal",         "isnormal", 1},
        {&intrin::signbit_,         "__builtin_signbit",          "signbit",  1},
        {&intrin::acosh_,           "__builtin_acosh",            "acosh",    1},
        {&intrin::asinh_,           "__builtin_asinh",            "asinh",    1},
        {&intrin::atanh_,           "__builtin_atanh",            "atanh",    1},
        {&intrin::exp_,             "__builtin_exp",              "exp",      1},
        {&intrin::exp2_,            "__builtin_exp2",             "exp2",     1},
        {&intrin::expm1_,           "__builtin_expm1",            "expm1",    1},
        {&intrin::mod_,             "__builtin_mod",              "mod",      2},
        {&intrin::pow_,             "__builtin_pow",              "pow",      2},
        {&intrin::fabs_,            "__builtin_fabs",             "fabs",     1},
        {&intrin::log_,             "__builtin_log",              "log",      1},
        {&intrin::log10_,           "__builtin_log10",            "log10",    1},
        {&intrin::log2_,            "__builtin_log2",             "log2",     1},
        {&intrin::logb_,            "__builtin_logb",             "logb",     1},
        {&intrin::lshift,           "__builtin_lshift",           "",         2},
        {&intrin::rshift,           "__builtin_rshift",           "",         2},
        {&intrin::epsilon_,         "__builtin_epsilon",          "",         0},
        {&intrin::nan_,             "__builtin_nan",              "",         0},
        {&intrin::inf_,             "__builtin_inf",              "",         0},
        {&intrin::ln_,              "__builtin_ln",               "",         2},
        {&intrin::debug_typestring, "__builtin_debug_typestring", "",         0},
        {&intrin::nullfunc,         "#nullfunc",                  "",         0}
    };

    for (auto &it : intrinsics) {
        m_intrinsics.push_back(it);
        m_generated.push_back(nullptr);
    }
}

ast_expression *intrin::do_fold(ast_value *val, ast_expression **exprs) {
    if (!val || !val->m_name.length())
        return nullptr;
    static constexpr size_t kPrefixLength = 10; // "__builtin_"
    for (auto &it : m_intrinsics) {
        if (val->m_name == it.name)
            return (vec_size(exprs) != it.args)
                        ? nullptr
                        : m_fold->intrinsic(val->m_name.c_str() + kPrefixLength, exprs);
    }
    return nullptr;
}

ast_expression *intrin::func_try(size_t offset, const char *compare) {
    for (auto &it : m_intrinsics) {
        const size_t index = &it - &m_intrinsics[0];
        if (strcmp(*(char **)((char *)&it + offset), compare))
            continue;
        if (m_generated[index])
            return m_generated[index];
        return m_generated[index] = (this->*it.intrin_func_t::function)();
    }
    return nullptr;
}

ast_expression *intrin::func_self(const char *name, const char *from) {
    ast_expression *find;
    /* try current first */
    if ((find = parser_find_global(m_parser, name)) && ((ast_value*)find)->m_vtype == TYPE_FUNCTION)
        for (auto &it : m_parser->functions)
            if (reinterpret_cast<ast_value*>(find)->m_name.length() && it->m_name == reinterpret_cast<ast_value*>(find)->m_name && it->m_builtin < 0)
                return find;
    /* try name second */
    if ((find = func_try(offsetof(intrin_func_t, name),  name)))
        return find;
    /* try alias third */
    if ((find = func_try(offsetof(intrin_func_t, alias), name)))
        return find;

    if (from) {
        error("need function `%s', compiler depends on it for `__builtin_%s'", name, from);
        return func_self("#nullfunc", nullptr);
    }
    return nullptr;
}

ast_expression *intrin::func(const char *name) {
    return func_self(name, nullptr);
}
