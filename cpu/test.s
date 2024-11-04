start:
	# out [num]
	# mov [num], 10
	# mov dx, [num] 
	# out [num]
	# dump
	print sus
	hlt

sus:
	db "amogus", 0

num:
	db 20
