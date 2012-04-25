#include <stdlib.h>
#include <string.h>
#include "ir.h"

/***********************************************************************
 *IR Builder
 */

ir_builder* ir_builder_new(const char *modulename)
{
    ir_builder* self;

    self = (ir_builder*)mem_a(sizeof(*self));
    MEM_VECTOR_INIT(self, functions);
    MEM_VECTOR_INIT(self, globals);
    self->name = NULL;
    ir_builder_set_name(self, modulename);

    /* globals which always exist */

    /* for now we give it a vector size */
    ir_builder_create_global(self, "OFS_RETURN", qc_variant);

    return self;
}

MEM_VEC_FUNCTIONS(ir_builder, ir_value*, globals)
MEM_VECTOR_FUNCTIONS(ir_builder, ir_function*, functions)

void ir_builder_delete(ir_builder* self)
{
    size_t i;
    mem_d((void*)self->name);
    for (i = 0; i != self->functions_count; ++i) {
        ir_function_delete(self->functions[i]);
    }
    MEM_VECTOR_CLEAR(self, functions);
    for (i = 0; i != self->globals_count; ++i) {
        ir_value_delete(self->globals[i]);
    }
    MEM_VECTOR_CLEAR(self, globals);
    mem_d(self);
}

void ir_builder_set_name(ir_builder *self, const char *name)
{
    if (self->name)
        mem_d((void*)self->name);
    self->name = util_strdup(name);
}

ir_function* ir_builder_get_function(ir_builder *self, const char *name)
{
    size_t i;
    for (i = 0; i < self->functions_count; ++i) {
        if (!strcmp(name, self->functions[i]->name))
            return self->functions[i];
    }
    return NULL;
}

ir_function* ir_builder_create_function(ir_builder *self, const char *name)
{
    ir_function *fn = ir_builder_get_function(self, name);
    if (fn) {
        return NULL;
    }

    fn = ir_function_new(self);
    ir_function_set_name(fn, name);
    ir_builder_functions_add(self, fn);
    return fn;
}

ir_value* ir_builder_get_global(ir_builder *self, const char *name)
{
    size_t i;
    for (i = 0; i < self->globals_count; ++i) {
        if (!strcmp(self->globals[i]->name, name))
            return self->globals[i];
    }
    return NULL;
}

ir_value* ir_builder_create_global(ir_builder *self, const char *name, int vtype)
{
    ir_value *ve = ir_builder_get_global(self, name);
    if (ve) {
        return NULL;
    }

    ve = ir_value_var(name, qc_global, vtype);
    ir_builder_globals_add(self, ve);
    return ve;
}

/***********************************************************************
 *IR Function
 */

void ir_function_naive_phi(ir_function*);
void ir_function_enumerate(ir_function*);
void ir_function_calculate_liferanges(ir_function*);

ir_function* ir_function_new(ir_builder* owner)
{
    ir_function *self;
    self = (ir_function*)mem_a(sizeof(*self));
    self->owner = owner;
    self->context.file = "<@no context>";
    self->context.line = 0;
    self->retype = qc_void;
    MEM_VECTOR_INIT(self, params);
    MEM_VECTOR_INIT(self, blocks);
    MEM_VECTOR_INIT(self, values);
    MEM_VECTOR_INIT(self, locals);
    ir_function_set_name(self, "<@unnamed>");

    self->run_id = 0;
    return self;
}
MEM_VECTOR_FUNCTIONS(ir_function, ir_value*, values)
MEM_VECTOR_FUNCTIONS(ir_function, ir_block*, blocks)
MEM_VECTOR_FUNCTIONS(ir_function, ir_value*, locals)

void ir_function_set_name(ir_function *self, const char *name)
{
    if (self->name)
        mem_d((void*)self->name);
    self->name = util_strdup(name);
}

void ir_function_delete(ir_function *self)
{
    size_t i;
    mem_d((void*)self->name);

    for (i = 0; i != self->blocks_count; ++i)
        ir_block_delete(self->blocks[i]);
    MEM_VECTOR_CLEAR(self, blocks);

    MEM_VECTOR_CLEAR(self, params);

    for (i = 0; i != self->values_count; ++i)
        ir_value_delete(self->values[i]);
    MEM_VECTOR_CLEAR(self, values);

    for (i = 0; i != self->locals_count; ++i)
        ir_value_delete(self->locals[i]);
    MEM_VECTOR_CLEAR(self, locals);

    mem_d(self);
}

