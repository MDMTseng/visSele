#pragma once
#ifndef _CAMERA_DEFINE_H_
#define _CAMERA_DEFINE_H_

#include "CameraStatus.h"

#define MAX_CROSS_LINE 9

// Camera handle type definition
typedef int CameraHandle;




// image look-up table transform approach
typedef enum
{
	LUTMODE_PARAM_GEN = 0, // Dynamically generate the LUT table by adjusting parameters
	LUTMODE_PRESET, // Use the default LUT table
	LUTMODE_USER_DEF // Use a user-defined LUT table
} emSdkLutMode;

// Camera's video stream control
typedef enum
{
	RUNMODE_PLAY = 0, // normal preview, capture the image will be displayed. (If the camera is in trigger mode, it will wait for the trigger frame to come)
	RUNMODE_PAUSE, // pause, will pause the camera's image output and will not capture the image
	RUNMODE_STOP // stop camera work. After de-initialization, the camera is in stop mode
} emSdkRunMode;

// SDK internal display interface display
typedef enum
{
	DISPLAYMODE_SCALE = 0, // zoom display mode, zoom to display the size of the control
	DISPLAYMODE_REAL, // 1: 1 display mode, when the image size is greater than the size of the display control, only the local
	DISPLAYMODE_2X, // Enlarge 2X
	DISPLAYMODE_4X, // enlarge 4X
	DISPLAYMODE_8X, // Enlarge 8X
	DISPLAYMODE_16X // Enlarge 16X
} emSdkDisplayMode;

// video status
typedef enum
{
	RECORD_STOP = 0, // stop
	RECORD_START, // Recording
	RECORD_PAUSE // pause
} emSdkRecordMode;

// Image mirroring
typedef enum
{
	MIRROR_DIRECTION_HORIZONTAL = 0, // horizontal image
	MIRROR_DIRECTION_VERTICAL // vertical mirror
} emSdkMirrorDirection;

// image rotation operation
typedef enum
{
	ROTATE_DIRECTION_0 = 0, // Do not rotate
	ROTATE_DIRECTION_90, // counterclockwise 90 degrees
	ROTATE_DIRECTION_180, // counterclockwise 180 degrees
	ROTATE_DIRECTION_270, // 270 degrees counterclockwise
} emSdkRotateDirection;

// Camera video frame rate
typedef enum
{
	FRAME_SPEED_LOW = 0, // low speed mode
	FRAME_SPEED_NORMAL, // normal mode
	FRAME_SPEED_HIGH, // high-speed mode (requires a higher transmission bandwidth, multi-device shared transmission bandwidth will affect the stability of the frame rate)
	FRAME_SPEED_SUPER // ultra-high-speed mode (requires a higher transmission bandwidth, multi-device shared transmission bandwidth will affect the stability of the frame rate)
} emSdkFrameSpeed;

// Save the file's format type
typedef enum
{
	FILE_JPG = 1, // JPG
	FILE_BMP = 2, // BMP 24bit
	FILE_RAW = 4, // camera output bayer format file, do not support bayer format output camera, can not be saved to the format
	FILE_PNG = 8, // PNG 24bit
	FILE_BMP_8BIT = 16, // BMP 8bit
	FILE_PNG_8BIT = 32, // PNG 8bit
	FILE_RAW_16BIT = 64
} emSdkFileType;

// The working mode of the image sensor in the camera
typedef enum
{
	CONTINUATION = 0, // continuous acquisition mode
	SOFT_TRIGGER, // Software trigger mode, the software to send instructions, the sensor began to capture the specified number of frames of the image acquisition is completed, stop the output
	EXTERNAL_TRIGGER // hardware trigger mode, when receiving an external signal, the sensor began to capture the specified number of frames of the image acquisition is complete, stop the output
} emSdkSnapMode;

// Anti-stroboscopic strobe at auto exposure
typedef enum
{
	LIGHT_FREQUENCY_50HZ = 0, // 50HZ, the general lighting is 50HZ
	LIGHT_FREQUENCY_60HZ // 60HZ, mainly refers to the monitor
} emSdkLightFrequency;

// camera configuration parameters, divided into A, B, C, D 4 group to save.
typedef enum
{
	PARAMETER_TEAM_DEFAULT = 0xff,
	PARAMETER_TEAM_A = 0,
	PARAMETER_TEAM_B = 1,
	PARAMETER_TEAM_C = 2,
	PARAMETER_TEAM_D = 3
} emSdkParameterTeam;


/* emSdkParameterMode Camera parameter loading mode, parameter loading is divided into two ways from the file and from the device loading

PARAM_MODE_BY_MODEL: All the same model cameras share ABCD four groups of parameter files. modify
A camera's parameter file will affect the entire same model
Camera parameter loading.

PARAM_MODE_BY_NAME: All cameras with the same device name share ABCD four groups of parameter files.
By default, when only one model of a camera is connected to the computer,
The device name is the same, and you want one of the cameras to load
Different parameter files, you can modify the way their device name
Let it load the specified parameter file.

PARAM_MODE_BY_SN: Camera according to their own unique serial number to load ABCD four groups of parameter files,
The serial number has been set within the camera at the factory and the serial number of each camera
Are different, in this way, the parameters of each camera files are independent.

You can flexibly use the above methods to load the parameters according to your environment. For example, to
MV-U300 as an example, you want more than one model of this camera on your computer share 4 groups of parameters, then
Use PARAM_MODE_BY_MODEL way; if you want one of them or several MV-U300 can
Use their own parameter file and the rest of the MV-U300 and use the same parameter file, then use
PARAM_MODE_BY_NAME way; if you want each MV-U300 to use a different profile, then
Use the PARAM_MODE_BY_SN method.
Parameter file exists in the installation directory \ Camera \ Configs directory to config file extension.
*/
typedef enum
{
	PARAM_MODE_BY_MODEL = 0, // Load parameters from file according to camera model name, eg MV-U300
	PARAM_MODE_BY_NAME, // Load parameters from file based on device nickname (tSdkCameraDevInfo.acFriendlyName), such as MV-U300, which can be customized
	PARAM_MODE_BY_SN, // Load parameters from the file based on the device's unique serial number, which has been written to the device at the factory, each with a different serial number.
	PARAM_MODE_IN_DEVICE // Load parameters from the device's solid-state memory. Not all models support reading and writing parameter sets from the camera, as determined by tSdkCameraCapbility.bParamInDevice
} emSdkParameterMode;


