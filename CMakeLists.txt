cmake_minimum_required(VERSION 3.23)

project(MarketOrderBookSimulator)

add_executable(mdism)

target_sources(mdism PRIVATE
    src/main.cpp
    src/OrderBook.cpp
    src/OrderBook.h
    src/Order.cpp
    src/Order.h
    src/OrderType.h
    src/OrderSide.h
    src/OrderStatus.h
    src/Trade.cpp
    src/Trade.h
)