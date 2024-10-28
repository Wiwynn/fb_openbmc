from fixmybmc.bmccheck import bmcCheck
from fixmybmc.remediation import remediation
from fixmybmc.status import Error, Problem
from fixmybmc.utils import run_cmd


@bmcCheck
def eth0_up():
    """
    Check if eth0 is up
    """
    check_cmd = "ip link show eth0"

    status = run_cmd(check_cmd.split(" "))
    if status.returncode != 0:
        return Error(
            description="Unable to determine eth0 state",
            cmd_status=status,
        )
    if "state UP" in status.stdout:
        return None
    return Problem(
        description="eth0 is not up.",
        remediation=restart_eth0,
    )


@remediation
def restart_eth0():
    """
    Restart eth0 interface via `ip link set eth0 down && ip link set eth0 up`
    """
    run_cmd("ip link set eth0 down".split(" "), capture_output=False)
    run_cmd("ip link set eth0 up".split(" "), capture_output=False)
