
#ifndef _MV_CAMERA_PARAMS_H_
#define _MV_CAMERA_PARAMS_H_

#include "PixelType.h"

#ifndef __cplusplus
typedef char    bool;
#define true    1
#define false   0
#endif

/// \~chinese GigE�豸��Ϣ              \~english GigE device info
typedef struct _MV_GIGE_DEVICE_INFO_
{
    unsigned int        nIpCfgOption;                               ///< [OUT] \~chinese IP����ѡ��             \~english IP Configuration Options
    unsigned int        nIpCfgCurrent;                              ///< [OUT] \~chinese ��ǰIP����             \~english IP Configuration:bit31-static bit30-dhcp bit29-lla
    unsigned int        nCurrentIp;                                 ///< [OUT] \~chinese ��ǰIP��ַ             \~english Current Ip
    unsigned int        nCurrentSubNetMask;                         ///< [OUT] \~chinese ��ǰ��������           \~english Curtent Subnet Mask
    unsigned int        nDefultGateWay;                             ///< [OUT] \~chinese ��ǰ����               \~english Current Gateway
    unsigned char       chManufacturerName[32];                     ///< [OUT] \~chinese ����������             \~english Manufacturer Name
    unsigned char       chModelName[32];                            ///< [OUT] \~chinese �ͺ�����               \~english Model Name
    unsigned char       chDeviceVersion[32];                        ///< [OUT] \~chinese �豸�汾               \~english Device Version 
    unsigned char       chManufacturerSpecificInfo[48];             ///< [OUT] \~chinese �����̵ľ�����Ϣ       \~english Manufacturer Specific Information
    unsigned char       chSerialNumber[16];                         ///< [OUT] \~chinese ���к�                 \~english Serial Number
    unsigned char       chUserDefinedName[16];                      ///< [OUT] \~chinese �û��Զ�������         \~english User Defined Name 
    unsigned int        nNetExport;                                 ///< [OUT] \~chinese ����IP��ַ             \~english NetWork IP Address

    unsigned int        nReserved[4];                               ///<       \~chinese Ԥ��                   \~english Reserved

}MV_GIGE_DEVICE_INFO;

///< \~chinese ����������Ϣ��С       \~english Maximum data information size
#define INFO_MAX_BUFFER_SIZE            64

/// \~chinese USB�豸��Ϣ               \~english USB device info
typedef struct _MV_USB3_DEVICE_INFO_
{
    unsigned char       CrtlInEndPoint;                             ///< [OUT] \~chinese ��������˵�           \~english Control input endpoint
    unsigned char       CrtlOutEndPoint;                            ///< [OUT] \~chinese ��������˵�           \~english Control output endpoint
    unsigned char       StreamEndPoint;                             ///< [OUT] \~chinese ���˵�                 \~english Flow endpoint
    unsigned char       EventEndPoint;                              ///< [OUT] \~chinese �¼��˵�               \~english Event endpoint
    unsigned short      idVendor;                                   ///< [OUT] \~chinese ��Ӧ��ID��             \~english Vendor ID Number
    unsigned short      idProduct;                                  ///< [OUT] \~chinese ��ƷID��               \~english Device ID Number
    unsigned int        nDeviceNumber;                              ///< [OUT] \~chinese �豸������             \~english Device Number
    unsigned char       chDeviceGUID[INFO_MAX_BUFFER_SIZE];         ///< [OUT] \~chinese �豸GUID��             \~english Device GUID Number
    unsigned char       chVendorName[INFO_MAX_BUFFER_SIZE];         ///< [OUT] \~chinese ��Ӧ������             \~english Vendor Name
    unsigned char       chModelName[INFO_MAX_BUFFER_SIZE];          ///< [OUT] \~chinese �ͺ�����               \~english Model Name
    unsigned char       chFamilyName[INFO_MAX_BUFFER_SIZE];         ///< [OUT] \~chinese ��������               \~english Family Name
    unsigned char       chDeviceVersion[INFO_MAX_BUFFER_SIZE];      ///< [OUT] \~chinese �豸�汾               \~english Device Version
    unsigned char       chManufacturerName[INFO_MAX_BUFFER_SIZE];   ///< [OUT] \~chinese ����������             \~english Manufacturer Name
    unsigned char       chSerialNumber[INFO_MAX_BUFFER_SIZE];       ///< [OUT] \~chinese ���к�                 \~english Serial Number
    unsigned char       chUserDefinedName[INFO_MAX_BUFFER_SIZE];    ///< [OUT] \~chinese �û��Զ�������         \~english User Defined Name
    unsigned int        nbcdUSB;                                    ///< [OUT] \~chinese ֧�ֵ�USBЭ��          \~english Support USB Protocol
    unsigned int        nDeviceAddress;                             ///< [OUT] \~chinese �豸��ַ               \~english Device Address

    unsigned int        nReserved[2];                               ///<       \~chinese Ԥ��                   \~english Reserved

}MV_USB3_DEVICE_INFO;

/// \~chinese CameraLink�豸��Ϣ        \~english CameraLink device info
typedef struct _MV_CamL_DEV_INFO_
{
    unsigned char       chPortID[INFO_MAX_BUFFER_SIZE];             ///< [OUT] \~chinese �˿ں�                 \~english Port ID
    unsigned char       chModelName[INFO_MAX_BUFFER_SIZE];          ///< [OUT] \~chinese �ͺ�����               \~english Model Name
    unsigned char       chFamilyName[INFO_MAX_BUFFER_SIZE];         ///< [OUT] \~chinese ����                   \~english Family Name
    unsigned char       chDeviceVersion[INFO_MAX_BUFFER_SIZE];      ///< [OUT] \~chinese �豸�汾               \~english Device Version
    unsigned char       chManufacturerName[INFO_MAX_BUFFER_SIZE];   ///< [OUT] \~chinese ����������             \~english Manufacturer Name
    unsigned char       chSerialNumber[INFO_MAX_BUFFER_SIZE];       ///< [OUT] \~chinese ���к�                 \~english Serial Number

    unsigned int        nReserved[38];                              ///<       \~chinese Ԥ��                   \~english Reserved

}MV_CamL_DEV_INFO;

///< \~chinese �豸�����Э������       \~english Device Transport Layer Protocol Type
#define MV_UNKNOW_DEVICE                0x00000000                  ///< \~chinese δ֪�豸���ͣ���������       \~english Unknown Device Type, Reserved 
#define MV_GIGE_DEVICE                  0x00000001                  ///< \~chinese GigE�豸                     \~english GigE Device
#define MV_1394_DEVICE                  0x00000002                  ///< \~chinese 1394-a/b �豸                \~english 1394-a/b Device
#define MV_USB_DEVICE                   0x00000004                  ///< \~chinese USB �豸                     \~english USB Device
#define MV_CAMERALINK_DEVICE            0x00000008                  ///< \~chinese CameraLink�豸               \~english CameraLink Device

/// \~chinese �豸��Ϣ                  \~english Device info
typedef struct _MV_CC_DEVICE_INFO_
{
    unsigned short          nMajorVer;                              ///< [OUT] \~chinese ��Ҫ�汾               \~english Major Version
    unsigned short          nMinorVer;                              ///< [OUT] \~chinese ��Ҫ�汾               \~english Minor Version
    unsigned int            nMacAddrHigh;                           ///< [OUT] \~chinese ��MAC��ַ              \~english High MAC Address
    unsigned int            nMacAddrLow;                            ///< [OUT] \~chinese ��MAC��ַ              \~english Low MAC Address
    unsigned int            nTLayerType;                            ///< [OUT] \~chinese �豸�����Э������     \~english Device Transport Layer Protocol Type

    unsigned int            nReserved[4];                           ///<       \~chinese Ԥ��                   \~english Reserved

    union
    {
        MV_GIGE_DEVICE_INFO stGigEInfo;                             ///< [OUT] \~chinese GigE�豸��Ϣ           \~english GigE Device Info
        MV_USB3_DEVICE_INFO stUsb3VInfo;                            ///< [OUT] \~chinese USB�豸��Ϣ            \~english USB Device Info
        MV_CamL_DEV_INFO    stCamLInfo;                             ///< [OUT] \~chinese CameraLink�豸��Ϣ     \~english CameraLink Device Info
        // more ...
    }SpecialInfo;

}MV_CC_DEVICE_INFO;

///< \~chinese ���֧�ֵĴ����ʵ������ \~english The maximum number of supported transport layer instances
#define MV_MAX_TLS_NUM                  8
///< \~chinese ���֧�ֵ��豸����       \~english The maximum number of supported devices
#define MV_MAX_DEVICE_NUM               256

/// \~chinese �豸��Ϣ�б�              \~english Device Information List
typedef struct _MV_CC_DEVICE_INFO_LIST_
{
    unsigned int        nDeviceNum;                                 ///< [OUT] \~chinese �����豸����           \~english Online Device Number
    MV_CC_DEVICE_INFO*  pDeviceInfo[MV_MAX_DEVICE_NUM];             ///< [OUT] \~chinese ֧�����256���豸      \~english Support up to 256 devices

}MV_CC_DEVICE_INFO_LIST;

/// \~chinese ͨ��GenTLö�ٵ��Ľӿ���Ϣ \~english Interface Information with GenTL
typedef struct _MV_GENTL_IF_INFO_
{
    unsigned char       chInterfaceID[INFO_MAX_BUFFER_SIZE];	    ///< [OUT] \~chinese GenTL�ӿ�ID            \~english Interface ID
    unsigned char       chTLType[INFO_MAX_BUFFER_SIZE];			    ///< [OUT] \~chinese ���������             \~english GenTL Type
    unsigned char       chDisplayName[INFO_MAX_BUFFER_SIZE];	    ///< [OUT] \~chinese Interface��ʾ����      \~english Display Name
    unsigned int        nCtiIndex;								    ///< [OUT] \~chinese GenTL��cti�ļ�����     \~english The Index of Cti Files

    unsigned int        nReserved[8];                               ///<       \~chinese Ԥ��                   \~english Reserved

}MV_GENTL_IF_INFO;

///< \~chinese ���֧�ֵ�GenTL�ӿ�����  \~english The maximum number of GenTL interface supported
#define MV_MAX_GENTL_IF_NUM             256

/// \~chinese ͨ��GenTLö�ٵ��Ľӿ���Ϣ�б� \~english Inferface Information List with GenTL
typedef struct _MV_GENTL_IF_INFO_LIST_
{
    unsigned int        nInterfaceNum;							    ///< [OUT] \~chinese ���߽ӿ�����           \~english Online Inferface Number
    MV_GENTL_IF_INFO*   pIFInfo[MV_MAX_GENTL_IF_NUM];			    ///< [OUT] \~chinese ֧�����256���ӿ�      \~english Support up to 256 inferfaces

}MV_GENTL_IF_INFO_LIST;

