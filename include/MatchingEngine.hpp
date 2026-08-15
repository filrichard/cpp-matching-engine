#pragma once

#include "Order.hpp"
#include "OrderBook.hpp"
#include "Trade.hpp"
#include "Types.hpp"

#include <atomic>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace matching_engine
{

/*
Owns one OrderBook per tradable symbol, assigns order ids, and routes
OrderId assignment is safe to call concurrently, but this class does not
synchronize access to the underlying OrderBooks themselves. Two threads
calling submit or cancelOrder concurrently, even for different symbols,
are NOT safe without external locking. This is intentional.
*/

class MatchingEngine
{
public:
    struct SubmitResult
    {
        OrderId orderId;
        std::vector< Trade > trades;
    };

    using TradeCallback = std::function< void( const Trade& ) >;

    MatchingEngine() = default;

    void addSymbol( std::string symbol );

    bool hasSymbol( const std::string& symbol ) const;

    std::vector< std::string > symbols() const;

    /// Submits a limit order for `symbol`. Throws std::invalid_argument
    /// if the symbol hasn't been registered via addSymbol().
    SubmitResult submitLimitOrder( ClientId clientId,
                                   const std::string& symbol,
                                   Side side,
                                   Price price,
                                   Quantity quantity,
                                   TimeInForce tif = TimeInForce::GTC );
 
    /// Submits a market order for `symbol`. Throws std::invalid_argument
    /// if the symbol hasn't been registered via addSymbol().
    SubmitResult submitMarketOrder( ClientId clientId,
                                    const std::string& symbol,
                                    Side side,
                                    Quantity quantity,
                                    TimeInForce tif = TimeInForce::IOC );

    bool cancelOrder( const std::string& symbol, OrderId id );

    const OrderBook* book( const std::string& symbol ) const;

    void setTradeCallback( TradeCallback callback );

private:
    OrderId nextOrderId() noexcept;

    OrderBook& bookForOrThrow( const std::string& symbol );

    void notify( const std::vector< Trade >& trades ) const;

    std::unordered_map< std::string, OrderBook > books_;
    std::atomic< OrderId > next_order_id_{ 1 };
    TradeCallback trade_callback_;
};

}