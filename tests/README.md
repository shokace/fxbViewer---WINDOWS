# Verification

Run `./dev.sh test` from the repository root. Tests run offscreen by default and
use temporary directories/settings; they do not alter the user's library. The
reference corpus is checked in, so normal tests require no downloads or Python.

## Regression coverage

| Behavior / original defect | Evidence in `test_fxbviewer.cpp` |
| --- | --- |
| Binary files treated as text lines | `referenceCorpus`, `embeddedBinaryText`, `referenceLibraryQueries` |
| Lost path when the manual array exceeded 10 files | `recursiveDeduplicatedScan` indexes 26 valid files |
| Duplicate paths from repeated/overlapping folder imports | `recursiveDeduplicatedScan`, `symlinkAndEmptyAndMissingFolders`, `uiSearchClipboardRevealAndClear` |
| Duplicate or stale results across searches | `searchModelAndSorting`, `uiSearchClipboardRevealAndClear` |
| Broken Clear Search reset | `uiSearchClipboardRevealAndClear`, `uiClearAndCloseWhileScanning` |
| UI blocked while indexing | `uiResponsiveAndScalable` types a query while the worker is active |
| Missing cancellation / unsafe worker lifetime | `cancelBeforeAndDuringScan`, `uiPersistenceRefreshRemoveAndCancel`, `uiClearAndCloseWhileScanning` |
| Fixed positions clipped large text | `uiResponsiveAndScalable`, plus live desktop inspection at 200% scale |
| Unwired copy/open-folder actions | `uiSearchClipboardRevealAndClear` checks clipboard and intercepts the actual file URL |
| Folder selections lost on restart | `uiPersistenceRefreshRemoveAndCancel` constructs a second window from saved settings |
| Changed/deleted files left stale results | `uiPersistenceRefreshRemoveAndCancel` deletes a bank and refreshes |
| Malformed/unreadable files silently skipped | `boundedFileReading`, `recursiveDeduplicatedScan`, `uiDropAndErrorReporting` |
| Invalid lengths / counts / truncation | `allTruncationsRejected`, `corruptHeadersAndCounts`, `malformedMutationSmoke` |
| Plugin-specific header exception | `realWorldSizeException` uses the unmodified Podolski export |
| Excessive file / text / library sizes | `boundedFileReading`, `textLimitIsVisible`, `indexingLimitsAreReported` |
| False negative across a long text run | `longTextDoesNotSplitMatches` |
| Dragging multiple/duplicate folders | `uiDropAndErrorReporting` sends drag/drop events to the window |

`referenceCorpus` is data-driven over 12 pinned files. It checks each SHA-256,
format, plugin ID, expected program names, and one-based program positions. See
[fixture provenance](fixtures/README.md). Tests exercise real ReaComp, Massive,
and Podolski data in addition to independently constructed records.

## Local results (2026-11-13)

- Ubuntu x86-64, GCC 15, Qt 5.15.18: **33 passed, 0 failed, 0 skipped**.
- Same environment, Qt 6.10.2: **33 passed, 0 failed, 0 skipped**.
- Qt 6 build with `-fsanitize=address,undefined -fno-omit-frame-pointer`:
  **33 passed, 0 failed, 0 skipped**, with leak detection enabled and no sanitizer
  diagnostics. Both sanitizer runtimes were verified linked.
- Live Qt 6 application launched under GDB; its 200%-scale desktop capture was
  inspected for clipping. Large-font populated results were also captured from
  the UI test. Set `FXBVIEWER_TEST_SCREENSHOT=/tmp/fxbviewer.png` when running the
  suite to capture that populated test window.
- Source diff whitespace check, launcher shell syntax, fixture hashes, and
  workflow YAML checked.

The Windows MSVC job in `.github/workflows/build.yml` is configured but has not
been executed in this Linux session. No remote CI success or Windows runtime
verification is claimed. No real Sylenth-specific fixture is bundled; opaque
chunk searching is tested, but universal Sylenth preset decoding is not claimed.

## Reproduce sanitizer checks

Use a fresh build directory, your chosen Qt qmake executable, and its normal
library environment:

```sh
mkdir -p ../fxbViewer-sanitized
cd ../fxbViewer-sanitized
qmake6 ../fxbViewer---WINDOWS/tests/tests.pro CONFIG+=debug CONFIG-=release \
  'QMAKE_CXXFLAGS+=-fsanitize=address,undefined -fno-omit-frame-pointer' \
  QMAKE_LFLAGS+=-fsanitize=address,undefined
make -j4
QT_QPA_PLATFORM=offscreen ASAN_OPTIONS=detect_leaks=1 \
  UBSAN_OPTIONS=halt_on_error=1 ./fxbviewer_tests
```

On this workstation's unpacked user-local Qt installation, also supply
`QMAKE_LIBDIR+=<Qt library directory>` to qmake and that directory in
`LD_LIBRARY_PATH`, as `dev.sh` does for ordinary tests.
