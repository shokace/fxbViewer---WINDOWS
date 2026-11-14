# Release procedure

Windows x64 is the distribution target. Qt 5 and Linux remain development/test
compatibility targets. Use MSVC 2022, Windows SDK and the exact Qt version in
`packaging/qt.json`. `VERSION` controls runtime and Windows resource versions.

## Gates

Every push and PR runs `.github/workflows/build.yml`:

1. Qt 5 and Qt 6 compile, run the fixture/UI regression suite and the executable
   smoke tour. Qt 6 also runs ASan/UBSan with leak detection.
2. MSVC builds and tests the release configuration on Windows.
3. Packaging uses `windeployqt`, restricts Qt dependencies to Qtbase, adds app-local
   VC runtime DLLs, verifies the Qt source hash, and includes all source/notices.
   ZIP and symbol archives get SHA-256 checksums and a per-file inventory.
4. A separate Windows runner without the Qt SDK extracts the ZIP into a path with
   spaces and accented characters. The app runs with development paths removed.
   The smoke test verifies real search counts and typo flags, four rendered
   windows, executable version, every packaged file hash, and that Qt/platform/
   CRT modules actually load from the package. An invalid output directory must
   fail promptly. Evidence is retained as `windows-smoke-evidence`.

A hosted Windows Server 2022 runner verifies packaging and native execution. It
is not evidence of exhaustive testing on every Windows 10/11 edition, GPU or
network filesystem. Before a broad public rollout, exercise your actual library
on your supported client machines. Windows ARM64 and 32-bit builds are not shipped.

## Build manually

In an x64 MSVC developer shell with the pinned Qt `bin` directory on PATH:

```powershell
mkdir build
cd build
qmake ../fxbViewer.pro CONFIG+=release CONFIG-=debug
nmake
cd ..
python scripts/prepare-qt-notices.py build/qt-notices --cache build/downloads
pwsh -File scripts/package-windows.ps1 -Executable build/release/fxbViewer.exe -OutputDirectory dist -QtSourceNotices build/qt-notices
pwsh -File scripts/smoke-windows.ps1 -Archive dist/FXB-Viewer-2.2.0-windows-x64.zip -EvidenceDirectory smoke-evidence
```

Use a fresh staging/evidence directory. Packaging refuses to overwrite an existing
package folder. Download the two ZIPs and `SHA256SUMS.txt` from the successful
`windows-x64-package` Actions artifact. Verify checksums after download.

## Draft release

Run the **Prepare release** workflow with the successful Build and test run ID.
It accepts only a successful run for the current `master` commit and checks the
workflow identity. It verifies asset hashes, then creates a version tag and a
**draft** GitHub Release containing the ZIP, symbols, and checksum file.
The original build run is linked in its notes. A reviewer can inspect and publish
the draft when distribution is desired. Never reuse a version tag for new bytes.

## Signing and servicing

Packages are currently unsigned and say so in their README and manifest. Publisher
signing requires a certificate owned by the publisher; no credential is embedded
or assumed. If adding Authenticode, sign the application before packaging, verify
with `signtool verify /pa`, record signing identity in the manifest, and perform
the same extraction smoke test on the final archive. Do not modify a ZIP after
checksums are generated. Signing can reduce unknown-publisher friction but does
not replace testing or guarantee SmartScreen reputation.

Qt and the app-local Microsoft runtime are serviced by shipping a new package.
Review upstream security/bug fixes regularly; update the Qt lockfile URL, hash,
and CI version together, and repeat all gates. Dependabot proposes updates to
pinned GitHub Actions. Qt source/notices must always match shipped Qt DLLs.

Install/update is ZIP extraction to a new folder; rollback is launching the prior
folder. No in-place migration or destructive uninstall is performed. Normal
instances share per-user settings (last writer wins); demo/smoke settings are
isolated and disposable. There is no telemetry or background network service.