/// \~chinese ͨ��GenTLö�ٵ����豸��Ϣ \~english Device Information with GenTL
typedef struct _MV_GENTL_DEV_INFO_
{
    unsigned char       chInterfaceID[INFO_MAX_BUFFER_SIZE];        ///< [OUT] \~chinese GenTL�ӿ�ID            \~english Interface ID
    unsigned char       chDeviceID[INFO_MAX_BUFFER_SIZE];           ///< [OUT] \~chinese �豸ID                 \~english Device ID
    unsigned char       chVendorName[INFO_MAX_BUFFER_SIZE];         ///< [OUT] \~chinese ��Ӧ������             \~english Vendor Name
    unsigned char       chModelName[INFO_MAX_BUFFER_SIZE];          ///< [OUT] \~chinese �ͺ�����               \~english Model Name
    unsigned char       chTLType[INFO_MAX_BUFFER_SIZE];             ///< [OUT] \~chinese ���������             \~english GenTL Type
    unsigned char       chDisplayName[INFO_MAX_BUFFER_SIZE];        ///< [OUT] \~chinese �豸��ʾ����           \~english Display Name
    unsigned char       chUserDefinedName[INFO_MAX_BUFFER_SIZE];    ///< [OUT] \~chinese �û��Զ�������         \~english User Defined Name
    unsigned char       chSerialNumber[INFO_MAX_BUFFER_SIZE];       ///< [OUT] \~chinese ���к�                 \~english Serial Number
    unsigned char       chDeviceVersion[INFO_MAX_BUFFER_SIZE];      ///< [OUT] \~chinese �豸�汾��             \~english Device Version
    unsigned int        nCtiIndex;								    ///< [OUT] \~chinese GenTL��cti�ļ�����     \~english The Index of Cti Files

    unsigned int        nReserved[8];                               ///<       \~chinese Ԥ��                   \~english Reserved

}MV_GENTL_DEV_INFO;

///< \~chinese ���֧�ֵ�GenTL�豸����  \~english The maximum number of GenTL devices supported
#define MV_MAX_GENTL_DEV_NUM            256

/// \~chinese ͨ��GenTLö�ٵ����豸��Ϣ�б� \~english Device Information List with GenTL
typedef struct _MV_GENTL_DEV_INFO_LIST_
{
    unsigned int        nDeviceNum;                                 ///< [OUT] \~chinese �����豸����           \~english Online Device Number
    MV_GENTL_DEV_INFO*  pDeviceInfo[MV_MAX_GENTL_DEV_NUM];          ///< [OUT] \~chinese ֧�����256���豸      \~english Support up to 256 devices

}MV_GENTL_DEV_INFO_LIST;

/// \~chinese �豸�ķ���ģʽ            \~english Device Access Mode
#define MV_ACCESS_Exclusive                     1                   /// \~chinese ��ռȨ�ޣ�����APPֻ������CCP�Ĵ���                    \~english Exclusive authority, other APP is only allowed to read the CCP register
#define MV_ACCESS_ExclusiveWithSwitch           2                   /// \~chinese ���Դ�5ģʽ����ռȨ�ޣ�Ȼ���Զ�ռȨ�޴�             \~english You can seize the authority from the 5 mode, and then open with exclusive authority
#define MV_ACCESS_Control                       3                   /// \~chinese ����Ȩ�ޣ�����APP���������мĴ���                     \~english Control authority, allows other APP reading all registers
#define MV_ACCESS_ControlWithSwitch             4                   /// \~chinese ���Դ�5��ģʽ����ռȨ�ޣ�Ȼ���Կ���Ȩ�޴�           \~english You can seize the authority from the 5 mode, and then open with control authority
#define MV_ACCESS_ControlSwitchEnable           5                   /// \~chinese �Կɱ���ռ�Ŀ���Ȩ�޴�                              \~english Open with seized control authority
#define MV_ACCESS_ControlSwitchEnableWithKey    6                   /// \~chinese ���Դ�5��ģʽ����ռȨ�ޣ�Ȼ���Կɱ���ռ�Ŀ���Ȩ�޴� \~english You can seize the authority from the 5 mode, and then open with seized control authority
#define MV_ACCESS_Monitor                       7                   /// \~chinese ��ģʽ���豸�������ڿ���Ȩ����                      \~english Open with read mode and is available under control authority

/// \~chinese Chunk����                 \~english The content of ChunkData
typedef struct _MV_CHUNK_DATA_CONTENT_
{
    unsigned char*      pChunkData;                                 ///< [OUT] \~chinese Chunk����              \~english Chunk Data
    unsigned int        nChunkID;                                   ///< [OUT] \~chinese Chunk ID               \~english Chunk ID
    unsigned int        nChunkLen;                                  ///< [OUT] \~chinese Chunk�ĳ���            \~english Chunk Length

    unsigned int        nReserved[8];                               ///<       \~chinese Ԥ��                   \~english Reserved

}MV_CHUNK_DATA_CONTENT;

/// \~chinese ���֡����Ϣ              \~english Output Frame Information
typedef struct _MV_FRAME_OUT_INFO_EX_
{
    unsigned short          nWidth;                                 ///< [OUT] \~chinese ͼ���                 \~english Image Width
    unsigned short          nHeight;                                ///< [OUT] \~chinese ͼ���                 \~english Image Height
    enum MvGvspPixelType    enPixelType;                            ///< [OUT] \~chinese ���ظ�ʽ               \~english Pixel Type

    unsigned int            nFrameNum;                              ///< [OUT] \~chinese ֡��                   \~english Frame Number
    unsigned int            nDevTimeStampHigh;                      ///< [OUT] \~chinese ʱ�����32λ           \~english Timestamp high 32 bits
    unsigned int            nDevTimeStampLow;                       ///< [OUT] \~chinese ʱ�����32λ           \~english Timestamp low 32 bits
    unsigned int            nReserved0;                             ///< [OUT] \~chinese ������8�ֽڶ���        \~english Reserved, 8-byte aligned
    int64_t                 nHostTimeStamp;                         ///< [OUT] \~chinese �������ɵ�ʱ���       \~english Host-generated timestamp

    unsigned int            nFrameLen;                              ///< [OUT] \~chinese ֡�ĳ���               \~english The Length of Frame

    /// \~chinese �豸ˮӡʱ��      \~english Device frame-specific time scale
    unsigned int            nSecondCount;                           ///< [OUT] \~chinese ����                   \~english The Seconds
    unsigned int            nCycleCount;                            ///< [OUT] \~chinese ������                 \~english The Count of Cycle
    unsigned int            nCycleOffset;                           ///< [OUT] \~chinese ����ƫ����             \~english The Offset of Cycle

    float                   fGain;                                  ///< [OUT] \~chinese ����                   \~english Gain
    float                   fExposureTime;                          ///< [OUT] \~chinese �ع�ʱ��               \~english Exposure Time
    unsigned int            nAverageBrightness;                     ///< [OUT] \~chinese ƽ������               \~english Average brightness

    /// \~chinese ��ƽ�����        \~english White balance
    unsigned int            nRed;                                   ///< [OUT] \~chinese ��ɫ                   \~english Red
    unsigned int            nGreen;                                 ///< [OUT] \~chinese ��ɫ                   \~english Green
    unsigned int            nBlue;                                  ///< [OUT] \~chinese ��ɫ                   \~english Blue

    unsigned int            nFrameCounter;                          ///< [OUT] \~chinese ��֡��                 \~english Frame Counter
    unsigned int            nTriggerIndex;                          ///< [OUT] \~chinese ��������               \~english Trigger Counting

    unsigned int            nInput;                                 ///< [OUT] \~chinese ����                   \~english Input
    unsigned int            nOutput;                                ///< [OUT] \~chinese ���                   \~english Output

    /// \~chinese ROI����           \~english ROI Region
    unsigned short          nOffsetX;                               ///< [OUT] \~chinese ˮƽƫ����             \~english OffsetX
    unsigned short          nOffsetY;                               ///< [OUT] \~chinese ��ֱƫ����             \~english OffsetY
    unsigned short          nChunkWidth;                            ///< [OUT] \~chinese Chunk��                \~english The Width of Chunk
    unsigned short          nChunkHeight;                           ///< [OUT] \~chinese Chunk��                \~english The Height of Chunk

    unsigned int            nLostPacket;                            ///< [OUT] \~chinese ��֡������             \~english Lost Packet Number In This Frame

    unsigned int            nUnparsedChunkNum;                      ///< [OUT] \~chinese δ������Chunkdata����  \~english Unparsed Chunk Number
    union
    {
        MV_CHUNK_DATA_CONTENT*  pUnparsedChunkContent;              ///< [OUT] \~chinese δ������Chunk          \~english Unparsed Chunk Content
        int64_t                 nAligning;                          ///< [OUT] \~chinese У׼                   \~english Aligning
    }UnparsedChunkList;

    unsigned int            nReserved[36];                          ///<       \~chinese Ԥ��                   \~english Reserved

}MV_FRAME_OUT_INFO_EX;

/// \~chinese ͼ��ṹ�壬���ͼ���ַ��ͼ����Ϣ    \~english Image Struct, output the pointer of Image and the information of the specific image
typedef struct _MV_FRAME_OUT_
{
    unsigned char*          pBufAddr;                               ///< [OUT] \~chinese ͼ��ָ���ַ           \~english  pointer of image
    MV_FRAME_OUT_INFO_EX    stFrameInfo;                            ///< [OUT] \~chinese ͼ����Ϣ               \~english information of the specific image

    unsigned int            nRes[16];                               ///<       \~chinese Ԥ��                   \~english Reserved

}MV_FRAME_OUT;

/// \~chinese ȡ������                  \~english The strategy of Grabbing
typedef enum _MV_GRAB_STRATEGY_
{
    MV_GrabStrategy_OneByOne            = 0,                        ///< \~chinese �Ӿɵ���һ֡һ֡�Ļ�ȡͼ��   \~english Grab One By One
    MV_GrabStrategy_LatestImagesOnly    = 1,                        ///< \~chinese ��ȡ�б������µ�һ֡ͼ��     \~english Grab The Latest Image
    MV_GrabStrategy_LatestImages        = 2,                        ///< \~chinese ��ȡ�б������µ�ͼ��         \~english Grab The Latest Images
    MV_GrabStrategy_UpcomingImage       = 3,                        ///< \~chinese �ȴ���һ֡ͼ��               \~english Grab The Upcoming Image

}MV_GRAB_STRATEGY;

