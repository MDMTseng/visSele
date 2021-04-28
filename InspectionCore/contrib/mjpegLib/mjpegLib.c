#include "mjpegLib.h"


#include <stdio.h>
#include "jpeglib.h"



// int mjpecLib_enc(const uint8_t *img, uint16_t w, uint16_t h, int q,encoder_buffer_callback enc_cb, void*cb_param)

int mjpecLib_enc(const char* filename, uint8_t *img, uint16_t w, uint16_t h, int q)
{
  // jpec_enc_t *enc = jpec_enc_new2(img, w, h, q);
  // int ret_codeLen;
  // const uint8_t * code= jpec_enc_run(enc, &ret_codeLen);
  // enc_cb(code,ret_codeLen,cb_param);
  // jpec_enc_del(enc);
  /* This struct contains the JPEG compression parameters and pointers to
   * working space (which is allocated as needed by the JPEG library).
   * It is possible to have several such structures, representing multiple
   * compression/decompression processes, in existence at once.  We refer
   * to any one struct (and its associated working data) as a "JPEG object".
   */
  struct jpeg_compress_struct cinfo;
  /* This struct represents a JPEG error handler.  It is declared separately
   * because applications often want to supply a specialized error handler
   * (see the second half of this file for an example).  But here we just
   * take the easy way out and use the standard error handler, which will
   * print a message on stderr and call exit() if compression fails.
   * Note that this struct must live as long as the main JPEG parameter
   * struct, to avoid dangling-pointer problems.
   */
  struct jpeg_error_mgr jerr;
  /* More stuff */
  FILE * outfile;		/* target file */
  JSAMPROW row_pointer[1];	/* pointer to JSAMPLE row[s] */
  int row_stride;		/* physical row width in image buffer */

  /* Step 1: allocate and initialize JPEG compression object */

  /* We have to set up the error handler first, in case the initialization
   * step fails.  (Unlikely, but it could happen if you are out of memory.)
   * This routine fills in the contents of struct jerr, and returns jerr's
   * address which we place into the link field in cinfo.
   */
  cinfo.err = jpeg_std_error(&jerr);
  /* Now we can initialize the JPEG compression object. */
  jpeg_create_compress(&cinfo);

  /* Step 2: specify data destination (eg, a file) */
  /* Note: steps 2 and 3 can be done in either order. */

  /* Here we use the library-supplied code to send compressed data to a
   * stdio stream.  You can also write your own code to do something else.
   * VERY IMPORTANT: use "b" option to fopen() if you are on a machine that
   * requires it in order to write binary files.
   */
  if ((outfile = fopen(filename, "wb")) == NULL) {
    fprintf(stderr, "can't open %s\n", filename);
    return -1;
  }
  jpeg_stdio_dest(&cinfo, outfile);

  /* Step 3: set parameters for compression */

  /* First we supply a description of the input image.
   * Four fields of the cinfo struct must be filled in:
   */
  cinfo.image_width = w; 	/* image width and height, in pixels */
  cinfo.image_height = h;
  cinfo.input_components = 3;		/* # of color components per pixel */
  cinfo.in_color_space = JCS_RGB; 	/* colorspace of input image */
  /* Now use the library's routine to set default compression parameters.
   * (You must set at least cinfo.in_color_space before calling this,
   * since the defaults depend on the source color space.)
   */
  jpeg_set_defaults(&cinfo);
  /* Now you can set any non-default parameters you wish to.
   * Here we just illustrate the use of quality (quantization table) scaling:
   */
  jpeg_set_quality(&cinfo, q, TRUE /* limit to baseline-JPEG values */);

  /* Step 4: Start compressor */

  /* TRUE ensures that we will write a complete interchange-JPEG file.
   * Pass TRUE unless you are very sure of what you're doing.
   */
  jpeg_start_compress(&cinfo, TRUE);

  /* Step 5: while (scan lines remain to be written) */
  /*           jpeg_write_scanlines(...); */

  /* Here we use the library's state variable cinfo.next_scanline as the
   * loop counter, so that we don't have to keep track ourselves.
   * To keep things simple, we pass one scanline per call; you can pass
   * more if you wish, though.
   */
  row_stride = w * 3;	/* JSAMPLEs per row in image_buffer */

  while (cinfo.next_scanline < cinfo.image_height) {
    /* jpeg_write_scanlines expects an array of pointers to scanlines.
     * Here the array is only one element long, but you could pass
     * more than one scanline at a time if that's more convenient.
     */
    row_pointer[0] = (unsigned char*)& img[cinfo.next_scanline * row_stride];
    (void) jpeg_write_scanlines(&cinfo, row_pointer, 1);
  }

  /* Step 6: Finish compression */

  jpeg_finish_compress(&cinfo);
  /* After finish_compress, we can close the output file. */
  fclose(outfile);

  /* Step 7: release JPEG compression object */

  /* This is an important step since it will release a good deal of memory. */
  jpeg_destroy_compress(&cinfo);

  /* And we're done! */
  return 0;
}


