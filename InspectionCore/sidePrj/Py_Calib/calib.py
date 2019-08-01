import numpy as np
import cv2
import glob
import json
import socket
import math
import time
import random


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

def fileConvert(image_path,src_ext,dst_ext):
    path=image_path
    images = glob.glob(path)
    print(images)
    for fname in images:
        img = cv2.imread(fname)
        newfnamename = fname.replace(src_ext,dst_ext)
        cv2.imwrite(newfnamename,img)





def findMainVecInfo(cornors,seedIdx=None):
    distArr=[]
    if(seedIdx==None):
        seedIdx=len(cornors)//2
    seedPt = cornors[seedIdx][0]
    for i in range(0, len(cornors)):
        if(i==seedIdx):
            continue
        curPt=cornors[i][0]
        dist = math.hypot(seedPt[0]-curPt[0], seedPt[1]-curPt[1])
        distArr.append({"dist":dist,"idx":i})
    distArr.sort(key=lambda distInfo: distInfo["dist"])
    nbInfo=distArr[0:4]

    print(nbInfo)
    #find if the top 4 neighbors has similar distance
    distRatio = nbInfo[3]["dist"]/nbInfo[0]["dist"]
    if(distRatio>1.2):
        return None

    for i in range(0, len(nbInfo)):
        idx = nbInfo[i]["idx"]
        curPt=cornors[idx][0]
        vec = [curPt[0]-seedPt[0], curPt[1]-seedPt[1]]
        norm= math.hypot(vec[0],vec[1])
        nbInfo[i]["vec"]=vec
        nbInfo[i]["nvec"]=[vec[0]/norm,vec[1]/norm]

    vec1 = {"vec":nbInfo[0]["vec"].copy(),"nvec":nbInfo[0]["nvec"].copy(),"info":[nbInfo[0]]}
    vec2 = {"vec":[0,0],"info":[]}
    for i in range(1, len(nbInfo)):
        nvecx = nbInfo[i]["nvec"]
        dotP = vec1["nvec"][0]*nvecx[0]+vec1["nvec"][1]*nvecx[1]
        if(dotP>0.8 or dotP<-0.8):
            vec1["info"].append(nbInfo[i])
        if(dotP<0.2 and dotP>-0.2):
            vec2["info"].append(nbInfo[i])

    vec1["vec"]=vec1["info"][0]["vec"].copy()
    vec1["dist"]=vec1["info"][0]["dist"]
    vec2["vec"]=vec2["info"][0]["vec"].copy()
    vec2["dist"]=vec2["info"][0]["dist"]


    if(math.fabs(vec1["vec"][0])>math.fabs(vec1["vec"][1])):#vec1 as X axis
        if(vec1["vec"][0]<0):
            vec1["vec"][0]*=-1
            vec1["vec"][1]*=-1
        if(vec2["vec"][1]<0):
            vec2["vec"][0]*=-1
            vec2["vec"][1]*=-1
        return {"vec1":vec1,"vec2":vec2}
    else:#vec2 as X axis
        if(vec1["vec"][1]<0):
            vec1["vec"][0]*=-1
            vec1["vec"][1]*=-1
        if(vec2["vec"][0]<0):
            vec2["vec"][0]*=-1
            vec2["vec"][1]*=-1
        return {"vec1":vec2,"vec2":vec1}

        

    



    return {"vec1":vec1,"vec2":vec2}

def close2one(num):
    if num>1 or num <-1:
        return 1/num
    return num

    

