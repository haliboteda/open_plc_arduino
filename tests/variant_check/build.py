"""Compiles the variant assertion sketches next to this file. Case P4.

A failure here is a broken variant header, not a broken sketch. Compiles
against the installed board package ($CORE_LIVE): arduino-cli refuses this repo
as a sketchbook platform (see this repo's CLAUDE.md, tests section), and P3 holds
the two identical.

Exit 0 = every sketch compiled, 1 = at least one did not, 2 = prerequisites missing.
"""

import re
import shutil
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent))

from _common import FQBN, Fail, Ok, Section, compile_argv, need_cli_and_core, run_capture, scratch  # noqa: E402

# Any directory here is treated as a sketch, so skip the ones that are not.
SKIP = {"__pycache__", ".vscode", "build"}


def main():
    cli, _ = need_cli_and_core()
    failed = 0
    for sketch in sorted(p for p in HERE.iterdir() if p.is_dir() and p.name not in SKIP):
        Section("P4  compiling %s" % sketch.name)
        build_path = scratch("variant_" + sketch.name)
        out, rc = run_capture(compile_argv(cli, "--warnings", "all", "--fqbn", FQBN,
                                           "--build-path", build_path, sketch))
        if rc == 0:
            Ok("PASS - the variant's assertions hold")
        else:
            Fail("a static_assert fired, or the sketch does not compile:")
            # The static_assert message is the payload, so show error lines.
            for line in re.split(r"\r?\n", out):
                if re.search(r"error|static_assert|assertion", line, re.I):
                    print("    %s" % line)
            failed += 1
        shutil.rmtree(str(build_path), ignore_errors=True)

    if failed:
        Fail("%d sketch(es) failed" % failed)
        return 1
    Ok("all variant assertions hold")
    return 0


if __name__ == "__main__":
    sys.exit(main())
