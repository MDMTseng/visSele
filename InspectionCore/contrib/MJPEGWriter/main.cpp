#include "MJPEGWriter.h"
int
main()
{
    MJPEGWriter test(7777);

    VideoCapture cap;
    bool ok = cap.open(0);
    if (!ok)
    {
        printf("no cam found ;(.\n");
        pthread_exit(NULL);
    }
    Mat frame;
    cap >> frame;
    test.write(frame);
    frame.release();
    test.start();

    Mat blank1 = Mat::zeros(Size(16,8),CV_8UC1);


    int frameCode=0;
    while(cap.isOpened()){
      cap >> frame; 

      frameCode++;
      // if(frameCode%10!=0)continue;
      // if(rand()%10!=0)continue;
      int tmpT=frameCode;
      for(int x=0;x<16;x++)
      {
          // get pixel
          Vec3b & color = frame.at<Vec3b>(0,x);


          color[0] = 255*(tmpT&1);
          tmpT>>=1;
          color[1] = 255*(tmpT&1);
          tmpT>>=1;
          color[2] = 255*(tmpT&1);
          tmpT>>=1;

      }

      cv::putText(frame, //target image
            "Hello, OpenCV!", //text
            cv::Point(10, frame.rows / 2), //top-left position
            cv::FONT_HERSHEY_DUPLEX,
            1.0,
            CV_RGB(118, 185, 0), //font color
            2);
      test.write(frame); frame.release();
      // test.write(blank1);
      
    }
    test.stop();
    exit(0);

}
