/*
 * Copyright (C) 2012
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
    "array"
};

size_t type_sizeof[TYPE_COUNT] = {
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
};

uint16_t type_store_instr[TYPE_COUNT] = {
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

    AINSTR_END, /* struct */
    AINSTR_END, /* union  */
    AINSTR_END, /* array  */
};

uint16_t field_store_instr[TYPE_COUNT] = {
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

    AINSTR_END, /* struct */
    AINSTR_END, /* union  */
    AINSTR_END, /* array  */
};

uint16_t type_storep_instr[TYPE_COUNT] = {
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

    AINSTR_END, /* struct */
    AINSTR_END, /* union  */
    AINSTR_END, /* array  */
};

uint16_t type_eq_instr[TYPE_COUNT] = {
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

    AINSTR_END, /* struct */
    AINSTR_END, /* union  */
    AINSTR_END, /* array  */
};

uint16_t type_ne_instr[TYPE_COUNT] = {
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

    AINSTR_END, /* struct */
    AINSTR_END, /* union  */
    AINSTR_END, /* array  */
};

uint16_t type_not_instr[TYPE_COUNT] = {
    INSTR_NOT_F, /* should use I when having integer support */
    INSTR_NOT_S,
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

    AINSTR_END, /* struct */
    AINSTR_END, /* union  */
    AINSTR_END, /* array  */
};

/* protos */
static void ir_gen_extparam(ir_builder *ir);

/* error functions */

static void irerror(lex_ctx ctx, const char *msg, ...)
{
    va_list ap;
    va_start(ap, msg);
    con_cvprintmsg((void*)&ctx, LVL_ERROR, "internal error", msg, ap);
    va_end(ap);
}

static bool irwarning(lex_ctx ctx, int warntype, const char *fmt, ...)
{
	va_list ap;
	int lvl = LVL_WARNING;

    if (warntype && !OPTS_WARN(warntype))
        return false;

    if (opts_werror)
	    lvl = LVL_ERROR;

	va_start(ap, fmt);
    con_vprintmsg(lvl, ctx.file, ctx.line, (opts_werror ? "error" : "warning"), fmt, ap);
	va_end(ap);

	return opts_werror;
}

/***********************************************************************
 * Vector utility functions
 */

bool GMQCC_WARN vec_ir_value_find(ir_value **vec, ir_value *what, size_t *idx)
{
    size_t i;
    size_t len = vec_size(vec);
    for (i = 0; i < len; ++i) {
        if (vec[i] == what) {
            if (idx) *idx = i;
            return true;
        }
    }
    return false;
}

bool GMQCC_WARN vec_ir_block_find(ir_block **vec, ir_block *what, size_t *idx)
{
    size_t i;
    size_t len = vec_size(vec);
    for (i = 0; i < len; ++i) {
        if (vec[i] == what) {
            if (idx) *idx = i;
            return true;
        }
    }
    return false;
}

bool GMQCC_WARN vec_ir_instr_find(ir_instr **vec, ir_instr *what, size_t *idx)
{
    size_t i;
    size_t len = vec_size(vec);
    for (i = 0; i < len; ++i) {
        if (vec[i] == what) {
            if (idx) *idx = i;
            return true;
        }
    }
    return false;
}

/***********************************************************************
 * IR Builder
 */

static void ir_block_delete_quick(ir_block* self);
static void ir_instr_delete_quick(ir_instr *self);
static void ir_function_delete_quick(ir_function *self);

ir_builder* ir_builder_new(const char *modulename)
{
    ir_builder* self;

    self = (ir_builder*)mem_a(sizeof(*self));
    if (!self)
        return NULL;

    self->functions   = NULL;
    self->globals     = NULL;
    self->fields      = NULL;
    self->extparams   = NULL;
    self->filenames   = NULL;
    self->filestrings = NULL;
    self->htglobals   = util_htnew(IR_HT_SIZE);
    self->htfields    = util_htnew(IR_HT_SIZE);
    self->htfunctions = util_htnew(IR_HT_SIZE);

    self->str_immediate = 0;
    self->name = NULL;
    if (!ir_builder_set_name(self, modulename)) {
        mem_d(self);
        return NULL;
    }

    return self;
}

void ir_builder_delete(ir_builder* self)
{
    size_t i;
    util_htdel(self->htglobals);
    util_htdel(self->htfields);
    util_htdel(self->htfunctions);
    mem_d((void*)self->name);
    for (i = 0; i != vec_size(self->functions); ++i) {
        ir_function_delete_quick(self->functions[i]);
    }
    vec_free(self->functions);
    for (i = 0; i != vec_size(self->extparams); ++i) {
        ir_value_delete(self->extparams[i]);
    }
    vec_free(self->extparams);
    for (i = 0; i != vec_size(self->globals); ++i) {
        ir_value_delete(self->globals[i]);
    }
    vec_free(self->globals);
    for (i = 0; i != vec_size(self->fields); ++i) {
        ir_value_delete(self->fields[i]);
    }
    vec_free(self->fields);
    vec_free(self->filenames);
    vec_free(self->filestrings);
    mem_d(self);
}

bool ir_builder_set_name(ir_builder *self, const char *name)
{
    if (self->name)
        mem_d((void*)self->name);
    self->name = util_strdup(name);
    return !!self->name;
}

ir_function* ir_builder_get_function(ir_builder *self, const char *name)
{
    return (ir_function*)util_htget(self->htfunctions, name);
}

ir_function* ir_builder_create_function(ir_builder *self, const char *name, int outtype)
{
    ir_function *fn = ir_builder_get_function(self, name);
    if (fn) {
        return NULL;
    }

    fn = ir_function_new(self, outtype);
    if (!ir_function_set_name(fn, name))
    {
        ir_function_delete(fn);
        return NULL;
    }
    vec_push(self->functions, fn);
    util_htset(self->htfunctions, name, fn);

    fn->value = ir_builder_create_global(self, fn->name, TYPE_FUNCTION);
    if (!fn->value) {
        ir_function_delete(fn);
        return NULL;
    }

    fn->value->hasvalue = true;
    fn->value->outtype = outtype;
    fn->value->constval.vfunc = fn;
    fn->value->context = fn->context;

    return fn;
}

ir_value* ir_builder_get_global(ir_builder *self, const char *name)
{
    return (ir_value*)util_htget(self->htglobals, name);
}

ir_value* ir_builder_create_global(ir_builder *self, const char *name, int vtype)
{
    ir_value *ve;

    if (name && name[0] != '#')
    {
        ve = ir_builder_get_global(self, name);
        if (ve) {
            return NULL;
        }
    }

    ve = ir_value_var(name, store_global, vtype);
    vec_push(self->globals, ve);
    util_htset(self->htglobals, name, ve);
    return ve;
}

ir_value* ir_builder_get_field(ir_builder *self, const char *name)
{
    return (ir_value*)util_htget(self->htfields, name);
}


ir_value* ir_builder_create_field(ir_builder *self, const char *name, int vtype)
{
    ir_value *ve = ir_builder_get_field(self, name);
    if (ve) {
        return NULL;
    }

    ve = ir_value_var(name, store_global, TYPE_FIELD);
    ve->fieldtype = vtype;
    vec_push(self->fields, ve);
    util_htset(self->htfields, name, ve);
    return ve;
}

/***********************************************************************
 *IR Function
 */

bool ir_function_naive_phi(ir_function*);
void ir_function_enumerate(ir_function*);
bool ir_function_calculate_liferanges(ir_function*);
bool ir_function_allocate_locals(ir_function*);

ir_function* ir_function_new(ir_builder* owner, int outtype)
{
    ir_function *self;
    self = (ir_function*)mem_a(sizeof(*self));

    if (!self)
        return NULL;

    memset(self, 0, sizeof(*self));

    self->name = NULL;
    if (!ir_function_set_name(self, "<@unnamed>")) {
        mem_d(self);
        return NULL;
    }
    self->owner = owner;
    self->context.file = "<@no context>";
    self->context.line = 0;
    self->outtype = outtype;
    self->value = NULL;
    self->builtin = 0;

    self->params = NULL;
    self->blocks = NULL;
    self->values = NULL;
    self->locals = NULL;

    self->code_function_def = -1;
    self->allocated_locals = 0;

    self->run_id = 0;
    return self;
}

bool ir_function_set_name(ir_function *self, const char *name)
{
    if (self->name)
        mem_d((void*)self->name);
    self->name = util_strdup(name);
    return !!self->name;
}

static void ir_function_delete_quick(ir_function *self)
{
    size_t i;
    mem_d((void*)self->name);

    for (i = 0; i != vec_size(self->blocks); ++i)
        ir_block_delete_quick(self->blocks[i]);
    vec_free(self->blocks);

    vec_free(self->params);

    for (i = 0; i != vec_size(self->values); ++i)
        ir_value_delete(self->values[i]);
    vec_free(self->values);

    for (i = 0; i != vec_size(self->locals); ++i)
        ir_value_delete(self->locals[i]);
    vec_free(self->locals);

    /* self->value is deleted by the builder */

    mem_d(self);
}

void ir_function_delete(ir_function *self)
{
    size_t i;
    mem_d((void*)self->name);

    for (i = 0; i != vec_size(self->blocks); ++i)
        ir_block_delete(self->blocks[i]);
    vec_free(self->blocks);

    vec_free(self->params);

    for (i = 0; i != vec_size(self->values); ++i)
        ir_value_delete(self->values[i]);
    vec_free(self->values);

    for (i = 0; i != vec_size(self->locals); ++i)
        ir_value_delete(self->locals[i]);
    vec_free(self->locals);

    /* self->value is deleted by the builder */

    mem_d(self);
}

void ir_function_collect_value(ir_function *self, ir_value *v)
{
    vec_push(self->values, v);
}

ir_block* ir_function_create_block(lex_ctx ctx, ir_function *self, const char *label)
{
    ir_block* bn = ir_block_new(self, label);
    bn->context = ctx;
    vec_push(self->blocks, bn);
    return bn;
}

