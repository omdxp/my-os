[BITS 32]

global print:function

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
