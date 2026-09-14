include(FetchContent)

set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    googlebenchmark
    GIT_REPOSITORY https://github.com/google/benchmark.git
    GIT_TAG        v1.9.1
)
FetchContent_MakeAvailable(googlebenchmark)

add_executable(OrderBookBenchmarks
    OrderBookBenchmarks.cpp
)

target_link_libraries(OrderBookBenchmarks PRIVATE
    matching_engine_core
    benchmark::benchmark
)

target_compile_features(OrderBookBenchmarks PRIVATE cxx_std_23)

# IMPORTANT: benchmark numbers are only meaningful in an optimized build.
# Configure the whole project with -DCMAKE_BUILD_TYPE=Release (or
# RelWithDebInfo) before building this target — a Debug build will
# produce numbers that don't reflect real performance at all.
