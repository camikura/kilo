#!/usr/bin/env bash

set -eu

cmake -B build && cd build
cmake .. -DBUILD_TESTS=ON
cmake --build .
ctest --verbose
