#include <stdlib.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <time.h>
#include <linux/fb.h>
#include <stdint.h>
#include <sys/mman.h>
#include <unistd.h>

#include <string.h>

#define TRUE 1
#define FALSE 0


#define USEQUEUE


int fb_fd = 9;
struct fb_fix_screeninfo finfo;
struct fb_var_screeninfo vinfo;
uint8_t *fbp = 0;

uint8_t *gridp = 0;

uint8_t *showp = 0;

uint8_t *donep = 0;

//#define GETSHOWP(texmex) (showp[(texmex)/8] & 1 << ((texmex)%8));

//#define MARKSHOWP(texmex)(showp[(texmex)/8] |=1 << ((texmex)%8));

inline uint32_t pixel_color(uint8_t r, uint8_t g, uint8_t b, struct fb_var_screeninfo *vinfo){
	return (r<<vinfo->red.offset) | (g<<vinfo->green.offset) | (b<<vinfo->blue.offset);
}


int width = 0;
int height = 0;

unsigned int fillRandom(unsigned int count){
	if(!gridp) return FALSE;
	if(count > width * height) return FALSE;
	for(; count; count--){
		int xpos = ((float)rand() / (float) RAND_MAX) * (float) width;
		int ypos = ((float)rand() / (float) RAND_MAX) * (float) height;

		uint8_t *offp = gridp + xpos + ypos * width;
		if(*offp == 10){
			count++;
			continue;
		}
		*offp = 10;


		int yorg = (ypos == 0) ? 0 : -1;
		int xorg = (xpos == 0) ? 0 : -1;
		int ym = (ypos + 1 >= height) ? 1 : 2;
		int xm = (xpos + 1 >= width)  ? 1 : 2;

		int x , y;

		for(y = yorg; y < ym; y++){
			offp = gridp + xpos + (ypos + y) * width;
			for(x = xorg; x < xm; x++){
				if(x == 0 && y == 0) continue;
				else offp[x] = offp[x] + 1;
			}
		}
	}
	return TRUE;
}

inline void mark(int x, int y){
	uint8_t *offp = donep + y * width;
//	uint8_t *offp = gridp + y * width;
	uint8_t *soffp = showp + y * width;
	long location = (x + vinfo.xoffset) * (vinfo.bits_per_pixel/8) + (y + vinfo.yoffset) * finfo.line_length;
	*((uint32_t*)(fbp + location)) = pixel_color(offp[x] * 255, soffp[x] * 25, 0, &vinfo);
//	usleep(10);
}

void expandGrid(const int x, const int y){
	if(x < 0 || x >=width) return;
	if(y < 0 || y >=height) return;
	int myx = x;
	int myy = y;
	//check self
	if(showp[myy * width + myx]) return;
	showp[myy * width + myx] = 5;
	//moving backwards 0 is start


	//move forwards

	while(TRUE){
		int i;
		int xd = myx;
		int yd = myy;
		//"probe" outwards, mark
		for(i = 1; i < 5; i++){
			xd = myx;
			yd = myy;
			switch(i){
				case 1: xd--; break;
				case 2: yd--; break;
				case 3: xd++; break;
				case 4: yd++; break;
				default: break;
			}
			if(xd >= width || xd < 0) continue;
			if(yd >= height|| yd < 0) continue;
			//check to make sure nofill
			if(showp[yd * width + xd]) continue;
			if(gridp[yd * width + xd]){
				//MARK the dir
				showp[yd * width + xd] = i;
				mark(xd, yd);
				//dont follow, force invalid path
				continue;
			}
			//all checks passed, valid path to take
			break;
		}
		if(i == 5){ //no valid dirs
			//get prev dir
			switch(showp[myy * width + myx]){
				//back that shit up
				case 1: myx++; break;
				case 2: myy++; break;
				case 3: myx--; break;
				case 4: myy--; break;
				default: return; break;//END OF LINE, FUCK OFF
			}
		} else { // found a valid
			//move along
			myx = xd;
			myy = yd;
			//MARK the dir
			showp[yd * width + xd] = i;
			mark(xd, yd);
		}
	}
}


inline int countFlagged(int xpos, int ypos){
	int yorg = (ypos == 0) ? 0 : -1;
	int xorg = (xpos == 0) ? 0 : -1;
	int ym = (ypos + 1 >= height) ? 1 : 2;
	int xm = (xpos + 1 >= width)  ? 1 : 2;
	int x , y, count = 0;
	for(y = yorg; y < ym; y++){
		uint8_t *offp = donep + xpos + (ypos + y) * width;
		for(x = xorg; x < xm; x++){
			if(x == 0 && y == 0) continue;
			else if(offp[x]) count++;
		}
	}
	return count;
}
inline int countUntested(int xpos, int ypos){
	int yorg = (ypos == 0) ? 0 : -1;
	int xorg = (xpos == 0) ? 0 : -1;
	int ym = (ypos + 1 >= height) ? 1 : 2;
	int xm = (xpos + 1 >= width)  ? 1 : 2;
	int x , y, count = 0;
	for(y = yorg; y < ym; y++){
		uint8_t *offp = showp + xpos + (ypos + y) * width;
		for(x = xorg; x < xm; x++){
			if(x == 0 && y == 0) continue;
			else if(!offp[x]) count++;
		}
	}
	return count;
}
typedef struct expqueue_s {
	int x;
	int y;
} expqueue_t;

