
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


#include <stack>


template<typename RB_Type>
class RingBuf {
protected:
    size_t whead_index = 0;  // Index for the write head; determines where new data is written.
    size_t tail_index = 0;   // Index for the tail; marks the position of the oldest data.


    RB_Type* buffer;      // Internal fixed-size array to store the ring buffer elements.
    size_t bufferL;    // Maximum capacity of the buffer.
public: 
    RingBuf(RB_Type *buffer, size_t size) : buffer(buffer), bufferL(size) {
        clear();
    }


    /**
     * @brief Calculates the number of elements currently stored in the buffer.
     *
     * This method computes the size based on the difference between the write head and tail index.
     * If the write head index is less than the tail index, it indicates that the write head has wrapped around,
     * and the calculation is adjusted by adding twice the size of the buffer (2*bufferL) to ensure a positive difference.
     *
     * @return size_t Current number of elements in the buffer.
     */
    size_t size() {
        int diff = whead_index - tail_index;
        if (diff < 0) diff += 2 * bufferL;
        return diff;
    }

    /**
     * @brief Returns the maximum capacity of the buffer.
     *
     * The capacity is predefined by the bufferL template parameter and does not change.
     *
     * @return size_t The total capacity of the buffer.
     */
    size_t capacity() {
        return bufferL;
    }

    /**
     * @brief Computes the remaining space in the buffer for additional elements.
     *
     * This is derived by subtracting the current size of the buffer from its total capacity.
     *
     * @return size_t Available space in the buffer for new elements.
     */
    size_t space() {
        return bufferL - size();
    }

    /**
     * @brief Resets the buffer to an empty state.
     *
     * This method sets both the write head and tail indices to zero, effectively clearing the buffer.
     */
    void clear() {
        whead_index = tail_index = 0;
    }

    /**
     * @brief Retrieves a pointer to the buffer element at the specified index from the write head.
     *
     * This method allows accessing elements relative to the write head's current position. 
     * Index 0 refers to the next position to write (not yet in the queue),
     * Index 1 refers to the most recently written data,
     * Index 2 refers to the second most recent data, and so on.
     * **Negative indices can be used to access future buffer positions.
     * If the requested index exceeds the size or the buffer is full at index 0, it returns NULL.
     *
     * @param idx The index from the write head to retrieve data from.
     * @return RB_Type* Pointer to the requested buffer element, or NULL if out of bounds or full.
     */
    RB_Type* getHead(int idx = 0) {
        if (idx > (int)size()) return NULL;
        // Allow negative indices to access future buffer positions
        // Return NULL if the requested negative index is beyond available space
        if(idx < 0 && (-idx >= space())) return NULL;
        if (idx == 0 && space() == 0) return NULL;

        int oidx = whead_index - idx;
        if (oidx < 0) oidx += 2 * bufferL;
        return &buffer[oidx % bufferL];
    }

    /**
     * @brief Pushes the current head of the buffer to the queue by advancing the write head index.
     *
     * This method should be called after modifying the data at the head (index 0) to add it to the buffer.
     * It increments the write head index, wrapping around using modulus operation with 2*bufferL if necessary.
     *
     * @return RB_Type* Pointer to the data that was just pushed to the buffer, or NULL if the buffer was full.
     */
    RB_Type* pushHead(const RB_Type *assign_data=NULL) {
        RB_Type* dat = getHead(0);
        if (dat == NULL) return NULL;
        if(assign_data)
          *dat = *assign_data;
        whead_index = (whead_index + 1) % (2 * bufferL);
        return dat;
    }

    RB_Type* pushHead(RB_Type data) {
        return pushHead(&data);
    }
    /**
     * @brief Retrieves a pointer to the buffer element at the specified index from the tail.
     *
     * This method allows accessing elements relative to the tail's current position.
     * Index 0 refers to the oldest data in the buffer, and Index 1 to the second oldest, and so on.
     * If the buffer is empty, it returns NULL.
     *
     * @param idx The index from the tail to retrieve data from.
     * @return RB_Type* Pointer to the requested buffer element, or NULL if the buffer is empty.
     */
    RB_Type* getTail(int idx = 0) {
        if (size() == 0) return NULL;
        
        int oidx = tail_index + idx;
        return &buffer[oidx % bufferL];
    }

    /**
     * @brief Removes the oldest element from the buffer by advancing the tail index.
     *
     * This method should be called after processing or consuming the oldest data in the buffer.
     * It increments the tail index, wrapping around using modulus operation with 2*bufferL if necessary.
     *
     * @return bool True if the oldest element was successfully removed, false if the buffer was empty.
     */
    bool consumeTail() {
        if (size() == 0) return false;
        
        tail_index = (tail_index + 1) % (2 * bufferL);
        return true;
    }
};


template <
  typename RB_Type
>
class RingBuf_ExternalBuffer:public RingBuf<RB_Type>
{
  public:
  RingBuf_ExternalBuffer():RingBuf<RB_Type>(NULL,0)
  {

  }

  void setBuffer(RB_Type *buffer, size_t size)
  {
    RingBuf<RB_Type>::buffer=buffer;
    RingBuf<RB_Type>::bufferL=size;
    RingBuf<RB_Type>::clear();
  }
};




template <
  typename RB_Type, unsigned N
>
class RingBuf_Static:public RingBuf<RB_Type>
{
  protected:
  RB_Type sbuffer[N];
  public:
  RingBuf_Static():RingBuf<RB_Type>(sbuffer,N)
  {

  }
};



template <typename RP_Type, typename RP_Idx_Type = uint32_t>
class ResourcePool_stack {
public:
    struct ResourceData {
        ResourceData() : occupied(false) {}
        ~ResourceData() {}
        bool occupied;
        RP_Type data;
    };

protected:
    ResourceData *buff;
    RP_Idx_Type buffL;
    int _size;
    std::stack<RP_Idx_Type> availableIndices;

public:
    ResourcePool_stack(ResourceData *buff, RP_Idx_Type len) : buff(buff), buffL(len), _size(len) {
        RESET_occupation();
    }

    void RESET_occupation() {
        while (!availableIndices.empty()) {
            availableIndices.pop();
        }
        for (RP_Idx_Type i = 0; i < buffL; ++i) {
            buff[i].occupied = false;
            availableIndices.push(i);
        }
    }

    RP_Type* applyResource() {
        if (availableIndices.empty()) {
            return nullptr; // No available resource
        }
        RP_Idx_Type index = availableIndices.top();
        availableIndices.pop();
        buff[index].occupied = true;
        --_size;
        return &(buff[index].data);
    }

    bool returnResource(RP_Type *res) {
        // Calculate index based on pointer offset
        RP_Idx_Type index = res - &(buff[0].data);

        // Check if the index is within bounds and the resource is occupied
        if (index >= 0 && index < buffL && buff[index].occupied) {
            buff[index].occupied = false;
            availableIndices.push(index);
            ++_size;
            return true;
        }
        return false; // Resource not found or already not occupied
    }

    int size() const {
        return _size;
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


  void RESET_occupation() {
      for (RP_Idx_Type i = 0; i < buffL; ++i) {
          buff[i].occupied = false;
      }
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
