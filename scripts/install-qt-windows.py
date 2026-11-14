#!/usr/bin/env python3
"""Pinned aqt adapter for Qt's compiler-specific 6.11 Windows repository layout.

Remove this adapter when upstream aqt supports the layout directly. Downloads,
checksums, extraction and relocation continue to use aqt's implementation.
"""
import importlib.metadata
import json
from pathlib import Path
import sys

from aqt import main
from aqt.archives import QtArchives

if __name__ == '__main__':
    if importlib.metadata.version('aqtinstall') != '3.3.0':
        raise RuntimeError('This compatibility adapter requires aqtinstall 3.3.0')
    config = json.loads((Path(__file__).resolve().parents[1] / 'packaging/qt.json').read_text())
    if config['version'] != '6.11.3':
        raise RuntimeError('Review repository layout adapter before changing Qt version')
    original = QtArchives._arch_ext

    def arch_extension(self):
        if str(self.version) == '6.11.3' and self.os_name == 'windows' and self.arch == 'win64_msvc2022_64':
            return '_msvc2022_64'
        return original(self)

    QtArchives._arch_ext = arch_extension
    sys.exit(main())
