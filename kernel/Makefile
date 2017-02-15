obj-m += max1133.o
CROSS_COMPILE=/home/matthias/Development/RPI/tools-master/arm-bcm2708/gcc-linaro-arm-linux-gnueabihf-raspbian/bin/arm-linux-gnueabihf-
ARCH=arm

#KBUILD_DIR=/lib/modules/$(shell uname -r)/build
KBUILD_DIR=/home/matthias/Development/RPI/rtkern/linux-rpi-4.8/
all:
	make -C $(KBUILD_DIR) M=$(PWD) modules \
		ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE)

clean:
	make -C $(KBUILD_DIR) M=$(PWD) clean
