#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/ioctl.h>

#include <linux/fb.h>

#include "arch.h"
#include "fs.h"
#include "stack.h"

/*
	read binary
	initialize registers
	initialize stack and call stack
	(maybe start from main label)
	implement all instructions
	draw instruction draws on framebuffer


	*/

typedef struct cpu
{
	reg_t regs[16];
	stack_t data_stack;
	stack_t call_stack;
	uint8_t* mem;
} cpu_t;

typedef struct framebuf 
{
	int fd;
	void* addr;
} framebuf_t;

#define FB_WIDTH 400
#define FB_HEIGHT 800


int initialize_framebuffer(framebuf_t* fb)
{
	int fbfd = open("/dev/fb0", O_RDWR);

	if (fbfd == -1) 
		return -1;

	struct fb_var_screeninfo vinfo;
	ioctl(fbfd, FBIOGET_VSCREENINFO, &vinfo);

	fb->addr = mmap(NULL, FB_WIDTH * FB_HEIGHT * sizeof(uint32_t), PROT_READ | PROT_WRITE, MAP_SHARED, fbfd, 0);

	if(fb->addr == MAP_FAILED)
		return -1;

	for (int y=100; y<200; y++) {
		for (int x=100; x<200; x++) {
			*(unsigned int*)fb->addr = 0x00FF0000; // Red
			fb->addr += 4;
		}
	}

	return 0;
}

void destroy_framebuffer(framebuf_t* fb)
{
	if(fb->addr == 0) 
		return;
	munmap(fb->addr, FB_WIDTH * FB_HEIGHT * sizeof(uint32_t));
	close(fb->fd);
}

int execute(cpu_t* cpu, framebuf_t* fb)
{
	while(1)
	{
		uint8_t opcode = cpu->mem[cpu->regs[IP]];
		instruction_t inst = opcode & 0b00011111;
		cpu->regs[IP] += sizeof(instruction_t);

		operand_t operands[MAX_OPERAND_CNT] = { 0 };
		uint8_t operand_cnt = opcode >> 5;
		
		for(int i = 0; i < operand_cnt; i++)
		{
			operands[i].type = cpu->regs[IP] >> 4;
			operands[i].length = cpu->regs[IP] & 0b1111;
			cpu->regs[IP]++;

			switch(operands[i].type)
			{
				case OP_VALUE:
					operands[i].value = ((reg_t*)cpu->mem)[cpu->regs[IP]];
					operands[i].actual_value = operands[i].value;
					break;
				case OP_REG:
					operands[i].value = ((reg_name_t*)cpu->mem)[cpu->regs[IP]];
					operands[i].actual_value = cpu->regs[operands[i].value];
					break;
				case OP_PTR:
					operands[i].value = ((reg_t*)cpu->mem)[cpu->regs[IP]];
					operands[i].actual_value = cpu->mem[operands[i].value];
					break;
				default:
					break;
			}
			
			cpu->regs[IP] += operands[i].length;
		}
		
		
		reg_t a = 0, b = 0, res = 0;


		#define BINARY_OP(INST, OP)			\
		case INST:					\
			stack_pop(&cpu->data_stack, &a);	\
			stack_pop(&cpu->data_stack, &b);	\
			OP;					\
			stack_push(&cpu->data_stack, &res);	\
			break;					\


		switch(inst)
		{
			case PUSH:
				stack_push(&cpu->data_stack, &operands[0].actual_value);
				break;
			case POP:
				stack_pop(&cpu->data_stack, &cpu->regs[operands[0].value]);
				break;
			case MOV:
				// if(operand_cnt != 2)
				// {
				// 	fprintf(stderr, "mov should have 2 operands!\n");
				// 	return -1;
				// }
				switch(operands[0].type)
				{
					case OP_PTR:
						cpu->mem[operands[1].value] = operands[0].actual_value;
						break;
					case OP_VALUE:
						operands[1].value = operands[0].actual_value;
						break;
					case OP_REG:
						cpu->regs[operands[1].value] = operands[0].actual_value;
						break;
					default:
						break;
				}
				break;
			case XCHG:
				a = cpu->regs[operands[0].value];
				cpu->regs[operands[0].value] = cpu->regs[operands[1].value];
				cpu->regs[operands[1].value] = a;
				break;
			case SLEEP:
				sleep(operands[0].actual_value);
				break;

			BINARY_OP(ADD, res = a + b);
			BINARY_OP(SUB, res = b - a);
			BINARY_OP(MUL, res = a * b);
			BINARY_OP(DIV, res = b / a);

			BINARY_OP(AND, res = a & b);
			BINARY_OP(OR, res = a | b);
			BINARY_OP(XOR, res = a ^ b);

			case CALL:
				stack_push(&cpu->call_stack, &cpu->regs[IP]);
				cpu->regs[IP] = operands[0].actual_value;
				break;
			case RET:
				stack_pop(&cpu->call_stack, &cpu->regs[IP]);
				break;
			case DUMP:
				printf("processor dump!\n");
				break;
			case CMP:
				break;
			case DRAW:
				if(fb->addr == 0)
				{
					if(initialize_framebuffer(fb) < 0)
					{
						fprintf(stderr, "can't initialize fb!\n");
						return -1;
					}
				}

				// memcpy

				// copy to framebuffer
				break;
			case HLT:
				destroy_framebuffer(fb);
				return 0;
		}
	}

	#undef BINARY_OP
	destroy_framebuffer(fb);
	return 0;
}


void print_buf(uint8_t* buf, size_t sz)
{
	printf("\n");
	for(int i = 0; i < sz; i++)
		printf("%02hhX ", buf[i]);
	printf("\n");
}

int main(int argc, char** argv)
{
	if(argc < 2)
	{
		fprintf(stderr, "no binary provided!\n");
		return -1;
	}

	cpu_t cpu = { 0 };
	STACK_INIT(&cpu.data_stack, sizeof(reg_t), 16);
	STACK_INIT(&cpu.call_stack, sizeof(reg_t), 16);

	int sz = read_file(argv[1], (char**)&cpu.mem); 
	if(sz < 0)
	{
		fprintf(stderr,"unable to read the binary!\n");
		free(cpu.mem);
		return -1;
	}
	
	print_buf(cpu.mem, sz);

	framebuf_t fb = { 0 };
	initialize_framebuffer(&fb);

	int retcode = 0;
	// int retcode = execute(&cpu, &fb);

	if(retcode != 0)
	{
		fprintf(stderr,"error during execution!\n");
		free(cpu.mem);
		return retcode;
	}

	free(cpu.mem);

	return 0;
}
