#include <asm/cpu_entry_area.h>
#include <linux/uaccess.h> //copy_from_user
#include <asm/pgtable.h>
#include <linux/slab.h> // mem functions
#include <linux/mm.h>
#include <asm/desc.h>
#include <asm/io.h> // virt_to_phys ---> use __pa ???


#include "inc/asm/asm_functions.h"
#include "inc/vmcs_encoding.h"
#include "inc/arch/segment.h"
#include "inc/utils/log.h"
#include "inc/ioooctls.h"
#include "inc/arch/crx.h"
#include "inc/arch/msr.h"
#include "inc/memory.h"
#include "inc/vmexit.h"
#include "inc/vmcs.h"

//
// START EPTP STUFF
//


// TODO: RETRIEVE CS AND MODE FROM HERE
void *guest_to_host(vm_t *vm, __u64 guest_addr)
{
	// TODO: Probably shouldn't assume this is physical
	__u64 guest_p_idx = guest_addr / PAGE_SIZE;
	__u64 guest_pt_idx = guest_p_idx % 512;
	__u64 guest_pd_idx = (guest_p_idx/512/512)%512;
	__u64 guest_pdpt_idx = (guest_p_idx/512/512) % 512;
	__u64 guest_pml4_idx = (guest_p_idx/512/512/512) % 512;
	__u64 guest_futureproof_idx = guest_p_idx/512/512/512/512;
	if (guest_futureproof_idx || guest_pml4_idx || guest_pdpt_idx) return false;

	__u64 host_physical = vm->pt[guest_pd_idx][guest_pt_idx].fields.PhysicalAddress << PAGE_SHIFT;
	__u64 host_virt_page = (__u64)phys_to_virt(host_physical);
	__u64 offset = guest_addr & 0xFFF;
	__u64 host_virt = host_virt_page + offset;
	return (void *)host_virt;
}

__u64 userspace_virt_to_phys(void *address)
{
	pgd_t *pgd;
	p4d_t *p4d;
	pud_t *pud;
	pmd_t *pmd;
	pte_t *ptep;
	spinlock_t *ptl;
	struct mm_struct *mm = current->mm;

	pgd = pgd_offset(mm, (__u64)address);
	if (pgd_none(*pgd) || unlikely(pgd_bad(*pgd)))
		return 0;

	p4d = p4d_offset(pgd, (__u64)address);
	if (p4d_none(*p4d) || unlikely(p4d_bad(*p4d)))
		return 0;

	pud = pud_offset(p4d, (__u64)address);
	if (pud_none(*pud) || unlikely(pud_bad(*pud)))
		return 0;

	pmd = pmd_offset(pud, (__u64)address);
	if (pmd_none(*pmd))
		return 0;

	ptep = pte_offset_map_lock(mm, pmd, (__u64)address, &ptl);
	pte_unmap_unlock(ptep, ptl);
	return pte_pfn(*ptep) << PAGE_SHIFT;
}

int ept_map_range(vm_t *vm, __u64 guest_pa, void *host_va_base, __u64 size)
{
  //jprintf(DEBUG, "size=%#llx hva=%#llx hpa=%#llx, gpa=%#llx\n", size, host_va_base, __pa(host_va_base), guest_pa);

	__u64 i;
	__u64 host_pa;
	void *host_va = host_va_base;

	// BUG: off by one to map additional host memory (potentially the EPTP!)
	for (i = 0; i <= size; i += PAGE_SIZE)
	{
		if ((__u64)host_va_base >> 63) host_va = host_va_base + i;
		else
		{
			host_pa = userspace_virt_to_phys(host_va_base + i);
			host_va = phys_to_virt(host_pa);
		}

		//jprintf(DEBUG, "mapping (host virt)0x%llx / (host kern)0x%llx to (guest phys)0x%llx\n", m.userspace_addr+i, virt_addr, m.guest_phys_addr+i);
		if (!ept_map(vm, guest_pa + i, (void *)host_va))
		{
                  //jprintf(DEBUG, "mapping failed!\n");
			return false;
		}
	}
	return true;
}

void ept_unmap(vm_t *vm, __u64 guest_pa)
{
	__u64 guest_p_idx = guest_pa / PAGE_SIZE;
	__u64 guest_pt_idx = guest_p_idx % 512;
	__u64 guest_pd_idx = (guest_p_idx/512/512)%512;
	__u64 guest_pdpt_idx = (guest_p_idx/512/512) % 512;
	__u64 guest_pml4_idx = (guest_p_idx/512/512/512) % 512;
	__u64 guest_futureproof_idx = guest_p_idx/512/512/512/512;
	if (guest_futureproof_idx || guest_pml4_idx || guest_pdpt_idx) return;

	vm->pt[guest_pd_idx][guest_pt_idx].all = 0;
}

void ept_unmap_range(vm_t *vm, __u64 guest_pa, __u64 size)
{
	__u64 i;
	for (i = 0; i < size; i += PAGE_SIZE)
	{
		ept_unmap(vm, guest_pa+i);
	}
}