void ir_function_collect_value(ir_function *self, ir_value *v)
{
    ir_function_values_add(self, v);
}

ir_block* ir_function_create_block(ir_function *self, const char *label)
{
    ir_block* bn = ir_block_new(self, label);
    memcpy(&bn->context, &self->context, sizeof(self->context));
    ir_function_blocks_add(self, bn);
    return bn;
}

void ir_function_finalize(ir_function *self)
{
    ir_function_naive_phi(self);
    ir_function_enumerate(self);
    ir_function_calculate_liferanges(self);
}

ir_value* ir_function_get_local(ir_function *self, const char *name)
{
    size_t i;
    for (i = 0; i < self->locals_count; ++i) {
        if (!strcmp(self->locals[i]->name, name))
            return self->locals[i];
    }
    return NULL;
}

ir_value* ir_function_create_local(ir_function *self, const char *name, int vtype)
{
    ir_value *ve = ir_function_get_local(self, name);
    if (ve) {
        return NULL;
    }

    ve = ir_value_var(name, qc_localvar, vtype);
    ir_function_locals_add(self, ve);
    return ve;
}

/***********************************************************************
 *IR Block
 */

ir_block* ir_block_new(ir_function* owner, const char *name)
{
    ir_block *self;
    self = (ir_block*)mem_a(sizeof(*self));
    self->owner = owner;
    self->context.file = "<@no context>";
    self->context.line = 0;
    self->final = ifalse;
    MEM_VECTOR_INIT(self, instr);
    MEM_VECTOR_INIT(self, entries);
    MEM_VECTOR_INIT(self, exits);
    self->label = NULL;
    ir_block_set_label(self, name);

    self->eid = 0;
    self->is_return = ifalse;
    self->run_id = 0;
    MEM_VECTOR_INIT(self, living);
    return self;
}
MEM_VECTOR_FUNCTIONS(ir_block, ir_instr*, instr)
MEM_VECTOR_FUNCTIONS_ALL(ir_block, ir_block*, entries)
MEM_VECTOR_FUNCTIONS_ALL(ir_block, ir_block*, exits)
MEM_VECTOR_FUNCTIONS_ALL(ir_block, ir_value*, living)

void ir_block_delete(ir_block* self)
{
    size_t i;
    mem_d((void*)self->label);
    for (i = 0; i != self->instr_count; ++i)
        ir_instr_delete(self->instr[i]);
    MEM_VECTOR_CLEAR(self, instr);
    MEM_VECTOR_CLEAR(self, entries);
    MEM_VECTOR_CLEAR(self, exits);
    MEM_VECTOR_CLEAR(self, living);
    mem_d(self);
}

void ir_block_set_label(ir_block *self, const char *name)
{
    if (self->label)
        mem_d((void*)self->label);
    self->label = util_strdup(name);
}

/***********************************************************************
 *IR Instructions
 */

ir_instr* ir_instr_new(ir_block* owner, int op)
{
    ir_instr *self;
    self = (ir_instr*)mem_a(sizeof(*self));
    self->owner = owner;
    self->context.file = "<@no context>";
    self->context.line = 0;
    self->opcode = op;
    self->_ops[0] = NULL;
    self->_ops[1] = NULL;
    self->_ops[2] = NULL;
    self->bops[0] = NULL;
    self->bops[1] = NULL;
    MEM_VECTOR_INIT(self, phi);

    self->eid = 0;
    return self;
}
MEM_VECTOR_FUNCTIONS(ir_instr, ir_phi_entry_t, phi)

void ir_instr_delete(ir_instr *self)
{
    ir_instr_op(self, 0, NULL, ifalse);
    ir_instr_op(self, 1, NULL, ifalse);
    ir_instr_op(self, 2, NULL, ifalse);
    MEM_VECTOR_CLEAR(self, phi);
    mem_d(self);
}

void ir_instr_op(ir_instr *self, int op, ir_value *v, qbool writing)
{
    if (self->_ops[op]) {
        if (writing)
            ir_value_writes_add(self->_ops[op], self);
        else
            ir_value_reads_add(self->_ops[op], self);
    }
    if (v) {
        if (writing)
            ir_value_writes_add(v, self);
        else
            ir_value_reads_add(v, self);
    }
    self->_ops[op] = v;
}

/***********************************************************************
 *IR Value
 */

