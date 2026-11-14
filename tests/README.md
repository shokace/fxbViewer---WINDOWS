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

## Search, cache, and demo coverage

The expanded suite also checks:

- Exact-name ranking above prefix/word matches and bank metadata; unordered terms;
  straight/typographic quoted phrases; accent folding; names-only scope.
- Missing/extra letters, substitutions, and transpositions; disabling typo recovery;
  no expansion when exact hits exist; no numeric-token guessing.
- Indexed results against an independent linear oracle, including short queries,
  reference banks, and candidate pruning for selective queries.
- High-diversity private text falls back without dropping matches when posting
  budgets are exceeded. This bounds index growth without relying on timing limits.
- Warm refresh reads zero payloads; changed-size and same-size edits invalidate
  cached banks; removal/corruption does not retain stale results; force rebuild
  rereads all banks; unchanged workers reuse the same immutable search snapshot.
- A–Z sorting ignores program numbers. Similar-name and names-only UI controls,
  refresh selection preservation, bounded previews retaining the match at the top,
  actual demo-file parsing, and refusal to overwrite demo data are exercised.

The reference corpus and its hashes remain unchanged. See the reproducible
[benchmark](../benchmarks/README.md) and [live demo instructions](../demo/README.md).

## Local results (2026-11-13, updated search engine)

- Ubuntu x86-64, GCC 15, Qt 5.15.18: **46 passed, 0 failed, 0 skipped**.
- Same environment, Qt 6.10.2: **46 passed, 0 failed, 0 skipped**.
- Qt 6 build with `-fsanitize=address,undefined -fno-omit-frame-pointer`:
  **46 passed, 0 failed, 0 skipped**, with leak detection enabled and no sanitizer
  diagnostics. Both sanitizer runtimes were verified linked.
- Live Qt 6 application and isolated demo launched; actual 200%-scale desktop captures
  were inspected for clipping, spelling recovery, and embedded-text previews. Large-font populated results were also captured from
  the UI test. Set `FXBVIEWER_TEST_SCREENSHOT=/tmp/fxbviewer.png` when running the
  suite to capture that populated test window.
- Source diff whitespace check, launcher shell syntax, fixture hashes, and
  workflow YAML checked.

Windows MSVC builds and regression tests also run in GitHub Actions. Current
release packaging and extracted-application smoke results are recorded per
commit in the [Build and test runs](https://github.com/shokace/fxbViewer---WINDOWS/actions/workflows/build.yml).

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

## Production hardening (2.2.0)

The suite now has **52 passing cases** locally on Qt 5.15.18 and Qt 6.10.2,
including ASan/UBSan with leak detection. New checks cover pre-cancelled reads,
non-regular files (including a Unix FIFO), oversized bank preset counts, 3,000
deterministic parser mutations, traversal/notice budgets, normalized context
across lines, preference persistence and settings-write errors.

CI also runs the executable's four-search `--smoke-test` on Linux and validates
the actual Windows ZIP on a separate runner. It checks extracted file hashes,
native screenshots, loaded Qt/platform/CRT module paths and a nonzero failure
exit for invalid output. See [release gates](../docs/RELEASING.md). The earlier
results above describe the 2.1 search-engine baseline, not the release package.
