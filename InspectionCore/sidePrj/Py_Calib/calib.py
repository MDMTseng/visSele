import numpy as np
import cv2
import glob
import json
import socket
import time


def start_tcp_server(host, port):
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.bind((host, port))
    #sock.settimeout(10)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.listen(1)
    return sock



#{"type":"cameraCalib","pgID":12442,"img_path":"*.jpg","board_dim":[7,9]}
def cameraCalib(msg):
    print(msg)
    if "img_path" not in msg:
        return
    if "board_dim" not in msg:
        return
    return chessBoardCalibResFormat(chessBoardCalib((msg["board_dim"][0],msg["board_dim"][1]),msg["img_path"]))



def cameraCalib_RectifyMap(msg):

    ret = cameraCalib(msg)
    if ret is None:
        return
    mtx=cv2.UMat(np.array(ret["mtx"],dtype=np.float32))
    dist=cv2.UMat(np.array(ret["dist"],dtype=np.float32))
    mapx,mapy = cv2.initUndistortRectifyMap(mtx,dist,None,mtx,(3000,2000),cv2.CV_32FC1)
    #print(mapx,mapy)
    return {"mapx":mapx.get().tolist(),"mapy":mapy.get().tolist()}



def _exit_(msg):
    return {"doEXIT":True}

openCVCmd={
    "cameraCalib":cameraCalib,
    "cameraCalib_RectifyMap":cameraCalib_RectifyMap,
    "exit":_exit_,
}


def start_tcp_serverX(host, port):    
    sock = start_tcp_server(host, port)
    doEXIT=False
    while not doEXIT:
        print( "Waiting for new Client")
        (csock, adr) = sock.accept()
        print( "Client Info: ", csock, adr)
        while not doEXIT:
            msg_json = csock.recv(1024)
            if not msg_json:
                #pass
                break
            try:
                msg_json = msg_json.decode('utf-8')
                print(msg_json)
                msg = json.loads(msg_json)
            except Exception as e:
                print('exception happened...',e)
                break

            if 'type' not in msg:
                break
            
            magType=msg['type']

            if magType not in openCVCmd:
                break

            cmdExec=openCVCmd[magType]

            retDict = cmdExec(msg)
            ACK=retDict is not None

            if retDict is None:
                retDict={}

            retDict["ACK"]=ACK
            if "pgID" in msg:
                retDict["pgID"]=msg["pgID"]
            calibRes_json = json.dumps(retDict)
            #print ("Client send: " + calibRes_json)
            # print(calibRes)
            #msg=msg.decode('utf-8')
            csock.send(calibRes_json.encode())
            
            if "doEXIT" in  retDict and retDict["doEXIT"] == True:
                doEXIT=True
        csock.close()
        

def chessBoardCalib(chessBoardDim,image_path):
    # termination criteria
    criteria = (cv2.TERM_CRITERIA_EPS + cv2.TERM_CRITERIA_MAX_ITER, 30, 0.001)

    # prepare object points, like (0,0,0), (1,0,0), (2,0,0) ....,(6,5,0)
    objp = np.zeros((chessBoardDim[0]*chessBoardDim[1],3), np.float32)
    objp[:,:2] = np.mgrid[0:chessBoardDim[0],0:chessBoardDim[1]].T.reshape(-1,2)

    # Arrays to store object points and image points from all the images.
    objpoints = [] # 3d point in real world space
    imgpoints = [] # 2d points in image plane.

    images = glob.glob(image_path)
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
    #cv2.destroyAllWindows()
    # print( gray.shape[::-1])

    src_pts = np.float32(imgpoints[0]).reshape(-1,1,2)
    dst_pts = np.float32(objpoints[0]).reshape(-1,1,3)
    H, mask = cv2.findHomography(src_pts, dst_pts, cv2.RANSAC,5.0)
    print(H,mask)
    ret, mtx, dist, rvecs, tvecs = cv2.calibrateCamera(objpoints, imgpoints, gray.shape[::-1], None, None)
    return {"ret":ret, "mtx":mtx, "dist":dist}#, "rvecs":rvecs, "tvecs":tvecs}
    

def chessBoardCalibResFormat(calibRes):
    # dst = cv2.undistort(gray, mtx, dist, None, mtx)
    # mapx,mapy = cv2.initUndistortRectifyMap(mtx,dist,None,mtx,gray.shape[::-1],cv2.CV_32FC1)
    # #cv2.imshow('img',dst)
    # #cv2.waitKey(5000)

    calibRes['mtx']=calibRes['mtx'].tolist()
    calibRes['dist']=calibRes['dist'].tolist()
    # calibRes['rvecs']=list(map(lambda ele:ele.tolist(),calibRes['rvecs']))
    # calibRes['tvecs']=list(map(lambda ele:ele.tolist(),calibRes['tvecs']))
    return calibRes


#start_tcp_serverX("", 1229)

calibRes=chessBoardCalib((6,9),'*.png')
print(calibRes)

img = cv2.imread(glob.glob('*.png')[0])
h,  w = img.shape[:2]
newcameramtx, roi=cv2.getOptimalNewCameraMatrix(calibRes['mtx'],calibRes['dist'],(w,h),1,(w,h))
dst = cv2.undistort(img, calibRes['mtx'],calibRes['dist'], None, newcameramtx)
#dst = cv2.warpPerspective(img,newcameramtx,img.shape[:2])
cv2.imshow('img',dst)
cv2.waitKey()


518
# calibRes_json = json.dumps(calibRes)
# outputfilename="mapx.json"
# with open(outputfilename, 'w') as outfile:
#     outfile.write(calibRes_json)