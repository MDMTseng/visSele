#ifndef _MV_CAM_API
#define _MV_CAM_API

#include "CameraDefine.h"
#include "CameraStatus.h"

#ifndef MAX_PATH
#define MAX_PATH 256
#endif

//BIG5 TRANS ALLOWED


/***************************************************** *****/
// function name: CameraSdkInit
// Function Description: Camera SDK initialization, before calling any other SDK interface, must be
// First call the interface for initialization. This function runs throughout the process
// Only need to be called once.
// parameters: iLanguageSel used to select the language and interface of the SDK internal tips,
// 0: English, 1: Chinese.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraSdkInit)(
	int     iLanguageSel
	);

/***************************************************** *****/
// function name: CameraEnumerateDevice
// Function Description: Enumerate the device and create a device list. Call CameraInit
// Before, you must call this function to get the device's information.
// Parameters: pCameraList Device list array pointer.
// The number of piNums device pointers passed to pCameraList when called
// The number of elements of the array, the function returns, save the actual number of devices found.
// Note that the value pointed to by piNums must be initialized and does not exceed the number of pCameraList array elements,
// Otherwise it may cause memory overflow.
// Special Note: CameraEnumerateDevice camera information list will be sorted according to acFriendlyName,
// For example, you could change the two cameras to "Camera1" and "Camera2" respectively,
// CameraEnumerateDevice returns a list of cameras named "Camera1" in front of the camera row named "Camera2"
// This method is relatively simple and effective, the code can be initialized in order, without special binding.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraEnumerateDevice)(
	tSdkCameraDevInfo* pCameraList, 
	INT*               piNums
	);

/***************************************************** *****/
// function name: CameraEnumerateDeviceEx
// Function Description: Enumerate the device and create a device list. Call CameraInitEx
// You must call this function before enumerating the device.
// parameters:
// Return value: return the number of devices, 0 means no.
/***************************************************** *****/
typedef INT (__stdcall *_CameraEnumerateDeviceEx)(
	);

/***************************************************** *****/
// function name: CameraIsOpened
// Function Description: Check whether the device has been opened by other applications. Call CameraInit
// Before, you can use this function to detect, if it has been opened, called
// CameraInit will return the device has been opened error code.
// Parameters: pCameraList Device enumeration information structure pointer, obtained by the CameraEnumerateDevice.
// pOpened The device's status pointer that returns whether the device is open, TRUE is open, and FALSE is idle.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraIsOpened)(
	tSdkCameraDevInfo*  pCameraList, 
	BOOL*       pOpened
	);

/***************************************************** *****/
// function name: CameraInit
// Function Description: Camera initialization. After initialization is successful, anything else can be called
// camera-related interface.
// Parameters: pCameraInfo The camera's device description information, by CameraEnumerateDevice
// function to get.
// iParamLoadMode The parameter loading method used when the camera initializes. -1 indicates the parameter loading method when using last exit.
// PARAM_MODE_BY_MODEL means loaded by model
// PARAM_MODE_BY_SN means loading by serial number
// PARAM_MODE_BY_NAME means that it is loaded by nickname
// For details, please refer to the definition of emSdkParameterMode in CameraDefine.h.
// The parameter group used when emTeam is initialized. -1 indicates the parameter group when the last exit was loaded.
// pCameraHandle The handle of the camera handle, after the initialization is successful, the pointer
// returns a valid handle to this camera, calling another camera
// related to the operating interface, you need to pass in the handle, mainly
// Used for the distinction between multiple cameras.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraInit)(
	tSdkCameraDevInfo*  pCameraInfo,
	int         emParamLoadMode,
	int         emTeam,
	CameraHandle*   pCameraHandle
	);

/***************************************************** *****/
// function name: CameraInitEx
// Function Description: Camera initialization. After initialization is successful, anything else can be called
// camera-related interface.
// Parameters: iDeviceIndex The camera index number, CameraEnumerateDeviceEx returns the number of cameras.
// iParamLoadMode The parameter loading method used when the camera initializes. -1 indicates the parameter loading method when using last exit.
// PARAM_MODE_BY_MODEL means loaded by model
// PARAM_MODE_BY_SN means loading by serial number
// PARAM_MODE_BY_NAME means that it is loaded by nickname
// For details, please refer to the definition of emSdkParameterMode in CameraDefine.h.
// The parameter group used when emTeam is initialized. -1 indicates the parameter group when the last exit was loaded.
// pCameraHandle The handle of the camera handle, after the initialization is successful, the pointer
// returns a valid handle to this camera, calling another camera
// related to the operating interface, you need to pass in the handle, mainly
// Used for the distinction between multiple cameras.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef CameraSdkStatus (__stdcall *_CameraInitEx)(
	int             iDeviceIndex,
	int             iParamLoadMode,
	int             emTeam,
	CameraHandle*   pCameraHandle
	);

/***************************************************** *****/
// function name: CameraInitEx2
// Function Description: Camera initialization. After initialization is successful, anything else can be called
// camera-related interface. Note that you need to call CameraEnumerateDeviceEx to enumerate the camera first
// parameter: CameraName camera name
// pCameraHandle The handle of the camera handle, after the initialization is successful, the pointer
// returns a valid handle to this camera, calling another camera
// related to the operating interface, you need to pass in the handle, mainly
// Used for the distinction between multiple cameras.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef CameraSdkStatus (__stdcall *_CameraInitEx2)(
	char* CameraName,
	CameraHandle   *pCameraHandle
	);

/***************************************************** *****/
// function name: CameraSetCallbackFunction
// Function Description: Set the image capture callback function. When a new image data frame is captured,
// The callback function pointed to by pCallBack will be called.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// pCallBack callback function pointer.
// pContext callback function additional parameters, when the callback function is called
// This additional parameter will be passed in, can be NULL. Used more
// Multiple cameras carry additional information.
// pCallbackOld is used to save the current callback function. Can be NULL.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraSetCallbackFunction)(
	CameraHandle        hCamera,
	CAMERA_SNAP_PROC    pCallBack,
	PVOID               pContext,
	CAMERA_SNAP_PROC*   pCallbackOld
	);

/***************************************************** *****/
// function name: CameraUnInit
// Function Description: Camera anti-initialization. Release resources.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraUnInit)(
	CameraHandle hCamera
	);

/***************************************************** *****/
// function name: CameraGetInformation
// Function Description: Get the description of the camera
// parameters: hCamera camera handle, obtained by the CameraInit function.
// pbuffer Pointer to the camera description information pointer.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraGetInformation)(
	CameraHandle    hCamera, 
	char**          pbuffer
	);

/***************************************************** *****/
// function name: CameraImageProcess
// Function Description: Will be obtained by the original camera output image data processing, superimposed saturation,
// Color gain and correction, noise reduction and other effects, and finally get RGB888
// format the image data.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// pbyIn Input buffer address for image data, can not be NULL.
// pbyOut Buffer address for image output after processing, can not be NULL.
// pFrInfo Input image frame header information, processing is completed, the header information
// The image format uiMediaType will change accordingly.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraImageProcess)(
	CameraHandle        hCamera, 
	BYTE*               pbyIn, 
	BYTE*               pbyOut,
	tSdkFrameHead*      pFrInfo
	);

/***************************************************** *****/
// function name: CameraImageProcessEx
// Function Description: Will be obtained by the original camera output image data processing, superimposed saturation,
// Color gain and correction, noise reduction and other effects, and finally get RGB888
// format the image data.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// pbyIn Input buffer address for image data, can not be NULL.
// pbyOut Buffer address for image output after processing, can not be NULL.
// pFrInfo Input image frame header information, processing is completed, the header information
// The output format of the image processed by uOutFormat can be one of CAMERA_MEDIA_TYPE_MONO8 CAMERA_MEDIA_TYPE_RGB CAMERA_MEDIA_TYPE_RGBA8.
// pbyIn The corresponding buffer size, which must match the format specified by uOutFormat.
// uReserved Reserved parameter, must be set to 0
// The image format uiMediaType will change accordingly.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef CameraSdkStatus (__stdcall *_CameraImageProcessEx)(
	CameraHandle        hCamera, 
	BYTE*               pbyIn, 
	BYTE*               pbyOut,
	tSdkFrameHead*      pFrInfo,
	UINT                uOutFormat,
	UINT                uReserved
	);


/***************************************************** *****/
// function name: CameraDisplayInit
// Function Description: Initialize the display module inside the SDK. Call CameraDisplayRGB24
// must be called before the function is initialized. If you are in the secondary development,
// use your own way for image display (do not call CameraDisplayRGB24),
// You do not need to call this function.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// hWndDisplay display window handle, the window is generally m_hWnd members.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraDisplayInit)(
	CameraHandle    hCamera,
	HWND            hWndDisplay
	);

/***************************************************** *****/
// function name: CameraDisplayRGB24
// Function Description: Display image. CameraDisplayInit must be called
// Initialize to call this function.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// pbyRGB24 image data buffer, RGB888 format.
// The header of the pFrInfo image.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraDisplayRGB24)(
	CameraHandle        hCamera,
	BYTE*               pbyRGB24, 
	tSdkFrameHead*      pFrInfo
	);

/***************************************************** *****/
// function name: CameraSetDisplayMode
// Function Description: Set the display mode. CameraDisplayInit must be called
// Initialize to call this function.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// iMode display mode, DISPLAYMODE_SCALE or
// DISPLAYMODE_REAL, see CameraDefine.h for details
// The definition of emSdkDisplayMode.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraSetDisplayMode)(
	CameraHandle    hCamera,
	INT             iMode
	);

/***************************************************** *****/
// function name: CameraSetDisplayOffset
// Function Description: Set the displayed starting offset value. Only if the display mode is DISPLAYMODE_REAL
// effective. For example, the size of the display control is 320X240, while the image is
// size 640X480, then when iOffsetX = 160, iOffsetY = 120
// The area shown is the center of the image at 320x240. Must be called before
// CameraDisplayInit to initialize this function can be called.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// X coordinate of iOffsetX offset.
// iOffsetY Offset Y coordinate.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraSetDisplayOffset)(
	CameraHandle    hCamera,
	int             iOffsetX, 
	int             iOffsetY
	);

/***************************************************** *****/
// function name: CameraSetDisplaySize
// Function Description: Set the size of the display control. Must be called before
// CameraDisplayInit to initialize this function can be called.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// iWidth width
// iHeight height
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraSetDisplaySize)(
	CameraHandle    hCamera, 
	INT             iWidth, 
	INT             iHeight
	);

/***************************************************** *****/
// function name: CameraGetImageBuffer
// Function Description: Get a frame of image data. In order to improve efficiency, SDK uses a zero-copy mechanism for image capture,
// CameraGetImageBuffer actually gets a buffer address in the kernel,
// After the function is successfully called, you must call CameraReleaseImageBuffer to release by
// CameraGetImageBuffer Get the buffer for the kernel to continue using
// This buffer.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// The header information pointer of the pFrameInfo image.
// pbyBuffer Buffer pointer to the image's data. due to
// A zero-copy mechanism is used to improve efficiency, so
// Here is a pointer to the pointer.
// UINT wTimes The time-out period for grabbing the image. Milliseconds. in
// wTimes time has not yet been the image, then the function
// will return the timeout message.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraGetImageBuffer)(
	CameraHandle        hCamera, 
	tSdkFrameHead*      pFrameInfo, 
	BYTE**              pbyBuffer,
	UINT                wTimes
	);

/***************************************************** *****/
// function name: CameraGetImageBufferEx
// Function Description: Get a frame of image data. The image obtained by this interface is processed RGB format. After the function is called,
// Do not need to call CameraReleaseImageBuffer release, do not call free release of such functions
// to free the image data buffer returned by this function.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// piWidth plastic pointer, returns the width of the image
// piHeight plastic pointer, returns the height of the image
// UINT wTimes The time-out period for grabbing the image. Milliseconds. in
// wTimes time has not yet been the image, then the function
// will return the timeout message.
// Return Value: Returns the first address of the RGB data buffer on success;
// otherwise 0
/***************************************************** *****/
typedef unsigned char* (__stdcall *_CameraGetImageBufferEx)(
	CameraHandle        hCamera, 
	INT*                piWidth,
	INT*                piHeight,
	UINT                wTimes
	);

/***************************************************** *****/
// function name: CameraSnapToBuffer
// Function Description: Snap an image into the buffer. Camera will enter snap mode, and
// Automatically switch to the capture mode resolution for image capture. Then
// The captured data is saved in the buffer.
// After the function is successfully called, CameraReleaseImageBuffer must be called
// Free the buffer from CameraSnapToBuffer. Specific reference
// CameraGetImageBuffer function description section.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// pFrameInfo pointer returns the header information of the image.
// pbyBuffer Pointer to the pointer to return the address of the image buffer.
// uWaitTimeMs timeout, in milliseconds. In that time, if still not
// The data that was successfully captured returns the time-out information.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraSnapToBuffer)(
	CameraHandle        hCamera,
	tSdkFrameHead*      pFrameInfo,
	BYTE**              pbyBuffer,
	UINT                uWaitTimeMs
	);

/***************************************************** *****/
// function name: CameraReleaseImageBuffer
// Function Description: Free up the buffer obtained by CameraGetImageBuffer.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// pbyBuffer The buffer address obtained by CameraGetImageBuffer.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraReleaseImageBuffer)(
	CameraHandle    hCamera, 
	BYTE*           pbyBuffer
	);

/***************************************************** *****/
// function name: CameraPlay
// Function Description: Let SDK into working mode, began to receive images sent from the camera
// data. If the current camera is in trigger mode, it needs to be received
// The frame will be updated before updating the image.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraPlay)(
	CameraHandle hCamera
	);

/***************************************************** *****/
// function name: CameraPause
// Function Description: Let SDK into suspend mode, do not receive the image data from the camera,
// Also send a command to the camera to pause the output, release the transmission bandwidth.
// Pause mode, camera parameters can be configured and take effect immediately.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraPause)(
	CameraHandle hCamera
	);

/***************************************************** *****/
// function name: CameraStop
// Function Description: Let the SDK into the stop state, the call is usually called when the initialization function,
// This function is called and can no longer be configured on the camera's parameters.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraStop)(
	CameraHandle hCamera
	);

/***************************************************** *****/
// function name: CameraInitRecord
// Function Description: Initialize a video.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// iFormat video format, currently only supports non-compressed and MSCV two ways.
// 0: no compression; 1: MSCV compression.
// pcSavePath save the path to the video file.
// b2GLimit If TRUE, the file is automatically split when it is larger than 2G.
// dwQuality video quality factor, the greater the better quality. The range is from 1 to 100.
// iFrameRate video frame rate. Proposed setting than the actual acquisition frame rate,
// This will not miss the frame.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraInitRecord)(
	CameraHandle    hCamera,
	int             iFormat,
	char*           pcSavePath,
	BOOL            b2GLimit,
	DWORD           dwQuality,
	int             iFrameRate
	);

/***************************************************** *****/
// function name: CameraStopRecord
// Function Description: End this video. After CameraInitRecord, you can pass this function
// to end a video, and complete the file save operation.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraStopRecord)(
	CameraHandle    hCamera
	);

/***************************************************** *****/
// function name: CameraPushFrame
// Function Description: Save a frame of data into the video stream. CameraInitRecord must be called
// to call this function. After CameraStopRecord is called, it can not be called again
// This function. Since our frame header carries the timestamp of the image capture
// information, so recording can be precise time synchronization, and not subject to instability frame rate
// Impact.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// pbyImageBuffer Image data buffer must be in RGB format.
// The header of the pFrInfo image.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraPushFrame)(
	CameraHandle    hCamera,
	BYTE*           pbyImageBuffer,
	tSdkFrameHead*  pFrInfo
	);

/***************************************************** *****/
// function name: CameraSaveImage
// Function Description: The image buffer data saved as a picture file.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// lpszFileName save the file the full path to the picture.
// pbyImageBuffer image data buffer.
// The header of the pFrInfo image.
// byFileType The format for saving the image. See CameraDefine.h for the range of values
// The type definition of emSdkFileType. Currently supported
// BMP, JPG, PNG, RAW four formats. One of the RAW said
// Raw data output by the camera, save the RAW format file requirements
// pbyImageBuffer and pFrInfo are CameraGetImageBuffer
// Get the data, and without CameraImageProcess conversion
// Into BMP format; the other hand, if you want to save into BMP, JPG or
// PNG format, then pbyImageBuffer and pFrInfo are
// CameraImageProcess processed RGB format data.
// Specific usage can refer to Advanced routines.
// byQuality The image quality factor saved, only when saved as JPG
// This parameter is valid from 1 to 100. The rest of the format
// can be written as 0
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraSaveImage)(
	CameraHandle    hCamera,
	char*           lpszFileName,
	BYTE*           pbyImageBuffer,
	tSdkFrameHead*  pFrInfo,
	UINT            byFileType,
	BYTE            byQuality
	);

/***************************************************** *****/
// function name: CameraGetImageResolution
// Function Description: Get the current preview resolution.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// psCurVideoSize structure pointer, used to return the current resolution.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraGetImageResolution)(
	CameraHandle            hCamera, 
	tSdkImageResolution*    psCurVideoSize
	);

/***************************************************** *****/
// function name: CameraGetImageResolutionEx
// Function Description: Get the resolution of the camera.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// iIndex index number, [0, N] indicates the default resolution (N is the maximum number of the default resolution, generally not more than 20), OXFF means the custom resolution (ROI)
// acDescription Description of the resolution. This message is valid only for preset resolution. Custom resolution can ignore this information
// Mode 0: Normal mode 1: Sum 2: Average 3: Skip 4: Resample
// ModeSize ignored in normal mode, the first one said 2X2 second said 3X3 ...
// x, y horizontal and vertical offset
// width, height width and height
// ZoomWidth, ZoomHeight When the final output zoom to what, 0 does not scale
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef CameraSdkStatus (__stdcall *_CameraGetImageResolutionEx)(
	CameraHandle            hCamera, 
	int*					iIndex,
	char					acDescription[32],
	int*					Mode,
	UINT*					ModeSize,
	int*					x,
	int*					y,
	int*					width,
	int*					height,
	int*					ZoomWidth,
	int*					ZoomHeight
	);

/***************************************************** *****/
// function name: CameraSetImageResolution
// Function Description: Set the preview resolution.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// pImageResolution struct Pointer to return the current resolution.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraSetImageResolution)(
	CameraHandle            hCamera, 
	tSdkImageResolution*    pImageResolution
	);

/***************************************************** *****/
// function name: CameraSetImageResolutionEx
// Function Description: Set the camera's resolution.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// iIndex index number, [0, N] indicates the default resolution (N is the maximum number of the default resolution, generally not more than 20), OXFF means the custom resolution (ROI)
// Mode 0: Normal mode 1: Sum 2: Average 3: Skip 4: Resample
// ModeSize ignored in normal mode, the first one said 2X2 second said 3X3 ...
// x, y horizontal and vertical offset
// width, height width and height
// ZoomWidth, ZoomHeight When the final output zoom to what, 0 does not scale
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef CameraSdkStatus (__stdcall *_CameraSetImageResolutionEx)(
	CameraHandle            hCamera, 
	int						iIndex,
	int						Mode,
	UINT					ModeSize,
	int						x,
	int						y,
	int						width,
	int						height,
	int						ZoomWidth,
	int						ZoomHeight
	);

/***************************************************** *****/
// function name: CameraGetMediaType
// Function Description: Get the current format of the camera output format index number.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// The piMediaType pointer, used to return the index number of the current format type.
// CameraGetCapability get the camera's properties,
// pMediaTypeDesc in tSdkCameraCapbility structure
// Members, the form of an array to save the camera supports the format,
// The index number pointed to by piMediaType is the index number of this array.
// pMediaTypeDesc [* piMediaType] .iMediaType indicates the current format
// encoding This encoding can be found in the [Image Format Definition] section of CameraDefine.h.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraGetMediaType)(
	CameraHandle    hCamera, 
	INT*            piMediaType
	);

/***************************************************** *****/
// function name: CameraSetMediaType
// Function Description: Set the camera's output raw data format.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// iMediaType gets the camera's properties from CameraGetCapability,
// pMediaTypeDesc in tSdkCameraCapbility structure
// Members, the form of an array to save the camera supports the format,
// iMediaType is the index number of this array.
// pMediaTypeDesc [iMediaType] .iMediaType indicates the current format
// encoding This encoding can be found in the [Image Format Definition] section of CameraDefine.h.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraSetMediaType)(
	CameraHandle    hCamera, 
	INT             iMediaType
	);

