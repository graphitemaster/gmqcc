#include <stdlib.h>
#include <string.h>

#include "gmqcc.h"
#include "ir.h"

/***********************************************************************
 * Type sizes used at multiple points in the IR codegen
 */

const char *type_name[TYPE_COUNT] = {
    "void",
    "string",
    "float",
    "vector",
    "entity",
    "field",
    "function",
    "pointer",
    "integer",
    "variant",
    "struct",
    "union",
    "array",

    "nil",
    "<no-expression>"
};

static size_t type_sizeof_[TYPE_COUNT] = {
    1, /* TYPE_VOID     */
    1, /* TYPE_STRING   */
    1, /* TYPE_FLOAT    */
    3, /* TYPE_VECTOR   */
    1, /* TYPE_ENTITY   */
    1, /* TYPE_FIELD    */
    1, /* TYPE_FUNCTION */
    1, /* TYPE_POINTER  */
    1, /* TYPE_INTEGER  */
    3, /* TYPE_VARIANT  */
    0, /* TYPE_STRUCT   */
    0, /* TYPE_UNION    */
    0, /* TYPE_ARRAY    */
    0, /* TYPE_NIL      */
    0, /* TYPE_NOESPR   */
};

const uint16_t type_store_instr[TYPE_COUNT] = {
    INSTR_STORE_F, /* should use I when having integer support */
    INSTR_STORE_S,
    INSTR_STORE_F,
    INSTR_STORE_V,
    INSTR_STORE_ENT,
    INSTR_STORE_FLD,
    INSTR_STORE_FNC,
    INSTR_STORE_ENT, /* should use I */
#if 0
    INSTR_STORE_I, /* integer type */
#else
    INSTR_STORE_F,
#endif

    INSTR_STORE_V, /* variant, should never be accessed */

    VINSTR_END, /* struct */
    VINSTR_END, /* union  */
    VINSTR_END, /* array  */
    VINSTR_END, /* nil    */
    VINSTR_END, /* noexpr */
};

const uint16_t field_store_instr[TYPE_COUNT] = {
    INSTR_STORE_FLD,
    INSTR_STORE_FLD,
    INSTR_STORE_FLD,
    INSTR_STORE_V,
    INSTR_STORE_FLD,
    INSTR_STORE_FLD,
    INSTR_STORE_FLD,
    INSTR_STORE_FLD,
#if 0
    INSTR_STORE_FLD, /* integer type */
#else
    INSTR_STORE_FLD,
#endif

    INSTR_STORE_V, /* variant, should never be accessed */

    VINSTR_END, /* struct */
    VINSTR_END, /* union  */
    VINSTR_END, /* array  */
    VINSTR_END, /* nil    */
    VINSTR_END, /* noexpr */
};

const uint16_t type_storep_instr[TYPE_COUNT] = {
    INSTR_STOREP_F, /* should use I when having integer support */
    INSTR_STOREP_S,
    INSTR_STOREP_F,
    INSTR_STOREP_V,
    INSTR_STOREP_ENT,
    INSTR_STOREP_FLD,
    INSTR_STOREP_FNC,
    INSTR_STOREP_ENT, /* should use I */
#if 0
    INSTR_STOREP_ENT, /* integer type */
#else
    INSTR_STOREP_F,
#endif

    INSTR_STOREP_V, /* variant, should never be accessed */

    VINSTR_END, /* struct */
    VINSTR_END, /* union  */
    VINSTR_END, /* array  */
    VINSTR_END, /* nil    */
    VINSTR_END, /* noexpr */
};

const uint16_t type_eq_instr[TYPE_COUNT] = {
    INSTR_EQ_F, /* should use I when having integer support */
    INSTR_EQ_S,
    INSTR_EQ_F,
    INSTR_EQ_V,
    INSTR_EQ_E,
    INSTR_EQ_E, /* FLD has no comparison */
    INSTR_EQ_FNC,
    INSTR_EQ_E, /* should use I */
#if 0
    INSTR_EQ_I,
#else
    INSTR_EQ_F,
#endif

    INSTR_EQ_V, /* variant, should never be accessed */

    VINSTR_END, /* struct */
    VINSTR_END, /* union  */
    VINSTR_END, /* array  */
    VINSTR_END, /* nil    */
    VINSTR_END, /* noexpr */
};

const uint16_t type_ne_instr[TYPE_COUNT] = {
    INSTR_NE_F, /* should use I when having integer support */
    INSTR_NE_S,
    INSTR_NE_F,
    INSTR_NE_V,
    INSTR_NE_E,
    INSTR_NE_E, /* FLD has no comparison */
    INSTR_NE_FNC,
    INSTR_NE_E, /* should use I */
#if 0
    INSTR_NE_I,
#else
    INSTR_NE_F,
#endif

    INSTR_NE_V, /* variant, should never be accessed */

    VINSTR_END, /* struct */
    VINSTR_END, /* union  */
    VINSTR_END, /* array  */
    VINSTR_END, /* nil    */
    VINSTR_END, /* noexpr */
};

const uint16_t type_not_instr[TYPE_COUNT] = {
    INSTR_NOT_F, /* should use I when having integer support */
    VINSTR_END,  /* not to be used, depends on string related -f flags */
    INSTR_NOT_F,
    INSTR_NOT_V,
    INSTR_NOT_ENT,
    INSTR_NOT_ENT,
    INSTR_NOT_FNC,
    INSTR_NOT_ENT, /* should use I */
#if 0
    INSTR_NOT_I, /* integer type */
#else
    INSTR_NOT_F,
#endif

    INSTR_NOT_V, /* variant, should never be accessed */

    VINSTR_END, /* struct */
    VINSTR_END, /* union  */
    VINSTR_END, /* array  */
    VINSTR_END, /* nil    */
    VINSTR_END, /* noexpr */
};

/* protos */
static void            ir_function_dump(ir_function*, char *ind, int (*oprintf)(const char*,...));

static ir_value*       ir_block_create_general_instr(ir_block *self, lex_ctx_t, const char *label,
                                                     int op, ir_value *a, ir_value *b, qc_type outype);
static bool GMQCC_WARN ir_block_create_store(ir_block*, lex_ctx_t, ir_value *target, ir_value *what);
static void            ir_block_dump(ir_block*, char *ind, int (*oprintf)(const char*,...));

static bool            ir_instr_op(ir_instr*, int op, ir_value *value, bool writing);
static void            ir_instr_dump(ir_instr* in, char *ind, int (*oprintf)(const char*,...));
/* error functions */

static void irerror(lex_ctx_t ctx, const char *msg, ...)
{
    va_list ap;
    va_start(ap, msg);
    con_cvprintmsg(ctx, LVL_ERROR, "internal error", msg, ap);
    va_end(ap);
}

static bool GMQCC_WARN irwarning(lex_ctx_t ctx, int warntype, const char *fmt, ...)
{
    bool    r;
    va_list ap;
    va_start(ap, fmt);
    r = vcompile_warning(ctx, warntype, fmt, ap);
    va_end(ap);
    return r;
}

/***********************************************************************
 * Vector utility functions
 */

static bool GMQCC_WARN vec_ir_value_find(std::vector<ir_value *> &vec, const ir_value *what, size_t *idx)
{
    for (auto &it : vec) {
        if (it != what)
            continue;
        if (idx)
            *idx = &it - &vec[0];
        return true;
    }
    return false;
}

static bool GMQCC_WARN vec_ir_block_find(std::vector<ir_block *> &vec, ir_block *what, size_t *idx)
{
    for (auto &it : vec) {
        if (it != what)
            continue;
        if (idx)
            *idx = &it - &vec[0];
        return true;
    }
    return false;
}

static bool GMQCC_WARN vec_ir_instr_find(std::vector<ir_instr *> &vec, ir_instr *what, size_t *idx)
{
    for (auto &it : vec) {
        if (it != what)
            continue;
        if (idx)
            *idx = &it - &vec[0];
        return true;
    }
    return false;
}

/***********************************************************************
 * IR Builder
 */

static void ir_block_delete_quick(ir_block* self);
static void ir_instr_delete_quick(ir_instr *self);
static void ir_function_delete_quick(ir_function *self);

ir_builder::ir_builder(const std::string& modulename)
: m_name(modulename),
  m_code(new code_t)
{
    m_htglobals   = util_htnew(IR_HT_SIZE);
    m_htfields    = util_htnew(IR_HT_SIZE);
    m_htfunctions = util_htnew(IR_HT_SIZE);

    m_nil = new ir_value("nil", store_value, TYPE_NIL);
    m_nil->m_cvq = CV_CONST;

    for (size_t i = 0; i != IR_MAX_VINSTR_TEMPS; ++i) {
        /* we write to them, but they're not supposed to be used outside the IR, so
         * let's not allow the generation of ir_instrs which use these.
         * So it's a constant noexpr.
         */
        m_vinstr_temp[i] = new ir_value("vinstr_temp", store_value, TYPE_NOEXPR);
        m_vinstr_temp[i]->m_cvq = CV_CONST;
    }
}

ir_builder::~ir_builder()
{
    util_htdel(m_htglobals);
    util_htdel(m_htfields);
    util_htdel(m_htfunctions);
    for (auto& f : m_functions)
        ir_function_delete_quick(f.release());
    m_functions.clear(); // delete them now before deleting the rest:

    delete m_nil;

    for (size_t i = 0; i != IR_MAX_VINSTR_TEMPS; ++i) {
        delete m_vinstr_temp[i];
    }

    m_extparams.clear();
    m_extparam_protos.clear();
}

ir_function* ir_builder::createFunction(const std::string& name, qc_type outtype)
{
    ir_function *fn = (ir_function*)util_htget(m_htfunctions, name.c_str());
    if (fn)
        return nullptr;

    fn = new ir_function(this, outtype);
    fn->m_name = name;
    m_functions.emplace_back(fn);
    util_htset(m_htfunctions, name.c_str(), fn);

    fn->m_value = createGlobal(fn->m_name, TYPE_FUNCTION);
    if (!fn->m_value) {
        delete fn;
        return nullptr;
    }

    fn->m_value->m_hasvalue = true;
    fn->m_value->m_outtype = outtype;
    fn->m_value->m_constval.vfunc = fn;
    fn->m_value->m_context = fn->m_context;

    return fn;
}

ir_value* ir_builder::createGlobal(const std::string& name, qc_type vtype)
{
    ir_value *ve;

    if (name[0] != '#')
    {
        ve = (ir_value*)util_htget(m_htglobals, name.c_str());
        if (ve) {
            return nullptr;
        }
    }

    ve = new ir_value(std::string(name), store_global, vtype);
    m_globals.emplace_back(ve);
    util_htset(m_htglobals, name.c_str(), ve);
    return ve;
}

ir_value* ir_builder::get_va_count()
{
    if (m_reserved_va_count)
        return m_reserved_va_count;
    return (m_reserved_va_count = createGlobal("reserved:va_count", TYPE_FLOAT));
}

ir_value* ir_builder::createField(const std::string& name, qc_type vtype)
{
    ir_value *ve = (ir_value*)util_htget(m_htfields, name.c_str());
    if (ve) {
        return nullptr;
    }

    ve = new ir_value(std::string(name), store_global, TYPE_FIELD);
    ve->m_fieldtype = vtype;
    m_fields.emplace_back(ve);
    util_htset(m_htfields, name.c_str(), ve);
    return ve;
}

/***********************************************************************
 *IR Function
 */

static bool ir_function_naive_phi(ir_function*);
static void ir_function_enumerate(ir_function*);
static bool ir_function_calculate_liferanges(ir_function*);
static bool ir_function_allocate_locals(ir_function*);

ir_function::ir_function(ir_builder* owner_, qc_type outtype_)
: m_owner(owner_),
  m_name("<@unnamed>"),
  m_outtype(outtype_)
{
    m_context.file = "<@no context>";
    m_context.line = 0;
}

ir_function::~ir_function()
{
}

static void ir_function_delete_quick(ir_function *self)
{
    for (auto& b : self->m_blocks)
        ir_block_delete_quick(b.release());
    delete self;
}

static void ir_function_collect_value(ir_function *self, ir_value *v)
{
    self->m_values.emplace_back(v);
}

ir_block* ir_function_create_block(lex_ctx_t ctx, ir_function *self, const char *label)
{
    ir_block* bn = new ir_block(self, label ? std::string(label) : std::string());
    bn->m_context = ctx;
    self->m_blocks.emplace_back(bn);

    if ((self->m_flags & IR_FLAG_BLOCK_COVERAGE) && self->m_owner->m_coverage_func)
        (void)ir_block_create_call(bn, ctx, nullptr, self->m_owner->m_coverage_func, false);

    return bn;
}

static bool instr_is_operation(uint16_t op)
{
    return ( (op >= INSTR_MUL_F  && op <= INSTR_GT) ||
             (op >= INSTR_LOAD_F && op <= INSTR_LOAD_FNC) ||
             (op == INSTR_ADDRESS) ||
             (op >= INSTR_NOT_F  && op <= INSTR_NOT_FNC) ||
             (op >= INSTR_AND    && op <= INSTR_BITOR) ||
             (op >= INSTR_CALL0  && op <= INSTR_CALL8) ||
             (op >= VINSTR_BITAND_V && op <= VINSTR_NEG_V) );
}

static bool ir_function_pass_peephole(ir_function *self)
{
    for (auto& bp : self->m_blocks) {
        ir_block *block = bp.get();
        for (size_t i = 0; i < block->m_instr.size(); ++i) {
            ir_instr *inst;
            inst = block->m_instr[i];

            if (i >= 1 &&
                (inst->m_opcode >= INSTR_STORE_F &&
                 inst->m_opcode <= INSTR_STORE_FNC))
            {
                ir_instr *store;
                ir_instr *oper;
                ir_value *value;

                store = inst;

                oper  = block->m_instr[i-1];
                if (!instr_is_operation(oper->m_opcode))
                    continue;

                /* Don't change semantics of MUL_VF in engines where these may not alias. */
                if (OPTS_FLAG(LEGACY_VECTOR_MATHS)) {
                    if (oper->m_opcode == INSTR_MUL_VF && oper->_m_ops[2]->m_memberof == oper->_m_ops[1])
                        continue;
                    if (oper->m_opcode == INSTR_MUL_FV && oper->_m_ops[1]->m_memberof == oper->_m_ops[2])
                        continue;
                }

                value = oper->_m_ops[0];

                /* only do it for SSA values */
                if (value->m_store != store_value)
                    continue;

                /* don't optimize out the temp if it's used later again */
                if (value->m_reads.size() != 1)
                    continue;

                /* The very next store must use this value */
                if (value->m_reads[0] != store)
                    continue;

                /* And of course the store must _read_ from it, so it's in
                 * OP 1 */
                if (store->_m_ops[1] != value)
                    continue;

                ++opts_optimizationcount[OPTIM_PEEPHOLE];
                (void)!ir_instr_op(oper, 0, store->_m_ops[0], true);

                block->m_instr.erase(block->m_instr.begin() + i);
                delete store;
            }
            else if (inst->m_opcode == VINSTR_COND)
            {
                /* COND on a value resulting from a NOT could
                 * remove the NOT and swap its operands
                 */
                while (true) {
                    ir_block *tmp;
                    size_t    inotid;
                    ir_instr *inot;
                    ir_value *value;
                    value = inst->_m_ops[0];

                    if (value->m_store != store_value || value->m_reads.size() != 1 || value->m_reads[0] != inst)
                        break;

                    inot = value->m_writes[0];
                    if (inot->_m_ops[0] != value ||
                        inot->m_opcode < INSTR_NOT_F ||
                        inot->m_opcode > INSTR_NOT_FNC ||
                        inot->m_opcode == INSTR_NOT_V || /* can't do these */
                        inot->m_opcode == INSTR_NOT_S)
                    {
                        break;
                    }

                    /* count */
                    ++opts_optimizationcount[OPTIM_PEEPHOLE];
                    /* change operand */
                    (void)!ir_instr_op(inst, 0, inot->_m_ops[1], false);
                    /* remove NOT */
                    tmp = inot->m_owner;
                    for (inotid = 0; inotid < tmp->m_instr.size(); ++inotid) {
                        if (tmp->m_instr[inotid] == inot)
                            break;
                    }
                    if (inotid >= tmp->m_instr.size()) {
                        compile_error(inst->m_context, "sanity-check failed: failed to find instruction to optimize out");
                        return false;
                    }
                    tmp->m_instr.erase(tmp->m_instr.begin() + inotid);
                    delete inot;
                    /* swap ontrue/onfalse */
                    tmp = inst->m_bops[0];
                    inst->m_bops[0] = inst->m_bops[1];
                    inst->m_bops[1] = tmp;
                }
                continue;
            }
        }
    }

    return true;
}