int ept_map(vm_t *vm, __u64 guest_pa, void *host_va)
{
	__u64 host_pa = (__u64) virt_to_phys(host_va);
	__u64 host_ppi = host_pa / PAGE_SIZE;

	__u64 guest_p_idx = guest_pa >> PAGE_SHIFT;
	__u64 guest_pt_idx = guest_p_idx % 512;
	__u64 guest_pd_idx = (guest_p_idx/512) % 512;
	__u64 guest_pdpt_idx = (guest_p_idx/512/512) % 512;
	__u64 guest_pml4_idx = (guest_p_idx/512/512/512) % 512;
	__u64 guest_futureproof_idx = guest_p_idx/512/512/512/512;

	if (guest_futureproof_idx)
	{
          //jprintf(ERROR, "Address %#llx will have to be supported by a future x86 CPU.", guest_pa);
		return false;
	}

	if (guest_pml4_idx || guest_pdpt_idx)
	{
          //jprintf(ERROR, "OOO does not support addresses as high as %#llx.", guest_pa);
		return false;
	}

	//jprintf(DEBUG, "hvpa=%llx hppa=%llx hppi=%llx gpa=%#llx, gpi=%#llx gpti=%#llx gpdi=%#llx\n", host_va, host_pa, host_ppi, guest_pa, guest_p_idx, guest_pt_idx, guest_pd_idx);

	vm->pt[guest_pd_idx][guest_pt_idx].fields.AccessedFlag = 0;
	vm->pt[guest_pd_idx][guest_pt_idx].fields.DirtyFlag = 0;
	vm->pt[guest_pd_idx][guest_pt_idx].fields.EPTMemoryType = 6; // write back
	vm->pt[guest_pd_idx][guest_pt_idx].fields.Execute = 1;
	vm->pt[guest_pd_idx][guest_pt_idx].fields.ExecuteForUserMode = 1;
	vm->pt[guest_pd_idx][guest_pt_idx].fields.IgnorePAT = 1;
	vm->pt[guest_pd_idx][guest_pt_idx].fields.PhysicalAddress = host_ppi;
	vm->pt[guest_pd_idx][guest_pt_idx].fields.Read = 1;
	vm->pt[guest_pd_idx][guest_pt_idx].fields.SuppressVE = 0;
	vm->pt[guest_pd_idx][guest_pt_idx].fields.Write = 1;

	//if (!guest_pd_idx && !guest_pt_idx) jprintf(DEBUG, "pt[0][0]=%#llx", vm->pt[0][0].all);

	return true;
}

void ept_init(vm_t *vm)
{
	int i;

	//jprintf(DEBUG, "eptp=%llx pml4=%llx pdpt=%llx pdt=%llx pt=%llx\n", &vm->eptp, &vm->pml4, &vm->pdpt, &vm->pd, &vm->pt);
	//jprintf(DEBUG, "PHYSICAL: eptp=%llx pml4=%llx pdpt=%llx pdt=%llx pt=%llx\n", __pa(&vm->eptp), __pa(&vm->pml4), __pa(&vm->pdpt), __pa(&vm->pd), __pa(&vm->pt));

	BUG_ON((PAGE_SIZE-1)&(__u64)&vm->eptp);
	BUG_ON((PAGE_SIZE-1)&(__u64)&vm->pml4);
	BUG_ON((PAGE_SIZE-1)&(__u64)&vm->pdpt);
	BUG_ON((PAGE_SIZE-1)&(__u64)&vm->pd);
	for (i = 0; i < 512; i++) BUG_ON((PAGE_SIZE-1)&(__u64)&vm->pt[i]);

	// top level EPT just has one entry --- we have it as an array for alignment
	vm->eptp[0].all = 0; // write back
	vm->eptp[0].fields.MemoryType = 6; // write back
	vm->eptp[0].fields.PageWalkLength = 4 - 1;
	vm->eptp[0].fields.DirtyAndAceessEnabled = 1;
	vm->eptp[0].fields.Reserved1 = 0;
	vm->eptp[0].fields.PML4Address = virt_to_phys(&vm->pml4[0]) / PAGE_SIZE;
	vm->eptp[0].fields.Reserved2 = 0;

	// a page of PML4 (page-map level 4) addresses 256TB memory, 512GB per entry
	vm->pml4[0].all = 0;
	vm->pml4[0].fields.Read = 1;
	vm->pml4[0].fields.Write = 1;
	vm->pml4[0].fields.Execute = 1;
	vm->pml4[0].fields.ExecuteForUserMode = 0;
	vm->pml4[0].fields.PhysicalAddress = (virt_to_phys(&vm->pdpt[0])) / PAGE_SIZE;

	// a page of PDPT (page directory pointer table) addresses 512GB memory, 1GB per entry
	vm->pdpt[0].all = 0;
	vm->pdpt[0].fields.Read = 1;
	vm->pdpt[0].fields.Write = 1;
	vm->pdpt[0].fields.Execute = 1;
	vm->pdpt[0].fields.ExecuteForUserMode = 0;
	vm->pdpt[0].fields.PhysicalAddress = (virt_to_phys(&vm->pd[0])) / PAGE_SIZE;

	// a page of PD (page directory) addresses 1GB memory, 2MB per entry
	for (i = 0; i < 512; i++)
	{
		vm->pd[i].all = 0;
		vm->pd[i].fields.Read = 1;
		vm->pd[i].fields.Write = 1;
		vm->pd[i].fields.Execute = 1;
		vm->pd[i].fields.ExecuteForUserMode = 0;
		vm->pd[i].fields.PhysicalAddress = (virt_to_phys(&vm->pt[i][0])) / PAGE_SIZE;
	}

	//jprintf(DEBUG, "pml4[0]=%#llx pdpt[0]=%#llx pd[0]=%#llx", vm->pml4[0].all, vm->pdpt[0].all, vm->pd[0].all);

	// page of PT (page table) addresses 2MB memory, 4KB per entry
	// (we'll map these later)
	vm->pt[0][0].all = 0;
}

void destroy_eptp(EPTP *eptp)
{
}

//
// END EPTP STUFF
//

//
// START VMCS STUFF
//

