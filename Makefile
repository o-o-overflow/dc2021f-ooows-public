CFLAGS ?= -Wall
LIBS ?= -lpthread
INCLUDE ?= inc
BIN ?= vmm
SRC=$(wildcard *.c)
KVM_STUB_SRC=$(wildcard kvm/*.c)
ROOT_OBJECTS=$(patsubst %.c, %.o, $(SRC))
KVM_STUB_OBJECTS=$(notdir $(patsubst %.c, %.o, $(KVM_STUB_SRC)))

all: vmm-kvm buildbios vga net p9fs devices-bin/noflag strip

strip: vmm vga net p9fs devices-bin
	strip -s devices-bin/net
	strip -s devices-bin/p9fs

buildbios: bios/bios.asm
	nasm -f bin -o bios/bios bios/bios.asm

buildboot:
	$(MAKE) -C boot

vga:
	gcc devices/ooows-vga.c devices/utils/handshake.c devices/utils/coms.c devices/utils/per-vm.c devices/utils/threadpool.c -Wall -o devices-bin/vga -I $(INCLUDE) -lrt -lpthread

devices/broadcooom/engine.out: devices/broadcooom/examples/engine.uc devices/broadcooom/assembler.py
	python3 devices/broadcooom/assembler.py --file devices/broadcooom/examples/engine.uc --output devices/broadcooom/engine.out

net: devices/broadcooom/engine.out
	g++ -std=c++11 -fno-rtti -Wno-packed-bitfield-compat -pthread devices/ooows-net.cpp devices/utils/mem-manager.cpp devices/utils/virtio.cpp devices/utils/handshake.c devices/broadcooom/microengine.cpp devices/broadcooom/phy.cpp -o devices-bin/net -I $(INCLUDE) -lmosquitto
	#g++ -std=c++11 -Wno-packed-bitfield-compat -pthread devices/ooows-net.cpp devices/utils/mem-manager.cpp devices/utils/virtio.cpp devices/utils/handshake.c devices/broadcooom/microengine.cpp devices/broadcooom/phy.cpp -D=TRACE devices/broadcooom/names.c -o devices-bin/net -I $(INCLUDE) -lmosquitto
	strip -s devices-bin/net
	cp devices/broadcooom/engine.out devices-bin/net-firmware
	chmod 644 devices-bin/net-firmware

devices-bin/noflag: devices/noflag.c
	gcc -Wall -o ./devices-bin/noflag devices/noflag.c

P9PATCH1=-DP9PATCH1
P9PATCHES=

p9fs:
	g++ -std=c++14 $(P9PATCHES) -pthread -fno-rtti devices/ooows-p9fs.cpp devices/utils/mem-manager.cpp devices/utils/virtio.cpp devices/p9fs/trequests.cpp devices/p9fs/rresponses.cpp devices/p9fs/p9core.cpp devices/p9fs/qidobject.cpp devices/utils/handshake.c -o devices-bin/p9fs -I $(INCLUDE)
	strip -s devices-bin/p9fs

clean:
	rm -rf *.o
	cd boot && $(MAKE) clean

$(ROOT_OBJECTS): %.o : %.c
	$(CC) $(CFLAGS) -c $< -I $(INCLUDE)

$(KVM_STUB_OBJECTS): %.o : kvm/%.c
	$(CC) $(CFLAGS) -c $< -I $(INCLUDE)

vmm-kvm: $(ROOT_OBJECTS) $(KVM_STUB_OBJECTS)
	$(CC) -o $(BIN) $^ $(CFLAGS) $(LIBS)

bootdisk: boot/boot.asm
	nasm -f bin -o boot/boot boot/boot.asm
	cp boot/boot virtualdisk