/// \~chinese ���紫��������Ϣ        \~english Network transmission information
typedef struct _MV_NETTRANS_INFO_
{
    int64_t             nReceiveDataSize;                           ///< [OUT] \~chinese �ѽ������ݴ�С[Start��Stop֮��]    \~english Received Data Size
    int                 nThrowFrameCount;                           ///< [OUT] \~chinese ��֡����                           \~english Throw frame number
    unsigned int        nNetRecvFrameCount;                         ///< [OUT] \~chinese �ѽ��յ�֡��                       \~english Received Frame Count
    int64_t             nRequestResendPacketCount;                  ///< [OUT] \~chinese �����ط�����                       \~english Request Resend Packet Count
    int64_t             nResendPacketCount;                         ///< [OUT] \~chinese �ط�����                           \~english Resend Packet Count

}MV_NETTRANS_INFO;

/// \~chinese ��Ϣ����                  \~english Information Type
#define MV_MATCH_TYPE_NET_DETECT        0x00000001                  ///< \~chinese ���������Ͷ�����Ϣ               \~english Network traffic and packet loss information
#define MV_MATCH_TYPE_USB_DETECT        0x00000002                  ///< \~chinese host���յ�����U3V�豸���ֽ�����  \~english The total number of bytes host received from U3V device

/// \~chinese ȫƥ���һ����Ϣ�ṹ��    \~english A fully matched information structure
typedef struct _MV_ALL_MATCH_INFO_
{
    unsigned int        nType;                                      ///< [IN]  \~chinese ��Ҫ�������Ϣ���ͣ�e.g. MV_MATCH_TYPE_NET_DETECT  \~english Information type need to output ,e.g. MV_MATCH_TYPE_NET_DETECT
    void*               pInfo;                                      ///< [OUT] \~chinese �������Ϣ���棬�ɵ����߷���                       \~english Output information cache, which is allocated by the caller
    unsigned int        nInfoSize;                                  ///< [IN]  \~chinese ��Ϣ����Ĵ�С                                     \~english Information cache size

}MV_ALL_MATCH_INFO;

/// \~chinese ���������Ͷ�����Ϣ�����ṹ�壬��Ӧ����Ϊ MV_MATCH_TYPE_NET_DETECT     \~english Network traffic and packet loss feedback structure, the corresponding type is MV_MATCH_TYPE_NET_DETECT
typedef struct _MV_MATCH_INFO_NET_DETECT_
{
    int64_t             nReceiveDataSize;                           ///< [OUT] \~chinese �ѽ������ݴ�С[Start��Stop֮��]    \~english Received data size 
    int64_t             nLostPacketCount;                           ///< [OUT] \~chinese ��ʧ�İ�����                       \~english Number of packets lost
    unsigned int        nLostFrameCount;                            ///< [OUT] \~chinese ��֡����                           \~english Number of frames lost
    unsigned int        nNetRecvFrameCount;                         ///< [OUT] \~chinese ����                               \~english Received Frame Count
    int64_t             nRequestResendPacketCount;                  ///< [OUT] \~chinese �����ط�����                       \~english Request Resend Packet Count
    int64_t             nResendPacketCount;                         ///< [OUT] \~chinese �ط�����                           \~english Resend Packet Count

}MV_MATCH_INFO_NET_DETECT;

/// \~chinese host�յ���u3v�豸�˵����ֽ�������Ӧ����Ϊ MV_MATCH_TYPE_USB_DETECT    \~english The total number of bytes host received from the u3v device side, the corresponding type is MV_MATCH_TYPE_USB_DETECT
typedef struct _MV_MATCH_INFO_USB_DETECT_
{
    int64_t             nReceiveDataSize;                           ///< [OUT] \~chinese �ѽ������ݴ�С [Open��Close֮��]   \~english Received data size
    unsigned int        nReceivedFrameCount;                        ///< [OUT] \~chinese ���յ���֡��                       \~english Number of frames received
    unsigned int        nErrorFrameCount;                           ///< [OUT] \~chinese ����֡��                           \~english Number of error frames

    unsigned int        nReserved[2];                               ///<       \~chinese ����                               \~english Reserved

}MV_MATCH_INFO_USB_DETECT;

/// \~chinese ��ʾ֡��Ϣ                \~english Display frame information
typedef struct _MV_DISPLAY_FRAME_INFO_
{
    void*                   hWnd;                                   ///< [IN] \~chinese ���ھ��                \~english HWND
    unsigned char*          pData;                                  ///< [IN] \~chinese ��ʾ������              \~english Data Buffer
    unsigned int            nDataLen;                               ///< [IN] \~chinese ���ݳ���                \~english Data Size
    unsigned short          nWidth;                                 ///< [IN] \~chinese ͼ���                  \~english Width
    unsigned short          nHeight;                                ///< [IN] \~chinese ͼ���                  \~english Height
    enum MvGvspPixelType    enPixelType;                            ///< [IN] \~chinese ���ظ�ʽ                \~english Pixel format

    unsigned int            nRes[4];                                ///<      \~chinese ����                    \~english Reserved

}MV_DISPLAY_FRAME_INFO;

/// \~chinese �����3D���ݸ�ʽ          \~english The saved format for 3D data
enum MV_SAVE_POINT_CLOUD_FILE_TYPE
{
    MV_PointCloudFile_Undefined         = 0,                        ///< \~chinese δ����ĵ��Ƹ�ʽ             \~english Undefined point cloud format
    MV_PointCloudFile_PLY               = 1,                        ///< \~chinese PLY���Ƹ�ʽ                  \~english The point cloud format named PLY
    MV_PointCloudFile_CSV               = 2,                        ///< \~chinese CSV���Ƹ�ʽ                  \~english The point cloud format named CSV
    MV_PointCloudFile_OBJ               = 3,                        ///< \~chinese OBJ���Ƹ�ʽ                  \~english The point cloud format named OBJ

};

/// \~chinese ����3D���ݵ�����          \~english Save 3D data to buffer
typedef struct _MV_SAVE_POINT_CLOUD_PARAM_
{
    unsigned int                    nLinePntNum;                    ///< [IN]  \~chinese �е�������ͼ���       \~english The number of points in each row,which is the width of the image
    unsigned int                    nLineNum;                       ///< [IN]  \~chinese ��������ͼ���         \~english The number of rows,which is the height of the image

    enum MvGvspPixelType            enSrcPixelType;                 ///< [IN]  \~chinese �������ݵ����ظ�ʽ     \~english The pixel format of the input data
    unsigned char*                  pSrcData;                       ///< [IN]  \~chinese �������ݻ���           \~english Input data buffer
    unsigned int                    nSrcDataLen;                    ///< [IN]  \~chinese �������ݳ���           \~english Input data length

    unsigned char*                  pDstBuf;                        ///< [OUT] \~chinese ����������ݻ���       \~english Output pixel data buffer
    unsigned int                    nDstBufSize;                    ///< [IN]  \~chinese �ṩ�������������С(nLinePntNum * nLineNum * (16*3 + 4) + 2048)   \~english Output buffer size provided(nLinePntNum * nLineNum * (16*3 + 4) + 2048) 
    unsigned int                    nDstBufLen;                     ///< [OUT] \~chinese ����������ݻ��泤��   \~english Output pixel data buffer size
    MV_SAVE_POINT_CLOUD_FILE_TYPE   enPointCloudFileType;           ///< [IN]  \~chinese �ṩ����ĵ����ļ����� \~english Output point data file type provided

    unsigned int        nReserved[8];                               ///<       \~chinese �����ֶ�               \~english Reserved

}MV_SAVE_POINT_CLOUD_PARAM;

/// \~chinese ����ͼƬ��ʽ              \~english Save image type
enum MV_SAVE_IAMGE_TYPE
{
    MV_Image_Undefined                  = 0,                        ///< \~chinese δ�����ͼ���ʽ             \~english Undefined Image Type
    MV_Image_Bmp                        = 1,                        ///< \~chinese BMPͼ���ʽ                  \~english BMP Image Type
    MV_Image_Jpeg                       = 2,                        ///< \~chinese JPEGͼ���ʽ                 \~english Jpeg Image Type
    MV_Image_Png                        = 3,                        ///< \~chinese PNGͼ���ʽ                  \~english Png  Image Type
    MV_Image_Tif                        = 4,                        ///< \~chinese TIFFͼ���ʽ                 \~english TIFF Image Type

};

/// \~chinese ͼƬ�������              \~english Save Image Parameters
typedef struct _MV_SAVE_IMAGE_PARAM_T_EX_
{
    unsigned char*          pData;                                  ///< [IN]  \~chinese �������ݻ���           \~english Input Data Buffer
    unsigned int            nDataLen;                               ///< [IN]  \~chinese �������ݳ���           \~english Input Data length
    enum MvGvspPixelType    enPixelType;                            ///< [IN]  \~chinese �������ݵ����ظ�ʽ     \~english Input Data Pixel Format
    unsigned short          nWidth;                                 ///< [IN]  \~chinese ͼ���                 \~english Image Width
    unsigned short          nHeight;                                ///< [IN]  \~chinese ͼ���                 \~english Image Height

    unsigned char*          pImageBuffer;                           ///< [OUT] \~chinese ���ͼƬ����           \~english Output Image Buffer
    unsigned int            nImageLen;                              ///< [OUT] \~chinese ���ͼƬ����           \~english Output Image length
    unsigned int            nBufferSize;                            ///< [IN]  \~chinese �ṩ�������������С   \~english Output buffer size provided
    enum MV_SAVE_IAMGE_TYPE enImageType;                            ///< [IN]  \~chinese ���ͼƬ��ʽ           \~english Output Image Format
    unsigned int            nJpgQuality;                            ///< [IN]  \~chinese JPG��������(50-99]��������ʽ��Ч   \~english Encoding quality(50-99]��Other formats are invalid

    
    unsigned int            iMethodValue;                           ///< [IN]  \~chinese ��ֵ���� 0-���� 1-���� 2-���ţ�����ֵĬ��Ϊ���ţ�  \~english Bayer interpolation method  0-Fast 1-Equilibrium 2-Optimal

    unsigned int            nReserved[3];                           ///<       \~chinese Ԥ��                   \~english Reserved

}MV_SAVE_IMAGE_PARAM_EX;