/***************************************************** *****/
// function name: CameraSetAeState
// Function Description: Set the camera exposure mode. Automatic or manual.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// bAeState TRUE, enable auto exposure; FALSE, stop auto exposure.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraSetAeState)(
	CameraHandle    hCamera, 
	BOOL            bAeState
	);

/***************************************************** *****/
// function name: CameraGetAeState
// Function Description: Get the camera's current exposure mode.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// pAeState Pointer to return the auto exposure enable status.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraGetAeState)(
	CameraHandle    hCamera, 
	BOOL*           pAeState
	);

/***************************************************** *****/
// function name: CameraSetSharpness
// Function Description: Set the sharpening parameter of the image processing.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// iSharpness sharpening parameter. Range by CameraGetCapability
// Get, usually [0,100], 0 means the sharpening is off.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraSetSharpness)(
	CameraHandle    hCamera, 
	int             iSharpness
	);

/***************************************************** *****/
// function name: CameraGetSharpness
// Function Description: Get the current sharpening setting.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// The piSharpness pointer returns the current sharpened setting.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraGetSharpness)(
	CameraHandle    hCamera, 
	int*            piSharpness
	);

/***************************************************** *****/
// function name: CameraSetLutMode
// Function Description: Set the camera's look-up table conversion mode LUT mode.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// emLutMode LUTMODE_PARAM_GEN means that LUT tables are generated dynamically by gamma and contrast parameters.
// LUTMODE_PRESET means that the default LUT table is used.
// LUTMODE_USER_DEF said the use of user-defined LUT table.
// The definition of LUTMODE_PARAM_GEN refers to the emSdkLutMode type in CameraDefine.h.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraSetLutMode)(
	CameraHandle    hCamera,
	int             emLutMode
	);

/***************************************************** *****/
// function name: CameraGetLutMode
// Function Description: Get the camera's look-up table transform mode LUT mode.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// pemLutMode pointer, return the current LUT mode. Meaning and CameraSetLutMode
// The same emLutMode parameters.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraGetLutMode)(
	CameraHandle    hCamera,
	int*            pemLutMode
	);

/***************************************************** *****/
// function name: CameraSelectLutPreset
// Function Description: Select the LUT table in the default LUT mode. You must use CameraSetLutMode first
// Set LUT mode to default mode.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// The index number of the iSel table. The number of tables by CameraGetCapability
// Get it.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraSelectLutPreset)(
	CameraHandle    hCamera,
	int             iSel
	);

/***************************************************** *****/
// function name: CameraGetLutPresetSel
// Function Description: Get LUT table index in default LUT mode.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// piSel pointer, return the index number of the table.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraGetLutPresetSel)(
	CameraHandle    hCamera,
	int*            piSel
	);

/***************************************************** *****/
// function name: CameraSetCustomLut
// Function Description: Set a custom LUT table. You must use CameraSetLutMode first
// Set LUT mode to custom mode.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// iChannel specifies the LUT color channel to be set, when LUT_CHANNEL_ALL,
// Three channels of LUT will be replaced at the same time.
// Refer to the definition of emSdkLutChannel in CameraDefine.h.
// pLut pointer to the address of the LUT table. LUT table unsigned short array, array size
// 4096, respectively, the code color channel from 0 to 4096 (12bit color precision) corresponding to the mapping value.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraSetCustomLut)(
	CameraHandle    hCamera,
	int       iChannel,
	USHORT*         pLut
	);

/***************************************************** *****/
// function name: CameraGetCustomLut
// Function Description: Get the current custom LUT table.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// iChannel Specifies the LUT color channel to get. When LUT_CHANNEL_ALL,
// returns the red channel's LUT table.
// Refer to the definition of emSdkLutChannel in CameraDefine.h.
// pLut pointer to the address of the LUT table. LUT table unsigned short array, array size
// 4096, respectively, the code color channel from 0 to 4096 (12bit color precision) corresponding to the mapping value.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraGetCustomLut)(
	CameraHandle    hCamera,
	int       iChannel,
	USHORT*         pLut
	);

/***************************************************** *****/
// function name: CameraGetCurrentLut
// Function Description: Get the camera's current LUT table, can be called in any LUT mode,
// Used to visually observe changes in the LUT curve.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// iChannel Specifies the LUT color channel to get. When LUT_CHANNEL_ALL,
// returns the red channel's LUT table.
// Refer to the definition of emSdkLutChannel in CameraDefine.h.
// pLut pointer to the address of the LUT table. LUT table unsigned short array, array size
// 4096, respectively, the code color channel from 0 to 4096 (12bit color precision) corresponding to the mapping value.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraGetCurrentLut)(
	CameraHandle    hCamera,
	int       iChannel,
	USHORT*         pLut
	);

/***************************************************** *****/
// function name: CameraSetWbMode
// Function Description: Set the camera white balance mode. Divided into manual and automatic two ways.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// bAuto TRUE, then the automatic mode is enabled.
// FALSE, then use manual mode, by calling
// CameraSetOnceWB to do a white balance.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraSetWbMode)(
	CameraHandle    hCamera,
	BOOL            bAuto
	);

/***************************************************** *****/
// function name: CameraGetWbMode
// Function Description: Get the current white balance mode.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// pbAuto pointer, returns TRUE for automatic mode, FALSE
// is manual mode.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraGetWbMode)(
	CameraHandle    hCamera,
	BOOL*           pbAuto
	);

/***************************************************** *****/
// function name: CameraSetPresetClrTemp
// Function Description: Select to specify the preset color temperature mode
// parameters: hCamera camera handle, obtained by the CameraInit function.
// iSel Preset color temperature mode index number, starting from 0
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraSetPresetClrTemp)(
	CameraHandle    hCamera,
	int             iSel
	);

/***************************************************** *****/
// function name: CameraGetPresetClrTemp
// Function Description: Get the currently selected preset color temperature mode.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// piSel pointer, return to the selected default color temperature index
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraGetPresetClrTemp)(
	CameraHandle    hCamera,
	int*            piSel
	);

/***************************************************** *****/
// function name: CameraSetUserClrTempGain
// Function Description: Set the digital gain in custom color temperature mode
// parameters: hCamera camera handle, obtained by the CameraInit function.
// iRgain Red Gain, ranging from 0 to 400, representing 0 to 4 times
// iGgain green gain, ranging from 0 to 400, representing 0 to 4 times
// iBgain Blue gain, range 0 to 400, representing 0 to 4 times
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraSetUserClrTempGain)(
	CameraHandle  hCamera,
	int       iRgain,
	int       iGgain,
	int       iBgain
	);


/***************************************************** *****/
// function name: CameraGetUserClrTempGain
// Function Description: Get the digital gain in custom color temperature mode
// parameters: hCamera camera handle, obtained by the CameraInit function.
// piRgain pointer returns the red gain, ranging from 0 to 400, representing 0 to 4 times
// piGgain pointer, returns the green gain, range 0 to 400, that 0 to 4 times
// The piBgain pointer returns the blue gain, ranging from 0 to 400, representing 0 to 4 times
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraGetUserClrTempGain)(
	CameraHandle  hCamera,
	int*      piRgain,
	int*      piGgain,
	int*      piBgain
	);

/***************************************************** *****/
// function name: CameraSetUserClrTempMatrix
// Function Description: Set the color matrix in custom color temperature mode
// parameters: hCamera camera handle, obtained by the CameraInit function.
// pMatrix points to the first address of an array of float [3] [3]
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraSetUserClrTempMatrix)(
	CameraHandle  hCamera,
	float*      pMatrix
	);


/***************************************************** *****/
// function name: CameraGetUserClrTempMatrix
// Function Description: Get the color matrix in custom color temperature mode
// parameters: hCamera camera handle, obtained by the CameraInit function.
// pMatrix points to the first address of an array of float [3] [3]
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraGetUserClrTempMatrix)(
	CameraHandle  hCamera,
	float*      pMatrix
	);

/***************************************************** *****/
// function name: CameraSetClrTempMode
// Function Description: Set the color temperature mode used when the white balance,
// There are three supported modes, Auto, Preset, and Custom.
// Auto mode, will automatically select the appropriate color temperature mode
// Preset mode, user-specified color temperature mode will be used
// Custom mode with user-defined color temperature digital gain and matrix
// parameters: hCamera camera handle, obtained by the CameraInit function.
// iMode mode, only one that is defined in emSdkClrTmpMode
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraSetClrTempMode)(
	CameraHandle  hCamera,
	int       iMode
	);

/***************************************************** *****/
// function name: CameraGetClrTempMode
// Function Description: Color temperature mode used to get white balance. Reference CameraSetClrTempMode
// Function description section.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// pimode pointer, return mode selection, refer to the definition of type emSdkClrTmpMode
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraGetClrTempMode)(
	CameraHandle  hCamera,
	int*      pimode
	);


/***************************************************** *****/
// function name: CameraSetOnceWB
// Function description: In manual white balance mode, calling this function will make a white balance.
// The effective time is when the next frame of image data is received.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraSetOnceWB)(
	CameraHandle    hCamera
	);

/***************************************************** *****/
// function name: CameraSetOnceBB
// Function Description: Perform a black balance operation.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraSetOnceBB)(
	CameraHandle    hCamera
	);


/***************************************************** *****/
// function name: CameraSetAeTarget
// Function Description: Set the brightness of the automatic exposure target value. The setting range is CameraGetCapability
// function to get.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// iAeTarget Brightness target value.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraSetAeTarget)(
	CameraHandle    hCamera, 
	int             iAeTarget
	);

/***************************************************** *****/
// function name: CameraGetAeTarget
// Function Description: Get the target brightness value for auto exposure.
// parameters: hCamera camera handle, obtained by the CameraInit function.
//* piAeTarget pointer, return the target value.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraGetAeTarget)(
	CameraHandle    hCamera, 
	int*            piAeTarget
	);

/***************************************************** *****/
// function name: CameraSetAeExposureRange
// Function Description: Set the exposure time adjustment range of auto exposure mode
// parameters: hCamera camera handle, obtained by the CameraInit function.
// fMinExposureTime Minimum exposure time (microseconds)
// fMaxExposureTime Maximum exposure time (microseconds)
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef CameraSdkStatus (__stdcall *_CameraSetAeExposureRange)(
	CameraHandle    hCamera, 
	double          fMinExposureTime,
	double			fMaxExposureTime
	);

/***************************************************** *****/
// function name: CameraGetAeExposureRange
// Function description: Get exposure time adjustment range of auto exposure mode
// parameters: hCamera camera handle, obtained by the CameraInit function.
// fMinExposureTime Minimum exposure time (microseconds)
// fMaxExposureTime Maximum exposure time (microseconds)
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef CameraSdkStatus (__stdcall *_CameraGetAeExposureRange)(
	CameraHandle    hCamera, 
	double*         fMinExposureTime,
	double*			fMaxExposureTime
	);

/***************************************************** *****/
// function name: CameraSetAeAnalogGainRange
// Function Description: Set the gain adjustment range of auto exposure mode
// parameters: hCamera camera handle, obtained by the CameraInit function.
// iMinAnalogGain minimum gain
// iMaxAnalogGain Maximum gain
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef CameraSdkStatus (__stdcall *_CameraSetAeAnalogGainRange)(
	CameraHandle    hCamera, 
	int				iMinAnalogGain,
	int				iMaxAnalogGain
	);

/***************************************************** *****/
// function name: CameraGetAeAnalogGainRange
// Function Description: Get gain adjustment range of auto exposure mode
// parameters: hCamera camera handle, obtained by the CameraInit function.
// iMinAnalogGain minimum gain
// iMaxAnalogGain Maximum gain
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef CameraSdkStatus (__stdcall *_CameraGetAeAnalogGainRange)(
	CameraHandle    hCamera, 
	int*			iMinAnalogGain,
	int*			iMaxAnalogGain
	);

/***************************************************** *****/
// function name: CameraSetAeThreshold
// Function Description: Set the adjustment threshold for auto exposure mode
// parameters: hCamera camera handle, obtained by the CameraInit function.
// iThreshold If abs (target brightness - image brightness) <iThreshold then stop automatic adjustment
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef CameraSdkStatus (__stdcall *_CameraSetAeThreshold)(
	CameraHandle    hCamera, 
	int				iThreshold
	);

/***************************************************** *****/
// function name: CameraGetAeThreshold
// Function Description: Get the adjustment threshold for auto exposure mode
// parameters: hCamera camera handle, obtained by the CameraInit function.
// iThreshold Read the adjustment threshold
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef CameraSdkStatus (__stdcall *_CameraGetAeThreshold)(
	CameraHandle    hCamera, 
	int*			iThreshold
	);

/***************************************************** *****/
// function name: CameraSetExposureTime
// Function Description: Set the exposure time. The unit is microseconds. For CMOS sensors, they are exposed the unit of 
// is calculated by line, therefore, the exposure time can not be in microseconds
// level continuously adjustable. But will follow the entire line to choose. Calling
// This function is to set the exposure time, it is recommended to call CameraGetExposureTime again
// to get the actual set value.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// fExposureTime exposure time, in microseconds.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraSetExposureTime)(
	CameraHandle    hCamera, 
	double          fExposureTime
	);

/***************************************************** *****/
// function name: CameraGetExposureLineTime
// Function Description: Get a line of exposure time. For CMOS sensors, they are exposed the unit of 
// is calculated by line, therefore, the exposure time can not be in microseconds
// level continuously adjustable. But will follow the entire line to choose. This function
// The role is to return the CMOS camera exposure line corresponding time.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// pfLineTime pointer, return the exposure time of his party, in microseconds.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraGetExposureLineTime)(
	CameraHandle    hCamera, 
	double*         pfLineTime
	);

/***************************************************** *****/
// function name: CameraGetExposureTime
// Function Description: Get the camera's exposure time. See CameraSetExposureTime
// function description.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// pfExposureTime pointer to return the current exposure time in microseconds.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraGetExposureTime)(
	CameraHandle    hCamera, 
	double*         pfExposureTime
	);

/***************************************************** *****/
// function name: CameraGetExposureTimeRange
// Function Description: Get the camera exposure time range
// parameters: hCamera camera handle, obtained by the CameraInit function.
// pfMin pointer, returns the minimum exposure time in microseconds.
// pfMax pointer, returns the maximum exposure time in microseconds.
// pfStep pointer, returns the exposure time step, in microseconds.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraGetExposureTimeRange)(
	CameraHandle    hCamera, 
	double*         pfMin,
	double*			pfMax,
	double*			pfStep
	);

/***************************************************** *****/
// function name: CameraSetAnalogGain
// Function Description: Set the camera's image analog gain value. This value is multiplied by CameraGetCapability
// camera attribute structure sExposeDesc.fAnalogGainStep, on
// Get the actual image signal magnification.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// Analog gain value set by iAnalogGain.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraSetAnalogGain)(
	CameraHandle    hCamera,
	INT             iAnalogGain
	);

/***************************************************** *****/
// function name: CameraGetAnalogGain
// Function Description: Get the analog gain value of the image signal. See CameraSetAnalogGain
// Detailed description.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// piAnalogGain pointer, returns the current analog gain value.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraGetAnalogGain)(
	CameraHandle    hCamera, 
	INT*            piAnalogGain
	);

/***************************************************** *****/
// function name: CameraSetGain
// Function Description: Set the digital gain of the image. The setting range is CameraGetCapability
// get the sRgbGainRange member representation in the camera's attribute structure.
// The actual magnification is the set value / 100.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// iRGain The gain of the red channel.
// iGGain green channel gain value.
// iBGain Blue channel gain value.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraSetGain)(
	CameraHandle    hCamera, 
	int             iRGain, 
	int             iGGain, 
	int             iBGain
	);


/***************************************************** *****/
// function name: CameraGetGain
// Function Description: Get the digital gain of image processing. For details, see CameraSetGain
// function description section.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// piRGain pointer returns the digital gain value of the red channel.
// piGGain pointer, returns the green channel's digital gain value.
// piBGain pointer returns the digital gain value of the blue channel.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraGetGain)(
	CameraHandle    hCamera, 
	int*            piRGain, 
	int*            piGGain, 
	int*            piBGain
	);


/***************************************************** *****/
// function name: CameraSetGamma
// Function Description: Set the Gamma value in LUT dynamic generation mode. The set value will be
// Save it inside the SDK right away, but only when the camera is in motion
// LUT mode generated by the parameter will not take effect. Please refer to CameraSetLutMode
// Function Description section.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// Gamma to be set by iGamma.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraSetGamma)(
	CameraHandle    hCamera, 
	int             iGamma
	);

/***************************************************** *****/
// function name: CameraGetGamma
// Function Description: Get Gamma value in LUT dynamic generation mode. Please refer to CameraSetGamma
// function description of the function.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// piGamma pointer, return the current Gamma value.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraGetGamma)(
	CameraHandle    hCamera, 
	int*            piGamma
	);

/***************************************************** *****/
// function name: CameraSetContrast
// Function Description: Set the LUT dynamic generation mode of the contrast value. The set value will be
// Save it inside the SDK right away, but only when the camera is in motion
// LUT mode generated by the parameter will not take effect. Please refer to CameraSetLutMode
// Function Description section.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// Contrast value set by iContrast.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraSetContrast)(
	CameraHandle    hCamera, 
	int             iContrast
	);

/***************************************************** *****/
// function name: CameraGetContrast
// Function Description: Obtain the LUT dynamic generation mode of the contrast value. Please refer to
// Description of the function of CameraSetContrast function.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// piContrast pointer, return the current contrast value.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraGetContrast)(
	CameraHandle    hCamera, 
	int*            piContrast
	);

/***************************************************** *****/
// function name: CameraSetSaturation
// Function Description: Set the saturation of image processing. Not valid for black and white cameras.
// The setting range is obtained by CameraGetCapability. 100 said
// said the original color, not enhanced.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// Saturation value set by iSaturation.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraSetSaturation)(
	CameraHandle    hCamera, 
	int             iSaturation
	);

/***************************************************** *****/
// function name: CameraGetSaturation
// Function Description: Get the saturation of image processing.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// piSaturation pointer, returns the saturation value of the current image processing.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraGetSaturation)(
	CameraHandle    hCamera, 
	int*            piSaturation
	);

/***************************************************** *****/
// function name: CameraSetMonochrome
// Function Description: Set the color to black and white function enabled.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// bEnable TRUE, said the color image is converted to black and white.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraSetMonochrome)(
	CameraHandle    hCamera, 
	BOOL            bEnable
	);

/***************************************************** *****/
// function name: CameraGetMonochrome
// Function Description: Get color conversion black and white function enabled.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// pbEnable pointer. Returning TRUE means that the color image is on
// Convert to black and white image function.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraGetMonochrome)(
	CameraHandle    hCamera, 
	BOOL*           pbEnable
	);

/***************************************************** *****/
// function name: CameraSetInverse
// Function Description: Set color image color flip function enable.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// bEnable TRUE, that open the image color flip function,
// Can get similar film negative effects.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraSetInverse)(
	CameraHandle    hCamera, 
	BOOL            bEnable
	);

/***************************************************** *****/
// function name: CameraGetInverse
// Function Description: Enables the image color reversal function to be enabled.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// pbEnable pointer to return the function enable status.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraGetInverse)(
	CameraHandle    hCamera, 
	BOOL*           pbEnable
	);

/***************************************************** *****/
// function name: CameraSetAntiFlick
// Function Description: Set the anti-strobe function enabled when auto exposure. For manual
// Invalid exposure mode.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// bEnable TRUE, turn on anti-strobe function; FALSE, turn off this function.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraSetAntiFlick)(
	CameraHandle    hCamera,
	BOOL            bEnable
	);

/***************************************************** *****/
// function name: CameraGetAntiFlick
// Function Description: Enable anti-strobe function when auto exposure is enabled.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// pbEnable pointer to return the enable status of this function.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraGetAntiFlick)(
	CameraHandle    hCamera, 
	BOOL*           pbEnable
	);

/***************************************************** *****/
// function name: CameraGetLightFrequency
// Function Description: When auto-exposure is obtained, the frequency of the flicker is selected.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// piFrequencySel pointer, return the selected index number. 0: 50HZ 1: 60HZ
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraGetLightFrequency)(
	CameraHandle    hCamera, 
	int*            piFrequencySel
	);

/***************************************************** *****/
// function name: CameraSetLightFrequency
// Function Description: Set the frequency of auto-flashing.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// iFrequencySel 0: 50HZ, 1: 60HZ
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraSetLightFrequency)(
	CameraHandle    hCamera,
	int             iFrequencySel
	);

