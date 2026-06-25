# Copyright (c) 2026 Microchip Technology Inc.
# SPDX-License-Identifier: Apache-2.0

zephyr_include_directories(.)

if("${ZEPHYR_TOOLCHAIN_VARIANT}" STREQUAL "xc32")
  # --- Device-specific flags from board.cmake ---
  if(NOT DEFINED XC32_PROCESSOR)
    message(FATAL_ERROR "XC32_PROCESSOR not defined. Set it in your board's board.cmake")
  endif()

  # Resolve DFP path
  if(DEFINED ENV{DFP_DIR})
    file(TO_CMAKE_PATH "$ENV{DFP_DIR}" _dfp_path)
  else()
    if(NOT DEFINED XC32_DFP_FAMILY)
      message(FATAL_ERROR "XC32_DFP_FAMILY not defined. Set it in your board's board.cmake")
    endif()
    if(DEFINED ENV{USERPROFILE})
      set(_home "$ENV{USERPROFILE}")
    elseif(DEFINED ENV{HOME})
      set(_home "$ENV{HOME}")
    endif()
    file(GLOB _dfp_versions "${_home}/.mchp_packs/Microchip/${XC32_DFP_FAMILY}/*")
    list(SORT _dfp_versions)
    list(GET _dfp_versions -1 _dfp_path)
    file(TO_CMAKE_PATH "${_dfp_path}" _dfp_path)
  endif()

  # Apply device flags globally
  zephyr_compile_options(-mprocessor=${XC32_PROCESSOR} -mdfp=${_dfp_path})
  zephyr_link_libraries(-mprocessor=${XC32_PROCESSOR} -mdfp=${_dfp_path})
  get_filename_component(_xc32_bin_dir "${CMAKE_C_COMPILER}" DIRECTORY)
  get_filename_component(_xc32_root "${_xc32_bin_dir}" DIRECTORY)
  zephyr_link_libraries(-L"${_xc32_root}/lib/gcc/pic32m/13.2.1/fpu64")

  # Select linker script based on kernel usage.
  # Bare-metal (PIC32M_NO_KERNEL_BOOT defined): use minimal XC32 linker script.
  # Kernel mode: use extended linker script with Zephyr sections.
  if(DEFINED PIC32M_NO_KERNEL_BOOT)
    set(SOC_LINKER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/linker_xc32.ld CACHE INTERNAL "")
  else()
    set(SOC_LINKER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/linker_xc32_kernel.ld CACHE INTERNAL "")
    zephyr_sources(pic32m_boot.c)
    zephyr_sources(gen_handler.S)
    zephyr_sources(irq_vector.S)
    zephyr_sources(pic32mz_isr.S)
    zephyr_link_libraries(-Wl,--wrap=main)
  endif()

  # Post-build: regenerate HEX using xc32-bin2hex (converts KSEG→physical).
  find_program(XC32_BIN2HEX xc32-bin2hex PATHS ${TOOLCHAIN_HOME} NO_DEFAULT_PATH)
  if(XC32_BIN2HEX)
    set_property(GLOBAL APPEND PROPERTY extra_post_build_commands
      COMMAND ${XC32_BIN2HEX} ${PROJECT_BINARY_DIR}/${KERNEL_ELF_NAME}
    )
  endif()

  # Device Configuration Words - handled by XC32 via #pragma config in source

else()
  # Zephyr SDK MIPS toolchain: needs custom startup, linker, address conversion.

  # PIC32MZ uses MIPS32r2 (microAptiv core)
  zephyr_compile_options(-march=mips32r2)

  # Use PIC32MZ-specific linker script
  set(SOC_LINKER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/linker.ld CACHE INTERNAL "")

  # Boot vector code
  zephyr_sources(vector.S)

  # Device Configuration Words
  zephyr_sources_ifdef(CONFIG_PIC32MZ_DEVCFG devcfg.c)

  # Post-build: convert HEX from KSEG virtual addresses to physical
  if(CONFIG_BUILD_OUTPUT_HEX)
    set_property(GLOBAL APPEND PROPERTY extra_post_build_commands
      COMMAND ${PYTHON_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/kseg_to_phys.py
        ${PROJECT_BINARY_DIR}/${KERNEL_HEX_NAME}
        ${PROJECT_BINARY_DIR}/${KERNEL_HEX_NAME}
    )
  endif()
endif()
