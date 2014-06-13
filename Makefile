obj-m := hello_world.o
kernel_path=/usr/src/linux
all:
	make -C $(kernel_path) M=$(PWD) modules
clean:
	make -C $(kernel_path) M=$(PWD) clean
