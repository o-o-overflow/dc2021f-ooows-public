CFLAGS ?= -Wall
INCLUDE ?= inc
I686_DIR=i686
MKDIR_P = mkdir -p
OUT_DIR=out
IMG_DIR=img

all: directories buildbootloader buildkernel

directories: $(OUT_DIR) $(IMG_DIR)

$(OUT_DIR):
	$(MKDIR_P) $(OUT_DIR)

$(IMG_DIR):
	$(MKDIR_P) $(IMG_DIR)

buildbootloader:
	nasm -f bin -o out/bootloader kernel/bootloader.S

buildkernel:
	$(I686_DIR)/bin/i686-elf-gcc -c kernel/kernel.c -o out/kernel.o -std=gnu99 -ffreestanding -Wall -Wextra
	$(I686_DIR)/bin/i686-elf-gcc -c kernel/malloc.c -o out/malloc.o -std=gnu99 -ffreestanding -Wall -Wextra
	$(I686_DIR)/bin/i686-elf-gcc -c kernel/virtio_drv.c -o out/virtio_drv.o -std=gnu99 -ffreestanding -Wall -Wextra
	$(I686_DIR)/bin/i686-elf-gcc -c kernel/noflag.c -o out/noflag.o -std=gnu99 -ffreestanding -Wall -Wextra	
	$(I686_DIR)/bin/i686-elf-gcc -c kernel/p9.c -o out/p9.o -std=gnu99 -ffreestanding -Wall -Wextra
	$(I686_DIR)/bin/i686-elf-as kernel/kernel_entry.asm -o out/kernel_entry.o
	$(I686_DIR)/bin/i686-elf-as kernel/asm_routines.asm -o out/asm_routines.o
	$(I686_DIR)/bin/i686-elf-gcc -T linker.ld -o out/myos -ffreestanding -nostdlib out/kernel_entry.o out/kernel.o out/malloc.o out/virtio_drv.o out/asm_routines.o out/noflag.o out/p9.o -lgcc
	objcopy -R .note -R .comment -S -O binary out/myos out/myos.bin
	python3 disk_creator.py out/bootloader out/myos.bin

clean:
	rm -rf out/*.bin
	rm -rf out/*.o
	-rm out/bootloader
	-rm out/myos
	-rm img/disk
