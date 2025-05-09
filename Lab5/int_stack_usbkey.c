/*
 * int_stack_usbkey.c - USB driver that controls the int_stack character device
 * The char device only appears when a specific USB device is plugged in.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/usb.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Mohamed Anas");
MODULE_DESCRIPTION("USB key driver for int_stack character device");
MODULE_VERSION("1.0");

/* Imported functions from int_stack.c */
extern int  int_stack_create_device(void);
extern void int_stack_remove_device(void);
extern void int_stack_cleanup(void);

/* USB device ID table for Sony DualShock 4 */
static struct usb_device_id ds4_table[] = {
    { USB_DEVICE(0x054c, 0x05c4) },  /* Sony DualShock 4 [CUH-ZCT1x] */
    { USB_DEVICE(0x054c, 0x09cc) },  /* Sony DualShock 4 [CUH-ZCT2x] */
    { }  /* terminator */
};
MODULE_DEVICE_TABLE(usb, ds4_table);

/* USB driver callbacks */
static int  ds4_probe(struct usb_interface *, const struct usb_device_id *);
static void ds4_disconnect(struct usb_interface *);

/* USB driver registration */
static struct usb_driver ds4_driver = {
    .name       = "int_stack_usbkey",
    .probe      = ds4_probe,
    .disconnect = ds4_disconnect,
    .id_table   = ds4_table,
};

/* Called when DualShock 4 is plugged in */
static int ds4_probe(struct usb_interface *intf, const struct usb_device_id *id)
{
    int ret;

    printk(KERN_INFO "int_stack_usbkey: USB device found! VID=%04X, PID=%04X, ifnum=%d\n",
           id->idVendor, id->idProduct, intf->cur_altsetting->desc.bInterfaceNumber);

    /* Create the character device */
    ret = int_stack_create_device();
    if (ret < 0) {
        printk(KERN_ERR "int_stack_usbkey: Failed to create char device, error %d\n", ret);
        return ret;
    }

    printk(KERN_INFO "int_stack_usbkey: USB key plugged (VID=%04X, PID=%04X)\n",
           id->idVendor, id->idProduct);
    return 0;
}

/* Called when the device is unplugged */
static void ds4_disconnect(struct usb_interface *intf)
{
    printk(KERN_INFO "int_stack_usbkey: USB device disconnected - removing char device\n");
    int_stack_remove_device();
    printk(KERN_INFO "int_stack_usbkey: USB key removed\n");
}

/* Module init & exit */
static int __init int_stack_usbkey_init(void)
{
    printk(KERN_INFO "int_stack_usbkey: Module init - registering driver for Sony DS4\n");
    return usb_register(&ds4_driver);
}

static void __exit int_stack_usbkey_exit(void)
{
    printk(KERN_INFO "int_stack_usbkey: Module exit - deregistering driver\n");
    usb_deregister(&ds4_driver);
    int_stack_cleanup();
    printk(KERN_INFO "int_stack_usbkey: Module exited\n");
}

module_init(int_stack_usbkey_init);
module_exit(int_stack_usbkey_exit); 