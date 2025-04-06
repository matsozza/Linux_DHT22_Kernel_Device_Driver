#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/init.h>
#include <linux/cdev.h>
#include <linux/device.h>

#define DHT_GPIO 17
#define DEVICE_NAME "dht22"

static dev_t dev_num;
static struct cdev dht22_cdev;
static struct class *dht22_class;

static char data_buf[32] = {0};
static int data_len = 0;

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
    const char *msg = "hello from kernel\n";
    size_t msg_len = strlen(msg);

    if (*offset >= msg_len)
        return 0; // EOF

    if (len > msg_len - *offset)
        len = msg_len - *offset;

    if (copy_to_user(buf, msg + *offset, len))
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

    pr_info("DHT22 driver loaded, device created at /dev/%s\n", DEVICE_NAME);
    return 0;
}

static void __exit dht22_exit(void)
{
    device_destroy(dht22_class, dev_num);
    class_destroy(dht22_class);
    cdev_del(&dht22_cdev);
    unregister_chrdev_region(dev_num, 1);
    gpio_free(DHT_GPIO);

    pr_info("DHT22 driver unloaded\n");
}

module_init(dht22_init);
module_exit(dht22_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Você");
MODULE_DESCRIPTION("Módulo minimalista para DHT22");
