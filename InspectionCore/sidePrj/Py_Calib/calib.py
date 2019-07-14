import numpy as np
import cv2
import glob
import json
import socket

def start_tcp_server(host, port):
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.bind((host, port))
    #sock.settimeout(10)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.listen(1)
    return sock

def start_tcp_serverX(host, port):    
    sock = start_tcp_server('',1229)

    while True:
        (csock, adr) = sock.accept()
        print( "Client Info: ", csock, adr)
        msg = csock.recv(1024)
        if not msg:
            pass
        else:
            msg=msg.decode('utf-8')
            print ("Client send: " + msg)
            csock.send("Hello I'm Server.\r\n".encode())
        csock.close()


def chessBoardCalib(chessBoardDim,dir):
    # termination criteria
    criteria = (cv2.TERM_CRITERIA_EPS + cv2.TERM_CRITERIA_MAX_ITER, 30, 0.001)

    chessBoardDim=(7,9)
    # prepare object points, like (0,0,0), (1,0,0), (2,0,0) ....,(6,5,0)
    objp = np.zeros((chessBoardDim[0]*chessBoardDim[1],3), np.float32)
    objp[:,:2] = np.mgrid[0:chessBoardDim[0],0:chessBoardDim[1]].T.reshape(-1,2)

    # Arrays to store object points and image points from all the images.
    objpoints = [] # 3d point in real world space
    imgpoints = [] # 2d points in image plane.

    images = glob.glob(dir+'*.jpg')
    print(images)
    
    if len(images) == 0 :
        return None
    for fname in images:
        img = cv2.imread(fname)
        gray = cv2.cvtColor(img,cv2.COLOR_BGR2GRAY)

        # Find the chess board corners
        ret, corners = cv2.findChessboardCorners(gray,chessBoardDim,None)

        # If found, add object points, image points (after refining them)
        if ret == True:
            objpoints.append(objp)

            corners2 = cv2.cornerSubPix(gray,corners,(11,11),(-1,-1),criteria)
            imgpoints.append(corners2)


            # Draw and display the corners
            #img = cv2.drawChessboardCorners(img, (7,6), corners2,ret)
            #cv2.imshow('img',img)
            #cv2.waitKey(500)
    cv2.destroyAllWindows()
    # print( gray.shape[::-1])
    ret, mtx, dist, rvecs, tvecs = cv2.calibrateCamera(objpoints, imgpoints, gray.shape[::-1], None, None)
    return (ret, mtx, dist, rvecs, tvecs)
    
    
    # dst = cv2.undistort(gray, mtx, dist, None, mtx)
    # mapx,mapy = cv2.initUndistortRectifyMap(mtx,dist,None,mtx,gray.shape[::-1],cv2.CV_32FC1)
    # #cv2.imshow('img',dst)
    # #cv2.waitKey(5000)
    # print(ret,mtx,dist,rvecs,tvecs)
    # mapx=mapx.tolist()
    # mapy.tolist()

    # print()


    # outputfilename="mapx.json"
    # with open(outputfilename, 'w') as outfile:
    #     json.dump(mapx, outfile)

(ret, mtx, dist, rvecs, tvecs)=chessBoardCalib((),"")
mtx=mtx.tolist()
dist=dist.tolist()
rvecs[0]=rvecs[0].tolist()
tvecs[0]=tvecs[0].tolist()

calibRes={"ret":ret, "mtx":mtx, "dist":dist, "rvecs":rvecs, "tvecs":tvecs}
print(calibRes)
outputfilename="mapx.json"
with open(outputfilename, 'w') as outfile:
    json.dump(calibRes, outfile)