cmake_minimum_required(VERSION 3.25)
project(DownstreamConsumer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(RuntimeRegistry CONFIG REQUIRED)
add_executable(consumer main.cpp)
target_link_libraries(consumer PRIVATE RuntimeRegistry::runtimeregistry)
