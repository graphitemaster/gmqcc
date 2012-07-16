#include "gmqcc.h"
#include "lexer.h"

typedef struct {
    lex_file *lex;
    int      tok;
} parser_t;

bool parser_do(parser_t *parser)
{
    return true;
}

bool parser_compile(const char *filename)
{
    parser_t *parser;

    parser = (parser_t*)mem_a(sizeof(parser_t));
    if (!parser)
        return false;

    parser->lex = lex_open(filename);

    if (!parser->lex) {
        printf("failed to open file \"%s\"\n", filename);
        return false;
    }

    for (parser->tok = lex_do(parser->lex);
         parser->tok != TOKEN_EOF && parser->tok < TOKEN_ERROR;
         parser->tok = lex_do(parser->lex))
    {
        if (!parser_do(parser)) {
            printf("parse error\n");
            lex_close(parser->lex);
            mem_d(parser);
            return false;
        }
    }

    lex_close(parser->lex);
    mem_d(parser);
    return true;
}