/***************************************************** *****/
// function name: CameraSetFrameSpeed
// Function Description: Set the camera output image frame rate. The frame rate mode the camera can choose from
// CameraFrameSpeedDesc in the info structure obtained by CameraGetCapability
// Indicates the maximum frame rate selection mode number.
// parameters: hCamera camera handle, obtained by the CameraInit function.
//             iFrameSpeed The selected frame rate mode index, ranging from 0 to
//             iFrameSpeedDesc - 1 in the info structure obtained by CameraGetCapability
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraSetFrameSpeed)(
	CameraHandle    hCamera, 
	int             iFrameSpeed
	);

/***************************************************** *****/
// function name: CameraGetFrameSpeed
// Function Description: Get the frame rate of the camera output image Select the index number. Specific usage reference
// Function Description section of CameraSetFrameSpeed function.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// piFrameSpeed pointer, return the selected frame rate mode index number.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraGetFrameSpeed)(
	CameraHandle    hCamera, 
	int*            piFrameSpeed
	);


/***************************************************** *****/
// function name: CameraSetParameterMode
// Function Description: Set the target of access to parameters.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// The object accessed by the iMode parameter. Reference CameraDefine.h
// The type definition of emSdkParameterMode.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraSetParameterMode)(
	CameraHandle    hCamera, 
	int             iTarget
	);

/***************************************************** *****/
// function name: CameraGetParameterMode
// Function Description:
// parameters: hCamera camera handle, obtained by the CameraInit function.
// int * piTarget
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraGetParameterMode)(
	CameraHandle    hCamera, 
	int*            piTarget
	);

/***************************************************** *****/
// function name: CameraSetParameterMask
// Function Description: Set the parameter access mask. The parameters are loaded and saved according to that
// Mask to determine whether the parameters of each module is loaded or saved.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// uMask mask. Reference PROP_SHEET_INDEX in CameraDefine.h
// type definition.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraSetParameterMask)(
	CameraHandle    hCamera, 
	UINT            uMask
	);

/***************************************************** *****/
// function name: CameraSaveParameter
// Function Description: Save the current camera parameters to the specified parameter group. The camera provides A, B, C, D.
// A, B, C, D four groups of space to save the parameters.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// iTeam PARAMETER_TEAM_A save to group A,
// PARAMETER_TEAM_B save to group B,
// PARAMETER_TEAM_C to C group,
// PARAMETER_TEAM_D is saved in group D
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraSaveParameter)(
	CameraHandle    hCamera, 
	int             iTeam
	);

/***************************************************** *****/
// function name: CameraReadParameterFromFile
// Function Description: Load parameters from the specified parameter file on the PC. My company camera parameters
// save the .config suffix file on your PC, under installation
// Camera \ Configs folder.
// parameters: hCamera camera handle, obtained by the CameraInit function.
//* sFileName The full path to the parameter file.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraReadParameterFromFile)(
	CameraHandle    hCamera,
	char*           sFileName
	);

/***************************************************** *****/
// function name: CameraLoadParameter
// Function Description: Load the parameters of the specified group into the camera.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// iTeam PARAMETER_TEAM_A Load group A parameters,
// PARAMETER_TEAM_B Load group B parameters,
// PARAMETER_TEAM_C Load C group parameters,
// PARAMETER_TEAM_D Load group D parameters,
// PARAMETER_TEAM_DEFAULT loads the default parameters.
// The type definition references the emSdkParameterTeam type in CameraDefine.h
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraLoadParameter)(
	CameraHandle    hCamera, 
	int             iTeam
	);

/***************************************************** *****/
// function name: CameraGetCurrentParameterGroup
// Function Description: Get the currently selected parameter group.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// piTeam pointer, return the currently selected parameter group. return value
// Reference iTeam parameters in CameraLoadParameter.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraGetCurrentParameterGroup)(
	CameraHandle    hCamera, 
	int*            piTeam
	);

/***************************************************** *****/
// function name: CameraSetTransPackLen
// Function Description: Set the sub-size of the image data transmitted by the camera.
// In the current SDK version, this interface is only valid for GIGE interface cameras,
// Used to control the subcontracting size of the network transmission. For support jumbo frame card,
// We suggest to choose 8K sub-size, which can effectively reduce the transmission
// Occupied CPU processing time.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// Index number for iPackSel subcontract length selection. Subcontracting length can be
// Get the pPackLenDesc member representation in the camera attribute structure,
// iPackLenDesc members indicate the maximum number of optional subcontracting modes.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraSetTransPackLen)(
	CameraHandle    hCamera, 
	INT             iPackSel
	);

/***************************************************** *****/
// function name: CameraGetTransPackLen
// Function Description: Obtain the selection index of the camera's current transmission subcontract size.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// piPackSel pointer, returns the index number of the currently selected subcontract size.
// See iPackSel in CameraSetTransPackLen
// Description.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraGetTransPackLen)(
	CameraHandle    hCamera, 
	INT*            piPackSel
	);

/***************************************************** *****/
// function name: CameraIsAeWinVisible
// Function Description: Get the auto exposure reference window display status.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// pbIsVisible pointer, return TRUE, then the current window will be
// is superimposed on the image content.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraIsAeWinVisible)(
	CameraHandle    hCamera,
	BOOL*           pbIsVisible
	);

/***************************************************** *****/
// function name: CameraSetAeWinVisible
// Function Description: Set the display status of the auto exposure reference window. When setting the window status
// To show, after calling CameraImageOverlay, it is possible to position the window
// superimposed on the image as a rectangle
// parameters: hCamera camera handle, obtained by the CameraInit function.
// bIsVisible TRUE, set to display; FALSE, do not show.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraSetAeWinVisible)(
	CameraHandle    hCamera,
	BOOL            bIsVisible
	);

/***************************************************** *****/
// function name: CameraGetAeWindow
// Function Description: Get the position of the auto exposure reference window.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// piHOff pointer, return to the upper left corner of the window position abscissa value.
// piVOff pointer, return to the upper left corner of the window position ordinate value.
// piWidth pointer, return the width of the window.
// piHeight pointer, return the height of the window.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraGetAeWindow)(
	CameraHandle    hCamera, 
	INT*            piHOff, 
	INT*            piVOff, 
	INT*            piWidth, 
	INT*            piHeight
	);

/***************************************************** *****/
// function name: CameraSetAeWindow
// Function Description: Set the reference window for auto exposure.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// iHOff abscissa in the upper left corner of the window
// iVOff window in the upper left corner of the vertical axis
// iWidth The width of the window
// iHeight the height of the window
// If iHOff, iVOff, iWidth, iHeight are all 0, then
// Set the window to center 1/2 size at each resolution. Can follow
// changes in resolution to follow changes; if iHOff, iVOff, iWidth, iHeight
// The determined window position range is outside the current resolution range,
// The center 1/2 size window is automatically used.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraSetAeWindow)(
	CameraHandle    hCamera, 
	int             iHOff, 
	int             iVOff, 
	int             iWidth, 
	int             iHeight
	);

/***************************************************** *****/
// function name: CameraSetMirror
// Function Description: Set the image mirroring operation. Mirror operation is divided into horizontal and vertical directions.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// iDir indicates the direction of the image. 0, said the horizontal direction; 1, said the vertical direction.
// bEnable TRUE, mirroring enabled; FALSE, mirroring disabled
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraSetMirror)(
	CameraHandle    hCamera, 
	int             iDir, 
	BOOL            bEnable
	);

/***************************************************** *****/
// function name: CameraGetMirror
// Function Description: Get the image status of the image.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// iDir indicates the mirror direction to be obtained.
// 0, said the horizontal direction; 1, said the vertical direction.
// pbEnable pointer, returns TRUE, then iDir refers to the direction
// image is enabled.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraGetMirror)(
	CameraHandle    hCamera, 
	int             iDir, 
	BOOL*           pbEnable
	);

/***************************************************** *****/
// function name: CameraGetWbWindow
// Function Description: Get the location of the white balance reference window.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// PiHOff pointer, return to the upper left corner of the reference window abscissa.
// PiVOff pointer returns the ordinate of the upper left corner of the reference window.
// PiWidth pointer, returns the width of the reference window.
// PiHeight pointer, return the reference window height.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraGetWbWindow)(
	CameraHandle    hCamera, 
	INT*            PiHOff, 
	INT*            PiVOff, 
	INT*            PiWidth, 
	INT*            PiHeight
	);

/***************************************************** *****/
// function name: CameraSetWbWindow
// Function Description: Set the position of the white balance reference window.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// iHOff reference window in the upper left corner of the horizontal axis.
// iVOff reference window in the upper left corner of the vertical axis.
// iWidth The width of the reference window.
// iHeight reference window height.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraSetWbWindow)(
	CameraHandle    hCamera, 
	INT             iHOff, 
	INT             iVOff, 
	INT             iWidth, 
	INT             iHeight
	);

/***************************************************** *****/
// function name: CameraIsWbWinVisible
// Function Description: Get the display status of the white balance window.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// pbShow pointer, returns TRUE, then the window is visible.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraIsWbWinVisible)(
	CameraHandle    hCamera,
	BOOL*           pbShow
	);

/***************************************************** *****/
// function name: CameraSetWbWinVisible
// Function Description: Set the display state of the white balance window.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// bShow TRUE, then set to visible. Calling
// CameraImageOverlay, the image content will be a rectangle
// superimposes the position of the white balance reference window.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraSetWbWinVisible)(
	CameraHandle    hCamera, 
	BOOL            bShow
	);

/***************************************************** *****/
// function name: CameraImageOverlay
// Function Description: The input image data superimposed cross line, white balance reference window,
// Automatic exposure reference window and other graphics. Only set to visible state
// Crosshair and reference window can be superimposed.
// Note that the function's input image must be in RGB format.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// pRgbBuffer image data buffer.
// The header of the pFrInfo image.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraImageOverlay)(
	CameraHandle    hCamera,
	BYTE*           pRgbBuffer,
	tSdkFrameHead*  pFrInfo
	);

/***************************************************** *****/
// function name: CameraSetCrossLine
// Function Description: Set the parameters of the specified crosshair.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// iLine said to set the state of the first few crosshairs. The range is [0,8], a total of nine.
// x abscissa center position of the abscissa value.
// Y axis position of the vertical axis value.
// uColor crosshair color in the format (R | (G << 8) | (B << 16))
// bVisible Crosshair display status. TRUE, that shows.
// Only crosshairs set to show status are called
// CameraImageOverlay will be superimposed on the image.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraSetCrossLine)(
	CameraHandle    hCamera, 
	int             iLine, 
	INT             x,
	INT             y,
	UINT            uColor,
	BOOL            bVisible
	);

/***************************************************** *****/
// function name: CameraGetCrossLine
// Function Description: Get the status of the specified crosshair.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// iLine said to get the first few crosshair status. The range is [0,8], a total of nine.
// px pointer to return the center of the crosshair abscissa.
// py pointer returns the abscissa of the center of the crosshair.
// The pcolor pointer returns the color of the crosshair in the format (R | (G << 8) | (B << 16)).
// pbVisible pointer, return TRUE, then the crosshair visible.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraGetCrossLine)(
	CameraHandle    hCamera, 
	INT             iLine,
	INT*            px,
	INT*            py,
	UINT*           pcolor,
	BOOL*           pbVisible
	);

/***************************************************** *****/
// function name: CameraGetCapability
// Function Description: Get the camera's characterization structure. The structure contains the camera
// Range of various parameters that can be set. Determine the parameters of the relevant function
// return, can also be used to dynamically create the camera's configuration interface.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// The pCameraInfo pointer returns the structure of the camera's characterization.
// tSdkCameraCapbility is defined in CameraDefine.h.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraGetCapability)(
	CameraHandle            hCamera, 
	tSdkCameraCapbility*    pCameraInfo
	);

/***************************************************** *****/
// function name: CameraWriteSN
// Function Description: Set the camera's serial number. My company camera serial number is divided into three levels.
// 0 level is my company custom camera serial number, the factory has been
// Set, 1 and 2 left for secondary development. Each level of sequence
// length is 32 bytes.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// pbySN serial number of the buffer zone.
// iLevel to set the serial number level, only 1 or 2.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraWriteSN)(
	CameraHandle    hCamera, 
	BYTE*           pbySN, 
	INT             iLevel
	);

/***************************************************** *****/
// function name: CameraReadSN
// Function Description: Read the camera specified level serial number. Please refer to the definition of serial number
// CameraWriteSN function description section.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// pbySN serial number of the buffer zone.
// iLevel The sequence number level to read. Only 1 and 2.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraReadSN)(
	CameraHandle        hCamera, 
	BYTE*               pbySN, 
	INT                 iLevel
	);
/***************************************************** *****/
// function name: CameraSetTriggerDelayTime
// Function Description: Set the trigger delay time in hardware trigger mode, in microseconds.
// When the hard trigger signal arrives, after the specified delay, and then start collecting
// image. Only some models of the camera support this feature. Please check
// Product Manual.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// uDelayTimeUs Hard trigger delay. Unit microseconds.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraSetTriggerDelayTime)(
	CameraHandle    hCamera, 
	UINT            uDelayTimeUs
	);

/***************************************************** *****/
// function name: CameraGetTriggerDelayTime
// Function Description: Get the currently set hard trigger delay time.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// puDelayTimeUs pointer, the return delay time, in microseconds.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraGetTriggerDelayTime)(
	CameraHandle    hCamera, 
	UINT*           puDelayTimeUs
	);

/***************************************************** *****/
// function name: CameraSetTriggerCount
// Function Description: Set the number of trigger frames in trigger mode. On software trigger and hardware trigger
// mode is valid. The default is 1 frame, that is, a trigger signal acquisition of a frame image.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// iCount The number of frames triggered at a time.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraSetTriggerCount)(
	CameraHandle    hCamera, 
	INT             iCount
	);

/***************************************************** *****/
// function name: CameraGetTriggerCount
// Function Description: Get the number of frames to trigger once.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// INT * piCount
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraGetTriggerCount)(
	CameraHandle    hCamera, 
	INT*            piCount
	);

/***************************************************** *****/
// function name: CameraSoftTrigger
// Function description: Execute soft trigger once. After execution, triggered by CameraSetTriggerCount
// The specified number of frames.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraSoftTrigger)(
	CameraHandle    hCamera
	);

/***************************************************** *****/
// function name: CameraSetTriggerMode
// Function Description: Set the camera's trigger mode.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// iModeSel mode select index number. Can be set by the mode
// CameraGetCapability function to get. Please refer to
// Definition of tSdkCameraCapbility in CameraDefine.h.
// In general, 0 means continuous acquisition mode; 1 means
// software trigger mode; 2 hardware trigger mode.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraSetTriggerMode)(
	CameraHandle    hCamera, 
	int             iModeSel
	);

/***************************************************** *****/
// function name: CameraGetTriggerMode
// Function Description: Get the camera's trigger mode.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// The piModeSel pointer returns the index number of the currently selected camera triggering mode.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraGetTriggerMode)(
	CameraHandle    hCamera,
	INT*            piModeSel
	);

/***************************************************** *****/
// function name: CameraSetStrobeMode
// Function Description: Set the STROBE signal on the IO pin. The signal can be flash control, you can also do external mechanical shutter control.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// iMode When STROBE_SYNC_WITH_TRIG_AUTO is synchronized with the trigger, the STROBE signal is automatically generated when the camera is exposed.
// At this point, the effective polarity can be set (CameraSetStrobePolarity).
// When STROBE_SYNC_WITH_TRIG_MANUAL, and the trigger signal synchronization, after the trigger, STROBE delay after the specified time (CameraSetStrobeDelayTime),
// Continue pulse of the specified time (CameraSetStrobePulseWidth)
// Effective polarity can be set (CameraSetStrobePolarity).
// STROBE signal is always high when STROBE_ALWAYS_HIGH, ignoring other settings
// When STROBE_ALWAYS_LOW, STROBE signal is always low, ignore other settings
//
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraSetStrobeMode)(
	CameraHandle    hCamera, 
	INT             iMode
	);

/***************************************************** *****/
// function name: CameraGetStrobeMode
// Function Description: or the mode set by the current STROBE signal.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// piMode pointer, return STROBE_SYNC_WITH_TRIG_AUTO, STROBE_SYNC_WITH_TRIG_MANUAL, STROBE_ALWAYS_HIGH or STROBE_ALWAYS_LOW.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraGetStrobeMode)(
	CameraHandle    hCamera, 
	INT*            piMode
	);

/***************************************************** *****/
// function name: CameraSetStrobeDelayTime
// Function description: When STROBE signal is in STROBE_SYNC_WITH_TRIG, set the relative trigger signal delay time by this function.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// Delay time relative to the trigger signal uDelayTimeUs, in units of us. Can be 0, but not negative.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraSetStrobeDelayTime)(
	CameraHandle    hCamera, 
	UINT            uDelayTimeUs
	);

/***************************************************** *****/
// function name: CameraGetStrobeDelayTime
// Function description: When STROBE signal is in STROBE_SYNC_WITH_TRIG, get the relative trigger signal delay time by this function.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// upDelayTimeUs pointer, return delay time, unit us.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraGetStrobeDelayTime)(
	CameraHandle    hCamera, 
	UINT*           upDelayTimeUs
	);

/***************************************************** *****/
// function name: CameraSetStrobePulseWidth
// Function description: This function sets the pulse width when STROBE signal is in STROBE_SYNC_WITH_TRIG.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// uTimeUs pulse width, in units of time us.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraSetStrobePulseWidth)(
	CameraHandle    hCamera, 
	UINT            uTimeUs
	);

/***************************************************** *****/
// function name: CameraGetStrobePulseWidth
// Function Description: The pulse width is obtained from this function when the STROBE signal is at STROBE_SYNC_WITH_TRIG.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// upTimeUs pointer, return pulse width. The unit is time us.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraGetStrobePulseWidth)(
	CameraHandle    hCamera, 
	UINT*           upTimeUs
	);


/***************************************************** *****/
// function name: CameraSetStrobePolarity
// Function description: This function sets the polarity of its active level when the STROBE signal is in STROBE_SYNC_WITH_TRIG. The default is high active, when the trigger signal comes, STROBE signal is pulled high.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// Polarity STROBE signal polarity, 0 is active low, 1 is active high. The default is active high.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraSetStrobePolarity)(
	CameraHandle    hCamera, 
	INT             iPolarity
	);

/***************************************************** *****/
// function name: CameraGetStrobePolarity
// Function Description: Get the effective polarity of the camera's current STROBE signal. The default is active high.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// ipPolarity pointer returns the current valid polarity of the STROBE signal.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraGetStrobePolarity)(
	CameraHandle    hCamera, 
	INT*            ipPolarity
	);

/***************************************************** *****/
// function name: CameraSetExtTrigSignalType
// Function Description: Set the type of trigger signal outside the camera. Upper edge, lower edge, or high and low level.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// iType external trigger signal type, the return value reference CameraDefine.h
// emExtTrigSignal type definition.

// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraSetExtTrigSignalType)(
	CameraHandle    hCamera, 
	INT             iType
	);

/***************************************************** *****/
// function name: CameraGetExtTrigSignalType
// Function Description: Get the type of the camera's current external trigger signal.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// ipType pointer, return the type of external trigger signal, the return value reference CameraDefine.h
// emExtTrigSignal type definition.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraGetExtTrigSignalType)(
	CameraHandle    hCamera, 
	INT*            ipType
	);

/***************************************************** *****/
// function name: CameraSetExtTrigShutterType
// Function Description: Set the mode of the camera shutter in the external trigger mode, the default is the standard shutter mode.
// The partially rolled shutter CMOS camera supports GRR mode.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// iType external trigger shutter mode. Refer to the emExtTrigShutterMode type in CameraDefine.h.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraSetExtTrigShutterType)(
	CameraHandle    hCamera, 
	INT             iType
	);

/***************************************************** *****/
// function name: CameraSetExtTrigShutterType
// Function Description: Get the external trigger mode, the camera shutter mode, the default is the standard shutter mode.
// The partially rolled shutter CMOS camera supports GRR mode.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// ipType pointer, return the current set of external trigger shutter mode. Return value reference
// emExtTrigShutterMode type in CameraDefine.h.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraGetExtTrigShutterType)(
	CameraHandle    hCamera, 
	INT*            ipType
	);

/***************************************************** *****/
// function name: CameraSetExtTrigDelayTime
// Function Description: Set the delay time of external trigger signal, the default is 0, the unit is microsecond.
// When the set value uDelayTimeUs is not 0, the camera receives the external trigger signal, the delay uDelayTimeUs microseconds before the image capture.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// uDelayTimeUs Delay time, in microseconds, defaults to 0.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraSetExtTrigDelayTime)(
	CameraHandle    hCamera, 
	UINT            uDelayTimeUs
	);

