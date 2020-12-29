#ifndef ___sadasd__SSEUTIL_HPP
#define ___sadasd__SSEUTIL_HPP
#include <stddef.h>


#define _ATTR_ALIGN16_ __attribute__ ((__aligned__ (16)))

void *aligned_malloc(size_t required_bytes, size_t alignment);

void aligned_free( void* p );

void SSE_Test();
#endif