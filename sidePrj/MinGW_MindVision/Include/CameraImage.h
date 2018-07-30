#ifndef _MV_CAMERA_IMAGE_H_
#define _MV_CAMERA_IMAGE_H_

#include "CameraDefine.h"
#include "CameraStatus.h"


/******************************************************/
// ������	: CameraImage_Create
// ��������	: ����һ���µ�Image
// ����		: Image
//			  pFrameBuffer ֡���ݻ�����
//			  pFrameHead ֡ͷ
//			  bCopy TRUE: ���Ƴ�һ���µ�֡����  FALSE: �����ƣ�ֱ��ʹ��pFrameBufferָ��Ļ�����
// ����ֵ   : �ɹ�ʱ������CAMERA_STATUS_SUCCESS (0);
//            ���򷵻ط�0ֵ�Ĵ�����,��ο�CameraStatus.h
//            �д�����Ķ��塣
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraImage_Create(
	void** Image,
	BYTE *pFrameBuffer, 
	tSdkFrameHead* pFrameHead,
	BOOL bCopy
	);

/******************************************************/
// ������	: CameraImage_CreateEmpty
// ��������	: ����һ���յ�Image
// ����		: Image
// ����ֵ   : �ɹ�ʱ������CAMERA_STATUS_SUCCESS (0);
//            ���򷵻ط�0ֵ�Ĵ�����,��ο�CameraStatus.h
//            �д�����Ķ��塣
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraImage_CreateEmpty(
	void** Image
	);

/******************************************************/
// ������	: CameraImage_Destroy
// ��������	: ����Image
// ����		: Image
// ����ֵ   : �ɹ�ʱ������CAMERA_STATUS_SUCCESS (0);
//            ���򷵻ط�0ֵ�Ĵ�����,��ο�CameraStatus.h
//            �д�����Ķ��塣
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraImage_Destroy(
	void* Image
	);

/******************************************************/
// ������	: CameraImage_GetData
// ��������	: ��ȡImage����
// ����		: Image
//			  DataBuffer ͼ������
//			  Head ͼ����Ϣ
// ����ֵ   : �ɹ�ʱ������CAMERA_STATUS_SUCCESS (0);
//            ���򷵻ط�0ֵ�Ĵ�����,��ο�CameraStatus.h
//            �д�����Ķ��塣
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraImage_GetData(
	void* Image,
	BYTE** DataBuffer,
	tSdkFrameHead** Head
	);

/******************************************************/
// ������	: CameraImage_GetUserData
// ��������	: ��ȡImage���û��Զ�������
// ����		: Image
//			  UserData �����û��Զ�������
// ����ֵ   : �ɹ�ʱ������CAMERA_STATUS_SUCCESS (0);
//            ���򷵻ط�0ֵ�Ĵ�����,��ο�CameraStatus.h
//            �д�����Ķ��塣
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraImage_GetUserData(
	void* Image,
	void** UserData
	);

/******************************************************/
// ������	: CameraImage_SetUserData
// ��������	: ����Image���û��Զ�������
// ����		: Image
//			  UserData �û��Զ�������
// ����ֵ   : �ɹ�ʱ������CAMERA_STATUS_SUCCESS (0);
//            ���򷵻ط�0ֵ�Ĵ�����,��ο�CameraStatus.h
//            �д�����Ķ��塣
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraImage_SetUserData(
	void* Image,
	void* UserData
	);

/******************************************************/
// ������	: CameraImage_IsEmpty
// ��������	: �ж�һ��Image�Ƿ�Ϊ��
// ����		: Image
//			  IsEmpty Ϊ�շ���:TRUE(1)  ���򷵻�:FALSE(0)
// ����ֵ   : �ɹ�ʱ������CAMERA_STATUS_SUCCESS (0);
//            ���򷵻ط�0ֵ�Ĵ�����,��ο�CameraStatus.h
//            �д�����Ķ��塣
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraImage_IsEmpty(
	void* Image,
	BOOL* IsEmpty
	);

/******************************************************/
// ������	: CameraImage_Draw
// ��������	: ����Image��ָ������
// ����		: Image
//			  hWnd Ŀ�Ĵ���
//			  Algorithm �����㷨  0�����ٵ������Բ�  1���ٶ�����������
// ����ֵ   : �ɹ�ʱ������CAMERA_STATUS_SUCCESS (0);
//            ���򷵻ط�0ֵ�Ĵ�����,��ο�CameraStatus.h
//            �д�����Ķ��塣
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraImage_Draw(
	void* Image,
	HWND hWnd,
	int Algorithm
	);

/******************************************************/
// ������	: CameraImage_DrawFit
// ��������	: ��������Image��ָ������
// ����		: Image
//			  hWnd Ŀ�Ĵ���
//			  Algorithm �����㷨  0�����ٵ������Բ�  1���ٶ�����������
// ����ֵ   : �ɹ�ʱ������CAMERA_STATUS_SUCCESS (0);
//            ���򷵻ط�0ֵ�Ĵ�����,��ο�CameraStatus.h
//            �д�����Ķ��塣
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraImage_DrawFit(
	void* Image,
	HWND hWnd,
	int Algorithm
	);

