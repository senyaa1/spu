start:
	# out [num]
	# mov [num], 10
	# mov dx, [num] 
	# out [num]
	# dump
	mov [sus], 0x41
	print sus
	hlt

sus:
	db "sussy", 10, 0

num:
	db 20