bool ir_function_pass_tailcall(ir_function *self)
{
    size_t b, p;

    for (b = 0; b < vec_size(self->blocks); ++b) {
        ir_value *funcval;
        ir_instr *ret, *call, *store = NULL;
        ir_block *block = self->blocks[b];

        if (!block->final || vec_size(block->instr) < 2)
            continue;

        ret = block->instr[vec_size(block->instr)-1];
        if (ret->opcode != INSTR_DONE && ret->opcode != INSTR_RETURN)
            continue;

        call = block->instr[vec_size(block->instr)-2];
        if (call->opcode >= INSTR_STORE_F && call->opcode <= INSTR_STORE_FNC) {
            /* account for the unoptimized
             * CALL
             * STORE %return, %tmp
             * RETURN %tmp
             * version
             */
            if (vec_size(block->instr) < 3)
                continue;

            store = call;
            call = block->instr[vec_size(block->instr)-3];
        }

        if (call->opcode < INSTR_CALL0 || call->opcode > INSTR_CALL8)
            continue;

        if (store) {
            /* optimize out the STORE */
            if (ret->_ops[0]   &&
                ret->_ops[0]   == store->_ops[0] &&
                store->_ops[1] == call->_ops[0])
            {
                ++optimization_count[OPTIM_MINOR];
                call->_ops[0] = store->_ops[0];
                vec_remove(block->instr, vec_size(block->instr) - 2, 1);
                ir_instr_delete(store);
            }
            else
                continue;
        }

        if (!call->_ops[0])
            continue;

        funcval = call->_ops[1];
        if (!funcval)
            continue;
        if (funcval->vtype != TYPE_FUNCTION || funcval->constval.vfunc != self)
            continue;

        /* now we have a CALL and a RET, check if it's a tailcall */
        if (ret->_ops[0] && call->_ops[0] != ret->_ops[0])
            continue;

        ++optimization_count[OPTIM_TAIL_RECURSION];
        vec_shrinkby(block->instr, 2);

        block->final = false; /* open it back up */

        /* emite parameter-stores */
        for (p = 0; p < vec_size(call->params); ++p) {
            /* assert(call->params_count <= self->locals_count); */
            if (!ir_block_create_store(block, call->context, self->locals[p], call->params[p])) {
                irerror(call->context, "failed to create tailcall store instruction for parameter %i", (int)p);
                return false;
            }
        }
        if (!ir_block_create_jump(block, call->context, self->blocks[0])) {
            irerror(call->context, "failed to create tailcall jump");
            return false;
        }

        ir_instr_delete(call);
        ir_instr_delete(ret);
    }

    return true;
}

bool ir_function_finalize(ir_function *self)
{
    if (self->builtin)
        return true;

    if (OPTS_OPTIMIZATION(OPTIM_TAIL_RECURSION)) {
        if (!ir_function_pass_tailcall(self)) {
            irerror(self->context, "tailcall optimization pass broke something in `%s`", self->name);
            return false;
        }
    }

    if (!ir_function_naive_phi(self))
        return false;

    ir_function_enumerate(self);

    if (!ir_function_calculate_liferanges(self))
        return false;
    if (!ir_function_allocate_locals(self))
        return false;
    return true;
}

ir_value* ir_function_create_local(ir_function *self, const char *name, int vtype, bool param)
{
    ir_value *ve;

    if (param &&
        vec_size(self->locals) &&
        self->locals[vec_size(self->locals)-1]->store != store_param) {
        irerror(self->context, "cannot add parameters after adding locals");
        return NULL;
    }

    ve = ir_value_var(name, (param ? store_param : store_local), vtype);
    vec_push(self->locals, ve);
    return ve;
}

/***********************************************************************
 *IR Block
 */

ir_block* ir_block_new(ir_function* owner, const char *name)
{
    ir_block *self;
    self = (ir_block*)mem_a(sizeof(*self));
    if (!self)
        return NULL;

    memset(self, 0, sizeof(*self));

    self->label = NULL;
    if (name && !ir_block_set_label(self, name)) {
        mem_d(self);
        return NULL;
    }
    self->owner = owner;
    self->context.file = "<@no context>";
    self->context.line = 0;
    self->final = false;

    self->instr   = NULL;
    self->entries = NULL;
    self->exits   = NULL;

    self->eid = 0;
    self->is_return = false;
    self->run_id = 0;

    self->living = NULL;

    self->generated = false;

    return self;
}

static void ir_block_delete_quick(ir_block* self)
{
    size_t i;
    if (self->label) mem_d(self->label);
    for (i = 0; i != vec_size(self->instr); ++i)
        ir_instr_delete_quick(self->instr[i]);
    vec_free(self->instr);
    vec_free(self->entries);
    vec_free(self->exits);
    vec_free(self->living);
    mem_d(self);
}

void ir_block_delete(ir_block* self)
{
    size_t i;
    if (self->label) mem_d(self->label);
    for (i = 0; i != vec_size(self->instr); ++i)
        ir_instr_delete(self->instr[i]);
    vec_free(self->instr);
    vec_free(self->entries);
    vec_free(self->exits);
    vec_free(self->living);
    mem_d(self);
}

bool ir_block_set_label(ir_block *self, const char *name)
{
    if (self->label)
        mem_d((void*)self->label);
    self->label = util_strdup(name);
    return !!self->label;
}

/***********************************************************************
 *IR Instructions
 */

ir_instr* ir_instr_new(lex_ctx ctx, ir_block* owner, int op)
{
    ir_instr *self;
    self = (ir_instr*)mem_a(sizeof(*self));
    if (!self)
        return NULL;

    self->owner = owner;
    self->context = ctx;
    self->opcode = op;
    self->_ops[0] = NULL;
    self->_ops[1] = NULL;
    self->_ops[2] = NULL;
    self->bops[0] = NULL;
    self->bops[1] = NULL;

    self->phi    = NULL;
    self->params = NULL;

    self->eid = 0;

    self->likely = true;
    return self;
}

static void ir_instr_delete_quick(ir_instr *self)
{
    vec_free(self->phi);
    vec_free(self->params);
    mem_d(self);
}

void ir_instr_delete(ir_instr *self)
{
    size_t i;
    /* The following calls can only delete from
     * vectors, we still want to delete this instruction
     * so ignore the return value. Since with the warn_unused_result attribute
     * gcc doesn't care about an explicit: (void)foo(); to ignore the result,
     * I have to improvise here and use if(foo());
     */
    for (i = 0; i < vec_size(self->phi); ++i) {
        size_t idx;
        if (vec_ir_instr_find(self->phi[i].value->writes, self, &idx))
            vec_remove(self->phi[i].value->writes, idx, 1);
        if (vec_ir_instr_find(self->phi[i].value->reads, self, &idx))
            vec_remove(self->phi[i].value->reads, idx, 1);
    }
    vec_free(self->phi);
    for (i = 0; i < vec_size(self->params); ++i) {
        size_t idx;
        if (vec_ir_instr_find(self->params[i]->writes, self, &idx))
            vec_remove(self->params[i]->writes, idx, 1);
        if (vec_ir_instr_find(self->params[i]->reads, self, &idx))
            vec_remove(self->params[i]->reads, idx, 1);
    }
    vec_free(self->params);
    (void)!ir_instr_op(self, 0, NULL, false);
    (void)!ir_instr_op(self, 1, NULL, false);
    (void)!ir_instr_op(self, 2, NULL, false);
    mem_d(self);
}

bool ir_instr_op(ir_instr *self, int op, ir_value *v, bool writing)
{
    if (self->_ops[op]) {
        size_t idx;
        if (writing && vec_ir_instr_find(self->_ops[op]->writes, self, &idx))
            vec_remove(self->_ops[op]->writes, idx, 1);
        else if (vec_ir_instr_find(self->_ops[op]->reads, self, &idx))
            vec_remove(self->_ops[op]->reads, idx, 1);
    }
    if (v) {
        if (writing)
            vec_push(v->writes, self);
        else
            vec_push(v->reads, self);
    }
    self->_ops[op] = v;
    return true;
}

/***********************************************************************
 *IR Value
 */

void ir_value_code_setaddr(ir_value *self, int32_t gaddr)
{
    self->code.globaladdr = gaddr;
    if (self->members[0]) self->members[0]->code.globaladdr = gaddr;
    if (self->members[1]) self->members[1]->code.globaladdr = gaddr;
    if (self->members[2]) self->members[2]->code.globaladdr = gaddr;
}

int32_t ir_value_code_addr(const ir_value *self)
{
    if (self->store == store_return)
        return OFS_RETURN + self->code.addroffset;
    return self->code.globaladdr + self->code.addroffset;
}

ir_value* ir_value_var(const char *name, int storetype, int vtype)
{
    ir_value *self;
    self = (ir_value*)mem_a(sizeof(*self));
    self->vtype = vtype;
    self->fieldtype = TYPE_VOID;
    self->outtype = TYPE_VOID;
    self->store = storetype;

    self->reads  = NULL;
    self->writes = NULL;

    self->cvq          = CV_NONE;
    self->hasvalue     = false;
    self->context.file = "<@no context>";
    self->context.line = 0;
    self->name = NULL;
    if (name && !ir_value_set_name(self, name)) {
        irerror(self->context, "out of memory");
        mem_d(self);
        return NULL;
    }

    memset(&self->constval, 0, sizeof(self->constval));
    memset(&self->code,     0, sizeof(self->code));

    self->members[0] = NULL;
    self->members[1] = NULL;
    self->members[2] = NULL;
    self->memberof = NULL;

    self->life = NULL;
    return self;
}

ir_value* ir_value_vector_member(ir_value *self, unsigned int member)
{
    ir_value *m;
    if (member >= 3)
        return NULL;

    if (self->members[member])
        return self->members[member];

    if (self->vtype == TYPE_VECTOR)
    {
        m = ir_value_var(self->name, self->store, TYPE_FLOAT);
        if (!m)
            return NULL;
        m->context = self->context;

        self->members[member] = m;
        m->code.addroffset = member;
    }
    else if (self->vtype == TYPE_FIELD)
    {
        if (self->fieldtype != TYPE_VECTOR)
            return NULL;
        m = ir_value_var(self->name, self->store, TYPE_FIELD);
        if (!m)
            return NULL;
        m->fieldtype = TYPE_FLOAT;
        m->context = self->context;

        self->members[member] = m;
        m->code.addroffset = member;
    }
    else
    {
        irerror(self->context, "invalid member access on %s", self->name);
        return NULL;
    }

    m->memberof = self;
    return m;
}

ir_value* ir_value_out(ir_function *owner, const char *name, int storetype, int vtype)
{
    ir_value *v = ir_value_var(name, storetype, vtype);
    if (!v)
        return NULL;
    ir_function_collect_value(owner, v);
    return v;
}

void ir_value_delete(ir_value* self)
{
    size_t i;
    if (self->name)
        mem_d((void*)self->name);
    if (self->hasvalue)
    {
        if (self->vtype == TYPE_STRING)
            mem_d((void*)self->constval.vstring);
    }
    for (i = 0; i < 3; ++i) {
        if (self->members[i])
            ir_value_delete(self->members[i]);
    }
    vec_free(self->reads);
    vec_free(self->writes);
    vec_free(self->life);
    mem_d(self);
}

bool ir_value_set_name(ir_value *self, const char *name)
{
    if (self->name)
        mem_d((void*)self->name);
    self->name = util_strdup(name);
    return !!self->name;
}

bool ir_value_set_float(ir_value *self, float f)
{
    if (self->vtype != TYPE_FLOAT)
        return false;
    self->constval.vfloat = f;
    self->hasvalue = true;
    return true;
}

bool ir_value_set_func(ir_value *self, int f)
{
    if (self->vtype != TYPE_FUNCTION)
        return false;
    self->constval.vint = f;
    self->hasvalue = true;
    return true;
}

bool ir_value_set_vector(ir_value *self, vector v)
{
    if (self->vtype != TYPE_VECTOR)
        return false;
    self->constval.vvec = v;
    self->hasvalue = true;
    return true;
}

bool ir_value_set_field(ir_value *self, ir_value *fld)
{
    if (self->vtype != TYPE_FIELD)
        return false;
    self->constval.vpointer = fld;
    self->hasvalue = true;
    return true;
}

