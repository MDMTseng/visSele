import socket
import select
import json
import schedule
import time
HEADER_LENGTH = 10
import copy
IP = "127.0.0.1"
PORT = 1234

# Create a socket
# socket.AF_INET - address family, IPv4, some otehr possible are AF_INET6, AF_BLUETOOTH, AF_UNIX
# socket.SOCK_STREAM - TCP, conection-based, socket.SOCK_DGRAM - UDP, connectionless, datagrams, socket.SOCK_RAW - raw IP packets
server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

# SO_ - socket option
# SOL_ - socket option level
# Sets REUSEADDR (as a socket option) to 1 on socket
server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)

# Bind, so server informs operating system that it's going to use given IP and port
# For a server using 0.0.0.0 means to listen on all available interfaces, useful to connect locally to 127.0.0.1 and remotely to LAN interface IP
server_socket.bind((IP, PORT))

# This makes server listen to new connections
server_socket.listen()

# List of sockets for select.select()
sockets_list = [server_socket]

# List of connected clients - socket as a key, user header and name as data
clients = {}

print(f'Listening for connections on {IP}:{PORT}...')

# Handles message receiving
def receive_message(client_socket):

    try:

        # Receive our "header" containing message length, it's size is defined and constant
        return client_socket.recv(999)


    except:

        # If we are here, client closed connection violently, for example by pressing ctrl+c on his script
        # or just lost his connection
        # socket.close() also invokes socket.shutdown(socket.SHUT_RDWR) what sends information about closing the socket (shutdown read/write)
        # and that's also a cause when we receive an empty message
        return False


machSetup = {
  "type": "get_setup_rsp",
  "ver": "0.0.0.0",
  "state_pulseOffset": [
    0,
    654,
    657,
    659,
    660,
    697,
    750,
    900,
    910,
    1073,
    1083
  ],
  "perRevPulseCount": 2400,
  "subPulseSkipCount": 16,
  "pulse_hz": 0,
  "mode": "NORMAL",
}

machState={
  "error_codes": [
    2
  ],
  "res_count":{
    "OK": 40,
    "NG": 10,
    "NA": 1,
    "ERR": 1
  }
}


def machUpdate_1s():
  None
  # machState["res_count"]["NG"]+=1

def machUpdate_100s():
  None
  # if machState["error_codes"] is None:
  #   machState["error_codes"]=[]
  # machState["error_codes"].append(2)
  # machState["res_count"]["NA"]+=1


schedule.every(1).seconds.do(machUpdate_1s)
schedule.every(100).seconds.do(machUpdate_100s)

while True:

    # Calls Unix select() system call or Windows select() WinSock call with three parameters:
    #   - rlist - sockets to be monitored for incoming data
    #   - wlist - sockets for data to be send to (checks if for example buffers are not full and socket is ready to send some data)
    #   - xlist - sockets to be monitored for exceptions (we want to monitor all sockets for errors, so we can use rlist)
    # Returns lists:
    #   - reading - sockets we received some data on (that way we don't have to check sockets manually)
    #   - writing - sockets ready for data to be send thru them
    #   - errors  - sockets with some exceptions
    # This is a blocking call, code execution will "wait" here and "get" notified in case any action should be taken
    read_sockets, _, exception_sockets = select.select(sockets_list, [], sockets_list)


    # Iterate over notified sockets
    for notified_socket in read_sockets:

        # If notified socket is a server socket - new connection, accept it
        if notified_socket == server_socket:

            # Accept new connection
            # That gives us new socket - client socket, connected to this given client only, it's unique for that client
            # The other returned object is ip/port set
            client_socket, client_address = server_socket.accept()

            print("NEW CONNECTION:")
            print(client_address)
            machState["error_codes"].append(2)

            # Add accepted socket to select.select() list
            sockets_list.append(client_socket)

            # Also save username and username header
            clients[client_socket] = client_socket

            print('Accepted new connection from {}'.format(*client_address))

        # Else existing socket is sending a message
        else:

            # Receive message
            message = notified_socket.recv(999);
            
            # If False, client disconnected, cleanup
            if len(message) == 0:
                # print('Closed connection from: {}'.format(clients[notified_socket]['data'].decode('utf-8')))

                # Remove from list for socket.socket()
                sockets_list.remove(notified_socket)

                # Remove from our list of users
                del clients[notified_socket]

                continue
            schedule.run_pending()


            user = clients[notified_socket]


            segIdxes=[-1]
            isInString=False
            pCounter=1
            for idx in range(1, len(message)):
              
              # print(message[idx])
              
              if(message[idx]==ord('"')):#deal with the { or } in the  string
                isInString=not isInString
              if isInString==True:
                continue

              if(message[idx]==ord('{')):
                pCounter+=1
              elif (message[idx]==ord('}')):
                pCounter-=1
                if(pCounter==0):
                  segIdxes.append(idx)
              


            print("\n")
            
            
            for idx in range(1, len(segIdxes)):

              jmsg = json.loads(message[segIdxes[idx-1]+1:segIdxes[idx]+1])
              _id=None
              if "id" in jmsg:
                _id=jmsg["id"]
              # the result is a Python dictionary:
              msg_type=jmsg["type"]
              retMsg={}
              if msg_type == "PING":
                retMsg=copy.copy(machState)
                retMsg["type"]="PONG"
              if msg_type == "inspRep":
                _id=None
                if jmsg["status"]==0:
                  machState["res_count"]["OK"]+=1
                elif jmsg["status"]==-1:
                  machState["res_count"]["NG"]+=1
                else:
                  machState["res_count"]["NA"]+=1


                print(jmsg)
              elif msg_type == "get_setup":
                retMsg=copy.copy(machSetup)
                retMsg["type"]="get_setup_rsp"
              elif msg_type == "set_setup":
                retMsg["type"]="set_setup_rsp"
                if "mode" in jmsg:
                  machSetup["mode"]=jmsg["mode"]
                if "state_pulseOffset" in jmsg:
                  machSetup["state_pulseOffset"]=jmsg["state_pulseOffset"]
                if "pulse_hz" in jmsg:
                  machSetup["pulse_hz"]=jmsg["pulse_hz"]
                print(jmsg)
                print(machSetup)
              elif msg_type == "res_count_clear":
                machState["res_count"]={
                    "OK": 0,
                    "NG": 0,
                    "NA": 0,
                    "ERR": 0
                  }
              elif msg_type == "error_clear":
                machState["error_codes"]=None
                
              if _id is not None:
                retMsg["id"]=_id
                notified_socket.send(json.dumps(retMsg).encode("utf-8"))
              # print(f'Received message from {user["data"].decode("utf-8")}: {message["data"].decode("utf-8")}')

            # Iterate over connected clients and broadcast message
            for client_socket in clients:

                # But don't sent it to sender
                if client_socket != notified_socket:
                    # client_socket.send(user['header'] + user['data'] + message['header'] + message['data'])
                    # Send user and message (both with their headers)
                    # We are reusing here message header sent by sender, and saved username header send by user when he connected
                    #client_socket.send(user['header'] + user['data'] + message['header'] + message['data'])
                    None

    # It's not really necessary to have this, but will handle some socket exceptions just in case
    for notified_socket in exception_sockets:

        # Remove from list for socket.socket()
        sockets_list.remove(notified_socket)

        # Remove from our list of users
        del clients[notified_socket]