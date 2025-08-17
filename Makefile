MOD_NAME = dht22_kernel
obj-m := $(MOD_NAME).o

# Current directory
PWD := $(shell pwd)

# Source files
SRC_DIR := $(PWD)/src
SRC_FILES := $(wildcard $(SRC_DIR)/*.c $(SRC_DIR)/*.h)

# Python files
PY_FILES = $(wildcard $(SRC_DIR)/*.py)

# Target destination
#TAR_DEV := rpi.local
TAR_DEV := 192.168.0.78
TAR_DEST := ~/$(MOD_NAME)

# Kernel object file
K_OBJ_FILE = $(patsubst %.o,%.ko,$(obj-m))

# Linux Kernel directory (or Raspberry Pi kernel) for ocmpilation
KDIR := ../linux_rpi

# Cross-Compiler and Architecture
CROSS_COMPILE := aarch64-rpi3-linux-gnu-
ARCH := arm64

all:
	@echo "\n--------------------------------------------------------------------------------"
	@echo "Compiling source files to kernel object" | fold -w 80
	@echo "--------------------------------------------------------------------------------"
	-clang-format -i $(SRC_FILES) --style=Microsoft --verbose # Try to do linting, if available
	make -C $(KDIR) M=$(SRC_DIR) CROSS_COMPILE=$(CROSS_COMPILE) ARCH=$(ARCH) KBUILD_MODPOST_WARN=1 modules

install:
	@echo "\n--------------------------------------------------------------------------------"
	@echo "Creating destination folder in Target" | fold -w 80
	@echo "--------------------------------------------------------------------------------"
	ssh $(TAR_DEV) 'sudo mkdir -p $(TAR_DEST)'
	ssh $(TAR_DEV) 'sudo chmod a+w $(TAR_DEST)'

	@echo "\n--------------------------------------------------------------------------------"
	@echo "Tranferring Kernel module to Target" | fold -w 80
	@echo "--------------------------------------------------------------------------------"
	scp $(SRC_DIR)/$(K_OBJ_FILE) $(TAR_DEV):$(TAR_DEST)

	@echo "\n--------------------------------------------------------------------------------"
	@echo "Tranferring Python files to Target" | fold -w 80
	@echo "--------------------------------------------------------------------------------"
	scp $(PY_FILES) $(TAR_DEV):$(TAR_DEST)

	@echo "\n--------------------------------------------------------------------------------"
	@echo "Installing module in source (removing old if needed)" | fold -w 80
	@echo "--------------------------------------------------------------------------------"
	- ssh rpi.local 'sudo rmmod $(patsubst %.o, %, $(obj-m))'
	ssh rpi.local 'sudo insmod $(TAR_DEST)/$(K_OBJ_FILE)'
	
	@echo "\n--------------------------------------------------------------------------------"
	@echo "Moving module to correct Linux module folders (to be installed in startup)" | fold -w 80
	@echo "--------------------------------------------------------------------------------"
	- ssh rpi.local 'sudo mkdir /lib/modules/$(uname -r)/extra'
	ssh rpi.local 'sudo mv -f $(TAR_DEST)/$(K_OBJ_FILE) /lib/modules/$(uname -r)/extra'

	@echo "\n--------------------------------------------------------------------------------"
	@echo "Checking kernel logs" | fold -w 80
	@echo "--------------------------------------------------------------------------------"
	ssh $(TAR_DEV) 'dmesg | tail'

	@echo "\n--------------------------------------------------------------------------------"
	@echo "Adding module to /etc/modules script" | fold -w 80
	@echo "--------------------------------------------------------------------------------"
	@ssh rpi.local \
	'if grep "$(MOD_NAME)" /etc/modules > /dev/null; then \
		echo "File already present in /etc/modules"; \
	else \
		echo "$(MOD_NAME)" | sudo tee -a /etc/modules; \
		echo "File added to /etc/modules"; \
	fi '

test:
	@echo "\n--------------------------------------------------------------------------------"
	@echo "Testing read from device driver" | fold -w 80
	@echo "--------------------------------------------------------------------------------"
	- ssh $(TAR_DEV) 'sudo hexdump -C /dev/dht22'

python:
	@echo "\n--------------------------------------------------------------------------------"
	@echo "Tranferring Python files to Target" | fold -w 80
	@echo "--------------------------------------------------------------------------------"
	scp $(PY_FILES) $(TAR_DEV):$(TAR_DEST)

	@echo "\n--------------------------------------------------------------------------------"
	@echo "Testing read from Python file" | fold -w 80
	@echo "--------------------------------------------------------------------------------"
	- ssh $(TAR_DEV) 'sudo python $(TAR_DEST)/dht22.py'

clean:
	@echo "\n--------------------------------------------------------------------------------"
	@echo "Cleaning all files" | fold -w 80
	@echo "--------------------------------------------------------------------------------"
	make -C $(KDIR) M=$(PWD) CROSS_COMPILE=$(CROSS_COMPILE) ARCH=$(ARCH) clean
