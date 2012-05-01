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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gmqcc.h"
#include "ast.h"

#define ast_instantiate(T, ctx, destroyfn)                          \
    T* self = (T*)mem_a(sizeof(T));                                 \
    if (!self) {                                                    \
        return NULL;                                                \
    }                                                               \
    ast_node_init((ast_node*)self, ctx);                            \
    ( (ast_node*)self )->node.destroy = (ast_node_delete*)destroyfn

/* It must not be possible to get here. */
static void _ast_node_destroy(ast_node *self)
{
    fprintf(stderr, "ast node missing destroy()\n");
    abort();
}

/* Initialize main ast node aprts */
static void ast_node_init(ast_node *self, lex_ctx ctx)
{
    self->node.context = ctx;
    self->node.destroy = &_ast_node_destroy;
    self->node.keep    = false;
}

/* General expression initialization */
static void ast_expression_init(ast_expression *self,
                                ast_expression_codegen *codegen)
{
    self->expression.codegen = codegen;
}

ast_value* ast_value_new(lex_ctx ctx, const char *name, int t)
{
    ast_instantiate(ast_value, ctx, ast_value_delete);
    ast_expression_init((ast_expression*)self,
                        (ast_expression_codegen*)&ast_value_codegen);
    self->expression.node.keep = true; /* keep */

    self->name = name ? util_strdup(name) : NULL;
    self->vtype = t;
    self->next = NULL;
    MEM_VECTOR_INIT(self, params);
    self->isconst = false;
    memset(&self->constval, 0, sizeof(self->constval));

    self->ir_v    = NULL;

    return self;
}
MEM_VEC_FUNCTIONS(ast_value, ast_value*, params)

