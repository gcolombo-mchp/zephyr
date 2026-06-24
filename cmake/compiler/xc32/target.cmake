# Copyright (c) 2026 Microchip Technology Inc.
# SPDX-License-Identifier: Apache-2.0

# XC32 target configuration.
# We do NOT include gcc/target.cmake because it loads target_mips.cmake
# which adds incompatible flags (-EL, -msoft-float, -G0, etc.)
# XC32 handles all of this via -mprocessor=.

# Load XC32's own MIPS target flags
include(${CMAKE_CURRENT_LIST_DIR}/target_mips.cmake)