ir_value* ir_value_var(const char *name, int storetype, int vtype)
{
    ir_value *self;
    self = (ir_value*)mem_a(sizeof(*self));
    self->vtype = vtype;
    self->store = storetype;
    MEM_VECTOR_INIT(self, reads);
    MEM_VECTOR_INIT(self, writes);
    self->has_constval = ifalse;
    self->context.file = "<@no context>";
    self->context.line = 0;
    self->name = NULL;
    ir_value_set_name(self, name);

    MEM_VECTOR_INIT(self, life);
    return self;
}
MEM_VECTOR_FUNCTIONS(ir_value, ir_life_entry_t, life)
MEM_VECTOR_FUNCTIONS(ir_value, ir_instr*, reads)
MEM_VECTOR_FUNCTIONS(ir_value, ir_instr*, writes)

ir_value* ir_value_out(ir_function *owner, const char *name, int storetype, int vtype)
{
    ir_value *v = ir_value_var(name, storetype, vtype);
    ir_function_collect_value(owner, v);
    return v;
}

void ir_value_delete(ir_value* self)
{
    mem_d((void*)self->name);
    if (self->has_constval)
    {
        if (self->vtype == qc_string)
            mem_d((void*)self->cvalue.vstring);
    }
    MEM_VECTOR_CLEAR(self, reads);
    MEM_VECTOR_CLEAR(self, writes);
    MEM_VECTOR_CLEAR(self, life);
    mem_d(self);
}

void ir_value_set_name(ir_value *self, const char *name)
{
    if (self->name)
        mem_d((void*)self->name);
    self->name = util_strdup(name);
}

qbool ir_value_set_float(ir_value *self, float f)
{
    if (self->vtype != qc_float)
        return ifalse;
    self->cvalue.vfloat = f;
    self->has_constval = itrue;
    return itrue;
}

qbool ir_value_set_vector(ir_value *self, qc_vec_t v)
{
    if (self->vtype != qc_vector)
        return ifalse;
    self->cvalue.vvec = v;
    self->has_constval = itrue;
    return itrue;
}

qbool ir_value_set_string(ir_value *self, const char *str)
{
    if (self->vtype != qc_string)
        return ifalse;
    self->cvalue.vstring = util_strdup(str);
    self->has_constval = itrue;
    return itrue;
}

qbool ir_value_set_int(ir_value *self, int i)
{
    if (self->vtype != qc_int)
        return ifalse;
    self->cvalue.vint = i;
    self->has_constval = itrue;
    return itrue;
}

qbool ir_value_lives(ir_value *self, size_t at)
{
    size_t i;
    for (i = 0; i < self->life_count; ++i)
    {
        ir_life_entry_t *life = &self->life[i];
        if (life->start <= at && at <= life->end)
            return itrue;
        if (life->start > at) /* since it's ordered */
            return ifalse;
    }
    return ifalse;
}

void ir_value_life_insert(ir_value *self, size_t idx, ir_life_entry_t e)
{
    size_t k;
    ir_value_life_add(self, e); /* naive... */
    for (k = self->life_count-1; k > idx; --k)
        self->life[k] = self->life[k-1];
    self->life[idx] = e;
}

qbool ir_value_life_merge(ir_value *self, size_t s)
{
    size_t i;
    ir_life_entry_t *life = NULL;
    ir_life_entry_t *before = NULL;
    ir_life_entry_t new_entry;

    /* Find the first range >= s */
    for (i = 0; i < self->life_count; ++i)
    {
        before = life;
        life = &self->life[i];
        if (life->start > s)
            break;
    }
    /* nothing found? append */
    if (i == self->life_count) {
        if (life && life->end+1 == s)
        {
            /* previous life range can be merged in */
            life->end++;
            return itrue;
        }
        if (life && life->end >= s)
            return ifalse;
        ir_life_entry_t e;
        e.start = e.end = s;
        ir_value_life_add(self, e);
        return itrue;
    }
    /* found */
    if (before)
    {
        if (before->end + 1 == s &&
            life->start - 1 == s)
        {
            /* merge */
            before->end = life->end;
            ir_value_life_remove(self, i);
            return itrue;
        }
        if (before->end + 1 == s)
        {
            /* extend before */
            before->end++;
            return itrue;
        }
        /* already contained */
        if (before->end >= s)
            return ifalse;
    }
    /* extend */
    if (life->start - 1 == s)
    {
        life->start--;
        return itrue;
    }
    /* insert a new entry */
    new_entry.start = new_entry.end = s;
    ir_value_life_insert(self, i, new_entry);
    return itrue;
}

/***********************************************************************
 *IR main operations
 */

