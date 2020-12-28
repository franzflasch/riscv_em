#!/bin/sh
gcc -I ../core/ -I . -O3 -Wall -Werror -Wpedantic plic_test.c plic.c -o plic_test
