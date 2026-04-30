ifeq ($(OS),Windows_NT)
CC=cl
else
CC=gcc
endif

PCC=$(CC)

HDSIZE=64

ifeq ($(PCC),cl)
define pop-c
	@# MSVC path: compile to OBJ, then convert to raw binary
	@# Warning: does not play nice with non-inline functions.
	cl /nologo /GS- /Zi /W3 /Od /D UNICODE /D _UNICODE /I include /c $(1) /Fo$(1).obj
	link.exe /NOLOGO /NODEFAULTLIB /ENTRY:pop_main /SUBSYSTEM:NATIVE /OUT:$(1).exe $(1).obj
	objcopy -O binary $(1).exe $(2)
	rm $(1).obj $(1).exe
endef
else
define pop-c
	@# GCC/Clang path: freestanding compile + LD to raw binary
	@# Warning: untested
	$(PCC) -I include -ffreestanding -fno-stack-protector -nostdlib \
	      -fshort-wchar -mno-red-zone -c $(1) -o $(1).o
	ld -nostdlib -e pop_main -Ttext=0x0 --oformat binary \
	       $(1).o -o $(2)
	rm $(1).o
endef
endif

define pop-c-minipop
	@# MSVC path: compile to OBJ, then convert to MINIPOP (non-functioning)
	cl /nologo /GS- /Zi /W3 /Od /D UNICODE /D _UNICODE /I include /c $(1) /Fo$(1).obj
	link.exe /NOLOGO /NODEFAULTLIB /ENTRY:pop_main /SUBSYSTEM:NATIVE /OUT:$(1).exe $(1).obj
	python minipop.py $(1).exe $(2)
	rm $(1).obj $(1).exe
endef

define pop-nasm
	nasm -f bin $(1) -o $(2)
endef

default: dev

dev: setup clean uefi
prod: setup clean uefi-prod

uefi: uefi-build uefi-run
uefi-prod: uefi-build uefi-make-image uefi-run-image

uefi-build: uefi-build-kernel uefi-build-apps

