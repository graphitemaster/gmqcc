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
#ifndef GMQCC_IR_HDR
#define GMQCC_IR_HDR

/* ir_value */

typedef struct
{
    /* both inclusive */
    size_t start;
    size_t end;
} ir_life_entry_t;

struct ir_function_s;
typedef struct ir_value_s {
    char      *name;
    int       vtype;
    int       store;
    lex_ctx   context;

    MEM_VECTOR_MAKE(struct ir_instr_s*, reads);
    MEM_VECTOR_MAKE(struct ir_instr_s*, writes);

    /* constantvalues */
    bool isconst;
    union {
        float    vfloat;
        int      vint;
        vector   vvec;
        char    *vstring;
        struct ir_value_s *vpointer;
    } constval;

    /* For the temp allocator */
    MEM_VECTOR_MAKE(ir_life_entry_t, life);
} ir_value;

/* ir_value can be a variable, or created by an operation */
ir_value* ir_value_var(const char *name, int st, int vtype);
/* if a result of an operation: the function should store
 * it to remember to delete it / garbage collect it
 */
ir_value* ir_value_out(struct ir_function_s *owner, const char *name, int st, int vtype);
void      ir_value_delete(ir_value*);
void      ir_value_set_name(ir_value*, const char *name);

MEM_VECTOR_PROTO_ALL(ir_value, struct ir_instr_s*, reads)
MEM_VECTOR_PROTO_ALL(ir_value, struct ir_instr_s*, writes)

bool GMQCC_WARN ir_value_set_float(ir_value*, float f);
#if 0
bool GMQCC_WARN ir_value_set_int(ir_value*, int i);
#endif
bool GMQCC_WARN ir_value_set_string(ir_value*, const char *s);
bool GMQCC_WARN ir_value_set_vector(ir_value*, vector v);
/*bool   ir_value_set_pointer_v(ir_value*, ir_value* p); */
/*bool   ir_value_set_pointer_i(ir_value*, int i);       */

MEM_VECTOR_PROTO(ir_value, ir_life_entry_t, life)
/* merge an instruction into the life-range */
/* returns false if the lifepoint was already known */
bool ir_value_life_merge(ir_value*, size_t);
/* check if a value lives at a specific point */
bool ir_value_lives(ir_value*, size_t);

void ir_value_dump(ir_value*, int (*oprintf)(const char*,...));
void ir_value_dump_life(ir_value *self, int (*oprintf)(const char*,...));

typedef struct ir_phi_entry_s
{
    ir_value          *value;
    struct ir_block_s *from;
} ir_phi_entry_t;

/* instruction */
typedef struct ir_instr_s
{
    int       opcode;
    lex_ctx   context;
    ir_value* (_ops[3]);
    struct ir_block_s* (bops[2]);

    MEM_VECTOR_MAKE(ir_phi_entry_t, phi);

    /* For the temp-allocation */
    size_t eid;

    struct ir_block_s *owner;
} ir_instr;

ir_instr* ir_instr_new(struct ir_block_s *owner, int opcode);
void      ir_instr_delete(ir_instr*);

MEM_VECTOR_PROTO(ir_value, ir_phi_entry_t, phi)
bool GMQCC_WARN ir_instr_op(ir_instr*, int op, ir_value *value, bool writing);

void ir_instr_dump(ir_instr* in, char *ind, int (*oprintf)(const char*,...));

/* block */
typedef struct ir_block_s
{
    char      *label;
    lex_ctx    context;
    bool       final; /* once a jump is added we're done */

    MEM_VECTOR_MAKE(ir_instr*, instr);
    MEM_VECTOR_MAKE(struct ir_block_s*, entries);
    MEM_VECTOR_MAKE(struct ir_block_s*, exits);
    MEM_VECTOR_MAKE(ir_value*, living);

    /* For the temp-allocation */
    size_t eid;
    bool   is_return;
    size_t run_id;

    struct ir_function_s *owner;
} ir_block;

ir_block* ir_block_new(struct ir_function_s *owner, const char *label);
void      ir_block_delete(ir_block*);

bool      ir_block_set_label(ir_block*, const char *label);

MEM_VECTOR_PROTO(ir_block, ir_instr*, instr)
MEM_VECTOR_PROTO_ALL(ir_block, ir_block*, exits)
MEM_VECTOR_PROTO_ALL(ir_block, ir_block*, entries)

