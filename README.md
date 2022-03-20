# HID over I2C demonstration

This project demonstrates how HID over I2C can be used as a flexible protocol between a Linux SoC and an STM32 MCU.
When enumerated by the kernel, the MCU will be treated as 3 separate virtual devices: a keyboard, a mouse
and a raw HID device. The number and purpose of the virtual devices is defined by the descriptors
sent by the MCU.
The demo code sends CAPS_LOCK key events when the button on the development board is pressed,
and lights up an LED when the CAPS_LOCK LED is activated from the host.

## Portability

This code is built for and tested on an [32F072BDISCOVERY][32F072BDISCOVERY] kit, as this is a low cost MCU line,
and the board has a separate connector for I2C communication.

Porting this code within the STM32 product line is guaranteed to be minimal effort, since the implementation
interacts with the hardware via the HAL library. You just need to make sure to configure equivalent STM32Cube
setup for the MCU as it is done in this example project.

Porting this code to another hardware will require changing `i2c_slave.c` to interact with the I2C and GPIO
FW of your choice of silicon.

## Usage pages

To successfully build the software, the HID usage pages must be generated into the `hid-rdf/include/hid/page/` path,
using the [hid-usage-tables] project.

## Host configuration

This project is tested with a Raspberry Pi 400, please refer to [this guide][raspberry-guide] on how to
configure a Linux kernel to use HID over I2C.


[32F072BDISCOVERY]: https://www.st.com/en/evaluation-tools/32f072bdiscovery.html
[hid-usage-tables]: https://github.com/IntergatedCircuits/hid-usage-tables
[raspberry-guide]: https://github.com/NordicPlayground/nrf52-i2c-hid-demo/blob/master/Raspbian/Raspbian_HID-Over-I2C_README.md