/// \~chinese ͼƬ�������              \~english Save Image Parameters
typedef struct _MV_SAVE_IMG_TO_FILE_PARAM_
{
    enum MvGvspPixelType    enPixelType;                            ///< [IN]  \~chinese�������ݵ����ظ�ʽ      \~english The pixel format of the input data
    unsigned char*          pData;                                  ///< [IN]  \~chinese �������ݻ���           \~english Input Data Buffer
    unsigned int            nDataLen;                               ///< [IN]  \~chinese �������ݳ���           \~english Input Data length
    unsigned short          nWidth;                                 ///< [IN]  \~chinese ͼ���                 \~english Image Width
    unsigned short          nHeight;                                ///< [IN]  \~chinese ͼ���                 \~english Image Height
    enum MV_SAVE_IAMGE_TYPE enImageType;                            ///< [IN]  \~chinese ����ͼƬ��ʽ           \~english Input Image Format
    unsigned int            nQuality;                               ///< [IN]  \~chinese JPG��������(50-99]��PNG��������[0-9]��������ʽ��Ч \~english JPG Encoding quality(50-99],PNG Encoding quality[0-9]��Other formats are invalid
    char                    pImagePath[256];                        ///< [IN]  \~chinese �����ļ�·��           \~english Input file path

    int                     iMethodValue;                           ///< [IN]  \~chinese ��ֵ���� 0-���� 1-���� 2-���ţ�����ֵĬ��Ϊ���ţ�  \~english Bayer interpolation method  0-Fast 1-Equilibrium 2-Optimal

    unsigned int            nReserved[8];                           ///<       \~chinese Ԥ��                   \~english Reserved

}MV_SAVE_IMG_TO_FILE_PARAM;

/// \~chinese ��ת�Ƕ�                  \~english Rotation angle
typedef enum _MV_IMG_ROTATION_ANGLE_
{
    MV_IMAGE_ROTATE_90                  = 1,
    MV_IMAGE_ROTATE_180                 = 2,
    MV_IMAGE_ROTATE_270                 = 3,

}MV_IMG_ROTATION_ANGLE;

/// \~chinese ͼ����ת�ṹ��            \~english Rotate image structure
typedef struct _MV_CC_ROTATE_IMAGE_PARAM_T_
{
    enum MvGvspPixelType    enPixelType;                            ///< [IN]  \~chinese ���ظ�ʽ               \~english Pixel format
    unsigned int            nWidth;                                 ///< [IN][OUT] \~chinese ͼ���             \~english Width
    unsigned int            nHeight;                                ///< [IN][OUT] \~chinese ͼ���             \~english Height

    unsigned char*          pSrcData;                               ///< [IN]  \~chinese �������ݻ���           \~english Input data buffer
    unsigned int            nSrcDataLen;                            ///< [IN]  \~chinese �������ݳ���           \~english Input data length

    unsigned char*          pDstBuf;                                ///< [OUT] \~chinese ������ݻ���           \~english Output data buffer
    unsigned int            nDstBufLen;                             ///< [OUT] \~chinese ������ݳ���           \~english Output data length
    unsigned int            nDstBufSize;                            ///< [IN]  \~chinese �ṩ�������������С   \~english Provided output buffer size

    MV_IMG_ROTATION_ANGLE   enRotationAngle;                        ///< [IN]  \~chinese ��ת�Ƕ�               \~english Rotation angle

    unsigned int            nRes[8];                                ///<       \~chinese Ԥ��                   \~english Reserved

}MV_CC_ROTATE_IMAGE_PARAM;

/// \~chinese ��ת����                  \~english Flip type
typedef enum _MV_IMG_FLIP_TYPE_
{
    MV_FLIP_VERTICAL                    = 1,
    MV_FLIP_HORIZONTAL                  = 2,

}MV_IMG_FLIP_TYPE;

/// \~chinese ͼ��ת�ṹ��            \~english Flip image structure
typedef struct _MV_CC_FLIP_IMAGE_PARAM_T_
{
    enum MvGvspPixelType    enPixelType;                            ///< [IN]  \~chinese ���ظ�ʽ               \~english Pixel format
    unsigned int            nWidth;                                 ///< [IN]  \~chinese ͼ���                 \~english Width
    unsigned int            nHeight;                                ///< [IN]  \~chinese ͼ���                 \~english Height

    unsigned char*          pSrcData;                               ///< [IN]  \~chinese �������ݻ���           \~english Input data buffer
    unsigned int            nSrcDataLen;                            ///< [IN]  \~chinese �������ݳ���           \~english Input data length

    unsigned char*          pDstBuf;                                ///< [OUT] \~chinese ������ݻ���           \~english Output data buffer
    unsigned int            nDstBufLen;                             ///< [OUT] \~chinese ������ݳ���           \~english Output data length
    unsigned int            nDstBufSize;                            ///< [IN]  \~chinese �ṩ�������������С   \~english Provided output buffer size

    MV_IMG_FLIP_TYPE        enFlipType;                             ///< [IN]  \~chinese ��ת����               \~english Flip type

    unsigned int            nRes[8];                                ///<       \~chinese Ԥ��                   \~english Reserved

}MV_CC_FLIP_IMAGE_PARAM;

/// \~chinese ����ת���ṹ��            \~english Pixel convert structure
typedef struct _MV_PIXEL_CONVERT_PARAM_T_
{
    unsigned short          nWidth;                                 ///< [IN]  \~chinese ͼ���                 \~english Width
    unsigned short          nHeight;                                ///< [IN]  \~chinese ͼ���                 \~english Height

    enum MvGvspPixelType    enSrcPixelType;                         ///< [IN]  \~chinese Դ���ظ�ʽ             \~english Source pixel format
    unsigned char*          pSrcData;                               ///< [IN]  \~chinese �������ݻ���           \~english Input data buffer
    unsigned int            nSrcDataLen;                            ///< [IN]  \~chinese �������ݳ���           \~english Input data length

    enum MvGvspPixelType    enDstPixelType;                         ///< [IN]  \~chinese Ŀ�����ظ�ʽ           \~english Destination pixel format
    unsigned char*          pDstBuffer;                             ///< [OUT] \~chinese ������ݻ���           \~english Output data buffer
    unsigned int            nDstLen;                                ///< [OUT] \~chinese ������ݳ���           \~english Output data length
    unsigned int            nDstBufferSize;                         ///< [IN]  \~chinese �ṩ�������������С   \~english Provided output buffer size

    unsigned int            nRes[4];                                ///<       \~chinese Ԥ��                   \~english Reserved

}MV_CC_PIXEL_CONVERT_PARAM;

/// \~chinese Gamma����                 \~english Gamma type
typedef enum _MV_CC_GAMMA_TYPE_
{
    MV_CC_GAMMA_TYPE_NONE               = 0,                        ///< \~chinese ������                       \~english Disable
    MV_CC_GAMMA_TYPE_VALUE              = 1,                        ///< \~chinese Gammaֵ                      \~english Gamma value
    MV_CC_GAMMA_TYPE_USER_CURVE         = 2,                        ///< \~chinese Gamma����                    \~english Gamma curve
                                                                    ///< \~chinese 8λ,���ȣ�256*sizeof(unsigned char)      \~english 8bit,length:256*sizeof(unsigned char)
                                                                    ///< \~chinese 10λ,���ȣ�1024*sizeof(unsigned short)   \~english 10bit,length:1024*sizeof(unsigned short)
                                                                    ///< \~chinese 12λ,���ȣ�4096*sizeof(unsigned short)   \~english 12bit,length:4096*sizeof(unsigned short)
                                                                    ///< \~chinese 16λ,���ȣ�65536*sizeof(unsigned short)  \~english 16bit,length:65536*sizeof(unsigned short)
    MV_CC_GAMMA_TYPE_LRGB2SRGB          = 3,                        ///< \~chinese linear RGB to sRGB           \~english linear RGB to sRGB
    MV_CC_GAMMA_TYPE_SRGB2LRGB          = 4,                        ///< \~chinese sRGB to linear RGB(��ɫ�ʲ�ֵʱ֧�֣�ɫ��У��ʱ��Ч) \~english sRGB to linear RGB

}MV_CC_GAMMA_TYPE;

// Gamma��Ϣ
/// \~chinese Gamma��Ϣ�ṹ��           \~english Gamma info structure
typedef struct _MV_CC_GAMMA_PARAM_T_
{
    MV_CC_GAMMA_TYPE    enGammaType;                                ///< [IN]  \~chinese Gamma����              \~english Gamma type
    float               fGammaValue;                                ///< [IN]  \~chinese Gammaֵ[0.1,4.0]       \~english Gamma value[0.1,4.0]
    unsigned char*      pGammaCurveBuf;                             ///< [IN]  \~chinese Gamma���߻���          \~english Gamma curve buffer
    unsigned int        nGammaCurveBufLen;                          ///< [IN]  \~chinese Gamma���߳���          \~english Gamma curve buffer size

    unsigned int        nRes[8];                                    ///<       \~chinese Ԥ��                   \~english Reserved

}MV_CC_GAMMA_PARAM;

/// \~chinese CCM����                   \~english CCM param
typedef struct _MV_CC_CCM_PARAM_T_
{
    bool                bCCMEnable;                                 ///< [IN]  \~chinese �Ƿ�����CCM            \~english CCM enable
    int                 nCCMat[9];                                  ///< [IN]  \~chinese CCM����(-8192~8192)    \~english Color correction matrix(-8192~8192)

    unsigned int        nRes[8];                                    ///<       \~chinese Ԥ��                   \~english Reserved

}MV_CC_CCM_PARAM;

/// \~chinese CCM����                   \~english CCM param
typedef struct _MV_CC_CCM_PARAM_EX_T_
{
    bool                bCCMEnable;                                 ///< [IN]  \~chinese �Ƿ�����CCM            \~english CCM enable
    int                 nCCMat[9];                                  ///< [IN]  \~chinese CCM����(-65536~65536)  \~english Color correction matrix(-65536~65536)
    unsigned int        nCCMScale;                                  ///< [IN]  \~chinese ����ϵ����2��������,���65536��    \~english Quantitative scale(Integer power of 2, <= 65536)

    unsigned int        nRes[8];                                    ///<       \~chinese Ԥ��                   \~english Reserved

}MV_CC_CCM_PARAM_EX;

