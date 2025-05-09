/*
 * int_stack.c - Character device driver implementing a stack for integers
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/rwsem.h>
#include <linux/ioctl.h>
#include <linux/device.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Mohammad Anas Alatasi");
MODULE_DESCRIPTION("Integer stack character device");
MODULE_VERSION("1.2");

/* Character device definitions */
#define DEVICE_NAME        "int_stack"
#define SUCCESS            0
#define DEFAULT_STACK_SIZE 10
#define INT_STACK_MAGIC    'S'
#define SET_STACK_SIZE     _IOW(INT_STACK_MAGIC, 1, unsigned int)

/* Stack data structure */
struct int_stack {
    int               *data;
    unsigned int       size;      /* current # elements */
    unsigned int       max_size;  /* capacity */
    struct rw_semaphore rwsem;    /* for concurrency */
};

/* File‐ops prototypes */
static int     device_open(struct inode *, struct file *);
static int     device_release(struct inode *, struct file *);
static ssize_t device_read(struct file *, char __user *, size_t, loff_t *);
static ssize_t device_write(struct file *, const char __user *, size_t, loff_t *);
static long    device_ioctl(struct file *, unsigned int, unsigned long);

/* File‐ops table */
static struct file_operations fops = {
    .open           = device_open,
    .release        = device_release,
    .read           = device_read,
    .write          = device_write,
    .unlocked_ioctl = device_ioctl,
};

/* In‐kernel stack pointer */
static struct int_stack *stack = NULL;

/* Major number for /dev/int_stack */
static int major_number;

/* Class/device for sysfs + udev */
static struct class  *int_stack_class;
static struct device *int_stack_device;

/* —— Stack management —— */

static int stack_init(struct int_stack *s, unsigned int max_size)
{
    if (!s)
        return -EINVAL;
    s->data = kmalloc(sizeof(int) * max_size, GFP_KERNEL);
    if (!s->data)
        return -ENOMEM;
    s->size     = 0;
    s->max_size = max_size;
    init_rwsem(&s->rwsem);
    printk(KERN_INFO "int_stack: Initialized with capacity %u\n", max_size);
    return SUCCESS;
}

static void stack_deinit(struct int_stack *s)
{
    if (s && s->data) {
        kfree(s->data);
        s->data = NULL;
        s->size = s->max_size = 0;
        printk(KERN_INFO "int_stack: Deinitialized\n");
    }
}

static int stack_push(struct int_stack *s, int value)
{
    if (!s || !s->data)
        return -EINVAL;
    if (s->size >= s->max_size) {
        printk(KERN_WARNING "int_stack: Overflow, cannot push %d\n", value);
        return -ERANGE;
    }
    s->data[s->size++] = value;
    printk(KERN_DEBUG "int_stack: Pushed %d (size=%u)\n", value, s->size);
    return SUCCESS;
}

static int stack_pop(struct int_stack *s, int *value)
{
    if (!s || !s->data)
        return -EINVAL;
    if (s->size == 0) {
        printk(KERN_WARNING "int_stack: Underflow\n");
        return -1;
    }
    *value = s->data[--s->size];
    printk(KERN_DEBUG "int_stack: Popped %d (size=%u)\n", *value, s->size);
    return SUCCESS;
}

static int stack_resize(struct int_stack *s, unsigned int new_size)
{
    int *new_data;
    if (!s || !s->data || new_size == 0)
        return -EINVAL;
    if (new_size < s->size) {
        printk(KERN_WARNING "int_stack: Shrinking %u→%u, data lost\n",
               s->size, new_size);
        s->size = new_size;
    }
    new_data = kmalloc(sizeof(int) * new_size, GFP_KERNEL);
    if (!new_data)
        return -ENOMEM;
    if (s->size)
        memcpy(new_data, s->data, sizeof(int) * s->size);
    kfree(s->data);
    s->data     = new_data;
    s->max_size = new_size;
    printk(KERN_INFO "int_stack: Resized to %u\n", new_size);
    return SUCCESS;
}

/* —— Character device methods —— */

