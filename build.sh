#!/bin/bash

git submodule update --init --recursive
mkdir build
cd build || exit
cmake ..
make -j
make install
