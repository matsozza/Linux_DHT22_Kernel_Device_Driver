// ----------------------------------------------------- Includes ------------------------------------------------------
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/irqreturn.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/uaccess.h>

// ------------------------------------------------ Macros & Defines ---------------------------------------------------
#define DHT_GPIO_QUERY 22
#define DHT_GPIO_OFFSET 512
#define DEVICE_NAME "dht22"
#define DEBUG 1
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
uint64_t prevTime_us = 0, currTime_us = 0;
int irq_number, irq = -1;
uint16_t timeBuffer[86], nInterrupts = 0;

// ----------------------------------------------------- Prototypes ----------------------------------------------------
void querySensor(DHT22_data_t *returnData);

// ---------------------------------------------------- Functions ------------------------------------------------------

static irqreturn_t dht22_irq_handler(int irq, void *dev_id)
{
    currTime_us = ktime_to_ns(ktime_get());
    timeBuffer[nInterrupts++] = (currTime_us - prevTime_us) / 1000;
    prevTime_us = currTime_us;
    debug("DHT22 Kernel - IRQ triggered! Num: %d -- Time: %d\n", (nInterrupts - 1), timeBuffer[nInterrupts - 1]);
    return IRQ_HANDLED;
}

static int dht22_open(struct inode *inode, struct file *file)
{
    if (!mutex_trylock(&dht22_mutex))
    {
        debug("DHT22 Kernel - Resource is busy!");
        return -EBUSY;
    }

    // Configure the Query pin as interruptable
    irq = gpio_to_irq(DHT_GPIO_OFFSET + DHT_GPIO_QUERY);
    int ret = request_irq(irq, dht22_irq_handler, IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING, "dht22_irq", dht22_gpio);

    if (ret)
    {
        debug("DHT Kernel  - Error - Failed to set query pin as interruptable");
        mutex_unlock(&dht22_mutex);
        return -EPERM;
    }

    DHT22_data_t *data;
    data = kzalloc(sizeof(DHT22_data_t), GFP_KERNEL);
    if (!data)
    {
        debug("DHT22 Kernel - Error - Unable to allocate memory in the file space");
        mutex_unlock(&dht22_mutex);
        return -ENOMEM;
    }
    file->private_data = data;

    debug("DHT22 Kernel - File opened succesfully");
    return 0;
}

static int dht22_release(struct inode *inode, struct file *file)
{
    // Configure pin to ignore interrupts
    free_irq(irq, dht22_gpio);
    kfree(file->private_data);
    debug("DHT22 Kernel - File released succesfully");
    mutex_unlock(&dht22_mutex);
    return 0;
}

static ssize_t dht22_read(struct file *file, char __user *buf, size_t len, loff_t *offset)
{
    DHT22_data_t *returnData = file->private_data;
    const char *msg;
    size_t msg_len;

    if (*offset == 0)
    {
        querySensor(returnData);
        while (returnData->done == 0)
        {
            cpu_relax();
        }
    }

    debug("\nLEN: %d", len);
    debug("\nOFFSET: %d", *offset);

    // Return result to user-space call

    // If offset pointer matches message len (or more), finish
    if (*offset >= sizeof(DHT22_data_t))
    {
        return 0;
    }

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

    // Aloca major/minor dinamicamente
    ret = alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME);
    if (ret < 0)
    {
        pr_err("Failed to allocate char device region\n");
        return ret;
    }

    // Inicializa a estrutura cdev e registra
    cdev_init(&dht22_cdev, &fops);
    dht22_cdev.owner = THIS_MODULE;
    ret = cdev_add(&dht22_cdev, dev_num, 1);
    if (ret < 0)
    {
        unregister_chrdev_region(dev_num, 1);
        pr_err("Failed to add cdev\n");
        return ret;
    }

    // Cria classe e dispositivo em /dev
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
        debug("DHT22 Kernel - Error getting pin for data query\n");
        return -ENODEV;
    }

    /*
    ret = gpio_request(DHT_GPIO_OFFSET + DHT_GPIO_QUERY, "dht22_data");
    if (ret) {
        pr_err("DHT22 Kernel - GPIO already in use or request failed\n");
        return ret;
    }
    */

    debug("DHT22 Kernel - Data query pin successfully requested\n");

    // End of device registering - return success.
    pr_info("DHT22 driver loaded! Device created at /dev/%s\n", DEVICE_NAME);
    return 0;
}

static void __exit dht22_exit(void)
{
    debug(KERN_INFO "DHT22 driver unloaded\n");

    // Desativa interrupção (se registrada)
    if (irq > 0)
        free_irq(irq, dht22_gpio);

    // Libera o GPIO
    if (dht22_gpio)
        gpiod_put(dht22_gpio);

    // Remove o device (se criado)
    if (dev_num)
        device_destroy(dht22_class, dev_num);

    // Destroi a classe
    if (dht22_class)
        class_destroy(dht22_class);

    // Libera o número major (se alocado)
    unregister_chrdev_region(dev_num, 1);
}

void querySensor(DHT22_data_t *returnData)
{
    int status;

    // Configure pin as output
    status = gpiod_direction_output(dht22_gpio, 1);
    if (status)
    {
        debug("DHT22 Kernel - Error -  Error setting data query pin to output\n");
        returnData->validity = 0;
    }
    debug("DHT22 Kernel - Query pin set as output");

    // Get the first reference timestamp - before querying the sensor
    prevTime_us = ktime_to_ns(ktime_get());
    debug("DHT22 Kernel - First timestamp captured before first trigger - %llu "
          "us.",
          prevTime_us);

    // Query the DHT22
    gpiod_set_value(dht22_gpio, 0); // Set LOW
    mdelay(20);
    gpiod_set_value(dht22_gpio, 1); // Set HIGH
    debug("DHT22 Kernel - DHT22 sensor queried for 20ms");

    // Configure the query pin as input
    status = gpiod_direction_input(dht22_gpio);
    if (status)
    {
        debug("DHT22 Kernel - Error - Error setting data query pin to input\n");
        returnData->validity = 0;
    }
    debug("DHT22 Kernel - Query pin set as input");

    // Wait 20ms
    msleep(20);

    // Process the received buffer to extract valid data
    debug("DHT22 Kernel - Total interrupts captured: %d", nInterrupts);

    // Valdate and process the results
    if (nInterrupts >= 85 && nInterrupts <= 87)
    {
        uint64_t decodedStream = 0;
        // Check for the 20000ms query + the answer of the two 80us sync pulses
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
            debug("DHT22 Kernel - Error - No sync pulses (80us) found");
            returnData->validity = 0;
        }
        else
        {
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
                    debug("DHT22 Kernel - Error - Wrong timing -> %d and %d on "
                          "idx %d",
                          timeBuffer[idxBuf], timeBuffer[idxBuf + 1], idxBuf);
                    returnData->validity = 0;
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
                debug("DHT22 Kernel - Error - Wrong CRC -> Received %d and "
                      "Calculated %d",
                      returnData->CRC, CRC_calc);
                returnData->validity = 0;
            }
            else
            {
                returnData->validity = 1;
            }
        }
    }
    else
    {
        debug("DHT22 Kernel - Error - Missing edges");
        returnData->validity = 0;
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
    nInterrupts = 0;
}

// ------------------------------------------------ Module metadata --------------------------------------------------
module_init(dht22_init);
module_exit(dht22_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Matheus Sozza");
MODULE_DESCRIPTION("Simple device driver for interfacing with the DHT22 "
                   "Temperature and Humidity sensor");
