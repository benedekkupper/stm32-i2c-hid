/// \file
///
/// \author Benedek Kupper
/// \date   2022
///
/// \copyright
///         This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
///         If a copy of the MPL was not distributed with this file, You can obtain one at
///         https://mozilla.org/MPL/2.0/.
///
#include "i2c_slave.h"
#include "i2c_slave_config.h"

using namespace i2c;

slave &slave::instance()
{
    static slave s;
    return s;
}

slave::slave()
{
    static const GPIO_InitTypeDef GPIO_InitStruct = {
        .Pin = interrupt_out_pin,
        .Mode = GPIO_MODE_OUTPUT_PP,
        .Pull = GPIO_PULLUP,
    };
    set_pin_interrupt(false);
    HAL_GPIO_Init(interrupt_out_port, const_cast<GPIO_InitTypeDef*>(&GPIO_InitStruct));
    i2c_slave_init_fn();
}

void slave::set_slave_address(address slave_addr)
{
    i2c_slave_handle->Init.OwnAddress1 = slave_addr.raw();
    if (slave_addr.is_10bit())
    {
        i2c_slave_handle->Init.AddressingMode = I2C_ADDRESSINGMODE_10BIT;
    }
    else
    {
        i2c_slave_handle->Init.OwnAddress1 <<= 1;
        i2c_slave_handle->Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    }
    HAL_I2C_Init(i2c_slave_handle);
}

void slave::set_pin_interrupt(bool asserted)
{
    // active low logic
    HAL_GPIO_WritePin(interrupt_out_port, interrupt_out_pin, static_cast<GPIO_PinState>(!asserted));
}

void slave::start_listen()
{
    HAL_I2C_EnableListen_IT(i2c_slave_handle);
}

void slave::stop_listen()
{
    HAL_I2C_DisableListen_IT(i2c_slave_handle);
}

void slave::nack()
{
    __HAL_I2C_GENERATE_NACK(i2c_slave_handle);
}

void slave::send_dummy()
{
    HAL_I2C_Slave_Seq_Transmit_IT(i2c_slave_handle, (uint8_t*)&i2c_slave_handle->ErrorCode, sizeof(i2c_slave_handle->ErrorCode), I2C_NEXT_FRAME);
}

void slave::send(const uint8_t *data, size_t size)
{
    _first_size = size;
    _second_size = 0;
    _second_data = nullptr;
    HAL_I2C_Slave_Seq_Transmit_DMA(i2c_slave_handle, const_cast<uint8_t *>(data), size, I2C_NEXT_FRAME);
}

void slave::send(const uint8_t *data1, size_t size1, const uint8_t *data2, size_t size2)
{
    _first_size = size1;
    _second_size = size2;
    _second_data = (_second_size > 0) ? const_cast<uint8_t*>(data2) : nullptr;
    HAL_I2C_Slave_Seq_Transmit_DMA(i2c_slave_handle, const_cast<uint8_t*>(data1), size1, I2C_NEXT_FRAME);
}

void slave::receive(uint8_t *data, size_t size)
{
    _first_size = size;
    _second_size = 0;
    _second_data = nullptr;
    HAL_I2C_Slave_Seq_Receive_DMA(i2c_slave_handle, data, size, I2C_LAST_FRAME);
}

void slave::receive(uint8_t *data1, size_t size1, uint8_t *data2, size_t size2)
{
    _first_size = size1;
    _second_size = size2;
    _second_data = (_second_size > 0) ? const_cast<uint8_t*>(data2) : nullptr;
    HAL_I2C_Slave_Seq_Receive_DMA(i2c_slave_handle, data1, size1, I2C_NEXT_FRAME);
}

void slave::handle_start(direction dir)
{
    bool success = _module != nullptr;
    if (success)
    {
        _last_dir = dir;
        size_t size = _first_size;
        if (size > 0)
        {
            if (_second_data == nullptr)
            {
                size += _second_size;
            }
            if (dir == direction::WRITE)
            {
                size -= __HAL_DMA_GET_COUNTER(i2c_slave_handle->hdmatx);
            }
            else
            {
                size -= __HAL_DMA_GET_COUNTER(i2c_slave_handle->hdmarx);
            }
        }
        success = _on_start(_module, dir, size);
    }
    if (!success)
    {
        // impossible to NACK in read direction
        if (dir == direction::WRITE)
        {
            nack();
        }
        else
        {
            send_dummy();
        }
    }
}

void slave::handle_tx_complete()
{
    if (_second_data != nullptr)
    {
        auto *data = _second_data;
        _second_data = nullptr;
        HAL_I2C_Slave_Seq_Transmit_DMA(i2c_slave_handle, data, _second_size, I2C_NEXT_FRAME);
    }
    else
    {
        send_dummy();
    }
}

void slave::handle_rx_complete()
{
    if (_second_data != nullptr)
    {
        auto *data = _second_data;
        _second_data = nullptr;
        HAL_I2C_Slave_Seq_Receive_DMA(i2c_slave_handle, data, _second_size, I2C_LAST_FRAME);
    }
    else
    {
        nack();
    }
}

void slave::handle_stop()
{
    if (_module != nullptr)
    {
        size_t size = _first_size;
        if (size > 0)
        {
            if (_second_data == nullptr)
            {
                size += _second_size;
            }
            if (_last_dir == direction::WRITE)
            {
                size -= __HAL_DMA_GET_COUNTER(i2c_slave_handle->hdmarx);
            }
            else
            {
                size -= __HAL_DMA_GET_COUNTER(i2c_slave_handle->hdmatx);
            }
        }
        _on_stop(_module, _last_dir, size);
        _first_size = 0;
        _second_size = 0;

        start_listen();
    }
}

extern "C" void HAL_I2C_AddrCallback(I2C_HandleTypeDef *hi2c, uint8_t TransferDirection, uint16_t AddrMatchCode)
{
    i2c::slave::instance().handle_start(static_cast<direction>(TransferDirection));
}

extern "C" void HAL_I2C_ListenCpltCallback(I2C_HandleTypeDef *hi2c)
{
    i2c::slave::instance().handle_stop();
}

extern "C" void HAL_I2C_SlaveTxCpltCallback(I2C_HandleTypeDef *hi2c)
{
    i2c::slave::instance().handle_tx_complete();
}

extern "C" void HAL_I2C_SlaveRxCpltCallback(I2C_HandleTypeDef *hi2c)
{
    i2c::slave::instance().handle_rx_complete();
}

