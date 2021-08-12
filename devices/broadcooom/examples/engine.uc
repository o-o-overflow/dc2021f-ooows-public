// Thread 0: rx_thread
// Thread 1: packet_processing
// Thread 2: packet_processing
// Thread 3: tx_thread

immed[reg_0, 0]
br!=ctx[reg_0, --, non_rx_threads]
rx_thread:
immed[reg_0, 0] // reg_0 is the idx in the RX_PACKET_BUFF

immed[reg_6, 0] // reg_6 is the idx of the RX_PACKET_PROCEESING_BUF

rx_thread_loop:
immed[reg_1, 0x2000] // reg_1 is the & of RX_PACKET_BUFF in mem
immed[reg_2, 1] // reg_2 is MASK_IS_USED

immed[reg_7, 34] // reg_7 is the & of the RX_PACKET_PROCEESING_BUF
immed[reg_8, 0x8000, <<16] // reg_8 is MASK_IS_READY

// Check the MASK_IS_USED of the current buffer
// Need to shift 9 times total
alu_shf[reg_3, --, B, reg_0, <<3]
alu_shf[reg_3, --, B, reg_3, <<3]
alu_shf[reg_3, --, B, reg_3, <<3] // reg_3 now has the actual idx of RX_PACKET_BUFF
immed[reg_4, 1]
alu[reg_4, --, B, reg_4]
ram[read, read_0, reg_3, reg_1, 0] ctx_swap
// read_0 has the is_used bit
immed[reg_4, 1]
br=[read_0, reg_4, rx_thread_loop] // If current rx_packet is in use, wait until it's free
alu[reg_3, reg_3, +, reg_4] // increment reg_3 to place where packets go
r_fifo_rd[reg_5, reg_3, reg_1] ctx_swap
// reg_5 has the size of the packet, which is at ram[reg_3+reg1]

// Add packet to tx packet processing queue
alu_shf[reg_9, --, B, reg_6, <<1]
alu[write_0, reg_5, ^, reg_8] // Set the ready bit with the size
alu[write_1, reg_3, +, reg_1] // Set packet size

// first write the size
immed[reg_4, 1]
alu[reg_7, reg_4, +, reg_7]
scratch[write, write_1, reg_9, reg_7, 1] ctx_swap
alu[reg_7, reg_7, -, reg_4]
scratch[write, write_0, reg_9, reg_7, 1] ctx_swap

// increment the tx packet processing index
immed[reg_4, 1]
alu[reg_6, reg_6, +4, reg_4]

// increment the index
immed[reg_4, 1]

alu[reg_0, reg_0, +8, reg_4]
br[rx_thread_loop]
non_rx_threads:
immed[reg_0, 1]
br=ctx[reg_0, --, packet_processing_first]
immed[reg_0, 2]
br=ctx[reg_0, --, packet_processing_second]
immed[reg_0, 3]
br=ctx[reg_0, --, tx_thread]

packet_processing_first:
immed[reg_0, 0]
br[packet_processing]

packet_processing_second:
immed[reg_0, 1]
br[packet_processing]

// reg_0 has the idx of the thread (used to split up work)
packet_processing:

alu[reg_1, --, B, reg_0] // reg_1 is the idx of the rx_ring_buff
immed[reg_2, 34] // reg_2 is the base address of rx_ring_buff
immed[reg_3, 0x8000, <<16] // reg_8 is MASK_IS_READY

packet_processing_loop:
alu_shf[reg_4, --, B, reg_1, <<1] // reg_4 has the actual index (structs of size 2)

immed[reg_5, 2]
alu[reg_5, --, B, reg_5]
scratch[read, read_0, reg_4, reg_2, 0] ctx_swap
// read_0 has is_ready/size_bytes and read_1 has &pkt
alu[reg_5, read_0, &, reg_3]
br!=[reg_5, reg_3, packet_processing_loop]

// Ethernet packet must be at least 14 bytes, ignore it if not 14 bytes
ld_field_w_clr[reg_5, 0b0111, read_0] // reg_5 has the packet size
immed[reg_6, 14]
br<[reg_5, reg_6, done_pkt]

