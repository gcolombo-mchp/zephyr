/**********************************************************************************************
 * © 2026 Microchip Technology Inc. and its subsidiares. All rights reserved.
 * This software includes AI generated code created using significant prompting. This
 * software is provided AS IS; you are Responsible for reviewing, testing, and validating for
 * your application.
 **********************************************************************************************/
/*
 * Copyright (c) 2026 Microchip Technology Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * PIC32MZ EF linker script.
 *
 * Memory layout (virtual addresses, KSEG0/KSEG1):
 *   Boot Flash:   0xBFC00000 (KSEG1, uncached) - 80KB (2 panels)
 *   Program Flash: 0x9D000000 (KSEG0, cached) - up to 2MB
 *   Data RAM:     0x80000000 (KSEG0, cached) - up to 512KB
 *   SFR:          0xBF800000 (KSEG1, uncached) - peripherals
 *   DEVCFG:       0xBFC0FFC0 (KSEG1, boot flash config space) - 16 bytes
 *
 * For XIP (execute-in-place from flash), code goes in FLASH region.
 * For RAM execution, a bootloader copies from flash to RAM first.
 * Default: XIP from program flash.
 */

#include <zephyr/linker/sections.h>
#include <zephyr/linker/linker-defs.h>
#include <zephyr/linker/linker-tool.h>
#include <zephyr/devicetree/sram.h>

#define ROMABLE_REGION FLASH
#define RAMABLE_REGION RAM

#define _VECTOR_SECTION_NAME    vector
#define _EXCEPTION_SECTION_NAME exceptions
#define _RESET_SECTION_NAME     reset

#define FLASH_BASE (CONFIG_FLASH_BASE_ADDRESS + CONFIG_FLASH_LOAD_OFFSET)
#define FLASH_SIZE (CONFIG_FLASH_SIZE * 1024)
#define RAM_BASE   DT_CHOSEN_SRAM_ADDR
#define RAM_SIZE   DT_CHOSEN_SRAM_SIZE

/*
 * PIC32MZ Memory Layout.
 *
 * The linker uses VIRTUAL addresses (KSEG0/KSEG1) because that's what the
 * CPU uses at runtime for instruction fetch and data access.
 * A post-build step converts the HEX from virtual to physical addresses
 * for MPLAB IPE programming (KSEG0: mask 0x1FFFFFFF, KSEG1: mask 0x1FFFFFFF).
 *
 * Virtual addresses used:
 *   0x9D000000 (KSEG0, cached)   - Program Flash execution
 *   0xBFC00000 (KSEG1, uncached) - Boot Flash (reset vector)
 *   0x80000000 (KSEG0, cached)   - Data RAM
 *   0xBFC0FFC0 (KSEG1)           - Configuration words
 */
MEMORY
{
    BOOT (rx)    : ORIGIN = 0xBFC00000, LENGTH = 0x14000
    FLASH (rx)   : ORIGIN = 0x9D000000, LENGTH = FLASH_SIZE
    RAM (rwx)    : ORIGIN = 0x80000000, LENGTH = RAM_SIZE
    DEVCFG_REGION (r) : ORIGIN = 0xBFC0FFC0, LENGTH = 0x10
    IDT_LIST (wx) : ORIGIN = 0xFFFFF7FF, LENGTH = 2K
}

REGION_ALIAS("REGION_TEXT", FLASH);
REGION_ALIAS("REGION_RODATA", FLASH);
REGION_ALIAS("REGION_DATA_VMA", RAM);
REGION_ALIAS("REGION_DATA_LMA", FLASH);
REGION_ALIAS("REGION_BSS", RAM);

ENTRY(CONFIG_KERNEL_ENTRY)

PROVIDE(__memory_base = RAM_BASE);
PROVIDE(__memory_size = RAM_SIZE);
PROVIDE(__stack = RAM_BASE + RAM_SIZE - 1024);

