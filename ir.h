#ifndef QCIR_H__
#define QCIR_H__

#include "astir.h"

/* ir_value */

typedef struct
{
    /* both inclusive */
    size_t start;
    size_t end;
} ir_life_entry_t;

struct ir_function_s;
typedef struct ir_value_s {
    const char *_name;
    ir_type_t  vtype;
    ir_store_t store;
    filecontext_t context;

    MAKE_VEC(struct ir_instr_s*, reads);
    MAKE_VEC(struct ir_instr_s*, writes);

    /* constantvalues */
    union {
        float vfloat;
        int   vint;
        qc_vec_t vvec;
        struct ir_value_s *vpointer;
        const char *vstring;
    } cvalue;
    qbool has_constval;

    /* For the temp allocator */
    MAKE_VEC(ir_life_entry_t, life);
} ir_value;

/* ir_value can be a variable, or created by an operation */
ir_value* ir_value_var(const char *name, ir_store_t st, ir_type_t vtype);
/* if a result of an operation: the function should store
 * it to remember to delete it / garbage collect it
 */
ir_value* ir_value_out(struct ir_function_s *owner, const char *name, ir_store_t st, ir_type_t vtype);
void      ir_value_delete(ir_value*);
void      ir_value_set_name(ir_value*, const char *name);

void    ir_value_reads_add(ir_value*, struct ir_instr_s*);
void    ir_value_writes_add(ir_value*, struct ir_instr_s*);

qbool   ir_value_set_float(ir_value*, float f);
qbool   ir_value_set_int(ir_value*, int i);
qbool   ir_value_set_string(ir_value*, const char *s);
qbool   ir_value_set_vector(ir_value*, qc_vec_t v);
/*qbool   ir_value_set_pointer_v(ir_value*, ir_value* p); */
/*qbool   ir_value_set_pointer_i(ir_value*, int i);       */

void ir_value_life_add(ir_value*, ir_life_entry_t e);
/* merge an instruction into the life-range */
/* returns false if the lifepoint was already known */
qbool ir_value_life_merge(ir_value*, size_t);
/* check if a value lives at a specific point */
qbool ir_value_lives(ir_value*, size_t);

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
    ir_op_t       opcode;
    filecontext_t context;
    ir_value*     (_ops[3]);
    struct ir_block_s* (bops[2]);

    MAKE_VEC(ir_phi_entry_t, phi);

    /* For the temp-allocation */
    size_t eid;

    struct ir_block_s *owner;
} ir_instr;

ir_instr* ir_instr_new(struct ir_block_s *owner, ir_op_t opcode);
void      ir_instr_delete(ir_instr*);

void ir_instr_phi_add(ir_instr*, ir_phi_entry_t e);
void ir_instr_op(ir_instr*, int op, ir_value *value, qbool writing);

void ir_instr_dump(ir_instr* in, char *ind, int (*oprintf)(const char*,...));

/* block */
typedef struct ir_block_s
{
    const char    *_label;
    filecontext_t context;
    qbool         final; /* once a jump is added we're done */

    MAKE_VEC(ir_instr*, instr);
    MAKE_VEC(struct ir_block_s*, entries);
    MAKE_VEC(struct ir_block_s*, exits);
    MAKE_VEC(ir_value*, living);

    /* For the temp-allocation */
    size_t eid;
    qbool  is_return;
    size_t run_id;

    struct ir_function_s *owner;
} ir_block;

ir_block* ir_block_new(struct ir_function_s *owner, const char *label);
void      ir_block_delete(ir_block*);

void      ir_block_set_label(ir_block*, const char *label);

void      ir_block_instr_add(ir_block*, ir_instr *instr);
void      ir_block_instr_remove(ir_block*, size_t idx);
void      ir_block_exits_add(ir_block*, ir_block *b);
void      ir_block_entries_add(ir_block*, ir_block *b);
qbool     ir_block_entries_find(ir_block*, ir_block *b, size_t *idx);

ir_value* ir_block_create_binop(ir_block*, const char *label, ir_op_t op,
                                ir_value *left, ir_value *right);
