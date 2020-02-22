#!/usr/bin/python
#coding=UTF-8
 
"""
TCP Client sample
"""
import time
from socket import *

# target_host = "www.google.com"
# target_port = 80
 
target_host = "192.168.2.43"
target_port = 5213
# client.send("{\"type\":\"PING\"}".encode())
 

clientSocket =socket(AF_INET,SOCK_STREAM)
clientSocket.connect((target_host,target_port))
time.sleep( 1 )
for i in range( 10):
  sentence = "{\"type\":\"PING\",\"id\":"+str(i)+"}"#input('Input lowercase sentence:')
  clientSocket.send(sentence.encode())
  modifiedSentence = clientSocket.recv(1024)
  print('From Server:',modifiedSentence.decode())



  sentence = "{\"type\":\"get_dev_info\",\"id\":"+str(i)+"}"#input('Input lowercase sentence:')
  clientSocket.send(sentence.encode())
  modifiedSentence = clientSocket.recv(1024)
  print('From Server:',modifiedSentence.decode())
  
  sentence = "{\"type\":\"get_setup\",\"id\":"+str(i)+"}"#input('Input lowercase sentence:')
  clientSocket.send(sentence.encode())
  modifiedSentence = clientSocket.recv(1024)
  print('From Server:',modifiedSentence.decode())
clientSocket.close()