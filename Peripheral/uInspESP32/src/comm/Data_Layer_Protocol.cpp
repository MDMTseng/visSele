#include "comm/Data_Layer_Protocol.hpp"
#include "string.h"
#include "stdio.h"
#include <ctype.h>
#include <stdlib.h>
#include <Arduino.h>


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

void Data_JsonRaw_Layer::enterProtocolError(ERROR_TYPE err,uint8_t *recv_data,size_t dataL)
{
  recvType=RTYPE::ERROR;
  errorCode=err;
  if(protocolErrorActive==false)
  {
    protocolErrorActive=true;
    // Counted on the transition, not per byte. Nothing recorded this before, so
    // a link that had gone deaf looked exactly like one that was idle.
    rx_latch_n++;
    recv_ERROR(err,recv_data,dataL);
  }
}

void Data_JsonRaw_Layer::clearProtocolError()
{
  protocolErrorActive=false;
  errorCode=ERROR_TYPE::NONE;
  recvType=RTYPE::INIT;
  buffIdx=0;
  jsegp.reset();
  jlevel=0;
}

// Whitespace-tolerant match for `"type" : "<value>"` starting at `at`.
// Returns the length matched, or 0.
//
// This used to be a memcmp against the exact bytes `"type":"RESET"`, and it is
// the ONLY way back once the parser has latched into RTYPE::ERROR -- no frame
// is delivered after that, so the post-parse handler that also accepts RESET
// can never be reached. Python's json.dumps emits `{"type": "RESET"}` with a
// space after the colon by default, so a host using the obvious call had no
// escape hatch at all and the board could only be recovered by power-cycling.
static int matchTypeValueAt(const uint8_t *b,int at,int n,const char *quoted)
{
  int i=at;
  const char *k="\"type\"";
  for(int j=0;k[j];j++,i++){ if(i>=n||b[i]!=k[j]) return 0; }
  while(i<n && (b[i]==' '||b[i]=='\t')) i++;
  if(i>=n||b[i]!=':') return 0;
  i++;
  while(i<n && (b[i]==' '||b[i]=='\t')) i++;
  for(int j=0;quoted[j];j++,i++){ if(i>=n||b[i]!=quoted[j]) return 0; }
  return i-at;
}

bool Data_JsonRaw_Layer::tryRecoverResetFromErrorBuffer()
{
  const int keyLen=(int)strlen("\"type\":\"RESET\"");   // shortest possible form

  int firstBrace=-1;
  for(int i=0;i<buffIdx;i++)
  {
    if(dataBuff[i]=='{')
    {
      firstBrace=i;
    }
    // Two escapes, not one. RESET is the historical hatch; clear_error is the
    // command a person actually sends when a machine has stopped answering, and
    // it used to be the one thing that could not work.
    bool viaClear=false;
    int mlen = (i+keyLen<=buffIdx) ? matchTypeValueAt(dataBuff,i,buffIdx,"\"RESET\"") : 0;
    if(mlen==0)
    {
      mlen = matchTypeValueAt(dataBuff,i,buffIdx,"\"clear_error\"");
      if(mlen>0) viaClear=true;
    }
    if(mlen>0)
    {
      int endIdx=i+mlen;
      while(endIdx<buffIdx && dataBuff[endIdx]!='}')
      {
        endIdx++;
      }
      if(endIdx<buffIdx)
      {
        // Order matters here, and it used to be wrong.
        //
        // Both handlers call clearProtocolError(), which sets buffIdx=0 and
        // puts the layer in RESYNC (discard everything up to the next newline).
        // The compaction below then ran anyway and subtracted the PRE-recovery
        // shift from that fresh zero, so buffIdx came out NEGATIVE -- measured
        // at -2104 and -16 on different inputs -- and the next byte executed
        // `dataBuff[buffIdx++]=c` at a negative index, writing BEFORE the
        // array. That is worse than running off the end: it lands on whatever
        // the compiler put ahead of dataBuff in the object.
        //
        // There is nothing to compact. RESYNC discards to the newline by
        // definition, so the buffer the handlers just emptied should stay
        // empty. AUDIT_BACKLOG_2026-08-18 P1; regression test in
        // tools/test_data_layer_overflow.cpp.
        if(viaClear) handleClearErrorRecovery();
        else         handleResetRecovery();
        return true;
      }
      if(firstBrace>0)
      {
        int shift=firstBrace;
        memmove(dataBuff,dataBuff+shift,buffIdx-shift);
        buffIdx-=shift;
      }
      return false;
    }
  }

  if(firstBrace>0)
  {
    int shift=firstBrace;
    memmove(dataBuff,dataBuff+shift,buffIdx-shift);
    buffIdx-=shift;
  }
  else if(buffIdx>=sizeof(dataBuff))
  {
    buffIdx=0;
  }
  return false;
}

