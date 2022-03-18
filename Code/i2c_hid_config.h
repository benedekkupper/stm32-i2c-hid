#ifndef __I2C_HID_CONFIG_H_
#define __I2C_HID_CONFIG_H_

#ifdef __cplusplus

namespace i2c_hid
{
    class device;
}

i2c_hid::device& get_device();

extern "C"
{
#endif

void create_i2c_hid_device(void);

void test_i2c_hid_device(void);

#ifdef __cplusplus
}
#endif

#endif // __I2C_HID_CONFIG_H_
