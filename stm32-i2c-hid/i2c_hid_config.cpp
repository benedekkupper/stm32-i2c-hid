extern "C"
{
#include "i2c_hid_config.h"
#include "main.h"
}
#include "hid/demo_app.hpp"
#include "i2c/hid/device.hpp"
#include "st/hal_i2c_slave.hpp"

extern I2C_HandleTypeDef hi2c2;

st::hal_i2c_slave& get_i2c_slave()
{
    static st::hal_i2c_slave slave{hi2c2, &MX_I2C2_Init, EXT_RESET_GPIO_Port, EXT_RESET_Pin};
    return slave;
}

i2c::hid::device& get_device()
{
    // vendor and product ID are inherited from USB
    // version indicates product HW / SW version
    static const i2c::hid::product_info product_info{0x0102, 0x0304, i2c::hid::version(0, 1)};

    // I2C slave address, configure in device-tree
    static const i2c::address bus_address{0x000a};

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

    static i2c::hid::device dev(hid::demo_app::instance(), product_info, get_i2c_slave(),
                                bus_address, hid_desc_address);
    return dev;
}

extern "C" void create_i2c_hid_device()
{
    get_device();
}

extern "C" __weak void test_i2c_hid_device() {}

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

extern "C" void HAL_I2C_AddrCallback([[maybe_unused]] I2C_HandleTypeDef* hi2c,
                                     uint8_t TransferDirection, uint16_t AddrMatchCode)
{
    get_i2c_slave().handle_start(static_cast<i2c::direction>(TransferDirection));
}

extern "C" void HAL_I2C_ListenCpltCallback([[maybe_unused]] I2C_HandleTypeDef* hi2c)
{
    get_i2c_slave().handle_stop();
}

extern "C" void HAL_I2C_SlaveTxCpltCallback([[maybe_unused]] I2C_HandleTypeDef* hi2c)
{
    get_i2c_slave().handle_tx_complete();
}

extern "C" void HAL_I2C_SlaveRxCpltCallback([[maybe_unused]] I2C_HandleTypeDef* hi2c)
{
    get_i2c_slave().handle_rx_complete();
}
