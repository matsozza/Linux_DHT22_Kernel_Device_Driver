# 🌡️ DHT22 Kernel Device Driver — Temperature & Humidity Sensor

A custom Linux Kernel Module (LKM) for interfacing with the DHT22 temperature and humidity sensor on Raspberry Pi using GPIO edge detection. Paired with a Python client for user-space access and verification.

---

## 📁 Repository Structure

| Path              | Description                                                              |
|-------------------|--------------------------------------------------------------------------|
| `src/dht22_kernel.c` | Kernel module source code that handles GPIO, IRQ, and bitstream decoding |
| `src/dht22.py`       | Python script to read binary data from `/dev/dht22` and unpack sensor values |
| `Makefile`           | Build and install automation for both the kernel module and Python test harness |

---

## 🚀 Getting Started

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