__u64 get_segment_base(__u64 gdt_base, __u16 segment)
{
	__u64 segment_base = 0;
	union __segment_selector_t selector;
	struct __segment_descriptor_32_t *descriptor;
	struct __segment_descriptor_32_t *descriptor_table;
	// flags is the whole guy
	selector.flags = segment;

	//jprintf(DEBUG, "Segment Selector: 0x%x  selector.index: 0x%x   selector.table: 0x%x\n", segment, selector.index, selector.table);
	// check for null selector
	if (selector.index == 0)
	{
		return 0;
	}

	if (!(selector.flags & ~3))
	{
          //jprintf(DEBUG, "Others would have returned...\n");
	}

	descriptor_table = (struct __segment_descriptor_32_t*)gdt_base;
	descriptor = &descriptor_table[selector.index];

	// Reference Vol 3A Fig 3-8 (page 99) + the struct
	segment_base = (unsigned)(descriptor->base_low | ((descriptor->base_middle) << 16) | ((descriptor->base_high) << 24));

	// Check if we have a System Entry. Apparently we only care about TSS entries
	// But if we have one we need handle it differently because system descs are expanded to 16 bytes (3-16 Vol 3A [pg 104])
	if (descriptor->system == 0 &&
	        ((descriptor->type == SEGMENT_DESCRIPTOR_TYPE_TSS_AVAILABLE) || (descriptor->type == SEGMENT_DESCRIPTOR_TYPE_TSS_BUSY)))
	{
		struct __segment_descriptor_64_t *expanded_descriptor;
		expanded_descriptor = (struct __segment_descriptor_64_t*)descriptor;
		segment_base |= (__u64)expanded_descriptor->base_upper << 32;
	}

	return segment_base;
}

int vmcs_alloc( vcpu_t *vcpu )
{
	BUG_ON((PAGE_SIZE-1)&(__u64)&vcpu->vmcs);
	BUG_ON((PAGE_SIZE-1)&(__u64)&vcpu->msr_bitmap);

	// allocate memory needed for our vmcs
	vcpu->vmcs_pa = __pa(&vcpu->vmcs);
	vcpu->vmcs.header.bits.revision_identifier = vmcs_revision_id();
	vcpu->vmcs.header.bits.shadow_vmcs_indicator = 0;
	//jprintf(INIT, "vmcs: 0x%08llx\n", (__u64)(vcpu->vmcs));
	return true;
}

void vmcs_reinit(vcpu_t *vcpu)
{
	int ret = 0;

	// ###########################
	// # Guest State Area (24.4) #
	// ###########################

	ret |= vmwrite(GUEST_RSP, vcpu->state.regs.rsp);
	ret |= vmwrite(GUEST_RIP, vcpu->state.regs.rip);
	// TODO: why does this cause error 33? ret |= vmwrite(GUEST_RFLAGS, vcpu->state.regs.rflags);

	// [+] EPTP: Extended Page Table Pointer (24.1.11)
	if (vcpu->vm->eptp[0].all == 0) panic("EPT is grilled up\n");
	//jprintf(DEBUG, "EPTP comin right atchya: %#llx\n", vcpu->vm->eptp[0].all);
	ret |= vmwrite(EPT_POINTER, vcpu->vm->eptp[0].all);

	// need to zero the bottom 3 bits to ensure RPL and TI are set to 0 for host selectors
	__u64 selector_mask = 7;
	struct __pseudo_descriptor_64_t gdtr;
	struct __pseudo_descriptor_64_t idtr;
	_sgdt(&gdtr);
	_sidt(&idtr);

	// ##########################
	// # Host State Area (24.5) #
	// ##########################
	ret |= vmwrite(HOST_CR0, _read_cr0());
	ret |= vmwrite(HOST_CR3, _read_cr3());
	ret |= vmwrite(HOST_CR4, _read_cr4());

	// host rsp taken care on in vmlaunch
	ret |= vmwrite(HOST_RIP, (__u64)vm_entrypoint);
	//jprintf(DEBUG, "HOST_RIP: va=%#lx pa=%#lx\n", (__u64)vm_entrypoint, __pa(vm_entrypoint));

	// host selector fields
	ret |= vmwrite(HOST_CS_SELECTOR, _read_cs() & ~selector_mask);
	ret |= vmwrite(HOST_SS_SELECTOR, _read_ss() & ~selector_mask);
	ret |= vmwrite(HOST_DS_SELECTOR, _read_ds() & ~selector_mask);
	ret |= vmwrite(HOST_ES_SELECTOR, _read_es() & ~selector_mask);
	ret |= vmwrite(HOST_FS_SELECTOR, _read_fs() & ~selector_mask);
	ret |= vmwrite(HOST_GS_SELECTOR, _read_gs() & ~selector_mask);
	ret |= vmwrite(HOST_TR_SELECTOR, _read_tr() & ~selector_mask);

	//jprintf(DEBUG, "HOST_cs_SELECTOR: 0x%x\n", _read_cs() & ~selector_mask);
	//jprintf(DEBUG, "HOST_ss_SELECTOR: 0x%x\n", _read_ss() & ~selector_mask);
	//jprintf(DEBUG, "HOST_ds_SELECTOR: 0x%x\n", _read_ds() & ~selector_mask);
	//jprintf(DEBUG, "HOST_es_SELECTOR: 0x%x\n", _read_es() & ~selector_mask);
	//jprintf(DEBUG, "HOST_fs_SELECTOR: 0x%x\n", _read_fs() & ~selector_mask);
	//jprintf(DEBUG, "HOST_gs_SELECTOR: 0x%x\n", _read_gs() & ~selector_mask);
	//jprintf(DEBUG, "HOST_tr_SELECTOR: 0x%x\n", _read_tr() & ~selector_mask);

	// host bases
	ret |= vmwrite(HOST_FS_BASE, __readmsr(IA32_FS_BASE));
	ret |= vmwrite(HOST_GS_BASE, __readmsr(IA32_GS_BASE));
	ret |= vmwrite(HOST_TR_BASE, get_segment_base(gdtr.base_address, _read_tr()));
	ret |= vmwrite(HOST_GDTR_BASE, gdtr.base_address);
	ret |= vmwrite(HOST_IDTR_BASE, idtr.base_address);

	//jprintf(DEBUG, "HOST_FS_BASE: 0x%lx\n", __readmsr(IA32_FS_BASE));
	//jprintf(DEBUG, "HOST_GS_BASE: 0x%lx\n", __readmsr(IA32_GS_BASE));
	//jprintf(DEBUG, "HOST_TR_BASE: 0x%lx\n", get_segment_base(gdtr.base_address, _read_tr()));
	//jprintf(DEBUG, "HOST_GDTR_BASE: 0x%lx\n", gdtr.base_address);
	//jprintf(DEBUG, "HOST_IDTR_BASE: 0x%lx\n", idtr.base_address);


	// host MSRs
	ret |= vmwrite(HOST_IA32_SYSENTER_CS, __readmsr(IA32_SYSENTER_CS));
	ret |= vmwrite(HOST_IA32_SYSENTER_ESP, __readmsr(IA32_SYSENTER_ESP));
	ret |= vmwrite(HOST_IA32_SYSENTER_EIP, __readmsr(IA32_SYSENTER_EIP));
	// TODO Check for support of these below with entry controls
	// not supported on my laptop, just commenting out for now
	ret |= vmwrite(HOST_IA32_PERF_GLOBAL_CTRL, __readmsr(IA32_PERF_GLOBAL_CTRL));
	//ret |= vmwrite(HOST_IA32_PAT, __readmsr(IA32_PAT));
	//ret |= vmwrite(HOST_IA32_EFER, __readmsr(IA32_EFER));

	if (ret) panic("no good (host)\n");
	//jprintf(DEBUG, "SUCESSFULLY SET UP HOST VMCS STATE!!\n");
}

