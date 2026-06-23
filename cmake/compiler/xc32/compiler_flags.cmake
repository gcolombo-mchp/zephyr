# Copyright (c) 2026 Microchip Technology Inc.
# SPDX-License-Identifier: Apache-2.0

# XC32 uses the same compiler flags as GCC (it IS GCC under the hood)
include(${ZEPHYR_BASE}/cmake/compiler/gcc/compiler_flags.cmake)

# Override: XC32 provides its own libc headers, do NOT use -nostdinc
set_compiler_property(PROPERTY nostdinc "")
# Also override the nosysdef property if it exists
set_compiler_property(PROPERTY nostdinc_include "")
