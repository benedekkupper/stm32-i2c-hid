/// @file
///
/// @author Benedek Kupper
/// @date   2024
///
/// @copyright
///         This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
///         If a copy of the MPL was not distributed with this file, You can obtain one at
///         https://mozilla.org/MPL/2.0/.
///
#include "hid/demo_app.hpp"

extern void set_led(bool on);

using namespace hid;

demo_app& demo_app::instance()
{
    using namespace hid::rdf;
    using namespace hid::page;
    // clang-format off
    static constexpr auto report_descriptor = descriptor(
        // first application: standard keyboard
        hid::app::keyboard::app_report_descriptor<report_ids::KEYBOARD>(),

        // second application: standard mouse
        hid::app::mouse::app_report_descriptor<report_ids::MOUSE>(),

        // third application: raw data
        usage_extended(custom_page::APPLICATION),
        collection::application(
            hid::app::opaque::report_descriptor<raw_in_report>(custom_page::IN_DATA),

            hid::app::opaque::report_descriptor<raw_out_report>(custom_page::OUT_DATA)
        )
    );
    // clang-format on
    static constexpr hid::report_protocol rp(report_descriptor);

    // proving the correctness of the descriptor parser
    static_assert(
        rp.descriptor
            .tag_value_unsigned_most(rdf::global::tag::REPORT_ID,
                                     [](const std::uint32_t& most, const std::uint32_t& current)
                                     { return most < current; })
            ->value_unsigned() == report_ids::MAX);
    static_assert(rp.max_input_size == sizeof(raw_in_report));
    static_assert(rp.max_output_size == sizeof(raw_out_report));
    static_assert(rp.max_feature_size == 0);
    static_assert(rp.max_report_id() == report_ids::MAX);

    static demo_app app(rp);
    return app;
}

void demo_app::start(protocol prot)
{
    receive_report(&_raw_out_buffer);
}

void demo_app::stop() {}

void demo_app::button_state_change(bool pressed)
{
    _keys_buffer.set_key_state(page::keyboard_keypad::KEYBOARD_CAPS_LOCK, pressed);

    if (send_report(&_keys_buffer) != result::OK)
    {
        // TODO deal with BUSY scenario
    }
}

void demo_app::set_report(report::type type, const std::span<const uint8_t>& data)
{
    // only output reports provided
    assert(type == report::type::OUTPUT);

    // data[0] is the report ID only if report IDs are used
    if (data[0] == kb_leds_report::ID)
    {
        auto* out_report = reinterpret_cast<const kb_leds_report*>(data.data());

        // use num_lock and caps_lock flag
        set_led(out_report->leds.test(page::leds::CAPS_LOCK));
    }
    else // if (data[0] == _raw_out_buffer.id())
    {
        // TODO: deal with custom data
    }

    receive_report(&_raw_out_buffer);
}

void demo_app::get_report(report::selector select, const std::span<uint8_t>& buffer)
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
        // not typical scenario
        send_report({}, select.type());
    }
}
