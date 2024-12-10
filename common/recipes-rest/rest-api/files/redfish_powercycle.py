import asyncio
import os
import time
from functools import lru_cache
from shutil import which
from typing import List, Optional, TYPE_CHECKING

import common_utils
import rest_pal_legacy
from aiohttp import web
from redfish_base import RedfishError

try:
    import pyrmd
except ImportError:
    pyrmd = None

POWER_UTIL_PATH = "/usr/local/bin/power-util"
WEDGE_POWER_PATH = "/usr/local/bin/wedge_power.sh"

# command: graceful-shutdown, off, on, reset, cycle, 12V-on, 12V-off, 12V-cycle
# Map each Redfish command with the relevant power_util command
# and expected status
POWER_UTIL_MAP = {
    "On": "on",
    "ForceOff": "12V-off",
    "GracefulShutdown": "graceful-shutdown",
    "GracefulRestart": "cycle",
    "ForceRestart": "12V-cycle",
    "ForceOn": "12V-on",
    "PushPowerButton": "reset",
}

# Commands:
#   status: Get the current power status

#   on: Power on microserver and main power if not powered on already
#     options:
#       -f: Re-do power on sequence no matter if microserver has
#           been powered on or not.

#   off: Power off microserver and main power ungracefully

#   reset: Power reset microserver ungracefully
#     options:
#       -s: Power reset whole wedge system ungracefully

WEDGE_POWER_MAP = {
    "On": "on",
    "ForceOff": "off",
    "GracefulShutdown": "off",  # ungraceful shutdown
    "GracefulRestart": "on -f",  # ungraceful
    "ForceRestart": "on -f",
    "ForceOn": "on",
    "PushPowerButton": "reset",
}


async def powercycle_post_handler(request: web.Request) -> web.Response:
    data = await request.json()
    fru_name = request.match_info["fru_name"]
    if rest_pal_legacy.pal_get_num_slots() > 1:
        server_name = fru_name.replace("server", "slot")
    allowed_resettypes = [
        "On",
        "ForceOff",
        "GracefulShutdown",
        "GracefulRestart",
        "ForceRestart",
        "ForceOn",
        "PushPowerButton",
    ]
    validation_error = await _validate_payload(request, allowed_resettypes)
    if validation_error:
        return RedfishError(
            status=400,
            message=validation_error,
        ).web_response()
    else:
        if is_power_util_available():  # then its compute
            reset_type = POWER_UTIL_MAP[data["ResetType"]]
            if fru_name is not None:
                cmd = [POWER_UTIL_PATH, server_name, reset_type]
                ret_code, resp, err = await common_utils.async_exec(cmd)
            else:
                raise NotImplementedError("Invalid URL")
        elif is_wedge_power_available():  # then its fboss
            reset_type = WEDGE_POWER_MAP[data["ResetType"]]
            cmd = [WEDGE_POWER_PATH, reset_type]
            ret_code, resp, err = await common_utils.async_exec(cmd)
        else:
            raise NotImplementedError("Util files not found")

    if ret_code == 0:
        return web.json_response(
            {"status": "OK", "Response": resp},
            status=200,
        )
    else:
        return RedfishError(
            status=500,
            message="Error changing {} system power state: {}".format(fru_name, err),
        ).web_response()


async def oobcycle_post_handler(request: web.Request) -> web.Response:
    allowed_resettypes = ["ForceRestart", "GracefulRestart"]
    validation_error = await _validate_payload(request, allowed_resettypes)
    if validation_error:
        return RedfishError(
            status=400,
            message=validation_error,
        ).web_response()
    else:
        if not TYPE_CHECKING:
            os.spawnvpe(
                os.P_NOWAIT, "sh", ["sh", "-c", "sleep 4 && reboot"], os.environ
            )
        return web.json_response(
            {"status": "OK"},
            status=200,
        )


