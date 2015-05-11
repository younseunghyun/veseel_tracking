#include <stdlib.h>
#include <stdio.h>

#include "opencv/cv.h"
#include "opencv/highgui.h"

typedef struct {
	unsigned char r;
	unsigned char g;
	unsigned char b;
} Color;


typedef struct PosList {
	int x;
	int y;
	struct PosList *next;
} PosList;

int
colordiff(Color a, Color b)
{
	int dr, dg, db;

	dr = (int)((a.r < b.r) ? (b.r - a.r) : (a.r - b.r));
	dg = (int)((a.g < b.g) ? (b.g - a.g) : (a.g - b.g));
	db = (int)((a.b < b.b) ? (b.b - a.b) : (a.b - b.b));
	return dr + dg + db;
}

PosList *
newnode(int x, int y)
{
	PosList *pos;

	pos = (PosList *)malloc(sizeof(PosList));
	pos->x = x;
	pos->y = y;
	pos->next = NULL;
	return pos;
}

void
delnode(PosList **pos)
{
	free(*pos);
	*pos = NULL;
}

void
pl_push(PosList **list, PosList *pos)
{
	pos->next = *list;
	*list = pos;
}

PosList *
pl_pop(PosList **list)
{
	PosList *pos;

	pos = *list;
	*list = (*list)->next;
	return pos;
}

void
dellist(PosList **list)
{
	PosList *a, *b;

	a = *list;
	while (a != NULL) {
		b = a->next;
		delnode(&a);
		a = b;
	}
}

int
contains(PosList *list, int x, int y)
{
	while (list != NULL) {
		if (list->x == x && list->y == y)
			return 1;
		list = list->next;
	}
	return 0;
}

void
rgrow(IplImage *source, IplImage *dest, Color color, int threshold)
{
	PosList *list_n;
	PosList *node_r;
	PosList *list_r;
	Color curcolor;
	int x, y;
	int sx, sy;
	int dx, dy;
	int offset;
	int mindiff, curdiff;
#ifdef REC
	CvVideoWriter *writer;
	CvSize size;
	IplImage *frame;
	int i;
#endif

#ifdef REC
	size = cvSize(dest->width, dest->height);
	frame = cvCreateImage(size, IPL_DEPTH_8U, 3);
	writer = cvCreateVideoWriter("rgrow.avi", CV_FOURCC('M', 'J', 'P', 'G'), 15.0, size, 1);
#endif
	mindiff = 255 * 3;
	for (y = 0; y < source->height; y++) {
		for (x = 0; x < source->width; x++) {
			offset = y * source->width * 3 + x * 3;
			curcolor.b = source->imageData[offset];
			curcolor.g = source->imageData[offset + 1];
			curcolor.r = source->imageData[offset + 2];
			curdiff = colordiff(color, curcolor);
			if (curdiff < mindiff) {
				sx = x;
				sy = y;
				mindiff = curdiff;
			}
			dest->imageData[y * dest->width + x] = 0;
		}
	}
	list_n = newnode(sx, sy);
	list_r = NULL;
	while (list_n != NULL) {
		sx = list_n->x;
		sy = list_n->y;
		pl_push(&list_r, pl_pop(&list_n));
		for (dy = -1; dy <= 1; dy++) {
			for (dx = -1; dx <= 1; dx++) {
				if (dx == 0 && dy == 0)
					continue;
				if (dx != 0 && dy != 0)
					continue;
				if (sx + dx == -1 || sx + dx == source->width ||
					sy + dy == -1 || sy + dy == source->height)
					break;
				if (contains(list_n, sx + dx, sy + dy))
					continue;
				if (contains(list_r, sx + dx, sy + dy))
					continue;
				offset = (sy + dy) * source->width * 3 + (sx + dx) * 3;
				curcolor.b = source->imageData[offset];
				curcolor.g = source->imageData[offset + 1];
				curcolor.r = source->imageData[offset + 2];
				curdiff = colordiff(color, curcolor);
				if (curdiff <= threshold)
					pl_push(&list_n, newnode(sx + dx, sy + dy));
			}
			if (dx < 2)
				break;
		}
	}
#ifdef REC
	cvCvtColor(dest, frame, CV_GRAY2BGR);
	cvWriteFrame(writer, frame);
	i = 0;
#endif
	node_r = list_r;
	while (node_r != NULL) {
		dest->imageData[node_r->y * dest->width + node_r->x] = 255;
#ifdef REC
		if (i % 100 == 0) {
			cvCvtColor(dest, frame, CV_GRAY2BGR);
			cvWriteFrame(writer, frame);
		}
		i++;
#endif
		node_r = node_r->next;
	}
	dellist(&list_r);
#ifdef REC
	cvReleaseVideoWriter(&writer);
#endif
}

int
main(int argc, char *argv[])
{
	IplImage *source = NULL;
	IplImage *dest = NULL;
	char *infile, *outfile;
	int64 t0, t1;
	double tps, deltatime;
	Color color;
	int threshold;
	std::cout << argc << std::endl;
	if (argc != 7) {
		puts("Usage: rgrow in out r g b threshold");
		return EXIT_FAILURE;
	}
	infile = argv[1];
	outfile = argv[2];
	color.r = (unsigned char)strtol(argv[3], NULL, 0);
	color.g = (unsigned char)strtol(argv[4], NULL, 0);
	color.b = (unsigned char)strtol(argv[5], NULL, 0);
	threshold = (int)strtol(argv[6], NULL, 0);
	source = cvLoadImage(infile, CV_LOAD_IMAGE_COLOR);
	if (source == NULL) {
		printf("Could not load image \"%s\".\n", infile);
		return EXIT_FAILURE;
	}
	dest = cvCreateImage(cvSize(source->width, source->height),
		IPL_DEPTH_8U, 1);
	t0 = cvGetTickCount();
	rgrow(source, dest, color, threshold);
	t1 = cvGetTickCount();
	tps = cvGetTickFrequency() * 1.0e6;
	deltatime = (double)(t1 - t0) / tps;

	cvSaveImage(outfile, dest, 0);
	printf("%dx%d image processed in %.3f seconds.\n",
		source->width, source->height, deltatime);
	cvReleaseImage(&source);
	cvReleaseImage(&dest);
	return EXIT_SUCCESS;
}