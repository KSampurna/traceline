cmake_minimum_required(VERSION 3.25)
project(traceline LANGUAGES CXX VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(TRACELINE_BUILD_TESTS "Build unit tests" ON)
option(TRACELINE_BUILD_BENCHMARKS "Build benchmarks" ON)
option(TRACELINE_ENABLE_SANITIZERS "Build with ASan+UBSan" OFF)
option(TRACELINE_NO_EXCEPTIONS "Build with -fno-exceptions (enforces std::expected discipline)" OFF)
option(TRACELINE_BUILD_FUZZERS "Build libFuzzer harnesses (requires Clang)" OFF)

add_library(traceline_core
    src/sensor.cpp
    src/fusion.cpp
    src/telemetry_bus.cpp
    src/flight_recorder.cpp
)
target_include_directories(traceline_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_options(traceline_core PRIVATE -Wall -Wextra -Wpedantic -Wshadow -Wconversion)

if(TRACELINE_ENABLE_SANITIZERS)
    target_compile_options(traceline_core PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(traceline_core PUBLIC -fsanitize=address,undefined)
endif()

if(TRACELINE_NO_EXCEPTIONS)
    target_compile_options(traceline_core PUBLIC -fno-exceptions)
endif()

if(TRACELINE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(TRACELINE_BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()

if(TRACELINE_BUILD_FUZZERS)
    add_subdirectory(fuzz)
endif()
