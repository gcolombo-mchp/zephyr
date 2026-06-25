# Copyright (c) 2026 Microchip Technology Inc.
# SPDX-License-Identifier: Apache-2.0

"""West command to reset target via PKOB (MCLR assert/release)."""

import os
import subprocess
import tempfile

from pathlib import Path
from textwrap import dedent
from west.commands import WestCommand

try:
    import yaml
except ImportError:
    yaml = None


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
            '-b', '--board', required=True,
            help='Board name (e.g. pic32mz_ef_sk)')
        return parser

    IPECMD_DEFAULT = ('C:/Program Files (x86)/Microchip/MPLABX/v5.35'
                       '/mplab_platform/mplab_ipe/ipecmd.exe')

    def _get_part_from_board(self, board_name):
        """Resolve board name to ipecmd part number via board.yml."""
        if yaml is None:
            self.die('PyYAML is required: pip install pyyaml')

        zephyr_base = Path(self.topdir) / 'zephyr'
        boards_dir = zephyr_base / 'boards'

        # Search for board.yml matching the board name
        for yml_path in boards_dir.rglob('board.yml'):
            with open(yml_path, 'r') as f:
                data = yaml.safe_load(f)
            if data and data.get('board', {}).get('name') == board_name:
                socs = data['board'].get('socs', [])
                if not socs:
                    self.die(f'No SoC defined in {yml_path}')
                soc_name = socs[0]['name']
                if not soc_name.startswith('pic'):
                    self.die(f'Board {board_name} SoC "{soc_name}" is not a PIC device')
                # pic32mz2048efh144 -> 32MZ2048EFH144
                return soc_name[3:].upper()

        self.die(f'Board "{board_name}" not found')

    def do_run(self, args, unknown_args):
        exe = self.IPECMD_DEFAULT
        if not os.path.isfile(exe):
            self.die(f'ipecmd not found at: {exe}')

        part = self._get_part_from_board(args.board)

        cmd = [exe, '-TPPKOB', f'-P{part}', '-OL', '-OK']

        self.inf(f'Resetting {args.board} (part: {part}) via PKOB...')
        try:
            with tempfile.TemporaryDirectory() as tmpdir:
                result = subprocess.run(cmd, capture_output=True, text=True,
                                        timeout=60, cwd=tmpdir)
                output = result.stdout + result.stderr
                if result.returncode != 0 or 'Failed' in output:
                    self.die(f'Reset failed: {output.strip()}')
                self.inf('Target reset complete.')
        except subprocess.TimeoutExpired:
            self.die('Reset timed out (>60s)')