static bool ir_function_pass_tailrecursion(ir_function *self)
{
    size_t p;

    for (auto& bp : self->m_blocks) {
        ir_block *block = bp.get();

        ir_value *funcval;
        ir_instr *ret, *call, *store = nullptr;

        if (!block->m_final || block->m_instr.size() < 2)
            continue;

        ret = block->m_instr.back();
        if (ret->m_opcode != INSTR_DONE && ret->m_opcode != INSTR_RETURN)
            continue;

        call = block->m_instr[block->m_instr.size()-2];
        if (call->m_opcode >= INSTR_STORE_F && call->m_opcode <= INSTR_STORE_FNC) {
            /* account for the unoptimized
             * CALL
             * STORE %return, %tmp
             * RETURN %tmp
             * version
             */
            if (block->m_instr.size() < 3)
                continue;

            store = call;
            call = block->m_instr[block->m_instr.size()-3];
        }

        if (call->m_opcode < INSTR_CALL0 || call->m_opcode > INSTR_CALL8)
            continue;

        if (store) {
            /* optimize out the STORE */
            if (ret->_m_ops[0]   &&
                ret->_m_ops[0]   == store->_m_ops[0] &&
                store->_m_ops[1] == call->_m_ops[0])
            {
                ++opts_optimizationcount[OPTIM_PEEPHOLE];
                call->_m_ops[0] = store->_m_ops[0];
                block->m_instr.erase(block->m_instr.end()-2);
                delete store;
            }
            else
                continue;
        }

        if (!call->_m_ops[0])
            continue;

        funcval = call->_m_ops[1];
        if (!funcval)
            continue;
        if (funcval->m_vtype != TYPE_FUNCTION || funcval->m_constval.vfunc != self)
            continue;

        /* now we have a CALL and a RET, check if it's a tailcall */
        if (ret->_m_ops[0] && call->_m_ops[0] != ret->_m_ops[0])
            continue;

        ++opts_optimizationcount[OPTIM_TAIL_RECURSION];
        block->m_instr.erase(block->m_instr.end()-2, block->m_instr.end());

        block->m_final = false; /* open it back up */

        /* emite parameter-stores */
        for (p = 0; p < call->m_params.size(); ++p) {
            /* assert(call->params_count <= self->locals_count); */
            if (!ir_block_create_store(block, call->m_context, self->m_locals[p].get(), call->m_params[p])) {
                irerror(call->m_context, "failed to create tailcall store instruction for parameter %i", (int)p);
                return false;
            }
        }
        if (!ir_block_create_jump(block, call->m_context, self->m_blocks[0].get())) {
            irerror(call->m_context, "failed to create tailcall jump");
            return false;
        }

        delete call;
        delete ret;
    }

    return true;
}

bool ir_function_finalize(ir_function *self)
{
    if (self->m_builtin)
        return true;

    for (auto& lp : self->m_locals) {
        ir_value *v = lp.get();
        if (v->m_reads.empty() && v->m_writes.size() && !(v->m_flags & IR_FLAG_NOREF)) {
            // if it's a vector check to ensure all it's members are unused before
            // claiming it's unused, otherwise skip the vector entierly
            if (v->m_vtype == TYPE_VECTOR)
            {
                size_t mask = (1 << 3) - 1, bits = 0;
                for (size_t i = 0; i < 3; i++)
                    if (!v->m_members[i] || (v->m_members[i]->m_reads.empty()
                        && v->m_members[i]->m_writes.size()))
                        bits |= (1 << i);
                // all components are unused so just report the vector
                if (bits == mask && irwarning(v->m_context, WARN_UNUSED_VARIABLE,
                    "unused variable: `%s`", v->m_name.c_str()))
                    return false;
                else if (bits != mask)
                    // individual components are unused so mention them
                    for (size_t i = 0; i < 3; i++)
                        if ((bits & (1 << i))
                            && irwarning(v->m_context, WARN_UNUSED_COMPONENT,
                                "unused vector component: `%s.%c`", v->m_name.c_str(), "xyz"[i]))
                            return false;
            }
            // just a standard variable
            else if (irwarning(v->m_context, WARN_UNUSED_VARIABLE,
                    "unused variable: `%s`", v->m_name.c_str())) return false;
        }
    }

    if (OPTS_OPTIMIZATION(OPTIM_PEEPHOLE)) {
        if (!ir_function_pass_peephole(self)) {
            irerror(self->m_context, "generic optimization pass broke something in `%s`", self->m_name.c_str());
            return false;
        }
    }

    if (OPTS_OPTIMIZATION(OPTIM_TAIL_RECURSION)) {
        if (!ir_function_pass_tailrecursion(self)) {
            irerror(self->m_context, "tail-recursion optimization pass broke something in `%s`", self->m_name.c_str());
            return false;
        }
    }

    if (!ir_function_naive_phi(self)) {
        irerror(self->m_context, "internal error: ir_function_naive_phi failed");
        return false;
    }

    for (auto& lp : self->m_locals) {
        ir_value *v = lp.get();
        if (v->m_vtype == TYPE_VECTOR ||
            (v->m_vtype == TYPE_FIELD && v->m_outtype == TYPE_VECTOR))
        {
            v->vectorMember(0);
            v->vectorMember(1);
            v->vectorMember(2);
        }
    }
    for (auto& vp : self->m_values) {
        ir_value *v = vp.get();
        if (v->m_vtype == TYPE_VECTOR ||
            (v->m_vtype == TYPE_FIELD && v->m_outtype == TYPE_VECTOR))
        {
            v->vectorMember(0);
            v->vectorMember(1);
            v->vectorMember(2);
        }
    }

    ir_function_enumerate(self);

    if (!ir_function_calculate_liferanges(self))
        return false;
    if (!ir_function_allocate_locals(self))
        return false;
    return true;
}

ir_value* ir_function_create_local(ir_function *self, const std::string& name, qc_type vtype, bool param)
{
    ir_value *ve;

    if (param &&
        !self->m_locals.empty() &&
        self->m_locals.back()->m_store != store_param)
    {
        irerror(self->m_context, "cannot add parameters after adding locals");
        return nullptr;
    }

    ve = new ir_value(std::string(name), (param ? store_param : store_local), vtype);
    if (param)
        ve->m_locked = true;
    self->m_locals.emplace_back(ve);
    return ve;
}

/***********************************************************************
 *IR Block
 */

ir_block::ir_block(ir_function* owner, const std::string& name)
: m_owner(owner),
  m_label(name)
{
    m_context.file = "<@no context>";
    m_context.line = 0;
}

ir_block::~ir_block()
{
    for (auto &i : m_instr)
        delete i;
}

static void ir_block_delete_quick(ir_block* self)
{
    for (auto &i : self->m_instr)
        ir_instr_delete_quick(i);
    self->m_instr.clear();
    delete self;
}

/***********************************************************************
 *IR Instructions
 */

ir_instr::ir_instr(lex_ctx_t ctx, ir_block* owner_, int op)
: m_opcode(op),
  m_context(ctx),
  m_owner(owner_)
{
}

ir_instr::~ir_instr()
{
    // The following calls can only delete from
    // vectors, we still want to delete this instruction
    // so ignore the return value. Since with the warn_unused_result attribute
    // gcc doesn't care about an explicit: (void)foo(); to ignore the result,
    // I have to improvise here and use if(foo());
    for (auto &it : m_phi) {
        size_t idx;
        if (vec_ir_instr_find(it.value->m_writes, this, &idx))
            it.value->m_writes.erase(it.value->m_writes.begin() + idx);
        if (vec_ir_instr_find(it.value->m_reads, this, &idx))
            it.value->m_reads.erase(it.value->m_reads.begin() + idx);
    }
    for (auto &it : m_params) {
        size_t idx;
        if (vec_ir_instr_find(it->m_writes, this, &idx))
            it->m_writes.erase(it->m_writes.begin() + idx);
        if (vec_ir_instr_find(it->m_reads, this, &idx))
            it->m_reads.erase(it->m_reads.begin() + idx);
    }
    (void)!ir_instr_op(this, 0, nullptr, false);
    (void)!ir_instr_op(this, 1, nullptr, false);
    (void)!ir_instr_op(this, 2, nullptr, false);
}

static void ir_instr_delete_quick(ir_instr *self)
{
    self->m_phi.clear();
    self->m_params.clear();
    self->_m_ops[0] = nullptr;
    self->_m_ops[1] = nullptr;
    self->_m_ops[2] = nullptr;
    delete self;
}

static bool ir_instr_op(ir_instr *self, int op, ir_value *v, bool writing)
{
    if (v && v->m_vtype == TYPE_NOEXPR) {
        irerror(self->m_context, "tried to use a NOEXPR value");
        return false;
    }

    if (self->_m_ops[op]) {
        size_t idx;
        if (writing && vec_ir_instr_find(self->_m_ops[op]->m_writes, self, &idx))
            self->_m_ops[op]->m_writes.erase(self->_m_ops[op]->m_writes.begin() + idx);
        else if (vec_ir_instr_find(self->_m_ops[op]->m_reads, self, &idx))
            self->_m_ops[op]->m_reads.erase(self->_m_ops[op]->m_reads.begin() + idx);
    }
    if (v) {
        if (writing)
            v->m_writes.push_back(self);
        else
            v->m_reads.push_back(self);
    }
    self->_m_ops[op] = v;
    return true;
}

/***********************************************************************
 *IR Value
 */

void ir_value::setCodeAddress(int32_t gaddr)
{
    m_code.globaladdr = gaddr;
    if (m_members[0]) m_members[0]->m_code.globaladdr = gaddr;
    if (m_members[1]) m_members[1]->m_code.globaladdr = gaddr;
    if (m_members[2]) m_members[2]->m_code.globaladdr = gaddr;
}

int32_t ir_value::codeAddress() const
{
    if (m_store == store_return)
        return OFS_RETURN + m_code.addroffset;
    return m_code.globaladdr + m_code.addroffset;
}

ir_value::ir_value(std::string&& name_, store_type store_, qc_type vtype_)
    : m_name(move(name_))
    , m_vtype(vtype_)
    , m_store(store_)
{
    m_fieldtype = TYPE_VOID;
    m_outtype = TYPE_VOID;
    m_flags = 0;

    m_cvq          = CV_NONE;
    m_hasvalue     = false;
    m_context.file = "<@no context>";
    m_context.line = 0;

    memset(&m_constval, 0, sizeof(m_constval));
    memset(&m_code,     0, sizeof(m_code));

    m_members[0] = nullptr;
    m_members[1] = nullptr;
    m_members[2] = nullptr;
    m_memberof = nullptr;

    m_unique_life = false;
    m_locked = false;
    m_callparam  = false;
}

ir_value::ir_value(ir_function *owner, std::string&& name, store_type storetype, qc_type vtype)
    : ir_value(move(name), storetype, vtype)
{
    ir_function_collect_value(owner, this);
}

ir_value::~ir_value()
{
    size_t i;
    if (m_hasvalue) {
        if (m_vtype == TYPE_STRING)
            mem_d((void*)m_constval.vstring);
    }
    if (!(m_flags & IR_FLAG_SPLIT_VECTOR)) {
        for (i = 0; i < 3; ++i) {
            if (m_members[i])
                delete m_members[i];
        }
    }
}


/*  helper function */
ir_value* ir_builder::literalFloat(float value, bool add_to_list) {
    ir_value *v = new ir_value("#IMMEDIATE", store_global, TYPE_FLOAT);
    v->m_flags |= IR_FLAG_ERASABLE;
    v->m_hasvalue = true;
    v->m_cvq = CV_CONST;
    v->m_constval.vfloat = value;

    m_globals.emplace_back(v);
    if (add_to_list)
        m_const_floats.emplace_back(v);
    return v;
}

ir_value* ir_value::vectorMember(unsigned int member)
{
    std::string name;
    ir_value *m;
    if (member >= 3)
        return nullptr;

    if (m_members[member])
        return m_members[member];

    if (!m_name.empty()) {
        char member_name[3] = { '_', char('x' + member), 0 };
        name = m_name + member_name;
    }

    if (m_vtype == TYPE_VECTOR)
    {
        m = new ir_value(move(name), m_store, TYPE_FLOAT);
        if (!m)
            return nullptr;
        m->m_context = m_context;

        m_members[member] = m;
        m->m_code.addroffset = member;
    }
    else if (m_vtype == TYPE_FIELD)
    {
        if (m_fieldtype != TYPE_VECTOR)
            return nullptr;
        m = new ir_value(move(name), m_store, TYPE_FIELD);
        if (!m)
            return nullptr;
        m->m_fieldtype = TYPE_FLOAT;
        m->m_context = m_context;

        m_members[member] = m;
        m->m_code.addroffset = member;
    }
    else
    {
        irerror(m_context, "invalid member access on %s", m_name.c_str());
        return nullptr;
    }

    m->m_memberof = this;
    return m;
}

size_t ir_value::size() const {
    if (m_vtype == TYPE_FIELD && m_fieldtype == TYPE_VECTOR)
        return type_sizeof_[TYPE_VECTOR];
    return type_sizeof_[m_vtype];
}

bool ir_value::setFloat(float f)
{
    if (m_vtype != TYPE_FLOAT)
        return false;
    m_constval.vfloat = f;
    m_hasvalue = true;
    return true;
}

bool ir_value::setFunc(int f)
{
    if (m_vtype != TYPE_FUNCTION)
        return false;
    m_constval.vint = f;
    m_hasvalue = true;
    return true;
}

bool ir_value::setVector(vec3_t v)
{
    if (m_vtype != TYPE_VECTOR)
        return false;
    m_constval.vvec = v;
    m_hasvalue = true;
    return true;
}

bool ir_value::setField(ir_value *fld)
{
    if (m_vtype != TYPE_FIELD)
        return false;
    m_constval.vpointer = fld;
    m_hasvalue = true;
    return true;
}

bool ir_value::setString(const char *str)
{
    if (m_vtype != TYPE_STRING)
        return false;
    m_constval.vstring = util_strdupe(str);
    m_hasvalue = true;
    return true;
}

#if 0
bool ir_value::setInt(int i)
{
    if (m_vtype != TYPE_INTEGER)
        return false;
    m_constval.vint = i;
    m_hasvalue = true;
    return true;
}
#endif

bool ir_value::lives(size_t at)
{
    for (auto& l : m_life) {
        if (l.start <= at && at <= l.end)
            return true;
        if (l.start > at) /* since it's ordered */
            return false;
    }
    return false;
}

bool ir_value::insertLife(size_t idx, ir_life_entry_t e)
{
    m_life.insert(m_life.begin() + idx, e);
    return true;
}

bool ir_value::setAlive(size_t s)
{
    size_t i;
    const size_t vs = m_life.size();
    ir_life_entry_t *life_found = nullptr;
    ir_life_entry_t *before = nullptr;
    ir_life_entry_t new_entry;

    /* Find the first range >= s */
    for (i = 0; i < vs; ++i)
    {
        before = life_found;
        life_found = &m_life[i];
        if (life_found->start > s)
            break;
    }
    /* nothing found? append */
    if (i == vs) {
        ir_life_entry_t e;
        if (life_found && life_found->end+1 == s)
        {
            /* previous life range can be merged in */
            life_found->end++;
            return true;
        }
        if (life_found && life_found->end >= s)
            return false;
        e.start = e.end = s;
        m_life.emplace_back(e);
        return true;
    }
    /* found */
    if (before)
    {
        if (before->end + 1 == s &&
            life_found->start - 1 == s)
        {
            /* merge */
            before->end = life_found->end;
            m_life.erase(m_life.begin()+i);
            return true;
        }
        if (before->end + 1 == s)
        {
            /* extend before */
            before->end++;
            return true;
        }
        /* already contained */
        if (before->end >= s)
            return false;
    }
    /* extend */
    if (life_found->start - 1 == s)
    {
        life_found->start--;
        return true;
    }
    /* insert a new entry */
    new_entry.start = new_entry.end = s;
    return insertLife(i, new_entry);
}

