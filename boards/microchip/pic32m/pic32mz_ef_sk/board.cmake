# Copyright (c) 2026 Microchip Technology Inc.
# SPDX-License-Identifier: Apache-2.0

# --- XC32 Toolchain Configuration ---
# This board uses Microchip XC32 as the default compiler.
# These variables are consumed by cmake/toolchain/xc32/xc32_toolchain_file.cmake.

# Device-specific identifiers (from SoC)
set(XC32_PROCESSOR "32MZ2048EFH144" CACHE STRING "XC32 device processor name")
set(XC32_DFP_FAMILY "PIC32MZ-EF_DFP" CACHE STRING "XC32 Device Family Pack name")

# Select XC32 toolchain if not explicitly overridden
if(NOT DEFINED ZEPHYR_TOOLCHAIN_VARIANT OR "${ZEPHYR_TOOLCHAIN_VARIANT}" STREQUAL "")
  set(ZEPHYR_TOOLCHAIN_VARIANT xc32)
endif()

# Set the toolchain file path for XC32
if("${ZEPHYR_TOOLCHAIN_VARIANT}" STREQUAL "xc32")
  if(NOT DEFINED CMAKE_TOOLCHAIN_FILE OR "${CMAKE_TOOLCHAIN_FILE}" STREQUAL "")
    set(CMAKE_TOOLCHAIN_FILE
      "${ZEPHYR_BASE}/cmake/toolchain/xc32/xc32_toolchain_file.cmake"
      CACHE FILEPATH "" FORCE)
  endif()
endif()

# --- Flash Runner ---
# Uses PKOB (on-board debugger) via MPLAB IPE from MPLAB X v5.35
# (v5.35 required for PKOB support on PIC32MZ Starter Kits)
board_runner_args(mplab_ipe
  "--tool=PKOB"
  "--part=${XC32_PROCESSOR}"
  "--ipecmd-path=C:/Program Files (x86)/Microchip/MPLABX/v5.35/mplab_platform/mplab_ipe/ipecmd.exe"
)

include(${ZEPHYR_BASE}/boards/common/mplab_ipe.board.cmake)
