[BITS 64]

global _start
global kernel_registers
global gdt
extern kernel_main

; segment selectors
CODE_SEG equ 0x08
DATA_SEG equ 0x10
LONG_MODE_CODE_SEG equ 0x18
LONG_MODE_DATA_SEG equ 0x20

_start:
	cli					 ; clear interrupts
	jmp long_mode_entry

kernel_registers:
	; set up segment registers for 64-bit mode
	mov ax, LONG_MODE_DATA_SEG
	mov ds, ax
	mov es, ax
	mov gs, ax
	mov fs, ax
	ret

long_mode_entry:
	; setup page tables for long mode
	mov rax, PML4_table ; load address of PML4 table
	mov cr3, rax        ; load page table base address into CR3

	; load GDT
	lgdt [gdt_descriptor]

	mov ax, LONG_MODE_DATA_SEG
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax
	mov ss, ax

	; setup stack for 64-bit mode
	mov rsp, 0x00200000
	mov rbp, rsp

	; code will jump to long_mode_new_gdt_complete
	; to switch code selector
	push qword 0x18                       ; code segment selector
	push qword long_mode_new_gdt_complete ; offset to jump to
	retfq                                 ; far return to 64-bit mode

long_mode_new_gdt_complete:
	; remap the PIC
	mov al, 0x11 ; start initialization in cascade mode
	out 0x20, al ; send ICW1 to master PIC
	
	mov al, 0x20 ; remap master PIC to interrupt vector 0x20
	out 0x21, al ; send ICW2 to master PIC

	mov al, 0x04 ; tell slave PIC its cascade identity
	out 0x21, al ; send ICW3 to master PIC

	mov al, 0x01 ; 8086 mode
	out 0x21, al ; send ICW4 to master PIC

	; remapping slave PIC
	mov al, 0x11 ; start initialization in cascade mode
	out 0xA0, al ; send ICW1 to slave PIC

	mov al, 0x28 ; remap slave PIC to interrupt vector 0x28
	out 0xA1, al ; send ICW2 to slave PIC

	mov al, 0x02 ; tell slave PIC its cascade identity
	out 0xA1, al ; send ICW3 to slave PIC

	mov al, 0x01 ; 8086 mode
	out 0xA1, al ; send ICW4 to slave PIC

	; unmask only necessary IRQs
	mov al, 0xFB ; 11111011 - unmask IRQ0 (timer) and IRQ1 (keyboard)
	out 0x21, al ; mask for master PIC

	; mask all IRQs on slave PIC
	mov al, 0xFF ; 11111111 - mask all IRQs
	out 0xA1, al ; mask for slave PIC

	mov al, 0x20 ; send End of Interrupt (EOI) to master PIC
	out 0x20, al ; send EOI to master PIC
	out 0xA0, al ; send EOI to slave PIC

	; call kernel main function
	jmp kernel_main
	jmp $

; global descriptor table (GDT)
align 8
gdt:
	; null segment
	dq 0x0000000000000000

	; 32-bit code segment
	dw 0xFFFF           ; limit low
	dw 0                ; base low
	db 0                ; base middle
	db 0x9a             ; access
	db 11001111b        ; granularity
	db 0                ; base high

	; 32-bit data segment
	dw 0xFFFF           ; limit low
	dw 0                ; base low
	db 0                ; base middle
	db 0x92             ; access
	db 11001111b        ; granularity
	db 0                ; base high

	; 64-bit code segment
	dw 0x0000           ; limit low
	dw 0x0000           ; base low
	db 0x00             ; base middle
	db 0x9a             ; access
	db 0x20             ; granularity (L bit set)
	db 0x00             ; base high

	; 64-bit data segment
	dw 0x0000           ; limit low
	dw 0x0000           ; base low
	db 0x00             ; base middle
	db 0x92             ; access
	db 0x00             ; granularity
	db 0x00             ; base high

	; 64-bit user code segment
	dw 0x0000           ; limit low
	dw 0x0000           ; base low
	db 0x00             ; base middle
	db 0xfa             ; access (DPL=3)
	db 0x20             ; granularity (L bit set)
	db 0x00             ; base high

	; 64-bit user data segment
	dw 0x0000           ; limit low
	dw 0x0000           ; base low
	db 0x00             ; base middle
	db 0xf2             ; access (DPL=3)
	db 0x00             ; granularity
	db 0x00             ; base high

	; TSS segment (will be filled in later)
	; set to zero as it will be initialized in kernel code
	dw 0x0000           ; limit low
	dw 0x0000           ; base low
	db 0x00             ; base middle
	db 0x00 		    ; access
	db 0x00             ; granularity
	db 0x00             ; base high
	
	; TSS base address (high 32 bits)
	dw 0x0000           ; limit low
	dw 0x0000           ; base low
	db 0x00             ; base middle
	db 0x00 		    ; access
	db 0x00             ; granularity
	db 0x00             ; base high

gdt_end:

gdt_descriptor:
	dw gdt_end - gdt - 1 ; size of GDT
	dq gdt               ; address of GDT


; page table definitions
align 4096
PML4_table:
	dq PDPT_table + 0x03 ; present, read/write
	times 511 dq 0       	; zero rest of table

align 4096
PDPT_table:
	dq PD_table + 0x03 ; present, read/write
	times 511 dq 0        ; zero rest of table

align 4096
PD_table:
	%assign addr 0x0000000 ; start of physical memory
	%rep 512
		dq addr + 0x83
		%assign addr addr + 0x200000 ; next 2 MB
	%endrep

