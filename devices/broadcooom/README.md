# ooows-broadcooom DEF CON 2021 Finals Challenge

This challenge is an implementation of the [IXP1200](https://en.wikipedia.org/wiki/IXP1200) network processor.

## Resources

I heavily used [Douglas E. Comer](https://www.cs.purdue.edu/homes/comer/) excellent book [Network Systems Design Using Network Processors: Intel IXP Version](https://www.amazon.com/gp/product/0131417924/ref=ppx_yo_dt_b_asin_title_o00_s00?ie=UTF8&psc=1), of which I am now the proud owner.

I also used [source code examples on the book's companion website](https://npbook.cs.purdue.edu/ixp1200/index.html).

Unfortuantely I was unable to acquire a real Intel assembler for the IXP1200, so I could not make the microengine bit-compatible with the IXP1200.
The assembly language is quite similar though, and most of the functionality is the same (many aspects were simplified).

## Components

- [`ooows-net.cpp`](../ooows-net.cpp) and [`ooows-net.hpp`](../ooows-net.hpp) act as the interface between the driver and the device, using virtio as the communication mechanism.
- [`microengine.cpp`](./microengine.cpp) and [`microengine.hpp`](./microengine.hpp) act as the "Core" processor of the IXP1200 and also execute the microengine instructions.
- [`engine.uc`](./examples/engine.uc) is the microengine code that runs.
- [`assembler.py`](./assembler.py) assembles the microcode to binary which the microengine runs.

# Bugs

There are two intended bugs in the microengine code, both revolving around the indirect memory references functionality of the IXP1200 (where the size of the memory transfer is taken from the result of the last ALU operation).

## TX IP Checksum Offloading IHL overflow

When the Transmit IP Checksum Offloading functionality of the device [is
enabled](https://github.com/o-o-overflow/dc2021f-ooows-public/blob/main/devices/ooows-net.cpp#L190),
then [the microengine code](https://github.com/o-o-overflow/dc2021f-ooows-public/blob/main/devices/broadcooom/examples/engine.uc#L322) will first [check that the IP version in the IP header is 4](https://github.com/o-o-overflow/dc2021f-ooows-public/blob/main/devices/broadcooom/examples/engine.uc#L343), then it will [incorrectly read in as many 4 byte values as specified in the IHL field](https://github.com/o-o-overflow/dc2021f-ooows-public/blob/main/devices/broadcooom/examples/engine.uc#L352).

The vulnerable instruction `ram[read, read_11, reg_12, reg_8, 0] ctx_swap` reads as many register-size values (4 bytes) from memory and writes them into registers starting at thread-local register `read_11`.

The number of registers that are written [to depends on the IHL field](https://github.com/o-o-overflow/dc2021f-ooows-public/blob/51e94703e3210cbac9b2c0b2ca30d6a3e82ce6f3/devices/broadcooom/examples/engine.uc#L350).
The `tx_thread` running on the microengine is thread context `3`, so `read_11`, according to [how the microengine maps out registers in `Microengine::absolute_register`](https://github.com/o-o-overflow/dc2021f-ooows-public/blob/main/devices/broadcooom/microengine.cpp#L864) it is absolute register `251` in [`Microengine::m_registers`](https://github.com/o-o-overflow/dc2021f-ooows-public/blob/main/devices/broadcooom/microengine.hpp#L41).

Because the IHL field is a nibble (half a byte), the highest value that can be used to read is `0xf` or 16, for a total of 64 bytes.

This allows us to overwrite memory ([due to the fact that the memory transfer functions to not check bounds when reading/writing from memory](https://github.com/o-o-overflow/dc2021f-ooows-public/blob/main/devices/broadcooom/microengine.cpp#L440) starting at [`Microengine::m_registers[251]`](https://github.com/o-o-overflow/dc2021f-ooows-public/blob/main/devices/broadcooom/microengine.hpp#L41).

Overwriting `Microengine::m_registers` means that we will [start overwriting `Microengine::m_code`](https://github.com/o-o-overflow/dc2021f-ooows-public/blob/main/devices/broadcooom/microengine.hpp#L42) with data that we control.

Due to the size, this means that we will be able to control the first ~8 (if my memory/math is correct) instructions in `m_code`.

Looking at the [start of the microengine code](https://github.com/o-o-overflow/dc2021f-ooows-public/blob/main/devices/broadcooom/examples/engine.uc#L6), we can see that the first 4 microengine instructions are only executed as part of the initialization of the microengine (essentially getting each thread to execute it's main loop).

However, we can control the first 4 instructions [starting at the start of the `rx_thread_loop`](https://github.com/o-o-overflow/dc2021f-ooows-public/blob/main/devices/broadcooom/examples/engine.uc#L14).

For now, let's assume that we can control these 4 instructions, how can we get the `rx_thread` to execute our instructions?
The [`rx_thread` is waiting toward the end of its loop for a new packet to come in from the PHY interface](https://github.com/o-o-overflow/dc2021f-ooows-public/blob/main/devices/broadcooom/examples/engine.uc#L32).
To trigger `rx_thread` to execute the new code, we need to upload another ooows kernel and have it send a packet (I believe any packet will work and cause `rx_thread` to loop back to code we control.

Now we have the microengine executing four instructions that we control! Is four instructions enough?

### Exploitation

Once we achieve arbitrary microcode execution, we need to be able to achieve arbitrary code execution to get the flag.
This is more complicated than one might expect: the microengine does not have access to any syscalls.
Functionally, the microcode has only [7 types of instructions](https://github.com/o-o-overflow/dc2021f-ooows-public/blob/main/devices/broadcooom/types.hpp#L211): ALU operations, load immediate, branch, load from registers, rx from PHY device, tx to core processor, access CSR registers, and memory reads/writes.

However, as we know from this bug, memory reads/writes do not have a bounds check!

We can read/write any memory location that is a positive offset of scratch memory which is at [`Microengine::m_scratch`](https://github.com/o-o-overflow/dc2021f-ooows-public/blob/main/devices/broadcooom/microengine.hpp#L39), and any memory location that is a positive offset of where [`Microengine::m_ram`](https://github.com/o-o-overflow/dc2021f-ooows-public/blob/main/devices/broadcooom/microengine.hpp#L63) points to, which is [8MB acquired through malloc](https://github.com/o-o-overflow/dc2021f-ooows-public/blob/main/devices/ooows-net.cpp#L25).

I believe some teams were able to achieve code execution only with these primitives, however my heap-foo was not so l33t.
So I combined these primitives to achieve an arbitrary read/write.

The idea is that because [`Microengine::m_ram`](https://github.com/o-o-overflow/dc2021f-ooows-public/blob/main/devices/broadcooom/microengine.hpp#L63) is a positive offset from [`Microengine::m_scratch`](https://github.com/o-o-overflow/dc2021f-ooows-public/blob/main/devices/broadcooom/microengine.hpp#L39), we can overwrite the [`Microengine::m_ram`](https://github.com/o-o-overflow/dc2021f-ooows-public/blob/main/devices/broadcooom/microengine.hpp#L63) pointer to point to any part of memory, then memory reads/writes to `ram` will use the new pointer value that we control.

The one quirk here that make our job more difficult is that the microengine's ram processing thread [first stores the pointer to mem on the stack](https://github.com/o-o-overflow/dc2021f-ooows-public/blob/main/devices/broadcooom/microengine.cpp#L429), then asks for a memory job from the queue.
This means that we will need to read/write from `ram` twice, and only the second time will use the correct value of `ram` that we controlled/overwrote.

I put this all together [in example shellcode](https://github.com/o-o-overflow/dc2021f-ooows-public/blob/main/devices/broadcooom/examples/shellcode.uc) that overall overwrites saved RIP of [`Microengine::interpreter_loop`](https://github.com/o-o-overflow/dc2021f-ooows-public/blob/main/devices/broadcooom/microengine.cpp#L145) and finally [triggers the break condition in `interpreter_loop`](https://github.com/o-o-overflow/dc2021f-ooows-public/blob/main/devices/broadcooom/microengine.cpp#L170).

Now that we have microengine shellcode, this is significantly longer than the 4 instructions that we control for this bug.
So what can we do?
We use the classic technique of using [a first stage shellcode](https://github.com/o-o-overflow/dc2021f-ooows-public/blob/main/devices/broadcooom/examples/stage_0.uc), and overwrites the rest of the microengine code of `rx_thread` with our target shellcode.

Where does the rest of our shellcode come from?
We can either have it be in the packet that we sent to trigger this bug, or we can have it be in the packet that we received to trigger `rx_thread` to execute our code.
Either way, the packets are stored at fixed locations in `ram` ring buffers.

## TX Ethernet Checksum Offloading overflow

The second bug in the challenge was in [the transmission ethernet checksum offloading](https://github.com/o-o-overflow/dc2021f-ooows-public/blob/main/devices/broadcooom/examples/engine.uc#L178).

This was a subtle bug which was the result of signed comparison and an off-by-one.

The signed comparison problem was that the ethernet checksum offloading would [try to load in maximum of 8 bytes worth of data from the packet to calculate the checksum](https://github.com/o-o-overflow/dc2021f-ooows-public/blob/main/devices/broadcooom/examples/engine.uc#L199).
The intention is the equivalent of `min(#bytesleft, 2*4)`.

However, the signed comparison bug is that [all branch instructions use signed comparison](https://github.com/o-o-overflow/dc2021f-ooows-public/blob/main/devices/broadcooom/microengine.cpp#L647) use [so the check in the microengine](https://github.com/o-o-overflow/dc2021f-ooows-public/blob/main/devices/broadcooom/examples/engine.uc#L201) will return `#bytesleft` if it has the highest bit set it will be considered negative, passed the check, and [used as an indirect memory reference](https://github.com/o-o-overflow/dc2021f-ooows-public/blob/main/devices/broadcooom/examples/engine.uc#L209) after [being shifted right by 2](https://github.com/o-o-overflow/dc2021f-ooows-public/blob/main/devices/broadcooom/examples/engine.uc#L208).

The problem is that [the maximum size of data that we can transmit to the device is 1518](https://github.com/o-o-overflow/dc2021f-ooows-public/blob/main/devices/ooows-net.cpp#L127) (which should be consistent with the Ethernet spec).

So the goal now is that the `#bytesleft` in the packet should be either negative or very large.

The off-by-one bug is at the end of the CRC calculation loop.
The microengine code [checks if the `#bytesleft` is >=7](https://github.com/o-o-overflow/dc2021f-ooows-public/blob/main/devices/broadcooom/examples/engine.uc#L295), and if it is then it will [subtract 8 from `#bytesleft`](https://github.com/o-o-overflow/dc2021f-ooows-public/blob/main/devices/broadcooom/examples/engine.uc#L303).

This means that if we send a packet a size of mod 7, at the last loop of the CRC calculation loop, it will set `#bytesleft = #bytesleft - 8`, which means that `#bytesleft` will be -1, which is represented as `0xFFFFFFFF` in hex.

On this last loop through, the value to read in [the indirect memory reference](https://github.com/o-o-overflow/dc2021f-ooows-public/blob/main/devices/broadcooom/examples/engine.uc#L209) will be very large.
Luckily, it's not so large, because the [microengine limits the count to be one byte](https://github.com/o-o-overflow/dc2021f-ooows-public/blob/main/devices/broadcooom/microengine.cpp#L5410), so "only" 255 register types (4 bytes each) will be read in, for a total of 1,020 bytes.

Again, this will allow us to control a significant amount of `m_code`, and follow the exploitation technique of the prior bug (we'll need to be careful about overwriting code in other threads, but we can handle that by overwriting that code with what's already there).

However, we've hit a bit of a snag, on the overwrite, we're on the last loop of calculating the packet CRC, which means we're actually at the end of our packet, and the data that we're overwriting with is written by memory outside our packet.
How can we control this memory if it's outside our packet?

Ring buffers to the rescue.
The core processor asks the microengine to send packets [by putting them on the tx ring buffer](https://github.com/o-o-overflow/dc2021f-ooows-public/blob/main/devices/ooows-net.cpp#L281), [the packet content is stored in RAM](https://github.com/o-o-overflow/dc2021f-ooows-public/blob/main/devices/ooows-net.cpp#L140), and [the size of the packet RAM ring buffer is 256](https://github.com/o-o-overflow/dc2021f-ooows-public/blob/main/devices/ooows-net.hpp#L29).

Now our idea to exploit this bug is:

1. Send 255 packets with our shellcode at the correct offset.
1. Enable TX Ethernet Checksum Offloading (you want to enable this now b/c it is very slow and enabling it for the other packets is very very slow).
1. Send a packet of size 7 which will cause the overwrite.
1. RX a packet (by sending it from another ooows kernel) to trigger `rx_thread` to start its loop over (I'm sure there are other ways to go here).
1. Microengine is now executing your shellcode!

Finally, we can achieve code execution through the previously described techniques.

# Fin

Anyway, this was my last DEF CON challenge as part of OOO, hope that now some time has passed you've "enjoyed" these trips down computing history.
