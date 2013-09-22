obj-m	:= AsiCardDriver.o

KERNELDIR ?= /lib/modules/$(shell uname -r)/build
#KERNELDIR ?= /usr/src/linux-headers-$(shell uname -r)
PWD       := $(shell pwd)

all:
	$(MAKE) -C $(KERNELDIR) M=$(PWD)
	gcc -o test test.c
	gcc -o ReadReg ReadReg.c

clean:
	rm -rf *.o *~ core .depend .*.cmd *.ko *.mod.c .tmp_versions *.symvers *.order test ReadReg

