/// @file
///
/// @author Benedek Kupper
/// @date   2024
///
/// @copyright
///         This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
///         If a copy of the MPL was not distributed with this file, You can obtain one at
///         https://mozilla.org/MPL/2.0/.
///
#ifndef __HAL_I2C_SLAVE_HPP_
#define __HAL_I2C_SLAVE_HPP_

#include "i2c/slave.hpp"
#include "st/stm32hal.h"

namespace st
{
class hal_i2c_slave : public i2c::slave
{
  public:
    hal_i2c_slave(I2C_HandleTypeDef& handle, void (*i2c_slave_init_fn)(void),
                  GPIO_TypeDef* interrupt_out_port, uint16_t interrupt_out_pin);

    void handle_start(i2c::direction dir);
    void handle_tx_complete();
    void handle_rx_complete();
    void handle_stop();

  private:
    void nack();
    void send_dummy();
    void set_pin_interrupt(bool asserted) override;
    void send(const std::span<const uint8_t>& a) override;
    void send(const std::span<const uint8_t>& a, const std::span<const uint8_t>& b) override;
    void receive(const std::span<uint8_t>& a) override;
    void receive(const std::span<uint8_t>& a, const std::span<uint8_t>& b) override;
    void set_slave_address(i2c::address slave_addr);
    void start_listen(i2c::address slave_addr) override;
    void start_listen();
    void stop_listen(i2c::address slave_addr) override;

    I2C_HandleTypeDef* handle_;
    GPIO_TypeDef* interrupt_out_port_;
    size_t first_size_{};
    size_t second_size_{};
    uint8_t* second_data_{};
    uint16_t interrupt_out_pin_;
    i2c::direction last_dir_{};
};
} // namespace st

#endif // __HAL_I2C_SLAVE_HPP_
