cmake_minimum_required(VERSION 3.1.0)
project(herald LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup(TARGETS)

find_package(Threads REQUIRED)

add_library(herald
  src/publisher.cpp
  src/subscriber.cpp
  src/sharedmem.cpp)

target_link_libraries(herald rt Threads::Threads)

target_include_directories(herald
    PUBLIC
        $<INSTALL_INTERFACE:include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)
