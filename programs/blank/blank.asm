[BITS 32]

section .asm

global _start

_start:

	push message
	mov eax, 1 ; command 1 print
	int 0x80
	add esp, 4 ; (message (4 bytes))
	
	jmp $

section .data
message: db 'Hello from program!', 0
