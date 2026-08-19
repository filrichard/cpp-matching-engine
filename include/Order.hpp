#pragma once

#include "Types.hpp"

#include <stdexcept>
#include <string>
#include <utility>

namespace matching_engine
{

/*
Represents a single order in the matching engine
Order owns its identity and quantity/status bookkeeping
Order's priority in relation to other orders is OrderBook's reponsibility
*/
class Order
{
    private:
        OrderId id_;
        ClientId client_id_;
        std::string symbol_;
        Side side_;
        OrderType type_;
        Price price_;
        Quantity quantity_;
        Quantity remaining_quantity_;
        TimeInForce tif_;
        OrderStatus status_;
        TimeStamp timestamp_;
    public:
        Order( OrderId id,
            ClientId client_id,
            std::string symbol,
            Side side,
            OrderType type,
            Price price,
            Quantity quantity,
            TimeInForce tif = TimeInForce::GTC,
            TimeStamp timestamp = std::chrono::system_clock::now() ) :

            id_( id ),
            client_id_( client_id ),
            symbol_( std::move( symbol ) ),
            side_( side ),
            type_( type ),
            price_( price ),
            quantity_( quantity ),
            remaining_quantity_( quantity ),
            tif_( tif ),
            status_( OrderStatus::New ),
            timestamp_( timestamp )
            {
                if ( quantity_ == 0 )
                    throw std::invalid_argument( "Order quantity must be greater than zero" );
                if ( type_ == OrderType::Limit && price_ <= 0 )
                    throw std::invalid_argument( "Limit order price must be greater than zero" );
                if ( type_ == OrderType::Market && price_ != 0 )
                    throw std::invalid_argument( "Market orders must be constructed with price zero" );
            }
        static Order makeLimit( OrderId id, ClientId client_id, std::string symbol,
                                Side side, Price price, Quantity quantity,
                                TimeInForce tif = TimeInForce::GTC )
                    {
                        return Order( id, client_id, std::move( symbol ), side, OrderType::Limit, price, quantity, tif );
                    }
        static Order makeMarket( OrderId id, ClientId client_id, std::string symbol,
                                    Side side, Quantity quantity,
                                    TimeInForce tif = TimeInForce::IOC )
                    {
                        return Order( id, client_id, std::move( symbol ), side, OrderType::Market,
                            /*price = */0, quantity, tif );
                    }
        
        // Getters
        OrderId id() const noexcept { return id_; }
        ClientId clientId() const noexcept { return client_id_; }
        const std::string& symbol() const noexcept { return symbol_; }
        Side side() const noexcept { return side_; }
        OrderType type() const noexcept { return type_; }
        Price price() const noexcept { return price_; }
        Quantity quantity() const noexcept { return quantity_; }
        Quantity remainingQuantity() const noexcept { return remaining_quantity_; }
        Quantity filledQuantity() const noexcept { return quantity_ - remaining_quantity_; }
        TimeInForce timeInForce() const noexcept { return tif_; }
        OrderStatus status() const noexcept { return status_; }
        TimeStamp timestamp() const noexcept { return timestamp_; }

        bool isBuy() const noexcept { return side_ == Side::Buy; }
        bool isSell() const noexcept { return side_ == Side::Sell; }
        bool isLimit() const noexcept { return type_ == OrderType::Limit; }
        bool isMarket() const noexcept { return type_ == OrderType::Market; }

        bool isFullyFilled() const noexcept { return remaining_quantity_ == 0; }

        // True if the order can still participate in matching / rest on the book
        bool isActive() const noexcept { return status_ == OrderStatus::New 
                                             || status_ == OrderStatus::PartiallyFilled; }
        
        // Setters
        /*
        Apply a fill of qty against this order. Throws if qty exceeds
        the remaining quantity or the order is no longer active
        */
        void fill( Quantity qty )
        {
            if ( !isActive() )
                throw std::logic_error( "Cannot fill an inactive order" );
            if ( qty == 0 || qty > remaining_quantity_ )
                throw std::invalid_argument( "Fill quantity exceeds remaining quantity" );
            remaining_quantity_ -= qty;
            status_ = isFullyFilled() ? OrderStatus::Filled : OrderStatus::PartiallyFilled;
        }
        // Cancel the order. Throws if it's already in a terminal state
        void cancel()
        {
            if ( !isActive() )
                throw std::logic_error( "Cannot cancel an inactive order" );
            status_ = OrderStatus::Cancelled;
        }
        /*
        Reject the order before it ever rests
        on the book. Only valid from the New state
        */
        void reject()
        {
            if ( status_ != OrderStatus::New )
                throw std::logic_error( "Only new orders may be rejected" );
            status_ = OrderStatus::Rejected;
        }

        void expire()
        {
            if ( !isActive() )
                throw std::logic_error( "Cannot expire an inactive order" );
            status_ = OrderStatus::Expired;
        }
        /*
        Identity based equality - two Orders are the same order
        if they share the same id
        */
        friend bool operator==( const Order& lhs, const Order& rhs ) noexcept
        {
            return lhs.id_ == rhs.id_;
        }
};
}