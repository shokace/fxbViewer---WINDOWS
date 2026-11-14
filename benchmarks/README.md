# Performance comparison

Run `./dev.sh benchmark` to build an optimized benchmark and print its JSON report.
The benchmark is local, deterministic, and needs no external preset downloads.
It writes synthetic VST2 files to a temporary directory, then uses the real reader,
scanner, models, and search engine. It deletes the temporary files on exit.

The retained linear reference reproduces the combined-text/QVariant substring
filter from commit `4e34bcf`. Each query is timed 15 times, taking the median;
clearing the previous query is outside the timed section. Indexed queries are
quoted so the comparison uses the **same phrase semantics and result counts**.
The benchmark fails if counts differ. New ranking/sorting is included in the
indexed time, but window rendering is not. Files have just been generated and
are already in the OS cache: this is not a cold-disk benchmark.

Corpus: **320 files / 32,832 rows** — 256 parameter banks with 128 presets each,
plus 64 opaque banks containing 128 KiB of readable private state. The opaque
records are bank-text results, not individual decoded programs.

The checked-in `results-linux-qt6.json` records the measured release (`-O2`) run
on this Ubuntu x86-64 workstation with GCC 15 / Qt 6.10.2. Values are machine-
and workload-specific; they are measurements, not universal latency promises.

## Tradeoffs

The search index costs extra time and memory at initial load. Normalization and
posting construction run on the background worker. The completed immutable index
is shared with the UI, and unchanged refreshes reuse it. Parsed private text is
stored once per bank; display descriptions and previews are assembled on demand.

Cache reuse is based on size and timestamps, so it can miss changes on unusual
filesystems that preserve all metadata. **Rebuild from disk** bypasses the cache.
The benchmark verifies both zero files read and reuse of the exact search snapshot
on an unchanged refresh, over seven repetitions.

Short/common queries naturally touch more candidates. The index budgets private-
bank postings; high-diversity banks use exact verification instead of unbounded
posting growth. Approximate matching applies only to preset names, only after
zero exact matches, and is explicitly labeled in the UI.

Correctness is checked separately with real reference banks, a linear oracle,
ranking and typo examples, cancellation, cache invalidation, and UI tests. See
`tests/README.md`. Timing is not used as a brittle pass/fail test threshold.

## Measured result (2026-11-13)

| Query | Linear median | Indexed median | Speedup | Matches |
| --- | ---: | ---: | ---: | ---: |
| `glass lead` | 24.29 ms | 2.27 ms | 10.7× | 2,731 |
| `velvet` | 29.72 ms | 1.23 ms | 24.1× | 1,366 |
| `plucked moon` | 22.75 ms | 1.19 ms | 19.1× | 1,365 |
| `sub bass` | 25.50 ms | 1.19 ms | 21.5× | 1,366 |
| `silver bell` | 22.81 ms | 1.14 ms | 19.9× | 1,366 |
| `unfindablexyz` | 22.81 ms | 0.33 ms | 69.2× | 0 |
| `chorale` | 15.75 ms | 0.38 ms | 41.8× | 64 |
| `042` | 52.11 ms | 0.39 ms | 134.0× | 129 |

Initial file reading took **140.1 ms**, followed by **206.1 ms** to build the search index. An unchanged refresh took **4.99 ms** median, reused all **320 files**, and read **zero** file payloads.

The earlier unmodified model was also measured before replacement; its raw report is retained in `baseline-4e34bcf-linux-qt6.json`. The table above uses the repeatable same-process reference comparison, with exact result-count checks.
