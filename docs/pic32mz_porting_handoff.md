# PIC32MZ2048EFH144 Zephyr Porting - Handoff Document

## Overview

This document describes the current state of porting Zephyr RTOS to the PIC32MZ2048EFH144 (MIPS microAptiv core) on the DM320007 Starter Kit board. The work uses XC32 v5.10 as the compiler (instead of the Zephyr SDK MIPS GCC) because the Zephyr SDK compiler produced code that wouldn't execute on the hardware.

## What Works

- **XC32 toolchain integrated into Zephyr's west build system**
- **Bare-metal LED blink + UART2 output** — compiles, programs, runs on hardware
- **`west pic32m-build -b pic32mz_ef_sk <sample>`** — custom west extension that injects XC32 cmake parameters
- **`west flash`** — programs via ICD4 using MPLAB IPE runner
- **`--wrap=main` mechanism** — XC32's crt0 calls `__wrap_main()` which can redirect to Zephyr's `z_prep_c()`
- **GPIO driver** created (`drivers/gpio/gpio_pic32mz.c`)
- **EVIC interrupt controller driver** created (`drivers/interrupt_controller/intc_pic32mz_evic.c`)
- **Clock control driver** created (`drivers/clock_control/clock_control_mchp_pic32mz.c`)
- **Configuration bits** in board-specific file (`boards/microchip/pic32m/pic32mz_ef_sk/board_config.c`)

## Current Blocker

**The Zephyr kernel cannot start because adding Zephyr-specific linker sections to the XC32 linker script breaks the boot process.**

### The Problem in Detail

Zephyr's kernel requires special linker sections for:
- Init levels (`.z_init_EARLY_*`, `.z_init_PRE_KERNEL_1_*`, etc.) — device driver initialization ordering
- Device structures (`._device*`) — registered device instances
- Static thread data (`.static_thread_data*`) — statically defined threads
- SW ISR table (`._sw_isr_table*`) — interrupt dispatch table
- ISR metadata (`.intList`) — used by `gen_isr_tables.py` during build

XC32's crt0 startup performs data initialization (`dinit`) that scans sections in `kseg0_program_mem`. When Zephyr sections are present, the dinit mechanism either:
- Tries to copy/initialize them incorrectly, corrupting memory
- Interprets section content as dinit records, causing infinite loops or invalid memory writes

This happens **before** `_main_entry()` is called, so the CPU never reaches our `__wrap_main()`.

### What Was Tried

1. **Separate `SECTIONS {}` block after `INCLUDE "p32MZ2048EFH144.ld"`** — dinit processes the new sections and crashes
2. **Second `MEMORY {}` block for IDT_LIST region** — confuses `xc32-bin2hex` which stops emitting boot code
3. **Sections inside the main SECTIONS block of a copied XC32 .ld** — still crashes, dinit picks them up
4. **`(NOLOAD)` attribute on RAM sections** — doesn't help for the flash sections
5. **Explicit `>kseg0_program_mem` region assignment** — dinit still finds them

### Confirmed Working Configuration

When `linker_xc32.ld` contains ONLY:
```
INCLUDE "p32MZ2048EFH144.ld"
```
...the boot works perfectly and `__wrap_main()` is reached.

## Architecture

### Boot Flow (current, working for bare-metal)
```
XC32 crt0 (_reset @ 0xBFC00000)
  → cache init, .data copy, BSS clear, TLB init
  → _main_entry()
    → __wrap_main()  [our pic32m_boot.c, via --wrap=main]
      → boot_debug_led()  [LED blinks confirming boot]
      → z_prep_c()  [NOT YET WORKING - needs linker sections]
        → z_cstart() → kernel → main thread → __real_main()
```

### Key Files