bool ir_value::mergeLife(const ir_value *other)
{
    size_t i, myi;

    if (other->m_life.empty())
        return true;

    if (m_life.empty()) {
        m_life = other->m_life;
        return true;
    }

    myi = 0;
    for (i = 0; i < other->m_life.size(); ++i)
    {
        const ir_life_entry_t &otherlife = other->m_life[i];
        while (true)
        {
            ir_life_entry_t *entry = &m_life[myi];

            if (otherlife.end+1 < entry->start)
            {
                /* adding an interval before entry */
                if (!insertLife(myi, otherlife))
                    return false;
                ++myi;
                break;
            }

            if (otherlife.start <  entry->start &&
                otherlife.end+1 >= entry->start)
            {
                /* starts earlier and overlaps */
                entry->start = otherlife.start;
            }

            if (otherlife.end   >  entry->end &&
                otherlife.start <= entry->end+1)
            {
                /* ends later and overlaps */
                entry->end = otherlife.end;
            }

            /* see if our change combines it with the next ranges */
            while (myi+1 < m_life.size() &&
                   entry->end+1 >= m_life[1+myi].start)
            {
                /* overlaps with (myi+1) */
                if (entry->end < m_life[1+myi].end)
                    entry->end = m_life[1+myi].end;
                m_life.erase(m_life.begin() + (myi + 1));
                entry = &m_life[myi];
            }

            /* see if we're after the entry */
            if (otherlife.start > entry->end)
            {
                ++myi;
                /* append if we're at the end */
                if (myi >= m_life.size()) {
                    m_life.emplace_back(otherlife);
                    break;
                }
                /* otherweise check the next range */
                continue;
            }
            break;
        }
    }
    return true;
}

static bool ir_values_overlap(const ir_value *a, const ir_value *b)
{
    /* For any life entry in A see if it overlaps with
     * any life entry in B.
     * Note that the life entries are orderes, so we can make a
     * more efficient algorithm there than naively translating the
     * statement above.
     */

    const ir_life_entry_t *la, *lb, *enda, *endb;

    /* first of all, if either has no life range, they cannot clash */
    if (a->m_life.empty() || b->m_life.empty())
        return false;

    la = &a->m_life.front();
    lb = &b->m_life.front();
    enda = &a->m_life.back() + 1;
    endb = &b->m_life.back() + 1;
    while (true)
    {
        /* check if the entries overlap, for that,
         * both must start before the other one ends.
         */
        if (la->start < lb->end &&
            lb->start < la->end)
        {
            return true;
        }

        /* entries are ordered
         * one entry is earlier than the other
         * that earlier entry will be moved forward
         */
        if (la->start < lb->start)
        {
            /* order: A B, move A forward
             * check if we hit the end with A
             */
            if (++la == enda)
                break;
        }
        else /* if (lb->start < la->start)  actually <= */
        {
            /* order: B A, move B forward
             * check if we hit the end with B
             */
            if (++lb == endb)
                break;
        }
    }
    return false;
}

/***********************************************************************
 *IR main operations
 */

static bool ir_check_unreachable(ir_block *self)
{
    /* The IR should never have to deal with unreachable code */
    if (!self->m_final/* || OPTS_FLAG(ALLOW_UNREACHABLE_CODE)*/)
        return true;
    irerror(self->m_context, "unreachable statement (%s)", self->m_label.c_str());
    return false;
}

bool ir_block_create_store_op(ir_block *self, lex_ctx_t ctx, int op, ir_value *target, ir_value *what)
{
    ir_instr *in;
    if (!ir_check_unreachable(self))
        return false;

    if (target->m_store == store_value &&
        (op < INSTR_STOREP_F || op > INSTR_STOREP_FNC))
    {
        irerror(self->m_context, "cannot store to an SSA value");
        irerror(self->m_context, "trying to store: %s <- %s", target->m_name.c_str(), what->m_name.c_str());
        irerror(self->m_context, "instruction: %s", util_instr_str[op]);
        return false;
    }

    in = new ir_instr(ctx, self, op);
    if (!in)
        return false;

    if (!ir_instr_op(in, 0, target, (op < INSTR_STOREP_F || op > INSTR_STOREP_FNC)) ||
        !ir_instr_op(in, 1, what, false))
    {
        delete in;
        return false;
    }
    self->m_instr.push_back(in);
    return true;
}

bool ir_block_create_state_op(ir_block *self, lex_ctx_t ctx, ir_value *frame, ir_value *think)
{
    ir_instr *in;
    if (!ir_check_unreachable(self))
        return false;

    in = new ir_instr(ctx, self, INSTR_STATE);
    if (!in)
        return false;

    if (!ir_instr_op(in, 0, frame, false) ||
        !ir_instr_op(in, 1, think, false))
    {
        delete in;
        return false;
    }
    self->m_instr.push_back(in);
    return true;
}

static bool ir_block_create_store(ir_block *self, lex_ctx_t ctx, ir_value *target, ir_value *what)
{
    int op = 0;
    qc_type vtype;
    if (target->m_vtype == TYPE_VARIANT)
        vtype = what->m_vtype;
    else
        vtype = target->m_vtype;

#if 0
    if      (vtype == TYPE_FLOAT   && what->m_vtype == TYPE_INTEGER)
        op = INSTR_CONV_ITOF;
    else if (vtype == TYPE_INTEGER && what->m_vtype == TYPE_FLOAT)
        op = INSTR_CONV_FTOI;
#endif
        op = type_store_instr[vtype];

    if (OPTS_FLAG(ADJUST_VECTOR_FIELDS)) {
        if (op == INSTR_STORE_FLD && what->m_fieldtype == TYPE_VECTOR)
            op = INSTR_STORE_V;
    }

    return ir_block_create_store_op(self, ctx, op, target, what);
}

bool ir_block_create_storep(ir_block *self, lex_ctx_t ctx, ir_value *target, ir_value *what)
{
    int op = 0;
    qc_type vtype;

    if (target->m_vtype != TYPE_POINTER)
        return false;

    /* storing using pointer - target is a pointer, type must be
     * inferred from source
     */
    vtype = what->m_vtype;

    op = type_storep_instr[vtype];
    if (OPTS_FLAG(ADJUST_VECTOR_FIELDS)) {
        if (op == INSTR_STOREP_FLD && what->m_fieldtype == TYPE_VECTOR)
            op = INSTR_STOREP_V;
    }

    return ir_block_create_store_op(self, ctx, op, target, what);
}

bool ir_block_create_return(ir_block *self, lex_ctx_t ctx, ir_value *v)
{
    ir_instr *in;
    if (!ir_check_unreachable(self))
        return false;

    self->m_final = true;

    self->m_is_return = true;
    in = new ir_instr(ctx, self, INSTR_RETURN);
    if (!in)
        return false;

    if (v && !ir_instr_op(in, 0, v, false)) {
        delete in;
        return false;
    }

    self->m_instr.push_back(in);
    return true;
}

bool ir_block_create_if(ir_block *self, lex_ctx_t ctx, ir_value *v,
                        ir_block *ontrue, ir_block *onfalse)
{
    ir_instr *in;
    if (!ir_check_unreachable(self))
        return false;
    self->m_final = true;
    /*in = new ir_instr(ctx, self, (v->m_vtype == TYPE_STRING ? INSTR_IF_S : INSTR_IF_F));*/
    in = new ir_instr(ctx, self, VINSTR_COND);
    if (!in)
        return false;

    if (!ir_instr_op(in, 0, v, false)) {
        delete in;
        return false;
    }

    in->m_bops[0] = ontrue;
    in->m_bops[1] = onfalse;

    self->m_instr.push_back(in);

    self->m_exits.push_back(ontrue);
    self->m_exits.push_back(onfalse);
    ontrue->m_entries.push_back(self);
    onfalse->m_entries.push_back(self);
    return true;
}

bool ir_block_create_jump(ir_block *self, lex_ctx_t ctx, ir_block *to)
{
    ir_instr *in;
    if (!ir_check_unreachable(self))
        return false;
    self->m_final = true;
    in = new ir_instr(ctx, self, VINSTR_JUMP);
    if (!in)
        return false;

    in->m_bops[0] = to;
    self->m_instr.push_back(in);

    self->m_exits.push_back(to);
    to->m_entries.push_back(self);
    return true;
}

bool ir_block_create_goto(ir_block *self, lex_ctx_t ctx, ir_block *to)
{
    self->m_owner->m_flags |= IR_FLAG_HAS_GOTO;
    return ir_block_create_jump(self, ctx, to);
}

ir_instr* ir_block_create_phi(ir_block *self, lex_ctx_t ctx, const char *label, qc_type ot)
{
    ir_value *out;
    ir_instr *in;
    if (!ir_check_unreachable(self))
        return nullptr;
    in = new ir_instr(ctx, self, VINSTR_PHI);
    if (!in)
        return nullptr;
    out = new ir_value(self->m_owner, label ? label : "", store_value, ot);
    if (!out) {
        delete in;
        return nullptr;
    }
    if (!ir_instr_op(in, 0, out, true)) {
        delete in;
        return nullptr;
    }
    self->m_instr.push_back(in);
    return in;
}

ir_value* ir_phi_value(ir_instr *self)
{
    return self->_m_ops[0];
}

void ir_phi_add(ir_instr* self, ir_block *b, ir_value *v)
{
    ir_phi_entry_t pe;

    if (!vec_ir_block_find(self->m_owner->m_entries, b, nullptr)) {
        // Must not be possible to cause this, otherwise the AST
        // is doing something wrong.
        irerror(self->m_context, "Invalid entry block for PHI");
        exit(EXIT_FAILURE);
    }

    pe.value = v;
    pe.from = b;
    v->m_reads.push_back(self);
    self->m_phi.push_back(pe);
}

/* call related code */
ir_instr* ir_block_create_call(ir_block *self, lex_ctx_t ctx, const char *label, ir_value *func, bool noreturn)
{
    ir_value *out;
    ir_instr *in;
    if (!ir_check_unreachable(self))
        return nullptr;
    in = new ir_instr(ctx, self, (noreturn ? VINSTR_NRCALL : INSTR_CALL0));
    if (!in)
        return nullptr;
    if (noreturn) {
        self->m_final = true;
        self->m_is_return = true;
    }
    out = new ir_value(self->m_owner, label ? label : "", (func->m_outtype == TYPE_VOID) ? store_return : store_value, func->m_outtype);
    if (!out) {
        delete in;
        return nullptr;
    }
    if (!ir_instr_op(in, 0, out, true) ||
        !ir_instr_op(in, 1, func, false))
    {
        delete in;
        return nullptr;
    }
    self->m_instr.push_back(in);
    /*
    if (noreturn) {
        if (!ir_block_create_return(self, ctx, nullptr)) {
            compile_error(ctx, "internal error: failed to generate dummy-return instruction");
            delete in;
            return nullptr;
        }
    }
    */
    return in;
}

ir_value* ir_call_value(ir_instr *self)
{
    return self->_m_ops[0];
}

void ir_call_param(ir_instr* self, ir_value *v)
{
    self->m_params.push_back(v);
    v->m_reads.push_back(self);
}

/* binary op related code */

ir_value* ir_block_create_binop(ir_block *self, lex_ctx_t ctx,
                                const char *label, int opcode,
                                ir_value *left, ir_value *right)
{
    qc_type ot = TYPE_VOID;
    switch (opcode) {
        case INSTR_ADD_F:
        case INSTR_SUB_F:
        case INSTR_DIV_F:
        case INSTR_MUL_F:
        case INSTR_MUL_V:
        case INSTR_AND:
        case INSTR_OR:
#if 0
        case INSTR_AND_I:
        case INSTR_AND_IF:
        case INSTR_AND_FI:
        case INSTR_OR_I:
        case INSTR_OR_IF:
        case INSTR_OR_FI:
#endif
        case INSTR_BITAND:
        case INSTR_BITOR:
        case VINSTR_BITXOR:
#if 0
        case INSTR_SUB_S: /* -- offset of string as float */
        case INSTR_MUL_IF:
        case INSTR_MUL_FI:
        case INSTR_DIV_IF:
        case INSTR_DIV_FI:
        case INSTR_BITOR_IF:
        case INSTR_BITOR_FI:
        case INSTR_BITAND_FI:
        case INSTR_BITAND_IF:
        case INSTR_EQ_I:
        case INSTR_NE_I:
#endif
            ot = TYPE_FLOAT;
            break;
#if 0
        case INSTR_ADD_I:
        case INSTR_ADD_IF:
        case INSTR_ADD_FI:
        case INSTR_SUB_I:
        case INSTR_SUB_FI:
        case INSTR_SUB_IF:
        case INSTR_MUL_I:
        case INSTR_DIV_I:
        case INSTR_BITAND_I:
        case INSTR_BITOR_I:
        case INSTR_XOR_I:
        case INSTR_RSHIFT_I:
        case INSTR_LSHIFT_I:
            ot = TYPE_INTEGER;
            break;
#endif
        case INSTR_ADD_V:
        case INSTR_SUB_V:
        case INSTR_MUL_VF:
        case INSTR_MUL_FV:
        case VINSTR_BITAND_V:
        case VINSTR_BITOR_V:
        case VINSTR_BITXOR_V:
        case VINSTR_BITAND_VF:
        case VINSTR_BITOR_VF:
        case VINSTR_BITXOR_VF:
        case VINSTR_CROSS:
#if 0
        case INSTR_DIV_VF:
        case INSTR_MUL_IV:
        case INSTR_MUL_VI:
#endif
            ot = TYPE_VECTOR;
            break;
#if 0
        case INSTR_ADD_SF:
            ot = TYPE_POINTER;
            break;
#endif
    /*
     * after the following default case, the value of opcode can never
     * be 1, 2, 3, 4, 5, 6, 7, 8, 9, 62, 63, 64, 65
     */
        default:
            /* ranges: */
            /* boolean operations result in floats */

            /*
             * opcode >= 10 takes true branch opcode is at least 10
             * opcode <= 23 takes false branch opcode is at least 24
             */
            if (opcode >= INSTR_EQ_F && opcode <= INSTR_GT)
                ot = TYPE_FLOAT;

            /*
             * At condition "opcode <= 23", the value of "opcode" must be
             * at least 24.
             * At condition "opcode <= 23", the value of "opcode" cannot be
             * equal to any of {1, 2, 3, 4, 5, 6, 7, 8, 9, 62, 63, 64, 65}.
             * The condition "opcode <= 23" cannot be true.
             *
             * Thus ot=2 (TYPE_FLOAT) can never be true
             */
#if 0
            else if (opcode >= INSTR_LE && opcode <= INSTR_GT)
                ot = TYPE_FLOAT;
            else if (opcode >= INSTR_LE_I && opcode <= INSTR_EQ_FI)
                ot = TYPE_FLOAT;
#endif
            break;
    };
    if (ot == TYPE_VOID) {
        /* The AST or parser were supposed to check this! */
        return nullptr;
    }

    return ir_block_create_general_instr(self, ctx, label, opcode, left, right, ot);
}

ir_value* ir_block_create_unary(ir_block *self, lex_ctx_t ctx,
                                const char *label, int opcode,
                                ir_value *operand)
{
    qc_type ot = TYPE_FLOAT;
    switch (opcode) {
        case INSTR_NOT_F:
        case INSTR_NOT_V:
        case INSTR_NOT_S:
        case INSTR_NOT_ENT:
        case INSTR_NOT_FNC: /*
        case INSTR_NOT_I:   */
            ot = TYPE_FLOAT;
            break;

        /*
         * Negation for virtual instructions is emulated with 0-value. Thankfully
         * the operand for 0 already exists so we just source it from here.
         */
        case VINSTR_NEG_F:
            return ir_block_create_general_instr(self, ctx, label, INSTR_SUB_F, nullptr, operand, ot);
        case VINSTR_NEG_V:
            return ir_block_create_general_instr(self, ctx, label, INSTR_SUB_V, nullptr, operand, TYPE_VECTOR);

        default:
            ot = operand->m_vtype;
            break;
    };
    if (ot == TYPE_VOID) {
        /* The AST or parser were supposed to check this! */
        return nullptr;
    }

    /* let's use the general instruction creator and pass nullptr for OPB */
    return ir_block_create_general_instr(self, ctx, label, opcode, operand, nullptr, ot);
}

static ir_value* ir_block_create_general_instr(ir_block *self, lex_ctx_t ctx, const char *label,
                                        int op, ir_value *a, ir_value *b, qc_type outype)
{
    ir_instr *instr;
    ir_value *out;

    out = new ir_value(self->m_owner, label ? label : "", store_value, outype);
    if (!out)
        return nullptr;

    instr = new ir_instr(ctx, self, op);
    if (!instr) {
        return nullptr;
    }

    if (!ir_instr_op(instr, 0, out, true) ||
        !ir_instr_op(instr, 1, a, false) ||
        !ir_instr_op(instr, 2, b, false) )
    {
        goto on_error;
    }

    self->m_instr.push_back(instr);

    return out;
on_error:
    delete instr;
    return nullptr;
}

