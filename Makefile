# Module name
obj-m := dht22_kernel.o

# Source files
SRC_FILES := $(wildcard *.c *.h)

# Target destination
TAR_DEV := rpi.local
TAR_DEST := ~

# Linux Kernel directory (or Raspberry Pi kernel) for ocmpilation
KDIR := ./linux

# Current directory
PWD := $(shell pwd)

# Cross-Compiler and Architecture
CROSS_COMPILE := aarch64-rpi3-linux-gnu-
ARCH := arm64

all:
	@echo "\n------------------------------------------------"
	@echo "Compiling source files to kernel object" | fold -w 48
	@echo "------------------------------------------------"
	-clang-format -i $(SRC_FILES) --style=Microsoft --verbose # Try to do linting, if available
	make -C $(KDIR) M=$(PWD) CROSS_COMPILE=$(CROSS_COMPILE) ARCH=$(ARCH) KBUILD_MODPOST_WARN=1 modules

install:
	@echo "\n------------------------------------------------"
	@echo "Tranferring Kernel module to Target" | fold -w 48
	@echo "------------------------------------------------"
	scp $(patsubst %.o, %.ko, $(obj-m)) $(TAR_DEV):$(TAR_DEST)

	@echo "\n------------------------------------------------"
	@echo "Installing module in source (removing old if needed)" | fold -w 48
	@echo "------------------------------------------------"
	- ssh rpi.local 'sudo rmmod $(patsubst %.o, %, $(obj-m))'
	ssh rpi.local 'sudo insmod $(patsubst %.o, %.ko, $(obj-m))'

	@echo "\n------------------------------------------------"
	@echo "Checking kernel logs" | fold -w 48
	@echo "------------------------------------------------"
	ssh $(TAR_DEV) 'dmesg | tail'

	@echo "\n------------------------------------------------"
	@echo "Testing read from device driver" | fold -w 48
	@echo "------------------------------------------------"
	- ssh $(TAR_DEV) 'sudo hexdump -C /dev/dht22'

clean:
	@echo "\n------------------------------------------------"
	@echo "Cleaning all files" | fold -w 48
	@echo "------------------------------------------------"
	make -C $(KDIR) M=$(PWD) CROSS_COMPILE=$(CROSS_COMPILE) ARCH=$(ARCH) clean
