/*
 * Copyright (C) 2012
 *     Dale Weiler
 *     Wolfgang Bumiller
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

prog_section_statement *code_statements;
int                    *code_linenums;
prog_section_def       *code_defs;
prog_section_field     *code_fields;
prog_section_function  *code_functions;
int                    *code_globals;
char                   *code_chars;
uint16_t                code_crc;
uint32_t                code_entfields;

void code_push_statement(prog_section_statement *stmt, int linenum)
{
    vec_push(code_statements, *stmt);
    vec_push(code_linenums,   linenum);
}

void code_pop_statement()
{
    vec_pop(code_statements);
    vec_pop(code_linenums);
}

void code_init() {
    prog_section_function  empty_function  = {0,0,0,0,0,0,0,{0}};
    prog_section_statement empty_statement = {0,{0},{0},{0}};
    prog_section_def       empty_def       = {0, 0, 0};
    int                    i               = 0;

    code_entfields = 0;

    /*
     * The way progs.dat is suppose to work is odd, there needs to be
     * some null (empty) statements, functions, and 28 globals
     */
    for(; i < 28; i++)
        vec_push(code_globals, 0);

    vec_push(code_chars, '\0');
    vec_push(code_functions,  empty_function);
    code_push_statement(&empty_statement, 0);
    vec_push(code_defs,       empty_def);
    vec_push(code_fields,     empty_def);
}

uint32_t code_genstring(const char *str)
{
    uint32_t off = vec_size(code_chars);
    while (*str) {
        vec_push(code_chars, *str);
        ++str;
    }
    vec_push(code_chars, 0);
    return off;
}

uint32_t code_cachedstring(const char *str)
{
    size_t s = 0;
    /* We could implement knuth-morris-pratt or something
     * and also take substrings, but I'm uncomfortable with
     * pointing to subparts of strings for the sake of clarity...
     */
    while (s < vec_size(code_chars)) {
        if (!strcmp(str, code_chars + s))
            return s;
        while (code_chars[s]) ++s;
        ++s;
    }
    return code_genstring(str);
}

qcint code_alloc_field (size_t qcsize)
{
    qcint pos = (qcint)code_entfields;
    code_entfields += qcsize;
    return pos;
}

