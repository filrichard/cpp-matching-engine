#include "MatchingEngine.hpp"
#include "Order.hpp"
#include "OrderBook.hpp"
#include "Trade.hpp"

#include <iostream>

using namespace matching_engine;

namespace
{

    void printTrade( const Trade& trade )
    {
        std::cout << "  TRADE #" << trade.id()
            << " " << trade.symbol()
            << " price=" << trade.price()
            << " qty=" << trade.quantity()
            << " buyOrder=" << trade.buyOrderId()
            << " sellOrder=" << trade.sellOrderId()
            << " aggressor=" << (trade.aggressorSide() == Side::Buy ? "BUY" : "SELL")
            << '\n';
    }

    void printBookSummary( const OrderBook& book )
    {
        std::cout << "  Book: " << book.symbol() << '\n';

        auto asks = book.askDepth( 5 );
        auto bids = book.bidDepth( 5 );

        std::cout << "    Asks (best first):\n";
        for ( const auto& level : asks )
        {
            std::cout << "      " << level.price << " x " << level.totalQuantity
                    << " (" << level.orderCount << " orders)\n";
        }
        std::cout << "    ---\n";
        std::cout << "    Bids (best first):\n";
        for ( const auto& level : bids )
        {
            std::cout << "      " << level.price << " x " << level.totalQuantity
                    << " (" << level.orderCount << " orders)\n";
        }
    }
}
    int main( void )
    {
        MatchingEngine engine;
        engine.addSymbol( "AAPL" );

        engine.setTradeCallback( []( const Trade& trade )
        {
            std::cout << "[callback] ";
            printTrade( trade );
        } );

        std::cout << "Submitting resting sell orders...\n";
        engine.submitLimitOrder( /*clientId=*/1, "AAPL", Side::Sell, 100, 10 );
        engine.submitLimitOrder( /*clientId=*/1, "AAPL", Side::Sell, 101, 15 );

        if ( const auto* book = engine.book( "AAPL" ) )
        {
            std::cout << '\n';
            printBookSummary( *book );
        }

        std::cout << "\nSubmitting an aggressive buy that sweeps both levels...\n";
        auto result = engine.submitLimitOrder( /*clientId=*/2, "AAPL", Side::Buy, 101, 20 );

        std::cout << "\nOrder #" << result.orderId << " generated "
                << result.trades.size() << " trade(s):\n";
        for ( const auto& trade : result.trades )
        {
            printTrade( trade );
        }

        std::cout << "\nFinal book state:\n";
        if ( const auto* book = engine.book( "AAPL" ) )
            printBookSummary( *book );

        return 0;
    }