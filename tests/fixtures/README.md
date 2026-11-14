# Fixture provenance

`generated/` contains seven small, deterministic files authored for this project.
`../generate_fixtures.py` constructs their big-endian records independently with
Python's `struct` module. They are deliberately synthetic, not Sylenth exports.
They cover parameter banks, individual parameter/chunk presets, opaque bank text,
empty banks, UTF-8, Latin-1, and ASCII names encoded as UTF-16LE/BE.

`reference/` contains five unmodified real-world fixtures from Charles Holbrow's
[vst2-preset-parser](https://github.com/CharlesHolbrow/vst2-preset-parser/tree/d6c4e0e985e6e47026b5f60a44458908967449d1/test),
pinned to commit `d6c4e0e985e6e47026b5f60a44458908967449d1`:

| File | Format | Expected name / useful observation |
| --- | --- | --- |
| rea-comp.FXB | FxBk | One unnamed ReaComp program |
| rea-comp.FXP | FxCk | Unnamed ReaComp program, 19 parameters |
| massive-test-patch.FXP | FPCh | `Untitled`, plugin ID `NiMa` |
| massive-test-bank.FXB | FBCh | Opaque bank; UTF-16 `Untitled` and `Massive` text |
| podolski-pad.fxp | FPCh | `initialize`, plugin ID `Podo`; outer byteSize is zero |

The upstream package declares the ISC license in its
[package.json](https://github.com/CharlesHolbrow/vst2-preset-parser/blob/d6c4e0e985e6e47026b5f60a44458908967449d1/package.json).
See `reference/NOTICE.txt`. Only test data is incorporated; no upstream parser
implementation is copied. Plugin names identify the source formats; plugins are
not bundled or executed.

`manifest.json` pins SHA-256 hashes and independently specified expected metadata.
Normal test runs read the checked-in fixtures and manifest without regenerating
them or accessing the network. Regenerate only when intentionally changing the
synthetic corpus, then review the manifest diff. The generator also records the
existing reference-file hashes; it does not download or rewrite reference files.

Format layout was cross-checked against this independent parser and JUCE's
[VST2 record definitions](https://github.com/juce-framework/JUCE/blob/master/modules/juce_audio_processors_headless/format_types/juce_VSTPluginFormatImpl.h).

These references do not establish full Sylenth-specific decoding. Plugin-private
bank chunks remain opaque and searchable as embedded text; compressed or encrypted
preset names may not be discoverable.
