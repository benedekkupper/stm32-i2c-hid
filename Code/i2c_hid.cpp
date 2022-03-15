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
    // TODO always make sure all reports can fit, including header
    assert(_buffer.size() >= sizeof(descriptor));
    assert(_buffer.size() >= (REPORT_LENGTH_SIZE + _app.report_protocol().max_input_size));

    i2c::slave::instance().register_module(address, this, &device::process_start, &device::process_stop);
}

device::~device()
{
    // deassert int pin
    i2c::slave::instance().set_pin_interrupt(false);

    // disable I2C
    i2c::slave::instance().unregister_module(this);

    // Clear context
    _get_report.clear();
    _in_queue.clear();
    _stage = 0;
}

void device::link_reset()
{
    // reset application
    _app.teardown(this);

    // flush input queue
    _in_queue.clear();

    // send 0 length input report
    queue_input_report(span<const uint8_t>());
}

hid::result device::receive_report(const span<uint8_t> &data)
{
    // save the target buffer for when the transfer is made
    _output_buffer = data;
    return hid::result::OK;
}

hid::result device::send_report(const span<const uint8_t> &data, hid::report_type type)
{
    // if the function is invoked in the GET_REPORT callback context,
    // and the report type and ID matches, transmit immediately (without interrupt)
    if ((_get_report.type() == type) && ((_get_report.id() == 0) || (_get_report.id() == data.front())))
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

    if ((command_data.size() != (cmd_size + sizeof(data_reg))) || (data_reg != registers::DATA))
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
    else if ((data.size() > sizeof(reg)) && (reg == registers::COMMAND))
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
    span<uint8_t> buffer { _buffer.data(), REPORT_LENGTH_SIZE + _app.report_protocol().max_input_size };
    // TODO: clear out the buffer's unused bytes

    // send the next report from the queue
    span<const uint8_t> input_data;
    if (_in_queue.front(input_data) && (input_data.size() > 0))
    {
        auto &report_length = *reinterpret_cast<le_uint16_t*>(buffer.data());
        report_length = static_cast<uint16_t>(input_data.size());

        // copy report into full size buffer
        std::copy(input_data.begin(), input_data.end(),
                buffer.data() + sizeof(report_length));
    }
    else
    {
        // this is a reset, or the master is only checking our presence on the bus
        auto &reset_sentinel = *reinterpret_cast<le_uint16_t*>(buffer.data());
        reset_sentinel = 0;
    }

    // send full size buffer (slave cannot control the length of the transfer, master will always read max size)
    i2c::slave::instance().send(span<const uint8_t>(buffer));

    // de-assert interrupt line until stop
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
            i2c::slave::instance().receive(_buffer);
            _stage++;
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

void device::set_output_report(size_t data_length)
{
    constexpr size_t reg_size = sizeof(le_uint16_t);
    uint16_t length = *reinterpret_cast<const le_uint16_t*>(_buffer.data() + reg_size);
    size_t header_size = reg_size + sizeof(length);
    size_t report_length = length - sizeof(length);

    // check length validity
    if ((data_length == (reg_size + length)) && (length > (sizeof(length))) &&
        (report_length <= _output_buffer.size()))
    {
#if 0
        // copy/move the output report into the requested buffer
        size_t offset = _buffer.size() - header_size;

        // move the output buffer contents down by offset
        if (offset < report_length)
        {
            std::move_backward(_output_buffer.data(),
                    _output_buffer.data() + report_length - offset,
                    _output_buffer.data() + offset);
        }

        // copy first part from GP buffer into provided buffer
        std::copy(_buffer.data() + header_size, _buffer.data() + std::min(_buffer.size(), data_length),
                _output_buffer.data());
#endif
        std::copy(_buffer.data() + header_size, _buffer.data() + data_length, _output_buffer.data());

        // pass it to the app
        _app.set_report(hid::report_type::OUTPUT, _output_buffer.subspan(0, report_length));
    }
}

void device::set_power(bool powered)
{
    if (_powered != powered)
    {
        _powered = powered;
        _app.set_power_mode(powered);
    }
}

bool device::set_report(hid::report_selector select, const span<const uint8_t> &data)
{
    const uint16_t length = *reinterpret_cast<const le_uint16_t*>(data.data());

    // check length validity
    if (data.size() == length)
    {
        _app.set_report(select.type(), data.subspan(sizeof(length)));
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
            if ((command_data.size() <= (cmd_size + sizeof(data_reg))) ||
                (data_reg != registers::DATA))
            {
                // invalid size or register
                return false;
            }

            return set_report(cmd.report_selector(), command_data.subspan(cmd_size + sizeof(data_reg)));

        case opcodes::SET_IDLE:
            if ((command_data.size() != (cmd_size + sizeof(data_reg) + sizeof(short_data))) ||
                (data_reg != registers::DATA) ||
                (u16_data.length != sizeof(short_data)))
            {
                // invalid size or register
                return false;
            }

            return _app.set_idle(u16_data.value, cmd.report_id());

        case opcodes::SET_PROTOCOL:
            // SPEC WTF: why isn't the 8-bit protocol value in the command_data.data instead of in the data register?
            if ((command_data.size() != (cmd_size + sizeof(data_reg) + sizeof(short_data))) ||
                (data_reg != registers::DATA) ||
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
    span<uint8_t> data { _buffer.data(), std::min(data_length, _buffer.size()) };
    uint16_t reg = *reinterpret_cast<const le_uint16_t*>(data.data());

    if (reg == registers::OUTPUT_REPORT)
    {
        // output report received
        set_output_report(data_length);
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
    if (_in_queue.front(input_data) && ((REPORT_LENGTH_SIZE + input_data.size()) <= data_length))
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
