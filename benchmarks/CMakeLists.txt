include(FetchContent)
FetchContent_Declare(
    benchmark
    GIT_REPOSITORY https://github.com/google/benchmark.git
    GIT_TAG v1.9.1
)
set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(benchmark)

add_executable(traceline_benchmarks bench_fusion.cpp bench_telemetry_bus.cpp)
target_link_libraries(traceline_benchmarks PRIVATE traceline_core benchmark::benchmark_main)
