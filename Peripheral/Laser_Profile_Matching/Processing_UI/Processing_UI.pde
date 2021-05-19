 
import processing.serial.*;
import static javax.swing.JOptionPane.*;
Serial myPort=null;        // The serial port
 
final boolean debug = true;
 
 
 
int UI_SelectArray(Object[] array,int defaultIdx)
{
  if(defaultIdx<0 || defaultIdx>=array.length)
  {
    defaultIdx=0;
  }
  Object selectStr=(Object)showInputDialog(null, "Choose", "Menu", PLAIN_MESSAGE, null, array, array[defaultIdx]);
  return java.util.Arrays.asList(array).indexOf(selectStr);
}
 
 
String UI_SelectSerialPortName(String preferNameContains)
{
  String[] options =Serial.list();
  int defaultIdx=0;
  for(int i=0;i<options.length;i++)
  {
    if(options[i].contains(preferNameContains))
    {
      defaultIdx=i;
      break;
    }
  }
  
  int idx=UI_SelectArray(options, defaultIdx);
  if(idx<0)return null;
  return options[idx];
}

int UI_SelectBaudrate(int defaultBaud)
{
  
  Integer[] options =new Integer[]{9600, 14400, 19200, 38400, 57600, 115200, 128000, 256000};
  int defaultIdx=java.util.Arrays.asList(options).indexOf(defaultBaud);
  int idx=UI_SelectArray(options, defaultIdx);
  if(idx<0)return -1;
  return options[idx];
}

 
void setup() {

  size(800, 600);
  try {
    if(debug) printArray(Serial.list());
    String portName = "/dev/tty.SLAB_USBtoUART";//UI_SelectSerialPortName("SLAB");
    if(debug) println(portName);
    if(portName!=null){
      int selBaud=115200;//UI_SelectBaudrate(115200);
      if(selBaud==-1)selBaud=115200;
      myPort = new Serial(this, portName, selBaud); // change baud rate to your liking
      myPort.bufferUntil('\n'); // buffer until CR/LF appears, but not required..
    }
    else {
      showMessageDialog(frame,"Device is not connected to the PC");
      exit();
    }
  }
  catch (Exception e)
  { //Print the type of error
    showMessageDialog(frame,"COM port is not available (may\nbe in use by another program)");
    println("Error:", e);
    exit();
  }
}




float []cacheRec=new float[1000];
final int reqRecID=456;

void CMD_parse(SimpPacketParse sBuffer)
{
  char TLC0=sBuffer.buffer[0];
  char TLC1=sBuffer.buffer[1];
  
  println("TLC:"+TLC0+TLC1);
  
  if(TLC0=='N' && TLC1=='N')
  {
    int offset =2;//start char+2 TLC mark
    String CMD = new String(sBuffer.buffer,offset,sBuffer.size()-offset);
    
    println("CMD:"+CMD);
  }
  
  
  if(TLC0=='t' && TLC1=='t')
  {
    int offset =2;//start char+2 TLC mark
    String CMD = new String(sBuffer.buffer,offset,sBuffer.size()-offset);
    
    println("CMD:"+CMD);
  }
  
  
  if(TLC0=='j' && TLC1=='s')
  {
    int offset =2;//start char+2 TLC mark
    String CMD = new String(sBuffer.buffer,offset,sBuffer.size()-offset-1);
   
    //println("CMD:"+CMD);
    JSONObject json= parseJSONObject(CMD);
    if (json != null) {
      println("CMD:"+CMD);
      
      int id = json.getInt("id");
      
      switch(id)
      {
        case reqRecID:
        {
          
          JSONArray recArr = json.getJSONArray("rec");
          for (int i = 0; i < recArr.size(); i++) {
            cacheRec[i]=(recArr.getInt(i));
          }
           
          if(recArr.size()>0)
          {
            pushMatrix();
            background(255);
            smooth();
            
            rectMode(CENTER); // show bounding boxes
            println(recArr.size());
            println(cacheRec[0]);
            translate(0, height/2);
            drawGraph(cacheRec,recArr.size(),1200,width,-0.4);
            popMatrix();
          }
        }
        break;
      }
      
  //size(300, 200);
  //background(255);
  //smooth();
  
  //rectMode(CENTER); // show bounding boxes
  //drawGraph(ssdsd,ssdsd[0],width,0.08);
    }
    else
    {
      
    }
  }
}

SimpPacketParse sBuffer = new SimpPacketParse(1024);


void mouseClicked() {
  //JSONObject json= new JSONObject();
  
  //json.setInt("id", 0);
  //json.setString("species", "Panthera leo");
  //json.setString("name", "Lion");
  //json.setString("ECHO", "LionSS");
  //String fd="@ST"+json.toString()+"$";
  
  String fd="@JS{\"id\":"+reqRecID+",\"type\":\"get_cache_rec\"}$@JS{\"id\":567,\"type\":\"empty_cache_rec\"}$";
  println(fd);
  myPort.write(fd);
}


void draw() {
  // Expand array size to the number of bytes you expect
  byte[] inBuffer = new byte[7];
  while (myPort.available() > 0) {
    int readLen=myPort.readBytes(inBuffer);
    {
      
      for(int i=0;i<readLen;i++)
      {
        if(sBuffer.feed((char)inBuffer[i]))
        {
          CMD_parse(sBuffer);
          sBuffer.clean();
        }
      }
    }
  }
}


float []ssdsd=new float[]{
  
  450,
528,608,640,704,740,552,792,821,822,768,914,1034,915,822,676,534,525,517,496,546,608,768,917,1066,1264,1369,1423,1477,1520,1345,1040,823,
662,624,662,682,714,733,756,784,804,909,984,994,1056,1062,1072,1168,1281,1382,1428,1484,1535,1584,1510,1385,1296,732,

};

void drawGraph(float[]arr,int length,float xcenter,int printWidth,float mult)
{
  for(int i=1;i<length;i++)
  {
    int X0=(i-1)*printWidth/length;
    float Y0=(arr[i-1]-xcenter)*mult;
    int X1=i*printWidth/length;
    float Y1=(arr[i]-xcenter)*mult;
    line(X0, Y0, X1, Y1);
  }
}