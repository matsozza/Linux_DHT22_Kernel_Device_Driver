# DHT22 Temperature and Humidity sensor - Kernel Device Driver


## Getting started: 

### Clone Raspberry Linux Kernel as a sibling folder to this repo
- Repo: https://github.com/raspberrypi/linux.git
- Branch I used: rpi-6.12.y
- Rename the folder cloned to "linux_rpi" so the VSCode will automatically resolve Intelisense includes / refs

### Commands to compile the linux headers (using branch **rpi-6.12.y**) in the **host machine**:
- Move to Raspberry linux recently cloned
- Run:
    - make ARCH=arm64 CROSS_COMPILE=aarch64-rpi3-linux-gnu- bcm2711_defconfig
    - make ARCH=arm64 CROSS_COMPILE=aarch64-rpi3-linux-gnu- modules_prepare

### Version used
The available ".ko" Kernel object in this repo was compiled using:
- 6.12.21-v8+ SMP preempt mod_unload modversions aarch64


