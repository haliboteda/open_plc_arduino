"""Compiles every example sketch in the board package that targets this board. Case P5.

    python build.py                 all of them (about 45 minutes)
    python build.py --only SDRAM    only libraries whose name matches

Examples are the first thing a new user compiles and the last thing anybody
re-checks; they rot silently when an API is renamed. Scope: the project's own
libraries and the upstream STM32duino ones (decision 68); upstream examples
never meant for an H743 are in EXCLUDED with their reason. Compiles the
installed board package ($CORE_LIVE), which P3 holds identical to this repo.

Exit 0 = every example compiled, 1 = at least one did not, 2 = prerequisites missing.
"""

import argparse
import re
import shutil
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent))

from _common import FQBN, Fail, Ok, Section, Warn, compile_argv, need_cli_and_core, run_capture, scratch  # noqa: E402

# Compiled first, so a break in the project's own code shows up early.
OWN_LIBRARIES = ("OpenPLC_Ports", "OpenPLC_SDRAM", "OpenPLC_IAP", "OpenPLC_Net", "OpenPLC_KNX")

# (library, example) -> why it cannot build for this board.
EXCLUDED = {
    ("Keyboard", "KeyboardMessage"): "needs USB HID; this board's USB menu offers CDC only",
    ("Mouse", "ButtonMouseControl"): "needs USB HID; this board's USB menu offers CDC only",
    ("SubGhz", "ReadRegister"): "needs the SubGhz radio, which only STM32WL parts have",
}

# (library, example) -> the KNX Role it is written for, when not the default.
# These refuse to compile for any other role.
KNX_ROLE = {
    ("OpenPLC_KNX", "KNX_Switch"): "tp_device",
    ("OpenPLC_KNX", "KNX_Inputs"): "tp_device",
}

SKIP_DIRS = {"__pycache__", ".vscode", "build"}


def walk_dirs(root):
    """Directories under root, pre-order, name-sorted at each level."""
    for child in sorted(p for p in root.iterdir() if p.is_dir() and p.name not in SKIP_DIRS):
        yield child
        yield from walk_dirs(child)


def fqbn_for(lib, name):
    role = KNX_ROLE.get((lib, name))
    return FQBN.replace("knxrole=dual_device", "knxrole=" + role) if role else FQBN


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--only", default="")
    ap.add_argument("--keep-build-dirs", action="store_true")
    args = ap.parse_args()

    cli, core = need_cli_and_core()
    libs_dir = core / "libraries"
    upstream = sorted(d.name for d in libs_dir.iterdir() if d.is_dir() and d.name not in OWN_LIBRARIES)

    sketches = []
    for lib in OWN_LIBRARIES + tuple(upstream):
        if args.only and args.only.lower() not in lib.lower():
            continue
        examples = libs_dir / lib / "examples"
        if not examples.is_dir():
            continue
        # An Arduino example is a directory holding a .ino of the same name.
        for d in walk_dirs(examples):
            if not (d / (d.name + ".ino")).exists():
                continue
            if (lib, d.name) in EXCLUDED:
                print("  skip %s / %s: %s" % (lib, d.name, EXCLUDED[(lib, d.name)]))
                continue
            sketches.append((lib, d.name, d))

    if not sketches:
        if args.only:
            Warn("no examples matched --only %s" % args.only)
            return 0
        Fail("no example sketches found under %s" % libs_dir)
        return 2

    failed = 0
    for lib, name, path in sketches:
        Section("P5  %s / %s" % (lib, name))
        build_path = scratch("ex_%s_%s" % (lib, name))
        out, rc = run_capture(compile_argv(cli, "--warnings", "all", "--fqbn", fqbn_for(lib, name),
                                           "--build-path", build_path, path))
        lines = [ln for ln in re.split(r"\r?\n", out)]
        if rc == 0:
            size = re.search(r"Sketch uses \d+ bytes", out)
            Ok("PASS - " + size.group(0) if size else "PASS")
        else:
            Fail(name)
            # Errors only: upstream -Wunknown-pragmas noise would bury the cause.
            errors = [ln for ln in lines if re.search(r"\berror\b|Error during build", ln, re.I)]
            for ln in (errors[:12] or [ln for ln in lines if ln][-10:]):
                print("    %s" % ln)
            failed += 1
        if not args.keep_build_dirs:
            shutil.rmtree(str(build_path), ignore_errors=True)

    print("  %d example(s) compiled, %d failed" % (len(sketches), failed))
    if failed:
        Fail("%d example(s) do not build" % failed)
        return 1
    Ok("every example builds")
    return 0


if __name__ == "__main__":
    sys.exit(main())
