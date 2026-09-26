# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (C) 2026 Shi Jimmy
# ---------------------------------------------------------------------------
# librtlsdr (RTL-SDR Blog fork) - static library.
#
# Driven by tetRa-android's top level CMakeLists.txt, which provides the
# `usb-1.0` target (vendored libusb).  On Android the library is used through
# rtlsdr_open_fd() (see open_fd.patch) because an unprivileged app can not let
# libusb enumerate USB devices.
# ---------------------------------------------------------------------------
cmake_minimum_required(VERSION 3.20)

add_library(rtlsdr STATIC
    src/librtlsdr.c
    src/tuner_e4k.c
    src/tuner_fc0012.c
    src/tuner_fc0013.c
    src/tuner_fc2580.c
    src/tuner_r82xx.c)

target_include_directories(rtlsdr
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")

target_compile_definitions(rtlsdr PRIVATE _GNU_SOURCE=1)

# librtlsdr prints "Detached kernel driver" only with DETACH_KERNEL_DRIVER;
# on Android the kernel driver of a DVB dongle (dvb_usb_rtl28xxu) is usually
# not bound, and detaching it needs root, so it is left disabled by default.
if(RTLSDR_DETACH_KERNEL_DRIVER)
    target_compile_definitions(rtlsdr PRIVATE DETACH_KERNEL_DRIVER)
endif()

if(TARGET usb-1.0)
    target_link_libraries(rtlsdr PUBLIC usb-1.0)
endif()

if(NOT ANDROID AND NOT MSVC)
    target_link_libraries(rtlsdr PUBLIC m)
endif()
