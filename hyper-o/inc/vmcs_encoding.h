// structs/defs that help us access our vmcs


/*
+--------------------+----------------------------------------------------------------------------+
|                    |                                                                            |
|  Bit Positions     |    Contents                                                                |
|                    |                                                                            |
+-------------------------------------------------------------------------------------------------+
|                    |                                                                            |
|   0                |   Access type (0 = full; 1 = high)                                         |
|                    |                                                                            |
+-------------------------------------------------------------------------------------------------+
|                    |                                                                            |
|   9:1              |   Index                                                                    |
|                    |                                                                            |
+-------------------------------------------------------------------------------------------------+
|                    |                                                                            |
|   11:10            |   Type:   (0: control  1: VM-Exit info  2: Guest state   3: Host state)    |
|                    |                                                                            |
+-------------------------------------------------------------------------------------------------+
|                    |                                                                            |
|   12               |   Reserved (Must be 0)                                                     |
|                    |                                                                            |
+-------------------------------------------------------------------------------------------------+
|                    |                                                                            |
|   14:13            |   Width:  (0: 16bit  1: 64bit  2: 32bit  3: natural-width)                 |
|                    |                                                                            |
+-------------------------------------------------------------------------------------------------+
|                    |                                                                            |
|   31:15            |   Reserved (Must be 0)                                                     |
|                    |                                                                            |
+--------------------+----------------------------------------------------------------------------+
*/

// not really proper sizes but I guess it's fine so long as we don't feed it bad values..
#define VMCS_ENCODE_COMPONENT( access, type, width, index )    ( unsigned )( ( unsigned short )( access ) | \
                                                                        ( ( unsigned short )( index ) << 1 ) | \
                                                                        ( ( unsigned short )( type ) << 10 ) | \
                                                                        ( ( unsigned short )( width ) << 13 ) )

#define VMCS_ENCODE_COMPONENT_FULL( type, width, index )    VMCS_ENCODE_COMPONENT( 0 , type, width, index )
#define VMCS_ENCODE_COMPONENT_FULL_16( type, index )        VMCS_ENCODE_COMPONENT_FULL( type, 0, index )
#define VMCS_ENCODE_COMPONENT_FULL_32( type, index )        VMCS_ENCODE_COMPONENT_FULL( type, 2, index )
#define VMCS_ENCODE_COMPONENT_FULL_64( type, index )        VMCS_ENCODE_COMPONENT_FULL( type, 1, index )



enum __vmcs_access_e
{
	// full == accesses full 64 bit field.
	// high == accesses upper (high) 32 bits of a field
	full = 0,
	high = 1
};

// recall the six different fields of the vmcs
// control: for VM-execution control, VM-exit control, and VM-entry control
// vmexit: for info regarding vmexits
// guest: processor state saved to guest area on vm exit & loaded on vmentry
// host: processor state loaded on vmexit
enum __vmcs_type_e
{
	control = 0,
	vmexit,
	guest,
	host
};


enum __vmcs_width_e
{
	// 16 bit
	word = 0,
	// 64 bit
	quadword,
	// 32 bit
	doubleword,
	// 64 bit (??)
	natural
};


// fuck it. copied from https://elixir.bootlin.com/linux/latest/source/arch/x86/include/asm/vmx.h

