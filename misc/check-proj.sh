#!/bin/sh

host="gmqcc.qc.to"
location=${host}"/files"
list=${location}"/files"
hashes=${location}"/hashes"

#download required things
download_list=$(wget -qO- ${list})
download_hashes=$(wget -qO- ${hashes})

download() {
    pushd ~/.gmqcc/testsuite >> /dev/null
    echo "$download_list" | while read -r line
    do
        echo "downloading $line ..."
        wget -q "${location}/$line"
    done

    echo "$download_hashes" > ~/.gmqcc/testsuite/hashes
    popd >> /dev/null
}

if [ -z "$download_list" -o -z "$download_hashes" ]; then
    echo "failed to download required information to check projects."

    if [ "$(ping -q -c1 "${host}")" ]; then
        echo "host ${host} seems to be up but missing required files."
        echo "please file bug report at: github.com/graphitemaster/gmqcc"
    else
        echo "host ${host} seems to be down, please try again later."
    fi

    echo "aborting"
    exit 1
fi

# we have existing contents around
if [ -f ~/.gmqcc/testsuite/hashes ]; then
    echo "$download_hashes" > /tmp/gmqcc_download_hashes
    diff -u ~/.gmqcc/testsuite/hashes /tmp/gmqcc_download_hashes >> /dev/null
    if [ $? -ne 0 ]; then
        echo "consistency errors in hashes (possible update), obtaining fresh contents"
        rm -rf ~/.gmqcc/testsuite/projects
        rm ~/.gmqcc/testsuite/*.zip

        download
    fi
else
    # do we even have the directory
    echo "preparing project testsuite for the first time"
    if [ ! -d ~/.gmqcc/testsuite ]; then
        mkdir -p ~/.gmqcc/testsuite
    fi

    download
fi

if [ ! -d ~/.gmqcc/testsuite/projects ]; then
    mkdir -p ~/.gmqcc/testsuite/projects
    pushd ~/.gmqcc/testsuite/projects >> /dev/null
    echo "$(ls ../ | cat | grep -v '^hashes$' | grep -v '^projects$')" | while read -r line
    do
        echo "extracting project $line"
        mkdir "$(echo "$line" | sed 's/\(.*\)\..*/\1/')"
        unzip -qq "../$line" -d $(echo "$line" | sed 's/\(.*\)\..*/\1/')
    done
    popd >> /dev/null
else
    echo "previous state exists, using it"
fi

# compile projects in those directories
which gmqcc >> /dev/null || (echo "error gmqcc not installed" && exit 1)
pushd ~/.gmqcc/testsuite/projects >> /dev/null
find . -maxdepth 1 -mindepth 1 -type d -printf "%f\n" | while read -r line
do
    echo -n "compiling $line... "
    pushd "$line" >> /dev/null
    gmqcc -std=qcc > /dev/null 2>&1
    if [ $? -ne 0 ]; then
        echo "error"
    else
        echo "success"
    fi

    popd >> /dev/null
done
popd >> /dev/null
