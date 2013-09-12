#!/bin/sh

if [ ! -d qcsrc ]; then
    echo "failed to find qcsrc directory in $(pwd), please run this script"
    echo "from xonotic-data.pk3dir"
    exit 1
else
    # ensure this is actually a xonotic repo
    pushd qcsrc > /dev/null
    if [ ! -d client -o ! -d common -o ! -d dpdefs -o ! -d menu -o ! -d server -o ! -d warpzonelib ]; then
        echo "this doesnt look like a xonotic source tree, aborting"
        popd >> /dev/null
        exit 1
    fi
fi

# force reset and update
git rev-parse
if [ $? -ne 0 ]; then
    echo "not a git directory, continuing without rebase"
else
    echo -n "resetting git state and updating ... "
    git reset --hard HEAD > /dev/null
    git pull > /dev/null
    echo "complete"
fi

echo -n "removing redundant files ... "
rm -f autocvarize.pl
rm -f autocvarize-update.sh
rm -f collect-precache.sh
rm -f fteqcc-bugs.qc
rm -f i18n-badwords.txt
rm -f i18n-guide.txt
rm -rf server-testcase
rm -f Makefile
rm -f *.src
echo "complete"

echo -n "creating prog.src files ... "
echo "csprogs.dat" > csprogs.src
find client common warpzonelib csqcmodellib -type f >> csprogs.src
ls server/w_*.qc | cat >> csprogs.src
echo "progs.dat" > progs.src
find server common warpzonelib csqcmodellib -type f >> progs.src
ls server/w_*.qc | cat >> progs.src
echo "menu.dat" > menu.src
find menu common warpzonelib -type f >> menu.src
ls server/w_*.qc | cat >> menu.src
echo "complete"

echo -n "creating zip archive ... "
zip -r -9 ../xonotic.zip * > /dev/null
echo "complete"

popd > /dev/null
echo "finished!"