/***************************************************** *****/
// function name: CameraGetExtTrigDelayTime
// Function description: Get the set delay time of external trigger signal, the default is 0, the unit is microsecond.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// UINT * upDelayTimeUs
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraGetExtTrigDelayTime)(
	CameraHandle    hCamera, 
	UINT*           upDelayTimeUs
	);

/***************************************************** *****/
// function name: CameraSetExtTrigJitterTime
// Function description: Set the out-shaking time of the external trigger signal of the camera, only when the external trigger signal mode is selected to be high level or low level,
// debounce time will take effect. The default is 0, the unit is microseconds, the maximum 150 milliseconds
// parameters: hCamera camera handle, obtained by the CameraInit function.
// UINT uTimeUs
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraSetExtTrigJitterTime)(
	CameraHandle    hCamera,
	UINT            uTimeUs
	);

/***************************************************** *****/
// function name: CameraGetExtTrigJitterTime
// Function Description: Get set the camera outside the trigger dithering time, the default is 0. The unit is microseconds.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// UINT * upTimeUs
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraGetExtTrigJitterTime)(
	CameraHandle    hCamera,
	UINT*           upTimeUs
	);

/***************************************************** *****/
// function name: CameraGetExtTrigCapability
// Function Description: Get the attribute mask of the camera triggering
// parameters: hCamera camera handle, obtained by the CameraInit function.
// puCapabilityMask pointer, returns the camera's external trigger mask, mask reference CameraDefine.h
// Macro definition beginning with EXT_TRIG_MASK_.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraGetExtTrigCapability)(
	CameraHandle    hCamera,
	UINT*           puCapabilityMask
	);

/***************************************************** *****/
// function name: CameraGetResolutionForSnap
// Function Description: Get the capture mode resolution select index number.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// pImageResolution pointer, returns the capture mode resolution.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraGetResolutionForSnap)(
	CameraHandle            hCamera,
	tSdkImageResolution*    pImageResolution
	);

/***************************************************** *****/
// function name: CameraSetResolutionForSnap
// Function Description: Set the resolution of the camera's output image in Snapshot mode.
// parameters: hCamera camera handle, obtained by the CameraInit function.
//             pImageResolution if pImageResolution-> iWidth
//             and pImageResolution-> iHeight are both 0,
//             Set to follow the current preview resolution. Grab
//             of the resolution of the image and the current settings
//             same preview resolution.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraSetResolutionForSnap)(
	CameraHandle            hCamera, 
	tSdkImageResolution*    pImageResolution
	);

/***************************************************** *****/
// function name: CameraCustomizeResolution
// Function Description: Open the resolution custom panel, and through the visual way
// to configure a custom resolution.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// pImageCustom pointer, return to the definition of the resolution.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraCustomizeResolution)(
	CameraHandle            hCamera,
	tSdkImageResolution*    pImageCustom
	);

/***************************************************** *****/
// function name: CameraCustomizeReferWin
// Function Description: Opens the reference window custom panel. And through the visual way
// Get a custom window location. Generally use a custom white balance
// and automatic exposure reference window.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// iWinType The purpose of the reference window to be generated. 0, automatic exposure reference window
// 1, white balance reference window.
// hParent handle of the function that called the window. Can be NULL.
// piHOff pointer, back to the upper left corner of the custom window abscissa.
// piVOff pointer, return to the upper left corner of the custom window ordinate.
// piWidth pointer, returns the width of the custom window.
// piHeight pointer, returns the height of the custom window.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraCustomizeReferWin)(
	CameraHandle    hCamera,
	INT             iWinType,
	HWND            hParent, 
	INT*            piHOff,
	INT*            piVOff,
	INT*            piWidth,
	INT*            piHeight
	);

/***************************************************** *****/
// function name: CameraShowSettingPage
// Function Description: Set the camera properties configuration window display status. CameraCreateSettingPage must be called first
// After successful creation of the camera properties configuration window, this function can be called
// display.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// bShow TRUE, display; FALSE, hide.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraShowSettingPage)(
	CameraHandle    hCamera,
	BOOL            bShow
	);

/***************************************************** *****/
// function name: CameraCreateSettingPage
// Function Description: Create the camera's property configuration window. Call this function, SDK internal will
// Help you create a good camera configuration window, eliminating the need for you to re-develop the camera
// Configure the interface time. It is strongly recommended that you use this function to make
// SDK for you to create a good configuration window.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// hParent application main window handle. Can be NULL.
// pWinText string pointer, the title bar displayed in the window.
// pCallbackFunc window message callback function, when the corresponding event occurs,
// The function pointed to by pCallbackFunc will be called,
// For example, pCallbackFunc is used when operations such as switching parameters
// is called back, indicating the type of message at the entry parameter.
// This will make it easy to develop your own interface and the UI we generate
// Synchronize. This parameter can be NULL.
// additional parameter to the pCallbackCtx callback function. Can be NULL. pCallbackCtx
// will be passed in as one of the arguments when pCallbackFunc is called back.
// You can use this parameter to make some flexible judgments.
// uReserved Reserved. Must be set to 0.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraCreateSettingPage)(
	CameraHandle            hCamera,
	HWND                    hParent,
	char*                   pWinText,
	CAMERA_PAGE_MSG_PROC    pCallbackFunc,
	PVOID                   pCallbackCtx,
	UINT                    uReserved
	);

/***************************************************** *****/
// function name: CameraSetActiveSettingSubPage
// Function Description: Set the camera configuration window activation page. There are multiple camera configuration windows
// sub-page structure, the function can be set which sub-page
// is active, displayed at the forefront.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// index The index of the subpage. Reference CameraDefine.h
// The definition of PROP_SHEET_INDEX.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraSetActiveSettingSubPage)(
	CameraHandle    hCamera,
	INT             index
	);

/***************************************************** *****/
// function name: CameraSpecialControl
// Function Description: Some special camera interface called, secondary development generally do not need
// transfer.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// dwCtrlCode control code.
// dwParam control sub-code, different dwCtrlCode, meaning different.
// lpData Additional parameters. Different dwCtrlCode, meaning different.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraSpecialControl)(
	CameraHandle    hCamera, 
	DWORD           dwCtrlCode,
	DWORD           dwParam,
	LPVOID          lpData
	);

/***************************************************** *****/
// function name: CameraGetFrameStatistic
// Function Description: Get the statistics of the frame rate received by the camera, including the error frame and frame loss.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// psFrameStatistic pointer, return statistics.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraGetFrameStatistic)(
	CameraHandle            hCamera, 
	tSdkFrameStatistic*     psFrameStatistic
	);

/***************************************************** *****/
// function name: CameraSetNoiseFilter
// Function Description: Set the image noise reduction module enable status.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// bEnable TRUE, enabled; FALSE, disabled.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraSetNoiseFilter)(
	CameraHandle    hCamera,
	BOOL            bEnable
	);

/***************************************************** *****/
// function name: CameraGetNoiseFilterState
// Function Description: Get the image noise reduction module enabled.
// parameters: hCamera camera handle, obtained by the CameraInit function.
//* pEnable pointer, return status. TRUE, to be enabled.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraGetNoiseFilterState)(
	CameraHandle    hCamera,
	BOOL*           pEnable
	);


/***************************************************** *****/
// function name: CameraRstTimeStamp
// Function Description: Reset the time stamp of the image acquisition, starting from 0.
// Parameters: CameraHandle hCamera
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef    CameraSdkStatus (__stdcall *_CameraRstTimeStamp)(
	CameraHandle    hCamera
	);

/***************************************************** *****/
// function name: CameraGetCapabilityEx
// Function Description: Get the camera's characterization structure. The structure contains the camera
// Range of various parameters that can be set. Determine the parameters of the relevant function
// return, can also be used to dynamically create the camera's configuration interface.
// Parameters: sDeviceModel The camera model, obtained from the scan list
// The pCameraInfo pointer returns the structure of the camera's characterization.
// tSdkCameraCapbility is defined in CameraDefine.h.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef CameraSdkStatus (__stdcall *_CameraGetCapabilityEx)(
	char*                   sDeviceModel, 
	tSdkCameraCapbility*    pCameraInfo,
	PVOID                   hCameraHandle
	);

/***************************************************** *****/
// function name: CameraSaveUserData
// Function Description: Save the user-defined data to the camera's non-volatile memory.
// The maximum length of user data area that each camera model may support is different.
// This length information can be obtained from the device's characterization.
// parameters: hCamera camera handle, obtained by the CameraInit function.
//             uStartAddr starting address, starting from 0.
//             pbData data buffer pointer
//             ilen write the length of the data, ilen + uStartAddr must
//                              less than the maximum length of the user area
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef CameraSdkStatus (__stdcall *_CameraSaveUserData)(
	CameraHandle    hCamera,
	UINT            uStartAddr,
	BYTE            *pbData,
	int             ilen
	);

/***************************************************** *****/
// function name: CameraLoadUserData
// Function Description: Read user-defined data from the camera's non-volatile memory.
// The maximum length of user data area that each camera model may support is different.
// This length information can be obtained from the device's characterization.
// parameters: hCamera camera handle, obtained by the CameraInit function.
//              uStartAddr starting address, starting from 0.
//              pbData data buffer pointer, to read the data back.
//              ilen read the length of the data, ilen + uStartAddr must
//              less than the maximum length of the user area
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef CameraSdkStatus (__stdcall *_CameraLoadUserData)(
	CameraHandle    hCamera,
	UINT            uStartAddr,
	BYTE            *pbData,
	int             ilen
	);



/***************************************************** *****/
// function name: CameraGetFriendlyName
// Function Description: Read user-defined device nickname.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// pName pointer, return to the end of the string 0,
// The device nickname does not exceed 32 bytes, so the pointer
// point to the buffer must be greater than or equal to 32 bytes of space.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef CameraSdkStatus (__stdcall *_CameraGetFriendlyName)(
	CameraHandle  hCamera,
	char*     pName
	);


/***************************************************** *****/
// function name: CameraSetFriendlyName
// Function Description: Set user-defined device nickname.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// pName pointer, a string ending in 0,
// The device nickname does not exceed 32 bytes, so the pointer
// point to the string must be less than or equal to 32 bytes of space.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef CameraSdkStatus (__stdcall *_CameraSetFriendlyName)(
	CameraHandle  hCamera,
	char*       pName
	);


/***************************************************** *****/
// function name: CameraSdkGetVersionString
// Function Description:
// Parameters: pVersionString pointer, returns the SDK version string.
// The pointer to the buffer size must be greater than
// 32 bytes
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef CameraSdkStatus (__stdcall *_CameraSdkGetVersionString)(
	char*       pVersionString
	);

/***************************************************** *****/
// function name: CameraCheckFwUpdate
// Function Description: Check the firmware version, whether you need to upgrade.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// pNeedUpdate pointer, return firmware detection status, TRUE that needs to be updated
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef CameraSdkStatus (__stdcall *_CameraCheckFwUpdate)(
	CameraHandle  hCamera,
	BOOL*     pNeedUpdate
	);

/***************************************************** *****/
// function name: CameraGetFirmwareVision
// Function Description: Get the firmware version of the string
// parameters: hCamera camera handle, obtained by the CameraInit function.
// pVersion must point to a buffer greater than 32 bytes,
// Returns the firmware version string.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef CameraSdkStatus (__stdcall *_CameraGetFirmwareVision)(
	CameraHandle  hCamera,
	char*     pVersion
	);

/***************************************************** *****/
// function name: CameraGetEnumInfo
// Function Description: Get the enumeration information of the specified device
// parameters: hCamera camera handle, obtained by the CameraInit function.
// pCameraInfo pointer, return the device enumeration information.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef CameraSdkStatus (__stdcall *_CameraGetEnumInfo)(
	CameraHandle    hCamera,
	tSdkCameraDevInfo*  pCameraInfo
	);

/***************************************************** *****/
// function name: CameraGetInerfaceVersion
// Function Description: Get the specified device interface version
// parameters: hCamera camera handle, obtained by the CameraInit function.
// pVersion points to a buffer greater than 32 bytes and returns the interface version string.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef CameraSdkStatus (__stdcall *_CameraGetInerfaceVersion)(
	CameraHandle    hCamera,
	char*       pVersion
	);

/***************************************************** *****/
// function name: CameraSetIOState
// Function Description: Set the level of the specified IO state, IO for the output IO, camera
// Reserved The number of programmable output IO by tSdkCameraCapbility
// iOutputIoCounts decision.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// iOutputIOIndex The index number of IO, starting from 0.
// uState The state to set, 1 is high and 0 is low
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef CameraSdkStatus (__stdcall *_CameraSetIOState)(
	CameraHandle    hCamera,
	INT         iOutputIOIndex,
	UINT        uState
	);

/***************************************************** *****/
// function name: CameraGetIOState
// Function Description: Set the level of the specified IO state, IO input type IO, the camera
// Reserved The number of programmable output IO by tSdkCameraCapbility
// iInputIoCounts decision.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// iInputIOIndex The index number of IO, starting from 0.
// puState pointer returns IO state, 1 is high, 0 is low
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef CameraSdkStatus (__stdcall *_CameraGetIOState)(
	CameraHandle      hCamera,
	INT               iInputIOIndex,
	UINT*             puState
	);

/***************************************************** *****/
// function name: CameraSetInPutIOMode
// Function Description: Set the input IO mode, the camera
// Reserved The number of programmable output IO by tSdkCameraCapbility
// iInputIoCounts decision.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// iInputIOIndex The index number of IO, starting from 0.
// iMode IO mode, reference emCameraGPIOMode in CameraDefine.h
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef CameraSdkStatus (__stdcall *_CameraSetInPutIOMode)(
	CameraHandle    hCamera,
	INT         iInputIOIndex,
	INT			iMode
	);

/***************************************************** *****/
// function name: CameraSetOutPutIOMode
// Function Description: Set the mode of output IO, camera
// Reserved The number of programmable output IO by tSdkCameraCapbility
// iOutputIoCounts decision.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// iOutputIOIndex The index number of IO, starting from 0.
// iMode IO mode, reference emCameraGPIOMode in CameraDefine.h
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef CameraSdkStatus (__stdcall *_CameraSetOutPutIOMode)(
	CameraHandle    hCamera,
	INT         iOutputIOIndex,
	INT			iMode
	);

/***************************************************** *****/
// function name: CameraSetOutPutPWM
// Function Description: Set the parameter of PWM type output, camera
// Reserved The number of programmable output IO by tSdkCameraCapbility
// iOutputIoCounts decision.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// iOutputIOIndex The index number of IO, starting from 0.
// iCycle PWM period, unit (us)
// uDuty occupancy ratio, the value of 1% to 99%
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef CameraSdkStatus (__stdcall *_CameraSetOutPutPWM)(
	CameraHandle    hCamera,
	INT         iOutputIOIndex,
	UINT		iCycle,
	UINT		uDuty
	);

/***************************************************** *****/
// function name: CameraSetBayerDecAlgorithm
// Function Description: Set Bayer data to color algorithm.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// iIspProcessor Select the object that executes the algorithm, refer to CameraDefine.h
// emSdkIspProcessor definition
// iAlgorithmSel The algorithm number to select. Starting from 0, the maximum value is given by tSdkCameraCapbility
// iBayerDecAlmSwDesc and iBayerDecAlmHdDesc decisions.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef CameraSdkStatus (__stdcall *_CameraSetBayerDecAlgorithm)(
	CameraHandle    hCamera,
	INT             iIspProcessor,
	INT             iAlgorithmSel
	);

/***************************************************** *****/
// function name: CameraGetBayerDecAlgorithm
// Function Description: The algorithm for getting Bayer data to color.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// iIspProcessor Select the object that executes the algorithm, refer to CameraDefine.h
// emSdkIspProcessor definition
// piAlgorithmSel returns the currently selected algorithm number. Starting from 0, the maximum value is given by tSdkCameraCapbility
// iBayerDecAlmSwDesc and iBayerDecAlmHdDesc decisions.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef CameraSdkStatus (__stdcall *_CameraGetBayerDecAlgorithm)(
	CameraHandle    hCamera,
	INT             iIspProcessor,
	INT*            piAlgorithmSel
	);

/***************************************************** *****/
// function name: CameraSetBlackLevel
// Function Description: Set the black level of image reference, the default value is 0
// parameters: hCamera camera handle, obtained by the CameraInit function.
// iBlackLevel The level to be set. The range is 0 to 255.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef CameraSdkStatus (__stdcall *_CameraSetBlackLevel)
	(
	CameraHandle    hCamera,
	INT             iBlackLevel
	);

/***************************************************** *****/
// function name: CameraGetBlackLevel
// Function Description: get the black level of the image benchmark, the default value is 0
// parameters: hCamera camera handle, obtained by the CameraInit function.
// piBlackLevel returns the current black level value. The range is 0 to 255.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef CameraSdkStatus (__stdcall *_CameraGetBlackLevel)
	(
	CameraHandle    hCamera,
	INT*            piBlackLevel
	);


/***************************************************** *****/
// function name: CameraSetWhiteLevel
// Function Description: Set the white level of the image. The default value is 255
// parameters: hCamera camera handle, obtained by the CameraInit function.
// iWhiteLevel The level to be set. The range is 0 to 255.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef CameraSdkStatus (__stdcall *_CameraSetWhiteLevel)
	(
	CameraHandle    hCamera,
	INT             iWhiteLevel
	);



/***************************************************** *****/
// function name: CameraGetWhiteLevel
// Function Description: Gets the white level of the image. The default value is 255
// parameters: hCamera camera handle, obtained by the CameraInit function.
// piWhiteLevel returns the current white level value. The range is 0 to 255.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef CameraSdkStatus (__stdcall *_CameraGetWhiteLevel)
	(
	CameraHandle    hCamera,
	INT*            piWhiteLevel
	);


/***************************************************** *****/
// function name: CameraSetIspOutFormat
// Function Description: Set the output format of image processing of CameraImageProcess function, support
// CAMERA_MEDIA_TYPE_MONO8 and CAMERA_MEDIA_TYPE_RGB8 and CAMERA_MEDIA_TYPE_RGBA8
// and CAMERA_MEDIA_TYPE_BGR8, CAMERA_MEDIA_TYPE_BGRA8
// (defined in CameraDefine.h) 5 types corresponding to 8-bit grayscale image and 24RGB, 32-bit RGB, 24-bit BGR, 32-bit BGR color image.
// The default output is CAMERA_MEDIA_TYPE_BGR8 format.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// uFormat to format. CAMERA_MEDIA_TYPE_MONO8 or CAMERA_MEDIA_TYPE_RGB8, CAMERA_MEDIA_TYPE_RGBA8, CAMERA_MEDIA_TYPE_BGR8, CAMERA_MEDIA_TYPE_BGRA8
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef CameraSdkStatus (__stdcall *_CameraSetIspOutFormat)
	(
	CameraHandle    hCamera,
	UINT            uFormat
	);

/***************************************************** *****/
// function name: CameraGetIspOutFormat
// Function Description: Get CameraGetImageBuffer function image output format, support
// CAMERA_MEDIA_TYPE_MONO8 and CAMERA_MEDIA_TYPE_RGB8 and CAMERA_MEDIA_TYPE_RGBA8
// and CAMERA_MEDIA_TYPE_BGR8, CAMERA_MEDIA_TYPE_BGRA8
// (defined in CameraDefine.h) 5 types corresponding to 8-bit grayscale image and 24RGB, 32-bit RGB, 24-bit BGR, 32-bit BGR color image.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// puFormat returns the format of the current setting. ?C AERA_MEDIA_TYPE_MONO8 or CAMERA_MEDIA_TYPE_RGB8, CAMERA_MEDIA_TYPE_RGBA8, CAMERA_MEDIA_TYPE_BGR8, CAMERA_MEDIA_TYPE_BGRA8
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef CameraSdkStatus (__stdcall *_CameraGetIspOutFormat)
	(
	CameraHandle    hCamera,
	UINT*           puFormat
	);

/***************************************************** *****/
// function name: CameraGetErrorString
// Function Description: Obtain the description string corresponding to the error code
// Parameters: iStatusCode error code. (Defined in CameraStatus.h)
// Return value: When successful, return the first address of the string corresponding to the error code.
// otherwise return NULL.
/***************************************************** *****/

typedef char* (__stdcall *_CameraGetErrorString)(
	CameraSdkStatus     iStatusCode
	);

