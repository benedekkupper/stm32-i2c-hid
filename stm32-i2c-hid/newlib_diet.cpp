#include "main.h"

/*
 * Some context:
 * https://hackaday.com/2021/07/19/the-newlib-embedded-c-standard-library-and-how-to-use-it/
 * https://stackoverflow.com/questions/48711221/how-to-prevent-inclusion-of-c-library-destructors-and-atexit
 * https://stackoverflow.com/questions/56550457/what-are-the-possible-side-effects-of-using-gccs-fno-math-errno
 */

/* needed when linking with -nostartfiles or -nostdlib */
extern "C" void _init(void) {}
extern "C" void _fini(void) {}

/* needed to eliminate the use of newlib reentrancy and impure_data burning up RAM */
extern "C" void __register_exitproc(void) {}

void operator delete(void*, unsigned int) {}
void operator delete(void*) {}

/* needed to avoid pulling in a lot of library code */
#ifndef NDEBUG
#if (defined(__CC_ARM)) || (defined(__ARMCC_VERSION)) || (defined(__ICCARM__))
extern "C" void __aeabi_assert(const char* failedExpr, const char* file, int line)
{
    // PRINTF("ASSERT ERROR \" %s \": file \"%s\" Line \"%d\" \n", failedExpr, file, line);
    while (true)
    {
        __BKPT(0);
    }
}
#elif (defined(__GNUC__))
extern "C" void __assert_func(const char* file, int line, const char* func, const char* failedExpr)
{
    // PRINTF("ASSERT ERROR \" %s \": file \"%s\" Line \"%d\" function name \"%s\" \n", failedExpr,
    // file, line, func);
    while (true)
    {
        __BKPT(0);
    }
}
#endif /* (defined(__CC_ARM) || (defined(__ICCARM__)) || (defined(__ARMCC_VERSION)) */
#endif /* NDEBUG */
