#!/usr/bin/env python3
import os
import sys
import mmap
import struct
from pyutils.memorymanager import MemoryManager

IOPORT = 0
MMIO   = 1

IO_DIR_READ = 0
IO_DIR_WRITE = 1

VMM_FD = 3
RAM_FD = 4
SYS_FD = 5
PIC_FD = 6

DISK_IRQ = 3

class IoPortRequest:
    def __init__(self, payload):
        self.port, self.direction, self.size, self.data, self.count = \
            struct.unpack("<HBBII", payload)

    def __str__(self):
        return 'Pio { %x %x %x }' % (self.port, self.data, self.size)

class OoowsDiskController():
    OOOWS_SECTOR     = 0x90
    OOOWS_DESTADDR   = 0x91
    OOOWS_SECTORSIZE = 0x92
    OOOWS_SECTORCNT  = 0x93
    OOOWS_DO_READ    = 0x94
    OOOWS_DO_WRITE   = 0x95

    def __init__(self, backingFile):
        # TODO: would prefer to take advantage of inheritance here
        meminfos = [
            {'fd' : RAM_FD, 'start_addr' : 0x0},
            {'fd' : SYS_FD, 'start_addr' : 0x100000}
        ]
        self.ram = MemoryManager(meminfos)
        self.backingFilename = backingFile
        self.handle = open(backingFile, 'rb+')
        self.filesize = os.stat(backingFile).st_size
        self.sector = 0
        self.destAddr = 0
        self.sectorCount = 0
        self.sectorSize = 512
        self.irq = DISK_IRQ

    def AssertIrq(self):
        os.write(PIC_FD, struct.pack("<I", self.irq))

    def IoPortRead(self, r):
        if r.port == self.OOOWS_SECTOR:
            return self.sector
        elif r.port == self.OOOWS_DESTADDR:
            return self.destAddr
        elif r.port == self.OOOWS_SECTORSIZE:
            return self.sectorSize
        elif r.port == self.OOOWS_SECTORCNT:
            return self.sectorCount

        return 0

    def IoPortWrite(self, r):
        if r.port == self.OOOWS_SECTOR:
            self.sector = r.data
        elif r.port == self.OOOWS_DESTADDR:
            self.destAddr = r.data
        elif r.port == self.OOOWS_SECTORSIZE:
            self.sectorSize = r.data
        elif r.port == self.OOOWS_SECTORCNT:
            self.sectorCount = r.data
        elif r.port == self.OOOWS_DO_READ:
            # seek to sector
            self.handle.seek(self.sectorSize * self.sector)

            # read out sectorCount
            data = self.handle.read(self.sectorSize * self.sectorCount)
            # write to the mem
            self.ram.write(self.destAddr, data)

        elif r.port == self.OOOWS_DO_WRITE:
            off  = self.sectorSize * self.sector
            size = self.sectorSize * self.sectorCount

            # writing outside the bounds of the disk is invalid
            if off + size < self.filesize:
                self.handle.seek(off)

                data = self.ram.read(self.destAddr, size)
                self.handle.write(data)

                self.handle.flush()

            self.AssertIrq()

        return 0

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

def ReadIoRequest():
    raw = VmmRead(28)
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
def WritePioResponse(value):
    VmmWrite(struct.pack("<I", value))

def DeviceHandshake():
    VmmWrite(b"INIT")
    if VmmRead(4) != b"TINI":
        return False
    VmmRead(16)

    return True

def OoowsDiskEntry():

    virtDiskPath = os.getenv("OOOWS_VM_VIRTDISK")
    if virtDiskPath == None:
        print("No virtdisk to attach")
        return 0

    controller = OoowsDiskController(virtDiskPath)

    while True:
        r = ReadIoRequest()
        if isinstance(r, IoPortRequest):
            value = controller.IoPortAccess(r)
            WritePioResponse(value)
        elif isinstance(r, MmioRequest):
            controller.MmioAccess(r)
        else:
            print("Unknown IO request type")
            return 1

    return 0;

def main(argc, argv):

    # TODO: this can be in a class/library if we want more python devices
    if not DeviceHandshake():
        return 1

    OoowsDiskEntry()

if __name__ == '__main__':
    sys.exit(main(sys.argv, len(sys.argv)))
