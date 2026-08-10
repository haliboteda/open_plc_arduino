#!/usr/bin/env bash
# Cross-compiles network_discovery for every supported platform.
# Pure Go stdlib, no cgo - can be run from any host with a Go toolchain,
# regardless of which OS/arch you're actually building for.
set -euo pipefail
cd "$(dirname "$0")"

build() {
  local goos="$1" goarch="$2" out="$3"
  mkdir -p "$(dirname "$out")"
  echo "Building ${goos}/${goarch} -> ${out}"
  GOOS="$goos" GOARCH="$goarch" go build -o "$out" .
}

build windows amd64 bin/windows_amd64/network_discovery.exe
build linux   amd64 bin/linux_amd64/network_discovery
build darwin  amd64 bin/darwin_amd64/network_discovery
build darwin  arm64 bin/darwin_arm64/network_discovery

chmod +x bin/linux_amd64/network_discovery \
         bin/darwin_amd64/network_discovery \
         bin/darwin_arm64/network_discovery \
         macos-launcher.sh 2>/dev/null || true

echo "Done."
