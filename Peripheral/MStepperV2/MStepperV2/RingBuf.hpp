
#ifndef __RingBufX_H__
#define __RingBufX_H__
#include <cstddef>
// #include <Arduino.h>

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
    int real_idx=tailIdx+idx;
    if(real_idx>=RBLen)real_idx-=RBLen;
    return real_idx;
  }


  RB_Idx_Type getHead(RB_Idx_Type idx){//allow negative idx
    int real_idx=headIdx-idx;
    if(real_idx>=RBLen)real_idx-=RBLen;
    if(real_idx<0)real_idx+=RBLen;
    return real_idx;
  }


  RB_Idx_Type size(){

    auto L=headIdx-tailIdx;
    if(L<0)L+=RBLen;
    return L;
    
  }
  RB_Idx_Type space(){return RBLen-size();}
  RB_Idx_Type capacity(){return RBLen;}
  
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
  RB_Type *new_buff;



  RingBuf(RB_Type *buff,RB_Idx_Type len)
  {
    RESET(buff,len);
  }
  
  RingBuf(RB_Idx_Type len=0)
  {
    new_buff=buff=NULL;
    RBC.RESET(len);
  }
  
  void RESET(RB_Type *buff,RB_Idx_Type len)
  {
    if(new_buff)
    {
      delete(new_buff);
    }
    new_buff=NULL;
    this->buff=buff;
    RBC.RESET(len);
  }

  void RESET(RB_Idx_Type len)
  {
    RB_Type *tmpBuff=(len==0)?NULL:new RB_Type[len];
    RESET(tmpBuff,len);
    new_buff=tmpBuff;
  }
  
  ~RingBuf()
  {
    if(new_buff)
    {
      delete(new_buff);
      new_buff=NULL;
    }
    buff=NULL;
  }

  RB_Idx_Type size()
  {
    return RBC.size();
  }
  RB_Idx_Type space()
  {
    return RBC.space();
  }
  RB_Idx_Type capacity()
  {
    return RBC.capacity();
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
  
  RB_Type* getHead()//the next empty block that's not in the Queue yet
  {
    if(RBC.space()==0)return NULL;
    return buff+RBC.getHead();
  }
  
  RB_Type* getHead(uint32_t idx)
  {
    // printf("@idx:%d RBC.size():%d\n",idx,RBC.size());
    if(idx>RBC.size())return NULL;//if(idx-1>=RBC.size())return NULL;
    // printf("-idx:%d\n",idx);
    int idxx=(int)RBC.getHead(idx);
    // printf("idxx:%d\n",idxx);
    return buff+idxx;
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
  
  
  int pushHead(RB_Type dat)
  {
    RB_Type* head=getHead();
    if(head==NULL)
    {
       return -1;
    }
    *head=dat;
    return RBC.pushHead();
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



template <
  typename RB_Type, unsigned N ,typename RB_Idx_Type=uint32_t
>
class RingBuf_Static:public RingBuf<RB_Type,RB_Idx_Type>
{

  public:
  RB_Type array[N];
  RingBuf_Static():RingBuf<RB_Type,RB_Idx_Type>(array,N)
  {

  }
};




template <
  typename RP_Type,typename RP_Idx_Type=uint32_t
>
class ResourcePool
{


public:

  struct ResourceData{
    ResourceData(){};
    ~ResourceData(){};
    bool occupied;
    RP_Type data;
  };
protected:
  
  ResourceData *buff;
  int buffL;

  int _size;

public:

  ResourcePool(struct ResourceData *buff,RP_Idx_Type len)
  {
    this->buff=buff;
    buffL=len;
    
    for(int i=0;i<buffL;i++)
    {
      buff[i].occupied=false;
    }
    _size=buffL;
  }

  RP_Type* applyResource()
  {
    for(int i=0;i<buffL;i++)
    {
      if(buff[i].occupied==false)
      {
        buff[i].occupied=true;
        _size--;
        return &(buff[i].data);
      }
    }
    return NULL;
  }


  bool returnResource(RP_Type *res)
  {
    
    for(int i=0;i<buffL;i++)
    {
      if(&(buff[i].data)==res)
      {
        buff[i].occupied=false;

        _size++;
        return true;
      }
    }
    return false;
  //   int addrDiff =(res-( &(buff[0].data) ));
  //   int idx=addrDiff/sizeof(ResourceData);
  //   if(idx<0)return false;
  //   if(idx>=buffL)return false;
  //   if(buff[idx].occupied==false)return false;

  //   buff[idx].occupied=false;
  //   return true;


  }

  int size()
  {
    return _size;
  }
  
};





// template <
//   typename RP_Type, unsigned N ,typename RP_Idx_Type=uint32_t
// >
// class ResourcePool_Static:public ResourcePool<RP_Type,RP_Idx_Type>
// {
//   protected:
//   ResourcePool_Static::ResourceData array[N];

//   public:
//   ResourcePool_Static():ResourcePool<RP_Type,RP_Idx_Type>(array,N)
//   {

//   }
// };



#endif /* __RingBufX_H__ */
