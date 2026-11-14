# FXB Viewer

Find which sound bank contains a preset name you remember. Add your folders once,
then search names, bank filenames, plugin IDs, paths, and readable embedded text.
No plugins are loaded, and your preset files are never modified.

The original utility was written by Shokace / Petar Juric to find Sylenth patches.
This update replaces the manual arrays, text-line scanning, blocking event handlers,
and fixed-position UI with separate readers, background indexing, and resizable
Qt Widgets layouts. Qt 6 is preferred; Qt 5.15 remains supported.

## Use

1. Choose **Add folder** or drop folders onto the window. Subfolders are scanned.
2. Type into the search box. Matching is literal, case-insensitive, and immediate
   against the completed in-memory index. Blank searches show all results.
3. Select a result to see its format, notices, and embedded text. Use **Copy path**
   or **Open containing folder** to locate the bank in your synth or DAW.
4. Use **Refresh library** after files change. Folder selections survive restarts;
   the app rebuilds its index at launch so results are not persisted stale.

Overlapping folders and file symlinks are deduplicated by canonical path. Directory
symlinks are not followed. Cancelling an index retains the previous completed
index. Removing a folder clears old matches before rebuilding. **Clear library**
removes folder selections and results, not files. Unreadable or malformed files
appear under **View issues**. Closing the app cancels and joins its worker safely.

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
- `core/resultmodel.*`: result rows and literal search filtering.
- `mainwindow.*`: layouts, library management, settings, and desktop actions.
- `tests/`: independent reference data and parser, scanner, model, and UI tests.