void ast_value_delete(ast_value* self)
{
    size_t i;
    if (self->name)
        mem_d((void*)self->name);
    for (i = 0; i < self->params_count; ++i)
        ast_value_delete(self->params[i]); /* delete, the ast_function is expected to die first */
    MEM_VECTOR_CLEAR(self, params);
    if (self->next) /* delete, not unref, types are always copied */
        ast_delete(self->next);
    if (self->isconst) {
        switch (self->vtype)
        {
        case TYPE_STRING:
            mem_d((void*)self->constval.vstring);
            break;
        case TYPE_FUNCTION:
            /* unlink us from the function node */
            self->constval.vfunc->vtype = NULL;
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

bool ast_value_set_name(ast_value *self, const char *name)
{
    if (self->name)
        mem_d((void*)self->name);
    self->name = util_strdup(name);
    return !!self->name;
}

ast_binary* ast_binary_new(lex_ctx ctx, int op,
                           ast_expression* left, ast_expression* right)
{
    ast_instantiate(ast_binary, ctx, ast_binary_delete);
    ast_expression_init((ast_expression*)self, (ast_expression_codegen*)&ast_binary_codegen);

    self->op = op;
    self->left = left;
    self->right = right;

    return self;
}

void ast_binary_delete(ast_binary *self)
{
    ast_unref(self->left);
    ast_unref(self->right);
    mem_d(self);
}

ast_entfield* ast_entfield_new(lex_ctx ctx, ast_expression *entity, ast_expression *field)
{
    ast_instantiate(ast_entfield, ctx, ast_entfield_delete);
    ast_expression_init((ast_expression*)self, (ast_expression_codegen*)&ast_entfield_codegen);

    self->entity = entity;
    self->field  = field;

    return self;
}

void ast_entfield_delete(ast_entfield *self)
{
    ast_unref(self->entity);
    ast_unref(self->field);
    mem_d(self);
}

ast_store* ast_store_new(lex_ctx ctx, int op,
                         ast_value *dest, ast_expression *source)
{
    ast_instantiate(ast_store, ctx, ast_store_delete);
    ast_expression_init((ast_expression*)self, (ast_expression_codegen*)&ast_store_codegen);

    self->op = op;
    self->dest = dest;
    self->source = source;

    return self;
}

void ast_store_delete(ast_store *self)
{
    ast_unref(self->dest);
    ast_unref(self->source);
    mem_d(self);
}

ast_block* ast_block_new(lex_ctx ctx)
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
    for (i = 0; i < self->exprs_count; ++i)
        ast_unref(self->exprs[i]);
    MEM_VECTOR_CLEAR(self, exprs);
    for (i = 0; i < self->locals_count; ++i)
        ast_delete(self->locals[i]);
    MEM_VECTOR_CLEAR(self, locals);
    mem_d(self);
}

ast_function* ast_function_new(lex_ctx ctx, const char *name, ast_value *vtype)
{
    ast_instantiate(ast_function, ctx, ast_function_delete);

    if (!vtype ||
        vtype->isconst ||
        vtype->vtype != TYPE_FUNCTION)
    {
        mem_d(self);
        return NULL;
    }

    self->vtype = vtype;
    self->name = name ? util_strdup(name) : NULL;
    MEM_VECTOR_INIT(self, blocks);

    self->ir_func = NULL;

    vtype->isconst = true;
    vtype->constval.vfunc = self;

    return self;
}

MEM_VEC_FUNCTIONS(ast_function, ast_block*, blocks)

void ast_function_delete(ast_function *self)
{
    size_t i;
    if (self->name)
        mem_d((void*)self->name);
    if (self->vtype) {
        /* ast_value_delete(self->vtype); */
        self->vtype->isconst = false;
        self->vtype->constval.vfunc = NULL;
        /* We use unref - if it was stored in a global table it is supposed
         * to be deleted from *there*
         */
        ast_unref(self->vtype);
    }
    for (i = 0; i < self->blocks_count; ++i)
        ast_delete(self->blocks[i]);
    MEM_VECTOR_CLEAR(self, blocks);
    mem_d(self);
}

/*********************************************************************/
/* AST codegen aprt
 */

bool ast_value_codegen(ast_value *self, ast_function *func, bool lvalue, ir_value **out)
{
    /* NOTE: This is the codegen for a variable used in an expression.
     * It is not the codegen to generate the value. For this purpose,
     * ast_local_codegen and ast_global_codegen are to be used before this
     * is executed. ast_function_codegen should take care of its locals,
     * and the ast-user should take care of ast_global_codegen to be used
     * on all the globals.
     */
    return false;
}

bool ast_global_codegen(ast_value *self, ir_builder *ir)
{
    ir_value *v = NULL;
    if (self->isconst && self->vtype == TYPE_FUNCTION)
    {
        ir_function *func = ir_builder_create_function(ir, self->name);
        if (!func)
            return false;

        self->constval.vfunc->ir_func = func;
        /* The function is filled later on ast_function_codegen... */
        return true;
    }

    v = ir_builder_create_global(ir, self->name, self->vtype);
    if (!v)
        return false;

    if (self->isconst) {
        switch (self->vtype)
        {
            case TYPE_FLOAT:
                if (!ir_value_set_float(v, self->constval.vfloat))
                    goto error;
                break;
            case TYPE_VECTOR:
                if (!ir_value_set_vector(v, self->constval.vvec))
                    goto error;
                break;
            case TYPE_STRING:
                if (!ir_value_set_string(v, self->constval.vstring))
                    goto error;
                break;
            case TYPE_FUNCTION:
                /* Cannot generate an IR value for a function,
                 * need a pointer pointing to a function rather.
                 */
                goto error;
            default:
                printf("TODO: global constant type %i\n", self->vtype);
                break;
        }
    }

    /* link us to the ir_value */
    self->ir_v = v;
    return true;

error: /* clean up */
    ir_value_delete(v);
    return false;
}

bool ast_function_codegen(ast_function *self, ir_builder *ir)
{
    if (!self->ir_func) {
        printf("ast_function's related ast_value was not generated yet\n");
        return false;
    }
    return false;
}

bool ast_block_codegen(ast_block *self, ast_function *func, bool lvalue, ir_value **out)
{
    return false;
}

bool ast_store_codegen(ast_store *self, ast_function *func, bool lvalue, ir_value **out)
{
    return false;
}

bool ast_binary_codegen(ast_binary *self, ast_function *func, bool lvalue, ir_value **out)
{
    return false;
}

bool ast_entfield_codegen(ast_entfield *self, ast_function *func, bool lvalue, ir_value **out)
{
    return false;
}
