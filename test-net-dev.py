#!/usr/bin/env python3
import os
import shutil
import sys
import time

from pwn import *

DIRECTORY = os.environ.get('VMM_DIRECTORY', '/app')

DEBUG_CONSOLE_PATH = os.environ.get('VMM_DEBUG_CONSOLE_PATH', '/app/ooowsserial-debug.py')
DISK_IMAGE_PATH = os.environ.get('VMM_DISK_IMAGE_PATH', '/app/disk')

OS_BOOTED = b"OOO OS BOOTED"

PREPARE_NET = b"prepare_net"

SET_DRIVER_FEATURES = b"net_set_driver_features"
ENABLE_CHKSUM_TX = b"net_enable_chksum_tx"
DISABLE_CHKSUM_TX = b"net_disable_chksum_tx"
ENABLE_ETH_CRC_TX = b"net_enable_tx_eth"
DISABLE_ETH_CRC_TX = b"net_disable_tx_eth"
ENABLE_PROMISC = b"net_enable_promisc"
DISABLE_PROMISC = b"net_disable_promisc"

TX_NORMAL_PACKET = b"tx_pkt"
TX_IPV4_PACKET = b"tx_ipv4_pkt"
RX_PACKET = b"rx_pkt"

LISTEN_ONE_MSG_CMD = ["mosquitto_sub", "-C", "1", "-N", "-t", "VPC"]
SEND_ONE_MSG_CMD = ["mosquitto_pub", "-s", "-t", "VPC"]

NORMAL_PACKET = b"DSTMACSRCMACLN\x61TLIDFOTP11IPIPDTDTDTDTAAA"
NORMAL_PACKET_CRC = b"\x4d\x1a\x85\xa2"

IPV4_NORMAL_PACKET = b"DSTMACSRCMACLN\x45GTLIDFOTP__IPIPDTDTDTDTAA"
IPV4_NORMAL_PACKET_CRC = b"\x7b\xa2\x62\xfa"

IPV4_CHKSUM_PACKET = b"DSTMACSRCMACLN\x45GTLIDFOTP<tIPIPDTDTDTDTAA"

BROADCAST_PACKET =b"TAAG\xff\xff\xff\xff\xff\xffSRCMACBodyOfPacketYo"
NON_BROADCAST_PACKET =b"TAAG\x11\x11\x11\x11\x11\xffSRCMACBodyOfPacketYo"


# context.log_level = 'debug'

def check_pkt_tx(p, cmd, correct):
    # Try to receive a packet
    listen = process(LISTEN_ONE_MSG_CMD)

    p.sendline(cmd)

    received_packet = listen.readall()

    assert(len(received_packet) > 4)
    pkt_data = received_packet[4:]

    if len(pkt_data) != len(correct):
        print("PUBLIC: packet tx fail: incorrect size")
        sys.exit(-1)

    if pkt_data != correct:
        print("PUBLIC: packet tx fail: incorrect data received")
        sys.exit(-1)

    listen.kill()

def check_pkt_rx(p, cmd, pkt, expected):
    p.sendline(cmd)
    p.recvuntil(cmd)

    # give it time to call into the driver
    time.sleep(10)

    send = process(SEND_ONE_MSG_CMD)
    send.send(pkt)
    send.shutdown()

    hex_size_regex = b"([0-9a-bA-B]+)\n"
    result = p.recvregex(hex_size_regex)
    match = re.search(hex_size_regex, result)
    size_hex = match.group(1)

    size = int(size_hex, 16)

    if size != len(expected):
        print("PUBLIC: packet rx fail: incorrect size")
        sys.exit(-1)

    raw_pkt = p.recvn(size)

    if raw_pkt != expected:
        print("PUBLIC: packet rx fail: incorrect data received")
        sys.exit(-1)

    send.kill()

def main():
    os.chdir(DIRECTORY)

    # Check if we need to run mosquitto server
    mosquitto = None
    if not 'DO_NOT_START_MOSQUITTO' in os.environ:
        mosquitto = process(["/usr/sbin/mosquitto", "-c", "/etc/mosquitto/mosquitto.conf"])

    shutil.copyfile(DEBUG_CONSOLE_PATH, "./devices-bin/ooowsserial.py")

    p = process(["./vmm", "test", DISK_IMAGE_PATH, "1", "devices.config"])

    p.recvuntil(OS_BOOTED)

    # important, need to initialize the structures
    p.recvuntil(b'$')
    p.sendline(PREPARE_NET)

    p.recvuntil(b'$')
    p.sendline(SET_DRIVER_FEATURES)

    p.recvuntil(b'$')
    check_pkt_tx(p, TX_NORMAL_PACKET, NORMAL_PACKET)

    p.recvuntil(b'$')
    check_pkt_tx(p, TX_IPV4_PACKET, IPV4_NORMAL_PACKET)

    p.recvuntil(b'$')
    p.sendline(ENABLE_CHKSUM_TX)

    p.recvuntil(b'$')
    check_pkt_tx(p, TX_IPV4_PACKET, IPV4_CHKSUM_PACKET)

    # Verify that non IPv4 packets are not affected
    p.recvuntil(b'$')
    check_pkt_tx(p, TX_NORMAL_PACKET, NORMAL_PACKET)

    p.recvuntil(b'$')
    p.sendline(DISABLE_CHKSUM_TX)

    p.recvuntil(b'$')
    p.sendline(ENABLE_ETH_CRC_TX)

    p.recvuntil(b'$')
    check_pkt_tx(p, TX_NORMAL_PACKET, NORMAL_PACKET + NORMAL_PACKET_CRC)

    p.recvuntil(b'$')
    check_pkt_tx(p, TX_IPV4_PACKET, IPV4_NORMAL_PACKET + IPV4_NORMAL_PACKET_CRC)

    p.recvuntil(b'$')
    check_pkt_rx(p, RX_PACKET, BROADCAST_PACKET, BROADCAST_PACKET[4:])

    p.recvuntil(b'$')
    p.sendline(ENABLE_PROMISC)

    p.recvuntil(b'$')
    check_pkt_rx(p, RX_PACKET, NON_BROADCAST_PACKET, NON_BROADCAST_PACKET[4:])

    p.kill()

    if mosquitto:
        mosquitto.kill()



if __name__ == '__main__':
    main()
