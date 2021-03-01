
/*

notch filter
1-2cos(w0)*z^-1+z^-2
----------------------
1-2r*cos(w0)*z^-1+r^2*z^-2
*/

interface calc_filter
{
  float h(float x);
}

float sigmoid(float x)
{
  return 1/(1+(float)Math.exp(-x));
}


float smoothSaturation(float v,float d_hwindow)
{
  int sign=v>0?1:-1;
  v*=sign;//v=abs(v)
  if(d_hwindow>1)d_hwindow=1;
  if(v<d_hwindow)return v*sign;
  
  if(d_hwindow==1)return sign;
  float eqv=v;
  eqv-=d_hwindow;
  
  float scaleV=1-d_hwindow;
  float y = (sigmoid(eqv/scaleV*2)-0.5)*2;
  
  y*=scaleV;
  return (y+d_hwindow)*sign;
}

class identification_filter implements calc_filter
{
  float h(float x){
    return x;
  }
    
}

class filterChase_filter implements calc_filter
{
  
  float chasePosMaxStep;
  float d_hwindow;
  filterChase_filter(float maxStep ,float d_hwindow)
  {
    this.d_hwindow=d_hwindow;
    this.chasePosMaxStep=maxStep;
  }
    

  float chasePos;
  float h(float x)
  {
    
    chasePos += smoothSaturation((x-chasePos)/chasePosMaxStep,d_hwindow)*chasePosMaxStep;
    return chasePos;
  }
}

class pack_filter implements calc_filter
{
  
  Vector<calc_filter> filters=new Vector<calc_filter>();
  pack_filter( calc_filter []filterArray)
  {
    if(filters!=null)
    {
      for(int i=0;i<filterArray.length;i++)
      {
        add(filterArray[i]);
      }
    }
  }
    
  pack_filter()
  {
  }
  void add(calc_filter f)
  {
    filters.addElement(f);
  }

  float chasePos;
  float h(float x)
  {
    float y=x;
    for (int i = 0 ; i < filters.size() ; i++){
      y= filters.get(i).h(y);
    }
    return y;
  }
}


class ZTrans_filter implements calc_filter
{
  float []ivZ;
  
  float []a;
  float []b;
  
  
  ZTrans_filter(final float []b,final float []a)
  {
    this(b,a,false);
  }
  //H(z)=(b0+b1*Z^-1+b2*Z^-2+...)/(1+a1*Z^-1+a2*Z^-2+...)
  ZTrans_filter(final float []b,final float []a, boolean keepDCGain1)
  {
    int maxOrder=a.length>b.length?a.length:b.length;
    ivZ=new float[maxOrder];
    this.a=new float[a.length];
    this.b=new float[b.length];
    
    float a_sum=0;
    
    
    float b_sum=0;
    if(keepDCGain1==true)
    {
      for(int i=0;i<a.length;i++)
      {
        a_sum+=a[i];
      }
      for(int i=0;i<b.length;i++)
      {
        b_sum+=b[i];
      }
    }
    else
    {
      a_sum=b_sum=1;
    }
    
    
    for(int i=0;i<a.length;i++)
    {
      this.a[i]=a[i];
    }
    
    
    for(int i=0;i<b.length;i++)
    {
      this.b[i]=b[i]*a_sum/b_sum;
    }
  }
  
  
  int idxRing(int idx,int len)
  {
    if(idx<0)idx+=len;
    return idx%len;
  }
  
  
  int Z0i=0;
  float h(float x)
  {//direct form II diagram
    
    
    
    ivZ[Z0i]=x;
    for(int i=1;i<a.length;i++)
    {
      ivZ[Z0i]-=a[i]*ivZ[idxRing(Z0i+i,ivZ.length)];
    }
    
    //println(ivZ);
    
    float output=0;
    for(int i=0;i<b.length;i++)
    {
      output+=b[i]*ivZ[idxRing(Z0i+i,ivZ.length)];
    }
    Z0i=idxRing(Z0i-1,ivZ.length);
    return output;
  }
  
}


class box_filter  implements calc_filter
{
  
  float []ivZ;
  float wsum=0;
  
  box_filter(int size)
  {
    ivZ=new float[size];
  }
  
  
  int idxRing(int idx,int len)
  {
    if(idx<0)idx+=len;
    return idx%len;
  }
  
  
  int Z0i=0;
  
