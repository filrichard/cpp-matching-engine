#include <catch2/catch_test_macros.hpp>

#include "MatchingEngine.hpp"
#include "Order.hpp"
#include "OrderBook.hpp"
#include "Trade.hpp"

using namespace matching_engine;

namespace
{
constexpr ClientId kClientA = 1;
constexpr ClientId kClientB = 2;
constexpr ClientId kClientC = 3;
}   // namespace

// ── Basic matching ─────────────────────────────────────────────────────

TEST_CASE ( "OrderBook: resting limit order with no match sits on the book", "[orderbook]" )
{
    OrderBook book ( "TEST" );

    auto trades = book.addOrder ( Order::makeLimit ( 1, kClientA, "TEST", Side::Buy, 100, 10 ) );

    CHECK ( trades.empty() );
    REQUIRE ( book.bestBid().has_value() );
    CHECK ( *book.bestBid() == 100 );
    CHECK_FALSE ( book.bestAsk().has_value() );
    CHECK ( book.activeOrderCount() == 1 );
}

TEST_CASE ( "OrderBook: a crossing limit order fully fills the resting order", "[orderbook]" )
{
    OrderBook book ( "TEST" );
    book.addOrder ( Order::makeLimit ( 1, kClientA, "TEST", Side::Sell, 100, 10 ) );

    auto trades = book.addOrder ( Order::makeLimit ( 2, kClientB, "TEST", Side::Buy, 100, 10 ) );

    REQUIRE ( trades.size() == 1 );
    const Trade& trade = trades.front();
    CHECK ( trade.price() == 100 );
    CHECK ( trade.quantity() == 10 );
    CHECK ( trade.buyOrderId() == 2 );
    CHECK ( trade.sellOrderId() == 1 );
    CHECK ( trade.aggressorSide() == Side::Buy );

    CHECK ( book.empty() );
    CHECK ( book.activeOrderCount() == 0 );
}

TEST_CASE ( "OrderBook: partial fill leaves the remainder resting", "[orderbook]" )
{
    OrderBook book ( "TEST" );
    book.addOrder ( Order::makeLimit ( 1, kClientA, "TEST", Side::Sell, 100, 5 ) );

    auto trades = book.addOrder ( Order::makeLimit ( 2, kClientB, "TEST", Side::Buy, 100, 10 ) );

    REQUIRE ( trades.size() == 1 );
    CHECK ( trades.front().quantity() == 5 );

    CHECK_FALSE ( book.bestAsk().has_value() );
    REQUIRE ( book.bestBid().has_value() );
    CHECK ( *book.bestBid() == 100 );

    auto depth = book.bidDepth ( 5 );
    REQUIRE ( depth.size() == 1 );
    CHECK ( depth.front().totalQuantity == 5 );
}

TEST_CASE ( "OrderBook: trade prints at the resting order's price, not the aggressor's",
            "[orderbook]" )
{
    OrderBook book ( "TEST" );
    book.addOrder ( Order::makeLimit ( 1, kClientA, "TEST", Side::Sell, 95, 10 ) );

    auto trades = book.addOrder ( Order::makeLimit ( 2, kClientB, "TEST", Side::Buy, 100, 10 ) );

    REQUIRE ( trades.size() == 1 );
    CHECK ( trades.front().price() == 95 );
}

TEST_CASE ( "OrderBook: time priority is FIFO within a price level", "[orderbook]" )
{
    OrderBook book ( "TEST" );
    book.addOrder ( Order::makeLimit ( 1, kClientA, "TEST", Side::Sell, 100, 5 ) );
    book.addOrder ( Order::makeLimit ( 2, kClientB, "TEST", Side::Sell, 100, 5 ) );

    auto trades = book.addOrder ( Order::makeLimit ( 3, kClientC, "TEST", Side::Buy, 100, 5 ) );

    REQUIRE ( trades.size() == 1 );
    CHECK ( trades.front().sellOrderId() == 1 );

    auto depth = book.askDepth ( 5 );
    REQUIRE ( depth.size() == 1 );
    CHECK ( depth.front().totalQuantity == 5 );
}

