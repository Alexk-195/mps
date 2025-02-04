cmake_minimum_required(VERSION 3.24)
project(mps)

set(CMAKE_CXX_STANDARD 17)

include_directories(src)

add_executable(mps_tutorial
        src/mps.cpp
        tutorials/mps_tutorials.cpp
        )

target_link_libraries(mps_tutorial pthread)