// Check dst_mac, if not in PROMISC mode && not our MAC, then go to done_pkt

// First 6 bytes of the ethernet packet is dst_mac
// Next 6 bytes of the ethernet packet is src_mac
// Next 2 bytes is the payload size

// if PROMISC, go right to sending packet to core
csr[reg_8, CSR_0] // reg_8 has the standard CSR including the PROMISC bit
immed[reg_6, 0x1]
alu[reg_7, reg_6, &, reg_8]
br=[reg_7, reg_6, send_pkt_to_core]

csr[reg_6, CSR_1] // reg_6 has the upper 32 bits of the MAC
csr[reg_7, CSR_2] // reg_7 has the lower 16 bits of the MAC (in the upper 16 bits of reg_7)
immed[reg_9, 0]
immed[reg_10, 2]
alu[reg_10, --, B, reg_10]
ram[read, read_2, read_1, reg_9, 0] ctx_swap // read_2 now has the first 32 bits of pkt and read_3 has the next 32 bits
// check if the MAC is broadcast (all 1s)
immed[reg_9, 0xffff, <<16]
immed[reg_10, 0xffff]
ld_field[reg_9, 0b0011, reg_10]
alu[reg_11, reg_9, ^, read_2] // reg_11 will be 0 iff reg_9 == reg_2
ld_field_w_clr[reg_12, 0b0011, read_3] // reg_12 has just the lowest 16 bits as the mac address
alu[reg_13, reg_10, ^, reg_12] // reg_13 will be 0 iff reg_10 == reg_12 (i.e. reg_12 is all ones)
immed[reg_14, 0]
alu[reg_15, reg_14, ^, reg_11]
alu[reg_15, reg_15, ^, reg_13] // reg_14 will be 0 iff 0 == reg_13 == reg_11
br=[reg_14, reg_15, send_pkt_to_core]

// check that it matches our pkt
br!=[reg_6, read_2, done_pkt]
ld_field_w_clr[reg_9, 0b0011, read_3] // reg_9 has just the lowest 16 bits as the mac address
br!=[reg_7, reg_9, done_pkt]

// Otherwise, send pkt to core processor
send_pkt_to_core:

// check RX_ETH_CRC_CHECK_MASK, if set check the CRC, if it doesn't match chuck it
csr[reg_6, CSR_0]
immed[reg_7, 0x8]
alu[reg_8, reg_6, &, reg_7]
br!=[reg_8, reg_7, really_send_pkt_to_core]

// check the CRC32 of the packet, if it doesn't match the last four bytes chuck it
// TODO

really_send_pkt_to_core:
immed[reg_6, 0]
rx_pkt[reg_5, read_1, reg_6] ctx_swap

done_pkt:

// free the pkt itself
immed[reg_6, 1]
alu[reg_7, read_1, -, reg_6] // reg_7 points to &pkt - 1 (metadata about is_used)
immed[write_0, 0]
ram[write, write_0, reg_7, write_0, 1] ctx_swap // reg_7[0] = 0

// mark this rx_ring_buff as free
scratch[write, write_0, reg_4, reg_2, 1] ctx_swap // zero out current rx_ring_buff

// go to the next
immed[reg_6, 2]
alu[reg_1, reg_1, +4, reg_6]

br[packet_processing_loop]

tx_thread:
immed[reg_0, 0] // reg_0 is the idx in the TX_RING_BUFF
immed[reg_1, 2] // reg_1 is the & of the TX_RING_BUFF
immed[reg_2, 0x8000, <<16] // reg_2 is MASK_TO_SEND

tx_thread_loop:
// Wait until something is ready for transmission
alu_shf[reg_5, --, B, reg_0, <<1]
scratch[read, read_0, reg_5, reg_1, 2] ctx_swap
// read_0 has is_sent/size_bytes and read_1 has &pkt
alu[reg_3, read_0, &, reg_2] // reg_3 has the result
br!=[reg_3, reg_2, tx_thread_loop] // if bit not set, jump back

// Copy just the lower 3 bytes to register as size
ld_field_w_clr[reg_3, 0b0111, read_0] // reg_3 has the packet size

