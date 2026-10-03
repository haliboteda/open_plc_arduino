#!/bin/sh
# platform.txt can only pick a binary per-OS (.windows/.linux/.macosx), not
# per-arch, so on macOS we dispatch to the right arch-specific binary here.
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ARCH=$(uname -m)
case "$ARCH" in
  arm64) exec "$DIR/bin/darwin_arm64/network_discovery" "$@" ;;
  *)     exec "$DIR/bin/darwin_amd64/network_discovery" "$@" ;;
esac