async def _validate_payload(
    request: web.Request, allowed_types: List[str]
) -> Optional[str]:

    fru_name = request.match_info.get("fru_name")
    allowed_frus = ["1", "server1", "server2", "server3", "server4"]
    if fru_name and fru_name not in allowed_frus:
        return "fru_name:{} is not in the allowed fru list:{}".format(
            fru_name, allowed_frus
        )
    payload = await request.json()
    if "ResetType" not in payload:
        return "ResetType Not Present in Payload"
    if payload["ResetType"] not in allowed_types:
        return "Submitted ResetType:{} is invalid. Allowed ResetTypes:{}".format(
            payload["ResetType"], allowed_types
        )
    return None


@lru_cache()
def is_power_util_available() -> bool:
    return which(POWER_UTIL_PATH) is not None


@lru_cache()
def is_wedge_power_available() -> bool:
    return which(WEDGE_POWER_PATH) is not None


##########################################################################
# Shutdown due to liquid cooling leak
##########################################################################


class Regs:
    def __init__(self, shutdown_time, clock):
        self.shutdown_time = shutdown_time
        self.clock = clock


TYPE_TO_REGS = {
    "ORV2_PSU": Regs(shutdown_time=0x6D, clock=0x12A),  # For testing
    "ORV3_HPR_PSU": Regs(shutdown_time=0x6D, clock=0x5A),
    "ORV3_HPR_BBU": Regs(shutdown_time=0xED, clock=0x65),
}


async def liquid_leak_shutdown_post_handler(request: web.Request) -> web.Response:
    if pyrmd is None:
        return RedfishError(
            status=500,
            message="pyrmd is not available, can't shutdown",
        ).web_response()

    allowed_resettypes = ["ForceOff"]
    validation_error = await _validate_payload(request, allowed_resettypes)
    if validation_error:
        return RedfishError(
            status=400,
            message=validation_error,
        ).web_response()

    # Get list of devices
    rmd = pyrmd.RackmonAsyncInterface()
    all_psus = await rmd.list()
    devices = {
        psu["devAddress"]: TYPE_TO_REGS[psu["deviceType"]]
        for psu in all_psus
        if psu["deviceType"] in TYPE_TO_REGS
    }
    print("Got devices:", list(devices.keys()))

    # Synchronize clock
    now = int(time.time())
    commands = [
        rmd.write(dev, regs.clock, [now >> 16, now & 0xFFFF])
        for (dev, regs) in devices.items()
    ]
    results = await asyncio.gather(*commands, return_exceptions=True)
    for i, r in enumerate(results):
        if isinstance(r, Exception):
            print(
                "Failed setting clock on device {}, continuing anyway: {!r}".format(
                    list(devices.keys())[i], r
                )
            )

    # Read model name, just for testing
    commands = [rmd.read(dev, 0x0, 8) for dev in devices.keys()]
    results = await asyncio.gather(*commands, return_exceptions=True)

    def tostr(r):
        # r is a list of 2-byte ints. Split them into 1-byte ints adn convert to chars
        return "".join(
            chr(byte) for word in r for byte in (word >> 8, word & 0xFF)
        ).rstrip("\x00")

    results = [r if isinstance(r, Exception) else tostr(r) for r in results]
    print(results)

    # Send shutdown command
    if True:
        shutdown_at = int(time.time()) + 20
        commands = [
            rmd.write(
                dev, regs.shutdown_time, [shutdown_at >> 16, shutdown_at & 0xFFFF]
            )
            for (dev, regs) in devices.items()
        ]
        results = await asyncio.gather(*commands, return_exceptions=True)
        bad = [
            (d, r)
            for (d, r) in zip(devices.keys(), results)
            if isinstance(r, Exception)
        ]
        if bad:
            print(
                "Failed to reset devices: {}".format(
                    ", ".join("{}: {!r}".format(d, r) for (d, r) in bad)
                )
            )
            return RedfishError(
                status=500,
                message="Failed to reset PSU/BBU addrs: {}".format(
                    ", ".join(str(d) for (d, r) in bad)
                ),
            ).web_response()
    return web.json_response(
        {"status": "OK"},
        status=200,
    )
