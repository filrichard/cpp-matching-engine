#pragma once

#include <chrono>
#include <cstdint>

namespace matching_engine
{
    // Identifiers
    using OrderId  = std::uint64_t;
    using ClientId = std::uint64_t;
    using TradeId  = std::uint64_t;

    // Quantities and Prices
    using Price    = std::uint64_t;
    using Quantity = std::uint64_t;

    // Time for reportable timestamps for time priority
    using TimeStamp = std::chrono::system_clock::time_point;

    // Enums
    enum class Side : std::uint8_t
    {
        Buy,
        Sell
    };

    enum class OrderType : std::uint8_t
    {
        Limit,
        Market
    };

    enum class TimeInForce : std::uint8_t
    {
        GTC, // Good until cancelled
        IOC, // Immediate or Cancel
        FOK, // Fill or Kill
        Day  // Expires at end of trading day
    };

    enum class OrderStatus : std::uint8_t
    {
        New,
        PartiallyFilled,
        Filled,
        Cancelled,
        Rejected,
        Expired
    };

    constexpr Side opposite( Side s ) noexcept
    {
        return s == Side::Buy ? Side::Sell : Side::Buy;
    }
}