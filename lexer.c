#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "gmqcc.h"
#include "lexer.h"

MEM_VEC_FUNCTIONS(token, char, value)
MEM_VEC_FUNCTIONS(lex_file, frame_macro, frames)

VECTOR_MAKE(char*, lex_filenames);

void lexerror(lex_file *lex, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
    vprintmsg(LVL_ERROR, lex->name, lex->sline, "parse error", fmt, ap);
	va_end(ap);
}

bool lexwarn(lex_file *lex, int warntype, const char *fmt, ...)
{
	va_list ap;
	int lvl = LVL_WARNING;

    if (!OPTS_WARN(warntype))
        return false;

    if (opts_werror)
	    lvl = LVL_ERROR;

	va_start(ap, fmt);
    vprintmsg(lvl, lex->name, lex->sline, "warning", fmt, ap);
	va_end(ap);

	return opts_werror;
}


#if 0
token* token_new()
{
    token *tok = (token*)mem_a(sizeof(token));
    if (!tok)
        return NULL;
    memset(tok, 0, sizeof(*tok));
    return tok;
}

void token_delete(token *self)
{
    if (self->next && self->next->prev == self)
        self->next->prev = self->prev;
    if (self->prev && self->prev->next == self)
        self->prev->next = self->next;
    MEM_VECTOR_CLEAR(self, value);
    mem_d(self);
}

token* token_copy(const token *cp)
{
    token* self = token_new();
    if (!self)
        return NULL;
    /* copy the value */
    self->value_alloc = cp->value_count + 1;
    self->value_count = cp->value_count;
    self->value = (char*)mem_a(self->value_alloc);
    if (!self->value) {
        mem_d(self);
        return NULL;
    }
    memcpy(self->value, cp->value, cp->value_count);
    self->value[self->value_alloc-1] = 0;

    /* rest */
    self->ctx = cp->ctx;
    self->ttype = cp->ttype;
    memcpy(&self->constval, &cp->constval, sizeof(self->constval));
    return self;
}

void token_delete_all(token *t)
{
    token *n;

    do {
        n = t->next;
        token_delete(t);
        t = n;
    } while(t);
}

token* token_copy_all(const token *cp)
{
    token *cur;
    token *out;

    out = cur = token_copy(cp);
    if (!out)
        return NULL;

    while (cp->next) {
        cp = cp->next;
        cur->next = token_copy(cp);
        if (!cur->next) {
            token_delete_all(out);
            return NULL;
        }
        cur->next->prev = cur;
        cur = cur->next;
    }

    return out;
}
#else
static void lex_token_new(lex_file *lex)
{
#if 0
    if (lex->tok)
        token_delete(lex->tok);
    lex->tok = token_new();
#else
    lex->tok.value_count = 0;
    lex->tok.constval.t  = 0;
    lex->tok.ctx.line = lex->sline;
    lex->tok.ctx.file = lex->name;
#endif
}
#endif

lex_file* lex_open(const char *file)
{
    lex_file *lex;
    FILE *in = util_fopen(file, "rb");

    if (!in) {
        lexerror(NULL, "open failed: '%s'\n", file);
        return NULL;
    }

    lex = (lex_file*)mem_a(sizeof(*lex));
    if (!lex) {
        fclose(in);
        lexerror(NULL, "out of memory\n");
        return NULL;
    }

    memset(lex, 0, sizeof(*lex));

    lex->file = in;
    lex->name = util_strdup(file);
    lex->line = 1; /* we start counting at 1 */

    lex->peekpos = 0;
    lex->eof = false;

    lex_filenames_add(lex->name);

    return lex;
}

void lex_cleanup(void)
{
    size_t i;
    for (i = 0; i < lex_filenames_elements; ++i)
        mem_d(lex_filenames_data[i]);
    mem_d(lex_filenames_data);
}

void lex_close(lex_file *lex)
{
    size_t i;
    for (i = 0; i < lex->frames_count; ++i)
        mem_d(lex->frames[i].name);
    MEM_VECTOR_CLEAR(lex, frames);

    if (lex->modelname)
        mem_d(lex->modelname);

    if (lex->file)
        fclose(lex->file);
#if 0
    if (lex->tok)
        token_delete(lex->tok);
#else
    MEM_VECTOR_CLEAR(&(lex->tok), value);
#endif
    /* mem_d(lex->name); collected in lex_filenames */
    mem_d(lex);
}

