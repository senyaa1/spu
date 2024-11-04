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
	char* addr;
	struct fb_var_screeninfo vinfo;
	struct fb_fix_screeninfo finfo;

} framebuf_t;

#define FB_WIDTH 600
#define FB_HEIGHT 300

const size_t FB_SIZE = FB_WIDTH * FB_HEIGHT * sizeof(uint32_t);

const char* FB_DEV = "/dev/fb1";

int initialize_framebuffer(framebuf_t* fb)
{
	int fbfd = open(FB_DEV, O_RDWR);

	if (fbfd == -1) return -1;

	ioctl(fbfd, FBIOGET_VSCREENINFO, &fb->vinfo);
	ioctl(fbfd, FBIOGET_FSCREENINFO, &fb->finfo);

	fb->addr = mmap(NULL, fb->vinfo.xres * fb->vinfo.yres * fb->vinfo.bits_per_pixel / 8, PROT_READ | PROT_WRITE, MAP_SHARED, fbfd, 0);

	printf("width: %d\theight: %d\n", fb->vinfo.xres, fb->vinfo.yres);

	if(fb->addr == MAP_FAILED) return -1;

	return 0;
}

void destroy_framebuffer(framebuf_t* fb)
{
	if(fb->addr == 0)  return;
	munmap(fb->addr, FB_SIZE);
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
	#define ENUMDMP(val)	printf(BLUE #val YELLOW "\t= 0x%X \n" RESET, cpu->regs[val]);
	#define FLAGDMP(val)	printf(((cpu->regs[FLAGS] & val) ? (GREEN UNDERLINE #val RESET " ") : (RED #val RESET " ")));

	printf("CPU\n");
	printf(WHITE UNDERLINE "Registers\n" RESET);
	
	ENUMDMP(AX)
	ENUMDMP(BX)
	ENUMDMP(CX)
	ENUMDMP(DX)
	ENUMDMP(EX)
	ENUMDMP(IP)
	ENUMDMP(IDTR)
	ENUMDMP(GDTR)
	ENUMDMP(FLAGS)

	printf("\t  ");
	FLAGDMP(ZF)
	FLAGDMP(SF)
	FLAGDMP(IF)

	printf("\n\n");

	printf(WHITE UNDERLINE "Call Stack\n" RESET);
	stack_print(&cpu->call_stack);

	printf(WHITE UNDERLINE "Data Stack\n" RESET);
	stack_print(&cpu->data_stack);

	#undef ENUMDMP
	#undef FLAGDMP
}

void set_flags(cpu_t* cpu, reg_t res)
{
	if(res == 0)	cpu->regs[FLAGS] |= ZF;
	else		cpu->regs[FLAGS] &= ~ZF;

	if(res < 0)	cpu->regs[FLAGS] |= SF;
	else		cpu->regs[FLAGS] &= ~SF;
}

#define BINARY_OP(INST, OP)			\
       case INST:				\
       stack_pop(&cpu->data_stack, &a);		\
       stack_pop(&cpu->data_stack, &b);		\
       OP;					\
       stack_push(&cpu->data_stack, &res);	\
       break;					\


