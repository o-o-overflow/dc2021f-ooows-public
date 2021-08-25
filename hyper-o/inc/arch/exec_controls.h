
// taken from https://revers.engineering/day-1-introduction-to-virtualization/
// Thanks Daax :)

// IA32_VMX_PINBASED_CTL_MSR (0x481)
union __vmx_pinbased_control_msr_t
{
	unsigned __int64 control;
	struct
	{
		unsigned __int64 external_interrupt_exiting : 1;
		unsigned __int64 reserved_0 : 2;
		unsigned __int64 nmi_exiting : 1;
		unsigned __int64 reserved_1 : 1;
		unsigned __int64 virtual_nmis : 1;
		unsigned __int64 vmx_preemption_timer : 1;
		unsigned __int64 process_posted_interrupts : 1;
	} bits;
};


// IA32_VMX_PRIMARY_PROCESSOR_BASED_CTL_MSR (0x482)
union __vmx_primary_processor_based_control_t
{
	unsigned __int64 control;
	struct
	{
		unsigned __int64 reserved_0 : 2;
		unsigned __int64 interrupt_window_exiting : 1;
		unsigned __int64 use_tsc_offsetting : 1;
		unsigned __int64 reserved_1 : 3;
		unsigned __int64 hlt_exiting : 1;
		unsigned __int64 reserved_2 : 1;
		unsigned __int64 invldpg_exiting : 1;
		unsigned __int64 mwait_exiting : 1;
		unsigned __int64 rdpmc_exiting : 1;
		unsigned __int64 rdtsc_exiting : 1;
		unsigned __int64 reserved_3 : 2;
		unsigned __int64 cr3_load_exiting : 1;
		unsigned __int64 cr3_store_exiting : 1;
		unsigned __int64 reserved_4 : 2;
		unsigned __int64 cr8_load_exiting : 1;
		unsigned __int64 cr8_store_exiting : 1;
		unsigned __int64 use_tpr_shadow : 1;
		unsigned __int64 nmi_window_exiting : 1;
		unsigned __int64 mov_dr_exiting : 1;
		unsigned __int64 unconditional_io_exiting : 1;
		unsigned __int64 use_io_bitmaps : 1;
		unsigned __int64 reserved_5 : 1;
		unsigned __int64 monitor_trap_flag : 1;
		unsigned __int64 use_msr_bitmaps : 1;
		unsigned __int64 monitor_exiting : 1;
		unsigned __int64 pause_exiting : 1;
		unsigned __int64 active_secondary_controls : 1;
	} bits;
};


// IA32_VMX_SECONDARY_PROCESSOR_BASED_CTL_MSR (0x48B)
union __vmx_secondary_processor_based_control_t
{
	unsigned __int64 control;
	struct
	{
		unsigned __int64 virtualize_apic_accesses : 1;
		unsigned __int64 enable_ept : 1;
		unsigned __int64 descriptor_table_exiting : 1;
		unsigned __int64 enable_rdtscp : 1;
		unsigned __int64 virtualize_x2apic : 1;
		unsigned __int64 enable_vpid : 1;
		unsigned __int64 wbinvd_exiting : 1;
		unsigned __int64 unrestricted_guest : 1;
		unsigned __int64 apic_register_virtualization : 1;
		unsigned __int64 virtual_interrupt_delivery : 1;
		unsigned __int64 pause_loop_exiting : 1;
		unsigned __int64 rdrand_exiting : 1;
		unsigned __int64 enable_invpcid : 1;
		unsigned __int64 enable_vmfunc : 1;
		unsigned __int64 vmcs_shadowing : 1;
		unsigned __int64 enable_encls_exiting : 1;
		unsigned __int64 rdseed_exiting : 1;
		unsigned __int64 enable_pml : 1;
		unsigned __int64 use_virtualization_exception : 1;
		unsigned __int64 conceal_vmx_from_pt : 1;
		unsigned __int64 enable_xsave_xrstor : 1;
		unsigned __int64 reserved_0 : 1;
		unsigned __int64 mode_based_execute_control_ept : 1;
		unsigned __int64 reserved_1 : 2;
		unsigned __int64 use_tsc_scaling : 1;
	} bits;
};


// IA32_VMX_EXIT_CTL_MSR (0x483)
union __vmx_exit_control_t
{
	unsigned __int64 control;
	struct
	{
		unsigned __int64 reserved_0 : 2;
		unsigned __int64 save_dbg_controls : 1;
		unsigned __int64 reserved_1 : 6;
		unsigned __int64 host_address_space_size : 1;
		unsigned __int64 reserved_2 : 2;
		unsigned __int64 load_ia32_perf_global_control : 1;
		unsigned __int64 reserved_3 : 2;
		unsigned __int64 ack_interrupt_on_exit : 1;
		unsigned __int64 reserved_4 : 2;
		unsigned __int64 save_ia32_pat : 1;
		unsigned __int64 load_ia32_pat : 1;
		unsigned __int64 save_ia32_efer : 1;
		unsigned __int64 load_ia32_efer : 1;
		unsigned __int64 save_vmx_preemption_timer_value : 1;
		unsigned __int64 clear_ia32_bndcfgs : 1;
		unsigned __int64 conceal_vmx_from_pt : 1;
	} bits;
};




// IA32_VMX_ENTRY_CTL_MSR (0x484)
union __vmx_entry_control_t
{
	unsigned __int64 control;
	struct
	{
		unsigned __int64 reserved_0 : 2;
		unsigned __int64 load_dbg_controls : 1;
		unsigned __int64 reserved_1 : 6;
		unsigned __int64 ia32e_mode_guest : 1;
		unsigned __int64 entry_to_smm : 1;
		unsigned __int64 deactivate_dual_monitor_treament : 1;
		unsigned __int64 reserved_3 : 1;
		unsigned __int64 load_ia32_perf_global_control : 1;
		unsigned __int64 load_ia32_pat : 1;
		unsigned __int64 load_ia32_efer : 1;
		unsigned __int64 load_ia32_bndcfgs : 1;
		unsigned __int64 conceal_vmx_from_pt : 1;
	} bits;
};





