section .asm

extern int21h_handler
extern no_interrupt_handler
extern isr80h_handler
extern interrupt_handler

global idt_load
global no_interrupt
global enable_interrupts
global disable_interrupts
global isr80h_wrapper
global interrupt_pointer_table

temp_rsp_storage: dq 0x00
%macro pushad_macro 0
	mov qword [temp_rsp_storage], rsp
	push rax
	push rcx
	push rdx
	push rbx
	push qword [temp_rsp_storage]
	push rbp
	push rsi
	push rdi
%endmacro

%macro popad_macro 0
	pop rdi
	pop rsi
	pop rbp
	pop qword [temp_rsp_storage]
	pop rbx
	pop rdx
	pop rcx
	pop rax
	mov rsp, qword [temp_rsp_storage]
%endmacro

enable_interrupts:
	sti
	ret

disable_interrupts:
	cli
	ret

idt_load:
    push ebp
    mov ebp, esp

    mov ebx, [ebp+8]
    lidt [ebx]
    
    pop ebp
    ret

no_interrupt:
	pushad_macro
	call no_interrupt_handler
	popad_macro
	iret

%macro interrupt 1
	global int%1
	int%1:
		; interrupt frame start
		; already pushed by processor upon entry to this interrupt
		; uint32_t ip;
		; uint32_t cs;
		; uint32_t flags;
		; uint32_t sp;
		; uint32_t ss;
		; push general purpose registers to stack
		pushad_macro
		; interrupt frame end
		push esp
		push dword %1
		call interrupt_handler
		add esp, 8
		popad_macro
		iret
%endmacro

%assign i 0
%rep 512
	interrupt i
%assign i i+1
%endrep

isr80h_wrapper:
	; interrupt frame start
	; already pushed by processor upon entry to this interrupt
	; uint32_t ip;
	; uint32_t cs;
	; uint32_t flags;
	; uint32_t sp;
	; uint32_t ss;
	; push general purpose registers to stack
	pushad_macro

	; interrupt frame end

	; push stack pointer to point to interrupt frame
	push esp

	; eax has command to be pushed to isr80h_handler stack
	push eax
	call isr80h_handler
	mov dword[tmp_res], eax
	add esp, 8 ; (8 bytes) = esp (4 bytes) + eax (4 bytes) (4 bytes in 32 bit system)

	; restore general purpose registers for user land
	popad_macro
	mov eax, [tmp_res]
	iretd

section .data
; stores result from isr80h_handler
tmp_res: dd 0

%macro interrupt_array_entry 1
	dd int%1
%endmacro

interrupt_pointer_table:
%assign i 0
%rep 512
	interrupt_array_entry i
%assign i i+1
%endrep