expqueue_t *theq = 0;
size_t theqs =0;
size_t theqp =0;

void addtoq(int x, int y){
	if(theqp >= theqs){
		theqs = theqp + 32;
		theq = realloc(theq, theqs* sizeof(expqueue_t));
	}
	theq[theqp].x = x;
	theq[theqp].y = y;
	theqp++;
}

void shuffleq(void){
	size_t ms;
	for(ms = theqp-1; ms >= 0; ms--){
		int p = rand() % ms;
		expqueue_t t = theq[ms];
		theq[ms] = theq[p];
		theq[p] = t;
	}
}
void doq(void){
	size_t i;
	for(i = 0; i < theqp; i++){
		expandGrid(theq[i].x, theq[i].y);
	}
	theqp = 0;
}

//shitty shitty solve, can optimizzle
int solve(void){
	int x, y;
	int numtested = 0;
	for(y = 0; y < height; y++){
		uint8_t *offp = gridp + y * width;
		uint8_t *soffp = showp + y * width;
		uint8_t *doffp = donep + y * width;
		for(x = 0; x < width; x++){
			if(!soffp[x] || doffp[x]) continue;
			int num = offp[x];
			int fl = countFlagged(x,y);
			if(num == fl){	//all flagged, can mark all
				doffp[x] = TRUE;
				numtested++;
				int yorg = (y == 0) ? 0 : -1;
				int xorg = (x == 0) ? 0 : -1;
				int ym = (y + 1 >= height) ? 1 : 2;
				int xm = (x + 1 >= width)  ? 1 : 2;
				int xj , yj;
				for(yj = yorg; yj < ym; yj++){
					uint8_t *Foffp = donep + x + (y + yj) * width;
					for(xj = xorg; xj < xm; xj++){
						if(xj == 0 && yj == 0) continue;
#ifdef USEQUEUE
						else if(!Foffp[xj]) addtoq(x + xj, y + yj);
#else
						else if(!Foffp[xj]) expandGrid(x + xj, y + yj);
#endif
					}
				}
			} else if (num == countUntested(x, y) + fl){
				doffp[x] = TRUE;
				numtested++;
				int yorg = (y == 0) ? 0 : -1;
				int xorg = (x == 0) ? 0 : -1;
				int ym = (y + 1 >= height) ? 1 : 2;
				int xm = (x + 1 >= width)  ? 1 : 2;
				int xj , yj;
				for(yj = yorg; yj < ym; yj++){
					uint8_t *Foffp = showp + x + (y + yj) * width;
					uint8_t *Joffp = donep + x + (y + yj) * width;
					for(xj = xorg; xj < xm; xj++){
						if(xj == 0 && yj == 0) continue;
						else if(!Foffp[xj]){
							Joffp[xj] = TRUE;
							mark(x + xj, y + yj);
						}
					}
				}
			}
		}
	}
	#ifdef USEQUEUE
	doq();
	#endif
	return numtested;
}


int main(int argc, char ** argv){
	srand(time(NULL));

	fb_fd = open("/dev/fb0", O_RDWR);
	ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo);
	vinfo.grayscale = 0;
	vinfo.bits_per_pixel = 32;
	ioctl(fb_fd, FBIOPUT_VSCREENINFO, &vinfo);
	ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo);
	ioctl(fb_fd, FBIOGET_FSCREENINFO, &finfo);

	long screensize = vinfo.yres_virtual * finfo.line_length;
	width = vinfo.xres;
	height = vinfo.yres;
	fbp = mmap(0, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, (off_t) 0);
	uint8_t * fb_copy = malloc(screensize);
	memcpy(fb_copy, fbp, screensize);
	gridp = malloc(width * height);


		showp = malloc(width * height);
		donep = malloc(width * height);


	while(TRUE){
		memset(gridp, 0, width * height);
		memset(showp, 0, width * height);
		memset(donep, 0, width * height);


		fillRandom(atoi(argv[1]));
		/*
		int x, y;
		for( y = 0; y < height; y++){
			for(x = 0; x < width; x++){
				long location = (x + vinfo.xoffset) * (vinfo.bits_per_pixel/8) + (y + vinfo.yoffset) * finfo.line_length;
				*((uint32_t*)(fbp + location)) = 0;
			}
		}
		*/
	//	memset(fbp, 0, screensize);
		memcpy(fbp, fb_copy, screensize);

		expandGrid(width/2, height/2);
		while(solve()); usleep(100000);
		sleep(1);
	}
	return FALSE;
}
