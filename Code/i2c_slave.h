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
#ifndef __I2C_SLAVE_H_
#define __I2C_SLAVE_H_

#include <cstddef>
#include <cstdint>

namespace i2c
{
    enum class direction : uint8_t
    {
        WRITE = 0, /// The master sends data to the receiver slave
        READ  = 1, /// The master receives data from the sender slave
    };

    class address
    {
    public:
        enum class mode : uint16_t
        {
            _7BIT  = 0,
            _10BIT = 0x7800,
        };

        constexpr address(uint16_t code, mode m = mode::_7BIT)
            : _code((code & code_mask(m)) | static_cast<uint16_t>(m))
        {
        }
        bool is_10bit() const
        {
            return (_code & MODE_MASK) == static_cast<uint16_t>(mode::_10BIT);
        }
        uint16_t raw() const
        {
            return _code;
        }
        // special reserved addresses
        static constexpr address general_call()
        {
            return address(0);
        }
        static constexpr address start_byte()
        {
            return address(1);
        }
        static constexpr address cbus()
        {
            return address(2);
        }
    private:
        static constexpr uint16_t code_mask(mode m)
        {
            return m == mode::_7BIT ? 0x7f : 0x3ff;
        }
        static constexpr uint16_t MODE_MASK = 0x7c00;
        const uint16_t _code;
    };

    class slave
    {
    public:
        static slave& instance();

        template<class T, bool(T::*ON_START)(direction, size_t), void(T::*ON_STOP)(direction, size_t)>
        void register_module(T* module, address slave_addr)
        {
            _on_start = [](void* m, direction dir, size_t s) {
                T* p = static_cast<T*>(m);
                return (p->*ON_START)(dir, s);
            };
            _on_stop = [](void* m, direction dir, size_t s) {
                T* p = static_cast<T*>(m);
                (p->*ON_STOP)(dir, s);
            };
            _module = static_cast<decltype(_module)>(module);

            // single module use case
            set_slave_address(slave_addr);
            start_listen();
        }

        template<class T>
        void unregister_module(T* module)
        {
            if (_module == static_cast<void*>(module))
            {
                _module = nullptr;
                _on_start = nullptr;
                _on_stop = nullptr;

                // single module use case, just shut down entirely
                stop_listen();
            }
        }

        void set_pin_interrupt(bool asserted);

        template <typename T>
        void send(const T& a)
        {
            send(a.data(), a.size());
        }
        template <typename T>
        void send(T* a)
        {
            send(reinterpret_cast<const uint8_t*>(a), sizeof(T));
        }
        template <typename T1, typename T2>
        void send(T1* a, T2 b)
        {
            send(reinterpret_cast<const uint8_t*>(a), sizeof(T1), b.data(), b.size());
        }
        template <typename T1, typename T2>
        void send(T1& a, T2& b)
        {
            send(a.data(), a.size(), b.data(), b.size());
        }

        template <typename T>
        void receive(T& a)
        {
            receive(a.data(), a.size());
        }
        template <typename T>
        void receive(T* a)
        {
            receive(reinterpret_cast<uint8_t*>(a), sizeof(T));
        }
        template <typename T1, typename T2>
        void receive(T1& a, T2& b)
        {
            receive(a.data(), a.size(), b.data(), b.size());
        }

        void handle_start(direction dir);
        void handle_tx_complete();
        void handle_rx_complete();
        void handle_stop();

    private:
        slave();
        void set_slave_address(address slave_addr);
        void start_listen();
        void stop_listen();
        void nack();
        void send_dummy();

        void send(const uint8_t* data, size_t size);
        void send(const uint8_t* data1, size_t size1, const uint8_t* data2, size_t size2);
        void receive(uint8_t* data, size_t size);
        void receive(uint8_t* data1, size_t size1, uint8_t* data2, size_t size2);

        // non-copyable
        slave(const slave&) = delete;
        slave& operator=(const slave&) = delete;
        // non-movable
        slave(const slave&&) = delete;
        slave& operator=(const slave&&) = delete;

    private:
        void* _module = nullptr;
        bool(*_on_start)(void *module, direction dir, size_t data_length) = nullptr;
        void(*_on_stop)(void *module, direction dir, size_t data_length) = nullptr;
        size_t _first_size;
        size_t _second_size;
        uint8_t* _second_data;
        direction _last_dir;
    };
}

#endif // __I2C_SLAVE_H_
