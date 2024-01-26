MODULE_NAME := hwBreakpointProc1
RESMAN_CORE_OBJS:=sys.o
RESMAN_GLUE_OBJS:=cvector.o
ifneq ($(KERNELRELEASE),)    
	$(MODULE_NAME)-objs:=$(RESMAN_GLUE_OBJS) $(RESMAN_CORE_OBJS)
	obj-m := hwBreakpointProc1.o
else
ifeq ($(KDIR),)
	$(error KDIR is not defined. Please set the KDIR variable.)
endif
all:
	make -C $(KDIR) ARCH=arm64 M=$(PWD) modules
clean:    
	rm -f *.ko *.o *.mod.o *.mod.c *.symvers    
endif    
