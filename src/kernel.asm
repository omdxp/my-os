[BITS 32]

global _start
global kernel_registers
extern kernel_main

; segment selectors
CODE_SEG equ 0x08
DATA_SEG equ 0x10
LONG_MODE_CODE_SEG equ 0x18
LONG_MODE_DATA_SEG equ 0x20

_start:
	mov ax, DATA_SEG
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax
	mov ss, ax

	; setup stack
	mov ebp, 0x00200000
	mov esp, ebp

	; load GDT
	lgdt [gdt_descriptor]

	; enable PAE in CR4
	mov eax, cr4
	or eax, 1 << 5
	mov cr4, eax

	; setup page tables
	mov eax, PML4_table
	mov cr3, eax

	; enable long mode in EFER MSR
	mov ecx, 0xC0000080
	rdmsr
	or eax, 1 << 8
	wrmsr

	; enable paging in CR0
	mov eax, cr0
	or eax, 1 << 31 ; set PG bit
	or eax, 1 << 0  ; set PE bit
	mov cr0, eax

	; jump to 64-bit code segment
	jmp LONG_MODE_CODE_SEG:long_mode_entry

[BITS 64]
kernel_registers:
	; set up segment registers for 64-bit mode
	mov ax, LONG_MODE_DATA_SEG
	mov ds, ax
	mov es, ax
	mov gs, ax
	mov fs, ax
	ret

long_mode_entry:
	mov ax, LONG_MODE_DATA_SEG
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax
	mov ss, ax

	; setup stack for 64-bit mode
	mov rbp, 0x00200000
	mov rsp, rbp

	; call kernel main function
	jmp kernel_main
	jmp $

; global descriptor table (GDT)
gdt:
	; null segment
	dq 0x0000000000000000

	; 32-bit code segment
	dw 0xFFFF           ; limit low
	dw 0x0000           ; base low
	db 0x00             ; base middle
	db 10011010b        ; access
	db 11001111b        ; granularity
	db 0x00             ; base high

	; 32-bit data segment
	dw 0xFFFF           ; limit low
	dw 0x0000           ; base low
	db 0x00             ; base middle
	db 10010010b        ; access
	db 11001111b        ; granularity
	db 0x00             ; base high

	; 64-bit code segment
	dw 0x0000           ; limit low
	dw 0x0000           ; base low
	db 0x00             ; base middle
	db 10011010b        ; access
	db 00100000b        ; granularity (L bit set)
	db 0x00             ; base high

	; 64-bit data segment
	dw 0x0000           ; limit low
	dw 0x0000           ; base low
	db 0x00             ; base middle
	db 10010010b        ; access
	db 00000000b        ; granularity
	db 0x00             ; base high

gdt_end:

gdt_descriptor:
	dw gdt_end - gdt - 1 ; size of GDT
	dd gdt               ; address of GDT


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
	%assign addr 0x00000000 ; start of physical memory
	%rep 512
		dq addr + 0x83
		%assign addr addr + 0x200000 ; next 2 MB
	%endrep

