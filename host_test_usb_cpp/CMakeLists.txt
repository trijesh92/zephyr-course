cmake_minimum_required(VERSION 3.10)
project(zephyr_host_test_usb_cpp_winusb LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# WinUSB, SetupAPI and cfgmgr32 are part of the Windows SDK.
# Link against setupapi.lib, winusb.lib and cfgmgr32.lib.

add_executable(host_test_usb
    src/main.cpp
    src/usb_connection.cpp
    src/usb_protocol.cpp
    src/usb_app.cpp
    include/usb_connection.h
    include/usb_protocol.h
    include/usb_app.h
)

target_include_directories(host_test_usb PRIVATE include)

target_compile_options(host_test_usb PRIVATE -Wall)

if(MSVC)
    target_compile_options(host_test_usb PRIVATE /W4 /EHsc)
endif()

if(WIN32)
    target_link_libraries(host_test_usb PRIVATE setupapi.lib winusb.lib cfgmgr32.lib)
endif()
