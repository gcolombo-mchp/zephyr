/**********************************************************************************************
 * © 2026 Microchip Technology Inc. and its subsidiares. All rights reserved.
 * This software includes AI generated code created using significant prompting. This
 * software is provided AS IS; you are Responsible for reviewing, testing, and validating for
 * your application.
 **********************************************************************************************/
# Copyright (c) 2026 Microchip Technology Inc.
#
# SPDX-License-Identifier: Apache-2.0

"""West extension: pic32m-build

Wraps 'west build' to automatically inject XC32 toolchain parameters
for PIC32M (MIPS) boards.

Usage:
    west pic32m-build -b pic32mz_ef_sk path/to/sample
    west pic32m-build -b pic32mz_ef_sk path/to/sample -p auto
    west pic32m-build -b pic32mz_ef_sk path/to/sample -- -DEXTRA=value

All arguments are passed through to 'west build' with XC32 cmake variables
automatically appended.
"""

import os
import pathlib
import subprocess
import sys

from west.commands import WestCommand


class Pic32mBuild(WestCommand):

    def __init__(self):
        super().__init__(
            'pic32m-build',
            'build for PIC32M with XC32',
            'Wraps "west build" with XC32 toolchain for PIC32M boards.',
            accepts_unknown_args=True)

    def do_add_parser(self, parser_adder):
        parser = parser_adder.add_parser(
            self.name,
            help=self.help,
            description=self.description)
        return parser

    def do_run(self, args, unknown_args):
        # Locate ZEPHYR_BASE
        zephyr_base = os.environ.get('ZEPHYR_BASE')
        if not zephyr_base:
            from west.util import west_topdir
            topdir = west_topdir()
            zephyr_base = str(pathlib.Path(topdir) / 'zephyr')

        # Toolchain file path (forward slashes for CMake)
        toolchain_file = (pathlib.Path(zephyr_base) /
                          'cmake' / 'toolchain' / 'xc32' /
                          'xc32_toolchain_file.cmake').as_posix()

        # CMake variables to inject
        xc32_cmake_args = [
            '-DZEPHYR_TOOLCHAIN_VARIANT=xc32',
            f'-DCMAKE_TOOLCHAIN_FILE={toolchain_file}',
        ]

        # Build the command: west build <user_args> -- <xc32_args> [<user_cmake_args>]
        cmd = [sys.executable, '-m', 'west', 'build']

        if '--' in unknown_args:
            # User already has cmake args after '--'
            sep_idx = unknown_args.index('--')
            cmd += unknown_args[:sep_idx]
            cmd += ['--']
            cmd += xc32_cmake_args
            cmd += unknown_args[sep_idx + 1:]
        else:
            cmd += unknown_args
            cmd += ['--']
            cmd += xc32_cmake_args

        self.inf(f'pic32m-build: {" ".join(cmd[3:])}')

        result = subprocess.run(cmd)
        raise SystemExit(result.returncode)
