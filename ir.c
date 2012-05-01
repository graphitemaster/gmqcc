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
 *IR Builder
 */

ir_builder* ir_builder_new(const char *modulename)
{
    ir_builder* self;

    self = (ir_builder*)mem_a(sizeof(*self));
    MEM_VECTOR_INIT(self, functions);
    MEM_VECTOR_INIT(self, globals);
    self->name = NULL;
    if (!ir_builder_set_name(self, modulename)) {
        mem_d(self);
        return NULL;
    }

    /* globals which always exist */

    /* for now we give it a vector size */
    ir_builder_create_global(self, "OFS_RETURN", TYPE_VARIANT);

    return self;
}

MEM_VEC_FUNCTIONS(ir_builder, ir_value*, globals)
MEM_VEC_FUNCTIONS(ir_builder, ir_function*, functions)

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

bool ir_builder_set_name(ir_builder *self, const char *name)
{
    if (self->name)
        mem_d((void*)self->name);
    self->name = util_strdup(name);
    return !!self->name;
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
    if (!ir_function_set_name(fn, name) ||
        !ir_builder_functions_add(self, fn) )
    {
        ir_function_delete(fn);
        return NULL;
    }
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

    ve = ir_value_var(name, store_global, vtype);
    if (!ir_builder_globals_add(self, ve)) {
        ir_value_delete(ve);
        return NULL;
    }
    return ve;
}

/***********************************************************************
 *IR Function
 */

bool ir_function_naive_phi(ir_function*);
void ir_function_enumerate(ir_function*);
bool ir_function_calculate_liferanges(ir_function*);

ir_function* ir_function_new(ir_builder* owner)
{
    ir_function *self;
    self = (ir_function*)mem_a(sizeof(*self));
    self->name = NULL;
    if (!ir_function_set_name(self, "<@unnamed>")) {
        mem_d(self);
        return NULL;
    }
    self->owner = owner;
    self->context.file = "<@no context>";
    self->context.line = 0;
    self->retype = TYPE_VOID;
    MEM_VECTOR_INIT(self, params);
    MEM_VECTOR_INIT(self, blocks);
    MEM_VECTOR_INIT(self, values);
    MEM_VECTOR_INIT(self, locals);

    self->run_id = 0;
    return self;
}
MEM_VEC_FUNCTIONS(ir_function, ir_value*, values)
MEM_VEC_FUNCTIONS(ir_function, ir_block*, blocks)
MEM_VEC_FUNCTIONS(ir_function, ir_value*, locals)

