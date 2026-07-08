include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.15.2
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# RapidCheck has no release tags -- pinned to a specific commit on master
# for reproducibility instead. RC_ENABLE_GTEST builds the rapidcheck_gtest
# extension (rapidcheck/gtest.h, RC_GTEST_PROP) used by
# test_fusion_properties.cpp.
FetchContent_Declare(
    rapidcheck
    GIT_REPOSITORY https://github.com/emil-e/rapidcheck.git
    GIT_TAG b2d9ed2dddefc4b84318d664b4f221eb792d89c7
)
set(RC_ENABLE_GTEST ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(rapidcheck)

add_executable(traceline_tests
    test_sensor.cpp
    test_fusion.cpp
    test_fusion_properties.cpp
    test_telemetry_bus.cpp
    test_flight_recorder.cpp
    test_replay_format.cpp
    ${CMAKE_SOURCE_DIR}/tools/replay_format.cpp
)
target_include_directories(traceline_tests PRIVATE ${CMAKE_SOURCE_DIR}/tools)
target_link_libraries(traceline_tests PRIVATE traceline_core GTest::gtest_main rapidcheck_gtest)
target_compile_options(traceline_tests PRIVATE -Wall -Wextra)

include(GoogleTest)
gtest_discover_tests(traceline_tests)
