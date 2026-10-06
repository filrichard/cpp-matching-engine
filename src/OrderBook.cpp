#include "OrderBook.hpp"

#include <algorithm>
#include <atomic>
#include <cassert>
#include <memory>

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

    OrderBook::~OrderBook()
    {
        clearOrders();
    }

    void OrderBook::PriceLevel::pushBack( OrderNode* node ) noexcept
    {
        node->previous = tail;
        node->next = nullptr;

        if ( tail )
            tail->next = node;
        else
            head = node;

        tail = node;
        ++orderCount;
    }

    void OrderBook::PriceLevel::unlink( OrderNode* node ) noexcept
    {
        if ( node->previous )
            node->previous->next = node->next;
        else
            head = node->next;

        if ( node->next )
            node->next->previous = node->previous;
        else
            tail = node->previous;

        node->previous = nullptr;
        node->next = nullptr;
        --orderCount;
    }

    OrderBook::AddOrderResult OrderBook::executeOrder( Order order )
    {
        std::vector< Trade > trades;

        if ( order_locations_.contains( order.id() ) )
            throw std::invalid_argument( "Duplicate OrderID submitted while still active" );

        if ( order.timeInForce() == TimeInForce::FOK && !canFullyFill( order ) )
        {
            order.reject();
            return AddOrderResult{ order.status(), order.filledQuantity(),
                                   order.remainingQuantity(), std::move( trades ) };
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
            {
                const AddOrderResult result{ order.status(), order.filledQuantity(),
                                             order.remainingQuantity(), std::move( trades ) };
                restOrder( std::move( order ) );
                return result;
            }
            else
                order.cancel();
        }
        return AddOrderResult{ order.status(), order.filledQuantity(),
                               order.remainingQuantity(), std::move( trades ) };
    }

    std::vector< Trade > OrderBook::addOrder( Order order )
    {
        return executeOrder( std::move( order ) ).trades;
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
            assert( level_it != book.end() );
            PriceLevel& level = level_it->second;
            level.totalQuantity -= loc.node->order.remainingQuantity();
            loc.node->order.cancel();
            level.unlink( loc.node );

            if ( level.empty() )
                book.erase( level_it );
        };

        if ( loc.side == Side::Buy )
            removeFrom( bids_ );
        else
            removeFrom( asks_ );
        
        order_locations_.erase( loc_it );
        releaseNode( loc.node );
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
            result.push_back( PriceLevelInfo{ price, level.totalQuantity, level.orderCount } );
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
            result.push_back( PriceLevelInfo{ price, level.totalQuantity, level.orderCount } );
        }
        return result;
    }

    void OrderBook::matchAgainstAsks( Order& incoming, std::vector< Trade >& trades )
    {
        assert( incoming.side() == Side::Buy );

        while ( incoming.remainingQuantity() > 0 && !asks_.empty() )
        {
            auto level_it = asks_.begin();
            const Price level_price = level_it->first;

            if ( incoming.isLimit() && incoming.price() < level_price )
                break;
            
            PriceLevel& level = level_it->second;

            while ( incoming.remainingQuantity() > 0 && !level.empty() )
            {
                PriceLevel::OrderNode* node = level.head;
                Order& resting = node->order;
                const Quantity fill_qty = std::min( incoming.remainingQuantity(), resting.remainingQuantity() );
                incoming.fill( fill_qty );
                resting.fill( fill_qty );
                level.totalQuantity -= fill_qty;

                trades.emplace_back( nextTradeId(), symbol_, level_price, fill_qty,
                                 /*buy_order_id=*/incoming.id(),
                                 /*sell_order_id=*/resting.id(),
                                 /*buy_client_id=*/incoming.clientId(),
                                 /*sell_client_id=*/resting.clientId(),
                                 /*aggressor_side=*/incoming.side() );
                
                if ( resting.isFullyFilled() )
                {
                    const OrderId resting_id = resting.id();
                    order_locations_.erase( resting_id );
                    level.unlink( node );
                    releaseNode( node );
                }
            }

            if ( level.empty() )
                asks_.erase( level_it );

        }
    }

    void OrderBook::matchAgainstBids( Order& incoming, std::vector< Trade >& trades )
    {
        assert( incoming.side() == Side::Sell );

        while( incoming.remainingQuantity() > 0 && !bids_.empty() )
        {
            auto level_it = bids_.begin();
            const Price level_price = level_it->first;

            if ( incoming.isLimit() && incoming.price() > level_price )
                break;
            
            PriceLevel& level = level_it->second;

            while ( incoming.remainingQuantity() > 0 && !level.empty() )
            {
                PriceLevel::OrderNode* node = level.head;
                Order& resting = node->order;
                const Quantity fill_qty = std::min( incoming.remainingQuantity(), resting.remainingQuantity() );
                incoming.fill( fill_qty );
                resting.fill( fill_qty );
                level.totalQuantity -= fill_qty;

                trades.emplace_back( nextTradeId(), symbol_, level_price, fill_qty,
                                 /*buy_order_id=*/resting.id(),
                                 /*sell_order_id=*/incoming.id(),
                                 /*buy_client_id=*/resting.clientId(),
                                 /*sell_client_id=*/incoming.clientId(),
                                 /*aggressor_side=*/incoming.side() );
                
                if ( resting.isFullyFilled() )
                {
                    const OrderId resting_id = resting.id();
                    order_locations_.erase( resting_id );
                    level.unlink( node );
                    releaseNode( node );
                }
            }

            if ( level.empty() )
                bids_.erase( level_it );

        }
    }

    bool OrderBook::canFullyFill( const Order& order ) const
    {
        Quantity available = 0;
        const Quantity needed = order.remainingQuantity();

        if ( order.isBuy() )
        {
            for ( auto const& [ price, level ] : asks_ )
            {
                if ( order.isLimit() && order.price() < price )
                    break;
                available += level.totalQuantity;
                if ( available >= needed )
                    return true;
            }
        }
        else
        {
            for ( auto const& [ price, level ] : bids_ )
            {
                if ( order.isLimit() && order.price() > price )
                    break;

                available += level.totalQuantity;

                if ( available >= needed )
                    return true;
            }
        }

        return false;
    }

    void OrderBook::restOrder( Order order )
    {
        const OrderId id = order.id();
        const Side side = order.side();
        const Price price = order.price();
        const Quantity qty = order.remainingQuantity();

        auto insert = [ & ]( auto& book )
        {
            auto [ level_it, created ] = book.try_emplace( price );
            PriceLevel& level = level_it->second;
            PriceLevel::OrderNode* node = nullptr;

            try
            {
                node = allocateNode( std::move( order ) );
                const auto [ location_it, inserted ] =
                    order_locations_.emplace( id, OrderLocation{ side, price, node } );
                if ( !inserted )
                    throw std::logic_error( "Duplicate OrderID submitted while still active" );

                (void)location_it;
                level.pushBack( node );
                level.totalQuantity += qty;
            }
            catch ( ... )
            {
                if ( node )
                    releaseNode( node );
                if ( created && level.empty() )
                    book.erase( level_it );
                throw;
            }
        };

        if ( side == Side::Buy )
            insert( bids_ );
        else
            insert( asks_ );
    }

    OrderBook::PriceLevel::OrderNode* OrderBook::allocateNode( Order order )
    {
        void* storage = order_node_pool_.allocate( sizeof( PriceLevel::OrderNode ),
                                                   alignof( PriceLevel::OrderNode ) );
        return std::construct_at( static_cast< PriceLevel::OrderNode* >( storage ), std::move( order ) );
    }

    void OrderBook::releaseNode( PriceLevel::OrderNode* node ) noexcept
    {
        std::destroy_at( node );
        order_node_pool_.deallocate( node, sizeof( PriceLevel::OrderNode ),
                                    alignof( PriceLevel::OrderNode ) );
    }

    void OrderBook::clearOrders() noexcept
    {
        auto clearSide = [ this ]( auto& book ) noexcept
        {
            for ( auto& [ price, level ] : book )
            {
                auto* node = level.head;
                while ( node )
                {
                    auto* next = node->next;
                    releaseNode( node );
                    node = next;
                }
            }
            book.clear();
        };

        clearSide( bids_ );
        clearSide( asks_ );
        order_locations_.clear();
    }

}
