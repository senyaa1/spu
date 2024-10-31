start:
	print sus
	jmp start

idt:
	db 1
	dd syscall_handler


syscall_handler:
	print sus

sus:
	db "sus!", 0
