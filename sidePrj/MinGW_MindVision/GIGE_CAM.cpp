// ConsoleApplication1.cpp : Defines the entry point for the console application.
//

#include "stdio.h"
#include <windows.h>
#include "CameraApi.h"
#include <assert.h>

BOOL InitCamera();
BOOL FetchImage();
BOOL UnInitCamera();
void printImgAscii(unsigned char *img,int width,int height, int printwidth);


CameraHandle    m_hCamera;	//������豸���|the handle of the camera we use
BYTE*           m_pFrameBuffer=NULL;
BYTE*           extFrameBuffer=NULL;
int main()
{
	InitCamera();

	for (int i=0;i<100;i++)
	{
		//FetchImage(); //Directly get image from camera
		Sleep(200);
		CameraSoftTrigger(m_hCamera);//SoftTrigger
	}
	UnInitCamera();
	system("pause");
    return 0;
}

void ImgDecodeAndRun(CameraHandle hCamera, BYTE *frameBuffer, tSdkFrameHead* frameInfo,PVOID pContext)
{
		static int cccccc=0;
		//printf("%s:\n",__func__);
		CameraSdkStatus status =
		CameraImageProcess(hCamera, frameBuffer, extFrameBuffer, frameInfo);
		if(cccccc++%1==0)
		{
			if(1)
			{
				printf("uiTimeStamp:%d\n", frameInfo->uiTimeStamp);
				printf("fAnalogGain:%f\n", frameInfo->fAnalogGain);
				printf("iContrast:%d\n", frameInfo->iContrast);
				printf("iGamma:%d\n", frameInfo->iGamma);
				printf("iHeight:%d\n", frameInfo->iHeight);
				printf("iWidth:%d\n", frameInfo->iWidth);
				printf("get image Buffer\n");
			}
			//memcpy(extFrameBuffer,m_pFrameBuffer,frameInfo->iWidth*frameInfo->iHeight*3);
			printImgAscii(extFrameBuffer, frameInfo->iWidth, frameInfo->iHeight, 10);
		}
}

//It will be called when it's triggered or when there is a new image(in continuous mode)
void _stdcall  GrabImageCallback(CameraHandle hCamera, BYTE *pFrameBuffer, tSdkFrameHead* pFrameHead,PVOID pContext)
{
	ImgDecodeAndRun(hCamera,pFrameBuffer,pFrameHead,pContext);
	/*CameraSdkStatus status;
	CBasicDlg *pThis = (CBasicDlg*)pContext;
	status = CameraImageProcess(pThis->m_hCamera, pFrameBuffer, pThis->m_pFrameBuffer,pFrameHead);
	if (pThis->m_sFrInfo.iWidth != pFrameHead->iWidth || pThis->m_sFrInfo.iHeight != pFrameHead->iHeight)
	{
		pThis->m_sFrInfo.iWidth = pFrameHead->iWidth;
		pThis->m_sFrInfo.iHeight = pFrameHead->iHeight;
		pThis->InvalidateRect(NULL);
	}*/
}

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

	{
		CameraSetTriggerMode(m_hCamera,1);
		//0 for continuous, 1 for soft trigger, 2 for HW trigger
		CameraSetTriggerCount(m_hCamera,1);
		CameraSetCallbackFunction(m_hCamera,GrabImageCallback,(PVOID)NULL,NULL);
	}
	//Get properties description for this camera.
	CameraGetCapability(m_hCamera, &sCameraInfo);
	printf(">>W:%d H:%d>>\n",
		sCameraInfo.sResolutionRange.iWidthMax,
		sCameraInfo.sResolutionRange.iHeightMax
	);

	m_pFrameBuffer = (BYTE *)CameraAlignMalloc(sCameraInfo.sResolutionRange.iWidthMax*sCameraInfo.sResolutionRange.iHeightMax * 3, 16);
	extFrameBuffer  = (BYTE *)CameraAlignMalloc(sCameraInfo.sResolutionRange.iWidthMax*sCameraInfo.sResolutionRange.iHeightMax * 3, 16);


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


BOOL FetchImage()
{
	tSdkFrameHead 	sFrameInfo;
	BYTE*			pbyBuffer;
	if (CameraGetImageBuffer(m_hCamera, &sFrameInfo, &pbyBuffer, 10000) == CAMERA_STATUS_SUCCESS)
	{

		ImgDecodeAndRun(m_hCamera,pbyBuffer,&sFrameInfo,NULL);
		CameraReleaseImageBuffer(m_hCamera, pbyBuffer);
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
