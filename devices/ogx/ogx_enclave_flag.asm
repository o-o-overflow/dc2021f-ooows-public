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
    mov r14, rdx
    lea r13, [rel .out]

.loop:
    ; Sleep
    lea rdi, [r14 + 0x1000]         ; ts
    mov qword [rdi], 10             ; ts.tv_sec
    mov qword [rdi + 8], 0          ; ts.tv_nsec
    lea rsi, qword [r14 + 0x1020]   ; rem
    mov qword [rsi], 0              ; rem.tv_sec
    mov qword [rsi + 8], 0          ; rem.tv_nsec
    mov rax, 35                     ; SYS_nanosleep
    syscall

    ; Check for error
    ; cmp rax, 0
    ; jnz .out

    ; Check if exit flag is set
    cmp qword [r14 + 0x1020], 0
    jnz .out

    jmp .loop

.out:
    mov rax, -1
    ret
