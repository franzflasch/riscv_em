# riscv_em
This is a risc-v emulator written in plain C.  
It started as a fun project a while ago and is now capable of doing a few things (but still just a hobby, won't be big and professional like qemu ;)  

### Ever wanted to know what the absolute minimum requirements are to run linux?
riscv_em is the answer. Yes, the emulator is capable of running Linux. (Although only nommu-linux for now.)  

One goal of this project is to be easily able to understand its source code and thus also the risc-v isa. You can also see this project as an attempt to directly translate the RISC-V ISA specs (Currently Unprivileged Spec v.20191213 and Privileged Spec v.20190608) into plain C.  
Implementation focus is simplicity NOT efficiency! Altough I always try to improve it's performance whenever possible, as long as the code does not suffer losses in readability!  

Currently the emulator supports RV32IMA and RV64IMA instructions.  
Furthermore it implements a CLINT (Core-Local Interrupt Controller) and also a PLIC (Platform Level Interrupt Controller), as well as a simple UART.  

Support for MMU (and full blown Linux) is already on my list for 2021.  

### How-To build:  
```console  
mkdir build && cd build  
# uclinux currently only works with RV64
cmake -DRV_ARCH=64 ..  
make  
```  

### Also build the device tree binary (device-tree-compiler needed):
```sh
cd dts
./build_dtb.sh
```

### Build a linux image for this emulator:
Please see https://github.com/franzflasch/linux_for_riscv_em  

### Start the emulator and load linux:
```sh
./build/riscv_em -f <linux_for_riscv_em-path>/output/linux/loader_64.bin -d dts/riscv_em.dtb
```
