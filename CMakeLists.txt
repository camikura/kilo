cmake_minimum_required(VERSION 3.30)

project(kilo C)

include_directories(include)

set(CMAKE_C_STANDARD 23)

add_executable(kilo src/main.c)
