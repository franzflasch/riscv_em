cmake_minimum_required(VERSION 3.12)

project (riscv_em)
set(CMAKE_BUILD_TYPE Release)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Werror -Wextra -Wpedantic")

set(SRC_CORE 
    src/core/riscv_core.c
    src/core/riscv_csr.c
    src/core/riscv_clint.c
)

set(INC_CORE
    src/core
)

set(SRC_SOC 
    src/soc/riscv_example_soc.c
)

set(INC_SOC
    src/soc
)

add_executable(riscv_em ${SRC_CORE} ${SRC_SOC} src/main.c)
target_include_directories(riscv_em PUBLIC . ${INC_CORE} ${INC_SOC})
target_compile_options(riscv_em PRIVATE -Wall -Wextra -pedantic -Werror)

install (TARGETS riscv_em
         ARCHIVE DESTINATION lib
         LIBRARY DESTINATION lib
         RUNTIME DESTINATION bin)

### Debian Package generation
set(CPACK_GENERATOR "DEB")
set(CPACK_PACKAGE_VERSION "0.1.0")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "https://github.com/franzflasch")
include(CPack)
