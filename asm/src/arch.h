#pragma once

#include <stdint.h>
#include <stdlib.h>

/*
	STRUCTURE


		opcode byte 1
	/-----------------------\       (1-4 bytes)	(1-4 bytes)
	1 2 3		4 5 6 7 8	 OPERAND1	OPERAND2
	^ ^ ^		^
	argtype		instruction num	(32 instructions) (see instruction_t)

	NONE			0

	#1
	reg			1
	value			2
	mem			3

	#2
	register <- mem		4
	register <- value	5
	register <- register	6
	mem <- register		7



	NO 
		mem <- mem (mov mem to register, then to mem to register)
		mem <- value (same)

	
	OPCODES

	Registers - 8 bit enum number
	Memory address - 32 bit
	Value - 32 bit

*/

/*
	ISA

//	REGISTERS
	
	stack pointer		SP!!!
	base pointer		BP
	instruction pointer	IP
	
	AX -	general purpose registers
	BX  
	CX
	DX
	EX

	FLAGS (ZF, CF, IF, OF)

	IDTR - interrupt descriptor table location, which is loaded by LIDT instruction
	Interrupts can be send to cpu via process signals, appropriate interrupt handlers are selected via IDT
//	


// ASM FEATURES
	db (hex/string)
	dw (hex)

	markers and jumps to those markers

//	MISC	2
	SLEEP
	HLT


//	MEM	3
	push (val/register/)
	pop register
	mov

//	CONTROL FLOW	
//	call
//	ret
//	jmp
//		jz
/		jnz

//		je, jne -> and then jump
//		jl
//		jle
//		jg
//		jge
//
//
	

//	IO

	IN
	OUT
	DRAW
	DUMP


	- MATH	6
	add
	sub
	mul
	div

	sin
	cos
		using taylor polynomials

	xor
	and
	or
	xchg
	

*/

typedef uint32_t reg_t;
#define MAX_OPERAND_CNT 2

typedef enum INSTRUCTIONS : uint8_t
{
	PUSH	= 1,
	POP	= 2,
	MOV	= 3,
	ADD	= 4,
	SUB	= 5,
	MUL	= 6,
	DIV	= 7,
	// SIN	= 8,
	// COS	= 9,
	DUMP	= 10,
	DRAW	= 11,
	IN	= 12,
	OUT	= 13,
	HLT	= 14,
	SLEEP	= 15,
	JMP	= 16,
	JZ	= 17,
	JNZ	= 18,
	JG 	= 19,
	JGE	= 20,
	JL 	= 21,
	JLE	= 22,
	CALL	= 23,
	RET	= 24,
	AND	= 25,
	OR	= 26,
	XOR	= 27,
	XCHG	= 28,
	INST_INVALID	= 0
} instruction_t;


typedef enum REGISTERS : uint8_t
{
	AX		= 1,
	BX		= 2,
	CX		= 3,
	DX		= 4,
	EX		= 5,
	IP		= 6,
	SP		= 7,
	BP		= 8,
	FLAGS		= 9,
	IDTR		= 10,
	GDTR		= 11,
	REG_INVALID	= 0
} reg_name_t;

typedef enum OPERANDS : uint8_t 
{
	OP_VALUE	= 1,
	OP_PTR		= 2,
	OP_REG		= 3,
	OP_LABEL	= 4,
	OP_INVALID	= 0,
} operand_type_t;

typedef struct operand
{
	operand_type_t type;
	size_t length;
	uint64_t value;

	char* operand_text;
	size_t operand_text_len;
} operand_t;

