#ifndef GMQCC_IR_HDR
#define GMQCC_IR_HDR
#include "gmqcc.h"

/*
 * Type large enough to hold all the possible IR flags. This should be
 * changed if the static assertion at the end of this file fails.
 */
typedef uint8_t ir_flag_t;

struct ir_value;
struct ir_instr;
struct ir_block;
struct ir_function;
struct ir_builder;

struct ir_life_entry_t {
    /* both inclusive */
    size_t start;
    size_t end;
};

enum {
    IR_FLAG_HAS_ARRAYS              = 1 << 0,
    IR_FLAG_HAS_UNINITIALIZED       = 1 << 1,
    IR_FLAG_HAS_GOTO                = 1 << 2,
    IR_FLAG_INCLUDE_DEF             = 1 << 3,
    IR_FLAG_ERASABLE                = 1 << 4,
    IR_FLAG_BLOCK_COVERAGE          = 1 << 5,

    IR_FLAG_SPLIT_VECTOR            = 1 << 6,

    IR_FLAG_LAST,
    IR_FLAG_MASK_NO_OVERLAP      = (IR_FLAG_HAS_ARRAYS | IR_FLAG_HAS_UNINITIALIZED),
    IR_FLAG_MASK_NO_LOCAL_TEMPS  = (IR_FLAG_HAS_ARRAYS | IR_FLAG_HAS_UNINITIALIZED)
};

struct ir_value {
    ir_value(std::string&& name, store_type storetype, qc_type vtype);
    ~ir_value();

    void* operator new(std::size_t); // to use mem_a
    void operator delete(void*); // to use mem_d

    std::string name;

    qc_type vtype;
    store_type store;
    lex_ctx_t context;
    qc_type fieldtype; // even the IR knows the subtype of a field
    qc_type outtype;   // and the output type of a function
    int cvq;       // 'const' vs 'var' qualifier
    ir_flag_t flags;

    std::vector<ir_instr *> reads;
    std::vector<ir_instr *> writes;

    // constant values
    bool hasvalue;
    union {
        qcfloat_t    vfloat;
        int          vint;
        vec3_t       vvec;
        int32_t      ivec[3];
        char        *vstring;
        ir_value    *vpointer;
        ir_function *vfunc;
    } constval;

    struct {
        int32_t globaladdr;
        int32_t name;
        int32_t local;         // filled by the local-allocator
        int32_t addroffset;    // added for members
        int32_t fieldaddr;     // to generate field-addresses early
    } code;

    // for accessing vectors
    ir_value *members[3];
    ir_value *memberof;

    bool unique_life;      // arrays will never overlap with temps
    bool locked;           // temps living during a CALL must be locked
    bool callparam;

    std::vector<ir_life_entry_t> life; // For the temp allocator
};

/*
 * ir_value can be a variable, or created by an operation
 * if a result of an operation: the function should store
 * it to remember to delete it / garbage collect it
 */
ir_value*       ir_value_vector_member(ir_value*, unsigned int member);
bool GMQCC_WARN ir_value_set_float(ir_value*, float f);
bool GMQCC_WARN ir_value_set_func(ir_value*, int f);
bool GMQCC_WARN ir_value_set_string(ir_value*, const char *s);
bool GMQCC_WARN ir_value_set_vector(ir_value*, vec3_t v);
bool GMQCC_WARN ir_value_set_field(ir_value*, ir_value *fld);
bool            ir_value_lives(ir_value*, size_t);
void            ir_value_dump_life(const ir_value *self, int (*oprintf)(const char*,...));

/* PHI data */
struct ir_phi_entry_t {
    ir_value *value;
    ir_block *from;
};

/* instruction */
struct ir_instr {
    int opcode;
    lex_ctx_t context;
    ir_value *(_ops[3]);
    ir_block *(bops[2]);

    std::vector<ir_phi_entry_t> phi;
    std::vector<ir_value *> params;

    // For the temp-allocation
    size_t eid;

    // For IFs
    bool likely;

    ir_block *owner;
};

/* block */
struct ir_block {
    void* operator new(std::size_t);
    void operator delete(void*);

    ir_block(ir_function *owner, const std::string& name);
    ~ir_block();

    ir_function *owner;
    std::string label;

    lex_ctx_t context;
    bool final = false; /* once a jump is added we're done */

    ir_instr **instr = nullptr;
    ir_block **entries = nullptr;
    ir_block **exits = nullptr;
    std::vector<ir_value *> living;

    /* For the temp-allocation */
    size_t entry_id  = 0;
    size_t eid       = 0;
    bool   is_return = false;

    bool generated = false;
    size_t code_start = 0;
};

ir_value*       ir_block_create_binop(ir_block*, lex_ctx_t, const char *label, int op, ir_value *left, ir_value *right);
ir_value*       ir_block_create_unary(ir_block*, lex_ctx_t, const char *label, int op, ir_value *operand);
bool GMQCC_WARN ir_block_create_store_op(ir_block*, lex_ctx_t, int op, ir_value *target, ir_value *what);
bool GMQCC_WARN ir_block_create_storep(ir_block*, lex_ctx_t, ir_value *target, ir_value *what);
ir_value*       ir_block_create_load_from_ent(ir_block*, lex_ctx_t, const char *label, ir_value *ent, ir_value *field, qc_type outype);
ir_value*       ir_block_create_fieldaddress(ir_block*, lex_ctx_t, const char *label, ir_value *entity, ir_value *field);
bool GMQCC_WARN ir_block_create_state_op(ir_block*, lex_ctx_t, ir_value *frame, ir_value *think);

/* This is to create an instruction of the form
 * <outtype>%label := opcode a, b
 */
