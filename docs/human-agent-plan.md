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
2. **Interview defense exercises (parallel with Phases 3-6)**
   - By hand, build a tiny book and apply updates.
   - By hand, compute one multi-level IOC fill, VWAP, fee, net cash.
   - Explain why Level 2 cannot model queue position/passive fills.
3. **Local operator workflow**
   - Personally run build/test commands at major checkpoints.
   - Read failures and ask agent for targeted fixes.
4. **Final understanding + storytelling**
   - Use completion/understanding gates as checklist.
   - Prepare a short walkthrough: architecture, invariants, error handling, benchmark methodology.

### Agent-owned tasks (delegate implementation)

1. **Phases 1-6 core implementation**
   - Scaffold project, domain types, parser/validator, sequencer, books, execution/fees, CLI/reporting.
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
