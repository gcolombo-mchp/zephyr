#!/usr/bin/env python3
# Copyright (c) 2026 Microchip Technology Inc.
# SPDX-License-Identifier: Apache-2.0
#
# Converts MIPS KSEG0/KSEG1 virtual addresses in Intel HEX to physical addresses.
# KSEG0 (0x80000000-0x9FFFFFFF) and KSEG1 (0xA0000000-0xBFFFFFFF) both map to
# physical 0x00000000-0x1FFFFFFF by stripping the upper 3 bits.
#
# Usage: python kseg_to_phys.py input.hex output.hex

import sys


def convert_hex_line(line):
    """Convert Extended Linear Address records from virtual to physical."""
    if not line.startswith(':') or len(line) < 11:
        return line

    record_type = int(line[7:9], 16)

    # Only process Extended Linear Address records (type 04)
    if record_type == 4:
        byte_count = int(line[1:3], 16)
        if byte_count == 2:
            upper_addr = int(line[9:13], 16)
            # KSEG0: 0x8000-0x9FFF -> 0x0000-0x1FFF
            # KSEG1: 0xA000-0xBFFF -> 0x0000-0x1FFF
            if upper_addr >= 0x8000 and upper_addr <= 0x9FFF:
                upper_addr &= 0x1FFF
            elif upper_addr >= 0xA000 and upper_addr <= 0xBFFF:
                upper_addr &= 0x1FFF

            # Rebuild the record with new address
            data = f'{byte_count:02X}0000{record_type:02X}{upper_addr:04X}'
            checksum = 0
            for i in range(0, len(data), 2):
                checksum += int(data[i:i+2], 16)
            checksum = (~checksum + 1) & 0xFF
            return f':{data}{checksum:02X}\n'

    # Type 05 (Start Linear Address) - convert entry point
    if record_type == 5:
        byte_count = int(line[1:3], 16)
        if byte_count == 4:
            entry_addr = int(line[9:17], 16)
            if entry_addr >= 0x80000000 and entry_addr <= 0x9FFFFFFF:
                entry_addr &= 0x1FFFFFFF
            elif entry_addr >= 0xA0000000 and entry_addr <= 0xBFFFFFFF:
                entry_addr &= 0x1FFFFFFF

            data = f'{byte_count:02X}0000{record_type:02X}{entry_addr:08X}'
            checksum = 0
            for i in range(0, len(data), 2):
                checksum += int(data[i:i+2], 16)
            checksum = (~checksum + 1) & 0xFF
            return f':{data}{checksum:02X}\n'

    return line


def main():
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} input.hex output.hex")
        sys.exit(1)

    input_file = sys.argv[1]
    output_file = sys.argv[2]

    with open(input_file, 'r') as f:
        lines = f.readlines()

    converted = []
    for line in lines:
        converted.append(convert_hex_line(line.strip() + '\n'))

    with open(output_file, 'w') as f:
        f.writelines(converted)


if __name__ == '__main__':
    main()
