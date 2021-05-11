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
    if(!resize(size()+1))
    {
      return false;
    }
    arr[size()-1]=d;
    return true;
  }


  bool resize(int _size)
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



int strMatching_lsum(char *target,char *str,int *ret_strLen)
{
  uint16_t strSum=0;
  uint16_t targetSum=0;
  int strLen=0;
  for(strLen=0;;strLen++)
  {
    char strch=str[strLen];
    char tarch=target[strLen];
    if(strch=='\0')
    {
      break;
    }
    
    if(tarch=='\0')
    {
      return -1;
    }
    strSum+=strch;
    targetSum+=tarch;
  }
  if(ret_strLen)*ret_strLen=strLen;
  if(strSum==targetSum)
  {
    return 0;
  }
  
  for(int i=0;;i++)
  {
    strSum+=str[strLen];
    
  }
  return -1;
}



class buffered_print
{
  char* buf;
  int capacity;
  int _size;
  public:
  buffered_print(int len)
  {
    buf=new char[len];
    capacity=len;
    _size=0;
    resize(0);
  }

  int size()
  {
    return _size;
  }

  int resize(int size)
  {
    if(size>capacity)
    {
      return -1;
    }
    _size=size;
    buf[_size+1]='\0';
    return 0;
  }

  const char* buffer()
  {
    return buf;
  }
  char charAt(int idx)
  {
    if(idx>=0)
    {
      if(idx>=_size)
        return '\0';
      return buf[idx];
    }

    idx+=_size;
    if(idx>=0)
    {
      if(idx>=_size)
        return '\0';
      return buf[idx];
    }
    
    return '\0';
  }

  
  int print(char *fmt, ...) 
  {    
    va_list argptr;
    va_start(argptr,fmt);
//    _size=snprintf(buf+_size,capacity-_size,fmt,argptr);
    _size=vsnprintf(buf+_size,capacity-_size,fmt,argptr);
    va_end(argptr);
    return (_size==capacity)?-1:0;
  }
};
