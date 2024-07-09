cmake_minimum_required(VERSION 3.19)
project(gsoc24-examples CXX)

set(HPX_DIR /home/johan511/hpx/build-master/lib/cmake/HPX)

find_package(HPX REQUIRED)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")

add_hpx_executable(sched SOURCES sched.cpp)

