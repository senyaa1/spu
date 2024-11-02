start:
	lidt idt

	int 1
	int 2

	hlt

idt:
	db 2
	dd syscall1, syscall2


syscall1:
	print sus
	iret

syscall2:
	print mog
	iret

sus:
	db "1!", 0


mog:
	db "2!", 0
