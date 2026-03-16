// ----------------------------------------------------- Includes ------------------------------------------------------
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/ktime.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/atomic.h>

// ------------------------------------------------ Macros & Defines ---------------------------------------------------
#define DHT_GPIO_QUERY 22
#define DHT_GPIO_OFFSET 512
#define DEVICE_NAME "dht22"
#define DEBUG 0 /* Debug messages for Kernel module functionality */

#if DEBUG == 1
#define debug(cmd, ...) printk(cmd, ##__VA_ARGS__)
#else
#define debug(cmd, ...)                                                                                                \
    do                                                                                                                 \
    {                                                                                                                  \
    } while (0)
#endif

// ------------------------------------------------------ Typedef ------------------------------------------------------
typedef struct
{
    uint16_t temperature;
    uint16_t humidity;
    uint8_t CRC;
    uint8_t validity;
    uint8_t done;
} DHT22_data_t;

// ---------------------------------------------------- Global Vars ----------------------------------------------------
static DEFINE_MUTEX(dht22_mutex);

static dev_t dev_num;
static struct cdev dht22_cdev;
static struct class *dht22_class;
static struct gpio_desc *dht22_gpio;

uint64_t prevTime_ns = 0;
int irq = -1;

uint16_t timeBuffer[86];
static atomic_t nInterrupts = ATOMIC_INIT(86);

// ----------------------------------------------------- Prototypes ----------------------------------------------------
void querySensor(DHT22_data_t *returnData);

// ---------------------------------------------------- Functions ------------------------------------------------------

static irqreturn_t dht22_irq_handler(int irq, void *dev_id)
{
    // Increment 'nInterrupts' and keep a local statyic copy to avoid race condition
    uint64_t nInterrupts_loc = atomic_inc_return(&nInterrupts) - 1;
    
    // Same assumption as 'nInterrupts' but for 'prevTime'
    uint64_t prevTime_ns_loc = prevTime_ns;
    
    // Ignore interrupts if we aren't actively listening for sensor data
    if (nInterrupts_loc < 86)
    {
        uint64_t now_ns = ktime_to_ns(ktime_get());
        timeBuffer[nInterrupts_loc] = (uint16_t)((now_ns - prevTime_ns_loc) / 1000);
        prevTime_ns = now_ns;
    }
    return IRQ_HANDLED;
}

static int dht22_open(struct inode *inode, struct file *file)
{
    DHT22_data_t *data;
    int ret;

    // Mutex check to prevent multiple concurrent opens
    if (!mutex_trylock(&dht22_mutex))
    {
        printk("DHT22 Kernel - Resource is busy!");
        return -EBUSY;
    }

    // Dynamic Memory Allocation
    data = kzalloc(sizeof(DHT22_data_t), GFP_KERNEL);
    if (!data)
    {
        printk("DHT22 Kernel - Error - Unable to allocate memory in the file space");
        mutex_unlock(&dht22_mutex);
        return -ENOMEM;
    }

    // Request IRQ when the device is actually opened
    irq = gpiod_to_irq(dht22_gpio);
    if (irq < 0) {
        ret = irq;
        goto err_free;
    }

    // We use IRQ_NO_AUTOEN because we only want it active during the querySensor() burst
    ret = request_irq(irq, dht22_irq_handler, 
                      IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING | IRQF_NO_AUTOEN, 
                      DEVICE_NAME, NULL);
    if (ret) 
    {
        printk("DHT22 Kernel - Error - Failed to request IRQ %d", irq);
        goto err_free;
    }

    file->private_data = data;
    return 0;

err_free:
    kfree(data);
    mutex_unlock(&dht22_mutex);
    return ret;
}

static int dht22_release(struct inode *inode, struct file *file)
{
    // Clean up IRQ
    if (irq >= 0) {
        free_irq(irq, NULL);
    }

    // Free per-file data
    if (file->private_data) {
        kfree(file->private_data);
    }

    // Unlock the device for the next user
    mutex_unlock(&dht22_mutex);
    
    pr_info("DHT22: Device closed and resources released\n");
    return 0;
}