/* VMCS Encodings */
enum vmcs_field
{
	VIRTUAL_PROCESSOR_ID            = 0x00000000,
	POSTED_INTR_NV                  = 0x00000002,
	GUEST_ES_SELECTOR               = 0x00000800,
	GUEST_CS_SELECTOR               = 0x00000802,
	GUEST_SS_SELECTOR               = 0x00000804,
	GUEST_DS_SELECTOR               = 0x00000806,
	GUEST_FS_SELECTOR               = 0x00000808,
	GUEST_GS_SELECTOR               = 0x0000080a,
	GUEST_LDTR_SELECTOR             = 0x0000080c,
	GUEST_TR_SELECTOR               = 0x0000080e,
	GUEST_INTR_STATUS               = 0x00000810,
	GUEST_PML_INDEX			= 0x00000812,
	HOST_ES_SELECTOR                = 0x00000c00,
	HOST_CS_SELECTOR                = 0x00000c02,
	HOST_SS_SELECTOR                = 0x00000c04,
	HOST_DS_SELECTOR                = 0x00000c06,
	HOST_FS_SELECTOR                = 0x00000c08,
	HOST_GS_SELECTOR                = 0x00000c0a,
	HOST_TR_SELECTOR                = 0x00000c0c,
	IO_BITMAP_A                     = 0x00002000,
	IO_BITMAP_A_HIGH                = 0x00002001,
	IO_BITMAP_B                     = 0x00002002,
	IO_BITMAP_B_HIGH                = 0x00002003,
	MSR_BITMAP                      = 0x00002004,
	MSR_BITMAP_HIGH                 = 0x00002005,
	VM_EXIT_MSR_STORE_ADDR          = 0x00002006,
	VM_EXIT_MSR_STORE_ADDR_HIGH     = 0x00002007,
	VM_EXIT_MSR_LOAD_ADDR           = 0x00002008,
	VM_EXIT_MSR_LOAD_ADDR_HIGH      = 0x00002009,
	VM_ENTRY_MSR_LOAD_ADDR          = 0x0000200a,
	VM_ENTRY_MSR_LOAD_ADDR_HIGH     = 0x0000200b,
	PML_ADDRESS			= 0x0000200e,
	PML_ADDRESS_HIGH		= 0x0000200f,
	TSC_OFFSET                      = 0x00002010,
	TSC_OFFSET_HIGH                 = 0x00002011,
	VIRTUAL_APIC_PAGE_ADDR          = 0x00002012,
	VIRTUAL_APIC_PAGE_ADDR_HIGH     = 0x00002013,
	APIC_ACCESS_ADDR		= 0x00002014,
	APIC_ACCESS_ADDR_HIGH		= 0x00002015,
	POSTED_INTR_DESC_ADDR           = 0x00002016,
	POSTED_INTR_DESC_ADDR_HIGH      = 0x00002017,
	VM_FUNCTION_CONTROL             = 0x00002018,
	VM_FUNCTION_CONTROL_HIGH        = 0x00002019,
	EPT_POINTER                     = 0x0000201a,
	EPT_POINTER_HIGH                = 0x0000201b,
	EOI_EXIT_BITMAP0                = 0x0000201c,
	EOI_EXIT_BITMAP0_HIGH           = 0x0000201d,
	EOI_EXIT_BITMAP1                = 0x0000201e,
	EOI_EXIT_BITMAP1_HIGH           = 0x0000201f,
	EOI_EXIT_BITMAP2                = 0x00002020,
	EOI_EXIT_BITMAP2_HIGH           = 0x00002021,
	EOI_EXIT_BITMAP3                = 0x00002022,
	EOI_EXIT_BITMAP3_HIGH           = 0x00002023,
	EPTP_LIST_ADDRESS               = 0x00002024,
	EPTP_LIST_ADDRESS_HIGH          = 0x00002025,
	VMREAD_BITMAP                   = 0x00002026,
	VMREAD_BITMAP_HIGH              = 0x00002027,
	VMWRITE_BITMAP                  = 0x00002028,
	VMWRITE_BITMAP_HIGH             = 0x00002029,
	XSS_EXIT_BITMAP                 = 0x0000202C,
	XSS_EXIT_BITMAP_HIGH            = 0x0000202D,
	ENCLS_EXITING_BITMAP		= 0x0000202E,
	ENCLS_EXITING_BITMAP_HIGH	= 0x0000202F,
	TSC_MULTIPLIER                  = 0x00002032,
	TSC_MULTIPLIER_HIGH             = 0x00002033,
	GUEST_PHYSICAL_ADDRESS          = 0x00002400,
	GUEST_PHYSICAL_ADDRESS_HIGH     = 0x00002401,
	VMCS_LINK_POINTER               = 0x00002800,
	VMCS_LINK_POINTER_HIGH          = 0x00002801,
	GUEST_IA32_DEBUGCTL             = 0x00002802,
	GUEST_IA32_DEBUGCTL_HIGH        = 0x00002803,
	GUEST_IA32_PAT			= 0x00002804,
	GUEST_IA32_PAT_HIGH		= 0x00002805,
	GUEST_IA32_EFER			= 0x00002806,
	GUEST_IA32_EFER_HIGH		= 0x00002807,
	GUEST_IA32_PERF_GLOBAL_CTRL	= 0x00002808,
	GUEST_IA32_PERF_GLOBAL_CTRL_HIGH = 0x00002809,
	GUEST_PDPTR0                    = 0x0000280a,
	GUEST_PDPTR0_HIGH               = 0x0000280b,
	GUEST_PDPTR1                    = 0x0000280c,
	GUEST_PDPTR1_HIGH               = 0x0000280d,
	GUEST_PDPTR2                    = 0x0000280e,
	GUEST_PDPTR2_HIGH               = 0x0000280f,
	GUEST_PDPTR3                    = 0x00002810,
	GUEST_PDPTR3_HIGH               = 0x00002811,
	GUEST_BNDCFGS                   = 0x00002812,
	GUEST_BNDCFGS_HIGH              = 0x00002813,
	GUEST_IA32_RTIT_CTL		= 0x00002814,
	GUEST_IA32_RTIT_CTL_HIGH	= 0x00002815,
	HOST_IA32_PAT			= 0x00002c00,
	HOST_IA32_PAT_HIGH		= 0x00002c01,
	HOST_IA32_EFER			= 0x00002c02,
	HOST_IA32_EFER_HIGH		= 0x00002c03,
	HOST_IA32_PERF_GLOBAL_CTRL	= 0x00002c04,
	HOST_IA32_PERF_GLOBAL_CTRL_HIGH	= 0x00002c05,
	PIN_BASED_VM_EXEC_CONTROL       = 0x00004000,
	CPU_BASED_VM_EXEC_CONTROL       = 0x00004002,
	EXCEPTION_BITMAP                = 0x00004004,
	PAGE_FAULT_ERROR_CODE_MASK      = 0x00004006,
	PAGE_FAULT_ERROR_CODE_MATCH     = 0x00004008,
	CR3_TARGET_COUNT                = 0x0000400a,
	VM_EXIT_CONTROLS                = 0x0000400c,
	VM_EXIT_MSR_STORE_COUNT         = 0x0000400e,
	VM_EXIT_MSR_LOAD_COUNT          = 0x00004010,
	VM_ENTRY_CONTROLS               = 0x00004012,
	VM_ENTRY_MSR_LOAD_COUNT         = 0x00004014,
	VM_ENTRY_INTR_INFO_FIELD        = 0x00004016,
	VM_ENTRY_EXCEPTION_ERROR_CODE   = 0x00004018,
	VM_ENTRY_INSTRUCTION_LEN        = 0x0000401a,
	TPR_THRESHOLD                   = 0x0000401c,
	SECONDARY_VM_EXEC_CONTROL       = 0x0000401e,
	PLE_GAP                         = 0x00004020,
	PLE_WINDOW                      = 0x00004022,
	VM_INSTRUCTION_ERROR            = 0x00004400,
	VM_EXIT_REASON                  = 0x00004402,
	VM_EXIT_INTR_INFO               = 0x00004404,
	VM_EXIT_INTR_ERROR_CODE         = 0x00004406,
	IDT_VECTORING_INFO_FIELD        = 0x00004408,
	IDT_VECTORING_ERROR_CODE        = 0x0000440a,
	VM_EXIT_INSTRUCTION_LEN         = 0x0000440c,
	VMX_INSTRUCTION_INFO            = 0x0000440e,
	GUEST_ES_LIMIT                  = 0x00004800,
	GUEST_CS_LIMIT                  = 0x00004802,
	GUEST_SS_LIMIT                  = 0x00004804,
	GUEST_DS_LIMIT                  = 0x00004806,
	GUEST_FS_LIMIT                  = 0x00004808,
	GUEST_GS_LIMIT                  = 0x0000480a,
	GUEST_LDTR_LIMIT                = 0x0000480c,
	GUEST_TR_LIMIT                  = 0x0000480e,
	GUEST_GDTR_LIMIT                = 0x00004810,
	GUEST_IDTR_LIMIT                = 0x00004812,
	GUEST_ES_AR_BYTES               = 0x00004814,
	GUEST_CS_AR_BYTES               = 0x00004816,
	GUEST_SS_AR_BYTES               = 0x00004818,
	GUEST_DS_AR_BYTES               = 0x0000481a,
	GUEST_FS_AR_BYTES               = 0x0000481c,
	GUEST_GS_AR_BYTES               = 0x0000481e,
	GUEST_LDTR_AR_BYTES             = 0x00004820,
	GUEST_TR_AR_BYTES               = 0x00004822,
	GUEST_INTERRUPTIBILITY_INFO     = 0x00004824,
	GUEST_ACTIVITY_STATE            = 0X00004826,
	GUEST_SYSENTER_CS               = 0x0000482A,
	VMX_PREEMPTION_TIMER_VALUE      = 0x0000482E,
	HOST_IA32_SYSENTER_CS           = 0x00004c00,
	CR0_GUEST_HOST_MASK             = 0x00006000,
	CR4_GUEST_HOST_MASK             = 0x00006002,
	CR0_READ_SHADOW                 = 0x00006004,
	CR4_READ_SHADOW                 = 0x00006006,
	CR3_TARGET_VALUE0               = 0x00006008,
	CR3_TARGET_VALUE1               = 0x0000600a,
	CR3_TARGET_VALUE2               = 0x0000600c,
	CR3_TARGET_VALUE3               = 0x0000600e,
	EXIT_QUALIFICATION              = 0x00006400,
	GUEST_LINEAR_ADDRESS            = 0x0000640a,
	GUEST_CR0                       = 0x00006800,
	GUEST_CR3                       = 0x00006802,
	GUEST_CR4                       = 0x00006804,
	GUEST_ES_BASE                   = 0x00006806,
	GUEST_CS_BASE                   = 0x00006808,
	GUEST_SS_BASE                   = 0x0000680a,
	GUEST_DS_BASE                   = 0x0000680c,
	GUEST_FS_BASE                   = 0x0000680e,
	GUEST_GS_BASE                   = 0x00006810,
	GUEST_LDTR_BASE                 = 0x00006812,
	GUEST_TR_BASE                   = 0x00006814,
	GUEST_GDTR_BASE                 = 0x00006816,
	GUEST_IDTR_BASE                 = 0x00006818,
	GUEST_DR7                       = 0x0000681a,
	GUEST_RSP                       = 0x0000681c,
	GUEST_RIP                       = 0x0000681e,
	GUEST_RFLAGS                    = 0x00006820,
	GUEST_PENDING_DBG_EXCEPTIONS    = 0x00006822,
	GUEST_SYSENTER_ESP              = 0x00006824,
	GUEST_SYSENTER_EIP              = 0x00006826,
	HOST_CR0                        = 0x00006c00,
	HOST_CR3                        = 0x00006c02,
	HOST_CR4                        = 0x00006c04,
	HOST_FS_BASE                    = 0x00006c06,
	HOST_GS_BASE                    = 0x00006c08,
	HOST_TR_BASE                    = 0x00006c0a,
	HOST_GDTR_BASE                  = 0x00006c0c,
	HOST_IDTR_BASE                  = 0x00006c0e,
	HOST_IA32_SYSENTER_ESP          = 0x00006c10,
	HOST_IA32_SYSENTER_EIP          = 0x00006c12,
	HOST_RSP                        = 0x00006c14,
	HOST_RIP                        = 0x00006c16,
	VMCS_GUEST_ACTIVITY_STATE	= 0x4826,
};


