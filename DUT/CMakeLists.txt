# Zephyr application: Pix-in-the-Loop DUT persona ("VENT-CTRL") for ESP32-S3.
#
# Build:  west build -b esp32s3_devkitc/esp32s3/procpu . -p auto
# Flash:  west flash
#
# The persona emulates a ventilation/climate-controller node well enough to
# exercise the bench's Phase 2 (Bus Conformance) suite: a UART command link, a
# UART sensor peer, an SPI master, and a GPIO interrupt responder.
cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(vent_ctrl_dut)

target_sources(app PRIVATE
    src/dut_main.c
    src/cmd_link.c
    src/sensor_peer.c
    src/pwm_actuators.c
    src/gpio_io.c
    src/adc_dut.c
)
zephyr_include_directories(src)
