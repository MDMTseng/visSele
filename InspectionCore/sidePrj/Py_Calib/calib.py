import numpy as np
import cv2
import glob
import json
import socket
import math
import time
import random
from array import array
from datetime import datetime
random.seed(datetime.now())

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

    src_pts = np.float32(imgpoints[0]).reshape(-1,1,2)
    dst_pts = np.float32(objpoints[0]).reshape(-1,1,3)
    H, mask = cv2.findHomography(src_pts, dst_pts, cv2.RANSAC,5.0)
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
    if(len(distArr)<4):
        return None

    distArr.sort(key=lambda distInfo: distInfo["dist"])
    nbInfo=distArr[0:4]
    #find if the top 4 neighbors has similar distance
    distRatio = nbInfo[3]["dist"]/nbInfo[0]["dist"]
    if(distRatio>1.3):
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
    interCount=0
    while mainVecInfo==None:
        interCount+=1
        if(interCount>10):
            return None
        mainVecInfo = findMainVecInfo(cornors,random.randint(0,len(cornors)-1))

    
    seedIdx=0
    coordArr=[None]*len(cornors)
    vec1=mainVecInfo["vec1"]["vec"]
    vec2=mainVecInfo["vec2"]["vec"]

    magVec1=math.hypot(vec1[0],vec1[1])
    nnvec1=[vec1[0]/magVec1/magVec1,vec1[1]/magVec1/magVec1]
    magVec2=math.hypot(vec2[0],vec2[1])
    nnvec2=[vec2[0]/magVec2/magVec2,vec2[1]/magVec2/magVec2]

    searchList=[seedIdx]


    advScale=1
    coordArr[seedIdx]=[0,0]

    coordEstThres=0.9
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
                        coordArr[j][0]+=advScale
                        searchList.append(j)
                        continue

                    if(nndot1<0 and c_one>coordEstThres):
                        coordArr[j]=coordArr[cur_search_idx].copy()
                        coordArr[j][0]-=advScale
                        searchList.append(j)
                        continue


                distRatio=dist/mainVecInfo["vec2"]["dist"]
                if(distRatio>1):distRatio=1/distRatio
                if(distRatio>coordEstThres):
                    nndot2=nnvec2[0]*vec[0]+nnvec2[1]*vec[1]
                    c_one=close2one(math.fabs(nndot2))
                    if(nndot2>0 and c_one>coordEstThres):
                        coordArr[j]=coordArr[cur_search_idx].copy()
                        coordArr[j][1]+=advScale
                        searchList.append(j)
                        continue

                    if(nndot2<0 and c_one>coordEstThres):
                        coordArr[j]=coordArr[cur_search_idx].copy()
                        coordArr[j][1]-=advScale
                        searchList.append(j)
                        continue

        searchList=searchList[curSListL:len(searchList)]

        

    return coordArr


def pixToEdgeDist(loc,W,H):
    return min(loc[0],loc[1],W-loc[0],H-loc[1])


def cameraCalibPointsRuleOut(objpoints, imgpoints,size,thres):
    # src_pts = np.float32(imgpoints[0]).reshape(-1,1,2)
    # dst_pts = np.float32(objpoints[0]).reshape(-1,1,2)
    # print(src_pts,dst_pts)
    # H, mask = cv2.findHomography(src_pts, dst_pts, cv2.RANSAC,5.0)
    # print(H)
    ret, mtx, dist, rvecs, tvecs = cv2.calibrateCamera(objpoints, imgpoints, size,None,None)#,flags=cv2.CALIB_RATIONAL_MODEL)


    tot_error=0
    # print( "-------")
    # print( ret, mtx)
    # print( dist)
    # print( "-------")
    #dist=np.asarray([-1]*5, dtype= np.float32)
    do_refine=False
    for i in range(0,len(objpoints)):
        rvec=rvecs[i]
        imgpoints2, _ = cv2.projectPoints(objpoints[i], rvec, tvecs[i], mtx, dist)
        error = cv2.norm(imgpoints[i],imgpoints2, cv2.NORM_INF)#/len(imgpoints2)

        checked_img_points=[]
        checked_obj_points=[]
        for j in range(0,len(imgpoints2)):
            pt1 = imgpoints[i][j][0]
            pt_prj = imgpoints2[j][0]
            dist = math.hypot(pt1[0]-pt_prj[0],pt1[1]-pt_prj[1])
            print( dist)
            if dist<thres:
                checked_img_points.append(imgpoints[i][j])
                checked_obj_points.append(objpoints[i][j])

        if(len(checked_obj_points)>0):
            imgpoints[i]=np.asarray(checked_img_points, dtype= np.float32)
            objpoints[i]=np.asarray(checked_obj_points, dtype= np.float32)
        else:
            do_refine=True
        print (i,"===Error",error, "len:",len(imgpoints[i]))
        #print (i,"===Error",error,"  file:",images_trusted[i])
        # sub = cv2.subtract (imgpoints[i],imgpoints2)
        # print (sub)
        tot_error += error
    ave_err = tot_error/len(objpoints)

    return (objpoints,imgpoints,ave_err)


