# Market Data and Order Book Simulator


## Project Status


- Status: planned greenfield project; no existing implementation is assumed.
- Target platform: Ubuntu 24.04 LTS or a comparable x86-64 Linux distribution.
- Language: C++20.
- Core implementation estimate: 4-6 focused days.
- Resume-ready estimate: 7-10 focused days, including real data provenance,
 sanitizers, fresh benchmarks, documentation, and an interview walkthrough.


## 1. Project Goal


Build a deterministic, single-process simulator that:


1. Reads recorded Level 2 market-data events from newline-delimited JSON.
2. Reconstructs bid and ask depth in sequence order.
3. Detects duplicate, out-of-order, missing, malformed, empty, locked, and crossed
  states according to explicit policies.
4. Simulates fee-aware immediate-or-cancel orders against visible depth.
5. Separates displayed paired-outcome discrepancies from depth- and fee-aware
  executable scenarios.
6. Keeps observed market state separate from hypothetical execution state.
7. Produces machine-readable replay, validation, execution, and benchmark results.
8. Demonstrates correctness and performance through deterministic tests, sanitizers,
  and reproducible Linux benchmarks.


The project is market infrastructure, not a claim of alpha, profitable arbitrage, or
exchange-equivalent execution.


## 2. Why This Scope


This scope demonstrates the strongest defensible parts of the original idea:


- C++ data structures and ownership
- Market-data normalization
- Deterministic event processing
- Sequence and data-integrity handling
- Order-book invariants
- Fixed-point numerical reasoning
- Execution and fee modeling
- Testing malformed and adversarial inputs
- Linux performance measurement


It intentionally avoids concurrency in the first release. Updates for one market are
inherently ordered, and a concurrent or lock-free design would add complexity without
a measured need. Independent markets could be partitioned across threads in a later
version, but only after the single-threaded implementation is correct and benchmarked.


## 3. Scope


### 3.1 Required First Release


- One executable named `mdsim`.
- Canonical versioned NDJSON input schema.
- Snapshot and absolute level-update messages.
- Multiple market identifiers in one input stream.
- Per-market sequence validation and synchronization state.
- Aggregated Level 2 bids and asks.
- Fixed-point price, quantity, notional, and fee arithmetic.
- Strict validation mode used by tests and reported benchmarks.
- Optional audit mode that records bad events without mutating valid state.
- Best bid, best ask, spread, midpoint, and visible depth queries.
- Non-mutating order quotes.
- Mutating immediate-or-cancel execution against a cloned simulation book.
- Configurable no-fee and basis-point fee models.
- Paired-outcome analysis that keeps displayed and executable results separate.
- Structured replay summary and final-book output.
- Unit, integration, regression, and deterministic replay tests.
- AddressSanitizer and UndefinedBehaviorSanitizer runs.
- ThreadSanitizer run once threading is introduced; otherwise document that the
 first release has no shared mutable state.
- Reproducible throughput and latency benchmark harness.
- Dataset manifest, architecture notes, assumptions, limitations, and raw benchmark
 results.


### 3.2 Optional After the Completion Gate


- Minimal cash and position ledger for simulated fills.
- A documented adapter for one public prediction-market feed.
- Property-based or coverage-guided fuzz testing.
- Parallel replay across independent markets.


Optional work must not delay the correctness, provenance, and measurement gates.


### 3.3 Explicit Non-Goals


- Sending orders to a live venue
- Handling credentials, wallets, deposits, or settlement
- Claiming exchange-identical matching behavior
- Passive limit-order queue-position simulation
- Reconstructing individual orders from aggregated Level 2 data
- Predicting prices, finding alpha, or reporting profitability
- A graphical interface, web service, or database
- UDP, `epoll`, lock-free queues, SIMD, CUDA, or zero-copy I/O
- Distributed processing or Kubernetes
- A full backtesting framework or strategy optimizer


These features either require unavailable information, overlap with other planned
projects, or add complexity that is not justified by the first release.


## 4. Market Model and Assumptions


### 4.1 Level 2, Not Level 3


The input represents aggregate quantity at each price level. It does not contain
individual order identifiers or queue positions. Therefore:


- The simulator can reconstruct displayed price levels.
- It can model an aggressive order consuming displayed depth.
- It cannot determine time priority among passive orders at the same price.
- It cannot make defensible passive-fill or queue-position claims.


Price-time priority should be explained as an exchange matching rule, but the first
release must clearly state that Level 2 data is insufficient to reproduce it within a
price level.


### 4.2 Fixed-Point Arithmetic


Do not use `float` or `double` for prices, quantities, notional, or fees.


Use small value types backed by signed 64-bit integers:


- `Price`: integer ticks
- `Quantity`: integer quantity units
- `Notional`: integer settlement-currency units
- `Fee`: integer settlement-currency units


Each dataset manifest defines:


- Price scale, such as 1,000,000 units per currency unit
- Quantity scale
- Currency scale
- Valid minimum and maximum price
- Tick size
- Minimum quantity increment


Parse decimal strings with explicit overflow, scale, and precision checks. Use
`__int128` for intermediate multiplication and division, then check before narrowing
to 64 bits.


For a price and quantity, compute settlement-currency units as:


```text
gross_notional =
   round(price_ticks * quantity_units * currency_scale
         / (price_scale * quantity_scale))
```


Choose and document the notional rounding rule before implementation. Use the same
rule in hand-calculated fixtures, the execution engine, and reports. Derive VWAP from
total gross notional and total filled quantity instead of averaging level prices.


### 4.3 Observable Versus Hypothetical State


Maintain two concepts:


- `ObservedBook`: reconstructed only from recorded market-data events.
- `SimulationBook`: a copy of an observed state that hypothetical orders may consume.


