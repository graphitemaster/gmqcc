#if 0
    /* Expected variables */
    qc_program     *prog;
#endif

#if !defined(QCVM_PROFILE)
#   define   QCVM_PROFILE 0
#endif

#if !defined(QCVM_TRACE)
#   define   QCVM_TRACE 0
#endif

#if !defined(FLOAT_IS_TRUE_FOR_INT)
#   define FLOAT_IS_TRUE_FOR_INT(x) ( (x) & 0x7FFFFFFF )
#endif

#if !defined(PRVM_ERROR)
#   define PRVM_ERROR prog->vmerror++, printvmerr
#endif

#if !defined(PRVM_NAME)
#   define PRVM_NAME (prog->filename)
#endif

#if !defined(PROG_GETSTRING)
#   define PROG_GETSTRING(x) prog_getstring(prog, (x))
#endif

#if !defined(PROG_ENTFIELD)
#   define PROG_ENTFIELD(x) prog_entfield(prog, (x))
#endif

#if !defined(PROG_GETEDICT)
#   define PROG_GETEDICT(x) prog_getedict(prog, (x))
#endif

#if !defined(PROG_ENTERFUNCTION)
#   define PROG_ENTERFUNCTION(x) prog_enterfunction(prog, (x))
#endif

#if !defined(PROG_LEAVEFUNCTION)
#   define PROG_LEAVEFUNCTION() prog_leavefunction(prog)
#endif