ir_value* ir_block_create_fieldaddress(ir_block *self, lex_ctx_t ctx, const char *label, ir_value *ent, ir_value *field)
{
    ir_value *v;

    /* Support for various pointer types todo if so desired */
    if (ent->m_vtype != TYPE_ENTITY)
        return nullptr;

    if (field->m_vtype != TYPE_FIELD)
        return nullptr;

    v = ir_block_create_general_instr(self, ctx, label, INSTR_ADDRESS, ent, field, TYPE_POINTER);
    v->m_fieldtype = field->m_fieldtype;
    return v;
}

ir_value* ir_block_create_load_from_ent(ir_block *self, lex_ctx_t ctx, const char *label, ir_value *ent, ir_value *field, qc_type outype)
{
    int op;
    if (ent->m_vtype != TYPE_ENTITY)
        return nullptr;

    /* at some point we could redirect for TYPE_POINTER... but that could lead to carelessness */
    if (field->m_vtype != TYPE_FIELD)
        return nullptr;

    switch (outype)
    {
        case TYPE_FLOAT:    op = INSTR_LOAD_F;   break;
        case TYPE_VECTOR:   op = INSTR_LOAD_V;   break;
        case TYPE_STRING:   op = INSTR_LOAD_S;   break;
        case TYPE_FIELD:    op = INSTR_LOAD_FLD; break;
        case TYPE_ENTITY:   op = INSTR_LOAD_ENT; break;
        case TYPE_FUNCTION: op = INSTR_LOAD_FNC; break;
#if 0
        case TYPE_POINTER: op = INSTR_LOAD_I;   break;
        case TYPE_INTEGER: op = INSTR_LOAD_I;   break;
#endif
        default:
            irerror(self->m_context, "invalid type for ir_block_create_load_from_ent: %s", type_name[outype]);
            return nullptr;
    }

    return ir_block_create_general_instr(self, ctx, label, op, ent, field, outype);
}

/* PHI resolving breaks the SSA, and must thus be the last
 * step before life-range calculation.
 */

static bool ir_block_naive_phi(ir_block *self);
bool ir_function_naive_phi(ir_function *self)
{
    for (auto& b : self->m_blocks)
        if (!ir_block_naive_phi(b.get()))
            return false;
    return true;
}

static bool ir_block_naive_phi(ir_block *self)
{
    size_t i;
    /* FIXME: optionally, create_phi can add the phis
     * to a list so we don't need to loop through blocks
     * - anyway: "don't optimize YET"
     */
    for (i = 0; i < self->m_instr.size(); ++i)
    {
        ir_instr *instr = self->m_instr[i];
        if (instr->m_opcode != VINSTR_PHI)
            continue;

        self->m_instr.erase(self->m_instr.begin()+i);
        --i; /* NOTE: i+1 below */

        for (auto &it : instr->m_phi) {
            ir_value *v = it.value;
            ir_block *b = it.from;
            if (v->m_store == store_value && v->m_reads.size() == 1 && v->m_writes.size() == 1) {
                /* replace the value */
                if (!ir_instr_op(v->m_writes[0], 0, instr->_m_ops[0], true))
                    return false;
            } else {
                /* force a move instruction */
                ir_instr *prevjump = b->m_instr.back();
                b->m_instr.pop_back();
                b->m_final = false;
                instr->_m_ops[0]->m_store = store_global;
                if (!ir_block_create_store(b, instr->m_context, instr->_m_ops[0], v))
                    return false;
                instr->_m_ops[0]->m_store = store_value;
                b->m_instr.push_back(prevjump);
                b->m_final = true;
            }
        }
        delete instr;
    }
    return true;
}

/***********************************************************************
 *IR Temp allocation code
 * Propagating value life ranges by walking through the function backwards
 * until no more changes are made.
 * In theory this should happen once more than once for every nested loop
 * level.
 * Though this implementation might run an additional time for if nests.
 */

/* Enumerate instructions used by value's life-ranges
 */
static void ir_block_enumerate(ir_block *self, size_t *_eid)
{
    size_t eid = *_eid;
    for (auto &i : self->m_instr)
        i->m_eid = eid++;
    *_eid = eid;
}

/* Enumerate blocks and instructions.
 * The block-enumeration is unordered!
 * We do not really use the block enumreation, however
 * the instruction enumeration is important for life-ranges.
 */
void ir_function_enumerate(ir_function *self)
{
    size_t instruction_id = 0;
    size_t block_eid = 0;
    for (auto& block : self->m_blocks)
    {
        /* each block now gets an additional "entry" instruction id
         * we can use to avoid point-life issues
         */
        block->m_entry_id = instruction_id;
        block->m_eid      = block_eid;
        ++instruction_id;
        ++block_eid;

        ir_block_enumerate(block.get(), &instruction_id);
    }
}

/* Local-value allocator
 * After finishing creating the liferange of all values used in a function
 * we can allocate their global-positions.
 * This is the counterpart to register-allocation in register machines.
 */
struct function_allocator {
    std::vector<std::unique_ptr<ir_value>> locals;
    std::vector<size_t> sizes;
    std::vector<size_t> positions;
    std::vector<bool> unique;
};

static bool function_allocator_alloc(function_allocator *alloc, ir_value *var)
{
    ir_value *slot;
    size_t vsize = var->size();

    var->m_code.local = alloc->locals.size();

    slot = new ir_value("reg", store_global, var->m_vtype);
    if (!slot)
        return false;

    if (!slot->mergeLife(var))
        goto localerror;

    alloc->locals.emplace_back(slot);
    alloc->sizes.push_back(vsize);
    alloc->unique.push_back(var->m_unique_life);

    return true;

localerror:
    delete slot;
    return false;
}

static bool ir_function_allocator_assign(ir_function *self, function_allocator *alloc, ir_value *v)
{
    size_t a;

    if (v->m_unique_life)
        return function_allocator_alloc(alloc, v);

    for (a = 0; a < alloc->locals.size(); ++a)
    {
        /* if it's reserved for a unique liferange: skip */
        if (alloc->unique[a])
            continue;

        ir_value *slot = alloc->locals[a].get();

        /* never resize parameters
         * will be required later when overlapping temps + locals
         */
        if (a < self->m_params.size() &&
            alloc->sizes[a] < v->size())
        {
            continue;
        }

        if (ir_values_overlap(v, slot))
            continue;

        if (!slot->mergeLife(v))
            return false;

        /* adjust size for this slot */
        if (alloc->sizes[a] < v->size())
            alloc->sizes[a] = v->size();

        v->m_code.local = a;
        return true;
    }
    if (a >= alloc->locals.size()) {
        if (!function_allocator_alloc(alloc, v))
            return false;
    }
    return true;
}

bool ir_function_allocate_locals(ir_function *self)
{
    size_t pos;
    bool   opt_gt = OPTS_OPTIMIZATION(OPTIM_GLOBAL_TEMPS);

    function_allocator lockalloc, globalloc;

    if (self->m_locals.empty() && self->m_values.empty())
        return true;

    size_t i;
    for (i = 0; i < self->m_locals.size(); ++i)
    {
        ir_value *v = self->m_locals[i].get();
        if ((self->m_flags & IR_FLAG_MASK_NO_LOCAL_TEMPS) || !OPTS_OPTIMIZATION(OPTIM_LOCAL_TEMPS)) {
            v->m_locked      = true;
            v->m_unique_life = true;
        }
        else if (i >= self->m_params.size())
            break;
        else
            v->m_locked = true; /* lock parameters locals */
        if (!function_allocator_alloc((v->m_locked || !opt_gt ? &lockalloc : &globalloc), v))
            return false;
    }
    for (; i < self->m_locals.size(); ++i)
    {
        ir_value *v = self->m_locals[i].get();
        if (v->m_life.empty())
            continue;
        if (!ir_function_allocator_assign(self, (v->m_locked || !opt_gt ? &lockalloc : &globalloc), v))
            return false;
    }

    /* Allocate a slot for any value that still exists */
    for (i = 0; i < self->m_values.size(); ++i)
    {
        ir_value *v = self->m_values[i].get();

        if (v->m_life.empty())
            continue;

        /* CALL optimization:
         * If the value is a parameter-temp: 1 write, 1 read from a CALL
         * and it's not "locked", write it to the OFS_PARM directly.
         */
        if (OPTS_OPTIMIZATION(OPTIM_CALL_STORES) && !v->m_locked && !v->m_unique_life) {
            if (v->m_reads.size() == 1 && v->m_writes.size() == 1 &&
                (v->m_reads[0]->m_opcode == VINSTR_NRCALL ||
                 (v->m_reads[0]->m_opcode >= INSTR_CALL0 && v->m_reads[0]->m_opcode <= INSTR_CALL8)
                )
               )
            {
                size_t param;
                ir_instr *call = v->m_reads[0];
                if (!vec_ir_value_find(call->m_params, v, &param)) {
                    irerror(call->m_context, "internal error: unlocked parameter %s not found", v->m_name.c_str());
                    return false;
                }
                ++opts_optimizationcount[OPTIM_CALL_STORES];
                v->m_callparam = true;
                if (param < 8)
                    v->setCodeAddress(OFS_PARM0 + 3*param);
                else {
                    size_t nprotos = self->m_owner->m_extparam_protos.size();
                    ir_value *ep;
                    param -= 8;
                    if (nprotos > param)
                        ep = self->m_owner->m_extparam_protos[param].get();
                    else
                    {
                        ep = self->m_owner->generateExtparamProto();
                        while (++nprotos <= param)
                            ep = self->m_owner->generateExtparamProto();
                    }
                    ir_instr_op(v->m_writes[0], 0, ep, true);
                    call->m_params[param+8] = ep;
                }
                continue;
            }
            if (v->m_writes.size() == 1 && v->m_writes[0]->m_opcode == INSTR_CALL0) {
                v->m_store = store_return;
                if (v->m_members[0]) v->m_members[0]->m_store = store_return;
                if (v->m_members[1]) v->m_members[1]->m_store = store_return;
                if (v->m_members[2]) v->m_members[2]->m_store = store_return;
                ++opts_optimizationcount[OPTIM_CALL_STORES];
                continue;
            }
        }

        if (!ir_function_allocator_assign(self, (v->m_locked || !opt_gt ? &lockalloc : &globalloc), v))
            return false;
    }

    if (lockalloc.sizes.empty() && globalloc.sizes.empty())
        return true;

    lockalloc.positions.push_back(0);
    globalloc.positions.push_back(0);

    /* Adjust slot positions based on sizes */
    if (!lockalloc.sizes.empty()) {
        pos = (lockalloc.sizes.size() ? lockalloc.positions[0] : 0);
        for (i = 1; i < lockalloc.sizes.size(); ++i)
        {
            pos = lockalloc.positions[i-1] + lockalloc.sizes[i-1];
            lockalloc.positions.push_back(pos);
        }
        self->m_allocated_locals = pos + lockalloc.sizes.back();
    }
    if (!globalloc.sizes.empty()) {
        pos = (globalloc.sizes.size() ? globalloc.positions[0] : 0);
        for (i = 1; i < globalloc.sizes.size(); ++i)
        {
            pos = globalloc.positions[i-1] + globalloc.sizes[i-1];
            globalloc.positions.push_back(pos);
        }
        self->m_globaltemps = pos + globalloc.sizes.back();
    }

    /* Locals need to know their new position */
    for (auto& local : self->m_locals) {
        if (local->m_locked || !opt_gt)
            local->m_code.local = lockalloc.positions[local->m_code.local];
        else
            local->m_code.local = globalloc.positions[local->m_code.local];
    }
    /* Take over the actual slot positions on values */
    for (auto& value : self->m_values) {
        if (value->m_locked || !opt_gt)
            value->m_code.local = lockalloc.positions[value->m_code.local];
        else
            value->m_code.local = globalloc.positions[value->m_code.local];
    }

    return true;
}

/* Get information about which operand
 * is read from, or written to.
 */
static void ir_op_read_write(int op, size_t *read, size_t *write)
{
    switch (op)
    {
    case VINSTR_JUMP:
    case INSTR_GOTO:
        *write = 0;
        *read = 0;
        break;
    case INSTR_IF:
    case INSTR_IFNOT:
#if 0
    case INSTR_IF_S:
    case INSTR_IFNOT_S:
#endif
    case INSTR_RETURN:
    case VINSTR_COND:
        *write = 0;
        *read = 1;
        break;
    case INSTR_STOREP_F:
    case INSTR_STOREP_V:
    case INSTR_STOREP_S:
    case INSTR_STOREP_ENT:
    case INSTR_STOREP_FLD:
    case INSTR_STOREP_FNC:
        *write = 0;
        *read  = 7;
        break;
    default:
        *write = 1;
        *read = 6;
        break;
    };
}

static bool ir_block_living_add_instr(ir_block *self, size_t eid) {
    bool changed = false;
    for (auto &it : self->m_living)
        if (it->setAlive(eid))
            changed = true;
    return changed;
}

static bool ir_block_living_lock(ir_block *self) {
    bool changed = false;
    for (auto &it : self->m_living) {
        if (it->m_locked)
            continue;
        it->m_locked = true;
        changed = true;
    }
    return changed;
}

static bool ir_block_life_propagate(ir_block *self, bool *changed)
{
    ir_instr *instr;
    ir_value *value;
    size_t i, o, mem;
    // bitmasks which operands are read from or written to
    size_t read, write;

    self->m_living.clear();

    for (auto &prev : self->m_exits) {
        for (auto &it : prev->m_living)
            if (!vec_ir_value_find(self->m_living, it, nullptr))
                self->m_living.push_back(it);
    }

    i = self->m_instr.size();
    while (i)
    { --i;
        instr = self->m_instr[i];

        /* See which operands are read and write operands */
        ir_op_read_write(instr->m_opcode, &read, &write);

        /* Go through the 3 main operands
         * writes first, then reads
         */
        for (o = 0; o < 3; ++o)
        {
            if (!instr->_m_ops[o]) /* no such operand */
                continue;

            value = instr->_m_ops[o];

            /* We only care about locals */
            /* we also calculate parameter liferanges so that locals
             * can take up parameter slots */
            if (value->m_store != store_value &&
                value->m_store != store_local &&
                value->m_store != store_param)
                continue;

            /* write operands */
            /* When we write to a local, we consider it "dead" for the
             * remaining upper part of the function, since in SSA a value
             * can only be written once (== created)
             */
            if (write & (1<<o))
            {
                size_t idx;
                bool in_living = vec_ir_value_find(self->m_living, value, &idx);
                if (!in_living)
                {
                    /* If the value isn't alive it hasn't been read before... */
                    /* TODO: See if the warning can be emitted during parsing or AST processing
                     * otherwise have warning printed here.
                     * IF printing a warning here: include filecontext_t,
                     * and make sure it's only printed once
                     * since this function is run multiple times.
                     */
                    /* con_err( "Value only written %s\n", value->m_name); */
                    if (value->setAlive(instr->m_eid))
                        *changed = true;
                } else {
                    /* since 'living' won't contain it
                     * anymore, merge the value, since
                     * (A) doesn't.
                     */
                    if (value->setAlive(instr->m_eid))
                        *changed = true;
                    // Then remove
                    self->m_living.erase(self->m_living.begin() + idx);
                }
                /* Removing a vector removes all members */
                for (mem = 0; mem < 3; ++mem) {
                    if (value->m_members[mem] && vec_ir_value_find(self->m_living, value->m_members[mem], &idx)) {
                        if (value->m_members[mem]->setAlive(instr->m_eid))
                            *changed = true;
                        self->m_living.erase(self->m_living.begin() + idx);
                    }
                }
                /* Removing the last member removes the vector */
                if (value->m_memberof) {
                    value = value->m_memberof;
                    for (mem = 0; mem < 3; ++mem) {
                        if (value->m_members[mem] && vec_ir_value_find(self->m_living, value->m_members[mem], nullptr))
                            break;
                    }
                    if (mem == 3 && vec_ir_value_find(self->m_living, value, &idx)) {
                        if (value->setAlive(instr->m_eid))
                            *changed = true;
                        self->m_living.erase(self->m_living.begin() + idx);
                    }
                }
            }
        }

        /* These operations need a special case as they can break when using
         * same source and destination operand otherwise, as the engine may
         * read the source multiple times. */
        if (instr->m_opcode == INSTR_MUL_VF ||
            instr->m_opcode == VINSTR_BITAND_VF ||
            instr->m_opcode == VINSTR_BITOR_VF ||
            instr->m_opcode == VINSTR_BITXOR ||
            instr->m_opcode == VINSTR_BITXOR_VF ||
            instr->m_opcode == VINSTR_BITXOR_V ||
            instr->m_opcode == VINSTR_CROSS)
        {
            value = instr->_m_ops[2];
            /* the float source will get an additional lifetime */
            if (value->setAlive(instr->m_eid+1))
                *changed = true;
            if (value->m_memberof && value->m_memberof->setAlive(instr->m_eid+1))
                *changed = true;
        }

        if (instr->m_opcode == INSTR_MUL_FV ||
            instr->m_opcode == INSTR_LOAD_V ||
            instr->m_opcode == VINSTR_BITXOR ||
            instr->m_opcode == VINSTR_BITXOR_VF ||
            instr->m_opcode == VINSTR_BITXOR_V ||
            instr->m_opcode == VINSTR_CROSS)
        {
            value = instr->_m_ops[1];
            /* the float source will get an additional lifetime */
            if (value->setAlive(instr->m_eid+1))
                *changed = true;
            if (value->m_memberof && value->m_memberof->setAlive(instr->m_eid+1))
                *changed = true;
        }

        for (o = 0; o < 3; ++o)
        {
            if (!instr->_m_ops[o]) /* no such operand */
                continue;

            value = instr->_m_ops[o];

            /* We only care about locals */
            /* we also calculate parameter liferanges so that locals
             * can take up parameter slots */
            if (value->m_store != store_value &&
                value->m_store != store_local &&
                value->m_store != store_param)
                continue;

            /* read operands */
            if (read & (1<<o))
            {
                if (!vec_ir_value_find(self->m_living, value, nullptr))
                    self->m_living.push_back(value);
                /* reading adds the full vector */
                if (value->m_memberof && !vec_ir_value_find(self->m_living, value->m_memberof, nullptr))
                    self->m_living.push_back(value->m_memberof);
                for (mem = 0; mem < 3; ++mem) {
                    if (value->m_members[mem] && !vec_ir_value_find(self->m_living, value->m_members[mem], nullptr))
                        self->m_living.push_back(value->m_members[mem]);
                }
            }
        }
        /* PHI operands are always read operands */
        for (auto &it : instr->m_phi) {
            value = it.value;
            if (!vec_ir_value_find(self->m_living, value, nullptr))
                self->m_living.push_back(value);
            /* reading adds the full vector */
            if (value->m_memberof && !vec_ir_value_find(self->m_living, value->m_memberof, nullptr))
                self->m_living.push_back(value->m_memberof);
            for (mem = 0; mem < 3; ++mem) {
                if (value->m_members[mem] && !vec_ir_value_find(self->m_living, value->m_members[mem], nullptr))
                    self->m_living.push_back(value->m_members[mem]);
            }
        }

        /* on a call, all these values must be "locked" */
        if (instr->m_opcode >= INSTR_CALL0 && instr->m_opcode <= INSTR_CALL8) {
            if (ir_block_living_lock(self))
                *changed = true;
        }
        /* call params are read operands too */
        for (auto &it : instr->m_params) {
            value = it;
            if (!vec_ir_value_find(self->m_living, value, nullptr))
                self->m_living.push_back(value);
            /* reading adds the full vector */
            if (value->m_memberof && !vec_ir_value_find(self->m_living, value->m_memberof, nullptr))
                self->m_living.push_back(value->m_memberof);
            for (mem = 0; mem < 3; ++mem) {
                if (value->m_members[mem] && !vec_ir_value_find(self->m_living, value->m_members[mem], nullptr))
                    self->m_living.push_back(value->m_members[mem]);
            }
        }

        /* (A) */
        if (ir_block_living_add_instr(self, instr->m_eid))
            *changed = true;
    }
    /* the "entry" instruction ID */
    if (ir_block_living_add_instr(self, self->m_entry_id))
        *changed = true;

    return true;
}

