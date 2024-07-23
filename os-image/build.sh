#!/bin/bash
set -e

BASEDIR="$(realpath $(dirname "$0"))"
cd $BASEDIR

# Kernel
./make-kernel.sh

# Sync source code
mkdir -p overlays/labrstim
cd overlays/labrstim
rsync -a \
	--delete-after \
	--exclude os-image \
	--exclude build \
	../../.. \
	.
cd ..

## Compile device tree
cd fw-boot/overlays
dtc -O dtb -o labrstim.dtbo labrstim-overlay.dts
cd ..


# Build image
cd ../..
/opt/gocode/bin/debos \
	-b kvm \
	-m4G \
	-c4 \
	--scratchsize=8G \
	labrstim-rpi4-image.yaml
