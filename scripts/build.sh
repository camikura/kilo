#!/usr/bin/env bash

set -eu

cmake -B build && cd build
cmake ..
cmake --build .
