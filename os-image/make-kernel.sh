#!/bin/bash
set -e

BASEDIR="$(realpath $(dirname "$0"))"
cd $BASEDIR

RT_PATCH_NAME=patch-6.6.52-rt43.patch

LINUX_ROOT_OVERLAY_DIR=$BASEDIR/overlays/linux-root
FW_BOOT_OVERLAY_DIR=$BASEDIR/overlays/fw-boot

export KERNEL=kernel8

if [ ! -d "gcc-tc" ]; then
    wget \
        https://developer.arm.com/-/media/Files/downloads/gnu/13.3.rel1/binrel/arm-gnu-toolchain-13.3.rel1-x86_64-aarch64-none-linux-gnu.tar.xz \
        -O arm-gnu-toolchain_aarch64.tar.xz

    mkdir ./gcc-tc
    tar -xf \
        arm-gnu-toolchain_aarch64.tar.xz \
        --strip-components=1 \
        -C ./gcc-tc
    rm arm-gnu-toolchain_aarch64.tar.xz
fi

if [ ! -d "gcc-tc/bin" ]; then
    echo "WARNING: Using host GCC cross-compiler instead of the ARM vendor one!"
    export CROSS_COMPILE=aarch64-linux-gnu-
else
    export CROSS_COMPILE=$BASEDIR/gcc-tc/bin/aarch64-none-linux-gnu-
fi

if [ ! -d "linux/" ]; then
    # fetch kernel sources
    git clone --depth 1 --branch rpi-6.6.y https://github.com/raspberrypi/linux

    # fetch RT patch
    wget -c https://mirrors.edge.kernel.org/pub/linux/kernel/projects/rt/6.6/$RT_PATCH_NAME.xz
    xz -d $RT_PATCH_NAME.xz

    cd linux

    # apply patch
    patch -p1 < ../$RT_PATCH_NAME
    rm ../$RT_PATCH_NAME

    # copy config
    cp ../kernel.config .config

    cd ..
fi

# build Linux
cd linux

make -j$(nproc --all) \
    ARCH=arm64 CROSS_COMPILE=$CROSS_COMPILE \
    CXXFLAGS="-march=armv8-a+crc -mtune=cortex-a72" \
    CFLAGS="-march=armv8-a+crc -mtune=cortex-a72" \
    Image.gz modules dtbs

make -j$(nproc --all) \
    ARCH=arm64 CROSS_COMPILE=$CROSS_COMPILE \
    INSTALL_MOD_PATH=$LINUX_ROOT_OVERLAY_DIR \
    modules_install

mkdir -p $FW_BOOT_OVERLAY_DIR/overlays/
cp arch/arm64/boot/Image.gz $FW_BOOT_OVERLAY_DIR/$KERNEL.img
cp arch/arm64/boot/dts/broadcom/*.dtb $FW_BOOT_OVERLAY_DIR/
cp arch/arm64/boot/dts/overlays/*.dtbo $FW_BOOT_OVERLAY_DIR/overlays/
cp arch/arm64/boot/dts/overlays/README $FW_BOOT_OVERLAY_DIR/overlays/