void vmcs_full_init(vm_t *vm, vcpu_t *vcpu)
{
	int ret = 0;

	// before addition
	//vmwrite(CPU_BASED_VM_EXEC_CONTROL, 0xb5986dfa);
	vmwrite(CPU_BASED_VM_EXEC_CONTROL, 0xa5986dfa);
	//vmwrite(CR0_GUEST_HOST_MASK, 0xfffffffffffffff7);
	vmwrite(CR0_GUEST_HOST_MASK, 0);
	vmwrite(CR0_READ_SHADOW, 0x60000010);
	vmwrite(CR3_TARGET_COUNT, 0x0);
	//vmwrite(CR4_GUEST_HOST_MASK, 0xffffffffffffe871);
	vmwrite(CR4_GUEST_HOST_MASK, 0);
	vmwrite(CR4_READ_SHADOW, 0x0);
	vmwrite(EXCEPTION_BITMAP, 0x60042);

	// guest shit from KVM
	vmwrite(GUEST_CR0, 0x30);
	//vmwrite(GUEST_CR0, 0x60000010);
	vmwrite(GUEST_CR3, 0x0);
	vmwrite(GUEST_CR4, 0x2040);
	vmwrite(GUEST_CS_AR_BYTES, 0x9b);
	vmwrite(GUEST_CS_BASE, 0xf0000);
	vmwrite(GUEST_CS_LIMIT, 0xffff);
	vmwrite(GUEST_CS_SELECTOR, 0xf000);
	vmwrite(GUEST_DR7, 0x400);
	vmwrite(GUEST_DS_AR_BYTES, 0x93);
	vmwrite(GUEST_DS_BASE, 0x0);
	vmwrite(GUEST_DS_LIMIT, 0xffff);
	vmwrite(GUEST_DS_SELECTOR, 0x0);
	vmwrite(GUEST_ES_AR_BYTES, 0x93);
	vmwrite(GUEST_ES_BASE, 0x0);
	vmwrite(GUEST_ES_LIMIT, 0xffff);
	vmwrite(GUEST_ES_SELECTOR, 0x0);
	vmwrite(GUEST_FS_AR_BYTES, 0x93);
	vmwrite(GUEST_FS_BASE, 0x0);
	vmwrite(GUEST_FS_LIMIT, 0xffff);
	vmwrite(GUEST_FS_SELECTOR, 0x0);
	vmwrite(GUEST_GDTR_BASE, 0x0);
	vmwrite(GUEST_GDTR_LIMIT, 0xffff);
	vmwrite(GUEST_GS_AR_BYTES, 0x93);
	vmwrite(GUEST_GS_BASE, 0x0);
	vmwrite(GUEST_GS_LIMIT, 0xffff);
	vmwrite(GUEST_GS_SELECTOR, 0x0);
	vmwrite(GUEST_IA32_DEBUGCTL, 0x0);
	vmwrite(GUEST_IA32_EFER, 0x0);
	vmwrite(GUEST_IA32_PAT, 0x7040600070406);
	vmwrite(GUEST_IDTR_BASE, 0x0);
	vmwrite(GUEST_IDTR_LIMIT, 0xffff);
	vmwrite(GUEST_INTERRUPTIBILITY_INFO, 0x0);
	vmwrite(GUEST_LDTR_AR_BYTES, 0x82);
	vmwrite(GUEST_LDTR_BASE, 0x0);
	vmwrite(GUEST_LDTR_LIMIT, 0xffff);
	vmwrite(GUEST_LDTR_SELECTOR, 0x0);
	vmwrite(GUEST_PENDING_DBG_EXCEPTIONS, 0x0);
	vmwrite(GUEST_PML_INDEX, 0x1ff);
	vmwrite(GUEST_RFLAGS, 0x2);
	// set from state: vmwrite(GUEST_RIP                   , 0xfff0);
	// set from state: vmwrite(GUEST_RSP                   , 0x0);
	vmwrite(GUEST_SS_AR_BYTES, 0x93);
	vmwrite(GUEST_SS_BASE, 0x0);
	vmwrite(GUEST_SS_LIMIT, 0xffff);
	vmwrite(GUEST_SS_SELECTOR, 0x0);
	vmwrite(GUEST_SYSENTER_CS, 0x0);
	vmwrite(GUEST_SYSENTER_EIP, 0x0);
	vmwrite(GUEST_SYSENTER_ESP, 0x0);
	vmwrite(GUEST_TR_AR_BYTES, 0x8b);
	vmwrite(GUEST_TR_BASE, 0x0);
	vmwrite(GUEST_TR_LIMIT, 0xffff);
	vmwrite(GUEST_TR_SELECTOR, 0x0);

	// host shit from KVM
	// done elsewhere: vmwrite(HOST_CR0                    , 0x80050033);
	// done elsewhere: vmwrite(HOST_CR3                    , 0x46a6004);
	// done elsewhere: vmwrite(HOST_CR4                    , 0x362ef0);
	// done elsewhere: vmwrite(HOST_CS_SELECTOR            , 0x10);
	// done elsewhere: vmwrite(HOST_DS_SELECTOR            , 0x0);
	// done elsewhere: vmwrite(HOST_ES_SELECTOR            , 0x0);
	// done elsewhere: vmwrite(HOST_FS_BASE                , 0x7f1c2b87b700);
	// done elsewhere: vmwrite(HOST_FS_SELECTOR            , 0x0);
	// done elsewhere: vmwrite(HOST_GDTR_BASE              , 0xfffffe0000001000);
	// done elsewhere: vmwrite(HOST_GS_BASE                , 0xffff888007200000);
	// done elsewhere: vmwrite(HOST_GS_SELECTOR            , 0x0);
	// done elsewhere: vmwrite(HOST_IA32_SYSENTER_CS       , 0x10);
	// done elsewhere: vmwrite(HOST_IA32_SYSENTER_EIP      , 0xffffffff81c01460);
	// done elsewhere: vmwrite(HOST_IA32_SYSENTER_ESP      , 0xfffffe0000002200);
	// done elsewhere: vmwrite(HOST_IDTR_BASE              , 0xfffffe0000000000);
	// done elsewhere: vmwrite(HOST_RIP                    , 0xffffffffc00b1100);
	// done elsewhere: vmwrite(HOST_SS_SELECTOR            , 0x18);
	// done elsewhere: vmwrite(HOST_TR_BASE                , 0xfffffe0000003000);
	// done elsewhere: vmwrite(HOST_TR_SELECTOR            , 0x40);
	// reproduced below: vmwrite(PIN_BASED_VM_EXEC_CONTROL   , 0x7f);
	
	vmwrite(HOST_IA32_EFER, 0xd01);
	//vmwrite(HOST_IA32_PAT, 0x407050600070106);
	// TODO: LOOK INTO IT
	//vmwrite(MSR_BITMAP, 0x445f000);
	vmwrite(PAGE_FAULT_ERROR_CODE_MASK, 0x0);
	vmwrite(PAGE_FAULT_ERROR_CODE_MATCH, 0x0);
	// WORKS WITHOUT THIS: vmwrite(PML_ADDRESS                 , 0x445d000);
	
	// before addition
	//vmwrite(SECONDARY_VM_EXEC_CONTROL, 0x328e2);
	vmwrite(SECONDARY_VM_EXEC_CONTROL, 0x128a2);
	vmwrite(TPR_THRESHOLD, 0x0);
	// WORKS WITHOUT THIS: vmwrite(TSC_OFFSET                  , 0xffffffff5d33827e);
	vmwrite(VIRTUAL_APIC_PAGE_ADDR, 0x0);
	vmwrite(VIRTUAL_PROCESSOR_ID, 0x1);
	vmwrite(VMCS_GUEST_ACTIVITY_STATE, 0x0);
	vmwrite(VMCS_LINK_POINTER, 0xffffffffffffffff);
	// WORKS WITHOUT THIS: vmwrite(VMREAD_BITMAP               , 0x466a000);
	// WORKS WITHOUT THIS: vmwrite(VMWRITE_BITMAP              , 0x4669000);
	vmwrite(VMX_PREEMPTION_TIMER_VALUE, 0xffffffff);
	vmwrite(VM_ENTRY_CONTROLS, 0xd1ff);
	vmwrite(VM_ENTRY_INTR_INFO_FIELD, 0x0);
	// WORKS WITHOUT THIS: vmwrite(VM_ENTRY_MSR_LOAD_ADDR      , 0x46ab860);
	vmwrite(VM_ENTRY_MSR_LOAD_COUNT, 0x0);
	vmwrite(VM_EXIT_CONTROLS, 0x2befff);
	// WORKS WITHOUT THIS: vmwrite(VM_EXIT_MSR_LOAD_ADDR       , 0x46ab8f0);
	//vmwrite(VM_EXIT_MSR_LOAD_COUNT, 0x0);
	//vmwrite(VM_EXIT_MSR_STORE_COUNT, 0x0);
	vmwrite(VM_FUNCTION_CONTROL, 0x0);
	vmwrite(XSS_EXIT_BITMAP, 0x0);

	//
	// fix some shit up
	//

	// pin exec controls. -- add premption timer
	ret |= vmwrite(PIN_BASED_VM_EXEC_CONTROL, 0x7e);
	union __vmx_pinbased_control_msr_t pin_controls;
	ret |= vmread(PIN_BASED_VM_EXEC_CONTROL, &pin_controls.control);
	//jprintf(DEBUG, "premption timer: %d\n", pin_controls.bits.vmx_preemption_timer);
	pin_controls.bits.vmx_preemption_timer = 1;
	//jprintf(ERROR, "\n\nPIN_BASED_VM_EXEC_CONTROL,: 0x%lx\n", pin_controls.control);
	vmwrite(PIN_BASED_VM_EXEC_CONTROL, pin_controls.control);

	//// pin exec controls. -- add premption timer
	//union __vmx_pinbased_control_msr_t pin_controls;
	//ret |= vmread(PIN_BASED_VM_EXEC_CONTROL, &pin_controls.control);
	//pin_controls.bits.vmx_preemption_timer = 1;
	//vmwrite(PIN_BASED_VM_EXEC_CONTROL, pin_controls.control);

	__u64 vmx_misc = __readmsr(IA32_VMX_MISC_MSR);
	//jprintf(DEBUG, "MISC_MSR: 0x%llx\n", vmx_misc);
	vmx_misc &= 0xFFFFFFFFFFFFFFE1;
	__u64 check = (__u32)((vmx_misc & 0xffffffff00000000) >> 32) | (__u32)(vmx_misc & 0xffffffff);
	//jprintf(DEBUG, "check: 0x%llx\n", check);
	//__wrmsr(IA32_VMX_MISC_MSR, (__u32)(vmx_misc & 0xffffffff), (__u32)((vmx_misc & 0xffffffff00000000) >> 32));
	
	// let's add a monitor trap man
	union __vmx_primary_processor_based_control_t primary_proc_controls;
	vmread(CPU_BASED_VM_EXEC_CONTROL, &primary_proc_controls.control);
	//jprintf(ERROR, "\n\n USE MSR BITMAP: %d\n", primary_proc_controls.bits.use_msr_bitmaps);
	//primary_proc_controls.bits.monitor_trap_flag = 1;
	primary_proc_controls.bits.use_msr_bitmaps = 0;
	//jprintf(ERROR, "\n\nFINAL CPU_BASED_VM_EXEC_CONTROL: 0x%lx\n", primary_proc_controls.control);
	vmwrite(CPU_BASED_VM_EXEC_CONTROL, primary_proc_controls.control);


	union __vmx_secondary_processor_based_control_t secondary_proc_controls;
	vmread(SECONDARY_VM_EXEC_CONTROL, &secondary_proc_controls.control);
	secondary_proc_controls.bits.wbinvd_exiting = 0;
	secondary_proc_controls.bits.enable_pml = 0;
	//jprintf(ERROR, "\n\nFINAL SECONDARY_VM_EXEC_CONTROL: 0x%lx\n", secondary_proc_controls.control);
	vmwrite(SECONDARY_VM_EXEC_CONTROL, secondary_proc_controls.control);

	vmcs_reinit(vcpu);

	if (ret) panic("no good (full)\n");
	//jprintf(DEBUG, "SUCESSFULLY SET UP OUR VMCS!!\n");
}