def genCornorsCoord(cornors):

    mainVecInfo=None
    while mainVecInfo==None:
        mainVecInfo = findMainVecInfo(cornors,random.randint(0,len(cornors)))

    
    seedIdx=0
    coordArr=[None]*len(cornors)
    vec1=mainVecInfo["vec1"]["vec"]
    vec2=mainVecInfo["vec2"]["vec"]

    magVec1=math.hypot(vec1[0],vec1[1])
    nnvec1=[vec1[0]/magVec1/magVec1,vec1[1]/magVec1/magVec1]
    magVec2=math.hypot(vec2[0],vec2[1])
    nnvec2=[vec2[0]/magVec2/magVec2,vec2[1]/magVec2/magVec2]


    print(mainVecInfo)

    searchList=[seedIdx]
    coordArr[seedIdx]=[0,0]

    coordEstThres=0.95
    while len(searchList)>0:
        curSListL=len(searchList)
        for i in range(0, curSListL):
            cur_search_idx=searchList[i]
            centPt = cornors[cur_search_idx][0]
            for j in range(0, len(cornors)):
                if not coordArr[j]==None:
                    continue
                cpt = cornors[j][0]
                vec = [cpt[0]-centPt[0],cpt[1]-centPt[1]]
                dist = math.hypot(vec[0],vec[1])
                distRatio=dist/mainVecInfo["vec1"]["dist"]
                if(distRatio>1):distRatio=1/distRatio
                if(distRatio>coordEstThres):
                    nndot1=nnvec1[0]*vec[0]+nnvec1[1]*vec[1]
                    c_one=close2one(math.fabs(nndot1))

                    if(nndot1>0 and c_one>coordEstThres):
                        coordArr[j]=coordArr[cur_search_idx].copy()
                        coordArr[j][0]+=1
                        searchList.append(j)
                        continue

                    if(nndot1<0 and c_one>coordEstThres):
                        coordArr[j]=coordArr[cur_search_idx].copy()
                        coordArr[j][0]-=1
                        searchList.append(j)
                        continue


                distRatio=dist/mainVecInfo["vec2"]["dist"]
                if(distRatio>1):distRatio=1/distRatio
                if(distRatio>coordEstThres):
                    nndot2=nnvec2[0]*vec[0]+nnvec2[1]*vec[1]
                    c_one=close2one(math.fabs(nndot2))
                    if(nndot2>0 and c_one>coordEstThres):
                        coordArr[j]=coordArr[cur_search_idx].copy()
                        coordArr[j][1]+=1
                        searchList.append(j)
                        continue

                    if(nndot2<0 and c_one>coordEstThres):
                        coordArr[j]=coordArr[cur_search_idx].copy()
                        coordArr[j][1]-=1
                        searchList.append(j)
                        continue

        searchList=searchList[curSListL:len(searchList)]

        

    return coordArr



def chessBoardCalibsss(chessBoardDim,image_path):
    # termination criteria
    criteria = (cv2.TERM_CRITERIA_EPS + cv2.TERM_CRITERIA_MAX_ITER, 30, 0.001)

    # prepare object points, like (0,0,0), (1,0,0), (2,0,0) ....,(6,5,0)
    # Arrays to store object points and image points from all the images.
    objpoints = [] # 3d point in real world space
    imgpoints = [] # 2d points in image plane.

    images = glob.glob(image_path)


    #fast = cv2.FastFeatureDetector_create(threshold=25,nonmaxSuppression = True)
    if len(images) == 0 :
        return None
    for fname in images:

        img = cv2.imread(fname)
        gray = cv2.cvtColor(img,cv2.COLOR_BGR2GRAY)


        corners = cv2.goodFeaturesToTrack(gray,3000,0.07,20,useHarrisDetector=True,k=0.1)
        
        corners_fine = cv2.cornerSubPix(gray,corners,(11,11),(-1,-1),criteria)
        coord = genCornorsCoord(corners_fine)

        for j in range(0, len(corners_fine)):
            x,y = corners_fine[j].ravel()
            if(not coord[j]==None ):
                cv2.circle(img,(x,y),3,255,-1)
                cv2.putText(img, str(coord[j][0]), (x,y)  , cv2.FONT_HERSHEY_PLAIN,0.8, (0, 0, 255), 1, cv2.LINE_AA)
                cv2.putText(img, str(coord[j][1]), (x,int(y+10)), cv2.FONT_HERSHEY_PLAIN,0.8, (0, 255, 0), 1, cv2.LINE_AA)
            else:
                cv2.circle(img,(x,y),3,(0, 0, 255),-1)

        cv2.imshow('img',img)
        cv2.waitKey()

        imgpoints.append(corners_fine)

        cornors_len=len(corners_fine)
        

    # src_pts = np.float32(imgpoints[0]).reshape(-1,1,2)
    # dst_pts = np.float32(objpoints[0]).reshape(-1,1,3)
    # print(src_pts,dst_pts)
    # H, mask = cv2.findHomography(src_pts, dst_pts, cv2.RANSAC,5.0)
    
    ret, mtx, dist, rvecs, tvecs = cv2.calibrateCamera(objpoints, imgpoints, gray.shape[::-1], None, None)




#start_tcp_serverX("", 1229)
#fileConvert("*.jpg",".jpg",".png")
chessBoardCalibsss((16,19),'*.jpg')
# calibRes=chessBoardCalib((6,9),'*.png')
# print(calibRes)

# img = cv2.imread(glob.glob('*.png')[0])
# h,  w = img.shape[:2]
# newcameramtx, roi=cv2.getOptimalNewCameraMatrix(calibRes['mtx'],calibRes['dist'],(w,h),1,(w,h))
# dst = cv2.undistort(img, calibRes['mtx'],calibRes['dist'], None, newcameramtx)
# #dst = cv2.warpPerspective(img,newcameramtx,img.shape[:2])
# cv2.imshow('img',dst)
# cv2.waitKey()


# calibRes_json = json.dumps(calibRes)
# outputfilename="mapx.json"
# with open(outputfilename, 'w') as outfile:
#     outfile.write(calibRes_json)