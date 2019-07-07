#include "WebSocketProtocol.h"
#include "sha1.h"
#include "Base64.h"
//#include <Ethernet2.h>
//#include "Eth_Boost.h"
//#include "RingBuff.h"


#define DEBUG_
#ifdef DEBUG_
#define DEBUG_print(A, ...) Serial.print(A,##__VA_ARGS__)
#define DEBUG_println(A, ...) Serial.println(A,##__VA_ARGS__)
#else
#define DEBUG_print(A, ...)
#define DEBUG_println(A, ...)
#endif

/*
example PKG:
GET / HTTP/1.1
Upgrade: websocket
Connection: Upgrade
Host: 10.0.0.52:5213
Origin: http://mdm.noip.me
Pragma: no-cache
Cache-Control: no-cache
Sec-WebSocket-Key: XY1RK1rsvSdk4Q4xggisMg==
Sec-WebSocket-Version: 13
Sec-WebSocket-Extensions: permessage-deflate; client_max_window_bits, x-webkit-deflate-frame
User-Agent: Mozilla/5.0 (Windows NT 6.1; WOW64) AppleWebKit/537.36 (KHTML, like Gec
*/
#define MEMCODE_(x)  pgm_read_byte_near(x)

const char HANDSHAKE_GETHTTP[] PROGMEM= ("GET /");
const char HANDSHAKE_UPGRAGE [] PROGMEM= ("Upgrade:");//
const char HANDSHAKE_CONNECTION [] PROGMEM= ("Connection:");//
const char HANDSHAKE_Host [] PROGMEM= ("Host:");//
const char HANDSHAKE_ORIGIN [] PROGMEM= ("Origin:");//
const char HANDSHAKE_PRAGMA [] PROGMEM= ("Pragma:");//ignore
const char HANDSHAKE_CACHECON [] PROGMEM= ("Cache-Control:");
const char HANDSHAKE_SECWSKEY [] PROGMEM= ("Sec-WebSocket-Key:");//
const char HANDSHAKE_SECWSVER [] PROGMEM= ("Sec-WebSocket-Version:");//
const char HANDSHAKE_EXTEN [] PROGMEM= ("Sec-WebSocket-Extensions:");//ignore

const char HANDSHAKE_HEADER [] PROGMEM= ("HTTP/1.1 101 Switching Protocols\r\n\
Upgrade: websocket\r\n\
Connection: Upgrade\r\n\
Sec-WebSocket-Accept: ");


const char HANDSHAKE_GUIDAPPEND[] PROGMEM="258EAFA5-E914-47DA-95CA-C5AB0DC85B11";


bool CheckHead(char* str,char* pattern)
{
	char i=0;
	char ch= MEMCODE_(pattern);
		//Serial.println();
	for(;ch!=0;str++)
	{	
		//Serial.print(ch);
		if(*str!=ch)return false;
		i++;
		ch= MEMCODE_(pattern+i);
	}
		//Serial.println();
	return true;
}

void WebSocketProtocol::printState()
{
	switch(state)
	{
		case DISCONNECTED: Serial.print(F("DISCONNECTED"));break;
		case WS_HANDSHAKE: Serial.print(F("WS_HANDSHAKE"));break;
		case WS_CONNECTED: Serial.print(F("WS_CONNECTED"));break;
		case UNKNOWN_CONNECTED: Serial.print(F("UNKNOWN_CONNECTED"));break;
	}
}

void WebSocketProtocol::printRecvOPState()
{
	switch(recvOPState)
	{
		case WSOP_CLOSE: Serial.print(F("WSOP_CLOSE"));break;
		case WSOP_OK: Serial.print(F("WSOP_OK"));break;
		case WSOP_UNKNOWN: Serial.print(F("WSOP_UNKNOWN"));break;
	}
}

WebSocketProtocol::WebSocketProtocol(const char *urlPrefix ):
    socket_urlPrefix(urlPrefix)
{
	
    rmClientOBJ();
}

