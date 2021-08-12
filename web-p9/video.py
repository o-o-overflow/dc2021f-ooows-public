import os
import asyncio
import functools
import sys
import socket
import struct
import json
import binascii
from multiprocessing import shared_memory

VMS_DIR = "/tmp/vms/"
IMG_SAVE_PATH = "web/static/imgs/"
SHM_TEXT_BASENAME = "vga-shm-text"
SHM_VIDEO_BASENAME = "vga-shm-video"

# NOTE: Apprently yellow is actually brown,
# and bright black is actually dark gray
# .. I think white will be lught gray and bright_white will be normal white
def parseTextMem(text_shm_name):
    text_shm = shared_memory.SharedMemory(text_shm_name)
    fcolor_map = {
            #0x0 : ["black", "\u001b[30m"],
            0x0 : ["black", ""],
            0x1 : ["blue", "\u001b[34m"],
            0x2 : ["green", "\u001b[32m"],
            0x3 : ["cyan", "\u001b[36m"],
            0x4 : ["red", "\u001b[31m"],
            0x5 : ["magenta", "\u001b[35m"],
            0x6 : ["brown", "\u001b[33m"],
            0x7 : ["light gray", "\u001b[37m"],
            0x8 : ["dark gray", "\u001b[30;1m"],
            0x9 : ["light blue", "\u001b[34;1m"],
            0xa : ["light green", "\u001b[32;1m"],
            0xb : ["light cyan", "\u001b[36;1m"],
            0xc : ["light red", "\u001b[31;1m"],
            0xd : ["light magenta", "\u001b[35;1m"],
            0xe : ["yellow", "\u001b[33;1m"],
            0xf : ["white", "\u001b[37;1m"]
            }
    bcolor_map = {
            #0x0 : ["black", "\u001b[40m"],
            0x0 : ["black", ""],
            0x1 : ["blue", "\u001b[44m"],
            0x2 : ["green", "\u001b[42m"],
            0x3 : ["cyan", "\u001b[46m"],
            0x4 : ["red", "\u001b[41m"],
            0x5 : ["magenta", "\u001b[45m"],
            0x6 : ["brown", "\u001b[43m"],
            0x7 : ["light gray", "\u001b[47m"],
            0x8 : ["dark gray", "\u001b[40;1m"],
            0x9 : ["light blue", "\u001b[44;1m"],
            0xa : ["light green", "\u001b[42;1m"],
            0xb : ["light cyan", "\u001b[46;1m"],
            0xc : ["light red", "\u001b[41;1m"],
            0xd : ["light magenta", "\u001b[45;1m"],
            0xe : ["yellow", "\u001b[43;1m"],
            0xf : ["white", "\u001b[47;1m"]
            }
    reset = "\u001b[0m"
    space = ord(' ')
    #TODO: blinking
    term_str = ""
    buf = text_shm.buf
    for x in range(0, buf.nbytes, 2):
        short = struct.unpack("h", buf[x:x+2].tobytes())[0]
        cbytes = short & 0xff
        # replace null bytes with space so our chars end up at the right place
        if (cbytes == 0):
            cbytes = space
        abytes = short >> 8
        fcolor = abytes & 0xf
        bcolor = abytes >> 4
        term_str += bcolor_map[bcolor][1] + fcolor_map[fcolor][1] + chr(cbytes) + reset

    text_shm.close()
    return term_str

def parseVideoMem(video_shm_name):
    video_shm = shared_memory.SharedMemory(video_shm_name)
    header = b'BM6\x14\x00\x00\x00\x00\x00\x006\x04\x00\x00(\x00\x00\x00@\x01\x00\x00\xc8\x00\x00\x00\x01\x00\x08\x00\x00\x00\x00\x00\x00\x10\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00\x80\x00\x00\x00\x00\x80\x00\x00\x80\x80\x00\x00\x00\x00\x80\x00\x80\x00\x80\x00\x00@\x80\x00\xc0\xc0\xc0\x00\x80\x80\x80\x00\xff\x00\x00\x00\x00\xff\x00\x00\xff\xff\x00\x00\x00\x00\xff\x00\xff\x00\xff\x00\x00\xff\xff\x00\xff\xff\xff'
    header += b'\x00'*961
    buf = video_shm.buf
    bitmap = bytearray()
    for x in range(0, buf.nbytes):
        bitmap.append(buf[x])

    new_bmp = header + bitmap
    # update file size in header
    new_bmp = bytearray(new_bmp)
    new_bmp[2:6] = struct.pack("I", len(new_bmp))
    video_shm.close()
    return bytes(bytes(new_bmp))

def videoTx(vmname, socketio, vmid):

    text_shm_name = SHM_TEXT_BASENAME + '-' + vmname
    video_shm_name = SHM_VIDEO_BASENAME + '-' + vmname

    txPath = os.path.join(VMS_DIR, vmname, "vga-notifs")

    sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    sock.connect(txPath)

    while True:
        data = sock.recv(1)
        if len(data) > 0:
            #print("GOT DATA OF LEN")
            update_type = data
            # update
            # text update
            if (update_type == b't'):
                term_str = parseTextMem(text_shm_name)
                msg = {'mode':'text', 'data':term_str}
                socketio.emit(f"update-{vmid}", {'mode':'text', 'data':term_str})
            # video update
            elif (update_type == b'v'):
                #print("VIDEO UPDATE")
                bmp_blob = parseVideoMem(video_shm_name)
                encoded = binascii.hexlify(bmp_blob).decode('utf-8')
                msg = {'mode':'video', 'data':encoded}
                socketio.emit(f"update-{vmid}", {'mode':'video', 'data':encoded})
            else:
                # ERROR
                continue
        elif (len(data)==0):
            #print("Done with video")
            return

def refresh(socketio, vmname, vmid):
    text_shm_name = SHM_TEXT_BASENAME + '-' + vmname
    video_shm_name = SHM_VIDEO_BASENAME + '-' + vmname

    mode = None

    with open(os.path.join(VMS_DIR, vmname, 'vga-notifs-cache'), 'rb') as f:
        mode = f.read()

    if mode == b't':
        term_str = parseTextMem(text_shm_name)
        msg = {'mode':'text', 'data':term_str}
        socketio.emit(f"update-{vmid}", msg)
    elif mode == b'v':
        bmp_blob = parseVideoMem(video_shm_name)
        encoded = binascii.hexlify(bmp_blob).decode('utf-8')
        msg = {'mode':'video', 'data':encoded}
        socketio.emit(f"update-{vmid}", msg)

async def videoHandler(websocket, path, vmname):
    producer_task = asyncio.ensure_future(
        videoTx(websocket, path, vmname))
    done, pending = await asyncio.wait(
        [producer_task],
        return_when=asyncio.FIRST_COMPLETED,
    )
    for task in pending:
        task.cancel()

def serve(vmname, socketio, vmid):
    videoTx(vmname, socketio, vmid)
    #print("Done serving")
