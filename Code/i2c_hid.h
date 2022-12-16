/// @file
///
/// @author Benedek Kupper
/// @date   2022
///
/// @copyright
///         This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
///         If a copy of the MPL was not distributed with this file, You can obtain one at
///         https://mozilla.org/MPL/2.0/.
///
#ifndef __I2C_HID_H_
#define __I2C_HID_H_

#include "base_types.h"
#include "hid/application.h"
#include "i2c_slave.h"

template <typename T>
constexpr T pack_str(const char* str)
{
    T val = 0;
    for (unsigned i = 0; (i < sizeof(T)) and (str[i] != '\0'); i++)
    {
        val = (val) | static_cast<T>(str[i]) << (i * 8);
    }
    return val;
}

namespace i2c_hid
{
    constexpr static hid::version SPEC_VERSION {1, 0};

    struct descriptor
    {
        le_uint16_t wHIDDescLength = sizeof(descriptor);
        hid::version bcdVersion = SPEC_VERSION;
        le_uint16_t wReportDescLength = 0;
        le_uint16_t wReportDescRegister = 0;
        le_uint16_t wInputRegister = 0;
        le_uint16_t wMaxInputLength = 0;
        le_uint16_t wOutputRegister = 0;
        le_uint16_t wMaxOutputLength = 0;
        le_uint16_t wCommandRegister = 0;
        le_uint16_t wDataRegister = 0;
        le_uint16_t wVendorID = 0;
        le_uint16_t wProductID = 0;
        le_uint16_t wVersionID = 0;
        le_uint32_t reserved = 0;

        constexpr descriptor()
        {
        }

        constexpr descriptor& reset()
        {
            *this = descriptor();
            return *this;
        }

        constexpr descriptor& set_protocol(const hid::report_protocol& rep_prot)
        {
            wReportDescLength   = rep_prot.descriptor.size();
            wMaxInputLength     = sizeof(le_uint16_t) + rep_prot.max_input_size;
            wMaxOutputLength    = sizeof(le_uint16_t) + rep_prot.max_output_size;
            return *this;
        }

        constexpr descriptor& set_product_info(const hid::product_info& pinfo)
        {
            wVendorID           = pinfo.vendor_id;
            wProductID          = pinfo.product_id;
            wVersionID          = pinfo.product_version;
            return *this;
        }

        template<typename T>
        constexpr descriptor& set_registers()
        {
            wReportDescRegister = static_cast<uint16_t>(T::REPORT_DESCRIPTOR);
            wInputRegister      = static_cast<uint16_t>(T::INPUT_REPORT);
            wOutputRegister     = static_cast<uint16_t>(T::OUTPUT_REPORT);
            wCommandRegister    = static_cast<uint16_t>(T::COMMAND);
            wDataRegister       = static_cast<uint16_t>(T::DATA);
            return *this;
        }
    };

    enum class opcodes : uint8_t
    {
        // Mandatory
        RESET           = 0x1, // Reset the device at any time
        GET_REPORT      = 0x2, // Request from HOST to DEVICE to retrieve a report (input/feature)
        SET_REPORT      = 0x3, // Request from HOST to DEVICE to set a report (output/feature)
        SET_POWER       = 0x8, // Request from HOST to DEVICE to indicate preferred power setting
        // Optional
        GET_IDLE        = 0x4, // Request from HOST to DEVICE to retrieve the current idle rate for a particular TLC.
        SET_IDLE        = 0x5, // Request from HOST to DEVICE to set the current idle rate for a particular TLC.
        GET_PROTOCOL    = 0x6, // Request from HOST to DEVICE to retrieve the protocol mode the device is operating in.
        SET_PROTOCOL    = 0x7, // Request from HOST to DEVICE to set the protocol mode the device should be operating in.
    };

