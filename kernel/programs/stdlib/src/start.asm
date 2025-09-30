[BITS 64]

global _start
extern c_start
extern myos_exit

section .asm

_start:
	call c_start
	call myos_exit
	ret
