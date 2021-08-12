# Frontend for vmm

import os
import queue
import shutil
import socket

import console
import video

from subprocess import Popen
from threading import Thread

env_or_default = lambda e, d: os.getenv(e) if os.getenv(e) else d

VMM_BIN = "./vmm"
VMM_DIR = os.path.join(os.path.dirname(os.path.realpath(__file__)), "../")

VM_STORE_DIR = env_or_default("OOOWS_VM_STORE_DIR", "/tmp/vms/")
DISKS_DIR = "/tmp/disks/"

class VmmWorker():
    def __init__(self, name=None):
        self.running = False
        self.process = None

        # VM attributes, not set until 'start'ed
        self.name = None

        # IoThreads, not set until view/console is requested
        self.consoleTxThread = None
        self.consoleRxThread = None
        self.consoleRxThreadQueue = None
        self.consolePort = 0
        # IoThreads, not set until view/vga is requested
        self.videoThread = None

        if not name is None:
            self.new(name)

    def new(self, name, disk=None):
        # disk is a Flask file object
        self.name = name

        if not os.path.exists(VM_STORE_DIR):
            os.mkdir(VM_STORE_DIR, 0o700)

        vmdir = os.path.join(VM_STORE_DIR, self.name)
        if not os.path.exists(vmdir):
            os.mkdir(vmdir, 0o700)

        to_disk = os.path.join(vmdir, "disk")
        if not disk is None:
            os.rename(disk, to_disk)

    def delete(self):

        vmdir = os.path.join(VM_STORE_DIR, self.name)
        shutil.rmtree(vmdir)

    def start(self):
        diskpath = os.path.join(VM_STORE_DIR, self.name, "disk")

        self.process = Popen([VMM_BIN, self.name, diskpath, "2", "devices.config"],
                             cwd=VMM_DIR)

    def stop(self):
        assert not self.process is None
        self.process.terminate()
        self.videoThread = None
        self.consoleTxThread = None
        self.consoleRxThread = None
        # Signal to the rx thread that we're done here
        self.txConsoleData(None)

    def stopped(self):
        if not self.process is None:
            return not self.process.poll() is None
        return True

    def txConsoleData(self, message):
        if self.consoleRxThreadQueue:
            self.consoleRxThreadQueue.put(message)

    def console(self, socketio, vmid):
        assert not self.name is None

        if self.consoleTxThread is None:
            self.consoleTxThread = socketio.start_background_task(target=console.consoleTx,
                                                                  vmname=self.name,
                                                                  socketio=socketio,
                                                                  vmid=vmid)
        if self.consoleRxThread is None:
            self.consoleRxThreadQueue = queue.Queue()
            self.consoleRxThread = socketio.start_background_task(target=console.consoleRx,
                                                                  vmname=self.name,
                                                                  rx_queue=self.consoleRxThreadQueue)


    def video(self, socketio, vmid):
        print(f"Creating video {self.videoThread} {self.name}")
        if not self.videoThread is None:
            # refresh the client with the vga cache contents
            video.refresh(socketio, self.name, vmid)
            return None

        if self.name is None:
            # indicate error here, shouldn't be able to come here
            return None


        # Use whatever threading support that socketio states
        self.videoThread = socketio.start_background_task(target=video.serve,
                                                          vmname=self.name,
                                                          vmid=vmid,
                                                          socketio=socketio)