//
// END VMCS STUFF
//

//
// START VMEXIT STUFF
//

void update_rip(vcpu_t *vcpu, int instr_size)
{
	vcpu->state.regs.rip += instr_size;
	return;
}

void yeet(vcpu_t *vcpu)
{
	jprintf(YEET, "YEET");
	vcpu->state.regs.rip = 0x59454554;
	return;
}

int get_guest_mode(vcpu_t *vcpu)
{
	union __cr0_t cr0;
	if (vmread(GUEST_CR0, &cr0.control))
	{
		panic("vmread");
	}

	int mode = -1;

	if ( (cr0.bits.protection_enable == 0) && (cr0.bits.paging_enable == 0) )
	{
		mode = MODE_REAL;
	}

	else if ( (cr0.bits.protection_enable == 1) && (cr0.bits.paging_enable == 0) )
	{
		mode = MODE_PROTECTED;
	}

	else if ( (cr0.bits.protection_enable == 1) && (cr0.bits.paging_enable == 1) )
	{
		mode = MODE_PAGING;
	}


	return mode;
}

__u64 get_guest_rip(vcpu_t *vcpu) {
	__u64 rip_gpa = 0;
	__u64 guest_cs  = 0;
	__u8 mode = get_guest_mode(vcpu);
	if (mode == MODE_REAL) {
		if (vmread(GUEST_CS_SELECTOR, &guest_cs)) panic("vmread");
		rip_gpa = guest_cs << 4 | vcpu->state.regs.rip;
	}

	// TODO IS THIS CORRECT FOR paging enabled??
	else {
		rip_gpa = vcpu->state.regs.rip;
	}

	return rip_gpa;
}