int execute(cpu_t* cpu, framebuf_t* fb)
{
	while(1)
	{
		uint8_t opcode = cpu->mem[cpu->regs[IP]];
		instruction_t inst = opcode & 0b00111111;
		operand_t operands[MAX_OPERAND_CNT] = { 0 };
		uint8_t operand_cnt = opcode >> 6;

		cpu->regs[IP] += sizeof(instruction_t);

		// printf("current insturction: %x\n", inst);
		// printf("operand cnt: %d\n", operand_cnt);
		// printf("ip: %d\n", cpu->regs[IP]);
		
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
					break;

				case OP_REGPTR:
					operands[i].value = ((reg_name_t*)cpu->mem)[cpu->regs[IP]];
					operands[i].actual_value = cpu->mem[cpu->regs[operands[i].value]];
					break;

				default:
					break;
			}
			
			// printf("is ptr: %d\t", ((operands[i].type & OPERAND_PTR_BIT) != 0));
			// printf("type: %d,\tlength: %d\tvalue: 0x%x\t\n", operands[i].type, operands[i].length, operands[i].value);
			
			cpu->regs[IP] += operands[i].length;

		}
		
		reg_t a = 0, b = 0, res = 0;
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
						cpu->regs[operands[0].value] = operands[1].actual_value;
						break;
					case OP_REGPTR:
						cpu->mem[cpu->regs[operands[0].value]] = operands[1].actual_value;
						break;

					case OP_VALUEPTR:
						cpu->mem[operands[0].value] = operands[1].actual_value;
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
			BINARY_OP(SUB, res = b - a; set_flags(cpu, res);)
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
				if(operand_cnt == 0)
				{
					stack_pop(&cpu->data_stack, &a);
					stack_pop(&cpu->data_stack, &b);
				}
				else
				{
					a = operands[1].actual_value;
					b = operands[0].actual_value;
				}

				set_flags(cpu, b - a);
				break;
			case DRAW:
				if(fb->addr == 0)
				{
					fprintf(stderr, "framebuffer is not initialized!\n");
					return -1;
				}
				
				uint32_t* dataptr = (uint32_t*)(cpu->mem + operands[0].actual_value);

				size_t y_offset = (fb->vinfo.yres - FB_HEIGHT) / 2;
				size_t x_offset = (fb->vinfo.xres - FB_WIDTH) / 2;



				for (size_t y = 0; y < FB_HEIGHT; y++)
				{
					for (size_t x = 0; x < FB_WIDTH; x++) 
					{
						size_t location = (x + fb->vinfo.xoffset + x_offset) * (fb->vinfo.bits_per_pixel/8) + (y + fb->vinfo.yoffset + y_offset) * fb->finfo.line_length;

						if (fb->vinfo.bits_per_pixel == 32) {
							*(fb->addr + location) = 100;			// Some blue
							*(fb->addr + location + 1) = 15+(x-100)/2;	// A little green
							*(fb->addr + location + 2) = 200-(y-100)/5;	// A lot of red
							*(fb->addr + location + 3) = 0;			// No transparency
						} else  { 
							int b = 10;
							int g = (x-100)/6;     
							int r = 31-(y-100)/16;    
							unsigned short int t = r<<11 | g << 5 | b;
							*((unsigned short int*)(fb->addr + location)) = t;
						}

					}
				}

				printf(GREEN UNDERLINE "copied to framebuffer!\n" RESET);
				break;
			case HLT:
				printf(GREEN "halting!\n" RESET);
				return 0;

			case JMP:
				cpu->regs[IP] = operands[0].value;
				break;
			case JZ:
				if(cpu->regs[FLAGS] & 1)					cpu->regs[IP] = operands[0].value;
				break;
			case JNZ:
				if(!(cpu->regs[FLAGS] & 1))					cpu->regs[IP] = operands[0].value;
				break;
			case JG:
				if(!(cpu->regs[FLAGS] & (1 << 1)))				cpu->regs[IP] = operands[0].value;
				break;
			case JGE:
				if(!(cpu->regs[FLAGS] & (1 << 1)) || (cpu->regs[FLAGS] & 1))	cpu->regs[IP] = operands[0].value;
				break;
			case JL:
				if(cpu->regs[FLAGS] & (1 << 1))					cpu->regs[IP] = operands[0].value;
				break;
			case JLE:
				if(cpu->regs[FLAGS] & (1 << 1) || (cpu->regs[FLAGS] & 1))	cpu->regs[IP] = operands[0].value;
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
			case IN:
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
						printf(GREEN "%s" RESET, cpu->mem + operands[0].actual_value);
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
	}
	return 0;
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
		return -1;
	}

	cpu.mem = realloc(cpu.mem, sz + 0xFFFFFF);
	
	// print_buf(cpu.mem, sz);

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
