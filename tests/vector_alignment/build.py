"""The application's start address must be a multiple of 1024. Case P15.

Evidence for R1-33; criterion in $PROD/docs/engineering/HOW-TO-RUN-TESTS.md.
The app's vector table sits at the start of FLASH, so VTOR's alignment lands on
LD_FLASH_OFFSET; an ASSERT in variants/STM32H7xx/H743/ldscript.ld refuses a bad
one. Two builds, and the second is the point:

  positive  the shipping offset                     -> must link
  negative  0x20200, deliberately not 1024-aligned  -> must NOT link, and the
            error must name the alignment

Without the negative build a deleted or always-true ASSERT stays green forever.

Exit 0 = the guard works, 1 = it does not, 2 = prerequisites missing.
"""

import re
import shutil
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent))

from _common import Fail, Ok, Section, compile_argv, need_cli_and_core, run_capture, scratch  # noqa: E402

# Any sketch will do: what is under test is the linker script.
SKETCH = HERE / "minimal"
FQBN = "OpenPLC_Alpha:stm32:OPEN-PLC:pnum=PLC_H743"
# Still 512-aligned: it breaks only the architectural rule, which the hardware
# would accept (TBLOFF is bit[31:7]) and nothing else would catch.
BAD_OFFSET = "0x20200"
# A fragment of the ASSERT text, so the negative build proves THIS guard fired.
EXPECTED_MESSAGE = "must be a multiple of 1024"


def compile_once(cli, offset, build_path):
    shutil.rmtree(str(build_path), ignore_errors=True)
    # --build-property REPLACES the flag list, so VECT_TAB_OFFSET is restated.
    flags = "-DVECT_TAB_OFFSET=" + (offset or "{build.flash_offset}")
    rest = ["--fqbn", FQBN,
            "--build-property", "compiler.c.extra_flags=" + flags,
            "--build-property", "compiler.cpp.extra_flags=" + flags]
    if offset:
        rest += ["--build-property", "build.flash_offset=" + offset]
    rest += ["--build-path", build_path, SKETCH]
    return run_capture(compile_argv(cli, *rest))


def main():
    cli, _ = need_cli_and_core()
    problems = 0

    Section("P15  positive -- the shipping offset must still link")
    out, rc = compile_once(cli, None, scratch("p15_ok"))
    if rc == 0:
        Ok("  links, as it must")
    else:
        Fail("  the shipping configuration does NOT link")
        for line in re.split(r"\r?\n", out):
            if re.search(r"error|assert", line, re.I):
                print("    %s" % line)
        problems += 1

    Section("P15  negative -- %s must be refused" % BAD_OFFSET)
    out, rc = compile_once(cli, BAD_OFFSET, scratch("p15_bad"))
    if rc == 0:
        Fail("  it LINKED: the ASSERT in ldscript.ld is gone or cannot fail")
        problems += 1
    elif EXPECTED_MESSAGE in out:
        Ok("  refused, and the error names the alignment")
    else:
        Fail("  the build failed, but not on the alignment ASSERT. Last lines:")
        for line in re.split(r"\r?\n", out)[-6:]:
            if line.strip():
                print("    %s" % line)
        problems += 1

    for name in ("p15_ok", "p15_bad"):
        shutil.rmtree(str(scratch(name)), ignore_errors=True)
    if problems:
        Fail("%d half(s) of the check did not hold" % problems)
        return 1
    Ok("the linker refuses an unaligned application start address")
    return 0


if __name__ == "__main__":
    sys.exit(main())
