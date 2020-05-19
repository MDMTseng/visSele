import numpy as np
import cv2
import glob
import json
import socket
import math
import time
import random
import sys
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

    minDistRatioIdx=None
    minDistRatio=999
    for i in range(0,6):
        distRatio = distArr[i+2]["dist"]/distArr[i+0]["dist"]
        
        #print("distRatio:",distRatio)
        if(minDistRatio>distRatio):
            minDistRatio=distRatio
            minDistRatioIdx=i


    nbInfo=distArr[minDistRatioIdx:4+minDistRatioIdx]
    print("nbInfo:",nbInfo,"\nminDistRatioIdx:",minDistRatioIdx,"minDistRatio:",minDistRatio)
    #find if the top 4 neighbors has similar distance
    if(minDistRatio>1.3):
        return None

    for nbEleInfo in nbInfo:
        idx = nbEleInfo["idx"]
        curPt=cornors[idx][0]
        vec = [curPt[0]-seedPt[0], curPt[1]-seedPt[1]]
        norm= math.hypot(vec[0],vec[1])
        nbEleInfo["vec"]=vec
        nbEleInfo["nvec"]=[vec[0]/norm,vec[1]/norm]

    vec1 = {"vec":nbInfo[0]["vec"].copy(),"nvec":nbInfo[0]["nvec"].copy(),"info":[nbInfo[0]]}
    vec2 = {"vec":[0,0],"info":[]}
    for nbEleInfo in (nbInfo):
        nvecx = nbEleInfo["nvec"]
        dotP = vec1["nvec"][0]*nvecx[0]+vec1["nvec"][1]*nvecx[1]

        #print(dotP)
        if(dotP>0.8 or dotP<-0.8):
            vec1["info"].append(nbEleInfo)
        if(dotP<0.2 and dotP>-0.2):
            vec2["info"].append(nbEleInfo)

    if(len(vec1["info"])==0 or len(vec2["info"])==0):
        return None
    
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

    

