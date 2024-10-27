#include <stdlib.h>
#include <stdbool.h>
#include <strings.h>
#include <unistd.h>

#include "io.h"
#include "fs.h"
#include "assembler.h"


int main(int argc, char** argv)
{
	asm_info_t asm_info = { 0 };

	int opt = 0;
	char* output_filename = "a.out";

	while ((opt = getopt(argc, argv, "o:")) != -1) {
		switch (opt) {
			case 'o':
				output_filename = optarg;
				break;
			default:
				print_error("unknown option!");
				return 1;
		}
	}

	if(argc - optind == 0)
	{
		print_error("no source code provided!");
		goto end;
	}

	int sz = read_file(argv[optind], &(asm_info.src_buf));
	if(sz < 0)
	{
		print_error("unable to read the source code!");
		goto end;
	}

	asm_info.src_buf_sz = sz;

	int final_sz = assemble(&asm_info);
	if(final_sz < 0)
	{
		goto end;
	}

	// print_buf(asm_info.asm_buf, asm_info.asm_buf_sz);

	if(fixup(&asm_info) < 0)
	{
		goto end;
	}

	// print_buf(asm_info.asm_buf, asm_info.asm_buf_sz);

	int out_sz = write_file(output_filename, asm_info.asm_buf, asm_info.asm_buf_sz);

	if(out_sz < 0)
	{
		print_error("unable to write final binary!");
		goto end;
	}


end:
	free(asm_info.src_buf);
	free(asm_info.asm_buf);
	free(asm_info.labels);
	free(asm_info.fixups);
	return 0;
}
