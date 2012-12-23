/*
 * Copyright (C) 2012
 *     Wolfgang Bumiller
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
#include "gmqcc.h"
#include "lexer.h"

typedef struct {
    bool on;
    bool was_on;
    bool had_else;
} ppcondition;

typedef struct {
    int   token;
    char *value;
    /* a copy from the lexer */
    union {
        vector v;
        int    i;
        double f;
        int    t; /* type */
    } constval;
} pptoken;

typedef struct {
    lex_ctx ctx;

    char   *name;
    char  **params;
    /* yes we need an extra flag since `#define FOO x` is not the same as `#define FOO() x` */
    bool    has_params;

    pptoken **output;
} ppmacro;

typedef struct {
    lex_file    *lex;
    int          token;
    unsigned int errors;

    bool         output_on;
    ppcondition *conditions;
    ppmacro    **macros;

    char        *output_string;

    char        *itemname;
    char        *includename;
} ftepp_t;

#define ftepp_tokval(f) ((f)->lex->tok.value)
#define ftepp_ctx(f)    ((f)->lex->tok.ctx)

static void ftepp_errorat(ftepp_t *ftepp, lex_ctx ctx, const char *fmt, ...)
{
    va_list ap;

    ftepp->errors++;

    va_start(ap, fmt);
    con_cvprintmsg((void*)&ctx, LVL_ERROR, "error", fmt, ap);
    va_end(ap);
}

static void ftepp_error(ftepp_t *ftepp, const char *fmt, ...)
{
    va_list ap;

    ftepp->errors++;

    va_start(ap, fmt);
    con_cvprintmsg((void*)&ftepp->lex->tok.ctx, LVL_ERROR, "error", fmt, ap);
    va_end(ap);
}

static bool GMQCC_WARN ftepp_warn(ftepp_t *ftepp, int warntype, const char *fmt, ...)
{
    bool    r;
    va_list ap;

    va_start(ap, fmt);
    r = vcompile_warning(ftepp->lex->tok.ctx, warntype, fmt, ap);
    va_end(ap);
    return r;
}

static pptoken *pptoken_make(ftepp_t *ftepp)
{
    pptoken *token = (pptoken*)mem_a(sizeof(pptoken));
    token->token = ftepp->token;
#if 0
    if (token->token == TOKEN_WHITE)
        token->value = util_strdup(" ");
    else
#else
        token->value = util_strdup(ftepp_tokval(ftepp));
#endif
    memcpy(&token->constval, &ftepp->lex->tok.constval, sizeof(token->constval));
    return token;
}

static void pptoken_delete(pptoken *self)
{
    mem_d(self->value);
    mem_d(self);
}

static ppmacro *ppmacro_new(lex_ctx ctx, const char *name)
{
    ppmacro *macro = (ppmacro*)mem_a(sizeof(ppmacro));

    (void)ctx;
    memset(macro, 0, sizeof(*macro));
    macro->name = util_strdup(name);
    return macro;
}

static void ppmacro_delete(ppmacro *self)
{
    size_t i;
    for (i = 0; i < vec_size(self->params); ++i)
        mem_d(self->params[i]);
    vec_free(self->params);
    for (i = 0; i < vec_size(self->output); ++i)
        pptoken_delete(self->output[i]);
    vec_free(self->output);
    mem_d(self->name);
    mem_d(self);
}

static ftepp_t* ftepp_new()
{
    ftepp_t *ftepp;

    ftepp = (ftepp_t*)mem_a(sizeof(*ftepp));
    memset(ftepp, 0, sizeof(*ftepp));

    ftepp->output_on = true;

    return ftepp;
}

static void ftepp_delete(ftepp_t *self)
{
    size_t i;
    if (self->itemname)
        mem_d(self->itemname);
    if (self->includename)
        vec_free(self->includename);
    for (i = 0; i < vec_size(self->macros); ++i)
        ppmacro_delete(self->macros[i]);
    vec_free(self->macros);
    vec_free(self->conditions);
    if (self->lex)
        lex_close(self->lex);
    mem_d(self);
}

static void ftepp_out(ftepp_t *ftepp, const char *str, bool ignore_cond)
{
    if (ignore_cond || ftepp->output_on)
    {
        size_t len;
        char  *data;
        len = strlen(str);
        data = vec_add(ftepp->output_string, len);
        memcpy(data, str, len);
    }
}

static void ftepp_update_output_condition(ftepp_t *ftepp)
{
    size_t i;
    ftepp->output_on = true;
    for (i = 0; i < vec_size(ftepp->conditions); ++i)
        ftepp->output_on = ftepp->output_on && ftepp->conditions[i].on;
}

static ppmacro* ftepp_macro_find(ftepp_t *ftepp, const char *name)
{
    size_t i;
    for (i = 0; i < vec_size(ftepp->macros); ++i) {
        if (!strcmp(name, ftepp->macros[i]->name))
            return ftepp->macros[i];
    }
    return NULL;
}

static void ftepp_macro_delete(ftepp_t *ftepp, const char *name)
{
    size_t i;
    for (i = 0; i < vec_size(ftepp->macros); ++i) {
        if (!strcmp(name, ftepp->macros[i]->name)) {
            vec_remove(ftepp->macros, i, 1);
            return;
        }
    }
}

static GMQCC_INLINE int ftepp_next(ftepp_t *ftepp)
{
    return (ftepp->token = lex_do(ftepp->lex));
}

/* Important: this does not skip newlines! */
static bool ftepp_skipspace(ftepp_t *ftepp)
{
    if (ftepp->token != TOKEN_WHITE)
        return true;
    while (ftepp_next(ftepp) == TOKEN_WHITE) {}
    if (ftepp->token >= TOKEN_EOF) {
        ftepp_error(ftepp, "unexpected end of preprocessor directive");
        return false;
    }
    return true;
}