// Same unlatch as RESET, but it delivers clear_error's own intent instead of a
// RESET's -- otherwise the command that got us out would be swallowed and the
// machine would stay in its error state with a healthy link.
void Data_JsonRaw_Layer::handleClearErrorRecovery()
{
  recv_CLEAR_ERROR();
  clearProtocolError();
  recvType=RTYPE::RESYNC;
}

void Data_JsonRaw_Layer::handleResetRecovery()
{
  recv_RESET();
  clearProtocolError();
  // The recovering RESET frame carries its own *HHHH trailer; consuming it
  // as INIT bytes would latch us right back. Skip to the next newline.
  recvType=RTYPE::RESYNC;
}

int Data_JsonRaw_Layer::ask_JsonRaw_version(){
  char sendMsg[200];
  sprintf(sendMsg,"{\"type\":\"ask_JsonRaw_version\",\"id\":100445,\"version\":\"%s\"}",VERSION);
  return send_json_string(0,(uint8_t*)sendMsg,strlen(sendMsg),0);
}
int Data_JsonRaw_Layer::rsp_JsonRaw_version(){
  char sendMsg[200];
  sprintf(sendMsg,"{\"type\":\"rsp_JsonRaw_version\",\"id\":100446,\"version\":\"%s\"}",VERSION);
  return send_json_string(0,(uint8_t*)sendMsg,strlen(sendMsg),0);
}

int Data_JsonRaw_Layer::send_RESET(){
  const int RESET_PACKET_SIZE=(sizeof(RESET_PACKET)-1);
  return send_json_string(0,(uint8_t*)RESET_PACKET,RESET_PACKET_SIZE,0);
}


int Data_JsonRaw_Layer::recv_jsonRaw_data(uint8_t *raw,int rawL,uint8_t opcode){
  
  if(opcode==1 )
  {
    
    return 0;

  }
  return 0;


}

uint16_t Data_JsonRaw_Layer::crc16_ccitt(const uint8_t *d,int len){
  uint16_t crc=0xFFFF;
  for(int i=0;i<len;i++){
    crc^=(uint16_t)d[i]<<8;
    for(int b=0;b<8;b++)
      crc=(crc&0x8000)?(crc<<1)^0x1021:(crc<<1);
  }
  return crc;
}

