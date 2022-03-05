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

    // printf("%c\n",c);

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
      else if(c==' '||c=='\t'||c=='\n')
      {
        continue;
      }
      else
      {
        recvType=RTYPE::ERROR;
        errorCode=ERROR_TYPE::INIT_CHAR_ERROR;
        recv_ERROR(errorCode);
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
        recvType=RTYPE::ERROR;//full and enter error clean state
        errorCode=ERROR_TYPE::RECV_BUFFER_FULL;
        recv_ERROR(errorCode);
      }
      // if(recvType==RTYPE::JSON && buffIdx>100)recvType=RTYPE::ERROR;//long json is error~
      switch(recvType)
      {
        case RTYPE::ERROR:{
          //init
          const int RESET_PACKET_SIZE=(sizeof(RESET_PACKET)-1);
          if(buffIdx>=RESET_PACKET_SIZE)//buff filled with enough data
          {
            
            // printf("buffIdx  %d >=RESET_PACKET_SIZE  %d \n",buffIdx,RESET_PACKET_SIZE);
            // for(int j=0;j<buffIdx;j++)
            // {
            //   printf("%c",dataBuff[j]);
            // }
            // printf("\n");
            



            int matching_state=0;//0 for no matching, 1 for matching ok, 2 for matching ok but not complete(wait for upcoming data)
            int dataShiftBackIdx=0;
            for(int j=0;j<buffIdx;j++)
            {

              bool matchPass=true;
              int k=0;
              for(k=0;k<RESET_PACKET_SIZE && (j+k)<buffIdx  ;k++)
              {
                if(RESET_PACKET[k] != dataBuff[j+k])
                {
                  matchPass=false;
                  break;
                }
                
              }

              if(matchPass)
              {
                //packet starts with j
                //may need data shift
                if(k<RESET_PACKET_SIZE)//incomplete matching
                {
                  matching_state=2;
                  dataShiftBackIdx=j;
                }
                else
                {
                  //hit RESET
                  matching_state=1;
                  dataShiftBackIdx=j+RESET_PACKET_SIZE-1;//shift and skip reset packet
                  recv_RESET();
                  recvType=RTYPE::INIT;
                }
                break;
              }
              else
              {
                continue;
              }



            }

            if(matching_state==0)
            {
              buffIdx=0;
            }
            else if(dataShiftBackIdx)
            {
              if(dataShiftBackIdx==buffIdx)//meaning reset idx
              {
                buffIdx=0;
              }
              else
              {
                for(int j=0;j+dataShiftBackIdx<buffIdx;j++)
                {
                  dataBuff[j]=dataBuff[j+dataShiftBackIdx];
                }
                buffIdx-=dataShiftBackIdx;
              }
            }

          }

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
              // if(uplayer_df!=NULL)
              //   uplayer_df->recv_data(dataBuff,buffIdx);
              recv_jsonRaw_data(dataBuff,buffIdx,1);//opcode 1 is for text
              
              recvType=RTYPE::INIT;
              if(uplayer_df!=NULL)uplayer_df->recv_data(dataBuff,buffIdx,true);
            }

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
              recvType=RTYPE::ERROR;
              
              errorCode=ERROR_TYPE::RAW_DATA_OVERSIZE;
              recv_ERROR(errorCode);
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
              recvType=RTYPE::ERROR;
              errorCode=ERROR_TYPE::RAW_CRC_ERROR;
              recv_ERROR(errorCode);
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






class Data_LOOPBACK_Layer2:public Data_Layer_IF
{

public:
    Data_LOOPBACK_Layer2()
    {
      srand((unsigned)time(NULL));
    }

    float errorRate=0.01;

    int send_data(int head_room,uint8_t *data,int len,int leg_room)
    {
      int dataSent=0;
      for(int i=0;dataSent<len;i++)
      {
        
        if(randomGen()<errorRate)//dose error
        {
          printf(">>>>>>>>>>>>>>>>>>>>>add error\n");
          char d=',';
          recv_data((uint8_t*)&d,1,false);
        }

        int len2send = 1+(rand()%(len-dataSent));


        recv_data(data+dataSent,len2send,false);

        dataSent+=len2send;

      }
    }

    // int recv_data(uint8_t *data,int len){}

    void connected(Data_Layer_IF* ch){}
};



class MData_uInsp:public Data_JsonRaw_Layer
{
  
  public:
  MData_uInsp():Data_JsonRaw_Layer()// throw(std::runtime_error)
  {
  }
  bool printRecv=false;
  int RCount=0;
  int recv_jsonRaw_data(uint8_t *raw,int rawL,uint8_t opcode){
    
    if(printRecv==false)return 0;
    if(opcode==1 )
    {
      cJSON *json=cJSON_Parse((char*)raw);
      if(json)
      {
        char *jstr = cJSON_Print(json);
        printf("JSON:\n%s\n====len:%d\n",jstr,rawL);
        delete jstr;
        char* tmpstr;
        if((tmpstr=JFetch_STRING(json,"type"))&&strcmp(tmpstr,"MSG")==0)
        {
          RCount++;
        }


        // for(int i=0;i<rawL;i++)
        // {
        //   printf("%X ",raw[i]);
        // }
        // printf("\n");

        cJSON_Delete(json);
      }
      else
      {
        printf("STR:\n%s\n====len:%d\n",raw,rawL);
      }
    }
    // printf(">>opcode:%d\n",opcode);
    return 0;


  }

  void connected(Data_Layer_IF* ch){
    
    printf(">>>%X connected\n",ch);
  }

  void disconnected(Data_Layer_IF* ch){
    printf(">>>%X disconnected\n",ch);
  }

  ~MData_uInsp()
  {
    close();
    printf("MData_uInsp DISTRUCT:%p\n",this);
  }


  int recv_RESET()
  {
    printf("Get recv_RESET\n");
  }

  int recv_ERROR(ERROR_TYPE errorcode)
  {
    printf("Get recv_ERROR:%d\n",errorcode);
  }
  
  // int send_data(int head_room,uint8_t *data,int len,int leg_room){
    
  //   // printf("==============\n");
  //   // for(int i=0;i<len;i++)
  //   // {
  //   //   printf("%d ",data[i]);
  //   // }
  //   // printf("\n");
  //   return recv_data(data,len, false);//LOOP back
  // }
};



// int test()
// {
  
//   if(0){
//     char const *JSONStr = R"TOKEN(
// [
// 	{
// 		"id": "0001",
// 		"type": "donut",
// 		"name": "Cake",
// 		"ppu": 0.55,
// 		"batters":
// 			{
// 				"batter":
// 					[
// 						{ "id": "1001", "type": "Regular" },
// 						{ "id": "1002", "type": "Chocolate" },
// 						{ "id": "1003", "type": "Blueberry" },
// 						{ "id": "1004", "type": "Devil's Food" }
// 					]
// 			},
// 		"topping":
// 			[
// 				{ "id": "5001", "type": "None" },
// 				{ "id": "5002", "type": "Glazed" },
// 				{ "id": "5005", "type": "Sugar" },
// 				{ "id": "5007", "type": "Powdered Sugar" },
// 				{ "id": "5006", "type": "Chocolate with Sprinkles" },
// 				{ "id": "5003", "type": "Chocolate" },
// 				{ "id": "5004", "type": "Maple" }
// 			]
// 	},
// 	{
// 		"id": "0002",
// 		"type": "donut",
// 		"name": "Raised",
// 		"ppu": 0.55,
// 		"batters":
// 			{
// 				"batter":
// 					[
// 						{ "id": "1001", "type": "Regular" }
// 					]
// 			},
// 		"topping":
// 			[
// 				{ "id": "5001", "type": "None" },
// 				{ "id": "5002", "type": "Glazed" },
// 				{ "id": "5005", "type": "Sugar" },
// 				{ "id": "5003", "type": "Chocolate" },
// 				{ "id": "5004", "type": "Maple" }
// 			]
// 	},
// 	{
// 		"id": "0003",
// 		"type": "donut",
// 		"name": "Old Fashioned",
// 		"ppu": 0.55,
// 		"batters":
// 			{
// 				"batter":
// 					[
// 						{ "id": "1001", "type": "Regular" },
// 						{ "id": "1002", "type": "Chocolate" }
// 					]
// 			},
// 		"topping":
// 			[
// 				{ "id": "5001", "type": "None" },
// 				{ "id": "5002", "type": "Glazed" },
// 				{ "id": "5003", "type": "Chocolate" },
// 				{ "id": "5004", "type": 
//         {
// 	"items":
// 		{
// 			"item":
// 				[
// 					{
// 						"id": "0001",
// 						"type": "donut",
// 						"name": "Cake",
// 						"ppu": 0.55,
// 						"batters":
// 							{
// 								"batter":
// 									[
// 										{ "id": "1001", "type": "Regular" },
// 										{ "id": "1002", "type": "Chocolate" },
// 										{ "id": "1003", "type": "Blueberry" },
// 										{ "id": "1004", "type": "Devil's Food" }
// 									]
// 							},
// 						"topping":
// 							[
// 								{ "id": "5001", "type": "None" },
// 								{ "id": "5002", "type": "Glazed" },
// 								{ "id": "5005", "type": "Sugar" },
// 								{ "id": "5007", "type": "Powdered Sugar" },
// 								{ "id": "5006", "type": "Chocolate with Sprinkles" },
// 								{ "id": "5003", "type": "Chocolate" },
// 								{ "id": "5004", "type": "Maple" }
// 							]
// 					},

// 					...

// 				]
// 		}
// }
        
//          }
// 			]
// 	},
//   {
// 	"items":
// 		{
// 			"item":
// 				[
// 					{
// 						"id": "0001",
// 						"type": "donut",
// 						"name": "Cake",
// 						"ppu": 0.55,
// 						"batters":
// 							{
// 								"batter":
// 									[
// 										{ "id": "1001", "type": "Regular" },
// 										{ "id": "1002", "type": "Chocolate" },
// 										{ "id": "1003", "type": "Blueberry" },
// 										{ "id": "1004", "type": "Devil's Food" }
// 									]
// 							},
// 						"topping":
// 							[
// 								{ "id": "5001", "type": "None" },
// 								{ "id": "5002", "type": "Glazed" },
// 								{ "id": "5005", "type": "Sugar" },
// 								{ "id": "5007", "type": "Powdered Sugar" },
// 								{ "id": "5006", "type": "Chocolate with Sprinkles" },
// 								{ "id": "5003", "type": "Chocolate" },
// 								{ "id": "5004", "type": "Maple" }
// 							]
// 					}
// 				]
// 		}
// }

// ]
//     )TOKEN";


//     json_seg_parser jsp;

//     int jlevel=0;
//     for(int i=0;JSONStr[i];i++)
//     {
//       json_seg_parser::RESULT res=jsp.newChar(JSONStr[i]);
//       // printf("r:%d  >>> %c\n",res,JSONStr[i]);


//       if(res>json_seg_parser::RESULT::ERROR)
//       {
//         if(res==json_seg_parser::RESULT::OBJECT_START||
//           res==json_seg_parser::RESULT::ARRAY_START)
//         {
//           jlevel++;
          
//           // printf("%c   lvl:%d \n",JSONStr[i],jlevel);
//         }
//         else if(
//           res==json_seg_parser::RESULT::OBJECT_COMPLETE||
//           res==json_seg_parser::RESULT::ARRAY_COMPLETE)
//         {
//           jlevel--;
//           // printf("\n%c  lvl:%d\n",JSONStr[i],jlevel);
//           // printf("%c",JSONStr[i]);
//         }
        
//         if(res==json_seg_parser::RESULT::KEY_START)
//         {
//            printf("\nKEY >k<\n");
//         }
//         if(res==json_seg_parser::RESULT::VAL_START)
//         {
//            printf("\nVAL >0<\n");
//         }
//         if(res==json_seg_parser::RESULT::STR_START)
//         {
//            printf("\nSTR >\"<\n");
//         }
//         else
//         {
//         }
//         printf("%c",JSONStr[i]);
//       }      
//       else
//       {
//         break;
//       }
//     }


//     return 0;
//   }




//   if(0)for(int i=0;i<1;i++)
//   {
//     //  Data_TCP_Layer *PHYLayer=new Data_TCP_Layer("127.0.0.1",1234);
//     Data_UART_Layer *PHYLayer=new Data_UART_Layer("/dev/cu.SLAB_USBtoUART",230400, "8N1");
//     // Data_LOOPBACK_Layer2 *PHYLayer=new Data_LOOPBACK_Layer2();

//     MData_uInsp *mift=new MData_uInsp();
//     mift->setDLayer(PHYLayer);
//     mift->send_RESET();
//     mift->send_RESET();
//     mift->RESET();
    
//     mift->ask_JsonRaw_version();

//     bool directStringFormat=true;
//     int id=0;
//     for(int k=0;k<1;k++)
//     {
//       mift->printRecv=true;
//       // PHYLayer->errorRate=0.0001;
//       uint8_t buffer[200];  
//       mift->send_printf(buffer,sizeof(buffer),directStringFormat,"{\"type\":\"PIN_CONF\",\"pin\":2,\"mode\":1,\"id\":%d}",id++);

      
//       int delay_ms=1;
//       for(int j=0;j<50;j++)
//       { 
//         // std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
//         mift->send_printf(buffer,sizeof(buffer),directStringFormat,"{\"type\":\"PIN_CONF\",\"pin\":2,\"output\":1,\"id\":%d}",id++);
//         // std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
//         mift->send_printf(buffer,sizeof(buffer),directStringFormat,"{\"type\":\"PIN_CONF\",\"pin\":2,\"output\":0,\"id\":%d}",id++);
//       }

      
//       // mift->printRecv=true;
//       // PHYLayer->errorRate=0;
//       // mift->RCount--;
//       mift->send_printf(buffer,sizeof(buffer),directStringFormat,"{\"type\":\"MSG\"}",id++);

//       mift->send_printf(buffer,sizeof(buffer),directStringFormat,"{\"type\":\"BYE\"}",id++);
//       if(mift->RCount!=0)
//       {
//         printf("===========k:%d========mift->RCount:%d=\n",k,mift->RCount);
//       }
//       // str_len=sprintf(str,"{\"type\":\"PIN_CONF\",\"pin\":2,\"mode\":0,\"id\":%d}",id++);//input
//       // mift->send_string(headerSize,(uint8_t*)str,str_len,sizeof(buffer)-headerSize-str_len);
//       // for(int j=0;j<500;j++)
//       // {
//       //   sleep(1);
//       //   str_len=sprintf(str,"{\"type\":\"PIN_CONF\",\"pin\":2,\"output\":-1,\"id\":%d}",id++);//get reading
//       //   mift->send_string(headerSize,(uint8_t*)str,str_len,sizeof(buffer)-headerSize-str_len);

//       // }

//       // std::this_thread::sleep_for(std::chrono::milliseconds(100));


//     }
    
//     sleep(1);
  
//     delete mift;
//   }
//   return 0;
// }