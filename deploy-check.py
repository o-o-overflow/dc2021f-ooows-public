#!/usr/bin/env python3
import os
import sys
import shutil
import hashlib
import subprocess

source_project = b"dc2021f-ooows-vmm"

projects = [b"dc2021f-ooows-broadcooom",
            b"dc2021f-ooows-flag-baby",
            b"dc2021f-ooows-p92021",
            b"dc2021f-ooows-ogx"]

BIOS_PATH = b"bios/bios"
VMM_PATH = b"vmm"
VGA_PATH = b"devices-bin/vga"
SERIAL_PATH = b"devices-bin/ooowsdisk.py"
DISK_PATH = b"devices-bin/ooowsserial.py"

P9_CHALL = b"devices-bin/p9fs"
NET_CHALL = b"devices-bin/net"
OGX_CHALL = b"devices-bin/ogx"

CHALLS = [P9_CHALL, NET_CHALL, OGX_CHALL]

MUST_EXEC = [VMM_PATH, VGA_PATH, SERIAL_PATH, DISK_PATH] + CHALLS

components = [BIOS_PATH,
              VMM_PATH,
              VGA_PATH,
              SERIAL_PATH,
              DISK_PATH]

def hashpath(path):
    with open(path, 'rb') as f:
        data = f.read()
        result = hashlib.md5(data)
        return result.digest()

def check(project, component):

    os.chdir(project)

    h = hashpath(os.path.join(b'service', component))

    os.chdir("..")
    return h

def is_an_exe_line(line):
    for exe in MUST_EXEC:
        if exe in line:
            return True

    return False

def is_a_web_line(line):
    return b"service/web" in line

def check_exec(project, component):

    os.chdir(project)

    p = subprocess.Popen(["git", "ls-files", "-s"], stdout=subprocess.PIPE)
    output = p.stdout.read()
    for line in output.split(b"\n"):
        if is_an_exe_line(line) and not is_a_web_line(line):
            mode = int(line.split(b" ")[0])
            mode &= 0xfff
            if mode != 2451:
                print("Bad perms on for %s in %s!"%(line, project))
                return False

    os.chdir('..')
    return True

def check_devices_bin(project, component):

    bins = os.path.join(project, b'service', b'devices-bin')

    cwd = os.getcwd()
    os.chdir(bins)
    for entry in os.listdir('.'):
        print(os.stat(entry))

    os.chdir(cwd)
    return

def check_exec_all():
    for project in projects:
        for component in components:
            check_exec(project, component)


    return True
        

def checkall(projects):
    ret = True
    
    bios_h = []
    vmm_h = []
    vga_h = []
    serial_h = []
    disk_h = []
    for project in projects:
        bios_h.append(check(project, BIOS_PATH))
        vmm_h.append(check(project, VMM_PATH))
        vga_h.append(check(project, VGA_PATH))
        serial_h.append(check(project, SERIAL_PATH))
        disk_h.append(check(project, DISK_PATH))

    bh = bios_h[0]
    if not all(map(lambda h: h == bh, bios_h)):
        print("FAILED: not all bioses are the same")
        ret = False

    vh = vmm_h[0]
    if not all(map(lambda h: h == vh, vmm_h)):
        print("FAILED: not all vmms are the same")
        ret = False
        
    gh = vga_h[0]
    if not all(map(lambda h: h == gh, vga_h)):
        print("FAILED: not all vgas are the same")
        ret = False
        
    sh = serial_h[0]
    if not all(map(lambda h: h == sh, serial_h)):
        print("FAILED: not all serials are the same")
        ret = False

    dh = disk_h[0]
    if not all(map(lambda h: h == dh, disk_h)):
        print("FAILED: not all disks are the same")
        ret = False
        

    return ret
        
def fixup(project):
    for component in components:
        good_path = os.path.join(source_project, component)
        bad_path = os.path.join(project, b'service', component)

        shutil.copyfile(good_path, bad_path)
        

def fixup_all():
    for project in projects:
        fixup(project)

def main(args):
    print("WARNING: run me from the ooows-vmm directory!!")

    os.chdir("..")

    if not checkall(projects):
        fixup_all()
    else:
        print("All binaries are the same across all services!")
        if (len(args) > 1 and args[1] == "force"):
            print("Forcing copy!")
            fixup_all()

    if not check_exec_all():
        print("Files were not executable")

    if not checkall(projects):
        print("Couldn't fixup the services!!")
    else:
        print("Fixup succeeded!")
    

if __name__ == '__main__':
    main(sys.argv)
