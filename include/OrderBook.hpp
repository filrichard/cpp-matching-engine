#pragma once

#include "Order.hpp"
#include "Trade.hpp"
#include "Types.hpp"

#include <cstddef>
#include <map>
#include <memory_resource>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace matching_engine
{

/*
Owns resting orders on both sides, matches incoming orders
against them using price-time priority and reports the
resulting trades. A single OrderBook is expected to be
driven by one thread (or externally synchronized).
Owned by MatchingEngine
*/
class OrderBook
{
private:
    /*
    One price level. Orders in strict FIFO (time-priority) order, plus
    a running total so depth queries don't need to walk the queue.
    */
    struct PriceLevel
    {
        struct OrderNode
        {
            explicit OrderNode( Order order ) : order( std::move( order ) ) {}

            Order order;
            OrderNode* previous = nullptr;
            OrderNode* next = nullptr;
        };

        OrderNode* head = nullptr;
        OrderNode* tail = nullptr;
        Quantity totalQuantity = 0;
        std::size_t orderCount = 0;

        bool empty() const noexcept { return head == nullptr; }

        void pushBack( OrderNode* node ) noexcept;
        void unlink( OrderNode* node ) noexcept;
    };

    /*
    Where a resting order lives, so cancelOrder() doesn't need to
    search either side of the book. The node address stays valid for
    the order's entire lifetime in the order-node pool.
    Makes cancellation O(1), aside from the O(log n) price-level lookup/erase
    */
    struct OrderLocation
    {
        Side side;
        Price price;
        PriceLevel::OrderNode* node;
    };

    // Bids sorted best-first (highest price) so begin() is always top-of-book
    using BidMap = std::map< Price, PriceLevel, std::greater< Price > >;
    // Asks sorted best-first (lowest price) so begin() is always top-of-book
    using AskMap = std::map< Price, PriceLevel, std::less< Price >>;

    /*
    Matches `incoming` against the given resting side, filling both
    sides in place, appending resulting Trades, and removing/shrinking
    resting orders/levels as they're consumed. Stops when `incoming`
    is fully filled or no more matchable price levels remain.

    TODO: self-trade prevention. decide on a policy:
     - skip-and-continue
     - reject
     - cancel-both
    before deploying in multi-client setting
    */
    void matchAgainstBids( Order& incoming, std::vector< Trade >& trades );
    void matchAgainstAsks( Order& incoming, std::vector< Trade >& trades );

    /*
    True if the opposite side currently has enough matchable quantity
    at an acceptable price, to fully fill `order` right now. Used to
    implement Fill-Or-Kill: must be checked before any fills happen
    */
    bool canFullyFill( const Order& order ) const;

    /*
    Adds a partially or fully unfilled order to the appropriate side
    of the book. Caller is responsible for having already decided the
    order should rest
    */
    void restOrder( Order order );
    PriceLevel::OrderNode* allocateNode( Order order );
    void releaseNode( PriceLevel::OrderNode* node ) noexcept;
    void clearOrders() noexcept;

    std::string symbol_;
    BidMap bids_;
    AskMap asks_;
    std::unordered_map< OrderId, OrderLocation > order_locations_;
    std::pmr::unsynchronized_pool_resource order_node_pool_;

public:
    // Complete outcome of processing one incoming order. The order itself is
    // not retained when it does not rest, so callers need this snapshot to
    // distinguish a fill, rejection, cancellation, or resting remainder.
    struct AddOrderResult
    {
        OrderStatus status;
        Quantity filledQuantity;
        Quantity remainingQuantity;
        std::vector< Trade > trades;
    };

    /*
    Snapshot of one aggregated price level, for depth/market-data
    queries. Does not expose individual resting orders
    */
    struct PriceLevelInfo
    {
        Price price;
        Quantity totalQuantity;
        std::size_t orderCount;
    };

    explicit OrderBook( std::string symbol );
    ~OrderBook();

    OrderBook( const OrderBook& ) = delete;
    OrderBook& operator=( const OrderBook& ) = delete;
    OrderBook( OrderBook&& ) = delete;
    OrderBook& operator=( OrderBook&& ) = delete;

    /*
    Submits a new order. Attempts to match it immediately against the
    opposite side of the book; what happens to any unmatched
    remainder depends on the order's type and TimeInForce:
      - GTC / Day (Limit): remainder rests on the book.
      - IOC:                remainder is discarded, never rests.
      - FOK:                if the order cannot be fully filled
                            immediately, nothing is matched at all
                            (all-or-nothing) and no trades occur.
      - Market:             matches whatever liquidity exists;
                            remainder is discarded (market orders
                            never rest, since they carry no price).
    Returns the final order state, quantities, and trades generated by this
    call. Trades are in execution order and may be empty.
    */
    AddOrderResult executeOrder( Order order );

    // Convenience API retained for callers interested only in executions.
    std::vector< Trade > addOrder( Order order );

    /*
    Cancels resting order by id. Returns false if
    no active order with that id exists on this book
    */
    bool cancelOrder( OrderId id );

    std::optional< Price > bestBid() const;
    std::optional< Price > bestAsk() const;
    std::optional< Price > spread() const;

    // Aggregated depth, best price first, up to `depth` levels per side
    std::vector< PriceLevelInfo > bidDepth( std::size_t depth ) const;
    std::vector< PriceLevelInfo > askDepth( std::size_t depth ) const;

    const std::string& symbol() const noexcept { return symbol_; }
    bool empty() const noexcept { return bids_.empty() && asks_.empty(); }
    std::size_t activeOrderCount() const noexcept { return order_locations_.size(); }
};

}