A simulated fill must never mutate the observed book. Otherwise later recorded
updates would be applied to a state that never existed at the source venue.


### 4.4 Execution Assumptions


The first release supports aggressive immediate-or-cancel orders:


- A buy walks asks from lowest to highest while `ask_price <= limit_price`.
- A sell walks bids from highest to lowest while `bid_price >= limit_price`.
- Fills occur at displayed prices.
- An order may fill fully, fill partially, or receive no fill.
- Unfilled quantity is cancelled.
- Visible quantity is assumed available at the instant of simulation.
- Network delay, queue movement, hidden liquidity, self-trade prevention, and venue
 rejection are not modeled.
- Slippage is the difference between the first executable level and the
 volume-weighted average fill price, not an invented market-impact estimate.


These assumptions describe a deterministic scenario, not a forecast of real fills.


### 4.5 Fee Assumptions


Define a `FeeModel` interface with:


- `NoFeeModel`
- `BasisPointFeeModel`


For the basis-point model, document:


```text
fee = round_up(gross_notional * fee_bps / 10,000)
```


Use integer arithmetic and an explicit rounding direction. Add a venue-specific model
only when its public fee formula, effective date, units, and rounding behavior are
recorded in the repository and covered by examples.


### 4.6 Displayed Versus Executable Paired Outcomes


For a narrowly scoped prediction-market demonstration, allow a configuration to pair
two complementary outcome markets with a fixed settlement payout. Support only the
"acquire both outcomes" scenario in the first release.


Report two different values:


1. `displayed_discrepancy_per_unit`: payout minus both best asks, before size and
  fees. This is a top-of-book observation, not an executable result.
2. `simulated_executable_difference`: payout value minus depth-weighted costs and
  fees for a requested common quantity.


For the executable scenario:


- Quote both outcome books without mutating observed state.
- Set common executable quantity to the smaller filled quantity.
- Requote both legs at exactly that common quantity.
- Report no executable scenario when common quantity is zero.
- Calculate settlement value, gross cost, each leg's fees, and net difference with
 the same fixed-point rules as single-book execution.
- Label the result simulated and exclude latency, adverse movement, rejection,
 settlement risk, and real venue execution.


Never call the displayed discrepancy an arbitrage or profit. It ignores available
size and fees. Never call the simulated difference realized profit.


## 5. Data Source and Provenance


### 5.1 Two Dataset Classes


Use both of the following:


1. **Generated fixtures:** small, human-readable datasets committed to the repository
  for exact correctness tests.
2. **Recorded public data:** a legally usable capture or published dataset used for
  the final end-to-end demonstration and benchmark.


Generated data is sufficient to develop the engine. The resume phrase
"prediction-market order books from recorded events" is allowed only after a real
prediction-market source is captured, normalized, and documented.


### 5.2 Source Selection Gate


Before claiming support for a particular venue:


- Confirm that historical data or public capture is permitted.
- Save the public documentation URL and access date.
- Record whether messages are snapshots, deltas, trades, or heartbeats.
- Determine whether sequence numbers are global, per connection, per channel, or per
 market.
- Determine whether updates contain absolute quantity or quantity deltas.
- Record tick-size, quantity, and fee rules.
- Confirm whether a snapshot is required before deltas.
- Do not infer undocumented semantics from field names.


If a source has no reliable sequence field, use it only as a raw-data example. Do not
claim sequence-gap detection for that source.


### 5.3 Dataset Manifest


Every non-generated dataset must have a checked-in JSON manifest similar to:


```json
{
 "schema_version": 1,
 "dataset_id": "example-market-2026-07-18",
 "source_name": "SOURCE_NAME",
 "source_documentation_url": "https://example.com/docs",
 "source_type": "recorded_public_stream",
 "captured_at_utc": "2026-07-18T00:00:00Z",
 "capture_duration_seconds": 0,
 "market_ids": [],
 "raw_event_count": 0,
 "canonical_event_count": 0,
 "raw_sha256": "",
 "canonical_sha256": "",
 "raw_bytes": 0,
 "canonical_bytes": 0,
 "price_scale": 1000000,
 "quantity_scale": 1000000,
 "currency_scale": 1000000,
 "tick_size": 1000,
 "license_or_terms_url": "https://example.com/terms",
 "normalizer_command": "",
 "normalizer_git_commit": "",
 "notes": ""
}
```


Do not commit API keys, authentication headers, account identifiers, or data whose
terms prohibit redistribution. When raw data cannot be committed, commit the
manifest, capture/normalization scripts, hashes, and a small redistributable fixture.


## 6. Canonical Event Schema


### 6.1 Envelope


Store one JSON object per line. Every message contains:


```json
{
 "schema_version": 1,
 "source": "generated",
 "market_id": "example-market",
 "sequence": 42,
 "exchange_timestamp_ns": 0,
 "receive_timestamp_ns": 0,
 "type": "level_update",
 "payload": {}
}
```


Rules:


- `schema_version` is required and must equal a supported version.
- `source` and `market_id` are non-empty UTF-8 strings with documented length limits.
- `sequence` is an unsigned 64-bit integer scoped per market.
- Timestamps are unsigned nanoseconds since the Unix epoch; zero means unavailable.
- Unknown top-level or payload fields are rejected in strict mode.
- Duplicate object keys are rejected rather than accepting the parser's last value.
- Input line length, number of levels, and numeric string length are bounded.
- Prices and quantities are decimal strings to prevent JSON floating-point loss.
- Invalid UTF-8 is rejected in strict mode.