clean:
	rm -rf uefi
	rm -f ./*.obj
	rm -f ./*.o
	rm -f ./*.bin
	rm -f ./*.img
	mkdir uefi
	mkdir uefi/EFI
	mkdir uefi/EFI/BOOT
	mkdir uefi/binstuff
	mkdir uefi/system
	
setup:
	@if [ ! -f OVMF_CODE.fd ]; then \
	    echo "getting OVMF_CODE.fd..."; \
	    curl -sSL "https://qemu.weilnetz.de/test/ovmf/usr/share/OVMF/OVMF_CODE.fd" -o OVMF_CODE.fd; \
	fi
	@if [ ! -f OVMF_VARS.fd ]; then \
	    echo "getting OVMF_VARS.fd..."; \
	    curl -sSL "https://qemu.weilnetz.de/test/ovmf/usr/share/OVMF/OVMF_VARS.fd" -o OVMF_VARS.fd; \
	fi
	@if [ ! -d include ]; then \
	    echo "getting gnu-efi includes..."; \
	    curl -sSL "https://github.com/ncroxon/gnu-efi/archive/refs/tags/4.0.4.tar.gz" -o gnu-efi.tgz; \
	    tar xf gnu-efi.tgz gnu-efi-4.0.4/inc --strip-components=1; \
	    mv inc include; \
	    rm gnu-efi.tgz; \
	fi

uefi-build-kernel:
ifeq ($(CC),cl)
	@# MSVC
	cl /nologo /Zi /W3 /WX- /Od /D UNICODE /D _UNICODE popefi.c popfile.c poplist.c popgfx.c /I include \
	   /link /subsystem:EFI_APPLICATION /entry:efi_main /out:uefi/EFI/BOOT/BOOTX64.EFI
else
	@# GCC/Clang
	$(CC) -I include -fno-stack-protector -fpic -fshort-wchar -mno-red-zone \
	      -c popefi.c -o popefi.o
	$(CC) -I include -fno-stack-protector -fpic -fshort-wchar -mno-red-zone \
	      -c popfile.c -o popfile.o
	$(CC) -I include -fno-stack-protector -fpic -fshort-wchar -mno-red-zone \
	      -c poplist.c -o poplist.o
	$(CC) -I include -fno-stack-protector -fpic -fshort-wchar -mno-red-zone \
	      -c popgfx.c -o popgfx.o
	ld -nostdlib -znocombreloc -T elf_x86_64_efi.lds \
	   -shared -Bsymbolic \
	   popefi.o popfile.o poplist.o popgfx.o -o BOOTX64.so
	objcopy --target=efi-app-x86_64 BOOTX64.so uefi/EFI/BOOT/BOOTX64.EFI
endif

uefi-build-apps:
	$(call pop-nasm,hello.S,uefi/system/hello.bin)
	$(call pop-c,hello.c,uefi/system/chello.bin)
	$(call pop-c,cmd.c,uefi/system/cmd.bin)
	$(call pop-c,fread.c,uefi/system/fread.bin)
	$(call pop-c,fwrite.c,uefi/system/fwrite.bin)
	$(call pop-c,argv.c,uefi/system/argv.bin)
	$(call pop-c,ls.c,uefi/system/ls.bin)
	$(call pop-c,red.c,uefi/system/red.bin)
	$(call pop-c,pat.c,uefi/system/pat.bin)
	$(call pop-c,clear.c,uefi/system/clear.bin)
	$(call pop-c,bsquare.c,uefi/system/bsquare.bin)
	$(call pop-c,type.c,uefi/system/type.bin)
	echo Before fwrite.bin | iconv -f utf8 -t utf16le > uefi/hello.txt
	cp thello.txt uefi/system/thello
	cd caramelized && make

uefi-run:
	qemu-system-x86_64 -drive if=pflash,format=raw,file=OVMF_CODE.fd,readonly=on \
	                   -drive if=pflash,format=raw,file=OVMF_VARS.fd \
	                   -drive format=raw,file=fat:rw:uefi

uefi-make-image:
ifeq ($(OS),Windows_NT)
	@if ! net session >nul 2>&1; then \
	    echo; \
	    echo "uefi-make-image: Administrator privileges required to make the HDD image on Windows."; \
	    exit 1; \
	fi
	rm -f uefi.vhd
	echo create vdisk file="$(subst /,\,$(abspath ./uefi.vhd))" maximum=$(HDSIZE) > temp.txt
	echo select vdisk file="$(subst /,\,$(abspath ./uefi.vhd))" >> temp.txt
	echo attach vdisk >> temp.txt
	echo create partition primary >> temp.txt
	echo format fs=fat quick >> temp.txt
	echo assign letter=X >> temp.txt
	echo exit >> temp.txt
	cmd /c diskpart /s temp.txt
	@# copy files into X: after mounting
	cp -r uefi/* X:/
	@# now detach
	echo select vdisk file="$(subst /,\,$(abspath ./uefi.vhd))" > temp.txt
	echo detach vdisk >> temp.txt
	echo exit >> temp.txt
	cmd /c diskpart /s temp.txt
	rm -f temp.txt
	qemu-img convert -f vpc uefi.vhd -O raw uefi.img
else
	dd if=/dev/zero of=uefi.img bs=1M count=$(HDSIZE)
	mkfs.fat uefi.img
	sudo mount -o loop uefi.img /mnt/popuefi
	cp uefi/* /mnt/popuefi/
	sudo umount /mnt/popuefi
endif

uefi-run-image:
	qemu-system-x86_64 -drive if=pflash,format=raw,file=OVMF_CODE.fd,readonly=on \
	                   -drive if=pflash,format=raw,file=OVMF_VARS.fd \
	                   -drive format=raw,file=uefi.img 

help:
	@echo "      make CC=... PCC=..."
	@echo "  or: make dev CC=... PCC=..."
	@echo "  or: make prod CC=... PCC=... HDSIZE=..."
	@echo "  or: make help"
	@echo
	@echo "'make' is equivalent to 'make dev'. The CC argument specifies the "
	@echo "C compiler used to compile the kernel. It defaults to cl on Windows "
	@echo "and gcc elsewhere. PCC is the C compiler used to compile Popcorn OS"
	@echo "applications. It defaults to CC. 'make dev' compiles the kernel"
	@echo "and applications and boots directly from the folder using QEMU."
	@echo "'make prod' compiles the kernel and apps and puts that in a"
	@echo "hard disk image 'uefi.img' and boots that using QEMU. HDSIZE is the"
	@echo "size of the disk image in megabytes. It defaults to 64. 'make prod'"
	@echo "on Windows can only be run as Administrator because of the use of"
	@echo "'diskpart'."