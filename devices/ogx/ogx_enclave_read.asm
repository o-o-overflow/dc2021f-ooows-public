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

    ; Just indicate that we want to read out 64 bytes
    mov rax, 64
    ret
