#include <iostream>
#include <array>
#include <stdexcept>
#include <thread>
#include <stack>


template<typename RB_Type>
class RingBuf {
protected:
    RB_Type* buffer;      // Internal fixed-size array to store the ring buffer elements.
    const size_t bufferL;    // Maximum capacity of the buffer.
public: 

    size_t whead_index = 0;  // Index for the write head; determines where new data is written.
    size_t tail_index = 0;   // Index for the tail; marks the position of the oldest data.


    RingBuf(RB_Type *buffer, size_t size) : buffer(buffer), bufferL(size) {
        RESET();
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
    void RESET() {
        whead_index = tail_index = 0;
    }

    /**
     * @brief Retrieves a pointer to the buffer element at the specified index from the write head.
     *
     * This method allows accessing elements relative to the write head's current position. 
     * Index 0 refers to the next position to write (not yet in the queue),
     * Index 1 refers to the most recently written data,
     * Index 2 refers to the second most recent data, and so on.
     * If the requested index exceeds the size or the buffer is full at index 0, it returns NULL.
     *
     * @param idx The index from the write head to retrieve data from.
     * @return RB_Type* Pointer to the requested buffer element, or NULL if out of bounds or full.
     */
    RB_Type* getHead(int idx = 0) {
        if (size() < idx) return NULL;
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
    RB_Type* pushHead() {
        RB_Type* dat = getHead(0);
        if (dat == NULL) return NULL;
        whead_index = (whead_index + 1) % (2 * bufferL);
        return dat;
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


const int N = 100;
RingBuf_Static<int, N> rb;


void producer() {
    int pushCount=N*2*3+3;
    int latestNumber=0;
    for (int i = 0;i<pushCount; ++i) {
        int *item =rb.getHead();
        if(item==NULL){
            std::cerr << "FULL    wait." << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
            i--;
            continue;
        }
        *item = i;
        latestNumber=*item;
        printf(">>>>> %d:%d<%d>%d:  \n",i,rb.whead_index,rb.size(),rb.tail_index);
        rb.pushHead();
        std::this_thread::sleep_for(std::chrono::milliseconds(std::rand() % 10));
    }
    printf("--------producer count:%d latestNum:%d end \n",pushCount,latestNumber);
}

void consumer() {
    int emptyTryCounter=0;
    for (int i = 0; ; ++i) {
        int *item =rb.getTail();
        if(item==NULL){
            // std::cerr << "Empty wait...." << std::endl;
            emptyTryCounter++;
            if(emptyTryCounter>20){
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            continue;
        }
        emptyTryCounter=0;
        printf("<<<<<<<<<<<<<<<<<<<<<<<< t:%d\n",*item);
        rb.consumeTail();
        std::this_thread::sleep_for(std::chrono::milliseconds(std::rand() % 11));
    }

    printf("--------consumer end at %d<%d>%d:  \n",rb.whead_index,rb.size(),rb.tail_index);
}



int main() {

    // Producer thread simulation
   
    std::thread producer_thread(producer);
    std::thread consumer_thread(consumer);

    producer_thread.join();
    consumer_thread.join();



    // for (int i = 0; i < 12; ++i) {
    //     int *item =rb.getHead();
    //     if(item==NULL){
    //         std::cerr << "Buffer is full" << std::endl;
    //         break;
    //     }
    //     *item = i;
    //     rb.pushHead();


    //     printf("pushing %d:%d<%d>%d:  ",i,rb.whead_index,rb.size(),rb.tail_index);
    //     for (int j = 0; j < rb.size(); j++)
    //     {
    //         int *item =rb.getTail(j);
    //         if(item==NULL){
    //             printf("\nBuffer ends at %d\n",j);
    //             break;
    //         }
    //         printf("%d,",*item);
    //     }
    //     printf("\n");

    //     while((std::rand() % 100)>40)
    //     {
    //         rb.consumeTail();
    //         printf("Consume....\n");

    //     }
    // }

    // printf("==============\n");

    // for (int i = 0; i < 12; ++i) {
    //     int *item =rb.getTail();
    //     if(item==NULL){
    //         std::cerr << "Buffer is empty" << std::endl;
    //         break;
    //     }


    //     printf("pushing %d:%d<%d>%d:  ",i,rb.whead_index,rb.size(),rb.tail_index);
    //     for (int j = 0; j < rb.size(); j++)
    //     {
    //         int *item =rb.getTail(j);
    //         if(item==NULL){
    //             printf("\nBuffer ends at %d\n",j);
    //             break;
    //         }
    //         printf("%d,",*item);
    //     }
    //     printf("\n");

    //     rb.consumeTail();
    // }

    return 0;
}