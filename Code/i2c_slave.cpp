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
#include "main.h"

using namespace i2c;

slave::slave(I2C_HandleTypeDef *hi2cx, void (*MX_Init)(), void *gpioport, uint16_t gpiopin)
    : _hi2c(hi2cx)
    , _pinport(gpioport)
    , _pin(gpiopin)
{
    GPIO_InitTypeDef GPIO_InitStruct = {
        .Pin = _pin,
        .Mode = GPIO_MODE_OUTPUT_PP,
        .Pull = GPIO_PULLUP,
    };
    set_pin_interrupt(false);
    HAL_GPIO_Init(reinterpret_cast<GPIO_TypeDef*>(_pinport), &GPIO_InitStruct);
    MX_Init();
}

void slave::set_slave_address(address slave_addr)
{
    _hi2c->Init.OwnAddress1 = slave_addr.raw();
    if (slave_addr.is_10bit())
    {
        _hi2c->Init.AddressingMode = I2C_ADDRESSINGMODE_10BIT;
    }
    else
    {
        _hi2c->Init.OwnAddress1 <<= 1;
        _hi2c->Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    }
    HAL_I2C_Init(_hi2c);
}

void slave::set_pin_interrupt(bool asserted)
{
    // active low logic
    HAL_GPIO_WritePin(reinterpret_cast<GPIO_TypeDef*>(_pinport), _pin, static_cast<GPIO_PinState>(!asserted));
}

void slave::start_listen()
{
    HAL_I2C_EnableListen_IT(_hi2c);
}

void slave::stop_listen()
{
    HAL_I2C_DisableListen_IT(_hi2c);
}

void slave::nack()
{
    __HAL_I2C_GENERATE_NACK(_hi2c);
}

void slave::send(const uint8_t *data, size_t size)
{
    _first_size = size;
    _second_size = 0;
    _second_data = nullptr;
    HAL_I2C_Slave_Seq_Transmit_DMA(_hi2c, const_cast<uint8_t *>(data), size, I2C_LAST_FRAME);
}

void slave::send(const uint8_t *data1, size_t size1, const uint8_t *data2, size_t size2)
{
    _first_size = size1;
    _second_size = size2;
    _second_data = const_cast<uint8_t*>(data2);
    HAL_I2C_Slave_Seq_Transmit_DMA(_hi2c, const_cast<uint8_t*>(data1), size1, I2C_NEXT_FRAME);
}

void slave::receive(uint8_t *data, size_t size)
{
    _first_size = size;
    _second_size = 0;
    _second_data = nullptr;
    HAL_I2C_Slave_Seq_Receive_DMA(_hi2c, data, size, I2C_LAST_FRAME);
}

void slave::receive(uint8_t *data1, size_t size1, uint8_t *data2, size_t size2)
{
    _first_size = size1;
    _second_size = size2;
    _second_data = const_cast<uint8_t*>(data2);
    HAL_I2C_Slave_Seq_Receive_DMA(_hi2c, data1, size1, I2C_NEXT_FRAME);
}

void slave::handle_start(direction dir)
{
    if (_module != nullptr)
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
                size -= __HAL_DMA_GET_COUNTER(_hi2c->hdmatx);
            }
            else
            {
                size -= __HAL_DMA_GET_COUNTER(_hi2c->hdmarx);
            }
        }
        bool success = _on_start(_module, dir, size);
        if (!success)
        {
            // impossible to NACK in read direction
            if (dir == direction::WRITE)
            {
                nack();
            }
            else
            {
                // TODO: send dummy bytes
            }
        }
    }
}

void slave::handle_tx_complete()
{
    if (_second_data != nullptr)
    {
        auto *data = _second_data;
        _second_data = nullptr;
        HAL_I2C_Slave_Seq_Transmit_DMA(_hi2c, data, _second_size, I2C_LAST_FRAME);
    }
    else
    {
        // TODO: send dummy bytes
    }
}

void slave::handle_rx_complete()
{
    if (_second_data != nullptr)
    {
        auto *data = _second_data;
        _second_data = nullptr;
        HAL_I2C_Slave_Seq_Receive_DMA(_hi2c, data, _second_size, I2C_LAST_FRAME);
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
                size -= __HAL_DMA_GET_COUNTER(_hi2c->hdmarx);
            }
            else
            {
                size -= __HAL_DMA_GET_COUNTER(_hi2c->hdmatx);
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