/* this one skips EOLs as well */
static bool ftepp_skipallwhite(ftepp_t *ftepp)
{
    if (ftepp->token != TOKEN_WHITE && ftepp->token != TOKEN_EOL)
        return true;
    do {
        ftepp_next(ftepp);
    } while (ftepp->token == TOKEN_WHITE || ftepp->token == TOKEN_EOL);
    if (ftepp->token >= TOKEN_EOF) {
        ftepp_error(ftepp, "unexpected end of preprocessor directive");
        return false;
    }
    return true;
}

/**
 * The huge macro parsing code...
 */
static bool ftepp_define_params(ftepp_t *ftepp, ppmacro *macro)
{
    do {
        ftepp_next(ftepp);
        if (!ftepp_skipspace(ftepp))
            return false;
        if (ftepp->token == ')')
            break;
        switch (ftepp->token) {
            case TOKEN_IDENT:
            case TOKEN_TYPENAME:
            case TOKEN_KEYWORD:
                break;
            default:
                ftepp_error(ftepp, "unexpected token in parameter list");
                return false;
        }
        vec_push(macro->params, util_strdup(ftepp_tokval(ftepp)));
        ftepp_next(ftepp);
        if (!ftepp_skipspace(ftepp))
            return false;
    } while (ftepp->token == ',');
    if (ftepp->token != ')') {
        ftepp_error(ftepp, "expected closing paren after macro parameter list");
        return false;
    }
    ftepp_next(ftepp);
    /* skipspace happens in ftepp_define */
    return true;
}

static bool ftepp_define_body(ftepp_t *ftepp, ppmacro *macro)
{
    pptoken *ptok;
    while (ftepp->token != TOKEN_EOL && ftepp->token < TOKEN_EOF) {
        ptok = pptoken_make(ftepp);
        vec_push(macro->output, ptok);
        ftepp_next(ftepp);
    }
    /* recursive expansion can cause EOFs here */
    if (ftepp->token != TOKEN_EOL && ftepp->token != TOKEN_EOF) {
        ftepp_error(ftepp, "unexpected junk after macro or unexpected end of file");
        return false;
    }
    return true;
}

static bool ftepp_define(ftepp_t *ftepp)
{
    ppmacro *macro;
    size_t l = ftepp_ctx(ftepp).line;

    (void)ftepp_next(ftepp);
    if (!ftepp_skipspace(ftepp))
        return false;

    switch (ftepp->token) {
        case TOKEN_IDENT:
        case TOKEN_TYPENAME:
        case TOKEN_KEYWORD:
            macro = ftepp_macro_find(ftepp, ftepp_tokval(ftepp));
            if (macro && ftepp->output_on) {
                if (ftepp_warn(ftepp, WARN_PREPROCESSOR, "redefining `%s`", ftepp_tokval(ftepp)))
                    return false;
                ftepp_macro_delete(ftepp, ftepp_tokval(ftepp));
            }
            macro = ppmacro_new(ftepp_ctx(ftepp), ftepp_tokval(ftepp));
            break;
        default:
            ftepp_error(ftepp, "expected macro name");
            return false;
    }

    (void)ftepp_next(ftepp);

    if (ftepp->token == '(') {
        macro->has_params = true;
        if (!ftepp_define_params(ftepp, macro))
            return false;
    }

    if (!ftepp_skipspace(ftepp))
        return false;

    if (!ftepp_define_body(ftepp, macro))
        return false;

    if (ftepp->output_on)
        vec_push(ftepp->macros, macro);
    else {
        ppmacro_delete(macro);
    }

    for (; l < ftepp_ctx(ftepp).line; ++l)
        ftepp_out(ftepp, "\n", true);
    return true;
}

/**
 * When a macro is used we have to handle parameters as well
 * as special-concatenation via ## or stringification via #
 *
 * Note: parenthesis can nest, so FOO((a),b) is valid, but only
 * this kind of parens. Curly braces or [] don't count towards the
 * paren-level.
 */
typedef struct {
    pptoken **tokens;
} macroparam;

static void macroparam_clean(macroparam *self)
{
    size_t i;
    for (i = 0; i < vec_size(self->tokens); ++i)
        pptoken_delete(self->tokens[i]);
    vec_free(self->tokens);
}

/* need to leave the last token up */
static bool ftepp_macro_call_params(ftepp_t *ftepp, macroparam **out_params)
{
    macroparam *params = NULL;
    pptoken    *ptok;
    macroparam  mp;
    size_t      parens = 0;
    size_t      i;

    if (!ftepp_skipallwhite(ftepp))
        return false;
    while (ftepp->token != ')') {
        mp.tokens = NULL;
        if (!ftepp_skipallwhite(ftepp))
            return false;
        while (parens || ftepp->token != ',') {
            if (ftepp->token == '(')
                ++parens;
            else if (ftepp->token == ')') {
                if (!parens)
                    break;
                --parens;
            }
            ptok = pptoken_make(ftepp);
            vec_push(mp.tokens, ptok);
            if (ftepp_next(ftepp) >= TOKEN_EOF) {
                ftepp_error(ftepp, "unexpected EOF in macro call");
                goto on_error;
            }
        }
        vec_push(params, mp);
        mp.tokens = NULL;
        if (ftepp->token == ')')
            break;
        if (ftepp->token != ',') {
            ftepp_error(ftepp, "expected closing paren or comma in macro call");
            goto on_error;
        }
        if (ftepp_next(ftepp) >= TOKEN_EOF) {
            ftepp_error(ftepp, "unexpected EOF in macro call");
            goto on_error;
        }
    }
    /* need to leave that up
    if (ftepp_next(ftepp) >= TOKEN_EOF) {
        ftepp_error(ftepp, "unexpected EOF in macro call");
        goto on_error;
    }
    */
    *out_params = params;
    return true;

on_error:
    if (mp.tokens)
        macroparam_clean(&mp);
    for (i = 0; i < vec_size(params); ++i)
        macroparam_clean(&params[i]);
    vec_free(params);
    return false;
}