ir_instr* ir_block_create_phi(ir_block*, lex_ctx_t, const char *label, qc_type vtype);
ir_value* ir_phi_value(ir_instr*);
void ir_phi_add(ir_instr*, ir_block *b, ir_value *v);
ir_instr* ir_block_create_call(ir_block*, lex_ctx_t, const char *label, ir_value *func, bool noreturn);
ir_value* ir_call_value(ir_instr*);
void ir_call_param(ir_instr*, ir_value*);

bool GMQCC_WARN ir_block_create_return(ir_block*, lex_ctx_t, ir_value *opt_value);

bool GMQCC_WARN ir_block_create_if(ir_block*, lex_ctx_t, ir_value *cond,
                                   ir_block *ontrue, ir_block *onfalse);
/*
 * A 'goto' is an actual 'goto' coded in QC, whereas
 * a 'jump' is a virtual construct which simply names the
 * next block to go to.
 * A goto usually becomes an OP_GOTO in the resulting code,
 * whereas a 'jump' usually doesn't add any actual instruction.
 */
bool GMQCC_WARN ir_block_create_jump(ir_block*, lex_ctx_t, ir_block *to);
bool GMQCC_WARN ir_block_create_goto(ir_block*, lex_ctx_t, ir_block *to);

/* function */
struct ir_function {
    void* operator new(std::size_t);
    void operator delete(void*);

    ir_function(ir_builder *owner, qc_type returntype);
    ~ir_function();

    ir_builder *owner;

    std::string name;
    qc_type     outtype;
    int        *params  = nullptr;
    ir_flag_t   flags   = 0;
    int         builtin = 0;

    std::vector<std::unique_ptr<ir_block>> blocks;

    /*
     * values generated from operations
     * which might get optimized away, so anything
     * in there needs to be deleted in the dtor.
     */
    std::vector<std::unique_ptr<ir_value>> values;
    std::vector<std::unique_ptr<ir_value>> locals;     /* locally defined variables */
    ir_value *value = nullptr;

    size_t allocated_locals = 0;
    size_t globaltemps      = 0;

    ir_block*  first = nullptr;
    ir_block*  last  = nullptr;

    lex_ctx_t  context;

    /*
     * for prototypes - first we generate all the
     * globals, and we remember teh function-defs
     * so we can later fill in the entry pos
     *
     * remember the ID:
     */
    qcint_t code_function_def = -1;

    /* for temp allocation */
    size_t run_id = 0;

    /* vararg support: */
    size_t max_varargs = 0;
};


ir_value*       ir_function_create_local(ir_function *self, const std::string& name, qc_type vtype, bool param);
bool GMQCC_WARN ir_function_finalize(ir_function*);
ir_block*       ir_function_create_block(lex_ctx_t ctx, ir_function*, const char *label);

/* builder */
#define IR_HT_SIZE          1024
#define IR_MAX_VINSTR_TEMPS 1

struct ir_builder {
    void* operator new(std::size_t);
    void operator delete(void*);
    ir_builder(const std::string& modulename);
    ~ir_builder();

    std::string name;
    std::vector<std::unique_ptr<ir_function>> functions;
    std::vector<std::unique_ptr<ir_value>>    globals;
    std::vector<std::unique_ptr<ir_value>>    fields;
    // for reusing them in vector-splits, TODO: sort this or use a radix-tree
    std::vector<ir_value*>                    const_floats;

    ht            htfunctions;
    ht            htglobals;
    ht            htfields;

    std::vector<std::unique_ptr<ir_value>> extparams;
    std::vector<std::unique_ptr<ir_value>> extparam_protos;

    // the highest func->allocated_locals
    size_t        max_locals              = 0;
    size_t        max_globaltemps         = 0;
    uint32_t      first_common_local      = 0;
    uint32_t      first_common_globaltemp = 0;

    std::vector<const char*> filenames;
    std::vector<qcint_t>     filestrings;

    // we cache the #IMMEDIATE string here
    qcint_t      str_immediate = 0;

    // there should just be this one nil
    ir_value    *nil;
    ir_value    *reserved_va_count = nullptr;
    ir_value    *coverage_func = nullptr;

    /* some virtual instructions require temps, and their code is isolated
     * so that we don't need to keep track of their liveness.
     */
    ir_value    *vinstr_temp[IR_MAX_VINSTR_TEMPS];

    /* code generator */
    code_t      *code;
};

ir_function* ir_builder_create_function(ir_builder*, const std::string& name, qc_type outtype);
ir_value*    ir_builder_create_global(ir_builder*, const std::string& name, qc_type vtype);
ir_value*    ir_builder_create_field(ir_builder*, const std::string& name, qc_type vtype);
ir_value*    ir_builder_get_va_count(ir_builder*);
bool         ir_builder_generate(ir_builder *self, const char *filename);
void         ir_builder_dump(ir_builder*, int (*oprintf)(const char*, ...));

/*
 * This code assumes 32 bit floats while generating binary
 * Blub: don't use extern here, it's annoying and shows up in nm
 * for some reason :P
 */
typedef int static_assert_is_32bit_float  [(sizeof(int32_t) == 4)   ? 1 : -1];
typedef int static_assert_is_32bit_integer[(sizeof(qcfloat_t) == 4) ? 1 : -1];

/*
 * If the condition creates a situation where this becomes -1 size it means there are
 * more IR_FLAGs than the type ir_flag_t is capable of holding. So either eliminate
 * the IR flag count or change the ir_flag_t typedef to a type large enough to accomodate
 * all the flags.
 */
typedef int static_assert_is_ir_flag_safe [((IR_FLAG_LAST) <= (ir_flag_t)(-1)) ? 1 : -1];

#endif