/// \~chinese CLUT����                  \~english CLUT param
typedef struct _MV_CC_CLUT_PARAM_T_
{
    bool                bCLUTEnable;                                ///< [IN]  \~chinese �Ƿ�����CLUT           \~english CLUT enable
    unsigned int        nCLUTScale;                                 ///< [IN]  \~chinese ����ϵ��(2��������,���65536)  \~english Quantitative scale(Integer power of 2, <= 65536)
    unsigned int        nCLUTSize;                                  ///< [IN]  \~chinese CLUT��С,[17,33]������ֵ17��     \~english CLUT size[17,33](Recommended values of 17)
    unsigned char*      pCLUTBuf;                                   ///< [IN]  \~chinese ����CLUT��             \~english CLUT buffer
    unsigned int        nCLUTBufLen;                                ///< [IN]  \~chinese ����CLUT�����С(nCLUTSize*nCLUTSize*nCLUTSize*sizeof(int)*3)  \~english CLUT buffer length(nCLUTSize*nCLUTSize*nCLUTSize*sizeof(int)*3)

    unsigned int        nRes[8];                                    ///<       \~chinese Ԥ��                   \~english Reserved

}MV_CC_CLUT_PARAM;

/// \~chinese �Աȶȵ��ڽṹ��          \~english Contrast structure
typedef struct _MV_CC_CONTRAST_PARAM_T_
{
    unsigned int            nWidth;                                 ///< [IN]  \~chinese ͼ�����(��С8)        \~english Image Width
    unsigned int            nHeight;                                ///< [IN]  \~chinese ͼ��߶�(��С8)        \~english Image Height
    unsigned char*          pSrcBuf;                                ///< [IN]  \~chinese �������ݻ���           \~english Input data buffer
    unsigned int            nSrcBufLen;                             ///< [IN]  \~chinese �������ݴ�С           \~english Input data length
    enum MvGvspPixelType    enPixelType;                            ///< [IN]  \~chinese ���ظ�ʽ               \~english Pixel format

    unsigned char*          pDstBuf;                                ///< [OUT] \~chinese ������ݻ���           \~english Output data buffer
    unsigned int            nDstBufSize;                            ///< [IN]  \~chinese �ṩ�������������С   \~english Provided output buffer size
    unsigned int            nDstBufLen;                             ///< [OUT] \~chinese ������ݳ���           \~english Output data length

    unsigned int            nContrastFactor;                        ///< [IN]  \~chinese �Աȶ�ֵ��[1,10000]     \~english Contrast factor,[1,10000]

    unsigned int            nRes[8];                                ///<       \~chinese Ԥ��                   \~english Reserved

}MV_CC_CONTRAST_PARAM;

/// \~chinese �񻯽ṹ��                \~english Sharpen structure
typedef struct _MV_CC_SHARPEN_PARAM_T_
{
    unsigned int            nWidth;                                 ///< [IN]  \~chinese ͼ�����(��С8)        \~english Image Width
    unsigned int            nHeight;                                ///< [IN]  \~chinese ͼ��߶�(��С8)        \~english Image Height
    unsigned char*          pSrcBuf;                                ///< [IN]  \~chinese �������ݻ���           \~english Input data buffer
    unsigned int            nSrcBufLen;                             ///< [IN]  \~chinese �������ݴ�С           \~english Input data length
    enum MvGvspPixelType    enPixelType;                            ///< [IN]  \~chinese ���ظ�ʽ               \~english Pixel format

    unsigned char*          pDstBuf;                                ///< [OUT] \~chinese ������ݻ���           \~english Output data buffer
    unsigned int            nDstBufSize;                            ///< [IN]  \~chinese �ṩ�������������С   \~english Provided output buffer size
    unsigned int            nDstBufLen;                             ///< [OUT] \~chinese ������ݳ���           \~english Output data length

    unsigned int            nSharpenAmount;                         ///< [IN]  \~chinese ��ȵ���ǿ�ȣ�[0,500]  \~english Sharpen amount,[0,500]
    unsigned int            nSharpenRadius;                         ///< [IN]  \~chinese ��ȵ��ڰ뾶(�뾶Խ�󣬺�ʱԽ��)��[1,21]   \~english Sharpen radius(The larger the radius, the longer it takes),[1,21]
    unsigned int            nSharpenThreshold;                      ///< [IN]  \~chinese ��ȵ�����ֵ��[0,255]  \~english Sharpen threshold,[0,255]

    unsigned int            nRes[8];                                ///<       \~chinese Ԥ��                   \~english Reserved

}MV_CC_SHARPEN_PARAM;

/// \~chinese ɫ��У���ṹ��            \~english Color correct structure
typedef struct _MV_CC_COLOR_CORRECT_PARAM_T_
{
    unsigned int            nWidth;                                 ///< [IN]  \~chinese ͼ�����               \~english Image Width
    unsigned int            nHeight;                                ///< [IN]  \~chinese ͼ��߶�               \~english Image Height
    unsigned char*          pSrcBuf;                                ///< [IN]  \~chinese �������ݻ���           \~english Input data buffer
    unsigned int            nSrcBufLen;                             ///< [IN]  \~chinese �������ݴ�С           \~english Input data length
    enum MvGvspPixelType    enPixelType;                            ///< [IN]  \~chinese ���ظ�ʽ               \~english Pixel format

    unsigned char*          pDstBuf;                                ///< [OUT] \~chinese ������ݻ���           \~english Output data buffer
    unsigned int            nDstBufSize;                            ///< [IN]  \~chinese �ṩ�������������С   \~english Provided output buffer size
    unsigned int            nDstBufLen;                             ///< [OUT] \~chinese ������ݳ���           \~english Output data length

    unsigned int            nImageBit;                              ///< [IN]  \~chinese ��Чͼ��λ��(8,10,12,16)   \~english Image bit(8 or 10 or 12 or 16)
    MV_CC_GAMMA_PARAM       stGammaParam;                           ///< [IN]  \~chinese Gamma��Ϣ              \~english Gamma info
    MV_CC_CCM_PARAM_EX      stCCMParam;                             ///< [IN]  \~chinese CCM��Ϣ                \~english CCM info
    MV_CC_CLUT_PARAM        stCLUTParam;                            ///< [IN]  \~chinese CLUT��Ϣ               \~english CLUT info

    unsigned int            nRes[8];                                ///<       \~chinese Ԥ��                   \~english Reserved

}MV_CC_COLOR_CORRECT_PARAM;

/// \~chinese ����ROI�ṹ��             \~english Rect ROI structure
typedef struct _MV_CC_RECT_I_
{
    unsigned int nX;                                                ///< \~chinese �������Ͻ�X������            \~english X Position
    unsigned int nY;                                                ///< \~chinese �������Ͻ�Y������            \~english Y Position
    unsigned int nWidth;                                            ///< \~chinese ���ο���                     \~english Rect Width
    unsigned int nHeight;                                           ///< \~chinese ���θ߶�                     \~english Rect Height

}MV_CC_RECT_I;

/// \~chinese �������ƽṹ��            \~english Noise estimate structure
typedef struct _MV_CC_NOISE_ESTIMATE_PARAM_T_
{
    unsigned int            nWidth;                                 ///< [IN]  \~chinese ͼ�����(��С8)        \~english Image Width
    unsigned int            nHeight;                                ///< [IN]  \~chinese ͼ��߶�(��С8)        \~english Image Height
    enum MvGvspPixelType    enPixelType;                            ///< [IN]  \~chinese ���ظ�ʽ               \~english Pixel format
    unsigned char*          pSrcBuf;                                ///< [IN]  \~chinese �������ݻ���           \~english Input data buffer
    unsigned int            nSrcBufLen;                             ///< [IN]  \~chinese �������ݴ�С           \~english Input data length

    MV_CC_RECT_I*           pstROIRect;                             ///< [IN]  \~chinese ͼ��ROI                \~english Image ROI
    unsigned int            nROINum;                                ///< [IN]  \~chinese ROI����                \~english ROI number

    ///< \~chinese Bayer���������Ʋ�����Mono8/RGB����Ч     \~english Bayer Noise estimate param,Mono8/RGB formats are invalid
    unsigned int            nNoiseThreshold;                        ///< [IN]  \~chinese ������ֵ[0,4095]       \~english Noise threshold[0,4095]
                                                                    ///< \~chinese ����ֵ:8bit,0xE0;10bit,0x380;12bit,0xE00     \~english Suggestive value:8bit,0xE0;10bit,0x380;12bit,0xE00

    unsigned char*          pNoiseProfile;                          ///< [OUT] \~chinese �����������           \~english Output Noise Profile
    unsigned int            nNoiseProfileSize;                      ///< [IN]  \~chinese �ṩ�������������С   \~english Provided output buffer size
    unsigned int            nNoiseProfileLen;                       ///< [OUT] \~chinese ����������Գ���       \~english Output Noise Profile length

    unsigned int            nRes[8];                                ///<       \~chinese Ԥ��                   \~english Reserved

}MV_CC_NOISE_ESTIMATE_PARAM;

/// \~chinese ������ṹ��            \~english Spatial denoise structure
typedef struct _MV_CC_SPATIAL_DENOISE_PARAM_T_
{
    unsigned int            nWidth;                                 ///< [IN]  \~chinese ͼ�����(��С8)        \~english Image Width
    unsigned int            nHeight;                                ///< [IN]  \~chinese ͼ��߶�(��С8)        \~english Image Height
    enum MvGvspPixelType    enPixelType;                            ///< [IN]  \~chinese ���ظ�ʽ               \~english Pixel format
    unsigned char*          pSrcBuf;                                ///< [IN]  \~chinese �������ݻ���           \~english Input data buffer
    unsigned int            nSrcBufLen;                             ///< [IN]  \~chinese �������ݴ�С           \~english Input data length

    unsigned char*          pDstBuf;                                ///< [OUT] \~chinese �������������       \~english Output data buffer
    unsigned int            nDstBufSize;                            ///< [IN]  \~chinese �ṩ�������������С   \~english Provided output buffer size
    unsigned int            nDstBufLen;                             ///< [OUT] \~chinese ������������ݳ���   \~english Output data length

    unsigned char*          pNoiseProfile;                          ///< [IN]  \~chinese ������������           \~english Input Noise Profile
    unsigned int            nNoiseProfileLen;                       ///< [IN]  \~chinese �����������Գ���       \~english Input Noise Profile length

    ///< \~chinese Bayer������������Mono8/RGB����Ч     \~english Bayer Spatial denoise param,Mono8/RGB formats are invalid
    unsigned int            nBayerDenoiseStrength;                  ///< [IN]  \~chinese ����ǿ��[0,100]        \~english Denoise Strength[0,100]
    unsigned int            nBayerSharpenStrength;                  ///< [IN]  \~chinese ��ǿ��[0,32]         \~english Sharpen Strength[0,32]
    unsigned int            nBayerNoiseCorrect;                     ///< [IN]  \~chinese ����У��ϵ��[0,1280]   \~english Noise Correct[0,1280]

    ///< \~chinese Mono8/RGB������������Bayer����Ч     \~english Mono8/RGB Spatial denoise param,Bayer formats are invalid
    unsigned int            nNoiseCorrectLum;                       ///< [IN]  \~chinese ����У��ϵ��[1,2000]   \~english Noise Correct Lum[1,2000]
    unsigned int            nNoiseCorrectChrom;                     ///< [IN]  \~chinese ɫ��У��ϵ��[1,2000]   \~english Noise Correct Chrom[1,2000]
    unsigned int            nStrengthLum;                           ///< [IN]  \~chinese ���Ƚ���ǿ��[0,100]    \~english Strength Lum[0,100]
    unsigned int            nStrengthChrom;                         ///< [IN]  \~chinese ɫ������ǿ��[0,100]    \~english Strength Chrom[0,100]
    unsigned int            nStrengthSharpen;                       ///< [IN]  \~chinese ��ǿ��[1,1000]       \~english Strength Sharpen[1,1000]

    unsigned int            nRes[8];                                ///<       \~chinese Ԥ��                   \~english Reserved

}MV_CC_SPATIAL_DENOISE_PARAM;

