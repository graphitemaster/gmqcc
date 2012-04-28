#include "ir.h"
void builder1()
{
	ir_builder *b = ir_builder_new("test");
	ir_value *va = ir_builder_create_global(b, "a", TYPE_FLOAT);
	ir_value *v3 = ir_builder_create_global(b, "const_f_3", TYPE_FLOAT);
	ir_value *vb = ir_builder_create_global(b, "b", TYPE_FLOAT);
	ir_value *vc = ir_builder_create_global(b, "c", TYPE_FLOAT);
	ir_value *vd = ir_builder_create_global(b, "d", TYPE_FLOAT);

	if (!ir_value_set_float(v3, 3.0f)  ||
	    !ir_value_set_float(vb, 4.0f)  ||
	    !ir_value_set_float(vc, 10.0f) ||
	    !ir_value_set_float(vd, 20.0f) )
	    abort();

	ir_function *fmain = ir_builder_create_function(b, "main");

	ir_value *la = ir_function_create_local(fmain, "loc1", TYPE_FLOAT);
	(void)la;

	ir_block *bmain = ir_function_create_block(fmain, "top");
	ir_block *blt   = ir_function_create_block(fmain, "less");
	ir_block *bge   = ir_function_create_block(fmain, "greaterequal");
	ir_block *bend  = ir_function_create_block(fmain, "end");

	if (!ir_block_create_store_op(bmain, INSTR_STORE_F, va, v3)) abort();
	ir_value *sum = ir_block_create_add(bmain, "%sum", va, vb);
	ir_value *prd = ir_block_create_mul(bmain, "%mul", sum, vc);
	ir_value *less = ir_block_create_binop(bmain, "%less",
	                                       INSTR_LT, prd, vd);

	if (!ir_block_create_if(bmain, less, blt, bge)) abort();

	ir_value *x1 = ir_block_create_binop(blt, "%x1", INSTR_ADD_F, sum, v3);
	if (!ir_block_create_goto(blt, bend)) abort();

	ir_value *vig = ir_block_create_binop(bge, "%ignore", INSTR_ADD_F, va, vb);
	if (!ir_block_create_store_op(bge, INSTR_STORE_F, la, vig)) abort();
	ir_value *x2 = ir_block_create_binop(bge, "%x2", INSTR_ADD_F, sum, v3);
	if (!ir_block_create_goto(bge, bend)) abort();

	ir_instr *retphi = ir_block_create_phi(bend, "%retval", TYPE_FLOAT);
	if (!ir_phi_add(retphi, blt, x1) ||
	    !ir_phi_add(retphi, bge, x2) )
	    abort();
	ir_value *retval = ir_phi_value(retphi);
	if (!ir_block_create_return(bend, retval)) abort();

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
	if (!ir_function_finalize(fmain)) abort();
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
}
