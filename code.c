/*
 * Copyright (C) 2012, 2013
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
#include <string.h>
#include "gmqcc.h"

/*
 * We could use the old method of casting to uintptr_t then to void*
 * or qcint; however, it's incredibly unsafe for two reasons.
 * 1) The compilers aliasing optimization can legally make it unstable
 *    (it's undefined behaviour).
 * 
 * 2) The cast itself depends on fresh storage (newly allocated in which
 *    ever function is using the cast macros), the contents of which are
 *    transferred in a way that the obligation to release storage is not
 *    propagated.
 */
typedef union {
    void   *enter;
    qcint   leave;
} code_hash_entry_t;

/* Some sanity macros */
#define CODE_HASH_ENTER(ENTRY) ((ENTRY).enter)
#define CODE_HASH_LEAVE(ENTRY) ((ENTRY).leave)

void code_push_statement(code_t *code, prog_section_statement *stmt, int linenum)
{
    vec_push(code->statements, *stmt);
    vec_push(code->linenums,   linenum);
}

void code_pop_statement(code_t *code)
{
    vec_pop(code->statements);
    vec_pop(code->linenums);
}

code_t *code_init() {
    static prog_section_function  empty_function  = {0,0,0,0,0,0,0,{0,0,0,0,0,0,0,0}};
    static prog_section_statement empty_statement = {0,{0},{0},{0}};
    static prog_section_def       empty_def       = {0, 0, 0};

    code_t *code       = (code_t*)mem_a(sizeof(code_t));
    int     i          = 0;

    memset(code, 0, sizeof(code_t));
    code->entfields    = 0;
    code->string_cache = util_htnew(OPTS_OPTIMIZATION(OPTIM_OVERLAP_STRINGS) ? 0x100 : 1024);

    /*
     * The way progs.dat is suppose to work is odd, there needs to be
     * some null (empty) statements, functions, and 28 globals
     */
    for(; i < 28; i++)
        vec_push(code->globals, 0);

    vec_push(code->chars, '\0');
    vec_push(code->functions,  empty_function);

    code_push_statement(code, &empty_statement, 0);

    vec_push(code->defs,    empty_def);
    vec_push(code->fields,  empty_def);

    return code;
}

void *code_util_str_htgeth(hash_table_t *ht, const char *key, size_t bin);

uint32_t code_genstring(code_t *code, const char *str) {
    size_t            hash;
    code_hash_entry_t existing;

    if (!str)
        return 0;

    if (!*str) {
        if (!code->string_cached_empty) {
            code->string_cached_empty = vec_size(code->chars);
            vec_push(code->chars, 0);
        }
        return code->string_cached_empty;
    }

    if (OPTS_OPTIMIZATION(OPTIM_OVERLAP_STRINGS)) {
        hash                      = ((unsigned char*)str)[strlen(str)-1];
        CODE_HASH_ENTER(existing) = code_util_str_htgeth(code->string_cache, str, hash);
    } else {
        hash                      = util_hthash(code->string_cache, str);
        CODE_HASH_ENTER(existing) = util_htgeth(code->string_cache, str, hash);
    }

    if (CODE_HASH_ENTER(existing))
        return CODE_HASH_LEAVE(existing);

    CODE_HASH_LEAVE(existing) = vec_size(code->chars);
    vec_upload(code->chars, str, strlen(str)+1);

    util_htseth(code->string_cache, str, hash, CODE_HASH_ENTER(existing));
    return CODE_HASH_LEAVE(existing);
}

qcint code_alloc_field (code_t *code, size_t qcsize)
{
    qcint pos = (qcint)code->entfields;
    code->entfields += qcsize;
    return pos;
}

