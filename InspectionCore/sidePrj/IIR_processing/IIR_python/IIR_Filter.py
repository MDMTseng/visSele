import numpy as np


def IIR():
  print("sss")


class calc_filter:
  def __init__(self):
    self.buffer=0
  
  def setValue(self,newValue=None):
    if newValue is not None:
      self.buffer=newValue

  def h(self,newValue=None):
    if newValue is not None:
      self.buffer=newValue
    return self.buffer

 


class delayFilter(calc_filter):

  def __init__(self, size,defaltValue=0):
    self.buffer=0
    self.curIdx=0
    self.setup(size+1,defaltValue)
  
  def setup(self,size,defaltValue=0):
    self.buffer=np.linspace(defaltValue, defaltValue, size)
    self.curIdx=0

  def setValue(self,newValue=None):
    if newValue is not None:
      self.buffer=newValue

  def push(self,newValue):
    self.buffer[self.curIdx]=newValue
    self.curIdx=(self.curIdx+1)% len(self.buffer)

  def h(self,newValue=None):
    if newValue is not None:
      self.push(newValue)
    return self.buffer[self.curIdx]



class ZFilter(calc_filter):

  def __init__(self, b,a,gainDC1=True):
    self.buffer=[]
    self.a=[]
    self.b=[]
    self.curIdx=0
    self.setup(b,a,gainDC1)
    self.cur_v=0
  

  def getDCGain(self):
    a_sum=sum(self.a)
    b_sum=sum(self.b)
    return b_sum/a_sum

  def setup(self, b,a,gainDC1=True):
    maxOrder=max(len(a),len(b))
    self.buffer=np.linspace(0, 0, maxOrder)
    self.a=a.copy()
    self.b=b.copy()

    if(gainDC1==True):
      dcGain=self.getDCGain()
      self.b=self.b/dcGain

  def setValue(self, value):
    b_sum=sum(self.b)
    self.cur_v=value
    value/=b_sum
    idx=0
    while(idx<len(self.buffer)):
      self.buffer[idx]=value
      idx+=1

  def idxRing(self,idx,length=None):

    if length is None:
      length=len(self.buffer)
    if(idx<0):idx+=length
    return idx%length
  


  def push(self,newValue):
    self.buffer[self.curIdx]=newValue

    idx=1
    while(idx<len(self.a)):
      self.buffer[self.curIdx]-=self.a[idx]*self.buffer[self.idxRing(self.curIdx+idx)]
      idx+=1


    output=0
    idx=0
    while(idx<len(self.b)):
      output+=self.b[idx]*self.buffer[self.idxRing(self.curIdx+idx)]
      idx+=1
    self.cur_v=output
    self.curIdx=self.idxRing(self.curIdx-1)

  def h(self,newValue=None):
    if newValue is not None:
      self.push(newValue)
    return self.cur_v


class notch_filter(calc_filter):

  # /*
  # notch filter
  
  # b = 1-2cos(w0)*z^-1+z^-2
  # ----------------------
  # a = 1-2r*cos(w0)*z^-1+r^2*z^-2
  # */

  def __init__(self, r,w0,dampingFactor=1):
    b=[1,  -2*np.cos(w0)*dampingFactor,  1*dampingFactor*dampingFactor]
    a=[1,-2*r*np.cos(w0)*dampingFactor,r*r*dampingFactor*dampingFactor]

    self.filter=ZFilter(b,a,True)

  def setValue(self, value):
    return self.filter.setValue(value)

  
  def h(self,newValue=None):
    return self.filter.h(newValue)


class spike_filter(calc_filter):

  # /*
  # notch filter
  
  # b = 1-2cos(w0)*z^-1+z^-2
  # ----------------------
  # a = 1-2r*cos(w0)*z^-1+r^2*z^-2
  # */

  def __init__(self, r,w0,dampingFactor=1):
    a=[1,  -2*np.cos(w0)*dampingFactor,  1*dampingFactor*dampingFactor]
    b=[1,-2*r*np.cos(w0)*dampingFactor,r*r*dampingFactor*dampingFactor]
    self.filter=ZFilter(b,a,True)

  def setValue(self, value):
    return self.filter.setValue(value)

  
  def h(self,newValue=None):
    return self.filter.h(newValue)


class approach_filter(calc_filter):

  def __init__(self,alpha=0.9):
    b=np.array([1-alpha])
    a=np.array([1,-alpha])
    self.filter=ZFilter(b,a,True)

  def setValue(self, value):
    return self.filter.setValue(value)

  
  def h(self,newValue=None):
    return self.filter.h(newValue)



class chase_filter(calc_filter):
  def __init__(self,step_max):
    self.pre_v=0
    self.step_max=0
    self.setStep(step_max)

  def setValue(self, value):
    self.pre_v=value
  def setStep(self, step_max):
    self.step_max=step_max

  
  def h(self,newValue=None):
    
    if newValue is not None:

      diff = newValue-self.pre_v
      if(diff>self.step_max):
        diff=self.step_max
      elif(diff<-self.step_max):
        diff=-self.step_max
      self.pre_v+=diff

    return self.pre_v



class BoxFilter(calc_filter):

  def __init__(self, size,defaltValue=0):
    self.buffer=0
    self.curSum=0
    self.curIdx=0
    self.setup(size,defaltValue)
  
  def setup(self,size,defaltValue=0):
    self.buffer=np.linspace(defaltValue, defaltValue, size)
    self.curSum=defaltValue*size
    self.curIdx=0
    
  def setValue(self, value):
    idx=0
    while(idx<len(self.buffer)):
      self.buffer[idx]=value
      idx+=1
    self.curSum=value*len(self.buffer)



    
  def push(self,newValue):
    self.curSum-=self.buffer[self.curIdx]
    self.curSum+=newValue
    self.buffer[self.curIdx]=newValue
    self.curIdx=(self.curIdx+1)% len(self.buffer)

  def h(self,newValue=None):
    if newValue is not None:
      self.push(newValue)
    return self.curSum/len(self.buffer)
    


class parallelFilter(calc_filter):
  def __init__(self,filters):
    self.Filters=filters
  
  def push(self,filter):
    self.Filters.append(filter)
  

  def setValue(self, value):
    idx=0
    while(idx<len(self.Filters)):
      self.Filters[idx].setValue(value[idx])
      idx+=1

  def h(self,newValue=None):
    if newValue is None:
      return  [filt.h(None) for filt in self.Filters]
    return  [self.Filters[i].h(v) for i,v in enumerate(newValue)]



class cascadeFilter(calc_filter):
  def __init__(self,filters):
    self.Filters=filters
  
  
  def push(self,filter):
    self.Filters.append(filter)

  def setValue(self, value):
    idx=0
    while(idx<len(self.Filters)):
      self.Filters[idx].setValue(value)
      idx+=1

  def h(self,newValue=None):
    output=newValue
    if(output is None):
      return self.Filters[-1].h(None)

    idx=0
    while(idx<len(self.Filters)):
      output=self.Filters[idx].h(output)
      idx+=1
    return  output