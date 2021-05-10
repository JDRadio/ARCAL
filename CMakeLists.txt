cmake_minimum_required(VERSION 3.16)
project(arcal)

if (NOT CMAKE_BUILD_TYPE)
    message("Setting build type to Release")
    set(CMAKE_BUILD_TYPE "Release")
endif ()

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/bin)

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup()

include_directories(src lib/plasma-dsp/plasma/src lib/rtl-sdr/include ${CMAKE_CURRENT_BINARY_DIR}/include ${CONAN_INCLUDE_DIRS})
link_directories(${CMAKE_CURRENT_BINARY_DIR}/lib ${CMAKE_CURRENT_BINARY_DIR}/lib64 ${CONAN_LIB_DIRS})

add_subdirectory(src)
add_subdirectory(lib)
