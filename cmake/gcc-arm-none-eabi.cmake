set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(CMAKE_C_COMPILER_FORCED TRUE)
set(CMAKE_CXX_COMPILER_FORCED TRUE)
set(CMAKE_C_COMPILER_ID GNU)
set(CMAKE_CXX_COMPILER_ID GNU)

# Some default GCC settings
# arm-none-eabi- must be part of path environment
set(TOOLCHAIN_PREFIX arm-none-eabi-)

set(CMAKE_C_COMPILER ${TOOLCHAIN_PREFIX}gcc)
set(CMAKE_ASM_COMPILER ${CMAKE_C_COMPILER})
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}g++)
set(CMAKE_LINKER ${TOOLCHAIN_PREFIX}g++)
set(CMAKE_OBJCOPY ${TOOLCHAIN_PREFIX}objcopy)
set(CMAKE_SIZE ${TOOLCHAIN_PREFIX}size)

set(CMAKE_EXECUTABLE_SUFFIX_ASM ".elf")
set(CMAKE_EXECUTABLE_SUFFIX_C ".elf")
set(CMAKE_EXECUTABLE_SUFFIX_CXX ".elf")

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# MCU specific flags
set(TARGET_FLAGS "-mcpu=cortex-m0 ")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${TARGET_FLAGS}")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fdata-sections -ffunction-sections ")

# skip warnings
if(FALSE)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wpedantic ")
endif()

if(CMAKE_BUILD_TYPE MATCHES Debug)
    set(CMAKE_C_FLAGS " ${CMAKE_C_FLAGS} -O0 -g3 ")
endif()

if(CMAKE_BUILD_TYPE MATCHES Release)
    set(CMAKE_C_FLAGS " ${CMAKE_C_FLAGS} -Os -g0 ")
endif()

set(CMAKE_ASM_FLAGS " ${CMAKE_C_FLAGS} -x assembler-with-cpp -MMD -MP ")
set(CMAKE_CXX_FLAGS " ${CMAKE_C_FLAGS} -fno-rtti -fno-exceptions -fno-threadsafe-statics ")

# extra size trimming
set(CMAKE_CXX_FLAGS " ${CMAKE_CXX_FLAGS} -fno-use-cxa-atexit ")

set(CMAKE_C_LINK_FLAGS " ${TARGET_FLAGS} ")
set(CMAKE_C_LINK_FLAGS " ${CMAKE_C_LINK_FLAGS} -T \"${CMAKE_SOURCE_DIR}/STM32F072RBTx_FLASH.ld\" ")
set(CMAKE_C_LINK_FLAGS " ${CMAKE_C_LINK_FLAGS} --specs=nano.specs ")
set(CMAKE_C_LINK_FLAGS " ${CMAKE_C_LINK_FLAGS} -Wl,-Map=${CMAKE_PROJECT_NAME}.map -Wl,--gc-sections ")
set(CMAKE_C_LINK_FLAGS " ${CMAKE_C_LINK_FLAGS} -Wl,--start-group -lc -lm -Wl,--end-group ")
set(CMAKE_C_LINK_FLAGS " ${CMAKE_C_LINK_FLAGS} -Wl,--print-memory-usage ")

# extra size trimming
set(CMAKE_C_LINK_FLAGS " ${CMAKE_C_LINK_FLAGS} -nostartfiles -nodefaultlibs -nostdlib ")

set(CMAKE_CXX_LINK_FLAGS " ${CMAKE_C_LINK_FLAGS} -Wl,--start-group -lstdc++ -lsupc++ -Wl,--end-group ")