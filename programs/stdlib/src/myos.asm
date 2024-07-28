[BITS 32]

section .asm

global print:function
global getkey:function
global myos_malloc:function
global myos_free:function
global myos_putchar:function

; void print(const char* filename)
print:
	push ebp
	mov ebp, esp
	push dword[ebp+8]
	mov eax, 1 ; command 1 print
	int 0x80
	add esp, 4
	pop ebp
	ret

; int getkey()
getkey:
	push ebp
	mov ebp, esp
	mov eax, 2 ; command 2 getkey
	int 0x80
	pop ebp
	ret

; void myos_putchar(char c)
myos_putchar:
	push ebp
	mov ebp, esp
	mov eax, 3 ; command 3 putchar
	push dword[ebp+8] ; variable c
	int 0x80
	add esp, 4
	pop ebp
	ret

; void* myos_malloc(size_t size)
myos_malloc:
	push ebp
	mov ebp, esp
	mov eax, 4 ; command 4 malloc
	push dword[ebp+8] ; variable size
	int 0x80
	add esp, 4
	pop ebp
	ret

; void myos_free(void* ptr)
myos_free:
	push ebp
	mov ebp, esp
	mov eax, 5 ; command 5 free
	push dword[ebp+8]
	int 0x80
	add esp, 4
	pop ebp
	ret
