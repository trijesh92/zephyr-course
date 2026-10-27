cmake_minimum_required(VERSION 3.10)
project(zephyr_host_test_usb_cpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find libusb-1.0
find_package(LibUSB1 REQUIRED)

add_executable(host_test_usb
    main.cpp
    usb_protocol.h
)

target_link_libraries(host_test_usb PRIVATE LibUSB1::LibUSB1)

target_compile_options(host_test_usb PRIVATE
    -Wall -Wextra -Wpedantic
)

if(MSVC)
    target_compile_options(host_test_usb PRIVATE /W4 /EHsc)
endif()