// The camera configuration page mask value generated by the SDK
typedef enum
{
	PROP_SHEET_INDEX_EXPOSURE = 0,
	PROP_SHEET_INDEX_ISP_COLOR,
	PROP_SHEET_INDEX_ISP_LUT,
	PROP_SHEET_INDEX_ISP_SHAPE,
	PROP_SHEET_INDEX_VIDEO_FORMAT,
	PROP_SHEET_INDEX_RESOLUTION,
	PROP_SHEET_INDEX_IO_CTRL,
	PROP_SHEET_INDEX_TRIGGER_SET,
	PROP_SHEET_INDEX_OVERLAY,
	PROP_SHEET_INDEX_DEVICE_INFO
} emSdkPropSheetMask;

// The callback message type of the camera configuration page generated by the SDK
typedef enum
{
	SHEET_MSG_LOAD_PARAM_DEFAULT = 0, // parameter is restored to default, trigger the message
	SHEET_MSG_LOAD_PARAM_GROUP, // Load the specified parameter group, trigger the message
	SHEET_MSG_LOAD_PARAM_FROMFILE, // After the parameter is loaded from the specified file, the message is triggered
	SHEET_MSG_SAVE_PARAM_GROUP // Trigger when the current parameter set is saved
} emSdkPropSheetMsg;

// Visualize the type of reference window selected
typedef enum
{
	REF_WIN_AUTO_EXPOSURE = 0,
	REF_WIN_WHITE_BALANCE,
} emSdkRefWinType;

// Visualize the type of reference window selected
typedef enum
{
	RES_MODE_PREVIEW = 0,
	RES_MODE_SNAPSHOT,
} emSdkResolutionMode;

// Color temperature mode at white balance
typedef enum
{
	CT_MODE_AUTO = 0, // Automatic identification of color temperature
	CT_MODE_PRESET, // Use the specified preset color temperature
	CT_MODE_USER_DEF // custom color temperature (gain and matrix)
} emSdkClrTmpMode;

// LUT color channel
typedef enum
{
	LUT_CHANNEL_ALL = 0, // R, B, G three-channel simultaneous adjustment
	LUT_CHANNEL_RED, // red channel
	LUT_CHANNEL_GREEN, // green channel
	LUT_CHANNEL_BLUE, // blue channel
} emSdkLutChannel;

// ISP processing unit
typedef enum
{
	ISP_PROCESSSOR_PC = 0, // Use the PC's software ISP module
	ISP_PROCESSSOR_DEVICE // Use the camera's own hardware ISP module
} emSdkIspProcessor;

// Flash signal control mode
typedef enum
{
	STROBE_SYNC_WITH_TRIG_AUTO = 0, // Synchronous with the trigger signal, the STROBE signal is automatically generated when the camera is exposed after the trigger. In this case, the effective polarity can be set (CameraSetStrobePolarity).
	STROBE_SYNC_WITH_TRIG_MANUAL, // Synchronize with trigger signal. After triggering, STROBE is delayed for the specified time (CameraSetStrobeDelayTime), then for the specified time (CameraSetStrobePulseWidth). The valid polarity can be set (CameraSetStrobePolarity).
	STROBE_ALWAYS_HIGH, // is always high, ignoring other STROBE signal settings
	STROBE_ALWAYS_LOW // Always low to ignore other STROBE signal settings
} emStrobeControl;

// Type of signal triggered by hardware
typedef enum
{
	EXT_TRIG_LEADING_EDGE = 0, // rising edge trigger, the default is this way
	EXT_TRIG_TRAILING_EDGE, // Falling edge trigger
	EXT_TRIG_HIGH_LEVEL, // high level trigger, the level width determines the exposure time, only some models of the camera supports level trigger mode.
	EXT_TRIG_LOW_LEVEL, // low trigger
	EXT_TRIG_DOUBLE_EDGE, // Dual edge trigger
} emExtTrigSignal;

// Shutter mode when hardware external trigger
typedef enum
{
	EXT_TRIG_EXP_STANDARD = 0, // standard way, the default is this way.
	EXT_TRIG_EXP_GRR, // global reset mode, the partial rolling shutter CMOS model of the camera supports this mode, with the external mechanical shutter, you can achieve the effect of global shutter suitable for high-speed moving objects
} emExtTrigShutterMode;

// Sharpness evaluation algorithm
typedef enum
{
	EVALUATE_DEFINITION_DEVIATION = 0, // Variance method
	EVALUATE_DEFINITION_SMD, // neighboring pixels gray-scale method
	EVALUATE_DEFINITION_GRADIENT, // Gradient statistics
	EVALUATE_DEFINITION_SOBEL, // Sobel
	EVALUATE_DEFINITION_ROBERT, // Robert
	EVALUATE_DEFINITION_LAPLACE, // Laplace
	EVALUATE_DEFINITION_ALG_MAX,
} emEvaluateDefinitionAlgorith;

// text output logo
typedef enum
{
	CAMERA_DT_VCENTER = 0x1, // center vertically
	CAMERA_DT_BOTTOM = 0x2, // Bottom alignment
	CAMERA_DT_HCENTER = 0x4, // Level is centered
	CAMERA_DT_RIGHT = 0x8, // right-aligned
	CAMERA_DT_SINGLELINE = 0x10, // single-line display
	CAMERA_DT_ALPHA_BLEND = 0x20, // Alpha mix
	CAMERA_DT_ANTI_ALIASING = 0x40, // anti-aliasing
} emCameraDrawTextFlags;

// GPIO mode
typedef enum
{
	IOMODE_TRIG_INPUT = 0, // trigger input
	IOMODE_STROBE_OUTPUT, // Flash output
	IOMODE_GP_INPUT, // universal input
	IOMODE_GP_OUTPUT, // Universal output
	IOMODE_PWM_OUTPUT, // PWM type output
} emCameraGPIOMode;

