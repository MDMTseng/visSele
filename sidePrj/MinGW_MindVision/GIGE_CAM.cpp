// ConsoleApplication1.cpp : Defines the entry point for the console application.
//

#include "stdio.h"
#include <windows.h>
#include "CameraApi.h"
#include <assert.h>

BOOL InitCamera();
BOOL DDD();
BOOL UnInitCamera();

int main()
{
	InitCamera();

	for (int i=0;i<100;i++)
	{
		DDD();
	}
	UnInitCamera();
	system("pause");
    return 0;
}

CameraHandle    m_hCamera;	//������豸���|the handle of the camera we use
BYTE*           m_pFrameBuffer=NULL;


BOOL InitCamera()
{
	tSdkCameraDevInfo sCameraList[10];
	INT iCameraNums;
	CameraSdkStatus status;
	tSdkCameraCapbility sCameraInfo;

	iCameraNums = 10;
	if (CameraEnumerateDevice(sCameraList, &iCameraNums) != CAMERA_STATUS_SUCCESS || iCameraNums == 0)
	{
		printf("No camera was found!");
		return FALSE;
	}

	for (int i=0; i< iCameraNums;i++)
	{
		printf("CAM:%d======\n", i);
		printf("acDriverVersion:%s\n", sCameraList[i].acDriverVersion);
		printf("acFriendlyName:%s\n", sCameraList[i].acFriendlyName);
		printf("acLinkName:%s\n", sCameraList[i].acLinkName);
		printf("acPortType:%s\n", sCameraList[i].acPortType);
		printf("acProductName:%s\n", sCameraList[i].acProductName);
		printf("acProductSeries:%s\n", sCameraList[i].acProductSeries);
		printf("acSensorType:%s\n", sCameraList[i].acSensorType);
		printf("acSn:%s\n", sCameraList[i].acSn);
		printf("\n\n\n\n");
	}


	if ((status = CameraInit(&sCameraList[0], -1, -1, &m_hCamera)) != CAMERA_STATUS_SUCCESS)
	{
		printf("Failed to init the camera! Error code is %d(%s)", status, CameraGetErrorString(status));

		return FALSE;
	}


	//Get properties description for this camera.
	CameraGetCapability(m_hCamera, &sCameraInfo);

	m_pFrameBuffer = (BYTE *)CameraAlignMalloc(sCameraInfo.sResolutionRange.iWidthMax*sCameraInfo.sResolutionRange.iHeightMax * 4, 16);


	CameraPlay(m_hCamera);
	return TRUE;
}


/**/
void printImgAscii(unsigned char *img,int width,int height, int printwidth)
{
	int step = width / printwidth;
	if (step < 1)
		step = 1;
	char grayAscii[] = "@%#x+=:-.. ";
	for (int i = 0; i < height; i += step)
	{
		unsigned char *ImLine = img+ width*3*i;
		for (int j = 0; j <width; j += step)
		{
			int tmp = (255 - ImLine[3 * j]) * (sizeof(grayAscii) - 2) / 255;
			char c = grayAscii[tmp];
			//printf("%02x ",ImLine[3*j+2]);
			putchar(c);
		}
		putchar('\n');
	}
}


BOOL DDD()
{
	tSdkFrameHead 	sFrameInfo;
	BYTE*			pbyBuffer;
	if (CameraGetImageBuffer(m_hCamera, &sFrameInfo, &pbyBuffer, 10000) == CAMERA_STATUS_SUCCESS)
	{
		CameraSdkStatus status =
			CameraImageProcess(m_hCamera, pbyBuffer, m_pFrameBuffer, &sFrameInfo);//����ģʽ

		printf("uiTimeStamp:%d\n", sFrameInfo.uiTimeStamp);
		printf("fAnalogGain:%f\n", sFrameInfo.fAnalogGain);
		printf("iContrast:%d\n", sFrameInfo.iContrast);
		printf("iGamma:%d\n", sFrameInfo.iGamma);
		printf("iHeight:%d\n", sFrameInfo.iHeight);
		printf("iWidth:%d\n", sFrameInfo.iWidth);

		printf("get image Buffer\n");

		CameraReleaseImageBuffer(m_hCamera, pbyBuffer);
		printImgAscii(m_pFrameBuffer, sFrameInfo.iWidth, sFrameInfo.iHeight, 10);
	}
	else
	{
		printf("Cannot get image Buffer\n");
	}
	return FALSE;
}


BOOL UnInitCamera()
{
	CameraPause(m_hCamera);
	if (m_pFrameBuffer)
	{
		CameraAlignFree(m_pFrameBuffer);
		m_pFrameBuffer = NULL;
	}

	CameraUnInit(m_hCamera);

	return TRUE;
}
