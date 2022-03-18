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
    class demo_app : public hid::application
    {
        enum report_ids : uint8_t
        {
            KEYBOARD = 1,
            MOUSE = 2,
            OPAQUE = 3,
            MAX = OPAQUE
        };

        enum class custom_page : hid::usage_id_type
        {
            PAGE_ID = 0xff010000,
            APPLICATION = 0x0001 | static_cast<usage_id_type>(PAGE_ID),
            IN_DATA = 0x0002 | static_cast<usage_id_type>(PAGE_ID),
            OUT_DATA = 0x0002 | static_cast<usage_id_type>(PAGE_ID),
            MAX_USAGE = 0x0003 | static_cast<usage_id_type>(PAGE_ID),
        };

    public:
        static demo_app &instance();

        void button_state_change(bool pressed);

        using keys_report    = reports::keyboard::keys_input_report<report_ids::KEYBOARD>;
        using kb_leds_report = reports::keyboard::output_report<report_ids::KEYBOARD>;
        using mouse_report   = reports::mouse::report<report_ids::MOUSE>;
        using raw_in_report  = reports::opaque::report<32, report_type::INPUT, report_ids::OPAQUE>;
        using raw_out_report = reports::opaque::report<32, report_type::INPUT, report_ids::OPAQUE>;

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
