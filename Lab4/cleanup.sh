#!/bin/bash

# Don't exit on error, as we want to try all cleanup steps
set +e

echo "=========== Cleanup started ==========="

echo "Checking for device file..."
if [ -e /dev/int_stack ]; then
    echo "- Removing device file /dev/int_stack"
    sudo rm /dev/int_stack
    if [ $? -eq 0 ]; then
        echo "  → Success: Device file removed"
    else
        echo "  → Warning: Failed to remove device file"
    fi
else
    echo "- Device file not found, skipping"
fi

echo "Checking if module is loaded..."
if lsmod | grep -q int_stack; then
    echo "- Unloading int_stack module"
    sudo rmmod int_stack
    if [ $? -eq 0 ]; then
        echo "  → Success: Module unloaded"
    else
        echo "  → Warning: Failed to unload module, it might be in use"
        echo "    You may need to reboot to fully clean up"
    fi
else
    echo "- Module not loaded, skipping"
fi

echo "Cleaning build files..."
echo "- Running kernel module clean"
make clean
echo "- Running userspace utility clean"
make -f Makefile.user clean

echo "=========== Cleanup completed ==========="
echo "Environment is now clean and ready for fresh builds." 