/*
// prepare yourself..
// Appendix B (page 1963)
enum __vmcs_guest_fields_e
{
	// ########################
	// # CONTROL 16bit FIELDS #
	// ########################
	CONTROL_VPID = VMCS_ENCODE_COMPONENT_FULL_16(control, 0),
	CONTROL_POSTED_INTR_NOTIF_VEC = VMCS_ENCODE_COMPONENT_FULL_16(control, 1),
	CONTROL_EPTP_INDEX = VMCS_ENCODE_COMPONENT_FULL_16(control, 2),

	// ######################
	// # GUEST 16bit FIELDS #
	// ######################
	GUEST_ES_SELECTOR = VMCS_ENCODE_COMPONENT_FULL_16( guest, 0 ),
	GUEST_CS_SELECTOR = VMCS_ENCODE_COMPONENT_FULL_16( guest, 1 ),
	GUEST_SS_SELECTOR = VMCS_ENCODE_COMPONENT_FULL_16( guest, 2 ),
	GUEST_DS_SELECTOR = VMCS_ENCODE_COMPONENT_FULL_16( guest, 3 ),
	GUEST_FS_SELECTOR = VMCS_ENCODE_COMPONENT_FULL_16( guest, 4 ),
	GUEST_GS_SELECTOR = VMCS_ENCODE_COMPONENT_FULL_16( guest, 5 ),
	GUEST_LDTR_SELECTOR = VMCS_ENCODE_COMPONENT_FULL_16( guest, 6 ),
	GUEST_TR_SELECTOR = VMCS_ENCODE_COMPONENT_FULL_16( guest, 7 ),
	GUEST_INTERRUPT_STATUS = VMCS_ENCODE_COMPONENT_FULL_16(guest, 8),
	GUEST_PML_INDEX = VMCS_ENCODE_COMPONENT_FULL_16(guest, 9),

	// #####################
	// # HOST 16bit FIELDS #
	// #####################
	HOST_ES_SELECTOR = VMCS_ENCODE_COMPONENT_FULL_16( host, 0 ),
	HOST_CS_SELECTOR = VMCS_ENCODE_COMPONENT_FULL_16( host, 1 ),
	HOST_SS_SELECTOR = VMCS_ENCODE_COMPONENT_FULL_16( host, 2 ),
	HOST_DS_SELECTOR = VMCS_ENCODE_COMPONENT_FULL_16( host, 3 ),
	HOST_FS_SELECTOR = VMCS_ENCODE_COMPONENT_FULL_16( host, 4 ),
	HOST_GS_SELECTOR = VMCS_ENCODE_COMPONENT_FULL_16( host, 5 ),
	HOST_TR_SELECTOR = VMCS_ENCODE_COMPONENT_FULL_16( host, 6 ),

	// ########################
	// # CONTROL 32bit FIELDS #
	// ########################
	CONTROL_PIN_CONTROLS = VMCS_ENCODE_COMPONENT_FULL_32( control, 0),
	CONTROL_PRIMARY_PROC_CONTROLS = VMCS_ENCODE_COMPONENT_FULL_32( control, 1),
	CONTROL_EXCEPTION_BITMASK = VMCS_ENCODE_COMPONENT_FULL_32( control, 2),
	CONTROL_PF_ERROR_MASK = VMCS_ENCODE_COMPONENT_FULL_32( control, 3),
	CONTROL_PF_ERROR_MATCH = VMCS_ENCODE_COMPONENT_FULL_32( control, 4),
	CONTROL_CR3_TARGET_COUNT = VMCS_ENCODE_COMPONENT_FULL_32( control, 5),
	CONTROL_VM_EXIT_CONTROLS = VMCS_ENCODE_COMPONENT_FULL_32( control, 6),
	CONTROL_VM_EXIT_MSR_STORE_CNT = VMCS_ENCODE_COMPONENT_FULL_32( control, 7),
	CONTROL_VM_EXIT_MSR_LOAD_CNT = VMCS_ENCODE_COMPONENT_FULL_32( control, 8),
	CONTROL_VM_ENTRY_CONTROLS = VMCS_ENCODE_COMPONENT_FULL_32( control, 9),
	CONTROL_VM_ENTRY_MSR_LOAD_CNT = VMCS_ENCODE_COMPONENT_FULL_32( control, 10),
	CONTROL_VM_ENTRY_INTR_INFO = VMCS_ENCODE_COMPONENT_FULL_32( control, 11),
	CONTROL_VM_ENTRY_EXCEPTION_ERROR_CODE = VMCS_ENCODE_COMPONENT_FULL_32( control, 12),
	CONTROL_VM_ENTRY_INSTR_LEN = VMCS_ENCODE_COMPONENT_FULL_32( control, 13),
	CONTROL_TPR_THRESH = VMCS_ENCODE_COMPONENT_FULL_32( control, 14),
	CONTROL_SECONDARY_PROC_CONTROLS = VMCS_ENCODE_COMPONENT_FULL_32( control, 15),
	CONTROL_PLE_GAP = VMCS_ENCODE_COMPONENT_FULL_32( control, 16),
	CONTROL_PLE_WINDOW = VMCS_ENCODE_COMPONENT_FULL_32( control, 17),

	// ######################
	// # GUEST 32bit FIELDS #
	// ######################
	GUEST_ES_LIMIT = VMCS_ENCODE_COMPONENT_FULL_32( guest, 0 ),
	GUEST_CS_LIMIT = VMCS_ENCODE_COMPONENT_FULL_32( guest, 1 ),
	GUEST_SS_LIMIT = VMCS_ENCODE_COMPONENT_FULL_32( guest, 2 ),
	GUEST_DS_LIMIT = VMCS_ENCODE_COMPONENT_FULL_32( guest, 3 ),
	GUEST_FS_LIMIT = VMCS_ENCODE_COMPONENT_FULL_32( guest, 4 ),
	GUEST_GS_LIMIT = VMCS_ENCODE_COMPONENT_FULL_32( guest, 5 ),
	GUEST_LDTR_LIMIT = VMCS_ENCODE_COMPONENT_FULL_32( guest, 6 ),
	GUEST_TR_LIMIT = VMCS_ENCODE_COMPONENT_FULL_32( guest, 7 ),
	GUEST_GDTR_LIMIT = VMCS_ENCODE_COMPONENT_FULL_32( guest, 8 ),
	GUEST_IDTR_LIMIT = VMCS_ENCODE_COMPONENT_FULL_32( guest, 9 ),
	GUEST_ES_ACCESS_RIGHTS = VMCS_ENCODE_COMPONENT_FULL_32( guest, 10 ),
	GUEST_CS_ACCESS_RIGHTS = VMCS_ENCODE_COMPONENT_FULL_32( guest, 11 ),
	GUEST_SS_ACCESS_RIGHTS = VMCS_ENCODE_COMPONENT_FULL_32( guest, 12 ),
	GUEST_DS_ACCESS_RIGHTS = VMCS_ENCODE_COMPONENT_FULL_32( guest, 13 ),
	GUEST_FS_ACCESS_RIGHTS = VMCS_ENCODE_COMPONENT_FULL_32( guest, 14 ),
	GUEST_GS_ACCESS_RIGHTS = VMCS_ENCODE_COMPONENT_FULL_32( guest, 15 ),
	GUEST_LDTR_ACCESS_RIGHTS = VMCS_ENCODE_COMPONENT_FULL_32( guest, 16 ),
	GUEST_TR_ACCESS_RIGHTS = VMCS_ENCODE_COMPONENT_FULL_32( guest, 17 ),
	GUEST_INTERRUPTIBILITY_STATE = VMCS_ENCODE_COMPONENT_FULL_32(guest, 18),
	GUEST_ACTIVITY_STATE = VMCS_ENCODE_COMPONENT_FULL_32(guest, 19),
	GUEST_SMBASE = VMCS_ENCODE_COMPONENT_FULL_32( guest, 20 ),
	GUEST_SYSENTER_CS = VMCS_ENCODE_COMPONENT_FULL_32( guest, 21 ),
	GUEST_PREEMPTION_TIMER_VAL = VMCS_ENCODE_COMPONENT_FULL_32(guest, 22),

	// ######################
	// # GUEST 64bit FIELDS #
	// ######################
	GUEST_VMCS_LINK_POINTER = VMCS_ENCODE_COMPONENT_FULL_64( guest, 0 ),
	GUEST_DEBUG_CONTROL = VMCS_ENCODE_COMPONENT_FULL_64( guest, 1 ),
	GUEST_PAT = VMCS_ENCODE_COMPONENT_FULL_64( guest, 2 ),
	GUEST_EFER = VMCS_ENCODE_COMPONENT_FULL_64( guest, 3 ),
	GUEST_PERF_GLOBAL_CONTROL = VMCS_ENCODE_COMPONENT_FULL_64( guest, 4 ),
	GUEST_PDPTE0 = VMCS_ENCODE_COMPONENT_FULL_64(guest, 5),
	GUEST_PDPTE1 = VMCS_ENCODE_COMPONENT_FULL_64(guest, 6),
	GUEST_PDPTE2 = VMCS_ENCODE_COMPONENT_FULL_64(guest, 7),
	GUEST_PDPTE3 = VMCS_ENCODE_COMPONENT_FULL_64(guest, 8),
	GUEST_BNDCFGS = VMCS_ENCODE_COMPONENT_FULL_64( guest, 9 ),


	// ########################
	// # GUEST NATURAL FIELDS #
	// ########################
	// 0x6800
	GUEST_CR0 = VMCS_ENCODE_COMPONENT_FULL( guest, natural, 0 ),
	// 0x6802
	GUEST_CR3 = VMCS_ENCODE_COMPONENT_FULL( guest, natural, 1 ),
	// 0x6804
	GUEST_CR4 = VMCS_ENCODE_COMPONENT_FULL( guest, natural, 2 ),
	// 0x6806
	GUEST_ES_BASE = VMCS_ENCODE_COMPONENT_FULL( guest, natural, 3 ),
	// 0x6808
	GUEST_CS_BASE = VMCS_ENCODE_COMPONENT_FULL( guest, natural, 4 ),
	// 0x680A
	GUEST_SS_BASE = VMCS_ENCODE_COMPONENT_FULL( guest, natural, 5 ),
	// 0x680C
	GUEST_DS_BASE = VMCS_ENCODE_COMPONENT_FULL( guest, natural, 6 ),
	// 0x680E
	GUEST_FS_BASE = VMCS_ENCODE_COMPONENT_FULL( guest, natural, 7 ),
	// 0x6810
	GUEST_GS_BASE = VMCS_ENCODE_COMPONENT_FULL( guest, natural, 8 ),
	// 0x6812
	GUEST_LDTR_BASE = VMCS_ENCODE_COMPONENT_FULL( guest, natural, 9 ),
	// 0x6814
	GUEST_TR_BASE = VMCS_ENCODE_COMPONENT_FULL( guest, natural, 10 ),
	// 0x6816
	GUEST_GDTR_BASE = VMCS_ENCODE_COMPONENT_FULL( guest, natural, 11 ),
	// 0x6818
	GUEST_IDTR_BASE = VMCS_ENCODE_COMPONENT_FULL( guest, natural, 12 ),
	// 0x681A
	GUEST_DR7 = VMCS_ENCODE_COMPONENT_FULL( guest, natural, 13 ),
	// 0x681C
	GUEST_RSP = VMCS_ENCODE_COMPONENT_FULL( guest, natural, 14 ),
	// 0x681E
	GUEST_RIP = VMCS_ENCODE_COMPONENT_FULL( guest, natural, 15 ),
	// 0x6820
	GUEST_RFLAGS = VMCS_ENCODE_COMPONENT_FULL( guest, natural, 16 ),
	// 0x6822
	GUEST_PENDING_DEBUG_EXCEPTIONS = VMCS_ENCODE_COMPONENT_FULL( guest, natural, 17),
	// 0x6824
	GUEST_SYSENTER_ESP = VMCS_ENCODE_COMPONENT_FULL( guest, natural, 18 ),
	// 0x6826
	GUEST_SYSENTER_EIP = VMCS_ENCODE_COMPONENT_FULL( guest, natural, 19 ),

};

enum __vmcs_control_fields_e
{
}

enum __vmcs_host_fields_e
{
	// ########################
	// # CONTROL 16bit FIELDS #
	// ########################
	CONTROL_VPID = VMCS_ENCODE_COMPONENT_FULL_16(control, 0),
	CONTROL_POSTED_INTR_NOTIF_VEC = VMCS_ENCODE_COMPONENT_FULL_16(control, 1),
	CONTROL_EPTP_INDEX = VMCS_ENCODE_COMPONENT_FULL_16(control, 2),

}
*/
