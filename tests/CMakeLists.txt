add_executable(pw_tests
    test_util.cpp
    test_detection.cpp
    ../src/util.cpp
    ../src/detection.cpp
)

target_include_directories(pw_tests PRIVATE ../src)
target_link_libraries(pw_tests PRIVATE
    doctest::doctest
    fmt::fmt
    nlohmann_json::nlohmann_json
    tabulate::tabulate
)

include(${doctest_SOURCE_DIR}/scripts/cmake/doctest.cmake)
doctest_discover_tests(pw_tests)
