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

# --- DFP path ---
# Override with environment variable DFP_DIR, otherwise auto-detect latest.
set(XC32_PROCESSOR "32MZ2048EFH144")

if(DEFINED ENV{DFP_DIR})
  file(TO_CMAKE_PATH "$ENV{DFP_DIR}" _dfp_path_normalized)
else()
  if(DEFINED ENV{USERPROFILE})
    set(_home "$ENV{USERPROFILE}")
  elseif(DEFINED ENV{HOME})
    set(_home "$ENV{HOME}")
  endif()
  file(GLOB _dfp "${_home}/.mchp_packs/Microchip/PIC32MZ-EF_DFP/*")
  list(SORT _dfp)
  list(GET _dfp -1 _dfp_path)
  file(TO_CMAKE_PATH "${_dfp_path}" _dfp_path_normalized)
endif()

# --- Compiler/linker flags ---
set(_xc32_common_flags "-mprocessor=${XC32_PROCESSOR} -mdfp=${_dfp_path_normalized}")

set(CMAKE_C_FLAGS "${_xc32_common_flags}" CACHE STRING "" FORCE)
set(CMAKE_CXX_FLAGS "${_xc32_common_flags}" CACHE STRING "" FORCE)
set(CMAKE_ASM_FLAGS "${_xc32_common_flags}" CACHE STRING "" FORCE)
set(CMAKE_EXE_LINKER_FLAGS "${_xc32_common_flags} -Wa,-W -L\"${_xc32_root}/lib/gcc/pic32m/13.2.1/fpu64\"" CACHE STRING "" FORCE)