static int device_open(struct inode *inode, struct file *file)
{
    if (!stack) {
        stack = kmalloc(sizeof(*stack), GFP_KERNEL);
        if (!stack)
            return -ENOMEM;
        if (stack_init(stack, DEFAULT_STACK_SIZE) != SUCCESS) {
            kfree(stack);
            stack = NULL;
            return -ENOMEM;
        }
    }
    try_module_get(THIS_MODULE);
    printk(KERN_INFO "int_stack: Device opened\n");
    return SUCCESS;
}

static int device_release(struct inode *inode, struct file *file)
{
    module_put(THIS_MODULE);
    printk(KERN_INFO "int_stack: Device released\n");
    return SUCCESS;
}

static ssize_t device_read(struct file *filp, char __user *buffer,
                           size_t length, loff_t *offset)
{
    int value, ret;
    if (length < sizeof(int))
        return -EINVAL;
    down_write(&stack->rwsem);
    ret = stack_pop(stack, &value);
    up_write(&stack->rwsem);
    if (ret < 0)
        return 0; /* empty → EOF */
    if (copy_to_user(buffer, &value, sizeof(int)))
        return -EFAULT;
    return sizeof(int);
}

static ssize_t device_write(struct file *filp, const char __user *buffer,
                            size_t length, loff_t *offset)
{
    int value, ret;
    if (length < sizeof(int))
        return -EINVAL;
    if (copy_from_user(&value, buffer, sizeof(int)))
        return -EFAULT;
    down_write(&stack->rwsem);
    ret = stack_push(stack, value);
    up_write(&stack->rwsem);
    if (ret < 0)
        return ret;
    return sizeof(int);
}

static long device_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    unsigned int new_size;
    int ret = 0;
    if (cmd == SET_STACK_SIZE) {
        if (get_user(new_size, (unsigned int __user *)arg))
            return -EFAULT;
        if (new_size == 0)
            return -EINVAL;
        down_write(&stack->rwsem);
        ret = stack_resize(stack, new_size);
        up_write(&stack->rwsem);
        return ret;
    }
    return -ENOTTY;
}

/* Functions exported for the USB key driver */
int int_stack_create_device(void)
{
    major_number = register_chrdev(0, DEVICE_NAME, &fops);
    if (major_number < 0)
        return major_number;

    int_stack_class = class_create(THIS_MODULE, DEVICE_NAME);
    if (IS_ERR(int_stack_class)) {
        unregister_chrdev(major_number, DEVICE_NAME);
        return PTR_ERR(int_stack_class);
    }

    int_stack_device = device_create(int_stack_class, NULL,
                                     MKDEV(major_number, 0),
                                     NULL, DEVICE_NAME);
    if (IS_ERR(int_stack_device)) {
        class_destroy(int_stack_class);
        unregister_chrdev(major_number, DEVICE_NAME);
        return PTR_ERR(int_stack_device);
    }

    printk(KERN_INFO "int_stack: Created device node /dev/%s (major=%d)\n", 
           DEVICE_NAME, major_number);
    return 0;
}
EXPORT_SYMBOL(int_stack_create_device);

void int_stack_remove_device(void)
{
    device_destroy(int_stack_class, MKDEV(major_number, 0));
    class_destroy(int_stack_class);
    unregister_chrdev(major_number, DEVICE_NAME);
    printk(KERN_INFO "int_stack: Removed device node /dev/%s\n", DEVICE_NAME);
}
EXPORT_SYMBOL(int_stack_remove_device);

void int_stack_cleanup(void)
{
    if (stack) {
        stack_deinit(stack);
        kfree(stack);
        stack = NULL;
    }
}
EXPORT_SYMBOL(int_stack_cleanup);

/* Module init & exit */
static int __init int_stack_init(void)
{
    printk(KERN_INFO "int_stack: Stack module loaded\n");
    return 0;
}

static void __exit int_stack_exit(void)
{
    printk(KERN_INFO "int_stack: Stack module unloaded\n");
}

module_init(int_stack_init);
module_exit(int_stack_exit);
