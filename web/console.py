import asyncio
import functools
import os
import socket
import websockets

VMS_DIR = "/tmp/vms/"

def consoleRx(vmname, rx_queue):
    rxPath = os.path.join(VMS_DIR, vmname, "com-1-rx")
    sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    sock.connect(rxPath)

    while True:
        message = rx_queue.get()
        if message == None:
            return
        # print(f"sending {bytes(message, 'utf-8')}")
        sock.send(bytes(message, 'utf-8'))

def consoleTx(vmname, socketio, vmid):
    txPath = os.path.join(VMS_DIR, vmname, "com-1-tx")
    sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    sock.connect(txPath)

    while True:
        data = sock.recv(1)
        if len(data) > 0:
            socketio.emit(f"console-{vmid}", {'data': data.decode('utf-8')})
        elif len(data) == 0:
            # print("Done with console tx")
            return

async def consoleHandler(websocket, path, vmname):
    consumer_task = asyncio.ensure_future(
        consoleRx(websocket, path, vmname))
    producer_task = asyncio.ensure_future(
        consoleTx(websocket, path, vmname))
    done, pending = await asyncio.wait(
        [consumer_task, producer_task],
        return_when=asyncio.FIRST_COMPLETED,
    )
    for task in pending:
        task.cancel()

def serve(vmname, host, port):
    loop = asyncio.new_event_loop()
    asyncio.set_event_loop(loop)

    handler = functools.partial(consoleHandler, vmname=vmname)
    start_server = websockets.serve(handler, host, port)

    asyncio.get_event_loop().run_until_complete(start_server)
    asyncio.get_event_loop().run_forever()
