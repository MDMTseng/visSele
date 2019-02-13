#include <cstdio>
#include <opencv2/opencv.hpp>
using namespace cv;
#include <vector>
/*

******************** Opencv Setup holes ********************

setup opencv sys PATH=C:\opencv\build\x64\vc14\bin
-Change the path and vc version name accordingly

In VC
Debug->Project properties

VC++ Directories
-include Directories += C:\opencv\build\include
-Library Directories += C:\opencv\build\x64\vc14\lib
Linker->Input
-Additional dependencies +=
opencv_calib3d2413.lib;opencv_contrib2413.lib;opencv_core2413.lib;opencv_features2d2413.lib;opencv_flann2413.lib;opencv_gpu2413.lib;opencv_highgui2413.lib;opencv_imgproc2413.lib;opencv_legacy2413.lib;opencv_ml2413.lib;opencv_nonfree2413.lib;opencv_objdetect2413.lib;opencv_ocl2413.lib;opencv_photo2413.lib;opencv_stitching2413.lib;opencv_superres2413.lib;opencv_ts2413.lib;opencv_video2413.lib;opencv_videostab2413.lib;

!!IMPORTANT!!:
lib files has two versions,
dbg(opencv_XXX____d.lib) and release(opencv_XXX____.lib)
notice that the d indicates if the lib file is for dbg build.
So choose that accordingly
Or you would have imread not working(the traditional way to read image still working but don't use it), 
Some lib address accessing violation, ui window name is scrambled 

*/



int webcam()
{
	cv::VideoCapture camera_test;
	int device_counts = 0;
	while (true) {
		if (!camera_test.open(device_counts)) {
			break;
		}
		camera_test.release();
		device_counts++;
	}
	
	std::cout << "devices count : " << device_counts << std::endl;
	if (device_counts == 0)
	{
		return -1;
	}
	std::vector< VideoCapture > camera(device_counts);
	std::vector< Mat > videoFrame(device_counts);
	std::vector< string > FrameWindow(device_counts);
	for (int i = 0; i<camera.size(); i++)
	{
		camera[i].open(i);

		if (!camera[i].isOpened()) {
			printf("NO camera idx[%d]  \n", camera.size());
			return -1;
		}
		FrameWindow[i] = "winN:" + std::to_string(i);
		namedWindow(FrameWindow[i], CV_WINDOW_AUTOSIZE); 
		camera[i].set(CV_CAP_PROP_SETTINGS, 1);
	}
	/*
	Size videoSize = Size((int)video.get(CV_CAP_PROP_FRAME_WIDTH), (int)video.get(CV_CAP_PROP_FRAME_HEIGHT));
	Mat videoFrame;
	Mat videoFrame2;
	int err;
	err = video.set(CV_CAP_PROP_FRAME_WIDTH, 640);
	printf("err=%d\n", err);
	video.set(CV_CAP_PROP_FRAME_HEIGHT, 480);
	printf("CV_CAP_PROP_EXPOSURE=%f\n", video.get(CV_CAP_PROP_EXPOSURE));
	video.set(CV_CAP_PROP_AUTO_EXPOSURE, 5);
	video.set(CV_CAP_PROP_BRIGHTNESS, 120);
	video.set(CV_CAP_PROP_CONTRAST, 27);

	video.set(CV_CAP_PROP_SETTINGS, 1);*/


	float exp = -3;
	int ccc = 0;
	while (true) {

		bool ifSkipping = false;
		for (int i = 00; i<camera.size(); i++)
		{
			camera[i] >> videoFrame[i];
			if (videoFrame[i].empty())
			{
				ifSkipping = true;
				break;
			}
		}

		if (ifSkipping) {
			continue;
		}
		for (int i = 0; i<videoFrame.size(); i++)
		{
			imshow(FrameWindow[i], videoFrame[i]);
		}
		if (waitKey(1) == 27) break;
	}
	return 0;
}
#include <time.h>

using namespace std;
int imageBlur()
{
	namedWindow("window", CV_WINDOW_AUTOSIZE);
	Mat dst;	
	Mat src = imread("data/test1.bmp", 1);
	system("dir");
	clock_t t = clock();
	for (int i=0;i<100;i++)
	{
		GaussianBlur(src, dst, Size(21, 21), 0, 0);
		imshow("window", dst);
	}
	t = clock() - t;
	printf("%fms \n", ((double)t) / CLOCKS_PER_SEC * 1000);
	printf("W:%d H:%d\n", dst.size().width, dst.size().height);
	imwrite("data/out1.bmp", dst);
	system("pause");
	return 0;
}


int main() {
	int ret;
	ret = imageBlur();
	//ret = webcam();
	return ret;
}