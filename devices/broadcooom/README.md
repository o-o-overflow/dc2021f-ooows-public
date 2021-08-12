# Requirements

- package `libmosquitto-dev` b/c of physical device controller

# Things Remaining

- [x] RAM memory access and controller
- [x] Load Field class of instructions
- [x] `T_FIFO_WR` and `R_FIFO_RD`
- [x] "Physical" Device
- [x] Assembler
- [x] Engine Thread to transmit packets
- [x] Indirect memory references (transfer bytes that are not set in an instruction)
- [x] Engine Thread to receive packets
- [x] Protocol to send packets to "parent" processor through `RX_PKT` instruction (ctx_swap)
- [x] Engine Threads to do packet processing
- [x] Ability to get CSR registers
- [ ] Hashing?
