"""Shared helpers for the board package's own tests. Standard library only.

No machine config file: every machine-local path comes from an environment
variable or the platform's default location (decision 78; see
$PROD/maps/test-architecture/issues/TA-07-do-component-repos-need-machine-config.md).

    ARDUINO_CLI         arduino-cli to use; default: `arduino-cli` on PATH
    ARDUINO_CLI_CONFIG  its --config-file; default: none (arduino-cli's own)
    ARDUINO15           Arduino's data directory; default: the platform's
    CORE_LIVE           the installed board package; default: the newest
                        $ARDUINO15/packages/OpenPLC_Alpha/hardware/stm32/<version>
"""

import os
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

TESTS = Path(__file__).resolve().parent
REPO = TESTS.parent

# Kept in step with $PROD/docs/build/BUILD-AND-TEST.md: pnum selects the
# variant header, so a wrong FQBN would compile a different variant.
FQBN = ("OpenPLC_Alpha:stm32:OPEN-PLC:pnum=PLC_H743,usb=CDCgen,xusb=FS,"
        "upload_method=cdcMethod,knxrole=dual_device")


def Section(text):
    print("\n===== %s" % text, flush=True)


def Ok(text):
    print(text, flush=True)


def Warn(text):
    print("WARN: %s" % text, flush=True)


def Fail(text):
    print("FAIL: %s" % text, flush=True)


def arduino15():
    named = os.environ.get("ARDUINO15")
    if named:
        return Path(named)
    if sys.platform == "win32":
        return Path(os.environ.get("LOCALAPPDATA", "")) / "Arduino15"
    if sys.platform == "darwin":
        return Path.home() / "Library" / "Arduino15"
    return Path.home() / ".arduino15"


def _version_key(name):
    return [int(p) if p.isdigit() else p for p in re.split(r"[.\-]", name)]


def core_live():
    """The installed board package, or None."""
    named = os.environ.get("CORE_LIVE")
    if named:
        return Path(named) if Path(named).is_dir() else None
    root = arduino15() / "packages" / "OpenPLC_Alpha" / "hardware" / "stm32"
    if not root.is_dir():
        return None
    versions = sorted((d for d in root.iterdir() if d.is_dir()), key=lambda d: _version_key(d.name))
    return versions[-1] if versions else None


def arduino_cli():
    """[cli, --config-file, cfg] ready to extend, or None when there is no cli."""
    cli = os.environ.get("ARDUINO_CLI") or shutil.which("arduino-cli")
    if not cli or not Path(cli).exists():
        return None
    argv = [cli]
    config = os.environ.get("ARDUINO_CLI_CONFIG")
    if config:
        argv += ["--config-file", config]
    return argv


def compile_argv(cli, *rest):
    """`arduino-cli compile` with the config flag after the subcommand."""
    return [cli[0], "compile"] + cli[1:] + [str(a) for a in rest]


def scratch(name):
    return Path(tempfile.gettempdir()) / ("core_tests_" + name)


def run_capture(argv):
    """(merged output, exit code)."""
    proc = subprocess.run([str(a) for a in argv], stdout=subprocess.PIPE,
                          stderr=subprocess.STDOUT, text=True, errors="replace")
    return proc.stdout or "", proc.returncode


def need_cli_and_core():
    """(cli argv, core path), or exits 2 naming what is missing."""
    cli = arduino_cli()
    if cli is None:
        Fail("arduino-cli not found: put it on PATH or set ARDUINO_CLI")
        sys.exit(2)
    core = core_live()
    if core is None:
        Fail("board package not installed under %s: install it in the IDE or set CORE_LIVE" % arduino15())
        sys.exit(2)
    return cli, core