// Take picture priority
typedef enum
{
	CAMERA_GET_IMAGE_PRIORITY_OLDEST = 0, // Get the oldest frame in the cache
	CAMERA_GET_IMAGE_PRIORITY_NEWEST, // get the latest frame in the cache (all the old frames will be discarded)
	CAMERA_GET_IMAGE_PRIORITY_NEXT, // Discard all frames in the cache, and if the camera is currently being exposed or the transmission is momentarily interrupted, waiting to receive the next frame (Note: This feature is not supported on some models of cameras, Camera this mark is equivalent to CAMERA_GET_IMAGE_PRIORITY_OLDEST)
} emCameraGetImagePriority;

// Soft trigger function flag
typedef enum
{
	CAMERA_ST_CLEAR_BUFFER_BEFORE = 0x1, // Empty the camera's cached frames before soft triggering
} emCameraSoftTriggerExFlags;

// Camera's device information
typedef struct
{
	char acProductSeries [32]; // product family
	char acProductName [32]; // product name
	char acFriendlyName [32]; // product nickname, the user can customize to change the nickname, stored in the camera, used to distinguish between multiple cameras at the same time, you can use CameraSetFriendlyName interface to change the nickname, the device takes effect after the restart.
	char acLinkName [32]; // kernel symbolic connection name, internal use
	char acDriverVersion [32]; // driver version
	char acSensorType [32]; // sensor type
	char acPortType [32]; // Interface type
	char acSn [32]; // Product unique serial number
	UINT uInstance; // Instance index of the model camera on the computer, used to distinguish the same type of multi-camera
} tSdkCameraDevInfo;


// The outer trigger mask returned by the CameraGetExtTrigCapability function
#define EXT_TRIG_MASK_GRR_SHUTTER 1
#define EXT_TRIG_MASK_LEVEL_MODE 2
#define EXT_TRIG_MASK_DOUBLE_EDGE 4

// Mask value of SKIP, BIN, RESAMPLE mode in tSdkResolutionRange structure
#define MASK_2X2_HD (1 << 0) // Hardware SKIP, BIN, Resample 2X2
#define MASK_3X3_HD (1 << 1)
#define MASK_4X4_HD (1 << 2)
#define MASK_5X5_HD (1 << 3)
#define MASK_6X6_HD (1 << 4)
#define MASK_7X7_HD (1 << 5)
#define MASK_8X8_HD (1 << 6)
#define MASK_9X9_HD (1 << 7)
#define MASK_10X10_HD (1 << 8)
#define MASK_11X11_HD (1 << 9)
#define MASK_12X12_HD (1 << 10)
#define MASK_13X13_HD (1 << 11)
#define MASK_14X14_HD (1 << 12)
#define MASK_15X15_HD (1 << 13)
#define MASK_16X16_HD (1 << 14)
#define MASK_17X17_HD (1 << 15)
#define MASK_2X2_SW (1 << 16) // Software SKIP, BIN, Resample 2X2
#define MASK_3X3_SW     (1<<17)
#define MASK_4X4_SW     (1<<18)
#define MASK_5X5_SW     (1<<19)
#define MASK_6X6_SW     (1<<20)
#define MASK_7X7_SW     (1<<21)
#define MASK_8X8_SW     (1<<22)
#define MASK_9X9_SW     (1<<23)     
#define MASK_10X10_SW   (1<<24)
#define MASK_11X11_SW   (1<<25)
#define MASK_12X12_SW   (1<<26)
#define MASK_13X13_SW   (1<<27)
#define MASK_14X14_SW   (1<<28)
#define MASK_15X15_SW   (1<<29)
#define MASK_16X16_SW   (1<<30)
#define MASK_17X17_SW   (1<<31)

// Camera's resolution setting range for component UI
typedef struct
{
	INT iHeightMax; // The maximum height of the image
	INT iHeightMin; // The minimum height of the image
	INT iWidthMax; // The maximum width of the image
	INT iWidthMin; // The minimum width of the image
	UINT uSkipModeMask; // SKIP Mode mask, 0, that does not support SKIP. bit0 is 1, which means SKIP 2x2 is supported; bit1 is 1, which means SKIP 3x3 is supported.
	UINT uBinSumModeMask; // The BIN (Sum) mode mask, which is 0, indicates that BIN is not supported. bit0 is 1, which means BIN 2x2 is supported; bit1 is 1, which means BIN 3x3 is supported.
	UINT uBinAverageModeMask; // The BIN (averaging) mode mask, which is 0, indicates that BIN is not supported. bit0 is 1, which means BIN 2x2 is supported; bit1 is 1, which means BIN 3x3 is supported.
	UINT uResampleMask; // Mask for hardware resampling
} tSdkResolutionRange;


// camera's resolution description
typedef struct
{
	INT iIndex; // index number, [0, N] indicates the default resolution (N is the maximum number of the default resolution, generally not more than 20), OXFF said custom resolution (ROI)
	char acDescription [32]; // Description of the resolution. This message is valid only for preset resolution. Custom resolution can ignore this information
	UINT uBinSumMode; // BIN (Summation) mode, the range can not exceed uSindex.ModeMask in tSdkResolutionRange
	UINT uBinAverageMode; // The BIN (averaging) mode, the range can not exceed uSinAverageModeMask in tSdkResolutionRange
	UINT uSkipMode; // Whether the size of SKIP, 0 means that the prohibition of SKIP mode, the range can not exceed tSdkResolutionRange in uSkipModeMask
	UINT uResampleMask; // Mask for hardware resampling
	INT iHOffsetFOV; // Acquire the vertical offset of the field of view relative to the upper-left corner of the Sensor's maximum field of view
	INT iVOffsetFOV; // Acquires the horizontal offset of the upper field of view relative to the maximum field of view of the Sensor
	INT iWidthFOV; // Acquire the width of the field of view
	INT iHeightFOV; // Acquire the height of the field of view
	INT iWidth; // The width of the image finally output by the camera
	INT iHeight; // The height of the image finally output by the camera
	INT iWidthZoomHd; // The width of the hardware scaling, the resolution is not required for this operation, this variable is set to 0.
	INT iHeightZoomHd; // The height of the hardware scaling, which is not required for the resolution of this operation. This variable is set to 0.
	INT iWidthZoomSw; // The width of the software scaling, the resolution does not need to do this operation, this variable is set to 0.
	INT iHeightZoomSw; // height of software scaling, do not need to do this resolution, this variable is set to 0.
} tSdkImageResolution;