TEST_CASE ( "OrderBook: a large aggressive order sweeps multiple price levels", "[orderbook]" )
{
    OrderBook book ( "TEST" );
    book.addOrder ( Order::makeLimit ( 1, kClientA, "TEST", Side::Sell, 100, 5 ) );
    book.addOrder ( Order::makeLimit ( 2, kClientA, "TEST", Side::Sell, 101, 5 ) );
    book.addOrder ( Order::makeLimit ( 3, kClientA, "TEST", Side::Sell, 102, 5 ) );

    auto trades = book.addOrder ( Order::makeLimit ( 4, kClientB, "TEST", Side::Buy, 102, 15 ) );

    REQUIRE ( trades.size() == 3 );
    CHECK ( trades[0].price() == 100 );
    CHECK ( trades[1].price() == 101 );
    CHECK ( trades[2].price() == 102 );
    CHECK ( book.empty() );
}

TEST_CASE ( "OrderBook: limit order does not match through its own price limit", "[orderbook]" )
{
    OrderBook book ( "TEST" );
    book.addOrder ( Order::makeLimit ( 1, kClientA, "TEST", Side::Sell, 105, 10 ) );

    auto trades = book.addOrder ( Order::makeLimit ( 2, kClientB, "TEST", Side::Buy, 100, 10 ) );

    CHECK ( trades.empty() );
    REQUIRE ( book.bestBid().has_value() );
    CHECK ( *book.bestBid() == 100 );
    REQUIRE ( book.bestAsk().has_value() );
    CHECK ( *book.bestAsk() == 105 );
}

// ── Time-in-force ───────────────────────────────────────────────────────

TEST_CASE ( "OrderBook: IOC order fills what it can and discards the remainder",
            "[orderbook][tif]" )
{
    OrderBook book ( "TEST" );
    book.addOrder ( Order::makeLimit ( 1, kClientA, "TEST", Side::Sell, 100, 5 ) );

    auto trades = book.addOrder (
        Order::makeLimit ( 2, kClientB, "TEST", Side::Buy, 100, 10, TimeInForce::IOC ) );

    REQUIRE ( trades.size() == 1 );
    CHECK ( trades.front().quantity() == 5 );
    CHECK_FALSE ( book.bestBid().has_value() );
    CHECK ( book.empty() );
}

TEST_CASE ( "OrderBook: FOK order is rejected outright if it can't be fully filled",
            "[orderbook][tif]" )
{
    OrderBook book ( "TEST" );
    book.addOrder ( Order::makeLimit ( 1, kClientA, "TEST", Side::Sell, 100, 5 ) );

    auto trades = book.addOrder (
        Order::makeLimit ( 2, kClientB, "TEST", Side::Buy, 100, 10, TimeInForce::FOK ) );

    CHECK ( trades.empty() );
    REQUIRE ( book.bestAsk().has_value() );
    CHECK ( *book.bestAsk() == 100 );
    auto depth = book.askDepth ( 5 );
    REQUIRE ( depth.size() == 1 );
    CHECK ( depth.front().totalQuantity == 5 );
}

TEST_CASE ( "OrderBook: FOK order matches fully when liquidity is sufficient", "[orderbook][tif]" )
{
    OrderBook book ( "TEST" );
    book.addOrder ( Order::makeLimit ( 1, kClientA, "TEST", Side::Sell, 100, 5 ) );
    book.addOrder ( Order::makeLimit ( 2, kClientA, "TEST", Side::Sell, 101, 10 ) );

    auto trades = book.addOrder (
        Order::makeLimit ( 3, kClientB, "TEST", Side::Buy, 101, 10, TimeInForce::FOK ) );

    REQUIRE ( trades.size() == 2 );
    CHECK ( trades[0].quantity() == 5 );
    CHECK ( trades[1].quantity() == 5 );
    REQUIRE ( book.bestAsk().has_value() );
    CHECK ( *book.bestAsk() == 101 );
}

// ── Market orders ───────────────────────────────────────────────────────