#define OPA ( (qcany*) (prog->globals + st->o1.u1) )
#define OPB ( (qcany*) (prog->globals + st->o2.u1) )
#define OPC ( (qcany*) (prog->globals + st->o3.u1) )
#define GLOBAL(x) ( (qcany*) (prog->globals + (x)) )

		while (1)
		{
		    prog_section_function  *newf;
		    qcany          *ed;
		    qcany          *ptr;

            ++st;
            /*
			prog->ip++;
		    st = prog->code + prog->ip;
		    */

#if QCVM_PROFILE
            prog->profile[st - prog->code]++;
#endif

#if QCVM_TRACE
            prog_print_statement(prog, st);
#endif

			switch (st->opcode)
			{
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
			case INSTR_MUL_F:
				OPC->_float = OPA->_float * OPB->_float;
				break;
			case INSTR_MUL_V:
				OPC->_float = OPA->vector[0]*OPB->vector[0] + OPA->vector[1]*OPB->vector[1] + OPA->vector[2]*OPB->vector[2];
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
				if( OPB->_float != 0.0f )
				{
					OPC->_float = OPA->_float / OPB->_float;
				}
				else
				{
					OPC->_float = 0.0f;
				}
				break;
			case INSTR_BITAND:
				OPC->_float = (int)OPA->_float & (int)OPB->_float;
				break;
			case INSTR_BITOR:
				OPC->_float = (int)OPA->_float | (int)OPB->_float;
				break;
			case INSTR_GE:
				OPC->_float = OPA->_float >= OPB->_float;
				break;
			case INSTR_LE:
				OPC->_float = OPA->_float <= OPB->_float;
				break;
			case INSTR_GT:
				OPC->_float = OPA->_float > OPB->_float;
				break;
			case INSTR_LT:
				OPC->_float = OPA->_float < OPB->_float;
				break;
			case INSTR_AND:
/* TODO change this back to float, and add AND_I to be used by fteqcc for anything not a float */
				OPC->_float = FLOAT_IS_TRUE_FOR_INT(OPA->_int) && FLOAT_IS_TRUE_FOR_INT(OPB->_int);
				break;
			case INSTR_OR:
/* TODO change this back to float, and add AND_I to be used by fteqcc for anything not a float */
				OPC->_float = FLOAT_IS_TRUE_FOR_INT(OPA->_int) || FLOAT_IS_TRUE_FOR_INT(OPB->_int);
				break;
			case INSTR_NOT_F:
				OPC->_float = !FLOAT_IS_TRUE_FOR_INT(OPA->_int);
				break;
			case INSTR_NOT_V:
				OPC->_float = !OPA->vector[0] && !OPA->vector[1] && !OPA->vector[2];
				break;
			case INSTR_NOT_S:
				OPC->_float = !OPA->string || !*PROG_GETSTRING(OPA->string);
				break;
			case INSTR_NOT_FNC:
				OPC->_float = !OPA->function;
				break;
			case INSTR_NOT_ENT:
				OPC->_float = (OPA->edict == 0);
				break;
			case INSTR_EQ_F:
				OPC->_float = OPA->_float == OPB->_float;
				break;
			case INSTR_EQ_V:
				OPC->_float = (OPA->vector[0] == OPB->vector[0]) &&
				              (OPA->vector[1] == OPB->vector[1]) &&
				              (OPA->vector[2] == OPB->vector[2]);
				break;
			case INSTR_EQ_S:
				OPC->_float = !strcmp(PROG_GETSTRING(OPA->string),PROG_GETSTRING(OPB->string));
				break;
			case INSTR_EQ_E:
				OPC->_float = OPA->_int == OPB->_int;
				break;
			case INSTR_EQ_FNC:
				OPC->_float = OPA->function == OPB->function;
				break;
			case INSTR_NE_F:
				OPC->_float = OPA->_float != OPB->_float;
				break;
			case INSTR_NE_V:
				OPC->_float = (OPA->vector[0] != OPB->vector[0]) || (OPA->vector[1] != OPB->vector[1]) || (OPA->vector[2] != OPB->vector[2]);
				break;
			case INSTR_NE_S:
				OPC->_float = strcmp(PROG_GETSTRING(OPA->string),PROG_GETSTRING(OPB->string));
				break;
			case INSTR_NE_E:
				OPC->_float = OPA->_int != OPB->_int;
				break;
			case INSTR_NE_FNC:
				OPC->_float = OPA->function != OPB->function;
				break;

		/*==================*/
			case INSTR_STORE_F:
			case INSTR_STORE_ENT:
			case INSTR_STORE_FLD:
			case INSTR_STORE_S:
			case INSTR_STORE_FNC:
				OPB->_int = OPA->_int;
				break;
			case INSTR_STORE_V:
				OPB->ivector[0] = OPA->ivector[0];
				OPB->ivector[1] = OPA->ivector[1];
				OPB->ivector[2] = OPA->ivector[2];
				break;

			case INSTR_STOREP_F:
			case INSTR_STOREP_ENT:
			case INSTR_STOREP_FLD:
			case INSTR_STOREP_S:
			case INSTR_STOREP_FNC:
				if (OPB->_int < 0 || OPB->_int >= prog->entitydata_count)
				{
					PRVM_ERROR("%s attempted to write to an out of bounds edict (%i)", PRVM_NAME, OPB->_int);
					goto cleanup;
				}
				if (OPB->_int < prog->entityfields && !prog->allowworldwrites)
					PRVM_ERROR("ERROR: assignment to world.%s (field %i) in %s\n", PROG_GETSTRING(PROG_ENTFIELD(OPB->_int)->name), OPB->_int, PRVM_NAME);
				ptr = (qcany*)(prog->entitydata + OPB->_int);
				ptr->_int = OPA->_int;
				break;
			case INSTR_STOREP_V:
				if (OPB->_int < 0 || OPB->_int + 2 >= prog->entitydata_count)
				{
					PRVM_ERROR("%s attempted to write to an out of bounds edict (%i)", PRVM_NAME, OPB->_int);
					goto cleanup;
				}
				if (OPB->_int < prog->entityfields && !prog->allowworldwrites)
					PRVM_ERROR("ERROR: assignment to world.%s (field %i) in %s\n", PROG_GETSTRING(PROG_ENTFIELD(OPB->_int)->name), OPB->_int, PRVM_NAME);
				ptr = (qcany*)(prog->entitydata + OPB->_int);
				ptr->ivector[0] = OPA->ivector[0];
				ptr->ivector[1] = OPA->ivector[1];
				ptr->ivector[2] = OPA->ivector[2];
				break;

			case INSTR_ADDRESS:
				if (OPA->edict < 0 || OPA->edict >= prog->entities)
				{
					PRVM_ERROR ("%s Progs attempted to address an out of bounds edict number %i", PRVM_NAME, OPA->edict);
					goto cleanup;
				}
				if ((unsigned int)(OPB->_int) >= (unsigned int)(prog->entityfields))
				{
					PRVM_ERROR("%s attempted to address an invalid field (%i) in an edict", PRVM_NAME, OPB->_int);
					goto cleanup;
				}

				ed = PROG_GETEDICT(OPA->edict);
				OPC->_int = ((qcint*)ed) - prog->entitydata;
				OPC->_int += OPB->_int;
				break;

			case INSTR_LOAD_F:
			case INSTR_LOAD_FLD:
			case INSTR_LOAD_ENT:
			case INSTR_LOAD_S:
			case INSTR_LOAD_FNC:
				if (OPA->edict < 0 || OPA->edict >= prog->entities)
				{
					PRVM_ERROR ("%s Progs attempted to read an out of bounds edict number", PRVM_NAME);
					goto cleanup;
				}
				if ((unsigned int)(OPB->_int) >= (unsigned int)(prog->entityfields))
				{
					PRVM_ERROR("%s attempted to read an invalid field in an edict (%i)", PRVM_NAME, OPB->_int);
					goto cleanup;
				}
				ed = PROG_GETEDICT(OPA->edict);
				OPC->_int = ((qcany*)( ((qcint*)ed) + OPB->_int ))->_int;
				break;

			case INSTR_LOAD_V:
				if (OPA->edict < 0 || OPA->edict >= prog->entities)
				{
					PRVM_ERROR ("%s Progs attempted to read an out of bounds edict number", PRVM_NAME);
					goto cleanup;
				}
				if (OPB->_int < 0 || OPB->_int + 3 >= prog->entityfields)
				{
					PRVM_ERROR("%s attempted to read an invalid field in an edict (%i)", PRVM_NAME, OPB->_int);
					goto cleanup;
				}
				ed = PROG_GETEDICT(OPA->edict);
				OPC->ivector[0] = ((qcany*)( ((qcint*)ed) + OPB->_int ))->ivector[0];
				OPC->ivector[1] = ((qcany*)( ((qcint*)ed) + OPB->_int ))->ivector[1];
				OPC->ivector[2] = ((qcany*)( ((qcint*)ed) + OPB->_int ))->ivector[2];
				break;

		/*==================*/

			case INSTR_IFNOT:
				if(!FLOAT_IS_TRUE_FOR_INT(OPA->_int))
				/* TODO add an "int-if", and change this one to OPA->_float
				   although mostly unneeded, thanks to the only float being false being 0x0 and 0x80000000 (negative zero)
				   and entity, string, field values can never have that value
				 */
				{
					st += st->o2.s1 - 1;	/* offset the s++ */
					if (++jumpcount >= maxjumps)
					{
						PRVM_ERROR("%s runaway loop counter hit limit of %li jumps\ntip: read above for list of most-executed functions", PRVM_NAME, jumpcount);
					}
				}
				break;

			case INSTR_IF:
				if(FLOAT_IS_TRUE_FOR_INT(OPA->_int))
				{
					st += st->o2.s1 - 1;	/* offset the s++ */
					/* no bounds check needed, it is done when loading progs */
					if (++jumpcount >= maxjumps)
					{
						PRVM_ERROR("%s runaway loop counter hit limit of %li jumps\ntip: read above for list of most-executed functions", PRVM_NAME, jumpcount);
					}
				}
				break;

			case INSTR_GOTO:
				st += st->o1.s1 - 1;	/* offset the s++ */
				/* no bounds check needed, it is done when loading progs */
				if (++jumpcount == 10000000)
				{
					PRVM_ERROR("%s runaway loop counter hit limit of %li jumps\ntip: read above for list of most-executed functions", PRVM_NAME, jumpcount);
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
					PRVM_ERROR("NULL function in %s", PRVM_NAME);

				if(!OPA->function || OPA->function >= (unsigned int)prog->functions_count)
				{
					PRVM_ERROR("%s CALL outside the program", PRVM_NAME);
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
					{
						prog->builtins[builtinnumber](prog);
					}
					else
						PRVM_ERROR("No such builtin #%i in %s; most likely cause: outdated engine build. Try updating!", builtinnumber, PRVM_NAME);
				}
				else
					st = prog->code + PROG_ENTERFUNCTION(newf);
				if (prog->vmerror)
				    goto cleanup;
				break;

			case INSTR_DONE:
			case INSTR_RETURN:
			    /* add instruction count to function profile count */
			    GLOBAL(OFS_RETURN)->ivector[0] = OPA->ivector[0];
			    GLOBAL(OFS_RETURN)->ivector[1] = OPA->ivector[1];
			    GLOBAL(OFS_RETURN)->ivector[2] = OPA->ivector[2];

                st = prog->code + PROG_LEAVEFUNCTION();
                if (!prog->stack_count)
                    goto cleanup;

                break;

			case INSTR_STATE:
				break;

/* LordHavoc: to be enabled when Progs version 7 (or whatever it will be numbered) is finalized */
/*
			case INSTR_ADD_I:
				OPC->_int = OPA->_int + OPB->_int;
				break;
			case INSTR_ADD_IF:
				OPC->_int = OPA->_int + (int) OPB->_float;
				break;
			case INSTR_ADD_FI:
				OPC->_float = OPA->_float + (float) OPB->_int;
				break;
			case INSTR_SUB_I:
				OPC->_int = OPA->_int - OPB->_int;
				break;
			case INSTR_SUB_IF:
				OPC->_int = OPA->_int - (int) OPB->_float;
				break;
			case INSTR_SUB_FI:
				OPC->_float = OPA->_float - (float) OPB->_int;
				break;
			case INSTR_MUL_I:
				OPC->_int = OPA->_int * OPB->_int;
				break;
			case INSTR_MUL_IF:
				OPC->_int = OPA->_int * (int) OPB->_float;
				break;
			case INSTR_MUL_FI:
				OPC->_float = OPA->_float * (float) OPB->_int;
				break;
			case INSTR_MUL_VI:
				OPC->vector[0] = (float) OPB->_int * OPA->vector[0];
				OPC->vector[1] = (float) OPB->_int * OPA->vector[1];
				OPC->vector[2] = (float) OPB->_int * OPA->vector[2];
				break;
			case INSTR_DIV_VF:
				{
					float temp = 1.0f / OPB->_float;
					OPC->vector[0] = temp * OPA->vector[0];
					OPC->vector[1] = temp * OPA->vector[1];
					OPC->vector[2] = temp * OPA->vector[2];
				}
				break;
			case INSTR_DIV_I:
				OPC->_int = OPA->_int / OPB->_int;
				break;
			case INSTR_DIV_IF:
				OPC->_int = OPA->_int / (int) OPB->_float;
				break;
			case INSTR_DIV_FI:
				OPC->_float = OPA->_float / (float) OPB->_int;
				break;
			case INSTR_CONV_IF:
				OPC->_float = OPA->_int;
				break;
			case INSTR_CONV_FI:
				OPC->_int = OPA->_float;
				break;
			case INSTR_BITAND_I:
				OPC->_int = OPA->_int & OPB->_int;
				break;
			case INSTR_BITOR_I:
				OPC->_int = OPA->_int | OPB->_int;
				break;
			case INSTR_BITAND_IF:
				OPC->_int = OPA->_int & (int)OPB->_float;
				break;
			case INSTR_BITOR_IF:
				OPC->_int = OPA->_int | (int)OPB->_float;
				break;
			case INSTR_BITAND_FI:
				OPC->_float = (int)OPA->_float & OPB->_int;
				break;
			case INSTR_BITOR_FI:
				OPC->_float = (int)OPA->_float | OPB->_int;
				break;
			case INSTR_GE_I:
				OPC->_float = OPA->_int >= OPB->_int;
				break;
			case INSTR_LE_I:
				OPC->_float = OPA->_int <= OPB->_int;
				break;
			case INSTR_GT_I:
				OPC->_float = OPA->_int > OPB->_int;
				break;
			case INSTR_LT_I:
				OPC->_float = OPA->_int < OPB->_int;
				break;
			case INSTR_AND_I:
				OPC->_float = OPA->_int && OPB->_int;
				break;
			case INSTR_OR_I:
				OPC->_float = OPA->_int || OPB->_int;
				break;
			case INSTR_GE_IF:
				OPC->_float = (float)OPA->_int >= OPB->_float;
				break;
			case INSTR_LE_IF:
				OPC->_float = (float)OPA->_int <= OPB->_float;
				break;
			case INSTR_GT_IF:
				OPC->_float = (float)OPA->_int > OPB->_float;
				break;
			case INSTR_LT_IF:
				OPC->_float = (float)OPA->_int < OPB->_float;
				break;
			case INSTR_AND_IF:
				OPC->_float = (float)OPA->_int && OPB->_float;
				break;
			case INSTR_OR_IF:
				OPC->_float = (float)OPA->_int || OPB->_float;
				break;
			case INSTR_GE_FI:
				OPC->_float = OPA->_float >= (float)OPB->_int;
				break;
			case INSTR_LE_FI:
				OPC->_float = OPA->_float <= (float)OPB->_int;
				break;
			case INSTR_GT_FI:
				OPC->_float = OPA->_float > (float)OPB->_int;
				break;
			case INSTR_LT_FI:
				OPC->_float = OPA->_float < (float)OPB->_int;
				break;
			case INSTR_AND_FI:
				OPC->_float = OPA->_float && (float)OPB->_int;
				break;
			case INSTR_OR_FI:
				OPC->_float = OPA->_float || (float)OPB->_int;
				break;
			case INSTR_NOT_I:
				OPC->_float = !OPA->_int;
				break;
			case INSTR_EQ_I:
				OPC->_float = OPA->_int == OPB->_int;
				break;
			case INSTR_EQ_IF:
				OPC->_float = (float)OPA->_int == OPB->_float;
				break;
			case INSTR_EQ_FI:
				OPC->_float = OPA->_float == (float)OPB->_int;
				break;
			case INSTR_NE_I:
				OPC->_float = OPA->_int != OPB->_int;
				break;
			case INSTR_NE_IF:
				OPC->_float = (float)OPA->_int != OPB->_float;
				break;
			case INSTR_NE_FI:
				OPC->_float = OPA->_float != (float)OPB->_int;
				break;
			case INSTR_STORE_I:
				OPB->_int = OPA->_int;
				break;
			case INSTR_STOREP_I:
#if PRBOUNDSCHECK
				if (OPB->_int < 0 || OPB->_int + 4 > pr_edictareasize)
				{
					PRVM_ERROR ("%s Progs attempted to write to an out of bounds edict", PRVM_NAME);
					goto cleanup;
				}
#endif
				ptr = (prvm_eval_t *)(prog->edictsfields + OPB->_int);
				ptr->_int = OPA->_int;
				break;
			case INSTR_LOAD_I:
#if PRBOUNDSCHECK
				if (OPA->edict < 0 || OPA->edict >= prog->max_edicts)
				{
					PRVM_ERROR ("%s Progs attempted to read an out of bounds edict number", PRVM_NAME);
					goto cleanup;
				}
				if (OPB->_int < 0 || OPB->_int >= progs->entityfields)
				{
					PRVM_ERROR ("%s Progs attempted to read an invalid field in an edict", PRVM_NAME);
					goto cleanup;
				}
#endif
				ed = PRVM_PROG_TO_EDICT(OPA->edict);
				OPC->_int = ((prvm_eval_t *)((int *)ed->v + OPB->_int))->_int;
				break;
			}
			*/

			default:
			    PRVM_ERROR("Illegal instruction in %s\n", PRVM_NAME);
			    goto cleanup;
			}
		}

#undef QCVM_PROFILE
#undef QCVM_TRACE
