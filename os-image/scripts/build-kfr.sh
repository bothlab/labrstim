#!/bin/sh
set -e

cd /srv/build/

git clone https://github.com/kfrlib/kfr.git \
  --depth 1 \
  --branch 6.0.2

cd kfr
mkdir b && cd b

# add C/C++ flags to existing ones
export CFLAGS="$CFLAGS -march=armv8-a+crc -mtune=cortex-a72"
export CXXFLAGS="$CXXFLAGS -march=armv8-a+crc -mtune=cortex-a72"

cmake -GNinja -DCMAKE_BUILD_TYPE=Release \
  -DENABLE_CAPI_BUILD=ON \
  -DDCMAKE_POSITION_INDEPENDENT_CODE=ON \
  -DCMAKE_CXX_COMPILER=clang++ \
  ..
ninja && ninja install

# Evil workaround for Meson bug https://github.com/mesonbuild/meson/issues/11404
sed -i 's/"LINKER:/"-Wl,/' /usr/local/lib/cmake/kfr/KFRConfig.cmake