Initial defensive limits should be explicit constants: 1 MiB per line, 256 bytes per
identifier, 100,000 levels per snapshot side, 100,000 changes per update, and 64
characters per numeric string. A manifest may lower these limits but may not silently
raise compile-time safety caps.


### 6.2 Snapshot Message


```json
{
 "schema_version": 1,
 "source": "generated",
 "market_id": "example-market",
 "sequence": 100,
 "exchange_timestamp_ns": 0,
 "receive_timestamp_ns": 0,
 "type": "snapshot",
 "payload": {
   "bids": [
     {"price": "0.480000", "quantity": "100.000000"}
   ],
   "asks": [
     {"price": "0.520000", "quantity": "80.000000"}
   ]
 }
}
```


A snapshot atomically replaces both sides only after the complete candidate snapshot
passes validation.


### 6.3 Level Update Message


```json
{
 "schema_version": 1,
 "source": "generated",
 "market_id": "example-market",
 "sequence": 101,
 "exchange_timestamp_ns": 0,
 "receive_timestamp_ns": 0,
 "type": "level_update",
 "payload": {
   "changes": [
     {
       "side": "bid",
       "price": "0.490000",
       "new_quantity": "25.000000"
     }
   ]
 }
}
```


`new_quantity` is absolute, not a delta:


- Missing level plus positive quantity: add.
- Existing level plus positive quantity: modify.
- Existing level plus zero quantity: cancel.
- Missing level plus zero quantity: validation error in strict mode and a counted
 no-op in audit mode.
- Negative quantity: malformed input.


All changes within one message are atomic. Reject repeated `(side, price)` keys within
the same batch. For snapshots, build new containers and swap only after validation.
For updates, validate the complete batch, journal every touched level's original
value, apply the candidate changes, check final invariants, and roll back from the
journal if validation fails. This avoids copying the entire book for every update.


### 6.4 Why Absolute Updates


Absolute level quantity makes duplicate handling idempotent and avoids ambiguity when
normalizing feeds. If a source provides signed deltas, its adapter must convert them
to canonical absolute quantities while synchronized. The raw input remains preserved
for auditability.


## 7. Sequencing and Synchronization


Each market has:


```text
WaitingForSnapshot -> Synchronized -> Stale
                        |              |
                        +-> Invalid    +-> Synchronized after valid snapshot
```


### 7.1 Default Strict Policy


- A market begins in `WaitingForSnapshot`.
- Deltas before the first snapshot are rejected.
- A valid snapshot establishes `last_sequence`.
- A delta with `sequence == last_sequence + 1` applies atomically.
- `sequence == last_sequence` with the same canonical message hash: count as an exact
 duplicate and do not apply it again.
- `sequence == last_sequence` with different content: conflicting duplicate; mark the
 market invalid and stop replay in strict mode.
- `sequence < last_sequence`: out-of-order event; do not apply.
- A delta with `sequence > last_sequence + 1`: sequence gap; mark the market stale and
 reject that and subsequent deltas.
- A valid snapshot with `sequence > last_sequence` may atomically replace the book
 and resynchronize a synchronized or stale market. Count it as a resynchronization
 and separately record whether it skipped sequence values.
- A malformed message never advances sequence state.


Do not sort the input before replay. Sorting would hide feed defects and make
receive-order behavior impossible to audit.


### 7.2 Duplicate Identity


Compute duplicate identity from a stable canonical representation, not the original
JSON whitespace or object key order. Hash collision must not be the sole equality
check: retain the prior normalized event or compare normalized fields when sequence
numbers match.


### 7.3 Configurable Policies


Expose policies explicitly rather than embedding them in the book:


- `strict`: fail the command on the first integrity violation.
- `audit`: count and report violations, leave valid state unchanged, and continue
 when doing so is unambiguous.


Benchmarks and correctness claims use `strict`.


## 8. Order Book Design


### 8.1 Production Representation


Use standard-library ordered containers:


```cpp
using BidLevels = std::map<Price, Quantity, std::greater<Price>>;
using AskLevels = std::map<Price, Quantity, std::less<Price>>;
```


This gives:


- Constant-time access to the best level through `begin()`
- `O(log L)` add, modify, and cancel for `L` active price levels
- Deterministic price ordering
- Straightforward depth walking


Choose clarity over a custom allocator or flat structure until measurements identify
a meaningful bottleneck.


### 8.2 Reference Representation


Implement a small `ReferenceBook` used by tests and benchmarks:


- Store each side in `std::vector<Level>`.
- Find updates with a linear scan.
- Sort after a mutation when needed.
- Apply the same validation rules.


The reference implementation provides:


- An independent correctness oracle for generated event streams.
- A clear performance baseline.
- A way to report results even if the vector representation is faster for shallow
 books.


Do not describe the ordered-map version as an optimization unless it wins under a
documented workload.


### 8.3 Book Invariants


After every accepted snapshot or update:


- Every stored quantity is positive.
- Every price and quantity follows the configured scale and increment.
- A price occurs at most once on a side.
- Bids are observable in descending order.
- Asks are observable in ascending order.
- Best bid and ask caches, if added, equal the first stored levels.
- Total visible quantity equals the sum of stored levels.
- No state changes occur after a rejected message.
- Applying the same accepted dataset from an empty process produces the same final
 serialized state.


### 8.4 Empty, Locked, and Crossed Books


Treat these states distinctly:


- Empty book: both sides empty; valid but not executable.
- One-sided book: one side empty; valid but only one trade direction may execute.
- Locked book: best bid equals best ask; record as a validation anomaly.
- Crossed book: best bid exceeds best ask; record as a validation anomaly.


The source policy decides whether locked or crossed states fail strict replay. The
default first-release policy rejects both after an atomic candidate update. The error
must include market, sequence, best bid, best ask, and event type.


