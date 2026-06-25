# Copyright (c) 2026 Microchip Technology Inc.
# SPDX-License-Identifier: Apache-2.0
#
# CMake toolchain file for Microchip XC32.
# Processed BEFORE any project() call to skip compiler identification.

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR MIPS)

# --- XC32 compiler path ---
# Override with environment variable XC32_DIR, otherwise auto-detect latest.
if(DEFINED ENV{XC32_DIR})
  file(TO_CMAKE_PATH "$ENV{XC32_DIR}" _xc32_root)
else()
  file(GLOB _xc32_versions "C:/Program Files/Microchip/xc32/v*")
  list(SORT _xc32_versions)
  list(GET _xc32_versions -1 _xc32_root)
  file(TO_CMAKE_PATH "${_xc32_root}" _xc32_root)
endif()

set(CMAKE_C_COMPILER "${_xc32_root}/bin/xc32-gcc.exe")
set(CMAKE_CXX_COMPILER "${_xc32_root}/bin/xc32-g++.exe")
set(CMAKE_ASM_COMPILER "${_xc32_root}/bin/xc32-gcc.exe")

set(CMAKE_C_COMPILER_WORKS TRUE)
set(CMAKE_CXX_COMPILER_WORKS TRUE)
set(CMAKE_ASM_COMPILER_WORKS TRUE)

set(CMAKE_C_COMPILER_ID GNU)
set(CMAKE_C_COMPILER_VERSION "13.2.1")
set(CMAKE_CXX_COMPILER_ID GNU)
set(CMAKE_CXX_COMPILER_VERSION "13.2.1")
set(CMAKE_C_COMPILER_FORCED TRUE)
set(CMAKE_CXX_COMPILER_FORCED TRUE)
set(CMAKE_C_ABI_COMPILED YES)
set(CMAKE_CXX_ABI_COMPILED YES)
set(CMAKE_ASM_ABI_COMPILED YES)

set(CMAKE_SYSROOT "${_xc32_root}/pic32m")

# --- Compiler/linker flags ---
# Device-specific flags (-mprocessor, -mdfp) are added later by the SoC
# CMakeLists.txt via zephyr_compile_options(), because board.cmake (which
# defines XC32_PROCESSOR) is not yet processed at toolchain file time.
# Here we only set minimal flags needed for CMake configuration to succeed.
set(CMAKE_C_FLAGS "" CACHE STRING "" FORCE)
set(CMAKE_CXX_FLAGS "" CACHE STRING "" FORCE)
set(CMAKE_ASM_FLAGS "" CACHE STRING "" FORCE)
set(CMAKE_EXE_LINKER_FLAGS "-Wa,-W" CACHE STRING "" FORCE)