bool ir_function_calculate_liferanges(ir_function *self)
{
    /* parameters live at 0 */
    for (size_t i = 0; i < self->m_params.size(); ++i)
        if (!self->m_locals[i].get()->setAlive(0))
            compile_error(self->m_context, "internal error: failed value-life merging");

    bool changed;
    do {
        self->m_run_id++;
        changed = false;
        for (auto i = self->m_blocks.rbegin(); i != self->m_blocks.rend(); ++i)
            ir_block_life_propagate(i->get(), &changed);
    } while (changed);

    if (self->m_blocks.size()) {
        ir_block *block = self->m_blocks[0].get();
        for (auto &it : block->m_living) {
            ir_value *v = it;
            if (v->m_store != store_local)
                continue;
            if (v->m_vtype == TYPE_VECTOR)
                continue;
            self->m_flags |= IR_FLAG_HAS_UNINITIALIZED;
            /* find the instruction reading from it */
            size_t s = 0;
            for (; s < v->m_reads.size(); ++s) {
                if (v->m_reads[s]->m_eid == v->m_life[0].end)
                    break;
            }
            if (s < v->m_reads.size()) {
                if (irwarning(v->m_context, WARN_USED_UNINITIALIZED,
                              "variable `%s` may be used uninitialized in this function\n"
                              " -> %s:%i",
                              v->m_name.c_str(),
                              v->m_reads[s]->m_context.file, v->m_reads[s]->m_context.line)
                   )
                {
                    return false;
                }
                continue;
            }
            if (v->m_memberof) {
                ir_value *vec = v->m_memberof;
                for (s = 0; s < vec->m_reads.size(); ++s) {
                    if (vec->m_reads[s]->m_eid == v->m_life[0].end)
                        break;
                }
                if (s < vec->m_reads.size()) {
                    if (irwarning(v->m_context, WARN_USED_UNINITIALIZED,
                                  "variable `%s` may be used uninitialized in this function\n"
                                  " -> %s:%i",
                                  v->m_name.c_str(),
                                  vec->m_reads[s]->m_context.file, vec->m_reads[s]->m_context.line)
                       )
                    {
                        return false;
                    }
                    continue;
                }
            }
            if (irwarning(v->m_context, WARN_USED_UNINITIALIZED,
                          "variable `%s` may be used uninitialized in this function", v->m_name.c_str()))
            {
                return false;
            }
        }
    }
    return true;
}

/***********************************************************************
 *IR Code-Generation
 *
 * Since the IR has the convention of putting 'write' operands
 * at the beginning, we have to rotate the operands of instructions
 * properly in order to generate valid QCVM code.
 *
 * Having destinations at a fixed position is more convenient. In QC
 * this is *mostly* OPC,  but FTE adds at least 2 instructions which
 * read from from OPA,  and store to OPB rather than OPC.   Which is
 * partially the reason why the implementation of these instructions
 * in darkplaces has been delayed for so long.
 *
 * Breaking conventions is annoying...
 */
static bool gen_global_field(code_t *code, ir_value *global)
{
    if (global->m_hasvalue)
    {
        ir_value *fld = global->m_constval.vpointer;
        if (!fld) {
            irerror(global->m_context, "Invalid field constant with no field: %s", global->m_name.c_str());
            return false;
        }

        /* copy the field's value */
        global->setCodeAddress(code->globals.size());
        code->globals.push_back(fld->m_code.fieldaddr);
        if (global->m_fieldtype == TYPE_VECTOR) {
            code->globals.push_back(fld->m_code.fieldaddr+1);
            code->globals.push_back(fld->m_code.fieldaddr+2);
        }
    }
    else
    {
        global->setCodeAddress(code->globals.size());
        code->globals.push_back(0);
        if (global->m_fieldtype == TYPE_VECTOR) {
            code->globals.push_back(0);
            code->globals.push_back(0);
        }
    }
    if (global->m_code.globaladdr < 0)
        return false;
    return true;
}

static bool gen_global_pointer(code_t *code, ir_value *global)
{
    if (global->m_hasvalue)
    {
        ir_value *target = global->m_constval.vpointer;
        if (!target) {
            irerror(global->m_context, "Invalid pointer constant: %s", global->m_name.c_str());
            /* nullptr pointers are pointing to the nullptr constant, which also
             * sits at address 0, but still has an ir_value for itself.
             */
            return false;
        }

        /* Here, relocations ARE possible - in fteqcc-enhanced-qc:
         * void() foo; <- proto
         * void() *fooptr = &foo;
         * void() foo = { code }
         */
        if (!target->m_code.globaladdr) {
            /* FIXME: Check for the constant nullptr ir_value!
             * because then code.globaladdr being 0 is valid.
             */
            irerror(global->m_context, "FIXME: Relocation support");
            return false;
        }

        global->setCodeAddress(code->globals.size());
        code->globals.push_back(target->m_code.globaladdr);
    }
    else
    {
        global->setCodeAddress(code->globals.size());
        code->globals.push_back(0);
    }
    if (global->m_code.globaladdr < 0)
        return false;
    return true;
}