/// \~chinese LSC�궨�ṹ��             \~english LSC calib structure
typedef struct _MV_CC_LSC_CALIB_PARAM_T_
{
    unsigned int            nWidth;                                 ///< [IN]  \~chinese ͼ�����[16,65535]     \~english Image Width
    unsigned int            nHeight;                                ///< [IN]  \~chinese ͼ��߶�[16-65535]     \~english Image Height
    enum MvGvspPixelType    enPixelType;                            ///< [IN]  \~chinese ���ظ�ʽ               \~english Pixel format
    unsigned char*          pSrcBuf;                                ///< [IN]  \~chinese �������ݻ���           \~english Input data buffer
    unsigned int            nSrcBufLen;                             ///< [IN]  \~chinese �������ݳ���           \~english Input data length

    unsigned char*          pCalibBuf;                              ///< [OUT] \~chinese ����궨������         \~english Output calib buffer
    unsigned int            nCalibBufSize;                          ///< [IN]  \~chinese �ṩ�ı궨�������С(nWidth*nHeight*sizeof(unsigned short))    \~english Provided output buffer size
    unsigned int            nCalibBufLen;                           ///< [OUT] \~chinese ����궨�����泤��     \~english Output calib buffer length

    unsigned int            nSecNumW;                               ///< [IN]  \~chinese ���ȷֿ���             \~english Width Sec num
    unsigned int            nSecNumH;                               ///< [IN]  \~chinese �߶ȷֿ���             \~english Height Sec num
    unsigned int            nPadCoef;                               ///< [IN]  \~chinese ��Ե���ϵ��[1,5]      \~english Pad Coef[1,5]
    unsigned int            nCalibMethod;                           ///< [IN]  \~chinese �궨��ʽ(0-����Ϊ��׼;1-��������Ϊ��׼;2-Ŀ������Ϊ��׼) \~english Calib method
    unsigned int            nTargetGray;                            ///< [IN]  \~chinese Ŀ������(�궨��ʽΪ2ʱ��Ч)    \~english Target Gray
                                                                    ///< \~chinese 8λ,��Χ��[0,255]            \~english 8bit,range:[0,255]
                                                                    ///< \~chinese 10λ,��Χ��[0,1023]          \~english 10bit,range:[0,1023]
                                                                    ///< \~chinese 12λ,��Χ��[0,4095]          \~english 12bit,range:[0,4095]

    unsigned int            nRes[8];                                ///<       \~chinese Ԥ��                   \~english Reserved

}MV_CC_LSC_CALIB_PARAM;

/// \~chinese LSCУ���ṹ��             \~english LSC correct structure
typedef struct _MV_CC_LSC_CORRECT_PARAM_T_
{
    unsigned int            nWidth;                                 ///< [IN]  \~chinese ͼ�����[16,65535]     \~english Image Width
    unsigned int            nHeight;                                ///< [IN]  \~chinese ͼ��߶�[16,65535]     \~english Image Height
    enum MvGvspPixelType    enPixelType;                            ///< [IN]  \~chinese ���ظ�ʽ               \~english Pixel format
    unsigned char*          pSrcBuf;                                ///< [IN]  \~chinese �������ݻ���           \~english Input data buffer
    unsigned int            nSrcBufLen;                             ///< [IN]  \~chinese �������ݳ���           \~english Input data length

    unsigned char*          pDstBuf;                                ///< [OUT] \~chinese ������ݻ���           \~english Output data buffer
    unsigned int            nDstBufSize;                            ///< [IN]  \~chinese �ṩ�������������С   \~english Provided output buffer size
    unsigned int            nDstBufLen;                             ///< [OUT] \~chinese ������ݳ���           \~english Output data length

    unsigned char*          pCalibBuf;                              ///< [IN]  \~chinese ����궨������         \~english Input calib buffer
    unsigned int            nCalibBufLen;                           ///< [IN]  \~chinese ����궨�����泤��     \~english Input calib buffer length

    unsigned int            nRes[8];                                ///<       \~chinese Ԥ��                   \~english Reserved

}MV_CC_LSC_CORRECT_PARAM;

/// \~chinese ˮӡ��Ϣ                  \~english  Frame-specific information
typedef struct _MV_CC_FRAME_SPEC_INFO_
{
    /// \~chinese �豸ˮӡʱ��      \~english Device frame-specific time scale
    unsigned int        nSecondCount;                               ///< [OUT] \~chinese ����                   \~english The Seconds
    unsigned int        nCycleCount;                                ///< [OUT] \~chinese ������                 \~english The Count of Cycle
    unsigned int        nCycleOffset;                               ///< [OUT] \~chinese ����ƫ����             \~english The Offset of Cycle

    float               fGain;                                      ///< [OUT] \~chinese ����                   \~english Gain
    float               fExposureTime;                              ///< [OUT] \~chinese �ع�ʱ��               \~english Exposure Time
    unsigned int        nAverageBrightness;                         ///< [OUT] \~chinese ƽ������               \~english Average brightness

    /// \~chinese ��ƽ�����        \~english White balance
    unsigned int        nRed;                                       ///< [OUT] \~chinese ��ɫ                   \~english Red
    unsigned int        nGreen;                                     ///< [OUT] \~chinese ��ɫ                   \~english Green
    unsigned int        nBlue;                                      ///< [OUT] \~chinese ��ɫ                   \~english Blue

    unsigned int        nFrameCounter;                              ///< [OUT] \~chinese ��֡��                 \~english Frame Counter
    unsigned int        nTriggerIndex;                              ///< [OUT] \~chinese ��������               \~english Trigger Counting

    unsigned int        nInput;                                     ///< [OUT] \~chinese ����                   \~english Input
    unsigned int        nOutput;                                    ///< [OUT] \~chinese ���                   \~english Output

    /// \~chinese ROI����           \~english ROI Region
    unsigned short      nOffsetX;                                   ///< [OUT] \~chinese ˮƽƫ����             \~english OffsetX
    unsigned short      nOffsetY;                                   ///< [OUT] \~chinese ��ֱƫ����             \~english OffsetY
    unsigned short      nFrameWidth;                                ///< [OUT] \~chinese ˮӡ��                 \~english The Width of Chunk
    unsigned short      nFrameHeight;                               ///< [OUT] \~chinese ˮӡ��                 \~english The Height of Chunk

    unsigned int        nReserved[16];                              ///<       \~chinese Ԥ��                   \~english Reserved

}MV_CC_FRAME_SPEC_INFO;

/// \~chinese ����������              \~english High Bandwidth decode structure
typedef struct _MV_CC_HB_DECODE_PARAM_T_
{
    unsigned char*          pSrcBuf;                                ///< [IN]  \~chinese �������ݻ���           \~english Input data buffer
    unsigned int            nSrcLen;                                ///< [IN]  \~chinese �������ݴ�С           \~english Input data size

    unsigned int            nWidth;                                 ///< [OUT] \~chinese ͼ���                 \~english Width
    unsigned int            nHeight;                                ///< [OUT] \~chinese ͼ���                 \~english Height
    unsigned char*          pDstBuf;                                ///< [OUT] \~chinese ������ݻ���           \~english Output data buffer
    unsigned int            nDstBufSize;                            ///< [IN]  \~chinese �ṩ�������������С   \~english Provided output buffer size
    unsigned int            nDstBufLen;                             ///< [OUT] \~chinese ������ݴ�С           \~english Output data size
    enum MvGvspPixelType    enDstPixelType;                         ///< [OUT] \~chinese ��������ظ�ʽ         \~english Output pixel format

    MV_CC_FRAME_SPEC_INFO   stFrameSpecInfo;                        ///< [OUT] \~chinese ˮӡ��Ϣ               \~english Frame Spec Info

    unsigned int            nRes[8];                                ///<       \~chinese Ԥ��                   \~english Reserved

}MV_CC_HB_DECODE_PARAM;

/// \~chinese ¼���ʽ����              \~english Record Format Type
typedef enum _MV_RECORD_FORMAT_TYPE_
{
    MV_FormatType_Undefined             = 0,                        ///< \~chinese δ�����¼���ʽ             \~english Undefined Recode Format Type
    MV_FormatType_AVI                   = 1,                        ///< \~chinese AVI¼���ʽ                  \~english AVI Recode Format Type

}MV_RECORD_FORMAT_TYPE;

/// \~chinese ¼�����                  \~english Record Parameters
typedef struct _MV_CC_RECORD_PARAM_T_
{
    enum MvGvspPixelType    enPixelType;                            ///< [IN]  \~chinese �������ݵ����ظ�ʽ     \~english Pixel Type

    unsigned short          nWidth;                                 ///< [IN]  \~chinese ͼ���(2�ı���)        \~english Width
    unsigned short          nHeight;                                ///< [IN]  \~chinese ͼ���(2�ı���)        \~english Height

    float                   fFrameRate;                             ///< [IN]  \~chinese ֡��fps(����1/16)      \~english The Rate of Frame
    unsigned int            nBitRate;                               ///< [IN]  \~chinese ����kbps(128-16*1024)  \~english The Rate of Bitrate

    MV_RECORD_FORMAT_TYPE   enRecordFmtType;                        ///< [IN]  \~chinese ¼���ʽ               \~english Recode Format Type

    char*                   strFilePath;                            ///< [IN]  \~chinese ¼���ļ����·��(���·���д������ģ���ת��utf-8)  \~english File Path

    unsigned int            nRes[8];                                ///<       \~chinese Ԥ��                   \~english Reserved

}MV_CC_RECORD_PARAM;

