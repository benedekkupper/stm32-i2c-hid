// https://stackoverflow.com/questions/74333402/how-to-implement-atomic-operations-on-multi-core-cortex-m0-m0-no-swp-no-ldr
// https://stackoverflow.com/questions/71626597/what-are-the-various-ways-to-disable-and-re-enable-interrupts-in-stm32-microcont
#include "st/stm32cmsis.h"

class interrupt_lock
{
    uint32_t priomask_;

  public:
    interrupt_lock()
    {
        priomask_ = __get_PRIMASK();
        __disable_irq();
    }

    ~interrupt_lock()
    {
        if (priomask_ == 0)
        {
            __enable_irq();
        }
    }
};

#if __CORTEX_M < 3
extern "C" bool __atomic_compare_exchange_4(volatile void* ptr, void* expected,
                                            unsigned int desired, [[maybe_unused]] bool weak,
                                            [[maybe_unused]] int success_memorder,
                                            [[maybe_unused]] int failure_memorder)
{
    interrupt_lock lock;
    unsigned int value = *(volatile unsigned int*)ptr;
    if (value == *(unsigned int*)expected)
    {
        *(volatile unsigned int*)ptr = desired;
        return true;
    }
    *(unsigned int*)expected = value;
    return false;
}

#endif