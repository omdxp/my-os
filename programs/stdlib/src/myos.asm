[BITS 32]

global print:function
global getkey:function
global myos_malloc:function

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
