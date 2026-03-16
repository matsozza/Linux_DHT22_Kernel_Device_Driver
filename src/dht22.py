import time
import struct
import logging
import os
from collections import namedtuple

DHT22_Data_t = namedtuple('DHT22_Data_t', 'temperature humidity CRC validity done')

# ==============================
# Configuration
# ==============================
BASE_DIR = os.path.dirname(os.path.abspath(__file__))
LOG_FILE = os.path.join(BASE_DIR, "dht22.log")

# ==============================
# Setup Logging
# ==============================

# Check if we can write to it (if not, remove it)
if not os.access(LOG_FILE, os.W_OK) and os.path.exists(LOG_FILE):
    os.remove(LOG_FILE)

# Recreate it (it will be empty and owned by the current user)
with open(LOG_FILE, "w") as f:
    f.write("Log started\n")
os.chmod(LOG_FILE, 0o666) # Ensure suitable permissions for everyone

logger = logging.getLogger("DHT22")
logger.setLevel(logging.INFO)
logger.propagate = False

# Individual handler
if not logger.handlers:
    fh = logging.FileHandler(LOG_FILE)
    fh.setFormatter(logging.Formatter("%(asctime)s %(levelname)s: %(message)s"))
    fh.setLevel(logging.INFO)
    logger.addHandler(fh)

def _read_from_device(device_path, bytes_to_read):
    """Reads data from a device file.

    Args:
        device_path (str): The path to the device file (e.g., '/dev/ttyUSB0').
        bytes_to_read (int): The number of bytes to read from the device.

    Returns:
        bytes: The data read from the device, or None on error.
    """
    try:
        with open(device_path, 'rb') as device_file:
            data = device_file.read(bytes_to_read)
            return data
    except FileNotFoundError:
        logger.error(f"Error: Device file not found at {device_path}")
        return None
    except OSError as e:
        logger.error(f"Error reading from device: {e}")
        return None

def read_dht22_data():
    """Reads data from DHT22 and decode it.

    Args:
        None

    Returns:
        dht22_data: Named tuple with sensor data
    """    
    # Decode DHT22 data from device driver
    device_path = "/dev/dht22" # Device path
    bytes_to_read = 8 # Number of bytes to read

    # Read raw DHT22 DD data and unpack it in the correct format
    raw_data = _read_from_device(device_path, bytes_to_read)
    if raw_data:
        logger.info(f"Read {len(raw_data)} bytes from device:")
        logger.info(f"{[hex(d) for d in raw_data]}")
        logger.info(f"{[(d) for d in raw_data]}")
        
        dht22_data = DHT22_Data_t._make(struct.unpack("<hh???x", raw_data))

        logger.info(f"Unpacked data {dht22_data}")
    else:
        dht22_data = DHT22_Data_t(temperature=0, humidity=0, CRC=0, validity=False, done=True)
    
    if dht22_data.validity and dht22_data.done:
        logger.info(f"Temperature: {dht22_data.temperature/10}")
        logger.info(f"Humidity: {dht22_data.humidity/10}")
    else:
        logger.error("Error - Invalid CRC or data error - Skipping")

    return dht22_data

if __name__ == "__main__":
    read_dht22_data()