qbool   ir_block_create_store_op(ir_block*, ir_op_t op, ir_value *target, ir_value *what);
qbool   ir_block_create_store(ir_block*, ir_value *target, ir_value *what);

ir_value* ir_block_create_add(ir_block*, const char *label, ir_value *l, ir_value *r);
ir_value* ir_block_create_sub(ir_block*, const char *label, ir_value *l, ir_value *r);
ir_value* ir_block_create_mul(ir_block*, const char *label, ir_value *l, ir_value *r);
ir_value* ir_block_create_div(ir_block*, const char *label, ir_value *l, ir_value *r);
ir_instr* ir_block_create_phi(ir_block*, const char *label, ir_type_t vtype);
ir_value* ir_phi_value(ir_instr*);
void      ir_phi_add(ir_instr*, ir_block *b, ir_value *v);

void      ir_block_create_return(ir_block*, ir_value *opt_value);

void      ir_block_create_if(ir_block*, ir_value *cond,
                             ir_block *ontrue, ir_block *onfalse);
/* A 'goto' is an actual 'goto' coded in QC, whereas
 * a 'jump' is a virtual construct which simply names the
 * next block to go to.
 * A goto usually becomes an OP_GOTO in the resulting code,
 * whereas a 'jump' usually doesn't add any actual instruction.
 */
void      ir_block_create_jump(ir_block*, ir_block *to);
void      ir_block_create_goto(ir_block*, ir_block *to);

void      ir_block_living_add(ir_block*, ir_value*);
void      ir_block_living_remove(ir_block*, size_t idx);
qbool     ir_block_living_find(ir_block*, ir_value*, size_t *idx);

void ir_block_dump(ir_block*, char *ind, int (*oprintf)(const char*,...));

/* function */

typedef struct ir_function_s
{
    const char    *_name;
    ir_type_t     retype;
    MAKE_VEC(ir_type_t, params);
    MAKE_VEC(ir_block*, blocks);

    /* values generated from operations
     * which might get optimized away, so anything
     * in there needs to be deleted in the dtor.
     */
    MAKE_VEC(ir_value*, values);

    /* locally defined variables */
    MAKE_VEC(ir_value*, locals);

    ir_block*     first;
    ir_block*     last;

    filecontext_t context;

    /* for temp allocation */
    size_t run_id;

    struct ir_builder_s *owner;
} ir_function;

ir_function* ir_function_new(struct ir_builder_s *owner);
void         ir_function_delete(ir_function*);

void ir_function_collect_value(ir_function*, ir_value *value);

void ir_function_set_name(ir_function*, const char *name);
void ir_function_params_add(ir_function*, ir_type_t p);
void ir_function_blocks_add(ir_function*, ir_block *b);

ir_value* ir_function_get_local(ir_function *self, const char *name);
ir_value* ir_function_create_local(ir_function *self, const char *name, ir_type_t vtype);

void ir_function_finalize(ir_function*);
/*
void ir_function_naive_phi(ir_function*);
void ir_function_enumerate(ir_function*);
void ir_function_calculate_liferanges(ir_function*);
*/

ir_block* ir_function_create_block(ir_function*, const char *label);

void ir_function_dump(ir_function*, char *ind, int (*oprintf)(const char*,...));

/* builder */
typedef struct ir_builder_s
{
    const char     *_name;
    MAKE_VEC(ir_function*, functions);
    MAKE_VEC(ir_value*, globals);
} ir_builder;

ir_builder* ir_builder_new(const char *modulename);
void        ir_builder_delete(ir_builder*);

void ir_builder_set_name(ir_builder *self, const char *name);

void ir_builder_functions_add(ir_builder* b, ir_function* f);
void ir_builder_globals_add(ir_builder* b, ir_value* f);

ir_function* ir_builder_get_function(ir_builder*, const char *fun);
ir_function* ir_builder_create_function(ir_builder*, const char *name);

ir_value* ir_builder_get_global(ir_builder*, const char *fun);
ir_value* ir_builder_create_global(ir_builder*, const char *name, ir_type_t vtype);

void ir_builder_dump(ir_builder*, int (*oprintf)(const char*, ...));

#endif