    template<hid::report::id::type REPORT_ID = 0>
    class command
    {
        constexpr static bool EXTENDED = std::integral_constant<bool, (REPORT_ID >= 0xf)>::value;
    public:
        constexpr command(opcodes opcode, bool sleep = false)
        {
            _buffer[0] = sleep;
            _buffer[1] = static_cast<uint8_t>(opcode);
        }
        constexpr command(opcodes opcode, hid::report::type type)
        {
            _buffer[0] = static_cast<uint8_t>(type) << 4;
            _buffer[1] = static_cast<uint8_t>(opcode);
            if (!EXTENDED)
            {
                _buffer[0] |= REPORT_ID;
            }
            else
            {
                _buffer[0] |= 0xf;
                _buffer[2] = REPORT_ID;
            }
        }
        // accessors for viewing
        constexpr opcodes opcode() const
        {
            return static_cast<opcodes>(_buffer[1]);
        }
        constexpr bool is_extended() const
        {
            return (opcode() >= opcodes::GET_REPORT) and (opcode() <= opcodes::SET_IDLE) and ((_buffer[0] & 0xf) == 0xf);
        }
        constexpr size_t size() const
        {
            return is_extended() ? 3 : 2;
        }
        constexpr hid::report::type report_type() const
        {
            return static_cast<hid::report::type>((_buffer[0] >> 4) & 0x3);
        }
        constexpr hid::report::id::type report_id() const
        {
            return is_extended() ? _buffer[2] : _buffer[0] & 0xf;
        }
        constexpr hid::report::selector report_selector() const
        {
            return hid::report::selector(this->report_type(), this->report_id());
        }
        constexpr bool sleep() const
        {
            return (_buffer[0] & 1) != 0;
        }
    private:
        uint8_t _buffer[EXTENDED ? 3 : 2] = {};
    };

    struct short_data
    {
        le_uint16_t length;
        le_uint16_t value;

        constexpr bool valid_size() const
        {
            return length == static_cast<uint16_t>(sizeof(*this));
        }
    };

    class device
    {
        enum registers : uint16_t
        {
            HID_DESCRIPTOR    = pack_str<uint16_t>("H"),  // actually configured in device constructor
            REPORT_DESCRIPTOR = pack_str<uint16_t>("RD"),
            COMMAND           = pack_str<uint16_t>("CM"),
            DATA              = pack_str<uint16_t>("DT"),
            INPUT_REPORT      = pack_str<uint16_t>("IR"), // SPEC WTF: only set in descriptor, never seen on the bus
            OUTPUT_REPORT     = pack_str<uint16_t>("OR"),
        };

        constexpr static std::size_t REPORT_LENGTH_SIZE = sizeof(std::uint16_t);

    public:
        device(hid::application& app, const hid::product_info& pinfo,
                i2c::address address, uint16_t hid_descriptor_reg_address = registers::HID_DESCRIPTOR);
        ~device();

        void link_reset();

        i2c::address get_bus_address() const
        {
            return _bus_address;
        }
        uint16_t get_hid_descriptor_reg_address() const
        {
            return _hid_descriptor_reg;
        }
        void get_hid_descriptor(descriptor& desc) const
        {
            // fill in the descriptors with the data from different sources
            desc.reset();
            desc.set_registers<registers>();
            desc.set_protocol(_app.report_protocol());
            desc.set_product_info(_pinfo);
        }
        bool get_power_state() const
        {
            return _powered;
        }

    private:
        hid::result send_report(const span<const uint8_t>& data, hid::report::type type);
        hid::result receive_report(const span<uint8_t>& data);

        bool process_start(i2c::direction dir, size_t data_length);
        void process_stop(i2c::direction dir, size_t data_length);

        template<typename T>
        T* get_buffer(size_t offset = 0)
        {
            return reinterpret_cast<T*>(_buffer.data() + offset);
        }

        bool queue_input_report(const span<const uint8_t>& data);

        void send_short_data(uint16_t value);
        bool get_report(hid::report::selector select);
        bool get_command(const span<const uint8_t>& command_data);

        bool reply_request(size_t data_length);

        bool get_input();

        void set_power(bool powered);
        bool set_report(hid::report::type type, const span<const uint8_t>& data);
        bool set_command(const span<const uint8_t>& command_data);
        void process_write(size_t data_length);
        void process_input_complete(size_t data_length);
        void process_reply_complete(size_t data_length);

        // non-copyable
        device(const device&) = delete;
        device& operator=(const device&) = delete;
        // non-movable
        device(const device&&) = delete;
        device& operator=(const device&&) = delete;

    private:
        hid::application& _app;
        const hid::product_info& _pinfo;
        i2c::address _bus_address;
        uint16_t _hid_descriptor_reg;
        uint8_t _stage = 0;
        bool _powered = false;
        hid::report::selector _get_report {};
        span<uint8_t> _output_buffer {};
        simple_queue<span<const uint8_t>> _in_queue {};
        std::array<uint8_t, sizeof(descriptor)> _buffer {};
    };
}

#endif /* __I2C_HID_H_ */
