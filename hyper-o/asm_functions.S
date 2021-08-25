.intel_syntax noprefix
#define HOST_RSP 0x00006c14

# Extern
.globl vmexit_handle
.type vmexit_handle @function

.section .text

.globl vm_entrypoint
.type vm_entrypoint @function

 ##################
 #   VMX INSTRS   #
 ##################
.globl _vmxon
.type _vmxon @function

.globl _vmxoff
.type _vmxoff @function

.globl _vmclear
.type _vmclear @function

.globl _vmptrld
.type _vmptrld @function

.globl _vmptrst
.type _vmptrst @function

.globl _vmlaunch
.type _vmlaunch @function

.globl _vmresume
.type _vmresume @function

.globl _vmwrite
.type _vmwrite @function

 ##################
 #    MISC/UTIL   #
 ##################
.globl check_vmx_error
.type  check_vmx_error@function

.globl fc_status
.type fc_status @function

.globl cr0_fixed0
.type cr0_fixed0 @function

.globl cr0_fixed1
.type cr0_fixed1 @function

.globl cr4_fixed0
.type cr4_fixed0 @function

.globl cr4_fixed1
.type cr4_fixed1 @function

.globl vmcs_revision_id
.type vmcs_revision_id @function

.globl asm_foo
.type asm_foo @function

.globl get_vmx_support
.type get_vmx_support @function

.globl _segmentlimit
.type _segmentlimit @function

.global _load_ar
.type _load_ar @function


 ##################
 #    READ REGS   #
 ##################
.globl __readmsr
.type __readmsr @function

.globl _read_cr0
.type _read_cr0 @function

.globl _read_cr3
.type _read_cr4 @function

.globl _read_cr4
.type _read_cr4 @function

.globl _read_dr7
.type _read_dr7 @function

.globl _read_rflags
.type _read_rflags @function

.globl _read_cs
.type _read_cs @function

.globl _read_ss
.type _read_ss @function

.globl _read_ds
.type _read_ds @function

.globl _read_es
.type _read_es @function

.globl _read_fs
.type _read_fs @function

.globl _read_gs
.type _read_gs @function

.globl _read_tr
.type _read_tr @function

.globl _read_ldtr
.type _read_ldtr @function

.globl _sgdt
.type _sgdt @function

.globl _sidt
.type _sidt @function

.macro SAVE_GP
        #push    rax
        push    rcx
        push    rdx
        push    rbx
        push    rbp
        push    rsi
        push    rdi
        push    r8
        push    r9
        push    r10
        push    r11
        push    r12
        push    r13
        push    r14
        push    r15
.endm

.macro RESTORE_GP
        pop     r15
        pop     r14
        pop     r13
        pop     r12
        pop     r11
        pop     r10
        pop     r9
        pop     r8
        pop     rdi
        pop     rsi
        pop     rbp
        pop     rbx
        pop     rdx
        pop     rcx
        #pop     rax
.endm

// TODO: Include this shit in a header (a bit of a pain for an ASM file. Will need to #ifdef ASM or smth)
#define OOO_X64_GPR_RAX		0
#define OOO_X64_GPR_RCX		1
#define OOO_X64_GPR_RDX		2
#define OOO_X64_GPR_RBX		3
#define OOO_X64_GPR_RSP		4
#define OOO_X64_GPR_RBP		5
#define OOO_X64_GPR_RSI		6
#define OOO_X64_GPR_RDI		7
#define OOO_X64_GPR_R8			8
#define OOO_X64_GPR_R9			9
#define OOO_X64_GPR_R10		10
#define OOO_X64_GPR_R11		11
#define OOO_X64_GPR_R12		12
#define OOO_X64_GPR_R13		13
#define OOO_X64_GPR_R14		14
#define OOO_X64_GPR_R15		15
#define OOO_X64_GPR_RIP		16
#define OOO_X64_GPR_RFLAGS		17
#define OOO_X64_NGPR			18

// RSP HANDLED SEPERATELY
.macro SAVE_GUEST_GPRS state
	mov [\state + OOO_X64_GPR_RCX*8], rcx
	mov [\state + OOO_X64_GPR_RDX*8], rdx
	mov [\state + OOO_X64_GPR_RBX*8], rbx
	mov [\state + OOO_X64_GPR_RBP*8], rbp
	mov [\state + OOO_X64_GPR_RSI*8], rsi
	mov [\state + OOO_X64_GPR_RDI*8], rdi
	mov [\state + OOO_X64_GPR_R8*8], r8
	mov [\state + OOO_X64_GPR_R9*8], r9
	mov [\state + OOO_X64_GPR_R10*8], r10
	mov [\state + OOO_X64_GPR_R11*8], r11
	mov [\state + OOO_X64_GPR_R12*8], r12
	mov [\state + OOO_X64_GPR_R13*8], r13
	mov [\state + OOO_X64_GPR_R14*8], r14
	mov [\state + OOO_X64_GPR_R15*8], r15
	mov [\state + OOO_X64_GPR_R15*8], r15
