ccflags-y := -Wno-declaration-after-statement -g

hyper-o-objs := main.o asm_functions.o log.o vmx.o 
obj-m += hyper-o.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

pwn:
	make -C ../pwnkernel/linux-5.8 M=$(PWD) modules

remote:
	make -C ../pwnkernel/linux-5.8 M=$(PWD) modules

clean:
	rm -rf *.o *~ core .depend .*.cmd *.ko *.mod.c .tmp_versions

