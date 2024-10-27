#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>

#include "assembler.h"
#include "io.h"


static size_t strim(char** s, size_t line_len)
{
	char* trimmed_ptr = *s;

	// left trim
	size_t i = 0;
	while(i < line_len)
	{
		if(!isspace(*trimmed_ptr))
			break;
		trimmed_ptr++;
	}

	size_t new_len = line_len - (trimmed_ptr - *s);

	// right trim
	while(new_len > 0)
	{
		if(!isspace(trimmed_ptr[new_len - 1]))
			break;
		new_len--;
	}

	*s = trimmed_ptr;
	return new_len;
}

static int read_value(char* valuestr, size_t n, uint64_t* val)
{
	char* terminated_str = (char*)calloc(n + 1, sizeof(char));
	memcpy(terminated_str, valuestr, n);

	char *end = 0;
	errno = 0;
	*val = strtoll(terminated_str, &end, 0);
	bool range_error = errno == ERANGE;

	if(range_error || (end - terminated_str) != n)
	{
		free(terminated_str);
		return 0;
	}

	free(terminated_str);
	return 1;
}


static operand_t parse_operand(char* operand_ptr, size_t len)
{
	size_t operand_len = strim(&operand_ptr, len);
	// printf("encountered operand (%d): \"%.*s\"\n", operand_len, operand_len, operand_ptr);

	operand_t op = { 
		.type = OP_INVALID,
		.length = 0, 
		.value = 0,
	};

	if (operand_ptr[0] == '[' && operand_ptr[operand_len - 1] == ']')
	{
		op.type = OP_PTR;
		op.length = sizeof(reg_t);
		if(!read_value(operand_ptr + 1, operand_len - 2, &op.value))
		{
			print_error("can't parse operand value!");
			op.type = OP_INVALID;
			return op;
		}
			
		return op;
	}
	
	#define REGCMP(reg_name)						\
		if(strncasecmp(operand_ptr, #reg_name, operand_len) == 0)	\
		{								\
			op.value = reg_name;					\
		}								\

	REGCMP(AX)
	REGCMP(BX)
	REGCMP(CX)
	REGCMP(DX)
	REGCMP(EX)
	REGCMP(IP)
	REGCMP(SP)
	REGCMP(BP)
	REGCMP(FLAGS)
	REGCMP(IDTR)
	REGCMP(GDTR)

	if(op.value != 0)
	{
		op.type = OP_REG;
		op.length = 1;
		return op;
	}


	uint64_t value = 0;
	if(read_value(operand_ptr, operand_len, &value))
	{
		op.type = OP_VALUE;
		op.length = sizeof(reg_t);
		op.value = value;
		return op;
	}

	for(int i = 0; i < operand_len; i++)		// no spaces in labels
	{
		if(ispunct(operand_ptr[i]) || isspace(operand_ptr[i]))
		{
			op.type = OP_INVALID;
			return op;
		}
	}

	// if can't be parsed as register, memory ptr or value, then consider it a label
	op.type = OP_LABEL;
	op.length = sizeof(reg_t);
	op.value = 0xDEADBEEF;
	op.operand_text = operand_ptr;
	op.operand_text_len = operand_len;
	
	return op;
	#undef REGCMP
}

static int assemble_instruction(const char* line, size_t line_len, uint8_t* buf, reg_t ip, size_t line_num, asm_info_t* asm_info)
{
	// printf("line: \"%.*s\"\n", line_len, line);
	char* t_line = (char*)line;
	size_t t_len = strim(&t_line, line_len);

	if(t_len == 0) return 0;

	if(t_line[t_len - 1] == ':')	// label
	{
		// printf("encountered label named %.*s at 0x%x\n", t_len - 1, t_line, ip);

		if(asm_info->labels_sz == 0)	
		{
			asm_info->labels_sz = 10 * sizeof(label_t);
			asm_info->labels = (label_t*)calloc(asm_info->labels_sz, sizeof(label_t));
		}

		if(asm_info->labels_sz < (asm_info->label_cnt + 1) * sizeof(label_t))
		{
			asm_info->labels_sz *= 2;
			asm_info->labels = realloc(asm_info->labels, asm_info->labels_sz);
		}

		asm_info->labels[asm_info->label_cnt++] = (label_t){t_line, t_len - 1, ip};
		
		return 0;
	}

	// printf("trimmed line: \"%.*s\"\n", t_len, t_line);

	size_t instruction_word_len = t_len;
	// match first word (instruction)
	for(int i = 0; i < t_len; i++)
	{
		if(isspace(t_line[i]))
		{
			instruction_word_len = i;
			break;
		}
	}

	// printf("instruction: %.*s\n", instruction_word_len, t_line);

	instruction_t inst = INST_INVALID;
	
	#define INSTCMP(inst_name)						\
		if(strncasecmp(t_line, #inst_name, instruction_word_len) == 0)	\
		{								\
			inst = inst_name;					\
		}								\

	INSTCMP(PUSH)
	INSTCMP(POP)
	INSTCMP(MOV)
	INSTCMP(ADD)
	INSTCMP(SUB)
	INSTCMP(MUL)
	INSTCMP(DIV)
	INSTCMP(DUMP)
	INSTCMP(DRAW)
	INSTCMP(IN)
	INSTCMP(OUT)
	INSTCMP(HLT)
	INSTCMP(SLEEP)
	INSTCMP(JMP)
	INSTCMP(JZ)
	INSTCMP(JNZ)
	INSTCMP(JG)
	INSTCMP(JGE)
	INSTCMP(JL)
	INSTCMP(JLE)
	INSTCMP(CALL)
	INSTCMP(RET)
	INSTCMP(AND)
	INSTCMP(OR)
	INSTCMP(XOR)
	INSTCMP(XCHG)

	#undef INSTCMP

	if (!inst)
	{
		print_error("encountered invalid instruction!");
		print_line(line, line_len, line_num);
		return -1;
	}

	buf[ip] = inst;
	ip += sizeof(instruction_t);

	// no operands
	if(instruction_word_len == t_len)
	{
		return sizeof(instruction_t);
	}

	// operands
	// printf("operands: %.*s\n", t_len - instruction_word_len, t_line + instruction_word_len);

	operand_t operands[MAX_OPERAND_CNT] = { 0 };

	size_t cur_operand_len = 0;
	size_t operand_cnt = 0;
	for(int i = instruction_word_len; i <= t_len; i++)
	{
		if(t_line[i] != ',' && i != t_len)
		{
			cur_operand_len++;
			continue;
		}

		operand_t op = parse_operand(t_line + i - cur_operand_len, cur_operand_len);
		operands[operand_cnt++] = op;


		if(!op.type)
		{
			print_error("can't parse operand!");
			print_line(line, line_len, line_num);
			return -1;
		}

		cur_operand_len = 0;
		if(operand_cnt > MAX_OPERAND_CNT) 
		{
			print_error("too many operands!");
			print_line(line, line_len, line_num);
			return -1;
		}
	}
/*
	#0
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
*/

	// check if specified operand and instruction combination is allowed
	// make a list of expected operands


	// for (int i = 0; i < operand_cnt; i++)
	// 	printf("type: %d,\tlength: %d\tvalue: 0x%x\t\n", operands[i].type, operands[i].length, operands[i].value);


	size_t operands_len_sum = 0;
	for(int i = 0; i < operand_cnt; i++)
	{
		memcpy(buf + ip, &operands[i].value, operands[i].length);
		operands_len_sum += operands[i].length;

		if(operands[i].type == OP_LABEL)
		{
			if(asm_info->fixups_sz == 0)	
			{
				asm_info->fixups_sz = 10 * sizeof(fixup_t);
				asm_info->fixups = (fixup_t*)calloc(asm_info->fixups_sz, sizeof(fixup_t));
			}

			if(asm_info->fixups_sz < (asm_info->fixup_cnt + 1) * sizeof(fixup_t))
			{
				asm_info->fixups_sz *= 2;
				asm_info->fixups = realloc(asm_info->fixups, asm_info->fixups_sz);
			}

			asm_info->fixups[asm_info->fixup_cnt++] = (fixup_t){operands[i].operand_text, operands[i].operand_text_len, (reg_t*)(buf + ip)};
		}

		ip += operands[i].length;
	}

	return sizeof(instruction_t) + operands_len_sum;

}


int assemble(asm_info_t* asm_info)
{
	if(asm_info->state != NOT_ASMED)
		return -1;

	asm_info->asm_buf_sz = INITIAL_ASM_BUF_SIZE;
	asm_info->asm_buf = (uint8_t*)calloc(asm_info->asm_buf_sz, sizeof(uint8_t));

	reg_t cur_ptr = 0;

	size_t cur_len = 0, comment_len = 0, line_num = 0;
	for(size_t i = 0; i < asm_info->src_buf_sz; i++)
	{	
		if(asm_info->src_buf[i] != '\n' && asm_info->src_buf[i] != '\r')
		{
			if((asm_info->src_buf[i] == ';' || asm_info->src_buf[i] == '#') && comment_len == 0) 
				comment_len = 1;	// starting from here every symbol in line is considered comment
			
			if(!comment_len)	cur_len++;			// Skip comments starting with ;
			else			comment_len++;

			continue;
		}

		line_num++;

		if(cur_len <= 2)
		{
			comment_len = 0;
			continue;
		}

		if(cur_ptr + MAX_INSTRUCTION_SIZE > asm_info->asm_buf_sz)
		{
			asm_info->asm_buf_sz *= 2;
			uint8_t* new_buf = realloc(asm_info->asm_buf, asm_info->asm_buf_sz);
			if(!new_buf)
			{
				print_error("memory allocation error!");
				return -1;
			}
			asm_info->asm_buf = new_buf;
		}

		int instruction_sz = assemble_instruction(asm_info->src_buf + i - cur_len - comment_len, cur_len, asm_info->asm_buf, cur_ptr, line_num, asm_info);

		if(instruction_sz < 0) 
			return -1;

		cur_ptr += instruction_sz;
		

		cur_len = 0;
		comment_len = 0;
	}

	asm_info->asm_buf_sz = cur_ptr;
	asm_info->state = ASM_PASS1;
	return cur_ptr;
}


int fixup(asm_info_t* asm_info)
{
	if(asm_info->state != ASM_PASS1)
		return -1;

	for(int i = 0; i < asm_info->fixup_cnt; i++)
	{
		bool found = false;
		for(int j = 0; j < asm_info->label_cnt; j++)
		{
			#define min(a,b) ((a < b) ? a : b)
			if(strncmp(asm_info->fixups[i].name, asm_info->labels[j].name, min(asm_info->labels[j].name_len, asm_info->fixups[i].name_len)) != 0)
				continue;
			#undef min

			*(asm_info->fixups[i].location) = asm_info->labels[j].value;
			found = true;
			break;
		}

		if(!found)
		{
			char* error_msg = 0;

			asprintf(&error_msg, "label " WHITE UNDERLINE "%.*s" RESET BOLD " is used but not defined!", asm_info->fixups[i].name_len, asm_info->fixups[i].name);
			print_error(error_msg);

			free(error_msg);
			return -1;
		}
	}

	asm_info->state = ASMED;
	return 0;
}
