#!/bin/bash

# Don't exit on error, as we want to try all cleanup steps
set +e

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

section "Cleanup started"

section "Checking for device file"
if [ -e /dev/int_stack ]; then
    echo "Removing device file /dev/int_stack"
    sudo rm /dev/int_stack
    if [ $? -eq 0 ]; then
        echo -e "${GREEN}Success:${NC} Device file removed"
    else
        echo -e "${RED}Warning:${NC} Failed to remove device file"
    fi
else
    echo -e "${YELLOW}Device file not found, skipping${NC}"
fi

section "Checking for loaded modules"
if lsmod | grep -q int_stack_usbkey; then
    echo "Unloading int_stack_usbkey module"
    sudo rmmod int_stack_usbkey
    if [ $? -eq 0 ]; then
        echo -e "${GREEN}Success:${NC} int_stack_usbkey module unloaded"
    else
        echo -e "${RED}Warning:${NC} Failed to unload int_stack_usbkey module, it might be in use"
    fi
else
    echo -e "${YELLOW}int_stack_usbkey module not loaded, skipping${NC}"
fi

if lsmod | grep -q int_stack; then
    echo "Unloading int_stack module"
    sudo rmmod int_stack
    if [ $? -eq 0 ]; then
        echo -e "${GREEN}Success:${NC} int_stack module unloaded"
    else
        echo -e "${RED}Warning:${NC} Failed to unload int_stack module, it might be in use"
        echo "You may need to reboot to fully clean up"
    fi
else
    echo -e "${YELLOW}int_stack module not loaded, skipping${NC}"
fi

section "Rebinding controller to original driver"
# Look for Sony DualShock 4 controller
for device in /sys/bus/usb/devices/*; do
    if [ -f "$device/idVendor" ] && [ -f "$device/idProduct" ]; then
        vendor=$(cat "$device/idVendor" 2>/dev/null || echo "")
        product=$(cat "$device/idProduct" 2>/dev/null || echo "")
        if [ "$vendor" = "054c" ] && [ "$product" = "05c4" ]; then
            USB_DEVICE=$(basename "$device")
            INTERFACE="${USB_DEVICE}:1.0"
            echo "Found controller at $INTERFACE, attempting to rebind to default driver..."
            
            # Unbind from our driver if still bound
            if [ -L "/sys/bus/usb/devices/$INTERFACE/driver" ]; then
                current_driver=$(basename $(readlink "/sys/bus/usb/devices/$INTERFACE/driver"))
                echo "Currently bound to: $current_driver"
                
                if [ "$current_driver" = "int_stack_usbkey" ]; then
                    echo "Unbinding from our driver..."
                    echo "$INTERFACE" | sudo tee "/sys/bus/usb/drivers/int_stack_usbkey/unbind" > /dev/null
                fi
            fi
            
            # Try to trigger re-binding to default driver
            echo "$INTERFACE" | sudo tee /sys/bus/usb/drivers/usbhid/bind > /dev/null 2>&1
            if [ $? -eq 0 ]; then
                echo -e "${GREEN}Successfully rebound to usbhid driver${NC}"
            else
                echo -e "${YELLOW}Note:${NC} Couldn't rebind controller to default driver"
                echo "The controller will rebind on its own when re-authorized or reconnected"
            fi
            break
        fi
    fi
done

section "Cleaning build files"
echo "Running kernel module clean"
make clean
echo "Running userspace utility clean"
make -f Makefile.user clean

section "Cleanup completed"
echo -e "${GREEN}Environment is now clean and ready for fresh builds.${NC}" 