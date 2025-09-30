[BITS 64]

section .asm

global restore_general_purpose_registers
global task_return
global user_registers

task_return:
	push qword [rdi+88] ; ss
	push qword [rdi+80] ; rsp
	mov rax, [rdi+72]   ; rflags
	or rax, 0x200       ; set interrupt flag
	push rax

	push qword 0x2b     ; user code segment
	push qword [rdi+56] ; rip
	call restore_general_purpose_registers

	iretq               ; return to user land

restore_general_purpose_registers:
	mov rsi, [rdi+8] ; rax
	mov rbp, [rdi+16] ; rbx
	mov rbx, [rdi+24] ; rcx
	mov rdx, [rdi+32] ; rdx
	mov rcx, [rdi+40] ; rsi
	mov rax, [rdi+48] ; rdi
	mov rdi, [rdi]    ; rdi
	ret

user_registers:
	mov ax, 0x2b ; user data segment | privilege level 3
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax
	ret