/***************************************************** *****/
// function name: CameraGetImageBufferEx2
// Function Description: Get a frame of image data. The image obtained by this interface is processed RGB format. After the function is called,
// Do not need to call CameraReleaseImageBuffer release, do not call free release of such functions
// to free the image data buffer returned by this function.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// pImageData Buffer to receive image data, the size must match the format specified by uOutFormat, otherwise the data will overflow
// piWidth plastic pointer, returns the width of the image
// piHeight plastic pointer, returns the height of the image
// wTimes capture image timeout. Milliseconds. in
// wTimes time has not yet been the image, then the function
// will return the timeout message.
// Return Value: Returns the first address of the RGB data buffer on success;
// otherwise 0
/***************************************************** *****/
typedef CameraSdkStatus (__stdcall *_CameraGetImageBufferEx2)(
	CameraHandle    hCamera, 
	BYTE*           pImageData,
	UINT            uOutFormat,
	int*            piWidth,
	int*            piHeight,
	UINT            wTimes
	);

/***************************************************** *****/
// function name: CameraGetImageBufferEx3
// Function Description: Get a frame of image data. The image obtained by this interface is processed RGB format. After the function is called,
// Do not need to call CameraReleaseImageBuffer release.
// uOutFormat 0: 8 BIT gray 1: rgb24 2: rgba32 3: bgr24 4: bgra32
// parameters: hCamera camera handle, obtained by the CameraInit function.
// pImageData Buffer to receive image data, the size must match the format specified by uOutFormat, otherwise the data will overflow
// piWidth plastic pointer, returns the width of the image
// piHeight plastic pointer, returns the height of the image
// puTimeStamp unsigned plastic, returns the image timestamp
// UINT wTimes The time-out period for grabbing the image. Milliseconds. in
// The image has not been acquired within the wTimes time, the function returns the time-out information.
// Return Value: Returns the first address of the RGB data buffer on success;
// otherwise 0
/***************************************************** *****/
typedef CameraSdkStatus (__stdcall *_CameraGetImageBufferEx3)(
	CameraHandle hCamera, 
	BYTE*pImageData,
	UINT uOutFormat,
	int *piWidth,
	int *piHeight,
	UINT* puTimeStamp,
	UINT wTimes
	);

/***************************************************** *****/
// function name: CameraGetCapabilityEx2
// Function Description: Get some features of the camera.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// pMaxWidth returns the width of the camera's maximum resolution
// pMaxHeight returns the height of the camera's maximum resolution
// pbColorCamera Returns whether the camera is a color camera. 1 for color camera, 0 for black and white camera
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef CameraSdkStatus (__stdcall *_CameraGetCapabilityEx2)(
	CameraHandle    hCamera,
	int*            pMaxWidth,
	int*            pMaxHeight,
	int*            pbColorCamera
	);


/***************************************************** *****/
// function name: CameraReConnect
// Function Description: Reconnect the device for USB device accidental disconnection after reconnection
// parameters: hCamera camera handle, obtained by the CameraInit function.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef CameraSdkStatus (__stdcall *_CameraReConnect)(
	CameraHandle    hCamera
	);


/***************************************************** *****/
// function name: CameraConnectTest
// Function Description: Test the camera's connection status, used to detect whether the camera dropped
// parameters: hCamera camera handle, obtained by the CameraInit function.
// Return Value: Upon successful, return CAMERA_STATUS_SUCCESS (0), said the camera connection status is normal;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef CameraSdkStatus (__stdcall *_CameraConnectTest)(
	CameraHandle    hCamera
	);

/***************************************************** *****/
// function name: CameraSetLedEnable
// Function Description: Set the camera's LED enable status, without the LED model, this function returns an error code that does not support.
// parameters: hCamera camera handle, obtained by the CameraInit function.
//             index The index number of the LED light, starting from 0. This parameter is 0 if there is only one LED with controllable brightness.
//             enable enable status
// Return Value: Upon successful, return CAMERA_STATUS_SUCCESS (0), said the camera connection status is normal;
// otherwise it returns non-zero value, refer to the definition of error code in CameraStatus.h.
/***************************************************** *****/
typedef CameraSdkStatus (__stdcall *_CameraSetLedEnable)(
	CameraHandle    hCamera,
	int             index,
	BOOL            enable
	);

/***************************************************** *****/
// function name: CameraGetLedEnable
// Function Description: Get the LED status of the camera, without the LED model, this function returns an error code that does not support.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// index The index number of the LED light, starting from 0. This parameter is 0 if there is only one LED with controllable brightness.
// enable pointer to return the LED enable status
// Return Value: Upon successful, return CAMERA_STATUS_SUCCESS (0), said the camera connection status is normal;
// otherwise it returns non-zero value, refer to the definition of error code in CameraStatus.h.
/***************************************************** *****/
typedef CameraSdkStatus (__stdcall *_CameraGetLedEnable)(
	CameraHandle    hCamera,
	int             index,
	BOOL*           enable
	);

/***************************************************** *****/
// function name: CameraSetLedOnOff
// Function Description: Set the camera's LED switch status, without LED model, this function returns an error code that does not support.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// index The index number of the LED light, starting from 0. This parameter is 0 if there is only one LED with controllable brightness.
// onoff LED switch status
// Return Value: Upon successful, return CAMERA_STATUS_SUCCESS (0), said the camera connection status is normal;
// otherwise it returns non-zero value, refer to the definition of error code in CameraStatus.h.
/***************************************************** *****/
typedef CameraSdkStatus (__stdcall *_CameraSetLedOnOff)(
	CameraHandle    hCamera,
	int             index,
	BOOL            onoff
	);

/***************************************************** *****/
// function name: CameraGetLedOnOff
// Function Description: Get the camera's LED on / off status, without LED model, this function returns an error code that does not support it.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// index The index number of the LED light, starting from 0. This parameter is 0 if there is only one LED with controllable brightness.
// onoff pointer, return LED switch status
// Return Value: Upon successful, return CAMERA_STATUS_SUCCESS (0), said the camera connection status is normal;
// otherwise it returns non-zero value, refer to the definition of error code in CameraStatus.h.
/***************************************************** *****/
typedef CameraSdkStatus (__stdcall *_CameraGetLedOnOff)(
	CameraHandle    hCamera,
	int             index,
	BOOL*           onoff
	);

/***************************************************** *****/
// function name: CameraSetLedDuration
// Function Description: Set the LED duration of the camera, the model without LED, this function returns an error code, which means it is not supported.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// index The index number of the LED light, starting from 0. This parameter is 0 if there is only one LED with controllable brightness.
// duration of the LED in milliseconds
// Return Value: Upon successful, return CAMERA_STATUS_SUCCESS (0), said the camera connection status is normal;
// otherwise it returns non-zero value, refer to the definition of error code in CameraStatus.h.
/***************************************************** *****/
typedef CameraSdkStatus (__stdcall *_CameraSetLedDuration)(
	CameraHandle    hCamera,
	int             index,
	UINT            duration
	);

/***************************************************** *****/
// function name: CameraGetLedDuration
// Function Description: Obtain the LED duration of the camera, without the LED model, this function returns an error code that does not support.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// index The index number of the LED light, starting from 0. This parameter is 0 if there is only one LED with controllable brightness.
// duration pointer to return LED duration in milliseconds
// Return Value: Upon successful, return CAMERA_STATUS_SUCCESS (0), said the camera connection status is normal;
// otherwise it returns non-zero value, refer to the definition of error code in CameraStatus.h.
/***************************************************** *****/
typedef CameraSdkStatus (__stdcall *_CameraGetLedDuration)(
	CameraHandle    hCamera,
	int             index,
	UINT*           duration
	);

/***************************************************** *****/
// function name: CameraSetLedBrightness
// Function Description: Set the camera's LED brightness, without the LED model, this function returns the error code, it does not support.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// index The index number of the LED light, starting from 0. This parameter is 0 if there is only one LED with controllable brightness.
// uBrightness LED brightness value, range 0 to 255. 0 means off, 255 is the brightest.
// Return Value: Upon successful, return CAMERA_STATUS_SUCCESS (0), said the camera connection status is normal;
// otherwise it returns non-zero value, refer to the definition of error code in CameraStatus.h.
/***************************************************** *****/
typedef CameraSdkStatus (__stdcall *_CameraSetLedBrightness)(
	CameraHandle    hCamera,
	int             index,
	UINT            uLightless
	);

/***************************************************** *****/
// function name: CameraGetLedBrightness
// Function Description: Get the camera's LED brightness, without the LED model, this function returns the error code, it does not support.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// index The index number of the LED light, starting from 0. This parameter is 0 if there is only one LED with controllable brightness.
// uBrightness pointer, returns the LED brightness value, ranging from 0 to 255. 0 means off, 255 is the brightest.
// Return Value: Upon successful, return CAMERA_STATUS_SUCCESS (0), said the camera connection status is normal;
// otherwise it returns non-zero value, refer to the definition of error code in CameraStatus.h.
/***************************************************** *****/
typedef CameraSdkStatus (__stdcall *_CameraGetLedBrightness)(
	CameraHandle    hCamera,
	int             index,
	UINT*           uLightless
	);


/***************************************************** *****/
// function name: CameraEnableTransferRoi
// Function Description: Enable or disable the camera's multi-zone transfer function, without this model, this function returns an error code that does not support.
// This function is mainly used in the camera side will capture the entire picture segmentation, only the transfer of designated multiple regions, in order to improve the transmission frame rate.
// When multiple areas are transferred to the PC, they will be automatically spliced into the whole picture. The parts that have not been transferred will be filled with black.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// uEnableMask Area enable status mask. The corresponding bit is 1 to enable. 0 is prohibited. Currently, the SDK supports 4 editable areas and the index range is 0 to 3, that is, bit0, bit1, bit2 and bit3 control the enabled state of the four areas.
// Return Value: Upon successful, return CAMERA_STATUS_SUCCESS (0), said the camera connection status is normal;
// This function returns CAMERA_STATUS_NOT_SUPPORTED (-4) for models that do not support multisite ROI transport that does not support
// Other non-zero value, refer to the definition of error code in CameraStatus.h.
/***************************************************** *****/
typedef CameraSdkStatus (__stdcall *_CameraEnableTransferRoi)(
	CameraHandle    hCamera,
	UINT            uEnableMask
	);


/***************************************************** *****/
// function name: CameraSetTransferRoi
// Function Description: Set the cropping area transmitted by the camera. On the camera side, after the image is captured from the sensor, it will be cropped to the specified area for transmission. This function returns an error code indicating that it is not supported.
// parameters: hCamera camera handle, obtained by the CameraInit function.
//             index number of index ROI area, starting from 0.
//             X1, Y1 coordinates of the upper left corner of the ROI area
//             X2, Y2 The coordinates of the upper right corner of the ROI area
// Return Value: Upon successful, return CAMERA_STATUS_SUCCESS (0), said the camera connection status is normal;
// This function returns CAMERA_STATUS_NOT_SUPPORTED (-4) for models that do not support multisite ROI transport that does not support
// Other non-zero value, refer to the definition of error code in CameraStatus.h.
/***************************************************** *****/
typedef CameraSdkStatus (__stdcall *_CameraSetTransferRoi)(
	CameraHandle    hCamera,
	int             index,
	UINT            X1,
	UINT            Y1,
	UINT            X2,
	UINT            Y2
	);


/***************************************************** *****/
// function name: CameraGetTransferRoi
// Function Description: Set the cropping area transmitted by the camera. On the camera side, after the image is captured from the sensor, it will be cropped to the specified area for transmission. This function returns an error code indicating that it is not supported.
// parameters: hCamera camera handle, obtained by the CameraInit function.
//             index number of index ROI area, starting from 0.
//             pX1, pY1 The coordinates of the upper left corner of the ROI area
//             pX2, pY2 The upper-right corner of the ROI area
// Return Value: Upon successful, return CAMERA_STATUS_SUCCESS (0), said the camera connection status is normal;
// This function returns CAMERA_STATUS_NOT_SUPPORTED (-4) for models that do not support multisite ROI transport that does not support
// Other non-zero value, refer to the definition of error code in CameraStatus.h.
/***************************************************** *****/
typedef CameraSdkStatus (__stdcall *_CameraGetTransferRoi)(
	CameraHandle    hCamera,
	int             index,
	UINT*           pX1,
	UINT*           pY1,
	UINT*           pX2,
	UINT*           pY2
	);

/***************************************************** *****/
// function name: CameraAlignMalloc
// Function Description: Apply for a period of aligned memory space. Function and malloc similar, but
// The returned memory is aligned with the number of bytes specified by align.
// Parameters: size space size.
//             align align the number of bytes.
// Return value: on success, returns a non-zero value, said the memory address. Failed to return NULL.
/***************************************************** *****/

typedef BYTE* (__stdcall *_CameraAlignMalloc)(
	int             size,
	int             align
	);

/***************************************************** *****/
// function name: CameraAlignFree
// Function Description: Free up memory space requested by CameraAlignMalloc function.
// Parameters: membuffer The first memory address returned by CameraAlignMalloc.
// Return value: None.
/***************************************************** *****/

typedef void (__stdcall *_CameraAlignFree)(
	BYTE*           membuffer
	);


/***************************************************** *****/
// function name: CameraSetAutoConnect
// Function Description: Set to automatically reconnect
// parameters: hCamera camera handle, obtained by the CameraInit function.
// bEnable Enables the camera to reconnect. When the bit TRUE, the SDK automatically detects whether the camera is disconnected or not, disconnects itself and reconnects.
// Return Value: Upon successful, return CAMERA_STATUS_SUCCESS (0), said the camera connection status is normal;
// This function returns CAMERA_STATUS_NOT_SUPPORTED (-4) for unsupported models that does not support
// Other non-zero value, refer to the definition of error code in CameraStatus.h.
/***************************************************** *****/
typedef CameraSdkStatus (__stdcall *_CameraSetAutoConnect)(
	CameraHandle hCamera,
	BOOL bEnable
	);

/***************************************************** *****/
// function name: CameraGetAutoConnect
// Function Description: Get auto-reconnect enabled
// parameters: hCamera camera handle, obtained by the CameraInit function.
// pbEnable returns camera reconnect enabled
// Return Value: Upon successful, return CAMERA_STATUS_SUCCESS (0), said the camera connection status is normal;
// This function returns CAMERA_STATUS_NOT_SUPPORTED (-4) for unsupported models that does not support
// Other non-zero value, refer to the definition of error code in CameraStatus.h.
/***************************************************** *****/
typedef CameraSdkStatus (__stdcall *_CameraGetAutoConnect)(CameraHandle hCamera,BOOL *pbEnable);

/***************************************************** *****/
// function name: CameraGetReConnectCounts
// Function Description: Get the number of times the camera automatically reconnects if CameraSetAutoConnect enables the camera to reconnect automatically. The default is enabled.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// puCounts returns the number of automatic reconnection dropped calls
// Return Value: Upon successful, return CAMERA_STATUS_SUCCESS (0), said the camera connection status is normal;
// This function returns CAMERA_STATUS_NOT_SUPPORTED (-4) for unsupported models that does not support
// Other non-zero value, refer to the definition of error code in CameraStatus.h.
/***************************************************** *****/
typedef CameraSdkStatus (__stdcall *_CameraGetReConnectCounts)(
	CameraHandle hCamera,
	UINT* puCounts
	);

/***************************************************** *****/
// function name: CameraSetSingleGrabMode
// Function Description: Enable single-frame capture mode
// parameters: hCamera camera handle, obtained by the CameraInit function.
// bEnable Enables single-frame fetch mode
// Return Value: Upon successful, return CAMERA_STATUS_SUCCESS (0), said the camera connection status is normal;
// This function returns CAMERA_STATUS_NOT_SUPPORTED (-4) for unsupported models that does not support
// Other non-zero value, refer to the definition of error code in CameraStatus.h.
/***************************************************** *****/
typedef CameraSdkStatus (__stdcall *_CameraSetSingleGrabMode)(CameraHandle hCamera, BOOL bEnable);

/***************************************************** *****/
// function name: CameraGetSingleGrabMode
// Function Description: Get the camera's single-frame capture mode
// parameters: hCamera camera handle, obtained by the CameraInit function.
// pbEnable returns the camera's single frame grabbing mode
// Return Value: Upon successful, return CAMERA_STATUS_SUCCESS (0), said the camera connection status is normal;
// This function returns CAMERA_STATUS_NOT_SUPPORTED (-4) for unsupported models that does not support
// Other non-zero value, refer to the definition of error code in CameraStatus.h.
/***************************************************** *****/
typedef CameraSdkStatus (__stdcall *_CameraGetSingleGrabMode)(CameraHandle hCamera, BOOL* pbEnable);

/***************************************************** *****/
// function name: CameraRestartGrab
// Function Description: When the camera is in single-frame capture mode, the SDK will enter the pause status every time a frame is successfully fetched. Calling this function causes the SDK to exit the paused state and begin to capture the next frame
// parameters: hCamera camera handle, obtained by the CameraInit function.
// Return Value: Upon successful, return CAMERA_STATUS_SUCCESS (0), said the camera connection status is normal;
// This function returns CAMERA_STATUS_NOT_SUPPORTED (-4) for unsupported models that does not support
// Other non-zero value, refer to the definition of error code in CameraStatus.h.
/***************************************************** *****/
typedef CameraSdkStatus (__stdcall *_CameraRestartGrab)(CameraHandle hCamera);

/***************************************************** *****/
// function name: CameraDrawText
// Function Description: draw text in the input image data
// Parameters: pRgbBuffer image data buffer
// The header of the pFrInfo image
// pFontFileName font file name
// FontWidth font width
// FontHeight font height
// pText The text to be output
// (Left, Top, Width, Height) The text output rectangle
// TextColor text color RGB
// uFlags Output flags, as defined in emCameraDrawTextFlags
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef CameraSdkStatus (__stdcall *_CameraDrawText)(
	BYTE*           pRgbBuffer,
	tSdkFrameHead*  pFrInfo,
	char const*		pFontFileName, 
	UINT			FontWidth,
	UINT			FontHeight,
	char const*		pText, 
	INT				Left,
	INT				Top,
	UINT			Width,
	UINT			Height,
	UINT			TextColor,
	UINT			uFlags
	);

/***************************************************** *****/
// function name: CameraGigeGetIp
// Function Description: Get the IP address of GIGE camera
// Parameters: pCameraInfo The device description of the camera, available from the CameraEnumerateDevice function.
// CamIp camera IP (Note: you must ensure that the incoming buffer is greater than or equal to 16 bytes)
// CamMask camera subnet mask (Note: you must ensure that the incoming buffer is greater than or equal to 16 bytes)
// CamGateWay Camera Gateway (Note: The incoming buffer must be guaranteed to be greater than or equal to 16 bytes)
// EtIp network card IP (Note: you must ensure that the incoming buffer is greater than or equal to 16 bytes)
// EtMask NIC subnet mask (Note: you must ensure that the incoming buffer is greater than or equal to 16 bytes)
// EtGateWay network card gateway (Note: you must ensure that the incoming buffer is greater than or equal to 16 bytes)
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef CameraSdkStatus (__stdcall *_CameraGigeGetIp)(
	tSdkCameraDevInfo* pCameraInfo,
	char* CamIp,
	char* CamMask,
	char* CamGateWay,
	char* EtIp,
	char* EtMask,
	char* EtGateWay
	);

/***************************************************** *****/
// function name: CameraGigeSetIp
// Function Description: Set the IP address of GIGE camera
// Parameters: pCameraInfo The device description of the camera, available from the CameraEnumerateDevice function.
// Ip camera IP (eg: 192.168.1.100)
// SubMask camera subnet mask (eg: 255.255.255.0)
// GateWay camera gateway (eg: 192.168.1.1)
// bPersistent TRUE: set the camera to a fixed IP, FALSE: set the camera automatically assigned IP (ignoring the parameters Ip, SubMask, GateWay)
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef CameraSdkStatus (__stdcall *_CameraGigeSetIp)(
	tSdkCameraDevInfo* pCameraInfo,
	char const* Ip,
	char const* SubMask,
	char const* GateWay,
	BOOL bPersistent
	);

/***************************************************** *****/
// function name: CameraGigeGetMac
// Function Description: Get the GIGE camera's MAC address
// Parameters: pCameraInfo The device description of the camera, available from the CameraEnumerateDevice function.
// CamMac camera MAC (Note: you must ensure that the incoming buffer is greater than or equal to 18 bytes)
// EtMac NIC MAC (Note: you must ensure that the incoming buffer is greater than or equal to 18 bytes)
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef CameraSdkStatus (__stdcall *_CameraGigeGetMac)(
	tSdkCameraDevInfo* pCameraInfo,
	char* CamMac,
	char* EtMac
	);