/* Get or put-back data
 * The following to functions do NOT understand what kind of data they
 * are working on.
 * The are merely wrapping get/put in order to count line numbers.
 */
static void lex_ungetch(lex_file *lex, int ch);
static int lex_try_trigraph(lex_file *lex, int old)
{
    int c2, c3;
    c2 = fgetc(lex->file);
    if (c2 != '?') {
        lex_ungetch(lex, c2);
        return old;
    }

    c3 = fgetc(lex->file);
    switch (c3) {
        case '=': return '#';
        case '/': return '\\';
        case '\'': return '^';
        case '(': return '[';
        case ')': return ']';
        case '!': return '|';
        case '<': return '{';
        case '>': return '}';
        case '-': return '~';
        default:
            lex_ungetch(lex, c3);
            lex_ungetch(lex, c2);
            return old;
    }
}

static int lex_try_digraph(lex_file *lex, int ch)
{
    int c2;
    c2 = fgetc(lex->file);
    if      (ch == '<' && c2 == ':')
        return '[';
    else if (ch == ':' && c2 == '>')
        return ']';
    else if (ch == '<' && c2 == '%')
        return '{';
    else if (ch == '%' && c2 == '>')
        return '}';
    else if (ch == '%' && c2 == ':')
        return '#';
    lex_ungetch(lex, c2);
    return ch;
}

static int lex_getch(lex_file *lex)
{
    int ch;

    if (lex->peekpos) {
        lex->peekpos--;
        if (lex->peek[lex->peekpos] == '\n')
            lex->line++;
        return lex->peek[lex->peekpos];
    }

    ch = fgetc(lex->file);
    if (ch == '\n')
        lex->line++;
    else if (ch == '?')
        return lex_try_trigraph(lex, ch);
    else if (!lex->flags.nodigraphs && (ch == '<' || ch == ':' || ch == '%'))
        return lex_try_digraph(lex, ch);
    return ch;
}

static void lex_ungetch(lex_file *lex, int ch)
{
    lex->peek[lex->peekpos++] = ch;
    if (ch == '\n')
        lex->line--;
}

/* classify characters
 * some additions to the is*() functions of ctype.h
 */

/* Idents are alphanumberic, but they start with alpha or _ */
static bool isident_start(int ch)
{
    return isalpha(ch) || ch == '_';
}

static bool isident(int ch)
{
    return isident_start(ch) || isdigit(ch);
}

/* isxdigit_only is used when we already know it's not a digit
 * and want to see if it's a hex digit anyway.
 */
