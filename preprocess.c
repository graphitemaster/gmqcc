#include "gmqcc.h"
#include "lexer.h"

bool preprocess(const char *filename)
{
    int tok;
    lex_file *lex = lex_open(filename);
    lex->flags.preprocessing = true;

    do {
        tok = lex_do(lex);
#if 0
        if (tok == TOKEN_EOL)
            printf("EOL");
        else if (tok >= TOKEN_START && tok <= TOKEN_FATAL)
            printf("%s: ", _tokennames[tok - TOKEN_START]);
        else
            printf("TOKEN: '%c'", tok);
        if (tok == TOKEN_WHITE)
            printf(">>%s<<\n", lex->tok.value);
        else
            printf("\n");
#else
        if (tok == TOKEN_EOL)
            printf("\n");
        else if (tok >= TOKEN_START && tok < TOKEN_EOF)
            printf("%s", lex->tok.value);
        else
            printf("%c", tok);
#endif
    } while (tok < TOKEN_EOF);

    lex_close(lex);
    return true;
}
