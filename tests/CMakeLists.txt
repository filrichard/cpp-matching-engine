include(FetchContent)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.7.1
)
FetchContent_MakeAvailable(Catch2)

add_executable(OrderBookTests
    OrderBookTests.cpp
)

target_link_libraries(OrderBookTests PRIVATE
    matching_engine_core
    Catch2::Catch2WithMain
)

target_compile_features(OrderBookTests PRIVATE cxx_std_23)

list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)
catch_discover_tests(OrderBookTests)
