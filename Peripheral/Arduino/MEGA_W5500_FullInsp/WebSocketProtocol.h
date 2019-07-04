/*
WebSocketProtocol-Arduino, a simple WebSocketProtocol implementation for Arduino

Copyright 2014 MDM  Performance Improving modification(Rely on lib Eth_Boost&RingBuff)

Based on previous implementations by
Copyright 2012 Per Ejeklint

Based on previous implementations by
Copyright 2010 Ben Swanson
and
Copyright 2010 Randall Brewer
and
Copyright 2010 Oliver Smith


Some code and concept based off of Webduino library
Copyright 2009 Ben Combee, Ran Talbott

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.

-------------
Now based off version 13
http://datatracker.ietf.org/doc/rfc6455/?include_text=1

Modified by Alexandros Baltas, 2013
www.codebender.cc

*/


#ifndef WEBSOCKETPRO_H_
#define WEBSOCKETPRO_H_


#include <Arduino.h> // Arduino 1.0 or greater is required
#include <stdlib.h>

#define private public //cheating- to access private _sock
#include <Ethernet.h>
#undef private
//#include <Ethernet.h>// Just for manage client, it doesn't do data transfer.


#ifndef null
#define null 0
#endif

// CRLF characters to terminate lines/handshakes in headers.
#define CRLF "\r\n"

    typedef enum WSState{DISCONNECTED,WS_HANDSHAKE,WS_CONNECTED,UNKNOWN_CONNECTED}WSState;
	typedef enum RecvOP{WSOP_CLOSE,WSOP_OK,WSOP_UNKNOWN}RecvOP; 
	
class WebSocketProtocol {
public:

	typedef struct {
    bool isFinal;
    byte opcode;
    byte mask[4];
	byte isMasking;
    unsigned int length;
    unsigned long long length64;
	}WPFrameInfo;
	/*
	DISCONNECTED---(handshake pkg)-->HANDSHAKE--(Send handshake pkg)-->
	CONNECTED--(Send close pkg)-->DISCONNECTED
	
	
	*/
    WebSocketProtocol(const char *urlPrefix = "/");
	
    //send(codeSendPkg(data2Send,Len))
    char * codeSendPkg(char *str, unsigned int length);
	
    //byte*recvdata=decodeRecvPkg(recvdata(), Len);
    char * processRecvPkg(char *str, unsigned int length);
    WSState getState();
    RecvOP getRecvOPState();
	void printState();
	void printRecvOPState();
  void rmClientOBJ();
  bool alive();
	void setClientOBJ(EthernetClient cli);
	EthernetClient getClientOBJ();
	char * codeSendPkg_setPkgL(char *Pkg, unsigned int length);
	
	char * codeSendPkg_getPkgContentSec(char *Pkg);
	char * codeSendPkg_endConnection(char *Pkg);
	
	WebSocketProtocol::WPFrameInfo getPkgframeInfo();
	
    char * decodeFrame(char *str, unsigned int length,WebSocketProtocol::WPFrameInfo *frameInfo);
    char * codeFrame(char *str_padLeast9B, unsigned int length,WebSocketProtocol::WPFrameInfo *frameInfo,unsigned int *totalL);
	void maskData(char *data, unsigned int length,byte* mask_4);
private:
    EthernetClient clientOBJ;
    WPFrameInfo recvFrameInfo;
    WSState state;
	RecvOP recvOPState;
    const char *socket_urlPrefix;

    // Discovers if the client's header is requesting an upgrade to a
    // websocket connection.
    char * doHandshake(char *str, unsigned int length);
    
    // Reads a frame from client. Returns false if user disconnects, 
    // or unhandled frame is received. Server must then disconnect, or an error occurs.
    
	
	
};

#endif