SECTIONS
{

    /*
     * Boot vector: MUST be first at 0xBFC00000 (reset entry point).
     * The CPU starts executing here after power-on reset.
     * Nothing else can precede this in the BOOT region.
     */
    SECTION_PROLOGUE(_VECTOR_SECTION_NAME,,)
    {
        KEEP(*(.vectors.*))
    } > BOOT

#include <zephyr/linker/rel-sections.ld>
#include <zephyr/linker/llext-sections.ld>

    SECTION_PROLOGUE(_RESET_SECTION_NAME,,)
    {
        . = ALIGN(0x10);
        KEEP(*(.reset.*))
    } GROUP_LINK_IN(ROMABLE_REGION)

    SECTION_PROLOGUE(_EXCEPTION_SECTION_NAME,,)
    {
        . = ALIGN(0x1000);
        KEEP(*(".exception.entry.*"))
        *(".exception.other.*")
    } GROUP_LINK_IN(ROMABLE_REGION)

    SECTION_PROLOGUE(_TEXT_SECTION_NAME,,)
    {
        . = ALIGN(4);
        *(.text)
        *(".text.*")
    } GROUP_LINK_IN(REGION_TEXT)

    __rodata_region_start = .;
#include <zephyr/linker/common-rom.ld>
#include <snippets-rom-sections.ld>
#include <zephyr/linker/thread-local-storage.ld>

    SECTION_PROLOGUE(_RODATA_SECTION_NAME,,)
    {
        . = ALIGN(8);
        *(.rodata)
        *(.rodata.*)
        *(.gnu.linkonce.r.*)
        *(.rodata1)
#include <snippets-rodata.ld>
    } GROUP_LINK_IN(REGION_RODATA)

#include <zephyr/linker/cplusplus-rom.ld>
    __rodata_region_end = .;

    SECTION_PROLOGUE(.plt,,)
    {
        *(.plt)
    }

    SECTION_PROLOGUE(.iplt,,)
    {
        *(.iplt)
    }

    SECTION_DATA_PROLOGUE(_DATA_SECTION_NAME,,)
    {
        . = ALIGN(8);
        _image_ram_start = .;
        __data_ram_start = .;

        *(.data)
        *(.data.*)
        *(.gnu.linkonce.d.*)
        *(.sdata)
        *(.sdata.*)
        . = ALIGN(8);
        SORT(CONSTRUCTORS)
#include <snippets-rwdata.ld>
    } GROUP_DATA_LINK_IN(REGION_DATA_VMA, REGION_DATA_LMA)

#include <zephyr/linker/common-ram.ld>
#include <snippets-ram-sections.ld>
#include <snippets-data-sections.ld>

    __data_ram_end = .;

    SECTION_DATA_PROLOGUE(_BSS_SECTION_NAME, (NOLOAD),)
    {
        . = ALIGN(4);
        __bss_start = .;
        *(.dynbss)
        *(.sbss)
        *(.sbss.*)
        *(.bss)
        *(.bss.*)
        *(.gnu.linkonce.b.*)
        *(.scommon)
        COMMON_SYMBOLS
        __bss_end = ALIGN(4);
    } GROUP_LINK_IN(REGION_BSS)

    SECTION_PROLOGUE(_NOINIT_SECTION_NAME, (NOLOAD),)
    {
        *(.noinit)
        *(.noinit.*)
#include <snippets-noinit.ld>
    } GROUP_LINK_IN(REGION_BSS)

#include <zephyr/linker/cplusplus-ram.ld>
#include <snippets-sections.ld>
#include <zephyr/linker/ram-end.ld>

    GROUP_END(RAMABLE_REGION)

    /*
     * Device Configuration Words.
     * Placed at fixed address 0xBFC0FFC0 in boot flash config space.
     */
    .pic32mz_devcfg : {
        KEEP(*(.pic32mz_devcfg))
        KEEP(*(".pic32mz_devcfg.*"))
    } > DEVCFG_REGION

#include <zephyr/linker/debug-sections.ld>

    .mdebug.abi32 : {
        KEEP(*(.mdebug.abi32))
    }

    SECTION_PROLOGUE(.gnu.attributes, 0,)
    {
        KEEP(*(.gnu.attributes))
    }

    /DISCARD/ : {
        *(.MIPS.abiflags)
        *(.pdr)
        *(.reginfo)
    }
}
