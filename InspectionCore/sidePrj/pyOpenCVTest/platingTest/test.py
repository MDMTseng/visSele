
# Import packages
import cv2
import numpy as np
import math

# Lists to store the bounding box coordinates
top_left_corner=[]
bottom_right_corner=[]

def draw_line_angled(draw_on_image,pt1,angle,length,color,thickness):
  if(angle>math.pi/2):angle-=math.pi
  if(angle<-math.pi/2):angle+=math.pi
  cos=math.cos(angle)*length+pt1[0]
  sin=math.sin(angle)*length+pt1[1]
  cv2.line(draw_on_image, tuple(int(x) for x in pt1), (int(cos), int(sin)),color, thickness)


def getRotTranMat(pt1,pt2,Theta):
  s=math.sin(-Theta)
  c=math.cos(-Theta)

  pts1 = np.float32([pt1,[pt1[0]+s,pt1[1]+c],[pt1[0]+c,pt1[1]-s]])
  pts2 = np.float32([pt2,[pt2[0],pt2[1]+1],[pt2[0]+1,pt2[1]]])
  return cv2.getAffineTransform(pts1,pts2)

# function which will be called on mouse input
def drawRectangle(action, x, y, flags, *userdata):
  # Referencing global variables 
  global top_left_corner, bottom_right_corner
  # Mark the top left corner when left mouse button is pressed
  if action == cv2.EVENT_LBUTTONDOWN:
    top_left_corner = [(x,y)]
    # When left mouse button is released, mark bottom right corner
  elif action == cv2.EVENT_LBUTTONUP:
    bottom_right_corner = [(x,y)]    
    # Draw the rectangle
    cv2.rectangle(c_image, top_left_corner[0], bottom_right_corner[0], (0,255,0),2, 8)
    print( top_left_corner[0],bottom_right_corner[0])
    cv2.imshow(windowID1,c_image)

# Read Images



# image = cv2.imread("Image_20220725210615485.bmp")
# image = cv2.imread("Image_20220725210617801.bmp")
# image = cv2.imread("Image_20220725210620788.bmp")
image = cv2.imread("Image_20220725210624081.bmp")
# c_image = image[531:1171, 916:1355]
c_image = image[400:1500, 600:1700]

# (916, 531) (1355, 1171)
# Make a temporary image, will be useful to clear the drawing
temp = c_image.copy()

windowID1="Window"
# Create a named window
cv2.namedWindow(windowID1)
windowID2="WindowX"
cv2.namedWindow(windowID2)
windowID3="Window3"
cv2.namedWindow(windowID3)
# highgui function called when mouse events occur
cv2.setMouseCallback(windowID1, drawRectangle)





def TargetRegionProcess(tarImg):
  # gray = cv2.rgb2gray(image)
  xs=30
  ys=50
  c_tarImg = tarImg[ ys:-ys,xs:-xs]
  hsv = cv2.cvtColor(c_tarImg, cv2.COLOR_BGR2HSV)

  # cv2.imshow(windowID1, hsv)

  hMin=10
  sMin=80
  vMin=0

  hMax=70#179
  sMax=255
  vMax=255

  mask = cv2.inRange(hsv, np.array([hMin, sMin, vMin]), np.array([hMax, sMax, vMax]))
  cv2.imshow(windowID2, mask)





  gray_tarImg =cv2.cvtColor(c_tarImg, cv2.COLOR_BGR2GRAY)

  x = cv2.Sobel(gray_tarImg,cv2.CV_64F,1, 0, ksize=3)
  y = cv2.Sobel(gray_tarImg,cv2.CV_64F,0, 1, ksize=3)
  absX = cv2.convertScaleAbs(x)
  absY = cv2.convertScaleAbs(y)
  Sobel = cv2.addWeighted(absX, 0.5, absY, 0.5, 0)
  # Sobel = cv2.GaussianBlur(Sobel, (5, 5), 0)
  # th, Sobel = cv2.threshold(Sobel, 60, 255, cv2.THRESH_BINARY)
  # Sobel = cv2.GaussianBlur(Sobel, (15, 15), 0)
  # th, Sobel = cv2.threshold(Sobel, 30, 255, cv2.THRESH_BINARY)

  cv2.imshow(windowID3, Sobel)
  k = cv2.waitKey(0)





