[BITS 64]

extern read_tsc
extern tsc_frequency
global tsc_microseconds:function

; 128 bit types are not supported by default in C

; TIME_MICROSECONDS tsc_microseconds(void);
tsc_microseconds:
	push rbp
	mov rbp, rsp
	sub rsp, 32
	call tsc_frequency
	; store frequency in rbp-8
	mov qword [rbp-8], rax
	; store current tsc in rbp-16
	call read_tsc
	mov qword [rbp-16], rax

	; calculate microseconds = (tsc * 1_000_000) / frequency
	mov rax, qword [rbp-16] ; tsc
	mov rcx, 1000000 	    ; 1_000_000
	mul rcx                 ; rdx:rax = tsc * 1_000_000 (rdx for overflow)

	mov rcx, qword [rbp-8]  ; frequency
	xor rdx, rdx            ; clear rdx for division
	div rcx                 ; rax = (tsc * 1_000_000) / frequency

	; restore stack and return
	add rsp, 32
	pop rbp
	ret