.endm

// RAX HANDLED SEPERATELY
// RSP HANDLED SEPRATELY
// RIP HANDLED SEPRATELY
.macro RESTORE_GUEST_GPRS state
	mov rcx, [\state + OOO_X64_GPR_RCX*8]
	mov rdx, [\state + OOO_X64_GPR_RDX*8]
	mov rbx, [\state + OOO_X64_GPR_RBX*8]
	mov rbp, [\state + OOO_X64_GPR_RBP*8]
	mov rsi, [\state + OOO_X64_GPR_RSI*8]
	mov rdi, [\state + OOO_X64_GPR_RDI*8]
	mov r8, [\state + OOO_X64_GPR_R8*8]
	mov r9, [\state + OOO_X64_GPR_R9*8]
	mov r10, [\state + OOO_X64_GPR_R10*8]
	mov r11, [\state + OOO_X64_GPR_R11*8]
	mov r12, [\state + OOO_X64_GPR_R12*8]
	mov r13, [\state + OOO_X64_GPR_R13*8]
	mov r14, [\state + OOO_X64_GPR_R14*8]
	mov r15, [\state + OOO_X64_GPR_R15*8]
	mov rax, [\state + OOO_X64_GPR_RAX*8]
.endm

asm_foo:
	mov rax, rdi
	ret
.size asm_foo, .-asm_foo;

get_vmx_support:
	push rbx
	mov rax, 1
	cpuid
	shr ecx, 5
	and ecx, 1
	mov eax, ecx
	pop rbx
	ret
.size get_vmx_support, .-get_vmx_support;

_segmentlimit:
	lsl rax, rdi
	ret
.size _segmentlimit, .-_segmentlimit

_load_ar:
	lar eax, edi
	jz load_ar_success
	xor rax, rax
load_ar_success:
	ret

.size _load_ar, .-_load_ar

.set MSR_IA32_VMX_BASIC, 0x480
vmcs_revision_id:
	mov rcx, MSR_IA32_VMX_BASIC
	rdmsr
	ret
.size vmcs_revision_id, .-vmcs_revision_id;

check_vmx_error:
	jc fail_cf
	jz fail_zf
	mov rax, 0
	ret
fail_cf:
	mov rax, -1
	ret
fail_zf:
	mov rax, -2
	ret
.size check_vmx_error, .-check_vmx_error

.set IA32_FEATURE_CONTROL, 0x3a
.set IA32_VMX_CR0_FIXED0, 0x486
.set IA32_VMX_CR0_FIXED1, 0x487
.set IA32_VMX_CR4_FIXED0, 0x488
.set IA32_VMX_CR4_FIXED1, 0x489

# assumes we have paging enabled, aren't in virtual 8086 mode, and aren't in SMX operation
# file:///home/jay/Downloads/335592-sdm-vol-4.pdf
# https://www.intel.com/content/dam/www/public/us/en/documents/manuals/64-ia-32-architectures-software-developer-vol-3d-part-4-manual.pdf
_vmxon:
	# first set cr4.vmxe (Virtual Machine Extensions Enable)
	mov rcx, cr4
	# set bit 13
	bts rcx, 13
	mov cr4, rcx
	# next we want to read IA32_FEATURE_CONTROL to check the lock bit (bit 0) as well as bit 2 (enables VMXON outside of SMX)
	mov rcx, IA32_FEATURE_CONTROL
	rdmsr
	# todo: use bt jc instead
	and eax, 0x5
	xor eax, 0x5
	jz good
	# tdo: just set the bits here instead (can we?)
	mov rax, -2
	ret
good:
	# alright now we gotta set bits in cr0 and cr4 according to
	# IA32_VMX_CR0_FIXED0 IA32_VMX_CR0_FIXED1
	# and
	# IA32_VMX_CR4_FIXED0 IA32_VMX_CR4_FIXED1
	mov r10, cr0
	mov rcx, IA32_VMX_CR0_FIXED1
	rdmsr
	and r10d, eax
	mov rcx, IA32_VMX_CR0_FIXED0
	rdmsr
	or r10d, eax
	mov cr0, r10
	# now CR4
	mov r10, cr4
	mov rcx, IA32_VMX_CR4_FIXED1
	rdmsr
	and r10d, eax
	mov rcx, IA32_VMX_CR4_FIXED0
	rdmsr
	or r10d, eax
	mov cr4, r10
	# Clear carry flag before so we can error check after
	#clc
	# we're finally ready to call vmxon
	vmxon [rdi]
	jmp check_vmx_error
