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
#ifndef __HID_DEMO_APP_H_
#define __HID_DEMO_APP_H_

#include "hid/application.h"
#include "hid/reports/keyboard.h"
#include "hid/reports/mouse.h"
#include "hid/reports/opaque.h"

namespace hid
{
    namespace page
    {
        enum class custom_page : usage_id_type;
        template <>
        struct info<custom_page>
        {
            constexpr static usage_id_type base_id = 0xff010000;
            constexpr static usage_id_type max_usage = 3 | base_id;
            constexpr static const char* name = "invalid";
        };
        enum class custom_page : hid::usage_id_type
        {
            APPLICATION = 0x0001 | info<custom_page>::base_id,
            IN_DATA = 0x0002 | info<custom_page>::base_id,
            OUT_DATA = 0x0003 | info<custom_page>::base_id,
        };
    }

    class demo_app : public hid::application
    {
        enum report_ids : uint8_t
        {
            KEYBOARD = 1,
            MOUSE = 2,
            OPAQUE = 3,
            MAX = OPAQUE
        };

    public:
        static demo_app &instance();

        void button_state_change(bool pressed);

        using keys_report    = reports::keyboard::keys_input_report<report_ids::KEYBOARD>;
        using kb_leds_report = reports::keyboard::output_report<report_ids::KEYBOARD>;
        using mouse_report   = reports::mouse::report<report_ids::MOUSE>;
        using raw_in_report  = reports::opaque::report<32, report_type::INPUT, report_ids::OPAQUE>;
        using raw_out_report = reports::opaque::report<32, report_type::OUTPUT, report_ids::OPAQUE>;

    private:
        keys_report _keys_buffer;
        mouse_report _mouse_buffer;
        raw_in_report _raw_in_buffer;
        raw_out_report _raw_out_buffer;

        constexpr explicit demo_app(const hid::report_protocol &rp)
            : application(rp)
        {
        }

        void start() override;
        void stop() override;
        void set_report(report_type type, const span<const uint8_t> &data) override;
        void get_report(report_selector select, const span<uint8_t> &buffer) override;
    };
}

#endif // __HID_DEMO_APP_H_