int vmexit_handle(vcpu_t *vcpu)
{
	union __vmx_exit_info_t exit_info = {0};
	union __vmx_exit_qual_t exit_qual = {0};
	int ret = 0;
	uint64_t guest_cs = -1;
	__u64 fault_gpa = 0;
	__u64 fault_gva = 0;
	void *fault_hva = NULL;
	__u64 rip_gpa = 0;
	void *rip_hva = NULL;
	ret = vmread(VM_EXIT_REASON, &exit_info.flags);
	ret |= vmread(GUEST_RIP, &vcpu->state.regs.rip);
	//ret |= vmread(GUEST_CS_SELECTOR, &guest_cs);
	vcpu->state.mode = get_guest_mode(vcpu);
	// TODO: is this always physical?
	vcpu->state.rip_gpa = get_guest_rip(vcpu);

	//uint64_t guest_addr;
	ret |= vmread(GUEST_LINEAR_ADDRESS, &fault_gpa);
	ret |= vmread(GUEST_PHYSICAL_ADDRESS, &fault_gva);
	if (ret != 0)
	{
		jprintf(ERROR, "vmread failed\n");
		return -1;
	}
	//jprintf(ERROR, "VMEXIT: rip_pa=%#llx pa=%#llx la=%#llx reason=%d\n", vcpu->state.rip_gpa, fault_gpa, fault_gva, exit_info.bits.exit_reason);
	//jprintf(ERROR, "..regs: rax=%#016llx rbx=%#016llx rcx=%#016llx rdx=%#016llx\n", vcpu->state.regs.rax, vcpu->state.regs.rbx, vcpu->state.regs.rcx, vcpu->state.regs.rdx);
	//jprintf(ERROR, "..regs: rdi=%#016llx rsi=%#016llx rsp=%#016llx rbp=%#016llx\n", vcpu->state.regs.rdi, vcpu->state.regs.rsi, vcpu->state.regs.rsp, vcpu->state.regs.rbp);

	switch (exit_info.bits.exit_reason)
	{
		case EXITCOOODE_CPUID:
                  //jprintf(DEBUG, "Guest executed cpuid\n");

			// BUMO
			//jprintf(DEBUG, "Updating OOOOOOO\n");
			vcpu->state.regs.rax = 0x4f4f4f4f4f4f4f4f;
			vcpu->state.regs.rbx = 0x4f4f4f4f4f4f4f4f;
			vcpu->state.regs.rcx = 0x4f4f4f4f4f4f4f4f;
			vcpu->state.regs.rdx = 0x4f4f4f4f4f4f4f4f;
			update_rip(vcpu, 2);
			return OOO_VMEXIT_HANDLED;
		case EXITCOOODE_HLT:
                  //jprintf(DEBUG, "Guest halted\n");
			update_rip(vcpu, 1);
			break;
		case EXITCOOODE_IO:
                  //jprintf(DEBUG, "Guest did in/out\n");
                  //jprintf(DEBUG, "guest_cs: 0x%llx\n", guest_cs);

			rip_gpa = get_guest_rip(vcpu);
			rip_hva = (void *)guest_to_host(vcpu->vm, rip_gpa);
			//jprintf(DEBUG, "pio rip gpa=0x%llx hva=%#llx\n", rip_gpa, rip_hva);

			// TODO: CHECK RETURN VALUE AND INJECT UND ??
			//decode_pio(vcpu, ((__u64 *)rip_hva)[0], (struct pio_t*)&vcpu->state.io);
			break;
		case EXITCOOODE_EPT_VIOLATION:
			//jprintf(YEET, "\n\nMMIO MAN\n\n");
			rip_gpa = get_guest_rip(vcpu);
			rip_hva = (void *)guest_to_host(vcpu->vm, rip_gpa);

			vmread(GUEST_PHYSICAL_ADDRESS, &fault_gpa);
			fault_hva = (void *)guest_to_host(vcpu->vm, fault_gpa);

			//int i;
			/* for (i=0; i < 2; i++) { */
			/* 	jprintf(DEBUG, "blah: 0x%llx\n", ((__u64 *)rip_hva)[i]); */
			/* } */
			//jprintf(DEBUG, "mmio rip_gpa=0x%llx rip_hva=0x%llx fault_gpa=%#llx fault_hva=%#llx\n", rip_gpa, rip_hva, fault_gpa, fault_hva);

			// TODO: heck yeah
			vmread(EXIT_QUALIFICATION, &exit_qual.flags);
			// TODO: don't update instr, don't return to guest
			//if (exit_qual.bits.fetch) {
			//	BUG_ON(1);
			//}

			// fill in the struct more?
			vcpu->state.mmio.phys_addr = fault_gpa;
			vcpu->state.mmio.is_write = (exit_qual.bits.data_write) ? 1 : 0;
			break;
		case EXITCOOODE_RDMSR:
                  //jprintf(DEBUG, "RDMSR OF MSR: 0x%x\n", vcpu->state.regs.rcx);
			// just set edx:eax
			vcpu->state.regs.rdx = 0x0;
			vcpu->state.regs.rax = 0x0;
			// we'll just support apic base for BSP check
			if (vcpu->state.regs.rcx == IA32_APIC_BASE) {
				// set bit 8 if we're vcpu0
				vcpu->state.regs.rax = APIC_DEFAULT_ADDRESS | IA32_APICBASE_ENABLE;
				if (vcpu->guest_cpu_id == 0) {
					// set bit 8 (BSP flag)
					vcpu->state.regs.rax |= IA32_APICBASE_BSP;
				}
			}
			update_rip(vcpu, 2);
			return OOO_VMEXIT_HANDLED;

		case EXITCOOODE_WRMSR:
			update_rip(vcpu, 2);
			return OOO_VMEXIT_HANDLED;

		case EXITCOOODE_TRAP_FLAG:
			rip_gpa = get_guest_rip(vcpu);
			//jprintf(ERROR, "\n\nSINGLE STEP: rip_gpa 0x%llx\n\n", rip_gpa);
			break;


		default:
			rip_gpa = get_guest_rip(vcpu);
			//jprintf(EXIT, "Currently unhandled exit reason %d  at guest_rip: 0x%llx\n", exit_info.bits.exit_reason, rip_gpa);
			rip_hva = (void *)guest_to_host(vcpu->vm, rip_gpa)-4;
			/* for (i=0; i < 2; i++) { */
			/* 	jprintf(DEBUG, "blah: 0x%llx\n", ((__u64 *)rip_hva)[i]); */
			/* } */
			break;
	}

	return exit_info.bits.exit_reason;
}

