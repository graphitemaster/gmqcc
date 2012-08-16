#if 0
    /* Expected variables */
    qc_program     *prog;
#endif

#define OPA ( (qcany*) (prog->globals + st->o1.u1) )
#define OPB ( (qcany*) (prog->globals + st->o2.u1) )
#define OPC ( (qcany*) (prog->globals + st->o3.u1) )

#define GLOBAL(x) ( (qcany*) (prog->globals + (x)) )

/* to be consistent with current darkplaces behaviour */
#if !defined(FLOAT_IS_TRUE_FOR_INT)
#   define FLOAT_IS_TRUE_FOR_INT(x) ( (x) & 0x7FFFFFFF )
#endif

while (1) {
	prog_section_function  *newf;
	qcany          *ed;
	qcany          *ptr;

    ++st;

#if QCVM_PROFILE
    prog->profile[st - prog->code]++;
#endif

#if QCVM_TRACE
    prog_print_statement(prog, st);
#endif

    switch (st->opcode)
    {
        default:
            qcvmerror(prog, "Illegal instruction in %s\n", prog->filename);
            goto cleanup;

		case INSTR_DONE:
		case INSTR_RETURN:
			/* TODO: add instruction count to function profile count */
			GLOBAL(OFS_RETURN)->ivector[0] = OPA->ivector[0];
			GLOBAL(OFS_RETURN)->ivector[1] = OPA->ivector[1];
			GLOBAL(OFS_RETURN)->ivector[2] = OPA->ivector[2];

            st = prog->code + prog_leavefunction(prog);
            if (!prog->stack_count)
                goto cleanup;

            break;

		case INSTR_MUL_F:
			OPC->_float = OPA->_float * OPB->_float;
			break;
		case INSTR_MUL_V:
			OPC->_float = OPA->vector[0]*OPB->vector[0] +
			              OPA->vector[1]*OPB->vector[1] +
			              OPA->vector[2]*OPB->vector[2];
			break;
		case INSTR_MUL_FV:
			OPC->vector[0] = OPA->_float * OPB->vector[0];
			OPC->vector[1] = OPA->_float * OPB->vector[1];
			OPC->vector[2] = OPA->_float * OPB->vector[2];
			break;
		case INSTR_MUL_VF:
			OPC->vector[0] = OPB->_float * OPA->vector[0];
			OPC->vector[1] = OPB->_float * OPA->vector[1];
			OPC->vector[2] = OPB->_float * OPA->vector[2];
			break;
		case INSTR_DIV_F:
			if (OPB->_float != 0.0f)
				OPC->_float = OPA->_float / OPB->_float;
			else
				OPC->_float = 0;
			break;

		case INSTR_ADD_F:
			OPC->_float = OPA->_float + OPB->_float;
			break;
		case INSTR_ADD_V:
			OPC->vector[0] = OPA->vector[0] + OPB->vector[0];
			OPC->vector[1] = OPA->vector[1] + OPB->vector[1];
			OPC->vector[2] = OPA->vector[2] + OPB->vector[2];
			break;
		case INSTR_SUB_F:
			OPC->_float = OPA->_float - OPB->_float;
			break;
		case INSTR_SUB_V:
			OPC->vector[0] = OPA->vector[0] - OPB->vector[0];
			OPC->vector[1] = OPA->vector[1] - OPB->vector[1];
			OPC->vector[2] = OPA->vector[2] - OPB->vector[2];
			break;

		case INSTR_EQ_F:
			OPC->_float = (OPA->_float == OPB->_float);
			break;
		case INSTR_EQ_V:
			OPC->_float = ((OPA->vector[0] == OPB->vector[0]) &&
				           (OPA->vector[1] == OPB->vector[1]) &&
				           (OPA->vector[2] == OPB->vector[2]) );
			break;
		case INSTR_EQ_S:
			OPC->_float = !strcmp(prog_getstring(prog, OPA->string),
			                      prog_getstring(prog, OPB->string));
			break;
		case INSTR_EQ_E:
			OPC->_float = (OPA->_int == OPB->_int);
			break;
		case INSTR_EQ_FNC:
			OPC->_float = (OPA->function == OPB->function);
			break;
		case INSTR_NE_F:
			OPC->_float = (OPA->_float != OPB->_float);
			break;
		case INSTR_NE_V:
			OPC->_float = ((OPA->vector[0] != OPB->vector[0]) ||
			               (OPA->vector[1] != OPB->vector[1]) ||
			               (OPA->vector[2] != OPB->vector[2]) );
			break;
		case INSTR_NE_S:
			OPC->_float = !!strcmp(prog_getstring(prog, OPA->string),
			                       prog_getstring(prog, OPB->string));
			break;
		case INSTR_NE_E:
			OPC->_float = (OPA->_int != OPB->_int);
			break;
		case INSTR_NE_FNC:
			OPC->_float = (OPA->function != OPB->function);
			break;

		case INSTR_LE:
			OPC->_float = (OPA->_float <= OPB->_float);
			break;
		case INSTR_GE:
			OPC->_float = (OPA->_float >= OPB->_float);
			break;
		case INSTR_LT:
			OPC->_float = (OPA->_float < OPB->_float);
			break;
		case INSTR_GT:
			OPC->_float = (OPA->_float > OPB->_float);
			break;

		case INSTR_LOAD_F:
		case INSTR_LOAD_S:
		case INSTR_LOAD_FLD:
		case INSTR_LOAD_ENT:
		case INSTR_LOAD_FNC:
			if (OPA->edict < 0 || OPA->edict >= prog->entities) {
			    qcvmerror(prog, "progs `%s` attempted to read an out of bounds entity", prog->filename);
				goto cleanup;
			}
			if ((unsigned int)(OPB->_int) >= (unsigned int)(prog->entityfields)) {
				qcvmerror(prog, "prog `%s` attempted to read an invalid field from entity (%i)",
				          prog->filename,
				          OPB->_int);
				goto cleanup;
			}
			ed = prog_getedict(prog, OPA->edict);
			OPC->_int = ((qcany*)( ((qcint*)ed) + OPB->_int ))->_int;
			break;
		case INSTR_LOAD_V:
			if (OPA->edict < 0 || OPA->edict >= prog->entities) {
			    qcvmerror(prog, "progs `%s` attempted to read an out of bounds entity", prog->filename);
				goto cleanup;
			}
			if (OPB->_int < 0 || OPB->_int + 3 > prog->entityfields)
			{
				qcvmerror(prog, "prog `%s` attempted to read an invalid field from entity (%i)",
				          prog->filename,
				          OPB->_int + 2);
				goto cleanup;
			}
			ed = prog_getedict(prog, OPA->edict);
			OPC->ivector[0] = ((qcany*)( ((qcint*)ed) + OPB->_int ))->ivector[0];
			OPC->ivector[1] = ((qcany*)( ((qcint*)ed) + OPB->_int ))->ivector[1];
			OPC->ivector[2] = ((qcany*)( ((qcint*)ed) + OPB->_int ))->ivector[2];
			break;

		case INSTR_ADDRESS:
			if (OPA->edict < 0 || OPA->edict >= prog->entities) {
				qcvmerror(prog, "prog `%s` attempted to address an out of bounds entity %i", prog->filename, OPA->edict);
				goto cleanup;
			}
			if ((unsigned int)(OPB->_int) >= (unsigned int)(prog->entityfields))
			{
				qcvmerror(prog, "prog `%s` attempted to read an invalid field from entity (%i)",
				          prog->filename,
				          OPB->_int);
				goto cleanup;
			}

			ed = prog_getedict(prog, OPA->edict);
			OPC->_int = ((qcint*)ed) - prog->entitydata;
			OPC->_int += OPB->_int;
			break;

		case INSTR_STORE_F:
		case INSTR_STORE_S:
		case INSTR_STORE_ENT:
		case INSTR_STORE_FLD:
		case INSTR_STORE_FNC:
			OPB->_int = OPA->_int;
			break;
		case INSTR_STORE_V:
			OPB->ivector[0] = OPA->ivector[0];
			OPB->ivector[1] = OPA->ivector[1];
			OPB->ivector[2] = OPA->ivector[2];
			break;

		case INSTR_STOREP_F:
		case INSTR_STOREP_S:
		case INSTR_STOREP_ENT:
		case INSTR_STOREP_FLD:
		case INSTR_STOREP_FNC:
			if (OPB->_int < 0 || OPB->_int >= prog->entitydata_count) {
				qcvmerror(prog, "`%s` attempted to write to an out of bounds edict (%i)", prog->filename, OPB->_int);
				goto cleanup;
			}
			if (OPB->_int < prog->entityfields && !prog->allowworldwrites)
				qcvmerror(prog, "`%s` tried to assign to world.%s (field %i)\n",
				          prog->filename,
				          prog_getstring(prog, prog_entfield(prog, OPB->_int)->name),
				          OPB->_int);
			ptr = (qcany*)(prog->entitydata + OPB->_int);
			ptr->_int = OPA->_int;
			break;
		case INSTR_STOREP_V:
			if (OPB->_int < 0 || OPB->_int + 2 >= prog->entitydata_count) {
				qcvmerror(prog, "`%s` attempted to write to an out of bounds edict (%i)", prog->filename, OPB->_int);
				goto cleanup;
			}
			if (OPB->_int < prog->entityfields && !prog->allowworldwrites)
				qcvmerror(prog, "`%s` tried to assign to world.%s (field %i)\n",
				          prog->filename,
				          prog_getstring(prog, prog_entfield(prog, OPB->_int)->name),
				          OPB->_int);
			ptr = (qcany*)(prog->entitydata + OPB->_int);
			ptr->ivector[0] = OPA->ivector[0];
			ptr->ivector[1] = OPA->ivector[1];
			ptr->ivector[2] = OPA->ivector[2];
			break;

		case INSTR_NOT_F:
			OPC->_float = !FLOAT_IS_TRUE_FOR_INT(OPA->_int);
			break;
		case INSTR_NOT_V:
			OPC->_float = !OPA->vector[0] &&
			              !OPA->vector[1] &&
			              !OPA->vector[2];
			break;
		case INSTR_NOT_S:
			OPC->_float = !OPA->string ||
			              !*prog_getstring(prog, OPA->string);
			break;
		case INSTR_NOT_ENT:
			OPC->_float = (OPA->edict == 0);
			break;
		case INSTR_NOT_FNC:
			OPC->_float = !OPA->function;
			break;

		case INSTR_IF:
		    /* this is consistent with darkplaces' behaviour */
			if(FLOAT_IS_TRUE_FOR_INT(OPA->_int))
			{
				st += st->o2.s1 - 1;	/* offset the s++ */
				if (++jumpcount >= maxjumps)
					qcvmerror(prog, "`%s` hit the runaway loop counter limit of %li jumps", prog->filename, jumpcount);
			}
			break;
		case INSTR_IFNOT:
			if(!FLOAT_IS_TRUE_FOR_INT(OPA->_int))
			{
				st += st->o2.s1 - 1;	/* offset the s++ */
				if (++jumpcount >= maxjumps)
					qcvmerror(prog, "`%s` hit the runaway loop counter limit of %li jumps", prog->filename, jumpcount);
			}
			break;

		case INSTR_CALL0:
		case INSTR_CALL1:
		case INSTR_CALL2:
		case INSTR_CALL3:
		case INSTR_CALL4:
		case INSTR_CALL5:
		case INSTR_CALL6:
		case INSTR_CALL7:
		case INSTR_CALL8:
			prog->argc = st->opcode - INSTR_CALL0;
			if (!OPA->function)
				qcvmerror(prog, "NULL function in `%s`", prog->filename);

			if(!OPA->function || OPA->function >= (unsigned int)prog->functions_count)
			{
				qcvmerror(prog, "CALL outside the program in `%s`", prog->filename);
				goto cleanup;
			}

			newf = &prog->functions[OPA->function];
			newf->profile++;

			prog->statement = (st - prog->code) + 1;

			if (newf->entry < 0)
			{
				/* negative statements are built in functions */
				int builtinnumber = -newf->entry;
				if (builtinnumber < prog->builtins_count && prog->builtins[builtinnumber])
					prog->builtins[builtinnumber](prog);
				else
					qcvmerror(prog, "No such builtin #%i in %s! Try updating your gmqcc sources",
					          builtinnumber, prog->filename);
			}
			else
				st = prog->code + prog_enterfunction(prog, newf);
			if (prog->vmerror)
				goto cleanup;
			break;

		case INSTR_STATE:
		    qcvmerror(prog, "`%s` tried to execute a STATE operation", prog->filename);
			break;

		case INSTR_GOTO:
			st += st->o1.s1 - 1;	/* offset the s++ */
			if (++jumpcount == 10000000)
					qcvmerror(prog, "`%s` hit the runaway loop counter limit of %li jumps", prog->filename, jumpcount);
			break;

		case INSTR_AND:
			OPC->_float = FLOAT_IS_TRUE_FOR_INT(OPA->_int) &&
			              FLOAT_IS_TRUE_FOR_INT(OPB->_int);
			break;
		case INSTR_OR:
			OPC->_float = FLOAT_IS_TRUE_FOR_INT(OPA->_int) ||
			              FLOAT_IS_TRUE_FOR_INT(OPB->_int);
			break;

		case INSTR_BITAND:
			OPC->_float = ((int)OPA->_float) & ((int)OPB->_float);
			break;
		case INSTR_BITOR:
			OPC->_float = ((int)OPA->_float) | ((int)OPB->_float);
			break;
    }
}

#undef QCVM_PROFILE
#undef QCVM_TRACE