// TX ETH CRC OFFLOAD (if CSR bit is set)
csr[reg_4, CSR_0] // reg_4 has the CSR including the TX_ETH_CRC_OFFLOAD_MASK
immed[reg_12, 0x10]
alu[reg_6, reg_12, &, reg_4]
br!=[reg_6, reg_12, ip_checksum_offloading]

// lol sorry, we only operate on word-aligned packets, don't wanna deal with that mess
immed[reg_4, 2]

// Calculate the CRC32 checksum of the packet
ld_field_w_clr[reg_4, 0b1111, reg_3] // reg_4 has the counter of bytes to process
immed[reg_12, 0] // reg_12 will have the CRC32
alu[reg_12, --, ~B, reg_12] // crc32 = ~0

start_crc_loop:
immed[reg_6, 2]
alu[reg_6, reg_4, >>, reg_6] // reg_6 is number of words left

immed[reg_7, 0]
br<=[reg_6, reg_7, append_crc_checksum]

// Load in as much memory of min(#bytesleft, 2*4)
immed[reg_7, 8]
br>=[reg_4, reg_7, done_min]
alu[reg_7, --, B, reg_6]

done_min:

// calc idx to fetch
alu_shf[reg_8, reg_3, -, reg_4, >>2] // reg_8 has the idx
alu_shf[--, --, B, reg_7, >>2]
ram[read, read_2, read_1, reg_8, 0] ctx_swap // read in up to 2 registers, starting at read_2 at &pkt+idx

// do the CRC32 on read_2 and read_3
immed[reg_6, 0x400] // reg_6 has the & of crc32_tab

// read_2 computation
ld_field_w_clr[reg_7, 0b1000, read_2] // reg_7 has the highest byte of read_2
immed[reg_8, 24]
alu[reg_7, reg_7, >>, reg_8]
alu[reg_8, reg_7, +8, reg_12] // (crc ^ *p++) & 0xFF

immed[reg_9, 8]
alu[reg_12, reg_12, >>, reg_9] // (crc >> 8)
ram[read, read_4, reg_6, reg_8, 1] ctx_swap // crc32_tab[(crc ^ *p++) & 0xFF]
alu[reg_12, reg_12, ^, read_4] // crc = crc32_tab[(crc ^ *p++) & 0xFF] ^ (crc >> 8)

ld_field_w_clr[reg_7, 0b0100, read_2] // reg_7 has the second highest byte of read_2
immed[reg_8, 16]
alu[reg_7, reg_7, >>, reg_8]
alu[reg_8, reg_7, +8, reg_12] // (crc ^ *p++) & 0xFF

immed[reg_9, 8]
alu[reg_12, reg_12, >>, reg_9] // (crc >> 8)
ram[read, read_4, reg_6, reg_8, 1] ctx_swap // crc32_tab[(crc ^ *p++) & 0xFF]
alu[reg_12, reg_12, ^, read_4] // crc = crc32_tab[(crc ^ *p++) & 0xFF] ^ (crc >> 8)

ld_field_w_clr[reg_7, 0b0010, read_2] // reg_7 has the third highest byte of read_2
immed[reg_8, 8]
alu[reg_7, reg_7, >>, reg_8]
alu[reg_8, reg_7, +8, reg_12] // (crc ^ *p++) & 0xFF

immed[reg_9, 8]
alu[reg_12, reg_12, >>, reg_9] // (crc >> 8)
ram[read, read_4, reg_6, reg_8, 1] ctx_swap // crc32_tab[(crc ^ *p++) & 0xFF]
alu[reg_12, reg_12, ^, read_4] // crc = crc32_tab[(crc ^ *p++) & 0xFF] ^ (crc >> 8)

ld_field_w_clr[reg_7, 0b0001, read_2] // reg_7 has the lowest byte of read_2
alu[reg_8, reg_7, +8, reg_12] // (crc ^ *p++) & 0xFF

immed[reg_9, 8]
alu[reg_12, reg_12, >>, reg_9] // (crc >> 8)
ram[read, read_4, reg_6, reg_8, 1] ctx_swap // crc32_tab[(crc ^ *p++) & 0xFF]
alu[reg_12, reg_12, ^, read_4] // crc = crc32_tab[(crc ^ *p++) & 0xFF] ^ (crc >> 8)