//if you see decodeRecvPkg return NULL call it
WSState WebSocketProtocol::getState()
{
	return state;
}

RecvOP WebSocketProtocol::getRecvOPState()
{
	return recvOPState;
}

char * WebSocketProtocol::processRecvPkg(char *str, unsigned int length)
{
	if (state == DISCONNECTED||
	(*str=='H'&&CheckHead(str,HANDSHAKE_GETHTTP)) ) {
		if (doHandshake(str,length)) {
		
			state = WS_HANDSHAKE;
			return NULL;
		}
		state = UNKNOWN_CONNECTED;
		return str;
	}
	if (state == UNKNOWN_CONNECTED)return str;
	//else if(state == WS_CONNECTED)
	state = WS_CONNECTED;
	return decodeFrame(str, length,&recvFrameInfo);
}

void WebSocketProtocol::maskData(char *data, unsigned int length,byte* mask_4)
{
	byte *mask=mask_4;
		
		char* tmpPtr=data;
		byte maskC=0;
		
		unsigned int L=length>>3;
		for ( ;L; L--)//try to minimize computation time
		{
			*tmpPtr^= mask[0]; tmpPtr++;
			*tmpPtr^= mask[1]; tmpPtr++;
			*tmpPtr^= mask[2]; tmpPtr++;
			*tmpPtr^= mask[3]; tmpPtr++;

			*tmpPtr^= mask[0]; tmpPtr++;
			*tmpPtr^= mask[1]; tmpPtr++;
			*tmpPtr^= mask[2]; tmpPtr++;
			*tmpPtr^= mask[3]; tmpPtr++;
		}
		unsigned int i;
		L=(length&0x7);
		for (i=0;i<L;i++,tmpPtr++)
			*tmpPtr^=mask[i&3];
}

char * WebSocketProtocol::decodeFrame(char *str, unsigned int length,WebSocketProtocol::WPFrameInfo *frameInfo)
{
	char* striter=str;
	if(length<2)
	{
		recvOPState=WSOP_UNKNOWN;
		return NULL;
	}
	
    byte bite=*(striter++);
	
    frameInfo->opcode = bite & 0xf; // Opcode
    frameInfo->isFinal = bite & 0x80; // Final frame?
    // Determine length (only accept <= 64 for now)
    bite =*(striter++);
    frameInfo->isMasking = bite & 0x80; // Length of payload
    frameInfo->length = bite & 0x7f; // Length of payload
	
	if (frameInfo->length==126) {
		frameInfo->length=(byte)striter[0];
		frameInfo->length<<=8;
		frameInfo->length|=(byte)striter[1];
			
		striter+=2;	
    }
	else if(frameInfo->length==127) {
		frameInfo->length=~((unsigned int)0);//max unsigned int
	//imposible for arduino..... 8Byte (64b)Length in total
		frameInfo->length64=(byte)(striter[0]&0x7F);//the MSB must be 0
		
		for(int j=0;j<7;j++)
		{
			frameInfo->length64<<=8;
			frameInfo->length64|=(byte)striter[1+j];
		}
		
		 striter+=8;
		   // return false;
		if(frameInfo->length64<~((unsigned int)0))
		  frameInfo->length=frameInfo->length64;
		else
		{
		  recvOPState=WSOP_UNKNOWN;
		  return false;
		}
    }
	
	if(frameInfo->isMasking)
	{
		frameInfo->mask[0] = *(striter++);
		frameInfo->mask[1] = *(striter++);
		frameInfo->mask[2] = *(striter++);
		frameInfo->mask[3] = *(striter++);
		
		maskData(striter, frameInfo->length,(byte*)frameInfo->mask);
	
	}
    /*if (!frameInfo->isFinal) {
    }*/
	switch (frameInfo->opcode) {
        case 0x01: // Txt frame
			striter[frameInfo->length]='\0';
        case 0x00: //cont frame
        case 0x02: // binary frame
			recvOPState=WSOP_OK;
			return striter;
            
        case 0x08:
            // Close frame. Answer with close and terminate tcp connection
            // TODO: Receive all bytes the client might send before closing? No?
            recvOPState=WSOP_CLOSE;
            return NULL;
            
        default:
            // Unexpected. Ignore. Probably should blow up entire universe here, but who cares.
    		
			recvOPState=WSOP_UNKNOWN;
			return NULL;
    }
	recvOPState=WSOP_UNKNOWN;
	return NULL;
}

