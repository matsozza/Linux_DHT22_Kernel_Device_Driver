# Nome do módulo
obj-m := dht22_kernel.o

# Diretório raiz do kernel clonado (ajuste o caminho conforme necessário)
KDIR := ./linux

# Diretório atual
PWD := $(shell pwd)

# Cross-Compiler and Architecture
CROSS_COMPILE := aarch64-rpi3-linux-gnu-
ARCH := arm64

all:
	@echo "\n------------------------------------------------"
	@echo "Compiling source files to kernel object" | fold -w 48
	@echo "------------------------------------------------"
	clang-format -i dht22_kernel.c --style=Microsoft --verbose
	make -C $(KDIR) M=$(PWD) CROSS_COMPILE=$(CROSS_COMPILE) ARCH=$(ARCH) KBUILD_MODPOST_WARN=1 modules
	scp dht22_kernel.ko rpi.local:~

install:
	@echo "\n------------------------------------------------"
	@echo "Installing module in source (removing old if needed)" | fold -w 48
	@echo "------------------------------------------------"
	- ssh rpi.local 'sudo rmmod dht22_kernel'
	ssh rpi.local 'sudo insmod dht22_kernel.ko'

	@echo "\n------------------------------------------------"
	@echo "Checking kernel logs" | fold -w 48
	@echo "------------------------------------------------"
	ssh rpi.local 'dmesg | tail'

	@echo "\n------------------------------------------------"
	@echo "Reading from device driver" | fold -w 48
	@echo "------------------------------------------------"
	#ssh rpi.local 'sudo cat /dev/dht22'

clean:
	@echo "\n------------------------------------------------"
	@echo "Cleaning all files" | fold -w 48
	@echo "------------------------------------------------"
	make -C $(KDIR) M=$(PWD) CROSS_COMPILE=$(CROSS_COMPILE) ARCH=$(ARCH) clean
