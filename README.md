# Order Book Simulator

A compact C++20 project for replaying Level 2 market-data events, reconstructing an order book, validating event integrity, and simulating fee-aware immediate-or-cancel executions against a synthetic book.

## Purpose

The simulator is designed to be deterministic and testable. It reads recorded market events, rebuilds bid/ask depth in sequence, checks for invalid or out-of-order states, and evaluates how aggressive orders would execute without mutating the observed market state.

## Build and run

```bash
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
```

The main executable is generated at [build/dev/mdsim](build/dev/mdsim) and can be invoked from the project root.

## Sample data

The repository includes a small synthetic book and a matching event feed under [data](data):

- [data/sample_book.json](data/sample_book.json) — a simple two-market snapshot for `quote` and `analyze-pair`
- [data/sample_events.ndjson](data/sample_events.ndjson) — a minimal replay stream with one snapshot and one level update

Example commands:

```bash
./build/dev/mdsim quote --book data/sample_book.json --market demo --side buy --quantity 1 --limit-price 0.530000 --fee-bps 25
./build/dev/mdsim replay --input data/sample_events.ndjson --summary ./summary.json --final-books ./books.json
./build/dev/mdsim validate --input data/sample_events.ndjson --output ./validation.json
```

## Benchmarking

A deterministic benchmark flow is available for a 100K-event replay workload.

Run it from the project root:

```bash
./tools/run_benchmark.sh
```

This generates a synthetic workload at [benchmarks/workloads/100k-events.ndjson](benchmarks/workloads/100k-events.ndjson), replays it through the CLI, and writes:

- [results/benchmarks/100k-summary.json](results/benchmarks/100k-summary.json)
- [results/benchmarks/100k-books.json](results/benchmarks/100k-books.json)
- [results/benchmarks/100k-benchmark.json](results/benchmarks/100k-benchmark.json)

Current verified result from the latest run:

- 100,000 parsed and accepted events
- 0 rejected events
- 0 book errors
- wall-clock time: 7.99 seconds
- throughput: about 12,508.74 events/second

## Project structure

- [app/main.cpp](app/main.cpp) — CLI entry point
- [include/mdsim](include/mdsim) — public headers and shared types
- [src](src) — parsing, sequencing, order-book, execution, fee, replay, and reporting logic
- [tests](tests) — Catch2-based unit and integration tests
- [docs](docs) — planning and design notes

## Flow

```text
Market events
  -> parser / sequencer
  -> order book state
  -> execution + fee model
  -> report writer
  -> replay / validation output
```

In short, recorded events are normalized into book state, a simulation book can be evaluated for hypothetical fills, and the results are emitted as structured replay or analysis output.
