#!/bin/bash
# Pre-transpile the standard library to C headers

mkdir -p lib
for f in lib/*.nr; do
    echo "Transpiling $f..."
    ./nira build $f
    mv "build/$f.c" "${f%.nr}.h"
done
rm -rf build
