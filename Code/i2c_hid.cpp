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
#include "i2c_hid.h"

using namespace i2c_hid;

device::device(hid::application &app, const hid::product_info &pinfo,
        i2c::address address, uint16_t hid_descriptor_reg_address)
    : _app(app), _pinfo(pinfo), _bus_address(address), _hid_descriptor_reg(hid_descriptor_reg_address)
{
    i2c::slave::instance().register_module(address, this, &device::process_start, &device::process_stop);
}

device::~device()
{
    // deassert int pin
    i2c::slave::instance().set_pin_interrupt(false);

    // disable I2C
    i2c::slave::instance().unregister_module(this);

    // clear context
    _get_report.clear();
    _in_queue.clear();
    _output_buffer = decltype(_output_buffer)();
}

void device::link_reset()
{
    // reset application
    _app.teardown(this);

    // clear context
    _get_report.clear();
    _in_queue.clear();
    _output_buffer = decltype(_output_buffer)();

    // send 0 length input report
    queue_input_report(span<const uint8_t>());
}

hid::result device::receive_report(const span<uint8_t> &data)
{
    if ((_output_buffer.size() == 0) or (_stage == 0))
    {
        // save the target buffer for when the transfer is made
        _output_buffer = data;
        return hid::result::OK;
    }
    else
    {
        // the previously passed buffer is being used for receiving data
        return hid::result::BUSY;
    }
}

hid::result device::send_report(const span<const uint8_t> &data, hid::report_type type)
{
    // if the function is invoked in the GET_REPORT callback context,
    // and the report type and ID matches, transmit immediately (without interrupt)
    if ((_get_report.type() == type) and ((_get_report.id() == 0) or (_get_report.id() == data.front())))
    {
        auto &report_length = *get_buffer<le_uint16_t>();
        report_length = static_cast<uint16_t>(data.size());

        // send the length 2 bytes, followed by the report data
        i2c::slave::instance().send(&report_length, data);

        // mark completion
        _get_report.clear();

        return hid::result::OK;
    }
    else if (type == hid::report_type::INPUT)
    {
        return queue_input_report(data) ? hid::result::OK : hid::result::BUSY;
    }
    else
    {
        // feature reports can only be sent if a GET_REPORT command is pending
        // output reports cannot be sent
        return hid::result::INVALID;
    }
}

bool device::get_report(hid::report_selector select)
{
    // mark which report needs to be redirected through the data register
    _get_report = select;

    // issue request to application, let it provide report through send_report()
    _app.get_report(select, _buffer);

    // if application provided the report, the request has been cleared
    return !_get_report.valid();
}

bool device::get_command(const span<const uint8_t> &command_data)
{
    auto &cmd = *reinterpret_cast<const command<>*>(command_data.data());
    auto cmd_size = cmd.size();
    uint16_t data_reg = *reinterpret_cast<const le_uint16_t*>(command_data.data() + cmd_size);

    if ((command_data.size() != (cmd_size + sizeof(data_reg))) or (data_reg != registers::DATA))
    {
        // invalid size or register
        return false;
    }

    switch (cmd.opcode())
    {
        case opcodes::GET_REPORT:
            return get_report(cmd.report_selector());

        case opcodes::GET_IDLE:
            send_short_data(static_cast<uint16_t>(_app.get_idle(cmd.report_id())));
            return true;

        case opcodes::GET_PROTOCOL:
            send_short_data(static_cast<uint16_t>(_app.get_protocol()));
            return true;

        default:
            return false;
    }
}

void device::send_short_data(uint16_t value)
{
    short_data &data = *get_buffer<short_data>();

    data.length = sizeof(data);
    data.value = value;

    i2c::slave::instance().send(&data);
}

bool device::reply_request(size_t data_length)
{
    const span<const uint8_t> data { _buffer.data(), data_length };
    uint16_t reg = *reinterpret_cast<const le_uint16_t*>(data.data());

    if (data.size() == sizeof(reg))
    {
        if (reg == _hid_descriptor_reg)
        {
            // HID descriptor tells the host the necessary parameters for communication
            descriptor &desc = *get_buffer<descriptor>();

            get_hid_descriptor(desc);
            i2c::slave::instance().send(&desc);
            return true;
        }
        else if (reg == registers::REPORT_DESCRIPTOR)
        {
            // Report descriptor lets the host interpret the raw report data
            auto &rdesc = _app.report_protocol().descriptor;

            i2c::slave::instance().send(rdesc);
            return true;
        }
        else
        {
            // invalid size or register
            return false;
        }
    }
    else if ((data.size() > sizeof(reg)) and (reg == registers::COMMAND))
    {
        return get_command(data.subspan(sizeof(reg)));
    }
    else
    {
        // invalid size or register
        return false;
    }
}

bool device::queue_input_report(const span<const uint8_t> &data)
{
    i2c::slave::instance().set_pin_interrupt(true);
    return _in_queue.push(data);
}

bool device::get_input()
{
    // send the next report from the queue
    span<const uint8_t> input_data;
    if (_in_queue.front(input_data) and (input_data.size() > 0))
    {
        auto &report_length = *reinterpret_cast<le_uint16_t*>(_buffer.data());
        report_length = static_cast<uint16_t>(input_data.size());

        i2c::slave::instance().send(&report_length, input_data);
    }
    else
    {
        // this is a reset, or the master is only checking our presence on the bus
        auto &reset_sentinel = *reinterpret_cast<le_uint16_t*>(_buffer.data());
        reset_sentinel = 0;

        i2c::slave::instance().send(&reset_sentinel);
    }

    // de-assert interrupt line now, as if it's only done at stop,
    // the host will try to read another report
    i2c::slave::instance().set_pin_interrupt(false);

    return true;
}

