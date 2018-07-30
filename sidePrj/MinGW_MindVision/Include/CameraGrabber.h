#ifndef _MV_CAMERA_GRABBER_H_
#define _MV_CAMERA_GRABBER_H_

#include "CameraDefine.h"
#include "CameraStatus.h"


/******************************************************/
// ������   : CameraGrabber_CreateFromDevicePage
// �������� : ��������б����û�ѡ��Ҫ�򿪵����
// ����     : �������ִ�гɹ����غ���������Grabber
// ����ֵ   : �ɹ�ʱ������CAMERA_STATUS_SUCCESS (0);
//            ���򷵻ط�0ֵ�Ĵ�����,��ο�CameraStatus.h
//            �д�����Ķ��塣
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraGrabber_CreateFromDevicePage(
	void** Grabber
	);

/******************************************************/
// ������   : CameraGrabber_Create
// �������� : ���豸������Ϣ����Grabber
// ����     : Grabber    �������ִ�гɹ����غ���������Grabber����
//			  pDevInfo	��������豸������Ϣ����CameraEnumerateDevice������á� 
// ����ֵ   : �ɹ�ʱ������CAMERA_STATUS_SUCCESS (0);
//            ���򷵻ط�0ֵ�Ĵ�����,��ο�CameraStatus.h
//            �д�����Ķ��塣
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraGrabber_Create(
	void** Grabber,
	tSdkCameraDevInfo* pDevInfo
	);

/******************************************************/
// ������   : CameraGrabber_Destroy
// �������� : ����Grabber
// ����     : Grabber
// ����ֵ   : �ɹ�ʱ������CAMERA_STATUS_SUCCESS (0);
//            ���򷵻ط�0ֵ�Ĵ�����,��ο�CameraStatus.h
//            �д�����Ķ��塣
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraGrabber_Destroy(
	void* Grabber
	);

/******************************************************/
// ������	: CameraGrabber_SetHWnd
// ��������	: ����Ԥ����Ƶ����ʾ����
// ����		: Grabber
//			  hWnd  ���ھ��
// ����ֵ   : �ɹ�ʱ������CAMERA_STATUS_SUCCESS (0);
//            ���򷵻ط�0ֵ�Ĵ�����,��ο�CameraStatus.h
//            �д�����Ķ��塣
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraGrabber_SetHWnd(
	void* Grabber,
	HWND hWnd
	);

/******************************************************/
// ������	: CameraGrabber_StartLive
// ��������	: ����Ԥ��
// ����		: Grabber
// ����ֵ   : �ɹ�ʱ������CAMERA_STATUS_SUCCESS (0);
//            ���򷵻ط�0ֵ�Ĵ�����,��ο�CameraStatus.h
//            �д�����Ķ��塣
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraGrabber_StartLive(
	void* Grabber
	);

/******************************************************/
// ������	: CameraGrabber_StopLive
// ��������	: ֹͣԤ��
// ����		: Grabber
// ����ֵ   : �ɹ�ʱ������CAMERA_STATUS_SUCCESS (0);
//            ���򷵻ط�0ֵ�Ĵ�����,��ο�CameraStatus.h
//            �д�����Ķ��塣
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraGrabber_StopLive(
	void* Grabber
	);

/******************************************************/
// ������	: CameraGrabber_SaveImage
// ��������	: ץͼ
// ����		: Grabber
//			  Image ����ץȡ����ͼ����Ҫ����CameraImage_Destroy�ͷţ�
//			  TimeOut ��ʱʱ�䣨���룩
// ����ֵ   : �ɹ�ʱ������CAMERA_STATUS_SUCCESS (0);
//            ���򷵻ط�0ֵ�Ĵ�����,��ο�CameraStatus.h
//            �д�����Ķ��塣
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraGrabber_SaveImage(
	void* Grabber,
	void** Image,
	DWORD TimeOut
	);

/******************************************************/
// ������	: CameraGrabber_SaveImageAsync
// ��������	: �ύһ���첽��ץͼ�����ύ�ɹ����ץͼ��ɻ�ص��û����õ���ɺ���
// ����		: Grabber
// ����ֵ   : �ɹ�ʱ������CAMERA_STATUS_SUCCESS (0);
//            ���򷵻ط�0ֵ�Ĵ�����,��ο�CameraStatus.h
//            �д�����Ķ��塣
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraGrabber_SaveImageAsync(
	void* Grabber
	);