//
// END VMEXIT STUFF
//

//
// START VCPU
//

vcpu_t *vcpu_alloc(vm_t *vm, int id)
{
	vcpu_t *vcpu = &vm->vcpus[id];
	vcpu->guest_cpu_id = id;
	vcpu->host_cpu_id = -1;
	vcpu->vm = vm;

	// alloc our vmcs
	if (!vmcs_alloc(vcpu)) panic("Failed to alloc vmcs\n");

	//printk(KERN_INFO "vmxon_region[0]: 0x%08llx\n", vmxon_region[0]);
	return vcpu;
}

void vcpu_handle_vmcs(vcpu_t *vcpu, int cur_cpu_id)
{
	uint64_t cur_vmcs_pa;
	if (_vmptrst((__u64) &cur_vmcs_pa) < 0) panic("VMPTRST FAILED\n");

	// if the current vmcs isn't that our our vcpu, load it
	if (vcpu->vmcs_pa != cur_vmcs_pa)
	{
          //jprintf(DEBUG, "Reloading vmcs pointer: %#llx.", vcpu->vmcs_pa);
		if(_vmptrld((__u64) &vcpu->vmcs_pa) < 0) panic("VMPTRLD Failed\n");
	}

	vcpu->host_cpu_id = cur_cpu_id;

	if (!vcpu->initialized)
	{
          //jprintf(DEBUG, "Initializing VMCS!\n");
		vmcs_full_init(vcpu->vm, vcpu);
		vcpu->initialized = 1;
	}
	else
	{
          //jprintf(DEBUG, "Re-initializing VMCS host section!\n");
		vmcs_reinit(vcpu);
	}
}

