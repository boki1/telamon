#!/bin/bash

mkdir -p build
cd build
cmake -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ .. && make -j2 && make test -j2 && ctest .