def genCornorsCoord(cornors,coordEstThres=0.9):

    mainVecInfo=None
    interCount=0
    while True:
        interCount+=1
        if(interCount>100):
            return None
        mainVecInfo = findMainVecInfo(cornors,random.randint(0,len(cornors)-1))
        if(mainVecInfo==None):
            print("ERROR...")
        else:
            vec1 = mainVecInfo['vec1']["vec"]
            ratio = math.fabs(vec1[0]/vec1[1])
            if(ratio>0.25 and ratio <4):
                mainVecInfo=None
                #print("ERROR... the vector is not close to xy axis")
                continue
            break
    
    seedIdx=random.randint(0,len(cornors)-1)
    coordArr=[None]*len(cornors)
    vec1=mainVecInfo["vec1"]["vec"]
    vec2=mainVecInfo["vec2"]["vec"]

    magVec1=math.hypot(vec1[0],vec1[1])
    nnvec1=[vec1[0]/magVec1,vec1[1]/magVec1]
    magVec2=math.hypot(vec2[0],vec2[1])
    nnvec2=[vec2[0]/magVec2,vec2[1]/magVec2]

    print("\n\nnvec1:   ",nnvec1,"\nnvec2:   ",nnvec2,"\n\n")


    searchList=[seedIdx]
    distInfo=[{"dist1":None,"dist2":None,"vec1":None,"vec2":None,"vec1_set":0,"vec2_set":0} for i in range(len(cornors))]
    distInfo[seedIdx]={
      "dist1":magVec1,"dist2":magVec2,
      "vec1":nnvec1,"vec2":nnvec2,
      "vec1_set":1,"vec2_set":1}

    advScale=1
    coordArr[seedIdx]=[0,0]

    count=0
    while len(searchList)>0:
        curSListL=len(searchList)
        print("curSListL:",curSListL)
        for i in range(0, curSListL):
            cur_search_idx=searchList[i]
            centPt = cornors[cur_search_idx][0]
            cur_dist_info=distInfo[cur_search_idx]
            xcount=0
            for j in range(0, len(cornors)):
                if(xcount>=2):break
                obj_dist_info=distInfo[j]
                if(obj_dist_info["vec1_set"]!=0 and obj_dist_info["vec2_set"]!=0 ):
                  continue
                
                cpt = cornors[j][0]
                vec = [cpt[0]-centPt[0],cpt[1]-centPt[1]]

                dist = math.hypot(vec[0],vec[1])
                vec[0]/=dist
                vec[1]/=dist
                
                if obj_dist_info["vec1_set"]==0:
                  distRatio=close2one(dist/cur_dist_info["dist1"])
                  if(distRatio>coordEstThres):
                      nndot1=cur_dist_info["vec1"][0]*vec[0]+cur_dist_info["vec1"][1]*vec[1]
                      c_one=close2one(math.fabs(nndot1))
                      if(c_one>coordEstThres):
                        #infect the initial vec2 data, need to be set when new data is avaliable
                        
                        if(obj_dist_info["dist2"]==None):
                          obj_dist_info["dist2"]=cur_dist_info["dist2"]
                          obj_dist_info["vec2"]=cur_dist_info["vec2"]
                          obj_dist_info["vec2_set"]=0

                        if(nndot1<0):
                          vec[0]*=-1
                          vec[1]*=-1

                        #set the vec1 data as current distance
                        obj_dist_info["dist1"]=dist
                        obj_dist_info["vec1"]=vec
                        obj_dist_info["vec1_set"]+=1
                        if(cur_dist_info["vec1_set"]==0):
                          cur_dist_info["dist1"]=dist
                          cur_dist_info["vec1"]=vec
                          cur_dist_info["vec1_set"]+=1

                        # print('v1-cur_dist_info,',cur_dist_info," cur_search_idx:",cur_search_idx)
                        # print('   obj_dist_info,',obj_dist_info," j:",j)
                        # print('\n')


                        if(coordArr[j] == None):
                          coordArr[j]=coordArr[cur_search_idx].copy()
                          coordArr[j][0]+=advScale if(nndot1>0) else -advScale
                          searchList.append(j)
                          xcount+=1
                          continue

                if obj_dist_info["vec2_set"]==0:
                  distRatio=close2one(dist/cur_dist_info["dist2"])
                  if(distRatio>coordEstThres):
                      nndot2=cur_dist_info["vec2"][0]*vec[0]+cur_dist_info["vec2"][1]*vec[1]
                      c_one=close2one(math.fabs(nndot2))
                      
                      if(c_one>coordEstThres):
                        #ref the initial vec2 data, need to be set when new data is avaliable
                        if(obj_dist_info["dist1"]==None):
                          obj_dist_info["dist1"]=cur_dist_info["dist1"]
                          obj_dist_info["vec1"]=cur_dist_info["vec1"]
                          obj_dist_info["vec1_set"]=0
                        if(nndot2<0):
                          vec[0]*=-1
                          vec[1]*=-1

                        obj_dist_info["dist2"]=dist
                        obj_dist_info["vec2_set"]+=1
                        obj_dist_info["vec2"]=vec
                        
                        if(cur_dist_info["vec2_set"]==0):
                          cur_dist_info["vec2_set"]+=1
                          cur_dist_info["dist2"]=dist
                          cur_dist_info["vec2"]=vec

                        # print('v2-cur_dist_info,',cur_dist_info," cur_search_idx:",cur_search_idx)
                        # print('   obj_dist_info,',obj_dist_info," j:",j)
                        # print('\n') 
                        if(coordArr[j] == None):
                          coordArr[j]=coordArr[cur_search_idx].copy()

                          coordArr[j][1]+=advScale if(nndot2>0) else -advScale
                          searchList.append(j)
                          xcount+=1
                          continue
                                
        count+=1             
        searchList=searchList[curSListL:len(searchList)]

    minCoorX=999
    minCoorY=999
    for coord in coordArr: 
        if(coord==None):continue
        if(minCoorX>coord[0]):minCoorX=coord[0]
        if(minCoorY>coord[1]):minCoorY=coord[1]
    
    for coord in coordArr: 
        if(coord==None):continue
        coord[0]-=minCoorX
        coord[1]-=minCoorY


    return coordArr,mainVecInfo


def pixToEdgeDist(loc,W,H):
    return min(loc[0],loc[1],W-loc[0],H-loc[1])


