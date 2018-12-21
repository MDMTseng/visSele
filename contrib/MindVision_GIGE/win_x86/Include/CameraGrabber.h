#ifndef _MV_CAMERA_GRABBER_H_
#define _MV_CAMERA_GRABBER_H_

#include "CameraDefine.h"
#include "CameraStatus.h"


/******************************************************/
// function name: CameraGrabber_CreateFromDevicePage
// Function Description: Pop-up camera list allows the user to select the camera to be opened
// Parameters: Grabber created by the function if the function succeeded
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraGrabber_CreateFromDevicePage(
	void** Grabber
	);

/******************************************************/
// function name: CameraGrabber_CreateByIndex
// Function Description: Create Grabber using camera list index
// Parameters: Grabber Grabber created by the function if function succeeded
// Index camera index
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraGrabber_CreateByIndex(
	void** Grabber,
	int Index
	);

/******************************************************/
// function name: CameraGrabber_CreateByName
// Feature Description: Create Grabber with camera name
// Parameters: Grabber Grabber created by the function if function succeeded
// Name camera name
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraGrabber_CreateByName(
	void** Grabber,
	char* Name
	);

/******************************************************/
// function name: CameraGrabber_Create
// Function Description: Create Grabber from device description
// Arguments: Grabber Returns the Grabber object created by the function if the function succeeds
// pDevInfo The device description of the camera, obtained by the CameraEnumerateDevice function.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraGrabber_Create(
	void** Grabber,
	tSdkCameraDevInfo* pDevInfo
	);

/******************************************************/
// function name: CameraGrabber_Destroy
// Function Description: Destroy Grabber
// parameter: Grabber
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraGrabber_Destroy(
	void* Grabber
	);

/******************************************************/
// function name: CameraGrabber_SetHWnd
// Function Description: Set the preview video display window
// parameter: Grabber
// hWnd window handle
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraGrabber_SetHWnd(
	void* Grabber,
	HWND hWnd
	);

/******************************************************/
// function name: CameraGrabber_SetPriority
// Function Description: Set the priority of drawing
// parameter: Grabber
// Priority Priority map see: emCameraGetImagePriority
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraGrabber_SetPriority(
	void* Grabber,
	UINT Priority
	);

/******************************************************/
// function name: CameraGrabber_StartLive
// Function Description: Start the preview
// parameter: Grabber
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraGrabber_StartLive(
	void* Grabber
	);

/******************************************************/
// function name: CameraGrabber_StopLive
// Function Description: Stop the preview
// parameter: Grabber
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraGrabber_StopLive(
	void* Grabber
	);

/******************************************************/
// function name: CameraGrabber_SaveImage
// Function Description: capture
// parameter: Grabber
// Image returns the captured image (need to call CameraImage_Destroy release)
// TimeOut timeout (milliseconds)
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraGrabber_SaveImage(
	void* Grabber,
	void** Image,
	DWORD TimeOut
	);

/******************************************************/
// function name: CameraGrabber_SaveImageAsync
// Function Description: Submit an asynchronous screenshot request, submit the successful completion of the capture will be completed callback function set by the completion of the function
// parameter: Grabber
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraGrabber_SaveImageAsync(
	void* Grabber
	);

/******************************************************/
// function name: CameraGrabber_SaveImageAsyncEx
// Function Description: Submit an asynchronous screenshot request, submit the successful completion of the capture will be completed callback function set by the completion of the function
// parameter: Grabber
// UserData You can get this value from Image using CameraImage_GetUserData
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraGrabber_SaveImageAsyncEx(
	void* Grabber,
	void* UserData
	);

/******************************************************/
// function name: CameraGrabber_SetSaveImageCompleteCallback
// Function Description: Set the asynchronous function to complete the capture
// parameter: Grabber
// Callback is called when a capture task is completed
// Context When Callback is called, it is passed as a parameter to the Callback
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraGrabber_SetSaveImageCompleteCallback(
	void* Grabber,
	pfnCameraGrabberSaveImageComplete Callback,
	void* Context
	);

/******************************************************/
// function name: CameraGrabber_SetFrameListener
// Function Description: Set the frame monitoring function
// parameter: Grabber
// Listener monitor function, this function returns 0 means to discard the current frame
// Context Listener is passed as a parameter when Listener is called
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraGrabber_SetFrameListener(
	void* Grabber,
	pfnCameraGrabberFrameListener Listener,
	void* Context
	);

/******************************************************/
// function name: CameraGrabber_SetRawCallback
// Function Description: Set the RAW callback function
// parameter: Grabber
// Callback Raw callback function
// Context When Callback is called, it is passed as a parameter to the Callback
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraGrabber_SetRawCallback(
	void* Grabber,
	pfnCameraGrabberFrameCallback Callback,
	void* Context
	);

/******************************************************/
// function name: CameraGrabber_SetRGBCallback
// Function Description: Set RGB callback function
// parameter: Grabber
// Callback RGB callback function
// Context When Callback is called, it is passed as a parameter to the Callback
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraGrabber_SetRGBCallback(
	void* Grabber,
	pfnCameraGrabberFrameCallback Callback,
	void* Context
	);

/******************************************************/
// function name: CameraGrabber_GetCameraHandle
// Function Description: Get the camera handle
// parameter: Grabber
// Camera handle returned by hCamera
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraGrabber_GetCameraHandle(
	void* Grabber,
	CameraHandle *hCamera
	);

/******************************************************/
// function name: CameraGrabber_GetStat
// Function Description: Get frame statistics
// parameter: Grabber
// statistics returned by stat
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraGrabber_GetStat(
	void* Grabber,
	tSdkGrabberStat *stat
	);

/******************************************************/
// function name: CameraGrabber_GetCameraDevInfo
// Function Description: Get the camera DevInfo
// parameter: Grabber
// Camera DevInfo returned by DevInfo
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraGrabber_GetCameraDevInfo(
	void* Grabber,
	tSdkCameraDevInfo *DevInfo
	);




#endif // _MV_CAMERA_GRABBER_H_