/******************************************************/
// ������	: CameraImage_DrawToDC
// ��������	: ����Image��ָ��DC
// ����		: Image
//			  hDC Ŀ��DC
//			  Algorithm �����㷨  0�����ٵ������Բ�  1���ٶ�����������
//			  xDst,yDst: Ŀ����ε����Ͻ�����
//			  cxDst,cyDst: Ŀ����εĿ��
// ����ֵ   : �ɹ�ʱ������CAMERA_STATUS_SUCCESS (0);
//            ���򷵻ط�0ֵ�Ĵ�����,��ο�CameraStatus.h
//            �д�����Ķ��塣
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraImage_DrawToDC(
	void* Image,
	HDC hDC,
	int Algorithm,
	int xDst,
	int yDst,
	int cxDst,
	int cyDst
	);

/******************************************************/
// ������	: CameraImage_DrawToDCFit
// ��������	: ��������Image��ָ��DC
// ����		: Image
//			  hDC Ŀ��DC
//			  Algorithm �����㷨  0�����ٵ������Բ�  1���ٶ�����������
//			  xDst,yDst: Ŀ����ε����Ͻ�����
//			  cxDst,cyDst: Ŀ����εĿ��
// ����ֵ   : �ɹ�ʱ������CAMERA_STATUS_SUCCESS (0);
//            ���򷵻ط�0ֵ�Ĵ�����,��ο�CameraStatus.h
//            �д�����Ķ��塣
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraImage_DrawToDCFit(
	void* Image,
	HDC hDC,
	int Algorithm,
	int xDst,
	int yDst,
	int cxDst,
	int cyDst
	);

/******************************************************/
// ������	: CameraImage_BitBlt
// ��������	: ����Image��ָ�����ڣ������ţ�
// ����		: Image
//			  hWnd Ŀ�Ĵ���
//			  xDst,yDst: Ŀ����ε����Ͻ�����
//			  cxDst,cyDst: Ŀ����εĿ��
//			  xSrc,ySrc: ͼ����ε����Ͻ�����
// ����ֵ   : �ɹ�ʱ������CAMERA_STATUS_SUCCESS (0);
//            ���򷵻ط�0ֵ�Ĵ�����,��ο�CameraStatus.h
//            �д�����Ķ��塣
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraImage_BitBlt(
	void* Image,
	HWND hWnd,
	int xDst,
	int yDst,
	int cxDst,
	int cyDst,
	int xSrc,
	int ySrc
	);

/******************************************************/
// ������	: CameraImage_BitBltToDC
// ��������	: ����Image��ָ��DC�������ţ�
// ����		: Image
//			  hDC Ŀ��DC
//			  xDst,yDst: Ŀ����ε����Ͻ�����
//			  cxDst,cyDst: Ŀ����εĿ��
//			  xSrc,ySrc: ͼ����ε����Ͻ�����
// ����ֵ   : �ɹ�ʱ������CAMERA_STATUS_SUCCESS (0);
//            ���򷵻ط�0ֵ�Ĵ�����,��ο�CameraStatus.h
//            �д�����Ķ��塣
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraImage_BitBltToDC(
	void* Image,
	HDC hDC,
	int xDst,
	int yDst,
	int cxDst,
	int cyDst,
	int xSrc,
	int ySrc
	);

/******************************************************/
// ������	: CameraImage_SaveAsBmp
// ��������	: ��bmp��ʽ����Image
// ����		: Image
//			  FileName �ļ���
// ����ֵ   : �ɹ�ʱ������CAMERA_STATUS_SUCCESS (0);
//            ���򷵻ط�0ֵ�Ĵ�����,��ο�CameraStatus.h
//            �д�����Ķ��塣
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraImage_SaveAsBmp(
	void* Image,
	char const* FileName
	);

/******************************************************/
// ������	: CameraImage_SaveAsJpeg
// ��������	: ��jpg��ʽ����Image
// ����		: Image
//			  FileName �ļ���
//			  Quality ��������(1-100)��100Ϊ������ѵ��ļ�Ҳ���
// ����ֵ   : �ɹ�ʱ������CAMERA_STATUS_SUCCESS (0);
//            ���򷵻ط�0ֵ�Ĵ�����,��ο�CameraStatus.h
//            �д�����Ķ��塣
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraImage_SaveAsJpeg(
	void* Image,
	char const* FileName,
	BYTE  Quality
	);

/******************************************************/
// ������	: CameraImage_SaveAsPng
// ��������	: ��png��ʽ����Image
// ����		: Image
//			  FileName �ļ���
// ����ֵ   : �ɹ�ʱ������CAMERA_STATUS_SUCCESS (0);
//            ���򷵻ط�0ֵ�Ĵ�����,��ο�CameraStatus.h
//            �д�����Ķ��塣
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraImage_SaveAsPng(
	void* Image,
	char const* FileName
	);

/******************************************************/
// ������	: CameraImage_SaveAsRaw
// ��������	: ����raw Image
// ����		: Image
//			  FileName �ļ���
//			  Format 0: 8Bit Raw     1: 16Bit Raw
// ����ֵ   : �ɹ�ʱ������CAMERA_STATUS_SUCCESS (0);
//            ���򷵻ط�0ֵ�Ĵ�����,��ο�CameraStatus.h
//            �д�����Ķ��塣
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraImage_SaveAsRaw(
	void* Image,
	char const* FileName,
	int Format
	);

/******************************************************/
// ������	: CameraImage_IPicture
// ��������	: ��Image����һ��IPicture
// ����		: Image
//			  Picture �´�����IPicture
// ����ֵ   : �ɹ�ʱ������CAMERA_STATUS_SUCCESS (0);
//            ���򷵻ط�0ֵ�Ĵ�����,��ο�CameraStatus.h
//            �д�����Ķ��塣
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraImage_IPicture(
	void* Image,
	IPicture** NewPic
	);




#endif // _MV_CAMERA_IMAGE_H_
