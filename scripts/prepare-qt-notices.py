#!/usr/bin/env python3
"""Ship the exact Qtbase source plus its license/copyright notices. Stdlib only."""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import shutil
import tarfile
import urllib.request


def prepare(destination, cache):
    config = json.loads((Path(__file__).resolve().parents[1] / 'packaging/qt.json').read_text())
    destination.mkdir(parents=True, exist_ok=True)
    cache.mkdir(parents=True, exist_ok=True)
    filename = config['source_url'].rsplit('/', 1)[1]
    archive = cache / filename
    if not archive.exists() or hashlib.sha256(archive.read_bytes()).hexdigest() != config['source_sha256']:
        temporary = archive.with_suffix('.download')
        with urllib.request.urlopen(config['source_url'], timeout=120) as response, temporary.open('wb') as output:
            shutil.copyfileobj(response, output)
        if hashlib.sha256(temporary.read_bytes()).hexdigest() != config['source_sha256']:
            temporary.unlink()
            raise RuntimeError('Qt source SHA-256 mismatch')
        temporary.replace(archive)
    shutil.copy2(archive, destination / filename)
    # Read selected regular members, never extract arbitrary archive paths or links.
    with tarfile.open(archive) as source:
        for member in source:
            path = PurePosixPath(member.name)
            if not member.isfile() or path.is_absolute() or '..' in path.parts:
                continue
            relative = Path(*path.parts[1:])
            name = relative.name.lower()
            if 'LICENSES' in relative.parts or name.startswith(('license', 'copying', 'copyright')) or name == 'qt_attribution.json':
                target = destination / 'notices' / relative
                target.parent.mkdir(parents=True, exist_ok=True)
                with source.extractfile(member) as stream, target.open('wb') as output:
                    shutil.copyfileobj(stream, output)
    (destination / 'provenance.json').write_text(json.dumps(config, indent=2) + '\n', encoding='utf-8')
    for required in ('LGPL-3.0-only.txt', 'GPL-3.0-only.txt'):
        if not (destination / 'notices/LICENSES' / required).is_file():
            raise RuntimeError(f'Missing Qt license: {required}')


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('destination', type=Path)
    parser.add_argument('--cache', type=Path, required=True)
    args = parser.parse_args()
    prepare(args.destination, args.cache)
