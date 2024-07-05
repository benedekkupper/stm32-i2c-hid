cmake_minimum_required(VERSION 3.22)

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Include toolchain file
include("cmake/gcc-arm-none-eabi.cmake")

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Enable CMake support for ASM and C languages
enable_language(C CXX ASM)

project(stm32-i2c-hid)

add_executable(${PROJECT_NAME})

add_subdirectory(cmake/stm32cubemx)
add_subdirectory(c2usb)
add_subdirectory(${PROJECT_NAME})

target_include_directories(${PROJECT_NAME} PRIVATE
    Core/Inc
    stm32header
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    stm32cubemx
    c2usb
)
