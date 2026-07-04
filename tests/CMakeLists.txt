include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.15.2
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(traceline_tests
    test_sensor.cpp
    test_fusion.cpp
    test_telemetry_bus.cpp
    test_flight_recorder.cpp
)
target_link_libraries(traceline_tests PRIVATE traceline_core GTest::gtest_main)
target_compile_options(traceline_tests PRIVATE -Wall -Wextra)

include(GoogleTest)
gtest_discover_tests(traceline_tests)