static bool macro_params_find(ppmacro *macro, const char *name, size_t *idx)
{
    size_t i;
    for (i = 0; i < vec_size(macro->params); ++i) {
        if (!strcmp(macro->params[i], name)) {
            *idx = i;
            return true;
        }
    }
    return false;
}

static void ftepp_stringify_token(ftepp_t *ftepp, pptoken *token)
{
    char        chs[2];
    const char *ch;
    chs[1] = 0;
    switch (token->token) {
        case TOKEN_STRINGCONST:
            ch = token->value;
            while (*ch) {
                /* in preprocessor mode strings already are string,
                 * so we don't get actual newline bytes here.
                 * Still need to escape backslashes and quotes.
                 */
                switch (*ch) {
                    case '\\': ftepp_out(ftepp, "\\\\", false); break;
                    case '"':  ftepp_out(ftepp, "\\\"", false); break;
                    default:
                        chs[0] = *ch;
                        ftepp_out(ftepp, chs, false);
                        break;
                }
                ++ch;
            }
            break;
        case TOKEN_WHITE:
            ftepp_out(ftepp, " ", false);
            break;
        case TOKEN_EOL:
            ftepp_out(ftepp, "\\n", false);
            break;
        default:
            ftepp_out(ftepp, token->value, false);
            break;
    }
}

static void ftepp_stringify(ftepp_t *ftepp, macroparam *param)
{
    size_t i;
    ftepp_out(ftepp, "\"", false);
    for (i = 0; i < vec_size(param->tokens); ++i)
        ftepp_stringify_token(ftepp, param->tokens[i]);
    ftepp_out(ftepp, "\"", false);
}

static void ftepp_recursion_header(ftepp_t *ftepp)
{
    ftepp_out(ftepp, "\n#pragma push(line)\n", false);
}

static void ftepp_recursion_footer(ftepp_t *ftepp)
{
    ftepp_out(ftepp, "\n#pragma pop(line)\n", false);
}

static bool ftepp_preprocess(ftepp_t *ftepp);
static bool ftepp_macro_expand(ftepp_t *ftepp, ppmacro *macro, macroparam *params)
{
    char     *old_string = ftepp->output_string;
    lex_file *old_lexer = ftepp->lex;
    bool retval = true;

    size_t    o, pi, pv;
    lex_file *inlex;

    int nextok;

    /* really ... */
    if (!vec_size(macro->output))
        return true;

    ftepp->output_string = NULL;
    for (o = 0; o < vec_size(macro->output); ++o) {
        pptoken *out = macro->output[o];
        switch (out->token) {
            case TOKEN_IDENT:
            case TOKEN_TYPENAME:
            case TOKEN_KEYWORD:
                if (!macro_params_find(macro, out->value, &pi)) {
                    ftepp_out(ftepp, out->value, false);
                    break;
                } else {
                    for (pv = 0; pv < vec_size(params[pi].tokens); ++pv) {
                        out = params[pi].tokens[pv];
                        if (out->token == TOKEN_EOL)
                            ftepp_out(ftepp, "\n", false);
                        else
                            ftepp_out(ftepp, out->value, false);
                    }
                }
                break;
            case '#':
                if (o + 1 < vec_size(macro->output)) {
                    nextok = macro->output[o+1]->token;
                    if (nextok == '#') {
                        /* raw concatenation */
                        ++o;
                        break;
                    }
                    if ( (nextok == TOKEN_IDENT    ||
                          nextok == TOKEN_KEYWORD  ||
                          nextok == TOKEN_TYPENAME) &&
                        macro_params_find(macro, macro->output[o+1]->value, &pi))
                    {
                        ++o;
                        ftepp_stringify(ftepp, &params[pi]);
                        break;
                    }
                }
                ftepp_out(ftepp, "#", false);
                break;
            case TOKEN_EOL:
                ftepp_out(ftepp, "\n", false);
                break;
            default:
                ftepp_out(ftepp, out->value, false);
                break;
        }
    }
    vec_push(ftepp->output_string, 0);
    /* Now run the preprocessor recursively on this string buffer */
    /*
    printf("__________\n%s\n=========\n", ftepp->output_string);
    */
    inlex = lex_open_string(ftepp->output_string, vec_size(ftepp->output_string)-1, ftepp->lex->name);
    if (!inlex) {
        ftepp_error(ftepp, "internal error: failed to instantiate lexer");
        retval = false;
        goto cleanup;
    }
    ftepp->output_string = old_string;
    ftepp->lex = inlex;
    ftepp_recursion_header(ftepp);
    if (!ftepp_preprocess(ftepp)) {
        lex_close(ftepp->lex);
        retval = false;
        goto cleanup;
    }
    ftepp_recursion_footer(ftepp);
    old_string = ftepp->output_string;

cleanup:
    ftepp->lex           = old_lexer;
    ftepp->output_string = old_string;
    return retval;
}