// read_3 computation
ld_field_w_clr[reg_7, 0b1000, read_3] // reg_7 has the highest byte of read_3
immed[reg_8, 24]
alu[reg_7, reg_7, >>, reg_8]
alu[reg_8, reg_7, +8, reg_12] // (crc ^ *p++) & 0xFF

immed[reg_9, 8]
alu[reg_12, reg_12, >>, reg_9] // (crc >> 8)
ram[read, read_4, reg_6, reg_8, 1] ctx_swap // crc32_tab[(crc ^ *p++) & 0xFF]
alu[reg_12, reg_12, ^, read_4] // crc = crc32_tab[(crc ^ *p++) & 0xFF] ^ (crc >> 8)

ld_field_w_clr[reg_7, 0b0100, read_3] // reg_7 has the second highest byte of read_3
immed[reg_8, 16]
alu[reg_7, reg_7, >>, reg_8]
alu[reg_8, reg_7, +8, reg_12] // (crc ^ *p++) & 0xFF

immed[reg_9, 8]
alu[reg_12, reg_12, >>, reg_9] // (crc >> 8)
ram[read, read_4, reg_6, reg_8, 1] ctx_swap // crc32_tab[(crc ^ *p++) & 0xFF]
alu[reg_12, reg_12, ^, read_4] // crc = crc32_tab[(crc ^ *p++) & 0xFF] ^ (crc >> 8)

ld_field_w_clr[reg_7, 0b0010, read_3] // reg_7 has the third highest byte of read_3
immed[reg_8, 8]
alu[reg_7, reg_7, >>, reg_8]
alu[reg_8, reg_7, +8, reg_12] // (crc ^ *p++) & 0xFF

immed[reg_9, 8]
alu[reg_12, reg_12, >>, reg_9] // (crc >> 8)
ram[read, read_4, reg_6, reg_8, 1] ctx_swap // crc32_tab[(crc ^ *p++) & 0xFF]
alu[reg_12, reg_12, ^, read_4] // crc = crc32_tab[(crc ^ *p++) & 0xFF] ^ (crc >> 8)

ld_field_w_clr[reg_7, 0b0001, read_3] // reg_7 has the lowest byte of read_3
alu[reg_8, reg_7, +8, reg_12] // (crc ^ *p++) & 0xFF

immed[reg_9, 8]
alu[reg_12, reg_12, >>, reg_9] // (crc >> 8)
ram[read, read_4, reg_6, reg_8, 1] ctx_swap // crc32_tab[(crc ^ *p++) & 0xFF]
alu[reg_12, reg_12, ^, read_4] // crc = crc32_tab[(crc ^ *p++) & 0xFF] ^ (crc >> 8)

// update the number of bytes processed

// Bug: should be >= 8 bytes
immed[reg_15, 7]
br>=[reg_4, reg_15, tx_crc32_regular_inc]

immed[reg_15, 0]
alu[reg_4, --, B, reg_15]
br[start_crc_loop]

tx_crc32_regular_inc:
immed[reg_15, 8]
alu[reg_4, reg_4, -, reg_15]
br[start_crc_loop]

// Append the CRC32 checksum of the packet
append_crc_checksum:
immed[reg_6, 0]
alu[reg_6, --, ~B, reg_6]
alu[reg_12, reg_12, ^, reg_6] // crc ^ ~0U

alu_shf[reg_6, --, B, reg_3, >>2] // reg_6 has the idx of the end of the pkt
alu[write_0, --, B, reg_12] // move crc32 to write_0
ram[write, write_0, read_1, reg_6, 1] ctx_swap // write the checksum to the end of the packet

// increment the frame size by 4 (going to append the CRC32 to the end)
immed[reg_12, 4]
alu[reg_3, reg_3, +, reg_12]

// IP checksum offloading (if CSR bit is set)
ip_checksum_offloading:
csr[reg_4, CSR_0] // reg_4 has the CSR including the TX_OFFLOAD_MASK
immed[reg_12, 0x4]
alu[reg_6, reg_12, &, reg_4]
br!=[reg_6, reg_12, tx_packet]

