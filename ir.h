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
    IR_FLAG_NOREF                   = 1 << 6,
    IR_FLAG_SPLIT_VECTOR            = 1 << 7,

    IR_FLAG_LAST,
    IR_FLAG_MASK_NO_OVERLAP      = (IR_FLAG_HAS_ARRAYS | IR_FLAG_HAS_UNINITIALIZED),
    IR_FLAG_MASK_NO_LOCAL_TEMPS  = (IR_FLAG_HAS_ARRAYS | IR_FLAG_HAS_UNINITIALIZED)
};

struct ir_value {
    ir_value(std::string&& name, store_type storetype, qc_type vtype);
    ir_value(ir_function *owner, std::string&& name, store_type storetype, qc_type vtype);
    ~ir_value();

    ir_value *vectorMember(unsigned int member);

    bool GMQCC_WARN setFloat(float);
    bool GMQCC_WARN setFunc(int);
    bool GMQCC_WARN setString(const char*);
    bool GMQCC_WARN setVector(vec3_t);
    bool GMQCC_WARN setField(ir_value*);
#if 0
    bool GMQCC_WARN setInt(int);
#endif

    bool lives(size_t at);
    void dumpLife(int (*oprintf)(const char*, ...)) const;

    void setCodeAddress(int32_t gaddr);
    int32_t codeAddress() const;

    bool insertLife(size_t idx, ir_life_entry_t);
    bool setAlive(size_t position);
    bool mergeLife(const ir_value *other);

    std::string m_name;

    qc_type m_vtype;
    store_type m_store;
    lex_ctx_t m_context;
    qc_type m_fieldtype; // even the IR knows the subtype of a field
    qc_type m_outtype;   // and the output type of a function
    int m_cvq;       // 'const' vs 'var' qualifier
    ir_flag_t m_flags;

    std::vector<ir_instr *> m_reads;
    std::vector<ir_instr *> m_writes;

    // constant values
    bool m_hasvalue;
    union {
        qcfloat_t    vfloat;
        int          vint;
        vec3_t       vvec;
        int32_t      ivec[3];
        char        *vstring;
        ir_value    *vpointer;
        ir_function *vfunc;
    } m_constval;

    struct {
        int32_t globaladdr;
        int32_t name;
        int32_t local;         // filled by the local-allocator
        int32_t addroffset;    // added for members
        int32_t fieldaddr;     // to generate field-addresses early
    } m_code;

    // for accessing vectors
    ir_value *m_members[3];
    ir_value *m_memberof;

    bool m_unique_life;      // arrays will never overlap with temps
    bool m_locked;           // temps living during a CALL must be locked
    bool m_callparam;

    std::vector<ir_life_entry_t> m_life; // For the temp allocator

    size_t size() const;

    void dump(int (*oprintf)(const char*, ...)) const;
};

/* PHI data */
struct ir_phi_entry_t {
    ir_value *value;
    ir_block *from;
};

/* instruction */
struct ir_instr {
    ir_instr(lex_ctx_t, ir_block *owner, int opcode);
    ~ir_instr();

    int m_opcode;
    lex_ctx_t m_context;
    ir_value *(_m_ops[3]) = { nullptr, nullptr, nullptr };
    ir_block *(m_bops[2]) = { nullptr, nullptr };

    std::vector<ir_phi_entry_t> m_phi;
    std::vector<ir_value *> m_params;

    // For the temp-allocation
    size_t m_eid = 0;

    // For IFs
    bool m_likely = true;

    ir_block *m_owner;
};

/* block */
struct ir_block {
    ir_block(ir_function *owner, const std::string& name);
    ~ir_block();

    ir_function *m_owner;
    std::string m_label;

    lex_ctx_t m_context;
    bool m_final = false; /* once a jump is added we're done */

    std::vector<ir_instr *> m_instr;
    std::vector<ir_block *> m_entries;
    std::vector<ir_block *> m_exits;
    std::vector<ir_value *> m_living;

    /* For the temp-allocation */
    size_t m_entry_id  = 0;
    size_t m_eid       = 0;
    bool   m_is_return = false;