static char *ir_strdup(const char *str)
{
    if (str && !*str) {
        /* actually dup empty strings */
        char *out = mem_a(1);
        *out = 0;
        return out;
    }
    return util_strdup(str);
}

bool ir_value_set_string(ir_value *self, const char *str)
{
    if (self->vtype != TYPE_STRING)
        return false;
    self->constval.vstring = ir_strdup(str);
    self->hasvalue = true;
    return true;
}

#if 0
bool ir_value_set_int(ir_value *self, int i)
{
    if (self->vtype != TYPE_INTEGER)
        return false;
    self->constval.vint = i;
    self->hasvalue = true;
    return true;
}
#endif

bool ir_value_lives(ir_value *self, size_t at)
{
    size_t i;
    for (i = 0; i < vec_size(self->life); ++i)
    {
        ir_life_entry_t *life = &self->life[i];
        if (life->start <= at && at <= life->end)
            return true;
        if (life->start > at) /* since it's ordered */
            return false;
    }
    return false;
}

bool ir_value_life_insert(ir_value *self, size_t idx, ir_life_entry_t e)
{
    size_t k;
    vec_push(self->life, e);
    for (k = vec_size(self->life)-1; k > idx; --k)
        self->life[k] = self->life[k-1];
    self->life[idx] = e;
    return true;
}