/***************************************************** *****/
// function name: CameraEnableFastResponse
// Function Description: Enable quick response
// parameters: hCamera camera handle, obtained by the CameraInit function.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef CameraSdkStatus (__stdcall *_CameraEnableFastResponse)(
	CameraHandle hCamera
	);

/***************************************************** *****/
// function name: CameraSetCorrectDeadPixel
// Function Description: Enable dead pixel correction
// parameters: hCamera camera handle, obtained by the CameraInit function.
// bEnable TRUE: enable dead pixel correction FALSE: disable dead pixel correction
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef CameraSdkStatus (__stdcall *_CameraSetCorrectDeadPixel)(
	CameraHandle hCamera,
	BOOL bEnable
	);

/***************************************************** *****/
// function name: CameraGetCorrectDeadPixel
// Function Description: Get dead pixel correction enabled state
// parameters: hCamera camera handle, obtained by the CameraInit function.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef CameraSdkStatus (__stdcall *_CameraGetCorrectDeadPixel)(
	CameraHandle hCamera,
	BOOL* pbEnable
	);

/***************************************************** *****/
// function name: CameraFlatFieldingCorrectSetEnable
// Function Description: Enables flat field correction
// parameters: hCamera camera handle, obtained by the CameraInit function.
// bEnable TRUE: Enable leveling correction FALSE: Turn off leveling correction
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef CameraSdkStatus (__stdcall *_CameraFlatFieldingCorrectSetEnable)(
	CameraHandle hCamera,
	BOOL bEnable
	);

/***************************************************** *****/
// function name: CameraFlatFieldingCorrectGetEnable
// Function Description: Get leveling enable status
// parameters: hCamera camera handle, obtained by the CameraInit function.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef CameraSdkStatus (__stdcall *_CameraFlatFieldingCorrectGetEnable)(
	CameraHandle hCamera,
	BOOL* pbEnable
	);

/***************************************************** *****/
// function name: CameraFlatFieldingCorrectSetParameter
// Function Description: Set the leveling correction parameters
// parameters: hCamera camera handle, obtained by the CameraInit function.
// dark field picture pDarkFieldingImage
// pDarkFieldingFrInfo dark field picture information
// pLightFieldingImage Bright field image
// pLightFieldingFrInfo Bright field image information
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef CameraSdkStatus (__stdcall *_CameraFlatFieldingCorrectSetParameter)(
	CameraHandle hCamera,
	BYTE const* pDarkFieldingImage,
	tSdkFrameHead const* pDarkFieldingFrInfo,
	BYTE const* pLightFieldingImage,
	tSdkFrameHead const* pLightFieldingFrInfo
	);

/***************************************************** *****/
// function name: CameraFlatFieldingCorrectSaveParameterToFile
// Function Description: Save the flat field correction parameters to the file
// parameters: hCamera camera handle, obtained by the CameraInit function.
// pszFileName file path
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef CameraSdkStatus (__stdcall *_CameraFlatFieldingCorrectSaveParameterToFile)(
	CameraHandle hCamera,
	char const* pszFileName
	);

/***************************************************** *****/
// function name: CameraFlatFieldingCorrectLoadParameterFromFile
// Function Description: Load flat field correction parameters from the file
// parameters: hCamera camera handle, obtained by the CameraInit function.
// pszFileName file path
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef CameraSdkStatus (__stdcall *_CameraFlatFieldingCorrectLoadParameterFromFile)(
	CameraHandle hCamera,
	char const* pszFileName
	);

/***************************************************** *****/
// function name: CameraCommonCall
// Function Description: Some special camera functions called, the second development generally do not need to call.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// pszCall function and parameters
// pszResult call results, different pszCall, meaning different.
// uResultBufSize The size of the buffer pointed to by pszResult
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef CameraSdkStatus (__stdcall *_CameraCommonCall)(
	CameraHandle    hCamera, 
	char const*		pszCall,
	char*			pszResult,
	UINT			uResultBufSize
	);

/***************************************************** *****/
// function name: CameraSetDenoise3DParams
// Function Description: Set 3D noise reduction parameters
// parameters: hCamera camera handle, obtained by the CameraInit function.
// bEnable is enabled or disabled
// nCount use a few pictures for noise reduction (2-8 photos)
// Weights noise reduction weight
// When using 3 pictures for noise reduction This parameter can be passed into the three floating point (0.3,0.3,0.4), the last picture weight is greater than the previous 2
// If you do not need to use the weight, then this parameter is passed to 0, that all images have the same weight (0.33,0.33,0.33)
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef CameraSdkStatus (__stdcall *_CameraSetDenoise3DParams)(
	CameraHandle    hCamera, 
	BOOL			bEnable,
	int				nCount,
	float			*Weights
	);

/***************************************************** *****/
// function name: CameraGetDenoise3DParams
// Function Description: Get the current 3D noise reduction parameters
// parameters: hCamera camera handle, obtained by the CameraInit function.
// bEnable is enabled or disabled
// nCount used a few pictures for noise reduction
// bUseWeight uses the noise reduction weight
// Weights noise reduction weight
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef CameraSdkStatus (__stdcall *_CameraGetDenoise3DParams)(
	CameraHandle    hCamera, 
	BOOL			*bEnable,
	int				*nCount,
	BOOL			*bUseWeight,
	float			*Weights
	);

/***************************************************** *****/
// function name: CameraManualDenoise3D
// Function description: Perform a noise reduction on a group of frames
// Parameters: InFramesHead Input frame header
// InFramesData Input frame data
// nCount Number of input frames
// Weights noise reduction weight
// When using 3 pictures for noise reduction This parameter can be passed into the three floating point (0.3,0.3,0.4), the last picture weight is greater than the previous 2
// If you do not need to use the weight, then this parameter is passed to 0, that all images have the same weight (0.33,0.33,0.33)
// OutFrameHead prints the frame header
// OutFrameData Output frame data
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef CameraSdkStatus (__stdcall *_CameraManualDenoise3D)(
	tSdkFrameHead	*InFramesHead,
	BYTE			**InFramesData,
	int				nCount,
	float			*Weights,
	tSdkFrameHead	*OutFrameHead,
	BYTE			*OutFrameData
	);

/***************************************************** *****/
// function name: CameraCustomizeDeadPixels
// Function Description: Open the dead pixel editing panel
// parameters: hCamera camera handle, obtained by the CameraInit function.
// hParent handle of the function that called the window. Can be NULL.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef CameraSdkStatus (__stdcall *_CameraCustomizeDeadPixels)(
	CameraHandle	hCamera,
	HWND			hParent
	);

/***************************************************** *****/
// function name: CameraReadDeadPixels
// Function Description: read the camera dead pixels
// parameters: hCamera camera handle, obtained by the CameraInit function.
// pRows bad point y coordinate
// pCols Dots x coordinates
// When pNumPixel is input, it indicates the size of the buffer in the rank and column and returns the number of bad pixels returned in the rank and file buffer.
// When pRows or pCols is NULL, the function returns the current number of dead pixels in the camera via pNumPixel
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef CameraSdkStatus (__stdcall *_CameraReadDeadPixels)(
	CameraHandle    hCamera,
	USHORT*			pRows,
	USHORT*			pCols,
	UINT*			pNumPixel
	);

/***************************************************** *****/
// function name: CameraAddDeadPixels
// Function Description: Add camera dead pixels
// parameters: hCamera camera handle, obtained by the CameraInit function.
// pRows bad point y coordinate
// pCols Dots x coordinates
// NumPixel ranks the number of pixels in the buffer
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef CameraSdkStatus (__stdcall *_CameraAddDeadPixels)(
	CameraHandle    hCamera,
	USHORT*			pRows,
	USHORT*			pCols,
	UINT			NumPixel
	);

/***************************************************** *****/
// function name: CameraRemoveDeadPixels
// Function Description: Remove the camera specified dead pixels
// parameters: hCamera camera handle, obtained by the CameraInit function.
// pRows bad point y coordinate
// pCols Dots x coordinates
// NumPixel ranks the number of pixels in the buffer
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef CameraSdkStatus (__stdcall *_CameraRemoveDeadPixels)(
	CameraHandle    hCamera,
	USHORT*			pRows,
	USHORT*			pCols,
	UINT			NumPixel
	);

/***************************************************** *****/
// function name: CameraRemoveAllDeadPixels
// Function Description: Remove all camera dead pixels
// parameters: hCamera camera handle, obtained by the CameraInit function.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef CameraSdkStatus (__stdcall *_CameraRemoveAllDeadPixels)(
	CameraHandle    hCamera
	);

/***************************************************** *****/
// function name: CameraSaveDeadPixels
// Function Description: Save the camera dead pixels to the camera memory
// parameters: hCamera camera handle, obtained by the CameraInit function.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef CameraSdkStatus (__stdcall *_CameraSaveDeadPixels)(
	CameraHandle    hCamera
	);

/***************************************************** *****/
// function name: CameraSaveDeadPixelsToFile
// Function Description: Save the camera dead pixels to the file
// parameters: hCamera camera handle, obtained by the CameraInit function.
// sFileName Full path of the bad point file.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef CameraSdkStatus (__stdcall *_CameraSaveDeadPixelsToFile)(
	CameraHandle    hCamera,
	char const*		sFileName
	);

/***************************************************** *****/
// function name: CameraLoadDeadPixelsFromFile
// Function Description: Load camera dead pixels from file
// parameters: hCamera camera handle, obtained by the CameraInit function.
// sFileName Full path of the bad point file.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef CameraSdkStatus (__stdcall *_CameraLoadDeadPixelsFromFile)(
	CameraHandle    hCamera,
	char const*		sFileName
	);

/***************************************************** *****/
// function name: CameraGetImageBufferPriority
// Function Description: Get a frame of image data. In order to improve efficiency, SDK uses a zero-copy mechanism for image capture,
// CameraGetImageBuffer actually gets a buffer address in the kernel,
// After the function is successfully called, you must call CameraReleaseImageBuffer to release by
// CameraGetImageBuffer Get the buffer for the kernel to continue using
// This buffer.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// The header information pointer of the pFrameInfo image.
// pbyBuffer Buffer pointer to the image's data. due to
// A zero-copy mechanism is used to improve efficiency, so
// Here is a pointer to the pointer.
// wTimes capture image timeout. Milliseconds. in
// wTimes time has not yet been the image, then the function
// will return the timeout message.
// Priority drawing priority see: emCameraGetImagePriority
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef CameraSdkStatus (__stdcall *_CameraGetImageBufferPriority)(
	CameraHandle        hCamera, 
	tSdkFrameHead*      pFrameInfo, 
	BYTE**              pbyBuffer,
	UINT                wTimes,
	UINT				Priority
	);

/***************************************************** *****/
// function name: CameraGetImageBufferPriorityEx
// Function Description: Get a frame of image data. The image obtained by this interface is processed RGB format. After the function is called,
// Do not need to call CameraReleaseImageBuffer release, do not call free release of such functions
// to free the image data buffer returned by this function.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// piWidth plastic pointer, returns the width of the image
// piHeight plastic pointer, returns the height of the image
// UINT wTimes The time-out period for grabbing the image. Milliseconds. in
// wTimes time has not yet been the image, then the function
// will return the timeout message.
// Priority drawing priority see: emCameraGetImagePriority
// Return Value: Returns the first address of the RGB data buffer on success;
// otherwise 0
/***************************************************** *****/

typedef unsigned char* (__stdcall *_CameraGetImageBufferPriorityEx)(
	CameraHandle        hCamera, 
	INT*                piWidth,
	INT*                piHeight,
	UINT                wTimes,
	UINT				Priority
	);

/***************************************************** *****/
// function name: CameraGetImageBufferPriorityEx2
// Function Description: Get a frame of image data. The image obtained by this interface is processed RGB format. After the function is called,
// Do not need to call CameraReleaseImageBuffer release, do not call free release of such functions
// to free the image data buffer returned by this function.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// pImageData Buffer to receive image data, the size must match the format specified by uOutFormat, otherwise the data will overflow
// piWidth plastic pointer, returns the width of the image
// piHeight plastic pointer, returns the height of the image
// wTimes capture image timeout. Milliseconds. in
// wTimes time has not yet been the image, then the function
// will return the timeout message.
// Priority drawing priority see: emCameraGetImagePriority
// Return Value: Returns the first address of the RGB data buffer on success;
// otherwise 0
/***************************************************** *****/
typedef CameraSdkStatus (__stdcall *_CameraGetImageBufferPriorityEx2)(
	CameraHandle    hCamera, 
	BYTE*           pImageData,
	UINT            uOutFormat,
	int*            piWidth,
	int*            piHeight,
	UINT            wTimes,
	UINT			Priority
	);

/***************************************************** *****/
// function name: CameraGetImageBufferPriorityEx3
// Function Description: Get a frame of image data. The image obtained by this interface is processed RGB format. After the function is called,
// Do not need to call CameraReleaseImageBuffer release.
// uOutFormat 0: 8 BIT gray 1: rgb24 2: rgba32 3: bgr24 4: bgra32
// parameters: hCamera camera handle, obtained by the CameraInit function.
// pImageData Buffer to receive image data, the size must match the format specified by uOutFormat, otherwise the data will overflow
// piWidth plastic pointer, returns the width of the image
// piHeight plastic pointer, returns the height of the image
// puTimeStamp unsigned plastic, returns the image timestamp
// UINT wTimes The time-out period for grabbing the image. Milliseconds. in
// The image has not been acquired within the wTimes time, the function returns the time-out information.
// Priority drawing priority see: emCameraGetImagePriority
// Return Value: Returns the first address of the RGB data buffer on success;
// otherwise 0
/***************************************************** *****/
typedef CameraSdkStatus (__stdcall *_CameraGetImageBufferPriorityEx3)(
	CameraHandle hCamera, 
	BYTE*pImageData,
	UINT uOutFormat,
	int *piWidth,
	int *piHeight,
	UINT* puTimeStamp,
	UINT wTimes,
	UINT Priority
	);

/***************************************************** *****/
// function name: CameraClearBuffer
// Function Description: Empty all frames that have been cached in the camera
// parameters: hCamera camera handle, obtained by the CameraInit function.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef CameraSdkStatus (__stdcall *_CameraClearBuffer)(
	CameraHandle hCamera
	);

/***************************************************** *****/
// function name: CameraSoftTriggerEx
// Function description: Execute soft trigger once. After execution, triggered by CameraSetTriggerCount
// The specified number of frames.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// uFlags function flag, as defined in emCameraSoftTriggerExFlags
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
typedef CameraSdkStatus (__stdcall *_CameraSoftTriggerEx)(
	CameraHandle hCamera,
	UINT uFlags
	);

/***************************************************** *****/
// function name: CameraSetHDR
// Function Description: Set the camera's HDR, camera support, models without HDR function, this function returns an error code that does not support.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// value HDR coefficient in the range of 0.0 to 1.0
// Return Value: Upon successful, return CAMERA_STATUS_SUCCESS (0), said the camera connection status is normal;
// otherwise it returns non-zero value, refer to the definition of error code in CameraStatus.h.
/***************************************************** *****/
typedef CameraSdkStatus (__stdcall *_CameraSetHDR)(
	CameraHandle    hCamera,
	float           value
	);

/***************************************************** *****/
// function name: CameraGetHDR
// Function Description: To get the HDR of the camera, you need the camera support, the model without HDR function, this function returns the error code, which means it is not supported.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// value HDR coefficient in the range of 0.0 to 1.0
// Return Value: Upon successful, return CAMERA_STATUS_SUCCESS (0), said the camera connection status is normal;
// otherwise it returns non-zero value, refer to the definition of error code in CameraStatus.h.
/***************************************************** *****/
typedef CameraSdkStatus (__stdcall *_CameraGetHDR)(
	CameraHandle    hCamera,
	float*          value
	);

/***************************************************** *****/
// function name: CameraGetFrameID
// Function Description: Get the ID of the current frame, camera support (network interface full range of support), this function returns an error code that does not support.
// parameters: hCamera camera handle, obtained by the CameraInit function.
//             id frame ID
// Return Value: Upon successful, return CAMERA_STATUS_SUCCESS (0), said the camera connection status is normal;
// otherwise it returns non-zero value, refer to the definition of error code in CameraStatus.h.
/***************************************************** *****/
typedef CameraSdkStatus (__stdcall *_CameraGetFrameID)(
	CameraHandle    hCamera,
	UINT*			id
	);

/***************************************************** *****/
// function name: CameraGetFrameTimeStamp
// Function Description: Get the current frame time stamp (in microseconds)
// parameters: hCamera camera handle, obtained by the CameraInit function.
// TimeStampL timestamp low 32 bits
// TimeStampH timestamp high 32 bits
// Return Value: Upon successful, return CAMERA_STATUS_SUCCESS (0), said the camera connection status is normal;
// otherwise it returns non-zero value, refer to the definition of error code in CameraStatus.h.
/***************************************************** *****/
typedef CameraSdkStatus (__stdcall *_CameraGetFrameTimeStamp)(
	CameraHandle    hCamera,
	UINT*           TimeStampL,
	UINT*			TimeStampH
	);

