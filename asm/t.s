start:
	print sus
	lidt idt
	lgdt gdt
	# jmp main
	mov ax, main
	push ax
	iret


idt:
	db 0xaf

	dd syscall_handler
	dd sus_handler

gdt:
	db 0xdf


main:
	print in_main
	hlt


syscall_handler:
	print sys

sus_handler:
	print sus

sus:
	db "amogus!", 0
sys:
	db "doing some important syscall schenanigans!", 0
in_main:
	db "dropped to userspace!", 0
