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
#ifndef __HID_DEMO_APP_HPP_
#define __HID_DEMO_APP_HPP_

#include "hid/app/keyboard.hpp"
#include "hid/app/mouse.hpp"
#include "hid/app/opaque.hpp"
#include "hid/application.hpp"

namespace hid
{
namespace page
{
enum class custom_page : std::uint8_t
{
    APPLICATION = 0x0001,
    IN_DATA = 0x0002,
    OUT_DATA = 0x0003,
};
template <>
struct info<custom_page>
{
    constexpr static page_id_t page_id = 0xff01;
    constexpr static usage_id_t max_usage_id = 3;
    constexpr static const char* name = "vendor";
};
} // namespace page

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
    static demo_app& instance();

    void button_state_change(bool pressed);

    using keys_report = app::keyboard::keys_input_report<report_ids::KEYBOARD>;
    using kb_leds_report = app::keyboard::output_report<report_ids::KEYBOARD>;
    using mouse_report = app::mouse::report<report_ids::MOUSE>;
    using raw_in_report = app::opaque::report<32, report::type::INPUT, report_ids::OPAQUE>;
    using raw_out_report = app::opaque::report<32, report::type::OUTPUT, report_ids::OPAQUE>;

  private:
    keys_report _keys_buffer;
    mouse_report _mouse_buffer;
    raw_in_report _raw_in_buffer;
    raw_out_report _raw_out_buffer;

    constexpr explicit demo_app(const hid::report_protocol& rp) : application(rp) {}

    void start(protocol prot) override;
    void stop() override;
    void set_report(report::type type, const std::span<const uint8_t>& data) override;
    void get_report(report::selector select, const std::span<uint8_t>& buffer) override;
};

} // namespace hid

#endif // __HID_DEMO_APP_HPP_
