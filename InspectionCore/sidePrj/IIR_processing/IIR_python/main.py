import numpy as np
import matplotlib.pyplot as plt
import scipy.fftpack
from IIR_Filter import *



def TriCos_window(n,N, a0, a1, a2, a3):

  n2PI_div_N=n*2*np.pi/N
  return (a0-a1*np.cos(n2PI_div_N)+a2*np.cos(2*n2PI_div_N)-a3*np.cos(3*n2PI_div_N))



def sigmoid(x):
  return 1 / (1 + np.exp(-x))


def sigmoid_well(x,center,width,sharpness=1):
  return sigmoid(sharpness*(-x+center-width/2))+sigmoid(sharpness*(x-center-width/2))

def sigmoid_square(x,center,width,sharpness=1):
  return 1-sigmoid_well(x,center,width,sharpness)


def chaseFilter(x,stepSize=1):
  
  def chasing(v,curv) : 
    diff=v-curv
    if(diff>stepSize):
      return curv+stepSize
    elif(diff<-stepSize):
      return curv-stepSize
    else:
      return v
  y=np.copy(x)
  i = 1
  while i < len(y):
    y[i]=chasing(y[i],y[i-1])
    i=i+1

  return y
   

fs=60
N = 32-1 #single side of filter 
x = np.linspace(0,N-1, N)
# y=1-(x+1)/40


# y_window = TriCos_window(np.linspace(N,N*2-1, N),N*2, 0.35875, 0.48829, 0.14128, 0.01168)
y_window = sigmoid(0.3*(-x+N/3))
f_stop=2/(fs/2/N)
y=y_window*sigmoid_well(x,f_stop,f_stop*1.2,2.5)* (1*(x<f_stop)+.2*(x>f_stop))
# y=np.convolve(y,np.linspace(1/3,1/3, 3), 'same')
DCGain=1
HfreqGain=y[-1]

#make the spectrum mirrored so that ifft would have real number only result
Y=np.concatenate(([DCGain],y,[HfreqGain] ,y[::-1]))
yf=scipy.fftpack.ifft(Y)

fullN=yf.size

#following are the singlesided filter
BlackManHarris_window = TriCos_window(np.linspace(fullN//2,fullN-1, fullN//2),fullN, 0.35875, 0.48829, 0.14128, 0.01168)
filter_fss=np.real(yf[:fullN//2])*BlackManHarris_window


filter_full=np.concatenate((filter_fss[::-1],filter_fss[1:]))
filter_full=filter_full/np.sum(filter_full)#make the DC gain 1

t = np.linspace(0,200-1, 200)

testY = (sigmoid_square(t,fs,fs/1.5,0.3) + sigmoid_square(t,fs*2.5,fs,4)  )
testChaseY=chaseFilter(testY,0.06)
result = np.convolve(filter_full,testChaseY)

res_t = np.linspace(0,result.size-1, result.size)-filter_full.size/2


plt.figure("FFT & FIR")

plt.subplot(211)
freqAxis = np.linspace(0,filter_fss.size-1-1, filter_fss.size-1)*fs/2/(filter_fss.size-1)
plt.plot(freqAxis,y, 'bo',freqAxis,y,  'r--')
plt.subplot(212)
plt.plot(filter_fss)

plt.figure("Conv test")
plt.plot(t,testY)
plt.plot(t,testChaseY)
plt.plot(res_t,result)

plt.show()
# y = np.sin(50.0 * 2.0*np.pi*x) + 0.5*np.sin(80.0 * 2.0*np.pi*x)
# yf = scipy.fftpack.fft(y)
# print(np.real(yf[0]))
# xf = np.linspace(0.0, 1.0/(2.0*T), N//2)

# fig, ax = plt.subplots()
# ax.plot(xf, 2.0/N * np.abs(yf[:N//2]))
# plt.show()