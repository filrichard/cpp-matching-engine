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

    bool OrderBook::cancelOrder( OrderId id )
    {
        auto loc_it = order_locations_.find( id );
        if ( loc_it == order_locations_.end() )
            return false;
        
        const OrderLocation loc = loc_it->second;

        auto removeFrom = [ & ]( auto& book )
        {
            auto level_it = book.find( loc.price );
            PriceLevel& level = level_it->second;
            level.totalQuantity -= loc.it->remainingQuantity();
            loc.it->cancel();
            level.orders.erase( loc.it );

            if ( level.orders.empty() )
                book.erase( level_it );
        };

        if ( loc.side == Side::Buy )
            removeFrom( bids_ );
        else
            removeFrom( asks_ );
        
        order_locations_.erase( loc_it );
        return true;
    }

    std::optional< Price > OrderBook::bestBid() const
    {
        if ( bids_.empty() )
            return std::nullopt;
        return bids_.begin()->first;
    }

    std::optional< Price > OrderBook::bestAsk() const
    {
        if ( asks_.empty() )
            return std::nullopt;
        return asks_.begin()->first;
    }

    std::optional< Price > OrderBook::spread() const
    {
        const auto bid = bestBid();
        const auto ask = bestAsk();
        if ( !bid || !ask )
            return std::nullopt;
        return *ask - *bid;
    }

    std::vector< OrderBook::PriceLevelInfo > OrderBook::bidDepth( std::size_t depth ) const
    {
        std::vector< PriceLevelInfo > result;
        result.reserve( std::min( depth, bids_.size() ) );
        for ( auto const& [ price, level ] : bids_ )
        {
            if ( result.size() >= depth )
                break;
            result.push_back( PriceLevelInfo{ price, level.totalQuantity, level.orders.size() } );
        }
        return result;
    }

    std::vector< OrderBook::PriceLevelInfo > OrderBook::askDepth( std::size_t depth ) const
    {
        std::vector< PriceLevelInfo > result;
        result.reserve( std::min( depth, asks_.size() ) );
        for ( auto const& [ price, level ] : asks_ )
        {
            if ( result.size() >= depth )
                break;
            result.push_back( PriceLevelInfo{ price, level.totalQuantity, level.orders.size() } );
        }
        return result;
    }

}