// Camera white balance color temperature mode description information
typedef struct
{
	INT iIndex; // mode index number
	char acDescription [32]; // Description information
} tSdkColorTemperatureDes;

// Camera frame rate description information
typedef struct
{
	INT iIndex; // frame rate index number, generally 0 corresponds to low speed mode, 1 corresponds to normal mode, 2 corresponds to high speed mode
	char acDescription [32]; // Description information
} tSdkFrameSpeed;

// Camera exposure function range definition
typedef struct
{
	UINT uiTargetMin; // AE Brightness Target Minimum
	UINT uiTargetMax; // AE Brightness Target Max
	UINT uiAnalogGainMin; // The minimum analog gain, defined in fAnalogGainStep
	UINT uiAnalogGainMax; // Maximum value of analog gain, defined in fAnalogGainStep
	float fAnalogGainStep; // Each time the analog gain increases by 1, the corresponding increase in magnification. For example, uiAnalogGainMin is typically 16 and fAnalogGainStep is typically 0.125, then the minimum magnification is 16 * 0.125 = 2x
	UINT uiExposeTimeMin; // The minimum exposure time in manual mode, unit: line. According to CameraGetExposureLineTime can get a line corresponding to the time (microseconds), resulting in the entire frame of exposure time
	UINT uiExposeTimeMax; // Max. Exposure time in manual mode, unit: line
} tSdkExpose;

// Trigger mode description
typedef struct
{
	INT iIndex; // mode index number
	char acDescription [32]; // Description of the mode
} tSdkTrigger;

// Description of subcontract size (mainly for web camera)
typedef struct
{
	INT iIndex; // Sub-size index number
	char acDescription [32]; // Description of the corresponding information
	UINT iPackSize;
} tSdkPackLength;

// The default LUT table description
typedef struct
{
	INT iIndex; // Number
	char acDescription [32]; // Description information
} tSdkPresetLut;

// AE algorithm description
typedef struct
{
	INT iIndex; // Number
	char acDescription [32]; // Description information
} tSdkAeAlgorithm;

// RAW to RGB algorithm description
typedef struct
{
	INT iIndex; // Number
	char acDescription [32]; // Description information
} tSdkBayerDecodeAlgorithm;


// frame rate statistics
typedef struct
{
	INT iTotal; // The total number of frames currently collected (including the error frame)
	INT iCapture; // Number of valid frames currently acquired
	INT iLost; // current number of lost frames
} tSdkFrameStatistic;

// Camera output image data format
typedef struct
{
	INT iIndex; // format type number
	char acDescription [32]; // Description information
	UINT iMediaType; // The corresponding image format encoding, such as CAMERA_MEDIA_TYPE_BAYGR8, is defined in this document.
} tSdkMediaType;

// gamma setting range
typedef struct
{
	INT iMin; // minimum value
	INT iMax; // the maximum value
} tGammaRange;

// Contrast setting range
typedef struct
{
	INT iMin; // minimum value
	INT iMax; // the maximum value
} tContrastRange;

// RGB three-channel digital gain setting range
typedef struct
{
	INT iRGainMin; // The minimum value of red gain
	INT iRGainMax; // The maximum value of red gain
	INT iGGainMin; // The minimum value of green gain
	INT iGGainMax; // The maximum value of green gain
	INT iBGainMin; // The minimum value of blue gain
	INT iBGainMax; // The maximum value of blue gain
} tRgbGainRange;

// Saturation setting range
typedef struct
{
	INT iMin; // minimum value
	INT iMax; // the maximum value
} tSaturationRange;

// Sharpen the setting range
typedef struct
{
	INT iMin; // minimum value
	INT iMax; // the maximum value
} tSharpnessRange;

// ISP module enable information
typedef struct
{
	BOOL bMonoSensor; // Indicates whether the camera model is a black and white camera, the color-related functions can not be adjusted if it is a black and white camera
	BOOL bWbOnce; // Indicates whether the model of the camera supports manual white balance
	BOOL bAutoWb; // Indicates whether the camera supports the auto white balance function
	BOOL bAutoExposure; // Indicates whether the model supports the auto exposure function
	BOOL bManualExposure; // Indicates whether the model supports the manual exposure function
	BOOL bAntiFlick; // Indicates whether the model supports anti-strobe function
	BOOL bDeviceIsp; // Indicates whether the model camera supports hardware ISP functions
	BOOL bForceUseDeviceIsp; // bDeviceIsp and bForceUseDeviceIsp at the same time as TRUE, said the mandatory hardware only ISP, can not be canceled.
	BOOL bZoomHD; // camera hardware supports image scaling output (can only be reduced).
} tSdkIspCapacity;

