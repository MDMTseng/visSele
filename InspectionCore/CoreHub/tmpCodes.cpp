
#include <main.h>


#include <sys/stat.h>

static std::timed_mutex mainThreadLock;
static int mainThreadLock_lock(int call_lineNumber,char* msg="",int try_lock_timeout_ms=0)
{

  if(try_lock_timeout_ms<=0)
  {
    //LOGI("%s_%d: Locking ",msg,call_lineNumber);
    mainThreadLock.lock();
  }
  else
  {
    using Ms = std::chrono::milliseconds;
    
    //LOGI("%s_%d: Locking %dms",msg,call_lineNumber,try_lock_timeout_ms);
    if(mainThreadLock.try_lock_for(Ms(try_lock_timeout_ms)))
    {
    }
    else
    {
      //LOGI("Lock failed");
      return -1;
    }
  }
  //LOGI("%s_%d: Locked ",msg,call_lineNumber);

  return 0;
}
static bool terminationFlag=false;

static void ImgPipeProcessThread(bool *terminationflag)
{

}


static std::thread mThread(ImgPipeProcessThread, &terminationFlag);


int tmain()
{

  return 0;
}