#!/bin/bash

# Exit immediately if a command exits with a non-zero status
set -e

# The task is the first argument passed to the script
TASK="$1"

case "$TASK" in
    source)
        echo "Sourcing the Zephyr environment..."
        source ${HOME}/zephyrproject/.venv/bin/activate
        source ${HOME}/zephyrproject/zephyr/zephyr-env.sh
        ;;
    build)
        echo "Building the project..."
        west build -b blackpill_f401cc -p always --\
         -DCMAKE_OBJCOPY=$HOME/zephyr-sdk-1.0.1/gnu/arm-zephyr-eabi/bin/arm-zephyr-eabi-objcopy   \
         -DCMAKE_OBJDUMP=$HOME/zephyr-sdk-1.0.1/gnu/arm-zephyr-eabi/bin/arm-zephyr-eabi-objdump
        ;;
    flash)
        echo "Flashing the application..."
        west flash --runner openocd
        ;;
    clean)
        echo "Cleaning up build artifacts..."
        rm -rf build
        ;;
    pristine)
        ;;
    *)
        echo "Usage: $0 {build|run|clean}"
        exit 1
        ;;
esac
