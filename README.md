# riscv_em
This is a simple risc-v emulator written in C. 
I wrote this single file risc-v emulator just for fun and to  
get a better knowledge of the risc-v instruction set.  

Currently only a subset of the RV32I instruction set is implemented.  
Only FENCE and the CSR instructions of the RV32I ISA are not implemented.  

Implementation focus was simplicity NOT efficiency!  

The emulator is capable of running simple c programs. Please see /firmware for details.  

# How-To build:  
```console  
mkdir build && cd build  
cmake ..  
make  
```  

# Run hello world firmware:  
```console  
./riscv_em ../firmware/hello_world_fw.bin  
```  
If you want to build your own firmware you need an RV32I toolchain.  
You can build it with my own gcc build script, if you want:  
https://github.com/franzflasch/build-tools-gcc.git  
Or you can also use the official risc-v toolchain:  
https://github.com/riscv/riscv-gnu-toolchain.git  

Look into /firmware and try to figure it out how to build your own firmware.  
It is quite simple;)  

# ISA Test How-To:  
1. Build riscv baremetal toolchain with the build-gcc toolchain script.  

2. Clone official ISA test repo, besides the riscv_em. Both projects must be in the same folder!  
```console
git clone https://github.com/riscv/riscv-tests.git   
```

3. go into isa_test_env/riscv_em directory.

4. Now call the build script, e.g. bne instruction:
```console
./build_test.sh bne
```

5. The binary can then be loaded with the emulator:
./riscv_em bne.bin

The tests are modified such, that if everything goes well, the emulator should return
with an "unknown opcode" message within a short time. If it stucks in a loop then
there is probably something wrong.

In any case if you add a new instruction, run the corresponding isa test and check with
objdump if the emulator ran everything correctly. Look at the PC addresses, that all jumps were
called correctly and that finally the pass label was called. If the fail label was called, then
it should stuck in a loop anyways, however it is always better to double check;)

objdump example calls 
- get text assembly:  
```console
riscv32-none-elf-objdump -d -j .text bne.elf   
```

- get data assembly:  
```console
riscv32-none-elf-objdump -d -j .data bne.elf   
```

