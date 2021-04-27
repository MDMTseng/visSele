
#ifndef XXXX_M_JPEG_LIB_H
#define XXXX_M_JPEG_LIB_H

#include <inttypes.h>
#include <stddef.h>





int mjpecLib_enc(const char* filename, uint8_t *img, uint16_t w, uint16_t h, int q);

// typedef void (*encoder_buffer_callback)(const uint8_t *rawbuffer,int size, void*cb_param);

// int mjpecLib_enc(const uint8_t *img, uint16_t w, uint16_t h, int q,encoder_buffer_callback enc_cb, void*cb_param);



typedef uint8_t* (*decoder_buffer_request_callback)(int W,int H,int channel,void* cb_param);

int jpecLib_dec(const char * filename ,decoder_buffer_request_callback cb,void* cb_param);


#endif