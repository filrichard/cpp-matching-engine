#pragma once

#include "Types.hpp"

#include <stdexcept>
#include <string>
#include <utility>

namespace matching_engine
{

/*
An immutable record of a single match between two orders.

Contains no setters and no mutation methods.

Via convention, `price` is always the resting order's price, not the incoming
order's price. `OrderBook` is reponsible for upholding this when constructing
Trades. This type only stores the result
*/
class Trade 
{
private:
    TradeId id_;
    std::string symbol_;
    Price price_;
    Quantity quantity_;
    OrderId buy_order_id_;
    OrderId sell_order_id_;
    ClientId buy_client_id_;
    ClientId sell_client_id_;
    Side aggressor_side_;
    TimeStamp timestamp_;

public:
    Trade( TradeId id,
          std::string symbol,
          Price price,
          Quantity quantity,
          OrderId buy_order_id,
          OrderId sell_order_id,
          ClientId buy_client_id,
          ClientId sell_client_id,
          Side aggressor_side,
          TimeStamp timestamp = std::chrono::system_clock::now() )
        : id_( id )
        , symbol_( std::move( symbol ) )
        , price_( price )
        , quantity_( quantity )
        , buy_order_id_( buy_order_id )
        , sell_order_id_( sell_order_id )
        , buy_client_id_( buy_client_id )
        , sell_client_id_( sell_client_id )
        , aggressor_side_( aggressor_side )
        , timestamp_( timestamp )
    {
        if ( price_ <= 0 )
            throw std::invalid_argument( "Trade price must be greater than zero" );

        if ( quantity_ == 0 )
            throw std::invalid_argument( "Trade quantity must be greater than zero" );

        if ( buy_order_id_ == sell_order_id_ )
            throw std::invalid_argument( "Trade cannot match an order against itself" );

    }

    TradeId id() const noexcept { return id_; }
    const std::string& symbol() const noexcept { return symbol_; }
    Price price() const noexcept { return price_; }
    Quantity quantity() const noexcept { return quantity_; }

    OrderId buyOrderId() const noexcept { return buy_order_id_; }
    OrderId sellOrderId() const noexcept { return sell_order_id_; }
    ClientId buyClientId() const noexcept { return buy_client_id_; }
    ClientId sellClientId() const noexcept { return sell_client_id_; }

    // Which side initiated the trade as opposed to the resting/passive side.
    Side aggressorSide() const noexcept { return aggressor_side_; }

    TimeStamp timestamp() const noexcept { return timestamp_; }

    OrderId aggressorOrderId() const noexcept
    {
        return aggressor_side_ == Side::Buy ? buy_order_id_ : sell_order_id_;
    }

    OrderId restingOrderId() const noexcept
    {
        return aggressor_side_ == Side::Buy ? sell_order_id_ : buy_order_id_;
    }

    friend bool operator==( const Trade& lhs, const Trade& rhs ) noexcept
    {
        return lhs.id_ == rhs.id_;
    }
};

}