| File | Purpose |
|------|---------|
| `soc/microchip/pic32m/pic32mz_ef/common/pic32m_boot.c` | `__wrap_main()` bootstrap |
| `soc/microchip/pic32m/pic32mz_ef/common/linker_xc32.ld` | Linker script (currently copied from DFP with Zephyr sections that break boot) |
| `soc/microchip/pic32m/pic32mz_ef/common/CMakeLists.txt` | SoC build config (conditionally includes XC32 vs Zephyr SDK) |
| `boards/microchip/pic32m/pic32mz_ef_sk/board_config.c` | `#pragma config` for all 4 DEVCFG words |
| `boards/microchip/pic32m/pic32mz_ef_sk/board.cmake` | Flash runner (MPLAB IPE + ICD4) |
| `cmake/toolchain/xc32/xc32_toolchain_file.cmake` | CMake toolchain file (compiler identification bypass) |
| `cmake/toolchain/xc32/generic.cmake` | Toolchain discovery |
| `cmake/compiler/xc32/target_mips.cmake` | Linker flags + DFP path |
| `cmake/compiler/xc32/compiler_flags.cmake` | Override `-nostdinc` |
| `scripts/west_commands/pic32m_build.py` | `west pic32m-build` extension |
| `drivers/gpio/gpio_pic32mz.c` | GPIO driver |
| `drivers/interrupt_controller/intc_pic32mz_evic.c` | EVIC driver |
| `drivers/clock_control/clock_control_mchp_pic32mz.c` | Clock driver |
| `samples/boards/microchip/pic32mz_blinky/` | Bare-metal test sample |

### Build Commands
```bash
# Build (uses XC32 automatically)
west pic32m-build -b pic32mz_ef_sk zephyr/samples/boards/microchip/pic32mz_blinky

# Flash
west flash
```

### Hardware
- **Board**: PIC32MZ EF Starter Kit (DM320007)
- **MCU**: PIC32MZ2048EFH144 (MIPS microAptiv, 200MHz, 2MB Flash, 512KB RAM)
- **Oscillator**: 24 MHz EC (external clock)
- **Programmer**: MPLAB ICD4 connected to RJ-11 debug connector
- **LED1**: Port H pin 0 (RH0), active low
- **UART2**: TX=RPB14, RX=RPG6, 115200 baud

### Toolchain
- **XC32 v5.10**: `C:\Program Files\Microchip\xc32\v5.10`
- **DFP**: `C:\Users\m91188\.mchp_packs\Microchip\PIC32MZ-EF_DFP\1.6.179`
- **MPLAB X**: v6.30 (for MPLAB IPE flash programmer)

## What Needs to Be Solved

The core problem: **how to add Zephyr kernel linker sections to the XC32 linker script without the dinit mechanism processing them**.

### Possible Solutions to Investigate

1. **Understand exactly how XC32 dinit decides which sections to process** — it likely uses ELF section flags (SHF_WRITE + SHF_ALLOC = data to initialize; SHF_ALLOC only = read-only, no init needed). The Zephyr sections may have SHF_WRITE set incorrectly.

2. **Use `__attribute__((section(".text.zephyr_init")))` in C source** — put init entries in `.text` which dinit never touches. This requires modifying how Zephyr generates init entries.

3. **Create a custom memory region** that dinit doesn't scan:
   ```
   MEMORY { zephyr_meta (r) : ORIGIN = 0x9D1F0000, LENGTH = 64K }
   ```
   Place Zephyr sections in this region. Dinit only processes `kseg0_program_mem` and `kseg0_data_mem`.

4. **Use the `--no-code-in-dinit` and `--no-dinit-in-serial-mem` linker flags** already present — check if additional flags exist to exclude specific sections.

5. **Place Zephyr sections in boot flash** (`kseg1_boot_mem_4B0` region, 0xBFC004B0+) which dinit might not scan.

6. **Mark sections as SHT_NOTE or SHT_PROGBITS without SHF_WRITE** using linker script tricks to prevent dinit from touching them.

7. **Check the XC32 linker map file** (`mem.map`) to see exactly what dinit includes — compare between working (no Zephyr sections) and broken (with sections) builds.

## Reference: Working Harmony Project

The project at `C:\gpio\` is a working Harmony GPIO example for the same board. It can be used as reference for:
- Correct DEVCFG values
- Linker script structure
- Boot code behavior
- CMake integration with XC32

## DFP Linker Script Location

The device-specific linker script is at:
```
C:\Users\m91188\.mchp_packs\Microchip\PIC32MZ-EF_DFP\1.6.179\xc32\32MZ2048EFH144\p32MZ2048EFH144.ld
```

It has 2017 lines with 3 `SECTIONS` blocks:
- Lines 268-677: Config word sections
- Lines 679-1981: Main code/data sections (this is where the problem occurs)
- Lines 1983-2017: Debug sections
