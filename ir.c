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
