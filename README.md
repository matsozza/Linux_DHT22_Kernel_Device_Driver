# DHT22 Temperature and Humidity sensor - Kernel Device Driver

## Commands to compile the linux headers (using branch **rpi-6.12.y**) in the **host machine**:
- make ARCH=arm64 CROSS_COMPILE=aarch64-rpi3-linux-gnu- bcmrpi3_defconfig
- make ARCH=arm64 CROSS_COMPILE=aarch64-rpi3-linux-gnu- modules_prepare

## Version used
The available ".ko" Kernel object in this repo was compiled using:
- 6.12.21-v8+ SMP preempt mod_unload modversions aarch64


