# Copyright (c) 2026 Microchip Technology Inc.
# SPDX-License-Identifier: Apache-2.0

# Microchip XC32 toolchain for PIC32 MIPS devices.

zephyr_get(XC32_TOOLCHAIN_PATH)

if(NOT XC32_TOOLCHAIN_PATH)
  if(EXISTS "C:/Program Files/Microchip/xc32/v5.10")
    set(XC32_TOOLCHAIN_PATH "C:/Program Files/Microchip/xc32/v5.10")
  endif()
endif()

if(NOT XC32_TOOLCHAIN_PATH)
  message(FATAL_ERROR "XC32_TOOLCHAIN_PATH is not set.")
endif()

set(TOOLCHAIN_HOME ${XC32_TOOLCHAIN_PATH}/bin)

set(COMPILER xc32)
set(LINKER xc32)
set(BINTOOLS xc32)
set(CROSS_COMPILE ${TOOLCHAIN_HOME}/xc32-)

# The CMAKE_TOOLCHAIN_FILE (xc32_toolchain_file.cmake) must be passed on the
# command line or set in CMakeLists before find_package(Zephyr).
# It handles: compiler identification skip, CMAKE_C_FLAGS with -mprocessor/-mdfp.

# XC32 uses its own libc
set(TOOLCHAIN_HAS_PICOLIBC OFF CACHE BOOL "")
set(TOOLCHAIN_HAS_NEWLIB OFF CACHE BOOL "")