static bool ftepp_macro_call(ftepp_t *ftepp, ppmacro *macro)
{
    size_t     o;
    macroparam *params = NULL;
    bool        retval = true;

    if (!macro->has_params) {
        if (!ftepp_macro_expand(ftepp, macro, NULL))
            return false;
        ftepp_next(ftepp);
        return true;
    }
    ftepp_next(ftepp);

    if (!ftepp_skipallwhite(ftepp))
        return false;

    if (ftepp->token != '(') {
        ftepp_error(ftepp, "expected macro parameters in parenthesis");
        return false;
    }

    ftepp_next(ftepp);
    if (!ftepp_macro_call_params(ftepp, &params))
        return false;

    if (vec_size(params) != vec_size(macro->params)) {
        ftepp_error(ftepp, "macro %s expects %u paramteters, %u provided", macro->name,
                    (unsigned int)vec_size(macro->params),
                    (unsigned int)vec_size(params));
        retval = false;
        goto cleanup;
    }

    if (!ftepp_macro_expand(ftepp, macro, params))
        retval = false;
    ftepp_next(ftepp);

cleanup:
    for (o = 0; o < vec_size(params); ++o)
        macroparam_clean(&params[o]);
    vec_free(params);
    return retval;
}

/**
 * #if - the FTEQCC way:
 *    defined(FOO) => true if FOO was #defined regardless of parameters or contents
 *    <numbers>    => True if the number is not 0
 *    !<factor>    => True if the factor yields false
 *    !!<factor>   => ERROR on 2 or more unary nots
 *    <macro>      => becomes the macro's FIRST token regardless of parameters
 *    <e> && <e>   => True if both expressions are true
 *    <e> || <e>   => True if either expression is true
 *    <string>     => False
 *    <ident>      => False (remember for macros the <macro> rule applies instead)
 * Unary + and - are weird and wrong in fteqcc so we don't allow them
 * parenthesis in expressions are allowed
 * parameter lists on macros are errors
 * No mathematical calculations are executed
 */
static bool ftepp_if_expr(ftepp_t *ftepp, bool *out, double *value_out);
static bool ftepp_if_op(ftepp_t *ftepp)
{
    ftepp->lex->flags.noops = false;
    ftepp_next(ftepp);
    if (!ftepp_skipspace(ftepp))
        return false;
    ftepp->lex->flags.noops = true;
    return true;
}
static bool ftepp_if_value(ftepp_t *ftepp, bool *out, double *value_out)
{
    ppmacro *macro;
    bool     wasnot = false;

    if (!ftepp_skipspace(ftepp))
        return false;

    while (ftepp->token == '!') {
        wasnot = true;
        ftepp_next(ftepp);
        if (!ftepp_skipspace(ftepp))
            return false;
    }

    switch (ftepp->token) {
        case TOKEN_IDENT:
        case TOKEN_TYPENAME:
        case TOKEN_KEYWORD:
            if (!strcmp(ftepp_tokval(ftepp), "defined")) {
                ftepp_next(ftepp);
                if (!ftepp_skipspace(ftepp))
                    return false;
                if (ftepp->token != '(') {
                    ftepp_error(ftepp, "`defined` keyword in #if requires a macro name in parenthesis");
                    return false;
                }
                ftepp_next(ftepp);
                if (!ftepp_skipspace(ftepp))
                    return false;
                if (ftepp->token != TOKEN_IDENT &&
                    ftepp->token != TOKEN_TYPENAME &&
                    ftepp->token != TOKEN_KEYWORD)
                {
                    ftepp_error(ftepp, "defined() used on an unexpected token type");
                    return false;
                }
                macro = ftepp_macro_find(ftepp, ftepp_tokval(ftepp));
                *out = !!macro;
                ftepp_next(ftepp);
                if (!ftepp_skipspace(ftepp))
                    return false;
                if (ftepp->token != ')') {
                    ftepp_error(ftepp, "expected closing paren");
                    return false;
                }
                break;
            }

            macro = ftepp_macro_find(ftepp, ftepp_tokval(ftepp));
            if (!macro || !vec_size(macro->output)) {
                *out = false;
                *value_out = 0;
            } else {
                /* This does not expand recursively! */
                switch (macro->output[0]->token) {
                    case TOKEN_INTCONST:
                        *value_out = macro->output[0]->constval.i;
                        *out = !!(macro->output[0]->constval.i);
                        break;
                    case TOKEN_FLOATCONST:
                        *value_out = macro->output[0]->constval.f;
                        *out = !!(macro->output[0]->constval.f);
                        break;
                    default:
                        *out = false;
                        break;
                }
            }
            break;
        case TOKEN_STRINGCONST:
            *out = false;
            break;
        case TOKEN_INTCONST:
            *value_out = ftepp->lex->tok.constval.i;
            *out = !!(ftepp->lex->tok.constval.i);
            break;
        case TOKEN_FLOATCONST:
            *value_out = ftepp->lex->tok.constval.f;
            *out = !!(ftepp->lex->tok.constval.f);
            break;

        case '(':
            ftepp_next(ftepp);
            if (!ftepp_if_expr(ftepp, out, value_out))
                return false;
            if (ftepp->token != ')') {
                ftepp_error(ftepp, "expected closing paren in #if expression");
                return false;
            }
            break;

        default:
            ftepp_error(ftepp, "junk in #if: `%s` ...", ftepp_tokval(ftepp));
            return false;
    }
    if (wasnot) {
        *out = !*out;
        *value_out = (*out ? 1 : 0);
    }
    return true;
}

/*
static bool ftepp_if_nextvalue(ftepp_t *ftepp, bool *out, double *value_out)
{
    if (!ftepp_next(ftepp))
        return false;
    return ftepp_if_value(ftepp, out, value_out);
}
*/