static bool gen_blocks_recursive(code_t *code, ir_function *func, ir_block *block)
{
    prog_section_statement_t stmt;
    ir_instr *instr;
    ir_block *target;
    ir_block *ontrue;
    ir_block *onfalse;
    size_t    stidx;
    size_t    i;
    int       j;

    block->m_generated = true;
    block->m_code_start = code->statements.size();
    for (i = 0; i < block->m_instr.size(); ++i)
    {
        instr = block->m_instr[i];

        if (instr->m_opcode == VINSTR_PHI) {
            irerror(block->m_context, "cannot generate virtual instruction (phi)");
            return false;
        }

        if (instr->m_opcode == VINSTR_JUMP) {
            target = instr->m_bops[0];
            /* for uncoditional jumps, if the target hasn't been generated
             * yet, we generate them right here.
             */
            if (!target->m_generated)
                return gen_blocks_recursive(code, func, target);

            /* otherwise we generate a jump instruction */
            stmt.opcode = INSTR_GOTO;
            stmt.o1.s1 = target->m_code_start - code->statements.size();
            stmt.o2.s1 = 0;
            stmt.o3.s1 = 0;
            if (stmt.o1.s1 != 1)
                code_push_statement(code, &stmt, instr->m_context);

            /* no further instructions can be in this block */
            return true;
        }

        if (instr->m_opcode == VINSTR_BITXOR) {
            stmt.opcode = INSTR_BITOR;
            stmt.o1.s1 = instr->_m_ops[1]->codeAddress();
            stmt.o2.s1 = instr->_m_ops[2]->codeAddress();
            stmt.o3.s1 = instr->_m_ops[0]->codeAddress();
            code_push_statement(code, &stmt, instr->m_context);
            stmt.opcode = INSTR_BITAND;
            stmt.o1.s1 = instr->_m_ops[1]->codeAddress();
            stmt.o2.s1 = instr->_m_ops[2]->codeAddress();
            stmt.o3.s1 = func->m_owner->m_vinstr_temp[0]->codeAddress();
            code_push_statement(code, &stmt, instr->m_context);
            stmt.opcode = INSTR_SUB_F;
            stmt.o1.s1 = instr->_m_ops[0]->codeAddress();
            stmt.o2.s1 = func->m_owner->m_vinstr_temp[0]->codeAddress();
            stmt.o3.s1 = instr->_m_ops[0]->codeAddress();
            code_push_statement(code, &stmt, instr->m_context);

            /* instruction generated */
            continue;
        }

        if (instr->m_opcode == VINSTR_BITAND_V) {
            stmt.opcode = INSTR_BITAND;
            stmt.o1.s1 = instr->_m_ops[1]->codeAddress();
            stmt.o2.s1 = instr->_m_ops[2]->codeAddress();
            stmt.o3.s1 = instr->_m_ops[0]->codeAddress();
            code_push_statement(code, &stmt, instr->m_context);
            ++stmt.o1.s1;
            ++stmt.o2.s1;
            ++stmt.o3.s1;
            code_push_statement(code, &stmt, instr->m_context);
            ++stmt.o1.s1;
            ++stmt.o2.s1;
            ++stmt.o3.s1;
            code_push_statement(code, &stmt, instr->m_context);

            /* instruction generated */
            continue;
        }

        if (instr->m_opcode == VINSTR_BITOR_V) {
            stmt.opcode = INSTR_BITOR;
            stmt.o1.s1 = instr->_m_ops[1]->codeAddress();
            stmt.o2.s1 = instr->_m_ops[2]->codeAddress();
            stmt.o3.s1 = instr->_m_ops[0]->codeAddress();
            code_push_statement(code, &stmt, instr->m_context);
            ++stmt.o1.s1;
            ++stmt.o2.s1;
            ++stmt.o3.s1;
            code_push_statement(code, &stmt, instr->m_context);
            ++stmt.o1.s1;
            ++stmt.o2.s1;
            ++stmt.o3.s1;
            code_push_statement(code, &stmt, instr->m_context);

            /* instruction generated */
            continue;
        }

        if (instr->m_opcode == VINSTR_BITXOR_V) {
            for (j = 0; j < 3; ++j) {
                stmt.opcode = INSTR_BITOR;
                stmt.o1.s1 = instr->_m_ops[1]->codeAddress() + j;
                stmt.o2.s1 = instr->_m_ops[2]->codeAddress() + j;
                stmt.o3.s1 = instr->_m_ops[0]->codeAddress() + j;
                code_push_statement(code, &stmt, instr->m_context);
                stmt.opcode = INSTR_BITAND;
                stmt.o1.s1 = instr->_m_ops[1]->codeAddress() + j;
                stmt.o2.s1 = instr->_m_ops[2]->codeAddress() + j;
                stmt.o3.s1 = func->m_owner->m_vinstr_temp[0]->codeAddress() + j;
                code_push_statement(code, &stmt, instr->m_context);
            }
            stmt.opcode = INSTR_SUB_V;
            stmt.o1.s1 = instr->_m_ops[0]->codeAddress();
            stmt.o2.s1 = func->m_owner->m_vinstr_temp[0]->codeAddress();
            stmt.o3.s1 = instr->_m_ops[0]->codeAddress();
            code_push_statement(code, &stmt, instr->m_context);

            /* instruction generated */
            continue;
        }

        if (instr->m_opcode == VINSTR_BITAND_VF) {
            stmt.opcode = INSTR_BITAND;
            stmt.o1.s1 = instr->_m_ops[1]->codeAddress();
            stmt.o2.s1 = instr->_m_ops[2]->codeAddress();
            stmt.o3.s1 = instr->_m_ops[0]->codeAddress();
            code_push_statement(code, &stmt, instr->m_context);
            ++stmt.o1.s1;
            ++stmt.o3.s1;
            code_push_statement(code, &stmt, instr->m_context);
            ++stmt.o1.s1;
            ++stmt.o3.s1;
            code_push_statement(code, &stmt, instr->m_context);

            /* instruction generated */
            continue;
        }

        if (instr->m_opcode == VINSTR_BITOR_VF) {
            stmt.opcode = INSTR_BITOR;
            stmt.o1.s1 = instr->_m_ops[1]->codeAddress();
            stmt.o2.s1 = instr->_m_ops[2]->codeAddress();
            stmt.o3.s1 = instr->_m_ops[0]->codeAddress();
            code_push_statement(code, &stmt, instr->m_context);
            ++stmt.o1.s1;
            ++stmt.o3.s1;
            code_push_statement(code, &stmt, instr->m_context);
            ++stmt.o1.s1;
            ++stmt.o3.s1;
            code_push_statement(code, &stmt, instr->m_context);

            /* instruction generated */
            continue;
        }

        if (instr->m_opcode == VINSTR_BITXOR_VF) {
            for (j = 0; j < 3; ++j) {
                stmt.opcode = INSTR_BITOR;
                stmt.o1.s1 = instr->_m_ops[1]->codeAddress() + j;
                stmt.o2.s1 = instr->_m_ops[2]->codeAddress();
                stmt.o3.s1 = instr->_m_ops[0]->codeAddress() + j;
                code_push_statement(code, &stmt, instr->m_context);
                stmt.opcode = INSTR_BITAND;
                stmt.o1.s1 = instr->_m_ops[1]->codeAddress() + j;
                stmt.o2.s1 = instr->_m_ops[2]->codeAddress();
                stmt.o3.s1 = func->m_owner->m_vinstr_temp[0]->codeAddress() + j;
                code_push_statement(code, &stmt, instr->m_context);
            }
            stmt.opcode = INSTR_SUB_V;
            stmt.o1.s1 = instr->_m_ops[0]->codeAddress();
            stmt.o2.s1 = func->m_owner->m_vinstr_temp[0]->codeAddress();
            stmt.o3.s1 = instr->_m_ops[0]->codeAddress();
            code_push_statement(code, &stmt, instr->m_context);

            /* instruction generated */
            continue;
        }

        if (instr->m_opcode == VINSTR_CROSS) {
            stmt.opcode = INSTR_MUL_F;
            for (j = 0; j < 3; ++j) {
                stmt.o1.s1 = instr->_m_ops[1]->codeAddress() + (j + 1) % 3;
                stmt.o2.s1 = instr->_m_ops[2]->codeAddress() + (j + 2) % 3;
                stmt.o3.s1 = instr->_m_ops[0]->codeAddress() + j;
                code_push_statement(code, &stmt, instr->m_context);
                stmt.o1.s1 = instr->_m_ops[1]->codeAddress() + (j + 2) % 3;
                stmt.o2.s1 = instr->_m_ops[2]->codeAddress() + (j + 1) % 3;
                stmt.o3.s1 = func->m_owner->m_vinstr_temp[0]->codeAddress() + j;
                code_push_statement(code, &stmt, instr->m_context);
            }
            stmt.opcode = INSTR_SUB_V;
            stmt.o1.s1 = instr->_m_ops[0]->codeAddress();
            stmt.o2.s1 = func->m_owner->m_vinstr_temp[0]->codeAddress();
            stmt.o3.s1 = instr->_m_ops[0]->codeAddress();
            code_push_statement(code, &stmt, instr->m_context);

            /* instruction generated */
            continue;
        }

        if (instr->m_opcode == VINSTR_COND) {
            ontrue  = instr->m_bops[0];
            onfalse = instr->m_bops[1];
            /* TODO: have the AST signal which block should
             * come first: eg. optimize IFs without ELSE...
             */

            stmt.o1.u1 = instr->_m_ops[0]->codeAddress();
            stmt.o2.u1 = 0;
            stmt.o3.s1 = 0;

            if (ontrue->m_generated) {
                stmt.opcode = INSTR_IF;
                stmt.o2.s1 = ontrue->m_code_start - code->statements.size();
                if (stmt.o2.s1 != 1)
                    code_push_statement(code, &stmt, instr->m_context);
            }
            if (onfalse->m_generated) {
                stmt.opcode = INSTR_IFNOT;
                stmt.o2.s1 = onfalse->m_code_start - code->statements.size();
                if (stmt.o2.s1 != 1)
                    code_push_statement(code, &stmt, instr->m_context);
            }
            if (!ontrue->m_generated) {
                if (onfalse->m_generated)
                    return gen_blocks_recursive(code, func, ontrue);
            }
            if (!onfalse->m_generated) {
                if (ontrue->m_generated)
                    return gen_blocks_recursive(code, func, onfalse);
            }
            /* neither ontrue nor onfalse exist */
            stmt.opcode = INSTR_IFNOT;
            if (!instr->m_likely) {
                /* Honor the likelyhood hint */
                ir_block *tmp = onfalse;
                stmt.opcode = INSTR_IF;
                onfalse = ontrue;
                ontrue = tmp;
            }
            stidx = code->statements.size();
            code_push_statement(code, &stmt, instr->m_context);
            /* on false we jump, so add ontrue-path */
            if (!gen_blocks_recursive(code, func, ontrue))
                return false;
            /* fixup the jump address */
            code->statements[stidx].o2.s1 = code->statements.size() - stidx;
            /* generate onfalse path */
            if (onfalse->m_generated) {
                /* fixup the jump address */
                code->statements[stidx].o2.s1 = onfalse->m_code_start - stidx;
                if (stidx+2 == code->statements.size() && code->statements[stidx].o2.s1 == 1) {
                    code->statements[stidx] = code->statements[stidx+1];
                    if (code->statements[stidx].o1.s1 < 0)
                        code->statements[stidx].o1.s1++;
                    code_pop_statement(code);
                }
                stmt.opcode = code->statements.back().opcode;
                if (stmt.opcode == INSTR_GOTO ||
                    stmt.opcode == INSTR_IF ||
                    stmt.opcode == INSTR_IFNOT ||
                    stmt.opcode == INSTR_RETURN ||
                    stmt.opcode == INSTR_DONE)
                {
                    /* no use jumping from here */
                    return true;
                }
                /* may have been generated in the previous recursive call */
                stmt.opcode = INSTR_GOTO;
                stmt.o1.s1 = onfalse->m_code_start - code->statements.size();
                stmt.o2.s1 = 0;
                stmt.o3.s1 = 0;
                if (stmt.o1.s1 != 1)
                    code_push_statement(code, &stmt, instr->m_context);
                return true;
            }
            else if (stidx+2 == code->statements.size() && code->statements[stidx].o2.s1 == 1) {
                code->statements[stidx] = code->statements[stidx+1];
                if (code->statements[stidx].o1.s1 < 0)
                    code->statements[stidx].o1.s1++;
                code_pop_statement(code);
            }
            /* if not, generate now */
            return gen_blocks_recursive(code, func, onfalse);
        }

        if ( (instr->m_opcode >= INSTR_CALL0 && instr->m_opcode <= INSTR_CALL8)
           || instr->m_opcode == VINSTR_NRCALL)
        {
            size_t p, first;
            ir_value *retvalue;

            first = instr->m_params.size();
            if (first > 8)
                first = 8;
            for (p = 0; p < first; ++p)
            {
                ir_value *param = instr->m_params[p];
                if (param->m_callparam)
                    continue;

                stmt.opcode = INSTR_STORE_F;
                stmt.o3.u1 = 0;

                if (param->m_vtype == TYPE_FIELD)
                    stmt.opcode = field_store_instr[param->m_fieldtype];
                else if (param->m_vtype == TYPE_NIL)
                    stmt.opcode = INSTR_STORE_V;
                else
                    stmt.opcode = type_store_instr[param->m_vtype];
                stmt.o1.u1 = param->codeAddress();
                stmt.o2.u1 = OFS_PARM0 + 3 * p;

                if (param->m_vtype == TYPE_VECTOR && (param->m_flags & IR_FLAG_SPLIT_VECTOR)) {
                    /* fetch 3 separate floats */
                    stmt.opcode = INSTR_STORE_F;
                    stmt.o1.u1 = param->m_members[0]->codeAddress();
                    code_push_statement(code, &stmt, instr->m_context);
                    stmt.o2.u1++;
                    stmt.o1.u1 = param->m_members[1]->codeAddress();
                    code_push_statement(code, &stmt, instr->m_context);
                    stmt.o2.u1++;
                    stmt.o1.u1 = param->m_members[2]->codeAddress();
                    code_push_statement(code, &stmt, instr->m_context);
                }
                else
                    code_push_statement(code, &stmt, instr->m_context);
            }
            /* Now handle extparams */
            first = instr->m_params.size();
            for (; p < first; ++p)
            {
                ir_builder *ir = func->m_owner;
                ir_value *param = instr->m_params[p];
                ir_value *targetparam;

                if (param->m_callparam)
                    continue;

                if (p-8 >= ir->m_extparams.size())
                    ir->generateExtparam();

                targetparam = ir->m_extparams[p-8];

                stmt.opcode = INSTR_STORE_F;
                stmt.o3.u1 = 0;

                if (param->m_vtype == TYPE_FIELD)
                    stmt.opcode = field_store_instr[param->m_fieldtype];
                else if (param->m_vtype == TYPE_NIL)
                    stmt.opcode = INSTR_STORE_V;
                else
                    stmt.opcode = type_store_instr[param->m_vtype];
                stmt.o1.u1 = param->codeAddress();
                stmt.o2.u1 = targetparam->codeAddress();
                if (param->m_vtype == TYPE_VECTOR && (param->m_flags & IR_FLAG_SPLIT_VECTOR)) {
                    /* fetch 3 separate floats */
                    stmt.opcode = INSTR_STORE_F;
                    stmt.o1.u1 = param->m_members[0]->codeAddress();
                    code_push_statement(code, &stmt, instr->m_context);
                    stmt.o2.u1++;
                    stmt.o1.u1 = param->m_members[1]->codeAddress();
                    code_push_statement(code, &stmt, instr->m_context);
                    stmt.o2.u1++;
                    stmt.o1.u1 = param->m_members[2]->codeAddress();
                    code_push_statement(code, &stmt, instr->m_context);
                }
                else
                    code_push_statement(code, &stmt, instr->m_context);
            }

            stmt.opcode = INSTR_CALL0 + instr->m_params.size();
            if (stmt.opcode > INSTR_CALL8)
                stmt.opcode = INSTR_CALL8;
            stmt.o1.u1 = instr->_m_ops[1]->codeAddress();
            stmt.o2.u1 = 0;
            stmt.o3.u1 = 0;
            code_push_statement(code, &stmt, instr->m_context);

            retvalue = instr->_m_ops[0];
            if (retvalue && retvalue->m_store != store_return &&
                (retvalue->m_store == store_global || retvalue->m_life.size()))
            {
                /* not to be kept in OFS_RETURN */
                if (retvalue->m_vtype == TYPE_FIELD && OPTS_FLAG(ADJUST_VECTOR_FIELDS))
                    stmt.opcode = field_store_instr[retvalue->m_fieldtype];
                else
                    stmt.opcode = type_store_instr[retvalue->m_vtype];
                stmt.o1.u1 = OFS_RETURN;
                stmt.o2.u1 = retvalue->codeAddress();
                stmt.o3.u1 = 0;
                code_push_statement(code, &stmt, instr->m_context);
            }
            continue;
        }

        if (instr->m_opcode == INSTR_STATE) {
            stmt.opcode = instr->m_opcode;
            if (instr->_m_ops[0])
                stmt.o1.u1 = instr->_m_ops[0]->codeAddress();
            if (instr->_m_ops[1])
                stmt.o2.u1 = instr->_m_ops[1]->codeAddress();
            stmt.o3.u1 = 0;
            code_push_statement(code, &stmt, instr->m_context);
            continue;
        }

        stmt.opcode = instr->m_opcode;
        stmt.o1.u1 = 0;
        stmt.o2.u1 = 0;
        stmt.o3.u1 = 0;

        /* This is the general order of operands */
        if (instr->_m_ops[0])
            stmt.o3.u1 = instr->_m_ops[0]->codeAddress();

        if (instr->_m_ops[1])
            stmt.o1.u1 = instr->_m_ops[1]->codeAddress();

        if (instr->_m_ops[2])
            stmt.o2.u1 = instr->_m_ops[2]->codeAddress();

        if (stmt.opcode == INSTR_RETURN || stmt.opcode == INSTR_DONE)
        {
            stmt.o1.u1 = stmt.o3.u1;
            stmt.o3.u1 = 0;
        }
        else if ((stmt.opcode >= INSTR_STORE_F &&
                  stmt.opcode <= INSTR_STORE_FNC) ||
                 (stmt.opcode >= INSTR_STOREP_F &&
                  stmt.opcode <= INSTR_STOREP_FNC))
        {
            /* 2-operand instructions with A -> B */
            stmt.o2.u1 = stmt.o3.u1;
            stmt.o3.u1 = 0;

            /* tiny optimization, don't output
             * STORE a, a
             */
            if (stmt.o2.u1 == stmt.o1.u1 &&
                OPTS_OPTIMIZATION(OPTIM_PEEPHOLE))
            {
                ++opts_optimizationcount[OPTIM_PEEPHOLE];
                continue;
            }
        }
        code_push_statement(code, &stmt, instr->m_context);
    }
    return true;
}

static bool gen_function_code(code_t *code, ir_function *self)
{
    ir_block *block;
    prog_section_statement_t stmt, *retst;

    /* Starting from entry point, we generate blocks "as they come"
     * for now. Dead blocks will not be translated obviously.
     */
    if (self->m_blocks.empty()) {
        irerror(self->m_context, "Function '%s' declared without body.", self->m_name.c_str());
        return false;
    }

    block = self->m_blocks[0].get();
    if (block->m_generated)
        return true;

    if (!gen_blocks_recursive(code, self, block)) {
        irerror(self->m_context, "failed to generate blocks for '%s'", self->m_name.c_str());
        return false;
    }

    /* code_write and qcvm -disasm need to know that the function ends here */
    retst = &code->statements.back();
    if (OPTS_OPTIMIZATION(OPTIM_VOID_RETURN) &&
        self->m_outtype == TYPE_VOID &&
        retst->opcode == INSTR_RETURN &&
        !retst->o1.u1 && !retst->o2.u1 && !retst->o3.u1)
    {
        retst->opcode = INSTR_DONE;
        ++opts_optimizationcount[OPTIM_VOID_RETURN];
    } else {
        lex_ctx_t last;

        stmt.opcode = INSTR_DONE;
        stmt.o1.u1  = 0;
        stmt.o2.u1  = 0;
        stmt.o3.u1  = 0;
        last.line   = code->linenums.back();
        last.column = code->columnnums.back();

        code_push_statement(code, &stmt, last);
    }
    return true;
}

qcint_t ir_builder::filestring(const char *filename)
{
    /* NOTE: filename pointers are copied, we never strdup them,
     * thus we can use pointer-comparison to find the string.
     */
    qcint_t  str;

    for (size_t i = 0; i != m_filenames.size(); ++i) {
        if (!strcmp(m_filenames[i], filename))
            return i;
    }

    str = code_genstring(m_code.get(), filename);
    m_filenames.push_back(filename);
    m_filestrings.push_back(str);
    return str;
}

bool ir_builder::generateGlobalFunction(ir_value *global)
{
    prog_section_function_t fun;
    ir_function            *irfun;

    size_t i;

    if (!global->m_hasvalue || (!global->m_constval.vfunc)) {
        irerror(global->m_context, "Invalid state of function-global: not constant: %s", global->m_name.c_str());
        return false;
    }

    irfun = global->m_constval.vfunc;
    fun.name = global->m_code.name;
    fun.file = filestring(global->m_context.file);
    fun.profile = 0; /* always 0 */
    fun.nargs = irfun->m_params.size();
    if (fun.nargs > 8)
        fun.nargs = 8;

    for (i = 0; i < 8; ++i) {
        if ((int32_t)i >= fun.nargs)
            fun.argsize[i] = 0;
        else
            fun.argsize[i] = type_sizeof_[irfun->m_params[i]];
    }

    fun.firstlocal = 0;
    fun.locals = irfun->m_allocated_locals;

    if (irfun->m_builtin)
        fun.entry = irfun->m_builtin+1;
    else {
        irfun->m_code_function_def = m_code->functions.size();
        fun.entry = m_code->statements.size();
    }

    m_code->functions.push_back(fun);
    return true;
}

ir_value* ir_builder::generateExtparamProto()
{
    char      name[128];

    util_snprintf(name, sizeof(name), "EXTPARM#%i", (int)(m_extparam_protos.size()));
    ir_value *global = new ir_value(name, store_global, TYPE_VECTOR);
    m_extparam_protos.emplace_back(global);

    return global;
}

void ir_builder::generateExtparam()
{
    prog_section_def_t def;
    ir_value          *global;

    if (m_extparam_protos.size() < m_extparams.size()+1)
        global = generateExtparamProto();
    else
        global = m_extparam_protos[m_extparams.size()].get();

    def.name = code_genstring(m_code.get(), global->m_name.c_str());
    def.type = TYPE_VECTOR;
    def.offset = m_code->globals.size();

    m_code->defs.push_back(def);

    global->setCodeAddress(def.offset);

    m_code->globals.push_back(0);
    m_code->globals.push_back(0);
    m_code->globals.push_back(0);

    m_extparams.emplace_back(global);
}

static bool gen_function_extparam_copy(code_t *code, ir_function *self)
{
    ir_builder *ir = self->m_owner;

    size_t numparams = self->m_params.size();
    if (!numparams)
        return true;

    prog_section_statement_t stmt;
    stmt.opcode = INSTR_STORE_F;
    stmt.o3.s1 = 0;
    for (size_t i = 8; i < numparams; ++i) {
        size_t ext = i - 8;
        if (ext >= ir->m_extparams.size())
            ir->generateExtparam();

        ir_value *ep = ir->m_extparams[ext];

        stmt.opcode = type_store_instr[self->m_locals[i]->m_vtype];
        if (self->m_locals[i]->m_vtype == TYPE_FIELD &&
            self->m_locals[i]->m_fieldtype == TYPE_VECTOR)
        {
            stmt.opcode = INSTR_STORE_V;
        }
        stmt.o1.u1 = ep->codeAddress();
        stmt.o2.u1 = self->m_locals[i].get()->codeAddress();
        code_push_statement(code, &stmt, self->m_context);
    }

    return true;
}

static bool gen_function_varargs_copy(code_t *code, ir_function *self)
{
    size_t i, ext, numparams, maxparams;

    ir_builder *ir = self->m_owner;
    ir_value   *ep;
    prog_section_statement_t stmt;

    numparams = self->m_params.size();
    if (!numparams)
        return true;

    stmt.opcode = INSTR_STORE_V;
    stmt.o3.s1 = 0;
    maxparams = numparams + self->m_max_varargs;
    for (i = numparams; i < maxparams; ++i) {
        if (i < 8) {
            stmt.o1.u1 = OFS_PARM0 + 3*i;
            stmt.o2.u1 = self->m_locals[i].get()->codeAddress();
            code_push_statement(code, &stmt, self->m_context);
            continue;
        }
        ext = i - 8;
        while (ext >= ir->m_extparams.size())
            ir->generateExtparam();

        ep = ir->m_extparams[ext];

        stmt.o1.u1 = ep->codeAddress();
        stmt.o2.u1 = self->m_locals[i].get()->codeAddress();
        code_push_statement(code, &stmt, self->m_context);
    }

    return true;
}