## 9. Component Architecture


```text
NDJSON file
   |
   v
LineReader -> JsonEventParser -> EventValidator -> MarketSequencer
                                                   |
                                                   v
                                            ObservedBookStore
                                                   |
                        +--------------------------+-------------------+
                        |                                              |
                        v                                              v
               Replay/validation summary                         BookView(s)
                                                                       |
                                                 +---------------------+---------+
                                                 |                               |
                                                 v                               v
                                      cloned SimulationBook          PairedOutcomeAnalyzer
                                                 |                               |
                                                 v                               v
                                      ExecutionSimulator        displayed and executable
                                                 |                    pair report
                                                 v
                                       fills and fee report
```


### 9.1 Main Components


- `DecimalParser`: converts bounded decimal strings to scaled integers.
- `JsonEventParser`: parses one line and reports field-specific errors.
- `EventValidator`: checks schema, ranges, increments, and message-level consistency.
- `MarketSequencer`: owns per-market synchronization and sequence policy.
- `OrderBook`: applies validated snapshots and updates atomically.
- `ReferenceBook`: independent, simple implementation for differential tests.
- `ObservedBookStore`: owns one sequencer and book per market.
- `BookView`: read-only top-of-book and depth query interface.
- `ExecutionSimulator`: walks a cloned book and returns fills.
- `FeeModel`: computes deterministic fees.
- `PairedOutcomeAnalyzer`: separates displayed parity from common-size simulated
 execution across two books.
- `ReplayEngine`: streams input without loading the entire file.
- `ReportWriter`: emits stable JSON and optional human-readable text.
- `BenchmarkRunner`: measures reference and production implementations.


### 9.2 Ownership


- `ReplayEngine` owns the parser and `ObservedBookStore`.
- `ObservedBookStore` owns books by market identifier.
- Events are value types moved from parser to replay; books do not retain references
 to parser buffers.
- `BookView` is non-owning and cannot outlive its book.
- `ExecutionSimulator` receives a book copy or an explicitly owned simulation state.
- No global mutable state or singleton is allowed.


### 9.3 Error Model


Use typed errors with:


- Category: I/O, JSON, schema, numeric, sequence, book invariant, execution, config
- Input line number
- Market identifier when available
- Sequence when available
- Stable error code
- Human-readable explanation


Use exceptions only at process boundaries or for unrecoverable construction errors.
Expected validation failures should use a result type such as
`std::expected<T, Error>` if the selected compiler supports the C++23 feature, or a
small local `Result<T>` compatible with C++20. Do not change the language standard
solely to obtain `std::expected`.


## 10. Proposed Repository Layout


```text
market-data-order-book-simulator/
├── CMakeLists.txt
├── CMakePresets.json
├── LICENSE
├── README.md
├── cmake/
│   ├── CompilerWarnings.cmake
│   └── Sanitizers.cmake
├── include/mdsim/
│   ├── decimal.hpp
│   ├── error.hpp
│   ├── event.hpp
│   ├── execution.hpp
│   ├── fee_model.hpp
│   ├── order_book.hpp
│   ├── paired_outcome.hpp
│   ├── reference_book.hpp
│   ├── replay_engine.hpp
│   ├── sequencer.hpp
│   └── types.hpp
├── src/
│   ├── decimal.cpp
│   ├── event_parser.cpp
│   ├── execution.cpp
│   ├── fee_model.cpp
│   ├── order_book.cpp
│   ├── paired_outcome.cpp
│   ├── reference_book.cpp
│   ├── replay_engine.cpp
│   ├── report_writer.cpp
│   └── sequencer.cpp
├── app/
│   └── main.cpp
├── tests/
│   ├── fixtures/
│   ├── integration/
│   └── unit/
├── benchmarks/
│   ├── benchmark_main.cpp
│   └── workloads/
├── tools/
│   ├── capture/
│   ├── normalize/
│   ├── generate_workload.py
│   ├── collect_system_info.sh
│   └── run_benchmarks.sh
├── data/
│   ├── fixtures/
│   └── manifests/
├── docs/
│   ├── architecture.md
│   ├── data-provenance.md
│   ├── event-schema.md
│   ├── execution-model.md
│   ├── interview-walkthrough.md
│   └── limitations.md
└── results/
   └── benchmarks/
```


Large captures and generated benchmark files should be ignored by Git. Keep manifests,
hashes, generators, commands, and small fixtures in version control.


## 11. Toolchain and Build


### 11.1 Linux Development Environment


Pin and record:


- Ubuntu 24.04 LTS
- CMake 3.28 or newer
- Ninja
- GCC 13 and Clang 18 where available
- Git
- Python 3 only for deterministic workload generation and source adapters


Recommended packages:


```bash
sudo apt-get update
sudo apt-get install -y build-essential clang cmake ninja-build \
 python3 python3-venv git
```


### 11.2 Dependencies


Keep the dependency set small:


- `nlohmann/json`, pinned to an exact release, for JSON parsing
- Catch2 v3 or GoogleTest, pinned to an exact release, for tests


Use CMake `FetchContent` or a checked-in dependency lock mechanism. Record exact
versions. The benchmark harness should use `std::chrono` and repository code unless a
benchmark library proves necessary.


### 11.3 CMake Targets


- `mdsim_core`: production library
- `mdsim`: CLI executable
- `mdsim_tests`: unit and integration tests
- `mdsim_bench`: benchmark executable


Required presets:


- `dev`: debug symbols and warnings
- `asan-ubsan`: AddressSanitizer and UndefinedBehaviorSanitizer
- `tsan`: reserved for threaded code
- `release`: `-O3 -DNDEBUG`, with exact compiler flags saved in results


Enable at least:


```text
-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow
```


Use warnings as errors in CI after third-party headers are isolated as system headers.


### 11.4 Expected Commands


```bash
cmake --preset dev
cmake --build --preset dev
ctest --preset dev --output-on-failure


cmake --preset asan-ubsan
cmake --build --preset asan-ubsan
ctest --preset asan-ubsan --output-on-failure


cmake --preset release
cmake --build --preset release
./tools/run_benchmarks.sh
```


The README must contain commands that work from a clean checkout.


## 12. CLI Contract


Prefer subcommands with explicit files and no interactive prompts.


### 12.1 Validate


```bash
mdsim validate \
 --input data/canonical/events.ndjson \
 --manifest data/manifests/dataset.json \
 --policy strict \
 --output results/validation.json
```


Reports counts for parsed, accepted, rejected, duplicate, out-of-order, gap,
resynchronization, locked, and crossed events.


### 12.2 Replay


```bash
mdsim replay \
 --input data/canonical/events.ndjson \
 --manifest data/manifests/dataset.json \
 --policy strict \
 --summary results/replay-summary.json \
 --final-books results/final-books.json
```


Stable final-book serialization sorts markets by identifier and levels by book order.
Running the command twice on the same input must produce byte-identical JSON after
excluding deliberately variable metadata such as wall-clock runtime.


### 12.3 Quote or Execute


```bash
mdsim quote \
 --book results/final-books.json \
 --market example-market \
 --side buy \
 --quantity 25.000000 \
 --limit-price 0.550000 \
 --fee-bps 20
```


Output includes:


- Requested, filled, and remaining quantity
- Ordered fills with level price and quantity
- Gross notional
- Fee
- Net cash effect
- Volume-weighted average fill price
- First-level price
- Slippage from first level
- Full, partial, or no-fill status
- All execution assumptions


`quote` is non-mutating. A separate `execute-scenario` command may process multiple
orders against one cloned simulation book.


### 12.4 Analyze Paired Outcomes


```bash
mdsim analyze-pair \
 --book results/final-books.json \
 --market-a outcome-yes \
 --market-b outcome-no \
 --quantity 25.000000 \
 --settlement-payout 1.000000 \
 --fee-bps-a 20 \
 --fee-bps-b 20
```


Output must place `displayed` and `simulated_executable` in separate objects and state
the common executable quantity and excluded real-world effects.


### 12.5 Exit Codes


- `0`: success
- `2`: command-line or configuration error
- `3`: input I/O error
- `4`: malformed event/schema error
- `5`: sequence-integrity error
- `6`: book-invariant error
- `7`: execution request error
- `8`: benchmark setup error


## 13. Detailed Implementation Phases


### Phase 0: Freeze Semantics and Evidence Rules


Estimated time: 2-3 hours.


Tasks:


- Create the new project repository.
- Select the Linux compiler and dependency versions.
- Write the first event-schema and execution-assumption documents.
- Choose strict locked/crossed behavior.
- Choose fixed-point scales for generated fixtures.
- Select or begin evaluating a public recorded data source.
- Add a claim ledger that marks all performance and data-volume results as pending.


Exit criteria:


- Schema examples parse as valid JSON.
- Every ambiguous term, especially "update," "fill," "latency," and "fee," has a
 written definition.
- No legacy metric appears as a target or achieved result.


### Phase 1: Scaffold and Domain Types


Estimated time: 3-4 hours.


Tasks:


- Add CMake targets, presets, warning flags, and test framework.
- Implement side, price, quantity, sequence, timestamp, and market identifier types.
- Implement checked decimal parsing and formatting.
- Implement typed errors and deterministic JSON output helpers.
- Add unit tests for scale conversion, boundaries, overflow, invalid signs, excessive
 precision, and round-trip formatting.


Exit criteria:


- Clean GCC and Clang debug builds.
- Domain tests pass.
- No core market value is represented by floating point.


### Phase 2: Event Parsing and Validation


Estimated time: 4-6 hours.


Tasks:


- Parse NDJSON one line at a time.
- Validate envelopes, snapshots, updates, field types, limits, and unknown fields.
- Normalize parsed messages into typed event values.
- Validate each update batch before exposing it to the sequencer.
- Add line number and field path to parse errors.
- Cap line size, level count, changes per message, and identifier length.


Exit criteria:


- Valid fixture messages round-trip to stable canonical JSON.
- Every malformed fixture fails with the expected stable error code.
- A large file can stream without loading all events into memory.


### Phase 3: Sequencer and Synchronization State


Estimated time: 3-5 hours.


Tasks:


- Implement per-market synchronization states.
- Enforce snapshot-before-delta.
- Detect exact and conflicting duplicates.
- Detect gaps and out-of-order events.
- Implement stale-market resynchronization through snapshots.
- Keep sequence policy separate from book mutation.


Exit criteria:


- A rejected message cannot alter sequence or book state.
- Interleaved markets maintain independent sequences.
- Strict and audit behavior are covered by table-driven tests.


### Phase 4: Production and Reference Books


Estimated time: 5-7 hours.


Tasks:


- Implement atomic snapshots and batched level updates.
- Implement update journaling and rollback without cloning the complete production
 book.
- Enforce price, quantity, tick, and book-state invariants.
- Implement best bid, best ask, spread, midpoint, top-N depth, and total visible
 quantity.
- Implement stable book serialization.
- Implement the vector-based reference book.
- Differentially compare reference and production state after every event in
 generated valid streams.


Exit criteria:


- Add, modify, cancel, snapshot replacement, empty, one-sided, locked, and crossed
 tests pass.
- Production and reference books agree on all valid generated streams.
- Final state is deterministic across repeated runs.


### Phase 5: Execution and Fees