static bool ftepp_if_expr(ftepp_t *ftepp, bool *out, double *value_out)
{
    if (!ftepp_if_value(ftepp, out, value_out))
        return false;

    if (!ftepp_if_op(ftepp))
        return false;

    if (ftepp->token == ')' || ftepp->token != TOKEN_OPERATOR)
        return true;

    /* FTEQCC is all right-associative and no precedence here */
    if (!strcmp(ftepp_tokval(ftepp), "&&") ||
        !strcmp(ftepp_tokval(ftepp), "||"))
    {
        bool next = false;
        char opc  = ftepp_tokval(ftepp)[0];
        double nextvalue;

        (void)nextvalue;
        if (!ftepp_next(ftepp))
            return false;
        if (!ftepp_if_expr(ftepp, &next, &nextvalue))
            return false;

        if (opc == '&')
            *out = *out && next;
        else
            *out = *out || next;

        *value_out = (*out ? 1 : 0);
        return true;
    }
    else if (!strcmp(ftepp_tokval(ftepp), "==") ||
             !strcmp(ftepp_tokval(ftepp), "!=") ||
             !strcmp(ftepp_tokval(ftepp), ">=") ||
             !strcmp(ftepp_tokval(ftepp), "<=") ||
             !strcmp(ftepp_tokval(ftepp), ">") ||
             !strcmp(ftepp_tokval(ftepp), "<"))
    {
        bool next = false;
        const char opc0 = ftepp_tokval(ftepp)[0];
        const char opc1 = ftepp_tokval(ftepp)[1];
        double other;

        if (!ftepp_next(ftepp))
            return false;
        if (!ftepp_if_expr(ftepp, &next, &other))
            return false;

        if (opc0 == '=')
            *out = (*value_out == other);
        else if (opc0 == '!')
            *out = (*value_out != other);
        else if (opc0 == '>') {
            if (opc1 == '=') *out = (*value_out >= other);
            else             *out = (*value_out > other);
        }
        else if (opc0 == '<') {
            if (opc1 == '=') *out = (*value_out <= other);
            else             *out = (*value_out < other);
        }
        *value_out = (*out ? 1 : 0);

        return true;
    }
    else {
        ftepp_error(ftepp, "junk after #if");
        return false;
    }
}

static bool ftepp_if(ftepp_t *ftepp, ppcondition *cond)
{
    bool result = false;
    double dummy = 0;

    memset(cond, 0, sizeof(*cond));
    (void)ftepp_next(ftepp);

    if (!ftepp_skipspace(ftepp))
        return false;
    if (ftepp->token == TOKEN_EOL) {
        ftepp_error(ftepp, "expected expression for #if-directive");
        return false;
    }

    if (!ftepp_if_expr(ftepp, &result, &dummy))
        return false;

    cond->on = result;
    return true;
}

/**
 * ifdef is rather simple
 */
static bool ftepp_ifdef(ftepp_t *ftepp, ppcondition *cond)
{
    ppmacro *macro;
    memset(cond, 0, sizeof(*cond));
    (void)ftepp_next(ftepp);
    if (!ftepp_skipspace(ftepp))
        return false;

    switch (ftepp->token) {
        case TOKEN_IDENT:
        case TOKEN_TYPENAME:
        case TOKEN_KEYWORD:
            macro = ftepp_macro_find(ftepp, ftepp_tokval(ftepp));
            break;
        default:
            ftepp_error(ftepp, "expected macro name");
            return false;
    }

    (void)ftepp_next(ftepp);
    if (!ftepp_skipspace(ftepp))
        return false;
    /* relaxing this condition
    if (ftepp->token != TOKEN_EOL && ftepp->token != TOKEN_EOF) {
        ftepp_error(ftepp, "stray tokens after #ifdef");
        return false;
    }
    */
    cond->on = !!macro;
    return true;
}

/**
 * undef is also simple
 */
static bool ftepp_undef(ftepp_t *ftepp)
{
    (void)ftepp_next(ftepp);
    if (!ftepp_skipspace(ftepp))
        return false;

    if (ftepp->output_on) {
        switch (ftepp->token) {
            case TOKEN_IDENT:
            case TOKEN_TYPENAME:
            case TOKEN_KEYWORD:
                ftepp_macro_delete(ftepp, ftepp_tokval(ftepp));
                break;
            default:
                ftepp_error(ftepp, "expected macro name");
                return false;
        }
    }

    (void)ftepp_next(ftepp);
    if (!ftepp_skipspace(ftepp))
        return false;
    /* relaxing this condition
    if (ftepp->token != TOKEN_EOL && ftepp->token != TOKEN_EOF) {
        ftepp_error(ftepp, "stray tokens after #ifdef");
        return false;
    }
    */
    return true;
}

/* Special unescape-string function which skips a leading quote
 * and stops at a quote, not just at \0
 */
static void unescape(const char *str, char *out) {
    ++str;
    while (*str && *str != '"') {
        if (*str == '\\') {
            ++str;
            switch (*str) {
                case '\\': *out++ = *str; break;
                case '"':  *out++ = *str; break;
                case 'a':  *out++ = '\a'; break;
                case 'b':  *out++ = '\b'; break;
                case 'r':  *out++ = '\r'; break;
                case 'n':  *out++ = '\n'; break;
                case 't':  *out++ = '\t'; break;
                case 'f':  *out++ = '\f'; break;
                case 'v':  *out++ = '\v'; break;
                default:
                    *out++ = '\\';
                    *out++ = *str;
                    break;
            }
            ++str;
            continue;
        }

        *out++ = *str++;
    }
    *out = 0;
}

