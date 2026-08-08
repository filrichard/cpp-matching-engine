#include "OrderBook.hpp"

#include <algorithm>
#include <atomic>
#include <cassert>

namespace matching_engine
{

namespace
{
// TODO: once MatchingEngine exists and owns multiple OrderBooks, consider
// moving trade id generation there instead, so ids are assigned in a
// single, engine-wide sequence rather than per-OrderBook static state.
    std::atomic< TradeId > g_next_trade_id{ 1 };

    TradeId nextTradeId() noexcept
    {
        return g_next_trade_id.fetch_add( 1, std::memory_order_relaxed );
    }
}

    OrderBook::OrderBook( std::string symbol ) : symbol_( std::move( symbol ) ) {}
    

    std::vector< Trade > OrderBook::addOrder( Order order )
    {
        std::vector< Trade > trades;

        if ( order.timeInForce() == TimeInForce::FOK && !canFullyFill( order ) )
        {
            order.reject();
            return trades;
        }

        if ( order.isBuy() )
            matchAgainstAsks( order, trades );
        else
            matchAgainstBids( order, trades );
        
        if ( order.remainingQuantity() > 0 )
        {
            const bool restsOnBook = order.isLimit() &&
            ( order.timeInForce() == TimeInForce::GTC ||
              order.timeInForce() == TimeInForce::Day );

            if ( restsOnBook )
                restOrder( std::move( order ) );
        }
        return trades;
    }

}