char * WebSocketProtocol::codeFrame(char *dat_padLeast8B, unsigned int length,WebSocketProtocol::WPFrameInfo *frameInfo,unsigned int *totalL)
{
	char* striter=dat_padLeast8B;
	char* retDat;
	frameInfo->length=length;
	retDat=dat_padLeast8B-2;// the simplest case: op;len;[data]
	if(frameInfo->isMasking)
	{
		maskData(striter, length,frameInfo->mask);
		retDat-=4;
		retDat[2]=frameInfo->mask[0];
		retDat[3]=frameInfo->mask[1];
		retDat[4]=frameInfo->mask[2];
		retDat[5]=frameInfo->mask[3];
		// op;len;mask0;mask1;mask2;mask3;[data]
	}
	
	if(length<126)// op;len;(0/4 mask);[data]
		retDat[1]=((frameInfo->isMasking)?0x80:0)|length;
	else
	{
		// op;len(126);[2B Length];(0/4 mask);[data]
		retDat-=2;//2 more Byte for Length 
		retDat[1]=((frameInfo->isMasking)?0x80:0)|126;
		retDat[2]=length>>8;
		retDat[3]=length&0xFF;
	}
	
	retDat[0]=
	((frameInfo->isFinal)?0x80:0)|frameInfo->opcode;
	
	*totalL=dat_padLeast8B-retDat+length;
	return retDat;
}


WebSocketProtocol::WPFrameInfo WebSocketProtocol::getPkgframeInfo()
{
	return recvFrameInfo;
}

