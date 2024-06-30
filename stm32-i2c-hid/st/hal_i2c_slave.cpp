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
#include "st/hal_i2c_slave.hpp"

namespace st
{
hal_i2c_slave::hal_i2c_slave(I2C_HandleTypeDef& handle, void (*const i2c_slave_init_fn)(void),
                             GPIO_TypeDef* interrupt_out_port, uint16_t interrupt_out_pin)
    : handle_(&handle),
      interrupt_out_port_(interrupt_out_port),
      interrupt_out_pin_(interrupt_out_pin)
{
    static const GPIO_InitTypeDef GPIO_InitStruct = {
        .Pin = interrupt_out_pin_,
        .Mode = GPIO_MODE_OUTPUT_PP,
        .Pull = GPIO_PULLUP,
    };
    set_pin_interrupt(false);
    HAL_GPIO_Init(interrupt_out_port_, const_cast<GPIO_InitTypeDef*>(&GPIO_InitStruct));
    i2c_slave_init_fn();
}

void hal_i2c_slave::set_slave_address(i2c::address slave_addr)
{
    handle_->Init.OwnAddress1 = slave_addr.raw();
    if (slave_addr.is_10bit())
    {
        handle_->Init.AddressingMode = I2C_ADDRESSINGMODE_10BIT;
    }
    else
    {
        handle_->Init.OwnAddress1 <<= 1;
        handle_->Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    }
    HAL_I2C_Init(handle_);
}

void hal_i2c_slave::set_pin_interrupt(bool asserted)
{
    // active low logic
    HAL_GPIO_WritePin(interrupt_out_port_, interrupt_out_pin_,
                      static_cast<GPIO_PinState>(!asserted));
}

void hal_i2c_slave::start_listen(i2c::address slave_addr)
{
    set_slave_address(slave_addr);
    start_listen();
}

void hal_i2c_slave::start_listen()
{
    HAL_I2C_EnableListen_IT(handle_);
}

void hal_i2c_slave::stop_listen(i2c::address slave_addr)
{
    HAL_I2C_DisableListen_IT(handle_);
}

void hal_i2c_slave::nack()
{
    __HAL_I2C_GENERATE_NACK(handle_);
}

void hal_i2c_slave::send_dummy()
{
    HAL_I2C_Slave_Seq_Transmit_IT(handle_, (uint8_t*)&handle_->ErrorCode,
                                  sizeof(handle_->ErrorCode), I2C_NEXT_FRAME);
}

void hal_i2c_slave::send(const std::span<const uint8_t>& a)
{
    first_size_ = a.size();
    second_size_ = 0;
    second_data_ = nullptr;
    HAL_I2C_Slave_Seq_Transmit_DMA(handle_, const_cast<uint8_t*>(a.data()), a.size(),
                                   I2C_NEXT_FRAME);
}

void hal_i2c_slave::send(const std::span<const uint8_t>& a, const std::span<const uint8_t>& b)
{
    first_size_ = a.size();
    second_size_ = b.size();
    second_data_ = (second_size_ > 0) ? const_cast<uint8_t*>(b.data()) : nullptr;
    HAL_I2C_Slave_Seq_Transmit_DMA(handle_, const_cast<uint8_t*>(a.data()), a.size(),
                                   I2C_NEXT_FRAME);
}

void hal_i2c_slave::receive(const std::span<uint8_t>& a)
{
    first_size_ = a.size();
    second_size_ = 0;
    second_data_ = nullptr;
    HAL_I2C_Slave_Seq_Receive_DMA(handle_, a.data(), a.size(), I2C_LAST_FRAME);
}

void hal_i2c_slave::receive(const std::span<uint8_t>& a, const std::span<uint8_t>& b)
{
    first_size_ = a.size();
    second_size_ = b.size();
    second_data_ = (second_size_ > 0) ? const_cast<uint8_t*>(b.data()) : nullptr;
    HAL_I2C_Slave_Seq_Receive_DMA(handle_, a.data(), a.size(), I2C_NEXT_FRAME);
}

void hal_i2c_slave::handle_start(i2c::direction dir)
{
    bool success = has_module();
    if (success)
    {
        last_dir_ = dir;
        size_t size = first_size_;
        if (size > 0)
        {
            if (second_data_ == nullptr)
            {
                size += second_size_;
            }
            if (dir == i2c::direction::WRITE)
            {
                size -= __HAL_DMA_GET_COUNTER(handle_->hdmatx);
            }
            else
            {
                size -= __HAL_DMA_GET_COUNTER(handle_->hdmarx);
            }
        }
        success = on_start(dir, size);
    }
    if (!success)
    {
        // impossible to NACK in read direction
        if (dir == i2c::direction::WRITE)
        {
            nack();
        }
        else
        {
            send_dummy();
        }
    }
}

void hal_i2c_slave::handle_tx_complete()
{
    if (second_data_ != nullptr)
    {
        auto* data = second_data_;
        second_data_ = nullptr;
        HAL_I2C_Slave_Seq_Transmit_DMA(handle_, data, second_size_, I2C_NEXT_FRAME);
    }
    else
    {
        send_dummy();
    }
}

void hal_i2c_slave::handle_rx_complete()
{
    if (second_data_ != nullptr)
    {
        auto* data = second_data_;
        second_data_ = nullptr;
        HAL_I2C_Slave_Seq_Receive_DMA(handle_, data, second_size_, I2C_LAST_FRAME);
    }
    else
    {
        nack();
    }
}

void hal_i2c_slave::handle_stop()
{
    if (has_module())
    {
        size_t size = first_size_;
        if (size > 0)
        {
            if (second_data_ == nullptr)
            {
                size += second_size_;
            }
            if (last_dir_ == i2c::direction::WRITE)
            {
                size -= __HAL_DMA_GET_COUNTER(handle_->hdmarx);
            }
            else
            {
                size -= __HAL_DMA_GET_COUNTER(handle_->hdmatx);
            }
        }
        on_stop(last_dir_, size);
        first_size_ = 0;
        second_size_ = 0;

        start_listen();
    }
}
} // namespace st