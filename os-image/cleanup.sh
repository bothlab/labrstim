#!/bin/bash
set +e

BASEDIR="$(realpath $(dirname "$0"))"
cd $BASEDIR

set -x
rm -rf linux/
rm -rf overlays/labrstim/
rm -rf overlays/linux-root/lib/modules/

rm -f overlays/fw-boot/*.dtb
rm -f overlays/fw-boot/*.img
rm -f overlays/fw-boot/overlays/*.dtbo
rm -f overlays/fw-boot/overlays/README

rm -f labrstim-rpi*.img.gz
rm -f labrstim-rpi*.img.bmap
