# C++ Matching Engine

[![Ubuntu CI](https://github.com/filrichard/cpp-matching-engine/actions/workflows/ci-ubuntu.yml/badge.svg)](https://github.com/filrichard/cpp-matching-engine/actions/workflows/ci-ubuntu.yml)
[![macOS CI](https://github.com/filrichard/cpp-matching-engine/actions/workflows/ci-macos.yml/badge.svg)](https://github.com/filrichard/cpp-matching-engine/actions/workflows/ci-macos.yml)
[![Windows CI](https://github.com/filrichard/cpp-matching-engine/actions/workflows/ci-windows.yml/badge.svg)](https://github.com/filrichard/cpp-matching-engine/actions/workflows/ci-windows.yml)

A learning project implementing a limit order book and matching engine in modern C++.

The engine supports:

- Limit and market orders
- Price-time priority matching
- Partial fills and multi-level order matching
- Good Till Cancelled, Immediate or Cancel, and Fill or Kill time-in-force policies
- Order cancellation
- Best bid, best ask, spread, and depth queries
- Multiple trading symbols through a MatchingEngine
- Trade callbacks

The project is built with CMake and tested with Catch2. Continuous integration runs the test suite on Ubuntu, macOS, and Windows. Currently still in active development.