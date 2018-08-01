#ifndef _MV_CAMERA_IMAGE_H_
#define _MV_CAMERA_IMAGE_H_

#include "CameraDefine.h"
#include "CameraStatus.h"


/**********************************************************/
// function name: CameraImage_Create
// Function Description: Create a new Image
// parameter: Image
// pFrameBuffer frame data buffer
// pFrameHead frame header
// bCopy TRUE: copy a new frame of data FALSE: do not copy directly to the buffer pointed to by pFrameBuffer
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/**********************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraImage_Create(
	void** Image,
	BYTE *pFrameBuffer, 
	tSdkFrameHead* pFrameHead,
	BOOL bCopy
	);

/******************************************************/
// function name: CameraImage_CreateEmpty
// Function Description: Create an empty Image
// parameter: Image
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraImage_CreateEmpty(
	void** Image
	);

/******************************************************/
// function name: CameraImage_Destroy
// Function Description: Destroy Image
// parameter: Image
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraImage_Destroy(
	void* Image
	);

/******************************************************/
// function name: CameraImage_GetData
// Function Description: Get Image data
// parameter: Image
// DataBuffer image data
// Head image information
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraImage_GetData(
	void* Image,
	BYTE** DataBuffer,
	tSdkFrameHead** Head
	);

/******************************************************/
// function name: CameraImage_GetUserData
// Function Description: Get Image user-defined data
// parameter: Image
// UserData returns user-defined data
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraImage_GetUserData(
	void* Image,
	void** UserData
	);

/******************************************************/
// function name: CameraImage_SetUserData
// Function Description: Set Image's user-defined data
// parameter: Image
// UserData user-defined data
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraImage_SetUserData(
	void* Image,
	void* UserData
	);

/******************************************************/
// function name: CameraImage_IsEmpty
// Function Description: to determine whether an Image is empty
// parameter: Image
// IsEmpty is empty Returns: TRUE (1) otherwise returns: FALSE (0)
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraImage_IsEmpty(
	void* Image,
	BOOL* IsEmpty
	);

/******************************************************/
// function name: CameraImage_Draw
// Function Description: Draw Image to the specified window
// parameter: Image
// hWnd destination window
// Algorithm scaling algorithm 0: fast but less quality 1: slow but good quality
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraImage_Draw(
	void* Image,
	HWND hWnd,
	int Algorithm
	);

/******************************************************/
// function name: CameraImage_DrawFit
// Function Description: Pull to draw Image to the specified window
// parameter: Image
// hWnd destination window
// Algorithm scaling algorithm 0: fast but less quality 1: slow but good quality
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraImage_DrawFit(
	void* Image,
	HWND hWnd,
	int Algorithm
	);

/******************************************************/
// function name: CameraImage_DrawToDC
// Function Description: Draw Image to specified DC
// parameter: Image
// hDC destination DC
// Algorithm scaling algorithm 0: fast but less quality 1: slow but good quality
// xDst, yDst: the coordinates of the upper left corner of the target rectangle
// cxDst, cyDst: The width and height of the target rectangle
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
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
// function name: CameraImage_DrawToDCFit
// Function Description: Pull up and draw Image to the specified DC
// parameter: Image
// hDC destination DC
// Algorithm scaling algorithm 0: fast but less quality 1: slow but good quality
// xDst, yDst: the coordinates of the upper left corner of the target rectangle
// cxDst, cyDst: The width and height of the target rectangle
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
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
// function name: CameraImage_BitBlt
// Function Description: Draw Image to the specified window (no zoom)
// parameter: Image
// hWnd destination window
// xDst, yDst: the coordinates of the upper left corner of the target rectangle
// cxDst, cyDst: The width and height of the target rectangle
// xSrc, ySrc: the coordinates of the upper left corner of the image rectangle
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
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
// function name: CameraImage_BitBltToDC
// Function Description: Draw Image to specified DC (no zoom)
// parameter: Image
// hDC destination DC
// xDst, yDst: the coordinates of the upper left corner of the target rectangle
// cxDst, cyDst: The width and height of the target rectangle
// xSrc, ySrc: the coordinates of the upper left corner of the image rectangle
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
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
// function name: CameraImage_SaveAsBmp
// Function Description: Save Image in bmp format
// parameter: Image
// FileName file name
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraImage_SaveAsBmp(
	void* Image,
	char const* FileName
	);

/******************************************************/
// function name: CameraImage_SaveAsJpeg
// Function Description: Save Image in jpg format
// parameter: Image
// FileName file name
// Quality saves the quality (1-100), 100 is the best quality but the largest file
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraImage_SaveAsJpeg(
	void* Image,
	char const* FileName,
	BYTE  Quality
	);

/******************************************************/
// function name: CameraImage_SaveAsPng
// Function Description: Save Image in png format
// parameter: Image
// FileName file name
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraImage_SaveAsPng(
	void* Image,
	char const* FileName
	);

/******************************************************/
// function name: CameraImage_SaveAsRaw
// Function Description: Save the raw Image
// parameter: Image
// FileName file name
// Format 0: 8Bit Raw 1: 16Bit Raw
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraImage_SaveAsRaw(
	void* Image,
	char const* FileName,
	int Format
	);

/******************************************************/
// function name: CameraImage_IPicture
// Function Description: Create an IPicture from Image
// parameter: Image
// Picture The newly created IPicture
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraImage_IPicture(
	void* Image,
	IPicture** NewPic
	);




#endif // _MV_CAMERA_IMAGE_H_
