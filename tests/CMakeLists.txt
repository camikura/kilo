add_executable(run_tests main.cc)

target_link_libraries(run_tests gtest gtest_main pthread)

include_directories(/opt/homebrew/include)
link_directories(/opt/homebrew/lib)

add_test(NAME run_tests COMMAND run_tests)
