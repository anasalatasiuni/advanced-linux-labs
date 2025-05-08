#!/bin/bash

# Exit on error
set -e

echo "Building int_stack kernel module..."
make

echo "Building kernel_stack userspace utility..."
make -f Makefile.user

# Check if module is already loaded
if lsmod | grep -q int_stack; then
    echo "Module is already loaded. Unloading it first..."
    sudo rmmod int_stack
fi

echo "Loading the module..."
sudo insmod int_stack.ko

# Get the major number from dmesg
MAJOR=$(dmesg | grep "int_stack: registered with major number" | tail -n 1 | awk '{print $NF}')

if [ -z "$MAJOR" ]; then
    echo "Error: Could not find major number for int_stack module."
    sudo rmmod int_stack
    exit 1
fi

# Check if device file already exists
if [ -e /dev/int_stack ]; then
    echo "Device file already exists. Removing it..."
    sudo rm /dev/int_stack
fi

echo "Creating device file with major number $MAJOR..."
sudo mknod /dev/int_stack c $MAJOR 0
sudo chmod 666 /dev/int_stack

echo "Setup complete! You can now use the kernel_stack utility:"
echo "  ./kernel_stack set-size <size>"
echo "  ./kernel_stack push <value>"
echo "  ./kernel_stack pop"
echo "  ./kernel_stack unwind"
echo
echo "To clean up, run: ./cleanup.sh" 