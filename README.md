# FXB Viewer

Find which sound bank contains a preset name you remember. Add your folders once,
then search names, bank filenames, plugin IDs, paths, and readable embedded text.
No plugins are loaded, and your preset files are never modified.

The original utility was written by Shokace / Petar Juric to find Sylenth patches.
This update replaces the manual arrays, text-line scanning, blocking event handlers,
and fixed-position UI with separate readers, background indexing, and resizable
Qt Widgets layouts. Search now uses an indexed, relevance-ranked engine. Qt 6 is preferred; Qt 5.15 remains supported.

## Windows distribution

Download the portable Windows x64 ZIP and `SHA256SUMS.txt` from the same GitHub
Release (or a successful Actions `windows-x64-package` artifact). Extract the
whole ZIP and launch `fxbViewer.exe`; the Qt/runtime dependencies are included.
Builds are currently unsigned. See the included README for updating, rollback,
and resetting preferences. The project is **all rights reserved**; bundled Qt
and other third-party components retain their own licenses.

[Release gates and packaging](docs/RELEASING.md) · [Changes](CHANGELOG.md)

## Use

1. Choose **Add folder** or drop folders onto the window. Subfolders are scanned.
2. Type into the search box. Words can appear in any order. Case and accents are
   ignored; quotes keep words together as a phrase. Best name matches appear first.
   Blank searches show all results. **Find similar names** tries one edit per word
   only when no exact matches exist. **Names only** excludes bank metadata.
3. Select a result to see its format, notices, and embedded text. Use **Copy path**
   or **Open containing folder** to locate the bank in your synth or DAW.
4. Use **Refresh library** after files change. Unchanged banks and their search
   index are reused in memory. **File → Rebuild from disk** bypasses that cache.
   Folder selections survive restarts; the index is rebuilt at launch.

Overlapping folders and file symlinks are deduplicated by canonical path. Directory
symlinks are not followed. Cancelling an index retains the previous completed
index. Removing a folder clears old matches before rebuilding. **Clear library**
removes folder selections and results, not files. Unreadable or malformed files
appear under **Review notices**. Closing the app cancels and joins its worker safely.

## Format support and limits

| Format | Behavior |
| --- | --- |
| FXB `FxBk` | Validates bank/program records; lists names and one-based positions |
| FXP `FxCk` | Validates parameter payload; lists the program name |
| FXP `FPCh` | Validates chunk length; lists the header name and indexes readable chunk text |
| FXB `FBCh` | Validates chunk length; returns one clearly labeled embedded-text result per bank |

Chunk data is plugin-specific. An embedded-text match can be metadata, not a preset
name. No universal decoder is claimed for Sylenth or other proprietary chunks;
compressed/encrypted names may not be searchable. Text extraction supports UTF-8
with Latin-1 fallback, plus ASCII text stored in UTF-16LE/BE. This is a search tool,
not a plugin host, audio previewer, or preset converter.

The reader bounds every record and rejects truncated payloads, impossible counts,
unknown record types, and trailing data. It tolerates a zero outer size with a
notice (present in the real Podolski fixture), while still checking payload sizes.
Files over 64 MiB are rejected. Embedded text is limited to 256 Ki characters per
file with a visible notice. A library is capped at 10,000 files, 100,000 presets,
and 64 Mi UTF-16 characters of embedded text; limits are reported, never silently
presented as a complete scan. No database or network service is required.

## Build and run

Open `fxbViewer.pro` in Qt Creator with a Qt 6 or Qt 5.15 desktop kit, or use qmake.
A C++17 compiler is required. Build outside the source directory.

On Ubuntu, install `build-essential qt6-base-dev gdb` (or `qtbase5-dev qt5-qmake`
for Qt 5), then:

```sh
./dev.sh demo       # Try the isolated sample library
./dev.sh benchmark  # Reproduce the performance comparison
./dev.sh build
./dev.sh run
./dev.sh debug
./dev.sh test
```

`dev.sh` prefers Qt 6 and also detects the user-local development tools set up on
this workstation under `~/.local/share/fxbviewer-dev`. Override with `QMAKE`.
Artifacts go to `../fxbViewer---WINDOWS-build/qt6-debug` (or `qt5-debug`). Set
`FXBVIEWER_BUILD_DIR` for another location. Qt major versions use separate build
folders to avoid mixing incompatible object files.

On Windows, use a Qt-enabled MSVC command prompt matching your Qt kit:

