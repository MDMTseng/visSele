#include "Data_Layer_Protocol.hpp"
#include "string.h"
#include "stdio.h"


Data_JsonRaw_Layer::Data_JsonRaw_Layer():Data_Layer_IF()// throw(std::runtime_error)
{
  packetID=0;
  rawRECVL=0;
  maxHeaderSize=base_headerLen+8;//for extend lrngth field
}


static uint32_t crc_table[16] = {
    0x00000000, 0x1db71064, 0x3b6e20c8, 0x26d930ac,
    0x76dc4190, 0x6b6b51f4, 0x4db26158, 0x5005713c,
    0xedb88320, 0xf00f9344, 0xd6d6a3e8, 0xcb61b38c,
    0x9b64c2b0, 0x86d3d2d4, 0xa00ae278, 0xbdbdf21c
};

static uint32_t crc_update(uint32_t crc, uint8_t data)
{
    uint8_t tbl_idx;
    tbl_idx = crc ^ (data >> (0 * 4));
    crc = crc_table[tbl_idx & 0x0f] ^ (crc >> 4);
    tbl_idx = crc ^ (data >> (1 * 4));
    crc = crc_table[tbl_idx & 0x0f] ^ (crc >> 4);
    return crc;
}

int Data_JsonRaw_Layer::ask_JsonRaw_version(){
  char sendMsg[200];
  sprintf(sendMsg,"{\"type\":\"ask_JsonRaw_version\",\"id\":100445,\"version\":\"%s\"}",VERSION);
  send_json_string(0,(uint8_t*)sendMsg,strlen(sendMsg),0);
  JsonRawStatus=-2;
  return 0;
}
int Data_JsonRaw_Layer::rsp_JsonRaw_version(){
  char sendMsg[200];
  sprintf(sendMsg,"{\"type\":\"rsp_JsonRaw_version\",\"id\":100446,\"version\":\"%s\"}",VERSION);
  send_json_string(0,(uint8_t*)sendMsg,strlen(sendMsg),0);
  JsonRawStatus=-2;
  return 0;
}

int Data_JsonRaw_Layer::recv_jsonRaw_data(uint8_t *raw,int rawL,uint8_t opcode){
  
  if(opcode==1 )
  {
    
    return 0;

  }
  return 0;


}

int Data_JsonRaw_Layer::send_json_string(int head_room,uint8_t *data,int len,int leg_room){
  return send_data(head_room,data,len,leg_room);
}

int Data_JsonRaw_Layer::send_raw_data(int opcode,int head_room,uint8_t *data,int len,int leg_room){
  int headRoomReq=base_headerLen;
  if(len>65535)headRoomReq=base_headerLen+8;
  else if(len>125)headRoomReq=base_headerLen+2;
  else headRoomReq=base_headerLen;


  if(head_room<headRoomReq)
  {//TODO: head room not enough use tmp space to continue the process
    return -1;
  }
  
  int rawL=len;

  {//move head
    len+=headRoomReq;
    data-=headRoomReq;
    head_room-=headRoomReq;
  }
  
  //Set header
  data[0]=1;
  data[1]=opcode;


  for(int i=crcFieldIdx;i<lenFieldIdx;i++)//write crc after len
  {
    data[i]=0;
  }


  //Set header (length field range1<=125 < range2 <= 65535 < range3 <= max 64bit)

  if(rawL>65535)
  {
      data[lenFieldIdx]=127;//mark the 64 bits length field
      for(int i=0;i<8;i++)
      {
        data[lenFieldIdx+1+i]=(rawL>>(8*(7-i)) )&0xFF;
      }
  }
  else if(rawL>125)
  {
      data[lenFieldIdx]=126;//mark the 16 bits length field
      for(int i=0;i<2;i++)
      {
        data[lenFieldIdx+1+i]=(rawL>>(8*(1-i)) )&0xFF;
      }
  }
  else
  {
      data[lenFieldIdx]=rawL;
  }
  //Set header END


  uint32_t crc=0;
  //calc crc
  for(int i=0;i<len;i++)//includes header to calc crc
  {
    crc=crc_update(crc,data[i]);
    // printf("crc>>%X<< %x\n",crc,data[i]);
  }
  for(int i=0;i<(lenFieldIdx-crcFieldIdx);i++)//write crc after len
  {
    data[crcFieldIdx+i]=(crc>>(8*(crcL-1-i)) )&0xFF;
  }

  return send_data(head_room,data,len,leg_room);
}
int Data_JsonRaw_Layer::send_string(int head_room,uint8_t *data,int len,int leg_room){
  return send_raw_data(1, head_room,data, len, leg_room);
}
int Data_JsonRaw_Layer::send_binary(int head_room,uint8_t *data,int len,int leg_room){
  return send_raw_data(2, head_room,data, len, leg_room);
}

