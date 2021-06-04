cmake_minimum_required(VERSION 3.7)
project(arcal)

if (NOT CMAKE_BUILD_TYPE)
    message("Setting build type to Release")
    set(CMAKE_BUILD_TYPE "Release")
endif ()

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/bin)

include_directories(src include ${CMAKE_CURRENT_BINARY_DIR}/include)
link_directories(${CMAKE_CURRENT_BINARY_DIR}/lib ${CMAKE_CURRENT_BINARY_DIR}/lib64)

add_subdirectory(src)
