/*
 * Copyright (C) 2012, 2013, 2014
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
#include <string.h>
#include <stdlib.h>

#include "gmqcc.h"

const unsigned int opts_opt_oflag[COUNT_OPTIMIZATIONS+1] = {
# define GMQCC_TYPE_OPTIMIZATIONS
# define GMQCC_DEFINE_FLAG(NAME, MIN_O) MIN_O,
#  include "opts.def"
    0
};

const opts_flag_def_t opts_opt_list[COUNT_OPTIMIZATIONS+1] = {
# define GMQCC_TYPE_OPTIMIZATIONS
# define GMQCC_DEFINE_FLAG(NAME, MIN_O) { #NAME, LONGBIT(OPTIM_##NAME) },
#  include "opts.def"
    { NULL, LONGBIT(0) }
};

const opts_flag_def_t opts_warn_list[COUNT_WARNINGS+1] = {
# define GMQCC_TYPE_WARNS
# define GMQCC_DEFINE_FLAG(X) { #X, LONGBIT(WARN_##X) },
#  include "opts.def"
    { NULL, LONGBIT(0) }
};

const opts_flag_def_t opts_flag_list[COUNT_FLAGS+1] = {
# define GMQCC_TYPE_FLAGS
# define GMQCC_DEFINE_FLAG(X) { #X, LONGBIT(X) },
#  include "opts.def"
    { NULL, LONGBIT(0) }
};

unsigned int opts_optimizationcount[COUNT_OPTIMIZATIONS];
opts_cmd_t   opts; /* command line options */

static void opts_setdefault(void) {
    memset(&opts, 0, sizeof(opts_cmd_t));
    OPTS_OPTION_BOOL(OPTION_CORRECTION) = true;
    OPTS_OPTION_STR(OPTION_PROGSRC)     = "progs.src";

    /* warnings */
    opts_set(opts.warn,  WARN_UNUSED_VARIABLE,           true);
    opts_set(opts.warn,  WARN_USED_UNINITIALIZED,        true);
    opts_set(opts.warn,  WARN_UNKNOWN_CONTROL_SEQUENCE,  true);
    opts_set(opts.warn,  WARN_EXTENSIONS,                true);
    opts_set(opts.warn,  WARN_FIELD_REDECLARED,          true);
    opts_set(opts.warn,  WARN_MISSING_RETURN_VALUES,     true);
    opts_set(opts.warn,  WARN_INVALID_PARAMETER_COUNT,   true);
    opts_set(opts.warn,  WARN_LOCAL_CONSTANTS,           true);
    opts_set(opts.warn,  WARN_VOID_VARIABLES,            true);
    opts_set(opts.warn,  WARN_IMPLICIT_FUNCTION_POINTER, true);
    opts_set(opts.warn,  WARN_VARIADIC_FUNCTION,         true);
    opts_set(opts.warn,  WARN_FRAME_MACROS,              true);
    opts_set(opts.warn,  WARN_EFFECTLESS_STATEMENT,      true);
    opts_set(opts.warn,  WARN_END_SYS_FIELDS,            true);
    opts_set(opts.warn,  WARN_ASSIGN_FUNCTION_TYPES,     true);
    opts_set(opts.warn,  WARN_CPP,                       true);
    opts_set(opts.warn,  WARN_MULTIFILE_IF,              true);
    opts_set(opts.warn,  WARN_DOUBLE_DECLARATION,        true);
    opts_set(opts.warn,  WARN_CONST_VAR,                 true);
    opts_set(opts.warn,  WARN_MULTIBYTE_CHARACTER,       true);
    opts_set(opts.warn,  WARN_UNKNOWN_PRAGMAS,           true);
    opts_set(opts.warn,  WARN_UNREACHABLE_CODE,          true);
    opts_set(opts.warn,  WARN_UNKNOWN_ATTRIBUTE,         true);
    opts_set(opts.warn,  WARN_RESERVED_NAMES,            true);
    opts_set(opts.warn,  WARN_UNINITIALIZED_CONSTANT,    true);
    opts_set(opts.warn,  WARN_DEPRECATED,                true);
    opts_set(opts.warn,  WARN_PARENTHESIS,               true);
    opts_set(opts.warn,  WARN_CONST_OVERWRITE,           true);
    opts_set(opts.warn,  WARN_DIRECTIVE_INMACRO,         true);
    opts_set(opts.warn,  WARN_BUILTINS,                  true);
    opts_set(opts.warn,  WARN_INEXACT_COMPARES,          true);

    /* flags */
    opts_set(opts.flags, ADJUST_VECTOR_FIELDS,           true);
    opts_set(opts.flags, CORRECT_TERNARY,                true);
    opts_set(opts.flags, BAIL_ON_WERROR,                 true);
    opts_set(opts.flags, LEGACY_VECTOR_MATHS,            true);
    opts_set(opts.flags, DARKPLACES_STRING_TABLE_BUG,    true);

}

void opts_backup_non_Wall() {
    size_t i;
    for (i = 0; i <= WARN_DEBUG; ++i)
        opts_set(opts.warn_backup, i, OPTS_WARN(i));
}

