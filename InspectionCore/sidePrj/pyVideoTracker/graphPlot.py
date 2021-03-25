import matplotlib.pyplot as plt
import csv
import sys
import cv2
import os
import argparse as ap
# import Tkinter, tkFileDialog
import tkinter as tk
from tkinter import filedialog

root = tk.Tk()
root.withdraw()




TOffset=30
YFactor=1.0/939
YbMult=1
graphOffset=0.5

def press(event):
  global TOffset
  global YbMult
  global YFactor
  global graphOffset
  print('press', event.key)
  if event.key == 'enter':
    print("enter")
  elif event.key == 'd':
    TOffset-=5
  elif event.key == 'a':
    TOffset+=5

  elif event.key == 'w':
    YbMult*=1.01
  elif event.key == 's':
    YbMult/=1.01

  elif event.key == 'u':
    graphOffset+=0.1
  elif event.key == 'j':
    graphOffset-=0.1

  elif event.key == 'i':
    YFactor*=1.01
  elif event.key == 'k':
    YFactor/=1.01

  elif event.key == 'y':
    YFactor*=-1
  elif event.key == 'h':
    YbMult*=-1


fig, ax = plt.subplots()
fig.canvas.mpl_disconnect(fig.canvas.manager.key_press_handler_id)
fig.canvas.mpl_connect('key_press_event', press)

# plt.ion()
def dataPlot(r_a,r_b):
  plotCount=0
  idxP2=5
  idxP1=3
  while True:
    plt.ion()
    index_a = 1
    index_b = index_a

    if(TOffset>0):
      index_b+=TOffset
    else:
      index_a-=TOffset
    plotT=[]

    plotYa1=[]
    plotYb1=[]
    initOffset_a1 = float(r_a[index_a][idxP1])-float(r_a[index_a][1])
    initOffset_b1 = float(r_b[index_b][idxP1])-float(r_b[index_b][1])

    plotYa2=[]
    plotYb2=[]
    initOffset_a2 = float(r_a[index_a][idxP2])-float(r_a[index_a][1])
    initOffset_b2 = float(r_b[index_b][idxP2])-float(r_b[index_b][1])
    while index_a < len(r_a) and index_b < len(r_b) :
      # print(index_a, r_a[index_a], "<>",index_b, r_b[index_b],)
      plotT.append(len(plotT)/30.0)
      plotYa1.append(( float(r_a[index_a][idxP1])-float(r_a[index_a][1]  )-initOffset_a1)*YFactor      +graphOffset)
      plotYb1.append(( float(r_b[index_b][idxP1])-float(r_b[index_b][1]  )-initOffset_b1)*YFactor*YbMult+graphOffset)


      plotYa2.append(( float(r_a[index_a][idxP2])-float(r_a[index_a][1]  )-initOffset_a2)*YFactor      )
      plotYb2.append(( float(r_b[index_b][idxP2])-float(r_b[index_b][1]  )-initOffset_b2)*YFactor*YbMult)
      index_a+= 1
      index_b+= 1


    ax.plot(plotT,plotYa1,'m',plotT,plotYb1,'k')
    ax.plot(plotT,plotYa2,'m',plotT,plotYb2,'k')
    plt.draw()
    print("plot..",plotCount)
    plotCount+=1
    plt.pause(0.1)
    ax.clear()
  
  plt.ioff()
  plt.show()


  # plt.draw()
  # plt.pause(0.0001)
  # input()
  # plt.plot(plotT,plotYa,plotT,plotYb)
  # plt.show()


def run(args):
  try:
    
    fileName_a=os.path.basename(args['a']).replace(".", "_")
    fileName_b=os.path.basename(args['b']).replace(".", "_")
    fig.canvas.set_window_title(fileName_a+'_'+fileName_b)
    # fig.suptitle(fileName_a+'_'+fileName_b)
    with open(args['a'], newline='') as a_csv, open(args['b'], newline='') as b_csv:
      r_a = list(csv.reader(a_csv, delimiter=','))
      r_b = list(csv.reader(b_csv, delimiter=','))
      
      dataPlot(r_a,r_b)


  except IOError as e:
    print(e)
    sys.exit(1)
  except Exception as other_exception:
    print(other_exception)
    sys.exit(2)


#python graphPlot.py -a ~/Movies/TEST/TEST0308/Test0308/Damped/EDGE/100cm\ X\(edge\)_mp4_2021-03-10-03-29-08.csv -b ~/Movies/TEST/TEST0308/Test0308/Damped/EDGE/100cm\ Y\(edge\)_mp4_2021-03-10-03-30-50.csv 




if __name__ == "__main__":
  # Parse command line arguments
  parser = ap.ArgumentParser(description='Process some integers.')
  parser.add_argument('-a', "--a", help="a.scv")
  parser.add_argument('-b', "--b", help="b.scv")
  args = parser.parse_args()
  args=vars(args)

  if(args['a']==None and args['b']==None):
    fileNames=filedialog.askopenfilenames()
    args['a'] = fileNames[0]
    args['b'] = fileNames[1]
  else:
    if(args['a']==None):
      args['a'] = filedialog.askopenfilename()
    if(args['b']==None):
      args['b'] = filedialog.askopenfilename()
  run(args)