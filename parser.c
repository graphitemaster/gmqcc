#include <stdio.h>
#include <stdarg.h>

#include "gmqcc.h"
#include "lexer.h"

typedef struct {
    lex_file *lex;
    int      tok;

    MEM_VECTOR_MAKE(ast_value*, globals);
    MEM_VECTOR_MAKE(ast_function*, functions);
} parser_t;

MEM_VEC_FUNCTIONS(parser_t, ast_value*, globals)
MEM_VEC_FUNCTIONS(parser_t, ast_function*, functions)

void parseerror(parser_t *parser, const char *fmt, ...)
{
	va_list ap;

    if (parser)
	    printf("error %s:%lu: ", parser->lex->tok->ctx.file, (unsigned long)parser->lex->tok->ctx.line);
	else
	    printf("error: ");

	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);

	printf("\n");
}

bool parser_next(parser_t *parser)
{
    /* lex_do kills the previous token */
    parser->tok = lex_do(parser->lex);
    if (parser->tok == TOKEN_EOF || parser->tok >= TOKEN_ERROR)
        return false;
    return true;
}

/* lift a token out of the parser so it's not destroyed by parser_next */
token *parser_lift(parser_t *parser)
{
    token *tok = parser->lex->tok;
    parser->lex->tok = NULL;
    return tok;
}

#define parser_tokval(p) (p->lex->tok->value)
#define parser_token(p)  (p->lex->tok)
#define parser_ctx(p)    (p->lex->tok->ctx)

ast_value* parser_find_global(parser_t *parser, const char *name)
{
    size_t i;
    for (i = 0; i < parser->globals_count; ++i) {
        if (!strcmp(parser->globals[i]->name, name))
            return parser->globals[i];
    }
    return NULL;
}

typedef struct {
    MEM_VECTOR_MAKE(ast_value*, p);
} paramlist_t;
MEM_VEC_FUNCTIONS(paramlist_t, ast_value*, p)

ast_value *parser_parse_type(parser_t *parser, bool *isfunc)
{
    paramlist_t params;
    ast_value *var;
    lex_ctx   ctx = parser_ctx(parser);
    int vtype = parser_token(parser)->constval.t;

    MEM_VECTOR_INIT(&params, p);

    *isfunc = false;

    if (!parser_next(parser))
        return NULL;

    if (parser->tok == '(') {
        *isfunc = true;
        while (true) {
            ast_value *param;
            bool dummy;

            if (!parser_next(parser)) {
                MEM_VECTOR_CLEAR(&params, p);
                return NULL;
            }

            param = parser_parse_type(parser, &dummy);
            (void)dummy;

            if (!param) {
                MEM_VECTOR_CLEAR(&params, p);
                return NULL;
            }

            if (!paramlist_t_p_add(&params, param)) {
                MEM_VECTOR_CLEAR(&params, p);
                parseerror(parser, "Out of memory while parsing typename");
                return NULL;
            }

            if (parser->tok == ',')
                continue;
            if (parser->tok == ')')
                break;
            MEM_VECTOR_CLEAR(&params, p);
            parseerror(parser, "Unexpected token");
            return NULL;
        }
        if (!parser_next(parser)) {
            MEM_VECTOR_CLEAR(&params, p);
            return NULL;
        }
    }

    var = ast_value_new(ctx, "<unnamed>", vtype);
    if (!var) {
        MEM_VECTOR_CLEAR(&params, p);
        return NULL;
    }
    MEM_VECTOR_MOVE(&params, p, var, params);
    return var;
}

