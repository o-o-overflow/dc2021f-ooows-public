#! /usr/bin/env python3

import shutil
from typing import Optional

import cbor2
import libnacl.public
import libnacl.sealed
import libnacl.utils

from pwn import *

DIRECTORY = os.environ.get("VMM_DIRECTORY", "/app")
DEBUG_CONSOLE_PATH = os.environ.get("VMM_DEBUG_CONSOLE_PATH", "/app/ooowsserial-debug.py")
DISK_IMAGE_PATH = os.environ.get("VMM_DISK_IMAGE_PATH", "/app/disk")
OS_BOOTED = b"OOO OS BOOTED"

OGX_PUB = b"\x87P>y8\xf7\xac\xa01\xcb\x9c\xd0>qv\xe9j\xe7,K@\xa5\x98\xf5Q\x12\xa8\xf7\x06O#s"


class LoadEvent(object):
    def __init__(self, enclave_id, enclave_code, enclave_data):
        assert (isinstance(enclave_id, int))
        assert (isinstance(enclave_code, list))
        assert (isinstance(enclave_data, list))
        self.enclave_id = enclave_id
        self.enclave_code = enclave_code
        self.enclave_data = enclave_data

    def encrypt(self, sender: libnacl.public.SecretKey, receiver: libnacl.public.PublicKey):
        p = dict(t="load", i=self.enclave_id, c=self.enclave_code, d=self.enclave_data)
        n = libnacl.utils.rand_nonce()
        c = libnacl.crypto_box(cbor2.dumps(p), n, receiver.pk, sender.sk)
        return cbor2.dumps(dict(s=list(sender.pk), m=list(c), n=list(n)))


class ResponseEvent(object):
    def __init__(self, enclave_id, response_data):
        assert (isinstance(enclave_id, int))
        assert (isinstance(response_data, list))
        self.enclave_id = enclave_id
        self.response_data = response_data

    def encrypt(self, sender: libnacl.public.SecretKey, receiver: libnacl.public.PublicKey):
        p = dict(t="response", i=self.enclave_id, r=self.response_data)
        n = libnacl.utils.rand_nonce()
        c = libnacl.crypto_box(cbor2.dumps(p), n, receiver.pk, sender.sk)
        return cbor2.dumps(dict(s=list(sender.pk), m=list(c), n=list(n)))


def decrypt_event(ogx_pub: libnacl.public.PublicKey, tester_key: libnacl.public.SecretKey, message):
    c_object = cbor2.loads(message)
    sender = bytes(c_object["s"])
    # assert (tester_key.pk == sender)
    c = bytes(c_object["m"])
    n = bytes(c_object["n"])
    p = libnacl.crypto_box_open(c, n, ogx_pub.pk, tester_key.sk)
    p_object = cbor2.loads(p)
    if p_object["t"] == "load":
        return LoadEvent(int(p_object["i"]), p_object["c"], p_object["d"])
    elif p_object["t"] == "response":
        return ResponseEvent(int(p_object["i"]), p_object["r"])
    else:
        raise Exception("unknown event")


def run_enclave(p,
                ogx_pub: libnacl.public.PublicKey,
                tester_key: libnacl.public.SecretKey,
                e: bytes) -> Optional[ResponseEvent]:
    log.info("sending an enclave ({} bytes)".format(len(e)))
    p.send(p16(len(e)))
    p.send(e)

    log.debug("waiting for response")
    r_size = p.recvline(timeout=5)
    if r_size == b"":
        return None
    r_size = int(b"0x" + r_size, 16)
    log.info("reading enclave response ({} bytes)".format(r_size))
    r = bytearray()
    for i in range(r_size):
        x = int(b"0x" + p.recvline(), 16)
        r.append(x)

    r = decrypt_event(ogx_pub, tester_key, r)
    assert (isinstance(r, ResponseEvent))
    log.info("received response: {}".format(r.response_data))
    return r


