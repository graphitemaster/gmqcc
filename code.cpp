#include <string.h>
#include "gmqcc.h"

/*
 * We could use the old method of casting to uintptr_t then to void*
 * or qcint_t; however, it's incredibly unsafe for two reasons.
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
    qcint_t leave;
} code_hash_entry_t;

/* Some sanity macros */
#define CODE_HASH_ENTER(ENTRY) ((ENTRY).enter)
#define CODE_HASH_LEAVE(ENTRY) ((ENTRY).leave)

void code_push_statement(code_t *code, prog_section_statement_t *stmt_in, lex_ctx_t ctx)
{
    prog_section_statement_t stmt = *stmt_in;

    if (OPTS_FLAG(TYPELESS_STORES)) {
        switch (stmt.opcode) {
            case INSTR_LOAD_S:
            case INSTR_LOAD_ENT:
            case INSTR_LOAD_FLD:
            case INSTR_LOAD_FNC:
                stmt.opcode = INSTR_LOAD_F;
                break;
            case INSTR_STORE_S:
            case INSTR_STORE_ENT:
            case INSTR_STORE_FLD:
            case INSTR_STORE_FNC:
                stmt.opcode = INSTR_STORE_F;
                break;
            case INSTR_STOREP_S:
            case INSTR_STOREP_ENT:
            case INSTR_STOREP_FLD:
            case INSTR_STOREP_FNC:
                stmt.opcode = INSTR_STOREP_F;
                break;
        }
    }


    if (OPTS_FLAG(SORT_OPERANDS)) {
        uint16_t pair;

        switch (stmt.opcode) {
            case INSTR_MUL_F:
            case INSTR_MUL_V:
            case INSTR_ADD_F:
            case INSTR_EQ_F:
            case INSTR_EQ_S:
            case INSTR_EQ_E:
            case INSTR_EQ_FNC:
            case INSTR_NE_F:
            case INSTR_NE_V:
            case INSTR_NE_S:
            case INSTR_NE_E:
            case INSTR_NE_FNC:
            case INSTR_AND:
            case INSTR_OR:
            case INSTR_BITAND:
            case INSTR_BITOR:
                if (stmt.o1.u1 < stmt.o2.u1) {
                    uint16_t a = stmt.o2.u1;
                    stmt.o1.u1 = stmt.o2.u1;
                    stmt.o2.u1 = a;
                }
                break;

            case INSTR_MUL_VF: pair = INSTR_MUL_FV; goto case_pair_gen;
            case INSTR_MUL_FV: pair = INSTR_MUL_VF; goto case_pair_gen;
            case INSTR_LT:     pair = INSTR_GT;     goto case_pair_gen;
            case INSTR_GT:     pair = INSTR_LT;     goto case_pair_gen;
            case INSTR_LE:     pair = INSTR_GE;     goto case_pair_gen;
            case INSTR_GE:     pair = INSTR_LE;

            case_pair_gen:
                if (stmt.o1.u1 < stmt.o2.u1) {
                    uint16_t x  = stmt.o1.u1;
                    stmt.o1.u1  = stmt.o2.u1;
                    stmt.o2.u1  = x;
                    stmt.opcode = pair;
                }
                break;
        }
    }

    code->statements.push_back(stmt);
    code->linenums.push_back(ctx.line);
    code->columnnums.push_back(ctx.column);
}

void code_pop_statement(code_t *code)
{
    code->statements.pop_back();
    code->linenums.pop_back();
    code->columnnums.pop_back();
}

void *code_t::operator new(std::size_t bytes) {
  return mem_a(bytes);
}

void code_t::operator delete(void *ptr) {
  mem_d(ptr);
}

code_t::code_t()
{
    static lex_ctx_t                empty_ctx       = {0, 0, 0};
    static prog_section_function_t  empty_function  = {0,0,0,0,0,0,0,{0,0,0,0,0,0,0,0}};
    static prog_section_statement_t empty_statement = {0,{0},{0},{0}};
    static prog_section_def_t       empty_def       = {0, 0, 0};

    string_cache = util_htnew(OPTS_OPTIMIZATION(OPTIM_OVERLAP_STRINGS) ? 0x100 : 1024);

    // The way progs.dat is suppose to work is odd, there needs to be
    // some null (empty) statements, functions, and 28 globals
    globals.insert(globals.begin(), 28, 0);

    chars.push_back('\0');
    functions.push_back(empty_function);

    code_push_statement(this, &empty_statement, empty_ctx);

    defs.push_back(empty_def);
    fields.push_back(empty_def);
}

