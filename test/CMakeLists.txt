cmake_minimum_required(VERSION 3.10)
project(TestActionScheduler)

# Set C standard to C11
set(CMAKE_C_STANDARD 11)

# Add the Unity test framework (download from GitHub)
include(FetchContent)
FetchContent_Declare(
    unity
    GIT_REPOSITORY https://github.com/ThrowTheSwitch/Unity.git
    GIT_TAG master  # You can specify a specific tag or commit hash here
)
FetchContent_MakeAvailable(unity)

# Add the target library source file
add_library(action_scheduler ../action_scheduler.c)
include_directories(../)
# Add the test executable
add_executable(test_action_scheduler ../test_action_scheduler.c)
target_link_libraries(test_action_scheduler action_scheduler unity)