Estimated time: 4-6 hours.


Tasks:


- Implement non-mutating quotes.
- Implement execution on an owned simulation-book copy.
- Walk price levels deterministically for buy and sell orders.
- Return full, partial, and no-fill outcomes.
- Compute gross notional, fee, net cash effect, VWAP, and slippage using checked
 integer arithmetic.
- Add no-fee and basis-point fee models.
- Implement displayed and common-size executable paired-outcome analysis.
- Reject zero, negative, off-increment, overflowing, or otherwise invalid requests.


Exit criteria:


- Buy and sell examples are manually calculable and match tests.
- Fees and rounding match documented examples.
- Displayed paired-outcome discrepancies cannot be confused with fee-aware
 executable results.
- Observed books remain unchanged after quotes and scenario execution.


### Phase 6: End-to-End CLI and Reports


Estimated time: 3-5 hours.


Tasks:


- Implement validate, replay, quote, analyze-pair, and benchmark commands.
- Emit stable machine-readable summaries.
- Include dataset ID, hash, schema version, executable version, and policy in reports.
- Add useful human-readable errors to standard error.
- Add golden integration fixtures and expected output.


Exit criteria:


- Clean-checkout commands reproduce golden output.
- Exit codes match the documented contract.
- Replaying the same fixture twice produces the same state and counts.


### Phase 7: Recorded Data Adapter


Estimated time: 4-8 hours, depending on source quality.


Tasks:


- Save raw source messages without modifying them.
- Implement a narrowly scoped normalizer into the canonical schema.
- Write source-specific semantic tests from public documentation examples.
- Produce and validate the dataset manifest and hashes.
- Reconcile snapshot, update, and sequence semantics without guessing.
- Record rejected raw messages and the reason for rejection.


Exit criteria:


- A documented command transforms raw data into canonical NDJSON.
- Event counts and hashes are reproducible.
- At least one recorded dataset replays in strict mode, or the source limitations are
 explicitly documented and the resume avoids recorded-source claims.


### Phase 8: Benchmark and Profile


Estimated time: 4-6 hours.


Tasks:


- Generate deterministic workloads with fixed seeds.
- Benchmark reference and production books.
- Measure end-to-end parse-and-apply throughput.
- Measure apply-only event latency.
- Record p50, p95, p99, maximum, event count, active depth, file size, and checksum.
- Run repeated trials, collect system details, and save raw outputs.
- Profile at least one representative run before attempting optimization.
- Change the implementation only for an observed bottleneck.


Exit criteria:


- The benchmark is reproducible from one documented command.
- Results include a meaningful baseline and do not hide losing cases.
- Release output proves work was not optimized away through a final-state checksum.
- No legacy 25% result appears unless a newly defined comparison independently
 produces it.


### Phase 9: Hardening, Documentation, and Interview Gate


Estimated time: 4-6 hours.


Tasks:


- Run sanitizers over all deterministic and generated stress tests.
- Add fuzzing or property tests if malformed-input coverage remains weak.
- Complete architecture, schema, assumptions, provenance, and limitations docs.
- Write an interview walkthrough.
- Perform a clean clone/build/test/replay/benchmark rehearsal.
- Audit prospective resume bullets against code and saved evidence.


Exit criteria:


- All completion gates in Section 19 pass.
- Every retained claim points to code, a test, a manifest, or a benchmark artifact.
- The candidate can explain the system without relying on the source code.


## 14. Test Plan


### 14.1 Fixed-Point and Domain Tests


- Valid zero, minimum increment, and maximum supported values
- Too many decimal places
- Positive sign, negative value, exponent notation, whitespace, empty string, NaN,
 and infinity rejection
- Multiplication intermediate overflow
- Narrowing overflow
- Fee round-up at exact and fractional units
- Parse-format-parse round trip


### 14.2 Parser and Schema Tests


- Valid snapshot and level update
- Missing required field
- Wrong field type
- Unsupported schema version
- Unknown field in strict mode
- Empty market identifier
- Oversized identifier, line, level array, or changes array
- Duplicate JSON keys are rejected
- Invalid UTF-8 policy
- Truncated JSON and multiple objects on one line
- Price or quantity that violates configured scale


### 14.3 Sequencing Tests


- Delta before snapshot
- First valid snapshot
- Consecutive updates
- Exact duplicate snapshot and update
- Same sequence with different content
- Older out-of-order event
- Single and multi-sequence gaps
- Delta while stale
- Resynchronizing snapshot
- Interleaved independent markets
- Malformed event does not advance sequence
- Unsigned sequence boundary behavior


### 14.4 Book Mutation Tests


- Add a bid and ask level
- Modify quantity up and down
- Cancel an existing level
- Cancel a missing level
- Replace a complete snapshot
- Duplicate prices within a snapshot
- Multiple atomic changes in one message
- Repeated side and price within one update batch
- One invalid change rejects the whole batch
- Best level changes after add, modify, and cancel
- Empty and one-sided books
- Locked and crossed candidate states
- Tick-size and quantity-increment violations
- Total depth and top-N queries


### 14.5 Execution Tests


- Buy fully fills at one ask
- Sell fully fills at one bid
- Buy and sell sweep multiple levels
- Partial fill from insufficient visible depth
- No fill because the limit is not marketable
- Exact limit-price boundary
- Fee-free and basis-point fees
- Fee rounding on small notional
- VWAP and slippage calculation
- Large valid order near arithmetic bounds
- Invalid side, quantity, or limit
- Quote leaves observed and simulation state unchanged
- Scenario execution mutates only its simulation book
- Second simulated order observes first simulated order's consumed depth
- Paired top-of-book discrepancy with no executable depth
- Paired legs with different available quantities use the smaller common quantity
- Paired executable costs sweep depth and include each leg's fees
- Paired result labels remain distinct in JSON output


