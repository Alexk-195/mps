cmake_minimum_required(VERSION 3.24)
project(mps)

set(CMAKE_CXX_STANDARD 17)

add_library(mps STATIC src/mps.cpp)
target_include_directories(mps PUBLIC src)
target_link_libraries(mps pthread)

add_executable(mps_tutorial tutorials/mps_tutorials.cpp)
target_link_libraries(mps_tutorial mps)


