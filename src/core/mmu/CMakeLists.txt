cmake_minimum_required(VERSION 3.12)

project (mmu_test)
set(CMAKE_BUILD_TYPE Release)
add_definitions ("-Wall -Werror -Wextra -Wpedantic")
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Werror -Wextra -Wpedantic")

OPTION(RV_ARCH "RISC-V Arch" "64")
if(RV_ARCH STREQUAL "64")
    add_compile_definitions(RV64)
endif()

add_executable (mmu unit_tests.c mmu.c ../../../Unity/src/unity.c)
target_include_directories(mmu PUBLIC . .. ../../../Unity/src/)