/* Define integrated device description information that can be used to dynamically build UI */
typedef struct
{

	tSdkTrigger * pTriggerDesc; // Trigger mode
	INT iTriggerDesc; // the number of trigger mode, that pTriggerDesc array size

	tSdkImageResolution * pImageSizeDesc; // The default resolution selection
	INT iImageSizeDesc; // the number of the default resolution, that pImageSizeDesc array size

	tSdkColorTemperatureDes * pClrTempDesc; // Preset color temperature mode for white balance
	INT iClrTempDesc;

	tSdkMediaType * pMediaTypeDesc; // camera output image format
	INT iMediaTypdeDesc; // Camera output image format the number of species, that pMediaTypeDesc array size.

	tSdkFrameSpeed * pFrameSpeedDesc; // Adjustable frame rate type, corresponding to the interface of ordinary high-speed and super three speed settings
	INT iFrameSpeedDesc; // The number of adjustable frame rate types, that is, the size of the pFrameSpeedDesc array.

	tSdkPackLength * pPackLenDesc; // transmission packet length, generally used for network equipment
	INT iPackLenDesc; // The number of transport subpackages to choose from, that is, the size of the pPackLenDesc array.

	INT iOutputIoCounts; // Number of programmable output IOs
	INT iInputIoCounts; // Number of programmable input IOs

	tSdkPresetLut * pPresetLutDesc; // camera preset LUT table
	INT iPresetLut; // The number of LUTs preset by the camera, that is, the size of the pPresetLutDesc array

	INT iUserDataMaxLen; // Indicates the maximum length of the camera for saving user data areas. 0 means none.
	BOOL bParamInDevice; // Indicates whether the device supports reading and writing parameter sets from the device. 1 is supported, 0 is not supported.

	tSdkAeAlgorithm * pAeAlmSwDesc; // Software AE Algorithm Description
	int iAeAlmSwDesc; // number of software automatic exposure algorithm

	tSdkAeAlgorithm * pAeAlmHdDesc; // hardware auto-exposure algorithm description, NULL means that does not support hardware auto exposure
	int iAeAlmHdDesc; // number of hardware automatic exposure algorithm, 0 means that does not support hardware auto exposure

	tSdkBayerDecodeAlgorithm * pBayerDecAlmSwDesc; // algorithm for Bayer conversion to RGB data
	int iBayerDecAlmSwDesc; // number of algorithms Bayer converts to RGB data

	tSdkBayerDecodeAlgorithm * pBayerDecAlmHdDesc; // Algorithm for hardware Bayer conversion to RGB data, NULL means no support
	int iBayerDecAlmHdDesc; // The number of algorithms Bayer converted to RGB data, 0 means no support

	/* Image parameter adjustment range definition, used to dynamically build UI */
	tSdkExpose sExposeDesc; // Exposure range value
	tSdkResolutionRange sResolutionRange; // Description of the resolution range
	tRgbGainRange sRgbGainRange; // Image digital gain range description
	tSaturationRange sSaturationRange; // Description of the saturation range
	tGammaRange sGammaRange; // Description of the gamma range
	tContrastRange sContrastRange; // Description of the contrast range
	tSharpnessRange sSharpnessRange; // Description of the sharpening range
	tSdkIspCapacity sIspCapacity; // ISP capability description


} tSdkCameraCapbility;


// image frame header information
typedef struct
{
	UINT uiMediaType; // Image Format, Image Format
	UINT uBytes; // image data bytes, Total bytes
	INT iWidth; // The width of the image, after calling the image processing function, the variable may be dynamically modified to indicate the processed image size
	INT iHeight; // The height of the image, after calling the image processing function, the variable may be dynamically modified to indicate the processed image size
	INT iWidthZoomSw; // The width of the software scaling, no need for software tailoring the image, this variable is set to 0.
	INT iHeightZoomSw; // Height of the software scaling, no need for software cropping the image, this variable is set to 0.
	BOOL bIsTrigger; // Indicates if trigger frame is trigger
	UINT uiTimeStamp; // The acquisition time of the frame, unit 0.1 millisecond
	UINT uiExpTime; // The current image exposure, in microseconds us
	float fAnalogGain; // The analog multiplier of the current image
	INT iGamma; // The gamma setting of the frame image, valid only when LUT mode is dynamic parameter generation, and -1
	INT iContrast; // The contrast setting of the frame image, valid only when LUT mode is dynamic parameter generation, and -1
	INT iSaturation; // The saturation value of the frame image, meaningless for monochrome camera, is 0
	float fRgain; // This frame image processing red digital multiplier, for the black and white camera meaningless, 1
	float fGgain; // This frame image processing of the green digital gain multiples, for the black and white camera meaningless, 1
	float fBgain; // This frame image processing blue digital multiplier, for the black and white camera meaningless, 1
} tSdkFrameHead;

// image frame description
typedef struct sCameraFrame
{
	tSdkFrameHead head; // Frame header
	BYTE * pBuffer; // data area
} tSdkFrame;

// image capture callback function definition
typedef void (WINAPI * CAMERA_SNAP_PROC) (CameraHandle hCamera, BYTE * pFrameBuffer, tSdkFrameHead * pFrameHead, PVOID pContext);

// Camera callback function definition generated by SDK
typedef void (WINAPI * CAMERA_PAGE_MSG_PROC) (CameraHandle hCamera, UINT MSG, UINT uParam, PVOID pContext);


//////////////////////////////////////////////////////////////////////////
// Grabber related

// Grabber statistics
typedef struct
{
	int Width, Height; // frame image size
	int Disp; // display the number of frames
	int Capture; // The number of valid frames collected
	int Lost; // Number of lost frames
	int Error; // error the number of frames
	float DispFps; // display frame rate
	float CapFps; // capture frame rate
} tSdkGrabberStat;

// image capture callback function definition
typedef void (__stdcall * pfnCameraGrabberFrameCallback) (
	void * Grabber,
	BYTE * pFrameBuffer,
	tSdkFrameHead * pFrameHead,
	void * Context);

// Frame monitor function definition
typedef int (__stdcall * pfnCameraGrabberFrameListener) (
	void * Grabber,
	int Phase,
	BYTE * pFrameBuffer,
	tSdkFrameHead * pFrameHead,
	void * Context);

// Asynchronous capture the callback function definition
typedef void (__stdcall * pfnCameraGrabberSaveImageComplete) (
	void * Grabber,
	void * Image, // Need to call CameraImage_Destroy release
	CameraSdkStatus Status,
	void * Context
	);


//----------------------------IMAGE FORMAT DEFINE------------------------------------
#define CAMERA_MEDIA_TYPE_MONO                           0x01000000
#define CAMERA_MEDIA_TYPE_RGB                            0x02000000 
#define CAMERA_MEDIA_TYPE_COLOR                          0x02000000
#define CAMERA_MEDIA_TYPE_CUSTOM                         0x80000000
#define CAMERA_MEDIA_TYPE_COLOR_MASK                     0xFF000000
#define CAMERA_MEDIA_TYPE_OCCUPY1BIT                     0x00010000
#define CAMERA_MEDIA_TYPE_OCCUPY2BIT                     0x00020000
#define CAMERA_MEDIA_TYPE_OCCUPY4BIT                     0x00040000
#define CAMERA_MEDIA_TYPE_OCCUPY8BIT                     0x00080000
#define CAMERA_MEDIA_TYPE_OCCUPY10BIT                    0x000A0000
#define CAMERA_MEDIA_TYPE_OCCUPY12BIT                    0x000C0000
#define CAMERA_MEDIA_TYPE_OCCUPY16BIT                    0x00100000
#define CAMERA_MEDIA_TYPE_OCCUPY24BIT                    0x00180000
#define CAMERA_MEDIA_TYPE_OCCUPY32BIT                    0x00200000
#define CAMERA_MEDIA_TYPE_OCCUPY36BIT                    0x00240000
#define CAMERA_MEDIA_TYPE_OCCUPY48BIT                    0x00300000
#define CAMERA_MEDIA_TYPE_OCCUPY64BIT					 0x00400000