code_t::~code_t()
{
    util_htdel(string_cache);
}

void *code_util_str_htgeth(hash_table_t *ht, const char *key, size_t bin);

uint32_t code_genstring(code_t *code, const char *str) {
    size_t            hash;
    code_hash_entry_t existing;

    if (!str)
        return 0;

    if (!*str) {
        if (!code->string_cached_empty) {
            code->string_cached_empty = code->chars.size();
            code->chars.push_back(0);
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

    CODE_HASH_LEAVE(existing) = code->chars.size();
    code->chars.insert(code->chars.end(), str, str + strlen(str) + 1);

    util_htseth(code->string_cache, str, hash, CODE_HASH_ENTER(existing));
    return CODE_HASH_LEAVE(existing);
}

qcint_t code_alloc_field (code_t *code, size_t qcsize)
{
    qcint_t pos = (qcint_t)code->entfields;
    code->entfields += qcsize;
    return pos;
}

static size_t code_size_generic(code_t *code, prog_header_t *code_header, bool lno) {
    size_t size = 0;
    if (lno) {
        size += 4;  /* LNOF */
        size += sizeof(uint32_t); /* version */
        size += sizeof(code_header->defs.length);
        size += sizeof(code_header->globals.length);
        size += sizeof(code_header->fields.length);
        size += sizeof(code_header->statements.length);
        size += sizeof(code->linenums[0])   * code->linenums.size();
        size += sizeof(code->columnnums[0]) * code->columnnums.size();
    } else {
        size += sizeof(prog_header_t);
        size += sizeof(prog_section_statement_t) * code->statements.size();
        size += sizeof(prog_section_def_t)       * code->defs.size();
        size += sizeof(prog_section_field_t)     * code->fields.size();
        size += sizeof(prog_section_function_t)  * code->functions.size();
        size += sizeof(int32_t)                  * code->globals.size();
        size += 1                                * code->chars.size();
    }
    return size;
}

#define code_size_binary(C, H) code_size_generic((C), (H), false)
#define code_size_debug(C, H)  code_size_generic((C), (H), true)

static void code_create_header(code_t *code, prog_header_t *code_header, const char *filename, const char *lnofile) {
    size_t i;

    code_header->statements.offset = sizeof(prog_header_t);
    code_header->statements.length = code->statements.size();
    code_header->defs.offset       = code_header->statements.offset + (sizeof(prog_section_statement_t) * code->statements.size());
    code_header->defs.length       = code->defs.size();
    code_header->fields.offset     = code_header->defs.offset       + (sizeof(prog_section_def_t)       * code->defs.size());
    code_header->fields.length     = code->fields.size();
    code_header->functions.offset  = code_header->fields.offset     + (sizeof(prog_section_field_t)     * code->fields.size());
    code_header->functions.length  = code->functions.size();
    code_header->globals.offset    = code_header->functions.offset  + (sizeof(prog_section_function_t)  * code->functions.size());
    code_header->globals.length    = code->globals.size();
    code_header->strings.offset    = code_header->globals.offset    + (sizeof(int32_t)                  * code->globals.size());
    code_header->strings.length    = code->chars.size();
    code_header->version           = 6;
    code_header->skip              = 0;

    if (OPTS_OPTION_BOOL(OPTION_FORCECRC))
        code_header->crc16         = OPTS_OPTION_U16(OPTION_FORCED_CRC);
    else
        code_header->crc16         = code->crc;
    code_header->entfield          = code->entfields;

    if (OPTS_FLAG(DARKPLACES_STRING_TABLE_BUG)) {
        /* >= + P */
        code->chars.push_back('\0'); /* > */
        code->chars.push_back('\0'); /* = */
        code->chars.push_back('\0'); /* P */
    }

    /* ensure all data is in LE format */
    util_swap_header(*code_header);
    util_swap_statements(code->statements);
    util_swap_defs_fields(code->defs);
    util_swap_defs_fields(code->fields);
    util_swap_functions(code->functions);
    util_swap_globals(code->globals);

    if (!OPTS_OPTION_BOOL(OPTION_QUIET)) {
        if (lnofile)
            con_out("writing '%s' and '%s'...\n", filename, lnofile);
        else
            con_out("writing '%s'\n", filename);
    }

    if (!OPTS_OPTION_BOOL(OPTION_QUIET) &&
        !OPTS_OPTION_BOOL(OPTION_PP_ONLY))
    {
        char buffer[1024];
        con_out("\nOptimizations:\n");
        for (i = 0; i < COUNT_OPTIMIZATIONS; ++i) {
            if (opts_optimizationcount[i]) {
                util_optimizationtostr(opts_opt_list[i].name, buffer, sizeof(buffer));
                con_out(
                    "    %s: %u\n",
                    buffer,
                    (unsigned int)opts_optimizationcount[i]
                );
            }
        }
    }
}

static void code_stats(const char *filename, const char *lnofile, code_t *code, prog_header_t *code_header) {
    if (OPTS_OPTION_BOOL(OPTION_QUIET) ||
        OPTS_OPTION_BOOL(OPTION_PP_ONLY))
            return;

    con_out("\nFile statistics:\n");
    con_out("    dat:\n");
    con_out("        name: %s\n",         filename);
    con_out("        size: %u (bytes)\n", code_size_binary(code, code_header));
    con_out("        crc:  0x%04X\n",     code->crc);

    if (lnofile) {
        con_out("    lno:\n");
        con_out("        name: %s\n",  lnofile);
        con_out("        size: %u (bytes)\n",  code_size_debug(code, code_header));
    }

    con_out("\n");
}

bool code_write(code_t *code, const char *filename, const char *lnofile) {
    prog_header_t code_header;
    FILE *fp = nullptr;

    code_create_header(code, &code_header, filename, lnofile);

    if (lnofile) {
        uint32_t version = 1;

        fp = fopen(lnofile, "wb");
        if (!fp)
            return false;

        util_endianswap(&version,             1,                       sizeof(version));
        util_endianswap(&code->linenums[0],   code->linenums.size(),   sizeof(code->linenums[0]));
        util_endianswap(&code->columnnums[0], code->columnnums.size(), sizeof(code->columnnums[0]));

        if (fwrite("LNOF",                          4,                                      1,                          fp) != 1 ||
            fwrite(&version,                        sizeof(version),                        1,                          fp) != 1 ||
            fwrite(&code_header.defs.length,        sizeof(code_header.defs.length),        1,                          fp) != 1 ||
            fwrite(&code_header.globals.length,     sizeof(code_header.globals.length),     1,                          fp) != 1 ||
            fwrite(&code_header.fields.length,      sizeof(code_header.fields.length),      1,                          fp) != 1 ||
            fwrite(&code_header.statements.length,  sizeof(code_header.statements.length),  1,                          fp) != 1 ||
            fwrite(&code->linenums[0],              sizeof(code->linenums[0]),              code->linenums.size(),      fp) != code->linenums.size() ||
            fwrite(&code->columnnums[0],            sizeof(code->columnnums[0]),            code->columnnums.size(),    fp) != code->columnnums.size())
        {
            con_err("failed to write lno file\n");
        }

        fclose(fp);
        fp = nullptr;
    }

    fp = fopen(filename, "wb");
    if (!fp)
        return false;

    if (1                       != fwrite(&code_header,         sizeof(prog_header_t)           , 1                      , fp) ||
        code->statements.size() != fwrite(&code->statements[0], sizeof(prog_section_statement_t), code->statements.size(), fp) ||
        code->defs.size()       != fwrite(&code->defs[0],       sizeof(prog_section_def_t)      , code->defs.size()      , fp) ||
        code->fields.size()     != fwrite(&code->fields[0],     sizeof(prog_section_field_t)    , code->fields.size()    , fp) ||
        code->functions.size()  != fwrite(&code->functions[0],  sizeof(prog_section_function_t) , code->functions.size() , fp) ||
        code->globals.size()    != fwrite(&code->globals[0],    sizeof(int32_t)                 , code->globals.size()   , fp) ||
        code->chars.size()      != fwrite(&code->chars[0],      1                               , code->chars.size()     , fp))
    {
        fclose(fp);
        return false;
    }

    fclose(fp);
    code_stats(filename, lnofile, code, &code_header);
    return true;
}