/******************************************************/
// ������	: CameraGrabber_SaveImageAsyncEx
// ��������	: �ύһ���첽��ץͼ�����ύ�ɹ����ץͼ��ɻ�ص��û����õ���ɺ���
// ����		: Grabber
//			  UserData ��ʹ��CameraImage_GetUserData��Image��ȡ��ֵ
// ����ֵ   : �ɹ�ʱ������CAMERA_STATUS_SUCCESS (0);
//            ���򷵻ط�0ֵ�Ĵ�����,��ο�CameraStatus.h
//            �д�����Ķ��塣
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraGrabber_SaveImageAsyncEx(
	void* Grabber,
	void* UserData
	);

/******************************************************/
// ������	: CameraGrabber_SetSaveImageCompleteCallback
// ��������	: �����첽��ʽץͼ����ɺ���
// ����		: Grabber
//			  Callback ����ץͼ�������ʱ������
//			  Context ��Callback������ʱ����Ϊ��������Callback
// ����ֵ   : �ɹ�ʱ������CAMERA_STATUS_SUCCESS (0);
//            ���򷵻ط�0ֵ�Ĵ�����,��ο�CameraStatus.h
//            �д�����Ķ��塣
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraGrabber_SetSaveImageCompleteCallback(
	void* Grabber,
	pfnCameraGrabberSaveImageComplete Callback,
	void* Context
	);

/******************************************************/
// ������	: CameraGrabber_SetFrameListener
// ��������	: ����֡��������
// ����		: Grabber
//			  Listener �����������˺�������0��ʾ������ǰ֡
//			  Context ��Listener������ʱ����Ϊ��������Listener
// ����ֵ   : �ɹ�ʱ������CAMERA_STATUS_SUCCESS (0);
//            ���򷵻ط�0ֵ�Ĵ�����,��ο�CameraStatus.h
//            �д�����Ķ��塣
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraGrabber_SetFrameListener(
	void* Grabber,
	pfnCameraGrabberFrameListener Listener,
	void* Context
	);

/******************************************************/
// ������	: CameraGrabber_SetRawCallback
// ��������	: ����RAW�ص�����
// ����		: Grabber
//			  Callback Raw�ص�����
//			  Context ��Callback������ʱ����Ϊ��������Callback
// ����ֵ   : �ɹ�ʱ������CAMERA_STATUS_SUCCESS (0);
//            ���򷵻ط�0ֵ�Ĵ�����,��ο�CameraStatus.h
//            �д�����Ķ��塣
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraGrabber_SetRawCallback(
	void* Grabber,
	pfnCameraGrabberFrameCallback Callback,
	void* Context
	);

/******************************************************/
// ������	: CameraGrabber_SetRGBCallback
// ��������	: ����RGB�ص�����
// ����		: Grabber
//			  Callback RGB�ص�����
//			  Context ��Callback������ʱ����Ϊ��������Callback
// ����ֵ   : �ɹ�ʱ������CAMERA_STATUS_SUCCESS (0);
//            ���򷵻ط�0ֵ�Ĵ�����,��ο�CameraStatus.h
//            �д�����Ķ��塣
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraGrabber_SetRGBCallback(
	void* Grabber,
	pfnCameraGrabberFrameCallback Callback,
	void* Context
	);

/******************************************************/
// ������	: CameraGrabber_GetCameraHandle
// ��������	: ��ȡ������
// ����		: Grabber
//			  hCamera ���ص�������
// ����ֵ   : �ɹ�ʱ������CAMERA_STATUS_SUCCESS (0);
//            ���򷵻ط�0ֵ�Ĵ�����,��ο�CameraStatus.h
//            �д�����Ķ��塣
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraGrabber_GetCameraHandle(
	void* Grabber,
	CameraHandle *hCamera
	);

/******************************************************/
// ������	: CameraGrabber_GetStat
// ��������	: ��ȡ֡ͳ����Ϣ
// ����		: Grabber
//			  stat ���ص�ͳ����Ϣ
// ����ֵ   : �ɹ�ʱ������CAMERA_STATUS_SUCCESS (0);
//            ���򷵻ط�0ֵ�Ĵ�����,��ο�CameraStatus.h
//            �д�����Ķ��塣
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraGrabber_GetStat(
	void* Grabber,
	tSdkGrabberStat *stat
	);

/******************************************************/
// ������	: CameraGrabber_GetCameraDevInfo
// ��������	: ��ȡ���DevInfo
// ����		: Grabber
//			  DevInfo ���ص����DevInfo
// ����ֵ   : �ɹ�ʱ������CAMERA_STATUS_SUCCESS (0);
//            ���򷵻ط�0ֵ�Ĵ�����,��ο�CameraStatus.h
//            �д�����Ķ��塣
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraGrabber_GetCameraDevInfo(
	void* Grabber,
	tSdkCameraDevInfo *DevInfo
	);




#endif // _MV_CAMERA_GRABBER_H_
