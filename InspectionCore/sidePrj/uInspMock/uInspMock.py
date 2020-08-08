import socket
import socketserver
import time
import threading
import json








def findJsonResetEnd(json_barr):
  if(len(json_barr)==0):
    return -1
  for idx in range(len(json_barr)-1):
    ch = json_barr[idx]
    if(ch!=ord('}')):
      continue
    ch = json_barr[idx+1]
    if(ch==ord('{')):
      return idx
  return -2


def findJsonSecEnd(json_barr):
  if(len(json_barr)==0):
    return -3
  pSum=0  
  if(json_barr[0]==ord('{')):
    for idx in range(len(json_barr)):
      ch = json_barr[idx]
      if(ch==ord('{')):
        pSum+=1
      if(ch==ord('}')):
        pSum-=1

      if(pSum==0):#section closed
        return idx
    return -1
  return -2



def jsonMsgStackingParse(bufferByte):
  jsonSecs=[]
  cutSecs=[]
  #print("NEW data>>",bufferByte)
  rescan=True
  while(rescan):
    rescan=False
    secEnd = findJsonSecEnd(bufferByte)
    resetEnd = -1
    if(secEnd>0):
      rescan=True
      secEnd+=1
      sec = bufferByte[0:secEnd]
      bufferByte = bufferByte[secEnd:len(bufferByte)]
      jsonSecs.append(sec)
    else:
      resetEnd = findJsonResetEnd(bufferByte)
      if(resetEnd>=0):#cutOff
        rescan=True
        resetEnd+=1
        cutSecs.append(bufferByte[0:resetEnd])
        bufferByte = bufferByte[resetEnd:len(bufferByte)]

  return bufferByte,jsonSecs,cutSecs
        

# bufferByte=bytearray(b'')

# byteArray_s=[bytearray(b'{'),bytearray(b'"a":{"b":'),bytearray(b'[50]}}{"c":{"b":[5]}'),bytearray(b'}{}')]

# for recvIdx in range(len(byteArray_s)):
#   bufferByte+=byteArray_s[recvIdx]
#   bufferByte,jsonSecs,cutSecs = jsonMsgStackingParse(bufferByte)
#   print(bufferByte,jsonSecs,cutSecs)



def CMDExec(data_json,connectionSocket):


  
  print(data_json,len(data_json))
  data_dict = json.loads(data_json)
  _type = data_dict['type']
  

  response_dict={}
  _id=None
  if "id" in data_dict:
    _id=data_dict['id']
    response_dict['id']=_id


  if(_type == 'inspRep'):
    return
  elif(_type == 'get_setup'):
    response_dict['type']='get_setup_rsp'
    response_dict['ver']='v0.0.0'
    response_dict['state_pulseOffset']=[0, 654 ,657,659, 660,  697, 750,  900,910,1073,1083]
    response_dict['perRevPulseCount']=2400
    response_dict['subPulseSkipCount']=8
    response_dict['pulse_hz']=0
  elif(_type == 'PING'):
    response_dict['type']='PONG'
    response_dict['error_codes']=[2,2]
    response_dict['res_count']={
      'OK':100,
      'NG':300,
      'NA':9999,
      'ERR':0
    }
    

  print(response_dict)
  response_json = json.dumps(response_dict)  
  connectionSocket.send(response_json.encode())

  #elif(type == 'PING'):



serverPort = 50007
serverSocket =socket.socket(socket.AF_INET,socket.SOCK_STREAM)
serverSocket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
serverSocket.bind (('',serverPort))
serverSocket.listen(1)


while 1:
  print('The server is ready to receive')
  connectionSocket,addr = serverSocket.accept()
  byteArray=bytearray(b'')
  while 1:
    recv_data = connectionSocket.recv(1024)
    if(len(recv_data)==0):break

    byteArray += bytearray(recv_data)
    print("oooo:",byteArray)
    byteArray,jsonSecs,cutSecs = jsonMsgStackingParse(byteArray)
    if(len(cutSecs)!=0):
      print("XXXXXXXXXXXXXXXXXXXXXXXX",byteArray,jsonSecs,cutSecs)
    
    for jsonSec in jsonSecs:
      CMDExec(jsonSec,connectionSocket)


  connectionSocket.close() 