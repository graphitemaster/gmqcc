QCC = ../gmqcc
VM = ../qcvm

TESTLIST = \
	globaldefs \
	fielddefs  \
	builtins   \
	variadic   \
	calls      \
	if1        \
	loops1     \
	maths1     \
	maths2     \
	equality   \
	fields1    \
	invalid-types \
	ngraphs    \
	invalid-assign

.PHONY: clean test

clean:
	rm -f gmqcc qcvm
	rm -f */deflist */*.gm.dat */*.qcc.dat */output
	rm -rf obj

test: $(TESTLIST)

obj/%.gm.dat: %/main.qc obj
	@echo "Testing:" $(subst obj/,,$(subst .gm.dat,,$@))
	@$(QCC) -std=gmqcc -o $@ $< > $@.out 2> $@.err

obj/%.qcc.dat: %/main.qc obj
	@echo "Testing:" $(subst obj/,,$(subst .qcc.dat,,$@))
	@$(QCC) -std=qcc -o $@ $< > $@.out 2> $@.err

#######################################################################

# Macro which causes something to be compiled either with -std=qcc or without...
# this may at some point be extended to also store information about the progs.dat
# somewhere but for now we only need to build the object.
define maketest
$(eval $dat = obj/${1}.${2}.dat)
$1: obj/$1.$2.dat
endef

#######################################################################

globaldefs: obj/globaldefs.gm.dat
	@$(VM) -printdefs $< > $@/deflist
	@diff $@/deflist $@/deflist.expected

$(eval $(call maketest,fielddefs,gm))
fielddefs:
	@$(VM) -printfields $< > $@/deflist
	@diff $@/deflist $@/deflist.expected

$(eval $(call maketest,builtins,qcc))
builtins:
	@$(VM) -string "Hello 1" $< > $@/output
	@test "`wc -l $@/output | awk '{ print $$1 }'`" = "1"
	@grep -qE '^Hello 1$$' $@/output
	@$(VM) -string "A test message Yeah" $< > $@/output
	@test "`wc -l $@/output | awk '{ print $$1 }'`" = "1"
	@grep -qE '^A test message Yeah$$' $@/output

$(eval $(call maketest,variadic,qcc))
variadic:
	@$(VM) -string "Hello 1" $< > $@/output
	@test "`wc -l $@/output | awk '{ print $$1 }'`" = "1"
	@grep -qE '^Hello 1$$' $@/output
	@$(VM) -string "A test message Yeah" $< > $@/output
	@test "`wc -l $@/output | awk '{ print $$1 }'`" = "1"
	@grep -qE '^A test message Yeah$$' $@/output

$(eval $(call maketest,calls,qcc))
calls:
	@$(VM) -float 1 -float 100 -float 10000 $< > $@/output
	@grep -qE '^70907$$' $@/output
	@$(VM) -float 3 -float 201 -float 90127 $< > $@/output
	@grep -qE '^632719$$' $@/output

$(eval $(call maketest,if1,qcc))
if1:
	@$(VM) -float 1 -float 100 -float 10000 $< > $@/output
	@grep -qE '^One$$' $@/output
	@$(VM) -float 2 -float 100 -float 10000 $< > $@/output
	@grep -qE '^Two$$' $@/output
	@$(VM) -float 3 -float 100 -float 10000 $< > $@/output
	@grep -qE '^Three$$' $@/output
	@$(VM) -float 4 -float 100 -float 10000 $< > $@/output
	@grep -qE '^Else$$' $@/output

$(eval $(call maketest,loops1,qcc))
loops1:
	@$(VM) -float 0 $< > $@/output
	@diff $@/output $@/0.expected
	@$(VM) -float 1 $< > $@/output
	@diff $@/output $@/1.expected
	@$(VM) -float 4 $< > $@/output
	@diff $@/output $@/4.expected
	@$(VM) -float 10 $< > $@/output
	@diff $@/output $@/10.expected

$(eval $(call maketest,maths1,qcc))
maths1:
	@$(VM) -float 0 -float 3 $< > $@/output
	@diff $@/output $@/0.3.expected
	@$(VM) -float 3 -float 6 $< > $@/output
	@diff $@/output $@/3.6.expected
	@$(VM) -float 0 -float 0 $< > $@/output
	@diff $@/output $@/0.0.expected

$(eval $(call maketest,maths2,qcc))
maths2:
	@$(VM) -vector '1 2 3' -vector '4 5 6' $< > $@/output
	@grep -qE '^dot = 32$$' $@/output
	@$(VM) -vector '-5 12 5.5' -vector '4 -5 1' $< > $@/output
	@grep -qE '^dot = -74.5$$' $@/output
	@$(VM) -vector '-5 12 5.5' -vector '0 0 0' $< > $@/output
	@grep -qE '^dot = 0$$' $@/output

$(eval $(call maketest,equality,qcc))
equality:
	@$(VM) -float 1 -float 1 $< > $@/output
	@diff $@/output $@/1.1.expected
	@$(VM) -float 1 -float 0 $< > $@/output
	@diff $@/output $@/1.0.expected
	@$(VM) -float 0 -float 1 $< > $@/output
	@diff $@/output $@/0.1.expected

$(eval $(call maketest,fields1,qcc))
fields1:
	@$(VM) -vector '150 2000 150' -vector '220 1300 -200' $< > $@/output
	@diff $@/output $@/expected

invalid-types-ok: obj invalid-types/assign.qc invalid-types/op.qc invalid-types/call1.qc invalid-types/call2.qc invalid-types/call3.qc
	@echo "Testing: invalid-types"
	@if $(QCC) -std=qcc -o obj/invalid.dat invalid-types/op.qc     > obj/invalid.out 2>&1 ; then echo "Successfully compiled a file which was supposed to fail: op.qc"     ; false ; else true ; fi
	@if $(QCC) -std=qcc -o obj/invalid.dat invalid-types/call1.qc  > obj/invalid.out 2>&1 ; then echo "Successfully compiled a file which was supposed to fail: call1.qc"  ; false ; else true ; fi
	@if $(QCC) -std=qcc -o obj/invalid.dat invalid-types/call2.qc  > obj/invalid.out 2>&1 ; then echo "Successfully compiled a file which was supposed to fail: call2.qc"  ; false ; else true ; fi
	@if $(QCC) -std=qcc -o obj/invalid.dat invalid-types/call3.qc  > obj/invalid.out 2>&1 ; then echo "Successfully compiled a file which was supposed to fail: call3.qc"  ; false ; else true ; fi
	@if $(QCC) -std=qcc -o obj/invalid.dat invalid-types/assign.qc > obj/invalid.out 2>&1 ; then echo "Successfully compiled a file which was supposed to fail: assign.qc" ; false ; else true ; fi
	@touch obj/invalid-types-ok

invalid-types: invalid-types-ok

$(eval $(call maketest,ngraphs,qcc))
ngraphs:
	@$(VM) $< > $@/output
	@diff $@/output $@/expected

invalid-assign-ok: obj invalid-assign/main.qc
	@echo "Testing: invalid-assign"
	@if $(QCC) -std=qcc -o obj/invalid.dat invalid-assign/main.qc > obj/invalid.out 2>&1 ; then echo "Successfully compiled a file which was supposed to fail: invalid-assign/main.qc" ; false ; else true ; fi

invalid-assign: invalid-assign-ok

#######################################################################
obj:
	mkdir obj

../gmqcc:
	$(MAKE) -C ..

../qcvm:
	$(MAKE) -C .. qcvm

