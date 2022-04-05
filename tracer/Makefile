KERNEL_PATH ?= /lib/modules/$(shell uname -r)/build

obj-m += execve_trace.o

all:
	make -C $(KERNEL_PATH) M=$(PWD) modules

clean:
	make -C $(KERNEL_PATH) M=$(PWD) clean

modules_install:
	make -C $(KERNEL_PATH) M=$(shell pwd) modules_install
	depmod


