#!/bin/bash

mkdir -p build
cd build
cmake .. && make -j2 && make test -j2 && ctest .
