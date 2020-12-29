
#include <SSEUTIL.hpp>
#include <xmmintrin.h>
#include <pmmintrin.h>
#include <immintrin.h>

#include <stdlib.h>
#include <stdio.h>
#include <memory.h>
#include <time.h>





void *aligned_malloc(size_t required_bytes, size_t alignment) {
  void *p1;
  void **p2;
  int offset=alignment-1+sizeof(void*);
  p1 = malloc(required_bytes + offset);               // the line you are missing
  p2=(void**)(((size_t)(p1)+offset)&~(alignment-1));  //line 5
  p2[-1]=p1; //line 6
  return p2;
}

void aligned_free( void* p ) {
    void* p1 = ((void**)p)[-1];         // get the pointer to the buffer we allocated
    free( p1 );
}




void _xx_algo_array( int16_t * dst, int16_t * src1, int16_t const * src2, size_t n )
{
  for(int i=0;i<n;i++)
  {
    int32_t m=(int32_t)src1[i]*src2[i];
    src1[i]=m>>(15+1);
    
    if((i&31)==31)
    {
      int ii=i-31;
      for(int j=0;j<16;j++)
      {
        src1[ii+j]=(src1[ii+j*2]+src1[ii+j*2+1]);
      }
    }

    // printf("src1[%d]:%d, dst[%d]:%d\n",i,src1[i],i/2,dst[i/2]);
  }
   
  // for(int i=0;i<n/2;i++)
  // {
  //   dst[i]=(src1[i*2]+src1[i*2+1]);
  // }

}
void _mm_algo_array( int16_t * dst, int16_t * src1, int16_t const * src2, size_t n )
{
  __m128i half = _mm_set_epi16(INT16_MAX/2,INT16_MAX/2,INT16_MAX/2,INT16_MAX/2,INT16_MAX/2,INT16_MAX/2,INT16_MAX/2,INT16_MAX/2);
  int count=n/8;
  int16_t * src1_BK=src1;
  for(  int i=0; i<count; i++,src1+=8,src2+=8)
  {
    *(__m128i*) src1 = _mm_mulhrs_epi16(*(__m128i*) src1, *(__m128i*) src2 );
    *(__m128i*) src1 = _mm_mulhrs_epi16(*(__m128i*) src1, half );

    if(i&1==1)
    { 
      // __m128i res = _mm_hadds_epi16(*(__m128i*) (src1-8), *(__m128i*) src1 );
      // _mm_stream_si128((__m128i*) dst,res);
      // // *(__m128i*) dst = _mm_hadds_epi16(*(__m128i*) (src1-8), *(__m128i*) src1 );
      // dst+=8;

      *(__m128i*) (src1-8) = _mm_hadds_epi16(*(__m128i*) (src1-8), *(__m128i*) src1 );
    }
    if(i&0xFFF==0)
    {
      _mm_prefetch(src1+0xFFF, _MM_HINT_NTA);
      _mm_prefetch(src2+0xFFF, _MM_HINT_NTA);
    }
    // mulhrs
  }
  // src1=src1_BK;
  // for(  int i=0; i<count/2;i++,dst+=8,src1+=16)
  // {
  //   *(__m128i*) dst = _mm_hadds_epi16(*(__m128i*) (src1), *(__m128i*) (src1+8) );
  // }
}
void _mm_algo_addS( int8_t * dst,int8_t *src,int n)
{
  int ptrAdv=(sizeof(__m128i)/sizeof(int8_t));
  
  for( int8_t const * end( dst + n ); dst != end; dst+=ptrAdv,src+=ptrAdv)
  {
    *(__m128i*) dst = _mm_add_epi8(*(__m128i*) dst, *(__m128i*) src );
  }
}
void _mm_algo_addSeq( int8_t * dst,int8_t ** linMems,int n, size_t count)
{
  memcpy(dst,linMems[0],n);
  for(int i=1; i<count; i++)
  {
    _mm_algo_addS( dst,linMems[i], n);
  }
}



