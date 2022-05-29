#!/bin/bash

set -e

if [ ! -d `pwd`/build ]; then
    mkdir -p `pwd`/build
fi

rm -rf `pwd`/build/*

cd `pwd`/build &&
    cmake .. &&
    make

cd ..

if [ ! -d /usr/include/mymuduo ]; then
    mkdir /usr/include/mymuduo
fi

for head in `ls *.h`
do
    cp $head /usr/include/mymuduo
done

cp `pwd`/lib/libmymuduo.so /usr/lib


ldconfig