bool ir_builder::generateFunctionLocals(ir_value *global)
{
    prog_section_function_t *def;
    ir_function             *irfun;
    uint32_t                 firstlocal, firstglobal;

    irfun = global->m_constval.vfunc;
    def   = &m_code->functions[0] + irfun->m_code_function_def;

    if (OPTS_OPTION_BOOL(OPTION_G) ||
        !OPTS_OPTIMIZATION(OPTIM_OVERLAP_LOCALS)        ||
        (irfun->m_flags & IR_FLAG_MASK_NO_OVERLAP))
    {
        firstlocal = def->firstlocal = m_code->globals.size();
    } else {
        firstlocal = def->firstlocal = m_first_common_local;
        ++opts_optimizationcount[OPTIM_OVERLAP_LOCALS];
    }

    firstglobal = (OPTS_OPTIMIZATION(OPTIM_GLOBAL_TEMPS) ? m_first_common_globaltemp : firstlocal);

    for (size_t i = m_code->globals.size(); i < firstlocal + irfun->m_allocated_locals; ++i)
        m_code->globals.push_back(0);

    for (auto& lp : irfun->m_locals) {
        ir_value *v = lp.get();
        if (v->m_locked || !OPTS_OPTIMIZATION(OPTIM_GLOBAL_TEMPS)) {
            v->setCodeAddress(firstlocal + v->m_code.local);
            if (!generateGlobal(v, true)) {
                irerror(v->m_context, "failed to generate local %s", v->m_name.c_str());
                return false;
            }
        }
        else
            v->setCodeAddress(firstglobal + v->m_code.local);
    }
    for (auto& vp : irfun->m_values) {
        ir_value *v = vp.get();
        if (v->m_callparam)
            continue;
        if (v->m_locked)
            v->setCodeAddress(firstlocal + v->m_code.local);
        else
            v->setCodeAddress(firstglobal + v->m_code.local);
    }
    return true;
}

bool ir_builder::generateGlobalFunctionCode(ir_value *global)
{
    prog_section_function_t *fundef;
    ir_function             *irfun;

    irfun = global->m_constval.vfunc;
    if (!irfun) {
        if (global->m_cvq == CV_NONE) {
            if (irwarning(global->m_context, WARN_IMPLICIT_FUNCTION_POINTER,
                          "function `%s` has no body and in QC implicitly becomes a function-pointer",
                          global->m_name.c_str()))
            {
                /* Not bailing out just now. If this happens a lot you don't want to have
                 * to rerun gmqcc for each such function.
                 */

                /* return false; */
            }
        }
        /* this was a function pointer, don't generate code for those */
        return true;
    }

    if (irfun->m_builtin)
        return true;

    /*
     * If there is no definition and the thing is eraseable, we can ignore
     * outputting the function to begin with.
     */
    if (global->m_flags & IR_FLAG_ERASABLE && irfun->m_code_function_def < 0) {
        return true;
    }

    if (irfun->m_code_function_def < 0) {
        irerror(irfun->m_context, "`%s`: IR global wasn't generated, failed to access function-def", irfun->m_name.c_str());
        return false;
    }
    fundef = &m_code->functions[irfun->m_code_function_def];

    fundef->entry = m_code->statements.size();
    if (!generateFunctionLocals(global)) {
        irerror(irfun->m_context, "Failed to generate locals for function %s", irfun->m_name.c_str());
        return false;
    }
    if (!gen_function_extparam_copy(m_code.get(), irfun)) {
        irerror(irfun->m_context, "Failed to generate extparam-copy code for function %s", irfun->m_name.c_str());
        return false;
    }
    if (irfun->m_max_varargs && !gen_function_varargs_copy(m_code.get(), irfun)) {
        irerror(irfun->m_context, "Failed to generate vararg-copy code for function %s", irfun->m_name.c_str());
        return false;
    }
    if (!gen_function_code(m_code.get(), irfun)) {
        irerror(irfun->m_context, "Failed to generate code for function %s", irfun->m_name.c_str());
        return false;
    }
    return true;
}

static void gen_vector_defs(code_t *code, prog_section_def_t def, const char *name)
{
    char  *component;
    size_t len, i;

    if (!name || name[0] == '#' || OPTS_FLAG(SINGLE_VECTOR_DEFS))
        return;

    def.type = TYPE_FLOAT;

    len = strlen(name);

    component = (char*)mem_a(len+3);
    memcpy(component, name, len);
    len += 2;
    component[len-0] = 0;
    component[len-2] = '_';

    component[len-1] = 'x';

    for (i = 0; i < 3; ++i) {
        def.name = code_genstring(code, component);
        code->defs.push_back(def);
        def.offset++;
        component[len-1]++;
    }

    mem_d(component);
}

static void gen_vector_fields(code_t *code, prog_section_field_t fld, const char *name)
{
    char  *component;
    size_t len, i;

    if (!name || OPTS_FLAG(SINGLE_VECTOR_DEFS))
        return;

    fld.type = TYPE_FLOAT;

    len = strlen(name);

    component = (char*)mem_a(len+3);
    memcpy(component, name, len);
    len += 2;
    component[len-0] = 0;
    component[len-2] = '_';

    component[len-1] = 'x';

    for (i = 0; i < 3; ++i) {
        fld.name = code_genstring(code, component);
        code->fields.push_back(fld);
        fld.offset++;
        component[len-1]++;
    }

    mem_d(component);
}

bool ir_builder::generateGlobal(ir_value *global, bool islocal)
{
    size_t             i;
    int32_t           *iptr;
    prog_section_def_t def;
    bool               pushdef = opts.optimizeoff;

    /* we don't generate split-vectors */
    if (global->m_vtype == TYPE_VECTOR && (global->m_flags & IR_FLAG_SPLIT_VECTOR))
        return true;

    def.type = global->m_vtype;
    def.offset = m_code->globals.size();
    def.name = 0;
    if (OPTS_OPTION_BOOL(OPTION_G) || !islocal)
    {
        pushdef = true;

        /*
         * if we're eraseable and the function isn't referenced ignore outputting
         * the function.
         */
        if (global->m_flags & IR_FLAG_ERASABLE && global->m_reads.empty()) {
            return true;
        }

        if (OPTS_OPTIMIZATION(OPTIM_STRIP_CONSTANT_NAMES) &&
            !(global->m_flags & IR_FLAG_INCLUDE_DEF) &&
            (global->m_name[0] == '#' || global->m_cvq == CV_CONST))
        {
            pushdef = false;
        }

        if (pushdef) {
            if (global->m_name[0] == '#') {
                if (!m_str_immediate)
                    m_str_immediate = code_genstring(m_code.get(), "IMMEDIATE");
                def.name = global->m_code.name = m_str_immediate;
            }
            else
                def.name = global->m_code.name = code_genstring(m_code.get(), global->m_name.c_str());
        }
        else
            def.name   = 0;
        if (islocal) {
            def.offset = global->codeAddress();
            m_code->defs.push_back(def);
            if (global->m_vtype == TYPE_VECTOR)
                gen_vector_defs(m_code.get(), def, global->m_name.c_str());
            else if (global->m_vtype == TYPE_FIELD && global->m_fieldtype == TYPE_VECTOR)
                gen_vector_defs(m_code.get(), def, global->m_name.c_str());
            return true;
        }
    }
    if (islocal)
        return true;

    switch (global->m_vtype)
    {
    case TYPE_VOID:
        if (0 == global->m_name.compare("end_sys_globals")) {
            // TODO: remember this point... all the defs before this one
            // should be checksummed and added to progdefs.h when we generate it.
        }
        else if (0 == global->m_name.compare("end_sys_fields")) {
            // TODO: same as above but for entity-fields rather than globsl
        }
        else if(irwarning(global->m_context, WARN_VOID_VARIABLES, "unrecognized variable of type void `%s`",
                          global->m_name.c_str()))
        {
            /* Not bailing out */
            /* return false; */
        }
        /* I'd argue setting it to 0 is sufficient, but maybe some depend on knowing how far
         * the system fields actually go? Though the engine knows this anyway...
         * Maybe this could be an -foption
         * fteqcc creates data for end_sys_* - of size 1, so let's do the same
         */
        global->setCodeAddress(m_code->globals.size());
        m_code->globals.push_back(0);
        /* Add the def */
        if (pushdef)
            m_code->defs.push_back(def);
        return true;
    case TYPE_POINTER:
        if (pushdef)
            m_code->defs.push_back(def);
        return gen_global_pointer(m_code.get(), global);
    case TYPE_FIELD:
        if (pushdef) {
            m_code->defs.push_back(def);
            if (global->m_fieldtype == TYPE_VECTOR)
                gen_vector_defs(m_code.get(), def, global->m_name.c_str());
        }
        return gen_global_field(m_code.get(), global);
    case TYPE_ENTITY:
        /* fall through */
    case TYPE_FLOAT:
    {
        global->setCodeAddress(m_code->globals.size());
        if (global->m_hasvalue) {
            if (global->m_cvq == CV_CONST && global->m_reads.empty())
                return true;
            iptr = (int32_t*)&global->m_constval.ivec[0];
            m_code->globals.push_back(*iptr);
        } else {
            m_code->globals.push_back(0);
        }
        if (!islocal && global->m_cvq != CV_CONST)
            def.type |= DEF_SAVEGLOBAL;
        if (pushdef)
            m_code->defs.push_back(def);

        return global->m_code.globaladdr >= 0;
    }
    case TYPE_STRING:
    {
        global->setCodeAddress(m_code->globals.size());
        if (global->m_hasvalue) {
            if (global->m_cvq == CV_CONST && global->m_reads.empty())
                return true;
            uint32_t load = code_genstring(m_code.get(), global->m_constval.vstring);
            m_code->globals.push_back(load);
        } else {
            m_code->globals.push_back(0);
        }
        if (!islocal && global->m_cvq != CV_CONST)
            def.type |= DEF_SAVEGLOBAL;
        if (pushdef)
            m_code->defs.push_back(def);
        return global->m_code.globaladdr >= 0;
    }
    case TYPE_VECTOR:
    {
        size_t d;
        global->setCodeAddress(m_code->globals.size());
        if (global->m_hasvalue) {
            iptr = (int32_t*)&global->m_constval.ivec[0];
            m_code->globals.push_back(iptr[0]);
            if (global->m_code.globaladdr < 0)
                return false;
            for (d = 1; d < type_sizeof_[global->m_vtype]; ++d) {
                m_code->globals.push_back(iptr[d]);
            }
        } else {
            m_code->globals.push_back(0);
            if (global->m_code.globaladdr < 0)
                return false;
            for (d = 1; d < type_sizeof_[global->m_vtype]; ++d) {
                m_code->globals.push_back(0);
            }
        }
        if (!islocal && global->m_cvq != CV_CONST)
            def.type |= DEF_SAVEGLOBAL;

        if (pushdef) {
            m_code->defs.push_back(def);
            def.type &= ~DEF_SAVEGLOBAL;
            gen_vector_defs(m_code.get(), def, global->m_name.c_str());
        }
        return global->m_code.globaladdr >= 0;
    }
    case TYPE_FUNCTION:
        global->setCodeAddress(m_code->globals.size());
        if (!global->m_hasvalue) {
            m_code->globals.push_back(0);
            if (global->m_code.globaladdr < 0)
                return false;
        } else {
            m_code->globals.push_back(m_code->functions.size());
            if (!generateGlobalFunction(global))
                return false;
        }
        if (!islocal && global->m_cvq != CV_CONST)
            def.type |= DEF_SAVEGLOBAL;
        if (pushdef)
            m_code->defs.push_back(def);
        return true;
    case TYPE_VARIANT:
        /* assume biggest type */
            global->setCodeAddress(m_code->globals.size());
            m_code->globals.push_back(0);
            for (i = 1; i < type_sizeof_[TYPE_VARIANT]; ++i)
                m_code->globals.push_back(0);
            return true;
    default:
        /* refuse to create 'void' type or any other fancy business. */
        irerror(global->m_context, "Invalid type for global variable `%s`: %s",
                global->m_name.c_str(), type_name[global->m_vtype]);
        return false;
    }
}

static GMQCC_INLINE void ir_builder_prepare_field(code_t *code, ir_value *field)
{
    field->m_code.fieldaddr = code_alloc_field(code, type_sizeof_[field->m_fieldtype]);
}

static bool ir_builder_gen_field(ir_builder *self, ir_value *field)
{
    prog_section_def_t def;
    prog_section_field_t fld;

    (void)self;

    def.type   = (uint16_t)field->m_vtype;
    def.offset = (uint16_t)self->m_code->globals.size();

    /* create a global named the same as the field */
    if (OPTS_OPTION_U32(OPTION_STANDARD) == COMPILER_GMQCC) {
        /* in our standard, the global gets a dot prefix */
        size_t len = field->m_name.length();
        char name[1024];

        /* we really don't want to have to allocate this, and 1024
         * bytes is more than enough for a variable/field name
         */
        if (len+2 >= sizeof(name)) {
            irerror(field->m_context, "invalid field name size: %u", (unsigned int)len);
            return false;
        }

        name[0] = '.';
        memcpy(name+1, field->m_name.c_str(), len); // no strncpy - we used strlen above
        name[len+1] = 0;

        def.name = code_genstring(self->m_code.get(), name);
        fld.name = def.name + 1; /* we reuse that string table entry */
    } else {
        /* in plain QC, there cannot be a global with the same name,
         * and so we also name the global the same.
         * FIXME: fteqcc should create a global as well
         * check if it actually uses the same name. Probably does
         */
        def.name = code_genstring(self->m_code.get(), field->m_name.c_str());
        fld.name = def.name;
    }

    field->m_code.name = def.name;

    self->m_code->defs.push_back(def);

    fld.type = field->m_fieldtype;

    if (fld.type == TYPE_VOID) {
        irerror(field->m_context, "field is missing a type: %s - don't know its size", field->m_name.c_str());
        return false;
    }

    fld.offset = field->m_code.fieldaddr;

    self->m_code->fields.push_back(fld);

    field->setCodeAddress(self->m_code->globals.size());
    self->m_code->globals.push_back(fld.offset);
    if (fld.type == TYPE_VECTOR) {
        self->m_code->globals.push_back(fld.offset+1);
        self->m_code->globals.push_back(fld.offset+2);
    }

    if (field->m_fieldtype == TYPE_VECTOR) {
        gen_vector_defs  (self->m_code.get(), def, field->m_name.c_str());
        gen_vector_fields(self->m_code.get(), fld, field->m_name.c_str());
    }

    return field->m_code.globaladdr >= 0;
}

static void ir_builder_collect_reusables(ir_builder *builder) {
    std::vector<ir_value*> reusables;

    for (auto& gp : builder->m_globals) {
        ir_value *value = gp.get();
        if (value->m_vtype != TYPE_FLOAT || !value->m_hasvalue)
            continue;
        if (value->m_cvq == CV_CONST || (value->m_name.length() >= 1 && value->m_name[0] == '#'))
            reusables.emplace_back(value);
    }
    builder->m_const_floats = move(reusables);
}