void _mm_algo_addTree( int8_t * dst,int8_t ** linMems,int n, size_t count)
{
  if(count<=1)return;
  int halfCount=count/2;
  _mm_algo_addTree(linMems[0],linMems, n,halfCount);

  _mm_algo_addTree(linMems[halfCount],linMems+halfCount,n, count-halfCount);
  
  _mm_algo_addS( linMems[0],linMems[halfCount],n);
}
void _mm_algo_( int16_t * dst, int16_t * src1, size_t width, size_t height, int16_t * src2,size_t t_width, size_t t_height)
{

  
  int INT16X8SET=640*480/4*2/8;//for 500M pixel

  int bufferW=width;
  int bufferH=t_height+1;

  int16_t *BufferSpace=(int16_t*)aligned_malloc(sizeof(int16_t)*8*(bufferW*bufferH*2/8), 16);


  __m128i half = _mm_set_epi16(INT16_MAX/2,INT16_MAX/2,INT16_MAX/2,INT16_MAX/2,INT16_MAX/2,INT16_MAX/2,INT16_MAX/2,INT16_MAX/2);
}


void _xx_mul_array( int16_t * dst, int16_t const * src1, int16_t const * src2, size_t n )
{
  for(int i=0;i<n;i++)
  {
    // dst[i]=src1[0]+src2[0];
    int32_t m=(int32_t)src1[i]*src2[i];
    dst[i]=m>>15;
  }
}
void _mm_mul_array( int16_t * dst, int16_t const * src1, int16_t const * src2, size_t n )
{
  for( int16_t const * end( dst + n ); dst != end; dst+=8,src1+=8,src2+=8)
  {
    // *(__m128i*) dst = _mm_add_epi16( *(__m128i*) src1, *(__m128i*) src2 );
    *(__m128i*) dst = _mm_mulhrs_epi16(*(__m128i*) src1, *(__m128i*) src2 );
    // mulhrs
  }
}



void _xx_add_array( int16_t * dst, int16_t const * src1, int16_t const * src2, size_t n )
{
  for(int i=0;i<n;i++)
  {
    dst[i]=src1[i]+src2[i];
  }
}
void _mm_add_array( int16_t * dst, int16_t const * src1, int16_t const * src2, size_t n )
{
  for( int16_t const * end( dst + n ); dst != end; dst+=8,src1+=8,src2+=8)
  {
    *(__m128i*) dst = _mm_add_epi16( *(__m128i*) src1, *(__m128i*) src2 );
  }
}


void _xx_addin_array( int16_t * dst, int16_t const * src, size_t n )
{
  for(int i=0;i<n;i++)
  {
    dst[i]+=src[i];
  }
}
void _mm_addin_array( int16_t * dst, int16_t const * src, size_t n )
{
  for( int16_t const * end( dst + n ); dst != end; dst+=8,src+=8)
  {
    *(__m128i*) dst = _mm_add_epi16( *(__m128i*) dst, *(__m128i*) src );
  }
}

