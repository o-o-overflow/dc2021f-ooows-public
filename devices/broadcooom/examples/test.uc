start:
  immed[reg_0, 0xffff, <<16]
  immed[reg_1, 0xffff]
  immed[reg_3, 1]
  ld_field[reg_3, 0b10, reg_1]
  alu[reg_3, @reg_0, +, reg_1]
  alu[reg_5, reg_4, +carry, reg_1]
  alu[write_0, --, B, reg_5]
  scratch[write, write_0, reg_2, reg_2, 1] ctx_swap
  scratch[read, read_0, reg_2, reg_2, 1] ctx_swap
  nop
  ctx_arb
  ram[write, write_0, reg_2, reg_2, 1] ctx_swap
  ram[read, read_0, reg_2, reg_2, 1] ctx_swap
  t_fifo_wr[reg_2, reg_2, reg_2] ctx_swap
  csr[reg_10, CSR_0]
  br=[reg_0, reg_0, start]
