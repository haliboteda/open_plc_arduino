"""Every board-package test that needs no board. Run it after every change.

    python tests/selfcheck.py          P3, P19, P4, P15, T2-21, T3-07, T3-08, T3-09
    python tests/selfcheck.py --full   the same, plus P5 (about 45 minutes)

Needs: arduino-cli (PATH or ARDUINO_CLI, plus ARDUINO_CLI_CONFIG for the IDE's
bundled one), the board package installed in the IDE, cmake and a host C
compiler for T2-21, T3-07, T3-08 and T3-09. A missing tool is reported as SKIP by name, never as PASS.
Paths and variables: tests/_common.py.

Exit 0 = nothing failed, 1 = something failed.
"""

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path

from _common import TESTS, Section, arduino_cli, core_live

STEPS = [
    ("P3", "the installed board package matches this repo", ["check_core_sync.py"], False),
    ("P19", "nothing in the board package writes flash sector 15", ["check_no_sector15_writes.py"], False),
    ("P4", "variant header assertions (FMC reserved pins, UART routing)", ["variant_check/build.py"], True),
    ("P15", "the application's start address stays 1024-aligned", ["vector_alignment/build.py"], True),
    ("P5", "every example sketch builds", ["examples_build/build.py"], True),
]

results = []


def run_py(step, what, script, needs_cli):
    Section("%s  %s" % (step, what))
    if needs_cli and (arduino_cli() is None or core_live() is None):
        results.append((step, what, "SKIP", "arduino-cli or the installed board package not found"))
        return
    rc = subprocess.call([sys.executable, str(TESTS / script[0])] + script[1:], cwd=str(TESTS))
    results.append((step, what, "PASS" if rc == 0 else "FAIL (exit %d)" % rc, ""))


CTESTS = [
    ("T2-21", "the root in force cannot revoke itself (real owner_root_ro.c, CTest)", "^T2-21$"),
    ("T3-07", "AI / AO in mV and mA apply the calibration, or fall back (CTest)", "^T3-07[.]"),
    ("T3-08", "the reset cause the bootloader publishes reaches the sketch (CTest)", "^T3-08[.]"),
    ("T3-09", "KNX TP1 data link: codec, acknowledge, repeats, collision (CTest)", "^T3-09[.]"),
]


def run_ctest():
    cmake = os.environ.get("CMAKE") or shutil.which("cmake")
    if not cmake:
        for step, what, _ in CTESTS:
            results.append((step, what, "SKIP", "cmake not found (PATH or CMAKE)"))
        return
    ctest = str(Path(cmake).with_name("ctest" + Path(cmake).suffix))
    # A gitignored CMakeUserPresets.json names this machine's compiler.
    preset = "local" if (TESTS / "CMakeUserPresets.json").exists() else "host"
    Section("build the host tests")
    for argv in ([cmake, "--preset", preset], [cmake, "--build", str(TESTS / "build")]):
        rc = subprocess.call(argv, cwd=str(TESTS))
        if rc != 0:
            for step, what, _ in CTESTS:
                results.append((step, what, "FAIL (exit %d)" % rc, Path(argv[0]).stem))
            return
    for step, what, regex in CTESTS:
        Section("%s  %s" % (step, what))
        rc = subprocess.call([ctest, "--test-dir", str(TESTS / "build"), "--output-on-failure", "-R", regex],
                             cwd=str(TESTS))
        results.append((step, what, "PASS" if rc == 0 else "FAIL (exit %d)" % rc, ""))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--full", action="store_true", help="also run P5 (about 45 minutes)")
    args = ap.parse_args()

    for step, what, script, needs_cli in STEPS:
        if step == "P5" and not args.full:
            results.append((step, what, "SKIP", "--full only"))
            continue
        run_py(step, what, script, needs_cli)
    run_ctest()

    Section("summary")
    for step, what, state, note in results:
        print("%-6s %-62s %s%s" % (step, what, state, ("  (%s)" % note) if note else ""))
    failed = [r for r in results if r[2].startswith("FAIL")]
    print("\n%s" % ("%d failed" % len(failed) if failed else "nothing failed"))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
