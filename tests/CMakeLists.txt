include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0
)
# Prevent overriding the parent project's compiler/linker settings on Windows
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

include(GoogleTest)

set(MPS_TEST_SOURCES
    test_timer.cpp
    test_ts_queue.cpp
    test_synchronized.cpp
    test_pool_lifecycle.cpp
    test_pool_workers.cpp
    test_pool_messages.cpp
    test_waiter.cpp
    test_distributor.cpp
    test_pool_thread.cpp
    test_priority.cpp
)

add_executable(mps_tests ${MPS_TEST_SOURCES})
target_link_libraries(mps_tests PRIVATE mps GTest::gtest_main)
target_include_directories(mps_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
gtest_discover_tests(mps_tests)

add_executable(mps_tests_tracking test_object_tracking.cpp)
target_link_libraries(mps_tests_tracking PRIVATE mps GTest::gtest_main)
target_include_directories(mps_tests_tracking PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(mps_tests_tracking PRIVATE MPS_TRACK_OBJECTS)
gtest_discover_tests(mps_tests_tracking)
