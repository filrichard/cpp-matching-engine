cmake_minimum_required(VERSION 3.25)

project(MatchingEngine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(matching_engine
    src/main.cpp
    src/OrderBook.cpp
    src/MatchingEngine.cpp
    src/Trade.cpp
)

target_include_directories(matching_engine PRIVATE include)

enable_testing()

add_subdirectory(tests)