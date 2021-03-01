import java.util.*;


FFT fft=new FFT();
  
float fs=60;
float s_r=0.94;
float s_fs=fs;
float s_f0=2f;
calc_filter iir_spike=new spike_filter(s_r,s_f0,s_fs,0.992);// 1 order low pass

  
float r=0.96;
float fa=0.1;
float f0=s_f0;
calc_filter iir_notch=new notch_filter(r,f0,fs,1);
calc_filter iir_notch2=new notch_filter(r,f0-fa,fs,1);
calc_filter iir_notch3=new notch_filter(r,f0+fa,fs,1);

ZTrans_filter dampfilter = new ZTrans_filter(
      new float[]{1},
      new float[]{1,0.5},true);


calc_filter chase_filter=new filterChase_filter(10,0.3);
  
float approach=0.5;
calc_filter iir_LP=new approach_filter(approach);// 1 order low pass
calc_filter boxFilter = new box_filter(5);




pack_filter progFilter1Cmd= new pack_filter(new calc_filter[]{chase_filter,iir_LP,boxFilter});
//pack_filter progFilter1Cmd= new pack_filter(new calc_filter[]{EqFilter});


pack_filter ballSystem= new pack_filter(new calc_filter[]{iir_spike});


//pack_filter bandStop= new pack_filter(new calc_filter[]{iir_notch,iir_notch2,iir_notch3,dampfilter});
pack_filter bandStop= new pack_filter();//new calc_filter[]{EqFilter});




class drawGraph
{
  float []data;
  int delayDisplay=0;
  drawGraph(int segments)
  {
    this(segments,0);
  }
  drawGraph(int segments,int delayDisp)
  {
    delayDisplay=delayDisp;
    data=new float[segments+delayDisp];
  }
  
  int idxRing(int idx,int len)
  {
    while(idx<0)idx+=len;
    return idx%len;
  }
  
  
  int headi=0;
  
  
  void newData(float x)
  {
    data[headi]=x;
    headi=idxRing(headi+1,data.length);
  }
  void draw(float mag,float w)
  {
    int c_idx=idxRing(headi,data.length);
    int Ylen = data.length-delayDisplay;
    for(int i=1;i<Ylen;i++)
    {
      int n_idx=idxRing(c_idx+1,data.length);
      line(w*(i-1)/(Ylen),-data[c_idx]*mag,w*(i)/(Ylen),-data[n_idx]*mag);
      c_idx=n_idx;
    }
  }
}

float [] filterEQSmooth(float []filterEQ)
{
  float []newfilter=new float[filterEQ.length];
  for(int i=0;i<filterEQ.length;i++)
  {
    float sum=0;
    for(int j=-1;j<2;j++)
    {
      int idx=i+j;
      if(idx<0){
        sum+=1;
        continue;
      }
      else if (idx>=filterEQ.length)idx=filterEQ.length-1;
      sum+=filterEQ[idx];
    }
    newfilter[i]=sum/3;
  }
  return newfilter;
}

int EQLen=64;
int dispDelay=64;
int gLen=300;

drawGraph g_raw=new drawGraph(gLen,dispDelay);
drawGraph g_filter1=new drawGraph(gLen,dispDelay);
drawGraph g_ballSystem=new drawGraph(gLen,dispDelay);
drawGraph g_final=new drawGraph(gLen);//After EQ, no delay
Complex[] firX;
void setup() {
  size(640, 360);
  strokeWeight(2);
  frameRate(fs);
  
  //noLoop();
  
  float []EQSetup=new float[EQLen];
  float wl=EQSetup.length*1.2f/fs;
  float wh=EQSetup.length*8f/fs;
  println("wl:"+wl+"  wh:"+wh);
  
  for(int k=0;k<=2;k++)
  {
    EQSetup=filterEQSmooth(EQSetup);
    
    EQSetup[0]=1;
    for(int i=1;i<EQSetup.length;i++)
    {
      //EQSetup[i]=1;
      if(i<wl)
        EQSetup[i]=1;
      else if( i>wh)
        EQSetup[i]=1;
    }
  }
  
  
  
  //EQSetup[1]=0;
  EQSetup[0]=1;//DC always 1
  //EQSetup=filterEQSmooth(EQSetup);
  EQ_filter EqFilter= new EQ_filter(EQSetup);
  bandStop.add(EqFilter);

}