/******************************************************/
// function name: CameraGrabber_CreateFromDevicePage
// Function Description: Pop-up camera list allows the user to select the camera to be opened
// Parameters: Grabber created by the function if the function succeeded
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/******************************************************/
typedef CameraSdkStatus (__stdcall *_CameraGrabber_CreateFromDevicePage)(
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
typedef CameraSdkStatus (__stdcall *_CameraGrabber_CreateByIndex)(
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
typedef CameraSdkStatus (__stdcall *_CameraGrabber_CreateByName)(
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
typedef CameraSdkStatus (__stdcall *_CameraGrabber_Create)(
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
typedef CameraSdkStatus (__stdcall *_CameraGrabber_Destroy)(
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
typedef CameraSdkStatus (__stdcall *_CameraGrabber_SetHWnd)(
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
typedef CameraSdkStatus (__stdcall *_CameraGrabber_SetPriority)(
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
typedef CameraSdkStatus (__stdcall *_CameraGrabber_StartLive)(
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
typedef CameraSdkStatus (__stdcall *_CameraGrabber_StopLive)(
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
typedef CameraSdkStatus (__stdcall *_CameraGrabber_SaveImage)(
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
typedef CameraSdkStatus (__stdcall *_CameraGrabber_SaveImageAsync)(
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
typedef CameraSdkStatus (__stdcall *_CameraGrabber_SaveImageAsyncEx)(
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
typedef CameraSdkStatus (__stdcall *_CameraGrabber_SetSaveImageCompleteCallback)(
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
typedef CameraSdkStatus (__stdcall *_CameraGrabber_SetFrameListener)(
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
typedef CameraSdkStatus (__stdcall *_CameraGrabber_SetRawCallback)(
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
typedef CameraSdkStatus (__stdcall *_CameraGrabber_SetRGBCallback)(
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
typedef CameraSdkStatus (__stdcall *_CameraGrabber_GetCameraHandle)(
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
typedef CameraSdkStatus (__stdcall *_CameraGrabber_GetStat)(
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
typedef CameraSdkStatus (__stdcall *_CameraGrabber_GetCameraDevInfo)(
	void* Grabber,
	tSdkCameraDevInfo *DevInfo
	);

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
typedef CameraSdkStatus (__stdcall *_CameraImage_Create)(
	void** Image,
	BYTE *pFrameBuffer, 
	tSdkFrameHead* pFrameHead,
	BOOL bCopy
	);

/******************************************************/
// function name: CameraImage_Destroy
// Function Description: Destroy Image
// parameter: Image
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/******************************************************/
typedef CameraSdkStatus (__stdcall *_CameraImage_Destroy)(
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
typedef CameraSdkStatus (__stdcall *_CameraImage_GetData)(
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
typedef CameraSdkStatus (__stdcall *_CameraImage_GetUserData)(
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
typedef CameraSdkStatus (__stdcall *_CameraImage_SetUserData)(
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
typedef CameraSdkStatus (__stdcall *_CameraImage_IsEmpty)(
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
typedef CameraSdkStatus (__stdcall *_CameraImage_Draw)(
	void* Image,
	HWND hWnd,
	int Algorithm
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
typedef CameraSdkStatus (__stdcall *_CameraImage_BitBlt)(
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
// function name: CameraImage_SaveAsBmp
// Function Description: Save Image in bmp format
// parameter: Image
// FileName file name
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/******************************************************/
typedef CameraSdkStatus (__stdcall *_CameraImage_SaveAsBmp)(
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
typedef CameraSdkStatus (__stdcall *_CameraImage_SaveAsJpeg)(
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
typedef CameraSdkStatus (__stdcall *_CameraImage_SaveAsPng)(
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
typedef CameraSdkStatus (__stdcall *_CameraImage_SaveAsRaw)(
	void* Image,
	char const* FileName,
	int Format
	);


#ifdef API_LOAD_MAIN
#define EXTERN 
#else
#define EXTERN extern
#endif

EXTERN INT gSdkLanguageSel;//0:English 1:Chinese

EXTERN _CameraSdkInit CameraSdkInit;

EXTERN _CameraSetCallbackFunction CameraSetCallbackFunction;

EXTERN _CameraGetInformation CameraGetInformation;

EXTERN _CameraSaveImage CameraSaveImage;

EXTERN _CameraInitRecord CameraInitRecord;

EXTERN _CameraStopRecord CameraStopRecord;

EXTERN _CameraPushFrame CameraPushFrame;

EXTERN _CameraSpecialControl CameraSpecialControl;

EXTERN _CameraSnapToBuffer CameraSnapToBuffer;

EXTERN _CameraIsOpened CameraIsOpened;

EXTERN _CameraInit CameraInit;

EXTERN _CameraInitEx CameraInitEx;

EXTERN _CameraInitEx2 CameraInitEx2;

EXTERN _CameraUnInit CameraUnInit;

EXTERN _CameraPlay CameraPlay;

EXTERN _CameraPause CameraPause;

EXTERN _CameraStop CameraStop;

EXTERN _CameraSetDisplayMode CameraSetDisplayMode;

EXTERN _CameraDisplayRGB24 CameraDisplayRGB24;

EXTERN _CameraSetDisplayOffset CameraSetDisplayOffset;

EXTERN _CameraImageOverlay CameraImageOverlay;

EXTERN _CameraDisplayInit CameraDisplayInit;

EXTERN _CameraSetDisplaySize CameraSetDisplaySize;

EXTERN _CameraGetImageBuffer CameraGetImageBuffer;

EXTERN _CameraGetImageBufferEx CameraGetImageBufferEx;

EXTERN _CameraReleaseImageBuffer CameraReleaseImageBuffer;

EXTERN _CameraCreateSettingPage CameraCreateSettingPage;

EXTERN _CameraSetActiveSettingSubPage CameraSetActiveSettingSubPage;

EXTERN _CameraCustomizeResolution CameraCustomizeResolution;

EXTERN _CameraSetMirror CameraSetMirror;

EXTERN _CameraGetMirror CameraGetMirror;

EXTERN _CameraSetMonochrome CameraSetMonochrome;

EXTERN _CameraGetMonochrome CameraGetMonochrome;

EXTERN _CameraSetInverse CameraSetInverse;

EXTERN _CameraGetInverse CameraGetInverse;

EXTERN _CameraGetImageResolution CameraGetImageResolution;

EXTERN _CameraGetImageResolutionEx CameraGetImageResolutionEx;

EXTERN _CameraSetImageResolution CameraSetImageResolution;

EXTERN _CameraSetImageResolutionEx CameraSetImageResolutionEx;

EXTERN _CameraGetMediaType CameraGetMediaType;

EXTERN _CameraSetMediaType CameraSetMediaType;

EXTERN _CameraSetAeState CameraSetAeState;

EXTERN _CameraGetAeState CameraGetAeState;

EXTERN _CameraSetAeTarget CameraSetAeTarget;

EXTERN _CameraGetAeTarget CameraGetAeTarget;

EXTERN _CameraSetAeExposureRange CameraSetAeExposureRange;

EXTERN _CameraGetAeExposureRange CameraGetAeExposureRange;

EXTERN _CameraSetAeAnalogGainRange CameraSetAeAnalogGainRange;

EXTERN _CameraGetAeAnalogGainRange CameraGetAeAnalogGainRange;

EXTERN _CameraSetAeThreshold CameraSetAeThreshold;

EXTERN _CameraGetAeThreshold CameraGetAeThreshold;

EXTERN _CameraSetExposureTime CameraSetExposureTime;

EXTERN _CameraGetExposureTime CameraGetExposureTime;

EXTERN _CameraGetExposureTimeRange CameraGetExposureTimeRange;

EXTERN _CameraGetExposureLineTime CameraGetExposureLineTime;

EXTERN _CameraSetAnalogGain CameraSetAnalogGain;

EXTERN _CameraGetAnalogGain CameraGetAnalogGain;

EXTERN _CameraSetSharpness CameraSetSharpness;

EXTERN _CameraGetSharpness CameraGetSharpness;

EXTERN _CameraGetPresetClrTemp CameraGetPresetClrTemp;

EXTERN _CameraSetPresetClrTemp CameraSetPresetClrTemp;

EXTERN _CameraSetUserClrTempGain CameraSetUserClrTempGain;

EXTERN _CameraGetUserClrTempGain CameraGetUserClrTempGain;

EXTERN _CameraSetUserClrTempMatrix CameraSetUserClrTempMatrix;

EXTERN _CameraGetUserClrTempMatrix CameraGetUserClrTempMatrix;

EXTERN _CameraSetClrTempMode CameraSetClrTempMode;

EXTERN _CameraGetClrTempMode CameraGetClrTempMode;

EXTERN _CameraSetLutMode CameraSetLutMode;

EXTERN _CameraGetLutMode CameraGetLutMode;

EXTERN _CameraSelectLutPreset CameraSelectLutPreset;

EXTERN _CameraGetLutPresetSel CameraGetLutPresetSel;

EXTERN _CameraSetCustomLut CameraSetCustomLut;

EXTERN _CameraGetCustomLut CameraGetCustomLut;

EXTERN _CameraGetCurrentLut CameraGetCurrentLut;

EXTERN _CameraSetOnceWB CameraSetOnceWB;

EXTERN _CameraSetOnceBB CameraSetOnceBB;

EXTERN _CameraSetWbMode CameraSetWbMode;

EXTERN _CameraGetWbMode CameraGetWbMode;

EXTERN _CameraSetWbWindow CameraSetWbWindow;

EXTERN _CameraSetGain CameraSetGain;

EXTERN _CameraGetGain CameraGetGain;

EXTERN _CameraSetGamma CameraSetGamma;

EXTERN _CameraGetGamma CameraGetGamma;

EXTERN _CameraSetSaturation CameraSetSaturation;

EXTERN _CameraGetSaturation CameraGetSaturation;

EXTERN _CameraSetContrast CameraSetContrast;

EXTERN _CameraGetContrast CameraGetContrast;

EXTERN _CameraSetFrameSpeed CameraSetFrameSpeed;

EXTERN _CameraGetFrameSpeed CameraGetFrameSpeed;

EXTERN _CameraSetAntiFlick CameraSetAntiFlick;

EXTERN _CameraGetAntiFlick CameraGetAntiFlick;

EXTERN _CameraGetLightFrequency CameraGetLightFrequency;

EXTERN _CameraSetLightFrequency CameraSetLightFrequency;

EXTERN _CameraSetTransPackLen CameraSetTransPackLen;

EXTERN _CameraGetTransPackLen CameraGetTransPackLen;

EXTERN _CameraWriteSN CameraWriteSN;

EXTERN _CameraReadSN CameraReadSN;

EXTERN _CameraSaveParameter CameraSaveParameter;

EXTERN _CameraLoadParameter CameraLoadParameter;

EXTERN _CameraGetCurrentParameterGroup CameraGetCurrentParameterGroup;

EXTERN _CameraEnumerateDevice CameraEnumerateDevice;

EXTERN _CameraEnumerateDeviceEx CameraEnumerateDeviceEx;

EXTERN _CameraGetCapability CameraGetCapability;

EXTERN _CameraImageProcess CameraImageProcess;

EXTERN _CameraImageProcessEx CameraImageProcessEx;

EXTERN _CameraSoftTrigger CameraSoftTrigger;

EXTERN _CameraSetTriggerMode CameraSetTriggerMode;

EXTERN _CameraGetTriggerMode CameraGetTriggerMode;

EXTERN _CameraSetStrobeMode CameraSetStrobeMode;

EXTERN _CameraGetStrobeMode CameraGetStrobeMode;

EXTERN _CameraSetStrobeDelayTime CameraSetStrobeDelayTime;

EXTERN _CameraGetStrobeDelayTime CameraGetStrobeDelayTime;

EXTERN _CameraSetStrobePulseWidth CameraSetStrobePulseWidth;

EXTERN _CameraGetStrobePulseWidth CameraGetStrobePulseWidth;

EXTERN _CameraSetStrobePolarity CameraSetStrobePolarity;

EXTERN _CameraGetStrobePolarity CameraGetStrobePolarity;

EXTERN _CameraSetExtTrigSignalType CameraSetExtTrigSignalType;

EXTERN _CameraGetExtTrigSignalType CameraGetExtTrigSignalType;

EXTERN _CameraSetExtTrigShutterType CameraSetExtTrigShutterType;

EXTERN _CameraGetExtTrigShutterType CameraGetExtTrigShutterType;

EXTERN _CameraSetExtTrigDelayTime CameraSetExtTrigDelayTime;

EXTERN _CameraGetExtTrigDelayTime CameraGetExtTrigDelayTime;

EXTERN _CameraSetExtTrigJitterTime CameraSetExtTrigJitterTime;

EXTERN _CameraGetExtTrigJitterTime CameraGetExtTrigJitterTime;

EXTERN _CameraGetExtTrigCapability CameraGetExtTrigCapability;

EXTERN _CameraShowSettingPage CameraShowSettingPage;

EXTERN _CameraGetFrameStatistic CameraGetFrameStatistic;

EXTERN _CameraGetResolutionForSnap CameraGetResolutionForSnap;

EXTERN _CameraSetResolutionForSnap CameraSetResolutionForSnap;

EXTERN _CameraIsAeWinVisible CameraIsAeWinVisible;

EXTERN _CameraIsWbWinVisible CameraIsWbWinVisible;

EXTERN _CameraGetNoiseFilterState CameraGetNoiseFilterState;

EXTERN _CameraSetParameterMode CameraSetParameterMode;

EXTERN _CameraGetParameterMode CameraGetParameterMode;

EXTERN _CameraSetParameterMask CameraSetParameterMask;

EXTERN _CameraGetTriggerCount CameraGetTriggerCount;

EXTERN _CameraGetCrossLine CameraGetCrossLine;

EXTERN _CameraSetCrossLine CameraSetCrossLine;

EXTERN _CameraGetTriggerDelayTime CameraGetTriggerDelayTime;

EXTERN _CameraSetTriggerDelayTime CameraSetTriggerDelayTime;

EXTERN _CameraSetAeWinVisible CameraSetAeWinVisible;

EXTERN _CameraSetNoiseFilter CameraSetNoiseFilter;

EXTERN _CameraSetTriggerCount CameraSetTriggerCount;

EXTERN _CameraCustomizeReferWin CameraCustomizeReferWin;

EXTERN _CameraSetAeWindow CameraSetAeWindow;

EXTERN _CameraReadParameterFromFile CameraReadParameterFromFile;

EXTERN _CameraSetWbWinVisible CameraSetWbWinVisible;

EXTERN _CameraRstTimeStamp CameraRstTimeStamp;

EXTERN _CameraGetCapabilityEx CameraGetCapabilityEx;

EXTERN _CameraSaveUserData CameraSaveUserData;

EXTERN _CameraLoadUserData CameraLoadUserData;

EXTERN _CameraGetFriendlyName CameraGetFriendlyName;

EXTERN _CameraSetFriendlyName CameraSetFriendlyName;

EXTERN _CameraSdkGetVersionString CameraSdkGetVersionString; 

EXTERN _CameraCheckFwUpdate CameraCheckFwUpdate;

EXTERN _CameraGetFirmwareVision CameraGetFirmwareVision;

EXTERN _CameraGetEnumInfo CameraGetEnumInfo;

EXTERN _CameraGetInerfaceVersion CameraGetInerfaceVersion;

EXTERN _CameraSetIOState CameraSetIOState;

EXTERN _CameraGetIOState CameraGetIOState;

EXTERN _CameraSetInPutIOMode CameraSetInPutIOMode;

EXTERN _CameraSetOutPutIOMode CameraSetOutPutIOMode;

EXTERN _CameraSetOutPutPWM CameraSetOutPutPWM;

EXTERN _CameraSetBayerDecAlgorithm CameraSetBayerDecAlgorithm;

EXTERN _CameraGetBayerDecAlgorithm CameraGetBayerDecAlgorithm;

EXTERN _CameraSetBlackLevel CameraSetBlackLevel;

EXTERN _CameraGetBlackLevel CameraGetBlackLevel;

EXTERN _CameraSetWhiteLevel CameraSetWhiteLevel;

EXTERN _CameraGetWhiteLevel CameraGetWhiteLevel;

EXTERN _CameraSetIspOutFormat CameraSetIspOutFormat;

EXTERN _CameraGetIspOutFormat CameraGetIspOutFormat;

EXTERN _CameraGetErrorString CameraGetErrorString;

EXTERN _CameraGetCapabilityEx2 CameraGetCapabilityEx2;

EXTERN _CameraGetImageBufferEx2 CameraGetImageBufferEx2;

EXTERN _CameraGetImageBufferEx3 CameraGetImageBufferEx3;

EXTERN _CameraReConnect CameraReConnect;

EXTERN _CameraConnectTest CameraConnectTest;

EXTERN _CameraSetLedEnable CameraSetLedEnable;

EXTERN _CameraGetLedEnable CameraGetLedEnable;

EXTERN _CameraSetLedOnOff CameraSetLedOnOff;

EXTERN _CameraGetLedOnOff CameraGetLedOnOff;

EXTERN _CameraSetLedDuration CameraSetLedDuration;

EXTERN _CameraGetLedDuration CameraGetLedDuration;

EXTERN _CameraSetLedBrightness CameraSetLedBrightness;

EXTERN _CameraGetLedBrightness CameraGetLedBrightness;

EXTERN _CameraEnableTransferRoi CameraEnableTransferRoi;

EXTERN _CameraSetTransferRoi CameraSetTransferRoi;

EXTERN _CameraGetTransferRoi CameraGetTransferRoi;

EXTERN _CameraAlignMalloc CameraAlignMalloc;

EXTERN _CameraAlignFree CameraAlignFree;

EXTERN _CameraSetAutoConnect CameraSetAutoConnect;

EXTERN _CameraGetAutoConnect CameraGetAutoConnect;

EXTERN _CameraGetReConnectCounts CameraGetReConnectCounts;

EXTERN _CameraSetSingleGrabMode CameraSetSingleGrabMode;

EXTERN _CameraGetSingleGrabMode CameraGetSingleGrabMode;

EXTERN _CameraRestartGrab CameraRestartGrab;

EXTERN _CameraDrawText CameraDrawText;

EXTERN _CameraGigeGetIp CameraGigeGetIp;

EXTERN _CameraGigeSetIp CameraGigeSetIp;

EXTERN _CameraGigeGetMac CameraGigeGetMac;

EXTERN _CameraEnableFastResponse CameraEnableFastResponse;

EXTERN _CameraSetCorrectDeadPixel CameraSetCorrectDeadPixel;

EXTERN _CameraGetCorrectDeadPixel CameraGetCorrectDeadPixel;

EXTERN _CameraFlatFieldingCorrectSetEnable CameraFlatFieldingCorrectSetEnable;

EXTERN _CameraFlatFieldingCorrectGetEnable CameraFlatFieldingCorrectGetEnable;

EXTERN _CameraFlatFieldingCorrectSetParameter CameraFlatFieldingCorrectSetParameter;

EXTERN _CameraFlatFieldingCorrectSaveParameterToFile CameraFlatFieldingCorrectSaveParameterToFile;

EXTERN _CameraFlatFieldingCorrectLoadParameterFromFile CameraFlatFieldingCorrectLoadParameterFromFile;

EXTERN _CameraCommonCall CameraCommonCall;

EXTERN _CameraSetDenoise3DParams CameraSetDenoise3DParams;

EXTERN _CameraGetDenoise3DParams CameraGetDenoise3DParams;

EXTERN _CameraManualDenoise3D CameraManualDenoise3D;

EXTERN _CameraCustomizeDeadPixels CameraCustomizeDeadPixels;

EXTERN _CameraReadDeadPixels CameraReadDeadPixels;

EXTERN _CameraAddDeadPixels CameraAddDeadPixels;

EXTERN _CameraRemoveDeadPixels CameraRemoveDeadPixels;

EXTERN _CameraRemoveAllDeadPixels CameraRemoveAllDeadPixels;

EXTERN _CameraSaveDeadPixels CameraSaveDeadPixels;

EXTERN _CameraSaveDeadPixelsToFile CameraSaveDeadPixelsToFile;

EXTERN _CameraLoadDeadPixelsFromFile CameraLoadDeadPixelsFromFile;

EXTERN _CameraGetImageBufferPriority CameraGetImageBufferPriority;

EXTERN _CameraGetImageBufferPriorityEx CameraGetImageBufferPriorityEx;

EXTERN _CameraGetImageBufferPriorityEx2 CameraGetImageBufferPriorityEx2;

EXTERN _CameraGetImageBufferPriorityEx3 CameraGetImageBufferPriorityEx3;

EXTERN _CameraClearBuffer CameraClearBuffer;

EXTERN _CameraSoftTriggerEx CameraSoftTriggerEx;

EXTERN _CameraSetHDR CameraSetHDR;

EXTERN _CameraGetHDR CameraGetHDR;

EXTERN _CameraGetFrameID CameraGetFrameID;

EXTERN _CameraGetFrameTimeStamp CameraGetFrameTimeStamp;

EXTERN _CameraGrabber_CreateFromDevicePage CameraGrabber_CreateFromDevicePage;

EXTERN _CameraGrabber_CreateByIndex CameraGrabber_CreateByIndex;

EXTERN _CameraGrabber_CreateByName CameraGrabber_CreateByName;

EXTERN _CameraGrabber_Create CameraGrabber_Create;

EXTERN _CameraGrabber_Destroy CameraGrabber_Destroy;

EXTERN _CameraGrabber_SetHWnd CameraGrabber_SetHWnd;

EXTERN _CameraGrabber_SetPriority CameraGrabber_SetPriority;

EXTERN _CameraGrabber_StartLive CameraGrabber_StartLive;

EXTERN _CameraGrabber_StopLive CameraGrabber_StopLive;

EXTERN _CameraGrabber_SaveImage CameraGrabber_SaveImage;

EXTERN _CameraGrabber_SaveImageAsync CameraGrabber_SaveImageAsync;

EXTERN _CameraGrabber_SaveImageAsyncEx CameraGrabber_SaveImageAsyncEx;

EXTERN _CameraGrabber_SetSaveImageCompleteCallback CameraGrabber_SetSaveImageCompleteCallback;

EXTERN _CameraGrabber_SetFrameListener CameraGrabber_SetFrameListener;

EXTERN _CameraGrabber_SetRawCallback CameraGrabber_SetRawCallback;

EXTERN _CameraGrabber_SetRGBCallback CameraGrabber_SetRGBCallback;

EXTERN _CameraGrabber_GetCameraHandle CameraGrabber_GetCameraHandle;

EXTERN _CameraGrabber_GetStat CameraGrabber_GetStat;

EXTERN _CameraGrabber_GetCameraDevInfo CameraGrabber_GetCameraDevInfo;

EXTERN _CameraImage_Create CameraImage_Create;

EXTERN _CameraImage_Destroy CameraImage_Destroy;

EXTERN _CameraImage_GetData CameraImage_GetData;

EXTERN _CameraImage_GetUserData CameraImage_GetUserData;

EXTERN _CameraImage_SetUserData CameraImage_SetUserData;

EXTERN _CameraImage_IsEmpty CameraImage_IsEmpty;

EXTERN _CameraImage_Draw CameraImage_Draw;

EXTERN _CameraImage_BitBlt CameraImage_BitBlt;

EXTERN _CameraImage_SaveAsBmp CameraImage_SaveAsBmp;

EXTERN _CameraImage_SaveAsJpeg CameraImage_SaveAsJpeg;

EXTERN _CameraImage_SaveAsPng CameraImage_SaveAsPng;

EXTERN _CameraImage_SaveAsRaw CameraImage_SaveAsRaw;


CameraSdkStatus LoadSdkApi();

CameraSdkStatus UnloadCameraSdk();

#ifdef API_LOAD_MAIN
#undef API_LOAD_MAIN


// Some functions may fail to load if the SDK version does not match.

#define CHCEK_API_LOAD 0 // 1: test after loading API function, if the loading fails, a prompt box will pop up. 0: Do not detect (can be compared by SDK version number).

#if CHCEK_API_LOAD
#define CHECK_API(API) if(API == NULL)\
{\
	MessageBoxA(NULL,#API,gSdkLanguageSel?"Function load failed!":"Function load failed!",0);\
	return CAMERA_STATUS_FAILED;\
}
#else
#define CHECK_API(API) 
#endif

HMODULE ghSDK = NULL;

CameraSdkStatus LoadSdkApi()
{
	char szCompany[64];
	char strPath[MAX_PATH];
	char strDir[MAX_PATH];
	HKEY hkey = NULL;
	DWORD dwType = REG_SZ;
	DWORD dwSize = MAX_PATH;
	BYTE abyValue[MAX_PATH];
	LONG status;

	gSdkLanguageSel = 0;

	if (ERROR_SUCCESS != RegOpenKeyExA(HKEY_LOCAL_MACHINE, "Software\\Industry Camera", 0, KEY_READ, &hkey))
		return CAMERA_STATUS_NOT_INITIALIZED;
	dwSize = sizeof(szCompany);
	status = RegQueryValueExA(hkey, "Company", NULL, &dwType, (LPBYTE)szCompany, &dwSize);
	RegCloseKey(hkey);
	hkey = NULL;
	if (status != ERROR_SUCCESS)
		return CAMERA_STATUS_NOT_INITIALIZED;

#ifdef _WIN64
	sprintf_s(strPath, sizeof(strPath), "Software\\%s\\Settings_X64", szCompany);
#else
	sprintf_s(strPath, sizeof(strPath), "Software\\%s\\Settings", szCompany);
#endif
	hkey = NULL;
	RegCreateKeyExA(HKEY_LOCAL_MACHINE, strPath, 0, NULL, 0, KEY_READ, NULL, &hkey, NULL);

	do
	{
		if (NULL != hkey)
		{
			memset(abyValue, 0x00, MAX_PATH);
			dwType = REG_SZ;
			dwSize = MAX_PATH;
			status = RegQueryValueExA(hkey, "Language", NULL, &dwType, abyValue, &dwSize);
			if (ERROR_SUCCESS == status)
			{
				abyValue[MAX_PATH-1] = '\0';
				if (strcmp((const char *)abyValue,"Chinese") == 0)
				{
					gSdkLanguageSel = 1;
				}
				else if (strcmp((const char *)abyValue,"TradChinese") == 0)
				{
					gSdkLanguageSel = 2;
				}
			}

			dwType = REG_SZ;
			dwSize = MAX_PATH;
			status = RegQueryValueExA(hkey, "SdkPath", NULL, &dwType, abyValue, &dwSize);
			if (ERROR_SUCCESS == status)
			{
				abyValue[MAX_PATH-1] = '\0';
				strcpy(strPath,(const char *)abyValue);
				break;
			}
		}

		MessageBoxA(NULL, "Failed to access registry", "Error", 0);
		return CAMERA_STATUS_FAILED;
	} while(0);

	if (hkey != NULL)
	{
		RegCloseKey(hkey);
		hkey = NULL;
	}

#ifndef _WIN64
	sprintf_s(strDir,sizeof(strDir),"%s%s",strPath,"\\MVCAMSDK.dll");
#else
	sprintf_s(strDir,sizeof(strDir),"%s%s",strPath,"\\MVCAMSDK_X64.dll");
#endif
	ghSDK = ::LoadLibraryA(strDir);


	if (NULL == ghSDK)
	{
		if (gSdkLanguageSel == 1)
		{
			sprintf_s(strPath,sizeof(strPath),"Failed to load file[%s] ,put the file on the directory or re-install the platform and try again!",strDir);
			MessageBoxA(NULL, strPath, "Error", 0);
		}
		else
		{
			sprintf_s(strPath,sizeof(strPath),"Failed to load file[%s] ,put the file on the directory or re-install the platform and try again!",strDir);
			MessageBoxA(NULL, strPath, "Error", 0);
		} 
		return CAMERA_STATUS_FAILED;
	}
	
#define GET_MVSDK_API(name)			\
	name = (_##name)GetProcAddress(ghSDK, #name);\
	CHECK_API(name)

	GET_MVSDK_API(CameraSdkInit);
	GET_MVSDK_API(CameraSetCallbackFunction);
	GET_MVSDK_API(CameraGetInformation);
	GET_MVSDK_API(CameraInit);
	GET_MVSDK_API(CameraInitEx);
	GET_MVSDK_API(CameraInitEx2);
	GET_MVSDK_API(CameraUnInit);
	GET_MVSDK_API(CameraImageProcess);
	GET_MVSDK_API(CameraImageProcessEx);
	GET_MVSDK_API(CameraPlay);
	GET_MVSDK_API(CameraPause);
	GET_MVSDK_API(CameraStop);
	GET_MVSDK_API(CameraDisplayRGB24);
	GET_MVSDK_API(CameraSetDisplayMode);
	GET_MVSDK_API(CameraImageOverlay);
	GET_MVSDK_API(CameraDisplayInit);
	GET_MVSDK_API(CameraSetDisplaySize);
	GET_MVSDK_API(CameraSetDisplayOffset);
	GET_MVSDK_API(CameraInitRecord);
	GET_MVSDK_API(CameraStopRecord);
	GET_MVSDK_API(CameraPushFrame);
	GET_MVSDK_API(CameraSpecialControl);
	GET_MVSDK_API(CameraSnapToBuffer);
	GET_MVSDK_API(CameraGetImageBuffer);
	GET_MVSDK_API(CameraGetImageBufferEx);
	GET_MVSDK_API(CameraReleaseImageBuffer);
	GET_MVSDK_API(CameraCreateSettingPage);
	GET_MVSDK_API(CameraSetActiveSettingSubPage);
	GET_MVSDK_API(CameraSetMirror);
	GET_MVSDK_API(CameraGetMirror);
	GET_MVSDK_API(CameraSetMonochrome);
	GET_MVSDK_API(CameraGetMonochrome);
	GET_MVSDK_API(CameraSetInverse);
	GET_MVSDK_API(CameraGetInverse);
	GET_MVSDK_API(CameraCustomizeResolution);
	GET_MVSDK_API(CameraGetImageResolution);
	GET_MVSDK_API(CameraGetImageResolutionEx);
	GET_MVSDK_API(CameraSetImageResolution);
	GET_MVSDK_API(CameraSetImageResolutionEx);
	GET_MVSDK_API(CameraGetMediaType);
	GET_MVSDK_API(CameraSetMediaType);
	GET_MVSDK_API(CameraSetAeState);
	GET_MVSDK_API(CameraGetAeState);
	GET_MVSDK_API(CameraSetAeTarget);
	GET_MVSDK_API(CameraGetAeTarget);
	GET_MVSDK_API(CameraSetAeExposureRange);
	GET_MVSDK_API(CameraGetAeExposureRange);
	GET_MVSDK_API(CameraSetAeAnalogGainRange);
	GET_MVSDK_API(CameraGetAeAnalogGainRange);
	GET_MVSDK_API(CameraSetAeThreshold);
	GET_MVSDK_API(CameraGetAeThreshold);
	GET_MVSDK_API(CameraIsAeWinVisible);
	GET_MVSDK_API(CameraSetExposureTime);
	GET_MVSDK_API(CameraGetExposureTime);
	GET_MVSDK_API(CameraGetExposureTimeRange);
	GET_MVSDK_API(CameraGetExposureLineTime);
	GET_MVSDK_API(CameraSetAnalogGain);
	GET_MVSDK_API(CameraGetAnalogGain);
	GET_MVSDK_API(CameraSetSharpness);
	GET_MVSDK_API(CameraGetSharpness);
	GET_MVSDK_API(CameraSetOnceWB);
	GET_MVSDK_API(CameraSetLutMode);
	GET_MVSDK_API(CameraGetLutMode);
	GET_MVSDK_API(CameraSelectLutPreset);
	GET_MVSDK_API(CameraGetLutPresetSel);
	GET_MVSDK_API(CameraSetCustomLut);  
	GET_MVSDK_API(CameraGetCustomLut);
	GET_MVSDK_API(CameraGetCurrentLut);
	GET_MVSDK_API(CameraSetWbMode);
	GET_MVSDK_API(CameraGetWbMode);
	GET_MVSDK_API(CameraSetWbWindow);
	GET_MVSDK_API(CameraIsWbWinVisible);
	GET_MVSDK_API(CameraSaveImage);
	GET_MVSDK_API(CameraSetGain);
	GET_MVSDK_API(CameraGetGain);
	GET_MVSDK_API(CameraSetGamma);
	GET_MVSDK_API(CameraGetGamma);
	GET_MVSDK_API(CameraSetSaturation);
	GET_MVSDK_API(CameraGetSaturation);
	GET_MVSDK_API(CameraSetContrast);
	GET_MVSDK_API(CameraGetContrast);
	GET_MVSDK_API(CameraSetFrameSpeed);
	GET_MVSDK_API(CameraGetFrameSpeed);
	GET_MVSDK_API(CameraSetAntiFlick);
	GET_MVSDK_API(CameraGetAntiFlick);
	GET_MVSDK_API(CameraGetLightFrequency);
	GET_MVSDK_API(CameraSetLightFrequency);
	GET_MVSDK_API(CameraSetTransPackLen);
	GET_MVSDK_API(CameraGetTransPackLen);
	GET_MVSDK_API(CameraWriteSN);
	GET_MVSDK_API(CameraReadSN);
	GET_MVSDK_API(CameraGetPresetClrTemp);
	GET_MVSDK_API(CameraSetPresetClrTemp);
	GET_MVSDK_API(CameraSaveParameter);
	GET_MVSDK_API(CameraLoadParameter);
	GET_MVSDK_API(CameraGetCurrentParameterGroup);
	GET_MVSDK_API(CameraEnumerateDevice);
	GET_MVSDK_API(CameraEnumerateDeviceEx);
	GET_MVSDK_API(CameraGetCapability);
	GET_MVSDK_API(CameraSoftTrigger);
	GET_MVSDK_API(CameraSetTriggerMode);
	GET_MVSDK_API(CameraGetTriggerMode);
	GET_MVSDK_API(CameraShowSettingPage);
	GET_MVSDK_API(CameraGetFrameStatistic);
	GET_MVSDK_API(CameraGetResolutionForSnap);
	GET_MVSDK_API(CameraSetResolutionForSnap);
	GET_MVSDK_API(CameraGetNoiseFilterState);
	GET_MVSDK_API(CameraSetParameterMode);
	GET_MVSDK_API(CameraGetParameterMode);
	GET_MVSDK_API(CameraSetParameterMask);
	GET_MVSDK_API(CameraGetTriggerCount);
	GET_MVSDK_API(CameraGetCrossLine);
	GET_MVSDK_API(CameraSetCrossLine);
	GET_MVSDK_API(CameraGetTriggerDelayTime);
	GET_MVSDK_API(CameraSetTriggerDelayTime);
	GET_MVSDK_API(CameraSetAeWinVisible);
	GET_MVSDK_API(CameraSetNoiseFilter);
	GET_MVSDK_API(CameraSetTriggerCount);
	GET_MVSDK_API(CameraCustomizeReferWin);
	GET_MVSDK_API(CameraSetAeWindow);
	GET_MVSDK_API(CameraReadParameterFromFile);
	GET_MVSDK_API(CameraSetWbWinVisible);
	GET_MVSDK_API(CameraRstTimeStamp);
	GET_MVSDK_API(CameraGetCapabilityEx);
	GET_MVSDK_API(CameraLoadUserData);
	GET_MVSDK_API(CameraSaveUserData);
	GET_MVSDK_API(CameraIsOpened);
	GET_MVSDK_API(CameraSetFriendlyName);
	GET_MVSDK_API(CameraGetFriendlyName);
	GET_MVSDK_API(CameraSetUserClrTempGain);
	GET_MVSDK_API(CameraGetUserClrTempGain);
	GET_MVSDK_API(CameraSetUserClrTempMatrix);
	GET_MVSDK_API(CameraGetUserClrTempMatrix);
	GET_MVSDK_API(CameraSetClrTempMode);
	GET_MVSDK_API(CameraGetClrTempMode);
	GET_MVSDK_API(CameraSdkGetVersionString);
	GET_MVSDK_API(CameraCheckFwUpdate);
	GET_MVSDK_API(CameraGetFirmwareVision);
	GET_MVSDK_API(CameraGetEnumInfo);
	GET_MVSDK_API(CameraGetInerfaceVersion);
	GET_MVSDK_API(CameraSetIOState);
	GET_MVSDK_API(CameraGetIOState);
	GET_MVSDK_API(CameraSetInPutIOMode);
	GET_MVSDK_API(CameraSetOutPutIOMode);
	GET_MVSDK_API(CameraSetOutPutPWM);
	CameraSetBayerDecAlgorithm = (_CameraSetBayerDecAlgorithm)GetProcAddress(ghSDK, "_CameraSetBayerDecAlgorithm@12");
	CHECK_API(CameraSetBayerDecAlgorithm);
	GET_MVSDK_API(CameraGetBayerDecAlgorithm);
	GET_MVSDK_API(CameraSetBlackLevel);
	GET_MVSDK_API(CameraGetBlackLevel);
	GET_MVSDK_API(CameraSetWhiteLevel);
	GET_MVSDK_API(CameraGetWhiteLevel);
	GET_MVSDK_API(CameraSetIspOutFormat);
	GET_MVSDK_API(CameraGetIspOutFormat);
	GET_MVSDK_API(CameraSetStrobeMode);
	GET_MVSDK_API(CameraGetStrobeMode);
	GET_MVSDK_API(CameraSetStrobeDelayTime);
	GET_MVSDK_API(CameraGetStrobeDelayTime);
	GET_MVSDK_API(CameraSetStrobePulseWidth);
	GET_MVSDK_API(CameraGetStrobePulseWidth);
	GET_MVSDK_API(CameraSetStrobePolarity);
	GET_MVSDK_API(CameraGetStrobePolarity);
	GET_MVSDK_API(CameraSetExtTrigSignalType);
	GET_MVSDK_API(CameraGetExtTrigSignalType);
	GET_MVSDK_API(CameraSetExtTrigShutterType);
	GET_MVSDK_API(CameraGetExtTrigShutterType);
	GET_MVSDK_API(CameraSetExtTrigDelayTime);
	GET_MVSDK_API(CameraGetExtTrigDelayTime);
	GET_MVSDK_API(CameraSetExtTrigJitterTime);
	GET_MVSDK_API(CameraGetExtTrigJitterTime);
	GET_MVSDK_API(CameraGetExtTrigCapability);
	GET_MVSDK_API(CameraGetErrorString);
	GET_MVSDK_API(CameraGetCapabilityEx2);
	GET_MVSDK_API(CameraGetImageBufferEx2);
	GET_MVSDK_API(CameraGetImageBufferEx3);
	GET_MVSDK_API(CameraReConnect);
	GET_MVSDK_API(CameraConnectTest);
	GET_MVSDK_API(CameraSetLedEnable);
	GET_MVSDK_API(CameraGetLedEnable);
	GET_MVSDK_API(CameraSetLedOnOff);
	GET_MVSDK_API(CameraGetLedOnOff);
	GET_MVSDK_API(CameraSetLedDuration);
	GET_MVSDK_API(CameraGetLedDuration);
	GET_MVSDK_API(CameraSetLedBrightness);
	GET_MVSDK_API(CameraGetLedBrightness);
	GET_MVSDK_API(CameraEnableTransferRoi);
	GET_MVSDK_API(CameraSetTransferRoi);
	GET_MVSDK_API(CameraGetTransferRoi);
	GET_MVSDK_API(CameraAlignMalloc);
	GET_MVSDK_API(CameraAlignFree);
	GET_MVSDK_API(CameraSetAutoConnect);
	GET_MVSDK_API(CameraGetAutoConnect);
	GET_MVSDK_API(CameraGetReConnectCounts);
	GET_MVSDK_API(CameraSetSingleGrabMode);
	GET_MVSDK_API(CameraGetSingleGrabMode);
	GET_MVSDK_API(CameraRestartGrab);
	GET_MVSDK_API(CameraDrawText);
	GET_MVSDK_API(CameraGigeGetIp);
	GET_MVSDK_API(CameraGigeSetIp);
	GET_MVSDK_API(CameraGigeGetMac);
	GET_MVSDK_API(CameraEnableFastResponse);
	GET_MVSDK_API(CameraSetCorrectDeadPixel);
	GET_MVSDK_API(CameraGetCorrectDeadPixel);
	GET_MVSDK_API(CameraFlatFieldingCorrectSetEnable);
	GET_MVSDK_API(CameraFlatFieldingCorrectGetEnable);
	GET_MVSDK_API(CameraFlatFieldingCorrectSetParameter);
	GET_MVSDK_API(CameraFlatFieldingCorrectSaveParameterToFile);
	GET_MVSDK_API(CameraFlatFieldingCorrectLoadParameterFromFile);
	GET_MVSDK_API(CameraCommonCall);
	GET_MVSDK_API(CameraSetDenoise3DParams);
	GET_MVSDK_API(CameraGetDenoise3DParams);
	GET_MVSDK_API(CameraManualDenoise3D);
	GET_MVSDK_API(CameraCustomizeDeadPixels);
	GET_MVSDK_API(CameraReadDeadPixels);
	GET_MVSDK_API(CameraAddDeadPixels);
	GET_MVSDK_API(CameraRemoveDeadPixels);
	GET_MVSDK_API(CameraRemoveAllDeadPixels);
	GET_MVSDK_API(CameraSaveDeadPixels);
	GET_MVSDK_API(CameraSaveDeadPixelsToFile);
	GET_MVSDK_API(CameraLoadDeadPixelsFromFile);
	GET_MVSDK_API(CameraGetImageBufferPriority);
	GET_MVSDK_API(CameraGetImageBufferPriorityEx);
	GET_MVSDK_API(CameraGetImageBufferPriorityEx2);
	GET_MVSDK_API(CameraGetImageBufferPriorityEx3);
	GET_MVSDK_API(CameraClearBuffer);
	GET_MVSDK_API(CameraSoftTriggerEx);
	GET_MVSDK_API(CameraSetHDR);
	GET_MVSDK_API(CameraGetHDR);
	GET_MVSDK_API(CameraGetFrameID);
	GET_MVSDK_API(CameraGetFrameTimeStamp);

	GET_MVSDK_API(CameraGrabber_CreateFromDevicePage);
	GET_MVSDK_API(CameraGrabber_CreateByIndex);
	GET_MVSDK_API(CameraGrabber_CreateByName);
	GET_MVSDK_API(CameraGrabber_Create);
	GET_MVSDK_API(CameraGrabber_Destroy);
	GET_MVSDK_API(CameraGrabber_SetHWnd);
	GET_MVSDK_API(CameraGrabber_SetPriority);
	GET_MVSDK_API(CameraGrabber_StartLive);
	GET_MVSDK_API(CameraGrabber_StopLive);
	GET_MVSDK_API(CameraGrabber_SaveImage);
	GET_MVSDK_API(CameraGrabber_SaveImageAsync);
	GET_MVSDK_API(CameraGrabber_SaveImageAsyncEx);
	GET_MVSDK_API(CameraGrabber_SetSaveImageCompleteCallback);
	GET_MVSDK_API(CameraGrabber_SetFrameListener);
	GET_MVSDK_API(CameraGrabber_SetRawCallback);
	GET_MVSDK_API(CameraGrabber_SetRGBCallback);
	GET_MVSDK_API(CameraGrabber_GetCameraHandle);
	GET_MVSDK_API(CameraGrabber_GetStat);
	GET_MVSDK_API(CameraGrabber_GetCameraDevInfo);

	GET_MVSDK_API(CameraImage_Create);
	GET_MVSDK_API(CameraImage_Destroy);
	GET_MVSDK_API(CameraImage_GetData);
	GET_MVSDK_API(CameraImage_GetUserData);
	GET_MVSDK_API(CameraImage_SetUserData);
	GET_MVSDK_API(CameraImage_IsEmpty);
	GET_MVSDK_API(CameraImage_Draw);
	GET_MVSDK_API(CameraImage_BitBlt);
	GET_MVSDK_API(CameraImage_SaveAsBmp);
	GET_MVSDK_API(CameraImage_SaveAsJpeg);
	GET_MVSDK_API(CameraImage_SaveAsPng);
	GET_MVSDK_API(CameraImage_SaveAsRaw);

#undef GET_MVSDK_API

	return CAMERA_STATUS_SUCCESS;
}

CameraSdkStatus UnloadCameraSdk()
{
	if (ghSDK)
	{
		FreeLibrary(ghSDK); 
	} 
	return CAMERA_STATUS_SUCCESS;
}
#endif

#endif
