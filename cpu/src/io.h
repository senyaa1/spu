#pragma once

#include <stdlib.h>
#include <stdint.h>
#include <linux/fb.h>

#define RED   "\x1B[31m"
#define GREEN   "\x1B[32m"
#define YELLOW   "\x1B[33m"
#define BLUE   "\x1B[34m"
#define MAGENTA   "\x1B[35m"
#define CYAN   "\x1B[36m"
#define WHITE   "\x1B[37m"
#define UNDERLINE "\e[4m"
#define RESET "\x1B[0m"

#define FB_WIDTH 480
#define FB_HEIGHT 360

static const char* FB_DEV = "/dev/fb1";
static const size_t FB_SIZE = FB_WIDTH * FB_HEIGHT * sizeof(uint32_t);

typedef struct framebuf 
{
	int fd;
	char* addr;
	struct fb_var_screeninfo vinfo;
	struct fb_fix_screeninfo finfo;

} framebuf_t;

void print_buf(uint8_t* buf, size_t sz);
int framebuffer_initialize(framebuf_t* fb);
void framebuffer_destroy(framebuf_t* fb);
void framebuffer_draw(framebuf_t* fb, uint8_t* dataptr);
