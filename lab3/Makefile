

CC := /usr/bin/gcc 

# obj-m is a list of what kernel modules to build.  The .o and other
# objects will be automatically built from the corresponding .c file -
# no need to list the source files explicitly.

obj-m := gpio_int.o

# KDIR is the location of the kernel source.  The current standard is
# to link to the associated source tree from the directory containing
# the compiled modules.

#KDIR	:= /usr/src/4.14.0-xilinx-v2018.3
KDIR	:= /usr/src/plnx_kernel
#KDIR	:= /usr/src/kernel_source

# PWD is the current working directory and the location of our module
# source files.

PWD   := $(shell pwd)

# default is the default make target.  The rule here says to run make
# with a working directory of the directory containing the kernel
# source and compile only the modules in the PWD (local) directory.

default:
	$(MAKE) -C $(KDIR) M=$(PWD) modules
	$(CC)  test1.c -o test1.exe
	$(CC)  test1.c -g -o test1_gdb
#	$(CC)  dma_latency_csv.c -o dma_latency_csv

clean::
	$(RM) .skeleton* *.cmd *.o *.ko *.mod.c *.exe
	$(RM) test1_gdb
	$(RM) -R .tmp*

