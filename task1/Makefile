obj-m += task1.o
all:
	make -C /lib/modules/$(shell uname -r)/build M=$(shell pwd) modules