int mjpecLib_enc(uint8_t *img, uint16_t w, uint16_t h, int q, uint8_t **ret_buff, unsigned long *ret_buffLen)

// std::pair<unsigned char*,unsigned long> CompressJPEG(
{
  if(ret_buff==NULL && ret_buffLen==NULL)
  {
    return -1;
  }
  // setup JPEG compression structure data
  jpeg_compress_struct  jcInfo;
  jpeg_error_mgr      jErr;  // JPEG error handler
  jcInfo.err        = jpeg_std_error ( &jErr );
  
  // initialize JPEG compression object
  jpeg_create_compress( &jcInfo );
  
  // specify data destination is memory
  jpeg_mem_dest( &jcInfo, ret_buff,ret_buffLen);
  
  // image format
  jcInfo.image_width    = w;
  jcInfo.image_height    = h;
  jcInfo.input_components  = 3;
  jcInfo.in_color_space  = JCS_RGB;
  
  // set default compression parameters
  jpeg_set_defaults( &jcInfo );
   
  // set image quality
  jpeg_set_quality( &jcInfo, q, TRUE );
  
  // start compressor
  jpeg_start_compress( &jcInfo, TRUE );
  int  iRowStride = jcInfo.image_width * jcInfo.input_components;
  while( jcInfo.next_scanline < jcInfo.image_height )  {
    JSAMPROW pData = &( img[ jcInfo.next_scanline * iRowStride ] );
    jpeg_write_scanlines( &jcInfo, &pData, 1 );
  }
  
  // finish compression
  jpeg_finish_compress( &jcInfo );
  
  // release JPEG compression object
  jpeg_destroy_compress( &jcInfo );
  
  return 0;
}


/*
 * Sample routine for JPEG decompression.  We assume that the source file name
 * is passed in.  We want to return 1 on success, 0 on error.
 */


int jpecLib_dec(const char * filename ,decoder_buffer_request_callback cb,void* cb_param)
//================================
{
  unsigned int type;  
  unsigned char * rowptr[1];    // pointer to an array
  unsigned char * jdata;        // data for the image
  struct jpeg_decompress_struct info; //for our jpeg info
  struct jpeg_error_mgr err;          //the error handler

  FILE* file = fopen(filename, "rb");  //open the file

  info.err = jpeg_std_error(& err);     
  jpeg_create_decompress(& info);   //fills info structure

  //if the jpeg file doesn't load
  if(!file) {
    //  fprintf(stderr, "Error reading JPEG file %s!", FileName);
     return -1;
  }

  jpeg_stdio_src(&info, file);    
  jpeg_read_header(&info, TRUE);   // read jpeg file header

  jpeg_start_decompress(&info);    // decompress the file

  jdata = cb(info.output_width,info.output_height,info.num_components,cb_param);
  //--------------------------------------------
  // read scanlines one at a time & put bytes 
  //    in jdata[] array. Assumes an RGB image
  //-------------------------------------------
  while (info.output_scanline < info.output_height) // loop
  {
    // Enable jpeg_read_scanlines() to fill our jdata array
    rowptr[0] = (unsigned char *)jdata +  // secret to method
            3* info.output_width * info.output_scanline; 

    jpeg_read_scanlines(&info, rowptr, 1);
  }
  //---------------------------------------------------

  jpeg_finish_decompress(&info);   //finish decompressing

  jpeg_destroy_decompress(&info);
  fclose(file);                    //close the file
  return 0;    // for OpenGL tex maps
}

