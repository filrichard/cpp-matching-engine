#include <benchmark/benchmark.h>

#include "MatchingEngine.hpp"
#include "Order.hpp"
#include "OrderBook.hpp"
#include "Trade.hpp"

using namespace matching_engine;

namespace {
constexpr ClientId kClient = 1;
} // namespace

// ── Resting insert ───────────────────────────────────────────────────────
// Cost of adding a new, non-crossing order to the book, as a function of
// existing book depth. The book is pre-populated with `depth` resting
// sell orders; each iteration inserts one more order at a brand-new
// price (so it always pays a fresh std::map node insertion, not a
// push_back into an existing level's list), then removes it again
// outside the timed region so depth stays constant across iterations.
//
// Caveat: PauseTiming()/ResumeTiming() have their own (small) overhead,
// which is included in every iteration here. That adds a roughly
// constant bias to the absolute numbers, but shouldn't distort the
// *relative* scaling across depths, which is really what this
// benchmark is trying to show (O(log n) insertion cost).
static void BM_OrderBook_RestingInsert(benchmark::State& state) {
    const int depth = static_cast<int>(state.range(0));
    OrderBook book("BENCH");

    OrderId id = 1;
    for (int i = 0; i < depth; ++i) {
        book.addOrder(Order::makeLimit(id++, kClient, "BENCH", Side::Sell,
                                        100'000 + i, 10));
    }

    Price probe_price = 1;
    for (auto _ : state) {
        auto trades = book.addOrder(Order::makeLimit(id, kClient, "BENCH",
                                                       Side::Sell, probe_price, 10));
        benchmark::DoNotOptimize(trades);

        state.PauseTiming();
        book.cancelOrder(id);
        ++id;
        ++probe_price;
        state.ResumeTiming();
    }

    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_OrderBook_RestingInsert)->Arg(10)->Arg(100)->Arg(1'000)->Arg(10'000);

// ── Single-level match ──────────────────────────────────────────────────
// One large resting order absorbs every aggressor without ever being
// fully consumed, isolating the matching loop + Trade construction cost
// from any std::map insert/erase overhead.
static void BM_OrderBook_SingleLevelMatch(benchmark::State& state) {
    OrderBook book("BENCH");
    book.addOrder(Order::makeLimit(1, kClient, "BENCH", Side::Sell, 100,
                                    1'000'000'000ULL));

    OrderId next_id = 2;
    for (auto _ : state) {
        auto trades = book.addOrder(Order::makeLimit(next_id++, kClient, "BENCH",
                                                       Side::Buy, 100, 1));
        benchmark::DoNotOptimize(trades);
    }

    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_OrderBook_SingleLevelMatch);

// ── Sweep ────────────────────────────────────────────────────────────────
// One aggressive order consumes `levels` distinct resting price levels in
// a single call — the worst-case latency path for a single addOrder().
static void BM_OrderBook_SweepLevels(benchmark::State& state) {
    const int levels = static_cast<int>(state.range(0));
    OrderBook book("BENCH");

    // A single shared counter for every order issued below — resting
    // replenishment and the sweeping aggressor alike — so ids can never
    // collide between the two. (An earlier version of this benchmark
    // used two independently-incrementing counters that happened to
    // start from the same value; after the first replenish, a resting
    // order and the next aggressor ended up with the same OrderId, and
    // Trade's own-order-id check correctly caught it.)
    OrderId next_id = 1;
    auto populate = [&] {
        for (int i = 0; i < levels; ++i) {
            book.addOrder(Order::makeLimit(next_id++, kClient, "BENCH", Side::Sell,
                                            100 + i, 1));
        }
    };
    populate();

    for (auto _ : state) {
        auto trades = book.addOrder(Order::makeLimit(next_id++, kClient, "BENCH",
                                                       Side::Buy, 100 + levels - 1, levels));
        benchmark::DoNotOptimize(trades);

        state.PauseTiming();
        populate(); // rebuild what the sweep just consumed
        state.ResumeTiming();
    }

    state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(levels));
}
BENCHMARK(BM_OrderBook_SweepLevels)->Arg(1)->Arg(10)->Arg(100)->Arg(1'000);

// ── Cancel ───────────────────────────────────────────────────────────────
// Cost of removing a resting order by id: an unordered_map lookup, a
// std::list erase, and (since each order here is alone at its price) a
// std::map level erase.
static void BM_OrderBook_Cancel(benchmark::State& state) {
    OrderBook book("BENCH");
    OrderId id = 1;
    Price price = 1;

    for (auto _ : state) {
        state.PauseTiming();
        book.addOrder(Order::makeLimit(id, kClient, "BENCH", Side::Buy, price++, 10));
        state.ResumeTiming();

        bool cancelled = book.cancelOrder(id);
        benchmark::DoNotOptimize(cancelled);
        ++id;
    }
}
BENCHMARK(BM_OrderBook_Cancel);

// ── Engine routing overhead ───────────────────────────────────────────────
// Same non-crossing insert workload as BM_OrderBook_RestingInsert, but
// through MatchingEngine::submitLimitOrder, to isolate what the engine's
// symbol lookup + OrderId assignment costs on top of OrderBook itself.
static void BM_MatchingEngine_SubmitNonCrossing(benchmark::State& state) {
    MatchingEngine engine;
    engine.addSymbol("BENCH");

    Price probe_price = 1;
    for (auto _ : state) {
        auto result = engine.submitLimitOrder(kClient, "BENCH", Side::Sell,
                                               probe_price, 10);
        benchmark::DoNotOptimize(result);

        state.PauseTiming();
        engine.cancelOrder("BENCH", result.orderId);
        ++probe_price;
        state.ResumeTiming();
    }

    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_MatchingEngine_SubmitNonCrossing);

BENCHMARK_MAIN();
