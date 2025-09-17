[BITS 64]

section .asm

global paging_load_directory
global paging_invalidate_tlb_entry

; void paging_load_directory(uintptr_t *directory);
paging_load_directory:
	mov rax, rdi ; load first argument (directory) into rax
	mov cr3, rax ; load page directory base register
	ret

; void paging_invalidate_tlb_entry(uintptr_t addr);
paging_invalidate_tlb_entry:
	invlpg [rdi] ; invalidate TLB entry for the given address
	ret
