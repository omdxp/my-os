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
global myos_fopen:function
global myos_fclose:function
global myos_fread:function
global myos_fseek:function
global myos_fstat:function
global myos_realloc:function
global myos_window_create:function
global myos_divert_stdout_to_window:function
global myos_process_get_window_event:function

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

; int myos_fopen(const char* filename, const char* mode)
myos_fopen:
	mov rax, 10        ; command 10 fopen
	push qword rsi     ; variable mode
	push qword rdi     ; variable filename
	int 0x80
	add rsp, 16        ; clean up stack
	ret

; void myos_fclose(size_t fd)
myos_fclose:
	mov rax, 11        ; command 11 fclose
	push qword rdi     ; variable fd
	int 0x80
	add rsp, 8         ; clean up stack
	ret

; long myos_fread(void* buffer, size_t size, size_t count, long fd)
myos_fread:
	mov rax, 12        ; command 12 fread
	push qword rcx     ; variable fd
	push qword rdx     ; variable count
	push qword rsi     ; variable size
	push qword rdi     ; variable ptr
	int 0x80
	add rsp, 32        ; clean up stack
	ret

; long myos_fseek(long fd, int offset, int whence)
myos_fseek:
	mov rax, 13        ; command 13 fseek
	push qword rdx     ; variable whence
	push qword rsi     ; variable offset
	push qword rdi     ; variable fd
	int 0x80
	add rsp, 24        ; clean up stack
	ret

; long myos_fstat(long fd, struct file_stat* filestat_out)
myos_fstat:
	mov rax, 14        ; command 14 fstat
	push qword rsi     ; variable filestat_out
	push qword rdi     ; variable fd
	int 0x80
	add rsp, 16        ; clean up stack
	ret

; void *myos_realloc(void *old_ptr, size_t new_size)
myos_realloc:
	mov rax, 15         ; command 15 realloc
	push qword rsi      ; variable new_size
	push qword rdi      ; variable old_ptr
	int 0x80
	add rsp, 16         ; clean up stack
	ret

; void *myos_window_create(const char* title, long width, long height, long flags, long id)
myos_window_create:
	mov rax, 16         ; command 16 window create
	push qword r8       ; variable id
	push qword rcx      ; variable flags
	push qword rdx      ; variable height
	push qword rsi      ; variable width
	push qword rdi      ; variable title
	int 0x80
	add rsp, 40         ; clean up stack
	ret

; void myos_divert_stdout_to_window(struct window* win)
myos_divert_stdout_to_window:
	mov rax, 17         ; command 17 divert stdout to window
	push qword rdi      ; variable win
	int 0x80
	add rsp, 8          ; clean up stack
	ret

; int myos_process_get_window_event(struct window_event* event)
myos_process_get_window_event:
	mov rax, 18         ; command 18 get window event
	push qword rdi      ; variable event
	int 0x80
	add rsp, 8          ; clean up stack
	ret