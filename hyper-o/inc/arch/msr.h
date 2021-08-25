#pragma once

// for guest/host areas
#define IA32_DEBUGCTL 0x01D9
#define IA32_SYSENTER_CS 0x174
#define IA32_SYSENTER_ESP 0x175
#define IA32_SYSENTER_EIP 0x176
#define IA32_PERF_GLOBAL_CTRL 0x38f
#define IA32_PAT 0x277
#define IA32_EFER 0xC0000080
#define IA32_FS_BASE 0xc0000100
#define IA32_GS_BASE 0xc0000101
#define IA32_APIC_BASE 0x1B

#define APIC_DEFAULT_ADDRESS 0xfee00000
#define IA32_APICBASE_BSP      (1<<8)
#define IA32_APICBASE_ENABLE   (1<<11)



// #####################
// # VMCS Control MSRs #
// #####################

// Execution Control MSRs (A.3)

// Pin Based control (A.3.1)
#define IA32_VMX_PINBASED_CTLS 0x481
#define IA32_VMX_TRUE_PINBASED_CTLS 0x48D
// Primary Proc Based controls (A.3.2)
#define IA32_VMX_PROCBASED_CTLS 0x482
#define IA32_VMX_TRUE_PROCBASED_CTLS 0x48E
// Secondary Proc Based controls (A.3.4)
#define IA32_VMX_PROCBASED_CTLS2 0x48B

// Exit controls MSRs (A.4)
#define IA32_VMX_EXIT_CTLS 0x483
#define IA32_VMX_TRUE_EXIT_CTLS 0x48F


// Entry controls MSRs (A.5)
#define IA32_VMX_ENTRY_CTLS 0x484
#define IA32_VMX_TRUE_ENTRY_CTLS 0x490

// ===============================================

// IA32_VMX_MISC_MSR (0x485)
#define IA32_VMX_MISC_MSR 0x485
union __vmx_misc_msr_t
{
	__u64 control;
	struct
	{
		__u64 vmx_preemption_tsc_rate : 5;
		__u64 store_lma_in_vmentry_control : 1;
		__u64 activate_state_bitmap : 3;
		__u64 reserved_0 : 5;
		__u64 pt_in_vmx : 1;
		__u64 rdmsr_in_smm : 1;
		__u64 cr3_target_value_count : 9;
		__u64 max_msr_vmexit : 3;
		__u64 allow_smi_blocking : 1;
		__u64 vmwrite_to_any : 1;
		__u64 interrupt_mod : 1;
		__u64 reserved_1 : 1;
		__u64 mseg_revision_identifier : 32;
	} bits;
};


// IA32_VMX_BASIC_MSR (0x480)
#define IA32_VMX_BASIC_MSR 0x480
union __vmx_basic_msr_t
{
	__u64 control;
	struct
	{
		__u64 vmcs_revision_identifier : 31;
		__u64 always_0 : 1;
		__u64 vmxon_region_size : 13;
		__u64 reserved_1 : 3;
		__u64 vmxon_physical_address_width : 1;
		__u64 dual_monitor_smi : 1;
		__u64 memory_type : 4;
		__u64 io_instruction_reporting : 1;
		__u64 true_controls : 1;
	} bits;
};

// generalized settings for the various control settings
// proc based, entry, exit, etc
union __vmx_generic_control_settings_t
{
	__u64 control;
	struct
	{
		__u32 allowed_0_settings;
		__u32 allowed_1_settings;
	};
};