int Data_JsonRaw_Layer::send_data(int head_room,uint8_t *data,int len,int leg_room){
  if(downlayer_df==NULL)return -1;
  return downlayer_df->send_data(head_room,data,len,leg_room);
}
int Data_JsonRaw_Layer::recv_data(uint8_t *data,int len, bool is_a_packet){



  if(recvType!=RTYPE::INIT)
  {
    //is in packet receive state
  }
  int i;
  for(i=0;i<len;i++)
  {
    char c=data[i];
    if(recvType==RTYPE::INIT)
    { 
      if(c=='{'||c=='[')
      {
        recvType=RTYPE::JSON;
        jsegpSum=0;
      }
      else if(c==0x1)
      {
        recvType=RTYPE::JSONRAW;
      }
      else
      {
//        printf("E:%d\n",c);
        // recvType=RTYPE::ERROR;
      }
      buffIdx=0;
    }
    

    if(recvType!=RTYPE::INIT)
    {

      dataBuff[buffIdx++]=c;
      switch(recvType)
      {
        case RTYPE::JSON:{
          int stat=jsegp.newChar(c);
          jsegpSum+=stat;
          if(jsegpSum==0)//ending
          {

            if(stat!=-1)
            {
              buffIdx=0;
              //ERROR;
              //printf("E:%d\n",c);
              break;
            }



            dataBuff[buffIdx]='\0';
            // if(uplayer_df!=NULL)
            //   uplayer_df->recv_data(dataBuff,buffIdx);
            
            recv_jsonRaw_data(dataBuff,buffIdx,1);//opcode 1 is for text
            
            recvType=RTYPE::INIT;
            if(uplayer_df!=NULL)uplayer_df->recv_data(dataBuff,buffIdx,true);
          }

        break;
        }
        case RTYPE::JSONRAW:{
          static int headerLen;
          static uint8_t f1_r3_op4_code;
          static uint64_t dataLen;
          static uint32_t targ_crc;
          static uint32_t calc_crc;

          if(buffIdx==1)
          {
            headerLen=base_headerLen;
            targ_crc=calc_crc=0;


            
            break;
          }
          if(buffIdx<headerLen)break;//accumulate for header
          if(buffIdx==headerLen)
          {
            if(buffIdx==base_headerLen)//base info collected
            {
              f1_r3_op4_code=dataBuff[1];
              targ_crc=0;
              for(int i=crcFieldIdx;i<lenFieldIdx;i++)
              {
                targ_crc<<=8;
                targ_crc|=dataBuff[i];
                dataBuff[i]=0;//zero the field to calc real crc
              }
              dataLen=dataBuff[lenFieldIdx];
              if(dataLen==126)
              {
                headerLen=base_headerLen+2;//update real header len with data size info 16bit
              }
              else if(dataLen==127)
              {
                headerLen=base_headerLen+8;//update real header len with data size info 64bit
              }
              break;
            }

            if(dataLen==126)
            {//16bit
              dataLen=(dataBuff[lenFieldIdx+1]<<8)+dataBuff[lenFieldIdx+2];
              
              // printf(">>dataLen:%d\n",dataLen);
            }
            if(dataLen==127)
            {//64bit
              dataLen=0;
              for(int i=0;i<8;i++)
              {
                dataLen=(dataLen<<8)|dataBuff[i+lenFieldIdx+1];
              }
              // printf(">>dataLen:%d\n",dataLen);
            }
            if((headerLen+dataLen)>sizeof(dataBuff))
            {
              //TODO: ERROR, oversize
            }
          }
          if(buffIdx==headerLen+1)
          {
            calc_crc=0;
            for(int i=0;i<buffIdx;i++)//calc header crc
            {
              calc_crc=crc_update(calc_crc,dataBuff[i]);
              // printf("calc_crc>>%X << %x\n",calc_crc,dataBuff[i]);
            }
          }
          else
          {
            calc_crc=crc_update(calc_crc,dataBuff[buffIdx-1]);
          }

          // printf("calc_crc>>%X << %x\n",calc_crc,dataBuff[buffIdx-1]);

          // printf(">>%d:%d\n",buffIdx,headerLen+dataLen);
          if(buffIdx==headerLen+dataLen)//the index reaches the headerLen+dataLen
          {
            if(calc_crc==targ_crc)
            {
              uint8_t opcode = f1_r3_op4_code&0xF;
              if(opcode ==1)
              {//string ends with zero
                dataBuff[buffIdx]='\0';
              }
              recv_jsonRaw_data(dataBuff+headerLen,dataLen,opcode);
              
              if(uplayer_df!=NULL)uplayer_df->recv_data(dataBuff+headerLen,dataLen,true);
            }
            else
            {
              // printf("CRC miss match:tar_crc:%X  calc_crc:%X \n",targ_crc,calc_crc);
            }
            recvType=RTYPE::INIT;
          }



        break;
        }
      }
    }

    // printf("stid>%d\n",jsegpSum);
  }
  

  if(recvType!=RTYPE::INIT)
  {
    //is in packet receive state
  }
  return 0;


}







