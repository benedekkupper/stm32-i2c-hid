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
#ifndef __HID_APPLICATION_H_
#define __HID_APPLICATION_H_

#include <cerrno>
#include "base_types.h"
#include "hid/report.h"
#include "hid/rdf/descriptor_view.h"

namespace hid
{
    /// Binary coded decimal (BCD) version format
    class version : public le_uint16_t
    {
    public:
        constexpr version(uint8_t major, uint8_t minor)
        {
            storage[1] = major;
            storage[0] = minor;
        }

        constexpr version(uint8_t major_high_nibble, uint8_t major_low_nibble,
                uint8_t minor_high_nibble, uint8_t minor_low_nibble)
        {
            storage[1] = (major_high_nibble << 4) | (major_low_nibble & 0xf);
            storage[0] = (minor_high_nibble << 4) | (minor_low_nibble & 0xf);
        }
    };

    struct product_info
    {
        uint16_t vendor_id;
        uint16_t product_id;
        version  product_version;

        product_info(uint16_t vendor_id, uint16_t product_id, version product_version)
            : vendor_id(vendor_id), product_id(product_id), product_version(product_version)
        {
        }
    };

    struct report_protocol
    {
        rdf::ce_descriptor_view descriptor;
        size_t max_input_size = 0;
        size_t max_output_size = 0;
        size_t max_feature_size = 0;
        uint8_t max_report_id = 0;

        constexpr report_protocol(const rdf::descriptor_view &descriptor, size_t max_input_size,
                size_t max_output_size = 0, size_t max_feature_size = 0, uint8_t max_report_id = 0)
            : descriptor(descriptor), max_input_size(max_input_size)
            , max_output_size(max_output_size), max_feature_size(max_feature_size), max_report_id(max_report_id)
        {
        }
    };

    enum class result : error_t
    {
        OK = 0,
        INVALID = -EINVAL,
        NO_TRANSPORT = -ENODEV,
        BUSY = -EBUSY,
    };

    class application
    {
    public:
        constexpr explicit application(const hid::report_protocol &rp)
            : _report_protocol(rp)
        {
        }

        constexpr const hid::report_protocol &report_protocol() const
        {
            return _report_protocol;
        }

        bool is_transport_valid() const
        {
            return _transport != nullptr;
        }

        template<class T>
        void setup(T *tp, result(T::*sender)(const span<const uint8_t> &data, report_type type),
                result(T::*receiver)(const span<uint8_t> &data))
        {
            if (is_transport_valid())
            {
                stop();
                _transport = nullptr;
            }
            _send_report    = reinterpret_cast<decltype(_send_report)>(sender);
            _receive_report = reinterpret_cast<decltype(_receive_report)>(receiver);
            _transport      = reinterpret_cast<decltype(_transport)>(tp);

            start();
        }

        template<class T>
        void teardown(T *tp)
        {
            if (reinterpret_cast<decltype(_transport)>(tp) == _transport)
            {
                stop();
                _transport = nullptr;
            }
        }

        virtual void set_report(report_type type, const span<const uint8_t> &data) = 0;

        // use send_report() to provide the requested report data
        virtual void get_report(report_selector select, const span<uint8_t> &buffer) = 0;

        virtual void in_report_sent(const span<const uint8_t> &data)
        {
        }

        // only for 101 keyboards and mice that want to support BIOS, etc
        virtual protocol get_protocol() const
        {
            return protocol::REPORT;
        }

        virtual bool set_protocol(protocol new_protocol)
        {
#if 1
            return false;
#else
            return new_protocol == protocol::REPORT;
#endif
        }

        // the whole idle concept is stupid, reject it here without hurting too many feelings
        uint32_t get_idle(uint8_t report_id = 0)
        {
            return 0;
        }
        bool set_idle(uint32_t idle_repeat_ms, uint8_t report_id = 0)
        {
            return false;
        }

        virtual void set_power_mode(bool enabled)
        {
        }

    protected:
        template <typename T>
        result send_report(const T &buffer, report_type type = report_type::INPUT)
        {
            if (buffer.size() == 0)
            {
                return result::INVALID;
            }
            else if (is_transport_valid())
            {
                return _send_report(_transport, span<const uint8_t>(buffer.data(), buffer.size()), type);
            }
            else
            {
                return result::NO_TRANSPORT;
            }
        }

        template <typename T>
        result receive_report(T &buffer)
        {
            if (buffer.size() == 0)
            {
                return result::INVALID;
            }
            else if (is_transport_valid())
            {
                return _receive_report(_transport, span<uint8_t>(buffer.data(), buffer.size()));
            }
            else
            {
                return result::NO_TRANSPORT;
            }
        }

    private:
        const hid::report_protocol &_report_protocol;
        void *_transport = nullptr;
        result (*_send_report)(void *transport, const span<const uint8_t> &data, report_type type) = nullptr;
        result (*_receive_report)(void *transport, const span<uint8_t> &data) = nullptr;

        virtual void start()
        {
        }
        virtual void stop()
        {
        }
    };
}

#endif // __HID_APPLICATION_H_
