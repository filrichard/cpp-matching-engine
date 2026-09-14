# C++ Matching Engine

A price-time-priority limit order book matching engine, written in C++23.

[![Ubuntu CI](https://github.com/filrichard/cpp-matching-engine/actions/workflows/ci-ubuntu.yml/badge.svg)](https://github.com/filrichard/cpp-matching-engine/actions/workflows/ci-ubuntu.yml)
[![macOS CI](https://github.com/filrichard/cpp-matching-engine/actions/workflows/ci-macos.yml/badge.svg)](https://github.com/filrichard/cpp-matching-engine/actions/workflows/ci-macos.yml)
[![Windows CI](https://github.com/filrichard/cpp-matching-engine/actions/workflows/ci-windows.yml/badge.svg)](https://github.com/filrichard/cpp-matching-engine/actions/workflows/ci-windows.yml)

## Overview

This engine matches buy and sell orders for one or more instruments using standard price-time priority: orders at a better price match first, and orders at the same price match in the order they arrived. It supports:

- **Order types**: Limit and Market
- **Time in force**: `GTC`, `IOC`, `FOK`, `Day`¹
- **Multi-symbol routing** through a single `MatchingEngine`, with per-symbol order books
- **Trade reporting** via return values and an optional callback hook
- **Depth / top-of-book queries** for market data consumers

¹ `Day` is currently treated identically to `GTC` — there's no end-of-day expiry mechanism yet. See [Known Limitations](#known-limitations).

## Architecture

```
include/
├── Types.hpp          # Shared aliases (OrderId, Price, Quantity, ...) and enums
├── Order.hpp           # Order entity — header-only, mutable within controlled bounds
├── Trade.hpp            # Trade entity — header-only, fully immutable
├── OrderBook.hpp        # Single-instrument book: matching, resting, cancellation
└── MatchingEngine.hpp   # Multi-symbol routing, order id assignment, trade callback

src/
├── OrderBook.cpp        # Matching algorithm implementation
├── MatchingEngine.cpp    # Routing implementation
└── main.cpp             # Manual smoke-test / demo entry point
```

`Order` and `Trade` are header-only — both are simple enough that every method is trivial to define inline in the class body, so there's no corresponding `.cpp` for either.

### Data structures

Each `OrderBook` holds two sides:

```cpp
std::map< Price, PriceLevel, std::greater< Price > > bids_;  // best (highest) price first
std::map< Price, PriceLevel, std::less< Price > >    asks_;  // best (lowest) price first
```

where each `PriceLevel` is a `std::list<Order>` in strict FIFO order (time priority within a price). A side index —

```cpp
std::unordered_map< OrderId, OrderLocation > order_locations_;
```

— maps an `OrderId` directly to its side, price, and `std::list` iterator, so `cancelOrder()` doesn't need to search either side of the book. This works because `std::list` iterators stay valid for the lifetime of the element, even as other orders are inserted or removed elsewhere in the list.

This isn't the fastest possible structure — an intrusive/lock-free layout would beat it — but it's a deliberate tradeoff: `O(log n)` insert, `O(1)` cancel (plus `O(log n)` if a price level empties out), and code that stays easy to reason about. See [Benchmarks](#benchmarks) for what this actually costs in practice.

### Key design decisions

- **Fixed-point integer prices** (`int64_t` ticks), not floating point — avoids rounding/comparison bugs in matching logic. Tick-to-decimal conversion is left to the I/O layer.
- **Trade prints at the resting (passive) order's price**, not the aggressor's — the standard exchange convention.
- **`Trade` is fully immutable** — every field fixed at construction, no setters. It's a historical fact, not a stateful entity.
- **`MatchingEngine` assigns `OrderId`s**, not the caller — mirrors how real venues work, and guarantees uniqueness without requiring caller coordination. `OrderBook::addOrder()` itself now defensively rejects a duplicate, still-active `OrderId` (see [Known Limitations](#known-limitations) for how this was found).
- **`cancelOrder()` requires the symbol as well as the id**, rather than the engine maintaining an internal `OrderId → symbol` index — avoids a second source of truth that would need to stay in sync with each book's own bookkeeping.
- **No implicit book creation.** Submitting an order for an unregistered symbol throws rather than silently creating one.

## Known Limitations

These are open items, not oversights — each was a deliberate scoping decision along the way:

- **No self-trade prevention.** A resting order and an incoming order from the same `ClientId` can currently match each other.
- **`Day` time-in-force doesn't actually expire** — treated identically to `GTC`. No end-of-day sweep exists yet.
- **No order modification / cancel-replace.** Changing price or quantity currently means cancel-then-resubmit, which loses time priority (arguably correct behavior for a price change, debatable for a pure quantity increase).
- **No stop / stop-limit orders** — `OrderType` currently only has `Limit` and `Market`.
- **Not thread-safe.** `OrderId` assignment is atomic, but nothing synchronizes concurrent access to a given `OrderBook`. This is a deliberate omission, not an oversight — the right locking granularity depends on how you intend to drive the engine.
- **Symbol is stored as `std::string` on every `Order`**, not interned into a lightweight `SymbolId`. Fine at this scale; would be worth revisiting if profiling ever showed string comparisons in a hot path.
- **`OrderBook::addOrder()` now rejects a reused, still-active `OrderId`** (throws `std::invalid_argument`) rather than allowing it to silently corrupt the cancel index. This was found via a bug in a benchmark harness, not user-facing code, but the underlying gap was real — see `OrderBook.cpp` for details.

## Getting Started

### Prerequisites

- CMake 3.25+
- A C++23 compiler: GCC 13+, Clang 17+, AppleClang 15+, or MSVC 19.34+ (Visual Studio 2022 17.4+)
- Catch2
- Google Benchmark

### Build

This project uses [CMake Presets](https://cmake.org/cmake/help/latest/manual/cmake-presets.7.html) to avoid a common footgun: a plain `-DCMAKE_BUILD_TYPE=Release` doesn't reliably produce an optimized build. It has no effect at all on multi-config generators (Visual Studio, Xcode), and on single-config generators it's silently ignored if the build directory already has a different build type cached — which is exactly what happens if an IDE previously configured it as Debug. The presets sidestep both problems by giving `debug` and `release` their own dedicated, pre-pinned build directories.

```bash
git clone <this-repo>
cd cpp-matching-engine

cmake --preset release
cmake --build --preset release
```

This builds three targets: `matching_engine_app` (a small demo), `OrderBookTests` (the test suite), and `OrderBookBenchmarks`.

### Run the demo

```bash
./build/release/matching_engine_app
```

Registers a symbol, rests a couple of sell orders, submits a crossing buy that sweeps both, and prints the resulting trades and final book state.

## Testing

```bash
ctest --preset release
```

25 test cases (Catch2 v3, fetched via `FetchContent`) covering:

- Basic matching: resting, full/partial fills, price-time (FIFO) priority, multi-level sweeps
- Limit orders not matching through their own price limit
- Time-in-force: IOC partial-fill-and-discard, FOK all-or-nothing (both reject and success paths)
- Market orders: sweeping liquidity, never resting a leftover
- Cancellation: normal cancel, cancelling an unknown/already-inactive order, cancelling one order without disturbing others at the same price level
- Duplicate-`OrderId` rejection (regression test — see [Known Limitations](#known-limitations))
- Depth/top-of-book queries and spread calculation
- `MatchingEngine` routing, id assignment, unregistered-symbol rejection, and the trade callback

## Benchmarks

```bash
./build/release/benchmarks/OrderBookBenchmarks --benchmark_repetitions=10 --benchmark_report_aggregates_only=true
```

Built with [Google Benchmark](https://github.com/google/benchmark) (also fetched via `FetchContent`). Five benchmarks isolate different parts of the hot path:

| Benchmark | What it isolates |
|---|---|
| `RestingInsert` (depth 10 → 10,000) | Cost of adding a new, non-crossing price level — `std::map` node allocation + insert |
| `SingleLevelMatch` | Pure matching-loop + `Trade` construction cost, no map/list mutation |
| `SweepLevels` (1 → 1,000 levels) | Cost of one aggressive order consuming N price levels — the worst-case latency path |
| `Cancel` | `unordered_map` lookup + `list::erase` + level cleanup |
| `MatchingEngine_SubmitNonCrossing` | Routing/id-assignment overhead on top of raw `OrderBook::addOrder()` |

### Sample results

Measured on a 10-core Apple Silicon Mac (M1 Pro), 10 repetitions, coefficient of variation under 2% on every benchmark:

| Benchmark | Median |
|---|---|
| `SingleLevelMatch` | **~55 ns** |
| `RestingInsert` (depth 10) | ~605 ns |
| `RestingInsert` (depth 10,000) | ~615 ns |
| `SweepLevels` (100 levels) | ~7.3 µs |
| `SweepLevels` (1,000 levels) | ~72.7 µs |
| `Cancel` | ~601 ns |
| `MatchingEngine::submitLimitOrder` (non-crossing) | ~606 ns |

A few things worth noting about these numbers:

- **Resting a new order costs ~11x more than matching against an existing one** (~605 ns vs. ~55 ns). That gap is allocation, not tree traversal: a crossing match just decrements an existing order's quantity in place, while a non-crossing insert allocates a fresh `std::map` node *and* a fresh `std::list` node.
- **The O(log n) insert cost is visible, but small**: depth 10 → 10,000 is a ~10 ns increase, consistent with ~10 extra tree comparisons (log₂(10,000) − log₂(10) ≈ 10) at roughly 1 ns each — the rest of the ~605 ns floor is the allocation cost above.
- **`SweepLevels` scales close to linearly at larger n**: 100 → 1,000 levels is almost exactly a 10x time increase. Backing out the fixed per-call overhead gives a marginal cost of ~73 ns per additional level swept — close to, and just above, the standalone `SingleLevelMatch` cost, with the difference being the extra `std::map::erase()` each fully-consumed level pays.
- **`MatchingEngine`'s routing/id-assignment layer adds only ~1 ns** over calling `OrderBook::addOrder()` directly.

**Caveats, stated plainly:** these numbers come from a laptop that wasn't fully idle (load average 5–8 on 10 cores during the run) and macOS's thread-affinity pinning doesn't work from user space, so a stray scheduler migration to an efficiency core can skew any individual run. They're internally consistent (tight coefficient of variation) and useful for relative comparisons and understanding where time goes — but they are **not** a controlled, reproducible benchmark-lab result, and they are **not** competitive with real HFT engines, which target tens of nanoseconds via intrusive/lock-free structures, custom allocators, and kernel-bypass networking — well outside this project's scope.
