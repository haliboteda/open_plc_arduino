"""The board package the IDE loads ($CORE_LIVE) matches this repo. Case P3.

The direction is one-way: edit and verify in $CORE_LIVE, then copy into this
repo and commit. $CORE_LIVE is not under version control, so anything verified
there and not copied across dies with the next IDE reinstall.

Exit 0 = identical, 1 = differences, 2 = $CORE_LIVE not found.
"""

import hashlib
import os
import re
import sys

from _common import REPO, Fail, Ok, Section, Warn, core_live

# Deliberate exclusions -- this is the sole source for why each is skipped:
#   installed.json        the IDE's install metadata, not source
#   tools/discovery/bin/  Go build output; the sources next to it are enough
#   *~                    editor backups
#   .claude/ .vscode/     per-machine editor and agent files, gitignored here
#   .gitignore .gitattributes CLAUDE.md tests/
#                         repo-only: they never ship inside the board package
# re.I so a case-only rename does not turn the check red.
SKIP = re.compile(
    r'^(installed\.json|\.gitignore$|\.gitattributes$|CLAUDE\.md$|tests[\\/]|\.claude[\\/]|\.vscode[\\/]'
    r'|tools[\\/]discovery[\\/]bin[\\/])|~$', re.I)


def walk_files(root):
    """Files under root, deterministic order, .git pruned."""
    for dirpath, dirnames, filenames in os.walk(root):
        dirnames[:] = sorted((d for d in dirnames if d != ".git"), key=str.lower)
        for name in sorted(filenames, key=str.lower):
            yield os.path.join(dirpath, name)


def sha256(path):
    # CRLF == LF: a web install is LF, an autocrlf checkout is CRLF.
    with open(path, "rb") as fh:
        return hashlib.sha256(fh.read().replace(b"\r\n", b"\n")).hexdigest()


def main():
    live = core_live()
    if live is None:
        Fail("board package not installed; set CORE_LIVE")
        return 2
    live_root = str(live.resolve()).rstrip("\\/")
    repo_root = str(REPO.resolve()).rstrip("\\/")

    Section("P3  core: live vs repo")
    print("  live  %s" % live_root)
    print("  repo  %s" % repo_root)

    only_live, diff, only_repo, live_rel = [], [], [], set()
    for full in walk_files(live_root):
        rel = full[len(live_root) + 1:]
        if SKIP.search(rel):
            continue
        live_rel.add(rel)
        other = os.path.join(repo_root, rel)
        if not os.path.exists(other):
            only_live.append(rel)
        elif sha256(full) != sha256(other):
            diff.append(rel)
    # A file deleted in live but still committed would come back on the next build.
    for full in walk_files(repo_root):
        rel = full[len(repo_root) + 1:]
        if not SKIP.search(rel) and rel not in live_rel:
            only_repo.append(rel)

    for f in only_live:
        Fail("ONLY-LIVE  %s" % f)
    for f in diff:
        Fail("DIFF       %s" % f)
    for f in only_repo:
        Warn("ONLY-REPO  %s" % f)

    n = len(only_live) + len(diff)
    if n:
        Fail("%d file(s) verified in live but not in the repo -- copy them across and commit" % n)
        return 1
    if only_repo:
        Warn("live matches the repo, but %d file(s) exist only in the repo" % len(only_repo))
        return 0
    Ok("live and repo are identical")
    return 0


if __name__ == "__main__":
    sys.exit(main())
