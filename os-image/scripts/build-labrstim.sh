#!/bin/sh
set -e

cd /srv/build/labrstim

# because of KFR, we need to compile C++ code with clang++
export CXX=clang++
export CC=gcc

meson setup \
  -Dguiclient=false \
  -Ddevice=true \
  b

cd b
ninja && ninja install