/*
int mjpecLib_enc(const uint8_t *img, uint16_t w, uint16_t h, int q,encoder_buffer_callback enc_cb, void*cb_param)
{
  jpec_enc_t *enc = jpec_enc_new2(img, w, h, q);
  int ret_codeLen;
  const uint8_t * code= jpec_enc_run(enc, &ret_codeLen);
  enc_cb(code,ret_codeLen,cb_param);
  jpec_enc_del(enc);
  return 0;
}


/*
uint8_t *jpecLib_dec(const uint8_t *code, size_t codeLen ,uint16_t *ret_w, uint16_t *ret_h)
{




  pjpeg_image_info_t pInfo;
  
  unsigned char ret = pjpeg_decode_init(
    pjpeg_image_info_t *pInfo, 
    pjpeg_need_bytes_callback_t pNeed_bytes_callback, 
    void *pCallback_data, 
    unsigned char reduce);

  unsigned char ret2=pjpeg_decode_mcu(void);

  return NULL;
}

#ifndef max
#define max(a,b)    (((a) > (b)) ? (a) : (b))
#endif
#ifndef min
#define min(a,b)    (((a) < (b)) ? (a) : (b))
#endif
 
struct ____SRC__DATA{
  uint8_t *dataBuffer;
  size_t len;
  int dataIdx;
};
static unsigned char pjpeg_need_bytes_callback(unsigned char* pBuf, unsigned char buf_size, unsigned char *pBytes_actually_read, void *pCallback_data)
{
   uint n;
   struct ____SRC__DATA* src_data =(struct sdsfsdfkdf*)pCallback_data;
   
   int src_data_restLen=src_data->len-src_data->dataIdx;
   n = min(src_data_restLen, buf_size);
   memcpy(pBuf,data->dataBuffer+data->dataIdx,n);
   data->dataIdx+=n;
   *pBytes_actually_read = (unsigned char)(n);
   return 0;
}
//------------------------------------------------------------------------------
// Loads JPEG image from specified file. Returns NULL on failure.
// On success, the malloc()'d image's width/height is written to *x and *y, and
// the number of components (1 or 3) is written to *comps.
// pScan_type can be NULL, if not it'll be set to the image's pjpeg_scan_type_t.
// Not thread safe.
// If reduce is non-zero, the image will be more quickly decoded at approximately
// 1/8 resolution (the actual returned resolution will depend on the JPEG 
// subsampling factor).
uint8 *pjpeg_load_from_file( uint8_t *src_raw,size_t src_raw_Len, int *x, int *y, int *comps, pjpeg_scan_type_t *pScan_type, int reduce)
{
   pjpeg_image_info_t image_info;
   int mcu_x = 0;
   int mcu_y = 0;
   uint row_pitch;
   uint8 *pImage;
   uint8 status;
   uint decoded_width, decoded_height;
   uint row_blocks_per_mcu, col_blocks_per_mcu;

   *x = 0;
   *y = 0;
   *comps = 0;
   if (pScan_type) *pScan_type = PJPG_GRAYSCALE;

   struct ____SRC__DATA src_data;
   src_data.dataIdx=0;
   src_data.dataBuffer=src_raw;
   src_data.len=src_raw_Len;

   status = pjpeg_decode_init(&image_info, pjpeg_need_bytes_callback, &src_data, (unsigned char)reduce);
         
   if (status)
   {
      printf("pjpeg_decode_init() failed with status %u\n", status);
      if (status == PJPG_UNSUPPORTED_MODE)
      {
         printf("Progressive JPEG files are not supported.\n");
      }
      return NULL;
   }
   
   if (pScan_type)
      *pScan_type = image_info.m_scanType;

   // In reduce mode output 1 pixel per 8x8 block.
   decoded_width = reduce ? (image_info.m_MCUSPerRow * image_info.m_MCUWidth) / 8 : image_info.m_width;
   decoded_height = reduce ? (image_info.m_MCUSPerCol * image_info.m_MCUHeight) / 8 : image_info.m_height;

   row_pitch = decoded_width * image_info.m_comps;
   pImage = (uint8 *)malloc(row_pitch * decoded_height);
   if (!pImage)
   {
      return NULL;
   }

   row_blocks_per_mcu = image_info.m_MCUWidth >> 3;// div8 jpeg is 8x8pix block
   col_blocks_per_mcu = image_info.m_MCUHeight >> 3;
   
   int error=0;

   for ( ; ; )
   {
      int y, x;
      uint8 *pDst_row;

      status = pjpeg_decode_mcu();
      
      if (status)
      {
         if (status != PJPG_NO_MORE_BLOCKS)
         {
           error=1;
         }
        
         break;
      }

      if (mcu_y >= image_info.m_MCUSPerCol)
      {
         error=2;
         break;
      }

      if (reduce)
      {
         // In reduce mode, only the first pixel of each 8x8 block is valid.
         pDst_row = pImage + mcu_y * col_blocks_per_mcu * row_pitch + mcu_x * row_blocks_per_mcu * image_info.m_comps;
         if (image_info.m_scanType == PJPG_GRAYSCALE)
         {
            *pDst_row = image_info.m_pMCUBufR[0];
         }
         else
         {
            uint y, x;
            for (y = 0; y < col_blocks_per_mcu; y++)
            {
               uint src_ofs = (y * 128U);
               for (x = 0; x < row_blocks_per_mcu; x++)
               {
                  pDst_row[0] = image_info.m_pMCUBufR[src_ofs];
                  pDst_row[1] = image_info.m_pMCUBufG[src_ofs];
                  pDst_row[2] = image_info.m_pMCUBufB[src_ofs];
                  pDst_row += 3;
                  src_ofs += 64;
               }

               pDst_row += row_pitch - 3 * row_blocks_per_mcu;
            }
         }
      }
      else
      {
         // Copy MCU's pixel blocks into the destination bitmap.
         pDst_row = pImage + (mcu_y * image_info.m_MCUHeight) * row_pitch + (mcu_x * image_info.m_MCUWidth * image_info.m_comps);

         for (y = 0; y < image_info.m_MCUHeight; y += 8)
         {
            const int by_limit = min(8, image_info.m_height - (mcu_y * image_info.m_MCUHeight + y));

            for (x = 0; x < image_info.m_MCUWidth; x += 8)
            {
               uint8 *pDst_block = pDst_row + x * image_info.m_comps;

               // Compute source byte offset of the block in the decoder's MCU buffer.
               uint src_ofs = (x * 8U) + (y * 16U);
               const uint8 *pSrcR = image_info.m_pMCUBufR + src_ofs;
               const uint8 *pSrcG = image_info.m_pMCUBufG + src_ofs;
               const uint8 *pSrcB = image_info.m_pMCUBufB + src_ofs;

               const int bx_limit = min(8, image_info.m_width - (mcu_x * image_info.m_MCUWidth + x));

               if (image_info.m_scanType == PJPG_GRAYSCALE)
               {
                  int bx, by;
                  for (by = 0; by < by_limit; by++)
                  {
                     uint8 *pDst = pDst_block;

                     for (bx = 0; bx < bx_limit; bx++)
                        *pDst++ = *pSrcR++;

                     pSrcR += (8 - bx_limit);

                     pDst_block += row_pitch;
                  }
               }
               else
               {
                  int bx, by;
                  for (by = 0; by < by_limit; by++)
                  {
                     uint8 *pDst = pDst_block;

                     for (bx = 0; bx < bx_limit; bx++)
                     {
                        pDst[0] = *pSrcR++;
                        pDst[1] = *pSrcG++;
                        pDst[2] = *pSrcB++;
                        pDst += 3;
                     }

                     pSrcR += (8 - bx_limit);
                     pSrcG += (8 - bx_limit);
                     pSrcB += (8 - bx_limit);

                     pDst_block += row_pitch;
                  }
               }
            }

            pDst_row += (row_pitch * 8);
         }
      }

      mcu_x++;
      if (mcu_x == image_info.m_MCUSPerRow)
      {
         mcu_x = 0;
         mcu_y++;
      }
   }


   if(error!=0)
   {
     free(pImage);
     return NULL;
   }
   *x = decoded_width;
   *y = decoded_height;
   *comps = image_info.m_comps;

   return pImage;
}
*/