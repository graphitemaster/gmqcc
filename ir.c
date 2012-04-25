#include <stdlib.h>
#include <string.h>
#include "ir.h"

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
	VEC_CLEAR(self, functions);
	for (i = 0; i != self->globals_count; ++i) {
		ir_value_delete(self->globals[i]);
	}
	VEC_CLEAR(self, globals);
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

ir_value* ir_builder_create_global(ir_builder *self, const char *name, ir_type_t vtype)
{
	ir_value *ve = ir_builder_get_global(self, name);
	if (ve) {
		return NULL;
	}

	ve = ir_value_var(name, qc_global, vtype);
	ir_builder_globals_add(self, ve);
	return ve;
}
