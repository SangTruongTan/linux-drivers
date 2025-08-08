obj-m := pcd_n.o

ARCH=arm
CROSS_COMPILE=arm-linux-gnueabihf-
TARGET_SOURCE=/mnt/worker/repos/ldd/sources/linux/
HOST_SOURCE=/lib/modules/$(shell uname -r)/build/

target:
	make ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) -C $(TARGET_SOURCE) M=$(PWD) modules
clean:
	make ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) -C $(TARGET_SOURCE) M=$(PWD) clean
host:
	make -C $(HOST_SOURCE) M=$(PWD) modules
