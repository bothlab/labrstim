#!/bin/sh
set -e

cd /srv/build/labrstim

meson setup \
  -Dguiclient=false \
  -Ddevice=true \
  b

cd b
ninja && ninja install