static void ir_builder_split_vector(ir_builder *self, ir_value *vec) {
    ir_value* found[3] = { nullptr, nullptr, nullptr };

    // must not be written to
    if (vec->m_writes.size())
        return;
    // must not be trying to access individual members
    if (vec->m_members[0] || vec->m_members[1] || vec->m_members[2])
        return;
    // should be actually used otherwise it won't be generated anyway
    if (vec->m_reads.empty())
        return;
    //size_t count = vec->m_reads.size();
    //if (!count)
    //    return;

    // may only be used directly as function parameters, so if we find some other instruction cancel
    for (ir_instr *user : vec->m_reads) {
        // we only split vectors if they're used directly as parameter to a call only!
        if ((user->m_opcode < INSTR_CALL0 || user->m_opcode > INSTR_CALL8) && user->m_opcode != VINSTR_NRCALL)
            return;
    }

    vec->m_flags |= IR_FLAG_SPLIT_VECTOR;

    // find existing floats making up the split
    for (ir_value *c : self->m_const_floats) {
        if (!found[0] && c->m_constval.vfloat == vec->m_constval.vvec.x)
            found[0] = c;
        if (!found[1] && c->m_constval.vfloat == vec->m_constval.vvec.y)
            found[1] = c;
        if (!found[2] && c->m_constval.vfloat == vec->m_constval.vvec.z)
            found[2] = c;
        if (found[0] && found[1] && found[2])
            break;
    }

    // generate floats for not yet found components
    if (!found[0])
        found[0] = self->literalFloat(vec->m_constval.vvec.x, true);
    if (!found[1]) {
        if (vec->m_constval.vvec.y == vec->m_constval.vvec.x)
            found[1] = found[0];
        else
            found[1] = self->literalFloat(vec->m_constval.vvec.y, true);
    }
    if (!found[2]) {
        if (vec->m_constval.vvec.z == vec->m_constval.vvec.x)
            found[2] = found[0];
        else if (vec->m_constval.vvec.z == vec->m_constval.vvec.y)
            found[2] = found[1];
        else
            found[2] = self->literalFloat(vec->m_constval.vvec.z, true);
    }

    // the .members array should be safe to use here
    vec->m_members[0] = found[0];
    vec->m_members[1] = found[1];
    vec->m_members[2] = found[2];

    // register the readers for these floats
    found[0]->m_reads.insert(found[0]->m_reads.end(), vec->m_reads.begin(), vec->m_reads.end());
    found[1]->m_reads.insert(found[1]->m_reads.end(), vec->m_reads.begin(), vec->m_reads.end());
    found[2]->m_reads.insert(found[2]->m_reads.end(), vec->m_reads.begin(), vec->m_reads.end());
}

static void ir_builder_split_vectors(ir_builder *self) {
    // member values may be added to self->m_globals during this operation, but
    // no new vectors will be added, we need to iterate via an index as
    // c++ iterators would be invalidated
    const size_t count = self->m_globals.size();
    for (size_t i = 0; i != count; ++i) {
        ir_value *v = self->m_globals[i].get();
        if (v->m_vtype != TYPE_VECTOR || !v->m_name.length() || v->m_name[0] != '#')
            continue;
        ir_builder_split_vector(self, v);
    }
}

bool ir_builder::generate(const char *filename)
{
    prog_section_statement_t stmt;
    char  *lnofile = nullptr;

    if (OPTS_FLAG(SPLIT_VECTOR_PARAMETERS)) {
        ir_builder_collect_reusables(this);
        if (!m_const_floats.empty())
            ir_builder_split_vectors(this);
    }

    for (auto& fp : m_fields)
        ir_builder_prepare_field(m_code.get(), fp.get());

    for (auto& gp : m_globals) {
        ir_value *global = gp.get();
        if (!generateGlobal(global, false)) {
            return false;
        }
        if (global->m_vtype == TYPE_FUNCTION) {
            ir_function *func = global->m_constval.vfunc;
            if (func && m_max_locals < func->m_allocated_locals &&
                !(func->m_flags & IR_FLAG_MASK_NO_OVERLAP))
            {
                m_max_locals = func->m_allocated_locals;
            }
            if (func && m_max_globaltemps < func->m_globaltemps)
                m_max_globaltemps = func->m_globaltemps;
        }
    }

    for (auto& fp : m_fields) {
        if (!ir_builder_gen_field(this, fp.get()))
            return false;
    }

    // generate nil
    m_nil->setCodeAddress(m_code->globals.size());
    m_code->globals.push_back(0);
    m_code->globals.push_back(0);
    m_code->globals.push_back(0);

    // generate virtual-instruction temps
    for (size_t i = 0; i < IR_MAX_VINSTR_TEMPS; ++i) {
        m_vinstr_temp[i]->setCodeAddress(m_code->globals.size());
        m_code->globals.push_back(0);
        m_code->globals.push_back(0);
        m_code->globals.push_back(0);
    }

    // generate global temps
    m_first_common_globaltemp = m_code->globals.size();
    m_code->globals.insert(m_code->globals.end(), m_max_globaltemps, 0);
    // FIXME:DELME:
    //for (size_t i = 0; i < m_max_globaltemps; ++i) {
    //    m_code->globals.push_back(0);
    //}
    // generate common locals
    m_first_common_local = m_code->globals.size();
    m_code->globals.insert(m_code->globals.end(), m_max_locals, 0);
    // FIXME:DELME:
    //for (i = 0; i < m_max_locals; ++i) {
    //    m_code->globals.push_back(0);
    //}

    // generate function code

    for (auto& gp : m_globals) {
        ir_value *global = gp.get();
        if (global->m_vtype == TYPE_FUNCTION) {
            if (!this->generateGlobalFunctionCode(global))
                return false;
        }
    }

    if (m_code->globals.size() >= 65536) {
        irerror(m_globals.back()->m_context,
            "This progs file would require more globals than the metadata can handle (%zu). Bailing out.",
            m_code->globals.size());
        return false;
    }

    /* DP errors if the last instruction is not an INSTR_DONE. */
    if (m_code->statements.back().opcode != INSTR_DONE)
    {
        lex_ctx_t last;

        stmt.opcode = INSTR_DONE;
        stmt.o1.u1  = 0;
        stmt.o2.u1  = 0;
        stmt.o3.u1  = 0;
        last.line   = m_code->linenums.back();
        last.column = m_code->columnnums.back();

        code_push_statement(m_code.get(), &stmt, last);
    }

    if (OPTS_OPTION_BOOL(OPTION_PP_ONLY))
        return true;

    if (m_code->statements.size() != m_code->linenums.size()) {
        con_err("Linecounter wrong: %lu != %lu\n",
                m_code->statements.size(),
                m_code->linenums.size());
    } else if (OPTS_FLAG(LNO)) {
        char  *dot;
        size_t filelen = strlen(filename);

        memcpy(vec_add(lnofile, filelen+1), filename, filelen+1);
        dot = strrchr(lnofile, '.');
        if (!dot) {
            vec_pop(lnofile);
        } else {
            vec_shrinkto(lnofile, dot - lnofile);
        }
        memcpy(vec_add(lnofile, 5), ".lno", 5);
    }

    if (!code_write(m_code.get(), filename, lnofile)) {
        vec_free(lnofile);
        return false;
    }

    vec_free(lnofile);
    return true;
}

/***********************************************************************
 *IR DEBUG Dump functions...
 */

#define IND_BUFSZ 1024

static const char *qc_opname(int op)
{
    if (op < 0) return "<INVALID>";
    if (op < VINSTR_END)
        return util_instr_str[op];
    switch (op) {
        case VINSTR_END:       return "END";
        case VINSTR_PHI:       return "PHI";
        case VINSTR_JUMP:      return "JUMP";
        case VINSTR_COND:      return "COND";
        case VINSTR_BITXOR:    return "BITXOR";
        case VINSTR_BITAND_V:  return "BITAND_V";
        case VINSTR_BITOR_V:   return "BITOR_V";
        case VINSTR_BITXOR_V:  return "BITXOR_V";
        case VINSTR_BITAND_VF: return "BITAND_VF";
        case VINSTR_BITOR_VF:  return "BITOR_VF";
        case VINSTR_BITXOR_VF: return "BITXOR_VF";
        case VINSTR_CROSS:     return "CROSS";
        case VINSTR_NEG_F:     return "NEG_F";
        case VINSTR_NEG_V:     return "NEG_V";
        default:               return "<UNK>";
    }
}

void ir_builder::dump(int (*oprintf)(const char*, ...)) const
{
    size_t i;
    char indent[IND_BUFSZ];
    indent[0] = '\t';
    indent[1] = 0;

    oprintf("module %s\n", m_name.c_str());
    for (i = 0; i < m_globals.size(); ++i)
    {
        oprintf("global ");
        if (m_globals[i]->m_hasvalue)
            oprintf("%s = ", m_globals[i]->m_name.c_str());
        m_globals[i].get()->dump(oprintf);
        oprintf("\n");
    }
    for (i = 0; i < m_functions.size(); ++i)
        ir_function_dump(m_functions[i].get(), indent, oprintf);
    oprintf("endmodule %s\n", m_name.c_str());
}

static const char *storenames[] = {
    "[global]", "[local]", "[param]", "[value]", "[return]"
};

void ir_function_dump(ir_function *f, char *ind,
                      int (*oprintf)(const char*, ...))
{
    size_t i;
    if (f->m_builtin != 0) {
        oprintf("%sfunction %s = builtin %i\n", ind, f->m_name.c_str(), -f->m_builtin);
        return;
    }
    oprintf("%sfunction %s\n", ind, f->m_name.c_str());
    util_strncat(ind, "\t", IND_BUFSZ-1);
    if (f->m_locals.size())
    {
        oprintf("%s%i locals:\n", ind, (int)f->m_locals.size());
        for (i = 0; i < f->m_locals.size(); ++i) {
            oprintf("%s\t", ind);
            f->m_locals[i].get()->dump(oprintf);
            oprintf("\n");
        }
    }
    oprintf("%sliferanges:\n", ind);
    for (i = 0; i < f->m_locals.size(); ++i) {
        const char *attr = "";
        size_t l, m;
        ir_value *v = f->m_locals[i].get();
        if (v->m_unique_life && v->m_locked)
            attr = "unique,locked ";
        else if (v->m_unique_life)
            attr = "unique ";
        else if (v->m_locked)
            attr = "locked ";
        oprintf("%s\t%s: %s %s %s%s@%i ", ind, v->m_name.c_str(), type_name[v->m_vtype],
                storenames[v->m_store],
                attr, (v->m_callparam ? "callparam " : ""),
                (int)v->m_code.local);
        if (v->m_life.empty())
            oprintf("[null]");
        for (l = 0; l < v->m_life.size(); ++l) {
            oprintf("[%i,%i] ", v->m_life[l].start, v->m_life[l].end);
        }
        oprintf("\n");
        for (m = 0; m < 3; ++m) {
            ir_value *vm = v->m_members[m];
            if (!vm)
                continue;
            oprintf("%s\t%s: @%i ", ind, vm->m_name.c_str(), (int)vm->m_code.local);
            for (l = 0; l < vm->m_life.size(); ++l) {
                oprintf("[%i,%i] ", vm->m_life[l].start, vm->m_life[l].end);
            }
            oprintf("\n");
        }
    }
    for (i = 0; i < f->m_values.size(); ++i) {
        const char *attr = "";
        size_t l, m;
        ir_value *v = f->m_values[i].get();
        if (v->m_unique_life && v->m_locked)
            attr = "unique,locked ";
        else if (v->m_unique_life)
            attr = "unique ";
        else if (v->m_locked)
            attr = "locked ";
        oprintf("%s\t%s: %s %s %s%s@%i ", ind, v->m_name.c_str(), type_name[v->m_vtype],
                storenames[v->m_store],
                attr, (v->m_callparam ? "callparam " : ""),
                (int)v->m_code.local);
        if (v->m_life.empty())
            oprintf("[null]");
        for (l = 0; l < v->m_life.size(); ++l) {
            oprintf("[%i,%i] ", v->m_life[l].start, v->m_life[l].end);
        }
        oprintf("\n");
        for (m = 0; m < 3; ++m) {
            ir_value *vm = v->m_members[m];
            if (!vm)
                continue;
            if (vm->m_unique_life && vm->m_locked)
                attr = "unique,locked ";
            else if (vm->m_unique_life)
                attr = "unique ";
            else if (vm->m_locked)
                attr = "locked ";
            oprintf("%s\t%s: %s@%i ", ind, vm->m_name.c_str(), attr, (int)vm->m_code.local);
            for (l = 0; l < vm->m_life.size(); ++l) {
                oprintf("[%i,%i] ", vm->m_life[l].start, vm->m_life[l].end);
            }
            oprintf("\n");
        }
    }
    if (f->m_blocks.size())
    {
        oprintf("%slife passes: %i\n", ind, (int)f->m_run_id);
        for (i = 0; i < f->m_blocks.size(); ++i) {
            ir_block_dump(f->m_blocks[i].get(), ind, oprintf);
        }

    }
    ind[strlen(ind)-1] = 0;
    oprintf("%sendfunction %s\n", ind, f->m_name.c_str());
}

void ir_block_dump(ir_block* b, char *ind,
                   int (*oprintf)(const char*, ...))
{
    oprintf("%s:%s\n", ind, b->m_label.c_str());
    util_strncat(ind, "\t", IND_BUFSZ-1);

    if (!b->m_instr.empty() && b->m_instr[0])
        oprintf("%s (%i) [entry]\n", ind, (int)(b->m_instr[0]->m_eid-1));
    for (auto &i : b->m_instr)
        ir_instr_dump(i, ind, oprintf);
    ind[strlen(ind)-1] = 0;
}

static void dump_phi(ir_instr *in, int (*oprintf)(const char*, ...))
{
    oprintf("%s <- phi ", in->_m_ops[0]->m_name.c_str());
    for (auto &it : in->m_phi) {
        oprintf("([%s] : %s) ", it.from->m_label.c_str(),
                                it.value->m_name.c_str());
    }
    oprintf("\n");
}

void ir_instr_dump(ir_instr *in, char *ind,
                       int (*oprintf)(const char*, ...))
{
    size_t i;
    const char *comma = nullptr;

    oprintf("%s (%i) ", ind, (int)in->m_eid);

    if (in->m_opcode == VINSTR_PHI) {
        dump_phi(in, oprintf);
        return;
    }

    util_strncat(ind, "\t", IND_BUFSZ-1);

    if (in->_m_ops[0] && (in->_m_ops[1] || in->_m_ops[2])) {
        in->_m_ops[0]->dump(oprintf);
        if (in->_m_ops[1] || in->_m_ops[2])
            oprintf(" <- ");
    }
    if (in->m_opcode == INSTR_CALL0 || in->m_opcode == VINSTR_NRCALL) {
        oprintf("CALL%i\t", in->m_params.size());
    } else
        oprintf("%s\t", qc_opname(in->m_opcode));

    if (in->_m_ops[0] && !(in->_m_ops[1] || in->_m_ops[2])) {
        in->_m_ops[0]->dump(oprintf);
        comma = ",\t";
    }
    else
    {
        for (i = 1; i != 3; ++i) {
            if (in->_m_ops[i]) {
                if (comma)
                    oprintf(comma);
                in->_m_ops[i]->dump(oprintf);
                comma = ",\t";
            }
        }
    }
    if (in->m_bops[0]) {
        if (comma)
            oprintf(comma);
        oprintf("[%s]", in->m_bops[0]->m_label.c_str());
        comma = ",\t";
    }
    if (in->m_bops[1])
        oprintf("%s[%s]", comma, in->m_bops[1]->m_label.c_str());
    if (in->m_params.size()) {
        oprintf("\tparams: ");
        for (auto &it : in->m_params)
            oprintf("%s, ", it->m_name.c_str());
    }
    oprintf("\n");
    ind[strlen(ind)-1] = 0;
}

static void ir_value_dump_string(const char *str, int (*oprintf)(const char*, ...))
{
    oprintf("\"");
    for (; *str; ++str) {
        switch (*str) {
            case '\n': oprintf("\\n"); break;
            case '\r': oprintf("\\r"); break;
            case '\t': oprintf("\\t"); break;
            case '\v': oprintf("\\v"); break;
            case '\f': oprintf("\\f"); break;
            case '\b': oprintf("\\b"); break;
            case '\a': oprintf("\\a"); break;
            case '\\': oprintf("\\\\"); break;
            case '"': oprintf("\\\""); break;
            default: oprintf("%c", *str); break;
        }
    }
    oprintf("\"");
}

void ir_value::dump(int (*oprintf)(const char*, ...)) const
{
    if (m_hasvalue) {
        switch (m_vtype) {
            default:
            case TYPE_VOID:
                oprintf("(void)");
                break;
            case TYPE_FUNCTION:
                oprintf("fn:%s", m_name.c_str());
                break;
            case TYPE_FLOAT:
                oprintf("%g", m_constval.vfloat);
                break;
            case TYPE_VECTOR:
                oprintf("'%g %g %g'",
                        m_constval.vvec.x,
                        m_constval.vvec.y,
                        m_constval.vvec.z);
                break;
            case TYPE_ENTITY:
                oprintf("(entity)");
                break;
            case TYPE_STRING:
                ir_value_dump_string(m_constval.vstring, oprintf);
                break;
#if 0
            case TYPE_INTEGER:
                oprintf("%i", m_constval.vint);
                break;
#endif
            case TYPE_POINTER:
                oprintf("&%s",
                    m_constval.vpointer->m_name.c_str());
                break;
        }
    } else {
        oprintf("%s", m_name.c_str());
    }
}

void ir_value::dumpLife(int (*oprintf)(const char*,...)) const
{
    oprintf("Life of %12s:", m_name.c_str());
    for (size_t i = 0; i < m_life.size(); ++i)
    {
        oprintf(" + [%i, %i]\n", m_life[i].start, m_life[i].end);
    }
}
