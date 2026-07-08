add_executable(traceline_replay replay_main.cpp replay_format.cpp)
target_link_libraries(traceline_replay PRIVATE traceline_core)
target_compile_options(traceline_replay PRIVATE -Wall -Wextra)