.size _vmxon, .-_vmxon;

_vmxoff:
	vmxoff
	jmp check_vmx_error
.size _vmxoff, .-_vmxoff;

_vmclear:
	vmclear [rdi]
	jmp check_vmx_error
.size _vmclear, .-_vmclear;

_vmptrld:
	vmptrld [rdi]
	jmp check_vmx_error
.size _vmptrld, .-_vmptrld

_vmptrst:
	vmptrst [rdi]
	jmp check_vmx_error
.size _vmptrst, .-_vmptrst

vm_entrypoint:
	# save guest rax
	push rax
	mov rax, [rsp+8]
	SAVE_GUEST_GPRS(rax)
	# save guest rax manually
	pop rdi
	mov [rax + OOO_X64_GPR_RAX*8], rdi
	# restore host rax
	pop rax
	RESTORE_GP
	xor rax, rax
	#vmxoff
	ret
.size vm_entrypoint, .-vm_entrypoint

_vmlaunch:
	# save host GPRs
	SAVE_GP
	# save host rax (guest state ptr)
	mov rax, rdi
	push rax
	# save RSP
	mov rbx, HOST_RSP
	mov rsi, rsp
	vmwrite rbx, rsi
	mov rax, rdi
	RESTORE_GUEST_GPRS(rax)
	vmlaunch
	# failure case. probably don't care about saving guest regs if we failed to launch
	pop rax
	RESTORE_GP
	xor rax, rax
	jmp check_vmx_error
.size _vmlaunch, .-_vmlaunch

_vmresume:
  # save host GPRs
  SAVE_GP
  # save host rax (guest state ptr)
  mov rax, rdi
  push rax
  # save RSP
  mov rbx, HOST_RSP
  mov rsi, rsp
  vmwrite rbx, rsi
  mov rax, rdi
  RESTORE_GUEST_GPRS(rax)
  vmresume
  # failure case. probably don't care about saving guest regs if we failed to launch
  pop rax
  RESTORE_GP
  jmp check_vmx_error
.size _vmresume, .-_vmresume


_vmwrite:
	vmwrite rdi, [rsi]
	jmp check_vmx_error
.size _vmwrite, .-_vmwrite

_vmread:
	vmread [rdi], rsi
	jmp check_vmx_error
.size _vmread, .-_vmread

cr0_fixed0:
	mov rcx, IA32_VMX_CR0_FIXED0
	rdmsr
	ret
.size cr0_fixed0, .-cr0_fixed0

cr0_fixed1:
	mov rcx, IA32_VMX_CR0_FIXED1
	rdmsr
	ret
.size cr0_fixed1, .-cr0_fixed1

__readmsr:
	mov rcx, rdi
	rdmsr
	shl rax, 32
	shrd rax, rdx, 32
	ret
.size __readmsr, .-__readmsr;

cr4_fixed0:
	mov rcx, IA32_VMX_CR4_FIXED0
	rdmsr
	ret
.size cr4_fixed0, .-cr4_fixed0

cr4_fixed1:
	mov rcx, IA32_VMX_CR4_FIXED1
	rdmsr
	ret
.size cr4_fixed1, .-cr4_fixed1

fc_status:
	mov rcx, 0x3a
	rdmsr
	ret
.size fc_status, .-fc_status;

_read_cr0:
	mov rax, cr0
	ret
.size _read_cr0, .-_read_cr0

_read_cr3:
	mov rax, cr3
	ret
.size _read_cr4, .-_read_cr4

_read_cr4:
	mov rax, cr4
	ret
.size _read_cr4, .-_read_cr4

_read_dr7:
	mov rax, dr7
	ret
.size _read_dr7, .-_read_dr7

_read_es:
	mov ax, es
	ret
.size _read_es, .-_read_es

_read_rflags:
	pushfq
	pop rax
	ret
.size _read_rflags, .-_read_rflags

_read_cs:
	mov ax, cs
	ret
.size _read_cs, .-_read_cs

_read_ss:
	mov ax, ss
	ret
.size _read_ss, .-_read_ss

_read_ds:
	mov ax, ds
	ret
.size _read_ds, .-_read_ds

_read_fs:
	mov ax, fs
	ret
.size _read_fs, .-_read_fs

_read_gs:
	mov ax, gs
	ret
.size _read_gs, .-_read_gs

_read_tr:
	str ax
	ret
.size _read_tr, .-_read_tr

_read_ldtr:
	sldt ax
	ret
.size _read_ldtr, .-_read_ldtr

_sgdt:
	sgdt [rdi]
	ret
.size _sgdt, .-_sgdt

_sidt:
	sidt [rdi]
	ret
.size _sidt, .-_sidt

.att_syntax noprefix
