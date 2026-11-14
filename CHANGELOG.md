# Changes

## 2.2.0

- Portable Windows x64 distribution with Qt/runtime DLLs, package inventory,
  SHA-256 checksums, separate MSVC symbols, and complete Qt source/license notices.
- Windows package validation on a separate runner without a Qt SDK: native window,
  real indexed searches, typo recovery, screenshots, and loaded-DLL origin checks.
- Interruptible file reading and parsing; reject non-regular files before opening;
  cap per-bank program counts and directory traversal/notice growth.
- Fix embedded-text previews after whitespace/accent normalization and phrases
  crossing lines. Reject oversized queries before normalization.
- Remember window layout and search preferences. Report settings write failures.
- Versioned Windows executable metadata, diagnostic information without paths,
  and an isolated `--smoke-test` command with timeout and useful failure codes.
- CI includes Qt 5/6 regression tests, deterministic malformed-file mutations,
  address/undefined-behavior sanitizers, and Windows deployment checks.

This release is unsigned. It reads/searches FXB/FXP files; it does not load plugins
or play audio. Plugin-private banks may only provide embedded text. No installer,
service, telemetry, or automatic updater is installed. See the packaged README
for supported systems, limits, update/rollback, and settings reset instructions.
