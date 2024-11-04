#pragma once

#include <stdint.h>
#include <stdlib.h>

/*
	STRUCTURE


		opcode byte 1
	/-----------------------\			
	1 2 		3 4 5 6 7 8	
	operandcnt	instruction num	type    length	



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

	FLAGS (ZF, CF, SF, OF, IF)

	IDTR - interrupt descriptor table location, which is loaded by LIDT instruction
	Interrupts can be send to cpu via process signals, appropriate interrupt handlers are selected via IDT
//	


// ASM FEATURES
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
	sin
	cos
		using taylor polynomials
*/

typedef enum EXPRESSIONS : uint8_t
{
	DB		= 1,
	DW		= 2,
	DD		= 3,
	DQ		= 4,
	UNDEFINED	= 0
} expression_t;

typedef uint32_t reg_t;
#define MAX_OPERAND_CNT 4

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
	INPUT	= 12,
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
	CMP	= 29,
	INT	= 30,
	PRINT	= 31,
	LIDT	= 32,
	LGDT	= 33,
	IRET	= 34,

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
	FLAGS		= 8,
	IDTR		= 9,
	GDTR		= 10,
	REG_INVALID	= 0
} reg_name_t;

#define OPERAND_PTR_BIT  (1 << 2)

typedef enum OPERANDS : uint8_t 
{
	OP_VALUE		= 0b1,
	OP_REG			= 0b10,

	OP_REGPTR		= 0b10 | OPERAND_PTR_BIT,
	OP_VALUEPTR		= 0b1 | OPERAND_PTR_BIT,

	OP_LABEL		= 32,
	OP_STR			= 33,

	OP_INVALID		= 0,
} operand_type_t;

typedef struct operand
{
	operand_type_t type;
	size_t length;
	uint64_t value;
	reg_t actual_value;

	char* operand_text;
	size_t operand_text_len;
} operand_t;

