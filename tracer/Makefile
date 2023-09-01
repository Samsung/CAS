KERNEL_PATH ?= /lib/modules/$(shell uname -r)/build

obj-m += bas_tracer.o

TRACER_DIR := $(PWD)
GIT_COMMIT := $(shell git -C $(TRACER_DIR) describe --always --dirty)
COMPILATION_TIME := $(shell date "+%F %T")
ccflags-y := -DGIT_COMMIT="\"$(GIT_COMMIT)\"" -DCOMPILATION_TIME="\"$(COMPILATION_TIME)\""

all:
	make -C $(KERNEL_PATH) M=$(PWD) modules

clean:
	make -C $(KERNEL_PATH) M=$(PWD) clean

modules_install:
	make -C $(KERNEL_PATH) M=$(shell pwd) modules_install
	depmod