static bool isxdigit_only(int ch)
{
    return (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F');
}

/* Append a character to the token buffer */
static bool GMQCC_WARN lex_tokench(lex_file *lex, int ch)
{
    if (!token_value_add(&lex->tok, ch)) {
        lexerror(lex, "out of memory");
        return false;
    }
    return true;
}

/* Append a trailing null-byte */
static bool GMQCC_WARN lex_endtoken(lex_file *lex)
{
    if (!token_value_add(&lex->tok, 0)) {
        lexerror(lex, "out of memory");
        return false;
    }
    lex->tok.value_count--;
    return true;
}

/* Skip whitespace and comments and return the first
 * non-white character.
 * As this makes use of the above getch() ungetch() functions,
 * we don't need to care at all about line numbering anymore.
 *
 * In theory, this function should only be used at the beginning
 * of lexing, or when we *know* the next character is part of the token.
 * Otherwise, if the parser throws an error, the linenumber may not be
 * the line of the error, but the line of the next token AFTER the error.
 *
 * This is currently only problematic when using c-like string-continuation,
 * since comments and whitespaces are allowed between 2 such strings.
 * Example:
printf(   "line one\n"
// A comment
          "A continuation of the previous string"
// This line is skipped
      , foo);

 * In this case, if the parse decides it didn't actually want a string,
 * and uses lex->line to print an error, it will show the ', foo);' line's
 * linenumber.
 *
 * On the other hand, the parser is supposed to remember the line of the next
 * token's beginning. In this case we would want skipwhite() to be called
 * AFTER reading a token, so that the parser, before reading the NEXT token,
 * doesn't store teh *comment's* linenumber, but the actual token's linenumber.
 *
 * THIS SOLUTION
 *    here is to store the line of the first character after skipping
 *    the initial whitespace in lex->sline, this happens in lex_do.
 */
static int lex_skipwhite(lex_file *lex)
{
    int ch = 0;
    bool haswhite = false;

    do
    {
        ch = lex_getch(lex);
        while (ch != EOF && isspace(ch)) {
            if (lex->flags.preprocessing) {
                if (ch == '\n') {
                    /* end-of-line */
                    /* see if there was whitespace first */
                    if (haswhite) { /* (lex->tok.value_count) { */
                        lex_ungetch(lex, ch);
                        if (!lex_endtoken(lex))
                            return TOKEN_FATAL;
                        return TOKEN_WHITE;
                    }
                    /* otherwise return EOL */
                    return TOKEN_EOL;
                }
                haswhite = true;
                if (!lex_tokench(lex, ch))
                    return TOKEN_FATAL;
            }
            ch = lex_getch(lex);
        }

        if (ch == '/') {
            ch = lex_getch(lex);
            if (ch == '/')
            {
                /* one line comment */
                ch = lex_getch(lex);

                if (lex->flags.preprocessing) {
                    haswhite = true;
                    if (!lex_tokench(lex, '/') ||
                        !lex_tokench(lex, '/'))
                    {
                        return TOKEN_FATAL;
                    }
                }

                while (ch != EOF && ch != '\n') {
                    if (lex->flags.preprocessing && !lex_tokench(lex, ch))
                        return TOKEN_FATAL;
                    ch = lex_getch(lex);
                }
                if (lex->flags.preprocessing) {
                    lex_ungetch(lex, '\n');
                    if (!lex_endtoken(lex))
                        return TOKEN_FATAL;
                    return TOKEN_WHITE;
                }
                continue;
            }
            if (ch == '*')
            {
                /* multiline comment */
                if (lex->flags.preprocessing) {
                    haswhite = true;
                    if (!lex_tokench(lex, '/') ||
                        !lex_tokench(lex, '*'))
                    {
                        return TOKEN_FATAL;
                    }
                }

                while (ch != EOF)
                {
                    ch = lex_getch(lex);
                    if (ch == '*') {
                        ch = lex_getch(lex);
                        if (ch == '/') {
                            if (lex->flags.preprocessing) {
                                if (!lex_tokench(lex, '*') ||
                                    !lex_tokench(lex, '/'))
                                {
                                    return TOKEN_FATAL;
                                }
                            }
                            break;
                        }
                    }
                    if (lex->flags.preprocessing) {
                        if (!lex_tokench(lex, ch))
                            return TOKEN_FATAL;
                    }
                }
                ch = ' '; /* cause TRUE in the isspace check */
                continue;
            }
            /* Otherwise roll back to the slash and break out of the loop */
            lex_ungetch(lex, ch);
            ch = '/';
            break;
        }
    } while (ch != EOF && isspace(ch));

    if (haswhite) {
        if (!lex_endtoken(lex))
            return TOKEN_FATAL;
        lex_ungetch(lex, ch);
        return TOKEN_WHITE;
    }
    return ch;
}

/* Get a token */
static bool GMQCC_WARN lex_finish_ident(lex_file *lex)
{
    int ch;

    ch = lex_getch(lex);
    while (ch != EOF && isident(ch))
    {
        if (!lex_tokench(lex, ch))
            return (lex->tok.ttype = TOKEN_FATAL);
        ch = lex_getch(lex);
    }

    /* last ch was not an ident ch: */
    lex_ungetch(lex, ch);

    return true;
}

/* read one ident for the frame list */
static int lex_parse_frame(lex_file *lex)
{
    int ch;

    lex_token_new(lex);

    ch = lex_getch(lex);
    while (ch != EOF && ch != '\n' && isspace(ch))
        ch = lex_getch(lex);

    if (ch == '\n')
        return 1;

    if (!isident_start(ch)) {
        lexerror(lex, "invalid framename, must start with one of a-z or _, got %c", ch);
        return -1;
    }

    if (!lex_tokench(lex, ch))
        return -1;
    if (!lex_finish_ident(lex))
        return -1;
    if (!lex_endtoken(lex))
        return -1;
    return 0;
}

/* read a list of $frames */
static bool lex_finish_frames(lex_file *lex)
{
    do {
        size_t i;
        int    rc;
        frame_macro m;

        rc = lex_parse_frame(lex);
        if (rc > 0) /* end of line */
            return true;
        if (rc < 0) /* error */
            return false;

        for (i = 0; i < lex->frames_count; ++i) {
            if (!strcmp(lex->tok.value, lex->frames[i].name)) {
                lex->frames[i].value = lex->framevalue++;
                if (lexwarn(lex, WARN_FRAME_MACROS, "duplicate frame macro defined: `%s`", lex->tok.value))
                    return false;
                break;
            }
        }
        if (i < lex->frames_count)
            continue;

        m.value = lex->framevalue++;
        m.name = lex->tok.value;
        lex->tok.value = NULL;
        lex->tok.value_alloc = lex->tok.value_count = 0;
        if (!lex_file_frames_add(lex, m)) {
            lexerror(lex, "out of memory");
            return false;
        }
    } while (true);
}

static int GMQCC_WARN lex_finish_string(lex_file *lex, int quote)
{
    int ch = 0;

    while (ch != EOF)
    {
        ch = lex_getch(lex);
        if (ch == quote)
            return TOKEN_STRINGCONST;

        if (!lex->flags.preprocessing && ch == '\\') {
            ch = lex_getch(lex);
            if (ch == EOF) {
                lexerror(lex, "unexpected end of file");
                lex_ungetch(lex, EOF); /* next token to be TOKEN_EOF */
                return (lex->tok.ttype = TOKEN_ERROR);
            }

            switch (ch) {
            case '\\': break;
            case 'a':  ch = '\a'; break;
            case 'b':  ch = '\b'; break;
            case 'r':  ch = '\r'; break;
            case 'n':  ch = '\n'; break;
            case 't':  ch = '\t'; break;
            case 'f':  ch = '\f'; break;
            case 'v':  ch = '\v'; break;
            default:
                lexwarn(lex, WARN_UNKNOWN_CONTROL_SEQUENCE, "unrecognized control sequence: \\%c", ch);
                /* so we just add the character plus backslash no matter what it actually is */
                if (!lex_tokench(lex, '\\'))
                    return (lex->tok.ttype = TOKEN_FATAL);
            }
            /* add the character finally */
            if (!lex_tokench(lex, ch))
                return (lex->tok.ttype = TOKEN_FATAL);
        }
        else if (!lex_tokench(lex, ch))
            return (lex->tok.ttype = TOKEN_FATAL);
    }
    lexerror(lex, "unexpected end of file within string constant");
    lex_ungetch(lex, EOF); /* next token to be TOKEN_EOF */
    return (lex->tok.ttype = TOKEN_ERROR);
}

static int GMQCC_WARN lex_finish_digit(lex_file *lex, int lastch)
{
    bool ishex = false;

    int  ch = lastch;

    /* parse a number... */
    lex->tok.ttype = TOKEN_INTCONST;

    if (!lex_tokench(lex, ch))
        return (lex->tok.ttype = TOKEN_FATAL);

    ch = lex_getch(lex);
    if (ch != '.' && !isdigit(ch))
    {
        if (lastch != '0' || ch != 'x')
        {
            /* end of the number or EOF */
            lex_ungetch(lex, ch);
            if (!lex_endtoken(lex))
                return (lex->tok.ttype = TOKEN_FATAL);

            lex->tok.constval.i = lastch - '0';
            return lex->tok.ttype;
        }

        ishex = true;
    }

    /* EOF would have been caught above */

    if (ch != '.')
    {
        if (!lex_tokench(lex, ch))
            return (lex->tok.ttype = TOKEN_FATAL);
        ch = lex_getch(lex);
        while (isdigit(ch) || (ishex && isxdigit_only(ch)))
        {
            if (!lex_tokench(lex, ch))
                return (lex->tok.ttype = TOKEN_FATAL);
            ch = lex_getch(lex);
        }
    }
    /* NOT else, '.' can come from above as well */
    if (ch == '.' && !ishex)
    {
        /* Allow floating comma in non-hex mode */
        lex->tok.ttype = TOKEN_FLOATCONST;
        if (!lex_tokench(lex, ch))
            return (lex->tok.ttype = TOKEN_FATAL);

        /* continue digits-only */
        ch = lex_getch(lex);
        while (isdigit(ch))
        {
            if (!lex_tokench(lex, ch))
                return (lex->tok.ttype = TOKEN_FATAL);
            ch = lex_getch(lex);
        }
    }
    /* put back the last character */
    /* but do not put back the trailing 'f' or a float */
    if (lex->tok.ttype == TOKEN_FLOATCONST && ch == 'f')
        ch = lex_getch(lex);

    /* generally we don't want words to follow numbers: */
    if (isident(ch)) {
        lexerror(lex, "unexpected trailing characters after number");
        return (lex->tok.ttype = TOKEN_ERROR);
    }
    lex_ungetch(lex, ch);

    if (!lex_endtoken(lex))
        return (lex->tok.ttype = TOKEN_FATAL);
    if (lex->tok.ttype == TOKEN_FLOATCONST)
        lex->tok.constval.f = strtod(lex->tok.value, NULL);
    else
        lex->tok.constval.i = strtol(lex->tok.value, NULL, 0);
    return lex->tok.ttype;
}

int lex_do(lex_file *lex)
{
    int ch, nextch;

    lex_token_new(lex);
#if 0
    if (!lex->tok)
        return TOKEN_FATAL;
#endif

    ch = lex_skipwhite(lex);
    lex->sline = lex->line;
    lex->tok.ctx.line = lex->sline;
    lex->tok.ctx.file = lex->name;

    if (lex->flags.preprocessing && (ch == TOKEN_WHITE || ch == TOKEN_EOL || ch == TOKEN_FATAL)) {
        return (lex->tok.ttype = ch);
    }

    if (lex->eof)
        return (lex->tok.ttype = TOKEN_FATAL);

    if (ch == EOF) {
        lex->eof = true;
        return (lex->tok.ttype = TOKEN_EOF);
    }

    /* modelgen / spiritgen commands */
    if (ch == '$') {
        const char *v;
        size_t frame;

        ch = lex_getch(lex);
        if (!isident_start(ch)) {
            lexerror(lex, "hanging '$' modelgen/spritegen command line");
            return lex_do(lex);
        }
        if (!lex_tokench(lex, ch))
            return (lex->tok.ttype = TOKEN_FATAL);
        if (!lex_finish_ident(lex))
            return (lex->tok.ttype = TOKEN_ERROR);
        if (!lex_endtoken(lex))
            return (lex->tok.ttype = TOKEN_FATAL);
        /* skip the known commands */
        v = lex->tok.value;

        if (!strcmp(v, "frame") || !strcmp(v, "framesave"))
        {
            /* frame/framesave command works like an enum
             * similar to fteqcc we handle this in the lexer.
             * The reason for this is that it is sensitive to newlines,
             * which the parser is unaware of
             */
            if (!lex_finish_frames(lex))
                 return (lex->tok.ttype = TOKEN_ERROR);
            return lex_do(lex);
        }

        if (!strcmp(v, "framevalue"))
        {
            ch = lex_getch(lex);
            while (ch != EOF && isspace(ch) && ch != '\n')
                ch = lex_getch(lex);

            if (!isdigit(ch)) {
                lexerror(lex, "$framevalue requires an integer parameter");
                return lex_do(lex);
            }

            lex_token_new(lex);
            lex->tok.ttype = lex_finish_digit(lex, ch);
            if (!lex_endtoken(lex))
                return (lex->tok.ttype = TOKEN_FATAL);
            if (lex->tok.ttype != TOKEN_INTCONST) {
                lexerror(lex, "$framevalue requires an integer parameter");
                return lex_do(lex);
            }
            lex->framevalue = lex->tok.constval.i;
            return lex_do(lex);
        }

        if (!strcmp(v, "framerestore"))
        {
            int rc;

            lex_token_new(lex);

            rc = lex_parse_frame(lex);

            if (rc > 0) {
                lexerror(lex, "$framerestore requires a framename parameter");
                return lex_do(lex);
            }
            if (rc < 0)
                return (lex->tok.ttype = TOKEN_FATAL);

            v = lex->tok.value;
            for (frame = 0; frame < lex->frames_count; ++frame) {
                if (!strcmp(v, lex->frames[frame].name)) {
                    lex->framevalue = lex->frames[frame].value;
                    return lex_do(lex);
                }
            }
            lexerror(lex, "unknown framename `%s`", v);
            return lex_do(lex);
        }

        if (!strcmp(v, "modelname"))
        {
            int rc;

            lex_token_new(lex);

            rc = lex_parse_frame(lex);

            if (rc > 0) {
                lexerror(lex, "$framerestore requires a framename parameter");
                return lex_do(lex);
            }
            if (rc < 0)
                return (lex->tok.ttype = TOKEN_FATAL);

            v = lex->tok.value;
            if (lex->modelname) {
                frame_macro m;
                m.value = lex->framevalue;
                m.name = lex->modelname;
                lex->modelname = NULL;
                if (!lex_file_frames_add(lex, m)) {
                    lexerror(lex, "out of memory");
                    return (lex->tok.ttype = TOKEN_FATAL);
                }
            }
            lex->modelname = lex->tok.value;
            lex->tok.value = NULL;
            lex->tok.value_alloc = lex->tok.value_count = 0;
            for (frame = 0; frame < lex->frames_count; ++frame) {
                if (!strcmp(v, lex->frames[frame].name)) {
                    lex->framevalue = lex->frames[frame].value;
                    break;
                }
            }
            return lex_do(lex);
        }

        if (!strcmp(v, "flush"))
        {
            size_t frame;
            for (frame = 0; frame < lex->frames_count; ++frame)
                mem_d(lex->frames[frame].name);
            MEM_VECTOR_CLEAR(lex, frames);
            /* skip line (fteqcc does it too) */
            ch = lex_getch(lex);
            while (ch != EOF && ch != '\n')
                ch = lex_getch(lex);
            return lex_do(lex);
        }

        if (!strcmp(v, "cd") ||
            !strcmp(v, "origin") ||
            !strcmp(v, "base") ||
            !strcmp(v, "flags") ||
            !strcmp(v, "scale") ||
            !strcmp(v, "skin"))
        {
            /* skip line */
            ch = lex_getch(lex);
            while (ch != EOF && ch != '\n')
                ch = lex_getch(lex);
            return lex_do(lex);
        }

        for (frame = 0; frame < lex->frames_count; ++frame) {
            if (!strcmp(v, lex->frames[frame].name)) {
                lex->tok.constval.i = lex->frames[frame].value;
                return (lex->tok.ttype = TOKEN_INTCONST);
            }
        }

        lexerror(lex, "invalid frame macro");
        return lex_do(lex);
    }

    /* single-character tokens */
    switch (ch)
    {
        case '(':
            if (!lex_tokench(lex, ch) ||
                !lex_endtoken(lex))
            {
                return (lex->tok.ttype = TOKEN_FATAL);
            }
            if (lex->flags.noops)
                return (lex->tok.ttype = ch);
            else
                return (lex->tok.ttype = TOKEN_OPERATOR);
        case ')':
        case ';':
        case '{':
        case '}':
        case '[':
        case ']':

        case '#':
            if (!lex_tokench(lex, ch) ||
                !lex_endtoken(lex))
            {
                return (lex->tok.ttype = TOKEN_FATAL);
            }
            return (lex->tok.ttype = ch);
        default:
            break;
    }

    if (lex->flags.noops)
    {
        /* Detect characters early which are normally
         * operators OR PART of an operator.
         */
        switch (ch)
        {
            case '+':
            case '-':
            case '*':
            case '/':
            case '<':
            case '>':
            case '=':
            case '&':
            case '|':
            case '^':
            case '~':
            case ',':
            case '!':
                if (!lex_tokench(lex, ch) ||
                    !lex_endtoken(lex))
                {
                    return (lex->tok.ttype = TOKEN_FATAL);
                }
                return (lex->tok.ttype = ch);
            default:
                break;
        }

        if (ch == '.')
        {
            if (!lex_tokench(lex, ch))
                return (lex->tok.ttype = TOKEN_FATAL);
            /* peak ahead once */
            nextch = lex_getch(lex);
            if (nextch != '.') {
                lex_ungetch(lex, nextch);
                if (!lex_endtoken(lex))
                    return (lex->tok.ttype = TOKEN_FATAL);
                return (lex->tok.ttype = ch);
            }
            /* peak ahead again */
            nextch = lex_getch(lex);
            if (nextch != '.') {
                lex_ungetch(lex, nextch);
                lex_ungetch(lex, nextch);
                if (!lex_endtoken(lex))
                    return (lex->tok.ttype = TOKEN_FATAL);
                return (lex->tok.ttype = ch);
            }
            /* fill the token to be "..." */
            if (!lex_tokench(lex, ch) ||
                !lex_tokench(lex, ch) ||
                !lex_endtoken(lex))
            {
                return (lex->tok.ttype = TOKEN_FATAL);
            }
            return (lex->tok.ttype = TOKEN_DOTS);
        }
    }

    if (ch == ',' || ch == '.') {
        if (!lex_tokench(lex, ch) ||
            !lex_endtoken(lex))
        {
            return (lex->tok.ttype = TOKEN_FATAL);
        }
        return (lex->tok.ttype = TOKEN_OPERATOR);
    }

    if (ch == '+' || ch == '-' || /* ++, --, +=, -=  and -> as well! */
        ch == '>' || ch == '<' || /* <<, >>, <=, >= */
        ch == '=' || ch == '!' || /* ==, != */
        ch == '&' || ch == '|')   /* &&, ||, &=, |= */
    {
        if (!lex_tokench(lex, ch))
            return (lex->tok.ttype = TOKEN_FATAL);

        nextch = lex_getch(lex);
        if (nextch == ch || nextch == '=') {
            if (!lex_tokench(lex, nextch))
                return (lex->tok.ttype = TOKEN_FATAL);
        } else if (ch == '-' && nextch == '>') {
            if (!lex_tokench(lex, nextch))
                return (lex->tok.ttype = TOKEN_FATAL);
        } else
            lex_ungetch(lex, nextch);

        if (!lex_endtoken(lex))
            return (lex->tok.ttype = TOKEN_FATAL);
        return (lex->tok.ttype = TOKEN_OPERATOR);
    }

    /*
    if (ch == '^' || ch == '~' || ch == '!')
    {
        if (!lex_tokench(lex, ch) ||
            !lex_endtoken(lex))
        {
            return (lex->tok.ttype = TOKEN_FATAL);
        }
        return (lex->tok.ttype = TOKEN_OPERATOR);
    }
    */

    if (ch == '*' || ch == '/') /* *=, /= */
    {
        if (!lex_tokench(lex, ch))
            return (lex->tok.ttype = TOKEN_FATAL);

        nextch = lex_getch(lex);
        if (nextch == '=') {
            if (!lex_tokench(lex, nextch))
                return (lex->tok.ttype = TOKEN_FATAL);
        } else
            lex_ungetch(lex, nextch);

        if (!lex_endtoken(lex))
            return (lex->tok.ttype = TOKEN_FATAL);
        return (lex->tok.ttype = TOKEN_OPERATOR);
    }

    if (isident_start(ch))
    {
        const char *v;

        if (!lex_tokench(lex, ch))
            return (lex->tok.ttype = TOKEN_FATAL);
        if (!lex_finish_ident(lex)) {
            /* error? */
            return (lex->tok.ttype = TOKEN_ERROR);
        }
        if (!lex_endtoken(lex))
            return (lex->tok.ttype = TOKEN_FATAL);
        lex->tok.ttype = TOKEN_IDENT;

        v = lex->tok.value;
        if (!strcmp(v, "void")) {
            lex->tok.ttype = TOKEN_TYPENAME;
            lex->tok.constval.t = TYPE_VOID;
        } else if (!strcmp(v, "int")) {
            lex->tok.ttype = TOKEN_TYPENAME;
            lex->tok.constval.t = TYPE_INTEGER;
        } else if (!strcmp(v, "float")) {
            lex->tok.ttype = TOKEN_TYPENAME;
            lex->tok.constval.t = TYPE_FLOAT;
        } else if (!strcmp(v, "string")) {
            lex->tok.ttype = TOKEN_TYPENAME;
            lex->tok.constval.t = TYPE_STRING;
        } else if (!strcmp(v, "entity")) {
            lex->tok.ttype = TOKEN_TYPENAME;
            lex->tok.constval.t = TYPE_ENTITY;
        } else if (!strcmp(v, "vector")) {
            lex->tok.ttype = TOKEN_TYPENAME;
            lex->tok.constval.t = TYPE_VECTOR;
        } else if (!strcmp(v, "for")  ||
                 !strcmp(v, "while")  ||
                 !strcmp(v, "do")     ||
                 !strcmp(v, "if")     ||
                 !strcmp(v, "else")   ||
                 !strcmp(v, "local")  ||
                 !strcmp(v, "return") ||
                 !strcmp(v, "const"))
        {
            lex->tok.ttype = TOKEN_KEYWORD;
        }
        else if (opts_standard != COMPILER_QCC)
        {
            /* other standards reserve these keywords */
            if (!strcmp(v, "switch") ||
                !strcmp(v, "struct") ||
                !strcmp(v, "union")  ||
                !strcmp(v, "break")  ||
                !strcmp(v, "continue"))
            {
                lex->tok.ttype = TOKEN_KEYWORD;
            }
        }

        return lex->tok.ttype;
    }

    if (ch == '"')
    {
        lex->flags.nodigraphs = true;
        if (lex->flags.preprocessing && !lex_tokench(lex, ch))
            return TOKEN_FATAL;
        lex->tok.ttype = lex_finish_string(lex, '"');
        if (lex->flags.preprocessing && !lex_tokench(lex, ch))
            return TOKEN_FATAL;
        while (!lex->flags.preprocessing && lex->tok.ttype == TOKEN_STRINGCONST)
        {
            /* Allow c style "string" "continuation" */
            ch = lex_skipwhite(lex);
            if (ch != '"') {
                lex_ungetch(lex, ch);
                break;
            }

            lex->tok.ttype = lex_finish_string(lex, '"');
        }
        lex->flags.nodigraphs = false;
        if (!lex_endtoken(lex))
            return (lex->tok.ttype = TOKEN_FATAL);
        return lex->tok.ttype;
    }

    if (ch == '\'')
    {
        /* we parse character constants like string,
         * but return TOKEN_CHARCONST, or a vector type if it fits...
         * Likewise actual unescaping has to be done by the parser.
         * The difference is we don't allow 'char' 'continuation'.
         */
        if (lex->flags.preprocessing && !lex_tokench(lex, ch))
            return TOKEN_FATAL;
        lex->tok.ttype = lex_finish_string(lex, '\'');
        if (lex->flags.preprocessing && !lex_tokench(lex, ch))
            return TOKEN_FATAL;
        if (!lex_endtoken(lex))
            return (lex->tok.ttype = TOKEN_FATAL);

         /* It's a vector if we can successfully scan 3 floats */
#ifdef WIN32
        if (sscanf_s(lex->tok.value, " %f %f %f ",
                   &lex->tok.constval.v.x, &lex->tok.constval.v.y, &lex->tok.constval.v.z) == 3)
#else
        if (sscanf(lex->tok.value, " %f %f %f ",
                   &lex->tok.constval.v.x, &lex->tok.constval.v.y, &lex->tok.constval.v.z) == 3)
#endif
        {
             lex->tok.ttype = TOKEN_VECTORCONST;
        }

        return lex->tok.ttype;
    }

    if (isdigit(ch))
    {
        lex->tok.ttype = lex_finish_digit(lex, ch);
        if (!lex_endtoken(lex))
            return (lex->tok.ttype = TOKEN_FATAL);
        return lex->tok.ttype;
    }

    lexerror(lex, "unknown token");
    return (lex->tok.ttype = TOKEN_ERROR);
}
