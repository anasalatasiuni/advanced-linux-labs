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

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Student");
MODULE_DESCRIPTION("A kernel module implementing an integer stack");
MODULE_VERSION("1.0");

#define DEVICE_NAME "int_stack"
#define SUCCESS 0
#define DEFAULT_STACK_SIZE 10
#define INT_STACK_MAGIC 'S'

/* Define ioctl commands */
#define SET_STACK_SIZE _IOW(INT_STACK_MAGIC, 1, unsigned int)

/* Stack data structure */
struct int_stack {
    int *data;
    unsigned int size;      /* Current number of elements */
    unsigned int max_size;  /* Maximum capacity */
    struct rw_semaphore rwsem; /* Reader-writer semaphore for synchronization */
};

static int major_number;
static struct int_stack *stack = NULL;

/* Function prototypes */
static int device_open(struct inode *, struct file *);
static int device_release(struct inode *, struct file *);
static ssize_t device_read(struct file *, char __user *, size_t, loff_t *);
static ssize_t device_write(struct file *, const char __user *, size_t, loff_t *);
static long device_ioctl(struct file *, unsigned int, unsigned long);

static struct file_operations fops = {
    .open = device_open,
    .release = device_release,
    .read = device_read,
    .write = device_write,
    .unlocked_ioctl = device_ioctl
};

/* Initialize the stack */
static int stack_init(struct int_stack *s, unsigned int max_size)
{
    if (!s)
        return -1;

    s->data = kmalloc(sizeof(int) * max_size, GFP_KERNEL);
    if (!s->data) {
        printk(KERN_ERR "int_stack: Failed to allocate memory for stack\n");
        return -ENOMEM;
    }

    s->size = 0;
    s->max_size = max_size;
    init_rwsem(&s->rwsem);
    printk(KERN_INFO "int_stack: Initialized stack with size %u\n", max_size);
    return SUCCESS;
}

/* Free the stack */
static void stack_deinit(struct int_stack *s)
{
    if (s) {
        if (s->data) {
            kfree(s->data);
            s->data = NULL;
        }
        s->size = 0;
        s->max_size = 0;
        printk(KERN_INFO "int_stack: Stack deinitialized\n");
    }
}

/* Push an element onto the stack */
static int stack_push(struct int_stack *s, int value)
{
    if (!s || !s->data)
        return -EINVAL;

    if (s->size >= s->max_size) {
        printk(KERN_WARNING "int_stack: Stack overflow, cannot push value %d\n", value);
        return -ERANGE;
    }

    s->data[s->size++] = value;
    printk(KERN_DEBUG "int_stack: Pushed value %d, stack size now %u\n", value, s->size);
    return SUCCESS;
}

/* Pop an element from the stack */
static int stack_pop(struct int_stack *s, int *value)
{
    if (!s || !s->data)
        return -EINVAL;

    if (s->size == 0) {
        printk(KERN_WARNING "int_stack: Stack underflow, cannot pop from empty stack\n");
        return -1; /* Stack is empty */
    }

    *value = s->data[--s->size];
    printk(KERN_DEBUG "int_stack: Popped value %d, stack size now %u\n", *value, s->size);
    return SUCCESS;
}

/* Resize the stack */
static int stack_resize(struct int_stack *s, unsigned int new_size)
{
    int *new_data;

    if (!s || !s->data)
        return -EINVAL;

    if (new_size == 0) {
        printk(KERN_WARNING "int_stack: Cannot resize stack to zero\n");
        return -EINVAL;
    }

    if (new_size < s->size) {
        printk(KERN_WARNING "int_stack: Shrinking stack from %u to %u elements, losing data\n", 
               s->size, new_size);
        /* We're shrinking the stack and losing elements */
        s->size = new_size;
    }

    new_data = kmalloc(sizeof(int) * new_size, GFP_KERNEL);
    if (!new_data) {
        printk(KERN_ERR "int_stack: Failed to allocate memory for resized stack\n");
        return -ENOMEM;
    }

    if (s->size > 0)
        memcpy(new_data, s->data, sizeof(int) * s->size);

    kfree(s->data);
    s->data = new_data;
    s->max_size = new_size;
    printk(KERN_INFO "int_stack: Resized stack to %u elements\n", new_size);

    return SUCCESS;
}

