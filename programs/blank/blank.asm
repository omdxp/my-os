[BITS 32]

section .asm

global _start

_start:

_loop:
	call getkey
	push eax
	mov eax, 3 ; command 3 putchar
	int 0x80
	add esp, 4
	jmp _loop

getkey:
	mov eax, 2 ; command 2 getkey
	int 0x80
	cmp eax, 0x00
	je getkey
	ret

section .data
message: db 'Hello from program!', 0
