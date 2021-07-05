#!/bin/bash

mkdir -p build
cd build
cmake -G"Ninja" -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ .. && ninja && ninja test