### 14.6 Integration and Determinism Tests


- Golden fixture produces expected accepted counts and final books
- Golden execution scenario produces exact fills and fee report
- Repeated replay produces byte-identical stable output
- Different JSON whitespace produces the same normalized event
- Production and reference books have identical states after each valid event
- Strict replay stops at the expected line
- Audit replay counts the violation and preserves prior valid state
- Recorded source fixture converts to expected canonical events


### 14.7 Generated and Adversarial Tests


Create a deterministic generator with a fixed seed that:


- Starts from a valid snapshot.
- Produces valid add, modify, and cancel messages.
- Maintains a non-crossed reference state.
- Optionally injects one labeled defect: duplicate, conflict, gap, reorder, malformed
 number, invalid cancel, or crossed update.


For each seed:


- Compare production and reference state after every accepted event.
- Verify that the labeled defect yields the expected error.
- Save a minimal failing seed or input when a test fails.


### 14.8 Sanitizer and Static Checks


- AddressSanitizer and UndefinedBehaviorSanitizer on unit, integration, and generated
 tests
- ThreadSanitizer after any shared-state concurrency is introduced
- Clang-Tidy on project-owned files with a checked-in configuration
- Optional coverage report to locate untested error paths, not as a resume metric


## 15. Benchmark Plan


### 15.1 Questions


The benchmark should answer:


1. How many events per second can the program parse, validate, sequence, and apply?
2. What are p50, p95, and p99 apply latencies for pre-parsed events?
3. How does active book depth affect update cost?
4. How does the ordered-map book compare with the vector reference implementation?
5. Is JSON parsing or book mutation the dominant cost?


### 15.2 Workloads


Generate at least:


- `small`: 100K events, approximately 10 active levels per side
- `medium`: 1M events, approximately 100 active levels per side
- `deep`: 1M events, approximately 1,000 active levels per side
- `multi-market`: 1M events interleaved across 100 markets
- `recorded`: the documented public dataset, if redistribution and semantics permit


Use a fixed seed and report:


- Snapshot frequency
- Add/modify/cancel ratio
- Number of markets
- Average and maximum active levels
- Canonical file size and SHA-256


The 1M-event workload is a benchmark input, not a resume claim until the generator,
command, and resulting output are committed.


### 15.3 Metrics


Record:


- Total event count
- Accepted and rejected event count
- Wall-clock runtime
- Events per second
- Apply-only p50, p95, p99, and maximum latency
- End-to-end parse-and-apply throughput
- Peak resident memory
- Final-state checksum
- CPU model, logical CPU count, RAM, kernel, distribution
- Compiler name/version and all optimization flags
- Git commit and dirty-worktree status
- Warm-up count, measured run count, and CPU affinity


Do not label file-read throughput or generated in-memory updates as network latency.
Define the start and end points for every latency number.


### 15.4 Methodology


- Use a release build.
- Run one warm-up and at least five measured trials.
- Pin the benchmark to one CPU with `taskset` when permitted.
- Avoid other heavy workloads during measurement.
- Measure both cold file replay and warm cached replay if disk behavior matters.
- Report median throughput across trials and preserve all individual trials.
- Compute latency percentiles from recorded samples with the percentile rule
 documented.
- Measure instrumentation overhead with an empty loop or no-op event path.
- Use the same normalized events for reference and production comparisons.
- Consume and print a final-state checksum.
- Never delete inconvenient results or report only the fastest trial.


Example:


```bash
cmake --preset release
cmake --build --preset release
taskset -c 2 ./build/release/mdsim_bench \
 --input benchmarks/workloads/medium-1m.ndjson \
 --implementations reference,ordered-map \
 --warmup-runs 1 \
 --measured-runs 5 \
 --output results/benchmarks/2026-07-18/raw.json
```


### 15.5 Optimization Rule


Profile before optimizing. Accept an optimization only when:


- It preserves all correctness and differential tests.
- The compared workload and build flags are identical.
- At least five fresh trials show a consistent improvement.
- The result includes absolute performance, not only a percentage.
- Memory or clarity regressions are recorded.


If the simple implementation is already sufficient, keep it.


## 16. CI Plan


Use a Linux CI workflow that:


- Configures and builds with GCC.
- Configures and builds with Clang.
- Runs all non-benchmark tests.
- Runs AddressSanitizer and UndefinedBehaviorSanitizer.
- Runs Clang-Tidy on project code.
- Validates that committed fixture hashes match manifests.
- Replays a small golden fixture and compares stable output.


Do not use shared CI runners for resume performance numbers. CI validates behavior;
benchmarks come from a documented, controlled machine.


## 17. Documentation Deliverables


### README


- Project purpose and non-goals
- Linux prerequisites
- Clean build and test commands
- Minimal replay and quote example
- Architecture diagram
- Link to schema, provenance, benchmark, and limitations documents
- Current verified results only


### Architecture


- Component boundaries
- Data flow
- Ownership and lifetime
- Complexity of core operations
- Why the first release is single-threaded
- Why fixed-point values and standard containers were chosen


### Event Schema


- Full field definitions
- Sequence scope
- Absolute update semantics
- Validation limits
- Versioning policy
- Valid and invalid examples


### Execution Model


- IOC behavior
- Depth walking
- Partial fills
- Fee formula and rounding
- VWAP and slippage definitions
- Observable versus hypothetical state


### Data Provenance


- Source URLs and access dates
- Capture/normalization commands
- Dataset hashes and counts
- License or terms constraints
- Known source limitations


### Benchmark Report


