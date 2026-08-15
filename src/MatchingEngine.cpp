#include "MatchingEngine.hpp"

#include <stdexcept>
#include <utility>

namespace matching_engine
{

void MatchingEngine::addSymbol( std::string symbol )
{
    if ( books_.contains( symbol ) )
        throw std::invalid_argument( "Symbol already registered: " );
    
    books_.emplace( symbol, OrderBook( symbol ) );
}

bool MatchingEngine::hasSymbol( const std::string& symbol ) const
{
    return books_.contains( symbol );
}

std::vector< std::string > MatchingEngine::symbols() const
{
    std::vector< std::string > result;
    result.reserve( books_.size() );
    for ( auto const& [ symbol, book ] : books_ )
    {
        result.push_back( symbol );
    }

    return result;
}

MatchingEngine::SubmitResult MatchingEngine::submitLimitOrder( ClientId clientId,
                                                               const std::string& symbol,
                                                               Side side,
                                                               Price price,
                                                               Quantity quantity,
                                                               TimeInForce tif )
{
    OrderBook& targetBook = bookForOrThrow( symbol );
    const OrderId id = nextOrderId();

    Order order = Order::makeLimit( id, clientId, symbol, side, price, quantity, tif );
    std::vector< Trade > trades = targetBook.addOrder( std::move( order ) );

    notify( trades );
    return SubmitResult{ id, std::move( trades ) };
}

MatchingEngine::SubmitResult MatchingEngine::submitMarketOrder( ClientId clientId,
                                                                const std::string& symbol,
                                                                Side side,
                                                                Quantity quantity,
                                                                TimeInForce tif )
{
    OrderBook& targetBook = bookForOrThrow( symbol );
    const OrderId id = nextOrderId();

    Order order = Order::makeMarket( id, clientId, symbol, side, quantity, tif );
    std::vector< Trade > trades = targetBook.addOrder( std::move( order ) );

    notify( trades );
    return SubmitResult{ id, std::move( trades ) };
}

bool MatchingEngine::cancelOrder( const std::string& symbol, OrderId id )
{
    auto it = books_.find( symbol );
    if ( it == books_.end() )
        return false;

    return it->second.cancelOrder( id );
}

const OrderBook* MatchingEngine::book( const std::string& symbol ) const
{
    auto it = books_.find( symbol );
    if ( it == books_.end() )
        return nullptr;
    
    return &it->second;
}

void MatchingEngine::setTradeCallback( TradeCallback callback )
{
    trade_callback_ = std::move( callback );
}

OrderId MatchingEngine::nextOrderId() noexcept
{
    return next_order_id_.fetch_add( 1, std::memory_order_relaxed );
}

OrderBook& MatchingEngine::bookForOrThrow( const std::string& symbol )
{
    auto it = books_.find( symbol );
    if ( it == books_.end() )
        throw std::invalid_argument( "Unknown symbol" );
    
    return it->second;
}

void MatchingEngine::notify( const std::vector< Trade >& trades ) const
{
    if ( !trade_callback_ )
        return;
    
    for ( const auto& trade : trades )
    {
        trade_callback_( trade );
    }
}

}