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
#ifndef __HID_APPLICATION_H_
#define __HID_APPLICATION_H_

#include <cerrno>
#include "base_types.h"
#include "hid/report.h"
#include "hid/report_protocol.h"
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

        constexpr uint8_t major() const
        {
            return storage[1];
        }

        constexpr uint8_t minor() const
        {
            return storage[0];
        }
    };

    struct product_info
    {
        uint16_t vendor_id;
        uint16_t product_id;
        version  product_version;

        constexpr product_info(uint16_t vendor_id, uint16_t product_id, version product_version)
            : vendor_id(vendor_id), product_id(product_id), product_version(product_version)
        {
        }
    };

    enum class result : error_t
    {
        OK            = 0,
        INVALID       = -EINVAL,
        NO_TRANSPORT  = -ENODEV,
        BUSY          = -EBUSY,
    };

    class application
    {
    public:
        constexpr explicit application(const hid::report_protocol& rp)
            : _report_protocol(rp)
        {
        }

        constexpr const hid::report_protocol& report_protocol() const
        {
            return _report_protocol;
        }

    private:
        /// @brief Initialize the application when the transport becomes active.
        ///        The application must always start in REPORT protocol (as opposed to BOOT) mode.
        virtual void start()
        {
        }

        /// @brief Stop and clean up the application when the transport is shut down.
        virtual void stop()
        {
        }

    public:
        /// @brief Called by the transport when a report is received from the host.
        ///        The report data is always loaded into the buffer provided by @ref receive_report call.
        /// @param type: received report's type (either OUTPUT or FEATURE)
        /// @param data: received report data
        virtual void set_report(report::type type, const span<const uint8_t>& data) = 0;

        /// @brief Called by the transport when a synchronous report reading is requested
        ///        by the host. Use @ref send_report to provide the requested report data.
        /// @param select: identifies the requested report (either INPUT or FEATURE)
        /// @param buffer: optional buffer space that is available for the report sending
        virtual void get_report(report::selector select, const span<uint8_t>& buffer) = 0;

        /// @brief Called by the transport when the host has received the INPUT report
        ///        that was provided through a successful call of @ref send_report,
        ///        outside of @ref get_report context.
        /// @param data: the report data that was sent
        virtual void in_report_sent(const span<const uint8_t>& data)
        {
        }

        /// @brief Called by the transport when the host changes the power level of the link.
        /// @note  This may happen outside of the operating time of the application
        ///        (which is between @ref start and @ref stop)
        /// @param enabled: whether the link is set to powered
        virtual void set_power_mode(bool enabled)
        {
        }

    protected:
        /// @brief  Send a report to the host.
        /// @param  data: the report data to send
        /// @param  type: either INPUT, or FEATURE (the latter only from @ref get_report call context)
        /// @return OK           if transmission is scheduled,
        ///         BUSY         if transport is busy with another report,
        ///         NO_TRANSPORT if transport is missing;
        ///         INVALID      if the buffer size is 0 or FEATURE report is provided from wrong context
        result send_report(const span<const uint8_t>& data, report::type type = report::type::INPUT)
        {
            if (data.size() == 0)
            {
                return result::INVALID;
            }
            else if (is_transport_valid())
            {
                return _send_report(_transport, data, type);
            }
            else
            {
                return result::NO_TRANSPORT;
            }
        }

        /// @brief  Send a @ref hid::report::data type structure to the host.
        /// @tparam T: a @ref hid::report::data type
        /// @param  report: the report data to send
        /// @return see @ref send_report above
        template <typename T>
        inline result send_report(const T* report)
        {
            return send_report(span<const uint8_t>(report->data(), sizeof(*report)), report->type());
        }

        /// @brief  Request receiving the next OUT or FEATURE report into the provided buffer.
        /// @param  data: The allocated buffer for receiving reports.
        /// @return OK           if transport is available
        ///         NO_TRANSPORT if transport is missing
        ///         INVALID      if the buffer size is 0
        result receive_report(const span<uint8_t>& data)
        {
            if (data.size() == 0)
            {
                return result::INVALID;
            }
            else if (is_transport_valid())
            {
                return _receive_report(_transport, data);
            }
            else
            {
                return result::NO_TRANSPORT;
            }
        }

        /// @brief  Request receiving the next OUT or FEATURE report into the provided
        ///         @ref hid::report::data type buffer.
        /// @tparam T: a @ref hid::report::data type
        /// @param  report: the report buffer to receive into
        /// @return see @ref receive_report above
        template <typename T>
        inline result receive_report(T* report)
        {
            return receive_report(span<uint8_t>(report->data(), sizeof(*report)));
        }

    public:
        /// @brief  Indicates the currently selected protocol.
        /// @return The current protocol, either REPORT (default) or BOOT.
        virtual protocol get_protocol() const
        {
            return protocol::REPORT;
        }

        /// @brief Called by the transport when the host wants to switch between default REPORT
        ///        and simplified BOOT protocol. BOOT protocol is only defined for USB
        ///        101 keyboard and mouse applications, and the support of BOOT protocol
        ///        is indicated by the USB HID interface descriptor.
        /// @param new_protocol: the new protocol to continue operating with
        /// @return true if successful, false otherwise
        virtual bool set_protocol(protocol new_protocol)
        {
#if 0
            return false;
#else
            return new_protocol == protocol::REPORT;
#endif
        }

        // SPEC WTF: The IDLE parameter is meant to control how often the device should resend
        // the same piece of information. This is introduced in the USB HID class spec,
        // somehow thinking that the host cannot keep track of the time by itself -
        // but then how does the host generate the Start of Frame with correct intervals?
        uint32_t get_idle(uint8_t report_id = 0) { return 0; }
        bool     set_idle(uint32_t idle_repeat_ms, uint8_t report_id = 0) { return false; }

        bool is_transport_valid() const
        {
            return _transport != nullptr;
        }

        template<class T>
        bool setup(T* tp, result(T::*sender)(const span<const uint8_t>& data, report::type type),
                result(T::*receiver)(const span<uint8_t>& data))
        {
            if (teardown(tp) or !is_transport_valid())
            {
                _send_report    = reinterpret_cast<decltype(_send_report)>(sender);
                _receive_report = reinterpret_cast<decltype(_receive_report)>(receiver);
                _transport      = reinterpret_cast<decltype(_transport)>(tp);

                start();
                return true;
            }
            else
            {
                return false;
            }
        }

        template<class T>
        bool teardown(T* tp)
        {
            if (reinterpret_cast<decltype(_transport)>(tp) == _transport)
            {
                stop();
                _transport = nullptr;
                return true;
            }
            else
            {
                return false;
            }
        }

    private:
        const hid::report_protocol& _report_protocol;
        void* _transport = nullptr;
        result (*_send_report)   (void* transport, const span<const uint8_t>& data, report::type type) = nullptr;
        result (*_receive_report)(void* transport, const span<uint8_t>& data) = nullptr;
    };
}

#endif // __HID_APPLICATION_H_
