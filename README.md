# Popcorn OS

Popcorn OS is a simple, single-threaded OS with a monolithic kernel. Unlike many hobby OSes, it reads apps from disk! It currently has:

    - A command processor
    - Simple graphical demos
    - A very hopeful maintainer
    
It is built as a UEFI application and uses a service table instead of traditional syscalls. 

## Compilation

Prerequisites:

    - MSVC or GCC
    - GMake
    - QEMU
    - cURL
    
This was only tested with Windows with MSVC. Your mileage may vary, and if you find a way to fix it, please submit a pull request!

To compile it, simply clone the repo with all prerequisites and run:
``` bash
make
```
Or run:
``` bash
make help
```
to get more information.

When you first run `make`, it will download all the dependencies. This was done so this repo only contains code. 
And then it will run normally. It will delete all previous build artifacts and compile the kernel, then the apps, and then run it directly from a folder using QEMU. Unless you do `make prod`, in which it creates a `.img` file first and then boots from that.

## Application format

Apps are packaged as flat binaries (`.bin`) that have a function at address 0 which takes `(pop_Services* svc, int argc, CHAR16** argv)`.
This function is commonly called `pop_main`. The service table is very documented in `popcorn.h`.