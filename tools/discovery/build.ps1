# Cross-compiles network_discovery for every supported platform.
# Pure Go stdlib, no cgo - this runs fine from Windows even though it
# produces Linux and macOS binaries.
$ErrorActionPreference = "Stop"
Set-Location $PSScriptRoot

$targets = @(
    @{ GOOS = "windows"; GOARCH = "amd64"; Out = "bin/windows_amd64/network_discovery.exe" },
    @{ GOOS = "linux";   GOARCH = "amd64"; Out = "bin/linux_amd64/network_discovery" },
    @{ GOOS = "darwin";  GOARCH = "amd64"; Out = "bin/darwin_amd64/network_discovery" },
    @{ GOOS = "darwin";  GOARCH = "arm64"; Out = "bin/darwin_arm64/network_discovery" }
)

foreach ($t in $targets) {
    $outDir = Split-Path $t.Out -Parent
    New-Item -ItemType Directory -Force -Path $outDir | Out-Null
    Write-Host "Building $($t.GOOS)/$($t.GOARCH) -> $($t.Out)"
    $env:GOOS = $t.GOOS
    $env:GOARCH = $t.GOARCH
    go build -o $t.Out .
    if ($LASTEXITCODE -ne 0) { throw "build failed for $($t.GOOS)/$($t.GOARCH)" }
}

Remove-Item Env:\GOOS -ErrorAction SilentlyContinue
Remove-Item Env:\GOARCH -ErrorAction SilentlyContinue
Write-Host "Done. Remember: bin/darwin_*/network_discovery and macos-launcher.sh need +x once copied to a real Mac/Linux filesystem (NTFS doesn't carry the exec bit)."
