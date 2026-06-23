# Copyright (c) 2026 Microchip Technology Inc.
# SPDX-License-Identifier: Apache-2.0

# XC32 binary tools configuration.
# XC32 provides its own versions of GNU binutils (xc32-objcopy, xc32-ar, etc.)
# plus the critical xc32-bin2hex that correctly converts KSEG addresses to physical.

find_program(CMAKE_OBJCOPY xc32-objcopy PATHS ${TOOLCHAIN_HOME} NO_DEFAULT_PATH)
find_program(CMAKE_OBJDUMP xc32-objdump PATHS ${TOOLCHAIN_HOME} NO_DEFAULT_PATH)
find_program(CMAKE_AS      xc32-as      PATHS ${TOOLCHAIN_HOME} NO_DEFAULT_PATH)
find_program(CMAKE_AR      xc32-ar      PATHS ${TOOLCHAIN_HOME} NO_DEFAULT_PATH)
find_program(CMAKE_RANLIB  xc32-ranlib  PATHS ${TOOLCHAIN_HOME} NO_DEFAULT_PATH)
find_program(CMAKE_READELF xc32-readelf PATHS ${TOOLCHAIN_HOME} NO_DEFAULT_PATH)
find_program(CMAKE_NM      xc32-nm      PATHS ${TOOLCHAIN_HOME} NO_DEFAULT_PATH)
find_program(CMAKE_STRIP   xc32-strip   PATHS ${TOOLCHAIN_HOME} NO_DEFAULT_PATH)

# xc32-bin2hex for correct HEX generation (KSEG->physical conversion)
find_program(CMAKE_BIN2HEX xc32-bin2hex PATHS ${TOOLCHAIN_HOME} NO_DEFAULT_PATH)

# Include GNU bintools target for the standard elfconvert/objcopy commands
include(${ZEPHYR_BASE}/cmake/bintools/gnu/target.cmake)
