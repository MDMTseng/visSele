import numpy as np
import matplotlib.pyplot as plt
import scipy.fftpack
from IIR_Filter import *




if __name__ == '__main__':

  fs=30
  f0=1
  w0=2*np.pi*f0/fs
  
  pf = parallelFilter([])

  idx=0
  while(idx<3):

    chF = chase_filter(1000/fs)
    bF =approach_filter(0.8)
    nF =notch_filter(0.85,w0)
    nF2 =notch_filter(0.95,w0*1.2)
    nF3 =notch_filter(0.95,w0/1.2)

    ballSys =spike_filter(0.8,w0)
    if(idx<2):
      cf = cascadeFilter([chF,bF,nF,ballSys])
    else:
      cf = cascadeFilter([chF,bF])

    pf.push(cf)
    idx+=1




  secs=30
  abs_time=np.linspace(0, secs, secs*fs)
  time=abs_time#+((np.random.rand(secs*fs)-1)*(1.8/fs))

  # inputX=10*( (time>3) & (time<7) )#+30*np.sin(time*2*np.pi)+30*np.sin(f0*0.5*time*2*np.pi)
  # inputX=1*(time>3)
  # inputX=[[10*np.sin(t*2*np.pi*0.1), 10*np.cos(t*2*np.pi*0.1),0  ] for t in time]
  inputX=[[10*((t>3) & (t<7) ), 10*((t>3) & (t<7) ),10*((t>3) & (t<7) )  ] for t in time]
  # inputX=[[
  #   0,
  #   np.sin(t*2*np.pi* (t/secs)*2 ),
  #   0
  #   ] for t in time]
  



  # pf.setValue([10,10,10])
  output=[pf.h(v) for v in inputX]
  plt.plot(abs_time,inputX)
  plt.plot(abs_time,output)

  plt.show()


  # bF_XYZ=parallelFilter([BoxFilter(20),BoxFilter(20),BoxFilter(20)])
  
  # # bF_XYZ =BoxFilter_XYZ(20)
  # print(bF_XYZ.h([1,1,1]))
  # print(bF_XYZ.h([10,10,10]))
  # print(bF_XYZ.h([10,10,10]))
  # print(bF_XYZ.h([10,10,10]))
  # print(bF_XYZ.h([1,1,1]))
  # # bF_XYZ.setValue([1000,1000,1])
  # print(bF_XYZ.h([50,50,50]))
  # print(bF_XYZ.h([1,1,1]))
  # print(bF_XYZ.h([50,50,50]))
  # print(bF_XYZ.h([1,1,1]))
  # print(bF_XYZ.h([1,1,1]))
  # print(bF_XYZ.h([1,1,1]))