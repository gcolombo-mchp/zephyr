# Copyright (c) 2026 Microchip Technology Inc.
# SPDX-License-Identifier: Apache-2.0
#
# XC32 toolchain setup for PIC32MZ EF Starter Kit (DM320007).
# Include this BEFORE find_package(Zephyr) in your CMakeLists.txt:
#
#   include(${ZEPHYR_BASE}/boards/microchip/pic32m/pic32mz_ef_sk/toolchain.cmake)
#

# Derive ZEPHYR_BASE from this file's location
# This file is at: ${ZEPHYR_BASE}/boards/microchip/pic32m/pic32mz_ef_sk/toolchain.cmake
cmake_path(SET _zbase NORMALIZE "${CMAKE_CURRENT_LIST_DIR}/../../../../..")

if(NOT DEFINED ZEPHYR_TOOLCHAIN_VARIANT)
  set(ZEPHYR_TOOLCHAIN_VARIANT xc32)
endif()

if("${ZEPHYR_TOOLCHAIN_VARIANT}" STREQUAL "xc32" AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  set(CMAKE_TOOLCHAIN_FILE
    "${_zbase}/cmake/toolchain/xc32/xc32_toolchain_file.cmake"
    CACHE FILEPATH "" FORCE)
endif()
