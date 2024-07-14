section .asm

extern int21h_handler
extern no_interrupt_handler
extern isr80h_handler

global int21h
global idt_load
global no_interrupt
global enable_interrupts
global disable_interrupts
global isr80h_wrapper

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

int21h:
	pushad
	call int21h_handler
	popad
	iret

no_interrupt:
	pushad
	call no_interrupt_handler
	popad
	iret

isr80h_wrapper:
	; interrupt frame start
	; already pushed by processor upon entry to this interrupt
	; uint32_t ip;
	; uint32_t cs;
	; uint32_t flags;
	; uint32_t sp;
	; uint32_t ss;
	; push general purpose registers to stack
	pushad

	; interrupt frame end

	; push stack pointer to point to interrupt frame
	push esp

	; eax has command to be pushed to isr80h_handler stack
	push eax
	call isr80h_handler
	mov dword[tmp_res], eax
	add esp, 8 ; (8 bytes) = esp (4 bytes) + eax (4 bytes) (4 bytes in 32 bit system)

	; restore general purpose registers for user land
	popad
	mov eax, [tmp_res]
	iretd

section .data
; stores result from isr80h_handler
tmp_res: dd 0
