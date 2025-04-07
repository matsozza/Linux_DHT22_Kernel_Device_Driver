#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/init.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/irqreturn.h>
#include <linux/types.h>


#define DHT_GPIO_QUERY 22
//#define DHT_GPIO_DATA 26
#define DHT_GPIO_OFFSET 512

#define DEVICE_NAME "dht22"

static dev_t dev_num;
static struct cdev dht22_cdev;
static struct class *dht22_class;

static struct gpio_desc *dht22_gpio;
int irq_number;
uint16_t timeBuffer[86];
uint16_t nInterrupts = 0;
ktime_t prevTime = 0, currTime = 0;
u64 prevTime_us = 0, currTime_us = 0;


static irqreturn_t dht22_irq_handler(int irq, void *dev_id)
{
    ktime_t currTime = ktime_get();           // monotonic, high-res
    u64 currTime_us = ktime_to_ns(currTime);           // convert to nanoseconds

    printk("DHT22 IRQ triggered! Num: %d -- Time: %d\n", (nInterrupts++), (currTime_us-prevTime_us)/1000);

    prevTime_us=currTime_us;
    return IRQ_HANDLED;
}

static int dht22_open(struct inode *inode, struct file *file)
{
    return 0;
}

static int dht22_release(struct inode *inode, struct file *file)
{
    return 0;
}

static ssize_t dht22_read(struct file *file, char __user *buf, size_t len, loff_t *offset)
{
    // Configure pin as output
    int status = gpiod_direction_output(dht22_gpio, 1);
	if (status) {
		printk("DHT22 Kernel - Error setting data query pin to output\n");
		return status;
	}
    printk("DHT22 Kernel - Query pin set as output");

    // Get the first reference timestamp - before querying the sensor
    ktime_t prevTime = ktime_get();           // monotonic, high-res
    u64 prevTime_us = ktime_to_ns(prevTime);           // convert to nanoseconds
    printk("DHT22 Kernel - First timestamp captured before first trigger - %lld us.", prevTime_us);

    // Query the DHT22
    gpiod_set_value(dht22_gpio, 0);  // Set LOW
    mdelay(20);
    gpiod_set_value(dht22_gpio, 1);  // Set HIGH
    printk("DHT22 Kernel - DHT22 sensor queried for 20ms");

    // Configure the query pin as input
    status = gpiod_direction_input(dht22_gpio);
	if (status) {
		printk("DHT22 Kernel - Error setting data query pin to input\n");
		return status;
	}
    printk("DHT22 Kernel - Query pin set as input");

    // Wait 100ms
    msleep(100);

    // Process the received buffer to extract valid data
    printk("Total interrupts captured: %d", nInterrupts);
    nInterrupts=0;

    // Send a message
    /*
    const char *msg = "Hello from DHT22!\n";
    size_t msg_len = strlen(msg);

    if (*offset >= msg_len)
        return 0; // EOF

    if (len > msg_len - *offset)
        len = msg_len - *offset;

    if (copy_to_user(buf, msg + *offset, len))
        return -EFAULT;       

    *offset += len;*/    
    return 0; // EOF - Run just once!
}

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = dht22_open,
    .read = dht22_read,
    .release = dht22_release,
};

static int __init dht22_init(void)
{
    int ret, irq;

    // Aloca major/minor dinamicamente
    ret = alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME);
    if (ret < 0) {
        pr_err("Failed to allocate char device region\n");
        return ret;
    }

    // Inicializa a estrutura cdev e registra
    cdev_init(&dht22_cdev, &fops);
    dht22_cdev.owner = THIS_MODULE;
    ret = cdev_add(&dht22_cdev, dev_num, 1);
    if (ret < 0) {
        unregister_chrdev_region(dev_num, 1);
        pr_err("Failed to add cdev\n");
        return ret;
    }

    // Cria classe e dispositivo em /dev
    dht22_class = class_create("dht22_class");
    if (IS_ERR(dht22_class)) {
        cdev_del(&dht22_cdev);
        unregister_chrdev_region(dev_num, 1);
        pr_err("Failed to create class\n");
        return PTR_ERR(dht22_class);
    }
    device_create(dht22_class, NULL, dev_num, NULL, DEVICE_NAME);

    // Configure the GPIO pins
    dht22_gpio = gpio_to_desc(DHT_GPIO_OFFSET + DHT_GPIO_QUERY);
	if (!dht22_gpio) {
		printk("DHT22 Kernel - Error getting pin for data query\n");
		return -ENODEV;
	}
    printk("DHT22 Kernel - Data query pin successfully requested\n");

    // Configure the Query pin as interruptable
    irq = gpio_to_irq(DHT_GPIO_OFFSET + DHT_GPIO_QUERY);
    ret = request_irq(irq, dht22_irq_handler,
        IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING,
        "dht22_irq", dht22_gpio);
    if(ret) printk("DHT Kernel - Failed to set query pin as interruptable");
    printk("DHT22 Kernel - Query pin configure to accept interrupts");
    

    // End of device registering - return success.
    pr_info("DHT22 driver loaded! Device created at /dev/%s\n", DEVICE_NAME);
    return 0;
}

static void __exit dht22_exit(void)
{
    device_destroy(dht22_class, dev_num);
    class_destroy(dht22_class);
    cdev_del(&dht22_cdev);
    unregister_chrdev_region(dev_num, 1);
    gpio_free(DHT_GPIO_QUERY);

    pr_info("DHT22 driver unloaded\n");
}

module_init(dht22_init);
module_exit(dht22_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Matheus Sozza");
MODULE_DESCRIPTION("Simple device driver for interfacing with the DHT22 Temperature and Humidity sensor");
