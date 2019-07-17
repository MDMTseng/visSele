import socket 
 
s = socket.socket()
host = "localhost"
port = 1229 
 
s.connect((host, port))
s.send('{"type":"cameraCalib","pgID":12442,"img_path":"*.jpg","board_dim":[7,9]}'.encode())
print(s.recv(1024))
s.close()