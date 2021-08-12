[org 0x7c00]
[bits 16]

  ; creating ap trampoline
  ; TODO: Do this is a nicer way
  mov word[0x80000], 0xea007c00 ; jmp 0x0:0x7c00
  mov word[0x80004], 0x00000000 ; jmp 0x0:0x7c00 (cont)
  mov byte[0x80010], 0xf4 ; hlt (shouldn't hit)

  ; disable interrupts
  cli
  cld
  xor ax, ax
  ; clear ds because it's used to load gdt
  mov ds, ax
  ; load our gdt_desc
  lgdt [gdt_desc]

  ; now we're ready to enter protected mode
  mov eax, cr0
  ; set bit 1 of cr0 to enable protected mode
  or eax, 0x1
  mov cr0, eax

  ; Need to clear the instr pipeline since we're no longer 16bit instructions
  ; To do this we need to execute a far jump -- to our new code segment
  ; our code segment is the first segment after our NULL segment (so 0x8)
  jmp 0x8:clear_pipe


[bits 32]
clear_pipe:
  ; now set up our segment regs
  mov ax, 0x10
  mov ds, ax
  mov es, ax
  mov ss, ax

  ;; 500 - 9FFFF	RAM	Free memory, 0x7C00 used for boot sector
  mov esp, 0x90000

  xor eax, eax
  mov ax, [kernel_loaded]
  test ax, ax
  jnz kjump

  ; only bsp should be executing here

  ; let's load our kernel and jump to it
  ; set our disk sector to 1
  mov ax, 1
  out 0x90, ax
	;; set our num sectors to the value filled in for us by disk_creator.py
  xor ax, ax
	mov al, byte[num_sectors]
	out 0x93, ax
  ; set destAddr to 0x100000
  mov eax, 0x100000
  out 0x91, eax

  ; load the kernel entry into memory at 0x100000
  out 0x94, ax
  mov byte[kernel_loaded], 0x1
kjump:
  jmp 0x8:0x100000
  hlt


end:
  hlt

;; 1st Double word:
;; Bits	Function	Description
;; 0-15	Limit 0:15	First 16 bits in the segment limiter
;; 16-31	Base 0:15	First 16 bits in the base address
;; 
;; 2nd Double word:
;; Bits	Function	Description
;; 0-7 Bits 16-23 in the base address
;; 8-11 Type bits
;; 12 Desciptor Type (0=system; 1=code or data)
;; 13-14	Descriptor Privilege Level	0 = Highest privilege (OS), 3 = Lowest privilege (User applications)
;; 15	Present flag	Set to 1 if segment is present
;; 16-19	Limit 16:19	Bits 16-19 in the segment limiter
;; 20-22	Attributes	Different attributes, depending on the segment type
;; 23	Granularity	Used together with the limiter, to determine the size of the segment
;; 24-31	Base 24:31	The last 24-31 bits in the base address

;; G | Size | Reserved | Avail
;; G | D/B  |    L     | Avail
align 8
gdt:
; null segment
gdt_null:
  dd 0
  dd 0
; code segment
gdt_code:
  dw 0FFFFh ;; First 16 bits in the segment limiter
  dw 0 ;; First 16 bits in the base address
  db 0 ;; Bits 16-23 in the base address
  ;; 8-15
  ;; 8-11: type bits --> 1010 --> code | conforming | readable | accessed == readable code segment, nonconforming
  ;; 12: Descriptor Type (S flag) --> 1 --> code or data
  ;; 13-14: DPL --> 00
  ;; 15 present flag --> 1
  db 10011010b
  ;; 16-19: Bits 16-19 in the limiter --> 1111
  ;; 20: available --> 0 (ignored)
  ;; 21: L (64-bit code segment) --> 0
  ;; 22 D/B = Default operation size (0=16bit ; 1=32bit) --> 1
  ;; 23: Granularity (bytes or 4K chunks) -->  1 (4k chunks)
  db 11001111b
  ;; 24-31: Base 24:31
  db 0

; data segment
gdt_data:
  dw 0FFFFh
  dw 0
  db 0
  ;; data r/w
  db 10010010b
  db 11001111b
  db 0

gdt_end:
gdt_desc:
  dw gdt_end - gdt
  dd gdt

kernel_loaded:
  db 0

times 497-($-$$) db 0
num_sectors:
  db 0x10 ; num_sectors (filled out by disk_creator.py)
times 510-($-$$) db 0
   dw 0xAA55

