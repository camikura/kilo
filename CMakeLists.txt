cmake_minimum_required(VERSION 3.30)

project(kilo)

include_directories(include /opt/homebrew/include)
link_directories(/opt/homebrew/lib)

set(CMAKE_CXX_STANDARD 20)

# project
add_subdirectory(src)

# tests
option(BUILD_TESTS "Build tests" OFF)
if(BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

