#ifndef __I2C_SLAVE_CONFIG_H_
#define __I2C_SLAVE_CONFIG_H_

#include "main.h"

extern I2C_HandleTypeDef hi2c2;

// configuration of MCU HW:
static I2C_HandleTypeDef *const i2c_slave_handle = &hi2c2;
static GPIO_TypeDef *const interrupt_out_port = EXT_RESET_GPIO_Port;
static uint16_t const interrupt_out_pin = EXT_RESET_Pin;
static void (*const i2c_slave_init_fn)(void) = &MX_I2C2_Init;

#endif // __I2C_SLAVE_CONFIG_H_
