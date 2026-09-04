"""Cross-compiles network_discovery for every supported platform.

    python build.py

Pure Go stdlib, no cgo, so this runs fine from Windows even though it produces
the Linux and macOS binaries too.

⚠️ bin/darwin_*/network_discovery and macos-launcher.sh need +x once they land
on a real Mac/Linux filesystem -- NTFS does not carry the exec bit.

Exit 0 = every target built.
"""

import os
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent

TARGETS = [
    ("windows", "amd64", "bin/windows_amd64/network_discovery.exe"),
    ("linux", "amd64", "bin/linux_amd64/network_discovery"),
    ("darwin", "amd64", "bin/darwin_amd64/network_discovery"),
    ("darwin", "arm64", "bin/darwin_arm64/network_discovery"),
]


def main():
    for goos, goarch, out in TARGETS:
        out_path = HERE / out
        out_path.parent.mkdir(parents=True, exist_ok=True)
        print("Building %s/%s -> %s" % (goos, goarch, out), flush=True)

        env = dict(os.environ, GOOS=goos, GOARCH=goarch)
        rc = subprocess.run(["go", "build", "-o", str(out_path), "."],
                            cwd=HERE, env=env).returncode
        if rc != 0:
            print("build failed for %s/%s" % (goos, goarch), file=sys.stderr)
            return rc

    print("Done.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
