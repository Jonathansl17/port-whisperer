cmake_minimum_required(VERSION 3.20)
project(port-whisperer VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Static linking on Linux for portable binaries
if(UNIX AND NOT APPLE)
    option(STATIC_BUILD "Build fully static binary" OFF)
    if(STATIC_BUILD)
        set(CMAKE_EXE_LINKER_FLAGS "-static")
    endif()
endif()

include(cmake/Dependencies.cmake)

add_executable(ports
    src/main.cpp
    src/util.cpp
    src/process.cpp
    src/detection.cpp
    src/docker.cpp
    src/scanner.cpp
    src/display.cpp
)

# Platform-specific sources
if(APPLE)
    target_sources(ports PRIVATE src/platform/darwin.cpp)
elseif(UNIX)
    target_sources(ports PRIVATE src/platform/linux.cpp)
endif()

target_include_directories(ports PRIVATE src)
target_link_libraries(ports PRIVATE
    fmt::fmt
    CLI11::CLI11
    nlohmann_json::nlohmann_json
    tabulate::tabulate
)

# Install
install(TARGETS ports DESTINATION bin)
install(CODE "
    execute_process(COMMAND \${CMAKE_COMMAND} -E create_symlink
        ports \${CMAKE_INSTALL_PREFIX}/bin/whoisonport)
")

# Tests
option(BUILD_TESTS "Build tests" OFF)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