#define CAMERA_MEDIA_TYPE_EFFECTIVE_PIXEL_SIZE_MASK      0x00FF0000
#define CAMERA_MEDIA_TYPE_EFFECTIVE_PIXEL_SIZE_SHIFT     16

#define CAMERA_MEDIA_TYPE_PIXEL_SIZE(type)				 (((type) & CAMERA_MEDIA_TYPE_EFFECTIVE_PIXEL_SIZE_MASK) >> CAMERA_MEDIA_TYPE_EFFECTIVE_PIXEL_SIZE_SHIFT)


#define CAMERA_MEDIA_TYPE_ID_MASK                        0x0000FFFF
#define CAMERA_MEDIA_TYPE_COUNT                          0x46 

/*mono*/
#define CAMERA_MEDIA_TYPE_MONO1P             (CAMERA_MEDIA_TYPE_MONO | CAMERA_MEDIA_TYPE_OCCUPY1BIT | 0x0037)
#define CAMERA_MEDIA_TYPE_MONO2P             (CAMERA_MEDIA_TYPE_MONO | CAMERA_MEDIA_TYPE_OCCUPY2BIT | 0x0038)
#define CAMERA_MEDIA_TYPE_MONO4P             (CAMERA_MEDIA_TYPE_MONO | CAMERA_MEDIA_TYPE_OCCUPY4BIT | 0x0039)
#define CAMERA_MEDIA_TYPE_MONO8              (CAMERA_MEDIA_TYPE_MONO | CAMERA_MEDIA_TYPE_OCCUPY8BIT | 0x0001)
#define CAMERA_MEDIA_TYPE_MONO8S             (CAMERA_MEDIA_TYPE_MONO | CAMERA_MEDIA_TYPE_OCCUPY8BIT | 0x0002)
#define CAMERA_MEDIA_TYPE_MONO10             (CAMERA_MEDIA_TYPE_MONO | CAMERA_MEDIA_TYPE_OCCUPY16BIT | 0x0003)
#define CAMERA_MEDIA_TYPE_MONO10_PACKED      (CAMERA_MEDIA_TYPE_MONO | CAMERA_MEDIA_TYPE_OCCUPY12BIT | 0x0004)
#define CAMERA_MEDIA_TYPE_MONO12             (CAMERA_MEDIA_TYPE_MONO | CAMERA_MEDIA_TYPE_OCCUPY16BIT | 0x0005)
#define CAMERA_MEDIA_TYPE_MONO12_PACKED      (CAMERA_MEDIA_TYPE_MONO | CAMERA_MEDIA_TYPE_OCCUPY12BIT | 0x0006)
#define CAMERA_MEDIA_TYPE_MONO14             (CAMERA_MEDIA_TYPE_MONO | CAMERA_MEDIA_TYPE_OCCUPY16BIT | 0x0025)
#define CAMERA_MEDIA_TYPE_MONO16             (CAMERA_MEDIA_TYPE_MONO | CAMERA_MEDIA_TYPE_OCCUPY16BIT | 0x0007)

/*Bayer */
#define CAMERA_MEDIA_TYPE_BAYGR8             (CAMERA_MEDIA_TYPE_MONO | CAMERA_MEDIA_TYPE_OCCUPY8BIT | 0x0008)
#define CAMERA_MEDIA_TYPE_BAYRG8             (CAMERA_MEDIA_TYPE_MONO | CAMERA_MEDIA_TYPE_OCCUPY8BIT | 0x0009)
#define CAMERA_MEDIA_TYPE_BAYGB8             (CAMERA_MEDIA_TYPE_MONO | CAMERA_MEDIA_TYPE_OCCUPY8BIT | 0x000A)
#define CAMERA_MEDIA_TYPE_BAYBG8             (CAMERA_MEDIA_TYPE_MONO | CAMERA_MEDIA_TYPE_OCCUPY8BIT | 0x000B)

#define CAMERA_MEDIA_TYPE_BAYGR10_MIPI       (CAMERA_MEDIA_TYPE_MONO | CAMERA_MEDIA_TYPE_OCCUPY10BIT | 0x0026)
#define CAMERA_MEDIA_TYPE_BAYRG10_MIPI       (CAMERA_MEDIA_TYPE_MONO | CAMERA_MEDIA_TYPE_OCCUPY10BIT | 0x0027)
#define CAMERA_MEDIA_TYPE_BAYGB10_MIPI       (CAMERA_MEDIA_TYPE_MONO | CAMERA_MEDIA_TYPE_OCCUPY10BIT | 0x0028)
#define CAMERA_MEDIA_TYPE_BAYBG10_MIPI       (CAMERA_MEDIA_TYPE_MONO | CAMERA_MEDIA_TYPE_OCCUPY10BIT | 0x0029)


#define CAMERA_MEDIA_TYPE_BAYGR10            (CAMERA_MEDIA_TYPE_MONO | CAMERA_MEDIA_TYPE_OCCUPY16BIT | 0x000C)
#define CAMERA_MEDIA_TYPE_BAYRG10            (CAMERA_MEDIA_TYPE_MONO | CAMERA_MEDIA_TYPE_OCCUPY16BIT | 0x000D)
#define CAMERA_MEDIA_TYPE_BAYGB10            (CAMERA_MEDIA_TYPE_MONO | CAMERA_MEDIA_TYPE_OCCUPY16BIT | 0x000E)
#define CAMERA_MEDIA_TYPE_BAYBG10            (CAMERA_MEDIA_TYPE_MONO | CAMERA_MEDIA_TYPE_OCCUPY16BIT | 0x000F)

