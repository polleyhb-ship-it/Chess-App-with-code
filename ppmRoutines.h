#include <stdio.h>
#include <string.h>
#include <stdint.h>

// structure to hold a ppm image format
typedef struct ppms_struct {
	unsigned char *image;
	int64_t xwid;
	int64_t ywid;
	char name[40];
	int64_t ncol;
}ppm_t;


int64_t readPPMimage(char *filename, ppm_t *InImage);
void RgbToRGBA(ppm_t *ppmImage); 
void softenRGBA(ppm_t *ppmImage);
void deblipAlpha( ppm_t *ppmImage, int64_t threshold);
void softenAlpha(int64_t iix, int64_t iiy, ppm_t * ppmImage, int64_t threshold);
void writePPMimage(char *filename, ppm_t InImage);
void freePPMimage(ppm_t InImage);

