#!/bin/bash

# Exit on error
set -e

# Text colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Helper function for section headers
section() {
    echo -e "\n${BLUE}==== $1 ====${NC}"
}

# Get USB device path for DualShock 4 controller
get_device_path() {
    # Look for Sony DualShock 4 controller (VID=054c, PID=05c4)
    for device in /sys/bus/usb/devices/*; do
        if [ -f "$device/idVendor" ] && [ -f "$device/idProduct" ]; then
            vendor=$(cat "$device/idVendor" 2>/dev/null || echo "")
            product=$(cat "$device/idProduct" 2>/dev/null || echo "")
            if [ "$vendor" = "054c" ] && [ "$product" = "05c4" ]; then
                basename "$device"
                return 0
            fi
        fi
    done
    return 1
}

section "Building kernel modules"
echo "Cleaning previous build..."
make clean
echo "Building int_stack and int_stack_usbkey modules..."
make

section "Building userspace utility"
echo "Building kernel_stack userspace utility..."
make -f Makefile.user

section "Checking for loaded modules"
# Check if modules are already loaded
if lsmod | grep -q int_stack_usbkey; then
    echo "USB key module is already loaded. Unloading it first..."
    sudo rmmod int_stack_usbkey
fi

if lsmod | grep -q int_stack; then
    echo "Stack module is already loaded. Unloading it first..."
    sudo rmmod int_stack
fi

section "Loading modules"
echo "Loading the int_stack module (provides character device functionality)..."
sudo insmod int_stack.ko
echo "Loading the int_stack_usbkey module (provides USB detection)..."
sudo insmod int_stack_usbkey.ko

section "Device node status before binding"
if [ -e /dev/int_stack ]; then
    echo -e "${GREEN}Device node /dev/int_stack exists${NC}"
else
    echo -e "${YELLOW}Device node /dev/int_stack does not exist yet${NC}"
    echo "This is expected, as the USB device hasn't been bound to our driver yet."
fi

section "Finding the Sony DualShock 4 controller"
USB_DEVICE=$(get_device_path)
if [ -z "$USB_DEVICE" ]; then
    echo -e "${RED}Error: Could not find Sony DualShock 4 controller.${NC}"
    echo "Make sure it's connected and try again."
    exit 1
fi

echo -e "Found Sony DualShock 4 controller at: ${GREEN}$USB_DEVICE${NC}"
vendor=$(cat "/sys/bus/usb/devices/$USB_DEVICE/idVendor")
product=$(cat "/sys/bus/usb/devices/$USB_DEVICE/idProduct")
echo "VID:PID = $vendor:$product"

# Check if interface is already bound to a driver
INTERFACE="${USB_DEVICE}:1.0"
BOUND_DRIVER=""
if [ -L "/sys/bus/usb/devices/$INTERFACE/driver" ]; then
    BOUND_DRIVER=$(basename $(readlink "/sys/bus/usb/devices/$INTERFACE/driver"))
    echo -e "Interface is currently bound to driver: ${YELLOW}$BOUND_DRIVER${NC}"
fi

section "Binding the controller to our driver"
if [ -n "$BOUND_DRIVER" ]; then
    echo "Unbinding from current driver ($BOUND_DRIVER)..."
    echo "$INTERFACE" | sudo tee "/sys/bus/usb/drivers/$BOUND_DRIVER/unbind" > /dev/null
fi

echo "Binding to our int_stack_usbkey driver..."
echo "$INTERFACE" | sudo tee /sys/bus/usb/drivers/int_stack_usbkey/bind > /dev/null

# Wait a moment for the device node to be created
sleep 1

section "Device node status after binding"
if [ -e /dev/int_stack ]; then
    echo -e "${GREEN}Success! Device node /dev/int_stack now exists${NC}"
    # Set permissions to allow non-root access
    sudo chmod 666 /dev/int_stack
else
    echo -e "${RED}Error: Device node /dev/int_stack was not created.${NC}"
    echo "Check dmesg for errors:"
    dmesg | tail -20
    exit 1
fi

section "Testing stack operations"
echo "Setting stack size to 5..."
./kernel_stack set-size 5
echo "Pushing values onto the stack..."
./kernel_stack push 10
./kernel_stack push 20
./kernel_stack push 30
echo "Current stack contents:"
./kernel_stack unwind

section "Setup complete!"
echo -e "You can now use the ${GREEN}kernel_stack${NC} utility to interact with the stack:"
echo "  ./kernel_stack set-size <size> - Change the stack size"
echo "  ./kernel_stack push <value>    - Push a value onto the stack"
echo "  ./kernel_stack pop             - Pop a value from the stack"
echo "  ./kernel_stack unwind          - Display all values in the stack"
echo
echo "To clean up, run: ./cleanup.sh"
echo
echo -e "${YELLOW}Note:${NC} If you disconnect the Sony DualShock 4 controller, the /dev/int_stack device will disappear"
echo "and you'll need to run this script again when you reconnect it." 