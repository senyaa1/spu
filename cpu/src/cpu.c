#include <string.h>
#include <stdio.h>
#include <unistd.h>

#include "arch.h"
#include "fs.h"
#include "stack.h"
#include "cpu.h"
#include "io.h"

#define BINARY_OP(INST, OP)			\
       case INST:				\
       stack_pop(&cpu->data_stack, &a);		\
       stack_pop(&cpu->data_stack, &b);		\
       OP;					\
       stack_push(&cpu->data_stack, &res);	\
       break;					\


static int load_idt(cpu_t* cpu)
{
	cpu->idt.iv_cnt = cpu->mem[cpu->regs[IDTR]];
	free(cpu->idt.interrupt_vectors);

	cpu->idt.interrupt_vectors = calloc(cpu->idt.iv_cnt, sizeof(reg_t));

	for(int i = 0; i < cpu->idt.iv_cnt; i++)
		memcpy(&cpu->idt.interrupt_vectors[i], &cpu->mem[cpu->regs[IDTR] + 1 + i * sizeof(reg_t)], sizeof(reg_t));

	return 0;
}

static int load_gdt(cpu_t* cpu)
{
	cpu->gdt.gdt_entry_cnt = cpu->mem[cpu->regs[GDTR]];
	free(cpu->gdt.gdt_entries);

	cpu->gdt.gdt_entries = calloc(cpu->gdt.gdt_entry_cnt, sizeof(gdt_entry_t));

	for(int i = 0; i < cpu->gdt.gdt_entry_cnt; i++)
		memcpy(&cpu->gdt.gdt_entries[i], &cpu->mem[cpu->regs[GDTR] + 1 + i * sizeof(gdt_entry_t)], sizeof(gdt_entry_t));

	return 0;
}

static void cpu_dump(cpu_t* cpu)
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

static void set_flags(cpu_t* cpu, reg_t res)
{
	if(res == 0)	cpu->regs[FLAGS] |= ZF;
	else		cpu->regs[FLAGS] &= ~ZF;

	if(res < 0)	cpu->regs[FLAGS] |= SF;
	else		cpu->regs[FLAGS] &= ~SF;
}

int execute(cpu_t* cpu, framebuf_t* fb)
{
	cpu->priv = 0;	// start in ring 0
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

			// check if allowed by gdt
			
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
				
				uint8_t* dataptr = (uint8_t*)(cpu->mem + operands[0].actual_value);

				size_t y_offset = (fb->vinfo.yres - FB_HEIGHT) / 2;
				size_t x_offset = (fb->vinfo.xres - FB_WIDTH) / 2;
				for (size_t y = 0; y < FB_HEIGHT; y++)
				{
					for (size_t x = 0; x < FB_WIDTH; x++) 
					{
						size_t location = (x + fb->vinfo.xoffset + x_offset) * (fb->vinfo.bits_per_pixel/8) + (y + fb->vinfo.yoffset + y_offset) * fb->finfo.line_length;

						int r = dataptr[y * FB_WIDTH + x];
						int g = dataptr[y * FB_WIDTH + x];
						int b = dataptr[y * FB_WIDTH + x];

						if (fb->vinfo.bits_per_pixel == 32) 
						{
							*(fb->addr + location) = b;
							*(fb->addr + location + 1) = g;
							*(fb->addr + location + 2) = r;
							*(fb->addr + location + 3) = 0;
						} 
						else  
						{ 
							*((unsigned short int*)(fb->addr + location)) = (r << 11 | g << 5 | b);
						}

					}
				}

				usleep(50000);	// too fast
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
				if(cpu->priv != 0)
				{
					fprintf(stderr, RED "No privs to load IDT!\n" RESET);
					return -1;
				}

				cpu->regs[IDTR] = operands[0].value;
				if(load_idt(cpu))
				{
					fprintf(stderr, RED "Unable to load IDT!\n" RESET);
					return -1;
				}

				printf(BLUE "loaded interrupt descriptor table at %d: sz: %d\n" RESET, cpu->regs[IDTR], cpu->idt.iv_cnt);
				break;
			case LGDT:
				if(cpu->priv != 0)
				{
					fprintf(stderr, RED "No privs to load GDT!\n" RESET);
					return -1;
				}

				cpu->regs[GDTR] = operands[0].value;

				if(load_gdt(cpu))
				{
					fprintf(stderr, RED "Unable to load GDT!\n" RESET);
					return -1;
				}

				printf(BLUE "loaded global descriptor table, entry cnt: %d\n" RESET, cpu->gdt.gdt_entry_cnt);
				break;
			case IRET:
				if(cpu->priv != 0)
				{
					fprintf(stderr, RED "No privs to IRET!\n" RESET);
					return -1;
				}
				stack_pop(&cpu->call_stack, &cpu->regs[IP]);
				cpu->priv = 1;
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
				cpu->priv = 0;
				break;
			default:
				fprintf(stderr, RED "Encountered INVALID instruction!: %d (%x) \n" RESET, inst, inst);
				return -1;
		}
	}
	return 0;
}