bool parser_do(parser_t *parser)
{
    if (parser->tok == TOKEN_TYPENAME)
    {
        bool isfunc = false;
        ast_function *func = NULL;
        lex_ctx ctx = parser_ctx(parser);
        ast_value *var = parser_parse_type(parser, &isfunc);
        if (!var)
            return false;

        if (parser->tok != TOKEN_IDENT) {
            parseerror(parser, "expected variable name\n");
            return false;
        }

        if (parser_find_global(parser, parser_tokval(parser))) {
            ast_value_delete(var);
            parseerror(parser, "global already exists: %s\n", parser_tokval(parser));
            return false;
        }

        if (!ast_value_set_name(var, parser_tokval(parser))) {
            parseerror(parser, "failed to set variable name\n");
            ast_value_delete(var);
            return false;
        }

        if (isfunc) {
            /* a function was defined */
            ast_value *fval;

            /* turn var into a value of TYPE_FUNCTION, with the old var
             * as return type
             */
            fval = ast_value_new(ctx, var->name, TYPE_FUNCTION);
            func = ast_function_new(ctx, var->name, fval);
            if (!fval || !func) {
                ast_value_delete(var);
                if (fval) ast_value_delete(fval);
                if (func) ast_function_delete(func);
                return false;
            }

            fval->expression.next = (ast_expression*)var;
            MEM_VECTOR_MOVE(var, params, fval, params);

            if (!parser_t_functions_add(parser, func)) {
                ast_value_delete(var);
                if (fval) ast_value_delete(fval);
                if (func) ast_function_delete(func);
                return false;
            }

            var = fval;
        }

        if (!parser_t_globals_add(parser, var) ||
            !parser_next(parser))
        {
            ast_value_delete(var);
            return false;
        }

        if (parser->tok == ';') {
            if (!parser_next(parser))
                return parser->tok == TOKEN_EOF;
            return true;
        }

        if (parser->tok != '=') {
            parseerror(parser, "expected '=' or ';'");
            return false;
        }

        if (!parser_next(parser))
            return false;

        if (parser->tok == '#') {
            if (!isfunc || !func) {
                parseerror(parser, "unexpected builtin number, '%s' is not a function", var->name);
                return false;
            }
            if (!parser_next(parser)) {
                parseerror(parser, "expected builtin number");
                return false;
            }
            if (parser->tok != TOKEN_INTCONST) {
                parseerror(parser, "builtin number must be an integer constant");
                return false;
            }
            if (parser_token(parser)->constval.i <= 0) {
                parseerror(parser, "builtin number must be positive integer greater than zero");
                return false;
            }

            func->builtin = -parser_token(parser)->constval.i;
        } else if (parser->tok == '{') {
            /* function body */
        } else {
            parseerror(parser, "TODO, const assignment");
        }

        if (!parser_next(parser))
            return false;

        if (parser->tok != ';') {
            parseerror(parser, "expected semicolon");
            return false;
        }

        return true;
    }
    else if (parser->tok == TOKEN_KEYWORD)
    {
        /* handle 'var' and 'const' */
        return false;
    }
    else if (parser->tok == '.')
    {
        /* entity-member declaration */
        return false;
    }
    else
    {
        parseerror(parser, "unexpected token: %s", parser->lex->tok->value);
        return false;
    }
    return true;
}

bool parser_compile(const char *filename)
{
    size_t i;
    parser_t *parser;
    ir_builder *ir;

    parser = (parser_t*)mem_a(sizeof(parser_t));
    if (!parser)
        return false;

    MEM_VECTOR_INIT(parser, globals);
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
            if (!parser_do(parser)) {
                if (parser->tok == TOKEN_EOF)
                    parseerror(parser, "unexpected eof");
                else
                    parseerror(parser, "parse error\n");
                lex_close(parser->lex);
                mem_d(parser);
                return false;
            }
        }
    }

    lex_close(parser->lex);

    ir = ir_builder_new("gmqcc_out");
    if (!ir) {
        printf("failed to allocate builder\n");
        goto cleanup;
    }

    for (i = 0; i < parser->globals_count; ++i) {
        if (!ast_global_codegen(parser->globals[i], ir)) {
            printf("failed to generate global %s\n", parser->globals[i]->name);
        }
    }
    for (i = 0; i < parser->functions_count; ++i) {
        if (!ast_function_codegen(parser->functions[i], ir)) {
            printf("failed to generate function %s\n", parser->functions[i]->name);
        }
        if (!ir_function_finalize(parser->functions[i]->ir_func)) {
            printf("failed to finalize function %s\n", parser->functions[i]->name);
        }
    }

    ir_builder_dump(ir, printf);

cleanup:
    for (i = 0; i < parser->globals_count; ++i) {
        ast_value_delete(parser->globals[i]);
    }
    MEM_VECTOR_CLEAR(parser, globals);

    mem_d(parser);
    return true;
}
