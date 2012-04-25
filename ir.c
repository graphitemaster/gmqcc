#include <stdlib.h>
#include <string.h>
#include "ir.h"

ir_builder* ir_builder_new(const char *modulename)
{
	ir_builder* self;

	self = (ir_builder*)malloc(sizeof(*self));
	VEC_INIT(self, functions);
	VEC_INIT(self, globals);
	self->_name = NULL;
	ir_builder_set_name(self, modulename);

	/* globals which always exist */

	/* for now we give it a vector size */
	ir_builder_create_global(self, "OFS_RETURN", qc_variant);

	return self;
}

MAKE_VEC_ADD(ir_builder, ir_value*, globals)
MAKE_VEC_ADD(ir_builder, ir_function*, functions)

void ir_builder_delete(ir_builder* self)
{
	size_t i;
	free((void*)self->_name);
	for (i = 0; i != self->functions_count; ++i) {
		ir_function_delete(self->functions[i]);
	}
	VEC_CLEAR(self, functions);
	for (i = 0; i != self->globals_count; ++i) {
		ir_value_delete(self->globals[i]);
	}
	VEC_CLEAR(self, globals);
	free(self);
}

void ir_builder_set_name(ir_builder *self, const char *name)
{
	if (self->_name)
		free((void*)self->_name);
	self->_name = strdup(name);
}

ir_function* ir_builder_get_function(ir_builder *self, const char *name)
{
	size_t i;
	for (i = 0; i < self->functions_count; ++i) {
		if (!strcmp(name, self->functions[i]->_name))
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
		if (!strcmp(self->globals[i]->_name, name))
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