bool ir_function_set_name(ir_function *self, const char *name)
{
    if (self->name)
        mem_d((void*)self->name);
    self->name = util_strdup(name);
    return !!self->name;
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

bool GMQCC_WARN ir_function_collect_value(ir_function *self, ir_value *v)
{
    return ir_function_values_add(self, v);
}

ir_block* ir_function_create_block(ir_function *self, const char *label)
{
    ir_block* bn = ir_block_new(self, label);
    memcpy(&bn->context, &self->context, sizeof(self->context));
    if (!ir_function_blocks_add(self, bn)) {
        ir_block_delete(bn);
        return NULL;
    }
    return bn;
}

bool ir_function_finalize(ir_function *self)
{
    if (!ir_function_naive_phi(self))
        return false;

    ir_function_enumerate(self);

    if (!ir_function_calculate_liferanges(self))
        return false;
    return true;
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

    ve = ir_value_var(name, store_local, vtype);
    if (!ir_function_locals_add(self, ve)) {
        ir_value_delete(ve);
        return NULL;
    }
    return ve;
}

/***********************************************************************
 *IR Block
 */

ir_block* ir_block_new(ir_function* owner, const char *name)
{
    ir_block *self;
    self = (ir_block*)mem_a(sizeof(*self));
    self->label = NULL;
    if (!ir_block_set_label(self, name)) {
        mem_d(self);
        return NULL;
    }
    self->owner = owner;
    self->context.file = "<@no context>";
    self->context.line = 0;
    self->final = false;
    MEM_VECTOR_INIT(self, instr);
    MEM_VECTOR_INIT(self, entries);
    MEM_VECTOR_INIT(self, exits);

    self->eid = 0;
    self->is_return = false;
    self->run_id = 0;
    MEM_VECTOR_INIT(self, living);
    return self;
}
MEM_VEC_FUNCTIONS(ir_block, ir_instr*, instr)
MEM_VEC_FUNCTIONS_ALL(ir_block, ir_block*, entries)
MEM_VEC_FUNCTIONS_ALL(ir_block, ir_block*, exits)
MEM_VEC_FUNCTIONS_ALL(ir_block, ir_value*, living)

void ir_block_delete(ir_block* self)
{
    size_t i;
    mem_d(self->label);
    for (i = 0; i != self->instr_count; ++i)
        ir_instr_delete(self->instr[i]);
    MEM_VECTOR_CLEAR(self, instr);
    MEM_VECTOR_CLEAR(self, entries);
    MEM_VECTOR_CLEAR(self, exits);
    MEM_VECTOR_CLEAR(self, living);
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
MEM_VEC_FUNCTIONS(ir_instr, ir_phi_entry_t, phi)

void ir_instr_delete(ir_instr *self)
{
    size_t i;
    /* The following calls can only delete from
     * vectors, we still want to delete this instruction
     * so ignore the return value. Since with the warn_unused_result attribute
     * gcc doesn't care about an explicit: (void)foo(); to ignore the result,
     * I have to improvise here and use if(foo());
     */
    for (i = 0; i < self->phi_count; ++i) {
        size_t idx;
        if (ir_value_writes_find(self->phi[i].value, self, &idx))
            if (ir_value_writes_remove(self->phi[i].value, idx)) GMQCC_SUPRESS_EMPTY_BODY;
        if (ir_value_reads_find(self->phi[i].value, self, &idx))
            if (ir_value_reads_remove (self->phi[i].value, idx)) GMQCC_SUPRESS_EMPTY_BODY;
    }
    MEM_VECTOR_CLEAR(self, phi);
    if (ir_instr_op(self, 0, NULL, false)) GMQCC_SUPRESS_EMPTY_BODY;
    if (ir_instr_op(self, 1, NULL, false)) GMQCC_SUPRESS_EMPTY_BODY;
    if (ir_instr_op(self, 2, NULL, false)) GMQCC_SUPRESS_EMPTY_BODY;
    mem_d(self);
}

bool ir_instr_op(ir_instr *self, int op, ir_value *v, bool writing)
{
    if (self->_ops[op]) {
        size_t idx;
        if (writing && ir_value_writes_find(self->_ops[op], self, &idx))
        {
            if (!ir_value_writes_remove(self->_ops[op], idx))
                return false;
        }
        else if (ir_value_reads_find(self->_ops[op], self, &idx))
        {
            if (!ir_value_reads_remove(self->_ops[op], idx))
                return false;
        }
    }
    if (v) {
        if (writing) {
            if (!ir_value_writes_add(v, self))
                return false;
        } else {
            if (!ir_value_reads_add(v, self))
                return false;
        }
    }
    self->_ops[op] = v;
    return true;
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
    self->isconst = false;
    self->context.file = "<@no context>";
    self->context.line = 0;
    self->name = NULL;
    ir_value_set_name(self, name);

    MEM_VECTOR_INIT(self, life);
    return self;
}
MEM_VEC_FUNCTIONS(ir_value, ir_life_entry_t, life)
MEM_VEC_FUNCTIONS_ALL(ir_value, ir_instr*, reads)
MEM_VEC_FUNCTIONS_ALL(ir_value, ir_instr*, writes)

ir_value* ir_value_out(ir_function *owner, const char *name, int storetype, int vtype)
{
    ir_value *v = ir_value_var(name, storetype, vtype);
    if (!v)
        return NULL;
    if (!ir_function_collect_value(owner, v))
    {
        ir_value_delete(v);
        return NULL;
    }
    return v;
}

void ir_value_delete(ir_value* self)
{
    mem_d((void*)self->name);
    if (self->isconst)
    {
        if (self->vtype == TYPE_STRING)
            mem_d((void*)self->constval.vstring);
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

bool ir_value_set_float(ir_value *self, float f)
{
    if (self->vtype != TYPE_FLOAT)
        return false;
    self->constval.vfloat = f;
    self->isconst = true;
    return true;
}

bool ir_value_set_vector(ir_value *self, vector v)
{
    if (self->vtype != TYPE_VECTOR)
        return false;
    self->constval.vvec = v;
    self->isconst = true;
    return true;
}

bool ir_value_set_string(ir_value *self, const char *str)
{
    if (self->vtype != TYPE_STRING)
        return false;
    self->constval.vstring = util_strdup(str);
    self->isconst = true;
    return true;
}

#if 0
bool ir_value_set_int(ir_value *self, int i)
{
    if (self->vtype != TYPE_INTEGER)
        return false;
    self->constval.vint = i;
    self->isconst = true;
    return true;
}
#endif

bool ir_value_lives(ir_value *self, size_t at)
{
    size_t i;
    for (i = 0; i < self->life_count; ++i)
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
    if (!ir_value_life_add(self, e)) /* naive... */
        return false;
    for (k = self->life_count-1; k > idx; --k)
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
    for (i = 0; i < self->life_count; ++i)
    {
        before = life;
        life = &self->life[i];
        if (life->start > s)
            break;
    }
    /* nothing found? append */
    if (i == self->life_count) {
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
        if (!ir_value_life_add(self, e))
            return false; /* failing */
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
            if (!ir_value_life_remove(self, i))
                return false; /* failing */
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

bool ir_values_overlap(ir_value *a, ir_value *b)
{
    /* For any life entry in A see if it overlaps with
     * any life entry in B.
     * Note that the life entries are orderes, so we can make a
     * more efficient algorithm there than naively translating the
     * statement above.
     */

    ir_life_entry_t *la, *lb, *enda, *endb;

    /* first of all, if either has no life range, they cannot clash */
    if (!a->life_count || !b->life_count)
        return false;

    la = a->life;
    lb = b->life;
    enda = la + a->life_count;
    endb = lb + b->life_count;
    while (true)
    {
        /* check if the entries overlap, for that,
         * both must start before the other one ends.
         */
        if (la->start <= lb->end &&
            lb->start <= la->end)
        {
            return true;
        }

        /* entries are ordered
         * one entry is earlier than the other
         * that earlier entry will be moved forward
         */
        if (la->end < lb->end)
        {
            /* order: A B, move A forward
             * check if we hit the end with A
             */
            if (++la == enda)
                break;
        }
        else if (lb->end < la->end)
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

bool ir_block_create_store_op(ir_block *self, int op, ir_value *target, ir_value *what)
{
    if (target->store == store_value) {
        fprintf(stderr, "cannot store to an SSA value\n");
        return false;
    } else {
        ir_instr *in = ir_instr_new(self, op);
        if (!in)
            return false;
        if (!ir_instr_op(in, 0, target, true) ||
            !ir_instr_op(in, 1, what, false)  ||
            !ir_block_instr_add(self, in) )
        {
            return false;
        }
        return true;
    }
}

bool ir_block_create_store(ir_block *self, ir_value *target, ir_value *what)
{
    int op = 0;
    int vtype;
    if (target->vtype == TYPE_VARIANT)
        vtype = what->vtype;
    else
        vtype = target->vtype;

    switch (vtype) {
        case TYPE_FLOAT:
#if 0
            if (what->vtype == TYPE_INTEGER)
                op = INSTR_CONV_ITOF;
            else
#endif
                op = INSTR_STORE_F;
            break;
        case TYPE_VECTOR:
            op = INSTR_STORE_V;
            break;
        case TYPE_ENTITY:
            op = INSTR_STORE_ENT;
            break;
        case TYPE_STRING:
            op = INSTR_STORE_S;
            break;
        case TYPE_FIELD:
            op = INSTR_STORE_FLD;
            break;
#if 0
        case TYPE_INTEGER:
            if (what->vtype == TYPE_INTEGER)
                op = INSTR_CONV_FTOI;
            else
                op = INSTR_STORE_I;
            break;
#endif
        case TYPE_POINTER:
#if 0
            op = INSTR_STORE_I;
#else
            op = INSTR_STORE_ENT;
#endif
            break;
        default:
            /* Unknown type */
            return false;
    }
    return ir_block_create_store_op(self, op, target, what);
}

bool ir_block_create_storep(ir_block *self, ir_value *target, ir_value *what)
{
    int op = 0;
    int vtype;

    if (target->vtype != TYPE_POINTER)
        return false;

    /* storing using pointer - target is a pointer, type must be
     * inferred from source
     */
    vtype = what->vtype;

    switch (vtype) {
        case TYPE_FLOAT:
            op = INSTR_STOREP_F;
            break;
        case TYPE_VECTOR:
            op = INSTR_STOREP_V;
            break;
        case TYPE_ENTITY:
            op = INSTR_STOREP_ENT;
            break;
        case TYPE_STRING:
            op = INSTR_STOREP_S;
            break;
        case TYPE_FIELD:
            op = INSTR_STOREP_FLD;
            break;
#if 0
        case TYPE_INTEGER:
            op = INSTR_STOREP_I;
            break;
#endif
        case TYPE_POINTER:
#if 0
            op = INSTR_STOREP_I;
#else
            op = INSTR_STOREP_ENT;
#endif
            break;
        default:
            /* Unknown type */
            return false;
    }
    return ir_block_create_store_op(self, op, target, what);
}

bool ir_block_create_return(ir_block *self, ir_value *v)
{
    ir_instr *in;
    if (self->final) {
        fprintf(stderr, "block already ended (%s)\n", self->label);
        return false;
    }
    self->final = true;
    self->is_return = true;
    in = ir_instr_new(self, INSTR_RETURN);
    if (!in)
        return false;

    if (!ir_instr_op(in, 0, v, false) ||
        !ir_block_instr_add(self, in) )
    {
        return false;
    }
    return true;
}

bool ir_block_create_if(ir_block *self, ir_value *v,
                        ir_block *ontrue, ir_block *onfalse)
{
    ir_instr *in;
    if (self->final) {
        fprintf(stderr, "block already ended (%s)\n", self->label);
        return false;
    }
    self->final = true;
    /*in = ir_instr_new(self, (v->vtype == TYPE_STRING ? INSTR_IF_S : INSTR_IF_F));*/
    in = ir_instr_new(self, VINSTR_COND);
    if (!in)
        return false;

    if (!ir_instr_op(in, 0, v, false)) {
        ir_instr_delete(in);
        return false;
    }

    in->bops[0] = ontrue;
    in->bops[1] = onfalse;

    if (!ir_block_instr_add(self, in))
        return false;

    if (!ir_block_exits_add(self, ontrue)    ||
        !ir_block_exits_add(self, onfalse)   ||
        !ir_block_entries_add(ontrue, self)  ||
        !ir_block_entries_add(onfalse, self) )
    {
        return false;
    }
    return true;
}

bool ir_block_create_jump(ir_block *self, ir_block *to)
{
    ir_instr *in;
    if (self->final) {
        fprintf(stderr, "block already ended (%s)\n", self->label);
        return false;
    }
    self->final = true;
    in = ir_instr_new(self, VINSTR_JUMP);
    if (!in)
        return false;

    in->bops[0] = to;
    if (!ir_block_instr_add(self, in))
        return false;

    if (!ir_block_exits_add(self, to) ||
        !ir_block_entries_add(to, self) )
    {
        return false;
    }
    return true;
}

bool ir_block_create_goto(ir_block *self, ir_block *to)
{
    ir_instr *in;
    if (self->final) {
        fprintf(stderr, "block already ended (%s)\n", self->label);
        return false;
    }
    self->final = true;
    in = ir_instr_new(self, INSTR_GOTO);
    if (!in)
        return false;

    in->bops[0] = to;
    if (!ir_block_instr_add(self, in))
        return false;

    if (!ir_block_exits_add(self, to) ||
        !ir_block_entries_add(to, self) )
    {
        return false;
    }
    return true;
}

ir_instr* ir_block_create_phi(ir_block *self, const char *label, int ot)
{
    ir_value *out;
    ir_instr *in;
    in = ir_instr_new(self, VINSTR_PHI);
    if (!in)
        return NULL;
    out = ir_value_out(self->owner, label, store_local, ot);
    if (!out) {
        ir_instr_delete(in);
        return NULL;
    }
    if (!ir_instr_op(in, 0, out, true)) {
        ir_instr_delete(in);
        ir_value_delete(out);
        return NULL;
    }
    if (!ir_block_instr_add(self, in)) {
        ir_instr_delete(in);
        ir_value_delete(out);
        return NULL;
    }
    return in;
}

ir_value* ir_phi_value(ir_instr *self)
{
    return self->_ops[0];
}

bool ir_phi_add(ir_instr* self, ir_block *b, ir_value *v)
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
    if (!ir_value_reads_add(v, self))
        return false;
    return ir_instr_phi_add(self, pe);
}

/* binary op related code */

ir_value* ir_block_create_binop(ir_block *self,
                                const char *label, int opcode,
                                ir_value *left, ir_value *right)
{
    ir_value *out = NULL;
    ir_instr *in  = NULL;

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

    out = ir_value_out(self->owner, label, store_local, ot);
    if (!out)
        return NULL;

    in = ir_instr_new(self, opcode);
    if (!in) {
        ir_value_delete(out);
        return NULL;
    }

    if (!ir_instr_op(in, 0, out, true) ||
        !ir_instr_op(in, 1, left, false) ||
        !ir_instr_op(in, 2, right, false) )
    {
        goto on_error;
    }

    if (!ir_block_instr_add(self, in))
        goto on_error;

    return out;
on_error:
    ir_instr_delete(in);
    ir_value_delete(out);
    return NULL;
}

ir_value* ir_block_create_load_from_ent(ir_block *self, const char *label, ir_value *ent, ir_value *field, int outype)
{
    ir_instr *instr;
    ir_value *out;
    int       op;

    if (ent->vtype != TYPE_ENTITY)
        return NULL;

    /* at some point we could redirect for TYPE_POINTER... but that could lead to carelessness */
    if (field->vtype != TYPE_FIELD)
        return NULL;

    switch (outype)
    {
        case TYPE_FLOAT:   op = INSTR_LOAD_F;   break;
        case TYPE_VECTOR:  op = INSTR_LOAD_V;   break;
        case TYPE_STRING:  op = INSTR_LOAD_S;   break;
        case TYPE_FIELD:   op = INSTR_LOAD_FLD; break;
        case TYPE_ENTITY:  op = INSTR_LOAD_ENT; break;
#if 0
        case TYPE_POINTER: op = INSTR_LOAD_I;   break;
        case TYPE_INTEGER: op = INSTR_LOAD_I;   break;
#endif
        default:
            return NULL;
    }

    out = ir_value_out(self->owner, label, store_local, outype);
    if (!out)
        return NULL;

    instr = ir_instr_new(self, op);
    if (!instr) {
        ir_value_delete(out);
        return NULL;
    }

    if (!ir_instr_op(instr, 0, out, true) ||
        !ir_instr_op(instr, 0, ent, false) ||
        !ir_instr_op(instr, 0, field, false))
    {
        goto on_error;
    }

    if (!ir_block_instr_add(self, instr))
        goto on_error;

    return out;
on_error:
    ir_instr_delete(instr);
    ir_value_delete(out);
    return NULL;
}

ir_value* ir_block_create_add(ir_block *self,
                              const char *label,
                              ir_value *left, ir_value *right)
{
    int op = 0;
    int l = left->vtype;
    int r = right->vtype;
    if (l == r) {
        switch (l) {
            default:
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
            return NULL;
    }
    return ir_block_create_binop(self, label, op, left, right);
}

ir_value* ir_block_create_sub(ir_block *self,
                              const char *label,
                              ir_value *left, ir_value *right)
{
    int op = 0;
    int l = left->vtype;
    int r = right->vtype;
    if (l == r) {

        switch (l) {
            default:
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
            return NULL;
    }
    return ir_block_create_binop(self, label, op, left, right);
}

ir_value* ir_block_create_mul(ir_block *self,
                              const char *label,
                              ir_value *left, ir_value *right)
{
    int op = 0;
    int l = left->vtype;
    int r = right->vtype;
    if (l == r) {

        switch (l) {
            default:
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
        else
            return NULL;
    }
    return ir_block_create_binop(self, label, op, left, right);
}

ir_value* ir_block_create_div(ir_block *self,
                              const char *label,
                              ir_value *left, ir_value *right)
{
    int op = 0;
    int l = left->vtype;
    int r = right->vtype;
    if (l == r) {

        switch (l) {
            default:
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
            return NULL;
    }
    return ir_block_create_binop(self, label, op, left, right);
}

/* PHI resolving breaks the SSA, and must thus be the last
 * step before life-range calculation.
 */

static bool ir_block_naive_phi(ir_block *self);
bool ir_function_naive_phi(ir_function *self)
{
    size_t i;

    for (i = 0; i < self->blocks_count; ++i)
    {
        if (!ir_block_naive_phi(self->blocks[i]))
            return false;
    }
    return true;
}

static bool ir_naive_phi_emit_store(ir_block *block, size_t iid, ir_value *old, ir_value *what)
{
    ir_instr *instr;
    size_t i;

    /* create a store */
    if (!ir_block_create_store(block, old, what))
        return false;

    /* we now move it up */
    instr = block->instr[block->instr_count-1];
    for (i = block->instr_count; i > iid; --i)
        block->instr[i] = block->instr[i-1];
    block->instr[i] = instr;

    return true;
}

static bool ir_block_naive_phi(ir_block *self)
{
    size_t i, p, w;
    /* FIXME: optionally, create_phi can add the phis
     * to a list so we don't need to loop through blocks
     * - anyway: "don't optimize YET"
     */
    for (i = 0; i < self->instr_count; ++i)
    {
        ir_instr *instr = self->instr[i];
        if (instr->opcode != VINSTR_PHI)
            continue;

        if (!ir_block_instr_remove(self, i))
            return false;
        --i; /* NOTE: i+1 below */

        for (p = 0; p < instr->phi_count; ++p)
        {
            ir_value *v = instr->phi[p].value;
            for (w = 0; w < v->writes_count; ++w) {
                ir_value *old;

                if (!v->writes[w]->_ops[0])
                    continue;

                /* When the write was to a global, we have to emit a mov */
                old = v->writes[w]->_ops[0];

                /* The original instruction now writes to the PHI target local */
                if (v->writes[w]->_ops[0] == v)
                    v->writes[w]->_ops[0] = instr->_ops[0];

                if (old->store != store_local)
                {
                    /* If it originally wrote to a global we need to store the value
                     * there as welli
                     */
                    if (!ir_naive_phi_emit_store(self, i+1, old, v))
                        return false;
                    if (i+1 < self->instr_count)
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
                    for (r = 0; r < old->reads_count; ++r)
                    {
                        size_t op;
                        ir_instr *ri = old->reads[r];
                        for (op = 0; op < ri->phi_count; ++op) {
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

typedef struct
{
    ir_value* *v;
    size_t    v_count;
    size_t    v_alloc;
} new_reads_t;
MEM_VEC_FUNCTIONS_ALL(new_reads_t, ir_value*, v)

/* Enumerate instructions used by value's life-ranges
 */
static void ir_block_enumerate(ir_block *self, size_t *_eid)
{
    size_t i;
    size_t eid = *_eid;
    for (i = 0; i < self->instr_count; ++i)
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
    for (i = 0; i < self->blocks_count; ++i)
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
        for (i = 0; i != self->blocks_count; ++i)
        {
            if (self->blocks[i]->is_return)
            {
                if (!ir_block_life_propagate(self->blocks[i], NULL, &changed))
                    return false;
            }
        }
    } while (changed);
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
    for (i = 0; i != self->living_count; ++i)
    {
        tempbool = ir_value_life_merge(self->living[i], eid);
        /* debug
        if (tempbool)
            fprintf(stderr, "block_living_add_instr() value instruction added %s: %i\n", self->living[i]->_name, (int)eid);
        */
        changed = changed || tempbool;
    }
    return changed;
}

static bool ir_block_life_prop_previous(ir_block* self, ir_block *prev, bool *changed)
{
    size_t i;
    /* values which have been read in a previous iteration are now
     * in the "living" array even if the previous block doesn't use them.
     * So we have to remove whatever does not exist in the previous block.
     * They will be re-added on-read, but the liferange merge won't cause
     * a change.
     */
    for (i = 0; i < self->living_count; ++i)
    {
        if (!ir_block_living_find(prev, self->living[i], NULL)) {
            if (!ir_block_living_remove(self, i))
                return false;
            --i;
        }
    }

    /* Whatever the previous block still has in its living set
     * must now be added to ours as well.
     */
    for (i = 0; i < prev->living_count; ++i)
    {
        if (ir_block_living_find(self, prev->living[i], NULL))
            continue;
        if (!ir_block_living_add(self, prev->living[i]))
            return false;
        /*
        printf("%s got from prev: %s\n", self->label, prev->living[i]->_name);
        */
    }
    return true;
}

static bool ir_block_life_propagate(ir_block *self, ir_block *prev, bool *changed)
{
    ir_instr *instr;
    ir_value *value;
    bool  tempbool;
    size_t i, o, p, rd;
    /* bitmasks which operands are read from or written to */
    size_t read, write;
    new_reads_t new_reads;
    char dbg_ind[16] = { '#', '0' };
    (void)dbg_ind;

    MEM_VECTOR_INIT(&new_reads, v);

    if (prev)
    {
        if (!ir_block_life_prop_previous(self, prev, changed))
            return false;
    }

    i = self->instr_count;
    while (i)
    { --i;
        instr = self->instr[i];

        /* PHI operands are always read operands */
        for (p = 0; p < instr->phi_count; ++p)
        {
            value = instr->phi[p].value;
            /* used this before new_reads - puts the last read into the life range as well
            if (!ir_block_living_find(self, value, NULL))
                ir_block_living_add(self, value);
            */
            /* fprintf(stderr, "read: %s\n", value->_name); */
            if (!new_reads_t_v_find(&new_reads, value, NULL))
            {
                if (!new_reads_t_v_add(&new_reads, value))
                    goto on_error;
            }
        }

        /* See which operands are read and write operands */
        ir_op_read_write(instr->opcode, &read, &write);

        /* Go through the 3 main operands */
        for (o = 0; o < 3; ++o)
        {
            if (!instr->_ops[o]) /* no such operand */
                continue;

            value = instr->_ops[o];

            /* We only care about locals */
            if (value->store != store_value &&
                value->store != store_local)
                continue;

            /* read operands */
            if (read & (1<<o))
            {
                /* used this before new_reads - puts the last read into the life range as well
                if (!ir_block_living_find(self, value, NULL))
                    ir_block_living_add(self, value);
                */
                /* fprintf(stderr, "read: %s\n", value->_name); */
                if (!new_reads_t_v_find(&new_reads, value, NULL))
                {
                    if (!new_reads_t_v_add(&new_reads, value))
                        goto on_error;
                }
            }

            /* write operands */
            /* When we write to a local, we consider it "dead" for the
             * remaining upper part of the function, since in SSA a value
             * can only be written once (== created)
             */
            if (write & (1<<o))
            {
                size_t idx, readidx;
                bool in_living = ir_block_living_find(self, value, &idx);
                bool in_reads = new_reads_t_v_find(&new_reads, value, &readidx);
                if (!in_living && !in_reads)
                {
                    /* If the value isn't alive it hasn't been read before... */
                    /* TODO: See if the warning can be emitted during parsing or AST processing
                     * otherwise have warning printed here.
                     * IF printing a warning here: include filecontext_t,
                     * and make sure it's only printed once
                     * since this function is run multiple times.
                     */
                    /* For now: debug info: */
                    fprintf(stderr, "Value only written %s\n", value->name);
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
                        fprintf(stderr, "value added id %s %i\n", value->name, (int)instr->eid);
                    */
                    *changed = *changed || tempbool;
                    /* Then remove */
                    if (!ir_block_living_remove(self, idx))
                        goto on_error;
                    if (in_reads)
                    {
                        if (!new_reads_t_v_remove(&new_reads, readidx))
                            goto on_error;
                    }
                }
            }
        }
        /* (A) */
        tempbool = ir_block_living_add_instr(self, instr->eid);
        /*fprintf(stderr, "living added values\n");*/
        *changed = *changed || tempbool;

        /* new reads: */
        for (rd = 0; rd < new_reads.v_count; ++rd)
        {
            if (!ir_block_living_find(self, new_reads.v[rd], NULL)) {
                if (!ir_block_living_add(self, new_reads.v[rd]))
                    goto on_error;
            }
            if (!i && !self->entries_count) {
                /* fix the top */
                *changed = *changed || ir_value_life_merge(new_reads.v[rd], instr->eid);
            }
        }
        MEM_VECTOR_CLEAR(&new_reads, v);
    }

    if (self->run_id == self->owner->run_id)
        return true;

    self->run_id = self->owner->run_id;

    for (i = 0; i < self->entries_count; ++i)
    {
        ir_block *entry = self->entries[i];
        ir_block_life_propagate(entry, self, changed);
    }

    return true;
on_error:
    MEM_VECTOR_CLEAR(&new_reads, v);
    return false;
}

/***********************************************************************
 *IR DEBUG Dump functions...
 */

#define IND_BUFSZ 1024

const char *qc_opname(int op)
{
    if (op < 0) return "<INVALID>";
    if (op < ( sizeof(asm_instr) / sizeof(asm_instr[0]) ))
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
	for (i = 0; i < b->globals_count; ++i)
	{
		oprintf("global ");
		if (b->globals[i]->isconst)
			oprintf("%s = ", b->globals[i]->name);
		ir_value_dump(b->globals[i], oprintf);
		oprintf("\n");
	}
	for (i = 0; i < b->functions_count; ++i)
		ir_function_dump(b->functions[i], indent, oprintf);
	oprintf("endmodule %s\n", b->name);
}

void ir_function_dump(ir_function *f, char *ind,
                      int (*oprintf)(const char*, ...))
{
	size_t i;
	oprintf("%sfunction %s\n", ind, f->name);
	strncat(ind, "\t", IND_BUFSZ);
	if (f->locals_count)
	{
		oprintf("%s%i locals:\n", ind, (int)f->locals_count);
		for (i = 0; i < f->locals_count; ++i) {
			oprintf("%s\t", ind);
			ir_value_dump(f->locals[i], oprintf);
			oprintf("\n");
		}
	}
	if (f->blocks_count)
	{

		oprintf("%slife passes: %i\n", ind, (int)f->blocks[0]->run_id);
		for (i = 0; i < f->blocks_count; ++i)
			ir_block_dump(f->blocks[i], ind, oprintf);

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

	for (i = 0; i < b->instr_count; ++i)
		ir_instr_dump(b->instr[i], ind, oprintf);
	ind[strlen(ind)-1] = 0;
}

void dump_phi(ir_instr *in, char *ind,
              int (*oprintf)(const char*, ...))
{
	size_t i;
	oprintf("%s <- phi ", in->_ops[0]->name);
	for (i = 0; i < in->phi_count; ++i)
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
		dump_phi(in, ind, oprintf);
		return;
	}

	strncat(ind, "\t", IND_BUFSZ);

	if (in->_ops[0] && (in->_ops[1] || in->_ops[2])) {
		ir_value_dump(in->_ops[0], oprintf);
		if (in->_ops[1] || in->_ops[2])
			oprintf(" <- ");
	}
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
	oprintf("\n");
	ind[strlen(ind)-1] = 0;
}

void ir_value_dump(ir_value* v, int (*oprintf)(const char*, ...))
{
	if (v->isconst) {
		switch (v->vtype) {
			case TYPE_VOID:
				oprintf("(void)");
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
				oprintf("\"%s\"", v->constval.vstring);
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

void ir_value_dump_life(ir_value *self, int (*oprintf)(const char*,...))
{
	size_t i;
	oprintf("Life of %s:\n", self->name);
	for (i = 0; i < self->life_count; ++i)
	{
		oprintf(" + [%i, %i]\n", self->life[i].start, self->life[i].end);
	}
}