immed[reg_6, 3]
alu[reg_12, read_1, +, reg_6] // reg 5 has &pkt+3, which contains the bytes of the IP pkt
immed[reg_6, 0]
ram[read, read_2, reg_12, reg_6, 1] ctx_swap // read_2 has the highest bits of the IP header

// check the version
ld_field_w_clr[reg_6, 0b1100, read_2] // reg_6 has [dscp/ecn, ver|IHL]

ld_field_w_clr[reg_7, 0b0100, reg_6] // reg_7 has [ver|IHL]
immed[reg_8, 0xF0, <<16] // reg_8 has a mask for ver
alu[reg_7, reg_7, &, reg_8] // reg_7 has [ver]
immed[reg_8, 0x40, <<16] // reg_8 has supported version
alu[reg_9, reg_8, ^, reg_7]
immed[reg_10, 0]
br!=[reg_10, reg_9, tx_packet] // if version is not 4, then bounce

// read IHL for size of headers to read
ld_field_w_clr[reg_7, 0b0100, reg_6] // reg_7 has [ver|IHL]
immed[reg_8, 0x0F, <<16] // reg_8 has a mask for IHL
alu[reg_7, reg_7, &, reg_8] // reg_7 has [IHL]
immed[reg_8, 16]
alu[--, reg_7, >>, reg_8] // alu now has IHL as it's
immed[reg_8, 1]
ram[read, read_11, reg_12, reg_8, 0] ctx_swap // indirect reference, to read the packet headers

// reg_6 has [dscp/ecn, ver|IHL], add that up with all the rest
immed[reg_8, 16]
alu[reg_7, reg_6, >>, reg_8] // reg_7 has our checksum
ld_field_w_clr[reg_9, 0b0011, read_11]
alu[reg_7, reg_7, +, reg_9]
ld_field_w_clr[reg_9, 0b1100, read_11]
alu[reg_9, reg_9, >>, reg_8]
alu[reg_7, reg_7, +, reg_9]

ld_field_w_clr[reg_9, 0b0011, read_12]
alu[reg_7, reg_7, +, reg_9]
ld_field_w_clr[reg_9, 0b1100, read_12]
alu[reg_9, reg_9, >>, reg_8]
alu[reg_7, reg_7, +, reg_9]

ld_field_w_clr[reg_9, 0b0011, read_13]
alu[reg_7, reg_7, +, reg_9]
ld_field_w_clr[reg_9, 0b1100, read_13]
alu[reg_9, reg_9, >>, reg_8]
alu[reg_7, reg_7, +, reg_9]

ld_field_w_clr[reg_9, 0b0011, read_14]
alu[reg_7, reg_7, +, reg_9]
ld_field_w_clr[reg_9, 0b1100, read_14]
alu[reg_9, reg_9, >>, reg_8]
alu[reg_7, reg_7, +, reg_9]

ld_field_w_clr[reg_9, 0b0011, read_15]
alu[reg_7, reg_7, +, reg_9]
ld_field_w_clr[reg_9, 0b1100, read_15]
alu[reg_9, reg_9, >>, reg_8]
alu[reg_7, reg_7, +, reg_9]

ld_field_w_clr[reg_9, 0b1100, reg_7]
alu[reg_9, reg_9, >>, reg_8]
alu[reg_7, reg_7, +16, reg_9] // add the carry and get rid of the upper bits
ld_field[reg_7, 0b1100, read_13]
ld_field_w_clr[write_5, 0b1111, reg_7] // write_5 has the new bits

immed[reg_10, 3]
immed[reg_11, 1]
alu[reg_11, --, B, reg_11]
ram[write, write_5, reg_12, reg_10, 0] ctx_swap // reg_12 is (&pkt+3)+3

tx_packet:
// Send to PHY to transmit
immed[reg_4, 0]
t_fifo_wr[reg_3, read_1, reg_4] ctx_swap

// mark as sent
alu[write_0, --, B, reg_3]
scratch[write, write_0, reg_5, reg_1, 1] ctx_swap

// Increment the idx
immed[reg_4, 1]
alu[reg_0, reg_0, +4, reg_4]

// wait for another
br[tx_thread_loop]
