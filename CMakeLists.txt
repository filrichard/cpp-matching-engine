cmake_minimum_required(VERSION 3.25)
project(matching_engine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

add_library(matching_engine_core
    src/OrderBook.cpp
    src/MatchingEngine.cpp
)

target_include_directories(matching_engine_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(matching_engine_core PUBLIC cxx_std_23)

if(MSVC)
    target_compile_options(matching_engine_core PRIVATE /W4)
else()
    target_compile_options(matching_engine_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(matching_engine_app src/main.cpp)
target_link_libraries(matching_engine_app PRIVATE matching_engine_core)

option(MATCHING_ENGINE_BUILD_TESTS "Build the test suite" ON)
if(MATCHING_ENGINE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

option(MATCHING_ENGINE_BUILD_BENCHMARKS "Build the benchmark suite" ON)
if(MATCHING_ENGINE_BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()