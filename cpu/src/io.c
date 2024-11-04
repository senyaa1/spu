#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/ioctl.h>

#include "io.h"


void print_buf(uint8_t* buf, size_t sz)
{
	printf("\n");
	for(int i = 0; i < sz; i++)
		printf("%02hhX ", buf[i]);
	printf("\n");
}


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