static char *ftepp_include_find_path(const char *file, const char *pathfile)
{
    FILE       *fp;
    char       *filename = NULL;
    const char *last_slash;
    size_t      len;

    if (!pathfile)
        return NULL;

    last_slash = strrchr(pathfile, '/');

    if (last_slash) {
        len = last_slash - pathfile;
        memcpy(vec_add(filename, len), pathfile, len);
        vec_push(filename, '/');
    }

    len = strlen(file);
    memcpy(vec_add(filename, len+1), file, len);
    vec_last(filename) = 0;

    fp = file_open(filename, "rb");
    if (fp) {
        file_close(fp);
        return filename;
    }
    vec_free(filename);
    return NULL;
}

static char *ftepp_include_find(ftepp_t *ftepp, const char *file)
{
    char *filename = NULL;

    filename = ftepp_include_find_path(file, ftepp->includename);
    if (!filename)
        filename = ftepp_include_find_path(file, ftepp->itemname);
    return filename;
}

static void ftepp_directive_warning(ftepp_t *ftepp) {
    char *message = NULL;

    if (!ftepp_skipspace(ftepp))
        return;

    /* handle the odd non string constant case so it works like C */
    if (ftepp->token != TOKEN_STRINGCONST) {
        vec_upload(message, "#warning", 8);
        ftepp_next(ftepp);
        while (ftepp->token != TOKEN_EOL) {
            vec_upload(message, ftepp_tokval(ftepp), strlen(ftepp_tokval(ftepp)));
            ftepp_next(ftepp);
        }
        vec_push(message, '\0');
        (void)!!ftepp_warn(ftepp, WARN_CPP, message);
        vec_free(message);
        return;
    }

    unescape  (ftepp_tokval(ftepp), ftepp_tokval(ftepp));
    (void)!!ftepp_warn(ftepp, WARN_CPP, "#warning %s", ftepp_tokval(ftepp));
}

static void ftepp_directive_error(ftepp_t *ftepp) {
    char *message = NULL;

    if (!ftepp_skipspace(ftepp))
        return;

    /* handle the odd non string constant case so it works like C */
    if (ftepp->token != TOKEN_STRINGCONST) {
        vec_upload(message, "#error", 6);
        ftepp_next(ftepp);
        while (ftepp->token != TOKEN_EOL) {
            vec_upload(message, ftepp_tokval(ftepp), strlen(ftepp_tokval(ftepp)));
            ftepp_next(ftepp);
        }
        vec_push(message, '\0');
        ftepp_error(ftepp, message);
        vec_free(message);
        return;
    }

    unescape  (ftepp_tokval(ftepp), ftepp_tokval(ftepp));
    ftepp_error(ftepp, "#error %s", ftepp_tokval(ftepp));
}

/**
 * Include a file.
 * FIXME: do we need/want a -I option?
 * FIXME: what about when dealing with files in subdirectories coming from a progs.src?
 */
static bool ftepp_include(ftepp_t *ftepp)
{
    lex_file *old_lexer = ftepp->lex;
    lex_file *inlex;
    lex_ctx  ctx;
    char     lineno[128];
    char     *filename;
    char     *old_includename;

    (void)ftepp_next(ftepp);
    if (!ftepp_skipspace(ftepp))
        return false;

    if (ftepp->token != TOKEN_STRINGCONST) {
        ftepp_error(ftepp, "expected filename to include");
        return false;
    }

    ctx = ftepp_ctx(ftepp);

    unescape(ftepp_tokval(ftepp), ftepp_tokval(ftepp));

    ftepp_out(ftepp, "\n#pragma file(", false);
    ftepp_out(ftepp, ftepp_tokval(ftepp), false);
    ftepp_out(ftepp, ")\n#pragma line(1)\n", false);

    filename = ftepp_include_find(ftepp, ftepp_tokval(ftepp));
    if (!filename) {
        ftepp_error(ftepp, "failed to open include file `%s`", ftepp_tokval(ftepp));
        return false;
    }
    inlex = lex_open(filename);
    if (!inlex) {
        ftepp_error(ftepp, "open failed on include file `%s`", filename);
        vec_free(filename);
        return false;
    }
    ftepp->lex = inlex;
    old_includename = ftepp->includename;
    ftepp->includename = filename;
    if (!ftepp_preprocess(ftepp)) {
        vec_free(ftepp->includename);
        ftepp->includename = old_includename;
        lex_close(ftepp->lex);
        ftepp->lex = old_lexer;
        return false;
    }
    vec_free(ftepp->includename);
    ftepp->includename = old_includename;
    lex_close(ftepp->lex);
    ftepp->lex = old_lexer;

    ftepp_out(ftepp, "\n#pragma file(", false);
    ftepp_out(ftepp, ctx.file, false);
    snprintf(lineno, sizeof(lineno), ")\n#pragma line(%lu)\n", (unsigned long)(ctx.line+1));
    ftepp_out(ftepp, lineno, false);

    /* skip the line */
    (void)ftepp_next(ftepp);
    if (!ftepp_skipspace(ftepp))
        return false;
    if (ftepp->token != TOKEN_EOL) {
        ftepp_error(ftepp, "stray tokens after #include");
        return false;
    }
    (void)ftepp_next(ftepp);

    return true;
}

/* Basic structure handlers */
static bool ftepp_else_allowed(ftepp_t *ftepp)
{
    if (!vec_size(ftepp->conditions)) {
        ftepp_error(ftepp, "#else without #if");
        return false;
    }
    if (vec_last(ftepp->conditions).had_else) {
        ftepp_error(ftepp, "multiple #else for a single #if");
        return false;
    }
    return true;
}