bool ir_value_life_merge(ir_value *self, size_t s)
{
    size_t i;
    ir_life_entry_t *life = NULL;
    ir_life_entry_t *before = NULL;
    ir_life_entry_t new_entry;

    /* Find the first range >= s */
    for (i = 0; i < vec_size(self->life); ++i)
    {
        before = life;
        life = &self->life[i];
        if (life->start > s)
            break;
    }
    /* nothing found? append */
    if (i == vec_size(self->life)) {
        ir_life_entry_t e;
        if (life && life->end+1 == s)
        {
            /* previous life range can be merged in */
            life->end++;
            return true;
        }
        if (life && life->end >= s)
            return false;
        e.start = e.end = s;
        vec_push(self->life, e);
        return true;
    }
    /* found */
    if (before)
    {
        if (before->end + 1 == s &&
            life->start - 1 == s)
        {
            /* merge */
            before->end = life->end;
            vec_remove(self->life, i, 1);
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
    if (life->start - 1 == s)
    {
        life->start--;
        return true;
    }
    /* insert a new entry */
    new_entry.start = new_entry.end = s;
    return ir_value_life_insert(self, i, new_entry);
}

bool ir_value_life_merge_into(ir_value *self, const ir_value *other)
{
    size_t i, myi;

    if (!vec_size(other->life))
        return true;

    if (!vec_size(self->life)) {
        size_t count = vec_size(other->life);
        ir_life_entry_t *life = vec_add(self->life, count);
        memcpy(life, other->life, count * sizeof(*life));
        return true;
    }

    myi = 0;
    for (i = 0; i < vec_size(other->life); ++i)
    {
        const ir_life_entry_t *life = &other->life[i];
        while (true)
        {
            ir_life_entry_t *entry = &self->life[myi];

            if (life->end+1 < entry->start)
            {
                /* adding an interval before entry */
                if (!ir_value_life_insert(self, myi, *life))
                    return false;
                ++myi;
                break;
            }

            if (life->start <  entry->start &&
                life->end+1 >= entry->start)
            {
                /* starts earlier and overlaps */
                entry->start = life->start;
            }

            if (life->end   >  entry->end &&
                life->start <= entry->end+1)
            {
                /* ends later and overlaps */
                entry->end = life->end;
            }

            /* see if our change combines it with the next ranges */
            while (myi+1 < vec_size(self->life) &&
                   entry->end+1 >= self->life[1+myi].start)
            {
                /* overlaps with (myi+1) */
                if (entry->end < self->life[1+myi].end)
                    entry->end = self->life[1+myi].end;
                vec_remove(self->life, myi+1, 1);
                entry = &self->life[myi];
            }

            /* see if we're after the entry */
            if (life->start > entry->end)
            {
                ++myi;
                /* append if we're at the end */
                if (myi >= vec_size(self->life)) {
                    vec_push(self->life, *life);
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

bool ir_values_overlap(const ir_value *a, const ir_value *b)
{
    /* For any life entry in A see if it overlaps with
     * any life entry in B.
     * Note that the life entries are orderes, so we can make a
     * more efficient algorithm there than naively translating the
     * statement above.
     */

    ir_life_entry_t *la, *lb, *enda, *endb;

    /* first of all, if either has no life range, they cannot clash */
    if (!vec_size(a->life) || !vec_size(b->life))
        return false;

    la = a->life;
    lb = b->life;
    enda = la + vec_size(a->life);
    endb = lb + vec_size(b->life);
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

bool ir_block_create_store_op(ir_block *self, lex_ctx ctx, int op, ir_value *target, ir_value *what)
{
    ir_instr *in;
    if (self->final) {
        irerror(self->context, "unreachable statement (%s)", self->label);
        return false;
    }
    in = ir_instr_new(ctx, self, op);
    if (!in)
        return false;

    if (target->store == store_value &&
        (op < INSTR_STOREP_F || op > INSTR_STOREP_FNC))
    {
        irerror(self->context, "cannot store to an SSA value");
        irerror(self->context, "trying to store: %s <- %s", target->name, what->name);
        irerror(self->context, "instruction: %s", asm_instr[op].m);
        return false;
    }

    if (!ir_instr_op(in, 0, target, true) ||
        !ir_instr_op(in, 1, what, false))
    {
        return false;
    }
    vec_push(self->instr, in);
    return true;
}

bool ir_block_create_store(ir_block *self, lex_ctx ctx, ir_value *target, ir_value *what)
{
    int op = 0;
    int vtype;
    if (target->vtype == TYPE_VARIANT)
        vtype = what->vtype;
    else
        vtype = target->vtype;

#if 0
    if      (vtype == TYPE_FLOAT   && what->vtype == TYPE_INTEGER)
        op = INSTR_CONV_ITOF;
    else if (vtype == TYPE_INTEGER && what->vtype == TYPE_FLOAT)
        op = INSTR_CONV_FTOI;
#endif
        op = type_store_instr[vtype];

    if (OPTS_FLAG(ADJUST_VECTOR_FIELDS)) {
        if (op == INSTR_STORE_FLD && what->fieldtype == TYPE_VECTOR)
            op = INSTR_STORE_V;
    }

    return ir_block_create_store_op(self, ctx, op, target, what);
}

bool ir_block_create_storep(ir_block *self, lex_ctx ctx, ir_value *target, ir_value *what)
{
    int op = 0;
    int vtype;

    if (target->vtype != TYPE_POINTER)
        return false;

    /* storing using pointer - target is a pointer, type must be
     * inferred from source
     */
    vtype = what->vtype;

    op = type_storep_instr[vtype];
    if (OPTS_FLAG(ADJUST_VECTOR_FIELDS)) {
        if (op == INSTR_STOREP_FLD && what->fieldtype == TYPE_VECTOR)
            op = INSTR_STOREP_V;
    }

    return ir_block_create_store_op(self, ctx, op, target, what);
}

bool ir_block_create_return(ir_block *self, lex_ctx ctx, ir_value *v)
{
    ir_instr *in;
    if (self->final) {
        irerror(self->context, "unreachable statement (%s)", self->label);
        return false;
    }
    self->final = true;
    self->is_return = true;
    in = ir_instr_new(ctx, self, INSTR_RETURN);
    if (!in)
        return false;

    if (v && !ir_instr_op(in, 0, v, false))
        return false;

    vec_push(self->instr, in);
    return true;
}

bool ir_block_create_if(ir_block *self, lex_ctx ctx, ir_value *v,
                        ir_block *ontrue, ir_block *onfalse)
{
    ir_instr *in;
    if (self->final) {
        irerror(self->context, "unreachable statement (%s)", self->label);
        return false;
    }
    self->final = true;
    /*in = ir_instr_new(ctx, self, (v->vtype == TYPE_STRING ? INSTR_IF_S : INSTR_IF_F));*/
    in = ir_instr_new(ctx, self, VINSTR_COND);
    if (!in)
        return false;

    if (!ir_instr_op(in, 0, v, false)) {
        ir_instr_delete(in);
        return false;
    }

    in->bops[0] = ontrue;
    in->bops[1] = onfalse;

    vec_push(self->instr, in);

    vec_push(self->exits, ontrue);
    vec_push(self->exits, onfalse);
    vec_push(ontrue->entries,  self);
    vec_push(onfalse->entries, self);
    return true;
}

bool ir_block_create_jump(ir_block *self, lex_ctx ctx, ir_block *to)
{
    ir_instr *in;
    if (self->final) {
        irerror(self->context, "unreachable statement (%s)", self->label);
        return false;
    }
    self->final = true;
    in = ir_instr_new(ctx, self, VINSTR_JUMP);
    if (!in)
        return false;

    in->bops[0] = to;
    vec_push(self->instr, in);

    vec_push(self->exits, to);
    vec_push(to->entries, self);
    return true;
}

bool ir_block_create_goto(ir_block *self, lex_ctx ctx, ir_block *to)
{
    ir_instr *in;
    if (self->final) {
        irerror(self->context, "unreachable statement (%s)", self->label);
        return false;
    }
    self->final = true;
    in = ir_instr_new(ctx, self, INSTR_GOTO);
    if (!in)
        return false;

    in->bops[0] = to;
    vec_push(self->instr, in);

    vec_push(self->exits, to);
    vec_push(to->entries, self);
    return true;
}

ir_instr* ir_block_create_phi(ir_block *self, lex_ctx ctx, const char *label, int ot)
{
    ir_value *out;
    ir_instr *in;
    in = ir_instr_new(ctx, self, VINSTR_PHI);
    if (!in)
        return NULL;
    out = ir_value_out(self->owner, label, store_value, ot);
    if (!out) {
        ir_instr_delete(in);
        return NULL;
    }
    if (!ir_instr_op(in, 0, out, true)) {
        ir_instr_delete(in);
        ir_value_delete(out);
        return NULL;
    }
    vec_push(self->instr, in);
    return in;
}

ir_value* ir_phi_value(ir_instr *self)
{
    return self->_ops[0];
}

void ir_phi_add(ir_instr* self, ir_block *b, ir_value *v)
{
    ir_phi_entry_t pe;

    if (!vec_ir_block_find(self->owner->entries, b, NULL)) {
        /* Must not be possible to cause this, otherwise the AST
         * is doing something wrong.
         */
        irerror(self->context, "Invalid entry block for PHI");
        abort();
    }

    pe.value = v;
    pe.from = b;
    vec_push(v->reads, self);
    vec_push(self->phi, pe);
}

/* call related code */
ir_instr* ir_block_create_call(ir_block *self, lex_ctx ctx, const char *label, ir_value *func)
{
    ir_value *out;
    ir_instr *in;
    in = ir_instr_new(ctx, self, INSTR_CALL0);
    if (!in)
        return NULL;
    out = ir_value_out(self->owner, label, (func->outtype == TYPE_VOID) ? store_return : store_value, func->outtype);
    if (!out) {
        ir_instr_delete(in);
        return NULL;
    }
    if (!ir_instr_op(in, 0, out, true) ||
        !ir_instr_op(in, 1, func, false))
    {
        ir_instr_delete(in);
        ir_value_delete(out);
        return NULL;
    }
    vec_push(self->instr, in);
    return in;
}

ir_value* ir_call_value(ir_instr *self)
{
    return self->_ops[0];
}

void ir_call_param(ir_instr* self, ir_value *v)
{
    vec_push(self->params, v);
    vec_push(v->reads, self);
}

/* binary op related code */

ir_value* ir_block_create_binop(ir_block *self, lex_ctx ctx,
                                const char *label, int opcode,
                                ir_value *left, ir_value *right)
{
    int ot = TYPE_VOID;
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
        default:
            /* ranges: */
            /* boolean operations result in floats */
            if (opcode >= INSTR_EQ_F && opcode <= INSTR_GT)
                ot = TYPE_FLOAT;
            else if (opcode >= INSTR_LE && opcode <= INSTR_GT)
                ot = TYPE_FLOAT;
#if 0
            else if (opcode >= INSTR_LE_I && opcode <= INSTR_EQ_FI)
                ot = TYPE_FLOAT;
#endif
            break;
    };
    if (ot == TYPE_VOID) {
        /* The AST or parser were supposed to check this! */
        return NULL;
    }

    return ir_block_create_general_instr(self, ctx, label, opcode, left, right, ot);
}

ir_value* ir_block_create_unary(ir_block *self, lex_ctx ctx,
                                const char *label, int opcode,
                                ir_value *operand)
{
    int ot = TYPE_FLOAT;
    switch (opcode) {
        case INSTR_NOT_F:
        case INSTR_NOT_V:
        case INSTR_NOT_S:
        case INSTR_NOT_ENT:
        case INSTR_NOT_FNC:
#if 0
        case INSTR_NOT_I:
#endif
            ot = TYPE_FLOAT;
            break;
        /* QC doesn't have other unary operations. We expect extensions to fill
         * the above list, otherwise we assume out-type = in-type, eg for an
         * unary minus
         */
        default:
            ot = operand->vtype;
            break;
    };
    if (ot == TYPE_VOID) {
        /* The AST or parser were supposed to check this! */
        return NULL;
    }

    /* let's use the general instruction creator and pass NULL for OPB */
    return ir_block_create_general_instr(self, ctx, label, opcode, operand, NULL, ot);
}

ir_value* ir_block_create_general_instr(ir_block *self, lex_ctx ctx, const char *label,
                                        int op, ir_value *a, ir_value *b, int outype)
{
    ir_instr *instr;
    ir_value *out;

    out = ir_value_out(self->owner, label, store_value, outype);
    if (!out)
        return NULL;

    instr = ir_instr_new(ctx, self, op);
    if (!instr) {
        ir_value_delete(out);
        return NULL;
    }

    if (!ir_instr_op(instr, 0, out, true) ||
        !ir_instr_op(instr, 1, a, false) ||
        !ir_instr_op(instr, 2, b, false) )
    {
        goto on_error;
    }

    vec_push(self->instr, instr);

    return out;
on_error:
    ir_instr_delete(instr);
    ir_value_delete(out);
    return NULL;
}

ir_value* ir_block_create_fieldaddress(ir_block *self, lex_ctx ctx, const char *label, ir_value *ent, ir_value *field)
{
    ir_value *v;

    /* Support for various pointer types todo if so desired */
    if (ent->vtype != TYPE_ENTITY)
        return NULL;

    if (field->vtype != TYPE_FIELD)
        return NULL;

    v = ir_block_create_general_instr(self, ctx, label, INSTR_ADDRESS, ent, field, TYPE_POINTER);
    v->fieldtype = field->fieldtype;
    return v;
}

ir_value* ir_block_create_load_from_ent(ir_block *self, lex_ctx ctx, const char *label, ir_value *ent, ir_value *field, int outype)
{
    int op;
    if (ent->vtype != TYPE_ENTITY)
        return NULL;

    /* at some point we could redirect for TYPE_POINTER... but that could lead to carelessness */
    if (field->vtype != TYPE_FIELD)
        return NULL;

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
            irerror(self->context, "invalid type for ir_block_create_load_from_ent: %s", type_name[outype]);
            return NULL;
    }

    return ir_block_create_general_instr(self, ctx, label, op, ent, field, outype);
}

ir_value* ir_block_create_add(ir_block *self, lex_ctx ctx,
                              const char *label,
                              ir_value *left, ir_value *right)
{
    int op = 0;
    int l = left->vtype;
    int r = right->vtype;
    if (l == r) {
        switch (l) {
            default:
                irerror(self->context, "invalid type for ir_block_create_add: %s", type_name[l]);
                return NULL;
            case TYPE_FLOAT:
                op = INSTR_ADD_F;
                break;
#if 0
            case TYPE_INTEGER:
                op = INSTR_ADD_I;
                break;
#endif
            case TYPE_VECTOR:
                op = INSTR_ADD_V;
                break;
        }
    } else {
#if 0
        if ( (l == TYPE_FLOAT && r == TYPE_INTEGER) )
            op = INSTR_ADD_FI;
        else if ( (l == TYPE_INTEGER && r == TYPE_FLOAT) )
            op = INSTR_ADD_IF;
        else
#endif
        {
            irerror(self->context, "invalid type for ir_block_create_add: %s", type_name[l]);
            return NULL;
        }
    }
    return ir_block_create_binop(self, ctx, label, op, left, right);
}

ir_value* ir_block_create_sub(ir_block *self, lex_ctx ctx,
                              const char *label,
                              ir_value *left, ir_value *right)
{
    int op = 0;
    int l = left->vtype;
    int r = right->vtype;
    if (l == r) {

        switch (l) {
            default:
                irerror(self->context, "invalid type for ir_block_create_sub: %s", type_name[l]);
                return NULL;
            case TYPE_FLOAT:
                op = INSTR_SUB_F;
                break;
#if 0
            case TYPE_INTEGER:
                op = INSTR_SUB_I;
                break;
#endif
            case TYPE_VECTOR:
                op = INSTR_SUB_V;
                break;
        }
    } else {
#if 0
        if ( (l == TYPE_FLOAT && r == TYPE_INTEGER) )
            op = INSTR_SUB_FI;
        else if ( (l == TYPE_INTEGER && r == TYPE_FLOAT) )
            op = INSTR_SUB_IF;
        else
#endif
        {
            irerror(self->context, "invalid type for ir_block_create_sub: %s", type_name[l]);
            return NULL;
        }
    }
    return ir_block_create_binop(self, ctx, label, op, left, right);
}

ir_value* ir_block_create_mul(ir_block *self, lex_ctx ctx,
                              const char *label,
                              ir_value *left, ir_value *right)
{
    int op = 0;
    int l = left->vtype;
    int r = right->vtype;
    if (l == r) {

        switch (l) {
            default:
                irerror(self->context, "invalid type for ir_block_create_mul: %s", type_name[l]);
                return NULL;
            case TYPE_FLOAT:
                op = INSTR_MUL_F;
                break;
#if 0
            case TYPE_INTEGER:
                op = INSTR_MUL_I;
                break;
#endif
            case TYPE_VECTOR:
                op = INSTR_MUL_V;
                break;
        }
    } else {
        if ( (l == TYPE_VECTOR && r == TYPE_FLOAT) )
            op = INSTR_MUL_VF;
        else if ( (l == TYPE_FLOAT && r == TYPE_VECTOR) )
            op = INSTR_MUL_FV;
#if 0
        else if ( (l == TYPE_VECTOR && r == TYPE_INTEGER) )
            op = INSTR_MUL_VI;
        else if ( (l == TYPE_INTEGER && r == TYPE_VECTOR) )
            op = INSTR_MUL_IV;
        else if ( (l == TYPE_FLOAT && r == TYPE_INTEGER) )
            op = INSTR_MUL_FI;
        else if ( (l == TYPE_INTEGER && r == TYPE_FLOAT) )
            op = INSTR_MUL_IF;
#endif
        else {
            irerror(self->context, "invalid type for ir_block_create_mul: %s", type_name[l]);
            return NULL;
        }
    }
    return ir_block_create_binop(self, ctx, label, op, left, right);
}

ir_value* ir_block_create_div(ir_block *self, lex_ctx ctx,
                              const char *label,
                              ir_value *left, ir_value *right)
{
    int op = 0;
    int l = left->vtype;
    int r = right->vtype;
    if (l == r) {

        switch (l) {
            default:
                irerror(self->context, "invalid type for ir_block_create_div: %s", type_name[l]);
                return NULL;
            case TYPE_FLOAT:
                op = INSTR_DIV_F;
                break;
#if 0
            case TYPE_INTEGER:
                op = INSTR_DIV_I;
                break;
#endif
        }
    } else {
#if 0
        if ( (l == TYPE_VECTOR && r == TYPE_FLOAT) )
            op = INSTR_DIV_VF;
        else if ( (l == TYPE_FLOAT && r == TYPE_INTEGER) )
            op = INSTR_DIV_FI;
        else if ( (l == TYPE_INTEGER && r == TYPE_FLOAT) )
            op = INSTR_DIV_IF;
        else
#endif
        {
            irerror(self->context, "invalid type for ir_block_create_div: %s", type_name[l]);
            return NULL;
        }
    }
    return ir_block_create_binop(self, ctx, label, op, left, right);
}

/* PHI resolving breaks the SSA, and must thus be the last
 * step before life-range calculation.
 */

static bool ir_block_naive_phi(ir_block *self);
bool ir_function_naive_phi(ir_function *self)
{
    size_t i;

    for (i = 0; i < vec_size(self->blocks); ++i)
    {
        if (!ir_block_naive_phi(self->blocks[i]))
            return false;
    }
    return true;
}

#if 0
static bool ir_naive_phi_emit_store(ir_block *block, size_t iid, ir_value *old, ir_value *what)
{
    ir_instr *instr;
    size_t i;

    /* create a store */
    if (!ir_block_create_store(block, old, what))
        return false;

    /* we now move it up */
    instr = vec_last(block->instr);
    for (i = vec_size(block->instr)-1; i > iid; --i)
        block->instr[i] = block->instr[i-1];
    block->instr[i] = instr;

    return true;
}
#endif

static bool ir_block_naive_phi(ir_block *self)
{
    size_t i, p; /*, w;*/
    /* FIXME: optionally, create_phi can add the phis
     * to a list so we don't need to loop through blocks
     * - anyway: "don't optimize YET"
     */
    for (i = 0; i < vec_size(self->instr); ++i)
    {
        ir_instr *instr = self->instr[i];
        if (instr->opcode != VINSTR_PHI)
            continue;

        vec_remove(self->instr, i, 1);
        --i; /* NOTE: i+1 below */

        for (p = 0; p < vec_size(instr->phi); ++p)
        {
            ir_value *v = instr->phi[p].value;
            ir_block *b = instr->phi[p].from;

            if (v->store == store_value &&
                vec_size(v->reads) == 1 &&
                vec_size(v->writes) == 1)
            {
                /* replace the value */
                if (!ir_instr_op(v->writes[0], 0, instr->_ops[0], true))
                    return false;
            }
            else
            {
                /* force a move instruction */
                ir_instr *prevjump = vec_last(b->instr);
                vec_pop(b->instr);
                b->final = false;
                instr->_ops[0]->store = store_global;
                if (!ir_block_create_store(b, instr->context, instr->_ops[0], v))
                    return false;
                instr->_ops[0]->store = store_value;
                vec_push(b->instr, prevjump);
                b->final = true;
            }

#if 0
            ir_value *v = instr->phi[p].value;
            for (w = 0; w < vec_size(v->writes); ++w) {
                ir_value *old;

                if (!v->writes[w]->_ops[0])
                    continue;

                /* When the write was to a global, we have to emit a mov */
                old = v->writes[w]->_ops[0];

                /* The original instruction now writes to the PHI target local */
                if (v->writes[w]->_ops[0] == v)
                    v->writes[w]->_ops[0] = instr->_ops[0];

                if (old->store != store_value && old->store != store_local && old->store != store_param)
                {
                    /* If it originally wrote to a global we need to store the value
                     * there as welli
                     */
                    if (!ir_naive_phi_emit_store(self, i+1, old, v))
                        return false;
                    if (i+1 < vec_size(self->instr))
                        instr = self->instr[i+1];
                    else
                        instr = NULL;
                    /* In case I forget and access instr later, it'll be NULL
                     * when it's a problem, to make sure we crash, rather than accessing
                     * invalid data.
                     */
                }
                else
                {
                    /* If it didn't, we can replace all reads by the phi target now. */
                    size_t r;
                    for (r = 0; r < vec_size(old->reads); ++r)
                    {
                        size_t op;
                        ir_instr *ri = old->reads[r];
                        for (op = 0; op < vec_size(ri->phi); ++op) {
                            if (ri->phi[op].value == old)
                                ri->phi[op].value = v;
                        }
                        for (op = 0; op < 3; ++op) {
                            if (ri->_ops[op] == old)
                                ri->_ops[op] = v;
                        }
                    }
                }
            }
#endif
        }
        ir_instr_delete(instr);
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
    size_t i;
    size_t eid = *_eid;
    for (i = 0; i < vec_size(self->instr); ++i)
    {
        self->instr[i]->eid = eid++;
    }
    *_eid = eid;
}

/* Enumerate blocks and instructions.
 * The block-enumeration is unordered!
 * We do not really use the block enumreation, however
 * the instruction enumeration is important for life-ranges.
 */
void ir_function_enumerate(ir_function *self)
{
    size_t i;
    size_t instruction_id = 0;
    for (i = 0; i < vec_size(self->blocks); ++i)
    {
        self->blocks[i]->eid = i;
        self->blocks[i]->run_id = 0;
        ir_block_enumerate(self->blocks[i], &instruction_id);
    }
}

static bool ir_block_life_propagate(ir_block *b, ir_block *prev, bool *changed);
bool ir_function_calculate_liferanges(ir_function *self)
{
    size_t i;
    bool changed;

    do {
        self->run_id++;
        changed = false;
        for (i = 0; i != vec_size(self->blocks); ++i)
        {
            if (self->blocks[i]->is_return)
            {
                vec_free(self->blocks[i]->living);
                if (!ir_block_life_propagate(self->blocks[i], NULL, &changed))
                    return false;
            }
        }
    } while (changed);
    if (vec_size(self->blocks)) {
        ir_block *block = self->blocks[0];
        for (i = 0; i < vec_size(block->living); ++i) {
            ir_value *v = block->living[i];
            if (v->memberof || v->store != store_local)
                continue;
            if (irwarning(v->context, WARN_USED_UNINITIALIZED,
                          "variable `%s` may be used uninitialized in this function", v->name))
            {
                return false;
            }
        }
    }
    return true;
}

/* Local-value allocator
 * After finishing creating the liferange of all values used in a function
 * we can allocate their global-positions.
 * This is the counterpart to register-allocation in register machines.
 */
typedef struct {
    ir_value **locals;
    size_t    *sizes;
    size_t    *positions;
} function_allocator;

static bool function_allocator_alloc(function_allocator *alloc, const ir_value *var)
{
    ir_value *slot;
    size_t vsize = type_sizeof[var->vtype];

    slot = ir_value_var("reg", store_global, var->vtype);
    if (!slot)
        return false;

    if (!ir_value_life_merge_into(slot, var))
        goto localerror;

    vec_push(alloc->locals, slot);
    vec_push(alloc->sizes, vsize);

    return true;

localerror:
    ir_value_delete(slot);
    return false;
}

bool ir_function_allocate_locals(ir_function *self)
{
    size_t i, a;
    bool   retval = true;
    size_t pos;

    ir_value *slot;
    const ir_value *v;

    function_allocator alloc;

    if (!vec_size(self->locals) && !vec_size(self->values))
        return true;

    alloc.locals    = NULL;
    alloc.sizes     = NULL;
    alloc.positions = NULL;

    for (i = 0; i < vec_size(self->locals); ++i)
    {
        if (!function_allocator_alloc(&alloc, self->locals[i]))
            goto error;
    }

    /* Allocate a slot for any value that still exists */
    for (i = 0; i < vec_size(self->values); ++i)
    {
        v = self->values[i];

        if (!vec_size(v->life))
            continue;

        for (a = 0; a < vec_size(alloc.locals); ++a)
        {
            slot = alloc.locals[a];

            if (ir_values_overlap(v, slot))
                continue;

            if (!ir_value_life_merge_into(slot, v))
                goto error;

            /* adjust size for this slot */
            if (alloc.sizes[a] < type_sizeof[v->vtype])
                alloc.sizes[a] = type_sizeof[v->vtype];

            self->values[i]->code.local = a;
            break;
        }
        if (a >= vec_size(alloc.locals)) {
            self->values[i]->code.local = vec_size(alloc.locals);
            if (!function_allocator_alloc(&alloc, v))
                goto error;
        }
    }

    if (!alloc.sizes) {
        goto cleanup;
    }

    /* Adjust slot positions based on sizes */
    vec_push(alloc.positions, 0);

    if (vec_size(alloc.sizes))
        pos = alloc.positions[0] + alloc.sizes[0];
    else
        pos = 0;
    for (i = 1; i < vec_size(alloc.sizes); ++i)
    {
        pos = alloc.positions[i-1] + alloc.sizes[i-1];
        vec_push(alloc.positions, pos);
    }

    self->allocated_locals = pos + vec_last(alloc.sizes);

    /* Take over the actual slot positions */
    for (i = 0; i < vec_size(self->values); ++i) {
        self->values[i]->code.local = alloc.positions[self->values[i]->code.local];
    }

    goto cleanup;

error:
    retval = false;
cleanup:
    for (i = 0; i < vec_size(alloc.locals); ++i)
        ir_value_delete(alloc.locals[i]);
    vec_free(alloc.locals);
    vec_free(alloc.sizes);
    vec_free(alloc.positions);
    return retval;
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

static bool ir_block_living_add_instr(ir_block *self, size_t eid)
{
    size_t i;
    bool changed = false;
    bool tempbool;
    for (i = 0; i != vec_size(self->living); ++i)
    {
        tempbool = ir_value_life_merge(self->living[i], eid);
        /* debug
        if (tempbool)
            irerror(self->context, "block_living_add_instr() value instruction added %s: %i", self->living[i]->_name, (int)eid);
        */
        changed = changed || tempbool;
    }
    return changed;
}

static bool ir_block_life_prop_previous(ir_block* self, ir_block *prev, bool *changed)
{
    size_t i;

    (void)changed;

    /* values which have been read in a previous iteration are now
     * in the "living" array even if the previous block doesn't use them.
     * So we have to remove whatever does not exist in the previous block.
     * They will be re-added on-read, but the liferange merge won't cause
     * a change.
     */
    for (i = 0; i < vec_size(self->living); ++i)
    {
        if (!vec_ir_value_find(prev->living, self->living[i], NULL)) {
            vec_remove(self->living, i, 1);
            --i;
        }
    }

    /* Whatever the previous block still has in its living set
     * must now be added to ours as well.
     */
    for (i = 0; i < vec_size(prev->living); ++i)
    {
        if (vec_ir_value_find(self->living, prev->living[i], NULL))
            continue;
        vec_push(self->living, prev->living[i]);
        /*
        irerror(self->contextt from prev: %s", self->label, prev->living[i]->_name);
        */
    }
    return true;
}

static bool ir_block_life_propagate(ir_block *self, ir_block *prev, bool *changed)
{
    ir_instr *instr;
    ir_value *value;
    bool  tempbool;
    size_t i, o, p;
    /* bitmasks which operands are read from or written to */
    size_t read, write;
    char dbg_ind[16] = { '#', '0' };
    (void)dbg_ind;

    if (prev)
    {
        if (!ir_block_life_prop_previous(self, prev, changed))
            return false;
    }

    i = vec_size(self->instr);
    while (i)
    { --i;
        instr = self->instr[i];

        /* PHI operands are always read operands */
        for (p = 0; p < vec_size(instr->phi); ++p)
        {
            value = instr->phi[p].value;
            if (value->memberof)
                value = value->memberof;
            if (!vec_ir_value_find(self->living, value, NULL))
                vec_push(self->living, value);
        }

        /* call params are read operands too */
        for (p = 0; p < vec_size(instr->params); ++p)
        {
            value = instr->params[p];
            if (value->memberof)
                value = value->memberof;
            if (!vec_ir_value_find(self->living, value, NULL))
                vec_push(self->living, value);
        }

        /* See which operands are read and write operands */
        ir_op_read_write(instr->opcode, &read, &write);

        if (instr->opcode == INSTR_MUL_VF)
        {
            /* the float source will get an additional lifetime */
            tempbool = ir_value_life_merge(instr->_ops[2], instr->eid+1);
            *changed = *changed || tempbool;
        }
        else if (instr->opcode == INSTR_MUL_FV)
        {
            /* the float source will get an additional lifetime */
            tempbool = ir_value_life_merge(instr->_ops[1], instr->eid+1);
            *changed = *changed || tempbool;
        }

        /* Go through the 3 main operands */
        for (o = 0; o < 3; ++o)
        {
            if (!instr->_ops[o]) /* no such operand */
                continue;

            value = instr->_ops[o];
            if (value->memberof)
                value = value->memberof;

            /* We only care about locals */
            /* we also calculate parameter liferanges so that locals
             * can take up parameter slots */
            if (value->store != store_value &&
                value->store != store_local &&
                value->store != store_param)
                continue;

            /* read operands */
            if (read & (1<<o))
            {
                if (!vec_ir_value_find(self->living, value, NULL))
                    vec_push(self->living, value);
            }

            /* write operands */
            /* When we write to a local, we consider it "dead" for the
             * remaining upper part of the function, since in SSA a value
             * can only be written once (== created)
             */
            if (write & (1<<o))
            {
                size_t idx;
                bool in_living = vec_ir_value_find(self->living, value, &idx);
                if (!in_living)
                {
                    /* If the value isn't alive it hasn't been read before... */
                    /* TODO: See if the warning can be emitted during parsing or AST processing
                     * otherwise have warning printed here.
                     * IF printing a warning here: include filecontext_t,
                     * and make sure it's only printed once
                     * since this function is run multiple times.
                     */
                    /* For now: debug info: */
                    /* con_err( "Value only written %s\n", value->name); */
                    tempbool = ir_value_life_merge(value, instr->eid);
                    *changed = *changed || tempbool;
                    /*
                    ir_instr_dump(instr, dbg_ind, printf);
                    abort();
                    */
                } else {
                    /* since 'living' won't contain it
                     * anymore, merge the value, since
                     * (A) doesn't.
                     */
                    tempbool = ir_value_life_merge(value, instr->eid);
                    /*
                    if (tempbool)
                        con_err( "value added id %s %i\n", value->name, (int)instr->eid);
                    */
                    *changed = *changed || tempbool;
                    /* Then remove */
                    vec_remove(self->living, idx, 1);
                }
            }
        }
        /* (A) */
        tempbool = ir_block_living_add_instr(self, instr->eid);
        /*con_err( "living added values\n");*/
        *changed = *changed || tempbool;

    }

    if (self->run_id == self->owner->run_id)
        return true;

    self->run_id = self->owner->run_id;

    for (i = 0; i < vec_size(self->entries); ++i)
    {
        ir_block *entry = self->entries[i];
        ir_block_life_propagate(entry, self, changed);
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
static bool ir_builder_gen_global(ir_builder *self, ir_value *global, bool islocal);

static bool gen_global_field(ir_value *global)
{
    if (global->hasvalue)
    {
        ir_value *fld = global->constval.vpointer;
        if (!fld) {
            irerror(global->context, "Invalid field constant with no field: %s", global->name);
            return false;
        }

        /* copy the field's value */
        ir_value_code_setaddr(global, vec_size(code_globals));
        vec_push(code_globals, fld->code.fieldaddr);
        if (global->fieldtype == TYPE_VECTOR) {
            vec_push(code_globals, fld->code.fieldaddr+1);
            vec_push(code_globals, fld->code.fieldaddr+2);
        }
    }
    else
    {
        ir_value_code_setaddr(global, vec_size(code_globals));
        vec_push(code_globals, 0);
        if (global->fieldtype == TYPE_VECTOR) {
            vec_push(code_globals, 0);
            vec_push(code_globals, 0);
        }
    }
    if (global->code.globaladdr < 0)
        return false;
    return true;
}

static bool gen_global_pointer(ir_value *global)
{
    if (global->hasvalue)
    {
        ir_value *target = global->constval.vpointer;
        if (!target) {
            irerror(global->context, "Invalid pointer constant: %s", global->name);
            /* NULL pointers are pointing to the NULL constant, which also
             * sits at address 0, but still has an ir_value for itself.
             */
            return false;
        }

        /* Here, relocations ARE possible - in fteqcc-enhanced-qc:
         * void() foo; <- proto
         * void() *fooptr = &foo;
         * void() foo = { code }
         */
        if (!target->code.globaladdr) {
            /* FIXME: Check for the constant nullptr ir_value!
             * because then code.globaladdr being 0 is valid.
             */
            irerror(global->context, "FIXME: Relocation support");
            return false;
        }

        ir_value_code_setaddr(global, vec_size(code_globals));
        vec_push(code_globals, target->code.globaladdr);
    }
    else
    {
        ir_value_code_setaddr(global, vec_size(code_globals));
        vec_push(code_globals, 0);
    }
    if (global->code.globaladdr < 0)
        return false;
    return true;
}

static bool gen_blocks_recursive(ir_function *func, ir_block *block)
{
    prog_section_statement stmt;
    ir_instr *instr;
    ir_block *target;
    ir_block *ontrue;
    ir_block *onfalse;
    size_t    stidx;
    size_t    i;

tailcall:
    block->generated = true;
    block->code_start = vec_size(code_statements);
    for (i = 0; i < vec_size(block->instr); ++i)
    {
        instr = block->instr[i];

        if (instr->opcode == VINSTR_PHI) {
            irerror(block->context, "cannot generate virtual instruction (phi)");
            return false;
        }

        if (instr->opcode == VINSTR_JUMP) {
            target = instr->bops[0];
            /* for uncoditional jumps, if the target hasn't been generated
             * yet, we generate them right here.
             */
            if (!target->generated) {
                block = target;
                goto tailcall;
            }

            /* otherwise we generate a jump instruction */
            stmt.opcode = INSTR_GOTO;
            stmt.o1.s1 = (target->code_start) - vec_size(code_statements);
            stmt.o2.s1 = 0;
            stmt.o3.s1 = 0;
            code_push_statement(&stmt, instr->context.line);

            /* no further instructions can be in this block */
            return true;
        }

        if (instr->opcode == VINSTR_COND) {
            ontrue  = instr->bops[0];
            onfalse = instr->bops[1];
            /* TODO: have the AST signal which block should
             * come first: eg. optimize IFs without ELSE...
             */

            stmt.o1.u1 = ir_value_code_addr(instr->_ops[0]);
            stmt.o2.u1 = 0;
            stmt.o3.s1 = 0;

            if (ontrue->generated) {
                stmt.opcode = INSTR_IF;
                stmt.o2.s1 = (ontrue->code_start) - vec_size(code_statements);
                code_push_statement(&stmt, instr->context.line);
            }
            if (onfalse->generated) {
                stmt.opcode = INSTR_IFNOT;
                stmt.o2.s1 = (onfalse->code_start) - vec_size(code_statements);
                code_push_statement(&stmt, instr->context.line);
            }
            if (!ontrue->generated) {
                if (onfalse->generated) {
                    block = ontrue;
                    goto tailcall;
                }
            }
            if (!onfalse->generated) {
                if (ontrue->generated) {
                    block = onfalse;
                    goto tailcall;
                }
            }
            /* neither ontrue nor onfalse exist */
            stmt.opcode = INSTR_IFNOT;
            if (!instr->likely) {
                /* Honor the likelyhood hint */
                ir_block *tmp = onfalse;
                stmt.opcode = INSTR_IF;
                onfalse = ontrue;
                ontrue = tmp;
            }
            stidx = vec_size(code_statements);
            code_push_statement(&stmt, instr->context.line);
            /* on false we jump, so add ontrue-path */
            if (!gen_blocks_recursive(func, ontrue))
                return false;
            /* fixup the jump address */
            code_statements[stidx].o2.s1 = vec_size(code_statements) - stidx;
            /* generate onfalse path */
            if (onfalse->generated) {
                /* fixup the jump address */
                code_statements[stidx].o2.s1 = (onfalse->code_start) - (stidx);
                stmt.opcode = vec_last(code_statements).opcode;
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
                stmt.o1.s1 = (onfalse->code_start) - vec_size(code_statements);
                stmt.o2.s1 = 0;
                stmt.o3.s1 = 0;
                code_push_statement(&stmt, instr->context.line);
                return true;
            }
            /* if not, generate now */
            block = onfalse;
            goto tailcall;
        }

        if (instr->opcode >= INSTR_CALL0 && instr->opcode <= INSTR_CALL8) {
            /* Trivial call translation:
             * copy all params to OFS_PARM*
             * if the output's storetype is not store_return,
             * add append a STORE instruction!
             *
             * NOTES on how to do it better without much trouble:
             * -) The liferanges!
             *      Simply check the liferange of all parameters for
             *      other CALLs. For each param with no CALL in its
             *      liferange, we can store it in an OFS_PARM at
             *      generation already. This would even include later
             *      reuse.... probably... :)
             */
            size_t p, first;
            ir_value *retvalue;

            first = vec_size(instr->params);
            if (first > 8)
                first = 8;
            for (p = 0; p < first; ++p)
            {
                ir_value *param = instr->params[p];

                stmt.opcode = INSTR_STORE_F;
                stmt.o3.u1 = 0;

                if (param->vtype == TYPE_FIELD)
                    stmt.opcode = field_store_instr[param->fieldtype];
                else
                    stmt.opcode = type_store_instr[param->vtype];
                stmt.o1.u1 = ir_value_code_addr(param);
                stmt.o2.u1 = OFS_PARM0 + 3 * p;
                code_push_statement(&stmt, instr->context.line);
            }
            /* Now handle extparams */
            first = vec_size(instr->params);
            for (; p < first; ++p)
            {
                ir_builder *ir = func->owner;
                ir_value *param = instr->params[p];
                ir_value *targetparam;

                if (p-8 >= vec_size(ir->extparams))
                    ir_gen_extparam(ir);

                targetparam = ir->extparams[p-8];

                stmt.opcode = INSTR_STORE_F;
                stmt.o3.u1 = 0;

                if (param->vtype == TYPE_FIELD)
                    stmt.opcode = field_store_instr[param->fieldtype];
                else
                    stmt.opcode = type_store_instr[param->vtype];
                stmt.o1.u1 = ir_value_code_addr(param);
                stmt.o2.u1 = ir_value_code_addr(targetparam);
                code_push_statement(&stmt, instr->context.line);
            }

            stmt.opcode = INSTR_CALL0 + vec_size(instr->params);
            if (stmt.opcode > INSTR_CALL8)
                stmt.opcode = INSTR_CALL8;
            stmt.o1.u1 = ir_value_code_addr(instr->_ops[1]);
            stmt.o2.u1 = 0;
            stmt.o3.u1 = 0;
            code_push_statement(&stmt, instr->context.line);

            retvalue = instr->_ops[0];
            if (retvalue && retvalue->store != store_return && vec_size(retvalue->life))
            {
                /* not to be kept in OFS_RETURN */
                if (retvalue->vtype == TYPE_FIELD)
                    stmt.opcode = field_store_instr[retvalue->vtype];
                else
                    stmt.opcode = type_store_instr[retvalue->vtype];
                stmt.o1.u1 = OFS_RETURN;
                stmt.o2.u1 = ir_value_code_addr(retvalue);
                stmt.o3.u1 = 0;
                code_push_statement(&stmt, instr->context.line);
            }
            continue;
        }

        if (instr->opcode == INSTR_STATE) {
            irerror(block->context, "TODO: state instruction");
            return false;
        }

        stmt.opcode = instr->opcode;
        stmt.o1.u1 = 0;
        stmt.o2.u1 = 0;
        stmt.o3.u1 = 0;

        /* This is the general order of operands */
        if (instr->_ops[0])
            stmt.o3.u1 = ir_value_code_addr(instr->_ops[0]);

        if (instr->_ops[1])
            stmt.o1.u1 = ir_value_code_addr(instr->_ops[1]);

        if (instr->_ops[2])
            stmt.o2.u1 = ir_value_code_addr(instr->_ops[2]);

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
        }

        code_push_statement(&stmt, instr->context.line);
    }
    return true;
}

static bool gen_function_code(ir_function *self)
{
    ir_block *block;
    prog_section_statement stmt;

    /* Starting from entry point, we generate blocks "as they come"
     * for now. Dead blocks will not be translated obviously.
     */
    if (!vec_size(self->blocks)) {
        irerror(self->context, "Function '%s' declared without body.", self->name);
        return false;
    }

    block = self->blocks[0];
    if (block->generated)
        return true;

    if (!gen_blocks_recursive(self, block)) {
        irerror(self->context, "failed to generate blocks for '%s'", self->name);
        return false;
    }

    /* otherwise code_write crashes since it debug-prints functions until AINSTR_END */
    stmt.opcode = AINSTR_END;
    stmt.o1.u1 = 0;
    stmt.o2.u1 = 0;
    stmt.o3.u1 = 0;
    code_push_statement(&stmt, vec_last(code_linenums));
    return true;
}

static qcint ir_builder_filestring(ir_builder *ir, const char *filename)
{
    /* NOTE: filename pointers are copied, we never strdup them,
     * thus we can use pointer-comparison to find the string.
     */
    size_t i;
    qcint  str;

    for (i = 0; i < vec_size(ir->filenames); ++i) {
        if (ir->filenames[i] == filename)
            return ir->filestrings[i];
    }

    str = code_genstring(filename);
    vec_push(ir->filenames, filename);
    vec_push(ir->filestrings, str);
    return str;
}

static bool gen_global_function(ir_builder *ir, ir_value *global)
{
    prog_section_function fun;
    ir_function          *irfun;

    size_t i;
    size_t local_var_end;

    if (!global->hasvalue || (!global->constval.vfunc))
    {
        irerror(global->context, "Invalid state of function-global: not constant: %s", global->name);
        return false;
    }

    irfun = global->constval.vfunc;

    fun.name    = global->code.name;
    fun.file    = ir_builder_filestring(ir, global->context.file);
    fun.profile = 0; /* always 0 */
    fun.nargs   = vec_size(irfun->params);
    if (fun.nargs > 8)
        fun.nargs = 8;

    for (i = 0;i < 8; ++i) {
        if ((int32_t)i >= fun.nargs)
            fun.argsize[i] = 0;
        else
            fun.argsize[i] = type_sizeof[irfun->params[i]];
    }

    fun.firstlocal = vec_size(code_globals);

    local_var_end = fun.firstlocal;
    for (i = 0; i < vec_size(irfun->locals); ++i) {
        if (!ir_builder_gen_global(ir, irfun->locals[i], true)) {
            irerror(irfun->locals[i]->context, "Failed to generate local %s", irfun->locals[i]->name);
            return false;
        }
    }
    if (vec_size(irfun->locals)) {
        ir_value *last = vec_last(irfun->locals);
        local_var_end = last->code.globaladdr;
        if (last->vtype == TYPE_FIELD && last->fieldtype == TYPE_VECTOR)
            local_var_end += type_sizeof[TYPE_VECTOR];
        else
            local_var_end += type_sizeof[last->vtype];
    }
    for (i = 0; i < vec_size(irfun->values); ++i)
    {
        /* generate code.globaladdr for ssa values */
        ir_value *v = irfun->values[i];
        ir_value_code_setaddr(v, local_var_end + v->code.local);
    }
    for (i = 0; i < irfun->allocated_locals; ++i) {
        /* fill the locals with zeros */
        vec_push(code_globals, 0);
    }

    fun.locals = vec_size(code_globals) - fun.firstlocal;

    if (irfun->builtin)
        fun.entry = irfun->builtin+1;
    else {
        irfun->code_function_def = vec_size(code_functions);
        fun.entry = vec_size(code_statements);
    }

    vec_push(code_functions, fun);
    return true;
}

static void ir_gen_extparam(ir_builder *ir)
{
    prog_section_def def;
    ir_value        *global;
    char             name[128];

    snprintf(name, sizeof(name), "EXTPARM#%i", (int)(vec_size(ir->extparams)+8));
    global = ir_value_var(name, store_global, TYPE_VECTOR);

    def.name = code_genstring(name);
    def.type = TYPE_VECTOR;
    def.offset = vec_size(code_globals);

    vec_push(code_defs, def);
    ir_value_code_setaddr(global, def.offset);
    vec_push(code_globals, 0);
    vec_push(code_globals, 0);
    vec_push(code_globals, 0);

    vec_push(ir->extparams, global);
}

static bool gen_function_extparam_copy(ir_function *self)
{
    size_t i, ext, numparams;

    ir_builder *ir = self->owner;
    ir_value   *ep;
    prog_section_statement stmt;

    numparams = vec_size(self->params);
    if (!numparams)
        return true;

    stmt.opcode = INSTR_STORE_F;
    stmt.o3.s1 = 0;
    for (i = 8; i < numparams; ++i) {
        ext = i - 8;
        if (ext >= vec_size(ir->extparams))
            ir_gen_extparam(ir);

        ep = ir->extparams[ext];

        stmt.opcode = type_store_instr[self->locals[i]->vtype];
        if (self->locals[i]->vtype == TYPE_FIELD &&
            self->locals[i]->fieldtype == TYPE_VECTOR)
        {
            stmt.opcode = INSTR_STORE_V;
        }
        stmt.o1.u1 = ir_value_code_addr(ep);
        stmt.o2.u1 = ir_value_code_addr(self->locals[i]);
        code_push_statement(&stmt, self->context.line);
    }

    return true;
}

static bool gen_global_function_code(ir_builder *ir, ir_value *global)
{
    prog_section_function *fundef;
    ir_function           *irfun;

    (void)ir;

    irfun = global->constval.vfunc;
    if (!irfun) {
        if (global->cvq == CV_NONE) {
            irwarning(global->context, WARN_IMPLICIT_FUNCTION_POINTER,
                      "function `%s` has no body and in QC implicitly becomes a function-pointer", global->name);
        }
        /* this was a function pointer, don't generate code for those */
        return true;
    }

    if (irfun->builtin)
        return true;

    if (irfun->code_function_def < 0) {
        irerror(irfun->context, "`%s`: IR global wasn't generated, failed to access function-def", irfun->name);
        return false;
    }
    fundef = &code_functions[irfun->code_function_def];

    fundef->entry = vec_size(code_statements);
    if (!gen_function_extparam_copy(irfun)) {
        irerror(irfun->context, "Failed to generate extparam-copy code for function %s", irfun->name);
        return false;
    }
    if (!gen_function_code(irfun)) {
        irerror(irfun->context, "Failed to generate code for function %s", irfun->name);
        return false;
    }
    return true;
}

static bool ir_builder_gen_global(ir_builder *self, ir_value *global, bool islocal)
{
    size_t           i;
    int32_t         *iptr;
    prog_section_def def;

    def.type   = global->vtype;
    def.offset = vec_size(code_globals);

    if (global->name) {
        if (global->name[0] == '#') {
            if (!self->str_immediate)
                self->str_immediate = code_genstring("IMMEDIATE");
            def.name = global->code.name = self->str_immediate;
        }
        else
            def.name = global->code.name = code_genstring(global->name);
    }
    else
        def.name   = 0;

    switch (global->vtype)
    {
    case TYPE_VOID:
        if (!strcmp(global->name, "end_sys_globals")) {
            /* TODO: remember this point... all the defs before this one
             * should be checksummed and added to progdefs.h when we generate it.
             */
        }
        else if (!strcmp(global->name, "end_sys_fields")) {
            /* TODO: same as above but for entity-fields rather than globsl
             */
        }
        else
            irwarning(global->context, WARN_VOID_VARIABLES, "unrecognized variable of type void `%s`",
                      global->name);
        /* I'd argue setting it to 0 is sufficient, but maybe some depend on knowing how far
         * the system fields actually go? Though the engine knows this anyway...
         * Maybe this could be an -foption
         * fteqcc creates data for end_sys_* - of size 1, so let's do the same
         */
        ir_value_code_setaddr(global, vec_size(code_globals));
        vec_push(code_globals, 0);
        /* Add the def */
        vec_push(code_defs, def);
        return true;
    case TYPE_POINTER:
        vec_push(code_defs, def);
        return gen_global_pointer(global);
    case TYPE_FIELD:
        vec_push(code_defs, def);
        return gen_global_field(global);
    case TYPE_ENTITY:
        /* fall through */
    case TYPE_FLOAT:
    {
        ir_value_code_setaddr(global, vec_size(code_globals));
        if (global->hasvalue) {
            iptr = (int32_t*)&global->constval.ivec[0];
            vec_push(code_globals, *iptr);
        } else {
            vec_push(code_globals, 0);
            if (!islocal)
                def.type |= DEF_SAVEGLOBAL;
        }
        vec_push(code_defs, def);

        return global->code.globaladdr >= 0;
    }
    case TYPE_STRING:
    {
        ir_value_code_setaddr(global, vec_size(code_globals));
        if (global->hasvalue) {
            vec_push(code_globals, code_genstring(global->constval.vstring));
        } else {
            vec_push(code_globals, 0);
            if (!islocal)
                def.type |= DEF_SAVEGLOBAL;
        }
        vec_push(code_defs, def);
        return global->code.globaladdr >= 0;
    }
    case TYPE_VECTOR:
    {
        size_t d;
        ir_value_code_setaddr(global, vec_size(code_globals));
        if (global->hasvalue) {
            iptr = (int32_t*)&global->constval.ivec[0];
            vec_push(code_globals, iptr[0]);
            if (global->code.globaladdr < 0)
                return false;
            for (d = 1; d < type_sizeof[global->vtype]; ++d)
            {
                vec_push(code_globals, iptr[d]);
            }
        } else {
            vec_push(code_globals, 0);
            if (global->code.globaladdr < 0)
                return false;
            for (d = 1; d < type_sizeof[global->vtype]; ++d)
            {
                vec_push(code_globals, 0);
            }
            if (!islocal)
                def.type |= DEF_SAVEGLOBAL;
        }

        vec_push(code_defs, def);
        return global->code.globaladdr >= 0;
    }
    case TYPE_FUNCTION:
        ir_value_code_setaddr(global, vec_size(code_globals));
        if (!global->hasvalue) {
            vec_push(code_globals, 0);
            if (global->code.globaladdr < 0)
                return false;
        } else {
            vec_push(code_globals, vec_size(code_functions));
            if (!gen_global_function(self, global))
                return false;
            if (!islocal)
                def.type |= DEF_SAVEGLOBAL;
        }
        vec_push(code_defs, def);
        return true;
    case TYPE_VARIANT:
        /* assume biggest type */
            ir_value_code_setaddr(global, vec_size(code_globals));
            vec_push(code_globals, 0);
            for (i = 1; i < type_sizeof[TYPE_VARIANT]; ++i)
                vec_push(code_globals, 0);
            return true;
    default:
        /* refuse to create 'void' type or any other fancy business. */
        irerror(global->context, "Invalid type for global variable `%s`: %s",
                global->name, type_name[global->vtype]);
        return false;
    }
}

static void ir_builder_prepare_field(ir_value *field)
{
    field->code.fieldaddr = code_alloc_field(type_sizeof[field->fieldtype]);
}

static bool ir_builder_gen_field(ir_builder *self, ir_value *field)
{
    prog_section_def def;
    prog_section_field fld;

    (void)self;

    def.type   = (uint16_t)field->vtype;
    def.offset = (uint16_t)vec_size(code_globals);

    /* create a global named the same as the field */
    if (opts_standard == COMPILER_GMQCC) {
        /* in our standard, the global gets a dot prefix */
        size_t len = strlen(field->name);
        char name[1024];

        /* we really don't want to have to allocate this, and 1024
         * bytes is more than enough for a variable/field name
         */
        if (len+2 >= sizeof(name)) {
            irerror(field->context, "invalid field name size: %u", (unsigned int)len);
            return false;
        }

        name[0] = '.';
        memcpy(name+1, field->name, len); /* no strncpy - we used strlen above */
        name[len+1] = 0;

        def.name = code_genstring(name);
        fld.name = def.name + 1; /* we reuse that string table entry */
    } else {
        /* in plain QC, there cannot be a global with the same name,
         * and so we also name the global the same.
         * FIXME: fteqcc should create a global as well
         * check if it actually uses the same name. Probably does
         */
        def.name = code_genstring(field->name);
        fld.name = def.name;
    }

    field->code.name = def.name;

    vec_push(code_defs, def);

    fld.type = field->fieldtype;

    if (fld.type == TYPE_VOID) {
        irerror(field->context, "field is missing a type: %s - don't know its size", field->name);
        return false;
    }

    fld.offset = field->code.fieldaddr;

    vec_push(code_fields, fld);

    ir_value_code_setaddr(field, vec_size(code_globals));
    vec_push(code_globals, fld.offset);
    if (fld.type == TYPE_VECTOR) {
        vec_push(code_globals, fld.offset+1);
        vec_push(code_globals, fld.offset+2);
    }

    return field->code.globaladdr >= 0;
}

bool ir_builder_generate(ir_builder *self, const char *filename)
{
    prog_section_statement stmt;
    size_t i;
    char   *lnofile = NULL;

    code_init();

    for (i = 0; i < vec_size(self->fields); ++i)
    {
        ir_builder_prepare_field(self->fields[i]);
    }

    for (i = 0; i < vec_size(self->globals); ++i)
    {
        if (!ir_builder_gen_global(self, self->globals[i], false)) {
            return false;
        }
    }

    for (i = 0; i < vec_size(self->fields); ++i)
    {
        if (!ir_builder_gen_field(self, self->fields[i])) {
            return false;
        }
    }

    /* generate function code */
    for (i = 0; i < vec_size(self->globals); ++i)
    {
        if (self->globals[i]->vtype == TYPE_FUNCTION) {
            if (!gen_global_function_code(self, self->globals[i])) {
                return false;
            }
        }
    }

    if (vec_size(code_globals) >= 65536) {
        irerror(vec_last(self->globals)->context, "This progs file would require more globals than the metadata can handle. Bailing out.");
        return false;
    }

    /* DP errors if the last instruction is not an INSTR_DONE
     * and for debugging purposes we add an additional AINSTR_END
     * to the end of functions, so here it goes:
     */
    stmt.opcode = INSTR_DONE;
    stmt.o1.u1 = 0;
    stmt.o2.u1 = 0;
    stmt.o3.u1 = 0;
    code_push_statement(&stmt, vec_last(code_linenums));

    if (opts_pp_only)
        return true;

    if (vec_size(code_statements) != vec_size(code_linenums)) {
        con_err("Linecounter wrong: %lu != %lu\n",
                (unsigned long)vec_size(code_statements),
                (unsigned long)vec_size(code_linenums));
    } else if (OPTS_FLAG(LNO)) {
        char *dot;
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

    if (lnofile)
        con_out("writing '%s' and '%s'...\n", filename, lnofile);
    else
        con_out("writing '%s'\n", filename);
    if (!code_write(filename, lnofile)) {
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

#ifdef WIN32
# define strncat(dst, src, sz) strncat_s(dst, sz, src, _TRUNCATE)
#endif

const char *qc_opname(int op)
{
    if (op < 0) return "<INVALID>";
    if (op < (int)( sizeof(asm_instr) / sizeof(asm_instr[0]) ))
        return asm_instr[op].m;
    switch (op) {
        case VINSTR_PHI:  return "PHI";
        case VINSTR_JUMP: return "JUMP";
        case VINSTR_COND: return "COND";
        default:          return "<UNK>";
    }
}

void ir_builder_dump(ir_builder *b, int (*oprintf)(const char*, ...))
{
    size_t i;
    char indent[IND_BUFSZ];
    indent[0] = '\t';
    indent[1] = 0;

    oprintf("module %s\n", b->name);
    for (i = 0; i < vec_size(b->globals); ++i)
    {
        oprintf("global ");
        if (b->globals[i]->hasvalue)
            oprintf("%s = ", b->globals[i]->name);
        ir_value_dump(b->globals[i], oprintf);
        oprintf("\n");
    }
    for (i = 0; i < vec_size(b->functions); ++i)
        ir_function_dump(b->functions[i], indent, oprintf);
    oprintf("endmodule %s\n", b->name);
}

void ir_function_dump(ir_function *f, char *ind,
                      int (*oprintf)(const char*, ...))
{
    size_t i;
    if (f->builtin != 0) {
        oprintf("%sfunction %s = builtin %i\n", ind, f->name, -f->builtin);
        return;
    }
    oprintf("%sfunction %s\n", ind, f->name);
    strncat(ind, "\t", IND_BUFSZ);
    if (vec_size(f->locals))
    {
        oprintf("%s%i locals:\n", ind, (int)vec_size(f->locals));
        for (i = 0; i < vec_size(f->locals); ++i) {
            oprintf("%s\t", ind);
            ir_value_dump(f->locals[i], oprintf);
            oprintf("\n");
        }
    }
    oprintf("%sliferanges:\n", ind);
    for (i = 0; i < vec_size(f->locals); ++i) {
        size_t l;
        ir_value *v = f->locals[i];
        oprintf("%s\t%s: unique ", ind, v->name);
        for (l = 0; l < vec_size(v->life); ++l) {
            oprintf("[%i,%i] ", v->life[l].start, v->life[l].end);
        }
        oprintf("\n");
    }
    for (i = 0; i < vec_size(f->values); ++i) {
        size_t l;
        ir_value *v = f->values[i];
        oprintf("%s\t%s: @%i ", ind, v->name, (int)v->code.local);
        for (l = 0; l < vec_size(v->life); ++l) {
            oprintf("[%i,%i] ", v->life[l].start, v->life[l].end);
        }
        oprintf("\n");
    }
    if (vec_size(f->blocks))
    {
        oprintf("%slife passes (check): %i\n", ind, (int)f->run_id);
        for (i = 0; i < vec_size(f->blocks); ++i) {
            if (f->blocks[i]->run_id != f->run_id) {
                oprintf("%slife pass check fail! %i != %i\n", ind, (int)f->blocks[i]->run_id, (int)f->run_id);
            }
            ir_block_dump(f->blocks[i], ind, oprintf);
        }

    }
    ind[strlen(ind)-1] = 0;
    oprintf("%sendfunction %s\n", ind, f->name);
}

void ir_block_dump(ir_block* b, char *ind,
                   int (*oprintf)(const char*, ...))
{
    size_t i;
    oprintf("%s:%s\n", ind, b->label);
    strncat(ind, "\t", IND_BUFSZ);

    for (i = 0; i < vec_size(b->instr); ++i)
        ir_instr_dump(b->instr[i], ind, oprintf);
    ind[strlen(ind)-1] = 0;
}

void dump_phi(ir_instr *in, int (*oprintf)(const char*, ...))
{
    size_t i;
    oprintf("%s <- phi ", in->_ops[0]->name);
    for (i = 0; i < vec_size(in->phi); ++i)
    {
        oprintf("([%s] : %s) ", in->phi[i].from->label,
                                in->phi[i].value->name);
    }
    oprintf("\n");
}

void ir_instr_dump(ir_instr *in, char *ind,
                       int (*oprintf)(const char*, ...))
{
    size_t i;
    const char *comma = NULL;

    oprintf("%s (%i) ", ind, (int)in->eid);

    if (in->opcode == VINSTR_PHI) {
        dump_phi(in, oprintf);
        return;
    }

    strncat(ind, "\t", IND_BUFSZ);

    if (in->_ops[0] && (in->_ops[1] || in->_ops[2])) {
        ir_value_dump(in->_ops[0], oprintf);
        if (in->_ops[1] || in->_ops[2])
            oprintf(" <- ");
    }
    if (in->opcode == INSTR_CALL0) {
        oprintf("CALL%i\t", vec_size(in->params));
    } else
        oprintf("%s\t", qc_opname(in->opcode));

    if (in->_ops[0] && !(in->_ops[1] || in->_ops[2])) {
        ir_value_dump(in->_ops[0], oprintf);
        comma = ",\t";
    }
    else
    {
        for (i = 1; i != 3; ++i) {
            if (in->_ops[i]) {
                if (comma)
                    oprintf(comma);
                ir_value_dump(in->_ops[i], oprintf);
                comma = ",\t";
            }
        }
    }
    if (in->bops[0]) {
        if (comma)
            oprintf(comma);
        oprintf("[%s]", in->bops[0]->label);
        comma = ",\t";
    }
    if (in->bops[1])
        oprintf("%s[%s]", comma, in->bops[1]->label);
    if (vec_size(in->params)) {
        oprintf("\tparams: ");
        for (i = 0; i != vec_size(in->params); ++i) {
            oprintf("%s, ", in->params[i]->name);
        }
    }
    oprintf("\n");
    ind[strlen(ind)-1] = 0;
}

void ir_value_dump_string(const char *str, int (*oprintf)(const char*, ...))
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

void ir_value_dump(ir_value* v, int (*oprintf)(const char*, ...))
{
    if (v->hasvalue) {
        switch (v->vtype) {
            default:
            case TYPE_VOID:
                oprintf("(void)");
                break;
            case TYPE_FUNCTION:
                oprintf("fn:%s", v->name);
                break;
            case TYPE_FLOAT:
                oprintf("%g", v->constval.vfloat);
                break;
            case TYPE_VECTOR:
                oprintf("'%g %g %g'",
                        v->constval.vvec.x,
                        v->constval.vvec.y,
                        v->constval.vvec.z);
                break;
            case TYPE_ENTITY:
                oprintf("(entity)");
                break;
            case TYPE_STRING:
                ir_value_dump_string(v->constval.vstring, oprintf);
                break;
#if 0
            case TYPE_INTEGER:
                oprintf("%i", v->constval.vint);
                break;
#endif
            case TYPE_POINTER:
                oprintf("&%s",
                    v->constval.vpointer->name);
                break;
        }
    } else {
        oprintf("%s", v->name);
    }
}

void ir_value_dump_life(const ir_value *self, int (*oprintf)(const char*,...))
{
    size_t i;
    oprintf("Life of %12s:", self->name);
    for (i = 0; i < vec_size(self->life); ++i)
    {
        oprintf(" + [%i, %i]\n", self->life[i].start, self->life[i].end);
    }
}