k=0
# Close the window when key q is pressed
while k!=113:
  # Display the image

  # c_image_bur = cv2.GaussianBlur(c_image, (15, 15), 0)
  kernel = np.ones((3,3), np.uint8)
  c_image_bur=c_image
  # c_image_bur = cv2.erode(c_image, kernel, iterations = 1)
  c_image_bur = cv2.dilate(c_image_bur, kernel, iterations = 1)

  hsv = cv2.cvtColor(c_image_bur, cv2.COLOR_BGR2HSV)

  hMin=0
  sMin=0
  vMin=190

  hMax=150#179
  sMax=255
  vMax=255

  mask = cv2.inRange(hsv, np.array([hMin, sMin, vMin]), np.array([hMax, sMax, vMax]))

  output = cv2.bitwise_and(c_image,c_image, mask= mask)
  # output=c_image.copy()




  contours, _ = cv2.findContours(mask, cv2.RETR_TREE, cv2.CHAIN_APPROX_SIMPLE)

  RectBox=None
  TargetCnt=None


  maxScore=0
  for cnt in contours:
    if( len(cnt)<300 ):continue
    epsilon = 0.001*cv2.arcLength(cnt, True)
    approx = cv2.approxPolyDP(cnt, epsilon, True)
    area = cv2.contourArea(approx)
    box = cv2.minAreaRect(approx) 
    box = np.int0(cv2.boxPoints (box))
    box_area = cv2.contourArea(box)

    cv2.drawContours(output, [approx], 0,(0,0,100), 3)
    if(box_area<10 ):continue
    areaRatio=area/box_area
    if(areaRatio>1):areaRatio=1/areaRatio
    if(areaRatio<0.7):continue
    # cv2.drawContours(output, [approx], 0,(255,0,0), 3)

    score=box_area*areaRatio
    print(area,box_area,areaRatio,score)
    if(maxScore<score):
      maxScore=score
      TargetCnt=cnt
      RectBox=box


  if(TargetCnt is not None):
    edge1=RectBox[0]-RectBox[1]
    edge2=RectBox[1]-RectBox[2]

    print(np.linalg.norm(edge1))
    print(np.linalg.norm(edge2))
    
    print(RectBox)

    ellipse = cv2.fitEllipse(TargetCnt)
    center=ellipse[0]
    sl_R=ellipse[1]
    angle=ellipse[2]*math.pi/180
    if(angle>math.pi/2):angle-=math.pi
    if(angle<-math.pi/2):angle+=math.pi




    # cv2.circle(output,ellipse[1], radius=0, color=(0, 0, 255), thickness=-1)


    tarImageSize=(200, 500)

    M =getRotTranMat(center,(tarImageSize[0]/2,tarImageSize[1]/2),angle)

    output_CROP = cv2.warpAffine(c_image, M= M, dsize=tarImageSize)


    cv2.ellipse(output, ellipse, (0, 255, 0), 2)
    draw_line_angled(output,center,angle,sl_R[0]/2, (0, 255, 0),2)
    color= (255,0,0)
    cv2.drawContours(output, [TargetCnt], 0,color, 3)

    TargetRegionProcess(output_CROP)
    cv2.imshow(windowID2, output_CROP)



  cv2.imshow(windowID1, output)
  k = cv2.waitKey(0)
  break
  # If c is pressed, clear the window, using the dummy image
  if (k == 99):
    c_image= temp.copy()
    cv2.imshow(windowID1, c_image)

cv2.destroyAllWindows()
