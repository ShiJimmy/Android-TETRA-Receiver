# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (C) 2026 Shi Jimmy
# ---------------------------------------------------------------------------
# TETRA Android receiver - native library (libtetra.so)
#
# Builds the complete receive chain:
#
#   libusb + librtlsdr (USB)  ->  dqpsk_demod (liquid-dsp)  ->  osmo-tetra
#   burst sync / MAC  ->  ETSI speech vocoder  ->  PCM  ->  JNI -> AudioTrack
#
# Consumed by the Android Gradle plugin (externalNativeBuild).
# ---------------------------------------------------------------------------
cmake_minimum_required(VERSION 3.20)
project(tetra C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# ------------------------------------------------------------------ debug
# In-app debug facilities (stdout/stderr log capture, iq.bin / bits.bin /
# diag.txt dumps, verbose status) are compiled OUT by default so that any
# build without an explicit flag is clean.  Only the Gradle `debug` build
# type passes -DTETRA_DEBUG=ON.
option(TETRA_DEBUG "Enable in-app debug features" OFF)
if(TETRA_DEBUG)
  add_compile_definitions(TETRA_DEBUG=1)
else()
  add_compile_definitions(TETRA_DEBUG=0)
endif()

# ------------------------------------------------------------------ vendors
add_subdirectory(thirdparty/libusb)          # target: usb-1.0
add_subdirectory(thirdparty/librtlsdr)       # target: rtlsdr (links usb-1.0)
add_subdirectory(thirdparty/liquid-dsp)      # target: liquid

# --------------------------------------------------------------- osmo-tetra
# compat layer + TETRA protocol stack (vendored from osmo-tetra-sq5bpf-2).
file(GLOB_RECURSE TETRA_SOURCES
  "${CMAKE_CURRENT_SOURCE_DIR}/compat/src/*.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/tetra/*.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/tetra/phy/*.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/tetra/lower_mac/*.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/tetra/crypto/*.c"
)
add_library(tetra_decoder STATIC ${TETRA_SOURCES})
target_include_directories(tetra_decoder PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/compat/include"
  "${CMAKE_CURRENT_SOURCE_DIR}/compat/libosmocore"
  "${CMAKE_CURRENT_SOURCE_DIR}/tetra"
)

# -------------------------------------------------------------- ETSI codec
# The TETRA speech decoder: amr-code/ channel decoder + c-code/ speech decoder.
set(ETSI "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/etsi_codec")

# Not distributed with this project (see thirdparty/etsi_codec/PROVENANCE.txt).
if(NOT EXISTS "${ETSI}/amr-code/cdec_tet.c")
  message(FATAL_ERROR
    "ETSI TETRA speech codec not found under ${ETSI}.\n"
    "It is not redistributed with this project.  Run\n"
    "  tools/fetch_etsi_codec.ps1   (Windows)\n"
    "  tools/fetch_etsi_codec.sh    (Linux/macOS)\n"
    "first, and read the licence notes shipped with the archive.")
endif()
set(CODEC_SOURCES
  "${ETSI}/amr-code/cdec_tet.c"
  "${ETSI}/amr-code/sub_cd.c"
  "${ETSI}/amr-code/init_params.c"
  "${ETSI}/amr-code/tetra_op.c"
  "${ETSI}/c-code/sdec_tet.c"
  "${ETSI}/c-code/sub_sc_d.c"
  "${ETSI}/c-code/sub_dsp.c"
  "${ETSI}/c-code/fbas_tet.c"
  "${ETSI}/c-code/fexp_tet.c"
  "${ETSI}/c-code/fmat_tet.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/voice/tetra_vocoder.c"
)
add_library(tetra_codec STATIC ${CODEC_SOURCES})
target_include_directories(tetra_codec PUBLIC
  "${ETSI}/amr-code"
  "${ETSI}/c-code"
  "${CMAKE_CURRENT_SOURCE_DIR}/voice"
)

# ------------------------------------------------------------------- engine
# π/4-DQPSK demodulator + receive engine (ties demod -> burst sync -> vocoder).
add_library(tetra_engine STATIC
  "${CMAKE_CURRENT_SOURCE_DIR}/engine/dqpsk_demod.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/engine/spectrum.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/engine/tetra_engine.c"
)
target_include_directories(tetra_engine PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/engine"
)
target_link_libraries(tetra_engine PUBLIC liquid tetra_decoder tetra_codec)

# ------------------------------------------------------------------ libtetra
# JNI bridge + USB capture thread -> shared library loaded by the app.
add_library(tetra SHARED
  "${CMAKE_CURRENT_SOURCE_DIR}/tetra_usb.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/tetra_jni.c"
)
target_link_libraries(tetra
  PRIVATE tetra_engine
  PRIVATE rtlsdr
  PRIVATE usb-1.0
  PRIVATE log
)

# ------------------------------------------------------------------ release
# In release builds compile the diagnostic printf()/fprintf() out of our own
# targets (vendored decoder + engine + JNI/USB).  Third-party libs are left
# alone; their (rare) error prints already go to /dev/null.
if(NOT TETRA_DEBUG)
  foreach(_tgt tetra_decoder tetra_codec tetra_engine tetra)
    target_compile_options(${_tgt} PRIVATE
      -include "${CMAKE_CURRENT_SOURCE_DIR}/silence_debug.h")
  endforeach()
endif()
