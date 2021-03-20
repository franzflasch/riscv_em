cmake_minimum_required(VERSION 3.12)

project (trap_test)
set(CMAKE_BUILD_TYPE Release)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Werror -Wextra -Wpedantic")

OPTION(RV_ARCH "RISC-V Arch" "64")
if(RV_ARCH STREQUAL "64")
    add_compile_definitions(RV64)
endif()

add_executable (trap unit_tests.c trap.c ../../../Unity/src/unity.c)
target_include_directories(trap PUBLIC . .. ../../../Unity/src/)