def obj_img_Shuffle(objpoints, imgpoints):
    objpoints_rand=[]
    imgpoints_rand=[]

    for i in range(0,len(objpoints)):#for exch image fo shuffle
        combined = list(zip(objpoints[i], imgpoints[i]))
        random.shuffle(combined)
        objpoints_randX, imgpoints_randX = zip(*combined)
        objpoints_rand.append(objpoints_randX)
        imgpoints_rand.append(imgpoints_randX)
    return objpoints_rand,imgpoints_rand


def cameraCalibPointsRuleOut(objpoints, imgpoints,img_size,thres=1,pickPercentage=0.5,minLen=10):
    # src_pts = np.float32(imgpoints[0]).reshape(-1,1,2)
    # dst_pts = np.float32(objpoints[0]).reshape(-1,1,2)
    # print(src_pts,dst_pts)
    # H, mask = cv2.findHomography(src_pts, dst_pts, cv2.RANSAC,5.0)
    # print(H)
    objpoints_shuf,imgpoints_shuf = obj_img_Shuffle(objpoints,imgpoints)
    objpoints_pick=[]
    imgpoints_pick=[]
    for i in range(0,len(objpoints_shuf)):#for each image's point pair
        datPtLen=len(objpoints_shuf[i])
        datPtPorp=round(datPtLen*pickPercentage)
        if( datPtPorp>minLen):
            objpoints_pick.append(np.asarray(objpoints_shuf[i][:datPtPorp], dtype= np.float32))
            imgpoints_pick.append(np.asarray(imgpoints_shuf[i][:datPtPorp], dtype= np.float32))
        else:
            objpoints_pick[i]=[]
            imgpoints_pick[i]=[]
    #print(objpoints_pick)
    #randomly pick a set of point pairs, do camera calibration
    ret, mtx, dist, rvecs, tvecs = cv2.calibrateCamera(objpoints_pick, imgpoints_pick, img_size,None,None)#,flags=cv2.CALIB_RATIONAL_MODEL)

    print("ret, mtx, dist, rvecs, tvecs:\n",ret, mtx, dist, rvecs, tvecs)
    inBoundThres=thres

    objpoints_inBound=[]
    imgpoints_inBound=[]
    error_inBound=[]
    totLen=0
    availLen=0
    errorSum=0
    errorMax=0

    #use the calibration info to exam how many points(in all) satisfy the calibration projection
    for i in range(0,len(objpoints_shuf)):#for each image's point shuffled pair
        imgpoints2, _ = cv2.projectPoints(objpoints[i], rvecs[i], tvecs[i], mtx, dist)
        #error = cv2.norm(imgpoints[i],imgpoints2, cv2.NORM_INF)#/len(imgpoints2)
        checked_img_points=[]
        checked_obj_points=[]
        for j in range(0,len(imgpoints2)):
            pt1 = imgpoints[i][j][0]
            pt_prj = imgpoints2[j][0]
            dist = math.hypot(pt1[0]-pt_prj[0],pt1[1]-pt_prj[1])
            if dist<inBoundThres:
                checked_img_points.append(imgpoints[i][j])
                checked_obj_points.append(objpoints[i][j])
                errorSum+=dist
                if(errorMax<dist):errorMax=dist
      
        tL=len(imgpoints2)
        totLen+=tL
        aL=len(checked_obj_points)
        availLen+=aL
        # print("tL:",tL," aL:",aL)
        objpoints_inBound.append(np.asarray(checked_obj_points, dtype= np.float32))
        imgpoints_inBound.append(np.asarray(checked_img_points, dtype= np.float32))
        #error_inBound.append()

    #print(availLen,":",totLen)
    if availLen>0:
        errorSum/=availLen
    return (objpoints_inBound,imgpoints_inBound,availLen,totLen,errorMax)


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
    print("GO..image_path:",image_path," images:",images)

    #fast = cv2.FastFeatureDetector_create(threshold=25,nonmaxSuppression = True)
    if len(images) == 0 :
        return None

    images_trusted=[]
    mainVecInfo_list=[]
    
    for fname in images:

        print("IMG:",fname)
        img = cv2.imread(fname)

        # blur = cv2.GaussianBlur(img,(25,25),0)
        # img = blur
        #img = cv2.resize(img,  (0,0), fx=0.5, fy=0.5)
        gray = cv2.cvtColor(img,cv2.COLOR_BGR2GRAY)
        width,height = gray.shape[::-1]

        corners=[]


        #///////////////
        ds_f=1000/width
        if(ds_f>1):ds_f=1
        img_ds = cv2.resize(img,  (0,0), fx=ds_f, fy=ds_f)
        gray_ds = cv2.cvtColor(img_ds,cv2.COLOR_BGR2GRAY)
        corners = cv2.goodFeaturesToTrack(gray_ds,3000,0.01,15,useHarrisDetector=True,k=0.1)
        for c in corners:
            c[0][0]/=ds_f
            c[0][1]/=ds_f
        #///////////////

        corners_fine = cv2.cornerSubPix(gray,corners,(21,21),(-1,-1),criteria)

        corners_fine = [x for x in corners_fine if pixToEdgeDist(x[0],width,height)>10]

        maxMeaningfulCoordCount=0
        coord=[]
        mainVecInfo=None
        xxCount=0
        for j in range(0,5):

            new_coord,_mainVecInfo = genCornorsCoord(corners_fine)
            
            if(new_coord==None):continue
            meaningfulCoordCount=0
            for nc in new_coord:
                if(nc!=None):
                    meaningfulCoordCount+=1

            if(new_coord!=None and maxMeaningfulCoordCount<meaningfulCoordCount):
                maxMeaningfulCoordCount=meaningfulCoordCount
                coord=new_coord
                mainVecInfo = _mainVecInfo

            if(meaningfulCoordCount>len(corners_fine)*0.8):break
        mainVecInfo_list.append(mainVecInfo)


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
        print("END... IMG:",fname)

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

    #refine....
    maxMatchingRatio=0.0
    maxMatchingAvailLen=0
    objpoints_Match=None
    imgpoints_Match=None
    print("---imgpoints:\n  ",len(imgpoints[0])) #real cornor points from all availiable images
    print("---objpoints:\n  ",len(objpoints[0])) #XY index grid from real cornor points 
    for x in range(20):
        thres = 0.1
        objpoints_, imgpoints_, availLen,totLen,error = cameraCalibPointsRuleOut(objpoints, imgpoints,imageSize,thres*2,0.2)
        print("  ",x," availLen>",availLen," totLen>",totLen," error:",error)

        
        if(maxMatchingRatio<availLen/totLen):
            objpoints_, imgpoints_, availLen,_totLen,error = cameraCalibPointsRuleOut(objpoints_, imgpoints_,imageSize,thres,1)
            print("2nd:",x," availLen>",availLen," _totLen>",_totLen," error:",error)
            maxMatchingRatio = availLen/totLen
            maxMatchingAvailLen=availLen
            objpoints_Match = objpoints_
            imgpoints_Match = imgpoints_
            if(maxMatchingRatio>0.8):break
    
    # objpoints, imgpoints,err = cameraCalibPointsRuleOut(objpoints, imgpoints,imageSize,1000)
    print("maxMatchingRatio:",maxMatchingRatio)
    print("maxMatchingAvailLen:",maxMatchingAvailLen)
    if(maxMatchingRatio<0.8 and maxMatchingAvailLen<1000):
        return None
    ret, mtx, dist, rvecs, tvecs = cv2.calibrateCamera(objpoints_Match, imgpoints_Match, imageSize,None,None)#,flags=cv2.CALIB_RATIONAL_MODEL)


    affine_matArr=[]
    no_dist=np.asarray([0]*5, dtype= np.float32)
    for i in range(0,len(objpoints)):#Map keypoints to ideal grid points 
        rvec=rvecs[i]
        tvec=tvecs[i]

        #objpoints[i]: ideal grid points
        #
        #projectPoints includes distortion compensation 
        semi_imgpoints2, _ = cv2.projectPoints(objpoints[i], rvec,tvec, mtx, no_dist)
        
        #imgpoints2: semi real keypoint's location includes distortion
        # rvec,tvec= calibrateCamera(real points) 
        # semi real points=projectPoints(ideal points,rvec,tvec)
        #--semi real points might not 100% equal to real points
        #  But semi real points is the best effert recovery from current "cv.calibrateCamera" 


        #findHomography will give linear transform
        mat,ret2=cv2.findHomography(objpoints[i], semi_imgpoints2)
        print( "----",i,"---\n",mat)
        #print("objpoints:",objpoints[i],"semi_imgpoints2:", semi_imgpoints2)
        affine_matArr.append(mat)

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



    # divmat = cv2.divide(affine_matArr[5],affine_matArr[1])
    # print( "----divmat---\n",divmat)

    print( "-------")
    print("ret:", ret,"\nmtx:", mtx,"\ndist:",dist)
    print( "-------")


    newcameramtx, roi=cv2.getOptimalNewCameraMatrix(mtx,dist,imageSize,1,imageSize)




    # for i in range(0, len(imgpoints_Match)):
    #     print("......",rvecs[i], tvecs[i])
    #     img = cv2.imread(images_trusted[i])
    #     gray = cv2.cvtColor(img,cv2.COLOR_BGR2GRAY)
    #     width,height = gray.shape[::-1]
    #     for j in range(0, len(imgpoints_Match[i])):
    #         x,y = imgpoints_Match[i][j].ravel()
    #         coord = objpoints_Match[i][j]
    #         cv2.circle(img,(x,y),3,128,-1)
    #         cv2.putText(img, str(int(coord[0])), (x,y)  , cv2.FONT_HERSHEY_PLAIN,0.8, (0, 0, 255), 1, cv2.LINE_AA)
    #         cv2.putText(img, str(int(coord[1])), (x,int(y+10)), cv2.FONT_HERSHEY_PLAIN,0.8, (0, 255, 0), 1, cv2.LINE_AA)
    #     cv2.imshow('img',img)
    #     cv2.waitKey()
    #     #cv2.imwrite(images_trusted[i]+'_output.jpg', img)




    #newcameramtx=affine_matArr[0]
    #np.linalg.inv( affine_matArr[0])
    calibImg_idx=0
    img = cv2.imread(images[calibImg_idx])
    gray = cv2.cvtColor(img,cv2.COLOR_BGR2GRAY)
    print("affine_matArr[",calibImg_idx,"]:\n",affine_matArr[calibImg_idx])

    affineCalibInvMatrix=np.linalg.inv( affine_matArr[calibImg_idx])
    print(">>>affineCalibInvMatrix>>>>\n",affineCalibInvMatrix)
    affineCalibInvMatrix[0][2]=0#zero the affine offset
    affineCalibInvMatrix[1][2]=0



    print("sqrt(det(camMtx):\n",math.sqrt(np.linalg.det(mtx))," sqrt(det(affineMtx):",math.sqrt(np.linalg.det(affineCalibInvMatrix)))
    newcameramtx=np.matmul(affineCalibInvMatrix,mtx)

    print(imageSize)
    
    print("mtx:\n",mtx," sqrt(det):",math.sqrt(np.linalg.det(newcameramtx)))


    keepAlive=True

    downSamp=imageSize[0]//500
    if(downSamp==0):
        downSamp=1
    dispSize=(imageSize[0]//downSamp,imageSize[1]//downSamp)


    print("downSamp:",downSamp,"\n")





    rotaVec=np.matmul(affineCalibInvMatrix, np.array([1, 0,1]))
    print(">>>rotaVec>>>>\n",rotaVec, "theta:",math.atan2(rotaVec[1],rotaVec[0]))
    baseRotation=-math.atan2(rotaVec[1],rotaVec[0])

    sqrt_det_affineMat=math.sqrt(np.linalg.det(affineCalibInvMatrix))
    baseMult=1/downSamp/sqrt_det_affineMat
    
    offsetX=0
    offsetY=0



    # baseRotation=0
    # baseMult=1

    rotation=0
    mult=1

    stepSize=1
    stepScale=2
    stepRotation=0.01

    #create the Undistort map (the map is in ideal coord, each xy in map indicates the location in original map)
     
    while keepAlive:
        newcameramtx_=newcameramtx
        newcameramtx_=np.matmul(rotationMatrix(rotation+baseRotation),newcameramtx_)
        
        newcameramtx_=np.matmul(np.matrix(
            [[1, 0,offsetX], [0, 1,offsetY], [0,0,1]]
            ),newcameramtx_)
        newcameramtx_=np.matmul(np.matrix(
            [[mult*baseMult, 0,0], [0, mult*baseMult,0], [0,0,1]]
            ),newcameramtx_)



        #print("newcameramtx:\n",newcameramtx_)
        if(False):
            dst = cv2.undistort(gray, mtx, dist, None, newcameramtx_)
        else:
            mapx,mapy = cv2.initUndistortRectifyMap(mtx,dist,None,newcameramtx_,dispSize,5)
            
            #print(">>>mapx,mapy>>>>\n",mapx,mapy)
            dst = cv2.remap(img,mapx,mapy,cv2.INTER_LINEAR)
        #print(mapx)
        
        print(" sqrt_det_affineMat:",sqrt_det_affineMat," tot:",1/baseMult/mult)
        #if(True):break
        cv2.imshow('img',dst)
        #cv2.imwrite("./cat2_.png", dst, [int(cv2.IMWRITE_PNG_COMPRESSION), 9])
        key = cv2.waitKey()
        if  (key ==ord('d')):
            offsetX+=stepSize
        elif(key ==ord('a')):
            offsetX-=stepSize
        elif(key ==ord('w')):
            offsetY-=stepSize
        elif(key ==ord('s')):
            offsetY+=stepSize
        elif(key ==ord('i')):
            mult*=stepScale
        elif(key ==ord('k')):
            mult/=stepScale
        elif(key ==ord('j')):
            rotation+=stepRotation
        elif(key ==ord('l')):
            rotation-=stepRotation
        elif(key ==ord('0')):
            rotation=0
            mult=1
            offsetY=0
            offsetX=0
        elif(key ==ord('1')):
            stepSize=1/100
            stepScale=1+0.1/50
            stepRotation=0.01/50
        elif(key ==ord('2')):
            stepSize=1/10
            stepScale=1+0.1/10
            stepRotation=0.01/10
        elif(key ==ord('3')):
            stepSize=1
            stepScale=1.1
            stepRotation=0.01
        elif(key ==27):
            break
    
    # dataStructure=np.hstack([
    #     [0,1,2,3],
    #     np.hstack(mapx),
    #     np.hstack(mapy)
    # ])
    output_file = open("CalibMap.bin", 'wb')

    INFO = genProtoPakNum("IF", "INFO")
    DIM = genProtoPakNum("DM", [imageSize[0],imageSize[1]],'L')
    DIM_S = genProtoPakNum("DS", [dispSize[0],dispSize[1]],'L')
    
    pack_OriginPixPerBlock = genProtoPakNum("OB",[sqrt_det_affineMat],'d')
    pack_MapPixPerBlock = genProtoPakNum("CB",[1/baseMult/mult],'d')
    pack_mmPerBlock = genProtoPakNum("MB",[0.5],'d')
    
    packArrX = genProtoPakNum("MX",np.hstack(mapx),'d')
    packArrY = genProtoPakNum("MY",np.hstack(mapy),'d')
    packArr = genProtoPakProtoPak("CM",
      INFO+DIM+DIM_S+
      pack_OriginPixPerBlock+pack_MapPixPerBlock+pack_mmPerBlock+
      packArrX+packArrY)
    for pack in packArr:
        pack.tofile(output_file)
    
    output_file.close()

    #print(dataStructure)
    #saveFloatArray2File("mapx.bin",dataStructure)
    # cv2.imwrite("./cat2_.png", dst, [int(cv2.IMWRITE_PNG_COMPRESSION), 9])

#"{\"type\":\"cameraCalib\",\"pgID\":12442,\"img_path\":\"*.jpg\",\"board_dim\":[7,9]}"
#start_tcp_serverX("", 1229)
#fileConvert("*.jpg",".jpg",".png")


if __name__ == "__main__":
  chessBoardCalibsss(sys.argv[1])

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