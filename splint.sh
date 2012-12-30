#!/bin/sh

#these are stupid flags ... i.e to inhibit warnings that are just stupid
FLAGS_STUPID="\
    -redef               \
    -noeffect            \
    -nullderef           \
    -usedef              \
    -type                \
    -mustfreeonly        \
    -nullstate           \
    -varuse              \
    -mustfreefresh       \
    -compdestroy         \
    -compmempass         \
    -nullpass            \
    -onlytrans           \
    -predboolint         \
    -boolops             \
    -exportlocal         \
    -incondefs           \
    -macroredef          \
    -retvalint           \
    -nullret             \
    -predboolothers      \
    -globstate           \
    -dependenttrans      \
    -branchstate         \
    -compdef             \
    -temptrans           \
    -usereleased         \
    -warnposix"

#flags that have no place anywhere else
#mostly stupid
FLAGS_OTHERS="\
    -shiftimplementation \
    +charindex           \
    -kepttrans           \
    -unqualifiedtrans    \
    +matchanyintegral    \
    -bufferoverflowhigh  \
    +voidabstract"

#these are flags that MAYBE shouldn't be required
# -nullassign should be surpressed in code with /*@null*/
# (although that might be odd?)
FLAGS_MAYBE="\
    -nullassign          \
    -unrecog             \
    -casebreak           \
    -retvalbool          \
    -retvalother         \
    -mayaliasunique      \
    -realcompare         \
    -observertrans       \
    -shiftnegative       \
    -freshtrans          \
    -abstract            \
    -statictrans"

#these are flags that shouldn't be required. I.e tofix in code so that
#these don't need to be here to onhibit the warning
# remove one flag from here at a time while fixing the code so that
FLAGS_TOFIX="\
    -castfcnptr"


splint $FLAGS_STUPID $FLAGS_MAYBE $FLAGS_TOFIX $FLAGS_OTHERS *.c *.h
