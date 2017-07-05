#include <stdio.h>
#include "acvImage_ToolBox.hpp"
int main()
{
  acvImage *ss = new acvImage();
  ss->SetROI(0,0,5,5);
  acvThreshold(ss,5);
   // printf() displays the string inside quotation
      printf("Hello, World!");
      return 0;
}