char * WebSocketProtocol::doHandshake(char *str, unsigned int length){

	if(!CheckHead(str,HANDSHAKE_GETHTTP))
	{
		state=UNKNOWN_CONNECTED;
		return NULL;	
	}
	
    char *temp=str;
	//str[length]='\0';
	str+=sizeof(HANDSHAKE_GETHTTP);
    char *bite_ptr=str;
	
	
    char key[80];
    char bite;
    
    bool hasUpgrade = false;
    bool hasConnection = false;
    bool isSupportedVersion = false;
    bool hasHost = false;
    bool hasOrigin = false;
    bool hasKey = false;
    byte counter = 0;
	
	byte L=length;
	
		//Serial.println(bite_ptr);

    for (;*bite_ptr;) {
	    if(*bite_ptr != '\n'){
		
			bite_ptr++;
			continue;
		}
		
		
        
		*(bite_ptr-1)=0;
		bite_ptr++;if(*bite_ptr==NULL)break;
		
	
	
	
	   // temp[counter - 2] = 0; // Terminate string before CRLF \r\n
		
		// Ignore case when comparing and allow 0-n whitespace after ':'. See the spec:
		// http://www.w3.org/Protocols/rfc2616/rfc2616-sec4.html
		if (!hasUpgrade && CheckHead(bite_ptr,HANDSHAKE_UPGRAGE)) {
			// OK, it's a websockets handshake for sure
			bite_ptr+=strlen(HANDSHAKE_UPGRAGE);
			hasUpgrade = true;	
		} else if (!hasConnection && CheckHead(bite_ptr, HANDSHAKE_CONNECTION)) {
			bite_ptr+=strlen(HANDSHAKE_CONNECTION);
			hasConnection = true;
		} else if (!hasOrigin && CheckHead(bite_ptr, HANDSHAKE_ORIGIN)) {
			bite_ptr+=strlen(HANDSHAKE_ORIGIN);
			hasOrigin = true;
		} else if (!hasHost && CheckHead(bite_ptr, HANDSHAKE_Host)) {
			bite_ptr+=strlen(HANDSHAKE_Host);
			hasHost = true;
		} else if (!hasKey && CheckHead(bite_ptr, HANDSHAKE_SECWSKEY)) {
			bite_ptr+=strlen(HANDSHAKE_SECWSKEY);
			for(;*bite_ptr==' ';bite_ptr++)//skip space
				if(!*bite_ptr){state=UNKNOWN_CONNECTED;return NULL;}//sudden termination error
			byte tmpc=0;
			for(;*bite_ptr!='\r';tmpc++,bite_ptr++)//Copy key and prevent sudden termination error
				if(!*bite_ptr){state=UNKNOWN_CONNECTED;return NULL;}
				else key[tmpc]=*bite_ptr;
			key[tmpc]='\0';
			
			hasKey=true;
		} else if (!isSupportedVersion && CheckHead(bite_ptr,HANDSHAKE_SECWSVER)
		&& strstr(bite_ptr+strlen(HANDSHAKE_SECWSVER)+1, "13")) {
			bite_ptr+=strlen(HANDSHAKE_SECWSVER)+3;
			isSupportedVersion = true;
			if (hasUpgrade && hasConnection && isSupportedVersion 
			&& hasHost && hasOrigin && hasKey)
				break;//usually the last one 
		}
           
        
    }
	
	bite_ptr=temp;
    // Assert that we have all headers that are needed. If so, go ahead and
    // send response headers.
    if (hasUpgrade && hasConnection && isSupportedVersion && hasHost && hasOrigin && hasKey) {
        //strcat(key,HANDSHAKE_GUIDAPPEND); // Add the omni-valid GUID
		
        strcpy_P(key+strlen(key), HANDSHAKE_GUIDAPPEND);
		
        Sha1.init();
        Sha1.print(key);
        uint8_t *hash = Sha1.result();
		strcpy_P(bite_ptr,HANDSHAKE_HEADER);
		bite_ptr+=strlen(HANDSHAKE_HEADER);
	
        base64_encode(bite_ptr, (char*)hash, 20);
		for(;*bite_ptr;bite_ptr++);
		strcpy(bite_ptr,CRLF);
		bite_ptr+=sizeof(CRLF)-1;
		strcpy(bite_ptr,CRLF);
		bite_ptr+=sizeof(CRLF)-1;
		
		//Serial.println(bite_ptr);
		
    } else {
        // Nope, failed handshake. Disconnect
		state=UNKNOWN_CONNECTED;
        return NULL;
    }
    
	state=WS_HANDSHAKE;/**/
    return temp;
}

char * WebSocketProtocol::codeSendPkg_setPkgL(char *Pkg, unsigned int length)
{
	Pkg[0]=(uint8_t) 0x81;
	Pkg[1]=(uint8_t) length;
}

char * WebSocketProtocol::codeSendPkg_getPkgContentSec(char *Pkg)
{
	return Pkg+2;
}

char * WebSocketProtocol::codeSendPkg_endConnection(char *Pkg)
{
    Pkg[0]= 0x08;
    Pkg[1]= 0x02;
    Pkg[2]= 0x03;
    Pkg[3]= 0xf1;
    Pkg[4]= 0;
	return Pkg;
}

void WebSocketProtocol::setClientOBJ(EthernetClient client)
{
		clientOBJ=client;
}

EthernetClient WebSocketProtocol::getClientOBJ()
{
    return clientOBJ;
}

bool WebSocketProtocol::alive()
{
  return recvOPState!=WSOP_CLOSE;
}

void WebSocketProtocol::rmClientOBJ()
{
	clientOBJ.sockindex = MAX_SOCK_NUM;
	state = DISCONNECTED;
	recvOPState=WSOP_CLOSE;
  clientOBJ=0;
}