int Data_JsonRaw_Layer::send_json_string(int head_room,uint8_t *data,int len,int leg_room){
  int r=send_data(head_room,data,len,leg_room);
  // Integrity trailer: *HHHH\n (CRC16-CCITT over the JSON bytes). Peers that
  // predate it ignore stray trailer text via their INIT resync; peers that
  // know it can drop corrupted frames instead of acting on them.
  char tr[8];
  uint16_t crc=crc16_ccitt(data,len);
  int tl=snprintf(tr,sizeof(tr),"*%04X\n",crc);
  send_data(0,(uint8_t*)tr,tl,0);
  return r;
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
void Data_JsonRaw_Layer::finishJsonFrame(bool crc_present,bool crc_ok){
  rx_frames++;
  if(crc_present && !crc_ok)
  {
    // Corrupted frame: drop it and resync -- acting on it would be worse
    // than losing it (the sender's timeout/fail-safe paths cover the loss).
    rx_crc_fail++;
  }
  else
  {
    if(crc_present)rx_crc_ok++;
    last_rx_ms=(uint32_t)millis();
    dataBuff[buffIdx]='\0';
    recv_jsonRaw_data(dataBuff,buffIdx,1);//opcode 1 is for text
    if(uplayer_df!=NULL)uplayer_df->recv_data(dataBuff,buffIdx,true);
  }
  recvType=RTYPE::INIT;
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
        jsegp.reset();
        jlevel=0;
      }
      else if(c==0x1)
      {
        recvType=RTYPE::JSONRAW;
      }
      else if(c==' '||c=='\t'||c=='\n'||c=='\r')
      {
        continue;
      }
      else
      {
        enterProtocolError(ERROR_TYPE::INIT_CHAR_ERROR,(uint8_t*)data,len);
      }
      buffIdx=0;
    }
    

    if(recvType!=RTYPE::INIT)
    {
      
      dataBuff[buffIdx++]=c;

      // if(buffIdx%20==1)
      // {
      //   printf("buffIdx  %d\n",buffIdx);
      // }

      if(buffIdx==sizeof(dataBuff))
      {
        enterProtocolError(ERROR_TYPE::RECV_BUFFER_FULL);
      }
      // if(recvType==RTYPE::JSON && buffIdx>100)recvType=RTYPE::ERROR;//long json is error~
      switch(recvType)
      {
        case RTYPE::ERROR:{
          tryRecoverResetFromErrorBuffer();
          break;
        }
        
        
        
        case RTYPE::JSON:{
          json_seg_parser::RESULT stat=jsegp.newChar(c);
          if(stat<=json_seg_parser::RESULT::ERROR)
          {
            // printf("jsegp.ERROR_CODE\n");
            recvType=RTYPE::ERROR;
            errorCode=ERROR_TYPE::JSON_FORMAT_ERROR;
            recv_ERROR(errorCode);
            break;
          }

          if(stat==json_seg_parser::RESULT::OBJECT_START||
          stat==json_seg_parser::RESULT::ARRAY_START)
          {
            jlevel++;
            
            // printf("%c   lvl:%d \n",JSONStr[i],jlevel);
          }
          else if(
            stat==json_seg_parser::RESULT::OBJECT_COMPLETE||
            stat==json_seg_parser::RESULT::ARRAY_COMPLETE)
          {
            jlevel--;

            if(jlevel==0)
            {
              dataBuff[buffIdx]='\0';
              // Defer dispatch until we know whether a CRC trailer follows.
              recvType=RTYPE::TRAILER;
              trailerIdx=0;
            }

          }

        break;
        }
        case RTYPE::RESYNC:{
          buffIdx--;   // not frame content
          if(c=='\n'||c=='\r') recvType=RTYPE::INIT;
          break;
        }
        case RTYPE::TRAILER:{
          buffIdx--;   // undo the generic append: trailer bytes are not frame
          if(trailerIdx==0)
          {
            if(c=='*'){ trailerBuf[trailerIdx++]=c; }
            else if(c=='\n'||c=='\r'){ finishJsonFrame(false,false); }
            else { finishJsonFrame(false,false); i--; }   // legacy frame; reprocess c
          }
          else if(c=='\n'||c=='\r')
          {
            bool ok=false;
            if(trailerIdx==5)
            {
              trailerBuf[5]='\0';
              uint16_t want=(uint16_t)strtoul(trailerBuf+1,NULL,16);
              ok=(want==crc16_ccitt(dataBuff,buffIdx));
            }
            finishJsonFrame(true,ok);
          }
          else if(trailerIdx<5 && isxdigit((unsigned char)c))
          {
            trailerBuf[trailerIdx++]=c;
          }
          else
          {
            finishJsonFrame(true,false);   // malformed trailer = corrupt
            i--;
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
              enterProtocolError(ERROR_TYPE::RAW_DATA_OVERSIZE);
              break;
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
              recvType=RTYPE::INIT;
            }
            else
            {
              // printf("CRC miss match:tar_crc:%X  calc_crc:%X \n",targ_crc,calc_crc);
              enterProtocolError(ERROR_TYPE::RAW_CRC_ERROR);
              break;
            }
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
