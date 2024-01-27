MODULE_NAME := rwMem
RESMAN_CORE_OBJS:=sys.o
RESMAN_GLUE_OBJS:=
ifneq ($(KERNELRELEASE),)
	$(MODULE_NAME)-objs:=$(RESMAN_GLUE_OBJS) $(RESMAN_CORE_OBJS)
	obj-m := rwMem.o
else
ifeq ($(KDIR),)
	$(error KDIR is not defined. Please set the KDIR variable.)
endif
all:
	make -C $(KDIR) M=$(PWD) ARCH=arm64 SUBARCH=arm64 modules
clean:
	rm -f build
endif
