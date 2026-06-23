# Copyright (c) 2026 Microchip Technology Inc.
# SPDX-License-Identifier: Apache-2.0

# XC32 MIPS target flags - intentionally empty for compile flags.
# All required flags (-mprocessor, -mdfp) are set in CMAKE_C_FLAGS
# via the toolchain file (xc32_toolchain_file.cmake).

# Linker flags
list(APPEND TOOLCHAIN_LD_FLAGS -Wl,--gc-sections)
list(APPEND TOOLCHAIN_LD_FLAGS -Wl,--no-code-in-dinit)
list(APPEND TOOLCHAIN_LD_FLAGS -Wl,--no-dinit-in-serial-mem)

# Add DFP linker script directory to search path so INCLUDE "p32MZ..." works
if(DEFINED ENV{USERPROFILE})
  set(_home "$ENV{USERPROFILE}")
elseif(DEFINED ENV{HOME})
  set(_home "$ENV{HOME}")
endif()
file(GLOB _dfp_vers "${_home}/.mchp_packs/Microchip/PIC32MZ-EF_DFP/*")
if(_dfp_vers)
  list(SORT _dfp_vers)
  list(GET _dfp_vers -1 _dfp)
  file(TO_CMAKE_PATH "${_dfp}/xc32/32MZ2048EFH144" _dfp_ld_dir)
  list(APPEND TOOLCHAIN_LD_FLAGS -L${_dfp_ld_dir})
endif()