// int Data_JsonRaw_Layer::sendString(Data_JsonRaw_Layer *JRL,uint8_t* buf, int bufL, bool directStringFormat, cJSON* json)
// {

//   if (JRL==NULL)
//   {
//     return -1;
//   }
//   int buff_head_room=JRL->max_head_room_size();
//   int buffSize=bufL-buff_head_room;
//   char *padded_buf=(char*)buf+buff_head_room;

//   int ret= cJSON_PrintPreallocated(json, padded_buf, buffSize-JRL->max_leg_room_size(), false);

//   if(ret == false)
//   {
//     return -1;
//   }

//   int contentSize=strlen(padded_buf);
//   if(directStringFormat)
//   {
//     ret = perifCH->send_json_string(buff_head_room,(uint8_t*)padded_buf,contentSize,buffSize-contentSize);
//   }
//   else
//   {
//     ret = perifCH->send_string(buff_head_room,(uint8_t*)padded_buf,contentSize,buffSize-contentSize);
//   }
//   return ret;
// }




int Data_JsonRaw_Layer::send_printf(uint8_t* buf, int bufL, bool directStringFormat, const char *fmt, ...)
{
  int buff_head_room=max_head_room_size();
  int buffSize=bufL-buff_head_room;
  uint8_t *padded_buf=buf+buff_head_room;

  va_list aptr;
  int ret;
  va_start(aptr, fmt);
  ret = vsnprintf ((char*)padded_buf, buffSize-max_leg_room_size(), fmt, aptr);
  va_end(aptr); 

  if(ret<0)return ret;

  int contentSize=ret;
  
  if(directStringFormat)
  {
    ret = send_json_string(buff_head_room,padded_buf,contentSize,buffSize-contentSize);
  }
  else
  {
    ret = send_string(buff_head_room,padded_buf,contentSize,buffSize-contentSize);
  }
  return ret;
}