# Copyright (c) 2026 Microchip Technology Inc.
# SPDX-License-Identifier: Apache-2.0

"""West command to reset target via PKOB (MCLR assert/release)."""

import argparse
import os
import subprocess
import sys
import tempfile
import time

from textwrap import dedent
from west.commands import WestCommand


class PkobReset(WestCommand):
    def __init__(self):
        super().__init__(
            'pkob-reset',
            'Reset target board via PKOB MCLR',
            dedent('''
                Resets the target by asserting MCLR via the PKOB on-board
                debugger, then releasing it. Equivalent to pressing a reset
                button on boards that don't have one.
            '''))

    def do_add_parser(self, parser_adder):
        parser = parser_adder.add_parser(
            self.name,
            help=self.help,
            description=self.description)
        parser.add_argument(
            '--tool', default='PKOB',
            help='Programming probe (default: PKOB)')
        parser.add_argument(
            '--part', default='32MZ2048EFH144',
            help='Target device (default: 32MZ2048EFH144)')
        parser.add_argument(
            '--ipecmd-path',
            default='C:/Program Files (x86)/Microchip/MPLABX/v5.35/mplab_platform/mplab_ipe/ipecmd.exe',
            help='Path to ipecmd executable')
        return parser

    def do_run(self, args, unknown_args):
        exe = args.ipecmd_path

        if not os.path.isfile(exe):
            self.die(f'ipecmd not found at: {exe}')

        # Connect and release from reset (asserts then de-asserts MCLR)
        cmd = [
            exe,
            f'-TP{args.tool}',
            f'-P{args.part}',
            '-OL',  # Release from reset
            '-OK',  # Silent mode
        ]

        self.inf(f'Resetting target via {args.tool}...')
        try:
            with tempfile.TemporaryDirectory() as tmpdir:
                result = subprocess.run(cmd, capture_output=True, text=True,
                                        timeout=60, cwd=tmpdir)
                if result.returncode == 0:
                    self.inf('Target reset complete.')
                else:
                    self.dbg('Simple reset failed, trying with connection test...')
                    cmd_retry = cmd + ['-Y']
                    result = subprocess.run(cmd_retry, capture_output=True, text=True,
                                            timeout=60, cwd=tmpdir)
                    if result.returncode == 0:
                        self.inf('Target reset complete.')
                    else:
                        self.die(f'Reset failed (status {result.returncode}): {result.stdout}')
        except subprocess.TimeoutExpired:
            self.die('Reset timed out (>60s)')
