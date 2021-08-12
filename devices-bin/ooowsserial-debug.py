#!/usr/bin/env python3
import os
import sys
import struct
import socket
from math import floor
from queue import Queue
from threading import Thread, Lock

IOPORT = 0
MMIO   = 1

IO_DIR_READ = 0
IO_DIR_WRITE = 1

VMM_FD = 3
RAM_FD = 4

VM_DIR = os.getenv("OOOWS_VM_STORE_DIR")
SERIAL_DIR = "/tmp"

class IoPortRequest:
    def __init__(self, payload):
        self.port, self.direction, self.size, self.data, self.count = \
            struct.unpack("<HBBII", payload)

    def __str__(self):
        return 'Pio { %x %x %x }' % (self.port, self.data, self.size)

class OoowsSerialController():

    COM1_RXTX       = 0x3F8
    COM2_RXTX       = 0x2F8

    def __init__(self, vmname):

        self._lock = Lock()

    def RxCom(self, ioAccess, port):
        # invalid access
        if ioAccess.size > 1:
            return 0

        b = os.read(sys.stdin.fileno(), 1)
        return struct.unpack("B", b)[0]

    def TxCom(self, ioAccess, port):
        unpacks = [("B", 0xff),
                   ("<H", 0xffff),
                   ("<I", 0xffffffff)]

        fmt, mask = unpacks[floor(ioAccess.size/2)]

        for i in struct.pack(fmt, ioAccess.data & mask):
            to_send = bytes([i])
            os.write(sys.stdout.fileno(), to_send)

        return 0

    def IoPortRead(self, r):
        value = 0

        self._lock.acquire()
        if r.port == self.COM1_RXTX:
            value = self.RxCom(r, 1)
        if r.port == self.COM2_RXTX:
            value =  self.RxCom(r, 2)
        self._lock.release()

        return value

    def IoPortWrite(self, r):
        value = 0

        self._lock.acquire()
        if r.port == self.COM1_RXTX:
            value = self.TxCom(r, 1)
        if r.port == self.COM2_RXTX:
            value = self.TxCom(r, 2)
        self._lock.release()

        return value

    def IoPortAccess(self, r):
        if r.direction == IO_DIR_READ:
            return self.IoPortRead(r)
        elif r.direction == IO_DIR_WRITE:
            return self.IoPortWrite(r)
        else:
            print("Impossible IO port access")

        return 0


def VmmWrite(s):
    return os.write(VMM_FD, s)

def VmmRead(n):
    return os.read(VMM_FD, n)

def ReadIoRequest(fd):
    raw = os.read(fd, 28)
    requestType = struct.unpack("<I", raw[:4])[0]
    payload = raw[4:]

    if requestType == IOPORT:
        return IoPortRequest(payload[:12])
    elif requestType == MMIO:
        return MmioRequest(payload)

    return None

# Let's assume for now PIOs just return the value read from the device
# This will always be 0 in the case of a write, but we'll leave it up
# to the VMM to decide what to do with the 0 in the write case
def WritePioResponse(fd, value):
    os.write(fd, (struct.pack("<I", value)))

def DeviceHandshake():
    VmmWrite(b"INIT")
    if VmmRead(4) != b"TINI":
        return False

    cpufds = struct.unpack("<IIII", VmmRead(16))

    return list(filter(lambda fd: fd != 0, cpufds))

def HandleIO(controller, fd):

    while True:
        r = ReadIoRequest(fd)
        if isinstance(r, IoPortRequest):
            value = controller.IoPortAccess(r)
            WritePioResponse(fd, value)
        else:
            print("Unknown Io request type")
            return 1

    return 0

def OoowsSerialEntry(cpufds):

    vmName = os.getenv("OOOWS_VM_NAME")
    if vmName == None:
        print("Cannot create serial pipes with VM Name")
        return 0

    controller = OoowsSerialController(vmName)

    threads = []
    for fd in cpufds:
        threads.append(Thread(target=HandleIO,
                              args=(controller, fd),
                              daemon=True))

    for thread in threads:
        thread.start()

    for thread in threads:
        thread.join()

    return 0

def main(argc, argv):

    # TODO: this can be in a class/library if we want more python devices
    cpufds = DeviceHandshake()
    if not len(cpufds) > 0:
        return 1

    OoowsSerialEntry(cpufds)

if __name__ == '__main__':
    sys.exit(main(sys.argv, len(sys.argv)))