void opts_restore_non_Wall() {
    size_t i;
    for (i = 0; i <= WARN_DEBUG; ++i)
        opts_set(opts.warn, i, OPTS_GENERIC(opts.warn_backup, i));
}

void opts_backup_non_Werror_all() {
    size_t i;
    for (i = 0; i <= WARN_DEBUG; ++i)
        opts_set(opts.werror_backup, i, OPTS_WERROR(i));
}

void opts_restore_non_Werror_all() {
    size_t i;
    for (i = 0; i <= WARN_DEBUG; ++i)
        opts_set(opts.werror, i, OPTS_GENERIC(opts.werror_backup, i));
}

void opts_init(const char *output, int standard, size_t arraysize) {
    opts_setdefault();

    OPTS_OPTION_STR(OPTION_OUTPUT)         = output;
    OPTS_OPTION_U32(OPTION_STANDARD)       = standard;
    OPTS_OPTION_U32(OPTION_MAX_ARRAY_SIZE) = arraysize;
    OPTS_OPTION_U16(OPTION_MEMDUMPCOLS)    = 16;
}

static bool opts_setflag_all(const char *name, bool on, uint32_t *flags, const opts_flag_def_t *list, size_t listsize) {
    size_t i;

    for (i = 0; i < listsize; ++i) {
        if (!strcmp(name, list[i].name)) {
            longbit lb = list[i].bit;

            if (on)
                flags[lb.idx] |= (1<<(lb.bit));
            else
                flags[lb.idx] &= ~(1<<(lb.bit));

            return true;
        }
    }
    return false;
}
bool opts_setflag  (const char *name, bool on) {
    return opts_setflag_all(name, on, opts.flags,        opts_flag_list, COUNT_FLAGS);
}
bool opts_setwarn  (const char *name, bool on) {
    return opts_setflag_all(name, on, opts.warn,         opts_warn_list, COUNT_WARNINGS);
}
bool opts_setwerror(const char *name, bool on) {
    return opts_setflag_all(name, on, opts.werror,       opts_warn_list, COUNT_WARNINGS);
}
bool opts_setoptim (const char *name, bool on) {
    return opts_setflag_all(name, on, opts.optimization, opts_opt_list,  COUNT_OPTIMIZATIONS);
}

void opts_set(uint32_t *flags, size_t idx, bool on) {
    longbit lb;
    LONGBIT_SET(lb, idx);

    if (on)
        flags[lb.idx] |= (1<<(lb.bit));
    else
        flags[lb.idx] &= ~(1<<(lb.bit));
}

void opts_setoptimlevel(unsigned int level) {
    size_t i;
    for (i = 0; i < COUNT_OPTIMIZATIONS; ++i)
        opts_set(opts.optimization, i, level >= opts_opt_oflag[i]);

    if (!level)
        opts.optimizeoff = true;
}

/*
 * Standard configuration parser and subsystem.  Yes, optionally you may
 * create ini files or cfg (the driver accepts both) for a project opposed
 * to supplying just a progs.src (since you also may need to supply command
 * line arguments or set the options of the compiler) [which cannot be done
 * from a progs.src.
 */
static char *opts_ini_rstrip(char *s) {
    char *p = s + strlen(s);
    while(p > s && util_isspace(*--p))
        *p = '\0';
    return s;
}

static char *opts_ini_lskip(const char *s) {
    while (*s && util_isspace(*s))
        s++;
    return (char*)s;
}

static char *opts_ini_next(const char *s, char c) {
    bool last = false;
    while (*s && *s != c && !(last && *s == ';'))
        last = !!util_isspace(*s), s++;

    return (char*)s;
}

static size_t opts_ini_parse (
    fs_file_t *filehandle,
    char *(*loadhandle)(const char *, const char *, const char *),
    char **errorhandle
) {
    size_t linesize;
    size_t lineno             = 1;
    size_t error              = 0;
    char  *line               = NULL;
    char   section_data[2048] = "";
    char   oldname_data[2048] = "";

    /* parsing and reading variables */
    char *parse_beg;
    char *parse_end;
    char *read_name;
    char *read_value;

    while (fs_file_getline(&line, &linesize, filehandle) != FS_FILE_EOF) {
        parse_beg = line;

        /* handle BOM */
        if (lineno == 1 && (
                (unsigned char)parse_beg[0] == 0xEF &&
                (unsigned char)parse_beg[1] == 0xBB &&
                (unsigned char)parse_beg[2] == 0xBF
            )
        ) {
            parse_beg ++; /* 0xEF */
            parse_beg ++; /* 0xBB */
            parse_beg ++; /* 0xBF */
        }

        if (*(parse_beg = opts_ini_lskip(opts_ini_rstrip(parse_beg))) == ';' || *parse_beg == '#') {
            /* ignore '#' is a perl extension */
        } else if (*parse_beg == '[') {
            /* section found */
            if (*(parse_end = opts_ini_next(parse_beg + 1, ']')) == ']') {
                * parse_end = '\0'; /* terminate bro */
                util_strncpy(section_data, parse_beg + 1, sizeof(section_data));
                section_data[sizeof(section_data) - 1] = '\0';
                *oldname_data                          = '\0';
            } else if (!error) {
                /* otherwise set error to the current line number */
                error = lineno;
            }
        } else if (*parse_beg && *parse_beg != ';') {
            /* not a comment, must be a name value pair :) */
            if (*(parse_end = opts_ini_next(parse_beg, '=')) != '=')
                  parse_end = opts_ini_next(parse_beg, ':');

            if (*parse_end == '=' || *parse_end == ':') {
                *parse_end = '\0'; /* terminate bro */
                read_name  = opts_ini_rstrip(parse_beg);
                read_value = opts_ini_lskip(parse_end + 1);
                if (*(parse_end = opts_ini_next(read_value, '\0')) == ';')
                    * parse_end = '\0';
                opts_ini_rstrip(read_value);

                /* valid name value pair, lets call down to handler */
                util_strncpy(oldname_data, read_name, sizeof(oldname_data));
                oldname_data[sizeof(oldname_data) - 1] ='\0';

                if ((*errorhandle = loadhandle(section_data, read_name, read_value)) && !error)
                    error = lineno;
            } else if (!error) {
                /* otherwise set error to the current line number */
                error = lineno;
            }
        }
        lineno++;
    }
    mem_d(line);
    return error;
}