/// \~chinese �����ͼ������            \~english Input Data
typedef struct _MV_CC_INPUT_FRAME_INFO_T_
{
    unsigned char*      pData;                                      ///< [IN]  \~chinese ͼ������ָ��           \~english Record Data
    unsigned int        nDataLen;                                   ///< [IN]  \~chinese ͼ���С               \~english The Length of Record Data

    unsigned int        nRes[8];                                    ///<       \~chinese Ԥ��                   \~english Reserved

}MV_CC_INPUT_FRAME_INFO;

/// \~chinese �ɼ�ģʽ                  \~english Acquisition mode
typedef enum _MV_CAM_ACQUISITION_MODE_
{
    MV_ACQ_MODE_SINGLE                  = 0,                        ///< \~chinese ��֡ģʽ                     \~english Single Mode
    MV_ACQ_MODE_MUTLI                   = 1,                        ///< \~chinese ��֡ģʽ                     \~english Multi Mode
    MV_ACQ_MODE_CONTINUOUS              = 2,                        ///< \~chinese �����ɼ�ģʽ                 \~english Continuous Mode

}MV_CAM_ACQUISITION_MODE;

/// \~chinese ����ģʽ                  \~english Gain Mode
typedef enum _MV_CAM_GAIN_MODE_
{
    MV_GAIN_MODE_OFF                    = 0,                        ///< \~chinese �ر�                         \~english Single Mode
    MV_GAIN_MODE_ONCE                   = 1,                        ///< \~chinese һ��                         \~english Multi Mode
    MV_GAIN_MODE_CONTINUOUS             = 2,                        ///< \~chinese ����                         \~english Continuous Mode

}MV_CAM_GAIN_MODE;

/// \~chinese �ع�ģʽ                  \~english Exposure Mode
typedef enum _MV_CAM_EXPOSURE_MODE_
{
    MV_EXPOSURE_MODE_TIMED              = 0,                        ///< \~chinese ʱ��                         \~english Timed
    MV_EXPOSURE_MODE_TRIGGER_WIDTH      = 1,                        ///< \~chinese �����������                 \~english TriggerWidth
}MV_CAM_EXPOSURE_MODE;

/// \~chinese �Զ��ع�ģʽ              \~english Auto Exposure Mode
typedef enum _MV_CAM_EXPOSURE_AUTO_MODE_
{
    MV_EXPOSURE_AUTO_MODE_OFF           = 0,                        ///< \~chinese �ر�                         \~english Off
    MV_EXPOSURE_AUTO_MODE_ONCE          = 1,                        ///< \~chinese һ��                         \~english Once
    MV_EXPOSURE_AUTO_MODE_CONTINUOUS    = 2,                        ///< \~chinese ����                         \~english Continuous

}MV_CAM_EXPOSURE_AUTO_MODE;

/// \~chinese ����ģʽ                  \~english Trigger Mode
typedef enum _MV_CAM_TRIGGER_MODE_
{
    MV_TRIGGER_MODE_OFF                 = 0,                        ///< \~chinese �ر�                         \~english Off
    MV_TRIGGER_MODE_ON                  = 1,                        ///< \~chinese ��                         \~english ON

}MV_CAM_TRIGGER_MODE;

/// \~chinese Gammaѡ����               \~english Gamma Selector
typedef enum _MV_CAM_GAMMA_SELECTOR_
{
    MV_GAMMA_SELECTOR_USER              = 1,                        ///< \~chinese �û�                         \~english Gamma Selector User
    MV_GAMMA_SELECTOR_SRGB              = 2,                        ///< \~chinese sRGB                         \~english Gamma Selector sRGB

}MV_CAM_GAMMA_SELECTOR;

/// \~chinese ��ƽ��                    \~english White Balance
typedef enum _MV_CAM_BALANCEWHITE_AUTO_
{
    MV_BALANCEWHITE_AUTO_OFF            = 0,                        ///< \~chinese �ر�                         \~english Off
    MV_BALANCEWHITE_AUTO_ONCE           = 2,                        ///< \~chinese һ��                         \~english Once
    MV_BALANCEWHITE_AUTO_CONTINUOUS     = 1,                        ///< \~chinese ����                         \~english Continuous

}MV_CAM_BALANCEWHITE_AUTO;

/// \~chinese ����Դ                    \~english Trigger Source
typedef enum _MV_CAM_TRIGGER_SOURCE_
{
    MV_TRIGGER_SOURCE_LINE0             = 0,                        ///< \~chinese Line0                        \~english Line0
    MV_TRIGGER_SOURCE_LINE1             = 1,                        ///< \~chinese Line1                        \~english Line1
    MV_TRIGGER_SOURCE_LINE2             = 2,                        ///< \~chinese Line2                        \~english Line2
    MV_TRIGGER_SOURCE_LINE3             = 3,                        ///< \~chinese Line3                        \~english Line3
    MV_TRIGGER_SOURCE_COUNTER0          = 4,                        ///< \~chinese Conuter0                     \~english Conuter0

    MV_TRIGGER_SOURCE_SOFTWARE          = 7,                        ///< \~chinese ������                       \~english Software
    MV_TRIGGER_SOURCE_FrequencyConverter= 8,                        ///< \~chinese ��Ƶ��                       \~english Frequency Converter

}MV_CAM_TRIGGER_SOURCE;

/// \~chinese GigEVision IP����         \~english GigEVision IP Configuration
#define MV_IP_CFG_STATIC                0x05000000                  ///< \~chinese ��̬                         \~english Static
#define MV_IP_CFG_DHCP                  0x06000000                  ///< \~chinese DHCP                         \~english DHCP
#define MV_IP_CFG_LLA                   0x04000000                  ///< \~chinese LLA                          \~english LLA

/// \~chinese GigEVision���紫��ģʽ    \~english GigEVision Net Transfer Mode
#define MV_NET_TRANS_DRIVER             0x00000001                  ///< \~chinese ����                         \~english Driver
#define MV_NET_TRANS_SOCKET             0x00000002                  ///< \~chinese Socket                       \~english Socket

/// \~chinese CameraLink������          \~english CameraLink Baud Rates (CLUINT32)
#define MV_CAML_BAUDRATE_9600           0x00000001                  ///< \~chinese 9600                         \~english 9600
#define MV_CAML_BAUDRATE_19200          0x00000002                  ///< \~chinese 19200                        \~english 19200
#define MV_CAML_BAUDRATE_38400          0x00000004                  ///< \~chinese 38400                        \~english 38400
#define MV_CAML_BAUDRATE_57600          0x00000008                  ///< \~chinese 57600                        \~english 57600
#define MV_CAML_BAUDRATE_115200         0x00000010                  ///< \~chinese 115200                       \~english 115200
#define MV_CAML_BAUDRATE_230400         0x00000020                  ///< \~chinese 230400                       \~english 230400
#define MV_CAML_BAUDRATE_460800         0x00000040                  ///< \~chinese 460800                       \~english 460800
#define MV_CAML_BAUDRATE_921600         0x00000080                  ///< \~chinese 921600                       \~english 921600
#define MV_CAML_BAUDRATE_AUTOMAX        0x40000000                  ///< \~chinese ���ֵ                       \~english Auto Max

/// \~chinese �쳣��Ϣ����              \~english Exception message type
#define MV_EXCEPTION_DEV_DISCONNECT     0x00008001                  ///< \~chinese �豸�Ͽ�����                 \~english The device is disconnected
#define MV_EXCEPTION_VERSION_CHECK      0x00008002                  ///< \~chinese SDK�������汾��ƥ��          \~english SDK does not match the driver version

///< \~chinese �豸Event�¼�������󳤶�    \~english Max length of event name
#define MAX_EVENT_NAME_SIZE             128

/// \~chinese Event�¼��ص���Ϣ\        \~english Event callback infomation
typedef struct _MV_EVENT_OUT_INFO_
{
    char                EventName[MAX_EVENT_NAME_SIZE];             ///< [OUT] \~chinese Event����              \~english Event name

    unsigned short      nEventID;                                   ///< [OUT] \~chinese Event��                \~english Event ID
    unsigned short      nStreamChannel;                             ///< [OUT] \~chinese ��ͨ�����             \~english Circulation number

    unsigned int        nBlockIdHigh;                               ///< [OUT] \~chinese ֡�Ÿ�λ               \~english BlockId high
    unsigned int        nBlockIdLow;                                ///< [OUT] \~chinese ֡�ŵ�λ               \~english BlockId low

    unsigned int        nTimestampHigh;                             ///< [OUT] \~chinese ʱ�����λ             \~english Timestramp high
    unsigned int        nTimestampLow;                              ///< [OUT] \~chinese ʱ�����λ             \~english Timestramp low

    void*               pEventData;                                 ///< [OUT] \~chinese Event����              \~english Event data
    unsigned int        nEventDataSize;                             ///< [OUT] \~chinese Event���ݳ���          \~english Event data len

    unsigned int        nReserved[16];                              ///<       \~chinese Ԥ��                   \~english Reserved

}MV_EVENT_OUT_INFO;

/// \~chinese �ļ���ȡ                  \~english File Access
typedef struct _MV_CC_FILE_ACCESS_T
{
    const char*         pUserFileName;                              ///< [IN]  \~chinese �û��ļ���             \~english User file name
    const char*         pDevFileName;                               ///< [IN]  \~chinese �豸�ļ���             \~english Device file name

    unsigned int        nReserved[32];                              ///<       \~chinese Ԥ��                   \~english Reserved

}MV_CC_FILE_ACCESS;

/// \~chinese �ļ���ȡ����              \~english File Access Progress
typedef struct _MV_CC_FILE_ACCESS_PROGRESS_T
{
    int64_t             nCompleted;                                 ///< [OUT] \~chinese ����ɵĳ���           \~english Completed Length
    int64_t             nTotal;                                     ///< [OUT] \~chinese �ܳ���                 \~english Total Length

    unsigned int        nReserved[8];                               ///<       \~chinese Ԥ��                   \~english Reserved

}MV_CC_FILE_ACCESS_PROGRESS;