- Question and baseline
- Hardware and software
- Dataset details
- Commands and methodology
- All trial results
- Summary table and charts
- Profile evidence
- Limitations and interpretation


### Interview Walkthrough


- Reconstruct a book by hand from a snapshot and updates.
- Explain every invariant and sequence failure path.
- Derive a partial fill and fee calculation.
- Explain why Level 2 cannot model queue position.
- Explain why replay remains single-threaded.
- Describe how to partition independent markets if concurrency becomes necessary.
- Identify the measured bottleneck and defend whether it was optimized.


## 18. Risks and Mitigations


### Ambiguous External Feed Semantics


Risk: source messages may not define sequence scope or update meaning clearly.


Mitigation: keep the canonical engine source-neutral, preserve raw data, implement a
source adapter only from public documentation, and avoid source-specific claims when
semantics remain uncertain.


### Unrealistic Fill Claims


Risk: displayed depth may disappear before an actual order reaches a venue.


Mitigation: call outputs simulated IOC fills, list excluded effects, and never label
them actual or guaranteed execution.


### Floating-Point or Overflow Errors


Risk: price-times-quantity and fee calculations may silently round or overflow.


Mitigation: use scaled integers, checked `__int128` intermediates, explicit rounding,
and boundary tests.


### Silent State Corruption


Risk: applying part of a bad snapshot or update batch leaves an invalid book.


Mitigation: validate candidate state before atomic commit and prove rejected events
do not change checksums.


### Benchmark Noise


Risk: scheduler activity, CPU scaling, disk cache, and per-event timing overhead
produce unstable latency results.


Mitigation: pin CPU affinity, separate throughput and sampled latency tests, warm up,
repeat trials, record raw output, and quantify timing overhead.


### Scope Expansion


Risk: networking, concurrency, backtesting, and venue-specific behavior delay a
correct first release.


Mitigation: enforce the non-goal list and complete the evidence gate before optional
features.


### Resume Overstatement


Risk: generated events or modeled fills are described as live market scale or real
trading.


Mitigation: retain a claim ledger and map every final noun, number, and technology to
an artifact before editing the resume.


## 19. Completion Gates


### Functional Gate


- Canonical snapshots and updates replay for multiple markets.
- Add, modify, and cancel behavior is correct.
- Best prices, spread, depth, and final serialization are correct.
- Full, partial, and no-fill IOC simulations are correct.
- Fees and rounding follow documented formulas.
- Displayed paired-outcome discrepancy and common-size executable simulation are
 calculated and reported separately.


### Integrity Gate


- Duplicate, conflicting duplicate, out-of-order, gap, malformed, locked, and crossed
 cases have explicit tested behavior.
- Rejected messages never partially mutate state.
- A later snapshot can resynchronize stale state.
- Production and reference implementations agree on generated valid streams.


### Determinism Gate


- Repeated replay produces the same accepted counts, final state, fills, and checksum.
- Golden output is byte-identical after excluding runtime metadata.
- Workload generation uses recorded seeds and produces stable hashes.


### Quality Gate


- Clean GCC and Clang builds pass.
- Unit and integration tests pass.
- AddressSanitizer and UndefinedBehaviorSanitizer report no known errors.
- ThreadSanitizer passes if threading is added.
- Static analysis has no unexplained high-severity findings.


### Evidence Gate


- Dataset source, access date, semantics, size, count, and license are documented.
- Raw and canonical hashes are saved.
- Benchmark hardware, compiler, flags, workloads, commands, and all trials are saved.
- A clear reference baseline is included.
- All numbers come from fresh runs on the implemented system.


### Understanding Gate


Sahas can, without reading the implementation:


- Build a small book by hand.
- Explain snapshot and absolute update semantics.
- Trace every sequence-error transition.
- Explain book container complexity and tradeoffs.
- Calculate a multi-level partial fill, VWAP, fee, and net cash effect.
- Explain the limits of Level 2 execution simulation.
- Defend the benchmark endpoints, baseline, and percentile methodology.
- Describe a reasonable parallelization design without claiming it already exists.


## 20. Suggested Commit Sequence


1. `chore: scaffold C++20 project and Linux build presets`
2. `feat: add fixed-point market domain types`
3. `feat: parse and validate canonical market events`
4. `feat: enforce per-market sequence integrity`
5. `feat: reconstruct aggregate order books`
6. `test: add reference book and differential replay cases`
7. `feat: simulate IOC fills and basis-point fees`
8. `feat: add replay validation and quote commands`
9. `data: document and normalize recorded market events`
10. `bench: add reproducible replay workloads and raw results`
11. `docs: record architecture assumptions and limitations`


Each commit should build and pass the tests relevant to its stage.


## 21. Resume Claim Gate


Do not edit the final resume until the completion gates pass.


Potential bullets, with all bracketed fields requiring fresh evidence:


- Built a C++20 simulator that reconstructed Level 2 prediction-market order books
 from `[VERIFY: source and event count]` recorded events with deterministic sequence
 validation.
- Implemented fixed-point, fee-aware IOC execution across visible depth, with tests
 for partial fills, atomic updates, malformed data, duplicates, gaps, and
 out-of-order events.
- Replayed `[VERIFY: count]` events at `[VERIFY: throughput]` events per second and
 `[VERIFY: p99 and endpoint]` p99 latency on `[VERIFY: hardware]` against
 `[VERIFY: baseline]`.


If no real source is completed, replace "prediction-market" and "recorded events"
with accurate generated-workload language. If no measured optimization beats the
reference implementation, report the absolute result without an improvement claim.


Never restore "live," "50K+," "multithreaded," "lock-free," or "25% lower latency"
from the old resume unless the rebuilt repository independently supports each term.