static bool ftepp_hash(ftepp_t *ftepp)
{
    ppcondition cond;
    ppcondition *pc;

    lex_ctx ctx = ftepp_ctx(ftepp);

    if (!ftepp_skipspace(ftepp))
        return false;

    switch (ftepp->token) {
        case TOKEN_KEYWORD:
        case TOKEN_IDENT:
        case TOKEN_TYPENAME:
            if (!strcmp(ftepp_tokval(ftepp), "define")) {
                return ftepp_define(ftepp);
            }
            else if (!strcmp(ftepp_tokval(ftepp), "undef")) {
                return ftepp_undef(ftepp);
            }
            else if (!strcmp(ftepp_tokval(ftepp), "ifdef")) {
                if (!ftepp_ifdef(ftepp, &cond))
                    return false;
                cond.was_on = cond.on;
                vec_push(ftepp->conditions, cond);
                ftepp->output_on = ftepp->output_on && cond.on;
                break;
            }
            else if (!strcmp(ftepp_tokval(ftepp), "ifndef")) {
                if (!ftepp_ifdef(ftepp, &cond))
                    return false;
                cond.on = !cond.on;
                cond.was_on = cond.on;
                vec_push(ftepp->conditions, cond);
                ftepp->output_on = ftepp->output_on && cond.on;
                break;
            }
            else if (!strcmp(ftepp_tokval(ftepp), "elifdef")) {
                if (!ftepp_else_allowed(ftepp))
                    return false;
                if (!ftepp_ifdef(ftepp, &cond))
                    return false;
                pc = &vec_last(ftepp->conditions);
                pc->on     = !pc->was_on && cond.on;
                pc->was_on = pc->was_on || pc->on;
                ftepp_update_output_condition(ftepp);
                break;
            }
            else if (!strcmp(ftepp_tokval(ftepp), "elifndef")) {
                if (!ftepp_else_allowed(ftepp))
                    return false;
                if (!ftepp_ifdef(ftepp, &cond))
                    return false;
                cond.on = !cond.on;
                pc = &vec_last(ftepp->conditions);
                pc->on     = !pc->was_on && cond.on;
                pc->was_on = pc->was_on || pc->on;
                ftepp_update_output_condition(ftepp);
                break;
            }
            else if (!strcmp(ftepp_tokval(ftepp), "elif")) {
                if (!ftepp_else_allowed(ftepp))
                    return false;
                if (!ftepp_if(ftepp, &cond))
                    return false;
                pc = &vec_last(ftepp->conditions);
                pc->on     = !pc->was_on && cond.on;
                pc->was_on = pc->was_on  || pc->on;
                ftepp_update_output_condition(ftepp);
                break;
            }
            else if (!strcmp(ftepp_tokval(ftepp), "if")) {
                if (!ftepp_if(ftepp, &cond))
                    return false;
                cond.was_on = cond.on;
                vec_push(ftepp->conditions, cond);
                ftepp->output_on = ftepp->output_on && cond.on;
                break;
            }
            else if (!strcmp(ftepp_tokval(ftepp), "else")) {
                if (!ftepp_else_allowed(ftepp))
                    return false;
                pc = &vec_last(ftepp->conditions);
                pc->on = !pc->was_on;
                pc->had_else = true;
                ftepp_next(ftepp);
                ftepp_update_output_condition(ftepp);
                break;
            }
            else if (!strcmp(ftepp_tokval(ftepp), "endif")) {
                if (!vec_size(ftepp->conditions)) {
                    ftepp_error(ftepp, "#endif without #if");
                    return false;
                }
                vec_pop(ftepp->conditions);
                ftepp_next(ftepp);
                ftepp_update_output_condition(ftepp);
                break;
            }
            else if (!strcmp(ftepp_tokval(ftepp), "include")) {
                return ftepp_include(ftepp);
            }
            else if (!strcmp(ftepp_tokval(ftepp), "pragma")) {
                ftepp_out(ftepp, "#", false);
                break;
            }
            else if (!strcmp(ftepp_tokval(ftepp), "warning")) {
                ftepp_directive_warning(ftepp);
                break;
            }
            else if (!strcmp(ftepp_tokval(ftepp), "error")) {
                ftepp_directive_error(ftepp);
                break;
            }
            else {
                if (ftepp->output_on) {
                    ftepp_error(ftepp, "unrecognized preprocessor directive: `%s`", ftepp_tokval(ftepp));
                    return false;
                } else {
                    ftepp_next(ftepp);
                    break;
                }
            }
            /* break; never reached */
        default:
            ftepp_error(ftepp, "unexpected preprocessor token: `%s`", ftepp_tokval(ftepp));
            return false;
        case TOKEN_EOL:
            ftepp_errorat(ftepp, ctx, "empty preprocessor directive");
            return false;
        case TOKEN_EOF:
            ftepp_error(ftepp, "missing newline at end of file", ftepp_tokval(ftepp));
            return false;

        /* Builtins! Don't forget the builtins! */
        case TOKEN_INTCONST:
        case TOKEN_FLOATCONST:
            ftepp_out(ftepp, "#", false);
            return true;
    }
    if (!ftepp_skipspace(ftepp))
        return false;
    return true;
}