/* Device open function */
static int device_open(struct inode *inode, struct file *file)
{
    /* Create stack if it doesn't exist yet */
    if (!stack) {
        stack = kmalloc(sizeof(struct int_stack), GFP_KERNEL);
        if (!stack) {
            printk(KERN_ERR "int_stack: Failed to allocate memory for stack structure\n");
            return -ENOMEM;
        }

        if (stack_init(stack, DEFAULT_STACK_SIZE) != SUCCESS) {
            kfree(stack);
            stack = NULL;
            printk(KERN_ERR "int_stack: Failed to initialize stack\n");
            return -ENOMEM;
        }
    }

    try_module_get(THIS_MODULE);
    printk(KERN_INFO "int_stack: Device opened\n");
    return SUCCESS;
}

/* Device release function */
static int device_release(struct inode *inode, struct file *file)
{
    module_put(THIS_MODULE);
    printk(KERN_INFO "int_stack: Device released\n");
    return SUCCESS;
}

/* Device read function (pop operation) */
static ssize_t device_read(struct file *filp, char __user *buffer, size_t length, loff_t *offset)
{
    int value;
    int result;

    if (length < sizeof(int)) {
        printk(KERN_WARNING "int_stack: Read buffer too small, need at least %lu bytes\n", sizeof(int));
        return -EINVAL;
    }

    /* Acquire write lock for popping from stack (modifies the stack) */
    down_write(&stack->rwsem);
    result = stack_pop(stack, &value);
    up_write(&stack->rwsem);

    if (result < 0) {
        printk(KERN_INFO "int_stack: Pop from empty stack\n");
        return 0; /* Empty stack returns 0 bytes read */
    }

    /* Copy value to user space */
    if (copy_to_user(buffer, &value, sizeof(int))) {
        printk(KERN_ERR "int_stack: Failed to copy data to user space\n");
        return -EFAULT;
    }

    return sizeof(int);
}

/* Device write function (push operation) */
static ssize_t device_write(struct file *filp, const char __user *buffer, size_t length, loff_t *offset)
{
    int value;
    int result;

    if (length < sizeof(int)) {
        printk(KERN_WARNING "int_stack: Write buffer too small, need at least %lu bytes\n", sizeof(int));
        return -EINVAL;
    }

    /* Get value from user space */
    if (copy_from_user(&value, buffer, sizeof(int))) {
        printk(KERN_ERR "int_stack: Failed to copy data from user space\n");
        return -EFAULT;
    }

    /* Acquire write lock for pushing to stack (modifies the stack) */
    down_write(&stack->rwsem);
    result = stack_push(stack, value);
    up_write(&stack->rwsem);

    if (result < 0) {
        printk(KERN_WARNING "int_stack: Push failed with error %d\n", result);
        return result; /* Return error code */
    }

    return sizeof(int);
}

/* Device ioctl function */
static long device_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    int result = 0;
    unsigned int new_size;

    switch (cmd) {
    case SET_STACK_SIZE:
        if (get_user(new_size, (unsigned int __user *)arg)) {
            printk(KERN_ERR "int_stack: Failed to get size from user space\n");
            return -EFAULT;
        }

        if (new_size == 0) {
            printk(KERN_WARNING "int_stack: Cannot set stack size to zero\n");
            return -EINVAL;
        }

        printk(KERN_INFO "int_stack: Changing stack size to %u\n", new_size);

        /* Acquire write lock for resizing stack (modifies the stack) */
        down_write(&stack->rwsem);
        result = stack_resize(stack, new_size);
        up_write(&stack->rwsem);
        
        if (result < 0) {
            printk(KERN_ERR "int_stack: Stack resize failed with error %d\n", result);
        }
        break;

    default:
        printk(KERN_WARNING "int_stack: Unknown ioctl command %u\n", cmd);
        return -ENOTTY;
    }

    return result;
}

/* Module initialization */
static int __init int_stack_init(void)
{
    major_number = register_chrdev(0, DEVICE_NAME, &fops);

    if (major_number < 0) {
        printk(KERN_ALERT "int_stack: Failed to register a major number\n");
        return major_number;
    }

    printk(KERN_INFO "int_stack: registered with major number %d\n", major_number);
    printk(KERN_INFO "int_stack: create a dev file with 'mknod /dev/%s c %d 0'\n",
           DEVICE_NAME, major_number);

    return SUCCESS;
}

/* Module cleanup */
static void __exit int_stack_exit(void)
{
    /* Free the stack if it exists */
    if (stack) {
        stack_deinit(stack);
        kfree(stack);
        stack = NULL;
    }

    unregister_chrdev(major_number, DEVICE_NAME);
    printk(KERN_INFO "int_stack: module unloaded\n");
}

module_init(int_stack_init);
module_exit(int_stack_exit); 