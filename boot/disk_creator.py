import sys
import math
import struct
import os

SECTOR_SIZE = 512

with open(sys.argv[1], 'rb') as f:
    boot_sector = f.read()

with open(sys.argv[2], 'rb') as f:
    kernel_code = f.read()

boot_sector = bytearray(boot_sector)

num_kernel_sectors_needed = math.ceil(len(kernel_code)/512.0)
boot_sector[497] = num_kernel_sectors_needed

boot_sector = bytes(boot_sector)
disk_pad = SECTOR_SIZE-(len(kernel_code)%SECTOR_SIZE)
kernel_code += b'\x00'*disk_pad

#print("total kernel sectors: %d" % (num_kernel_sectors_needed))
#print(disk_pad)
#print(len(kernel_code))
all_disk = boot_sector+kernel_code

with open("img/disk", 'wb+') as f:
    f.write(all_disk)
