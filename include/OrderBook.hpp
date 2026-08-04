#pragma once

#include "Order.hpp"
#include "Trade.hpp"
#include "Types.hpp"

#include <cstddef>
#include <list>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace matching_engine
{

class OrderBook
{
private:
    struct PriceLevel
    {
        std::list< Order > orders;
        Quantity totalQuantity = 0;
    };

    struct OrderLocation
    {
        Side side;
        Price price;
        std::list< Order >::iterator it;
    };

    using BidMap = std::map< Price, PriceLevel, std::greater< Price > >;
    using AskMap = std::map< Price, PriceLevel, std::less< Price >>;

    void matchAgainstBids( Order& incoming, std::vector< Trade >& trades );
    void matchAgainstAsks( Order& incoming, std::vector< Trade >& trades );

    bool canFullyFill( const Order& order ) const;

    void restOrder( Order order );

    std::string symbol_;
    BidMap bids_;
    AskMap asks_;
    std::unordered_map< OrderId, OrderLocation > order_locations_;

public:
    struct PriceLevelInfo
    {
        Price price;
        Quantity totalQuantity;
        std::size_t orderCount;
    };

    explicit OrderBook( std::string symbol );

    std::vector< Trade > addOrder( Order order );

    bool cancelOrder( OrderId id );

    std::optional< Price > bestBid() const;
    std::optional< Price > bestAsk() const;
    std::optional< Price > spread() const;

    std::vector< PriceLevelInfo > bidDepth( std::size_t depth ) const;
    std::vector< PriceLevelInfo > askDepth( std::size_t depth ) const;

    const std::string& symbol() const noexcept { return symbol_; }
    bool empty() const noexcept { return bids_.empty() && asks_.empty(); }
    std::size_t activeOrderCount() const noexcept { return order_locations_.size(); }
};

}