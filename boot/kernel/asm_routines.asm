.section .text
.global _rdmsr
.type _rdmsr, @function
_rdmsr:
        mov 0x4(%esp), %ecx
        rdmsr
        ret

.section .text
.global _out
.type _out, @function
_out:
        movw 0x8(%esp), %dx
        movw 0x4(%esp), %ax
        out %ax, (%dx)
        ret

.section .text
.global _outd
.type _outd, @function
_outd:
        movw 0x8(%esp), %dx
        mov 0x4(%esp), %eax
        out %eax, (%dx)
        ret

.section .text
.global _outb
.type _outb, @function
_outb:
        movw 0x8(%esp), %dx
        movb 0x4(%esp), %al
        out %al, (%dx)
        ret

.section .text
.global _ind
.type _ind, @function
_ind:
        movw 0x4(%esp), %dx
        in (%dx), %eax
        ret

.section .text
.global _inb
.type _inb, @function
_inb:
        movw 0x4(%esp), %dx
        in (%dx), %al
        ret

.section .text
.global _inw
.type _inw, @function
_inw:
        movw 0x4(%esp), %dx
        in (%dx), %ax
        ret
