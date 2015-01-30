#ifndef GMQCC_FOLD_HDR
#define GMQCC_FOLD_HDR
#include "lexer.h"
#include "gmqcc.h"

struct ir_builder;
struct ir_value;

struct ast_function;
struct ast_ifthen;
struct ast_ternary;
struct ast_expression;
struct ast_value;

struct parser_t;

struct fold {
    fold();
    fold(parser_t *parser);
    ~fold();

    bool generate(ir_builder *ir);
    ast_expression *op(const oper_info *info, ast_expression **opexprs);
    ast_expression *intrinsic(const char *intrinsic, ast_expression **arg);

    static int cond_ternary(ir_value *condval, ast_function *func, ast_ternary *branch);
    static int cond_ifthen(ir_value *condval, ast_function *func, ast_ifthen *branch);

    static ast_expression *superfluous(ast_expression *left, ast_expression *right, int op);
    static ast_expression *binary(lex_ctx_t ctx, int op, ast_expression *left, ast_expression *right);

    ast_expression *constgen_float(qcfloat_t value, bool inexact);
    ast_expression *constgen_vector(vec3_t value);
    ast_expression *constgen_string(const char *str, bool translate);
    ast_expression *constgen_string(const std::string &str, bool translate);

    ast_value *imm_float(size_t index) const { return m_imm_float[index]; }
    ast_value *imm_vector(size_t index) const { return m_imm_vector[index]; }

protected:
    static qcfloat_t immvalue_float(ast_value *value);
    static vec3_t immvalue_vector(ast_value *value);
    static const char *immvalue_string(ast_value *value);

    lex_ctx_t ctx();

    bool immediate_true(ast_value *v);

    bool check_except_float_impl(void (*callback)(void), ast_value *a, ast_value *b);
    bool check_inexact_float(ast_value *a, ast_value *b);

    ast_expression *op_mul_vec(vec3_t vec, ast_value *sel, const char *set);
    ast_expression *op_neg(ast_value *a);
    ast_expression *op_not(ast_value *a);
    ast_expression *op_add(ast_value *a, ast_value *b);
    ast_expression *op_sub(ast_value *a, ast_value *b);
    ast_expression *op_mul(ast_value *a, ast_value *b);
    ast_expression *op_div(ast_value *a, ast_value *b);
    ast_expression *op_mod(ast_value *a, ast_value *b);
    ast_expression *op_bor(ast_value *a, ast_value *b);
    ast_expression *op_band(ast_value *a, ast_value *b);
    ast_expression *op_xor(ast_value *a, ast_value *b);
    ast_expression *op_lshift(ast_value *a, ast_value *b);
    ast_expression *op_rshift(ast_value *a, ast_value *b);
    ast_expression *op_andor(ast_value *a, ast_value *b, float expr);
    ast_expression *op_tern(ast_value *a, ast_value *b, ast_value *c);
    ast_expression *op_exp(ast_value *a, ast_value *b);
    ast_expression *op_lteqgt(ast_value *a, ast_value *b);
    ast_expression *op_ltgt(ast_value *a, ast_value *b, bool lt);
    ast_expression *op_cmp(ast_value *a, ast_value *b, bool ne);
    ast_expression *op_bnot(ast_value *a);
    ast_expression *op_cross(ast_value *a, ast_value *b);
    ast_expression *op_length(ast_value *a);

    ast_expression *intrinsic_isfinite(ast_value *a);
    ast_expression *intrinsic_isinf(ast_value *a);
    ast_expression *intrinsic_isnan(ast_value *a);
    ast_expression *intrinsic_isnormal(ast_value *a);
    ast_expression *intrinsic_signbit(ast_value *a);
    ast_expression *intrinsic_acosh(ast_value *a);
    ast_expression *intrinsic_asinh(ast_value *a);
    ast_expression *intrinsic_atanh(ast_value *a);
    ast_expression *intrinsic_exp(ast_value *a);
    ast_expression *intrinsic_exp2(ast_value *a);
    ast_expression *intrinsic_expm1(ast_value *a);
    ast_expression *intrinsic_mod(ast_value *lhs, ast_value *rhs);
    ast_expression *intrinsic_pow(ast_value *lhs, ast_value *rhs);
    ast_expression *intrinsic_fabs(ast_value *a);

    static qcfloat_t immvalue_float(ir_value *value);
    static vec3_t immvalue_vector(ir_value *value);

    static int cond(ir_value *condval, ast_function *func, ast_ifthen *branch);

private:
    friend struct intrin;

    std::vector<ast_value*> m_imm_float;
    std::vector<ast_value*> m_imm_vector;
    std::vector<ast_value*> m_imm_string;
    hash_table_t *m_imm_string_untranslate; /* map<string, ast_value*> */
    hash_table_t *m_imm_string_dotranslate; /* map<string, ast_value*> */
    parser_t *m_parser;
    bool m_initialized;
};

#endif