static void code_create_header(code_t *code, prog_header *code_header) {
    code_header->statements.offset = sizeof(prog_header);
    code_header->statements.length = vec_size(code->statements);
    code_header->defs.offset       = code_header->statements.offset + (sizeof(prog_section_statement) * vec_size(code->statements));
    code_header->defs.length       = vec_size(code->defs);
    code_header->fields.offset     = code_header->defs.offset       + (sizeof(prog_section_def)       * vec_size(code->defs));
    code_header->fields.length     = vec_size(code->fields);
    code_header->functions.offset  = code_header->fields.offset     + (sizeof(prog_section_field)     * vec_size(code->fields));
    code_header->functions.length  = vec_size(code->functions);
    code_header->globals.offset    = code_header->functions.offset  + (sizeof(prog_section_function)  * vec_size(code->functions));
    code_header->globals.length    = vec_size(code->globals);
    code_header->strings.offset    = code_header->globals.offset    + (sizeof(int32_t)                * vec_size(code->globals));
    code_header->strings.length    = vec_size(code->chars);
    code_header->version           = 6;

    if (OPTS_OPTION_BOOL(OPTION_FORCECRC))
        code_header->crc16         = OPTS_OPTION_U16(OPTION_FORCED_CRC);
    else
        code_header->crc16         = code->crc;
    code_header->entfield          = code->entfields;

    if (OPTS_FLAG(DARKPLACES_STRING_TABLE_BUG)) {
        util_debug("GEN", "Patching stringtable for -fdarkplaces-stringtablebug\n");

        /* >= + P */
        vec_push(code->chars, '\0'); /* > */
        vec_push(code->chars, '\0'); /* = */
        vec_push(code->chars, '\0'); /* P */
    }

    /* ensure all data is in LE format */
    util_endianswap(&code_header->version,    1, sizeof(code_header->version));
    util_endianswap(&code_header->crc16,      1, sizeof(code_header->crc16));
    util_endianswap(&code_header->statements, 2, sizeof(code_header->statements.offset));
    util_endianswap(&code_header->defs,       2, sizeof(code_header->statements.offset));
    util_endianswap(&code_header->fields,     2, sizeof(code_header->statements.offset));
    util_endianswap(&code_header->functions,  2, sizeof(code_header->statements.offset));
    util_endianswap(&code_header->strings,    2, sizeof(code_header->statements.offset));
    util_endianswap(&code_header->globals,    2, sizeof(code_header->statements.offset));
    util_endianswap(&code_header->entfield,   1, sizeof(code_header->entfield));

    /*
     * These are not part of the header but we ensure LE format here to save on duplicated
     * code.
     */  
    util_endianswap(code->statements, vec_size(code->statements), sizeof(prog_section_statement));
    util_endianswap(code->defs,       vec_size(code->defs),       sizeof(prog_section_def));
    util_endianswap(code->fields,     vec_size(code->fields),     sizeof(prog_section_field));
    util_endianswap(code->functions,  vec_size(code->functions),  sizeof(prog_section_function));
    util_endianswap(code->globals,    vec_size(code->globals),    sizeof(int32_t));
}

/*
 * Same principle except this one allocates memory and writes the lno(optional) and the dat file
 * directly out to allocated memory. Which is actually very useful for the future library support
 * we're going to add.
 */   
bool code_write_memory(code_t *code, uint8_t **datmem, size_t *sizedat, uint8_t **lnomem, size_t *sizelno) {
    prog_header code_header;
    uint32_t    offset  = 0;

    if (!datmem)
        return false;

    code_create_header(code, &code_header);

    #define WRITE_CHUNK(C,X,S)                                     \
        do {                                                       \
            memcpy((void*)(&(*C)[offset]), (const void*)(X), (S)); \
            offset += (S);                                         \
        } while (0)

    /* Calculate size required to store entire file out to memory */
    if (lnomem) {
        uint32_t version = 1;

        *sizelno += 4;               /* LNOF */
        *sizelno += sizeof(version);
        *sizelno += sizeof(code_header.defs.length);
        *sizelno += sizeof(code_header.globals.length);
        *sizelno += sizeof(code_header.fields.length);
        *sizelno += sizeof(code_header.statements.length);
        *sizelno += sizeof(code->linenums[0]) * vec_size(code->linenums);

        *lnomem   = (uint8_t*)mem_a(*sizelno);

        WRITE_CHUNK(lnomem, "LNOF",                         4);
        WRITE_CHUNK(lnomem, &version,                       sizeof(version));
        WRITE_CHUNK(lnomem, &code_header.defs.length,       sizeof(code_header.defs.length));
        WRITE_CHUNK(lnomem, &code_header.globals.length,    sizeof(code_header.globals.length));
        WRITE_CHUNK(lnomem, &code_header.fields.length,     sizeof(code_header.fields.length));
        WRITE_CHUNK(lnomem, &code_header.statements.length, sizeof(code_header.statements.length));

        /* something went terribly wrong */
        if (offset != *sizelno) {
            mem_d(*lnomem);
            *sizelno = 0;
            return false;
        }
        offset = 0;
    }

    /* Write out the dat */
    *sizedat += sizeof(prog_header);
    *sizedat += sizeof(prog_section_statement) * vec_size(code->statements);
    *sizedat += sizeof(prog_section_def)       * vec_size(code->defs);
    *sizedat += sizeof(prog_section_field)     * vec_size(code->fields);
    *sizedat += sizeof(prog_section_function)  * vec_size(code->functions);
    *sizedat += sizeof(int32_t)                * vec_size(code->globals);
    *sizedat += 1                              * vec_size(code->chars);

    *datmem = (uint8_t*)mem_a(*sizedat);

    WRITE_CHUNK(datmem, &code_header,     sizeof(prog_header));
    WRITE_CHUNK(datmem, code->statements, sizeof(prog_section_statement) * vec_size(code->statements));
    WRITE_CHUNK(datmem, code->defs,       sizeof(prog_section_def)       * vec_size(code->defs));
    WRITE_CHUNK(datmem, code->fields,     sizeof(prog_section_field)     * vec_size(code->fields));
    WRITE_CHUNK(datmem, code->functions,  sizeof(prog_section_function)  * vec_size(code->functions));
    WRITE_CHUNK(datmem, code->globals,    sizeof(int32_t)                * vec_size(code->globals));
    WRITE_CHUNK(datmem, code->chars,      1                              * vec_size(code->chars));

    #undef WRITE_CHUNK

    vec_free(code->statements);
    vec_free(code->linenums);
    vec_free(code->defs);
    vec_free(code->fields);
    vec_free(code->functions);
    vec_free(code->globals);
    vec_free(code->chars);

    util_htdel(code->string_cache);
    mem_d(code);

    return true;
}

