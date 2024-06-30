target_sources(${PROJECT_NAME} PRIVATE
    hid/demo_app.cpp
    i2c_hid_config.cpp
    st/hal_i2c_slave.cpp
    cortex_m0_atomic.cpp
    newlib_diet.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# a dummy target to run compile time verification of report descriptor
add_library(${PROJECT_NAME}-verify)
target_sources(${PROJECT_NAME}-verify PRIVATE
    hid/demo_app.cpp
)
target_include_directories(${PROJECT_NAME}-verify PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(${PROJECT_NAME}-verify PRIVATE
    c2usb
)
target_compile_options(${PROJECT_NAME}-verify PRIVATE "-fexceptions")
