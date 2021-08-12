bits 64

global _start

_start:
%ifdef ENCLAVE
    ; CFI tag
.cfi db "BBYOOOGX"
%else
    ; Set up data pointer
    mov rdx, rsp
    add rdx, 0x40
%endif

    ; Set the fail-safe return address
    lea r13, [rel .out]

    ; Touch some bad memory
    mov rax, 1
    mov rbx, qword [rax]

.out:
    mov rax, -1
    ret

.err:
    int3
    hlt
