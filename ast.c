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

#define ast_instantiate(T, ctx, destroyfn)                    \
    T *self = (T*)mem_a(sizeof(T));                           \
    ast_node_init((ast_node*)self, ctx);                      \
    ( (ast_node*)self )->node.destroy = (ast_node_delete*)destroyfn;

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

ast_value* ast_value_new(lex_ctx ctx, const char *name, int t, bool keep)
{
    ast_instantiate(ast_value, ctx, ast_value_delete);
    ast_expression_init((ast_expression*)self,
                        (ast_expression_codegen*)&ast_value_codegen);
    self->expression.node.keep = keep;

    self->name = name ? util_strdup(name) : NULL;
    self->vtype = t;
    self->next = NULL;
    MEM_VECTOR_INIT(self, params);
    self->isconst = false;
    memset(&self->constval, 0, sizeof(self->constval));

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
        ast_unref(self->params[i]);
    MEM_VECTOR_CLEAR(self, params);
    if (self->next) /* delete, not unref, types are always copied */
        ast_delete(self->next);
    if (self->isconst) {
        switch (self->vtype)
        {
        case TYPE_STRING:
            mem_d((void*)self->constval.vstring);
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
                           ast_value* left, ast_value* right)
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

ast_store* ast_store_new(lex_ctx ctx, int op,
                         ast_value *dest, ast_value *source)
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
    for (i = 0; i < self->locals_count; ++i)
        ast_delete(self->locals[i]);
    MEM_VECTOR_CLEAR(self, locals);
    for (i = 0; i < self->exprs_count; ++i)
        ast_unref(self->exprs[i]);
    MEM_VECTOR_CLEAR(self, exprs);
    mem_d(self);
}

ast_function* ast_function_new(lex_ctx ctx, const char *name, ast_value *vtype)
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

/* Some dummies so it compiles... */
bool ast_value_codegen(ast_value *self, ast_function *func, ir_value **out)
{
    return false;
}

bool ast_block_codegen(ast_block *self, ast_function *func, ir_value **out)
{
    return false;
}

bool ast_store_codegen(ast_store *self, ast_function *func, ir_value **out)
{
    return false;
}

bool ast_binary_codegen(ast_binary *self, ast_function *func, ir_value **out)
{
    return false;
}