def rotationMatrix(theta):
    c, s = np.cos(theta), np.sin(theta)
    return np.array(((c,-s,0), (s, c,0),(0,0,1)))



def genProtoPakNum(type2B,array2save,array_type='B'):
    if len(type2B)!=2:
        return None
    headerArr =[]
    headerArr.append( array('B', map(ord, type2B)) )
    if(isinstance(array2save, str)):
        array2save = map(ord, array2save)
    farr = array(array_type, np.hstack(array2save))
    lenX = (farr.itemsize*len(farr)).to_bytes(8, byteorder="big", signed=False)
    headerArr.append( array('B', lenX) )
    headerArr.append( farr )
    return headerArr
    
def genProtoPakProtoPak(type2B,array2save):
    
    if len(type2B)!=2:
        return None
    headerArr =[]
    headerArr.append( array('B', map(ord, type2B)) )
    totL = 0
    for ele in array2save:
        totL+=ele.itemsize*len(ele)

    lenX = (totL).to_bytes(8, byteorder="big", signed=False)
    headerArr.append( array('B', lenX) )
    headerArr+= array2save 
    return headerArr

def chessBoardCalibsss(image_path):
    # termination criteria
    criteria = (cv2.TERM_CRITERIA_EPS + cv2.TERM_CRITERIA_MAX_ITER, 30, 0.01)

    # prepare object points, like (0,0,0), (1,0,0), (2,0,0) ....,(6,5,0)
    # Arrays to store object points and image points from all the images.
    objpoints = [] # 3d point in real world space
    imgpoints = [] # 2d points in image plane.

    images = glob.glob(image_path)

    #fast = cv2.FastFeatureDetector_create(threshold=25,nonmaxSuppression = True)
    if len(images) == 0 :
        return None

    images_trusted=[]
    for fname in images:

        print("IMG:",fname)
        img = cv2.imread(fname)

        gray = cv2.cvtColor(img,cv2.COLOR_BGR2GRAY)
        width,height = gray.shape[::-1]

        corners = cv2.goodFeaturesToTrack(gray,3000,0.1,20,useHarrisDetector=True,k=0.04)
        
        corners_fine = cv2.cornerSubPix(gray,corners,(21,21),(-1,-1),criteria)

        corners_fine = [x for x in corners_fine if pixToEdgeDist(x[0],width,height)>10]



        # for j in range(0, len(corners_fine)):
        #     loc = corners_fine[j][0]
        #     x=int(loc[0])
        #     y=int(loc[1])
        #     cv2.circle(gray,(x,y),3,255,-1)
        # cv2.imshow('gray',gray)
        # cv2.waitKey(0)


        maxMeaningfulCoordCount=0
        coord=[]
        for j in range(0,5):
            new_coord = genCornorsCoord(corners_fine)
            meaningfulCoordCount=0
            for nc in new_coord:
                if(nc!=None):
                    meaningfulCoordCount+=1

            if(new_coord!=None and maxMeaningfulCoordCount<meaningfulCoordCount):
                maxMeaningfulCoordCount=meaningfulCoordCount
                coord=new_coord


        if(len(coord)==0):
            continue
        
        corners_trusted = [] # 3d point in real world space
        coord_trusted = [] # 2d points in image plane.
        for j in range(0, len(coord)):
            if(coord[j]!=None):
                coord3=coord[j].copy()
                coord3.append(0)
                coord_trusted.append(coord3)
                corners_trusted.append(corners_fine[j].tolist())
        
        # if(len(corners_trusted)/len(corners_fine)<0.8):
        #     continue
        #print(corners_trusted)
        imgpoints.append(np.asarray(corners_trusted, dtype= np.float32))
        objpoints.append(np.asarray(coord_trusted, dtype= np.float32))
        
        images_trusted.append(fname)
        

        # for j in range(0, len(corners_trusted)):
        #     loc = corners_trusted[j][0]
        #     coord = coord_trusted[j]
        #     x=int(loc[0])
        #     y=int(loc[1])
        #     cv2.circle(img,(x,y),3,255,-1)
        #     cv2.putText(img, str(coord[0]), (x,y)  , cv2.FONT_HERSHEY_PLAIN,0.8, (0, 0, 255), 1, cv2.LINE_AA)
        #     cv2.putText(img, str(coord[1]), (x,int(y+10)), cv2.FONT_HERSHEY_PLAIN,0.8, (0, 255, 0), 1, cv2.LINE_AA)
        # cv2.imshow('img',img)
        # cv2.waitKey()



    imageSize = gray.shape[::-1]

    # #refine....
    # for x in range(5):
    #     thres = (4-x)/4+4 if x >2 else 10
    #     objpoints, imgpoints,err = cameraCalibPointsRuleOut(objpoints, imgpoints,imageSize,thres)
    #     print(x," err:",err)
    #     # src_pts = np.float32(imgpoints[0]).reshape(-1,1,2)
    #     # dst_pts = np.float32(objpoints[0]).reshape(-1,1,2)
    #     # print(src_pts,dst_pts)
    #     # H, mask = cv2.findHomography(src_pts, dst_pts, cv2.RANSAC,5.0)
    #     # print(H)
    
    # objpoints, imgpoints,err = cameraCalibPointsRuleOut(objpoints, imgpoints,imageSize,1000)
    # print(x," err:",err)

    ret, mtx, dist, rvecs, tvecs = cv2.calibrateCamera(objpoints, imgpoints, imageSize,None,None)#,flags=cv2.CALIB_RATIONAL_MODEL)


    matArr=[]
    no_dist=np.asarray([0]*5, dtype= np.float32)
    for i in range(0,len(objpoints)):
        rvec=rvecs[i]
        tvec=tvecs[i]
        # rvec=np.asarray([[0]*3], dtype= np.float32)
        # tvec=np.asarray([[0]*3], dtype= np.float32)
        imgpoints2, _ = cv2.projectPoints(objpoints[i], rvec,tvec, mtx, no_dist)
        mat,ret2=cv2.findHomography(objpoints[i], imgpoints2)
        print( "----",i,"---\n",mat)
        matArr.append(mat)

        # img = cv2.imread(images_trusted[i]) 
        # width,height = gray.shape[::-1]
        # for j in range(0, len(imgpoints2)):
        #     x,y = imgpoints2[j].ravel()
        #     x2,y2= imgpoints[i][j].ravel()
        #     coord = objpoints[i][j]
        #     cv2.circle(img,(x,y),3,255,-1)
        #     cv2.circle(img,(x2,y2),1,100,-1)
        #     cv2.putText(img, str(coord[0]), (x,y)  , cv2.FONT_HERSHEY_PLAIN,0.8, (0, 0, 255), 1, cv2.LINE_AA)
        #     cv2.putText(img, str(coord[1]), (x,int(y+10)), cv2.FONT_HERSHEY_PLAIN,0.8, (0, 255, 0), 1, cv2.LINE_AA)
        # cv2.imshow('img',img)
        # cv2.waitKey()



    # divmat = cv2.divide(matArr[5],matArr[1])
    # print( "----divmat---\n",divmat)

    print( "-------")
    print("ret:", ret,"\nmtx:", mtx,"\ndist:",dist)
    print( "-------")



    print(ret, mtx, dist)

    newcameramtx, roi=cv2.getOptimalNewCameraMatrix(mtx,dist,imageSize,1,imageSize)




    for i in range(0, len(imgpoints)):
        print(rvecs[i], tvecs[i])
        img = cv2.imread(images_trusted[i])
        gray = cv2.cvtColor(img,cv2.COLOR_BGR2GRAY)
        width,height = gray.shape[::-1]
        for j in range(0, len(imgpoints[i])):
            x,y = imgpoints[i][j].ravel()
            coord = objpoints[i][j]
            cv2.circle(img,(x,y),3,255,-1)
            cv2.putText(img, str(coord[0]), (x,y)  , cv2.FONT_HERSHEY_PLAIN,0.8, (0, 0, 255), 1, cv2.LINE_AA)
            cv2.putText(img, str(coord[1]), (x,int(y+10)), cv2.FONT_HERSHEY_PLAIN,0.8, (0, 255, 0), 1, cv2.LINE_AA)
        cv2.imshow('img',img)
        cv2.waitKey()





    #newcameramtx=matArr[0]
    #np.linalg.inv( matArr[0])
    calibImg_idx=0
    img = cv2.imread(images[calibImg_idx])

    gray = cv2.cvtColor(img,cv2.COLOR_BGR2GRAY)
    print("matArr[",calibImg_idx,"]:\n",matArr[calibImg_idx])

    newcameramtx=np.matmul(np.linalg.inv( matArr[calibImg_idx]),mtx)

    coorOriginLoc = imgpoints[calibImg_idx][0][0]
    print(imageSize)
    
    print("mtx:\n",mtx)
    offsetX=30
    offsetY=10
    mult=30

    rotation=0*np.pi/180

    keepAlive=True

    downSamp=5
    dispSize=(imageSize[0]//downSamp,imageSize[1]//downSamp)
    while keepAlive:
        newcameramtx_=np.matmul(np.matrix(
            [[1, 0,offsetX], [0, 1,offsetY], [0,0,1]]
            ),newcameramtx)

        newcameramtx_=np.matmul(np.matrix(
            [[mult, 0,0], [0, mult,0], [0,0,1]]
            ),newcameramtx_)

        newcameramtx_=np.matmul(rotationMatrix(rotation),newcameramtx_)
        

        print("newcameramtx:\n",newcameramtx_)
        if(False):
            dst = cv2.undistort(gray, mtx, dist, None, newcameramtx_)
        else:
            mapx,mapy = cv2.initUndistortRectifyMap(mtx,dist,None,newcameramtx_,dispSize,5)
            dst = cv2.remap(img,mapx,mapy,cv2.INTER_LINEAR)
        # print(tvecs[0])
        
        
        cv2.imshow('img',dst)
        #cv2.imwrite("./cat2_.png", dst, [int(cv2.IMWRITE_PNG_COMPRESSION), 9])
        key = cv2.waitKey()
        print("key:",key)
        stepSize=0.2
        if  (key ==ord('d')):
            offsetX+=stepSize
        elif(key ==ord('a')):
            offsetX-=stepSize
        elif(key ==ord('w')):
            offsetY-=stepSize
        elif(key ==ord('s')):
            offsetY+=stepSize
        elif(key ==ord('i')):
            mult*=1.01
        elif(key ==ord('k')):
            mult/=1.01
        elif(key ==ord('j')):
            rotation+=0.01
        elif(key ==ord('l')):
            rotation-=0.01
        elif(key ==27):
            break
    
    # dataStructure=np.hstack([
    #     [0,1,2,3],
    #     np.hstack(mapx),
    #     np.hstack(mapy)
    # ])

    output_file = open("CalibInfo.bin", 'wb')

    INFO = genProtoPakNum("IF", "INFO")
    DIM = genProtoPakNum("DM", [imageSize[0],imageSize[1]],'L')
    DIM_S = genProtoPakNum("DS", [dispSize[0],dispSize[1]],'L')
    packArrX = genProtoPakNum("MX",np.hstack(mapx),'d')
    packArrY = genProtoPakNum("MY",np.hstack(mapy),'d')
    packArr = genProtoPakProtoPak("CM",INFO+DIM+DIM_S+packArrX+packArrY)
    for pack in packArr:
        pack.tofile(output_file)
    
    output_file.close()

    #print(dataStructure)
    #saveFloatArray2File("mapx.bin",dataStructure)
    # cv2.imwrite("./cat2_.png", dst, [int(cv2.IMWRITE_PNG_COMPRESSION), 9])

#"{\"type\":\"cameraCalib\",\"pgID\":12442,\"img_path\":\"*.jpg\",\"board_dim\":[7,9]}"
#start_tcp_serverX("", 1229)
#fileConvert("*.jpg",".jpg",".png")
chessBoardCalibsss('*.jpg')

# calibRes=chessBoardCalib((6,9),'*.jpg')
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