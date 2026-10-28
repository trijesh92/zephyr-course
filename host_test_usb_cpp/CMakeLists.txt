cmake_minimum_required(VERSION 3.10)
project(zephyr_host_test_usb_cpp_winusb LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# WinUSB is part of Windows SDK - no external package needed
# Link against setupapi.lib and winusb.lib (already referenced via #pragma comment in source)

add_executable(host_test_usb
    main.cpp
    usb_protocol.h
)

target_compile_options(host_test_usb PRIVATE
    -Wall
)

if(MSVC)
    target_compile_options(host_test_usb PRIVATE /W4 /EHsc)
endif()

# Windows-specific link libraries
if(WIN32)
    target_link_libraries(host_test_usb PRIVATE setupapi.lib winusb.lib)
endif()