    bool m_generated = false;
    size_t m_code_start = 0;
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
    ir_function(ir_builder *owner, qc_type returntype);
    ~ir_function();

    ir_builder *m_owner;

    std::string      m_name;
    qc_type          m_outtype;
    std::vector<int> m_params;
    ir_flag_t        m_flags   = 0;
    int              m_builtin = 0;

    std::vector<std::unique_ptr<ir_block>> m_blocks;

    /*
     * values generated from operations
     * which might get optimized away, so anything
     * in there needs to be deleted in the dtor.
     */
    std::vector<std::unique_ptr<ir_value>> m_values;
    std::vector<std::unique_ptr<ir_value>> m_locals;     /* locally defined variables */
    ir_value *m_value = nullptr;

    size_t m_allocated_locals = 0;
    size_t m_globaltemps      = 0;

    ir_block*  m_first = nullptr;
    ir_block*  m_last  = nullptr;

    lex_ctx_t  m_context;

    /*
     * for prototypes - first we generate all the
     * globals, and we remember teh function-defs
     * so we can later fill in the entry pos
     *
     * remember the ID:
     */
    qcint_t m_code_function_def = -1;

    /* for temp allocation */
    size_t m_run_id = 0;

    /* vararg support: */
    size_t m_max_varargs = 0;
};


ir_value*       ir_function_create_local(ir_function *self, const std::string& name, qc_type vtype, bool param);
bool GMQCC_WARN ir_function_finalize(ir_function*);
ir_block*       ir_function_create_block(lex_ctx_t ctx, ir_function*, const char *label);

/* builder */
#define IR_HT_SIZE          1024
#define IR_MAX_VINSTR_TEMPS 1

struct ir_builder {
    ir_builder(const std::string& modulename);
    ~ir_builder();

    ir_function *createFunction(const std::string &name, qc_type outtype);
    ir_value *createGlobal(const std::string &name, qc_type vtype);
    ir_value *createField(const std::string &name, qc_type vtype);
    ir_value *get_va_count();
    bool generate(const char *filename);
    void dump(int (*oprintf)(const char*, ...)) const;

    ir_value *generateExtparamProto();
    void generateExtparam();

    ir_value *literalFloat(float value, bool add_to_list);

    std::string m_name;
    std::vector<std::unique_ptr<ir_function>> m_functions;
    std::vector<std::unique_ptr<ir_value>>    m_globals;
    std::vector<std::unique_ptr<ir_value>>    m_fields;
    // for reusing them in vector-splits, TODO: sort this or use a radix-tree
    std::vector<ir_value*>                    m_const_floats;

    ht            m_htfunctions;
    ht            m_htglobals;
    ht            m_htfields;

    // extparams' ir_values reference the ones from extparam_protos
    std::vector<std::unique_ptr<ir_value>> m_extparam_protos;
    std::vector<ir_value*>                 m_extparams;

    // the highest func->allocated_locals
    size_t        m_max_locals              = 0;
    size_t        m_max_globaltemps         = 0;
    uint32_t      m_first_common_local      = 0;
    uint32_t      m_first_common_globaltemp = 0;

    std::vector<const char*> m_filenames;
    std::vector<qcint_t>     m_filestrings;

    // we cache the #IMMEDIATE string here
    qcint_t      m_str_immediate = 0;

    // there should just be this one nil
    ir_value    *m_nil;
    ir_value    *m_reserved_va_count = nullptr;
    ir_value    *m_coverage_func = nullptr;

    /* some virtual instructions require temps, and their code is isolated
     * so that we don't need to keep track of their liveness.
     */
    ir_value    *m_vinstr_temp[IR_MAX_VINSTR_TEMPS];

    /* code generator */
    std::unique_ptr<code_t> m_code;

private:
    qcint_t filestring(const char *filename);
    bool generateGlobal(ir_value*, bool is_local);
    bool generateGlobalFunction(ir_value*);
    bool generateGlobalFunctionCode(ir_value*);
    bool generateFunctionLocals(ir_value*);
};

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
