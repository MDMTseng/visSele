
class EQ_filter implements calc_filter
{
  float []ivZ;
  float []outputBuffer;
  
  double TriCos_window(int n,int N,double a0,double a1,double a2,double a3)
  {
    double n2PI_div_N=n*2*Math.PI/N;
    return (a0-a1*Math.cos(n2PI_div_N)+a2*Math.cos(2*n2PI_div_N)-a3*Math.cos(3*n2PI_div_N));
  }
  double Nuttall_window(int n,int N)
  {
    double a0=0.355768;
    double a1=0.487396;
    double a2=0.144232;
    double a3=0.012604;
    return TriCos_window( n, N, a0, a1, a2, a3);
  }
  double BlackManNuttall_window(int n,int N)
  {
    double a0=0.3635819;
    double a1=0.4891775;
    double a2=0.1365995;
    double a3=0.0106411;
    return TriCos_window( n, N, a0, a1, a2, a3);
  }
  double BlackManHarris_window(int n,int N)
  {
    double a0=0.35875;
    double a1=0.48829;
    double a2=0.14128;
    double a3=0.01168;
    return TriCos_window( n, N, a0, a1, a2, a3);
  }
  double windowFunction(int n,int N)
  {
    return BlackManHarris_window(n,N);
    //return 1;
  }
  Complex[] Filter2FIR(float []FreqMag)
  {
    
    FFT fft=new FFT();
    Complex []mag_C=new Complex[FreqMag.length*2];
    mag_C[FreqMag.length]=new Complex(0,0);
    for(int i=0;i<FreqMag.length;i++)
    {
      mag_C[i]=new Complex(FreqMag[i],0);
      if(i>0)
      {
        mag_C[mag_C.length-i]=mag_C[i];//new Complex(0,0);
      }
    }
    //Complex czero=new Complex(0,0);
    //for(int i=FreqMag.length;i<mag_C.length;i++)
    //{
    //  mag_C[i]=czero;
    //}
    
    
    
    return fft.ifft(mag_C);
  }

  public Complex[] fir;
  EQ_filter(float[] EQSetting)
  {
    
    fir = Filter2FIR(EQSetting);
    
    ivZ=new float[fir.length];
    
    int windowSpan = (fir.length-2)/2;
    
    int N=fir.length;
    
    
    //println("fir.length:"+fir.length);
    //println("windowSpan:"+windowSpan);
    
    fir[0]=fir[0].times(new Complex(windowFunction(N/2,N),0));
    //println("fir[0]:"+fir[0]);
    for(int i=1;i<windowSpan+1;i++)
    {
      
      float w = (float)windowFunction(i+N/2,N);
      fir[i]=fir[i].times(new Complex(w,0));
      //println("-fir["+i+"]:"+fir[i]);
    }
    
    
    float wEqFactor=0;
    wEqFactor=fir[0].re();
    for(int i=1;i<windowSpan+1;i++)
    {
      wEqFactor+=2*fir[i].re();
    }
    
    fir[0]=fir[0].times(new Complex(1.0/wEqFactor,0));
    
    
    for(int i=1;i<windowSpan+1;i++)
    {
      fir[i]=fir[i].times(new Complex(1.0/wEqFactor,0));
    }
    
    
  }
  
  int idxRing(int idx,int len)
  {
    while(idx<0)idx+=len;
    return idx%len;
  }
  
  
  int Z0i=0;
  
  float h(float x)
  {
    boolean pEBG=false;
    ivZ[Z0i]=x;
    // [0] * * * m * * *  => windowspan =3
    int windowSpan = (fir.length-2)/2;
    int centerIdx = Z0i-windowSpan-1;
    
    float temp=ivZ[idxRing(centerIdx,ivZ.length)];
    float w=fir[0].re();
    float output =temp*w;
    if(pEBG)
    {
      
      println("============Z0i:"+Z0i+"  windowSpan:"+windowSpan);
      println("id["+0+"]:"+idxRing(centerIdx,ivZ.length)+"  temp:"+temp+" w:"+w);
    }
    for(int i=1;i<windowSpan+1;i++)
    {
      temp=ivZ[idxRing(centerIdx+i,ivZ.length)]+
           ivZ[idxRing(centerIdx-i,ivZ.length)];
           
      w=fir[i].re();
      if(pEBG)
        println("id["+i+"]:"+idxRing(centerIdx+i,ivZ.length)+"::"+idxRing(centerIdx-i,ivZ.length)+"  temp:"+temp+" w:"+w);
      output +=  temp*w;
    }
    if(pEBG)
      println("output:"+output);
    
    
    Z0i=idxRing(Z0i+1,ivZ.length);
    return output;
  }
  
  
  
  
  
  
}