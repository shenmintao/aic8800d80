ifneq ($(KERNELRELEASE),)

obj-m += aic_zlp_quirk.o

else

KVER ?= $(shell uname -r)
KDIR ?= /lib/modules/$(KVER)/build

.PHONY: all clean

all:
	$(MAKE) -C $(KDIR) M=$(CURDIR) modules

clean:
	$(MAKE) -C $(KDIR) M=$(CURDIR) clean

endif
