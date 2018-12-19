#ifndef _MVCAMAPI_H_
#define _MVCAMAPI_H_


#ifdef DLL_EXPORT
#define MVSDK_API extern "C" __declspec (dllexport)
#else
#define MVSDK_API extern "C" __declspec (dllimport)
#endif

#include "CameraDefine.h"
#include "CameraStatus.h"

// BIG5 TRANS ALLOWED

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
MVSDK_API CameraSdkStatus __stdcall CameraSdkInit (
                                                   int iLanguageSel
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
MVSDK_API CameraSdkStatus __stdcall CameraEnumerateDevice (
                                                           tSdkCameraDevInfo * pCameraList,
                                                           INT * piNums
                                                          );


/***************************************************** *****/
// function name: CameraEnumerateDeviceEx
// Function Description: Enumerate the device and create a device list. Call CameraInitEx
// You must call this function before enumerating the device.
// parameters:
// Return value: return the number of devices, 0 means no.
/***************************************************** *****/
MVSDK_API INT __stdcall CameraEnumerateDeviceEx (
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
MVSDK_API CameraSdkStatus __stdcall CameraIsOpened (
                                                    tSdkCameraDevInfo * pCameraList,
                                                    BOOL * pOpened
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
MVSDK_API CameraSdkStatus __stdcall CameraInit (
                                                tSdkCameraDevInfo * pCameraInfo,
                                                int emParamLoadMode,
                                                int emTeam,
                                                CameraHandle * pCameraHandle
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
MVSDK_API CameraSdkStatus __stdcall CameraInitEx (
                                                  int iDeviceIndex,
                                                  int iParamLoadMode,
                                                  int emTeam,
                                                  CameraHandle * pCameraHandle
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
MVSDK_API CameraSdkStatus __stdcall CameraInitEx2 (
                                                   char * CameraName,
                                                   CameraHandle * pCameraHandle
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
MVSDK_API CameraSdkStatus __stdcall CameraSetCallbackFunction (
                                                               CameraHandle hCamera,
                                                               CAMERA_SNAP_PROC pCallBack,
                                                               PVOID pContext,
                                                               CAMERA_SNAP_PROC * pCallbackOld
                                                              );

/***************************************************** *****/
// function name: CameraUnInit
// Function Description: Camera anti-initialization. Release resources.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
MVSDK_API CameraSdkStatus __stdcall CameraUnInit (
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
MVSDK_API CameraSdkStatus __stdcall CameraGetInformation (
                                                          CameraHandle hCamera,
                                                          char ** pbuffer
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
MVSDK_API CameraSdkStatus __stdcall CameraImageProcess (
                                                        CameraHandle hCamera,
                                                        BYTE * pbyIn,
                                                        BYTE * pbyOut,
                                                        tSdkFrameHead * pFrInfo
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
MVSDK_API CameraSdkStatus __stdcall CameraImageProcessEx (
                                                          CameraHandle hCamera,
                                                          BYTE * pbyIn,
                                                          BYTE * pbyOut,
                                                          tSdkFrameHead * pFrInfo,
                                                          UINT uOutFormat,
                                                          UINT uReserved
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
MVSDK_API CameraSdkStatus __stdcall CameraDisplayInit (
                                                       CameraHandle hCamera,
                                                       HWND hWndDisplay
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
MVSDK_API CameraSdkStatus __stdcall CameraDisplayRGB24 (
                                                        CameraHandle hCamera,
                                                        BYTE * pbyRGB24,
                                                        tSdkFrameHead * pFrInfo
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
MVSDK_API CameraSdkStatus __stdcall CameraSetDisplayMode (
                                                          CameraHandle hCamera,
                                                          INT iMode
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
MVSDK_API CameraSdkStatus __stdcall CameraSetDisplayOffset (
                                                            CameraHandle hCamera,
                                                            int iOffsetX,
                                                            int iOffsetY
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
MVSDK_API CameraSdkStatus __stdcall CameraSetDisplaySize (
                                                          CameraHandle hCamera,
                                                          INT iWidth,
                                                          INT iHeight
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
MVSDK_API CameraSdkStatus __stdcall CameraGetImageBuffer (
                                                          CameraHandle hCamera,
                                                          tSdkFrameHead * pFrameInfo,
                                                          BYTE ** pbyBuffer,
                                                          UINT wTimes
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
MVSDK_API unsigned char * __stdcall CameraGetImageBufferEx (
                                                            CameraHandle hCamera,
                                                            INT * piWidth,
                                                            INT * piHeight,
                                                            UINT wTimes
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
MVSDK_API CameraSdkStatus __stdcall CameraSnapToBuffer (
                                                        CameraHandle hCamera,
                                                        tSdkFrameHead * pFrameInfo,
                                                        BYTE ** pbyBuffer,
                                                        UINT uWaitTimeMs
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
MVSDK_API CameraSdkStatus __stdcall CameraReleaseImageBuffer (
                                                              CameraHandle hCamera,
                                                              BYTE * pbyBuffer
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
MVSDK_API CameraSdkStatus __stdcall CameraPlay (
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
MVSDK_API CameraSdkStatus __stdcall CameraPause (
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
MVSDK_API CameraSdkStatus __stdcall CameraStop (
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
MVSDK_API CameraSdkStatus __stdcall CameraInitRecord (
                                                      CameraHandle hCamera,
                                                      int iFormat,
                                                      char * pcSavePath,
                                                      BOOL b2GLimit,
                                                      DWORD dwQuality,
                                                      int iFrameRate
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
MVSDK_API CameraSdkStatus __stdcall CameraStopRecord (
                                                      CameraHandle hCamera
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
MVSDK_API CameraSdkStatus __stdcall CameraPushFrame (
                                                     CameraHandle hCamera,
                                                     BYTE * pbyImageBuffer,
                                                     tSdkFrameHead * pFrInfo
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
MVSDK_API CameraSdkStatus __stdcall CameraSaveImage (
                                                     CameraHandle hCamera,
                                                     char * lpszFileName,
                                                     BYTE * pbyImageBuffer,
                                                     tSdkFrameHead * pFrInfo,
                                                     UINT byFileType,
                                                     BYTE byQuality
                                                    );

/***************************************************** *****/
// function name: CameraSaveImageEx
// Function Description: The image buffer data saved as a picture file.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// lpszFileName save the file the full path to the picture.
// pbyImageBuffer image data buffer.
// uImageFormat 0: 8 BIT gray 1: rgb24 2: rgba32 3: bgr24 4: bgra32
// iWidth picture width
// iHeight picture height
// byFileType The format for saving the image. See CameraDefine.h for the range of values
// The type definition of emSdkFileType. Currently supported
// BMP, JPG, PNG, RAW four formats. One of the RAW said
// Raw data output by the camera, save the RAW format file requirements
// pbyImageBuffer and pFrInfo are CameraGetImageBuffer
// Get the data, and without CameraImageProcess conversion
// Into BMP format; the other hand, if you want to save into BMP, JPG or
// PNG format, then pbyImageBuffer is
// CameraImageProcess processed RGB format data.
// Specific usage can refer to Advanced routines.
// byQuality The image quality factor saved, only when saved as JPG
// This parameter is valid from 1 to 100. The rest of the format
// can be written as 0
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
MVSDK_API CameraSdkStatus __stdcall CameraSaveImageEx (
                                                       CameraHandle hCamera,
                                                       char * lpszFileName,
                                                       BYTE * pbyImageBuffer,
                                                       UINT uImageFormat,
                                                       int iWidth,
                                                       int iHeight,
                                                       UINT byFileType,
                                                       BYTE byQuality
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
MVSDK_API CameraSdkStatus __stdcall CameraGetImageResolution (
                                                              CameraHandle hCamera,
                                                              tSdkImageResolution * psCurVideoSize
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
MVSDK_API CameraSdkStatus __stdcall CameraGetImageResolutionEx (
                                                                CameraHandle hCamera,
                                                                int * iIndex,
                                                                char acDescription [32],
                                                                int * Mode,
                                                                UINT * ModeSize,
                                                                int * x,
                                                                int * y,
                                                                int * width,
                                                                int * height,
                                                                int * ZoomWidth,
                                                                int * ZoomHeight
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
MVSDK_API CameraSdkStatus __stdcall CameraSetImageResolution (
                                                              CameraHandle hCamera,
                                                              tSdkImageResolution * pImageResolution
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
MVSDK_API CameraSdkStatus __stdcall CameraSetImageResolutionEx (
                                                                CameraHandle hCamera,
                                                                int iIndex,
                                                                int Mode,
                                                                UINT ModeSize,
                                                                int x,
                                                                int y,
                                                                int width,
                                                                int height,
                                                                int ZoomWidth,
                                                                int ZoomHeight
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
MVSDK_API CameraSdkStatus __stdcall CameraGetMediaType (
                                                        CameraHandle hCamera,
                                                        INT * piMediaType
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
MVSDK_API CameraSdkStatus __stdcall CameraSetMediaType (
                                                        CameraHandle hCamera,
                                                        INT iMediaType
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
MVSDK_API CameraSdkStatus __stdcall CameraSetAeState (
                                                      CameraHandle hCamera,
                                                      BOOL bAeState
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
MVSDK_API CameraSdkStatus __stdcall CameraGetAeState (
                                                      CameraHandle hCamera,
                                                      BOOL * pAeState
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
MVSDK_API CameraSdkStatus __stdcall CameraSetSharpness (
                                                        CameraHandle hCamera,
                                                        int iSharpness
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
MVSDK_API CameraSdkStatus __stdcall CameraGetSharpness (
                                                        CameraHandle hCamera,
                                                        int * piSharpness
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
MVSDK_API CameraSdkStatus __stdcall CameraSetLutMode (
                                                      CameraHandle hCamera,
                                                      int emLutMode
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
MVSDK_API CameraSdkStatus __stdcall CameraGetLutMode (
                                                      CameraHandle hCamera,
                                                      int * pemLutMode
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
MVSDK_API CameraSdkStatus __stdcall CameraSelectLutPreset (
                                                           CameraHandle hCamera,
                                                           int iSel
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
MVSDK_API CameraSdkStatus __stdcall CameraGetLutPresetSel (
                                                           CameraHandle hCamera,
                                                           int * piSel
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
MVSDK_API CameraSdkStatus __stdcall CameraSetCustomLut (
                                                        CameraHandle hCamera,
                                                        int iChannel,
                                                        USHORT * pLut
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
MVSDK_API CameraSdkStatus __stdcall CameraGetCustomLut (
                                                        CameraHandle hCamera,
                                                        int iChannel,
                                                        USHORT * pLut
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
MVSDK_API CameraSdkStatus __stdcall CameraGetCurrentLut (
                                                         CameraHandle hCamera,
                                                         int iChannel,
                                                         USHORT * pLut
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
MVSDK_API CameraSdkStatus __stdcall CameraSetWbMode (
                                                     CameraHandle hCamera,
                                                     BOOL bAuto
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
MVSDK_API CameraSdkStatus __stdcall CameraGetWbMode (
                                                     CameraHandle hCamera,
                                                     BOOL * pbAuto
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
MVSDK_API CameraSdkStatus __stdcall CameraSetPresetClrTemp (
                                                            CameraHandle hCamera,
                                                            int iSel
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
MVSDK_API CameraSdkStatus __stdcall CameraGetPresetClrTemp (
                                                            CameraHandle hCamera,
                                                            int * piSel
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
MVSDK_API CameraSdkStatus __stdcall CameraSetUserClrTempGain (
                                                              CameraHandle hCamera,
                                                              int iRgain,
                                                              int iGgain,
                                                              int iBgain
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
MVSDK_API CameraSdkStatus __stdcall CameraGetUserClrTempGain (
                                                              CameraHandle hCamera,
                                                              int * piRgain,
                                                              int * piGgain,
                                                              int * piBgain
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
MVSDK_API CameraSdkStatus __stdcall CameraSetUserClrTempMatrix (
                                                                CameraHandle hCamera,
                                                                float * pMatrix
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
MVSDK_API CameraSdkStatus __stdcall CameraGetUserClrTempMatrix (
                                                                CameraHandle hCamera,
                                                                float * pMatrix
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
MVSDK_API CameraSdkStatus __stdcall CameraSetClrTempMode (
                                                          CameraHandle hCamera,
                                                          int iMode
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
MVSDK_API CameraSdkStatus __stdcall CameraGetClrTempMode (
                                                          CameraHandle hCamera,
                                                          int * pimode
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
MVSDK_API CameraSdkStatus __stdcall CameraSetOnceWB (
                                                     CameraHandle hCamera
                                                    );

/***************************************************** *****/
// function name: CameraSetOnceBB
// Function Description: Perform a black balance operation.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
MVSDK_API CameraSdkStatus __stdcall CameraSetOnceBB (
                                                     CameraHandle hCamera
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
MVSDK_API CameraSdkStatus __stdcall CameraSetAeTarget (
                                                       CameraHandle hCamera,
                                                       int iAeTarget
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
MVSDK_API CameraSdkStatus __stdcall CameraGetAeTarget (
                                                       CameraHandle hCamera,
                                                       int * piAeTarget
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
MVSDK_API CameraSdkStatus __stdcall CameraSetAeExposureRange (
                                                              CameraHandle hCamera,
                                                              double fMinExposureTime,
                                                              double fMaxExposureTime
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
MVSDK_API CameraSdkStatus __stdcall CameraGetAeExposureRange (
                                                              CameraHandle hCamera,
                                                              double * fMinExposureTime,
                                                              double * fMaxExposureTime
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
MVSDK_API CameraSdkStatus __stdcall CameraSetAeAnalogGainRange (
                                                                CameraHandle hCamera,
                                                                int iMinAnalogGain,
                                                                int iMaxAnalogGain
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
MVSDK_API CameraSdkStatus __stdcall CameraGetAeAnalogGainRange (
                                                                CameraHandle hCamera,
                                                                int * iMinAnalogGain,
                                                                int * iMaxAnalogGain
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
MVSDK_API CameraSdkStatus __stdcall CameraSetAeThreshold (
                                                          CameraHandle hCamera,
                                                          int iThreshold
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
MVSDK_API CameraSdkStatus __stdcall CameraGetAeThreshold (
                                                          CameraHandle hCamera,
                                                          int * iThreshold
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
MVSDK_API CameraSdkStatus __stdcall CameraSetExposureTime (
                                                           CameraHandle hCamera,
                                                           double fExposureTime
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
MVSDK_API CameraSdkStatus __stdcall CameraGetExposureLineTime (
                                                               CameraHandle hCamera,
                                                               double * pfLineTime
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
MVSDK_API CameraSdkStatus __stdcall CameraGetExposureTime (
                                                           CameraHandle hCamera,
                                                           double * pfExposureTime
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
MVSDK_API CameraSdkStatus __stdcall CameraGetExposureTimeRange (
                                                                CameraHandle hCamera,
                                                                double * pfMin,
                                                                double * pfMax,
                                                                double * pfStep
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
MVSDK_API CameraSdkStatus __stdcall CameraSetAnalogGain (
                                                         CameraHandle hCamera,
                                                         INT iAnalogGain
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
MVSDK_API CameraSdkStatus __stdcall CameraGetAnalogGain (
                                                         CameraHandle hCamera,
                                                         INT * piAnalogGain
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
MVSDK_API CameraSdkStatus __stdcall CameraSetGain (
                                                   CameraHandle hCamera,
                                                   int iRGain,
                                                   int iGGain,
                                                   int iBGain
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
MVSDK_API CameraSdkStatus __stdcall CameraGetGain (
                                                   CameraHandle hCamera,
                                                   int * piRGain,
                                                   int * piGGain,
                                                   int * piBGain
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
MVSDK_API CameraSdkStatus __stdcall CameraSetGamma (
                                                    CameraHandle hCamera,
                                                    int iGamma
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
MVSDK_API CameraSdkStatus __stdcall CameraGetGamma (
                                                    CameraHandle hCamera,
                                                    int * piGamma
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
MVSDK_API CameraSdkStatus __stdcall CameraSetContrast (
                                                       CameraHandle hCamera,
                                                       int iContrast
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
MVSDK_API CameraSdkStatus __stdcall CameraGetContrast (
                                                       CameraHandle hCamera,
                                                       int * piContrast
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
MVSDK_API CameraSdkStatus __stdcall CameraSetSaturation (
                                                         CameraHandle hCamera,
                                                         int iSaturation
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
MVSDK_API CameraSdkStatus __stdcall CameraGetSaturation (
                                                         CameraHandle hCamera,
                                                         int * piSaturation
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
MVSDK_API CameraSdkStatus __stdcall CameraSetMonochrome (
                                                         CameraHandle hCamera,
                                                         BOOL bEnable
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
MVSDK_API CameraSdkStatus __stdcall CameraGetMonochrome (
                                                         CameraHandle hCamera,
                                                         BOOL * pbEnable
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
MVSDK_API CameraSdkStatus __stdcall CameraSetInverse (
                                                      CameraHandle hCamera,
                                                      BOOL bEnable
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
MVSDK_API CameraSdkStatus __stdcall CameraGetInverse (
                                                      CameraHandle hCamera,
                                                      BOOL * pbEnable
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
MVSDK_API CameraSdkStatus __stdcall CameraSetAntiFlick (
                                                        CameraHandle hCamera,
                                                        BOOL bEnable
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
MVSDK_API CameraSdkStatus __stdcall CameraGetAntiFlick (
                                                        CameraHandle hCamera,
                                                        BOOL * pbEnable
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
MVSDK_API CameraSdkStatus __stdcall CameraGetLightFrequency (
                                                             CameraHandle hCamera,
                                                             int * piFrequencySel
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
MVSDK_API CameraSdkStatus __stdcall CameraSetLightFrequency (
                                                             CameraHandle hCamera,
                                                             int iFrequencySel
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
MVSDK_API CameraSdkStatus __stdcall CameraSetFrameSpeed (
                                                         CameraHandle hCamera,
                                                         int iFrameSpeed
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
MVSDK_API CameraSdkStatus __stdcall CameraGetFrameSpeed (
                                                         CameraHandle hCamera,
                                                         int * piFrameSpeed
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
MVSDK_API CameraSdkStatus __stdcall CameraSetParameterMode (
                                                            CameraHandle hCamera,
                                                            int iMode
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
MVSDK_API CameraSdkStatus __stdcall CameraGetParameterMode (
                                                            CameraHandle hCamera,
                                                            int * piTarget
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
MVSDK_API CameraSdkStatus __stdcall CameraSetParameterMask (
                                                            CameraHandle hCamera,
                                                            UINT uMask
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
MVSDK_API CameraSdkStatus __stdcall CameraSaveParameter (
                                                         CameraHandle hCamera,
                                                         int iTeam
                                                        );


/***************************************************** *****/
// function name: CameraSaveParameterToFile
// Function Description: Save the current camera parameters to the specified file. The file can be copied to
// Other computers for other cameras to load, you can also do parameter backup.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// The full path to the sFileName parameter file.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
MVSDK_API CameraSdkStatus __stdcall CameraSaveParameterToFile (
                                                               CameraHandle hCamera,
                                                               char * sFileName
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
MVSDK_API CameraSdkStatus __stdcall CameraReadParameterFromFile (
                                                                 CameraHandle hCamera,
                                                                 char * sFileName
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
MVSDK_API CameraSdkStatus __stdcall CameraLoadParameter (
                                                         CameraHandle hCamera,
                                                         int iTeam
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
MVSDK_API CameraSdkStatus __stdcall CameraGetCurrentParameterGroup (
                                                                    CameraHandle hCamera,
                                                                    int * piTeam
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
MVSDK_API CameraSdkStatus __stdcall CameraSetTransPackLen (
                                                           CameraHandle hCamera,
                                                           INT iPackSel
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
MVSDK_API CameraSdkStatus __stdcall CameraGetTransPackLen (
                                                           CameraHandle hCamera,
                                                           INT * piPackSel
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
MVSDK_API CameraSdkStatus __stdcall CameraIsAeWinVisible (
                                                          CameraHandle hCamera,
                                                          BOOL * pbIsVisible
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
MVSDK_API CameraSdkStatus __stdcall CameraSetAeWinVisible (
                                                           CameraHandle hCamera,
                                                           BOOL bIsVisible
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
MVSDK_API CameraSdkStatus __stdcall CameraGetAeWindow (
                                                       CameraHandle hCamera,
                                                       INT * piHOff,
                                                       INT * piVOff,
                                                       INT * piWidth,
                                                       INT * piHeight
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
MVSDK_API CameraSdkStatus __stdcall CameraSetAeWindow (
                                                       CameraHandle hCamera,
                                                       int iHOff,
                                                       int iVOff,
                                                       int iWidth,
                                                       int iHeight
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
MVSDK_API CameraSdkStatus __stdcall CameraSetMirror (
                                                     CameraHandle hCamera,
                                                     int iDir,
                                                     BOOL bEnable
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
MVSDK_API CameraSdkStatus __stdcall CameraGetMirror (
                                                     CameraHandle hCamera,
                                                     int iDir,
                                                     BOOL * pbEnable
                                                    );

/***************************************************** *****/
// function name: CameraSetRotate
// Function Description: Set the image rotation operation
// parameters: hCamera camera handle, obtained by the CameraInit function.
// iRot indicates the rotation angle (counterclockwise) (0: no rotation 1: 90 degrees 2: 180 degrees 3: 270 degrees)
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
MVSDK_API CameraSdkStatus __stdcall CameraSetRotate (
                                                     CameraHandle hCamera,
                                                     int iRot
                                                    );

/***************************************************** *****/
// function name: CameraGetRotate
// Function Description: Get the rotation status of the image.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// iRot indicates the direction of rotation to be obtained.
// (counterclockwise) (0: do not rotate 1:90 degrees 2: 180 degrees 3: 270 degrees)
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
MVSDK_API CameraSdkStatus __stdcall CameraGetRotate (
                                                     CameraHandle hCamera,
                                                     int * iRot
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
MVSDK_API CameraSdkStatus __stdcall CameraGetWbWindow (
                                                       CameraHandle hCamera,
                                                       INT * PiHOff,
                                                       INT * PiVOff,
                                                       INT * PiWidth,
                                                       INT * PiHeight
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
MVSDK_API CameraSdkStatus __stdcall CameraSetWbWindow (
                                                       CameraHandle hCamera,
                                                       INT iHOff,
                                                       INT iVOff,
                                                       INT iWidth,
                                                       INT iHeight
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
MVSDK_API CameraSdkStatus __stdcall CameraIsWbWinVisible (
                                                          CameraHandle hCamera,
                                                          BOOL * pbShow
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
MVSDK_API CameraSdkStatus __stdcall CameraSetWbWinVisible (
                                                           CameraHandle hCamera,
                                                           BOOL bShow
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
MVSDK_API CameraSdkStatus __stdcall CameraImageOverlay (
                                                        CameraHandle hCamera,
                                                        BYTE * pRgbBuffer,
                                                        tSdkFrameHead * pFrInfo
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
MVSDK_API CameraSdkStatus __stdcall CameraSetCrossLine (
                                                        CameraHandle hCamera,
                                                        int iLine,
                                                        INT x,
                                                        INT y,
                                                        UINT uColor,
                                                        BOOL bVisible
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
MVSDK_API CameraSdkStatus __stdcall CameraGetCrossLine (
                                                        CameraHandle hCamera,
                                                        INT iLine,
                                                        INT * px,
                                                        INT * py,
                                                        UINT * pcolor,
                                                        BOOL * pbVisible
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
MVSDK_API CameraSdkStatus __stdcall CameraGetCapability (
                                                         CameraHandle hCamera,
                                                         tSdkCameraCapbility * pCameraInfo
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
MVSDK_API CameraSdkStatus __stdcall CameraGetCapabilityEx (
                                                           char * sDeviceModel,
                                                           tSdkCameraCapbility * pCameraInfo,
                                                           PVOID hCameraHandle
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
MVSDK_API CameraSdkStatus __stdcall CameraWriteSN (
                                                   CameraHandle hCamera,
                                                   BYTE * pbySN,
                                                   INT iLevel
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
MVSDK_API CameraSdkStatus __stdcall CameraReadSN (
                                                  CameraHandle hCamera,
                                                  BYTE * pbySN,
                                                  INT iLevel
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
MVSDK_API CameraSdkStatus __stdcall CameraSetTriggerDelayTime (
                                                               CameraHandle hCamera,
                                                               UINT uDelayTimeUs
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
MVSDK_API CameraSdkStatus __stdcall CameraGetTriggerDelayTime (
                                                               CameraHandle hCamera,
                                                               UINT * puDelayTimeUs
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
MVSDK_API CameraSdkStatus __stdcall CameraSetTriggerCount (
                                                           CameraHandle hCamera,
                                                           INT iCount
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
MVSDK_API CameraSdkStatus __stdcall CameraGetTriggerCount (
                                                           CameraHandle hCamera,
                                                           INT * piCount
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
MVSDK_API CameraSdkStatus __stdcall CameraSoftTrigger (
                                                       CameraHandle hCamera
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
MVSDK_API CameraSdkStatus __stdcall CameraSetTriggerMode (
                                                          CameraHandle hCamera,
                                                          int iModeSel
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
MVSDK_API CameraSdkStatus __stdcall CameraGetTriggerMode (
                                                          CameraHandle hCamera,
                                                          INT * piModeSel
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
MVSDK_API CameraSdkStatus __stdcall CameraSetStrobeMode (
                                                         CameraHandle hCamera,
                                                         INT iMode
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
MVSDK_API CameraSdkStatus __stdcall CameraGetStrobeMode (
                                                         CameraHandle hCamera,
                                                         INT * piMode
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
MVSDK_API CameraSdkStatus __stdcall CameraSetStrobeDelayTime (
                                                              CameraHandle hCamera,
                                                              UINT uDelayTimeUs
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
MVSDK_API CameraSdkStatus __stdcall CameraGetStrobeDelayTime (
                                                              CameraHandle hCamera,
                                                              UINT * upDelayTimeUs
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
MVSDK_API CameraSdkStatus __stdcall CameraSetStrobePulseWidth (
                                                               CameraHandle hCamera,
                                                               UINT uTimeUs
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
MVSDK_API CameraSdkStatus __stdcall CameraGetStrobePulseWidth (
                                                               CameraHandle hCamera,
                                                               UINT * upTimeUs
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
MVSDK_API CameraSdkStatus __stdcall CameraSetStrobePolarity (
                                                             CameraHandle hCamera,
                                                             INT uPolarity
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
MVSDK_API CameraSdkStatus __stdcall CameraGetStrobePolarity (
                                                             CameraHandle hCamera,
                                                             INT * upPolarity
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
MVSDK_API CameraSdkStatus __stdcall CameraSetExtTrigSignalType (
                                                                CameraHandle hCamera,
                                                                INT iType
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
MVSDK_API CameraSdkStatus __stdcall CameraGetExtTrigSignalType (
                                                                CameraHandle hCamera,
                                                                INT * ipType
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
MVSDK_API CameraSdkStatus __stdcall CameraSetExtTrigShutterType (
                                                                 CameraHandle hCamera,
                                                                 INT iType
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
MVSDK_API CameraSdkStatus __stdcall CameraGetExtTrigShutterType (
                                                                 CameraHandle hCamera,
                                                                 INT * ipType
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
MVSDK_API CameraSdkStatus __stdcall CameraSetExtTrigDelayTime (
                                                               CameraHandle hCamera,
                                                               UINT uDelayTimeUs
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
MVSDK_API CameraSdkStatus __stdcall CameraGetExtTrigDelayTime (
                                                               CameraHandle hCamera,
                                                               UINT * upDelayTimeUs
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
MVSDK_API CameraSdkStatus __stdcall CameraSetExtTrigJitterTime (
                                                                CameraHandle hCamera,
                                                                UINT uTimeUs
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
MVSDK_API CameraSdkStatus __stdcall CameraGetExtTrigJitterTime (
                                                                CameraHandle hCamera,
                                                                UINT * upTimeUs
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
MVSDK_API CameraSdkStatus __stdcall CameraGetExtTrigCapability (
                                                                CameraHandle hCamera,
                                                                UINT * puCapabilityMask
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
MVSDK_API CameraSdkStatus __stdcall CameraGetResolutionForSnap (
                                                                CameraHandle hCamera,
                                                                tSdkImageResolution * pImageResolution
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
MVSDK_API CameraSdkStatus __stdcall CameraSetResolutionForSnap (
                                                                CameraHandle hCamera,
                                                                tSdkImageResolution * pImageResolution
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
MVSDK_API CameraSdkStatus __stdcall CameraCustomizeResolution (
                                                               CameraHandle hCamera,
                                                               tSdkImageResolution * pImageCustom
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
MVSDK_API CameraSdkStatus __stdcall CameraCustomizeReferWin (
                                                             CameraHandle hCamera,
                                                             INT iWinType,
                                                             HWND hParent,
                                                             INT * piHOff,
                                                             INT * piVOff,
                                                             INT * piWidth,
                                                             INT * piHeight
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
MVSDK_API CameraSdkStatus __stdcall CameraShowSettingPage (
                                                           CameraHandle hCamera,
                                                           BOOL bShow
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
MVSDK_API CameraSdkStatus __stdcall CameraCreateSettingPage (
                                                             CameraHandle hCamera,
                                                             HWND hParent,
                                                             char * pWinText,
                                                             CAMERA_PAGE_MSG_PROC pCallbackFunc,
                                                             PVOID pCallbackCtx,
                                                             UINT uReserved
                                                            );

/***************************************************** *****/
// function name: CameraCreateSettingPageEx
// Function Description: Create the camera's property configuration window. Call this function, SDK internal will
// Help you create a good camera configuration window, eliminating the need for you to re-develop the camera
// Configure the interface time. It is strongly recommended that you use this function to make
// SDK for you to create a good configuration window.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
MVSDK_API CameraSdkStatus __stdcall CameraCreateSettingPageEx (
                                                               CameraHandle hCamera
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
MVSDK_API CameraSdkStatus __stdcall CameraSetActiveSettingSubPage (
                                                                   CameraHandle hCamera,
                                                                   INT index
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
MVSDK_API CameraSdkStatus __stdcall CameraSpecialControl (
                                                          CameraHandle hCamera,
                                                          DWORD dwCtrlCode,
                                                          DWORD dwParam,
                                                          LPVOID lpData
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
MVSDK_API CameraSdkStatus __stdcall CameraGetFrameStatistic (
                                                             CameraHandle hCamera,
                                                             tSdkFrameStatistic * psFrameStatistic
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
MVSDK_API CameraSdkStatus __stdcall CameraSetNoiseFilter (
                                                          CameraHandle hCamera,
                                                          BOOL bEnable
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
MVSDK_API CameraSdkStatus __stdcall CameraGetNoiseFilterState (
                                                               CameraHandle hCamera,
                                                               BOOL * pEnable
                                                              );

/***************************************************** *****/
// function name: CameraRstTimeStamp
// Function Description: Reset the time stamp of the image acquisition, starting from 0.
// Parameters: CameraHandle hCamera
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
MVSDK_API CameraSdkStatus __stdcall CameraRstTimeStamp (
                                                        CameraHandle hCamera
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
MVSDK_API CameraSdkStatus __stdcall CameraSaveUserData (
                                                        CameraHandle hCamera,
                                                        UINT uStartAddr,
                                                        BYTE * pbData,
                                                        int ilen
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
MVSDK_API CameraSdkStatus __stdcall CameraLoadUserData (
                                                        CameraHandle hCamera,
                                                        UINT uStartAddr,
                                                        BYTE * pbData,
                                                        int ilen
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
MVSDK_API CameraSdkStatus __stdcall CameraGetFriendlyName (
                                                           CameraHandle hCamera,
                                                           char * pName
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
MVSDK_API CameraSdkStatus __stdcall CameraSetFriendlyName (
                                                           CameraHandle hCamera,
                                                           char * pName
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
MVSDK_API CameraSdkStatus __stdcall CameraSdkGetVersionString (
                                                               char * pVersionString
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
MVSDK_API CameraSdkStatus __stdcall CameraCheckFwUpdate (
                                                         CameraHandle hCamera,
                                                         BOOL * pNeedUpdate
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
MVSDK_API CameraSdkStatus __stdcall CameraGetFirmwareVision (
                                                             CameraHandle hCamera,
                                                             char * pVersion
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
MVSDK_API CameraSdkStatus __stdcall CameraGetEnumInfo (
                                                       CameraHandle hCamera,
                                                       tSdkCameraDevInfo * pCameraInfo
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
MVSDK_API CameraSdkStatus __stdcall CameraGetInerfaceVersion (
                                                              CameraHandle hCamera,
                                                              char * pVersion
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
MVSDK_API CameraSdkStatus __stdcall CameraSetIOState (
                                                      CameraHandle hCamera,
                                                      INT iOutputIOIndex,
                                                      UINT uState
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
MVSDK_API CameraSdkStatus __stdcall CameraGetIOState (
                                                      CameraHandle hCamera,
                                                      INT iInputIOIndex,
                                                      UINT * puState
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
MVSDK_API CameraSdkStatus __stdcall CameraSetInPutIOMode (
                                                          CameraHandle hCamera,
                                                          INT iInputIOIndex,
                                                          INT iMode
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
MVSDK_API CameraSdkStatus __stdcall CameraSetOutPutIOMode (
                                                           CameraHandle hCamera,
                                                           INT iOutputIOIndex,
                                                           INT iMode
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
MVSDK_API CameraSdkStatus __stdcall CameraSetOutPutPWM (
                                                        CameraHandle hCamera,
                                                        INT iOutputIOIndex,
                                                        UINT iCycle,
                                                        UINT uDuty
                                                       );

/***************************************************** *****/
// function name: CameraSetAeAlgorithm
// Function Description: Set the algorithm selected when auto exposure, different algorithms apply
// Different scenes.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// iIspProcessor Select the object that executes the algorithm, refer to CameraDefine.h
// emSdkIspProcessor definition
// iAeAlgorithmSel The algorithm number to select. Starting from 0, the maximum value is given by tSdkCameraCapbility
// iAeAlmSwDesc and iAeAlmHdDesc decisions.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
MVSDK_API CameraSdkStatus __stdcall CameraSetAeAlgorithm (
                                                          CameraHandle hCamera,
                                                          INT iIspProcessor,
                                                          INT iAeAlgorithmSel
                                                         );

/***************************************************** *****/
// function name: CameraGetAeAlgorithm
// Function Description: Get the current AE selected algorithm
// parameters: hCamera camera handle, obtained by the CameraInit function.
// iIspProcessor Select the object that executes the algorithm, refer to CameraDefine.h
// emSdkIspProcessor definition
// piAeAlgorithmSel returns the currently selected algorithm number. Starting from 0, the maximum value is given by tSdkCameraCapbility
// iAeAlmSwDesc and iAeAlmHdDesc decisions.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
MVSDK_API CameraSdkStatus __stdcall CameraGetAeAlgorithm (
                                                          CameraHandle hCamera,
                                                          INT iIspProcessor,
                                                          INT * piAlgorithmSel
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
MVSDK_API CameraSdkStatus __stdcall CameraSetBayerDecAlgorithm (
                                                                CameraHandle hCamera,
                                                                INT iIspProcessor,
                                                                INT iAlgorithmSel
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
MVSDK_API CameraSdkStatus __stdcall CameraGetBayerDecAlgorithm (
                                                                CameraHandle hCamera,
                                                                INT iIspProcessor,
                                                                INT * piAlgorithmSel
                                                               );

/***************************************************** *****/
// function name: CameraSetIspProcessor
// Function Description: Set the image processing unit algorithm execution object, by the PC side or the camera side
// to execute the algorithm which, when executed by the camera side, will reduce the PC-side CPU usage.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// iIspProcessor Reference CameraDefine.h
// emSdkIspProcessor definition.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
MVSDK_API CameraSdkStatus __stdcall CameraSetIspProcessor (
                                                           CameraHandle hCamera,
                                                           INT iIspProcessor
                                                          );

/***************************************************** *****/
// function name: CameraGetIspProcessor
// Function Description: get the image processing unit algorithm execution object.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// piIspProcessor returns the selected object, the return value reference CameraDefine.h
// emSdkIspProcessor definition.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
MVSDK_API CameraSdkStatus __stdcall CameraGetIspProcessor (
                                                           CameraHandle hCamera,
                                                           INT * piIspProcessor
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
MVSDK_API CameraSdkStatus __stdcall CameraSetBlackLevel (
                                                         CameraHandle hCamera,
                                                         INT iBlackLevel
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
MVSDK_API CameraSdkStatus __stdcall CameraGetBlackLevel (
                                                         CameraHandle hCamera,
                                                         INT * piBlackLevel
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
MVSDK_API CameraSdkStatus __stdcall CameraSetWhiteLevel (
                                                         CameraHandle hCamera,
                                                         INT iWhiteLevel
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
MVSDK_API CameraSdkStatus __stdcall CameraGetWhiteLevel (
                                                         CameraHandle hCamera,
                                                         INT * piWhiteLevel
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
MVSDK_API CameraSdkStatus __stdcall CameraSetIspOutFormat (
                                                           CameraHandle hCamera,
                                                           UINT uFormat
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
MVSDK_API CameraSdkStatus __stdcall CameraGetIspOutFormat (
                                                           CameraHandle hCamera,
                                                           UINT * puFormat
                                                          );

/***************************************************** *****/
// function name: CameraGetErrorString
// Function Description: Obtain the description string corresponding to the error code
// Parameters: iStatusCode error code. (Defined in CameraStatus.h)
// Return value: When successful, return the first address of the string corresponding to the error code.
// otherwise return NULL.
/***************************************************** *****/
MVSDK_API char * __stdcall CameraGetErrorString (
                                                 CameraSdkStatus iStatusCode
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
MVSDK_API CameraSdkStatus __stdcall CameraGetImageBufferEx2 (
                                                             CameraHandle hCamera,
                                                             BYTE * pImageData,
                                                             UINT uOutFormat,
                                                             int * piWidth,
                                                             int * piHeight,
                                                             UINT wTimes
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
MVSDK_API CameraSdkStatus __stdcall CameraGetImageBufferEx3 (
                                                             CameraHandle hCamera,
                                                             BYTE * pImageData,
                                                             UINT uOutFormat,
                                                             int * piWidth,
                                                             int * piHeight,
                                                             UINT * puTimeStamp,
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
MVSDK_API CameraSdkStatus __stdcall CameraGetCapabilityEx2 (
                                                            CameraHandle hCamera,
                                                            int * pMaxWidth,
                                                            int * pMaxHeight,
                                                            int * pbColorCamera
                                                           );

/***************************************************** *****/
// function name: CameraReConnect
// Function Description: Reconnect the device for USB device accidental disconnection after reconnection
// parameters: hCamera camera handle, obtained by the CameraInit function.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
MVSDK_API CameraSdkStatus __stdcall CameraReConnect (
                                                     CameraHandle hCamera
                                                    );


/***************************************************** *****/
// function name: CameraConnectTest
// Function Description: Test the camera's connection status, used to detect whether the camera dropped
// parameters: hCamera camera handle, obtained by the CameraInit function.
// Return Value: Upon successful, return CAMERA_STATUS_SUCCESS (0), said the camera connection status is normal;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
MVSDK_API CameraSdkStatus __stdcall CameraConnectTest (
                                                       CameraHandle hCamera
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
MVSDK_API CameraSdkStatus __stdcall CameraSetLedEnable (
                                                        CameraHandle hCamera,
                                                        int index,
                                                        BOOL enable
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
MVSDK_API CameraSdkStatus __stdcall CameraGetLedEnable (
                                                        CameraHandle hCamera,
                                                        int index,
                                                        BOOL * enable
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
MVSDK_API CameraSdkStatus __stdcall CameraSetLedOnOff (
                                                       CameraHandle hCamera,
                                                       int index,
                                                       BOOL onoff
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
MVSDK_API CameraSdkStatus __stdcall CameraGetLedOnOff (
                                                       CameraHandle hCamera,
                                                       int index,
                                                       BOOL * onoff
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
MVSDK_API CameraSdkStatus __stdcall CameraSetLedDuration (
                                                          CameraHandle hCamera,
                                                          int index,
                                                          UINT duration
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
MVSDK_API CameraSdkStatus __stdcall CameraGetLedDuration (
                                                          CameraHandle hCamera,
                                                          int index,
                                                          UINT * duration
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
MVSDK_API CameraSdkStatus __stdcall CameraSetLedBrightness (
                                                            CameraHandle hCamera,
                                                            int index,
                                                            UINT uBrightness
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
MVSDK_API CameraSdkStatus __stdcall CameraGetLedBrightness (
                                                            CameraHandle hCamera,
                                                            int index,
                                                            UINT * uBrightness
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
MVSDK_API CameraSdkStatus __stdcall CameraEnableTransferRoi (
                                                             CameraHandle hCamera,
                                                             UINT uEnableMask
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
MVSDK_API CameraSdkStatus __stdcall CameraSetTransferRoi (
                                                          CameraHandle hCamera,
                                                          int index,
                                                          UINT X1,
                                                          UINT Y1,
                                                          UINT X2,
                                                          UINT Y2
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
MVSDK_API CameraSdkStatus __stdcall CameraGetTransferRoi (
                                                          CameraHandle hCamera,
                                                          int index,
                                                          UINT * pX1,
                                                          UINT * pY1,
                                                          UINT * pX2,
                                                          UINT * pY2
                                                         );

/***************************************************** *****/
// function name: CameraAlignMalloc
// Function Description: Apply for a period of aligned memory space. Function and malloc similar, but
// The returned memory is aligned with the number of bytes specified by align.
// Parameters: size space size.
//             align align the number of bytes.
// Return value: on success, returns a non-zero value, said the memory address. Failed to return NULL.
/***************************************************** *****/
MVSDK_API BYTE * __stdcall CameraAlignMalloc (
                                              int size,
                                              int align
                                             );

/***************************************************** *****/
// function name: CameraAlignFree
// Function Description: Free up memory space requested by CameraAlignMalloc function.
// Parameters: membuffer The first memory address returned by CameraAlignMalloc.
// Return value: None.
/***************************************************** *****/
MVSDK_API void __stdcall CameraAlignFree (
                                          BYTE * membuffer
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
MVSDK_API CameraSdkStatus __stdcall CameraSetAutoConnect (CameraHandle hCamera, BOOL bEnable);

/***************************************************** *****/
// function name: CameraGetAutoConnect
// Function Description: Get auto-reconnect enabled
// parameters: hCamera camera handle, obtained by the CameraInit function.
// pbEnable returns camera reconnect enabled
// Return Value: Upon successful, return CAMERA_STATUS_SUCCESS (0), said the camera connection status is normal;
// This function returns CAMERA_STATUS_NOT_SUPPORTED (-4) for unsupported models that does not support
// Other non-zero value, refer to the definition of error code in CameraStatus.h.
/***************************************************** *****/
MVSDK_API CameraSdkStatus __stdcall CameraGetAutoConnect (CameraHandle hCamera, BOOL * pbEnable);

/***************************************************** *****/
// function name: CameraGetReConnectCounts
// Function Description: Get the number of times the camera automatically reconnects if CameraSetAutoConnect enables the camera to reconnect automatically. The default is enabled.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// puCounts returns the number of automatic reconnection dropped calls
// Return Value: Upon successful, return CAMERA_STATUS_SUCCESS (0), said the camera connection status is normal;
// This function returns CAMERA_STATUS_NOT_SUPPORTED (-4) for unsupported models that does not support
// Other non-zero value, refer to the definition of error code in CameraStatus.h.
/***************************************************** *****/
MVSDK_API CameraSdkStatus __stdcall CameraGetReConnectCounts (CameraHandle hCamera, UINT * puCounts);

/***************************************************** *****/
// function name: CameraSetSingleGrabMode
// Function Description: Enable single-frame capture mode
// parameters: hCamera camera handle, obtained by the CameraInit function.
// bEnable Enables single-frame fetch mode
// Return Value: Upon successful, return CAMERA_STATUS_SUCCESS (0), said the camera connection status is normal;
// This function returns CAMERA_STATUS_NOT_SUPPORTED (-4) for unsupported models that does not support
// Other non-zero value, refer to the definition of error code in CameraStatus.h.
/***************************************************** *****/
MVSDK_API CameraSdkStatus __stdcall CameraSetSingleGrabMode (CameraHandle hCamera, BOOL bEnable);

/***************************************************** *****/
// function name: CameraGetSingleGrabMode
// Function Description: Get the camera's single-frame capture mode
// parameters: hCamera camera handle, obtained by the CameraInit function.
// pbEnable returns the camera's single frame grabbing mode
// Return Value: Upon successful, return CAMERA_STATUS_SUCCESS (0), said the camera connection status is normal;
// This function returns CAMERA_STATUS_NOT_SUPPORTED (-4) for unsupported models that does not support
// Other non-zero value, refer to the definition of error code in CameraStatus.h.
/***************************************************** *****/
MVSDK_API CameraSdkStatus __stdcall CameraGetSingleGrabMode (CameraHandle hCamera, BOOL * pbEnable);

/***************************************************** *****/
// function name: CameraRestartGrab
// Function Description: When the camera is in single-frame capture mode, the SDK will enter the pause status every time a frame is successfully fetched. Calling this function causes the SDK to exit the paused state and begin to capture the next frame
// parameters: hCamera camera handle, obtained by the CameraInit function.
// Return Value: Upon successful, return CAMERA_STATUS_SUCCESS (0), said the camera connection status is normal;
// This function returns CAMERA_STATUS_NOT_SUPPORTED (-4) for unsupported models that does not support
// Other non-zero value, refer to the definition of error code in CameraStatus.h.
/***************************************************** *****/
MVSDK_API CameraSdkStatus __stdcall CameraRestartGrab (CameraHandle hCamera);

/***************************************************** *****/
// function name: CameraEvaluateImageDefinition
// Function Description: Picture clarity assessment
// parameters: hCamera camera handle, obtained by the CameraInit function.
// The evaluation algorithm used by iAlgorithSel, as defined in emEvaluateDefinitionAlgorith
// pbyIn Input buffer address for image data, can not be NULL.
// pFrInfo The header information of the input image
// DefinitionValue Returns the sharpness of the estimate (bigger and clearer)
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
MVSDK_API CameraSdkStatus __stdcall CameraEvaluateImageDefinition (
                                                                   CameraHandle hCamera,
                                                                   INT iAlgorithSel,
                                                                   BYTE * pbyIn,
                                                                   tSdkFrameHead * pFrInfo,
                                                                   double * DefinitionValue
                                                                  );

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
MVSDK_API CameraSdkStatus __stdcall CameraDrawText (
                                                    BYTE * pRgbBuffer,
                                                    tSdkFrameHead * pFrInfo,
                                                    char const * pFontFileName,
                                                    UINT FontWidth,
                                                    UINT FontHeight,
                                                    char const * pText,
                                                    INT Left,
                                                    INT Top,
                                                    UINT Width,
                                                    UINT Height,
                                                    UINT TextColor,
                                                    UINT uFlags
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
MVSDK_API CameraSdkStatus __stdcall CameraGigeGetIp (
                                                     tSdkCameraDevInfo * pCameraInfo,
                                                     char * CamIp,
                                                     char * CamMask,
                                                     char * CamGateWay,
                                                     char * EtIp,
                                                     char * EtMask,
                                                     char * EtGateWay
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
MVSDK_API CameraSdkStatus __stdcall CameraGigeSetIp (
                                                     tSdkCameraDevInfo * pCameraInfo,
                                                     char const * Ip,
                                                     char const * SubMask,
                                                     char const * GateWay,
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
MVSDK_API CameraSdkStatus __stdcall CameraGigeGetMac (
                                                      tSdkCameraDevInfo * pCameraInfo,
                                                      char * CamMac,
                                                      char * EtMac
                                                     );

/***************************************************** *****/
// function name: CameraEnableFastResponse
// Function Description: Enable quick response
// parameters: hCamera camera handle, obtained by the CameraInit function.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
MVSDK_API CameraSdkStatus __stdcall CameraEnableFastResponse (
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
MVSDK_API CameraSdkStatus __stdcall CameraSetCorrectDeadPixel (
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
MVSDK_API CameraSdkStatus __stdcall CameraGetCorrectDeadPixel (
                                                               CameraHandle hCamera,
                                                               BOOL * pbEnable
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
MVSDK_API CameraSdkStatus __stdcall CameraFlatFieldingCorrectSetEnable (
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
MVSDK_API CameraSdkStatus __stdcall CameraFlatFieldingCorrectGetEnable (
                                                                        CameraHandle hCamera,
                                                                        BOOL * pbEnable
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
MVSDK_API CameraSdkStatus __stdcall CameraFlatFieldingCorrectSetParameter (
                                                                           CameraHandle hCamera,
                                                                           BYTE const * pDarkFieldingImage,
                                                                           tSdkFrameHead const * pDarkFieldingFrInfo,
                                                                           BYTE const * pLightFieldingImage,
                                                                           tSdkFrameHead const * pLightFieldingFrInfo
                                                                          );

/***************************************************** *****/
// function name: CameraFlatFieldingCorrectGetParameterState
// Function Description: Get the status of the leveling parameters
// parameters: hCamera camera handle, obtained by the CameraInit function.
// pbValid returns the parameter is valid
// pFilePath returns the path to the parameter file
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
MVSDK_API CameraSdkStatus __stdcall CameraFlatFieldingCorrectGetParameterState (
                                                                                CameraHandle hCamera,
                                                                                BOOL * pbValid,
                                                                                char * pFilePath
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
MVSDK_API CameraSdkStatus __stdcall CameraFlatFieldingCorrectSaveParameterToFile (
                                                                                  CameraHandle hCamera,
                                                                                  char const * pszFileName
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
MVSDK_API CameraSdkStatus __stdcall CameraFlatFieldingCorrectLoadParameterFromFile (
                                                                                    CameraHandle hCamera,
                                                                                    char const * pszFileName
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
MVSDK_API CameraSdkStatus __stdcall CameraCommonCall (
                                                      CameraHandle hCamera,
                                                      char const * pszCall,
                                                      char * pszResult,
                                                      UINT uResultBufSize
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
MVSDK_API CameraSdkStatus __stdcall CameraSetDenoise3DParams (
                                                              CameraHandle hCamera,
                                                              BOOL bEnable,
                                                              int nCount,
                                                              float * Weights
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
MVSDK_API CameraSdkStatus __stdcall CameraGetDenoise3DParams (
                                                              CameraHandle hCamera,
                                                              BOOL * bEnable,
                                                              int * nCount,
                                                              BOOL * bUseWeight,
                                                              float * Weights
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
MVSDK_API CameraSdkStatus __stdcall CameraManualDenoise3D (
                                                           tSdkFrameHead * InFramesHead,
                                                           BYTE ** InFramesData,
                                                           int nCount,
                                                           float * Weights,
                                                           tSdkFrameHead * OutFrameHead,
                                                           BYTE * OutFrameData
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
MVSDK_API CameraSdkStatus __stdcall CameraCustomizeDeadPixels (
                                                               CameraHandle hCamera,
                                                               HWND hParent
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
MVSDK_API CameraSdkStatus __stdcall CameraReadDeadPixels (
                                                          CameraHandle hCamera,
                                                          USHORT * pRows,
                                                          USHORT * pCols,
                                                          UINT * pNumPixel
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
MVSDK_API CameraSdkStatus __stdcall CameraAddDeadPixels (
                                                         CameraHandle hCamera,
                                                         USHORT * pRows,
                                                         USHORT * pCols,
                                                         UINT NumPixel
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
MVSDK_API CameraSdkStatus __stdcall CameraRemoveDeadPixels (
                                                            CameraHandle hCamera,
                                                            USHORT * pRows,
                                                            USHORT * pCols,
                                                            UINT NumPixel
                                                           );

/***************************************************** *****/
// function name: CameraRemoveAllDeadPixels
// Function Description: Remove all camera dead pixels
// parameters: hCamera camera handle, obtained by the CameraInit function.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
MVSDK_API CameraSdkStatus __stdcall CameraRemoveAllDeadPixels (
                                                               CameraHandle hCamera
                                                              );

/***************************************************** *****/
// function name: CameraSaveDeadPixels
// Function Description: Save the camera dead pixels to the camera memory
// parameters: hCamera camera handle, obtained by the CameraInit function.
// Return Value: On success, CAMERA_STATUS_SUCCESS (0) is returned;
// Otherwise, return a non-zero error code, please refer to CameraStatus.h
// The definition of the error code.
/***************************************************** *****/
MVSDK_API CameraSdkStatus __stdcall CameraSaveDeadPixels (
                                                          CameraHandle hCamera
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
MVSDK_API CameraSdkStatus __stdcall CameraSaveDeadPixelsToFile (
                                                                CameraHandle hCamera,
                                                                char const * sFileName
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
MVSDK_API CameraSdkStatus __stdcall CameraLoadDeadPixelsFromFile (
                                                                  CameraHandle hCamera,
                                                                  char const * sFileName
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
MVSDK_API CameraSdkStatus __stdcall CameraGetImageBufferPriority (
                                                                  CameraHandle hCamera,
                                                                  tSdkFrameHead * pFrameInfo,
                                                                  BYTE ** pbyBuffer,
                                                                  UINT wTimes,
                                                                  UINT Priority
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
MVSDK_API unsigned char * __stdcall CameraGetImageBufferPriorityEx (
                                                                    CameraHandle hCamera,
                                                                    INT * piWidth,
                                                                    INT * piHeight,
                                                                    UINT wTimes,
                                                                    UINT Priority
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
MVSDK_API CameraSdkStatus __stdcall CameraGetImageBufferPriorityEx2 (
                                                                     CameraHandle hCamera,
                                                                     BYTE * pImageData,
                                                                     UINT uOutFormat,
                                                                     int * piWidth,
                                                                     int * piHeight,
                                                                     UINT wTimes,
                                                                     UINT Priority
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
MVSDK_API CameraSdkStatus __stdcall CameraGetImageBufferPriorityEx3 (
                                                                     CameraHandle hCamera,
                                                                     BYTE * pImageData,
                                                                     UINT uOutFormat,
                                                                     int * piWidth,
                                                                     int * piHeight,
                                                                     UINT * puTimeStamp,
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
MVSDK_API CameraSdkStatus __stdcall CameraClearBuffer (
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
MVSDK_API CameraSdkStatus __stdcall CameraSoftTriggerEx (
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
MVSDK_API CameraSdkStatus __stdcall CameraSetHDR (
                                                  CameraHandle hCamera,
                                                  float value
                                                 );

/***************************************************** *****/
// function name: CameraGetHDR
// Function Description: To get the HDR of the camera, you need the camera support, the model without HDR function, this function returns the error code, which means it is not supported.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// value HDR coefficient in the range of 0.0 to 1.0
// Return Value: Upon successful, return CAMERA_STATUS_SUCCESS (0), said the camera connection status is normal;
// otherwise it returns non-zero value, refer to the definition of error code in CameraStatus.h.
/***************************************************** *****/
MVSDK_API CameraSdkStatus __stdcall CameraGetHDR (
                                                  CameraHandle hCamera,
                                                  float * value
                                                 );

/***************************************************** *****/
// function name: CameraGetFrameID
// Function Description: Get the ID of the current frame, camera support (network interface full range of support), this function returns an error code that does not support.
// parameters: hCamera camera handle, obtained by the CameraInit function.
//             id frame ID
// Return Value: Upon successful, return CAMERA_STATUS_SUCCESS (0), said the camera connection status is normal;
// otherwise it returns non-zero value, refer to the definition of error code in CameraStatus.h.
/***************************************************** *****/
MVSDK_API CameraSdkStatus __stdcall CameraGetFrameID (
                                                      CameraHandle hCamera,
                                                      UINT * id
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
MVSDK_API CameraSdkStatus __stdcall CameraGetFrameTimeStamp (
                                                             CameraHandle hCamera,
                                                             UINT * TimeStampL,
                                                             UINT * TimeStampH
                                                            );

/***************************************************** *****/
// function name: CameraSetHDRGainMode
// Function Description: Set the camera's gain mode, the need for camera support, without gain mode switching function models, this function returns an error code that does not support.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// value 0: low gain 1: high gain
// Return Value: Upon successful, return CAMERA_STATUS_SUCCESS (0), said the camera connection status is normal;
// otherwise it returns non-zero value, refer to the definition of error code in CameraStatus.h.
/***************************************************** *****/
MVSDK_API CameraSdkStatus __stdcall CameraSetHDRGainMode (
                                                          CameraHandle hCamera,
                                                          int value
                                                         );

/***************************************************** *****/
// function name: CameraGetHDRGainMode
// Function Description: Get the camera's gain mode, the need for camera support, without gain mode switching function models, this function returns an error code that does not support.
// parameters: hCamera camera handle, obtained by the CameraInit function.
// value 0: low gain 1: high gain
// Return Value: Upon successful, return CAMERA_STATUS_SUCCESS (0), said the camera connection status is normal;
// otherwise it returns non-zero value, refer to the definition of error code in CameraStatus.h.
/***************************************************** *****/
MVSDK_API CameraSdkStatus __stdcall CameraGetHDRGainMode (
                                                          CameraHandle hCamera,
                                                          int * value
                                                         );

#endif

