import numpy as np
import cv2
import glob
import json
# termination criteria
criteria = (cv2.TERM_CRITERIA_EPS + cv2.TERM_CRITERIA_MAX_ITER, 30, 0.001)



chessBoardDim=(7,9)

# prepare object points, like (0,0,0), (1,0,0), (2,0,0) ....,(6,5,0)
objp = np.zeros((chessBoardDim[0]*chessBoardDim[1],3), np.float32)
objp[:,:2] = np.mgrid[0:chessBoardDim[0],0:chessBoardDim[1]].T.reshape(-1,2)

# Arrays to store object points and image points from all the images.
objpoints = [] # 3d point in real world space
imgpoints = [] # 2d points in image plane.

images = glob.glob('*.jpg')
print(images)
for fname in images:
    img = cv2.imread(fname)
    gray = cv2.cvtColor(img,cv2.COLOR_BGR2GRAY)

    # Find the chess board corners
    ret, corners = cv2.findChessboardCorners(gray, (chessBoardDim[0],chessBoardDim[1]),None)

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
print( gray.shape[::-1])
ret, mtx, dist, rvecs, tvecs = cv2.calibrateCamera(objpoints, imgpoints, gray.shape[::-1], None, None)
dst = cv2.undistort(gray, mtx, dist, None, mtx)
mapx,mapy = cv2.initUndistortRectifyMap(mtx,dist,None,mtx,gray.shape[::-1],cv2.CV_32FC1)
#cv2.imshow('img',dst)
#cv2.waitKey(5000)
print(ret,mtx,dist,rvecs,tvecs)
print(mapx,mapy)


outputfilename="mapx.json"
with open(outputfilename, 'wb') as outfile:
    json.dump(mapx, outfile)