cmake_minimum_required(VERSION 3.12)

project (riscv_em)
set(CMAKE_BUILD_TYPE Release)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Werror -Wextra -Wpedantic")

set(SRC_HELPER 
    src/helpers/fifo.c
)

set(INC_HELPER
    src/helpers
)

set(SRC_CORE 
    src/core/core.c
    src/core/csr.c
)

set(INC_CORE
    src/core
)

set(SRC_PERIPH
    src/peripherals/clint.c
    src/peripherals/plic.c
    src/peripherals/uart.c
)

set(INC_PERIPH
    src/peripherals
)

set(SRC_SOC 
    src/soc/riscv_example_soc.c
)

set(INC_SOC
    src/soc
)

add_executable(riscv_em ${SRC_HELPER} ${SRC_CORE} ${SRC_PERIPH} ${SRC_SOC} src/main.c)
target_include_directories(riscv_em PUBLIC . ${INC_HELPER} ${INC_CORE} ${INC_PERIPH} ${INC_SOC})
target_compile_options(riscv_em PRIVATE -Wall -Wextra -pedantic -Werror)
target_link_libraries(riscv_em pthread)

install (TARGETS riscv_em
         ARCHIVE DESTINATION lib
         LIBRARY DESTINATION lib
         RUNTIME DESTINATION bin)

### Debian Package generation
set(CPACK_GENERATOR "DEB")
set(CPACK_PACKAGE_VERSION "0.1.0")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "https://github.com/franzflasch")
include(CPack)