bool device::process_start(i2c::direction dir, size_t data_length)
{
    bool success = false;

    if (_stage == 0)
    {
        assert(data_length == 0);

        if (dir == i2c::direction::READ)
        {
            // when no register address is sent, the host is reading an input report
            // on the request of the interrupt pin
            success = get_input();
        }
        else
        {
            // first part of the transfer, receive register / command
            _stage = 1;
            i2c::slave::instance().receive(_buffer, _output_buffer);
            success = true;
        }
    }
    else if (dir == i2c::direction::READ)
    {
        // successive start, we must reply to received command
        success = reply_request(data_length);
    }

    return success;
}

void device::set_power(bool powered)
{
    if (_powered != powered)
    {
        _powered = powered;
        _app.set_power_mode(powered);
    }
}

bool device::set_report(hid::report_type type, const span<const uint8_t> &data)
{
    uint16_t length = *reinterpret_cast<const le_uint16_t*>(data.data());
    size_t report_length = length - sizeof(length);

    // check length validity
    if ((data.size() == length) and (report_length <= _output_buffer.size()))
    {
        // copy/move the output report into the requested buffer
        size_t header_size = (data.data() - _buffer.data()) + sizeof(length);
        size_t offset = _buffer.size() - header_size;

        // move the output buffer contents down by offset
        if (offset < report_length)
        {
            std::move_backward(_output_buffer.data(),
                    _output_buffer.data() + report_length - offset,
                    _output_buffer.data() + offset);
        }

        // copy first part from GP buffer into provided buffer
        std::copy(data.begin() + sizeof(length), data.end(),
                _output_buffer.begin());

        auto report = _output_buffer.subspan(0, report_length);

        // clear the buffer before the callback, so it can set a new buffer
        _output_buffer = decltype(_output_buffer)();

        // pass it to the app
        _app.set_report(type, report);

        return true;
    }
    else
    {
        return false;
    }
}

bool device::set_command(const span<const uint8_t> &command_data)
{
    const auto &cmd = *reinterpret_cast<const command<>*>(command_data.data());
    auto cmd_size = cmd.size();
    uint16_t data_reg = *reinterpret_cast<const le_uint16_t*>(command_data.data() + cmd_size);
    const short_data &u16_data = *reinterpret_cast<const short_data*>(command_data.data() + cmd_size + sizeof(data_reg));

    switch (cmd.opcode())
    {
        case opcodes::RESET:
            if (command_data.size() != cmd_size)
            {
                // invalid size
                return false;
            }

            link_reset();
            return true;

        case opcodes::SET_POWER:
            if (command_data.size() != cmd_size)
            {
                // invalid size
                return false;
            }

            set_power(!cmd.sleep());
            return true;

        case opcodes::SET_REPORT:
            if ((command_data.size() <= (cmd_size + sizeof(data_reg))) or
                (data_reg != registers::DATA))
            {
                // invalid size or register
                return false;
            }

            return set_report(cmd.report_type(), command_data.subspan(cmd_size + sizeof(data_reg)));

        case opcodes::SET_IDLE:
            if ((command_data.size() != (cmd_size + sizeof(data_reg) + sizeof(short_data))) or
                (data_reg != registers::DATA) or
                (u16_data.length != sizeof(short_data)))
            {
                // invalid size or register
                return false;
            }

            return _app.set_idle(u16_data.value, cmd.report_id());

        case opcodes::SET_PROTOCOL:
            // SPEC WTF: why isn't the 8-bit protocol value in the command_data.data instead of in the data register?
            if ((command_data.size() != (cmd_size + sizeof(data_reg) + sizeof(short_data))) or
                (data_reg != registers::DATA) or
                (u16_data.length != sizeof(short_data)))
            {
                // invalid size or register
                return false;
            }

            return _app.set_protocol(static_cast<hid::protocol>(static_cast<uint16_t>(u16_data.value)));

        default:
            return false;
    }
}

void device::process_write(size_t data_length)
{
    span<uint8_t> data { _buffer.data(), data_length };
    uint16_t reg = *reinterpret_cast<const le_uint16_t*>(data.data());

    if (reg == registers::OUTPUT_REPORT)
    {
        // output report received
        set_report(hid::report_type::OUTPUT, data.subspan(sizeof(reg)));
    }
    else if (reg == registers::COMMAND)
    {
        // set command received
        set_command(data.subspan(sizeof(reg)));
    }
    else
    {
        // invalid register
    }
}

void device::process_input_complete(size_t data_length)
{
    span<const uint8_t> input_data;

    // verify sent length
    if (_in_queue.front(input_data) and ((REPORT_LENGTH_SIZE + input_data.size()) <= data_length))
    {
        // input report transmit complete, remove from queue
        _in_queue.pop();

        // if the sent data is a reset response (instead of length, the first 2 bytes are 0)
        if (input_data.size() == 0)
        {
            // completed reset, initialize applications
            _app.setup(this, &device::send_report, &device::receive_report);
        }
        else
        {
            _app.in_report_sent(input_data);
        }
    }
    else if (!_in_queue.empty())
    {
        // assert interrupt line if input reports are pending
        i2c::slave::instance().set_pin_interrupt(true);
    }
}

void device::process_reply_complete(size_t data_length)
{
    // nothing to do when a getter finishes
}

void device::process_stop(i2c::direction dir, size_t data_length)
{
    // received request from host
    if (dir == i2c::direction::WRITE)
    {
        process_write(data_length);
    }
    else if (_stage == 0)
    {
        // input register transmit complete
        process_input_complete(data_length);
    }
    else
    {
        // reply transmit complete
        process_reply_complete(data_length);
    }

    // reset I2C restart counter
    _stage = 0;
}
