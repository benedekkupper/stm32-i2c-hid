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
interacts with the hardware via the HAL library.
All you need to do is replace the current STM32CubeMX configuration with your choice
and generate the new cmake project with it.

Porting this code to another MCU type will require porting `hal_i2c_slave.cpp` to interact with the I2C and GPIO
FW of your choice of silicon.

## Customizing the HID application

You can easily extend the HID functionality by modifying the report descriptor and adapting the app code.
Either find the existing application and usages in the HID usage tables,
or define your own vendor-specific usage page(s) - even by adding them to your [hid-usage-tables] fork.
The [hid-rp] library allows you to perform compile-time verification of the report descriptor,
helping you avoid some mistakes. This verification isn't all-encompassing,
and it relies on C++ exceptions. That's why there is a separate cmake target, that compiles the related source file
for verification with exceptions enabled. Exceptions are disabled in the firmware to be flashed itself.

## Host configuration

This project is tested with a Raspberry Pi 400, please refer to [this guide][raspberry-guide] on how to
configure a Linux kernel to use HID over I2C.


[32F072BDISCOVERY]: https://www.st.com/en/evaluation-tools/32f072bdiscovery.html
[hid-usage-tables]: https://github.com/IntergatedCircuits/hid-usage-tables
[hid-rp]: https://github.com/IntergatedCircuits/hid-rp
[raspberry-guide]: https://github.com/NordicPlayground/nrf52-i2c-hid-demo/blob/master/Raspbian/Raspbian_HID-Over-I2C_README.md
