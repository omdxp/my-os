[BITS 64]

section .asm

global print:function
global myos_getkey:function
global myos_malloc:function
global myos_free:function
global myos_putchar:function
global myos_process_load_start:function
global myos_system:function
global myos_process_get_arguments:function
global myos_exit:function

; void print(const char* filename)
print:
	push qword rdi ; variable filename
	mov rax, 1     ; command 1 print
	int 0x80
	add rsp, 8
	ret

; int myos_getkey()
myos_getkey:
	mov rax, 2 ; command 2 getkey
	int 0x80
	ret

; void myos_putchar(char c)
myos_putchar:
	mov rax, 3     ; command 3 putchar
	push qword rdi ; variable c
	int 0x80
	add rsp, 8
	ret

; void* myos_malloc(size_t size)
myos_malloc:
	mov rax, 4     ; command 4 malloc
	push qword rdi ; variable size
	int 0x80
	add rsp, 8
	ret

; void myos_free(void* ptr)
myos_free:
	mov rax, 5     ; command 5 free
	push qword rdi ; variable ptr
	int 0x80
	add rsp, 8
	ret

; void myos_process_load_start(const char* filename)
myos_process_load_start:
	mov rax, 6     ; command 6 process load start
	push qword rdi ; variable filename
	int 0x80
	add rsp, 8
	ret

; int myos_system(struct command_argument* arguments)
myos_system:
	mov rax, 7     ; command 7 invoke system command
	push qword rdi ; variable arguments
	int 0x80
	add rsp, 8
	ret

; void myos_process_get_arguments(struct process_arguments* arguments)
myos_process_get_arguments:
	mov rax, 8     ; command 8 process get arguments
	push qword rdi ; variable arguments
	int 0x80
	add rsp, 8
	ret

; void myos_exit()
myos_exit:
	mov rax, 9 ; command 9 exit
	int 0x80
	ret
