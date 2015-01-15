#ifndef GMQCC_PARSER_HDR
#define GMQCC_PARSER_HDR
#include "gmqcc.h"
#include "lexer.h"
#include "ast.h"
#include "intrin.h"

struct parser_t;

struct fold_t {
    parser_t *parser;
    std::vector<ast_value*> imm_float;
    std::vector<ast_value*> imm_vector;
    std::vector<ast_value*> imm_string;
    hash_table_t *imm_string_untranslate; /* map<string, ast_value*> */
    hash_table_t *imm_string_dotranslate; /* map<string, ast_value*> */
};

#define parser_ctx(p) ((p)->lex->tok.ctx)

struct parser_t {
    parser_t() { }

    lex_file *lex;
    int tok;

    bool ast_cleaned;

    std::vector<ast_expression *> globals;
    std::vector<ast_expression *> fields;
    std::vector<ast_function *> functions;
    size_t translated;

    /* must be deleted first, they reference immediates and values */
    std::vector<ast_value *> accessors;

    ast_value *nil;
    ast_value *reserved_version;

    size_t crc_globals;
    size_t crc_fields;

    ast_function *function;
    ht aliases;

    /* All the labels the function defined...
     * Should they be in ast_function instead?
     */
    std::vector<ast_label*> labels;
    std::vector<ast_goto*> gotos;
    std::vector<const char *> breaks;
    std::vector<const char *> continues;

    /* A list of hashtables for each scope */
    ht *variables;
    ht htfields;
    ht htglobals;
    ht *typedefs;

    /* not to be used directly, we use the hash table */
    ast_expression **_locals;
    size_t *_blocklocals;
    ast_value **_typedefs;
    size_t *_blocktypedefs;
    lex_ctx_t *_block_ctx;

    /* we store the '=' operator info */
    const oper_info *assign_op;

    /* magic values */
    ast_value *const_vec[3];

    /* pragma flags */
    bool noref;

    /* collected information */
    size_t max_param_count;

    fold_t *fold;
    intrin m_intrin;
};


/* parser.c */
char           *parser_strdup     (const char *str);
ast_expression *parser_find_global(parser_t *parser, const char *name);

/* fold.c */
fold_t         *fold_init           (parser_t *);
void            fold_cleanup        (fold_t *);
ast_expression *fold_constgen_float (fold_t *, qcfloat_t, bool);
ast_expression *fold_constgen_vector(fold_t *, vec3_t);
ast_expression *fold_constgen_string(fold_t *, const char *, bool);
bool            fold_generate       (fold_t *, ir_builder *);
ast_expression *fold_op             (fold_t *, const oper_info *, ast_expression **);
ast_expression *fold_intrin         (fold_t *, const char      *, ast_expression **);

ast_expression *fold_binary         (lex_ctx_t ctx, int, ast_expression *, ast_expression *);
int             fold_cond_ifthen    (ir_value *, ast_function *, ast_ifthen  *);
int             fold_cond_ternary   (ir_value *, ast_function *, ast_ternary *);

#endif