int vcpu_update_sregs(vcpu_t *vcpu) {
	/* out (OOO_GET_SREGS) / in (OOO_SET_SREGS) */
	//struct ooo_segment cs, ds, es, fs, gs, ss;
	//struct ooo_segment tr, ldt;
	//struct ooo_dtable gdt, idt;
	//__u64 cr0, cr2, cr3, cr4, cr8;
	//__u64 efer;
	//__u64 apic_base;
	//__u64 interrupt_bitmap[(OOO_NR_INTERRUPTS + 63) / 64];

	return 0;
}

int vcpu_run(vcpu_t *vcpu)
{
	int cur_cpu_id = smp_processor_id();
	int ret = 0;

	vcpu_handle_vmcs(vcpu, cur_cpu_id);

	// write in sregs
	if (vmwrite(GUEST_RSP, vcpu->state.regs.rsp)) {
		panic("vmwrite");
	}

	__u64 rip_gpa = get_guest_rip(vcpu);
	__u64 rip_hva = (void *)guest_to_host(vcpu->vm, rip_gpa);
	int i;

	if (!vcpu->launched)
	{
          jprintf(DEBUG, "Launching VM!\n");
          ret = _vmlaunch(&vcpu->state.regs);
	}
	else
	{
          jprintf(DEBUG, "Resuming VM!\n");
          ret = _vmresume(&vcpu->state.regs);
	}

	if (ret < 0)
	{
		__u64 read_ret = 0;
		__u64 error = 0;
		read_ret = vmread(VM_INSTRUCTION_ERROR, &error);
		//jprintf(ERROR, "%s FAILED! wrapper_ret=%d vmread_ret=%lld error=%lld\n", vcpu->launched ? "VMRESUME" : "VMLAUNCH", ret, read_ret, error);
		return -error;
	}

	if (vmread(GUEST_RSP, &vcpu->state.regs.rsp)) {
		panic("vmread");
	}

	vcpu->launched = 1; // it's launched now
	//jprintf(ERROR, "Returned from vmlaunch. Guest code rax: %d\n", vcpu->state.regs.rax);
	//jprintf(YEET, "Returned from vmlaunch. Guest code rcx: %d\n", vcpu->state.regs.rcx);

	// handle our exit reason.
	ret = vmexit_handle(vcpu);
        //jprintf(ERROR, "VMExit returned %d\n", ret);

	// clear the dingus to avoid inter-CPU complexity
	if (_vmclear((__u64) &vcpu->vmcs_pa) < 0) panic("vmclear failed!");
	vcpu->launched = 0;
	return ret;
}

int vcpu_destroy(vcpu_t *vcpu)
{
	return true;
}

//
// END VCPU
//

//
// START VMM STUFF
//

vm_t *vm_alloc()
{
	unsigned int iter;

	vm_t *vm = kzalloc(sizeof(vm_t), GFP_KERNEL);
	//jprintf(DEBUG, "vm_t allocated at virt=%#llx phys=%#llx\n", (__u64)vm, virt_to_phys(vm));
	if (!vm)
	{
		jprintf(ERROR, "Couldn't allocate memory for vm\n");
		return NULL;
	}

	ept_init(vm);

	// allocate the CPUs
	for ( iter = 0; iter < MAX_VCPUS; iter++ ) vcpu_alloc(vm, iter);
	return vm;
}

int vm_destroy(vm_t *vm)
{
	int i;
	if (!vm)
	{
		jprintf(ERROR, "Bad VMM context in destroy\n");
		return false;
	}
	for (i = 0; i < MAX_VCPUS; i++)
	{
		jprintf(DEBUG, "Destroying vcpu: %d\n", i);
		vcpu_destroy(&vm->vcpus[i]);
	}

	kfree(vm);
	return true;
}

//
// END VMM
//

struct __vmcs_t *vmxon_virtual[MAX_PCPUS];
__u64 vmxon_physical[MAX_PCPUS];

void vmx_cpu_off(void *arg)
{
	int cpu = smp_processor_id();
	int ret;

	// Just here for clean testing. Will want to remove ofc
	ret = _vmxoff();
	if (ret < 0) panic("vmxoff failed for CPU %d\n", cpu);

	//jprintf(DEBUG, "Turned off vmx\n");
	kfree(vmxon_virtual[cpu]);
}

void vmx_cpu_on(void *arg)
{
	int cpu = smp_processor_id();
	int supported;
	int ret;

	// check for vmx support on the processor
	supported = get_vmx_support();
	if (!supported) panic("VMX Operation is not supported\n");

	// allocate memory needed for our vmxon region
	vmxon_virtual[cpu] = kzalloc(4096, GFP_KERNEL);
	if (!vmxon_virtual[cpu]) panic("Couldn't allocate space for vmxon");

	vmxon_virtual[cpu]->header.all = vmcs_revision_id();
	vmxon_physical[cpu] = __pa(vmxon_virtual[cpu]);
	//jprintf(INIT, "vmxon_region: va=%#016llx pa=%#016llx\n", (__u64)(&vmxon_virtual[cpu]), vmxon_physical[cpu]);

	// turn on vmx operation for the given processor
	ret = _vmxon((__u64) &vmxon_physical[cpu]);
	if (ret < 0) panic("VMXON Failed: %d\n", ret);

	//jprintf(INIT, "Sucessfully entered physical cpu %d into VMX operation!!\n", cpu);
}

void vmx_off()
{
	int i;
	for (i = 0; i < num_online_cpus(); i++) smp_call_function_single(i, &vmx_cpu_off, NULL, 1);

}

void vmx_on()
{
	int i;
	for (i = 0; i < num_online_cpus(); i++) smp_call_function_single(i, &vmx_cpu_on, NULL, 1);
}
