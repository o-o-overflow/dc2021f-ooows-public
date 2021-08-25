//
// Note on exemption of DR4 and DR5:
//		Debug registers DR4 and DR5 are reserved when debug extensions are enabled (when the DE flag in control
//		register CR4 is set) and attempts to reference the DR4 and DR5 registers cause invalid-opcode exceptions (#UD).
//
//		When debug extensions are not enabled (when the DE flag is clear), these registers are aliased to debug registers
//		DR6 and DR7.
//
//		MOV DRn instructions do not check that addresses written to DR0–DR3 are in the linear-address limits of
//		the processor implementation. Breakpoint conditions for 8-byte memory read/writes are supported in all modes.
//

union __drx_t
{
	unsigned __int64 breakpoint_linear_address;
	void *breakpoint_address;
};


//
// The debug status register (DR6) reports debug conditions that were sampled at the time the last debug exception
// was generated. Updates to this register only occur when an exception is generated. The flags in this register
// show the following information:
//
// B0 through B3 (breakpoint condition detected) flags (bits 0 through 3)
//		Indicates (when set) that its associated breakpoint condition was met when a debug exception was generated.
//		These flags are set if the condition described for each breakpoint by the LENn, and R/Wn flags in debug control
//		register DR7 is true. They may or may not be set if the breakpoint is not enabled by the Ln or the Gn flags in
//		register DR7. Therefore on a #DB, a debug handler should check only those B0-B3 bits which correspond to an enabled
//		breakpoint.
//
// BD (debug register access detected) flag (bit 13)
//		Indicates that the next instruction in the instruction stream accesses one of the debug registers (DR0 through DR7).
//		This flag is enabled when the GD (general detect) flag in debug control register DR7 is set.
//
// BS (single step) flag (bit 14)
//		Indicates (when set) that the debug exception was triggered by the single-step execution mode
//		(enabled with the TF flag in the EFLAGS register). The single-step mode is the highest priority
//		debug exception. When the BS flag is set, any of the other debug status bits also may be set.
//
// BT (task switch) flag (bit 15)
//		Indicates (when set) that the debug exception resulted from a task switch where the T flag
//		(debug trap flag) in the TSS of the target task was set. There is no flag in debug control
//		register DR7 to enable or disable this exception; the T flag of the TSS is the only enabling flag.
//
// RTM (restricted transactional memory) flag (bit 16)
//		Indicates (when clear) that a debug exception (#DB) or breakpoint exception (#BP) occurred inside
//		an RTM region while advanced debugging of RTM transactional regions was enabled. This bit is set
//		for any other debug exception (including all those that occur when advanced debugging of RTM transactional
//		regions is not enabled). This bit is always 1 if the processor does not support RTM.
//
//		_1 = required set to 1
//		_0 = required set to 0
//
//		Reserved_1 is the upper 32 bits of the register and required to be set to 0 otherwise a #GP may be generated.
//

union __dr6_t
{
	unsigned __int64 flags;
	struct
	{
		unsigned __int64 B0 : 1;
		unsigned __int64 B1 : 1;
		unsigned __int64 B2 : 1;
		unsigned __int64 B3 : 1;
		unsigned __int64 always_1 : 8;
		unsigned __int64 always_0 : 1;
		unsigned __int64 BD : 1;
		unsigned __int64 BS : 1;
		unsigned __int64 BT : 1;
		unsigned __int64 RTM : 1;
	};
};

//
// The debug control register (DR7) enables or disables breakpoints and sets breakpoint conditions.
//
// L0-L3 (local breakpoint enable)
//		Enable the breakpoint condition for the associated breakpoint for the current task. When a
//		breakpoint condition is detected and its associated Ln flag is set, a debug exception is generated.
//		The processor automatically clears these flags on every task switch to avoid unwanted breakpoint
//		conditions in the new task.
//
// G0-G3 (global breakpoint enable)
//		Enable the breakpoint condition for the associated breakpoint for all tasks. When a breakpoint is detected
//		and its associated Gn flag is set, a debug exception is generated. The processor does not clear these flags
//		on a task switch, allowing the breakpoint to be enabled for all tasks.
//
// LE and GE (local and global exact breakpoint enable)
//		Not supported in IA-32, or Intel 64 processors. When set, these flags cause the processor to detect the
//		exact instruction that caused a data breakpoint condition. Recommended to set LE and GE to 1 if exact
//		breakpoints are required.
//
// RTM (restricted transactional memory)
//		Enables advanced debugging of RTM regions. This is only enabled when IA32_DEBUGCTRL.RTM is set.
//
// GD (general detect enable)
//		Enables debug-register protection, which causes a debug exception to be generated prior to any _mov_ instruction
//		that accesses a debug register. When the condition is detected, the BD flag in DR6 is set prior to generating an
//		exception. This condition is provided to support in-circuit emulators.
//
//		The processor clears the GD flag upon entering the debug exception handler, to allow the handler to access the debug
//		registers.
//
// R/W0-R/W3 (read/write)
//		Specifies the breakpoint condition for the corresponding breakpoint. The DE flag in CR4 determines how the bits in the R/Wn
//		fields are interpreted. When the DE flag is set, the processor interprets bits as follows:
//
//		00 - Break on instruction execution only
//		01 - Break on data writes only
//		10 - Break on I/O read/write
//		11 - Break on data read/write but not instruction fetches
//
//		When the DE flag is clear, the processor interprets the R/Wn bits as follows:
//
//		00 - Break on instruction execution only
//		01 - Break on data writes only
//		10 - Undefined
//		11 - Break on data read/write but not instruction fetches
//
// LEN0-LEN3 (length)
//		These fields specify the size of the memory location at the address specified in the corresponding debug register (DR0-DR3).
//		The fields are interpreted as follows:
//
//		00 - 1-byte length
//		01 - 2-byte length
//		10 - Undefined (or 8-byte length)
//		11 - 4-byte length
//
//		If the corresponding RWn fields in DR7 is 00 (instruction execution), then the LENn field should also be 00. Using other lengths
//		results in undefined behavior.
//
//		_1 = required set to 1
//		_0 = required set to 0
//
//		Reserved_1 is the upper 32 bits of the register and required to be set to 0 otherwise a #GP may be generated.
//
// Miscellaneous:
//		Instruction-breakpoint and general detect condition result in faults; where as other debug-exception conditions result
//		in traps.
//

union __dr7_t
{
	unsigned __int64 flags;
	struct
	{
		unsigned __int64 L0 : 1;
		unsigned __int64 G0 : 1;
		unsigned __int64 L1 : 1;
		unsigned __int64 G1 : 1;
		unsigned __int64 L2 : 1;
		unsigned __int64 G2 : 1;
		unsigned __int64 L3 : 1;
		unsigned __int64 G3 : 1;
		unsigned __int64 LE : 1;
		unsigned __int64 GE : 1;
		unsigned __int64 always_1 : 1;
		unsigned __int64 RTM : 1;
		unsigned __int64 always_0 : 1;
		unsigned __int64 GD : 1;
		unsigned __int64 reserved_0 : 2;
		unsigned __int64 RW0 : 2;
		unsigned __int64 LEN0 : 2;
		unsigned __int64 RW1 : 2;
		unsigned __int64 LEN1 : 2;
		unsigned __int64 RW2 : 2;
		unsigned __int64 LEN2 : 2;
		unsigned __int64 RW3 : 2;
		unsigned __int64 LEN3 : 2;
		unsigned __int64 reserved_1 : 32;
	} bits;
};