bool code_write(const char *filename, const char *lnofile) {
    prog_header  code_header;
    FILE        *fp           = NULL;
    size_t       it           = 2;

    code_header.statements.offset = sizeof(prog_header);
    code_header.statements.length = vec_size(code_statements);
    code_header.defs.offset       = code_header.statements.offset + (sizeof(prog_section_statement) * vec_size(code_statements));
    code_header.defs.length       = vec_size(code_defs);
    code_header.fields.offset     = code_header.defs.offset       + (sizeof(prog_section_def)       * vec_size(code_defs));
    code_header.fields.length     = vec_size(code_fields);
    code_header.functions.offset  = code_header.fields.offset     + (sizeof(prog_section_field)     * vec_size(code_fields));
    code_header.functions.length  = vec_size(code_functions);
    code_header.globals.offset    = code_header.functions.offset  + (sizeof(prog_section_function)  * vec_size(code_functions));
    code_header.globals.length    = vec_size(code_globals);
    code_header.strings.offset    = code_header.globals.offset    + (sizeof(int32_t)                * vec_size(code_globals));
    code_header.strings.length    = vec_size(code_chars);
    code_header.version           = 6;
    if (opts.forcecrc)
        code_header.crc16         = opts.forced_crc;
    else
        code_header.crc16         = code_crc;
    code_header.entfield          = code_entfields;

    if (OPTS_FLAG(DARKPLACES_STRING_TABLE_BUG)) {
        util_debug("GEN", "Patching stringtable for -fdarkplaces-stringtablebug\n");

        /* >= + P */
        vec_push(code_chars, '\0'); /* > */
        vec_push(code_chars, '\0'); /* = */
        vec_push(code_chars, '\0'); /* P */
    }

    if (lnofile) {
        uint32_t lnotype = *(unsigned int*)"LNOF";
        uint32_t version = 1;

        fp = util_fopen(lnofile, "wb");
        if (!fp)
            return false;

        if (fwrite(&lnotype,                        sizeof(lnotype),                        1,                       fp) != 1 ||
            fwrite(&version,                        sizeof(version),                        1,                       fp) != 1 ||
            fwrite(&code_header.defs.length,        sizeof(code_header.defs.length),        1,                       fp) != 1 ||
            fwrite(&code_header.globals.length,     sizeof(code_header.globals.length),     1,                       fp) != 1 ||
            fwrite(&code_header.fields.length,      sizeof(code_header.fields.length),      1,                       fp) != 1 ||
            fwrite(&code_header.statements.length,  sizeof(code_header.statements.length),  1,                       fp) != 1 ||
            fwrite(code_linenums,                   sizeof(code_linenums[0]),               vec_size(code_linenums), fp) != vec_size(code_linenums))
        {
            con_err("failed to write lno file\n");
        }

        fclose(fp);
        fp = NULL;
    }

    /* ensure all data is in LE format */
    util_endianswap(&code_header,    1,                         sizeof(prog_header));
    util_endianswap(code_statements, vec_size(code_statements), sizeof(prog_section_statement));
    util_endianswap(code_defs,       vec_size(code_defs),       sizeof(prog_section_def));
    util_endianswap(code_fields,     vec_size(code_fields),     sizeof(prog_section_field));
    util_endianswap(code_functions,  vec_size(code_functions),  sizeof(prog_section_function));
    util_endianswap(code_globals,    vec_size(code_globals),    sizeof(int32_t));

    fp = util_fopen(filename, "wb");
    if (!fp)
        return false;

    if (1                         != fwrite(&code_header,    sizeof(prog_header)           , 1                        , fp) ||
        vec_size(code_statements) != fwrite(code_statements, sizeof(prog_section_statement), vec_size(code_statements), fp) ||
        vec_size(code_defs)       != fwrite(code_defs,       sizeof(prog_section_def)      , vec_size(code_defs)      , fp) ||
        vec_size(code_fields)     != fwrite(code_fields,     sizeof(prog_section_field)    , vec_size(code_fields)    , fp) ||
        vec_size(code_functions)  != fwrite(code_functions,  sizeof(prog_section_function) , vec_size(code_functions) , fp) ||
        vec_size(code_globals)    != fwrite(code_globals,    sizeof(int32_t)               , vec_size(code_globals)   , fp) ||
        vec_size(code_chars)      != fwrite(code_chars,      1                             , vec_size(code_chars)     , fp))
    {
        fclose(fp);
        return false;
    }

    util_debug("GEN","HEADER:\n");
    util_debug("GEN","    version:    = %d\n", code_header.version );
    util_debug("GEN","    crc16:      = %d\n", code_header.crc16   );
    util_debug("GEN","    entfield:   = %d\n", code_header.entfield);
    util_debug("GEN","    statements  = {.offset = % 8d, .length = % 8d}\n", code_header.statements.offset, code_header.statements.length);
    util_debug("GEN","    defs        = {.offset = % 8d, .length = % 8d}\n", code_header.defs      .offset, code_header.defs      .length);
    util_debug("GEN","    fields      = {.offset = % 8d, .length = % 8d}\n", code_header.fields    .offset, code_header.fields    .length);
    util_debug("GEN","    functions   = {.offset = % 8d, .length = % 8d}\n", code_header.functions .offset, code_header.functions .length);
    util_debug("GEN","    globals     = {.offset = % 8d, .length = % 8d}\n", code_header.globals   .offset, code_header.globals   .length);
    util_debug("GEN","    strings     = {.offset = % 8d, .length = % 8d}\n", code_header.strings   .offset, code_header.strings   .length);

    /* FUNCTIONS */
    util_debug("GEN", "FUNCTIONS:\n");
    for (; it < vec_size(code_functions); it++) {
        size_t j = code_functions[it].entry;
        util_debug("GEN", "    {.entry =% 5d, .firstlocal =% 5d, .locals =% 5d, .profile =% 5d, .name =% 5d, .file =% 5d, .nargs =% 5d, .argsize ={%d,%d,%d,%d,%d,%d,%d,%d} }\n",
            code_functions[it].entry,
            code_functions[it].firstlocal,
            code_functions[it].locals,
            code_functions[it].profile,
            code_functions[it].name,
            code_functions[it].file,
            code_functions[it].nargs,
            code_functions[it].argsize[0],
            code_functions[it].argsize[1],
            code_functions[it].argsize[2],
            code_functions[it].argsize[3],
            code_functions[it].argsize[4],
            code_functions[it].argsize[5],
            code_functions[it].argsize[6],
            code_functions[it].argsize[7]

        );
        util_debug("GEN", "    NAME: %s\n", &code_chars[code_functions[it].name]);
        /* Internal functions have no code */
        if (code_functions[it].entry >= 0) {
            util_debug("GEN", "    CODE:\n");
            for (;;) {
                if (code_statements[j].opcode != INSTR_DONE)
                    util_debug("GEN", "        %-12s {% 5i,% 5i,% 5i}\n",
                        asm_instr[code_statements[j].opcode].m,
                        code_statements[j].o1.s1,
                        code_statements[j].o2.s1,
                        code_statements[j].o3.s1
                    );
                else {
                    util_debug("GEN", "        DONE  {0x00000,0x00000,0x00000}\n");
                    break;
                }
                j++;
            }
        }
    }

    vec_free(code_statements);
    vec_free(code_defs);
    vec_free(code_fields);
    vec_free(code_functions);
    vec_free(code_globals);
    vec_free(code_chars);
    fclose(fp);
    return true;
}
