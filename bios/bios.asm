[org 0xfc000]
[bits 16]

entry:

  ;; determine if we're the BSP, if we're not we shouldn't be here
  mov cx, 0x1B
  rdmsr
  bt ax, 8
  jnc end

  ; load our boot image
  lea ebx, [biosImage]
  mov ax, 0xf000
  ; es will be for accessing bitmap data
  mov es, ax
  mov ax, 0xA000
  ; fs will be for writing to VGA MMIO
  mov fs, ax
  ; get the offset to the bitmap data
  mov eax, dword[es:bx+10]
  ; get width (80) == x == 32
  mov ecx, dword[es:bx+18]
  mov dword[biosBitmapWidth], ecx
  ; get height (25) == y == 64
  mov edx, dword[es:bx+22]
  mov dword[biosBitmapHeight], edx

  ;;lea ebx, [biosImage]
  ;;mov edx, dword[biosBitmapWidth]
  ;;mov dword[fs:0], edx
  ;;hlt

  ; add the bitmap offset to ebx
  add ebx, eax
  mov dword[biosImageBitmap], ebx

  ; TODO: Will want to adjust starting x and y if we want
  ; it to be displayed in the middle

  ; for (y=0; y < bmp->height; y++)
  ;   for (x=0; x < bmp->width; x++)
  ;     VGA[x+y*320] = bitmap[x+y*width]

  ; offset = 320*y + x;
  ; x (0-319)
  xor esi, esi
  ; y (0-199)
  xor edi, edi
yLoop:
  mov edx, dword[biosBitmapHeight]
  cmp edi, edx
  je vgaLoopDone
  mov esi, 0
xLoop:
  mov eax, edi
  mov ebx, dword[biosBitmapHeight]
  ; eax = y*width
  mul ebx
  ; eax = y*width + x
  add eax, esi
  ; ebx = &bitmap
  mov ebx, dword[biosImageBitmap]
  ; ebx = &bitmap[x+y*width]
  add ebx, eax
  mov al, byte[es:bx]
  mov byte[savebyte], al

  ; now get the index into VGA
  mov eax, edi
  ; constant width for our vga video mode
  mov ebx, 320
  ; eax = y*320
  mul ebx
  ; eax = y*320 + x
  add eax, esi
  mov ebx, eax
  mov al, byte[savebyte]
  mov byte[fs:bx], al

  mov ebx, dword[biosBitmapWidth]
  ; TODO: OFF BY 1?? Should I be sub'ing?
  ;sub ebx, 1
  cmp esi, ebx
  je xLoopDone
  add esi, 1
  jmp xLoop

xLoopDone:
  add edi, 1
  jmp yLoop

vgaLoopDone:
  mov ax, 0xA000
  mov fs, ax
  mov bx, 0x0
  mov word[fs:bx], 0x0f41
  ;hlt
	;;; change mode to text mode
	mov al, 3
	mov dx, 0x3b0
	out dx, ax

	lea ebx, [biosName]
	mov ax, 0xf000
	mov fs, ax

	mov ax, 0xb800
	mov es, ax

	xor si, si
	mov ax, 0x0f00
	mov al, byte [fs:bx]
biosTextWrite:
	mov word [es:si], ax
	inc si
        inc si
        inc bx
	mov al, byte [fs:bx]
	test al, al
        jne biosTextWrite

loadBootSector:
	;; read in our boot sector from disk next
	;; jump to the bootsector, the user's OS will now run

	;; set our disk sector count to 1
	mov ax, 1
	out 0x93, ax
	;; let's be pedantic and set our sector size to 512 manually
	mov ax, 512
	out 0x92, ax
	;; set destAddr to 0x7c00
	mov ax, 0x7c00
	out 0x91, ax

	;; load the boot sector into memory at 0x7c00
	out 0x94, ax

	;; check for the boot signature
	xor ax, ax
	mov al, byte [0x7dfe]
	cmp al, 0x55
	jnz bootFailed
	mov al, byte [0x7dff]
	cmp al, 0xAA
	jnz bootFailed

	;;xor ax, ax
	;;mov ax, word [es:si]
	;;cmp ax, 0xAABB
	;;je end

	;;mov dx, 0x3b0
	;;out dx, ax


	;;;; xor ax, ax ; ax = 0
	;;;; not ax ; ax = 0xFFFF
	;;mov ax, 0xA010
	;;mov es, ax
	;;mov ax, 0x1337
	;;mov si, 0x0500
	;;mov byte [es:si], al
	;;mov ax, word [es:si]
	;;cmp al, 0x44
	;;je end
	;;in al, 0x92

	jmp 0x0000:0x7c00


bootFailed:
        lea ebx, [biosFailedBoot]

	mov ax, 0x0f00
        mov al, byte [fs:bx]
biosFailedBootWrite:
	mov word [es:si], ax
	inc si
	inc si
        inc bx
	mov al, byte [fs:bx]
	test al, al
        jne biosFailedBootWrite

end:
        jmp end

align 8
biosName: db "OOOWS BIOS VERSION 0.1 (c) Proprietary works of OOOCorp...  ", 0
biosFailedBoot db "Boot device not found", 0

align 8
savebyte: resb 1
saveword: resb 2
savedword: resb 4
biosImageBitmap: resb 8
biosBitmapWidth: resb 8
biosBitmapHeight: resb 8


align 8
biosImage:
  incbin "bios/boot_image"
biosImageEnd:
biosImageSize: dw biosImageEnd-biosImage

times 0x3ff0-($-$$) db 0
bios_trampoline:
        jmp entry

;section data
