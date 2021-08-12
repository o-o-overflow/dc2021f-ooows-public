immed[reg_0, 0]
immed[reg_1, 0x2000]
ram[read, @reg_127, reg_0, reg_1, 47] //  I can overwrite 37 instructions (185), plus the one register of garbage (4 bytes), total of 47 32-bits