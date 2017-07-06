#include <stdio.h>
#include "acvImage_ToolBox.hpp"

#include "acvImage_BasicDrawTool.hpp"

void plotPic()
{

}



int main()
{
  acvImage *ss = new acvImage(20,20,3);
  ss->SetROI(0,0,5,5);
  BITMAPINFOHEADER bitmapInfoHeader;

  int ret=  LoadBitmapFile(ss,"sss",&bitmapInfoHeader);
  acvThreshold(ss,5);

  delete(ss);
  // printf() displays the string inside quotation
  printf("Hello, World! %d",ret);
  return 0;
}
