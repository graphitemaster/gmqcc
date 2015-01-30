#ifndef GMQCC_INTRIN_HDR
#define GMQCC_INTRIN_HDR
#include "gmqcc.h"

struct fold;
struct parser_t;

struct ast_function;
struct ast_expression;
struct ast_value;

struct intrin;

struct intrin_func_t {
    ast_expression *(intrin::*function)();
    const char *name;
    const char *alias;
    size_t args;
};

struct intrin {
    intrin() = default;
    intrin(parser_t *parser);

    ast_expression *debug_typestring();
    ast_expression *do_fold(ast_value *val, ast_expression **exprs);
    ast_expression *func_try(size_t offset, const char *compare);
    ast_expression *func_self(const char *name, const char *from);
    ast_expression *func(const char *name);

protected:
    lex_ctx_t ctx() const;
    ast_function *value(ast_value **out, const char *name, qc_type vtype);
    void reg(ast_value *const value, ast_function *const func);

    ast_expression *nullfunc();
    ast_expression *isfinite_();
    ast_expression *isinf_();
    ast_expression *isnan_();
    ast_expression *isnormal_();
    ast_expression *signbit_();
    ast_expression *acosh_();
    ast_expression *asinh_();
    ast_expression *atanh_();
    ast_expression *exp_();
    ast_expression *exp2_();
    ast_expression *expm1_();
    ast_expression *pow_();
    ast_expression *mod_();
    ast_expression *fabs_();
    ast_expression *epsilon_();
    ast_expression *nan_();
    ast_expression *inf_();
    ast_expression *ln_();
    ast_expression *log_variant(const char *name, float base);
    ast_expression *log_();
    ast_expression *log10_();
    ast_expression *log2_();
    ast_expression *logb_();
    ast_expression *shift_variant(const char *name, size_t instr);
    ast_expression *lshift();
    ast_expression *rshift();

    void error(const char *fmt, ...);

private:
    parser_t *m_parser;
    fold *m_fold;
    std::vector<intrin_func_t> m_intrinsics;
    std::vector<ast_expression*> m_generated;
};


#endif
