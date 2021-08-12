#!/usr/bin/env python3
import os
import shutil
import sys

from pwn import *

DIRECTORY = os.environ.get('VMM_DIRECTORY', '/app')

DEBUG_CONSOLE_PATH = os.environ.get('VMM_DEBUG_CONSOLE_PATH', '/app/ooowsserial-debug.py')
DISK_IMAGE_PATH = os.environ.get('VMM_DISK_IMAGE_PATH', '/app/disk')

OS_BOOTED = b"OOO OS BOOTED"

P9_PREPARE = b"prepare_p9"
P9_VERSION = b"p9_version"
P9_ATTACH = b"p9_attach"
P9_CREATE_FILE = b"p9_create_file"
P9_CREATE_DIR = b"p9_create_dir"
P9_WRITE_FILE = b"p9_write_file"
P9_READ_FILE = b"p9_read_file"
P9_WALK_TO = b"p9_walk"
P9_REMOVE_FILE = b"p9_remove"
P9_STAT_FILE = b"p9_stat"
P9_WALK_REL = b"p9_walk_rel"
P9_CLUNK = b"p9_clunk"

CONTENTS = b"Lorem ipsum blah blah blah who cares"
FAIL = b"FAILED"
P9_VERSION_STR = b"P92021"

def checked_recvuntil(p, s):
    o = p.recvuntil(s)
    print(o)
    return FAIL in o

def expected_directory_structure(path):
    l1, l2, l3 = os.walk(path)
    structure = l1[1] == [['alpha', 'left']] and\
        l2[1] == [['beta']] and\
        l3[1] == [[]]

    content = open(os.path.join(path, 'left'), 'rb').read() == "The final test..."
    return structure and content

def main():
    os.chdir(DIRECTORY)

    shutil.copyfile(DEBUG_CONSOLE_PATH, "./devices-bin/ooowsserial.py")

    # attempt to remove test's runtime store before running test
    if os.path.exists("/tmp/vms/test"):
        shutil.rmtree("/tmp/vms/test")

    p = process(["./vmm", "test", DISK_IMAGE_PATH, "1", "devices.config"])

    p.recvuntil(OS_BOOTED)

    p.recvuntil(b'$')

    p.sendline(P9_PREPARE)
    p.recvuntil(b'$')

    p.sendline(P9_VERSION)

    output = p.recvuntil(b'$')
    if not P9_VERSION_STR in output:
        print("PUBLIC: expected version 'P92021'")
        sys.exit(-1)
        

    # this returns a fid for us to use, we'll assume it's 1
    p.sendline(P9_ATTACH + b" share")
    if checked_recvuntil(p, b'$'):
        print("PUBLIC: failed to attach to 'share'")
        sys.exit(-1)

    p.sendline(P9_WALK_TO + b" ..")
    if checked_recvuntil(p, b'$'):
        print("PUBLIC: failed walk to '.' from '/'")
        sys.exit(-1)

    p.sendline(P9_CREATE_DIR + b" alpha")
    if checked_recvuntil(p, b'$'):
        print("PUBLIC: failed creation of directory in '/'")
        sys.exit(-1)

    p.sendline(P9_CREATE_DIR + b" beta")
    if checked_recvuntil(p, b'$'):
        print("PUBLIC: failed creation of subdirectory")
        sys.exit(-1)

    p.sendline(P9_CREATE_FILE + b" write_test")
    if checked_recvuntil(p, b'$'):
        print("PUBLIC: failed creation of file in subdirectory")
        sys.exit(-1)

    p.sendline(P9_WRITE_FILE + b" " + CONTENTS)
    if checked_recvuntil(p, b'$'):
        print("PUBLIC: failed to write to file")
        sys.exit(-1)

    checks = [ ]
    for _ in range(5):
        p.sendline(P9_READ_FILE)
        check = p.recvuntil(b'$')
        checks.append(CONTENTS in check)

    print(checks)
    if not any(checks):
        print("PUBLIC: failed to read back file")
        sys.exit(-1)

    p.sendline(P9_CLUNK)
    if checked_recvuntil(p, b'$'):
        print("PUBLIC: failed to close file")
        sys.exit(-1)
    
    p.sendline(P9_WALK_TO + b" ..")
    if checked_recvuntil(p, b'$'):
        print("PUBLIC: failed to traverse to '/'")
        sys.exit(-1)

    p.sendline(P9_CREATE_FILE + b" left")
    if checked_recvuntil(p, b'$'):
        print("PUBLIC: failed to create file in '/'")
        sys.exit(-1)

    p.sendline(P9_WRITE_FILE + b" The final test...") 
    if checked_recvuntil(p, b'$'):
        print("PUBLIC: failed to write file")
        sys.exit(-1)

    p.sendline(P9_CLUNK)
    if checked_recvuntil(p, b'$'):
        print("PUBLIC: failed to write file")
        sys.exit(-1)

    p.sendline(P9_WALK_TO + b" ..")
    if checked_recvuntil(p, b'$'):
        print("PUBLIC: failed to walk to '/'")
        sys.exit(-1)

    p.sendline(P9_WALK_REL + b" alpha")
    if checked_recvuntil(p, b'$'):
        print("PUBLIC: failed to walk to directory")
        sys.exit(-1)

    p.sendline(P9_WALK_REL + b" beta")
    if checked_recvuntil(p, b'$'):
        print("PUBLIC: failed to walk to subdirectory")
        sys.exit(-1)

    p.sendline(P9_WALK_REL + b" write_test")
    if checked_recvuntil(p, b'$'):
        print("PUBLIC: failed to walk to file")
        sys.exit(-1)

    p.sendline(P9_REMOVE_FILE)
    if checked_recvuntil(p, b'$'):
        print("PUBLIC: failed to remove file")
        sys.exit(-1)

    if expected_directory_structure("/tmp/vms/test/9pshare/"):
        print("PUBLIC: unexpected directory structure created")
        sys.exit(-1)

if __name__ == '__main__':
    main()
