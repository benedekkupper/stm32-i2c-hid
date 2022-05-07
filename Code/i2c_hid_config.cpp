#include "i2c_hid_config.h"
#include "i2c_hid.h"
#include "hid/demo_app.h"
#include "main.h"

i2c_hid::device& get_device()
{
    // vendor and product ID are inherited from USB
    // version indicates product HW / SW version
    static const hid::product_info product_info { 0x0102, 0x0304, hid::version(0,1) };

    // I2C slave address, configure in device-tree
    static const i2c::address bus_address { 0x000a };

    // which memory address to send to the device to get back the HID descriptor
    static const uint16_t hid_desc_address = 0x0001;

#if 0 // example device-tree configuration
    i2c_hid: i2c-hid-device@000a {
        compatible = "hid-over-i2c";
        reg = <0x000a>;
        hid-descr-addr = <0x0001>;
        interrupts-extended = <&gpio 27 8>; // 8 == IRQ_TYPE_LEVEL_LOW
    };
#endif

    static i2c_hid::device dev(hid::demo_app::instance(), product_info,
            bus_address, hid_desc_address);
    return dev;
}

extern "C" void create_i2c_hid_device()
{
    get_device();
}

extern "C" __weak void test_i2c_hid_device()
{
}

void set_led(bool value)
{
    HAL_GPIO_WritePin(GPIOC, LD3_Pin, (GPIO_PinState)(value));
}

extern "C" void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == B1_Pin)
    {
        bool pressed = HAL_GPIO_ReadPin(B1_GPIO_Port, B1_Pin);

        hid::demo_app::instance().button_state_change(pressed);
    }
}
