cmake_minimum_required(VERSION 3.30 FATAL_ERROR)
cmake_policy(VERSION 3.30)

set(GAS_MADRONA_DIR "" CACHE STRING "Directory containing madrona")

if (NOT GAS_MADRONA_DIR)
  message(FATAL_ERROR "Must set GAS_MADRONA_DIR")
endif()

include("${GAS_MADRONA_DIR}/cmake/madrona_init.cmake")

project(gas LANGUAGES C CXX OBJC)

include(setup)
include(dependencies)

add_subdirectory("${GAS_MADRONA_DIR}")
