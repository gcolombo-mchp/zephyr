# Copyright (c) 2026 Microchip Technology Inc.
# SPDX-License-Identifier: Apache-2.0

# XC32 compiler configuration for Zephyr.
# XC32 is based on GCC but with Microchip-specific extensions.
# We reuse most GCC compiler property definitions.

include(${ZEPHYR_BASE}/cmake/compiler/gcc/generic.cmake)

# Override: XC32 does not use picolibc specs
list(REMOVE_ITEM TOOLCHAIN_C_FLAGS "-specs=picolibc.specs")
list(REMOVE_ITEM TOOLCHAIN_LD_FLAGS "-specs=picolibc.specs")
