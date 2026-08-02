# Human-Agent Plan for Building the Order Book Simulator

## TL;DR

- You (human) should focus on interview-defensible fundamentals: market semantics, fixed-point math, sequencing rules, order book invariants, and being able to run/build/test/explain the system.
- The agent should implement most coding-heavy phases from the main plan, with you reviewing key design and test evidence.
- This split follows the detailed phase plan in [order-book-plan.md](./order-book-plan.md), especially Phase 0-9 and the Understanding Gate.

## Skills and Tools to Learn/Install Before Coding

### Install first (minimum setup)

Based on Toolchain section in [order-book-plan.md](./order-book-plan.md):

- `build-essential`
- `clang`
- `cmake` (3.28+ target from plan)
- `ninja-build`
- `python3` and `python3-venv`
- `git`

Suggested command:

```bash
sudo apt-get update
sudo apt-get install -y build-essential clang cmake ninja-build python3 python3-venv git
```

### Core skills to learn (high ROI for interviews)

1. C++ project workflow basics:
   - CMake presets
   - Build/test cycle (`cmake --preset dev`, `cmake --build --preset dev`, `ctest --preset dev`)
2. C++ correctness fundamentals:
   - value/reference semantics
   - ownership and const-correctness
   - `std::vector`, `std::map`, iteration complexity
3. Domain fundamentals:
   - Level 2 order book model (not Level 3)
   - snapshots vs absolute level updates
   - sequence integrity (duplicate, out-of-order, gap, resync)
   - fixed-point arithmetic for price/qty/fees
4. Testing + quality signals:
   - unit vs integration tests
   - sanitizer purpose (ASan/UBSan)
   - deterministic outputs and reproducibility

## Brief Development Plan (Human vs Agent)

This maps directly to phases in [order-book-plan.md](./order-book-plan.md).

### Human-owned tasks (you should do these)

1. **Phase 0 decisions and semantics sign-off**
   - Approve definitions for snapshot/update/fill/fee/latency terms.
   - Approve strict policy behavior for invalid sequencing and locked/crossed states.
   - Use USD settlement with $0.001 price ticks (0.1 cents), integer quantities, and
     $0.001 notional/fee units throughout the first release.
2. **Interview defense exercises (parallel with Phases 3-6)**
   - By hand, build a tiny book and apply updates.
   - By hand, compute one multi-level IOC fill, VWAP, fee, net cash.
   - Explain why Level 2 cannot model queue position/passive fills.
   - Implement and be ready to explain the core book mutation and IOC walk
     algorithms in [src/order_book.cpp](../src/order_book.cpp) and
     [src/execution.cpp](../src/execution.cpp).
3. **Local operator workflow**
   - Personally run build/test commands at major checkpoints.
   - Read failures and ask agent for targeted fixes.
4. **Final understanding + storytelling**
   - Use completion/understanding gates as checklist.
   - Prepare a short walkthrough: architecture, invariants, error handling, benchmark methodology.

### Agent-owned tasks (delegate implementation)

1. **Phases 1-6 core implementation**
   - Scaffold project, domain types, parser/validator, sequencer, books,
     execution/fees, CLI/reporting, replay plumbing, and JSON output helpers.
   - Leave the core mutation and depth-walk algorithms to the human when they
     want to learn or revise the matching behavior.
2. **Phase 7 data adapter work**
   - Normalize one recorded source into canonical schema, with manifest + reproducible hashes.
3. **Phase 8 benchmark harness**
   - Deterministic workloads, repeated trials, saved raw results and system metadata.
4. **Phase 9 hardening/docs support**
   - Sanitizer runs, additional stress/property tests as needed, finalize technical docs.

### Checkpoint rhythm (time-efficient)

- **Checkpoint A (after Phase 2):** you validate semantics + parser behavior.
- **Checkpoint B (after Phase 4):** you do hand-worked book/update example.
- **Checkpoint C (after Phase 6):** you do hand-worked execution/fee example and CLI walkthrough.
- **Checkpoint D (after Phase 9):** you rehearse interview defense against Understanding Gate items.

If short on time, prioritize Checkpoints B and C; they give the highest interview payoff.

