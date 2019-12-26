/*
 * Ring Buffer Library for Arduino
 *
 * Copyright Jean-Luc Béchennec 2018
 *
 * This software is distributed under the GNU Public Licence v2 (GPLv2)
 *
 * Please read the LICENCE file
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

 /*
  * Note about interrupt safe implementation
  *
  * To be safe from interrupts, a sequence of C instructions must be framed
  * by a pair of interrupt disable and enable instructions and ensure that the
  * compiler will not move writing of variables to memory outside the protected
  * area. This is called a critical section. Usually the manipulated variables
  * receive the volatile qualifier so that any changes are immediately written
  * to memory. Here the approach is different. First of all you have to know
  * that volatile is useless if the variables are updated in a function and
  * that this function is called within the critical section. Indeed, the
  * semantics of the C language require that the variables in memory be updated
  * before returning from the function. But beware of function inlining because
  * the compiler may decide to delete a function call in favor of simply
  * inserting its code in the caller. To force the compiler to use a real
  * function call, __attribute__((noinline)) have been added to the push and
  * pop functions. In this way the lockedPush and lockedPop functions ensure
  * that in the critical section a push and pop function call respectively will
  * be used by the compiler. This ensures that, because of the function call,
  * the variables are written to memory in the critical section and also
  * ensures that, despite the reorganization of the instructions due to
  * optimizations, the critical section will be well opened and closed at the
  * right place because function calls, due to potential side effects, are not
  * subject to such reorganizations.
  */

#ifndef __RingBufX_H__
#define __RingBufX_H__

#include <Arduino.h>

/*
 * Set the integer size used to store the size of the buffer according of
 * the size given in the template instanciation. Thanks to Niklas Gürtler
 * to share his knowledge of C++ template meta programming.
 * https://niklas-guertler.de/
 *
 * If Index argument is true, the ring buffer has a size and an index
 * stored in an uint8_t (Type below) because its size is within [1,255].
 * Intermediate computation may need an uint16_t (BiggerType below).
 * If Index argument is false, the ring buffer has a size and an index
 * stored in an uint16_t (Type below) because its size is within [256,65535].
 * Intermediate computation may need an uint32_t (BiggerType below).
 */

template <typename RB_Idx_Type=uint32_t>
class RingBufIdxCounter
{
  RB_Idx_Type headIdx;
  RB_Idx_Type tailIdx;
  RB_Idx_Type dataSize;
  RB_Idx_Type RBLen;

  public:
  
  RingBufIdxCounter()
  {
    RESET(0);
  }
  RingBufIdxCounter(RB_Idx_Type len)
  {
    RESET(len);
  }

  void RESET(RB_Idx_Type len)
  {
    RBLen=len;
    headIdx=0;
    tailIdx=0;
    dataSize=0;
  }

  void clear(){headIdx=0;tailIdx=0;dataSize=0;}
  RB_Idx_Type getHead(){return headIdx;}
  RB_Idx_Type getTail(){return tailIdx;}
  RB_Idx_Type getTail(RB_Idx_Type idx){
    int real_idx=getTail()+idx;
    if(real_idx>=RBLen)real_idx-=RBLen;
    return real_idx;
  }
  RB_Idx_Type size(){return dataSize;}
  RB_Idx_Type space(){return RBLen-dataSize;}
  
  RB_Idx_Type getNextHead(){
    RB_Idx_Type bk_headIdx=headIdx;
    if(bk_headIdx==RBLen-1)bk_headIdx=0;
    else bk_headIdx++;
    return bk_headIdx;
  }
  
  
  int consumeTail(){
    if(dataSize==0)return -1;//Tail pass the head... ERROR
    if(tailIdx==RBLen-1)tailIdx=0;
    else tailIdx++;
    dataSize--;
    return 0;
  }

  int pushHead(){
    if(dataSize==RBLen)//queue is full.... ERROR
    {
      return -1;
    }
    if(headIdx==RBLen-1)headIdx=0;
    else headIdx++;
    
    
    dataSize++;
    return 0;
  }

  
  int pullHead(){
    if(dataSize==0)//queue is full.... ERROR
    {
      return -1;
    }
    if(headIdx==0)headIdx=RBLen-1;
    else headIdx--;
    
    
    dataSize--;
    return 0;
  }
};

template <
  typename RB_Type,typename RB_Idx_Type=uint32_t
>
class RingBuf
{
  public:
  RingBufIdxCounter<RB_Idx_Type> RBC;
  RB_Type *buff;
  RingBuf(RB_Type *buff,RB_Idx_Type len)
  {
    this->buff=buff;
    RBC.RESET(len);
  }

  RB_Idx_Type size()
  {
    return RBC.size();
  }
  
  void clear()
  {
      RBC.clear();
  }


  RB_Idx_Type getHead_Idx()
  {
    return RBC.getHead();
  }
  RB_Idx_Type getTail_Idx()
  {
    return RBC.getTail();
  }
  
  RB_Type* getHead()
  {
    if(RBC.space()==0)return NULL;
    return buff+RBC.getHead();
  }

  RB_Idx_Type getTail_Idx(uint32_t idx)
  {
    return RBC.getTail(idx);
  }
  RB_Type* getTail(uint32_t idx)
  {
    if(idx>=RBC.size())return NULL;
    return buff+RBC.getTail(idx);
  }

  RB_Type* getTail()
  {
    if(RBC.size()==0)return NULL;
    return buff+RBC.getTail();
  }
  
  
  int pushHead()
  {
    return RBC.pushHead();
  }
  
  int pullHead()
  {
    return RBC.pullHead();
  }

  int consumeTail()
  {
    return RBC.consumeTail();
  }
};

#endif /* __RingBufX_H__ */