boolean simStop=false;
boolean addNoise=false;
int inputType=1;
void keyPressed() {
  switch(key)
  {
   case '0':
   inputType =0;
   break;
   case '1':
   inputType =1;
   break;
   case '2':
   inputType =2;
   break;
   case '3':
   inputType =3;
   break;
   case '4':
   
   inputType =4;
   break;
   case '5':
   
   inputType =5;
   break;
   case '6':
   
   inputType =6;
   break;
   
   
   case 's':
   
   simStop=!simStop;
   break;
   
   case 'n':
   
   addNoise=!addNoise;
   break;
  }
}



float walkerPos=0;
float walkerSpeed=0;
void draw() {
  if(simStop)return;
  stroke(0);
  background(204);
  float cmdPos=0;
  
  //{
    
  //  stroke(102, 0, 204,150);
  //  int h=height-200;
  //  int xMul=8;
  //  int yMul=230;
  //  for(int i=0;i<firX.length;i++)
  //  {
  //    line(i*xMul,h,i*xMul,h-((float)firX[i].re()*yMul));
  //  }
  //  stroke(0, 255, 0,150);
  //  for(int i=0;i<firX.length;i++)
  //  {
  //    line(i*xMul+1,h,i*xMul+1,h-((float)firX[i].im()*yMul));
  //  }
  //}

  
  
  walkerSpeed += random(-1, 1)*0.4;
  walkerSpeed = smoothSaturation(walkerSpeed,0);
  walkerPos=smoothSaturation(walkerPos+walkerSpeed,0);
  switch(inputType)
  {
    case 0:
      cmdPos=(float)Math.sin(2*Math.PI*millis()/1000*f0);
    break;
    
    case 1:
      
      cmdPos=(float)Math.sin(2*Math.PI*millis()/1000*(0.1));
    break;
    case 2:
      cmdPos=(float)Math.sin(2*Math.PI*millis()/1000*(f0*0.5))+(float)Math.sin(2*Math.PI*millis()/1000*f0);
    break;
    case 3:
      cmdPos=(float)Math.sin(2*Math.PI*millis()/1000*(f0+0.5));
    break;
    case 4:
      cmdPos= walkerPos;
    break;
    
    case 5:
      cmdPos= 1;
    break;
    case 6:
      cmdPos= -1;
    break;
  }
  if(addNoise)
   cmdPos+=walkerPos*0.2;
  
  //cmdPos=(float)Math.pow(cmdPos,11);
  //cmdPos=cmdPos>0?1:-1;
  cmdPos*=70;
  
  line(0,0,0,height);
  float y=cmdPos;
  float y_raw=y;
  translate(width/2, height/2);
  //point(y, 0);
  //text("raw:",20,0);
  
  
  
  translate(0, 10);
  y = progFilter1Cmd.h(y);
  float y_filer1=y;
  //point(y, 0);
  //text("filter1:",20,0);
  
  
  
  translate(0, 10);
  float BKy=y;
  y = ballSystem.h(y);
  float y_ballSystem=y;
  //point(y, 0);
  //text("ballSystem:",20,0);
  
  
  translate(0, 10);
  y=bandStop.h(y);
  //point(y, 0);
  //text("iir_notch:",20,0);
  
  
  
  
  
  {
    stroke(0, 0, 0);
    pushMatrix();
    
    g_raw.newData(y_raw);
    translate(-width/2,0);
    g_raw.draw(1,width);
    popMatrix();
  }
  {
    stroke(102, 0, 204,150);
    pushMatrix();
    
    g_filter1.newData(y_filer1);
    translate(-width/2,0);
    g_filter1.draw(1,width);
    popMatrix();
  }
  
  {
    stroke(0, 102, 204,150);
    pushMatrix();
    
    g_ballSystem.newData(y_ballSystem);
    translate(-width/2,0);
    g_ballSystem.draw(1,width);
    popMatrix();
  }
  
  {
    stroke(204, 102, 0,150);
    pushMatrix();
    
    g_final.newData(y);
    translate(-width/2,0);
    g_final.draw(1,width);
    popMatrix();
  }
  
}