## **Interview Checklist**

- **Domain Model:** Level 2 order book semantics: snapshots vs level updates, price ticks, integer quantities.
- **Fixed-Point Math:** how scaled decimals are represented/parsed and why (see `src/decimal.cpp`).
- **Sequencing Rules:** snapshot requirement, duplicates, conflicting duplicates, gaps, resync; review `tests/sequencer_test.cpp`.
- **Snapshot Validation:** `OrderBook::replace_snapshot` checks and locked/crossed detection (`src/order_book.cpp`).
- **Batched Updates & Atomicity:** `OrderBook::apply_update` validation, `touched` duplicate detection, journaling, rollback on locked/crossed.
- **Lock/Cross Policy:** rationale for detecting and reverting to preserve invariants (`locked_or_crossed`).
- **Missing/Invalid Level Handling:** deletion of missing levels, zero/negative price or quantity errors.
- **Execution Walk & Fills:** IOC depth-walk, fill aggregation, VWAP, and fee application (`src/execution.cpp`).
- **Invariants & Tests:** where unit tests live and how they assert behaviors; run with `ctest --preset dev`.
- **CLI & Replay:** `mdsim` commands (`validate`, `replay`, `quote`, `analyze-pair`) and the replay engine/report writer responsibilities.
- **Edge Cases to Explain:** reverse-order rollback via journal, deterministic outputs for benchmarking, why Level 2 cannot model queue position.
- **Talking Tips:** be ready to step through one snapshot → update → IOC example by hand, and point to `src/order_book.cpp` and `src/execution.cpp` as the two hot spots.

### Files & Lines To Read

- `OrderBook::replace_snapshot`: [src/order_book.cpp](src/order_book.cpp#L53-L67) d
- `OrderBook::apply_update` (validation + duplicate detection): [src/order_book.cpp](src/order_book.cpp#L70-L89) d
- `OrderBook::apply_update` (journaling + mutation): [src/order_book.cpp](src/order_book.cpp#L91-L113) d
- `OrderBook` rollback on locked/crossed: [src/order_book.cpp](src/order_book.cpp#L115-L128) d
- Fixed-point parsing: `parse_scaled_decimal`: [src/decimal.cpp](src/decimal.cpp#L30-L85)
- Fixed-point formatting: `format_scaled_decimal`: [src/decimal.cpp](src/decimal.cpp#L87-L112)
- IOC depth-walk helper `quote_from_levels`: [src/execution.cpp](src/execution.cpp#L36-L62)
- IOC flow and fees: `quote_ioc`: [src/execution.cpp](src/execution.cpp#L66-L105)
- Sequencer behaviors (tests): snapshot/acceptance: [tests/sequencer_test.cpp](tests/sequencer_test.cpp#L35-L41)
- Sequencer behaviors (tests): duplicates/conflicts: [tests/sequencer_test.cpp](tests/sequencer_test.cpp#L43-L55)
- Sequencer behaviors (tests): gap/resync: [tests/sequencer_test.cpp](tests/sequencer_test.cpp#L57-L67)
- CLI arg parsing: [app/main.cpp](app/main.cpp#L54-L82)
- CLI decimal parsing helpers: [app/main.cpp](app/main.cpp#L85-L96)
- Book file loading and snapshot application: [app/main.cpp](app/main.cpp#L155-L206)
- Quote output JSON / VWAP/slippage computation: [app/main.cpp](app/main.cpp#L209-L237)
## Final Human Edit Points

The surrounding scaffolding is now in place. The human should still be ready to
rewrite or refine these core sections when learning, changing behavior, or
adapting the CLI contract:

- [src/order_book.cpp](../src/order_book.cpp): snapshot validation, batched
   update journaling, and locked/crossed handling.
- [src/execution.cpp](../src/execution.cpp): IOC depth walk, fill aggregation,
   fee application, and slippage bookkeeping.
- [app/main.cpp](../app/main.cpp): command-line behavior, argument validation,
   and report routing if the CLI contract changes.

Completed agent-owned surfaces now include the replay engine, report writer,
final-book serialization, and the `mdsim` CLI commands for `validate`,
`replay`, `quote`, and `analyze-pair`.