/*
 * returns true/false for a char that contains ("true" or "false" or numeric 0/1)
 */
static bool opts_ini_bool(const char *value) {
    if (!strcmp(value, "true"))  return true;
    if (!strcmp(value, "false")) return false;
    return !!strtol(value, NULL, 10);
}

static char *opts_ini_load(const char *section, const char *name, const char *value) {
    char *error = NULL;
    bool  found = false;

    /*
     * undef all of these because they may still be defined like in my
     * case they where.
     */
    #undef GMQCC_TYPE_FLAGS
    #undef GMQCC_TYPE_OPTIMIZATIONS
    #undef GMQCC_TYPE_WARNS

    /* flags */
    #define GMQCC_TYPE_FLAGS
    #define GMQCC_DEFINE_FLAG(X)                                       \
    if (!strcmp(section, "flags") && !strcmp(name, #X)) {              \
        opts_set(opts.flags, X, opts_ini_bool(value));                 \
        found = true;                                                  \
    }
    #include "opts.def"

    /* warnings */
    #define GMQCC_TYPE_WARNS
    #define GMQCC_DEFINE_FLAG(X)                                       \
    if (!strcmp(section, "warnings") && !strcmp(name, #X)) {           \
        opts_set(opts.warn, WARN_##X, opts_ini_bool(value));           \
        found = true;                                                  \
    }
    #include "opts.def"

    /* Werror-individuals */
    #define GMQCC_TYPE_WARNS
    #define GMQCC_DEFINE_FLAG(X)                                       \
    if (!strcmp(section, "errors") && !strcmp(name, #X)) {             \
        opts_set(opts.werror, WARN_##X, opts_ini_bool(value));         \
        found = true;                                                  \
    }
    #include "opts.def"

    /* optimizations */
    #define GMQCC_TYPE_OPTIMIZATIONS
    #define GMQCC_DEFINE_FLAG(X,Y)                                     \
    if (!strcmp(section, "optimizations") && !strcmp(name, #X)) {      \
        opts_set(opts.optimization, OPTIM_##X, opts_ini_bool(value));  \
        found = true;                                                  \
    }
    #include "opts.def"

    /* nothing was found ever! */
    if (!found) {
        if (strcmp(section, "flags")         &&
            strcmp(section, "warnings")      &&
            strcmp(section, "optimizations"))
        {
            vec_append(error, 17,             "invalid section `");
            vec_append(error, strlen(section), section);
            vec_push  (error, '`');
            vec_push  (error, '\0');
        } else {
            vec_append(error, 18,             "invalid variable `");
            vec_append(error, strlen(name), name);
            vec_push  (error, '`');
            vec_append(error, 14,              " in section: `");
            vec_append(error, strlen(section), section);
            vec_push  (error, '`');
            vec_push  (error, '\0');
        }
    }
    return error;
}

/*
 * Actual loading subsystem, this finds the ini or cfg file, and properly
 * loads it and executes it to set compiler options.
 */
void opts_ini_init(const char *file) {
    /*
     * Possible matches are:
     *  gmqcc.ini
     *  gmqcc.cfg
     */
    char       *error = NULL;
    size_t     line;
    fs_file_t  *ini;

    if (!file) {
        /* try ini */
        if (!(ini = fs_file_open((file = "gmqcc.ini"), "r")))
            /* try cfg */
            if (!(ini = fs_file_open((file = "gmqcc.cfg"), "r")))
                return;
    } else if (!(ini = fs_file_open(file, "r")))
        return;

    con_out("found ini file `%s`\n", file);

    if ((line = opts_ini_parse(ini, &opts_ini_load, &error)) != 0) {
        /* there was a parse error with the ini file */
        con_printmsg(LVL_ERROR, file, line, 0 /*TODO: column for ini error*/, "error", error);
        vec_free(error);
    }

    fs_file_close(ini);
}
