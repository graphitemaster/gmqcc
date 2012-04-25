#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ast.h"

#define ast_setfunc(me, fn, what) ( *(void**)&((me)->fn) = what )

#define ast_instantiate(T, ctx, destroyfn)                    \
    T *self = (T*)mem_a(sizeof(T));                           \
    ast_node_init((ast_node*)self, ctx);                      \
    ast_setfunc(&((ast_node*)self)->node, destroy, destroyfn)

/* It must not be possible to get here. */
static void _ast_node_destroy(ast_node *self)
{
    fprintf(stderr, "ast node missing destroy()\n");
    abort();
}

/* Initialize main ast node aprts */
static void ast_node_init(ast_node *self, lex_ctx_t ctx)
{
    self->node.context = ctx;
    self->node.destroy = &_ast_node_destroy;
}

/* General expression initialization */
static void ast_expression_init(ast_expression *self,
                                ast_expression_codegen *codegen)
{
    ast_setfunc(&self->expression, codegen, codegen);
}

ast_value* ast_value_new(lex_ctx_t ctx, const char *name, qc_type_t t)
{
    ast_instantiate(ast_value, ctx, ast_value_delete);
    ast_expression_init((ast_expression*)self,
                        (ast_expression_codegen*)&ast_value_codegen);

    self->name = name ? util_strdup(name) : NULL;
    self->vtype = t;
    self->next = NULL;
    MEM_VECTOR_INIT(self, params);
    self->has_constval = false;
    memset(&self->cvalue, 0, sizeof(self->cvalue));

    self->ir_v = NULL;

    return self;
}
MEM_VEC_FUNCTIONS(ast_value, ast_value*, params)

void ast_value_delete(ast_value* self)
{
    size_t i;
    if (self->name)
        mem_d((void*)self->name);
    for (i = 0; i < self->params_count; ++i)
        ast_delete(self->params[i]);
    MEM_VECTOR_CLEAR(self, params);
    if (self->next)
        ast_delete(self->next);
    if (self->has_constval) {
        switch (self->vtype)
        {
        case qc_string:
            mem_d((void*)self->cvalue.vstring);
            break;
        /* NOTE: delete function? currently collected in
         * the parser structure
         */
        default:
            break;
        }
    }
    mem_d(self);
}

void ast_value_set_name(ast_value *self, const char *name)
{
    if (self->name)
        mem_d((void*)self->name);
    self->name = util_strdup(name);
}

ast_binary* ast_binary_new(lex_ctx_t ctx, qc_op_t op,
                           ast_value* left, ast_value* right)
{
    ast_instantiate(ast_binary, ctx, ast_binary_delete);
    ast_expression_init((ast_expression*)self, (ast_expression_codegen*)codegen);

    self->op = op;
    self->left = left;
    self->right = right;

    return self;
}

void ast_binary_delete(ast_binary *self)
{
    mem_d(self);
}

ast_block* ast_block_new(lex_ctx_t ctx)
{
    ast_instantiate(ast_block, ctx, ast_block_delete);
    ast_expression_init((ast_expression*)self,
                        (ast_expression_codegen*)&ast_block_codegen);

    MEM_VECTOR_INIT(self, locals);
    MEM_VECTOR_INIT(self, exprs);

    return self;
}
MEM_VEC_FUNCTIONS(ast_block, ast_value*, locals)
MEM_VEC_FUNCTIONS(ast_block, ast_expression*, exprs)

void ast_block_delete(ast_block *self)
{
    size_t i;
    for (i = 0; i < self->locals_count; ++i)
        ast_delete(self->locals[i]);
    MEM_VECTOR_CLEAR(self, locals);
    for (i = 0; i < self->exprs_count; ++i)
        ast_delete(self->exprs[i]);
    MEM_VECTOR_CLEAR(self, exprs);
    mem_d(self);
}

ast_function* ast_function_new(lex_ctx_t ctx, const char *name, ast_value *vtype)
{
    ast_instantiate(ast_function, ctx, ast_function_delete);

    self->vtype = vtype;
    self->name = name ? util_strdup(name) : NULL;
    MEM_VECTOR_INIT(self, blocks);

    return self;
}

MEM_VEC_FUNCTIONS(ast_function, ast_block*, blocks)

void ast_function_delete(ast_function *self)
{
    size_t i;
    if (self->name)
        mem_d((void*)self->name);
    if (self->vtype)
        ast_value_delete(self->vtype);
    for (i = 0; i < self->blocks_count; ++i)
        ast_delete(self->blocks[i]);
    MEM_VECTOR_CLEAR(self, blocks);
    mem_d(self);
}

/*********************************************************************/
/* AST codegen aprt
 */

static qbool ast_value_gen_global(ir_builder *ir, ast_value *self, ir_value **out)
{
	ir_value *v;
	*out = NULL;

	/* Generate functions */
	if (self->vtype == qc_function && self->has_constval)
	{
		/* Without has_constval it would be invalid... function pointers actually have
		 * type qc_pointer and next with type qc_function
		 */
		ast_function *func = self->cvalue.vfunc;
		(void)func;
		if (!ast_function_codegen(func, ir))
			return false;

		/* Here we do return NULL anyway */
		return true;
	}
	else if (self->vtype == qc_function && !self->has_constval) {
		fprintf(stderr,
		"!v->has_constval <- qc_function body missing - FIXME: remove when implementing prototypes\n");
		fprintf(stderr, "Value: %s\n", self->_name);
		abort();
	}

	v = ir_builder_create_global(ir, self->_name, self->vtype);
	self->ir_v = v;

	*out = v;
	return true;
}

qbool ast_value_codegen(ast_value *self, ast_function *func, ir_value **out)
{
	if (!func)
		return ast_value_gen_global(parser.ir, self, out);
	return false;
}

qbool ast_function_codegen(ast_function *self, ir_builder *builder)
{
	size_t i;
	for (i = 0; i < self->blocks_count; ++i)
	{
		ast_expression *expr;
		ir_value *out;

		expr = (ast_expression*)self->blocks[i];

		if (!(expr->expression.codegen)(expr, self, &out))
		{
			/* there was an error while building this expression... */
			return false;
		}
		(void)out;
	}
	return true;
}

qbool ast_block_codegen(ast_block *self, ir_function *func, ir_value **out)
{
    return false;
}

qbool ast_bin_store_codegen(ast_binary *self, ir_function *func, ir_value **out)
{
    return false;
}