bool code_write(code_t *code, const char *filename, const char *lnofile) {
    prog_header  code_header;
    FILE        *fp           = NULL;
    size_t       it           = 2;

    code_create_header(code, &code_header);

    if (lnofile) {
        uint32_t version = 1;

        fp = fs_file_open(lnofile, "wb");
        if (!fp)
            return false;

        util_endianswap(&version,      1,                         sizeof(version));
        util_endianswap(code->linenums, vec_size(code->linenums), sizeof(code->linenums[0]));


        if (fs_file_write("LNOF",                          4,                                      1,                        fp) != 1 ||
            fs_file_write(&version,                        sizeof(version),                        1,                        fp) != 1 ||
            fs_file_write(&code_header.defs.length,        sizeof(code_header.defs.length),        1,                        fp) != 1 ||
            fs_file_write(&code_header.globals.length,     sizeof(code_header.globals.length),     1,                        fp) != 1 ||
            fs_file_write(&code_header.fields.length,      sizeof(code_header.fields.length),      1,                        fp) != 1 ||
            fs_file_write(&code_header.statements.length,  sizeof(code_header.statements.length),  1,                        fp) != 1 ||
            fs_file_write(code->linenums,                  sizeof(code->linenums[0]),              vec_size(code->linenums), fp) != vec_size(code->linenums))
        {
            con_err("failed to write lno file\n");
        }

        fs_file_close(fp);
        fp = NULL;
    }

    fp = fs_file_open(filename, "wb");
    if (!fp)
        return false;

    if (1                          != fs_file_write(&code_header,     sizeof(prog_header)           , 1                         , fp) ||
        vec_size(code->statements) != fs_file_write(code->statements, sizeof(prog_section_statement), vec_size(code->statements), fp) ||
        vec_size(code->defs)       != fs_file_write(code->defs,       sizeof(prog_section_def)      , vec_size(code->defs)      , fp) ||
        vec_size(code->fields)     != fs_file_write(code->fields,     sizeof(prog_section_field)    , vec_size(code->fields)    , fp) ||
        vec_size(code->functions)  != fs_file_write(code->functions,  sizeof(prog_section_function) , vec_size(code->functions) , fp) ||
        vec_size(code->globals)    != fs_file_write(code->globals,    sizeof(int32_t)               , vec_size(code->globals)   , fp) ||
        vec_size(code->chars)      != fs_file_write(code->chars,      1                             , vec_size(code->chars)     , fp))
    {
        fs_file_close(fp);
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
    for (; it < vec_size(code->functions); it++) {
        size_t j = code->functions[it].entry;
        util_debug("GEN", "    {.entry =% 5d, .firstlocal =% 5d, .locals =% 5d, .profile =% 5d, .name =% 5d, .file =% 5d, .nargs =% 5d, .argsize ={%d,%d,%d,%d,%d,%d,%d,%d} }\n",
            code->functions[it].entry,
            code->functions[it].firstlocal,
            code->functions[it].locals,
            code->functions[it].profile,
            code->functions[it].name,
            code->functions[it].file,
            code->functions[it].nargs,
            code->functions[it].argsize[0],
            code->functions[it].argsize[1],
            code->functions[it].argsize[2],
            code->functions[it].argsize[3],
            code->functions[it].argsize[4],
            code->functions[it].argsize[5],
            code->functions[it].argsize[6],
            code->functions[it].argsize[7]

        );
        util_debug("GEN", "    NAME: %s\n", &code->chars[code->functions[it].name]);
        /* Internal functions have no code */
        if (code->functions[it].entry >= 0) {
            util_debug("GEN", "    CODE:\n");
            for (;;) {
                if (code->statements[j].opcode != INSTR_DONE)
                    util_debug("GEN", "        %-12s {% 5i,% 5i,% 5i}\n",
                        asm_instr[code->statements[j].opcode].m,
                        code->statements[j].o1.s1,
                        code->statements[j].o2.s1,
                        code->statements[j].o3.s1
                    );
                else {
                    util_debug("GEN", "        DONE  {0x00000,0x00000,0x00000}\n");
                    break;
                }
                j++;
            }
        }
    }

    fs_file_close(fp);
    return true;
}

void code_cleanup(code_t *code) {
    vec_free(code->statements);
    vec_free(code->linenums);
    vec_free(code->defs);
    vec_free(code->fields);
    vec_free(code->functions);
    vec_free(code->globals);
    vec_free(code->chars);

    util_htdel(code->string_cache);

    mem_d(code);
}
