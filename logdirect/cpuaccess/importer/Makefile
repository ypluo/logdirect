KERNEL_DIR=/usr/src/linux-headers-5.14.0+

obj-m += importer.o
all:
	make -C $(KERNEL_DIR) M=$(PWD) modules
clean:
	make -C $(KERNEL_DIR) M=$(PWD) clean