ir_value* ir_block_create_binop(ir_block*, const char *label, int op,
                                ir_value *left, ir_value *right);
bool GMQCC_WARN ir_block_create_store_op(ir_block*, int op, ir_value *target, ir_value *what);
bool GMQCC_WARN ir_block_create_store(ir_block*, ir_value *target, ir_value *what);

ir_value* ir_block_create_add(ir_block*, const char *label, ir_value *l, ir_value *r);
ir_value* ir_block_create_sub(ir_block*, const char *label, ir_value *l, ir_value *r);
ir_value* ir_block_create_mul(ir_block*, const char *label, ir_value *l, ir_value *r);
ir_value* ir_block_create_div(ir_block*, const char *label, ir_value *l, ir_value *r);
ir_instr* ir_block_create_phi(ir_block*, const char *label, int vtype);
ir_value* ir_phi_value(ir_instr*);
bool GMQCC_WARN ir_phi_add(ir_instr*, ir_block *b, ir_value *v);

bool GMQCC_WARN ir_block_create_return(ir_block*, ir_value *opt_value);

bool GMQCC_WARN ir_block_create_if(ir_block*, ir_value *cond,
                             ir_block *ontrue, ir_block *onfalse);
/* A 'goto' is an actual 'goto' coded in QC, whereas
 * a 'jump' is a virtual construct which simply names the
 * next block to go to.
 * A goto usually becomes an OP_GOTO in the resulting code,
 * whereas a 'jump' usually doesn't add any actual instruction.
 */
bool GMQCC_WARN ir_block_create_jump(ir_block*, ir_block *to);
bool GMQCC_WARN ir_block_create_goto(ir_block*, ir_block *to);

MEM_VECTOR_PROTO_ALL(ir_block, ir_value*, living)

void ir_block_dump(ir_block*, char *ind, int (*oprintf)(const char*,...));

/* function */

typedef struct ir_function_s
{
    char *name;
    int   retype;
    MEM_VECTOR_MAKE(int, params);
    MEM_VECTOR_MAKE(ir_block*, blocks);

    /* values generated from operations
     * which might get optimized away, so anything
     * in there needs to be deleted in the dtor.
     */
    MEM_VECTOR_MAKE(ir_value*, values);

    /* locally defined variables */
    MEM_VECTOR_MAKE(ir_value*, locals);

    ir_block*     first;
    ir_block*     last;

    lex_ctx       context;

    /* for temp allocation */
    size_t run_id;

    struct ir_builder_s *owner;
} ir_function;

ir_function* ir_function_new(struct ir_builder_s *owner);
void         ir_function_delete(ir_function*);

bool GMQCC_WARN ir_function_collect_value(ir_function*, ir_value *value);

bool ir_function_set_name(ir_function*, const char *name);
MEM_VECTOR_PROTO(ir_function, int, params)
MEM_VECTOR_PROTO(ir_function, ir_block*, blocks)

ir_value* ir_function_get_local(ir_function *self, const char *name);
ir_value* ir_function_create_local(ir_function *self, const char *name, int vtype);

bool GMQCC_WARN ir_function_finalize(ir_function*);
/*
bool ir_function_naive_phi(ir_function*);
bool ir_function_enumerate(ir_function*);
bool ir_function_calculate_liferanges(ir_function*);
*/

ir_block* ir_function_create_block(ir_function*, const char *label);

void ir_function_dump(ir_function*, char *ind, int (*oprintf)(const char*,...));

/* builder */
typedef struct ir_builder_s
{
    char *name;
    MEM_VECTOR_MAKE(ir_function*, functions);
    MEM_VECTOR_MAKE(ir_value*, globals);
} ir_builder;

ir_builder* ir_builder_new(const char *modulename);
void        ir_builder_delete(ir_builder*);

bool ir_builder_set_name(ir_builder *self, const char *name);

MEM_VECTOR_PROTO(ir_builder, ir_function*, functions)
MEM_VECTOR_PROTO(ir_builder, ir_value*, globals)

ir_function* ir_builder_get_function(ir_builder*, const char *fun);
ir_function* ir_builder_create_function(ir_builder*, const char *name);

ir_value* ir_builder_get_global(ir_builder*, const char *fun);
ir_value* ir_builder_create_global(ir_builder*, const char *name, int vtype);

void ir_builder_dump(ir_builder*, int (*oprintf)(const char*, ...));

#endif