qbool ir_block_create_store_op(ir_block *self, int op, ir_value *target, ir_value *what)
{
    if (target->store == qc_localval) {
        fprintf(stderr, "cannot store to an SSA value\n");
        return ifalse;
    } else {
        ir_instr *in = ir_instr_new(self, op);
        ir_instr_op(in, 0, target, itrue);
        ir_instr_op(in, 1, what, ifalse);
        ir_block_instr_add(self, in);
        return itrue;
    }
}

qbool ir_block_create_store(ir_block *self, ir_value *target, ir_value *what)
{
    int op = 0;
    int vtype;
    if (target->vtype == qc_variant)
        vtype = what->vtype;
    else
        vtype = target->vtype;

    switch (vtype) {
        case qc_float:
            if (what->vtype == qc_int)
                op = INSTR_CONV_ITOF;
            else
                op = INSTR_STORE_F;
            break;
        case qc_vector:
            op = INSTR_STORE_V;
            break;
        case qc_entity:
            op = INSTR_STORE_ENT;
            break;
        case qc_string:
            op = INSTR_STORE_S;
            break;
        case qc_int:
            if (what->vtype == qc_int)
                op = INSTR_CONV_FTOI;
            else
                op = INSTR_STORE_I;
            break;
        case qc_pointer:
            op = INSTR_STORE_I;
            break;
    }
    return ir_block_create_store_op(self, op, target, what);
}

void ir_block_create_return(ir_block *self, ir_value *v)
{
    ir_instr *in;
    if (self->final) {
        fprintf(stderr, "block already ended (%s)\n", self->_label);
        return;
    }
    self->final = itrue;
    self->is_return = itrue;
    in = ir_instr_new(self, INSTR_RETURN);
    ir_instr_op(in, 0, v, ifalse);
    ir_block_instr_add(self, in);
}

void ir_block_create_if(ir_block *self, ir_value *v,
                        ir_block *ontrue, ir_block *onfalse)
{
    ir_instr *in;
    if (self->final) {
        fprintf(stderr, "block already ended (%s)\n", self->_label);
        return;
    }
    self->final = itrue;
    //in = ir_instr_new(self, (v->vtype == qc_string ? INSTR_IF_S : INSTR_IF_F));
    in = ir_instr_new(self, VINSTR_COND);
    ir_instr_op(in, 0, v, ifalse);
    in->bops[0] = ontrue;
    in->bops[1] = onfalse;
    ir_block_instr_add(self, in);

    ir_block_exits_add(self, ontrue);
    ir_block_exits_add(self, onfalse);
    ir_block_entries_add(ontrue, self);
    ir_block_entries_add(onfalse, self);
}

void ir_block_create_jump(ir_block *self, ir_block *to)
{
    ir_instr *in;
    if (self->final) {
        fprintf(stderr, "block already ended (%s)\n", self->_label);
        return;
    }
    self->final = itrue;
    in = ir_instr_new(self, VINSTR_JUMP);
    in->bops[0] = to;
    ir_block_instr_add(self, in);

    ir_block_exits_add(self, to);
    ir_block_entries_add(to, self);
}

void ir_block_create_goto(ir_block *self, ir_block *to)
{
    ir_instr *in;
    if (self->final) {
        fprintf(stderr, "block already ended (%s)\n", self->_label);
        return;
    }
    self->final = itrue;
    in = ir_instr_new(self, INSTR_GOTO);
    in->bops[0] = to;
    ir_block_instr_add(self, in);

    ir_block_exits_add(self, to);
    ir_block_entries_add(to, self);
}

ir_instr* ir_block_create_phi(ir_block *self, const char *label, int ot)
{
    ir_value *out;
    ir_instr *in;
    in = ir_instr_new(self, VINSTR_PHI);
    out = ir_value_out(self->owner, label, qc_localval, ot);
    ir_instr_op(in, 0, out, itrue);
    ir_block_instr_add(self, in);
    return in;
}

ir_value* ir_phi_value(ir_instr *self)
{
    return self->_ops[0];
}

void ir_phi_add(ir_instr* self, ir_block *b, ir_value *v)
{
    ir_phi_entry_t pe;

    if (!ir_block_entries_find(self->owner, b, NULL)) {
        /* Must not be possible to cause this, otherwise the AST
         * is doing something wrong.
         */
        fprintf(stderr, "Invalid entry block for PHI\n");
        abort();
    }

    pe.value = v;
    pe.from = b;
    ir_value_reads_add(v, self);
    ir_instr_phi_add(self, pe);
}
