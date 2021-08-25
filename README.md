# ooows-* DEF CON 2021 Finals

This repo contains the source for `ooows-flag-baby`, `ooows-p92021`, `ooows-ogx`, `ooows-broadcooom`, and `ooows-hyper-o` which were used in DEF CON 2021 Finals.

The core of the ooows framework (such as [vmm.c](./vmm.c), [devicebus.c](./devicebus.c), [apic.c](./apic.c)) was conceptualized/written by [mike_pizza](https://twitter.com/michaeljpizza) and [jay](https://twitter.com/JakeCorina).

## Kernel

[Our kernel](./boot/kernel/kernel.c) contains many functions to test the different devices (in fact, it's how we did the testing of patches during the CTF).

## `ooows-flag-baby`

[noflag.sh](./devices-bin/noflag.sh), written by [Zardus](https://twitter.com/Zardus) was the first challenge, and the
goal was for teams to understand how to upload a disk image that could
talk to this device.

## `ooows-p92021`

[p92021](./devices/p9fs/), written by [mike_pizza](https://twitter.com/michaeljpizza), was the second challenge, and it was an implementation of a plan 9 file system device, and it had several bugs (please check the source).

## `ooows-ogx`

[ogx](./devices/ogx/), written by [nullptr](https://twitter.com/nullptr), was intended as the third challenge (but after the first day we realized that we needed to run all challenges simultaneously), and it was an SGX-like device.

## `ooows-broadcooom`

[broadcooom](./devices/broadcooom), written by [adamd](https://twitter.com/adamdoupe), was intended as the fourth challenge, and it was a network device.

## `ooows-hyper-o`

[hyper-o](./hyper-o/), written by [Zardus](https://twitter.com/Zardus) and [jay](https://twitter.com/JakeCorina), was the final challenge where kvm was replaced by a custom hypervisor called `hyper-o`.

## Compilation

1. install submodules: `git submodule init`
1. `git submodule update`
1. `cd boot/i686`
1. `./runme.sh`
1. `cd ../../`
1. install mosquitto dependencies for net device: `apt install libmosquitto-dev mosquitto mosquitto-clients mosquitto-dev`
1. Install `python3-bitstruct` and `python3-lark-parser` or equivalents
1. `make && make buildboot`
1. Install `cmake`, `nasm`, `clang`, `llvm-10`, `libboost-all-dev`, `libseccomp-devel`, `libsodium-devel`, `python3-libnacl`, `python3-cbor2`, or equivalents


1. Do something like the following to build and install ogx:

```
mkdir -p devices/ogx/build
cd devices/ogx/build
cmake ..
make -j
cp ogx ogx_enclave_flag.bin ../ogx.pub ../ogx.sec ../../../devices-bin
cd -
```

## Running

You'll need access to `/dev/kvm` to run things.

1. `sudo adduser $(id -un) kvm`
1. Either login and logout to refresh perms or do `su - $USER`
1. To run from the commandline you'll do: `./vmm <vmname> <bootdisk> <num_vcpus> <device_config_file>`
e.g. `./vmm test boot/img/disk 1 devices.config`

This won't give you output at this point, so you'll need to setup the web server.

1. `cd web`
1. Make a venv (because you're a good person) `python -m venv ~/.virtualenvs/ooows`
1. `workon ooows`
1. `pip install -r requirements.txt`
1. `./init-db.py`, which will create `/tmp/disks` and `ooows-web.db`
1. Run the flask web server: `python app.py --debug`
1. Visit the website in a browser `http://localhost:5000`
1. Upload a disk image (use `boot/img/disk` for example).
1. Click the play button on the new image.
1. Click the "video" button once the image is successfully running.
1. No matter what you upload, you should see "OOOWS BIOS VERSION 0.1 (C)..." in the video output.

## Virtio
Virtio is a virtual device specification for paravirtualized environments.
If you want to write your device without having to think too much about PIO or MMIO we've implemented a Virto base class from which you can inherit.

If you don't care about the workings of virtio, you can [skip straight to the next section](#writing-a-virtio-device)

The core data structure of Virtio is the virtual queue. A virtq consists of:
##### 1. Descriptor Table
At it's heart virtio is used to supply buffers of data from the guest, to the host (device). The desciptor table descibes these buffers. Each entry contains:
- The address of the buffer
- The size of the buffer
- Flags denoting RDONLY, WRONLY, or "next" which are used for chaining buffers together.

Currenty we do not support flags or chaining so you don't need to worry about it.

##### 2. Available struct/ring
The Available struct houses a head index and ring buffer of desciptor IDs. These IDs serve as indices into the desciptor table.
It's purpose is to let the device know which entries/buffers in the desciptor table are ready for use.
##### 3. Used struct/ring
The Used struct is the same as the Available struct but it's updated by the device. It's used to inform the guest side which buffers have been consumed (and potentially written to!).
Elements if the ring buffer are slightly more than indices, they also include a length to let the guest know how much, if any, data has been written back to the buffer in question.

<br>

Each device may have several virtqs. For example, it is common to have 1 virtq for data, and another for control messages.

For the full spec [look here](https://docs.oasis-open.org/virtio/virtio/v1.1/virtio-v1.1.html). We're using [split queues](https://docs.oasis-open.org/virtio/virtio/v1.1/cs01/virtio-v1.1-cs01.html#x1-240006), not packed queues, and our method is [virtio over MMIO](https://docs.oasis-open.org/virtio/virtio/v1.1/cs01/virtio-v1.1-cs01.html#x1-1440002).

### Writing a Virtio Device

[`devices/utils/virtio.hpp`](devices/utils/virtio.hpp) provides a base class `MMIOVirtioDev` that will handle almost all of the communication for you.
To create an MMIOVirtioDev you need to supply it with the MMIO start address of your device, and the number of virtq's you want your device to have.

##### At a minimum, in your constructor you should:
- Define your [devices ID](https://docs.oasis-open.org/virtio/virtio/v1.1/cs01/virtio-v1.1-cs01.html#x1-1930005) `m_device_id` with one of the predefined virtio device types
- Define and set your devices [configuration space](https://docs.oasis-open.org/virtio/virtio/v1.1/cs01/virtio-v1.1-cs01.html#x1-180004) with `set_config_space`.
> Device configuration space is generally used for rarely-changing or initialization-time parameters.

e.g. for a network device, the host places the MAC in the configuration space.
- Set your devices [feature bits](https://docs.oasis-open.org/virtio/virtio/v1.1/cs01/virtio-v1.1-cs01.html#x1-130002) with `set_device_features` if you intend to use them. Feature bits are what communicate device capabilities to the guest driver. There's generally a negotiation of features by the guest driver to determine the subset of capabilities that are both offered by the device and supported by the driver.

##### In main:
All you should need to do after instantiating your device instance is call `dev->handle_IO`. This will handle all the MMIO the vmm throws your way.

##### Functions you need to implement:
- `got_data(uint16_t vq_idx)` Is the <ins>primary</ins> function you are responsible for. MMIOVirtioDev will call this when the guest has notified us of a new buffer being available, and will hand you the relevant virtq index that the buffer was added to (recall that most devices have more than 1 virtq).
- `config_space_write` the default implementation will allow writing to anything in your device's configuration space which is likely not what you want.
- `config_space_read` same as above but you may not care about the guest reading anything here.


##### Functions provided:
- `get_buf(uint16_t vq_idx)`
When you've been notified of a new buffer via `got_data` you'll want to then retrieve the VirtBuf abstraction class that will wrap the guest provided buffer for you.
- `put_buf(uint16_t vq_idx, VirtBuf *vbuf)`
When you're done using the memory provided by the guest, and have perhaps written data to it, you'll want to add the buffer to Used. `put_buf` does that for you.
- `send_irq(uint8_t)`
