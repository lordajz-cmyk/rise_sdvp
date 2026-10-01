#!/usr/bin/env python3
"""Configure MP101 CM5 peripherals and MCU boot pins; exit 10 until the next boot."""
import os
from pathlib import Path
import re
import shutil
import tempfile

BEGIN = "# BEGIN rise_sdvp MP101"
END = "# END rise_sdvp MP101"
BLOCK = f"""{BEGIN}
[cm5]
dtoverlay=dwc2,dr_mode=host
dtparam=pciex1=on
dtoverlay=uart2-pi5
dtoverlay=uart4-pi5
dtparam=cooling_fan=on
gpio=17=op,dh
gpio=16=op,dl
[all]
{END}
"""


def configure(config, state_dir, boot_id):
    original = config.read_text()
    unmanaged = re.sub(r"(?m)^" + re.escape(BEGIN) + r"\n.*?^" +
                       re.escape(END) + r"\n?", "", original, flags=re.S)
    managed_settings = {line for line in BLOCK.splitlines()
                        if line and not line.startswith(("#", "["))}
    preserved = []
    section = "all"
    for raw_line in unmanaged.splitlines(keepends=True):
        line = raw_line.split("#", 1)[0].strip()
        if line.startswith("["):
            section = line[1:-1]
        if section not in ("all", "cm5", "pi5"):
            preserved.append(raw_line)
            continue
        if (re.match(r"dtparam=.*\bspi=(on|true|1)\b", line) or
                re.match(r"dtoverlay=(spi0(?:[-,]|$)|uart3-pi5(?:,|$))", line)):
            raise ValueError(f"{config}: {line} uses the MP101 SWD GPIOs; disable it for CM5 first")
        # Move matching manual settings into our block, avoiding duplicate overlays.
        if line not in managed_settings:
            preserved.append(raw_line)
    wanted = "".join(preserved).rstrip() + "\n\n" + BLOCK
    state_dir.mkdir(parents=True, exist_ok=True)
    marker = state_dir / "mp101-reboot-required"
    changed = wanted != original
    if changed:
        backup = config.with_name(config.name + ".mp101.bak")
        if not backup.exists():
            shutil.copy2(config, backup)
        marker.write_text(boot_id + "\n")
        fd, temp = tempfile.mkstemp(prefix=".mp101-", dir=config.parent)
        try:
            with os.fdopen(fd, "w") as output:
                output.write(wanted)
                output.flush()
                os.fsync(output.fileno())
            os.chmod(temp, config.stat().st_mode & 0o777)
            os.replace(temp, config)
        finally:
            if os.path.exists(temp):
                os.unlink(temp)
    if marker.exists():
        if marker.read_text().strip() == boot_id:
            return True
        marker.unlink()
    return False


if __name__ == "__main__":
    try:
        pending = configure(Path("/boot/firmware/config.txt"),
                            Path("/var/lib/rise_sdvp"),
                            Path("/proc/sys/kernel/random/boot_id").read_text().strip())
        if pending:
            print("MP101 boot settings installed. Reboot, then rerun the same installation command.")
        raise SystemExit(10 if pending else 0)
    except (OSError, ValueError) as error:
        raise SystemExit(str(error))
