cmake_minimum_required(VERSION 3.19)
project(gsoc24-examples CXX)

set(HPX_DIR /home/johan511/hpx/build-master/lib/cmake/HPX)

find_package(HPX REQUIRED)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")

add_executable(future future.cpp)
target_link_libraries(future PUBLIC HPX::hpx)

add_executable(matmul matmul.cpp)
target_link_libraries(matmul PUBLIC HPX::hpx)

# add_executable(sender sender.cpp)
# target_link_libraries(sender PUBLIC HPX::hpx)
