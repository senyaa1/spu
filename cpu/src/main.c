#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/ioctl.h>

#include <linux/fb.h>

#include "arch.h"
#include "fs.h"
#include "stack.h"
#include "io.h"


void print_buf(uint8_t* buf, size_t sz)
{
	printf("\n");
	for(int i = 0; i < sz; i++)
		printf("%02hhX ", buf[i]);
	printf("\n");
}

typedef struct idt
{
	uint8_t iv_cnt;
	reg_t* interrupt_vectors;
} idt_t;

typedef struct cpu
{
	reg_t regs[16];
	stack_t data_stack;
	stack_t call_stack;
	uint8_t* mem;

	idt_t idt;
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

int load_idt(cpu_t* cpu)
{
	cpu->idt.iv_cnt = cpu->mem[cpu->regs[IDTR]];
	free(cpu->idt.interrupt_vectors);

	cpu->idt.interrupt_vectors = calloc(cpu->idt.iv_cnt, sizeof(reg_t));

	for(int i = 0; i < cpu->idt.iv_cnt; i++)
		memcpy(&cpu->idt.interrupt_vectors[i], &cpu->mem[cpu->regs[IDTR] + 1 + i * sizeof(reg_t)], sizeof(reg_t));

	return 0;
}

void cpu_dump(cpu_t* cpu)
{
	#define ENUMDMP(val) printf("\t" #val ": %d \n", cpu->regs[val]);

	printf("CPU\n");
	printf("==Registers==\n");
	ENUMDMP(AX)
	ENUMDMP(BX)
	ENUMDMP(CX)
	ENUMDMP(DX)
	ENUMDMP(EX)
	ENUMDMP(IP)
	ENUMDMP(SP)
	ENUMDMP(FLAGS)
	ENUMDMP(IDTR)
	ENUMDMP(GDTR)

	
	#undef ENUMDMP
}

int execute(cpu_t* cpu, framebuf_t* fb)
{
	while(1)
	{
		uint8_t opcode = cpu->mem[cpu->regs[IP]];
		instruction_t inst = opcode & 0b00111111;

		operand_t operands[MAX_OPERAND_CNT] = { 0 };
		uint8_t operand_cnt = opcode >> 6;

		cpu->regs[IP] += sizeof(instruction_t);

		printf("current insturction: %x\n", inst);
		printf("operand cnt: %d\n", operand_cnt);
		printf("ip: %d\n", cpu->regs[IP]);
		
		for(int i = 0; i < operand_cnt; i++)
		{
			operands[i].type = cpu->mem[cpu->regs[IP]] >> 4;
			operands[i].length = cpu->mem[cpu->regs[IP]] & 0b1111;
			cpu->regs[IP]++;


			reg_t data = 0;

			switch(operands[i].type)
			{
				case OP_VALUE:
					memcpy(&operands[i].value, cpu->mem + cpu->regs[IP], sizeof(reg_t));
					operands[i].actual_value = operands[i].value;
					break;
				case OP_VALUEPTR:
					memcpy(&operands[i].value, cpu->mem + cpu->regs[IP], sizeof(reg_t));
					operands[i].actual_value = cpu->mem[operands[i].value];
					break;
				case OP_REG:
					operands[i].value = ((reg_name_t*)cpu->mem)[cpu->regs[IP]];
					operands[i].actual_value = cpu->regs[operands[i].value];
				case OP_REGPTR:
					operands[i].value = ((reg_name_t*)cpu->mem)[cpu->regs[IP]];
					operands[i].actual_value = cpu->mem[cpu->regs[operands[i].value]];
					break;

				default:
					break;
			}
			
			printf("is ptr: %d\t", ((operands[i].type & OPERAND_PTR_BIT) != 0));
			printf("type: %d,\tlength: %d\tvalue: 0x%x\t\n", operands[i].type, operands[i].length, operands[i].value);
			
			cpu->regs[IP] += operands[i].length;

		}
		// printf("ip: %d\n", cpu->regs[IP]);
		
		
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
				if(operand_cnt != 2)
				{
					fprintf(stderr, "mov should have 2 operands!\n");
					return -1;
				}
				printf(RED "val: %d\n" RESET, operands[1].actual_value);
				switch(operands[0].type)
				{
					case OP_VALUE:
						fprintf(stderr, RED "can't move i to a fucking value!!\n" RESET);
						return -1;
					case OP_REG:
						printf(RED "moving to reg\n" RESET);
						cpu->regs[operands[0].value] = operands[1].actual_value;
						break;
					case OP_REGPTR:
						cpu->mem[cpu->regs[operands[0].value]] = operands[1].actual_value;
						printf(RED "moving to regptr\n" RESET);
						break;

					case OP_VALUEPTR:
						printf(RED "moving to valueptr\n" RESET);
						cpu->mem[operands[0].value] = operands[1].actual_value;
						break;
					default:
						printf(RED "da hell??\n" RESET);
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
			BINARY_OP(SUB, res = b - a; cpu->regs[FLAGS] |= (res == 0));
			BINARY_OP(MUL, res = a * b);
			BINARY_OP(DIV, res = b / a);

			BINARY_OP(AND, res = a & b);
			BINARY_OP(OR, res = a | b; );
			BINARY_OP(XOR, res = a ^ b);

			case CALL:
				stack_push(&cpu->call_stack, &cpu->regs[IP]);
				cpu->regs[IP] = operands[0].actual_value;
				break;
			case RET:
				stack_pop(&cpu->call_stack, &cpu->regs[IP]);
				break;
			case DUMP:
				cpu_dump(cpu);
				break;
			case CMP:
				stack_pop(&cpu->call_stack, &a);
				stack_pop(&cpu->call_stack, &b);

				if(a == b)	cpu->regs[FLAGS] |= 1;		// ZF (0)
				else		cpu->regs[FLAGS] &= ~1;

				if(a < b)	cpu->regs[FLAGS] |= (1 << 1);	// CF (1)
				else		cpu->regs[FLAGS] &= ~(1 << 1);

				break;
			case DRAW:
				if(fb->addr == 0)
				{
					fprintf(stderr, "framebuffer is not initialized!\n");
					return -1;
				}

				// memcpy

				// copy to framebuffer
				break;
			case HLT:
				printf(GREEN "halting!\n" RESET);
				return 0;

			case JMP:
				cpu->regs[IP] = operands[0].value;
				break;
			case JZ:
				if(cpu->regs[FLAGS] & 1) cpu->regs[IP] = operands[0].value;
				break;
			case JNZ:
				if(!(cpu->regs[FLAGS] & 1)) cpu->regs[IP] = operands[0].value;
				break;
			case JG:
				if(!(cpu->regs[FLAGS] & (1 << 1))) cpu->regs[IP] = operands[0].value;
				break;
			case JGE:
				if(!(cpu->regs[FLAGS] & (1 << 1)) || (cpu->regs[FLAGS] & 1)) cpu->regs[IP] = operands[0].value;
				break;
			case JL:
				if(cpu->regs[FLAGS] & (1 << 1)) cpu->regs[IP] = operands[0].value;
				break;
			case JLE:
				if(cpu->regs[FLAGS] & (1 << 1) || (cpu->regs[FLAGS] & 1)) cpu->regs[IP] = operands[0].value;
				break;

			case OUT:
				switch(operand_cnt)
				{
					case 0:
						stack_pop(&cpu->data_stack, &a);
						printf(GREEN "%d " RESET, a);
						break;
					case 1:
						printf(GREEN "%d " RESET, operands[0].actual_value);
						break;
				}
				break;
			case INPUT:
				printf(BLUE "input: ");
				scanf("%d", &a);
				printf(RESET);
				stack_push(&cpu->data_stack, &a);
				break;
			case PRINT:
				switch(operand_cnt)
				{
					case 0:
						stack_pop(&cpu->data_stack, &a);
						printf(GREEN "%d" RESET, a);
						break;
					case 1:
						// printf("printing string at %d\n", operands[0].value);
						printf(GREEN "%s\n" RESET, cpu->mem + operands[0].actual_value);
						break;
				}
				break;
			case LIDT:
				cpu->regs[IDTR] = operands[0].value;
				if(load_idt(cpu))
				{
					fprintf(stderr, RED "Unable to load IDT!\n" RESET);
					return -1;
				}
				printf(BLUE "loaded interrupt descriptor table at %d: sz: %d\n" RESET, cpu->regs[IDTR], cpu->idt.iv_cnt);
				break;
			case LGDT:
				cpu->regs[GDTR] = operands[0].value;
				printf(BLUE "loaded global descriptor table at %x: first byte: %x\n" RESET, cpu->regs[GDTR], cpu->mem[cpu->regs[GDTR]]);
				break;
			case IRET:
				stack_pop(&cpu->call_stack, &cpu->regs[IP]);
				break;
			case INT:
				stack_push(&cpu->call_stack, &cpu->regs[IP]);
				printf("INTERRUPTING!\n");
				if(operands[0].actual_value - 1 > cpu->idt.iv_cnt)
				{
					fprintf(stderr, RED "Interrupt handler not found!\n" RESET);
					return -1;
				}
				cpu->regs[IP] = cpu->idt.interrupt_vectors[operands[0].actual_value - 1];
				break;
			default:
				fprintf(stderr, RED "Encountered INVALID instruction!: %d (%x) \n" RESET, inst, inst);
				return -1;
		}
		
		// stack_print(&cpu->data_stack, "amogus", 1, "sus()");


	}

	#undef BINARY_OP
	return 0;
}


const int RAM_OVERALLOCATION = 100000;

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
		return -1;
	}

	cpu.mem = realloc(cpu.mem, sz + RAM_OVERALLOCATION);
	
	print_buf(cpu.mem, sz);

	framebuf_t fb = { 0 };
	initialize_framebuffer(&fb);

	int retcode = execute(&cpu, &fb);

	destroy_framebuffer(&fb);
	free(cpu.mem);
	free(cpu.idt.interrupt_vectors);
	stack_dtor(&cpu.call_stack);
	stack_dtor(&cpu.data_stack);

	return retcode;
}