static ssize_t dht22_read(struct file *file, char __user *buf, size_t len, loff_t *offset)
{
    DHT22_data_t *returnData = file->private_data;

    if (*offset == 0)
    {
        querySensor(returnData);
        if (returnData->done != 1)
        {
            debug("DHT22 Kernel - Error - Unable to read from the sensor");
            return -EIO;
        }
    }

    // If offset pointer matches message len (or more), finish
    if (*offset >= sizeof(DHT22_data_t))
        return 0;

    // If asking to read more info than available, adjust 'len'
    if (len > (sizeof(DHT22_data_t) - *offset))
        len = sizeof(DHT22_data_t) - *offset;

    if (copy_to_user(buf, (char *)returnData + *offset, len))
        return -EFAULT;

    *offset += len;
    return len;
}

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = dht22_open,
    .read = dht22_read,
    .release = dht22_release,
};

static int __init dht22_init(void)
{
    int ret;

    // Allocate major / minor dinamically
    ret = alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME);
    if (ret < 0)
    {
        pr_err("Failed to allocate char device region\n");
        return ret;
    }

    // Init. cdev structure and register
    cdev_init(&dht22_cdev, &fops);
    dht22_cdev.owner = THIS_MODULE;
    ret = cdev_add(&dht22_cdev, dev_num, 1);
    if (ret < 0)
    {
        unregister_chrdev_region(dev_num, 1);
        pr_err("Failed to add cdev\n");
        return ret;
    }

    // Create class and device into /dev
    dht22_class = class_create("dht22_class");
    if (IS_ERR(dht22_class))
    {
        cdev_del(&dht22_cdev);
        unregister_chrdev_region(dev_num, 1);
        pr_err("Failed to create class\n");
        return PTR_ERR(dht22_class);
    }
    device_create(dht22_class, NULL, dev_num, NULL, DEVICE_NAME);

    // Configure the GPIO pins
    dht22_gpio = gpio_to_desc(DHT_GPIO_OFFSET + DHT_GPIO_QUERY);
    if (!dht22_gpio)
    {
        printk("DHT22 Kernel - Error getting pin for data query\n");
        return -ENODEV;
    }

    // End of device registering - All ok
    printk("DHT22 driver loaded! Device created at /dev/%s\n", DEVICE_NAME);
    return 0;
}

static void __exit dht22_exit(void)
{
    // Destroy the device node in /dev first
    if (dht22_class && dev_num)
        device_destroy(dht22_class, dev_num);

    // Destroy the class
    if (dht22_class)
        class_destroy(dht22_class);

    // Delete the character device
    // (This unlinks the fops (open/read/release) from the kernel)
    cdev_del(&dht22_cdev);

    // Finally, release the major/minor numbers
    unregister_chrdev_region(dev_num, 1);
    printk("DHT22 driver unloaded\n");
}

