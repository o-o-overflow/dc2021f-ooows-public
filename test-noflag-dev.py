#!/usr/bin/env python3
import os
import shutil
import sys

from pwn import *

DIRECTORY = os.environ.get('VMM_DIRECTORY', '/app')

DEBUG_CONSOLE_PATH = os.environ.get('VMM_DEBUG_CONSOLE_PATH', '/app/ooowsserial-debug.py')
DISK_IMAGE_PATH = os.environ.get('VMM_DISK_IMAGE_PATH', '/app/disk')

OS_BOOTED = b"OOO OS BOOTED"

NOFLAG_FILE = b"noflag_file"

TEST1_TXT = b"ABCDEFGHIJ"
TEST2_TXT = b"QRSTUVWXYZ"

def main():
    os.chdir(DIRECTORY)

    shutil.copyfile(DEBUG_CONSOLE_PATH, "./devices-bin/ooowsserial.py")

    p = process(["./vmm", "test", DISK_IMAGE_PATH, "1", "devices.config"])

    p.recvuntil(OS_BOOTED)

    p.recvuntil(b'$')

    with open("/tmp/noflxg_test", "wb") as f:
        f.write(TEST1_TXT)

    p.sendline(NOFLAG_FILE + b' /tmp/noflxg_test')

    output = p.recvuntil(b'$')
    if not TEST1_TXT in output:
        print("PUBLIC: unable to read contents of file")
        sys.exit(-1)

    with open("/tmp/noflxg_test2", "wb") as f:
        f.write(TEST2_TXT)

    p.sendline(NOFLAG_FILE + b' /tmp/noflxg_test2')
    output = p.recvuntil(b'$')
    if not TEST2_TXT in output:
        print("PUBLIC: unable to read contents of file")
        sys.exit(-1)

if __name__ == '__main__':
    main()