#define CAMERA_MEDIA_TYPE_BAYGR12            (CAMERA_MEDIA_TYPE_MONO | CAMERA_MEDIA_TYPE_OCCUPY16BIT | 0x0010)
#define CAMERA_MEDIA_TYPE_BAYRG12            (CAMERA_MEDIA_TYPE_MONO | CAMERA_MEDIA_TYPE_OCCUPY16BIT | 0x0011)
#define CAMERA_MEDIA_TYPE_BAYGB12            (CAMERA_MEDIA_TYPE_MONO | CAMERA_MEDIA_TYPE_OCCUPY16BIT | 0x0012)
#define CAMERA_MEDIA_TYPE_BAYBG12            (CAMERA_MEDIA_TYPE_MONO | CAMERA_MEDIA_TYPE_OCCUPY16BIT | 0x0013)


#define CAMERA_MEDIA_TYPE_BAYGR10_PACKED     (CAMERA_MEDIA_TYPE_MONO | CAMERA_MEDIA_TYPE_OCCUPY12BIT | 0x0026)
#define CAMERA_MEDIA_TYPE_BAYRG10_PACKED     (CAMERA_MEDIA_TYPE_MONO | CAMERA_MEDIA_TYPE_OCCUPY12BIT | 0x0027)
#define CAMERA_MEDIA_TYPE_BAYGB10_PACKED     (CAMERA_MEDIA_TYPE_MONO | CAMERA_MEDIA_TYPE_OCCUPY12BIT | 0x0028)
#define CAMERA_MEDIA_TYPE_BAYBG10_PACKED     (CAMERA_MEDIA_TYPE_MONO | CAMERA_MEDIA_TYPE_OCCUPY12BIT | 0x0029)

#define CAMERA_MEDIA_TYPE_BAYGR12_PACKED     (CAMERA_MEDIA_TYPE_MONO | CAMERA_MEDIA_TYPE_OCCUPY12BIT | 0x002A)
#define CAMERA_MEDIA_TYPE_BAYRG12_PACKED     (CAMERA_MEDIA_TYPE_MONO | CAMERA_MEDIA_TYPE_OCCUPY12BIT | 0x002B)
#define CAMERA_MEDIA_TYPE_BAYGB12_PACKED     (CAMERA_MEDIA_TYPE_MONO | CAMERA_MEDIA_TYPE_OCCUPY12BIT | 0x002C)
#define CAMERA_MEDIA_TYPE_BAYBG12_PACKED     (CAMERA_MEDIA_TYPE_MONO | CAMERA_MEDIA_TYPE_OCCUPY12BIT | 0x002D)

#define CAMERA_MEDIA_TYPE_BAYGR16            (CAMERA_MEDIA_TYPE_MONO | CAMERA_MEDIA_TYPE_OCCUPY16BIT | 0x002E)
#define CAMERA_MEDIA_TYPE_BAYRG16            (CAMERA_MEDIA_TYPE_MONO | CAMERA_MEDIA_TYPE_OCCUPY16BIT | 0x002F)
#define CAMERA_MEDIA_TYPE_BAYGB16            (CAMERA_MEDIA_TYPE_MONO | CAMERA_MEDIA_TYPE_OCCUPY16BIT | 0x0030)
#define CAMERA_MEDIA_TYPE_BAYBG16            (CAMERA_MEDIA_TYPE_MONO | CAMERA_MEDIA_TYPE_OCCUPY16BIT | 0x0031)

/*RGB */
#define CAMERA_MEDIA_TYPE_RGB8               (CAMERA_MEDIA_TYPE_COLOR | CAMERA_MEDIA_TYPE_OCCUPY24BIT | 0x0014)
#define CAMERA_MEDIA_TYPE_BGR8               (CAMERA_MEDIA_TYPE_COLOR | CAMERA_MEDIA_TYPE_OCCUPY24BIT | 0x0015)
#define CAMERA_MEDIA_TYPE_RGBA8              (CAMERA_MEDIA_TYPE_COLOR | CAMERA_MEDIA_TYPE_OCCUPY32BIT | 0x0016)
#define CAMERA_MEDIA_TYPE_BGRA8              (CAMERA_MEDIA_TYPE_COLOR | CAMERA_MEDIA_TYPE_OCCUPY32BIT | 0x0017)
#define CAMERA_MEDIA_TYPE_RGB10              (CAMERA_MEDIA_TYPE_COLOR | CAMERA_MEDIA_TYPE_OCCUPY48BIT | 0x0018)
#define CAMERA_MEDIA_TYPE_BGR10              (CAMERA_MEDIA_TYPE_COLOR | CAMERA_MEDIA_TYPE_OCCUPY48BIT | 0x0019)
#define CAMERA_MEDIA_TYPE_RGB12              (CAMERA_MEDIA_TYPE_COLOR | CAMERA_MEDIA_TYPE_OCCUPY48BIT | 0x001A)
#define CAMERA_MEDIA_TYPE_BGR12              (CAMERA_MEDIA_TYPE_COLOR | CAMERA_MEDIA_TYPE_OCCUPY48BIT | 0x001B)
#define CAMERA_MEDIA_TYPE_RGB16              (CAMERA_MEDIA_TYPE_COLOR | CAMERA_MEDIA_TYPE_OCCUPY48BIT | 0x0033)
#define CAMERA_MEDIA_TYPE_BGR16              (CAMERA_MEDIA_TYPE_COLOR | CAMERA_MEDIA_TYPE_OCCUPY48BIT | 0x004B)
#define CAMERA_MEDIA_TYPE_RGBA16             (CAMERA_MEDIA_TYPE_COLOR | CAMERA_MEDIA_TYPE_OCCUPY64BIT | 0x0064)
#define CAMERA_MEDIA_TYPE_BGRA16             (CAMERA_MEDIA_TYPE_COLOR | CAMERA_MEDIA_TYPE_OCCUPY64BIT | 0x0051)
#define CAMERA_MEDIA_TYPE_RGB10V1_PACKED     (CAMERA_MEDIA_TYPE_COLOR | CAMERA_MEDIA_TYPE_OCCUPY32BIT | 0x001C)
#define CAMERA_MEDIA_TYPE_RGB10P32           (CAMERA_MEDIA_TYPE_COLOR | CAMERA_MEDIA_TYPE_OCCUPY32BIT | 0x001D)
#define CAMERA_MEDIA_TYPE_RGB12V1_PACKED     (CAMERA_MEDIA_TYPE_COLOR | CAMERA_MEDIA_TYPE_OCCUPY36BIT | 0X0034)
#define CAMERA_MEDIA_TYPE_RGB565P            (CAMERA_MEDIA_TYPE_COLOR | CAMERA_MEDIA_TYPE_OCCUPY16BIT | 0x0035)
#define CAMERA_MEDIA_TYPE_BGR565P            (CAMERA_MEDIA_TYPE_COLOR | CAMERA_MEDIA_TYPE_OCCUPY16BIT | 0X0036)

