# Nome do módulo
obj-m := dht22_kernel.o

# Diretório raiz do kernel clonado (ajuste o caminho conforme necessário)
KDIR := ./linux
#KDIR := /lib/modules/$(shell uname -r)/build

# Diretório atual
PWD := $(shell pwd)

# Cross-Compiler and Architecture
CROSS_COMPILE := aarch64-rpi3-linux-gnu-
ARCH := arm64

all:
	@echo "\n------------------------------------------------"
	@echo "Compiling source files to kernel object" | fold -w 48
	@echo "------------------------------------------------"
	make -C $(KDIR) M=$(PWD) CROSS_COMPILE=$(CROSS_COMPILE) ARCH=$(ARCH) KBUILD_MODPOST_WARN=1 modules
	scp dht22_kernel.ko rpi.local:~

target:
	@echo "\n------------------------------------------------"
	@echo "Copying to target" | fold -w 48
	@echo "------------------------------------------------"
	#ssh rpi.local 'cd ~; rm -rf *'
	scp dht22_kernel.c matheus@rpi.local:/home/matheus

	@echo "\n------------------------------------------------"
	@echo "Compiling and installing in target" | fold -w 48
	@echo "------------------------------------------------"
	# ssh rpi.local 'make -C /lib/modules/6.6.74+rpt-rpi-v8/build M=$(pwd) modules'
	ssh rpi.local 'sudo rmmod dht22_kernel; sudo insmod dht22_kernel.ko; dmesg | tail'

	@echo "\n------------------------------------------------"
	@echo "Trying to read the device" | fold -w 48
	@echo "------------------------------------------------"
	ssh rpi.local 'cat /dev/dht22'

clean:
	@echo "\n------------------------------------------------"
	@echo "Cleaning all files" | fold -w 48
	@echo "------------------------------------------------"
	make -C $(KDIR) M=$(PWD) CROSS_COMPILE=$(CROSS_COMPILE) ARCH=$(ARCH) clean
