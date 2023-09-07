#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
class Data_Layer_IF
{

    protected:
    Data_Layer_IF *uplayer_df;
    Data_Layer_IF *downlayer_df;
    int maxHeaderSize=0;
    int maxLegSize=0;
    bool isConnected;
    public:
    Data_Layer_IF(){
      uplayer_df=downlayer_df=NULL;
      isConnected=false;
    }

    int max_head_room_size()
    {
      int hsize=maxHeaderSize;
      if(downlayer_df)hsize+=downlayer_df->max_head_room_size();
      return hsize;
    }
    int max_leg_room_size()
    {
      int hsize=maxHeaderSize;
      if(downlayer_df)hsize+=downlayer_df->max_leg_room_size();
      return hsize;
    }
    
    int setDLayer(Data_Layer_IF *down)
    {
      if(downlayer_df)
      {
        // downlayer_df->close();//supposedly downlayer should call the disconnected at here
        delete downlayer_df;
      }
      downlayer_df=down;
      down->setULayer(this);
      return 0;
    }
    
    int setULayer(Data_Layer_IF *up)
    {
      if(uplayer_df)
      {
        uplayer_df->disconnected(this);
      }
      uplayer_df=up;

      if(isConnected && uplayer_df!=NULL)
      {
        uplayer_df->connected(this);
      }

      return 0;
    }

    virtual int onError(int code,const char* describe)
    {
      close();
      return 0;
    }

    virtual int close(){
      
      printf("close this:%p \n",this);
      //tell uplayer

      //destroy downlayer

      if(downlayer_df!=NULL)
      {
        Data_Layer_IF *ptr=downlayer_df;
        downlayer_df=NULL;
        delete ptr;
      }
      else{
        //  disconnected(NULL);
      }
      return 0;
    };
    
    virtual int send_data(int head_room,uint8_t *data,int len,int leg_room){
      if(downlayer_df==NULL)return -1;
      return downlayer_df->send_data(head_room,data,len,leg_room);
    }

    //this should be called from downlayer
    virtual int recv_data(uint8_t *data,int len, bool is_a_packet=false){
      if(uplayer_df==NULL)return -1;
      return uplayer_df->recv_data(data,len);
    }
    virtual void connected(Data_Layer_IF* ch)=0;//downlayer docking complete
    virtual void disconnected(Data_Layer_IF* ch)
    {
      printf(" disconnected:%p ch:%p  this:%p!!!!\n",downlayer_df,ch,this);
      if(downlayer_df!=NULL)
      {
        Data_Layer_IF *ptr=downlayer_df;
        downlayer_df=NULL;
        delete ptr;
      }

      if(uplayer_df!=NULL)
      {
        uplayer_df->disconnected(this);
        
      }
    }

    virtual ~Data_Layer_IF()
    {
      // printf("DESTRUCT!!!! :%p\n",this);
      // close();
      // printf(">>>>>>>%p\n",this);
    }
};
