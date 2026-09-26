# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (C) 2026 Shi Jimmy
# ---------------------------------------------------------------------------
# libusb (vendored) - Android static library.
#
# Replaces libusb's android/jni/libusb.mk with a CMake equivalent:
#   * config.h         <- copied from upstream libusb/android/config.h
#   * linux_usbfs.c    <- usbfs(4) backend, works with the file descriptor
#                         handed over by UsbDeviceConnection.getFileDescriptor()
#   * events_posix.c / threads_posix.c
#
# Only built for Android (the host test harness does not need USB).
# ---------------------------------------------------------------------------
cmake_minimum_required(VERSION 3.20)

set(LIBUSB_SOURCES
    libusb/core.c
    libusb/descriptor.c
    libusb/hotplug.c
    libusb/io.c
    libusb/sync.c
    libusb/strerror.c
    libusb/os/linux_usbfs.c
    libusb/os/events_posix.c
    libusb/os/threads_posix.c)

add_library(usb-1.0 STATIC ${LIBUSB_SOURCES})

target_include_directories(usb-1.0
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/libusb"
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")   # config.h

target_compile_options(usb-1.0 PRIVATE -fvisibility=hidden -pthread -O2 -w)
target_link_libraries(usb-1.0 PUBLIC log)