static bool ftepp_preprocess(ftepp_t *ftepp)
{
    ppmacro *macro;
    bool     newline = true;

    ftepp->lex->flags.preprocessing = true;
    ftepp->lex->flags.mergelines    = false;
    ftepp->lex->flags.noops         = true;

    ftepp_next(ftepp);
    do
    {
        if (ftepp->token >= TOKEN_EOF)
            break;
#if 0
        newline = true;
#endif

        switch (ftepp->token) {
            case TOKEN_KEYWORD:
            case TOKEN_IDENT:
            case TOKEN_TYPENAME:
                if (ftepp->output_on)
                    macro = ftepp_macro_find(ftepp, ftepp_tokval(ftepp));
                else
                    macro = NULL;
                if (!macro) {
                    ftepp_out(ftepp, ftepp_tokval(ftepp), false);
                    ftepp_next(ftepp);
                    break;
                }
                if (!ftepp_macro_call(ftepp, macro))
                    ftepp->token = TOKEN_ERROR;
                break;
            case '#':
                if (!newline) {
                    ftepp_out(ftepp, ftepp_tokval(ftepp), false);
                    ftepp_next(ftepp);
                    break;
                }
                ftepp->lex->flags.mergelines = true;
                if (ftepp_next(ftepp) >= TOKEN_EOF) {
                    ftepp_error(ftepp, "error in preprocessor directive");
                    ftepp->token = TOKEN_ERROR;
                    break;
                }
                if (!ftepp_hash(ftepp))
                    ftepp->token = TOKEN_ERROR;
                ftepp->lex->flags.mergelines = false;
                break;
            case TOKEN_EOL:
                newline = true;
                ftepp_out(ftepp, "\n", true);
                ftepp_next(ftepp);
                break;
            case TOKEN_WHITE:
                /* same as default but don't set newline=false */
                ftepp_out(ftepp, ftepp_tokval(ftepp), false);
                ftepp_next(ftepp);
                break;
            default:
                newline = false;
                ftepp_out(ftepp, ftepp_tokval(ftepp), false);
                ftepp_next(ftepp);
                break;
        }
    } while (!ftepp->errors && ftepp->token < TOKEN_EOF);

    /* force a 0 at the end but don't count it as added to the output */
    vec_push(ftepp->output_string, 0);
    vec_shrinkby(ftepp->output_string, 1);

    return (ftepp->token == TOKEN_EOF);
}

/* Like in parser.c - files keep the previous state so we have one global
 * preprocessor. Except here we will want to warn about dangling #ifs.
 */
static ftepp_t *ftepp;

static bool ftepp_preprocess_done()
{
    bool retval = true;
    if (vec_size(ftepp->conditions)) {
        if (ftepp_warn(ftepp, WARN_MULTIFILE_IF, "#if spanning multiple files, is this intended?"))
            retval = false;
    }
    lex_close(ftepp->lex);
    ftepp->lex = NULL;
    if (ftepp->itemname) {
        mem_d(ftepp->itemname);
        ftepp->itemname = NULL;
    }
    return retval;
}

bool ftepp_preprocess_file(const char *filename)
{
    ftepp->lex = lex_open(filename);
    ftepp->itemname = util_strdup(filename);
    if (!ftepp->lex) {
        con_out("failed to open file \"%s\"\n", filename);
        return false;
    }
    if (!ftepp_preprocess(ftepp))
        return false;
    return ftepp_preprocess_done();
}

bool ftepp_preprocess_string(const char *name, const char *str)
{
    ftepp->lex = lex_open_string(str, strlen(str), name);
    ftepp->itemname = util_strdup(name);
    if (!ftepp->lex) {
        con_out("failed to create lexer for string \"%s\"\n", name);
        return false;
    }
    if (!ftepp_preprocess(ftepp))
        return false;
    return ftepp_preprocess_done();
}


void ftepp_add_macro(const char *name, const char *value) {
    char *create = NULL;

    /* use saner path for empty macros */
    if (!value) {
        ftepp_add_define("__builtin__", name);
        return;
    }

    vec_upload(create, "#define ", 8);
    vec_upload(create, name,  strlen(name));
    vec_push  (create, ' ');
    vec_upload(create, value, strlen(value));
    vec_push  (create, 0);

    ftepp_preprocess_string("__builtin__", create);
    vec_free  (create);
}

bool ftepp_init()
{
    char minor[32];
    char major[32];

    ftepp = ftepp_new();
    if (!ftepp)
        return false;

    memset(minor, 0, sizeof(minor));
    memset(major, 0, sizeof(major));

    /* set the right macro based on the selected standard */
    ftepp_add_define(NULL, "GMQCC");
    if (opts.standard == COMPILER_FTEQCC) {
        ftepp_add_define(NULL, "__STD_FTEQCC__");
        /* 1.00 */
        major[0] = '"';
        major[1] = '1';
        major[2] = '"';

        minor[0] = '"';
        minor[1] = '0';
        minor[2] = '"';
    } else if (opts.standard == COMPILER_GMQCC) {
        ftepp_add_define(NULL, "__STD_GMQCC__");
        sprintf(major, "\"%d\"", GMQCC_VERSION_MAJOR);
        sprintf(minor, "\"%d\"", GMQCC_VERSION_MINOR);
    } else if (opts.standard == COMPILER_QCC) {
        ftepp_add_define(NULL, "__STD_QCC__");
        /* 1.0 */
        major[0] = '"';
        major[1] = '1';
        major[2] = '"';

        minor[0] = '"';
        minor[1] = '0';
        minor[2] = '"';
    }

    ftepp_add_macro("__STD_VERSION_MINOR__", minor);
    ftepp_add_macro("__STD_VERSION_MAJOR__", major);

    return true;
}

void ftepp_add_define(const char *source, const char *name)
{
    ppmacro *macro;
    lex_ctx ctx = { "__builtin__", 0 };
    ctx.file = source;
    macro = ppmacro_new(ctx, name);
    vec_push(ftepp->macros, macro);
}

const char *ftepp_get()
{
    return ftepp->output_string;
}

void ftepp_flush()
{
    vec_free(ftepp->output_string);
}

void ftepp_finish()
{
    if (!ftepp)
        return;
    ftepp_delete(ftepp);
    ftepp = NULL;
}