void querySensor(DHT22_data_t *returnData)
{
    int status;

    atomic_set(&nInterrupts, 0);
    enable_irq(irq);

    // Configure pin as output
    status = gpiod_direction_output(dht22_gpio, 1);
    if (status)
    {
        printk("DHT22 Kernel - Error -  Error setting data query pin to output\n");
        returnData->validity = 0;
        return;
    }
    debug("DHT22 Kernel - Query pin set as output");

    // Get the first reference timestamp - before querying the sensor
    prevTime_ns = ktime_to_ns(ktime_get());
    debug("DHT22 Kernel - First timestamp before first edge - %llu ns.", prevTime_ns);

    // Query the DHT22
    gpiod_set_value(dht22_gpio, 0); // Set LOW
    mdelay(20);
    gpiod_set_value(dht22_gpio, 1); // Set HIGH
    debug("DHT22 Kernel - DHT22 sensor queried for 20ms");

    // Configure the query pin as input
    status = gpiod_direction_input(dht22_gpio);
    if (status)
    {
        printk("DHT22 Kernel - Error - Error setting data query pin to input\n");
        returnData->validity = 0;
        return;
    }
    debug("DHT22 Kernel - Query pin set as input");

    // Wait 20~21ms
    usleep_range(20000, 21000);

    // Process the received buffer to extract valid data
    disable_irq(irq);
    debug("DHT22 Kernel - Total interrupts captured: %d", atomic_read(&nInterrupts));

    // Validate and process the results
    if (atomic_read(&nInterrupts) >= 85 && atomic_read(&nInterrupts) <= 87)
    {
        uint64_t decodedStream = 0;
        // Check for the 20ms query + the answer of the two 80us sync pulses
        uint8_t syncCntr = 0, idxSync;
        for (idxSync = 0; idxSync < 5; idxSync++)
        {
            if (timeBuffer[idxSync] > 19000 && timeBuffer[idxSync] < 21000 && timeBuffer[idxSync + 2] > 50 &&
                timeBuffer[idxSync + 2] < 100 && timeBuffer[idxSync + 3] > 50 && timeBuffer[idxSync + 3] < 100)
            {
                syncCntr = 1;
                idxSync += 4;
                break;
            }
        }

        if (syncCntr != 1)
        {
            printk("DHT22 Kernel - Error - No sync pulses (80us) found");
            returnData->validity = 0;
            return;
        }

        debug("DHT22 Kernel - Sync pulses found on %d and %d", idxSync - 2, idxSync - 1);

        // Loop over the stream to decode the actual data
        uint8_t idxData = 0;
        for (uint8_t idxBuf = idxSync; idxBuf < 80 + idxSync; idxBuf += 2)
        {
            // Bit 0 - 50us + 26us
            // Bit 1 - 50us + 70us
            if (timeBuffer[idxBuf] >= 30 && timeBuffer[idxBuf] <= 70 && timeBuffer[idxBuf + 1] >= 6 &&
                timeBuffer[idxBuf + 1] <= 46)
            {
                // Zero - No action
                debug("DHT22 Kernel - Measurement - Got a '0' - Time: %d - "
                      "Dev.: %d",
                      timeBuffer[idxBuf + 1], timeBuffer[idxBuf + 1] - 26);
            }
            else if (timeBuffer[idxBuf] >= 30 && timeBuffer[idxBuf] <= 70 && timeBuffer[idxBuf + 1] >= 50 &&
                     timeBuffer[idxBuf + 1] <= 90)
            {
                decodedStream = (decodedStream | (1ULL << (39 - idxData)));
                debug("DHT22 Kernel - Measurement - Got a '1' - Time: %d - "
                      "Dev.: %d",
                      timeBuffer[idxBuf + 1], timeBuffer[idxBuf + 1] - 70);
            }
            else
            {
                printk("DHT22 Kernel - Error - Wrong timing -> %d and %d on "
                       "idx %d",
                       timeBuffer[idxBuf], timeBuffer[idxBuf + 1], idxBuf);
                returnData->validity = 0;
                return;
            }
            idxData++;
        }

        returnData->humidity = (uint16_t)((decodedStream & 0xAAAAAAFFFF000000) >> (6 * 4));
        returnData->temperature = (int16_t)((decodedStream & 0xAAAAAA0000FFFF00) >> (2 * 4));
        returnData->CRC = (uint8_t)((decodedStream & 0xAAAAAA00000000FF) >> (0 * 4));

        uint8_t CRC_calc =
            (uint8_t)((uint8_t)(returnData->temperature >> 8) + (uint8_t)(returnData->temperature & 0xFF) +
                      (uint8_t)(returnData->humidity >> 8) + (uint8_t)(returnData->humidity & 0xFF));

        if (returnData->CRC != CRC_calc)
        {
            printk("DHT22 Kernel - Error - Wrong CRC -> Received %d and "
                   "Calculated %d",
                   returnData->CRC, CRC_calc);
            returnData->validity = 0;
            return;
        }
        else
        {
            returnData->validity = 1;
        }
    }
    else
    {
        printk("DHT22 Kernel - Error - Missing edges");
        returnData->validity = 0;
        return;
    }

    // Print obtained values on kernel log
    if (returnData->validity)
    {
        debug("DHT22 Kernel - Values obtained - Temp: %d.%d C and Humidity: "
              "%d.%d %%",
              returnData->temperature / 10, returnData->temperature % 10, returnData->humidity / 10,
              returnData->humidity % 10);
    }
    else
    {
        debug("DHT22 Kernel - Values obtained - Temp: -- C and Humidity: -- %%");
    }
    returnData->done = 1;
}

// ------------------------------------------------ Module metadata --------------------------------------------------
module_init(dht22_init);
module_exit(dht22_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Matheus Sozza");
MODULE_DESCRIPTION("Simple device driver for interfacing with the DHT22 "
                   "Temperature and Humidity sensor");