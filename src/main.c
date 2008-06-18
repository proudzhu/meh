
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#include "meh.h"

extern Display *display;

/* Supported Formats */
extern struct imageformat libjpeg;
extern struct imageformat giflib;
extern struct imageformat libpng;
extern struct imageformat gif;
struct imageformat *formats[] = {
	&libjpeg,
	&gif,
	&libpng,
	&giflib,
	NULL
};


void usage(){
	printf("USAGE: meh [FILE1 [FILE2 [...]]]\n");
	printf("       meh -list                 : treat stdin as list of files\n");
	printf("       meh -ctl                  : display files as they are received on stdin\n");
	exit(1);
}

struct image *imgopen(const char *filename){
	struct image *img = NULL;
	struct imageformat **fmt = formats;
	FILE *f;
	if((f = fopen(filename, "rb")) == NULL){
		fprintf(stderr, "Cannot open '%s'\n", filename);
		return NULL;
	}
	for(fmt = formats; *fmt; fmt++){
		if((img = (*fmt)->open(f))){
			img->fmt = *fmt;
			return img;
		}
	}
	fprintf(stderr, "Unknown file type: '%s'\n", filename);
	return NULL;
}

struct imagenode{
	struct imagenode *next, *prev;
	char *filename;
};

struct imagenode *buildlist(int argc, char *argv[]){
	struct imagenode *head = NULL, *tail = NULL, *tmp;
	if(argc){
		while(argc--){
			tmp = malloc(sizeof(struct imagenode));
			if(!head)
				head = tail = tmp;
			tmp->filename = *argv++;
			tmp->prev = tail;
			tail->next = tmp;
			tail=tmp;
		}
		tail->next = head;
		head->prev = tail;
		return head;
	}else{
		fprintf(stderr, "No files to view\n");
		exit(1);
	}
}

struct imagenode *images;

const char *nextimage(){
	images = images->next;
	return images->filename;
}

const char *previmage(){
	images = images->prev;
	return images->filename;
}

void run(){
	const char *(*direction)() = nextimage;
	const char *filename = direction();
	int width = 0, height = 0;
	struct image *img = NULL;
	int redraw = 0;

	for(;;){
		XEvent event;
		for(;;){
			if(redraw && !XPending(display))
				break;
			XNextEvent(display, &event);
			switch(event.type){
				case MapNotify:
					break;
				case ConfigureNotify:
					if(width != event.xconfigure.width || height != event.xconfigure.height){
						width = event.xconfigure.width;
						height = event.xconfigure.height;
						redraw = 1;

						/* Some window managers need reminding */
						if(img)
							setaspect(img->width, img->height);
					}
					break;
				case Expose:
					redraw = 1;
					break;
				case KeyPress:
					switch(XLookupKeysym(&event.xkey, 0)){
						case XK_Escape:
							exit(0);
							break;
						case XK_q:
							exit(0);
							break;
						case XK_t:
						case XK_n:
							if(XLookupKeysym(&event.xkey, 0) == XK_t){
								direction = nextimage;
							}else{
								direction = previmage;
							}
							filename = direction();
							/* Pass through */
						case XK_r:
							if(img){
								if(img->buf)
									free(img->buf);
								free(img);
							}
							img = NULL;
							redraw = 1;
							break;
						case XK_Return:
							puts(filename);
							fflush(stdout);
							break;
					}
					break;
			}
		}
		if(redraw){
			if(!img){
				const char *firstimg = filename;
				while(!(img = imgopen(filename))){
					filename = direction();
					if(filename == firstimg){
						fprintf(stderr, "No valid images to view\n");
						exit(1);
					}
				}
				img->buf = NULL;
				setaspect(img->width, img->height);
				continue; /* make sure setaspect can do its thing */
			}
			if(!img->buf){
				img->buf = malloc(3 * img->width * img->height);
				if(img->fmt->read(img)){
					fprintf(stderr, "read error!\n");
				}
				continue; /* Allow for some events to be read, read is slow */
			}
			drawimage(img, width, height);
			redraw = 0;
		}
	}
}

int main(int argc, char *argv[]){
	if(argc < 2)
		usage();
	xinit();

	images = buildlist(argc - 1, &argv[1]);
	run();

	return 0;
}

