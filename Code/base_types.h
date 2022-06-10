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
#ifndef __BASE_TYPES_H_
#define __BASE_TYPES_H_

#include "etl/span.h"
#include "etl/unaligned_type.h"

using le_uint16_t = etl::le_uint16_t;
using le_uint32_t = etl::le_uint32_t;

template <typename T, size_t EXTENT = etl::dynamic_extent>
using span = etl::span<T, EXTENT>;

// a simplified single element queue
template<typename T>
class simple_queue
{
public:
    constexpr simple_queue()
    {}
    constexpr size_t size() const
    { return 1; }
    constexpr bool empty() const
    { return !_set; }
    constexpr bool full() const
    { return _set; }
    constexpr void clear()
    { _set = false; }
    constexpr bool push(const T& value)
    {
        if (full())
        {
            return false;
        }
        else
        {
            _value = value;
            _set = true;
            return true;
        }
    }
    constexpr bool front(T& value)
    {
        if (empty())
        {
            return false;
        }
        else
        {
            value = _value;
            return true;
        }
    }
    constexpr bool pop(T& value)
    {
        if (empty())
        {
            return false;
        }
        else
        {
            value = _value;
            _set = false;
            return true;
        }
    }
    constexpr bool pop()
    {
        if (empty())
        {
            return false;
        }
        else
        {
            _set = false;
            return true;
        }
    }
private:
    T _value;
    bool _set = false;
};

#endif // __BASE_TYPES_H_
