#!/usr/bin/env python3
"""Deterministic independent VST2 fixtures. Does not call the C++ reader."""
from pathlib import Path
import hashlib
import json
import struct

ROOT = Path(__file__).resolve().parent / 'fixtures'
OUT = ROOT / 'generated'
OUT.mkdir(exist_ok=True)

def record(kind, count, payload, version=1):
    body = kind + struct.pack('>I4sII', version, b'Test', 100, count) + payload
    return b'CcnK' + struct.pack('>I', len(body)) + body

def program(name, params=(0.25, 0.75)):
    return record(b'FxCk', len(params), name.ljust(28, b'\0') + struct.pack('>' + 'f' * len(params), *params))

lead = program(b'Glass Lead')
bass = program(b'Deep Bass')
fixtures = {
    'standard.fxb': record(b'FxBk', 2, bytes(128) + lead + bass),
    'parameter.fxp': lead,
    'chunk.fxp': record(b'FPCh', 1, b'Warm Pad'.ljust(28, b'\0') + struct.pack('>I', 11) + b'\x00Warm Glow\x00'),
    'unicode.fxp': program('Étoile Pad'.encode()),
    'latin1.fxp': program('Crème Lead'.encode('latin1')),
    'empty.fxb': record(b'FxBk', 0, bytes(128)),
}
payload = b'\xff\x00Hidden Lead\x00Deep Bass\x00' + 'Crystal Keys'.encode('utf-16le') + b'\0\0' + 'Silver Bell'.encode('utf-16be') + b'\0\0'
fixtures['opaque.fxb'] = record(b'FBCh', 4, bytes(128) + struct.pack('>I', len(payload)) + payload)
for name, data in fixtures.items():
    (OUT / name).write_bytes(data)

# Expectations are explicit, independent of the reader under test.
expected = {
    'standard.fxb': ('FxBk', 'Test', ['Glass Lead', 'Deep Bass']),
    'parameter.fxp': ('FxCk', 'Test', ['Glass Lead']),
    'chunk.fxp': ('FPCh', 'Test', ['Warm Pad']),
    'unicode.fxp': ('FxCk', 'Test', ['Étoile Pad']),
    'latin1.fxp': ('FxCk', 'Test', ['Crème Lead']),
    'empty.fxb': ('FxBk', 'Test', []),
    'opaque.fxb': ('FBCh', 'Test', []),
}
manifest = []
for name, (kind, plugin, names) in expected.items():
    manifest.append(dict(file='generated/' + name, sha256=hashlib.sha256(fixtures[name]).hexdigest(), format=kind, plugin=plugin, names=names))
references = {
    'rea-comp.FXB': ('FxBk', 'recm', ['']),
    'rea-comp.FXP': ('FxCk', 'recm', ['']),
    'massive-test-patch.FXP': ('FPCh', 'NiMa', ['Untitled']),
    'massive-test-bank.FXB': ('FBCh', 'NiMa', []),
    'podolski-pad.fxp': ('FPCh', 'Podo', ['initialize']),
}
for name, (kind, plugin, names) in references.items():
    data = (ROOT / 'reference' / name).read_bytes()
    manifest.append(dict(file='reference/' + name, sha256=hashlib.sha256(data).hexdigest(), format=kind, plugin=plugin, names=names))
(ROOT / 'manifest.json').write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + '\n')