/// \~chinese Gige�Ĵ�������            \~english The transmission type of Gige
typedef enum _MV_GIGE_TRANSMISSION_TYPE_
{
    MV_GIGE_TRANSTYPE_UNICAST               = 0x0,                  ///< \~chinese ��ʾ����(Ĭ��)                           \~english Unicast mode
    MV_GIGE_TRANSTYPE_MULTICAST             = 0x1,                  ///< \~chinese ��ʾ�鲥                                 \~english Multicast mode
    MV_GIGE_TRANSTYPE_LIMITEDBROADCAST      = 0x2,                  ///< \~chinese ��ʾ�������ڹ㲥���ݲ�֧��               \~english Limited broadcast mode,not support
    MV_GIGE_TRANSTYPE_SUBNETBROADCAST       = 0x3,                  ///< \~chinese ��ʾ�����ڹ㲥���ݲ�֧��                 \~english Subnet broadcast mode,not support
    MV_GIGE_TRANSTYPE_CAMERADEFINED         = 0x4,                  ///< \~chinese ��ʾ���豸��ȡ���ݲ�֧��                 \~english Transtype from camera,not support
    MV_GIGE_TRANSTYPE_UNICAST_DEFINED_PORT  = 0x5,                  ///< \~chinese ��ʾ�û��Զ���Ӧ�ö˽���ͼ������Port��   \~english User Defined Receive Data Port
    MV_GIGE_TRANSTYPE_UNICAST_WITHOUT_RECV  = 0x00010000,           ///< \~chinese ��ʾ�����˵���������ʵ��������ͼ������   \~english Unicast without receive data
    MV_GIGE_TRANSTYPE_MULTICAST_WITHOUT_RECV= 0x00010001,           ///< \~chinese ��ʾ�鲥ģʽ������ʵ��������ͼ������     \~english Multicast without receive data

}MV_GIGE_TRANSMISSION_TYPE;

/// \~chinese ���紫��ģʽ              \~english Transmission type
typedef struct _MV_TRANSMISSION_TYPE_T
{
    MV_GIGE_TRANSMISSION_TYPE   enTransmissionType;                 ///< [IN]  \~chinese ����ģʽ                   \~english Transmission type
    unsigned int                nDestIp;                            ///< [IN]  \~chinese Ŀ��IP���鲥ģʽ��������   \~english Destination IP
    unsigned short              nDestPort;                          ///< [IN]  \~chinese Ŀ��Port���鲥ģʽ�������� \~english Destination port

    unsigned int                nReserved[32];                      ///<       \~chinese Ԥ��                       \~english Reserved

}MV_TRANSMISSION_TYPE;

/// \~chinese ����������Ϣ              \~english Action Command
typedef struct _MV_ACTION_CMD_INFO_T
{
    unsigned int        nDeviceKey;                                 ///< [IN]  \~chinese �豸��Կ                                   \~english Device Key;
    unsigned int        nGroupKey;                                  ///< [IN]  \~chinese ���                                       \~english Group Key
    unsigned int        nGroupMask;                                 ///< [IN]  \~chinese ������                                     \~english Group Mask

    unsigned int        bActionTimeEnable;                          ///< [IN]  \~chinese ֻ�����ó�1ʱAction Time����Ч����1ʱ��Ч  \~english Action Time Enable
    int64_t             nActionTime;                                ///< [IN]  \~chinese Ԥ����ʱ�䣬����Ƶ�й�                     \~english Action Time

    const char*         pBroadcastAddress;                          ///< [IN]  \~chinese �㲥����ַ                                 \~english Broadcast Address
    unsigned int        nTimeOut;                                   ///< [IN]  \~chinese �ȴ�ACK�ĳ�ʱʱ�䣬���Ϊ0��ʾ����ҪACK    \~english TimeOut

    unsigned int        bSpecialNetEnable;                          ///< [IN]  \~chinese ֻ�����ó�1ʱָ��������IP����Ч����1ʱ��Ч \~english Special IP Enable
    unsigned int        nSpecialNetIP;                              ///< [IN]  \~chinese ָ��������IP                               \~english Special Net IP address

    unsigned int        nReserved[14];                              ///<       \~chinese Ԥ��                                       \~english Reserved

}MV_ACTION_CMD_INFO;

/// \~chinese �����������Ϣ          \~english Action Command Result
typedef struct _MV_ACTION_CMD_RESULT_T
{
    unsigned char       strDeviceAddress[12 + 3 + 1];               ///< [OUT] \~chinese �豸IP                 \~english IP address of the device

    int                 nStatus;                                    ///< [OUT] \~chinese ״̬��                 \~english status code returned by the device
                                                                    //1.0x0000:success.
                                                                    //2.0x8001:Command is not supported by the device.
                                                                    //3.0x8013:The device is not synchronized to a master clock to be used as time reference.
                                                                    //4.0x8015:A device queue or packet data has overflowed.
                                                                    //5.0x8016:The requested scheduled action command was requested at a time that is already past.

    unsigned int        nReserved[4];                               ///<      \~chinese Ԥ��                    \~english Reserved

}MV_ACTION_CMD_RESULT;

/// \~chinese �����������Ϣ�б�      \~english Action Command Result List
typedef struct _MV_ACTION_CMD_RESULT_LIST_T
{
    unsigned int            nNumResults;                            ///< [OUT] \~chinese ����ֵ����             \~english Number of returned values
    MV_ACTION_CMD_RESULT*   pResults;                               ///< [OUT] \~chinese ����������           \~english Reslut of action command

}MV_ACTION_CMD_RESULT_LIST;

/// \~chinese ÿ���ڵ��Ӧ�Ľӿ�����    \~english Interface type corresponds to each node 
enum MV_XML_InterfaceType
{
    IFT_IValue,                                                     ///< \~chinese Value                        \~english IValue interface
    IFT_IBase,                                                      ///< \~chinese Base                         \~english IBase interface
    IFT_IInteger,                                                   ///< \~chinese Integer                      \~english IInteger interface
    IFT_IBoolean,                                                   ///< \~chinese Boolean                      \~english IBoolean interface
    IFT_ICommand,                                                   ///< \~chinese Command                      \~english ICommand interface
    IFT_IFloat,                                                     ///< \~chinese Float                        \~english IFloat interface
    IFT_IString,                                                    ///< \~chinese String                       \~english IString interface
    IFT_IRegister,                                                  ///< \~chinese Register                     \~english IRegister interface
    IFT_ICategory,                                                  ///< \~chinese Category                     \~english ICategory interface
    IFT_IEnumeration,                                               ///< \~chinese Enumeration                  \~english IEnumeration interface
    IFT_IEnumEntry,                                                 ///< \~chinese EnumEntry                    \~english IEnumEntry interface
    IFT_IPort,                                                      ///< \~chinese Port                         \~english IPort interface
};

/// \~chinese �ڵ�ķ���ģʽ            \~english Node Access Mode
enum MV_XML_AccessMode
{
    AM_NI,                                                          ///< \~chinese ����ʵ��                     \~english Not implemented
    AM_NA,                                                          ///< \~chinese ������                       \~english Not available
    AM_WO,                                                          ///< \~chinese ֻд                         \~english Write Only
    AM_RO,                                                          ///< \~chinese ֻ��                         \~english Read Only
    AM_RW,                                                          ///< \~chinese ��д                         \~english Read and Write
    AM_Undefined,                                                   ///< \~chinese δ����                       \~english Object is not yet initialized
    AM_CycleDetect,                                                 ///< \~chinese �ڲ�����AccessModeѭ�����   \~english used internally for AccessMode cycle detection
};

/// \~chinese ���XML������             \~english Max XML Symbolic Number 
#define MV_MAX_XML_SYMBOLIC_NUM         64
/// \~chinese ö������ֵ                \~english Enumeration Value
typedef struct _MVCC_ENUMVALUE_T
{
    unsigned int        nCurValue;                                  ///< [OUT] \~chinese ��ǰֵ                 \~english Current Value
    unsigned int        nSupportedNum;                              ///< [OUT] \~chinese ���ݵ���Ч���ݸ���     \~english Number of valid data
    unsigned int        nSupportValue[MV_MAX_XML_SYMBOLIC_NUM];     ///< [OUT] \~chinese ֧�ֵ�ö��ֵ           \~english Support Value 

    unsigned int        nReserved[4];                               ///<       \~chinese Ԥ��                   \~english Reserved

}MVCC_ENUMVALUE;

/// \~chinese Int����ֵ                 \~english Int Value
typedef struct _MVCC_INTVALUE_T
{
    unsigned int        nCurValue;                                  ///< [OUT] \~chinese ��ǰֵ                 \~english Current Value
    unsigned int        nMax;                                       ///< [OUT] \~chinese ���ֵ                 \~english Max
    unsigned int        nMin;                                       ///< [OUT] \~chinese ��Сֵ                 \~english Min
    unsigned int        nInc;                                       ///< [OUT] \~chinese                        \~english Inc

    unsigned int        nReserved[4];                               ///<       \~chinese Ԥ��                   \~english Reserved

}MVCC_INTVALUE;

/// \~chinese Int����ֵEx               \~english Int Value Ex
typedef struct _MVCC_INTVALUE_EX_T
{
    int64_t             nCurValue;                                  ///< [OUT] \~chinese ��ǰֵ                 \~english Current Value
    int64_t             nMax;                                       ///< [OUT] \~chinese ���ֵ                 \~english Max
    int64_t             nMin;                                       ///< [OUT] \~chinese ��Сֵ                 \~english Min
    int64_t             nInc;                                       ///< [OUT] \~chinese Inc                    \~english Inc

    unsigned int        nReserved[16];                              ///<       \~chinese Ԥ��                   \~english Reserved

}MVCC_INTVALUE_EX;

/// \~chinese Float����ֵ               \~english Float Value
typedef struct _MVCC_FLOATVALUE_T
{
    float               fCurValue;                                  ///< [OUT] \~chinese ��ǰֵ                 \~english Current Value
    float               fMax;                                       ///< [OUT] \~chinese ���ֵ                 \~english Max
    float               fMin;                                       ///< [OUT] \~chinese ��Сֵ                 \~english Min

    unsigned int        nReserved[4];                               ///<       \~chinese Ԥ��                   \~english Reserved

}MVCC_FLOATVALUE;

/// \~chinese String����ֵ              \~english String Value
typedef struct _MVCC_STRINGVALUE_T
{
    char                chCurValue[256];                            ///< [OUT] \~chinese ��ǰֵ                 \~english Current Value

    int64_t             nMaxLength;                                 ///< [OUT] \~chinese ��󳤶�               \~english MaxLength
    unsigned int        nReserved[2];                               ///<       \~chinese Ԥ��                   \~english Reserved

}MVCC_STRINGVALUE;

#endif /* _MV_CAMERA_PARAMS_H_ */