# noinspection PyBroadException
def main():
    try:
        os.chdir(DIRECTORY)

        # Generate and load keys
        log.info("loading tester keypair and device public key")
        tester_key = libnacl.public.SecretKey()
        ogx_pub = libnacl.public.PublicKey(OGX_PUB)

        log.info("launching the vmm and waiting for ready status")
        shutil.copyfile(DEBUG_CONSOLE_PATH, "./devices-bin/ooowsserial.py")
        vmm_args = ["./vmm", "test", DISK_IMAGE_PATH, "1", "devices.config"]
        p = process(vmm_args)
        p.recvuntil(OS_BOOTED)

        log.info("starting the ogx patch testing function")
        p.sendline("test_ogx_patch")
        p.recvuntil("test_ogx_patch\r\n")

        # Load payloads to perform some simple computation in non-flag enclaves
        log.info("testing whether enclaves are working")
        for enclave in range(1, 8):
            e_code = list(b"BBYOOOGX\xb9\x00\x00\x00\x00H\x83\xf9@s\t\x804\n~H\xff\xc1\xeb\xf1\xb8@\x00\x00\x00\xc3")
            e_data = [ord(x) for x in random.sample(string.ascii_letters * 4, 64)]
            log.debug("data: {}".format(e_data))
            log.debug("expected result: {}".format([x ^ 0x7e for x in e_data]))
            e = LoadEvent(enclave, e_code, e_data).encrypt(tester_key, ogx_pub)
            r = run_enclave(p, ogx_pub, tester_key, e)
            if not r:
                print("PUBLIC: no results from enclave")
                sys.exit(-1)

            # Validate the result
            if len(r.response_data) != len(e_data):
                print("PUBLIC: enclave computation corrupted")
                sys.exit(-1)

            for i, x in enumerate(r.response_data):
                if (x ^ 0x7e) != e_data[i]:
                    print("PUBLIC: enclave computation corrupted")
                    sys.exit(-1)

        # Trying to evict the flag enclave should not kill the device
        # log.info("evicting enclave 0")
        # e_code = list(
        #     b"BBYOOOGXL\x8d-\x08\x00\x00\x00\xb8\x01\x00\x00\x00H\x8b\x18H\xc7\xc0\xff\xff\xff\xff\xc3\xcc\xf4")
        # e_data = list()
        # e = LoadEvent(7, e_code, e_data).encrypt(tester_key, ogx_pub)
        # run_enclave(p, ogx_pub, tester_key, e)

        # log.debug("waiting for flag enclave response")
        # r_size = p.recvline(timeout=15)
        # if r_size == b"":
        #     print("PUBLIC: enclave stability test 1 failed")
        #     sys.exit(-1)
        # r_size = int(b"0x" + r_size, 16)
        # log.info("reading flag enclave response ({} bytes)".format(r_size))
        # r = bytearray()
        # for i in range(r_size):
        #     x = int(b"0x" + p.recvline(), 16)
        #     r.append(x)
        # r = decrypt_event(ogx_pub, tester_key, r)
        # assert (isinstance(r, ResponseEvent))
        # log.info("received flag response: {}".format(r.response_data))

        log.info("testing that enclaves are still working")
        e_code = list(b"BBYOOOGX\xb8@\x00\x00\x00\xc3")
        e_data = list()
        e = LoadEvent(1, e_code, e_data).encrypt(tester_key, ogx_pub)
        r = run_enclave(p, ogx_pub, tester_key, e)
        if len(r.response_data) != 64:
            print("PUBLIC: stability test failed")
            sys.exit(-1)

        # Try to load something in the flag enclave to test whether eviction worked
        log.info("loading payload in enclave 0")
        e_code = list(b"BBYOOOGX\xb8@\x00\x00\x00\xc3")
        e_data = list()
        e = LoadEvent(0, e_code, e_data).encrypt(tester_key, ogx_pub)
        r = run_enclave(p, ogx_pub, tester_key, e)
        if r:
            print("PUBLIC: enclave management test failed")
            sys.exit(-1)

        p.kill()
    except:
        import traceback
        traceback.print_exc()
        sys.exit(-1)

    print("PUBLIC: ...sure, why not")


if __name__ == "__main__":
    main()