  float h(float x)
  {
    wsum+=x-ivZ[Z0i];
    ivZ[Z0i]=x;
    
    Z0i=idxRing(Z0i+1,ivZ.length);
    return wsum/ivZ.length;
  }
}


class notch_filter  implements calc_filter
{
  /*
  notch filter
  
  b = 1-2cos(w0)*z^-1+z^-2
  ----------------------
  a = 1-2r*cos(w0)*z^-1+r^2*z^-2
  */

  ZTrans_filter filter;
  notch_filter(float r,float w0,float dampingFactor)
  {
    //super(
    //  new float[]{1,-2*r*(float)Math.cos(w0),r*r},
    //  new float[]{1,  -2*(float)Math.cos(w0),  1},true);
    filter = new ZTrans_filter(
      new float[]{1,  -2*(float)Math.cos(w0)*dampingFactor,  1*dampingFactor*dampingFactor},
      new float[]{1,-2*r*(float)Math.cos(w0)*dampingFactor,r*r*dampingFactor*dampingFactor},true);
  }
  
  notch_filter(float r,float f0, float fs,float dampingFactor)
  {
    this(r,(float)(2*Math.PI*f0/fs),dampingFactor);
  }
  
  float h(float x)
  {
    return filter.h(x);
  }
}



class lowpass_filter  implements calc_filter
{
  /*
  notch filter
  
  b = 1-2cos(w0)*z^-1+z^-2
  ----------------------
  a = 1-2r*cos(w0)*z^-1+r^2*z^-2
  */

  ZTrans_filter filter;
  lowpass_filter(float r,float w0)
  {
    float a=(float)(Math.sin(1)/Math.sin(1));
    filter = new ZTrans_filter(
      new float[]{-a,1},
      new float[]{1,-a},true);
  }
  
  lowpass_filter(float r,float f0, float fs)
  {
    this(r,(float)(2*Math.PI*f0/fs));
  }
  
  float h(float x)
  {
    return filter.h(x);
  }
}

class spike_filter  implements calc_filter
{
  ZTrans_filter filter;
/*
spike filter

b = 1-2r*cos(w0)*z^-1+r^2*z^-2
----------------------
a = 1-2cos(w0)*z^-1+z^-2
*/
  spike_filter(float r,float w0,float dampingFactor)
  {
    //super(
    //  new float[]{1,-2*r*(float)Math.cos(w0),r*r},
    //  new float[]{1,  -2*(float)Math.cos(w0),  1},true);
    filter = new ZTrans_filter(
      new float[]{1,-2*r*(float)Math.cos(w0)*dampingFactor,r*r*dampingFactor*dampingFactor},
      new float[]{1,  -2*(float)Math.cos(w0)*dampingFactor,  1*dampingFactor*dampingFactor},true);
  }
  
  spike_filter(float r,float f0, float fs,float dampingFactor)
  {
    this(r,(float)(2*Math.PI*f0/fs),dampingFactor);
  }
  
  float h(float x)
  {
    return filter.h(x);
  }
}

class bandstop_filter  implements calc_filter//TODO: DONE
{
  ZTrans_filter zfilter;
  
  void initZ(float a,float b)
  {
    
    zfilter=new ZTrans_filter(
      new float[]{(1-b)/(1+b),-(2*a)/(1+b),  1},
      new float[]{1,-(2*a)/(1+b),(1-b)/(1+b)});
  }
  
  
  void initZ_wlh(float wl,float wh)
  {
    float a = 0;
    float b = 0;
    initZ(a,b);
  }
  
  bandstop_filter(float fl,float fh,float fs)
  {
    float a =0;
    float b =0;
    initZ(a,b);
  }
  
  bandstop_filter(float wl,float wh)
  {
    float a =0;
    float b =0;
    initZ(a,b);
  }
  
  float h(float x)
  {
    return zfilter.h(x);
  }
  
}



class approach_filter  extends ZTrans_filter
{
  
/*
notch filter

b = 1-2cos(w0)*z^-1+z^-2
----------------------
a = 1-2r*cos(w0)*z^-1+r^2*z^-2
*/
  approach_filter(float alpha)
  {
    super(
      new float[]{1-alpha},
      new float[]{1,-alpha});
  }
}
  //H(z)=(b0+b1*Z^-1+b2*Z^-2+...)/(1+a1*Z^-1+a2*Z^-2+...)
  