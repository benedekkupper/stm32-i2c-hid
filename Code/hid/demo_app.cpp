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
#include "hid/demo_app.h"

extern void set_led(bool on);

using namespace hid;

demo_app &demo_app::instance()
{
    using namespace hid::rdf;

    static constexpr auto report_descriptor = (
        // first application: standard keyboard
        hid::reports::keyboard::app_report_descriptor<report_ids::KEYBOARD>(),

        // second application: standard mouse
        hid::reports::mouse::app_report_descriptor<report_ids::MOUSE>(),

        // third application: raw data
        usage_extended(custom_page::APPLICATION),
        collection::application(
            hid::reports::opaque::report_descriptor<raw_in_report>(custom_page::IN_DATA),

            hid::reports::opaque::report_descriptor<raw_out_report>(custom_page::OUT_DATA)
        )
    );
    static constexpr hid::report_protocol rp(report_descriptor,
            sizeof(raw_in_report), sizeof(raw_out_report), 0, report_ids::MAX);
    static demo_app app(rp);
    return app;
}

void demo_app::start()
{
    receive_report(&_raw_out_buffer);
}

void demo_app::stop()
{
}

void demo_app::button_state_change(bool pressed)
{
    _keys_buffer.set_key_state(page::keyboard_keypad::CAPSLOCK, pressed);

    if (send_report(&_keys_buffer) != result::OK)
    {
        // TODO deal with BUSY scenario
    }
}

void demo_app::set_report(report_type type, const span<const uint8_t> &data)
{
    // only output reports provided
    assert(type == report_type::OUTPUT);

    // data[0] is the report ID only if report IDs are used
    if (data[0] == kb_leds_report::id())
    {
        auto *out_report = reinterpret_cast<const kb_leds_report*>(data.data());

        // use num_lock and caps_lock flag
        set_led(out_report->get_led_state(page::leds::CAPS_LOCK));
    }
    else // if (data[0] == _raw_out_buffer.id())
    {
        // TODO: deal with custom data
    }

    receive_report(&_raw_out_buffer);
}

void demo_app::get_report(report_selector select, const span<uint8_t> &buffer)
{
    if (select == _keys_buffer.selector())
    {
        send_report(&_keys_buffer);
    }
    else if (select == _mouse_buffer.selector())
    {
        send_report(&_mouse_buffer);
    }
    else if (select == _raw_in_buffer.selector())
    {
        send_report(&_raw_in_buffer);
    }
    else
    {
        assert(false);
    }
}
