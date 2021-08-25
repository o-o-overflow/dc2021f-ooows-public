//
// The status flags (bits 0, 2, 4, 6, 7, and 11) of the EFLAGS register indicate the results of arithmetic instructions,
// such as the ADD, SUB, MUL, and DIV instructions. The status flag functions are:
//
// CF (bit 0)
//		Carry flag — Set if an arithmetic operation generates a carry or a borrow out of the mostsignificant
//		bit of the result; cleared otherwise. This flag indicates an overflow condition for
//		unsigned-integer arithmetic. It is also used in multiple-precision arithmetic.
//
// PF (bit 2)
//		Parity flag — Set if the least-significant byte of the result contains an even number of 1 bits;
//		cleared otherwise.
//
// AF (bit 4)
//		Auxiliary Carry flag — Set if an arithmetic operation generates a carry or a borrow out of bit
//		3 of the result; cleared otherwise. This flag is used in binary-coded decimal (BCD) arithmetic.
//
// ZF (bit 6)
//		Zero flag — Set if the result is zero; cleared otherwise.
//
// SF (bit 7)
//		Sign flag — Set equal to the most-significant bit of the result, which is the sign bit of a signed
//		integer. (0 indicates a positive value and 1 indicates a negative value.)
//
// OF (bit 11)
//		Overflow flag — Set if the integer result is too large a positive number or too small a negative
//		number (excluding the sign-bit) to fit in the destination operand; cleared otherwise. This flag
//		indicates an overflow condition for signed-integer (two’s complement) arithmetic.
//
//		Of these status flags, only the CF flag can be modified directly, using the STC, CLC, and CMC instructions. Also the
//		bit instructions (BT, BTS, BTR, and BTC) copy a specified bit into the CF flag.
//

union __rflags_t
{
	unsigned __int64 flags;
	struct
	{
		unsigned __int64 cf : 1;
		unsigned __int64 always_1 : 1;
		unsigned __int64 pf : 1;
		unsigned __int64 reserved_0 : 1;
		unsigned __int64 af : 1;
		unsigned __int64 reserved_1 : 1;
		unsigned __int64 zf : 1;
		unsigned __int64 sf : 1;
		unsigned __int64 tf : 1;
		unsigned __int64 intf : 1;
		unsigned __int64 df : 1;
		unsigned __int64 of : 1;
		unsigned __int64 iopl : 1;
		unsigned __int64 nt : 1;
		unsigned __int64 reserved_2 : 1;
		unsigned __int64 rf : 1;
		unsigned __int64 vf : 1;
		unsigned __int64 ac : 1;
		unsigned __int64 vif : 1;
		unsigned __int64 vip : 1;
		unsigned __int64 idf : 1;
		unsigned __int64 reserved_3 : 9;
	} bits;
};
