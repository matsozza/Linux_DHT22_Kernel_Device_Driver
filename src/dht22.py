import time
import struct
from collections import namedtuple

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
        print(f"Error: Device file not found at {device_path}")
        return None
    except OSError as e:
        print(f"Error reading from device: {e}")
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
        print(f"Read {len(raw_data)} bytes from device:")
        print(f"{[hex(d) for d in raw_data]}")
        print(f"{[(d) for d in raw_data]}")
        
        DHT22_Data_t = namedtuple('DHT22_Data_t', 'temperature humidity CRC validity done')
        dht22_data = DHT22_Data_t._make(struct.unpack("<hh???x", raw_data))

        print(f"Unpacked data {dht22_data}")
        if dht22_data.validity and dht22_data.done:
            print(f"Temperature: {dht22_data.temperature/10}")
            print(f"Humidity: {dht22_data.humidity/10}")
        else:
            print("Error - Invalid CRC or data not ready")
            
        return dht22_data
    else:
        return None

if __name__ == "__main__":
    read_dht22_data()