void SSE_Test()
{
  {
  
    // int INT16X8SET=5000000*2/8;//for 500M pixel
    int INT16X8SET=1000000/4*2/8;//for 500M pixel

    int loopTimes=360;//100000000/INT16X8SET;
    int16_t *input1=(int16_t*)aligned_malloc(sizeof(int16_t)*8*INT16X8SET, 16);
    int16_t *input2=(int16_t*)aligned_malloc(sizeof(int16_t)*8*INT16X8SET, 16);
    int16_t *output=(int16_t*)aligned_malloc(sizeof(int16_t)*8*INT16X8SET, 16);

    
    printf("input1_ptr:%p input2_ptr:%p output_ptr:%p\n",
          input1,input2,output);

    if(0)
    {
    
      input1[0]=input1[2]=INT16_MAX;
      input2[0]=input2[2]=INT16_MAX;
      input1[1]=input1[3]=INT16_MAX*5/10;
      input2[1]=input2[3]=INT16_MAX*5/10;
      output[0]=3;


      clock_t t = clock();
      for(int i=0;i<loopTimes;i++)
        _xx_algo_array(output, input1,input2,8*INT16X8SET);
      printf("C   :%fms \n", (double)(clock() - t) / CLOCKS_PER_SEC * 1000);
      
      printf("%d %d %d %d\n",
            input1[0], input1[1], input1[2], input1[3]);
    }

    if(0)
    {
      input1[0]=input1[2]=INT16_MAX;
      input2[0]=input2[2]=INT16_MAX;
      input1[1]=input1[3]=INT16_MAX*5/10;
      input2[1]=input2[3]=INT16_MAX*5/10;
      output[0]=3;
      clock_t t = clock();
      for(int i=0;i<loopTimes;i++)
        _mm_algo_array(output, input1,input2,8*INT16X8SET);
      // _mm_add_array(output, input1,8*INT16X8SET);

      printf("SSE :%fms \n", (double)(clock() - t) / CLOCKS_PER_SEC * 1000);

      printf("%d %d %d %d\n",
            input1[0], input1[1], input1[2], input1[3]);
    }

    aligned_free( input1 );
    aligned_free( input2 );
    aligned_free( output );
  }

  {
    int ddddd=128/8;
    int imgWidth=640;
    int imgHeight=480;
    int totalPixSize=imgWidth*imgHeight;
    int pixSize=totalPixSize;//for 500M pixel
    int baseUnit=sizeof(__m128)/sizeof(int8_t);
    pixSize=(pixSize/baseUnit)*baseUnit;

    int memCount=8;

    int8_t *inputs=(int8_t*)aligned_malloc(pixSize*memCount, 16);
    int8_t *output=(int8_t*)aligned_malloc(pixSize, 16);


    int tempFeatureCount=64;
    int8_t**inputArr=new int8_t*[tempFeatureCount];
    int tempHeight=64;
    int availPixSize=(imgWidth-tempHeight)*(imgHeight-tempHeight-1);


    for(int i=0;i<tempFeatureCount;i++)
    {
      inputArr[i]=inputs+(i/(tempFeatureCount/memCount))*pixSize;//+(rand()%(tempHeight*imgWidth));
    }

    int loopTimes=360;

    {
      clock_t t = clock();
      for(int i=0;i<loopTimes;i++)
        _mm_algo_addSeq(output,inputArr,availPixSize/ddddd*ddddd,tempFeatureCount);
      printf("_mm_algo_addSeq   :%fms \n", (double)(clock() - t) / CLOCKS_PER_SEC * 1000);
    }


    {
      clock_t t = clock();
      for(int i=0;i<loopTimes;i++)
        _mm_algo_addTree(output,inputArr,availPixSize,tempFeatureCount);
      printf("_mm_algo_addTree  :%fms \n", (double)(clock() - t) / CLOCKS_PER_SEC * 1000);
    }




    delete inputArr;
    aligned_free( inputs );
    aligned_free( output );
  }
  //_mm_load_ps

  if(0)
  {

    float input1[4] _ATTR_ALIGN16_= { 1.2f, 3.5f, 1.7f, 2.8f };
    float input2[4] _ATTR_ALIGN16_= { -0.7f, 2.6f, 3.3f, -0.8f };
    float output[1000*1000]_ATTR_ALIGN16_;
    __m128 a = _mm_load_ps(input1);
    __m128 b = _mm_load_ps(input2);
    __m128 t = _mm_add_ps(a, b);
    _mm_store_ps(output, t);
    _mm_store_ps(output+5, t);
    printf("%f %f %f %f\n",
          output[0], output[1], output[2], output[3]);

    
  }

  return ;
}
