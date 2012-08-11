#ifndef TEST_AST_MACROS_HDR
#define TEST_AST_MACROS_HDR

#define TESTVARS()   \
ast_block *curblock; \
lex_ctx    ctx

#define TESTINIT()   \
ctx.file = NULL;     \
ctx.line = 1;

#define DEFVAR(name) \
ast_value *name

#define VAR(type, name) \
name = ast_value_new(ctx, #name, type)

#define VARnamed(type, name, varname) \
name = ast_value_new(ctx, #varname, type)

#define MKGLOBAL(name) \
assert(globals_add(name) >= 0)

#define FIELD(type, name) \
name = ast_value_new(ctx, #name, TYPE_FIELD);                  \
do {                                                           \
    ast_value *field_##name = ast_value_new(ctx, #name, type); \
    name->expression.next = (ast_expression*)field_##name;     \
    MKFIELD(name);                                             \
} while (0)

#define MKFIELD(name) \
assert(fields_add(name) >= 0)

#define MKCONSTFLOAT(name, value)  \
do {                               \
    name->isconst = true;          \
    name->constval.vfloat = value; \
    MKGLOBAL(name);                \
} while(0)

#define MKCONSTSTRING(name, value)               \
do {                                             \
    name->isconst = true;                        \
    name->constval.vstring = util_strdup(value); \
    MKGLOBAL(name);                              \
} while(0)

#define STATE(a)                                 \
do {                                             \
    ast_expression *exp = (ast_expression*)(a);  \
    assert(ast_block_exprs_add(curblock, exp)); \
} while(0)

#define ASSIGN(op, a, b) \
(ast_expression*)ast_store_new(ctx, INSTR_##op, (ast_expression*)(a), (ast_expression*)(b))

#define BIN(op, a, b) \
(ast_expression*)ast_binary_new(ctx, INSTR_##op, (ast_expression*)(a), (ast_expression*)(b))

#define ENTFIELD(a, b) \
(ast_expression*)ast_entfield_new(ctx, (ast_expression*)(a), (ast_expression*)(b))

#define CALL(what)                                             \
do {                                                           \
    ast_call *call = ast_call_new(ctx, (ast_expression*)what); \

#define CALLPARAM(x)                                       \
    assert(ast_call_params_add(call, (ast_expression*)x));

#define ENDCALL()                                \
    STATE(call);                                 \
} while(0)

#define ENDCALLWITH(as, where)                      \
    {                                               \
        ast_expression *as = (ast_expression*)call; \
        where;                                      \
    }                                               \
} while(0)

#define WHILE(cond)                                    \
do {                                                   \
    ast_expression *wh_cond = (ast_expression*)(cond); \
    ast_block *wh_body = ast_block_new(ctx);           \
    ast_block *oldcur = curblock;                      \
    ast_loop  *loop;                                   \
    curblock = wh_body;

#define ENDWHILE()                                             \
    curblock = oldcur;                                         \
    loop = ast_loop_new(ctx, NULL, (ast_expression*)wh_cond,   \
                        NULL, NULL, (ast_expression*)wh_body); \
    assert(loop);                                              \
    STATE(loop);                                               \
} while(0)

#define BUILTIN(name, outtype, number)                              \
do {                                                                \
    ast_function *func_##name;                                      \
    ast_value    *thisfuncval;                                      \
    ast_function *thisfunc;                                         \
    DEFVAR(return_##name);                                          \
    VARnamed(TYPE_FUNCTION, name, name);                            \
    VARnamed(outtype, return_##name, "#returntype");                \
    name->expression.next = (ast_expression*)return_##name;         \
    MKGLOBAL(name);                                                 \
    func_##name = ast_function_new(ctx, #name, name);               \
    thisfunc = func_##name;                                         \
    (void)thisfunc;                                                 \
    thisfuncval = name;                                             \
    (void)thisfuncval;                                              \
    assert(functions_add(func_##name) >= 0);                        \
    func_##name->builtin = number;

#define ENDBUILTIN() } while(0)

#define PARAM(ptype, name)                           \
do {                                                 \
    DEFVAR(parm);                                    \
    VARnamed(ptype, parm, name);                     \
    assert(ast_value_params_add(thisfuncval, parm)); \
} while(0)

#define FUNCTION(name, outtype)                                   \
do {                                                              \
    ast_function *thisfunc;                                       \
    ast_function *func_##name;                                    \
    ast_block    *my_funcblock;                                   \
    DEFVAR(var_##name);                                           \
    DEFVAR(return_##name);                                        \
    VARnamed(TYPE_FUNCTION, var_##name, name);                    \
    VARnamed(outtype, return_##name, "#returntype");              \
    var_##name->expression.next = (ast_expression*)return_##name; \
    MKGLOBAL(var_##name);                                         \
    func_##name = ast_function_new(ctx, #name, var_##name);       \
    thisfunc = func_##name;                                       \
    (void)thisfunc;                                               \
    assert(functions_add(func_##name) >= 0);                      \
    my_funcblock = ast_block_new(ctx);                            \
    assert(my_funcblock);                                         \
    assert(ast_function_blocks_add(func_##name, my_funcblock));   \
    curblock = my_funcblock;

#define MKLOCAL(var) \
    assert(ast_block_locals_add(curblock, var))

#define ENDFUNCTION(name) \
} while(0)

#endif