/*YUV and YCbCr*/
#define CAMERA_MEDIA_TYPE_YUV411_8_UYYVYY    (CAMERA_MEDIA_TYPE_COLOR | CAMERA_MEDIA_TYPE_OCCUPY12BIT | 0x001E)
#define CAMERA_MEDIA_TYPE_YUV422_8_UYVY      (CAMERA_MEDIA_TYPE_COLOR | CAMERA_MEDIA_TYPE_OCCUPY16BIT | 0x001F)
#define CAMERA_MEDIA_TYPE_YUV422_8           (CAMERA_MEDIA_TYPE_COLOR | CAMERA_MEDIA_TYPE_OCCUPY16BIT | 0x0032)
#define CAMERA_MEDIA_TYPE_YUV8_UYV           (CAMERA_MEDIA_TYPE_COLOR | CAMERA_MEDIA_TYPE_OCCUPY24BIT | 0x0020)
#define CAMERA_MEDIA_TYPE_YCBCR8_CBYCR       (CAMERA_MEDIA_TYPE_COLOR | CAMERA_MEDIA_TYPE_OCCUPY24BIT | 0x003A)
//CAMERA_MEDIA_TYPE_YCBCR422_8 : YYYYCbCrCbCr
#define CAMERA_MEDIA_TYPE_YCBCR422_8             (CAMERA_MEDIA_TYPE_COLOR | CAMERA_MEDIA_TYPE_OCCUPY16BIT | 0x003B)
#define CAMERA_MEDIA_TYPE_YCBCR422_8_CBYCRY      (CAMERA_MEDIA_TYPE_COLOR | CAMERA_MEDIA_TYPE_OCCUPY16BIT | 0x0043)
#define CAMERA_MEDIA_TYPE_YCBCR411_8_CBYYCRYY    (CAMERA_MEDIA_TYPE_COLOR | CAMERA_MEDIA_TYPE_OCCUPY12BIT | 0x003C)
#define CAMERA_MEDIA_TYPE_YCBCR601_8_CBYCR       (CAMERA_MEDIA_TYPE_COLOR | CAMERA_MEDIA_TYPE_OCCUPY24BIT | 0x003D)
#define CAMERA_MEDIA_TYPE_YCBCR601_422_8         (CAMERA_MEDIA_TYPE_COLOR | CAMERA_MEDIA_TYPE_OCCUPY16BIT | 0x003E)
#define CAMERA_MEDIA_TYPE_YCBCR601_422_8_CBYCRY  (CAMERA_MEDIA_TYPE_COLOR | CAMERA_MEDIA_TYPE_OCCUPY16BIT | 0x0044)
#define CAMERA_MEDIA_TYPE_YCBCR601_411_8_CBYYCRYY    (CAMERA_MEDIA_TYPE_COLOR | CAMERA_MEDIA_TYPE_OCCUPY12BIT | 0x003F)
#define CAMERA_MEDIA_TYPE_YCBCR709_8_CBYCR           (CAMERA_MEDIA_TYPE_COLOR | CAMERA_MEDIA_TYPE_OCCUPY24BIT | 0x0040)
#define CAMERA_MEDIA_TYPE_YCBCR709_422_8             (CAMERA_MEDIA_TYPE_COLOR | CAMERA_MEDIA_TYPE_OCCUPY16BIT | 0x0041)
#define CAMERA_MEDIA_TYPE_YCBCR709_422_8_CBYCRY      (CAMERA_MEDIA_TYPE_COLOR | CAMERA_MEDIA_TYPE_OCCUPY16BIT | 0x0045)
#define CAMERA_MEDIA_TYPE_YCBCR709_411_8_CBYYCRYY    (CAMERA_MEDIA_TYPE_COLOR | CAMERA_MEDIA_TYPE_OCCUPY12BIT | 0x0042)

/*RGB Planar */
#define CAMERA_MEDIA_TYPE_RGB8_PLANAR        (CAMERA_MEDIA_TYPE_COLOR | CAMERA_MEDIA_TYPE_OCCUPY24BIT | 0x0021)
#define CAMERA_MEDIA_TYPE_RGB10_PLANAR       (CAMERA_MEDIA_TYPE_COLOR | CAMERA_MEDIA_TYPE_OCCUPY48BIT | 0x0022)
#define CAMERA_MEDIA_TYPE_RGB12_PLANAR       (CAMERA_MEDIA_TYPE_COLOR | CAMERA_MEDIA_TYPE_OCCUPY48BIT | 0x0023)
#define CAMERA_MEDIA_TYPE_RGB16_PLANAR       (CAMERA_MEDIA_TYPE_COLOR | CAMERA_MEDIA_TYPE_OCCUPY48BIT | 0x0024)

/*MindVision 12bit packed bayer*/
#define CAMERA_MEDIA_TYPE_BAYGR12_PACKED_MV     (CAMERA_MEDIA_TYPE_MONO | CAMERA_MEDIA_TYPE_OCCUPY12BIT | 0x0060)
#define CAMERA_MEDIA_TYPE_BAYRG12_PACKED_MV     (CAMERA_MEDIA_TYPE_MONO | CAMERA_MEDIA_TYPE_OCCUPY12BIT | 0x0061)
#define CAMERA_MEDIA_TYPE_BAYGB12_PACKED_MV     (CAMERA_MEDIA_TYPE_MONO | CAMERA_MEDIA_TYPE_OCCUPY12BIT | 0x0062)
#define CAMERA_MEDIA_TYPE_BAYBG12_PACKED_MV     (CAMERA_MEDIA_TYPE_MONO | CAMERA_MEDIA_TYPE_OCCUPY12BIT | 0x0063)

/*MindVision 12bit packed monochome*/
#define CAMERA_MEDIA_TYPE_MONO12_PACKED_MV     (CAMERA_MEDIA_TYPE_MONO | CAMERA_MEDIA_TYPE_OCCUPY12BIT | 0x0064)


#endif