```bat
mkdir build
cd build
qmake ..\fxbViewer.pro CONFIG+=release CONFIG-=debug
nmake
release\fxbViewer.exe
mkdir tests
cd tests
qmake ..\..\tests\tests.pro CONFIG+=release CONFIG-=debug
nmake
set QT_QPA_PLATFORM=offscreen
release\fxbviewer_tests.exe
```

For MinGW kits, use `mingw32-make` instead of `nmake`. To distribute the Windows
executable, run your kit's `windeployqt` on it to bundle its Qt runtime/plugins.
The GitHub workflow defines Linux Qt 5/6 and Windows Qt 6 build/test jobs.

## Debugging and tests

`./dev.sh debug` runs GDB with debug symbols and logs to `gdb.log` in the build
directory. After a crash, enter `bt`; to investigate a hang, press Ctrl+C and use
`thread apply all bt`. On Wayland, `QT_QPA_PLATFORM=xcb ./dev.sh debug` enables X11
window inspection. Ordinary runs use the platform selected by Qt.

`./dev.sh test` runs the QtTest suite headlessly. It uses checked-in data and needs
no network access. Tests cover binary parsing and every truncated prefix, malformed
counts/lengths, text encodings and limits, recursive scanning, duplicate/overlapping
folders, the former >10-file bug, cancellation, missing files, search and sorting,
settings persistence, refresh/removal, copy/reveal actions, clear/close during
indexing, and large-font/resizable UI behavior.

Five real-world fixtures from ReaComp, Massive, and Podolski complement seven
independently generated fixtures. See [provenance and hashes](tests/fixtures/README.md)
and the [test suite](tests/test_fxbviewer.cpp). Reference data is attributed under
its upstream ISC license; no external parser code is incorporated.

## Source layout

- `core/bankreader.*`: bounded VST2 reader and embedded-text extraction; no widgets.
- `core/library.*`: discovery, canonical deduplication, indexing, cancellation.
- `core/searchindex.*`: immutable trigram postings, relevance ranking, typo candidates.
- `core/resultmodel.*`: result rows, search filtering, contextual previews, sorting.
- `core/demodata.*`: deterministic synthetic VST2 bank generator.
- `mainwindow.*`: layouts, library management, settings, and desktop actions.
- `tests/`: independent reference data and parser, scanner, model, and UI tests.

## Smarter search and faster refresh

- `glass lead` and `lead glass` find the same set of matches. Exact names and
  phrase/prefix matches rank above scattered words, bank names, or private text.
- `etoile` finds `Étoile`. Hyphens and underscores act as word separators.
- `"glass lead"` requires that phrase; straight and typographic quotes work.
- `glass leed` can recover `Glass Lead` when no exact result exists. Similar-name
  results are labeled; private chunk text is never guessed into a preset name.
- The **Match source** column explains each hit, and the preview shows relevant
  context. Previews are bounded without discarding the matched snippet.
- Preset-name A–Z sorting ignores the displayed program number. The table and
  preview have a draggable divider, and unchanged refreshes preserve selection.

Search precomputes normalized text and three-character posting lists on the
indexing worker. It intersects the smallest candidate lists before checking exact
contents, so it avoids scanning every full bank on each keystroke. One/two-character
queries use direct verification. Highly diverse private chunks fall back to direct
verification when posting budgets are reached; they remain searchable. Typo lookup
uses deletion signatures and verifies one insertion, deletion, substitution, or
adjacent transposition per word. It applies to name words of 4–32 characters, with
a 20,000-word vocabulary cap; exact search remains complete within the file/text
limits above. A query is limited to 256 characters. Each bank is capped at 100,000 programs,
directory discovery at one million entries, and scanner notices at 256 plus a
truncation notice. Limits are reported; choose smaller folders to index the rest.

Refresh cache keys use canonical path, byte size, modification time, and metadata
change time. This is an in-memory optimization, not a content-hash guarantee on
filesystems with coarse or preserved timestamps. **Rebuild from disk** always
rereads files. Changed/deleted files invalidate their results, failed reads are
retried, and files changed during reading are reported for retry. A cancelled
refresh never replaces the completed index.

See [measured performance and tradeoffs](benchmarks/README.md),
[the demo walkthrough](demo/README.md), and [verification](tests/README.md).

Window layout and search options survive restarts. Settings failures are shown in
the window. Help → Copy diagnostic information copies version/platform/counts
without paths or preset names. `--smoke-test <directory>` runs the isolated demo
tour, verifies search results, saves screenshots/JSON, and exits with a status code.
