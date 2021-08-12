import os
import mmap

class MemRange:
    def __init__(self, fd, start_addr, size):
        self.fd = fd
        self.start_addr = start_addr
        self.size = size
        self.end_addr = start_addr + size
        self.mem = mmap.mmap(fd, 0)

    def read(self, src_addr, size):
        offset = src_addr - self.start_addr
        end = offset + size
        data = self.mem[offset:end]
        return data

    def write(self, dest_addr, data):
        # write data to dest_addr
        offset = dest_addr - self.start_addr
        end = offset + len(data)
        self.mem[offset:end] = bytearray(data)

class MemoryManager:
    def __init__(self, meminfos):
        self.memranges = []
        for info in meminfos:
            fd = info['fd']
            start_addr = info['start_addr']
            size = os.fstat(fd).st_size
            self.memranges.append( MemRange(fd, start_addr, size) )

    def read(self, src_addr, size):
        for memrange in self.memranges:
            if src_addr >= memrange.start_addr and src_addr < memrange.end_addr:
                print("reading from fd: " + str(memrange.fd) + " at src_addr: " + hex(src_addr) + " of size: " + str(size))
                data = memrange.read(src_addr, size)
                return bytes(data)
        return -1

    def write(self, dest_addr, data):
        for memrange in self.memranges:
            if dest_addr >= memrange.start_addr and dest_addr < memrange.end_addr:
                memrange.write(dest_addr, data)
                return 0
        return -1



