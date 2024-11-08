#!/usr/bin/env python3
#
# Copyright 2020-present Facebook. All Rights Reserved.
#
# This program file is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation; version 2 of the License.
#
# This program is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
# for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program in a file named COPYING; if not, write to the
# Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor,
# Boston, MA 02110-1301 USA
#

import json
import subprocess
import typing as t

import aiohttp.web

from rest_utils import DEFAULT_TIMEOUT_SEC


# Define a separate type for the dictionary returned by the functions
AuroraChipResponse = t.Dict[str, t.Any]


def execute_shell_command(command: t.List[str], check=True) -> str:
    try:
        proc = subprocess.run(
            command,
            check=check,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            encoding="utf-8",
            timeout=DEFAULT_TIMEOUT_SEC,
        )
        return proc.stdout

    except subprocess.CalledProcessError as e:
        error_data = {
            "reason": repr(command) + " exited with non-zero",
            "stdout": str(e.stdout),
            "stderr": str(e.stderr),
            "returncode": e.returncode,
        }
        raise aiohttp.web.HTTPInternalServerError(
            body=json.dumps(error_data),
            content_type="application/json",
        ) from e


# Endpoint to program the Aurora chip
def program_aurora_chip() -> AuroraChipResponse:
    command = ["/usr/local/bin/improve_aura_pll.sh"]
    output = execute_shell_command(command)
    if output.strip().endswith("FIX_APPLIED") or output.strip().endswith(
        "AURA_FIX_SUCCESS"
    ):
        return {
            "Status": "improve_aura_pll.sh succeeded",
            "Output": output,
            "Actions": [],
            "Resources": [],
        }
    else:
        raise aiohttp.web.HTTPInternalServerError(
            body=json.dumps(
                {
                    "reason": "improve_aura_pll.sh did not report fix being "
                    "successfully applied",
                    "stdout": output,
                }
            ),
            content_type="application/json",
        )


# Endpoint to check the check of the Aurora chip
def check_aurora_chip_check() -> AuroraChipResponse:
    command = ["/usr/local/bin/improve_aura_pll.sh", "check"]
    # improve_aura_pll.sh check script can return non-zero on
    # FIX_NOT_APPLIED or UNSUPPORTED_HW_AURA_CRYSTAL_NOT_FOUND use cases
    output = execute_shell_command(command, check=False)
    return {
        "Status": "checked",
        "Output": output,
        "Actions": [],
        "Resources": [],
    }
