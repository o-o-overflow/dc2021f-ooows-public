bits 64

global ogx_trampoline
global ogx_trampoline_size

; Set up enclave execution.
;
; @param rdi Label.
; @param rsi Code address.
; @param rdx Data address.
; @param rcx Stack address.
ogx_trampoline:
    ; Set up enclave privileges
    mov r8, qword [rsp]
    push rbp
    mov rax, 3
    mov r10, rdx
    mov r11, rcx
    mov rcx, rdi
    shl rcx, 1
    shl rax, cl
    not rax
    xor rdx, rdx
    xor rcx, rcx
    wrpkru

    ; Set up the stack
    mov rdx, r10
    mov rcx, r11
    add rcx, 0x1000
    mov qword [rcx], rsp
    lea rsp, [rcx]
    mov rbp, rsp

    ; Invoke the enclave (protected with CFI check)
    mov r10, 0x58474f4f4f594242
    cmp qword [rsi], r10
    je .call
    ud1
.call:
    add rsi, 8
    call rsi
    mov r11, rax

    ; Restore the stack
    ; TODO: We should check that the stack pointer resides *outside* enclave memory, otherwise the contents are
    ;       potentially untrustworthy.  However, the return-edge check would still need to be subverted, and the rest
    ;       of the program is protected with clang CFI.  So maybe it doesn't matter.
    mov rcx, [rsp]
    mov rsp, rcx

    ; Restore privileges
    xor rdx, rdx
    xor rcx, rcx
    xor rax, rax
    wrpkru

    ; Returning anywhere after here is useless since non-enclave permissions won't have been restored

    ; Check the return edge
    pop rbp
    pop rdx
    mov rcx, 0x59424258474f4f4f
    cmp qword [rdx], rcx
    je .out
    ud1

    ; Exit the enclave
.out:
    add rdx, 8
    push rdx
    mov rax, r11
    ret

ogx_trampoline_size dq $-ogx_trampoline
