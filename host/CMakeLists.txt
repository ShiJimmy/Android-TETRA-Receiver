# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (C) 2026 Shi Jimmy
cmake_minimum_required(VERSION 3.16)
project(tetra_host C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(NOT MSVC)
  add_compile_options(-Wall -Wextra -Wno-unused-parameter)
endif()

# Root of the Android native sources.
set(TETRA_CPP "${CMAKE_CURRENT_LIST_DIR}/../app/src/main/cpp")

file(GLOB_RECURSE TETRA_SOURCES
  "${TETRA_CPP}/compat/src/*.c"
  "${TETRA_CPP}/tetra/*.c"
  "${TETRA_CPP}/tetra/phy/*.c"
  "${TETRA_CPP}/tetra/lower_mac/*.c"
  "${TETRA_CPP}/tetra/crypto/*.c"
)

add_library(tetra_decoder STATIC ${TETRA_SOURCES})

target_include_directories(tetra_decoder PUBLIC
  "${TETRA_CPP}/compat/include"
  "${TETRA_CPP}/compat/libosmocore"
  "${TETRA_CPP}/tetra"
)

# Host-only socket shim (maps POSIX socket headers onto WinSock2).
target_include_directories(tetra_decoder PUBLIC
  "${CMAKE_CURRENT_LIST_DIR}/include"
)

if(WIN32)
  target_link_libraries(tetra_decoder PUBLIC ws2_32)
endif()

# ------------------------------------------------------------------ smoke test
add_executable(tetra_smoke "${CMAKE_CURRENT_LIST_DIR}/tests/smoke.c")
target_link_libraries(tetra_smoke PRIVATE tetra_decoder)

# -------------------------------------------------------------- ETSI codec
# The TETRA speech decoder: amr-code/ channel decoder + c-code/ speech decoder.
set(ETSI "${TETRA_CPP}/thirdparty/etsi_codec")

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
  "${TETRA_CPP}/voice/tetra_vocoder.c"
)

add_library(tetra_codec STATIC ${CODEC_SOURCES})
target_include_directories(tetra_codec PUBLIC
  "${ETSI}/amr-code"
  "${ETSI}/c-code"
  "${TETRA_CPP}/voice"
)

# Reference channel encoder (separate executable: cannot share globals with
# the decoder, so it links its own copy of the amr-code encoder files).
add_executable(encode_driver
  "${CMAKE_CURRENT_LIST_DIR}/tests/encode_driver.c"
  "${ETSI}/amr-code/ccod_tet.c"
  "${ETSI}/amr-code/sub_cc.c"
  "${ETSI}/amr-code/init_params.c"
  "${ETSI}/amr-code/tetra_op.c"
)
target_include_directories(encode_driver PRIVATE "${ETSI}/amr-code")

add_executable(decode_driver "${CMAKE_CURRENT_LIST_DIR}/tests/decode_driver.c")
target_link_libraries(decode_driver PRIVATE tetra_codec)

# ------------------------------------------------------------------ liquid-dsp
add_subdirectory("${TETRA_CPP}/thirdparty/liquid-dsp" liquid-build)

# ------------------------------------------------------------------ engine
add_library(tetra_engine STATIC
  "${TETRA_CPP}/engine/dqpsk_demod.c"
  "${TETRA_CPP}/engine/spectrum.c"
  "${TETRA_CPP}/engine/tetra_engine.c"
)
target_include_directories(tetra_engine PUBLIC
  "${TETRA_CPP}/engine"
)
target_link_libraries(tetra_engine PUBLIC liquid tetra_decoder tetra_codec)

add_executable(engine_test "${CMAKE_CURRENT_LIST_DIR}/tests/engine_test.c")
target_link_libraries(engine_test PRIVATE tetra_engine)

add_executable(iq_decode "${CMAKE_CURRENT_LIST_DIR}/tests/iq_decode.c")
target_link_libraries(iq_decode PRIVATE tetra_engine)

enable_testing()
add_test(NAME tetra_smoke COMMAND tetra_smoke)
add_test(NAME engine_test COMMAND engine_test)
