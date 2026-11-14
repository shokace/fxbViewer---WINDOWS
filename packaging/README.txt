FXB Viewer for Windows x64

Extract the entire ZIP to a writable folder, then open fxbViewer.exe.
Keep its DLLs and platforms folder beside it. No Qt installation, administrator
rights, network connection, synthesizer or plugin installation is required.
Windows 10 (1809 or later) and Windows 11 x64 are the intended platforms.

Add a folder containing .fxb or .fxp files. Type remembered words into the search
box. Words may appear in any order; use quotes for a phrase. Similar-name search
recovers small typos only when no exact matches exist. Select a result to copy
its path or open its containing folder. Plugin-private text is labeled as bank
text; it is not represented as a decoded preset list.

Refresh reuses unchanged files. File > Rebuild from disk forces a complete read.
The app never changes your preset files, loads plugins or plays sounds.
Clear library removes only the remembered folder list and search results.

For a safe demo, run: fxbViewer.exe --demo
For diagnostics: Help > Copy diagnostic information. This excludes folder paths
and preset names. Review notices explains unreadable or unsupported banks.

Preferences live in HKEY_CURRENT_USER\Software\fxbViewer\FXB Viewer on Windows.
To reset them, close the app, back up that registry key and remove it. The demo
uses temporary separate preferences. Multiple normal instances share preferences;
the last instance to save wins. Presets and indexes are never stored in settings.

To update, close the application and extract the newer ZIP into a new folder.
Keep the previous version for rollback; do not mix DLLs from different releases.
To uninstall, remove the extracted folder and optionally the preferences above.
Your sound banks remain untouched. No services or scheduled tasks are installed.

This build is unsigned. Windows may display an unknown-publisher/SmartScreen
warning. Verify the ZIP against SHA256SUMS.txt from the same GitHub build/release.
Checksums detect corruption; they are not a substitute for publisher signing.

Limits: 64 MiB per file; 100,000 presets per bank/library; 10,000 files per
library; 64 Mi characters of private text per library; 256 Ki characters per
file; one million discovered directory entries. Reaching a limit is reported.
Cancel is checked during reading/parsing/indexing. An operating-system disk or
network read can still take time to return; prefer available local folders.

Source and issue tracker:
https://github.com/shokace/fxbViewer---WINDOWS
Copyright Petar Juric. All rights reserved. See LICENSE and THIRD_PARTY_NOTICES.txt.
