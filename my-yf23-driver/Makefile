obj-m	:= MyYF23Driver.o

#KERNELDIR ?= /lib/modules/$(shell uname -r)/build
KERNELDIR ?= /usr/src/linux-headers-$(shell uname -r)
PWD       := $(shell pwd)

all:
	$(MAKE) -C $(KERNELDIR) M=$(PWD)

clean:
	rm -rf *.o *~ core .depend .*.cmd *.ko *.mod.c .tmp_versions *.symvers *.order 

