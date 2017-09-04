
#include <stdlib.h>
#include <stdio.h>

#include "jpeglib.h"


struct jpgData
{
	int Height;
	int Width;
	int *BMap;
	int Depth;
};



int loadJpg(const char* Name, struct jpgData *data) {
	if(data ==NULL)return -1;
  unsigned char a, r, g, b;
  int width, height;
  struct jpeg_decompress_struct cinfo;
  struct jpeg_error_mgr jerr;

  FILE * infile;        /* source file */
  JSAMPARRAY pJpegBuffer;       /* Output row buffer */
  int row_stride;       /* physical row width in output buffer */
  if ((infile = fopen(Name, "rb")) == NULL) {
    fprintf(stderr, "can't open %s\n", Name);
    return 0;
  }
  cinfo.err = jpeg_std_error(&jerr);
  jpeg_create_decompress(&cinfo);
  jpeg_stdio_src(&cinfo, infile);
  (void) jpeg_read_header(&cinfo, TRUE);
  (void) jpeg_start_decompress(&cinfo);
  width = cinfo.output_width;
  height = cinfo.output_height;

  unsigned char * pDummy = new unsigned char [width*height*4];
  unsigned char * pTest = pDummy;
  if (!pDummy) {
    printf("NO MEM FOR JPEG CONVERT!\n");
    return 0;
  }
  row_stride = width * cinfo.output_components;
  pJpegBuffer = (*cinfo.mem->alloc_sarray)
    ((j_common_ptr) &cinfo, JPOOL_IMAGE, row_stride, 1);

  while (cinfo.output_scanline < cinfo.output_height) {
    (void) jpeg_read_scanlines(&cinfo, pJpegBuffer, 1);
    for (int x = 0; x < width; x++) {
      a = 0; // alpha value is not supported on jpg
      r = pJpegBuffer[0][cinfo.output_components * x];
      if (cinfo.output_components > 2) {
        g = pJpegBuffer[0][cinfo.output_components * x + 1];
        b = pJpegBuffer[0][cinfo.output_components * x + 2];
      } else {
        g = r;
        b = r;
      }
      *(pDummy++) = b;
      *(pDummy++) = g;
      *(pDummy++) = r;
      *(pDummy++) = a;
    }
  }
  fclose(infile);
  (void) jpeg_finish_decompress(&cinfo);
  jpeg_destroy_decompress(&cinfo);

  data->BMap = (int*)pTest;
  data->Height = height;
  data->Width = width;
  data->Depth = 32;
}


int main()
{
	struct jpgData jpg_dat={0};
	loadJpg("lena.jpg",&jpg_dat);
	printf("W:%d  H:%d D:%d \n",jpg_dat.Width,jpg_dat.Height,jpg_dat.Depth);
	return 0;
}
