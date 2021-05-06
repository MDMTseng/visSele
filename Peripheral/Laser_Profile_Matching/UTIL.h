template <class T>
class mArray
{
  protected:
  int cur_size;
  int max_size;
  public:
  
  T* arr;
  
  mArray(int max_size=0)
  {
    RESET( max_size);
  }

  
  void RESET(int max_size)
  {
    cur_size=0;
    if(arr)
      delete(arr);
    
    if(max_size<=0)
    {
      arr=NULL;
      this->max_size=0;
      
      return;
    }

    
    arr=new T[max_size];
    this->max_size=max_size;
  }
  ~mArray()
  {
    RESET(0);
  }

  
  bool push_back(T d)
  {
    if(!setSize(size()+1))
    {
      return false;
    }
    arr[size()-1]=d;
    return true;
  }


  bool setSize(int _size)
  {
    if(_size<0)return false;
    if(_size>max_size)return false;
    cur_size=_size;
    return true;
  }

  
  int size()
  {
    return cur_size;
  }
  int maxSize()
  {
    return max_size;
  }
};





int16_t tempSamp(int idx, uint16_t targetLen , int16_t* temp, uint16_t tempL)
{
  if (idx < 0)return 0;
  int eq_idx = idx * tempL / targetLen;
  if (eq_idx >= tempL)return 0;
  return temp[eq_idx];
}




uint32_t tempSAD(int16_t* temp1, uint16_t temp1L, int16_t* temp2, uint16_t temp2L, int16_t NA_Thres = 3000, int *ret_NA_Count = NULL)
{
  uint32_t diffSum = 0;
  int NA_Count = 0;
  for (int i = 0; i < temp1L; i++)
  {
    if (temp1[i] > NA_Thres)
    {
      NA_Count++;
      continue;
    }
    int16_t tempV = tempSamp(i, temp1L, temp2, temp2L);
    int16_t diff = temp1[i] - tempV;
    if (diff < 0)diff = -diff;
    diffSum += diff;
  }
  if (ret_NA_Count)*ret_NA_Count = NA_Count;

  int divCount = (temp1L - NA_Count);
  if (divCount == 0)
  {
    return NA_Thres;
  }
  return diffSum / divCount;
}



uint32_t tempSAD2(int16_t* temp1, uint16_t temp1L, int16_t* temp2, uint16_t temp2L, int16_t NA_Thres = 3000, int *ret_NA_Count = NULL)
{

  if (  (temp1L < (temp2L * 2 / 3)) ||
        (temp1L > (temp2L * 3 / 2)))
  {
    return -1;
  }


  uint32_t diffSum = 0;
  const int tCacheSize = 3;

  const int tCacheLen = tCacheSize * 2 + 1;

  int16_t tCache[tCacheLen];

  for (int i = 0; i < tCacheLen - 1; i++)
  {
    tCache[i] = tempSamp(i, temp1L, temp2, temp2L);
  }
  int tCacheHead = tCacheLen - 1;
  int NA_Count = 0;
  for (int i = tCacheSize; i < temp1L - tCacheSize; i++)
  {
    if (temp1[i] > NA_Thres)
    {
      NA_Count++;
      continue;
    }
    int16_t tempV = tempSamp(i + tCacheSize, temp1L, temp2, temp2L);
    tCache[tCacheHead] = tempV;
    tCacheHead++;
    if (tCacheHead >= tCacheLen)tCacheHead = 0;

    int16_t minDiff = NA_Thres;
    for (int j = 0; j < tCacheLen; j++)
    {
      int16_t diff = temp1[i] - tCache[j];
      if (diff < 0)diff = -diff;
      if (minDiff > diff)
      {
        minDiff = diff;
      }

    }

    diffSum += minDiff;
  }
  if (ret_NA_Count)*ret_NA_Count = NA_Count;
  int divCount = (temp1L - 2 * tCacheSize - NA_Count);
  if (divCount == 0)
  {
    return NA_Thres;
  }
  return diffSum / divCount;
}