TEST_CASE ( "OrderBook: market order matches without needing a price and never rests",
            "[orderbook]" )
{
    OrderBook book ( "TEST" );
    book.addOrder ( Order::makeLimit ( 1, kClientA, "TEST", Side::Sell, 100, 5 ) );
    book.addOrder ( Order::makeLimit ( 2, kClientA, "TEST", Side::Sell, 105, 20 ) );

    auto trades = book.addOrder ( Order::makeMarket ( 3, kClientB, "TEST", Side::Buy, 10 ) );

    REQUIRE ( trades.size() == 2 );
    CHECK ( trades[0].price() == 100 );
    CHECK ( trades[0].quantity() == 5 );
    CHECK ( trades[1].price() == 105 );
    CHECK ( trades[1].quantity() == 5 );
    REQUIRE ( book.bestAsk().has_value() );
    CHECK ( *book.bestAsk() == 105 );
}

TEST_CASE (
    "OrderBook: market order with insufficient liquidity fills what exists and discards the rest",
    "[orderbook]" )
{
    OrderBook book ( "TEST" );
    book.addOrder ( Order::makeLimit ( 1, kClientA, "TEST", Side::Sell, 100, 5 ) );

    auto trades = book.addOrder ( Order::makeMarket ( 2, kClientB, "TEST", Side::Buy, 100 ) );

    REQUIRE ( trades.size() == 1 );
    CHECK ( trades.front().quantity() == 5 );
    CHECK ( book.empty() );
}

// ── Cancellation ────────────────────────────────────────────────────────

TEST_CASE ( "OrderBook: cancelling a resting order removes it from the book",
            "[orderbook][cancel]" )
{
    OrderBook book ( "TEST" );
    book.addOrder ( Order::makeLimit ( 1, kClientA, "TEST", Side::Buy, 100, 10 ) );

    CHECK ( book.cancelOrder ( 1 ) );
    CHECK ( book.empty() );
    CHECK ( book.activeOrderCount() == 0 );
}

TEST_CASE ( "OrderBook: cancelling an unknown or already-inactive order returns false",
            "[orderbook][cancel]" )
{
    OrderBook book ( "TEST" );

    CHECK_FALSE ( book.cancelOrder ( 999 ) );

    book.addOrder ( Order::makeLimit ( 1, kClientA, "TEST", Side::Buy, 100, 10 ) );
    CHECK ( book.cancelOrder ( 1 ) );
    CHECK_FALSE ( book.cancelOrder ( 1 ) );
}

TEST_CASE ( "OrderBook: cancelling one order at a price level leaves the others intact",
            "[orderbook][cancel]" )
{
    OrderBook book ( "TEST" );
    book.addOrder ( Order::makeLimit ( 1, kClientA, "TEST", Side::Buy, 100, 5 ) );
    book.addOrder ( Order::makeLimit ( 2, kClientB, "TEST", Side::Buy, 100, 7 ) );

    CHECK ( book.cancelOrder ( 1 ) );

    auto depth = book.bidDepth ( 5 );
    REQUIRE ( depth.size() == 1 );
    CHECK ( depth.front().totalQuantity == 7 );
    CHECK ( depth.front().orderCount == 1 );
}

// ── Depth / top of book ─────────────────────────────────────────────────

TEST_CASE ( "OrderBook: depth snapshot reports best-first aggregated levels", "[orderbook][depth]" )
{
    OrderBook book ( "TEST" );
    book.addOrder ( Order::makeLimit ( 1, kClientA, "TEST", Side::Buy, 99, 5 ) );
    book.addOrder ( Order::makeLimit ( 2, kClientA, "TEST", Side::Buy, 100, 3 ) );
    book.addOrder ( Order::makeLimit ( 3, kClientA, "TEST", Side::Buy, 100, 4 ) );

    auto depth = book.bidDepth ( 10 );

    REQUIRE ( depth.size() == 2 );
    CHECK ( depth[0].price == 100 );
    CHECK ( depth[0].totalQuantity == 7 );
    CHECK ( depth[0].orderCount == 2 );
    CHECK ( depth[1].price == 99 );
    CHECK ( depth[1].totalQuantity == 5 );
}

