#include "gmqcc.h"
#include "ir.h"

#ifdef assert
#   undef assert
#endif
/* (note: 'do {} while(0)' forces the need for a semicolon after assert() */
#define assert(x) do { if ( !(x) ) { printf("Assertion failed: %s\n", #x); abort(); } } while(0)

int main()
{
	ir_builder *b  = ir_builder_new("test");
	ir_value   *va = ir_builder_create_global(b, "a", TYPE_FLOAT);
	ir_value   *v3 = ir_builder_create_global(b, "const_f_3", TYPE_FLOAT);
	ir_value   *vb = ir_builder_create_global(b, "b", TYPE_FLOAT);
	ir_value   *vc = ir_builder_create_global(b, "c", TYPE_FLOAT);
	ir_value   *vd = ir_builder_create_global(b, "d", TYPE_FLOAT);

	ir_function *fmain  = NULL;
	ir_value    *la     = NULL;
	ir_block    *bmain  = NULL;
	ir_block    *blt    = NULL;
	ir_block    *bge    = NULL;
	ir_block    *bend   = NULL;
	ir_value    *sum    = NULL;
	ir_value    *prd    = NULL;
	ir_value    *less   = NULL;
	ir_value    *x1     = NULL;
	ir_value    *vig    = NULL;
	ir_value    *x2     = NULL;
	ir_instr    *retphi = NULL;
	ir_value    *retval = NULL;

	assert(ir_value_set_float(v3, 3.0f)  );
	assert(ir_value_set_float(vb, 4.0f)  );
	assert(ir_value_set_float(vc, 10.0f) );
	assert(ir_value_set_float(vd, 20.0f) );

	fmain = ir_builder_create_function(b, "main");
	assert(fmain);

	la = ir_function_create_local(fmain, "loc1", TYPE_FLOAT);
	assert(la);

	assert( bmain = ir_function_create_block(fmain, "top")          );
	assert( blt   = ir_function_create_block(fmain, "less")         );
	assert( bge   = ir_function_create_block(fmain, "greaterequal") );
	assert( bend  = ir_function_create_block(fmain, "end")          );

	assert(ir_block_create_store_op(bmain, INSTR_STORE_F, va, v3));
	assert( sum  = ir_block_create_add(bmain, "%sum", va, vb)               );
	assert( prd  = ir_block_create_mul(bmain, "%mul", sum, vc)              );
	assert( less = ir_block_create_binop(bmain, "%less", INSTR_LT, prd, vd) );

	assert(ir_block_create_if(bmain, less, blt, bge));

	x1 = ir_block_create_binop(blt, "%x1", INSTR_ADD_F, sum, v3);
	assert(x1);
	assert(ir_block_create_goto(blt, bend));

	vig = ir_block_create_binop(bge, "%ignore", INSTR_ADD_F, va, vb);
	assert(vig);
	assert(ir_block_create_store_op(bge, INSTR_STORE_F, la, vig));
	x2 = ir_block_create_binop(bge, "%x2", INSTR_ADD_F, sum, v3);
	assert(x2);
	assert(ir_block_create_goto(bge, bend));

	retphi = ir_block_create_phi(bend, "%retval", TYPE_FLOAT);
	assert(retphi);
	assert(ir_phi_add(retphi, blt, x1));
	assert(ir_phi_add(retphi, bge, x2));
	retval = ir_phi_value(retphi);
	assert(retval);
	assert(ir_block_create_return(bend, retval));

	/*
	printf("%i  should be 1\n", ir_value_life_merge(va, 31));
	printf("%i  should be 1\n", ir_value_life_merge(va, 33));
	printf("%i  should be 0\n", ir_value_life_merge(va, 33));
	printf("%i  should be 1\n", ir_value_life_merge(va, 1));
	printf("%i  should be 1\n", ir_value_life_merge(va, 2));
	printf("%i  should be 1\n", ir_value_life_merge(va, 20));
	printf("%i  should be 1\n", ir_value_life_merge(va, 21));
	printf("%i  should be 1\n", ir_value_life_merge(va, 8));
	printf("%i  should be 1\n", ir_value_life_merge(va, 9));
	printf("%i  should be 1\n", ir_value_life_merge(va, 3));
	printf("%i  should be 0\n", ir_value_life_merge(va, 9));
	printf("%i  should be 1\n", ir_value_life_merge(va, 17));
	printf("%i  should be 1\n", ir_value_life_merge(va, 18));
	printf("%i  should be 1\n", ir_value_life_merge(va, 19));
	printf("%i  should be 0\n", ir_value_life_merge(va, 19));
	ir_value_dump_life(va, printf);
	printf("%i  should be 1\n", ir_value_life_merge(va, 10));
	printf("%i  should be 1\n", ir_value_life_merge(va, 9));
	printf("%i  should be 0\n", ir_value_life_merge(va, 10));
	printf("%i  should be 0\n", ir_value_life_merge(va, 10));
	ir_value_dump_life(va, printf);
	*/

	ir_builder_dump(b, printf);
	assert(ir_function_finalize(fmain));
	ir_builder_dump(b, printf);

	ir_value_dump_life(sum, printf);
	ir_value_dump_life(prd, printf);
	ir_value_dump_life(less, printf);
	ir_value_dump_life(x1, printf);
	ir_value_dump_life(x2, printf);
	ir_value_dump_life(retval, printf);
	ir_value_dump_life(vig, printf);
	ir_value_dump_life(la, printf);

	ir_builder_delete(b);
	return 0;
}