TEST_CASE ( "OrderBook: spread reflects best bid and best ask", "[orderbook][depth]" )
{
    OrderBook book ( "TEST" );
    CHECK_FALSE ( book.spread().has_value() );

    book.addOrder ( Order::makeLimit ( 1, kClientA, "TEST", Side::Buy, 98, 5 ) );
    book.addOrder ( Order::makeLimit ( 2, kClientB, "TEST", Side::Sell, 102, 5 ) );

    REQUIRE ( book.spread().has_value() );
    CHECK ( *book.spread() == 4 );
}

// ── MatchingEngine ──────────────────────────────────────────────────────

TEST_CASE ( "MatchingEngine: submitting to an unregistered symbol throws", "[engine]" )
{
    MatchingEngine engine;
    CHECK_THROWS_AS ( engine.submitLimitOrder ( kClientA, "MSFT", Side::Buy, 100, 10 ),
                      std::invalid_argument );
}

TEST_CASE ( "MatchingEngine: registering the same symbol twice throws", "[engine]" )
{
    MatchingEngine engine;
    engine.addSymbol ( "AAPL" );
    CHECK_THROWS_AS ( engine.addSymbol ( "AAPL" ), std::invalid_argument );
}

TEST_CASE ( "MatchingEngine: submitted orders are assigned distinct, increasing ids", "[engine]" )
{
    MatchingEngine engine;
    engine.addSymbol ( "AAPL" );

    auto first = engine.submitLimitOrder ( kClientA, "AAPL", Side::Buy, 100, 10 );
    auto second = engine.submitLimitOrder ( kClientA, "AAPL", Side::Buy, 100, 10 );

    CHECK ( second.orderId > first.orderId );
}

TEST_CASE ( "MatchingEngine: routes orders to the correct symbol's book", "[engine]" )
{
    MatchingEngine engine;
    engine.addSymbol ( "AAPL" );
    engine.addSymbol ( "MSFT" );

    engine.submitLimitOrder ( kClientA, "AAPL", Side::Buy, 100, 10 );
    engine.submitLimitOrder ( kClientB, "MSFT", Side::Sell, 200, 5 );

    REQUIRE ( engine.book ( "AAPL" ) != nullptr );
    REQUIRE ( engine.book ( "MSFT" ) != nullptr );
    CHECK ( engine.book ( "AAPL" )->bestBid().value() == 100 );
    CHECK ( engine.book ( "MSFT" )->bestAsk().value() == 200 );
}

TEST_CASE ( "MatchingEngine: a crossing order produces trades in the result and via the callback",
            "[engine]" )
{
    MatchingEngine engine;
    engine.addSymbol ( "AAPL" );

    std::vector< Trade > observed;
    engine.setTradeCallback ( [&observed] ( const Trade& trade ) {
        observed.push_back ( trade );
    } );

    engine.submitLimitOrder ( kClientA, "AAPL", Side::Sell, 100, 10 );
    auto result = engine.submitLimitOrder ( kClientB, "AAPL", Side::Buy, 100, 10 );

    REQUIRE ( result.trades.size() == 1 );
    REQUIRE ( observed.size() == 1 );
    CHECK ( observed.front().id() == result.trades.front().id() );
}

TEST_CASE ( "MatchingEngine: cancelOrder requires the correct symbol", "[engine][cancel]" )
{
    MatchingEngine engine;
    engine.addSymbol ( "AAPL" );
    engine.addSymbol ( "MSFT" );

    auto submitted = engine.submitLimitOrder ( kClientA, "AAPL", Side::Buy, 100, 10 );

    CHECK_FALSE ( engine.cancelOrder ( "MSFT", submitted.orderId ) );
    CHECK_FALSE ( engine.cancelOrder ( "GOOG", submitted.orderId ) );
    CHECK ( engine.cancelOrder ( "AAPL", submitted.orderId ) );
}
