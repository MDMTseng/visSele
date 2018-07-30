#ifndef _MVCAMAPI_H_
#define _MVCAMAPI_H_


#ifdef DLL_EXPORT
#define MVSDK_API extern "C" __declspec(dllexport)
#else
#define MVSDK_API extern "C" __declspec(dllimport)
#endif

#include "CameraDefine.h"
#include "CameraStatus.h"

//BIG5 TRANS ALLOWED

/******************************************************/
// 函数名   : CameraSdkInit
// 功能描述 : 相机SDK初始化，在调用任何SDK其他接口前，必须
//        先调用该接口进行初始化。该函数在整个进程运行
//        期间只需要调用一次。
// 参数     : iLanguageSel 用于选择SDK内部提示信息和界面的语种,
//               0:表示英文,1:表示中文。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraSdkInit(
    int     iLanguageSel
);

/******************************************************/
// 函数名   : CameraEnumerateDevice
// 功能描述 : 枚举设备，并建立设备列表。在调用CameraInit
//        之前，必须调用该函数来获得设备的信息。
// 参数     : pCameraList    设备列表数组指针。
//             piNums        设备的个数指针，调用时传入pCameraList
//                            数组的元素个数，函数返回时，保存实际找到的设备个数。
//              注意，piNums指向的值必须初始化，且不超过pCameraList数组元素个数，
//              否则有可能造成内存溢出。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraEnumerateDevice(
    tSdkCameraDevInfo* pCameraList,
    INT*               piNums
);


/******************************************************/
// 函数名 	: CameraEnumerateDeviceEx
// 功能描述	: 枚举设备，并建立设备列表。在调用CameraInitEx
//			  之前，必须调用该函数枚举设备。
// 参数	     :
// 返回值     : 返回设备个数，0表示无。
/******************************************************/
MVSDK_API INT __stdcall CameraEnumerateDeviceEx(
);


/******************************************************/
// 函数名   : CameraIsOpened
// 功能描述 : 检测设备是否已经被其他应用程序打开。在调用CameraInit
//        之前，可以使用该函数进行检测，如果已经被打开，调用
//        CameraInit会返回设备已经被打开的错误码。
// 参数     : pCameraList 设备的枚举信息结构体指针，由CameraEnumerateDevice获得。
//            pOpened       设备的状态指针，返回设备是否被打开的状态，TRUE为打开，FALSE为空闲。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraIsOpened(
  tSdkCameraDevInfo*  pCameraList,
  BOOL*               pOpened
);


/******************************************************/
// 函数名   : CameraInit
// 功能描述 : 相机初始化。初始化成功后，才能调用任何其他
//        相机相关的操作接口。
// 参数     : pCameraInfo    该相机的设备描述信息，由CameraEnumerateDevice
//               函数获得。
//            iParamLoadMode  相机初始化时使用的参数加载方式。-1表示使用上次退出时的参数加载方式。
//                             为 PARAM_MODE_BY_MODEL 表示按型号加载
//                             为 PARAM_MODE_BY_SN 表示按序列号加载
//                             为 PARAM_MODE_BY_NAME 表示按昵称加载
//                             详细请参开CameraDefine.h中 emSdkParameterMode 定义。
//            emTeam         初始化时使用的参数组。-1表示加载上次退出时的参数组。
//            pCameraHandle  相机的句柄指针，初始化成功后，该指针
//               返回该相机的有效句柄，在调用其他相机
//               相关的操作接口时，都需要传入该句柄，主要
//               用于多相机之间的区分。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraInit(
    tSdkCameraDevInfo*  pCameraInfo,
    int                 emParamLoadMode,
    int                 emTeam,
    CameraHandle*       pCameraHandle
);

/******************************************************/
// 函数名 	: CameraInitEx
// 功能描述	: 相机初始化。初始化成功后，才能调用任何其他
//			  相机相关的操作接口。
// 参数	    : iDeviceIndex    相机的索引号，CameraEnumerateDeviceEx返回相机个数。
//            iParamLoadMode  相机初始化时使用的参数加载方式。-1表示使用上次退出时的参数加载方式。
//                             为 PARAM_MODE_BY_MODEL 表示按型号加载
//                             为 PARAM_MODE_BY_SN 表示按序列号加载
//                             为 PARAM_MODE_BY_NAME 表示按昵称加载
//                             详细请参开CameraDefine.h中 emSdkParameterMode 定义。
//            emTeam         初始化时使用的参数组。-1表示加载上次退出时的参数组。
//            pCameraHandle  相机的句柄指针，初始化成功后，该指针
//							 返回该相机的有效句柄，在调用其他相机
//							 相关的操作接口时，都需要传入该句柄，主要
//							 用于多相机之间的区分。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraInitEx(
    int             iDeviceIndex,
    int             iParamLoadMode,
    int             emTeam,
    CameraHandle*   pCameraHandle
);

/******************************************************/
// 函数名 	: CameraInitEx2
// 功能描述	: 相机初始化。初始化成功后，才能调用任何其他
//			  相机相关的操作接口。	注意需要先调用CameraEnumerateDeviceEx枚举相机
// 参数	    : CameraName    相机名称
//            pCameraHandle  相机的句柄指针，初始化成功后，该指针
//							 返回该相机的有效句柄，在调用其他相机
//							 相关的操作接口时，都需要传入该句柄，主要
//							 用于多相机之间的区分。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraInitEx2(
	char* CameraName,
	CameraHandle   *pCameraHandle
);

/******************************************************/
// 函数名   : CameraSetCallbackFunction
// 功能描述 : 设置图像捕获的回调函数。当捕获到新的图像数据帧时，
//        pCallBack所指向的回调函数就会被调用。
// 参数     : hCamera 相机的句柄，由CameraInit函数获得。
//            pCallBack 回调函数指针。
//            pContext  回调函数的附加参数，在回调函数被调用时
//            该附加参数会被传入，可以为NULL。多用于
//            多个相机时携带附加信息。
//            pCallbackOld  用于保存当前的回调函数。可以为NULL。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraSetCallbackFunction(
    CameraHandle        hCamera,
    CAMERA_SNAP_PROC    pCallBack,
    PVOID               pContext,
    CAMERA_SNAP_PROC*   pCallbackOld
);

/******************************************************/
// 函数名   : CameraUnInit
// 功能描述 : 相机反初始化。释放资源。
// 参数     : hCamera 相机的句柄，由CameraInit函数获得。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraUnInit(
    CameraHandle hCamera
);

/******************************************************/
// 函数名   : CameraGetInformation
// 功能描述 : 获得相机的描述信息
// 参数     : hCamera 相机的句柄，由CameraInit函数获得。
//            pbuffer 指向相机描述信息指针的指针。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraGetInformation(
    CameraHandle    hCamera,
    char**          pbuffer
);

/******************************************************/
// 函数名   : CameraImageProcess
// 功能描述 : 将获得的相机原始输出图像数据进行处理，叠加饱和度、
//        颜色增益和校正、降噪等处理效果，最后得到RGB888
//        格式的图像数据。
// 参数     : hCamera  相机的句柄，由CameraInit函数获得。
//            pbyIn    输入图像数据的缓冲区地址，不能为NULL。
//            pbyOut   处理后图像输出的缓冲区地址，不能为NULL。
//            pFrInfo  输入图像的帧头信息，处理完成后，帧头信息
//             中的图像格式uiMediaType会随之改变。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraImageProcess(
    CameraHandle        hCamera,
    BYTE*               pbyIn,
    BYTE*               pbyOut,
    tSdkFrameHead*      pFrInfo
);

/******************************************************/
// 函数名 	: CameraImageProcessEx
// 功能描述	: 将获得的相机原始输出图像数据进行处理，叠加饱和度、
//			  颜色增益和校正、降噪等处理效果，最后得到RGB888
//			  格式的图像数据。
// 参数	    : hCamera      相机的句柄，由CameraInit函数获得。
//            pbyIn	     输入图像数据的缓冲区地址，不能为NULL。
//            pbyOut        处理后图像输出的缓冲区地址，不能为NULL。
//            pFrInfo       输入图像的帧头信息，处理完成后，帧头信息
//            uOutFormat    处理完后图像的输出格式可以是CAMERA_MEDIA_TYPE_MONO8 CAMERA_MEDIA_TYPE_RGB CAMERA_MEDIA_TYPE_RGBA8的其中一种。
//                          pbyIn对应的缓冲区大小，必须和uOutFormat指定的格式相匹配。
//            uReserved     预留参数，必须设置为0
//					   中的图像格式uiMediaType会随之改变。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraImageProcessEx(
    CameraHandle        hCamera,
    BYTE*               pbyIn,
    BYTE*               pbyOut,
    tSdkFrameHead*      pFrInfo,
    UINT                uOutFormat,
    UINT                uReserved
);



/******************************************************/
// 函数名   : CameraDisplayInit
// 功能描述 : 初始化SDK内部的显示模块。在调用CameraDisplayRGB24
//        前必须先调用该函数初始化。如果您在二次开发中，
//        使用自己的方式进行图像显示(不调用CameraDisplayRGB24)，
//        则不需要调用本函数。
// 参数     : hCamera   相机的句柄，由CameraInit函数获得。
//            hWndDisplay 显示窗口的句柄，一般为窗口的m_hWnd成员。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraDisplayInit(
    CameraHandle    hCamera,
    HWND            hWndDisplay
);

/******************************************************/
// 函数名   : CameraDisplayRGB24
// 功能描述 : 显示图像。必须调用过CameraDisplayInit进行
//        初始化才能调用本函数。
// 参数     : hCamera  相机的句柄，由CameraInit函数获得。
//            pbyRGB24 图像的数据缓冲区，RGB888格式。
//            pFrInfo  图像的帧头信息。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraDisplayRGB24(
    CameraHandle        hCamera,
    BYTE*               pbyRGB24,
    tSdkFrameHead*      pFrInfo
);

/******************************************************/
// 函数名   : CameraSetDisplayMode
// 功能描述 : 设置显示的模式。必须调用过CameraDisplayInit
//        进行初始化才能调用本函数。
// 参数     : hCamera  相机的句柄，由CameraInit函数获得。
//            iMode    显示模式，DISPLAYMODE_SCALE或者
//             DISPLAYMODE_REAL,具体参见CameraDefine.h
//             中emSdkDisplayMode的定义。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraSetDisplayMode(
    CameraHandle    hCamera,
    INT             iMode
);

/******************************************************/
// 函数名   : CameraSetDisplayOffset
// 功能描述 : 设置显示的起始偏移值。仅当显示模式为DISPLAYMODE_REAL
//        时有效。例如显示控件的大小为320X240，而图像的
//        的尺寸为640X480，那么当iOffsetX = 160,iOffsetY = 120时
//        显示的区域就是图像的居中320X240的位置。必须调用过
//        CameraDisplayInit进行初始化才能调用本函数。
// 参数     : hCamera   相机的句柄，由CameraInit函数获得。
//            iOffsetX  偏移的X坐标。
//            iOffsetY  偏移的Y坐标。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraSetDisplayOffset(
    CameraHandle    hCamera,
    int             iOffsetX,
    int             iOffsetY
);

/******************************************************/
// 函数名   : CameraSetDisplaySize
// 功能描述 : 设置显示控件的尺寸。必须调用过
//        CameraDisplayInit进行初始化才能调用本函数。
// 参数     : hCamera   相机的句柄，由CameraInit函数获得。
//            iWidth    宽度
//            iHeight   高度
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraSetDisplaySize(
    CameraHandle    hCamera,
    INT             iWidth,
    INT             iHeight
);

/******************************************************/
// 函数名   : CameraGetImageBuffer
// 功能描述 : 获得一帧图像数据。为了提高效率，SDK在图像抓取时采用了零拷贝机制，
//        CameraGetImageBuffer实际获得是内核中的一个缓冲区地址，
//        该函数成功调用后，必须调用CameraReleaseImageBuffer释放由
//        CameraGetImageBuffer得到的缓冲区,以便让内核继续使用
//        该缓冲区。
// 参数     : hCamera   相机的句柄，由CameraInit函数获得。
//            pFrameInfo  图像的帧头信息指针。
//            pbyBuffer   指向图像的数据的缓冲区指针。由于
//              采用了零拷贝机制来提高效率，因此
//              这里使用了一个指向指针的指针。
//            UINT wTimes 抓取图像的超时时间。单位毫秒。在
//              wTimes时间内还未获得图像，则该函数
//              会返回超时信息。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraGetImageBuffer(
    CameraHandle        hCamera,
    tSdkFrameHead*      pFrameInfo,
    BYTE**              pbyBuffer,
    UINT                wTimes
);

/******************************************************/
// 函数名 	: CameraGetImageBufferEx
// 功能描述	: 获得一帧图像数据。该接口获得的图像是经过处理后的RGB格式。该函数调用后，
//			  不需要调用 CameraReleaseImageBuffer 释放，也不要调用free之类的函数释放
//              来释放该函数返回的图像数据缓冲区。
// 参数	    : hCamera	  相机的句柄，由CameraInit函数获得。
//            piWidth    整形指针，返回图像的宽度
//            piHeight   整形指针，返回图像的高度
//            UINT wTimes 抓取图像的超时时间。单位毫秒。在
//						  wTimes时间内还未获得图像，则该函数
//						  会返回超时信息。
// 返回值   : 成功时，返回RGB数据缓冲区的首地址;
//            否则返回0。
/******************************************************/
MVSDK_API unsigned char* __stdcall CameraGetImageBufferEx(
    CameraHandle        hCamera,
    INT*                piWidth,
    INT*                piHeight,
    UINT                wTimes
);


/******************************************************/
// 函数名   : CameraSnapToBuffer
// 功能描述 : 抓拍一张图像到缓冲区中。相机会进入抓拍模式，并且
//        自动切换到抓拍模式的分辨率进行图像捕获。然后将
//        捕获到的数据保存到缓冲区中。
//        该函数成功调用后，必须调用CameraReleaseImageBuffer
//        释放由CameraSnapToBuffer得到的缓冲区。具体请参考
//        CameraGetImageBuffer函数的功能描述部分。
// 参数     : hCamera   相机的句柄，由CameraInit函数获得。
//            pFrameInfo  指针，返回图像的帧头信息。
//            pbyBuffer   指向指针的指针，用来返回图像缓冲区的地址。
//            uWaitTimeMs 超时时间，单位毫秒。在该时间内，如果仍然没有
//              成功捕获的数据，则返回超时信息。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraSnapToBuffer(
    CameraHandle        hCamera,
    tSdkFrameHead*      pFrameInfo,
    BYTE**              pbyBuffer,
    UINT                uWaitTimeMs
);

/******************************************************/
// 函数名   : CameraReleaseImageBuffer
// 功能描述 : 释放由CameraGetImageBuffer获得的缓冲区。
// 参数     : hCamera   相机的句柄，由CameraInit函数获得。
//            pbyBuffer   由CameraGetImageBuffer获得的缓冲区地址。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraReleaseImageBuffer(
    CameraHandle    hCamera,
    BYTE*           pbyBuffer
);

/******************************************************/
// 函数名   : CameraPlay
// 功能描述 : 让SDK进入工作模式，开始接收来自相机发送的图像
//        数据。如果当前相机是触发模式，则需要接收到
//        触发帧以后才会更新图像。
// 参数     : hCamera   相机的句柄，由CameraInit函数获得。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraPlay(
    CameraHandle hCamera
);

/******************************************************/
// 函数名   : CameraPause
// 功能描述 : 让SDK进入暂停模式，不接收来自相机的图像数据，
//        同时也会发送命令让相机暂停输出，释放传输带宽。
//        暂停模式下，可以对相机的参数进行配置，并立即生效。
// 参数     : hCamera   相机的句柄，由CameraInit函数获得。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraPause(
    CameraHandle hCamera
);

/******************************************************/
// 函数名   : CameraStop
// 功能描述 : 让SDK进入停止状态，一般是反初始化时调用该函数，
//        该函数被调用，不能再对相机的参数进行配置。
// 参数     : hCamera   相机的句柄，由CameraInit函数获得。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraStop(
    CameraHandle hCamera
);

/******************************************************/
// 函数名   : CameraInitRecord
// 功能描述 : 初始化一次录像。
// 参数     : hCamera   相机的句柄，由CameraInit函数获得。
//            iFormat   录像的格式，当前只支持不压缩和MSCV两种方式。
//              0:不压缩；1:MSCV方式压缩。
//            pcSavePath  录像文件保存的路径。
//            b2GLimit    如果为TRUE,则文件大于2G时自动分割。
//            dwQuality   录像的质量因子，越大，则质量越好。范围1到100.
//            iFrameRate  录像的帧率。建议设定的比实际采集帧率大，
//              这样就不会漏帧。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraInitRecord(
    CameraHandle    hCamera,
    int             iFormat,
    char*           pcSavePath,
    BOOL            b2GLimit,
    DWORD           dwQuality,
    int             iFrameRate
);

/******************************************************/
// 函数名   : CameraStopRecord
// 功能描述 : 结束本次录像。当CameraInitRecord后，可以通过该函数
//        来结束一次录像，并完成文件保存操作。
// 参数     : hCamera   相机的句柄，由CameraInit函数获得。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraStopRecord(
    CameraHandle    hCamera
);

/******************************************************/
// 函数名   : CameraPushFrame
// 功能描述 : 将一帧数据存入录像流中。必须调用CameraInitRecord
//        才能调用该函数。CameraStopRecord调用后，不能再调用
//        该函数。由于我们的帧头信息中携带了图像采集的时间戳
//        信息，因此录像可以精准的时间同步，而不受帧率不稳定
//        的影响。
// 参数     : hCamera     相机的句柄，由CameraInit函数获得。
//            pbyImageBuffer    图像的数据缓冲区，必须是RGB格式。
//            pFrInfo           图像的帧头信息。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraPushFrame(
    CameraHandle    hCamera,
    BYTE*           pbyImageBuffer,
    tSdkFrameHead*  pFrInfo
);

/******************************************************/
// 函数名   : CameraSaveImage
// 功能描述 : 将图像缓冲区的数据保存成图片文件。
// 参数     : hCamera    相机的句柄，由CameraInit函数获得。
//            lpszFileName   图片保存文件完整路径。
//            pbyImageBuffer 图像的数据缓冲区。
//            pFrInfo        图像的帧头信息。
//            byFileType     图像保存的格式。取值范围参见CameraDefine.h
//               中emSdkFileType的类型定义。目前支持
//               BMP、JPG、PNG、RAW四种格式。其中RAW表示
//               相机输出的原始数据，保存RAW格式文件要求
//               pbyImageBuffer和pFrInfo是由CameraGetImageBuffer
//               获得的数据，而且未经CameraImageProcess转换
//               成BMP格式；反之，如果要保存成BMP、JPG或者
//               PNG格式，则pbyImageBuffer和pFrInfo是由
//               CameraImageProcess处理后的RGB格式数据。
//                 具体用法可以参考Advanced的例程。
//            byQuality      图像保存的质量因子，仅当保存为JPG格式
//                 时该参数有效，范围1到100。其余格式
//                           可以写成0。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraSaveImage(
    CameraHandle    hCamera,
    char*           lpszFileName,
    BYTE*           pbyImageBuffer,
    tSdkFrameHead*  pFrInfo,
    UINT            byFileType,
    BYTE            byQuality
);

/******************************************************/
// 函数名   : CameraSaveImageEx
// 功能描述 : 将图像缓冲区的数据保存成图片文件。
// 参数     : hCamera    相机的句柄，由CameraInit函数获得。
//            lpszFileName   图片保存文件完整路径。
//            pbyImageBuffer 图像的数据缓冲区。
//            uImageFormat	 0 : 8 BIT gray 1:rgb24 2:rgba32 3:bgr24 4:bgra32
//			  iWidth		 图片宽度
//			  iHeight		 图片高度
//            byFileType     图像保存的格式。取值范围参见CameraDefine.h
//               中emSdkFileType的类型定义。目前支持
//               BMP、JPG、PNG、RAW四种格式。其中RAW表示
//               相机输出的原始数据，保存RAW格式文件要求
//               pbyImageBuffer和pFrInfo是由CameraGetImageBuffer
//               获得的数据，而且未经CameraImageProcess转换
//               成BMP格式；反之，如果要保存成BMP、JPG或者
//               PNG格式，则pbyImageBuffer是由
//               CameraImageProcess处理后的RGB格式数据。
//                 具体用法可以参考Advanced的例程。
//            byQuality      图像保存的质量因子，仅当保存为JPG格式
//                 时该参数有效，范围1到100。其余格式
//                           可以写成0。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraSaveImageEx(
	CameraHandle    hCamera,
	char*           lpszFileName,
	BYTE*           pbyImageBuffer,
	UINT			uImageFormat,
	int				iWidth,
	int				iHeight,
	UINT            byFileType,
	BYTE            byQuality
	);

/******************************************************/
// 函数名   : CameraGetImageResolution
// 功能描述 : 获得当前预览的分辨率。
// 参数     : hCamera    相机的句柄，由CameraInit函数获得。
//            psCurVideoSize 结构体指针，用于返回当前的分辨率。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraGetImageResolution(
    CameraHandle            hCamera,
    tSdkImageResolution*    psCurVideoSize
);

/******************************************************/
// 函数名   : CameraGetImageResolutionEx
// 功能描述 : 获取相机的分辨率。
// 参数     : hCamera      相机的句柄，由CameraInit函数获得。
//            iIndex	   索引号，[0,N]表示预设的分辨率(N 为预设分辨率的最大个数，一般不超过20),OXFF 表示自定义分辨率(ROI)
//			  acDescription 该分辨率的描述信息。仅预设分辨率时该信息有效。自定义分辨率可忽略该信息
//			  Mode		   0: 普通模式     1：Sum       2：Average        3：Skip        4：Resample
//			  ModeSize	   普通模式下忽略，第1位表示2X2 第二位表示3X3 ...
//			  x, y		   水平、垂直偏移
//			  width, height 宽高
//			  ZoomWidth,ZoomHeight 最终输出时缩放为多大，0表示不缩放
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraGetImageResolutionEx(
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

/******************************************************/
// 函数名   : CameraSetImageResolution
// 功能描述 : 设置预览的分辨率。
// 参数     : hCamera      相机的句柄，由CameraInit函数获得。
//            pImageResolution 结构体指针，用于返回当前的分辨率。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraSetImageResolution(
    CameraHandle            hCamera,
    tSdkImageResolution*    pImageResolution
);

/******************************************************/
// 函数名   : CameraSetImageResolutionEx
// 功能描述 : 设置相机的分辨率。
// 参数     : hCamera      相机的句柄，由CameraInit函数获得。
//            iIndex	   索引号，[0,N]表示预设的分辨率(N 为预设分辨率的最大个数，一般不超过20),OXFF 表示自定义分辨率(ROI)
//			  Mode		   0: 普通模式     1：Sum       2：Average        3：Skip        4：Resample
//			  ModeSize	   普通模式下忽略，第1位表示2X2 第二位表示3X3 ...
//			  x, y		   水平、垂直偏移
//			  width, height 宽高
//			  ZoomWidth,ZoomHeight 最终输出时缩放为多大，0表示不缩放
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraSetImageResolutionEx(
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

/******************************************************/
// 函数名   : CameraGetMediaType
// 功能描述 : 获得相机当前输出原始数据的格式索引号。
// 参数     : hCamera   相机的句柄，由CameraInit函数获得。
//            piMediaType   指针，用于返回当前格式类型的索引号。
//              由CameraGetCapability获得相机的属性，
//              在tSdkCameraCapbility结构体中的pMediaTypeDesc
//              成员中，以数组的形式保存了相机支持的格式，
//              piMediaType所指向的索引号，就是该数组的索引号。
//              pMediaTypeDesc[*piMediaType].iMediaType则表示当前格式的
//              编码。该编码请参见CameraDefine.h中[图像格式定义]部分。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraGetMediaType(
    CameraHandle    hCamera,
    INT*            piMediaType
);

/******************************************************/
// 函数名   : CameraSetMediaType
// 功能描述 : 设置相机的输出原始数据格式。
// 参数     : hCamera   相机的句柄，由CameraInit函数获得。
//            iMediaType  由CameraGetCapability获得相机的属性，
//              在tSdkCameraCapbility结构体中的pMediaTypeDesc
//              成员中，以数组的形式保存了相机支持的格式，
//              iMediaType就是该数组的索引号。
//              pMediaTypeDesc[iMediaType].iMediaType则表示当前格式的
//              编码。该编码请参见CameraDefine.h中[图像格式定义]部分。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraSetMediaType(
    CameraHandle    hCamera,
    INT             iMediaType
);

/******************************************************/
// 函数名   : CameraSetAeState
// 功能描述 : 设置相机曝光的模式。自动或者手动。
// 参数     : hCamera   相机的句柄，由CameraInit函数获得。
//            bAeState    TRUE，使能自动曝光；FALSE，停止自动曝光。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraSetAeState(
    CameraHandle    hCamera,
    BOOL            bAeState
);

/******************************************************/
// 函数名   : CameraGetAeState
// 功能描述 : 获得相机当前的曝光模式。
// 参数     : hCamera  相机的句柄，由CameraInit函数获得。
//            pAeState   指针，用于返回自动曝光的使能状态。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraGetAeState(
    CameraHandle    hCamera,
    BOOL*           pAeState
);

/******************************************************/
// 函数名   : CameraSetSharpness
// 功能描述 : 设置图像的处理的锐化参数。
// 参数     : hCamera  相机的句柄，由CameraInit函数获得。
//            iSharpness 锐化参数。范围由CameraGetCapability
//               获得，一般是[0,100]，0表示关闭锐化处理。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraSetSharpness(
    CameraHandle    hCamera,
    int             iSharpness
);

/******************************************************/
// 函数名   : CameraGetSharpness
// 功能描述 : 获取当前锐化设定值。
// 参数     : hCamera   相机的句柄，由CameraInit函数获得。
//            piSharpness 指针，返回当前设定的锐化的设定值。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraGetSharpness(
    CameraHandle    hCamera,
    int*            piSharpness
);

/******************************************************/
// 函数名   : CameraSetLutMode
// 功能描述 : 设置相机的查表变换模式LUT模式。
// 参数     : hCamera  相机的句柄，由CameraInit函数获得。
//            emLutMode  LUTMODE_PARAM_GEN 表示由伽马和对比度参数动态生成LUT表。
//             LUTMODE_PRESET    表示使用预设的LUT表。
//             LUTMODE_USER_DEF  表示使用用户自定的LUT表。
//             LUTMODE_PARAM_GEN的定义参考CameraDefine.h中emSdkLutMode类型。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraSetLutMode(
    CameraHandle    hCamera,
    int             emLutMode
);

/******************************************************/
// 函数名   : CameraGetLutMode
// 功能描述 : 获得相机的查表变换模式LUT模式。
// 参数     : hCamera  相机的句柄，由CameraInit函数获得。
//            pemLutMode 指针，返回当前LUT模式。意义与CameraSetLutMode
//             中emLutMode参数相同。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraGetLutMode(
    CameraHandle    hCamera,
    int*            pemLutMode
);

/******************************************************/
// 函数名   : CameraSelectLutPreset
// 功能描述 : 选择预设LUT模式下的LUT表。必须先使用CameraSetLutMode
//        将LUT模式设置为预设模式。
// 参数     : hCamera  相机的句柄，由CameraInit函数获得。
//            iSel     表的索引号。表的个数由CameraGetCapability
//             获得。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraSelectLutPreset(
    CameraHandle    hCamera,
    int             iSel
);

/******************************************************/
// 函数名   : CameraGetLutPresetSel
// 功能描述 : 获得预设LUT模式下的LUT表索引号。
// 参数     : hCamera  相机的句柄，由CameraInit函数获得。
//            piSel      指针，返回表的索引号。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraGetLutPresetSel(
    CameraHandle    hCamera,
    int*            piSel
);

/******************************************************/
// 函数名   : CameraSetCustomLut
// 功能描述 : 设置自定义的LUT表。必须先使用CameraSetLutMode
//        将LUT模式设置为自定义模式。
// 参数     : hCamera  相机的句柄，由CameraInit函数获得。
//             iChannel 指定要设定的LUT颜色通道，当为LUT_CHANNEL_ALL时，
//                      三个通道的LUT将被同时替换。
//                      参考CameraDefine.h中emSdkLutChannel定义。
//            pLut     指针，指向LUT表的地址。LUT表为无符号短整形数组，数组大小为
//           4096，分别代码颜色通道从0到4096(12bit颜色精度)对应的映射值。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraSetCustomLut(
    CameraHandle    hCamera,
    int       iChannel,
    USHORT*         pLut
);

/******************************************************/
// 函数名   : CameraGetCustomLut
// 功能描述 : 获得当前使用的自定义LUT表。
// 参数     : hCamera  相机的句柄，由CameraInit函数获得。
//             iChannel 指定要获得的LUT颜色通道。当为LUT_CHANNEL_ALL时，
//                      返回红色通道的LUT表。
//                      参考CameraDefine.h中emSdkLutChannel定义。
//            pLut       指针，指向LUT表的地址。LUT表为无符号短整形数组，数组大小为
//           4096，分别代码颜色通道从0到4096(12bit颜色精度)对应的映射值。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraGetCustomLut(
    CameraHandle    hCamera,
    int       iChannel,
    USHORT*         pLut
);

/******************************************************/
// 函数名   : CameraGetCurrentLut
// 功能描述 : 获得相机当前的LUT表，在任何LUT模式下都可以调用,
//        用来直观的观察LUT曲线的变化。
// 参数     : hCamera  相机的句柄，由CameraInit函数获得。
//             iChannel 指定要获得的LUT颜色通道。当为LUT_CHANNEL_ALL时，
//                      返回红色通道的LUT表。
//                      参考CameraDefine.h中emSdkLutChannel定义。
//            pLut       指针，指向LUT表的地址。LUT表为无符号短整形数组，数组大小为
//           4096，分别代码颜色通道从0到4096(12bit颜色精度)对应的映射值。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraGetCurrentLut(
    CameraHandle    hCamera,
    int       iChannel,
    USHORT*         pLut
);

/******************************************************/
// 函数名   : CameraSetWbMode
// 功能描述 : 设置相机白平衡模式。分为手动和自动两种方式。
// 参数     : hCamera  相机的句柄，由CameraInit函数获得。
//            bAuto      TRUE，则表示使能自动模式。
//             FALSE，则表示使用手动模式，通过调用
//                 CameraSetOnceWB来进行一次白平衡。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraSetWbMode(
    CameraHandle    hCamera,
    BOOL            bAuto
);

/******************************************************/
// 函数名   : CameraGetWbMode
// 功能描述 : 获得当前的白平衡模式。
// 参数     : hCamera  相机的句柄，由CameraInit函数获得。
//            pbAuto   指针，返回TRUE表示自动模式，FALSE
//             为手动模式。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraGetWbMode(
    CameraHandle    hCamera,
    BOOL*           pbAuto
);

/******************************************************/
// 函数名   : CameraSetPresetClrTemp
// 功能描述 : 选择指定预设色温模式
// 参数     : hCamera  相机的句柄，由CameraInit函数获得。
//            iSel 预设色温的模式索引号，从0开始
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraSetPresetClrTemp(
    CameraHandle    hCamera,
    int             iSel
);

/******************************************************/
// 函数名   : CameraGetPresetClrTemp
// 功能描述 : 获得当前选择的预设色温模式。
// 参数     : hCamera  相机的句柄，由CameraInit函数获得。
//            piSel  指针，返回选择的预设色温索引号
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraGetPresetClrTemp(
    CameraHandle    hCamera,
    int*            piSel
);

/******************************************************/
// 函数名   : CameraSetUserClrTempGain
// 功能描述 : 设置自定义色温模式下的数字增益
// 参数     : hCamera  相机的句柄，由CameraInit函数获得。
//            iRgain  红色增益，范围0到400，表示0到4倍
//            iGgain  绿色增益，范围0到400，表示0到4倍
//            iBgain  蓝色增益，范围0到400，表示0到4倍
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraSetUserClrTempGain(
  CameraHandle  hCamera,
  int       iRgain,
  int       iGgain,
  int       iBgain
);


/******************************************************/
// 函数名   : CameraGetUserClrTempGain
// 功能描述 : 获得自定义色温模式下的数字增益
// 参数     : hCamera  相机的句柄，由CameraInit函数获得。
//            piRgain  指针，返回红色增益，范围0到400，表示0到4倍
//            piGgain  指针，返回绿色增益，范围0到400，表示0到4倍
//            piBgain  指针，返回蓝色增益，范围0到400，表示0到4倍
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraGetUserClrTempGain(
  CameraHandle  hCamera,
  int*      piRgain,
  int*      piGgain,
  int*      piBgain
);

/******************************************************/
// 函数名   : CameraSetUserClrTempMatrix
// 功能描述 : 设置自定义色温模式下的颜色矩阵
// 参数     : hCamera  相机的句柄，由CameraInit函数获得。
//            pMatrix 指向一个float[3][3]数组的首地址
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraSetUserClrTempMatrix(
  CameraHandle  hCamera,
  float*      pMatrix
);


/******************************************************/
// 函数名   : CameraGetUserClrTempMatrix
// 功能描述 : 获得自定义色温模式下的颜色矩阵
// 参数     : hCamera  相机的句柄，由CameraInit函数获得。
//            pMatrix 指向一个float[3][3]数组的首地址
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraGetUserClrTempMatrix(
  CameraHandle  hCamera,
  float*      pMatrix
);

/******************************************************/
// 函数名   : CameraSetClrTempMode
// 功能描述 : 设置白平衡时使用的色温模式，
//              支持的模式有三种，分别是自动，预设和自定义。
//              自动模式下，会自动选择合适的色温模式
//              预设模式下，会使用用户指定的色温模式
//              自定义模式下，使用用户自定义的色温数字增益和矩阵
// 参数     : hCamera 相机的句柄，由CameraInit函数获得。
//            iMode 模式，只能是emSdkClrTmpMode中定义的一种
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraSetClrTempMode(
  CameraHandle  hCamera,
  int       iMode
);

/******************************************************/
// 函数名   : CameraGetClrTempMode
// 功能描述 : 获得白平衡时使用的色温模式。参考CameraSetClrTempMode
//              中功能描述部分。
// 参数     : hCamera 相机的句柄，由CameraInit函数获得。
//            pimode 指针，返回模式选择，参考emSdkClrTmpMode类型定义
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraGetClrTempMode(
  CameraHandle  hCamera,
  int*      pimode
);



/******************************************************/
// 函数名   : CameraSetOnceWB
// 功能描述 : 在手动白平衡模式下，调用该函数会进行一次白平衡。
//        生效的时间为接收到下一帧图像数据时。
// 参数     : hCamera  相机的句柄，由CameraInit函数获得。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraSetOnceWB(
    CameraHandle    hCamera
);

/******************************************************/
// 函数名   : CameraSetOnceBB
// 功能描述 : 执行一次黑平衡操作。
// 参数     : hCamera  相机的句柄，由CameraInit函数获得。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraSetOnceBB(
    CameraHandle    hCamera
);


/******************************************************/
// 函数名   : CameraSetAeTarget
// 功能描述 : 设定自动曝光的亮度目标值。设定范围由CameraGetCapability
//        函数获得。
// 参数     : hCamera  相机的句柄，由CameraInit函数获得。
//            iAeTarget  亮度目标值。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraSetAeTarget(
    CameraHandle    hCamera,
    int             iAeTarget
);

/******************************************************/
// 函数名   : CameraGetAeTarget
// 功能描述 : 获得自动曝光的亮度目标值。
// 参数     : hCamera   相机的句柄，由CameraInit函数获得。
//            *piAeTarget 指针，返回目标值。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraGetAeTarget(
    CameraHandle    hCamera,
    int*            piAeTarget
);

/******************************************************/
// 函数名   : CameraSetAeExposureRange
// 功能描述 : 设定自动曝光模式的曝光时间调节范围
// 参数     : hCamera  相机的句柄，由CameraInit函数获得。
//           fMinExposureTime 最小曝光时间（微秒）
//			 fMaxExposureTime 最大曝光时间（微秒）
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraSetAeExposureRange(
	CameraHandle    hCamera,
	double          fMinExposureTime,
	double			fMaxExposureTime
	);

/******************************************************/
// 函数名   : CameraGetAeExposureRange
// 功能描述 : 获得自动曝光模式的曝光时间调节范围
// 参数     : hCamera   相机的句柄，由CameraInit函数获得。
//           fMinExposureTime 最小曝光时间（微秒）
//			 fMaxExposureTime 最大曝光时间（微秒）
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraGetAeExposureRange(
	CameraHandle    hCamera,
	double*         fMinExposureTime,
	double*			fMaxExposureTime
	);

/******************************************************/
// 函数名   : CameraSetAeAnalogGainRange
// 功能描述 : 设定自动曝光模式的增益调节范围
// 参数     : hCamera  相机的句柄，由CameraInit函数获得。
//           iMinAnalogGain 最小增益
//			 iMaxAnalogGain 最大增益
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraSetAeAnalogGainRange(
	CameraHandle    hCamera,
	int				iMinAnalogGain,
	int				iMaxAnalogGain
	);

/******************************************************/
// 函数名   : CameraGetAeAnalogGainRange
// 功能描述 : 获得自动曝光模式的增益调节范围
// 参数     : hCamera   相机的句柄，由CameraInit函数获得。
//           iMinAnalogGain 最小增益
//			 iMaxAnalogGain 最大增益
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraGetAeAnalogGainRange(
	CameraHandle    hCamera,
	int*			iMinAnalogGain,
	int*			iMaxAnalogGain
	);

/******************************************************/
// 函数名   : CameraSetAeThreshold
// 功能描述 : 设置自动曝光模式的调节阈值
// 参数     : hCamera   相机的句柄，由CameraInit函数获得。
//           iThreshold   如果 abs(目标亮度-图像亮度) < iThreshold 则停止自动调节
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraSetAeThreshold(
	CameraHandle    hCamera,
	int				iThreshold
	);

/******************************************************/
// 函数名   : CameraGetAeThreshold
// 功能描述 : 获取自动曝光模式的调节阈值
// 参数     : hCamera   相机的句柄，由CameraInit函数获得。
//           iThreshold   读取到的调节阈值
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraGetAeThreshold(
	CameraHandle    hCamera,
	int*			iThreshold
	);

/******************************************************/
// 函数名   : CameraSetExposureTime
// 功能描述 : 设置曝光时间。单位为微秒。对于CMOS传感器，其曝光
//        的单位是按照行来计算的，因此，曝光时间并不能在微秒
//        级别连续可调。而是会按照整行来取舍。在调用
//        本函数设定曝光时间后，建议再调用CameraGetExposureTime
//        来获得实际设定的值。
// 参数     : hCamera      相机的句柄，由CameraInit函数获得。
//            fExposureTime 曝光时间，单位微秒。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/

MVSDK_API CameraSdkStatus __stdcall CameraSetExposureTime(
    CameraHandle    hCamera,
    double          fExposureTime
);

/******************************************************/
// 函数名   : CameraGetExposureLineTime
// 功能描述 : 获得一行的曝光时间。对于CMOS传感器，其曝光
//        的单位是按照行来计算的，因此，曝光时间并不能在微秒
//        级别连续可调。而是会按照整行来取舍。这个函数的
//          作用就是返回CMOS相机曝光一行对应的时间。
// 参数     : hCamera  相机的句柄，由CameraInit函数获得。
//            pfLineTime 指针，返回一行的曝光时间，单位为微秒。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/

MVSDK_API CameraSdkStatus __stdcall CameraGetExposureLineTime(
    CameraHandle    hCamera,
    double*         pfLineTime
);

/******************************************************/
// 函数名   : CameraGetExposureTime
// 功能描述 : 获得相机的曝光时间。请参见CameraSetExposureTime
//        的功能描述。
// 参数     : hCamera        相机的句柄，由CameraInit函数获得。
//            pfExposureTime   指针，返回当前的曝光时间，单位微秒。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraGetExposureTime(
    CameraHandle    hCamera,
    double*         pfExposureTime
);

/******************************************************/
// 函数名   : CameraGetExposureTimeRange
// 功能描述 : 获得相机的曝光时间范围
// 参数     : hCamera        相机的句柄，由CameraInit函数获得。
//            pfMin			指针，返回曝光时间的最小值，单位微秒。
//            pfMax			指针，返回曝光时间的最大值，单位微秒。
//            pfStep		指针，返回曝光时间的步进值，单位微秒。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraGetExposureTimeRange(
	CameraHandle    hCamera,
	double*         pfMin,
	double*			pfMax,
	double*			pfStep
	);

/******************************************************/
// 函数名   : CameraSetAnalogGain
// 功能描述 : 设置相机的图像模拟增益值。该值乘以CameraGetCapability获得
//        的相机属性结构体中sExposeDesc.fAnalogGainStep，就
//        得到实际的图像信号放大倍数。
// 参数     : hCamera   相机的句柄，由CameraInit函数获得。
//            iAnalogGain 设定的模拟增益值。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraSetAnalogGain(
    CameraHandle    hCamera,
    INT             iAnalogGain
);

/******************************************************/
// 函数名   : CameraGetAnalogGain
// 功能描述 : 获得图像信号的模拟增益值。参见CameraSetAnalogGain
//        详细说明。
// 参数     : hCamera    相机的句柄，由CameraInit函数获得。
//            piAnalogGain 指针，返回当前的模拟增益值。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraGetAnalogGain(
    CameraHandle    hCamera,
    INT*            piAnalogGain
);

/******************************************************/
// 函数名   : CameraSetGain
// 功能描述 : 设置图像的数字增益。设定范围由CameraGetCapability
//        获得的相机属性结构体中sRgbGainRange成员表述。
//        实际的放大倍数是设定值/100。
// 参数     : hCamera  相机的句柄，由CameraInit函数获得。
//            iRGain   红色通道的增益值。
//            iGGain   绿色通道的增益值。
//            iBGain   蓝色通道的增益值。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraSetGain(
    CameraHandle    hCamera,
    int             iRGain,
    int             iGGain,
    int             iBGain
);


/******************************************************/
// 函数名   : CameraGetGain
// 功能描述 : 获得图像处理的数字增益。具体请参见CameraSetGain
//        的功能描述部分。
// 参数     : hCamera  相机的句柄，由CameraInit函数获得。
//            piRGain  指针，返回红色通道的数字增益值。
//            piGGain    指针，返回绿色通道的数字增益值。
//            piBGain    指针，返回蓝色通道的数字增益值。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraGetGain(
    CameraHandle    hCamera,
    int*            piRGain,
    int*            piGGain,
    int*            piBGain
);


/******************************************************/
// 函数名   : CameraSetGamma
// 功能描述 : 设定LUT动态生成模式下的Gamma值。设定的值会
//        马上保存在SDK内部，但是只有当相机处于动态
//        参数生成的LUT模式时，才会生效。请参考CameraSetLutMode
//        的函数说明部分。
// 参数     : hCamera  相机的句柄，由CameraInit函数获得。
//            iGamma     要设定的Gamma值。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraSetGamma(
    CameraHandle    hCamera,
    int             iGamma
);

/******************************************************/
// 函数名   : CameraGetGamma
// 功能描述 : 获得LUT动态生成模式下的Gamma值。请参考CameraSetGamma
//        函数的功能描述。
// 参数     : hCamera  相机的句柄，由CameraInit函数获得。
//            piGamma    指针，返回当前的Gamma值。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraGetGamma(
    CameraHandle    hCamera,
    int*            piGamma
);

/******************************************************/
// 函数名   : CameraSetContrast
// 功能描述 : 设定LUT动态生成模式下的对比度值。设定的值会
//        马上保存在SDK内部，但是只有当相机处于动态
//        参数生成的LUT模式时，才会生效。请参考CameraSetLutMode
//        的函数说明部分。
// 参数     : hCamera  相机的句柄，由CameraInit函数获得。
//            iContrast  设定的对比度值。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraSetContrast(
    CameraHandle    hCamera,
    int             iContrast
);

/******************************************************/
// 函数名   : CameraGetContrast
// 功能描述 : 获得LUT动态生成模式下的对比度值。请参考
//        CameraSetContrast函数的功能描述。
// 参数     : hCamera  相机的句柄，由CameraInit函数获得。
//            piContrast 指针，返回当前的对比度值。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraGetContrast(
    CameraHandle    hCamera,
    int*            piContrast
);

/******************************************************/
// 函数名   : CameraSetSaturation
// 功能描述 : 设定图像处理的饱和度。对黑白相机无效。
//        设定范围由CameraGetCapability获得。100表示
//        表示原始色度，不增强。
// 参数     : hCamera    相机的句柄，由CameraInit函数获得。
//            iSaturation  设定的饱和度值。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraSetSaturation(
    CameraHandle    hCamera,
    int             iSaturation
);

/******************************************************/
// 函数名   : CameraGetSaturation
// 功能描述 : 获得图像处理的饱和度。
// 参数     : hCamera    相机的句柄，由CameraInit函数获得。
//            piSaturation 指针，返回当前图像处理的饱和度值。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraGetSaturation(
    CameraHandle    hCamera,
    int*            piSaturation
);

/******************************************************/
// 函数名   : CameraSetMonochrome
// 功能描述 : 设置彩色转为黑白功能的使能。
// 参数     : hCamera 相机的句柄，由CameraInit函数获得。
//            bEnable   TRUE，表示将彩色图像转为黑白。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraSetMonochrome(
    CameraHandle    hCamera,
    BOOL            bEnable
);

/******************************************************/
// 函数名   : CameraGetMonochrome
// 功能描述 : 获得彩色转换黑白功能的使能状况。
// 参数     : hCamera  相机的句柄，由CameraInit函数获得。
//            pbEnable   指针。返回TRUE表示开启了彩色图像
//             转换为黑白图像的功能。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraGetMonochrome(
    CameraHandle    hCamera,
    BOOL*           pbEnable
);

/******************************************************/
// 函数名   : CameraSetInverse
// 功能描述 : 设置彩图像颜色翻转功能的使能。
// 参数     : hCamera  相机的句柄，由CameraInit函数获得。
//            bEnable    TRUE，表示开启图像颜色翻转功能，
//             可以获得类似胶卷底片的效果。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraSetInverse(
    CameraHandle    hCamera,
    BOOL            bEnable
);

/******************************************************/
// 函数名   : CameraGetInverse
// 功能描述 : 获得图像颜色反转功能的使能状态。
// 参数     : hCamera  相机的句柄，由CameraInit函数获得。
//            pbEnable   指针，返回该功能使能状态。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraGetInverse(
    CameraHandle    hCamera,
    BOOL*           pbEnable
);

/******************************************************/
// 函数名   : CameraSetAntiFlick
// 功能描述 : 设置自动曝光时抗频闪功能的使能状态。对于手动
//        曝光模式下无效。
// 参数     : hCamera  相机的句柄，由CameraInit函数获得。
//            bEnable    TRUE，开启抗频闪功能;FALSE，关闭该功能。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraSetAntiFlick(
    CameraHandle    hCamera,
    BOOL            bEnable
);

/******************************************************/
// 函数名   : CameraGetAntiFlick
// 功能描述 : 获得自动曝光时抗频闪功能的使能状态。
// 参数     : hCamera  相机的句柄，由CameraInit函数获得。
//            pbEnable   指针，返回该功能的使能状态。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraGetAntiFlick(
    CameraHandle    hCamera,
    BOOL*           pbEnable
);

/******************************************************/
// 函数名   : CameraGetLightFrequency
// 功能描述 : 获得自动曝光时，消频闪的频率选择。
// 参数     : hCamera      相机的句柄，由CameraInit函数获得。
//            piFrequencySel 指针，返回选择的索引号。0:50HZ 1:60HZ
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraGetLightFrequency(
    CameraHandle    hCamera,
    int*            piFrequencySel
);

/******************************************************/
// 函数名   : CameraSetLightFrequency
// 功能描述 : 设置自动曝光时消频闪的频率。
// 参数     : hCamera     相机的句柄，由CameraInit函数获得。
//            iFrequencySel 0:50HZ , 1:60HZ
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraSetLightFrequency(
    CameraHandle    hCamera,
    int             iFrequencySel
);

/******************************************************/
// 函数名   : CameraSetFrameSpeed
// 功能描述 : 设定相机输出图像的帧率。相机可供选择的帧率模式由
//        CameraGetCapability获得的信息结构体中iFrameSpeedDesc
//        表示最大帧率选择模式个数。
// 参数     : hCamera   相机的句柄，由CameraInit函数获得。
//            iFrameSpeed 选择的帧率模式索引号，范围从0到
//              CameraGetCapability获得的信息结构体中iFrameSpeedDesc - 1
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraSetFrameSpeed(
    CameraHandle    hCamera,
    int             iFrameSpeed
);

/******************************************************/
// 函数名   : CameraGetFrameSpeed
// 功能描述 : 获得相机输出图像的帧率选择索引号。具体用法参考
//        CameraSetFrameSpeed函数的功能描述部分。
// 参数     : hCamera    相机的句柄，由CameraInit函数获得。
//            piFrameSpeed 指针，返回选择的帧率模式索引号。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraGetFrameSpeed(
    CameraHandle    hCamera,
    int*            piFrameSpeed
);


/******************************************************/
// 函数名   : CameraSetParameterMode
// 功能描述 : 设定参数存取的目标对象。
// 参数     : hCamera  相机的句柄，由CameraInit函数获得。
//            iMode  参数存取的对象。参考CameraDefine.h
//          中emSdkParameterMode的类型定义。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraSetParameterMode(
    CameraHandle    hCamera,
    int             iMode
);

/******************************************************/
// 函数名   : CameraGetParameterMode
// 功能描述 :
// 参数     : hCamera  相机的句柄，由CameraInit函数获得。
//            int* piTarget
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraGetParameterMode(
    CameraHandle    hCamera,
    int*            piTarget
);

/******************************************************/
// 函数名   : CameraSetParameterMask
// 功能描述 : 设置参数存取的掩码。参数加载和保存时会根据该
//        掩码来决定各个模块参数的是否加载或者保存。
// 参数     : hCamera 相机的句柄，由CameraInit函数获得。
//            uMask     掩码。参考CameraDefine.h中PROP_SHEET_INDEX
//            类型定义。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraSetParameterMask(
    CameraHandle    hCamera,
    UINT            uMask
);

/******************************************************/
// 函数名   : CameraSaveParameter
// 功能描述 : 保存当前相机参数到指定的参数组中。相机提供了A,B,C,D
//        A,B,C,D四组空间来进行参数的保存。
// 参数     : hCamera  相机的句柄，由CameraInit函数获得。
//            iTeam      PARAMETER_TEAM_A 保存到A组中,
//             PARAMETER_TEAM_B 保存到B组中,
//             PARAMETER_TEAM_C 保存到C组中,
//             PARAMETER_TEAM_D 保存到D组中
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraSaveParameter(
    CameraHandle    hCamera,
    int             iTeam
);


/******************************************************/
// 函数名   : CameraSaveParameterToFile
// 功能描述 : 保存当前相机参数到指定的文件中。该文件可以复制到
//        别的电脑上供其他相机加载，也可以做参数备份用。
// 参数     : hCamera  相机的句柄，由CameraInit函数获得。
//            sFileName  参数文件的完整路径。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraSaveParameterToFile(
  CameraHandle  hCamera,
  char*       sFileName
);


/******************************************************/
// 函数名   : CameraReadParameterFromFile
// 功能描述 : 从PC上指定的参数文件中加载参数。我公司相机参数
//        保存在PC上为.config后缀的文件，位于安装下的
//        Camera\Configs文件夹中。
// 参数     : hCamera  相机的句柄，由CameraInit函数获得。
//            *sFileName 参数文件的完整路径。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraReadParameterFromFile(
    CameraHandle    hCamera,
    char*           sFileName
);

/******************************************************/
// 函数名   : CameraLoadParameter
// 功能描述 : 加载指定组的参数到相机中。
// 参数     : hCamera  相机的句柄，由CameraInit函数获得。
//            iTeam    PARAMETER_TEAM_A 加载A组参数,
//             PARAMETER_TEAM_B 加载B组参数,
//             PARAMETER_TEAM_C 加载C组参数,
//             PARAMETER_TEAM_D 加载D组参数,
//             PARAMETER_TEAM_DEFAULT 加载默认参数。
//             类型定义参考CameraDefine.h中emSdkParameterTeam类型
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraLoadParameter(
    CameraHandle    hCamera,
    int             iTeam
);

/******************************************************/
// 函数名   : CameraGetCurrentParameterGroup
// 功能描述 : 获得当前选择的参数组。
// 参数     : hCamera  相机的句柄，由CameraInit函数获得。
//            piTeam     指针，返回当前选择的参数组。返回值
//             参考CameraLoadParameter中iTeam参数。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraGetCurrentParameterGroup(
    CameraHandle    hCamera,
    int*            piTeam
);

/******************************************************/
// 函数名   : CameraSetTransPackLen
// 功能描述 : 设置相机传输图像数据的分包大小。
//        目前的SDK版本中，该接口仅对GIGE接口相机有效，
//        用来控制网络传输的分包大小。对于支持巨帧的网卡，
//        我们建议选择8K的分包大小，可以有效的降低传输
//        所占用的CPU处理时间。
// 参数     : hCamera  相机的句柄，由CameraInit函数获得。
//            iPackSel   分包长度选择的索引号。分包长度可由
//             获得相机属性结构体中pPackLenDesc成员表述，
//             iPackLenDesc成员则表示最大可选的分包模式个数。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraSetTransPackLen(
    CameraHandle    hCamera,
    INT             iPackSel
);

/******************************************************/
// 函数名   : CameraGetTransPackLen
// 功能描述 : 获得相机当前传输分包大小的选择索引号。
// 参数     : hCamera  相机的句柄，由CameraInit函数获得。
//            piPackSel  指针，返回当前选择的分包大小索引号。
//             参见CameraSetTransPackLen中iPackSel的
//             说明。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraGetTransPackLen(
    CameraHandle    hCamera,
    INT*            piPackSel
);

/******************************************************/
// 函数名   : CameraIsAeWinVisible
// 功能描述 : 获得自动曝光参考窗口的显示状态。
// 参数     : hCamera    相机的句柄，由CameraInit函数获得。
//            pbIsVisible  指针，返回TRUE，则表示当前窗口会
//               被叠加在图像内容上。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraIsAeWinVisible(
    CameraHandle    hCamera,
    BOOL*           pbIsVisible
);

/******************************************************/
// 函数名   : CameraSetAeWinVisible
// 功能描述 : 设置自动曝光参考窗口的显示状态。当设置窗口状态
//        为显示，调用CameraImageOverlay后，能够将窗口位置
//        以矩形的方式叠加在图像上。
// 参数     : hCamera   相机的句柄，由CameraInit函数获得。
//            bIsVisible  TRUE，设置为显示；FALSE，不显示。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraSetAeWinVisible(
    CameraHandle    hCamera,
    BOOL            bIsVisible
);

/******************************************************/
// 函数名   : CameraGetAeWindow
// 功能描述 : 获得自动曝光参考窗口的位置。
// 参数     : hCamera  相机的句柄，由CameraInit函数获得。
//            piHOff     指针，返回窗口位置左上角横坐标值。
//            piVOff     指针，返回窗口位置左上角纵坐标值。
//            piWidth    指针，返回窗口的宽度。
//            piHeight   指针，返回窗口的高度。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraGetAeWindow(
    CameraHandle    hCamera,
    INT*            piHOff,
    INT*            piVOff,
    INT*            piWidth,
    INT*            piHeight
);

/******************************************************/
// 函数名   : CameraSetAeWindow
// 功能描述 : 设置自动曝光的参考窗口。
// 参数     : hCamera  相机的句柄，由CameraInit函数获得。
//            iHOff    窗口左上角的横坐标
//            iVOff      窗口左上角的纵坐标
//            iWidth     窗口的宽度
//            iHeight    窗口的高度
//        如果iHOff、iVOff、iWidth、iHeight全部为0，则
//        窗口设置为每个分辨率下的居中1/2大小。可以随着
//        分辨率的变化而跟随变化；如果iHOff、iVOff、iWidth、iHeight
//        所决定的窗口位置范围超出了当前分辨率范围内，
//          则自动使用居中1/2大小窗口。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraSetAeWindow(
    CameraHandle    hCamera,
    int             iHOff,
    int             iVOff,
    int             iWidth,
    int             iHeight
);

/******************************************************/
// 函数名   : CameraSetMirror
// 功能描述 : 设置图像镜像操作。镜像操作分为水平和垂直两个方向。
// 参数     : hCamera  相机的句柄，由CameraInit函数获得。
//            iDir     表示镜像的方向。0，表示水平方向；1，表示垂直方向。
//            bEnable  TRUE，使能镜像;FALSE，禁止镜像
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraSetMirror(
    CameraHandle    hCamera,
    int             iDir,
    BOOL            bEnable
);

/******************************************************/
// 函数名   : CameraGetMirror
// 功能描述 : 获得图像的镜像状态。
// 参数     : hCamera  相机的句柄，由CameraInit函数获得。
//            iDir     表示要获得的镜像方向。
//             0，表示水平方向；1，表示垂直方向。
//            pbEnable   指针，返回TRUE，则表示iDir所指的方向
//             镜像被使能。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraGetMirror(
    CameraHandle    hCamera,
    int             iDir,
    BOOL*           pbEnable
);

/******************************************************/
// 函数名   : CameraSetRotate
// 功能描述 : 设置图像旋转操作
// 参数     : hCamera  相机的句柄，由CameraInit函数获得。
//            iRot     表示旋转的角度（逆时针方向）（0：不旋转 1:90度 2:180度 3:270度）
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraSetRotate(
	CameraHandle    hCamera,
	int             iRot
	);

/******************************************************/
// 函数名   : CameraGetRotate
// 功能描述 : 获得图像的旋转状态。
// 参数     : hCamera  相机的句柄，由CameraInit函数获得。
//            iRot     表示要获得的旋转方向。
//               （逆时针方向）（0：不旋转 1:90度 2:180度 3:270度）
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraGetRotate(
	CameraHandle    hCamera,
	int*            iRot
	);

/******************************************************/
// 函数名   : CameraGetWbWindow
// 功能描述 : 获得白平衡参考窗口的位置。
// 参数     : hCamera  相机的句柄，由CameraInit函数获得。
//            PiHOff   指针，返回参考窗口的左上角横坐标 。
//            PiVOff     指针，返回参考窗口的左上角纵坐标 。
//            PiWidth    指针，返回参考窗口的宽度。
//            PiHeight   指针，返回参考窗口的高度。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraGetWbWindow(
    CameraHandle    hCamera,
    INT*            PiHOff,
    INT*            PiVOff,
    INT*            PiWidth,
    INT*            PiHeight
);

/******************************************************/
// 函数名   : CameraSetWbWindow
// 功能描述 : 设置白平衡参考窗口的位置。
// 参数     : hCamera 相机的句柄，由CameraInit函数获得。
//            iHOff   参考窗口的左上角横坐标。
//            iVOff     参考窗口的左上角纵坐标。
//            iWidth    参考窗口的宽度。
//            iHeight   参考窗口的高度。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraSetWbWindow(
    CameraHandle    hCamera,
    INT             iHOff,
    INT             iVOff,
    INT             iWidth,
    INT             iHeight
);

/******************************************************/
// 函数名   : CameraIsWbWinVisible
// 功能描述 : 获得白平衡窗口的显示状态。
// 参数     : hCamera  相机的句柄，由CameraInit函数获得。
//            pbShow   指针，返回TRUE，则表示窗口是可见的。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraIsWbWinVisible(
    CameraHandle    hCamera,
    BOOL*           pbShow
);

/******************************************************/
// 函数名   : CameraSetWbWinVisible
// 功能描述 : 设置白平衡窗口的显示状态。
// 参数     : hCamera  相机的句柄，由CameraInit函数获得。
//            bShow      TRUE，则表示设置为可见。在调用
//             CameraImageOverlay后，图像内容上将以矩形
//             的方式叠加白平衡参考窗口的位置。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraSetWbWinVisible(
    CameraHandle    hCamera,
    BOOL            bShow
);

/******************************************************/
// 函数名   : CameraImageOverlay
// 功能描述 : 将输入的图像数据上叠加十字线、白平衡参考窗口、
//        自动曝光参考窗口等图形。只有设置为可见状态的
//        十字线和参考窗口才能被叠加上。
//        注意，该函数的输入图像必须是RGB格式。
// 参数     : hCamera  相机的句柄，由CameraInit函数获得。
//            pRgbBuffer 图像数据缓冲区。
//            pFrInfo    图像的帧头信息。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraImageOverlay(
    CameraHandle    hCamera,
    BYTE*           pRgbBuffer,
    tSdkFrameHead*  pFrInfo
);

/******************************************************/
// 函数名   : CameraSetCrossLine
// 功能描述 : 设置指定十字线的参数。
// 参数     : hCamera  相机的句柄，由CameraInit函数获得。
//            iLine    表示要设置第几条十字线的状态。范围为[0,8]，共9条。
//            x          十字线中心位置的横坐标值。
//            y      十字线中心位置的纵坐标值。
//            uColor     十字线的颜色，格式为(R|(G<<8)|(B<<16))
//            bVisible   十字线的显示状态。TRUE，表示显示。
//             只有设置为显示状态的十字线，在调用
//             CameraImageOverlay后才会被叠加到图像上。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraSetCrossLine(
    CameraHandle    hCamera,
    int             iLine,
    INT             x,
    INT             y,
    UINT            uColor,
    BOOL            bVisible
);

/******************************************************/
// 函数名   : CameraGetCrossLine
// 功能描述 : 获得指定十字线的状态。
// 参数     : hCamera  相机的句柄，由CameraInit函数获得。
//            iLine    表示要获取的第几条十字线的状态。范围为[0,8]，共9条。
//            px     指针，返回该十字线中心位置的横坐标。
//            py     指针，返回该十字线中心位置的横坐标。
//            pcolor     指针，返回该十字线的颜色，格式为(R|(G<<8)|(B<<16))。
//            pbVisible  指针，返回TRUE，则表示该十字线可见。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraGetCrossLine(
    CameraHandle    hCamera,
    INT             iLine,
    INT*            px,
    INT*            py,
    UINT*           pcolor,
    BOOL*           pbVisible
);

/******************************************************/
// 函数名   : CameraGetCapability
// 功能描述 : 获得相机的特性描述结构体。该结构体中包含了相机
//        可设置的各种参数的范围信息。决定了相关函数的参数
//        返回，也可用于动态创建相机的配置界面。
// 参数     : hCamera   相机的句柄，由CameraInit函数获得。
//            pCameraInfo 指针，返回该相机特性描述的结构体。
//                        tSdkCameraCapbility在CameraDefine.h中定义。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraGetCapability(
    CameraHandle            hCamera,
    tSdkCameraCapbility*    pCameraInfo
);

/******************************************************/
// 函数名   : CameraGetCapabilityEx
// 功能描述 : 获得相机的特性描述结构体。该结构体中包含了相机
//        可设置的各种参数的范围信息。决定了相关函数的参数
//        返回，也可用于动态创建相机的配置界面。
// 参数     : sDeviceModel    相机的型号，由扫描列表中获取
//            pCameraInfo 指针，返回该相机特性描述的结构体。
//                        tSdkCameraCapbility在CameraDefine.h中定义。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraGetCapabilityEx(
    char*                   sDeviceModel,
    tSdkCameraCapbility*    pCameraInfo,
    PVOID                   hCameraHandle
);


/******************************************************/
// 函数名   : CameraWriteSN
// 功能描述 : 设置相机的序列号。我公司相机序列号分为3级。
//        0级的是我公司自定义的相机序列号，出厂时已经
//        设定好，1级和2级留给二次开发使用。每级序列
//        号长度都是32个字节。
// 参数     : hCamera  相机的句柄，由CameraInit函数获得。
//            pbySN    序列号的缓冲区。
//            iLevel   要设定的序列号级别，只能是1或者2。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraWriteSN(
    CameraHandle    hCamera,
    BYTE*           pbySN,
    INT             iLevel
);

/******************************************************/
// 函数名   : CameraReadSN
// 功能描述 : 读取相机指定级别的序列号。序列号的定义请参考
//          CameraWriteSN函数的功能描述部分。
// 参数     : hCamera  相机的句柄，由CameraInit函数获得。
//            pbySN    序列号的缓冲区。
//            iLevel     要读取的序列号级别。只能是1和2。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraReadSN(
    CameraHandle        hCamera,
    BYTE*               pbySN,
    INT                 iLevel
);
/******************************************************/
// 函数名   : CameraSetTriggerDelayTime
// 功能描述 : 设置硬件触发模式下的触发延时时间，单位微秒。
//        当硬触发信号来临后，经过指定的延时，再开始采集
//        图像。仅部分型号的相机支持该功能。具体请查看
//        产品说明书。
// 参数     : hCamera    相机的句柄，由CameraInit函数获得。
//            uDelayTimeUs 硬触发延时。单位微秒。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraSetTriggerDelayTime(
    CameraHandle    hCamera,
    UINT            uDelayTimeUs
);

/******************************************************/
// 函数名   : CameraGetTriggerDelayTime
// 功能描述 : 获得当前设定的硬触发延时时间。
// 参数     : hCamera     相机的句柄，由CameraInit函数获得。
//            puDelayTimeUs 指针，返回延时时间，单位微秒。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraGetTriggerDelayTime(
    CameraHandle    hCamera,
    UINT*           puDelayTimeUs
);

/******************************************************/
// 函数名   : CameraSetTriggerCount
// 功能描述 : 设置触发模式下的触发帧数。对软件触发和硬件触发
//        模式都有效。默认为1帧，即一次触发信号采集一帧图像。
// 参数     : hCamera 相机的句柄，由CameraInit函数获得。
//            iCount    一次触发采集的帧数。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraSetTriggerCount(
    CameraHandle    hCamera,
    INT             iCount
);

/******************************************************/
// 函数名   : CameraGetTriggerCount
// 功能描述 : 获得一次触发的帧数。
// 参数     : hCamera  相机的句柄，由CameraInit函数获得。
//            INT* piCount
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraGetTriggerCount(
    CameraHandle    hCamera,
    INT*            piCount
);

/******************************************************/
// 函数名   : CameraSoftTrigger
// 功能描述 : 执行一次软触发。执行后，会触发由CameraSetTriggerCount
//          指定的帧数。
// 参数     : hCamera  相机的句柄，由CameraInit函数获得。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraSoftTrigger(
    CameraHandle    hCamera
);

/******************************************************/
// 函数名   : CameraSetTriggerMode
// 功能描述 : 设置相机的触发模式。
// 参数     : hCamera  相机的句柄，由CameraInit函数获得。
//            iModeSel   模式选择索引号。可设定的模式由
//             CameraGetCapability函数获取。请参考
//               CameraDefine.h中tSdkCameraCapbility的定义。
//             一般情况，0表示连续采集模式；1表示
//             软件触发模式；2表示硬件触发模式。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraSetTriggerMode(
    CameraHandle    hCamera,
    int             iModeSel
);

/******************************************************/
// 函数名   : CameraGetTriggerMode
// 功能描述 : 获得相机的触发模式。
// 参数     : hCamera  相机的句柄，由CameraInit函数获得。
//            piModeSel  指针，返回当前选择的相机触发模式的索引号。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraGetTriggerMode(
    CameraHandle    hCamera,
    INT*            piModeSel
);

/******************************************************/
// 函数名 	: CameraSetStrobeMode
// 功能描述	: 设置IO引脚端子上的STROBE信号。该信号可以做闪光灯控制，也可以做外部机械快门控制。
// 参数	    : hCamera 相机的句柄，由CameraInit函数获得。
//             iMode   当为STROBE_SYNC_WITH_TRIG_AUTO      和触发信号同步，触发后，相机进行曝光时，自动生成STROBE信号。
//                                                         此时，有效极性可设置(CameraSetStrobePolarity)。
//                     当为STROBE_SYNC_WITH_TRIG_MANUAL时，和触发信号同步，触发后，STROBE延时指定的时间后(CameraSetStrobeDelayTime)，
//                                                         再持续指定时间的脉冲(CameraSetStrobePulseWidth)，
//                                                         有效极性可设置(CameraSetStrobePolarity)。
//                     当为STROBE_ALWAYS_HIGH时，STROBE信号恒为高,忽略其他设置
//                     当为STROBE_ALWAYS_LOW时，STROBE信号恒为低,忽略其他设置
//
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall  CameraSetStrobeMode(
    CameraHandle    hCamera,
    INT             iMode
);

/******************************************************/
// 函数名 	: CameraGetStrobeMode
// 功能描述	: 或者当前STROBE信号设置的模式。
// 参数	    : hCamera 相机的句柄，由CameraInit函数获得。
//             piMode  指针，返回STROBE_SYNC_WITH_TRIG_AUTO,STROBE_SYNC_WITH_TRIG_MANUAL、STROBE_ALWAYS_HIGH或者STROBE_ALWAYS_LOW。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall  CameraGetStrobeMode(
    CameraHandle    hCamera,
    INT*            piMode
);

/******************************************************/
// 函数名 	: CameraSetStrobeDelayTime
// 功能描述	: 当STROBE信号处于STROBE_SYNC_WITH_TRIG时，通过该函数设置其相对触发信号延时时间。
// 参数	    : hCamera       相机的句柄，由CameraInit函数获得。
//             uDelayTimeUs  相对触发信号的延时时间，单位为us。可以为0，但不能为负数。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall  CameraSetStrobeDelayTime(
    CameraHandle    hCamera,
    UINT            uDelayTimeUs
);

/******************************************************/
// 函数名 	: CameraGetStrobeDelayTime
// 功能描述	: 当STROBE信号处于STROBE_SYNC_WITH_TRIG时，通过该函数获得其相对触发信号延时时间。
// 参数	    : hCamera           相机的句柄，由CameraInit函数获得。
//             upDelayTimeUs     指针，返回延时时间，单位us。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall  CameraGetStrobeDelayTime(
    CameraHandle    hCamera,
    UINT*           upDelayTimeUs
);

/******************************************************/
// 函数名 	: CameraSetStrobePulseWidth
// 功能描述	: 当STROBE信号处于STROBE_SYNC_WITH_TRIG时，通过该函数设置其脉冲宽度。
// 参数	    : hCamera       相机的句柄，由CameraInit函数获得。
//             uTimeUs       脉冲的宽度，单位为时间us。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall  CameraSetStrobePulseWidth(
    CameraHandle    hCamera,
    UINT            uTimeUs
);

/******************************************************/
// 函数名 	: CameraGetStrobePulseWidth
// 功能描述	: 当STROBE信号处于STROBE_SYNC_WITH_TRIG时，通过该函数获得其脉冲宽度。
// 参数	    : hCamera   相机的句柄，由CameraInit函数获得。
//             upTimeUs  指针，返回脉冲宽度。单位为时间us。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall  CameraGetStrobePulseWidth(
    CameraHandle    hCamera,
    UINT*           upTimeUs
);

/******************************************************/
// 函数名 	: CameraSetStrobePolarity
// 功能描述	: 当STROBE信号处于STROBE_SYNC_WITH_TRIG时，通过该函数设置其有效电平的极性。默认为高有效，当触发信号到来时，STROBE信号被拉高。
// 参数	    : hCamera   相机的句柄，由CameraInit函数获得。
//             iPolarity STROBE信号的极性，0为低电平有效，1为高电平有效。默认为高电平有效。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall  CameraSetStrobePolarity(
    CameraHandle    hCamera,
    INT             uPolarity
);

/******************************************************/
// 函数名 	: CameraGetStrobePolarity
// 功能描述	: 获得相机当前STROBE信号的有效极性。默认为高电平有效。
// 参数	    : hCamera       相机的句柄，由CameraInit函数获得。
//             ipPolarity    指针，返回STROBE信号当前的有效极性。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall  CameraGetStrobePolarity(
    CameraHandle    hCamera,
    INT*            upPolarity
);

/******************************************************/
// 函数名 	: CameraSetExtTrigSignalType
// 功能描述	: 设置相机外触发信号的种类。上边沿、下边沿、或者高、低电平方式。
// 参数	    : hCamera   相机的句柄，由CameraInit函数获得。
//             iType     外触发信号种类，返回值参考CameraDefine.h中
//                       emExtTrigSignal类型定义。

// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall  CameraSetExtTrigSignalType(
    CameraHandle    hCamera,
    INT             iType
);

/******************************************************/
// 函数名 	: CameraGetExtTrigSignalType
// 功能描述	: 获得相机当前外触发信号的种类。
// 参数	    : hCamera   相机的句柄，由CameraInit函数获得。
//             ipType    指针，返回外触发信号种类，返回值参考CameraDefine.h中
//                       emExtTrigSignal类型定义。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall  CameraGetExtTrigSignalType(
    CameraHandle    hCamera,
    INT*            ipType
);

/******************************************************/
// 函数名 	: CameraSetExtTrigShutterType
// 功能描述	: 设置外触发模式下，相机快门的方式，默认为标准快门方式。
//              部分滚动快门的CMOS相机支持GRR方式。
// 参数	    : hCamera   相机的句柄，由CameraInit函数获得。
//             iType     外触发快门方式。参考CameraDefine.h中emExtTrigShutterMode类型。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall  CameraSetExtTrigShutterType(
    CameraHandle    hCamera,
    INT             iType
);

/******************************************************/
// 函数名 	: CameraSetExtTrigShutterType
// 功能描述	: 获得外触发模式下，相机快门的方式，默认为标准快门方式。
//              部分滚动快门的CMOS相机支持GRR方式。
// 参数	    : hCamera   相机的句柄，由CameraInit函数获得。
//             ipType    指针，返回当前设定的外触发快门方式。返回值参考
//                       CameraDefine.h中emExtTrigShutterMode类型。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall  CameraGetExtTrigShutterType(
    CameraHandle    hCamera,
    INT*            ipType
);

/******************************************************/
// 函数名 	: CameraSetExtTrigDelayTime
// 功能描述	: 设置外触发信号延时时间，默认为0，单位为微秒。
//              当设置的值uDelayTimeUs不为0时，相机接收到外触发信号后，将延时uDelayTimeUs个微秒后再进行图像捕获。
// 参数	    : hCamera       相机的句柄，由CameraInit函数获得。
//             uDelayTimeUs  延时时间，单位为微秒，默认为0.
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall  CameraSetExtTrigDelayTime(
    CameraHandle    hCamera,
    UINT            uDelayTimeUs
);

/******************************************************/
// 函数名 	: CameraGetExtTrigDelayTime
// 功能描述	: 获得设置的外触发信号延时时间，默认为0，单位为微秒。
// 参数	    : hCamera   相机的句柄，由CameraInit函数获得。
//            UINT* upDelayTimeUs
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall  CameraGetExtTrigDelayTime(
    CameraHandle    hCamera,
    UINT*           upDelayTimeUs
);

/******************************************************/
// 函数名 	: CameraSetExtTrigJitterTime
// 功能描述	: 设置相机外触发信号的消抖时间，只有当外触发信号模式选择高电平或者低电平触发时，
//              去抖时间才会生效。默认为0，单位为微秒，最大150毫秒
// 参数	    : hCamera   相机的句柄，由CameraInit函数获得。
//            UINT uTimeUs
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall  CameraSetExtTrigJitterTime(
    CameraHandle    hCamera,
    UINT            uTimeUs
);

/******************************************************/
// 函数名 	: CameraGetExtTrigJitterTime
// 功能描述	: 获得设置的相机外触发消抖时间，默认为0.单位为微秒。
// 参数	    : hCamera   相机的句柄，由CameraInit函数获得。
//            UINT* upTimeUs
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall  CameraGetExtTrigJitterTime(
    CameraHandle    hCamera,
    UINT*           upTimeUs
);

/******************************************************/
// 函数名 	: CameraGetExtTrigCapability
// 功能描述	: 获得相机外触发的属性掩码
// 参数	    : hCamera           相机的句柄，由CameraInit函数获得。
//             puCapabilityMask  指针，返回该相机外触发特性掩码，掩码参考CameraDefine.h中
//                               EXT_TRIG_MASK_ 开头的宏定义。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall  CameraGetExtTrigCapability(
    CameraHandle    hCamera,
    UINT*           puCapabilityMask
);


/******************************************************/
// 函数名   : CameraGetResolutionForSnap
// 功能描述 : 获得抓拍模式下的分辨率选择索引号。
// 参数     : hCamera        相机的句柄，由CameraInit函数获得。
//            pImageResolution 指针，返回抓拍模式的分辨率。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraGetResolutionForSnap(
    CameraHandle            hCamera,
    tSdkImageResolution*    pImageResolution
);

/******************************************************/
// 函数名   : CameraSetResolutionForSnap
// 功能描述 : 设置抓拍模式下相机输出图像的分辨率。
// 参数     : hCamera       相机的句柄，由CameraInit函数获得。
//            pImageResolution 如果pImageResolution->iWidth
//                 和 pImageResolution->iHeight都为0，
//                         则表示设定为跟随当前预览分辨率。抓
//                         怕到的图像的分辨率会和当前设定的
//                 预览分辨率一样。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraSetResolutionForSnap(
    CameraHandle            hCamera,
    tSdkImageResolution*    pImageResolution
);

/******************************************************/
// 函数名   : CameraCustomizeResolution
// 功能描述 : 打开分辨率自定义面板，并通过可视化的方式
//        来配置一个自定义分辨率。
// 参数     : hCamera    相机的句柄，由CameraInit函数获得。
//            pImageCustom 指针，返回自定义的分辨率。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraCustomizeResolution(
    CameraHandle            hCamera,
    tSdkImageResolution*    pImageCustom
);

/******************************************************/
// 函数名   : CameraCustomizeReferWin
// 功能描述 : 打开参考窗口自定义面板。并通过可视化的方式来
//        获得一个自定义窗口的位置。一般是用自定义白平衡
//        和自动曝光的参考窗口。
// 参数     : hCamera  相机的句柄，由CameraInit函数获得。
//            iWinType   要生成的参考窗口的用途。0,自动曝光参考窗口；
//             1,白平衡参考窗口。
//            hParent    调用该函数的窗口的句柄。可以为NULL。
//            piHOff     指针，返回自定义窗口的左上角横坐标。
//            piVOff     指针，返回自定义窗口的左上角纵坐标。
//            piWidth    指针，返回自定义窗口的宽度。
//            piHeight   指针，返回自定义窗口的高度。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraCustomizeReferWin(
    CameraHandle    hCamera,
    INT             iWinType,
    HWND            hParent,
    INT*            piHOff,
    INT*            piVOff,
    INT*            piWidth,
    INT*            piHeight
);

/******************************************************/
// 函数名   : CameraShowSettingPage
// 功能描述 : 设置相机属性配置窗口显示状态。必须先调用CameraCreateSettingPage
//        成功创建相机属性配置窗口后，才能调用本函数进行
//        显示。
// 参数     : hCamera  相机的句柄，由CameraInit函数获得。
//            bShow    TRUE，显示;FALSE，隐藏。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraShowSettingPage(
    CameraHandle    hCamera,
    BOOL            bShow
);

/******************************************************/
// 函数名   : CameraCreateSettingPage
// 功能描述 : 创建该相机的属性配置窗口。调用该函数，SDK内部会
//        帮您创建好相机的配置窗口，省去了您重新开发相机
//        配置界面的时间。强烈建议使用您使用该函数让
//        SDK为您创建好配置窗口。
// 参数     : hCamera     相机的句柄，由CameraInit函数获得。
//            hParent       应用程序主窗口的句柄。可以为NULL。
//            pWinText      字符串指针，窗口显示的标题栏。
//            pCallbackFunc 窗口消息的回调函数，当相应的事件发生时，
//              pCallbackFunc所指向的函数会被调用，
//              例如切换了参数之类的操作时，pCallbackFunc
//              被回调时，在入口参数处指明了消息类型。
//              这样可以方便您自己开发的界面和我们生成的UI
//              之间进行同步。该参数可以为NULL。
//            pCallbackCtx  回调函数的附加参数。可以为NULL。pCallbackCtx
//              会在pCallbackFunc被回调时，做为参数之一传入。
//              您可以使用该参数来做一些灵活的判断。
//            uReserved     预留。必须设置为0。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraCreateSettingPage(
    CameraHandle            hCamera,
    HWND                    hParent,
    char*                   pWinText,
    CAMERA_PAGE_MSG_PROC    pCallbackFunc,
    PVOID                   pCallbackCtx,
    UINT                    uReserved
);

/******************************************************/
// 函数名   : CameraCreateSettingPageEx
// 功能描述 : 创建该相机的属性配置窗口。调用该函数，SDK内部会
//        帮您创建好相机的配置窗口，省去了您重新开发相机
//        配置界面的时间。强烈建议使用您使用该函数让
//        SDK为您创建好配置窗口。
// 参数     : hCamera     相机的句柄，由CameraInit函数获得。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraCreateSettingPageEx(
    CameraHandle            hCamera
);


/******************************************************/
// 函数名   : CameraSetActiveSettingSubPage
// 功能描述 : 设置相机配置窗口的激活页面。相机配置窗口有多个
//        子页面构成，该函数可以设定当前哪一个子页面
//        为激活状态，显示在最前端。
// 参数     : hCamera  相机的句柄，由CameraInit函数获得。
//            index      子页面的索引号。参考CameraDefine.h中
//             PROP_SHEET_INDEX的定义。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraSetActiveSettingSubPage(
    CameraHandle    hCamera,
    INT             index
);

/******************************************************/
// 函数名   : CameraSpecialControl
// 功能描述 : 相机一些特殊配置所调用的接口，二次开发时一般不需要
//        调用。
// 参数     : hCamera  相机的句柄，由CameraInit函数获得。
//            dwCtrlCode 控制码。
//            dwParam    控制子码，不同的dwCtrlCode时，意义不同。
//            lpData     附加参数。不同的dwCtrlCode时，意义不同。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraSpecialControl(
    CameraHandle    hCamera,
    DWORD           dwCtrlCode,
    DWORD           dwParam,
    LPVOID          lpData
);

/******************************************************/
// 函数名   : CameraGetFrameStatistic
// 功能描述 : 获得相机接收帧率的统计信息，包括错误帧和丢帧的情况。
// 参数     : hCamera        相机的句柄，由CameraInit函数获得。
//            psFrameStatistic 指针，返回统计信息。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraGetFrameStatistic(
    CameraHandle            hCamera,
    tSdkFrameStatistic*     psFrameStatistic
);

/******************************************************/
// 函数名   : CameraSetNoiseFilter
// 功能描述 : 设置图像降噪模块的使能状态。
// 参数     : hCamera 相机的句柄，由CameraInit函数获得。
//            bEnable   TRUE，使能；FALSE，禁止。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraSetNoiseFilter(
    CameraHandle    hCamera,
    BOOL            bEnable
);

/******************************************************/
// 函数名   : CameraGetNoiseFilterState
// 功能描述 : 获得图像降噪模块的使能状态。
// 参数     : hCamera  相机的句柄，由CameraInit函数获得。
//            *pEnable   指针，返回状态。TRUE，为使能。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraGetNoiseFilterState(
    CameraHandle    hCamera,
    BOOL*           pEnable
);

/******************************************************/
// 函数名   : CameraRstTimeStamp
// 功能描述 : 复位图像采集的时间戳，从0开始。
// 参数     : CameraHandle hCamera
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraRstTimeStamp(
    CameraHandle    hCamera
);

/******************************************************/
// 函数名   : CameraSaveUserData
// 功能描述 : 将用户自定义的数据保存到相机的非易性存储器中。
//              每个型号的相机可能支持的用户数据区最大长度不一样。
//              可以从设备的特性描述中获取该长度信息。
// 参数     : hCamera    相机的句柄，由CameraInit函数获得。
//            uStartAddr  起始地址，从0开始。
//            pbData      数据缓冲区指针
//            ilen        写入数据的长度，ilen + uStartAddr必须
//                        小于用户区最大长度
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraSaveUserData(
    CameraHandle    hCamera,
    UINT            uStartAddr,
    BYTE            *pbData,
    int             ilen
);

/******************************************************/
// 函数名   : CameraLoadUserData
// 功能描述 : 从相机的非易性存储器中读取用户自定义的数据。
//              每个型号的相机可能支持的用户数据区最大长度不一样。
//              可以从设备的特性描述中获取该长度信息。
// 参数     : hCamera    相机的句柄，由CameraInit函数获得。
//            uStartAddr  起始地址，从0开始。
//            pbData      数据缓冲区指针，返回读到的数据。
//            ilen        读取数据的长度，ilen + uStartAddr必须
//                        小于用户区最大长度
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraLoadUserData(
    CameraHandle    hCamera,
    UINT            uStartAddr,
    BYTE            *pbData,
    int             ilen
);

/******************************************************/
// 函数名   : CameraGetFriendlyName
// 功能描述 : 读取用户自定义的设备昵称。
// 参数     : hCamera    相机的句柄，由CameraInit函数获得。
//            pName      指针，返回指向0结尾的字符串，
//             设备昵称不超过32个字节，因此该指针
//             指向的缓冲区必须大于等于32个字节空间。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraGetFriendlyName(
  CameraHandle  hCamera,
  char*     pName
);

/******************************************************/
// 函数名   : CameraSetFriendlyName
// 功能描述 : 设置用户自定义的设备昵称。
// 参数     : hCamera    相机的句柄，由CameraInit函数获得。
//            pName      指针，指向0结尾的字符串，
//             设备昵称不超过32个字节，因此该指针
//             指向字符串必须小于等于32个字节空间。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraSetFriendlyName(
  CameraHandle  hCamera,
  char*       pName
);

/******************************************************/
// 函数名   : CameraSdkGetVersionString
// 功能描述 :
// 参数     : pVersionString 指针，返回SDK版本字符串。
//                            该指针指向的缓冲区大小必须大于
//                            32个字节
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraSdkGetVersionString(
  char*       pVersionString
);

/******************************************************/
// 函数名   : CameraCheckFwUpdate
// 功能描述 : 检测固件版本，是否需要升级。
// 参数     : hCamera 相机的句柄，由CameraInit函数获得。
//            pNeedUpdate 指针，返回固件检测状态，TRUE表示需要更新
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraCheckFwUpdate(
  CameraHandle  hCamera,
  BOOL*     pNeedUpdate
);

/******************************************************/
// 函数名   : CameraGetFirmwareVision
// 功能描述 : 获得固件版本的字符串
// 参数     : hCamera 相机的句柄，由CameraInit函数获得。
//            pVersion 必须指向一个大于32字节的缓冲区，
//                      返回固件的版本字符串。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraGetFirmwareVision(
  CameraHandle  hCamera,
  char*     pVersion
);

/******************************************************/
// 函数名   : CameraGetEnumInfo
// 功能描述 : 获得指定设备的枚举信息
// 参数     : hCamera 相机的句柄，由CameraInit函数获得。
//            pCameraInfo 指针，返回设备的枚举信息。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraGetEnumInfo(
  CameraHandle    hCamera,
  tSdkCameraDevInfo*  pCameraInfo
);

/******************************************************/
// 函数名   : CameraGetInerfaceVersion
// 功能描述 : 获得指定设备接口的版本
// 参数     : hCamera 相机的句柄，由CameraInit函数获得。
//            pVersion 指向一个大于32字节的缓冲区，返回接口版本字符串。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraGetInerfaceVersion(
  CameraHandle    hCamera,
  char*       pVersion
);

/******************************************************/
// 函数名   : CameraSetIOState
// 功能描述 : 设置指定IO的电平状态，IO为输出型IO，相机
//              预留可编程输出IO的个数由tSdkCameraCapbility中
//              iOutputIoCounts决定。
// 参数     : hCamera 相机的句柄，由CameraInit函数获得。
//            iOutputIOIndex IO的索引号，从0开始。
//            uState 要设定的状态，1为高，0为低
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraSetIOState(
  CameraHandle    hCamera,
  INT         iOutputIOIndex,
  UINT        uState
);

/******************************************************/
// 函数名   : CameraGetIOState
// 功能描述 : 设置指定IO的电平状态，IO为输入型IO，相机
//              预留可编程输出IO的个数由tSdkCameraCapbility中
//              iInputIoCounts决定。
// 参数     : hCamera 相机的句柄，由CameraInit函数获得。
//            iInputIOIndex IO的索引号，从0开始。
//            puState 指针，返回IO状态,1为高，0为低
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraGetIOState(
  CameraHandle    hCamera,
  INT         iInputIOIndex,
  UINT*         puState
);

/******************************************************/
// 函数名   : CameraSetInPutIOMode
// 功能描述 : 设置输入IO的模式，相机
//              预留可编程输出IO的个数由tSdkCameraCapbility中
//              iInputIoCounts决定。
// 参数     : hCamera 相机的句柄，由CameraInit函数获得。
//            iInputIOIndex IO的索引号，从0开始。
//            iMode IO模式,参考CameraDefine.h中emCameraGPIOMode
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraSetInPutIOMode(
	CameraHandle    hCamera,
	INT         iInputIOIndex,
	INT			iMode
	);

/******************************************************/
// 函数名   : CameraSetOutPutIOMode
// 功能描述 : 设置输出IO的模式，相机
//              预留可编程输出IO的个数由tSdkCameraCapbility中
//              iOutputIoCounts决定。
// 参数     : hCamera 相机的句柄，由CameraInit函数获得。
//            iOutputIOIndex IO的索引号，从0开始。
//            iMode IO模式,参考CameraDefine.h中emCameraGPIOMode
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraSetOutPutIOMode(
	CameraHandle    hCamera,
	INT         iOutputIOIndex,
	INT			iMode
	);

/******************************************************/
// 函数名   : CameraSetOutPutPWM
// 功能描述 : 设置PWM型输出的参数，相机
//              预留可编程输出IO的个数由tSdkCameraCapbility中
//              iOutputIoCounts决定。
// 参数     : hCamera 相机的句柄，由CameraInit函数获得。
//            iOutputIOIndex IO的索引号，从0开始。
//            iCycle PWM的周期，单位(us)
//			  uDuty  占用比，取值1%~99%
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraSetOutPutPWM(
	CameraHandle    hCamera,
	INT         iOutputIOIndex,
	UINT		iCycle,
	UINT		uDuty
	);

/******************************************************/
// 函数名   : CameraSetAeAlgorithm
// 功能描述 : 设置自动曝光时选择的算法，不同的算法适用于
//        不同的场景。
// 参数     : hCamera       相机的句柄，由CameraInit函数获得。
//            iIspProcessor   选择执行该算法的对象，参考CameraDefine.h
//                emSdkIspProcessor的定义
//            iAeAlgorithmSel   要选择的算法编号。从0开始，最大值由tSdkCameraCapbility
//                中iAeAlmSwDesc和iAeAlmHdDesc决定。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraSetAeAlgorithm(
    CameraHandle    hCamera,
    INT             iIspProcessor,
    INT             iAeAlgorithmSel
);

/******************************************************/
// 函数名   : CameraGetAeAlgorithm
// 功能描述 : 获得当前自动曝光所选择的算法
// 参数     : hCamera       相机的句柄，由CameraInit函数获得。
//            iIspProcessor   选择执行该算法的对象，参考CameraDefine.h
//                emSdkIspProcessor的定义
//            piAeAlgorithmSel  返回当前选择的算法编号。从0开始，最大值由tSdkCameraCapbility
//                中iAeAlmSwDesc和iAeAlmHdDesc决定。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraGetAeAlgorithm(
    CameraHandle    hCamera,
    INT             iIspProcessor,
    INT*            piAlgorithmSel
);

/******************************************************/
// 函数名   : CameraSetBayerDecAlgorithm
// 功能描述 : 设置Bayer数据转彩色的算法。
// 参数     : hCamera       相机的句柄，由CameraInit函数获得。
//            iIspProcessor   选择执行该算法的对象，参考CameraDefine.h
//                emSdkIspProcessor的定义
//            iAlgorithmSel     要选择的算法编号。从0开始，最大值由tSdkCameraCapbility
//                中iBayerDecAlmSwDesc和iBayerDecAlmHdDesc决定。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraSetBayerDecAlgorithm(
    CameraHandle    hCamera,
    INT             iIspProcessor,
    INT             iAlgorithmSel
);

/******************************************************/
// 函数名   : CameraGetBayerDecAlgorithm
// 功能描述 : 获得Bayer数据转彩色所选择的算法。
// 参数     : hCamera       相机的句柄，由CameraInit函数获得。
//            iIspProcessor   选择执行该算法的对象，参考CameraDefine.h
//                emSdkIspProcessor的定义
//            piAlgorithmSel    返回当前选择的算法编号。从0开始，最大值由tSdkCameraCapbility
//                中iBayerDecAlmSwDesc和iBayerDecAlmHdDesc决定。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraGetBayerDecAlgorithm(
    CameraHandle    hCamera,
    INT             iIspProcessor,
    INT*            piAlgorithmSel
);

/******************************************************/
// 函数名   : CameraSetIspProcessor
// 功能描述 : 设置图像处理单元的算法执行对象，由PC端或者相机端
//        来执行算法，当由相机端执行时，会降低PC端的CPU占用率。
// 参数     : hCamera   相机的句柄，由CameraInit函数获得。
//            iIspProcessor 参考CameraDefine.h中
//              emSdkIspProcessor的定义。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraSetIspProcessor(
    CameraHandle    hCamera,
    INT             iIspProcessor
);

/******************************************************/
// 函数名   : CameraGetIspProcessor
// 功能描述 : 获得图像处理单元的算法执行对象。
// 参数     : hCamera    相机的句柄，由CameraInit函数获得。
//            piIspProcessor 返回选择的对象，返回值参考CameraDefine.h中
//               emSdkIspProcessor的定义。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraGetIspProcessor(
    CameraHandle    hCamera,
    INT*            piIspProcessor
);

/******************************************************/
// 函数名   : CameraSetBlackLevel
// 功能描述 : 设置图像的黑电平基准，默认值为0
// 参数     : hCamera   相机的句柄，由CameraInit函数获得。
//            iBlackLevel 要设定的电平值。范围为0到255。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraSetBlackLevel(
  CameraHandle    hCamera,
  INT         iBlackLevel
);

/******************************************************/
// 函数名   : CameraGetBlackLevel
// 功能描述 : 获得图像的黑电平基准，默认值为0
// 参数     : hCamera    相机的句柄，由CameraInit函数获得。
//            piBlackLevel 返回当前的黑电平值。范围为0到255。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraGetBlackLevel(
  CameraHandle    hCamera,
  INT*        piBlackLevel
);

/******************************************************/
// 函数名   : CameraSetWhiteLevel
// 功能描述 : 设置图像的白电平基准，默认值为255
// 参数     : hCamera   相机的句柄，由CameraInit函数获得。
//            iWhiteLevel 要设定的电平值。范围为0到255。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraSetWhiteLevel(
  CameraHandle    hCamera,
  INT         iWhiteLevel
);

/******************************************************/
// 函数名   : CameraGetWhiteLevel
// 功能描述 : 获得图像的白电平基准，默认值为255
// 参数     : hCamera    相机的句柄，由CameraInit函数获得。
//            piWhiteLevel 返回当前的白电平值。范围为0到255。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraGetWhiteLevel(
  CameraHandle    hCamera,
  INT*        piWhiteLevel
);


/******************************************************/
// 函数名 	: CameraSetIspOutFormat
// 功能描述	: 设置CameraImageProcess函数的图像处理的输出格式，支持
//              CAMERA_MEDIA_TYPE_MONO8和CAMERA_MEDIA_TYPE_RGB8和CAMERA_MEDIA_TYPE_RGBA8
//              以及CAMERA_MEDIA_TYPE_BGR8、CAMERA_MEDIA_TYPE_BGRA8
//              (在CameraDefine.h中定义)5种，分别对应8位灰度图像和24RGB、32位RGB、24位BGR、32位BGR彩色图像。
//              默认输出是CAMERA_MEDIA_TYPE_BGR8格式。
// 参数	    : hCamera		相机的句柄，由CameraInit函数获得。
//             uFormat	要设定格式。CAMERA_MEDIA_TYPE_MONO8或者CAMERA_MEDIA_TYPE_RGB8、CAMERA_MEDIA_TYPE_RGBA8	、CAMERA_MEDIA_TYPE_BGR8、CAMERA_MEDIA_TYPE_BGRA8
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraSetIspOutFormat(
    CameraHandle    hCamera,
    UINT            uFormat
);

/******************************************************/
// 函数名 	: CameraGetIspOutFormat
// 功能描述	: 获得CameraGetImageBuffer函数图像处理的输出格式，支持
//              CAMERA_MEDIA_TYPE_MONO8和CAMERA_MEDIA_TYPE_RGB8和CAMERA_MEDIA_TYPE_RGBA8
//              以及CAMERA_MEDIA_TYPE_BGR8、CAMERA_MEDIA_TYPE_BGRA8
//              (在CameraDefine.h中定义)5种，分别对应8位灰度图像和24RGB、32位RGB、24位BGR、32位BGR彩色图像。
// 参数	    : hCamera		相机的句柄，由CameraInit函数获得。
//             puFormat	返回当前设定的格式。AMERA_MEDIA_TYPE_MONO8或者CAMERA_MEDIA_TYPE_RGB8、CAMERA_MEDIA_TYPE_RGBA8	、CAMERA_MEDIA_TYPE_BGR8、CAMERA_MEDIA_TYPE_BGRA8
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraGetIspOutFormat(
    CameraHandle    hCamera,
    UINT*           puFormat
);

/******************************************************/
// 函数名 	: CameraGetErrorString
// 功能描述	: 获得错误码对应的描述字符串
// 参数	    : iStatusCode		错误码。(定义于CameraStatus.h中)
// 返回值   : 成功时，返回错误码对应的字符串首地址;
//            否则返回NULL。
/******************************************************/
MVSDK_API char* __stdcall CameraGetErrorString(
    CameraSdkStatus     iStatusCode
);

/******************************************************/
// 函数名 	: CameraGetImageBufferEx2
// 功能描述	: 获得一帧图像数据。该接口获得的图像是经过处理后的RGB格式。该函数调用后，
//			  不需要调用 CameraReleaseImageBuffer 释放，也不要调用free之类的函数释放
//              来释放该函数返回的图像数据缓冲区。
// 参数	    : hCamera	    相机的句柄，由CameraInit函数获得。
//             pImageData  接收图像数据的缓冲区，大小必须和uOutFormat指定的格式相匹配，否则数据会溢出
//             piWidth     整形指针，返回图像的宽度
//             piHeight    整形指针，返回图像的高度
//             wTimes      抓取图像的超时时间。单位毫秒。在
//						wTimes时间内还未获得图像，则该函数
//						会返回超时信息。
// 返回值   : 成功时，返回RGB数据缓冲区的首地址;
//            否则返回0。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraGetImageBufferEx2(
    CameraHandle    hCamera,
    BYTE*           pImageData,
    UINT            uOutFormat,
    int*            piWidth,
    int*            piHeight,
    UINT            wTimes
);

/******************************************************/
// 函数名 	: CameraGetImageBufferEx3
// 功能描述	: 获得一帧图像数据。该接口获得的图像是经过处理后的RGB格式。该函数调用后，
//			  不需要调用 CameraReleaseImageBuffer 释放.
//              uOutFormat 0 : 8 BIT gray 1:rgb24 2:rgba32 3:bgr24 4:bgra32
// 参数	    : hCamera	    相机的句柄，由CameraInit函数获得。
//             pImageData  接收图像数据的缓冲区，大小必须和uOutFormat指定的格式相匹配，否则数据会溢出
//            piWidth      整形指针，返回图像的宽度
//            piHeight     整形指针，返回图像的高度
//            puTimeStamp  无符号整形，返回图像时间戳
//            UINT wTimes  抓取图像的超时时间。单位毫秒。在
//			wTimes       时间内还未获得图像，则该函数会返回超时信息。
// 返回值   : 成功时，返回RGB数据缓冲区的首地址;
//            否则返回0。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraGetImageBufferEx3(
	CameraHandle hCamera,
	BYTE*pImageData,
	UINT uOutFormat,
	int *piWidth,
	int *piHeight,
	UINT* puTimeStamp,
	UINT wTimes
);

/******************************************************/
// 函数名 	: CameraGetCapabilityEx2
// 功能描述	: 获得该相机的一些特性。
// 参数	    : hCamera		    相机的句柄，由CameraInit函数获得。
//             pMaxWidth	    返回该相机最大分辨率的宽度
//             pMaxHeight      返回该相机最大分辨率的高度
//             pbColorCamera    返回该相机是否是彩色相机。1表示彩色相机，0表示黑白相机
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraGetCapabilityEx2(
    CameraHandle    hCamera,
    int*            pMaxWidth,
    int*            pMaxHeight,
    int*            pbColorCamera
);

/******************************************************/
// 函数名 	: CameraReConnect
// 功能描述	: 重新连接设备，用于USB设备意外掉线后重连
// 参数	    : hCamera	   相机的句柄，由CameraInit函数获得。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraReConnect(
    CameraHandle    hCamera
);


/******************************************************/
// 函数名 	: CameraConnectTest
// 功能描述	: 测试相机的连接状态，用于检测相机是否掉线
// 参数	    : hCamera	   相机的句柄，由CameraInit函数获得。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0)，表示相机连接状态正常;
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraConnectTest(
    CameraHandle    hCamera
);

/******************************************************/
// 函数名 	: CameraSetLedEnable
// 功能描述	: 设置相机的LED使能状态，不带LED的型号，此函数返回错误代码，表示不支持。
// 参数	    : hCamera	   相机的句柄，由CameraInit函数获得。
//             index       LED灯的索引号，从0开始。如果只有一个可控制亮度的LED，则该参数为0 。
//             enable      使能状态
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0)，表示相机连接状态正常;
//            否则返回 非0值，参考CameraStatus.h中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraSetLedEnable(
	CameraHandle    hCamera,
	int             index,
	BOOL            enable
	);

/******************************************************/
// 函数名 	: CameraGetLedEnable
// 功能描述	: 获得相机的LED使能状态，不带LED的型号，此函数返回错误代码，表示不支持。
// 参数	    : hCamera	   相机的句柄，由CameraInit函数获得。
//             index       LED灯的索引号，从0开始。如果只有一个可控制亮度的LED，则该参数为0 。
//             enable      指针，返回LED使能状态
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0)，表示相机连接状态正常;
//            否则返回 非0值，参考CameraStatus.h中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraGetLedEnable(
	CameraHandle    hCamera,
	int             index,
	BOOL*           enable
	);

/******************************************************/
// 函数名 	: CameraSetLedOnOff
// 功能描述	: 设置相机的LED开关状态，不带LED的型号，此函数返回错误代码，表示不支持。
// 参数	    : hCamera	   相机的句柄，由CameraInit函数获得。
//             index       LED灯的索引号，从0开始。如果只有一个可控制亮度的LED，则该参数为0 。
//             onoff	   LED开关状态
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0)，表示相机连接状态正常;
//            否则返回 非0值，参考CameraStatus.h中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraSetLedOnOff(
	CameraHandle    hCamera,
	int             index,
	BOOL            onoff
	);

/******************************************************/
// 函数名 	: CameraGetLedOnOff
// 功能描述	: 获得相机的LED开关状态，不带LED的型号，此函数返回错误代码，表示不支持。
// 参数	    : hCamera	   相机的句柄，由CameraInit函数获得。
//             index       LED灯的索引号，从0开始。如果只有一个可控制亮度的LED，则该参数为0 。
//             onoff	   指针，返回LED开关状态
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0)，表示相机连接状态正常;
//            否则返回 非0值，参考CameraStatus.h中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraGetLedOnOff(
	CameraHandle    hCamera,
	int             index,
	BOOL*           onoff
	);

/******************************************************/
// 函数名 	: CameraSetLedDuration
// 功能描述	: 设置相机的LED持续时间，不带LED的型号，此函数返回错误代码，表示不支持。
// 参数	    : hCamera	    相机的句柄，由CameraInit函数获得。
//             index        LED灯的索引号，从0开始。如果只有一个可控制亮度的LED，则该参数为0 。
//             duration		LED持续时间，单位毫秒
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0)，表示相机连接状态正常;
//            否则返回 非0值，参考CameraStatus.h中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraSetLedDuration(
	CameraHandle    hCamera,
	int             index,
	UINT            duration
	);

/******************************************************/
// 函数名 	: CameraGetLedDuration
// 功能描述	: 获得相机的LED持续时间，不带LED的型号，此函数返回错误代码，表示不支持。
// 参数	    : hCamera	    相机的句柄，由CameraInit函数获得。
//             index        LED灯的索引号，从0开始。如果只有一个可控制亮度的LED，则该参数为0 。
//             duration		指针，返回LED持续时间，单位毫秒
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0)，表示相机连接状态正常;
//            否则返回 非0值，参考CameraStatus.h中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraGetLedDuration(
	CameraHandle    hCamera,
	int             index,
	UINT*           duration
	);

/******************************************************/
// 函数名 	: CameraSetLedBrightness
// 功能描述	: 设置相机的LED亮度，不带LED的型号，此函数返回错误代码，表示不支持。
// 参数	    : hCamera	   相机的句柄，由CameraInit函数获得。
//             index      LED灯的索引号，从0开始。如果只有一个可控制亮度的LED，则该参数为0 。
//             uBrightness LED亮度值，范围0到255. 0表示关闭，255最亮。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0)，表示相机连接状态正常;
//            否则返回 非0值，参考CameraStatus.h中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraSetLedBrightness(
    CameraHandle    hCamera,
    int             index,
    UINT            uBrightness
);

/******************************************************/
// 函数名 	: CameraGetLedBrightness
// 功能描述	: 获得相机的LED亮度，不带LED的型号，此函数返回错误代码，表示不支持。
// 参数	    : hCamera	   相机的句柄，由CameraInit函数获得。
//             index      LED灯的索引号，从0开始。如果只有一个可控制亮度的LED，则该参数为0 。
//             uBrightness 指针，返回LED亮度值，范围0到255. 0表示关闭，255最亮。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0)，表示相机连接状态正常;
//            否则返回 非0值，参考CameraStatus.h中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraGetLedBrightness(
    CameraHandle    hCamera,
    int             index,
    UINT*           uBrightness
);

/******************************************************/
// 函数名 	: CameraEnableTransferRoi
// 功能描述	: 使能或者禁止相机的多区域传输功能，不带该功能的型号，此函数返回错误代码，表示不支持。
//              该功能主要用于在相机端将采集的整幅画面切分，只传输指定的多个区域，以提高传输帧率。
//              多个区域传输到PC上后，会自动拼接成整幅画面，没有被传输的部分，会用黑色填充。
// 参数	    : hCamera	    相机的句柄，由CameraInit函数获得。
//             uEnableMask 区域使能状态掩码，对应的比特位为1表示使能。0为禁止。目前SDK支持4个可编辑区域，index范围为0到3，即bit0 ，bit1，bit2，bit3控制4个区域的使能状态。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0)，表示相机连接状态正常;
//            对于不支持多区域ROI传输的型号，该函数会返回 CAMERA_STATUS_NOT_SUPPORTED(-4) 表示不支持
//            其它非0值，参考CameraStatus.h中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraEnableTransferRoi(
    CameraHandle    hCamera,
    UINT            uEnableMask
);


/******************************************************/
// 函数名 	: CameraSetTransferRoi
// 功能描述	: 设置相机传输的裁剪区域。在相机端，图像从传感器上被采集后，将会被裁剪成指定的区域来传送，此函数返回错误代码，表示不支持。
// 参数	    : hCamera	   相机的句柄，由CameraInit函数获得。
//             index      ROI区域的索引号，从0开始。
//             X1,Y1      ROI区域的左上角坐标
//             X2,Y2      ROI区域的右上角坐标
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0)，表示相机连接状态正常;
//            对于不支持多区域ROI传输的型号，该函数会返回 CAMERA_STATUS_NOT_SUPPORTED(-4) 表示不支持
//            其它非0值，参考CameraStatus.h中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraSetTransferRoi(
    CameraHandle    hCamera,
    int             index,
    UINT            X1,
    UINT            Y1,
    UINT            X2,
    UINT            Y2
);


/******************************************************/
// 函数名 	: CameraGetTransferRoi
// 功能描述	: 设置相机传输的裁剪区域。在相机端，图像从传感器上被采集后，将会被裁剪成指定的区域来传送，此函数返回错误代码，表示不支持。
// 参数	    : hCamera	   相机的句柄，由CameraInit函数获得。
//             index      ROI区域的索引号，从0开始。
//             pX1,pY1      ROI区域的左上角坐标
//             pX2,pY2      ROI区域的右上角坐标
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0)，表示相机连接状态正常;
//            对于不支持多区域ROI传输的型号，该函数会返回 CAMERA_STATUS_NOT_SUPPORTED(-4) 表示不支持
//            其它非0值，参考CameraStatus.h中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraGetTransferRoi(
    CameraHandle    hCamera,
    int             index,
    UINT*           pX1,
    UINT*           pY1,
    UINT*           pX2,
    UINT*           pY2
);

/******************************************************/
// 函数名 	: CameraAlignMalloc
// 功能描述	: 申请一段对齐的内存空间。功能和malloc类似，但
//						是返回的内存是以align指定的字节数对齐的。
// 参数	    : size	   空间的大小。
//            align    地址对齐的字节数。
// 返回值   : 成功时，返回非0值，表示内存首地址。失败返回NULL。
/******************************************************/
MVSDK_API BYTE* __stdcall CameraAlignMalloc(
    int             size,
    int             align
    );

/******************************************************/
// 函数名 	: CameraAlignFree
// 功能描述	: 释放由CameraAlignMalloc函数申请的内存空间。
// 参数	    : membuffer	   由CameraAlignMalloc返回的内存首地址。
// 返回值   : 无。
/******************************************************/
MVSDK_API void __stdcall CameraAlignFree(
    BYTE*           membuffer
);

/******************************************************/
// 函数名 	: CameraSetAutoConnect
// 功能描述	: 设置自动使能重连
// 参数	    : hCamera	   相机的句柄，由CameraInit函数获得。
//			  bEnable	   使能相机重连，当位TRUE时，SDK内部自动检测相机是否掉线，掉线后自己重连。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0)，表示相机连接状态正常;
//            对于不支持的型号，该函数会返回 CAMERA_STATUS_NOT_SUPPORTED(-4) 表示不支持
//            其它非0值，参考CameraStatus.h中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraSetAutoConnect(CameraHandle hCamera,BOOL bEnable);

/******************************************************/
// 函数名 	: CameraGetAutoConnect
// 功能描述	: 获取自动重连使能
// 参数	    : hCamera	   相机的句柄，由CameraInit函数获得。
//			  pbEnable	   返回相机重连使能
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0)，表示相机连接状态正常;
//            对于不支持的型号，该函数会返回 CAMERA_STATUS_NOT_SUPPORTED(-4) 表示不支持
//            其它非0值，参考CameraStatus.h中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraGetAutoConnect(CameraHandle hCamera,BOOL *pbEnable);

/******************************************************/
// 函数名 	: CameraGetReConnectCounts
// 功能描述	: 获得相机自动重连的次数，前提是CameraSetAutoConnect 使能相机自动重连功能。默认是使能的。
// 参数	    : hCamera	   相机的句柄，由CameraInit函数获得。
//			 puCounts	   返回掉线自动重连的次数
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0)，表示相机连接状态正常;
//            对于不支持的型号，该函数会返回 CAMERA_STATUS_NOT_SUPPORTED(-4) 表示不支持
//            其它非0值，参考CameraStatus.h中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraGetReConnectCounts(CameraHandle hCamera,UINT* puCounts);

/******************************************************/
// 函数名 	: CameraSetSingleGrabMode
// 功能描述	: 使能单帧抓取模式
// 参数	    : hCamera	   相机的句柄，由CameraInit函数获得。
//			  bEnable	   使能单帧抓取模式
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0)，表示相机连接状态正常;
//            对于不支持的型号，该函数会返回 CAMERA_STATUS_NOT_SUPPORTED(-4) 表示不支持
//            其它非0值，参考CameraStatus.h中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraSetSingleGrabMode(CameraHandle hCamera, BOOL bEnable);

/******************************************************/
// 函数名 	: CameraGetSingleGrabMode
// 功能描述	: 获得相机的单帧抓取模式
// 参数	    : hCamera	   相机的句柄，由CameraInit函数获得。
//			 pbEnable	   返回相机的单帧抓取模式
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0)，表示相机连接状态正常;
//            对于不支持的型号，该函数会返回 CAMERA_STATUS_NOT_SUPPORTED(-4) 表示不支持
//            其它非0值，参考CameraStatus.h中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraGetSingleGrabMode(CameraHandle hCamera, BOOL* pbEnable);

/******************************************************/
// 函数名 	: CameraRestartGrab
// 功能描述	: 当相机处于单帧抓取模式时，每当成功抓取到一帧后SDK会进入暂停状态，调用此函数可使SDK退出暂停状态并开始抓取下一帧
// 参数	    : hCamera	   相机的句柄，由CameraInit函数获得。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0)，表示相机连接状态正常;
//            对于不支持的型号，该函数会返回 CAMERA_STATUS_NOT_SUPPORTED(-4) 表示不支持
//            其它非0值，参考CameraStatus.h中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraRestartGrab(CameraHandle hCamera);

/******************************************************/
// 函数名   : CameraEvaluateImageDefinition
// 功能描述 : 图片清晰度评估
// 参数     : hCamera  相机的句柄，由CameraInit函数获得。
//			  iAlgorithSel 使用的评估算法,详见emEvaluateDefinitionAlgorith中的定义
//            pbyIn    输入图像数据的缓冲区地址，不能为NULL。
//            pFrInfo  输入图像的帧头信息
//			  DefinitionValue 返回的清晰度估值（越大越清晰）
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraEvaluateImageDefinition(
	CameraHandle        hCamera,
	INT					iAlgorithSel,
	BYTE*               pbyIn,
	tSdkFrameHead*      pFrInfo,
	double*				DefinitionValue
	);

/******************************************************/
// 函数名   : CameraDrawText
// 功能描述 : 在输入的图像数据中绘制文字
// 参数     : pRgbBuffer 图像数据缓冲区
//			  pFrInfo 图像的帧头信息
//			  pFontFileName 字体文件名
//			  FontWidth 字体宽度
//			  FontHeight 字体高度
//			  pText 要输出的文字
//			  (Left, Top, Width, Height) 文字的输出矩形
//			  TextColor 文字颜色RGB
//			  uFlags 输出标志,详见emCameraDrawTextFlags中的定义
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraDrawText(
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

/******************************************************/
// 函数名   : CameraGigeGetIp
// 功能描述 : 获取GIGE相机的IP地址
// 参数     : pCameraInfo 相机的设备描述信息，可由CameraEnumerateDevice函数获得。
//			  CamIp 相机IP(注意：必须保证传入的缓冲区大于等于16字节)
//			  CamMask 相机子网掩码(注意：必须保证传入的缓冲区大于等于16字节)
//			  CamGateWay 相机网关(注意：必须保证传入的缓冲区大于等于16字节)
//			  EtIp 网卡IP(注意：必须保证传入的缓冲区大于等于16字节)
//			  EtMask 网卡子网掩码(注意：必须保证传入的缓冲区大于等于16字节)
//			  EtGateWay 网卡网关(注意：必须保证传入的缓冲区大于等于16字节)
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraGigeGetIp(
		tSdkCameraDevInfo* pCameraInfo,
		char* CamIp,
		char* CamMask,
		char* CamGateWay,
		char* EtIp,
		char* EtMask,
		char* EtGateWay
	);

/******************************************************/
// 函数名   : CameraGigeSetIp
// 功能描述 : 设置GIGE相机的IP地址
// 参数     : pCameraInfo 相机的设备描述信息，可由CameraEnumerateDevice函数获得。
//			  Ip 相机IP(如：192.168.1.100)
//			  SubMask 相机子网掩码(如：255.255.255.0)
//			  GateWay 相机网关(如：192.168.1.1)
//			  bPersistent TRUE: 设置相机为固定IP，FALSE：设置相机自动分配IP（忽略参数Ip, SubMask, GateWay）
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraGigeSetIp(
	tSdkCameraDevInfo* pCameraInfo,
	char const* Ip,
	char const* SubMask,
	char const* GateWay,
	BOOL bPersistent
	);

/******************************************************/
// 函数名   : CameraGigeGetMac
// 功能描述 : 获取GIGE相机的MAC地址
// 参数     : pCameraInfo 相机的设备描述信息，可由CameraEnumerateDevice函数获得。
//			  CamMac 相机MAC(注意：必须保证传入的缓冲区大于等于18字节)
//			  EtMac 网卡MAC(注意：必须保证传入的缓冲区大于等于18字节)
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraGigeGetMac(
	tSdkCameraDevInfo* pCameraInfo,
	char* CamMac,
	char* EtMac
	);

/******************************************************/
// 函数名   : CameraEnableFastResponse
// 功能描述 : 使能快速响应
// 参数     : hCamera  相机的句柄，由CameraInit函数获得。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraEnableFastResponse(
	CameraHandle hCamera
	);

/******************************************************/
// 函数名   : CameraSetCorrectDeadPixel
// 功能描述 : 使能坏点修正
// 参数     : hCamera  相机的句柄，由CameraInit函数获得。
//				bEnable     TRUE: 使能坏点修正   FALSE: 关闭坏点修正
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraSetCorrectDeadPixel(
	CameraHandle hCamera,
	BOOL bEnable
	);

/******************************************************/
// 函数名   : CameraGetCorrectDeadPixel
// 功能描述 : 获取坏点修正使能状态
// 参数     : hCamera  相机的句柄，由CameraInit函数获得。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraGetCorrectDeadPixel(
	CameraHandle hCamera,
	BOOL* pbEnable
	);

/******************************************************/
// 函数名   : CameraFlatFieldingCorrectSetEnable
// 功能描述 : 使能平场校正
// 参数     : hCamera  相机的句柄，由CameraInit函数获得。
//				bEnable     TRUE: 使能平场校正   FALSE: 关闭平场校正
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraFlatFieldingCorrectSetEnable(
	CameraHandle hCamera,
	BOOL bEnable
	);

/******************************************************/
// 函数名   : CameraFlatFieldingCorrectGetEnable
// 功能描述 : 获取平场校正使能状态
// 参数     : hCamera  相机的句柄，由CameraInit函数获得。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraFlatFieldingCorrectGetEnable(
	CameraHandle hCamera,
	BOOL* pbEnable
	);

/******************************************************/
// 函数名   : CameraFlatFieldingCorrectSetParameter
// 功能描述 : 设置平场校正参数
// 参数     :	hCamera  相机的句柄，由CameraInit函数获得。
//				pDarkFieldingImage 暗场图片
//				pDarkFieldingFrInfo 暗场图片信息
//				pLightFieldingImage 明场图片
//				pLightFieldingFrInfo 明场图片信息
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraFlatFieldingCorrectSetParameter(
	CameraHandle hCamera,
	BYTE const* pDarkFieldingImage,
	tSdkFrameHead const* pDarkFieldingFrInfo,
	BYTE const* pLightFieldingImage,
	tSdkFrameHead const* pLightFieldingFrInfo
	);

/******************************************************/
// 函数名   : CameraFlatFieldingCorrectGetParameterState
// 功能描述 : 获取平场校正参数的状态
// 参数     :	hCamera  相机的句柄，由CameraInit函数获得。
//				pbValid 返回参数是否有效
//				pFilePath 返回参数文件的路径
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraFlatFieldingCorrectGetParameterState(
	CameraHandle hCamera,
	BOOL *pbValid,
	char *pFilePath
	);

/******************************************************/
// 函数名   : CameraFlatFieldingCorrectSaveParameterToFile
// 功能描述 : 保存平场校正参数到文件
// 参数     :	hCamera  相机的句柄，由CameraInit函数获得。
//				pszFileName 文件路径
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraFlatFieldingCorrectSaveParameterToFile(
	CameraHandle hCamera,
	char const* pszFileName
	);

/******************************************************/
// 函数名   : CameraFlatFieldingCorrectLoadParameterFromFile
// 功能描述 : 从文件中加载平场校正参数
// 参数     :	hCamera  相机的句柄，由CameraInit函数获得。
//				pszFileName 文件路径
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraFlatFieldingCorrectLoadParameterFromFile(
	CameraHandle hCamera,
	char const* pszFileName
	);

/******************************************************/
// 函数名   : CameraCommonCall
// 功能描述 : 相机的一些特殊功能调用，二次开发时一般不需要调用。
// 参数     : hCamera   相机的句柄，由CameraInit函数获得。
//            pszCall   功能及参数
//            pszResult 调用结果，不同的pszCall时，意义不同。
//            uResultBufSize pszResult指向的缓冲区的字节大小
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraCommonCall(
	CameraHandle    hCamera,
	char const*		pszCall,
	char*			pszResult,
	UINT			uResultBufSize
	);

/******************************************************/
// 函数名   : CameraSetDenoise3DParams
// 功能描述 : 设置3D降噪参数
// 参数     : hCamera   相机的句柄，由CameraInit函数获得。
//            bEnable  启用或禁用
//            nCount   使用几张图片进行降噪(2-8张)
//            Weights  降噪权重
//					   如当使用3张图片进行降噪则这个参数可以传入3个浮点(0.3,0.3,0.4)，最后一张图片的权重大于前2张
//					   如果不需要使用权重，则把这个参数传入0，表示所有图片的权重相同(0.33,0.33,0.33)
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraSetDenoise3DParams(
	CameraHandle    hCamera,
	BOOL			bEnable,
	int				nCount,
	float			*Weights
	);

/******************************************************/
// 函数名   : CameraGetDenoise3DParams
// 功能描述 : 获取当前的3D降噪参数
// 参数     : hCamera   相机的句柄，由CameraInit函数获得。
//            bEnable  启用或禁用
//            nCount   使用了几张图片进行降噪
//			  bUseWeight 是否使用了降噪权重
//            Weights  降噪权重
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraGetDenoise3DParams(
	CameraHandle    hCamera,
	BOOL			*bEnable,
	int				*nCount,
	BOOL			*bUseWeight,
	float			*Weights
	);

/******************************************************/
// 函数名   : CameraManualDenoise3D
// 功能描述 : 对一组帧进行一次降噪处理
// 参数     : InFramesHead  输入帧头
//			  InFramesData  输入帧数据
//            nCount   输入帧的数量
//            Weights  降噪权重
//					   如当使用3张图片进行降噪则这个参数可以传入3个浮点(0.3,0.3,0.4)，最后一张图片的权重大于前2张
//					   如果不需要使用权重，则把这个参数传入0，表示所有图片的权重相同(0.33,0.33,0.33)
//			  OutFrameHead 输出帧头
//			  OutFrameData 输出帧数据
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraManualDenoise3D(
	tSdkFrameHead	*InFramesHead,
	BYTE			**InFramesData,
	int				nCount,
	float			*Weights,
	tSdkFrameHead	*OutFrameHead,
	BYTE			*OutFrameData
	);

/******************************************************/
// 函数名   : CameraCustomizeDeadPixels
// 功能描述 : 打开坏点编辑面板
// 参数     : hCamera    相机的句柄，由CameraInit函数获得。
//            hParent    调用该函数的窗口的句柄。可以为NULL。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraCustomizeDeadPixels(
	CameraHandle	hCamera,
	HWND			hParent
	);

/******************************************************/
// 函数名   : CameraReadDeadPixels
// 功能描述 : 读取相机坏点
// 参数     : hCamera   相机的句柄，由CameraInit函数获得。
//			  pRows 坏点y坐标
//			  pCols 坏点x坐标
//			  pNumPixel 输入时表示行列缓冲区的大小，返回时表示行列缓冲区中返回的坏点数量。
//			  当pRows或者pCols为NULL时函数会把相机当前的坏点个数通过pNumPixel返回
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraReadDeadPixels(
	CameraHandle    hCamera,
	USHORT*			pRows,
	USHORT*			pCols,
	UINT*			pNumPixel
	);

/******************************************************/
// 函数名   : CameraAddDeadPixels
// 功能描述 : 添加相机坏点
// 参数     : hCamera   相机的句柄，由CameraInit函数获得。
//			  pRows 坏点y坐标
//			  pCols 坏点x坐标
//			  NumPixel 行列缓冲区中的坏点个数
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraAddDeadPixels(
	CameraHandle    hCamera,
	USHORT*			pRows,
	USHORT*			pCols,
	UINT			NumPixel
	);

/******************************************************/
// 函数名   : CameraRemoveDeadPixels
// 功能描述 : 删除相机指定坏点
// 参数     : hCamera   相机的句柄，由CameraInit函数获得。
//			  pRows 坏点y坐标
//			  pCols 坏点x坐标
//			  NumPixel 行列缓冲区中的坏点个数
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraRemoveDeadPixels(
	CameraHandle    hCamera,
	USHORT*			pRows,
	USHORT*			pCols,
	UINT			NumPixel
	);

/******************************************************/
// 函数名   : CameraRemoveAllDeadPixels
// 功能描述 : 删除相机的所有坏点
// 参数     : hCamera   相机的句柄，由CameraInit函数获得。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraRemoveAllDeadPixels(
	CameraHandle    hCamera
	);

/******************************************************/
// 函数名   : CameraSaveDeadPixels
// 功能描述 : 保存相机坏点到相机存储中
// 参数     : hCamera   相机的句柄，由CameraInit函数获得。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraSaveDeadPixels(
	CameraHandle    hCamera
	);

/******************************************************/
// 函数名   : CameraSaveDeadPixelsToFile
// 功能描述 : 保存相机坏点到文件中
// 参数     : hCamera   相机的句柄，由CameraInit函数获得。
//			  sFileName  坏点文件的完整路径。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraSaveDeadPixelsToFile(
	CameraHandle    hCamera,
	char const*		sFileName
	);

/******************************************************/
// 函数名   : CameraLoadDeadPixelsFromFile
// 功能描述 : 从文件加载相机坏点
// 参数     : hCamera   相机的句柄，由CameraInit函数获得。
//			  sFileName  坏点文件的完整路径。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraLoadDeadPixelsFromFile(
	CameraHandle    hCamera,
	char const*		sFileName
	);

/******************************************************/
// 函数名   : CameraGetImageBufferPriority
// 功能描述 : 获得一帧图像数据。为了提高效率，SDK在图像抓取时采用了零拷贝机制，
//        CameraGetImageBuffer实际获得是内核中的一个缓冲区地址，
//        该函数成功调用后，必须调用CameraReleaseImageBuffer释放由
//        CameraGetImageBuffer得到的缓冲区,以便让内核继续使用
//        该缓冲区。
// 参数     : hCamera   相机的句柄，由CameraInit函数获得。
//            pFrameInfo  图像的帧头信息指针。
//            pbyBuffer   指向图像的数据的缓冲区指针。由于
//              采用了零拷贝机制来提高效率，因此
//              这里使用了一个指向指针的指针。
//            wTimes 抓取图像的超时时间。单位毫秒。在
//              wTimes时间内还未获得图像，则该函数
//              会返回超时信息。
//			  Priority 取图优先级 详见：emCameraGetImagePriority
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraGetImageBufferPriority(
	CameraHandle        hCamera,
	tSdkFrameHead*      pFrameInfo,
	BYTE**              pbyBuffer,
	UINT                wTimes,
	UINT				Priority
	);

/******************************************************/
// 函数名 	: CameraGetImageBufferPriorityEx
// 功能描述	: 获得一帧图像数据。该接口获得的图像是经过处理后的RGB格式。该函数调用后，
//			  不需要调用 CameraReleaseImageBuffer 释放，也不要调用free之类的函数释放
//              来释放该函数返回的图像数据缓冲区。
// 参数	    : hCamera	  相机的句柄，由CameraInit函数获得。
//            piWidth    整形指针，返回图像的宽度
//            piHeight   整形指针，返回图像的高度
//            UINT wTimes 抓取图像的超时时间。单位毫秒。在
//						  wTimes时间内还未获得图像，则该函数
//						  会返回超时信息。
//			  Priority   取图优先级 详见：emCameraGetImagePriority
// 返回值   : 成功时，返回RGB数据缓冲区的首地址;
//            否则返回0。
/******************************************************/
MVSDK_API unsigned char* __stdcall CameraGetImageBufferPriorityEx(
	CameraHandle        hCamera,
	INT*                piWidth,
	INT*                piHeight,
	UINT                wTimes,
	UINT				Priority
	);

/******************************************************/
// 函数名 	: CameraGetImageBufferPriorityEx2
// 功能描述	: 获得一帧图像数据。该接口获得的图像是经过处理后的RGB格式。该函数调用后，
//			  不需要调用 CameraReleaseImageBuffer 释放，也不要调用free之类的函数释放
//              来释放该函数返回的图像数据缓冲区。
// 参数	    : hCamera	    相机的句柄，由CameraInit函数获得。
//             pImageData  接收图像数据的缓冲区，大小必须和uOutFormat指定的格式相匹配，否则数据会溢出
//             piWidth     整形指针，返回图像的宽度
//             piHeight    整形指针，返回图像的高度
//             wTimes      抓取图像的超时时间。单位毫秒。在
//						wTimes时间内还未获得图像，则该函数
//						会返回超时信息。
//			  Priority	   取图优先级 详见：emCameraGetImagePriority
// 返回值   : 成功时，返回RGB数据缓冲区的首地址;
//            否则返回0。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraGetImageBufferPriorityEx2(
	CameraHandle    hCamera,
	BYTE*           pImageData,
	UINT            uOutFormat,
	int*            piWidth,
	int*            piHeight,
	UINT            wTimes,
	UINT			Priority
	);

/******************************************************/
// 函数名 	: CameraGetImageBufferPriorityEx3
// 功能描述	: 获得一帧图像数据。该接口获得的图像是经过处理后的RGB格式。该函数调用后，
//			  不需要调用 CameraReleaseImageBuffer 释放.
//              uOutFormat 0 : 8 BIT gray 1:rgb24 2:rgba32 3:bgr24 4:bgra32
// 参数	    : hCamera	    相机的句柄，由CameraInit函数获得。
//             pImageData  接收图像数据的缓冲区，大小必须和uOutFormat指定的格式相匹配，否则数据会溢出
//            piWidth      整形指针，返回图像的宽度
//            piHeight     整形指针，返回图像的高度
//            puTimeStamp  无符号整形，返回图像时间戳
//            UINT wTimes  抓取图像的超时时间。单位毫秒。在
//			  wTimes       时间内还未获得图像，则该函数会返回超时信息。
//			  Priority	   取图优先级 详见：emCameraGetImagePriority
// 返回值   : 成功时，返回RGB数据缓冲区的首地址;
//            否则返回0。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraGetImageBufferPriorityEx3(
	CameraHandle hCamera,
	BYTE*pImageData,
	UINT uOutFormat,
	int *piWidth,
	int *piHeight,
	UINT* puTimeStamp,
	UINT wTimes,
	UINT Priority
	);

/******************************************************/
// 函数名   : CameraClearBuffer
// 功能描述 : 清空相机内已缓存的所有帧
// 参数     : hCamera  相机的句柄，由CameraInit函数获得。
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraClearBuffer(
	CameraHandle hCamera
	);

/******************************************************/
// 函数名   : CameraSoftTriggerEx
// 功能描述 : 执行一次软触发。执行后，会触发由CameraSetTriggerCount
//          指定的帧数。
// 参数     : hCamera  相机的句柄，由CameraInit函数获得。
//			  uFlags 功能标志,详见emCameraSoftTriggerExFlags中的定义
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0);
//            否则返回非0值的错误码,请参考CameraStatus.h
//            中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraSoftTriggerEx(
	CameraHandle hCamera,
	UINT uFlags
	);

/******************************************************/
// 函数名 	: CameraSetHDR
// 功能描述	: 设置相机的HDR，需要相机支持，不带HDR功能的型号，此函数返回错误代码，表示不支持。
// 参数	    : hCamera	   相机的句柄，由CameraInit函数获得。
//            value		   HDR系数，范围0.0到1.0
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0)，表示相机连接状态正常;
//            否则返回 非0值，参考CameraStatus.h中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraSetHDR(
	CameraHandle    hCamera,
	float           value
	);

/******************************************************/
// 函数名 	: CameraGetHDR
// 功能描述	: 获取相机的HDR，需要相机支持，不带HDR功能的型号，此函数返回错误代码，表示不支持。
// 参数	    : hCamera	   相机的句柄，由CameraInit函数获得。
//            value		   HDR系数，范围0.0到1.0
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0)，表示相机连接状态正常;
//            否则返回 非0值，参考CameraStatus.h中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraGetHDR(
	CameraHandle    hCamera,
	float*          value
	);

/******************************************************/
// 函数名 	: CameraGetFrameID
// 功能描述	: 获取当前帧的ID，需相机支持(网口全系列支持)，此函数返回错误代码，表示不支持。
// 参数	    : hCamera	   相机的句柄，由CameraInit函数获得。
//            id		   帧ID
// 返回值   : 成功时，返回CAMERA_STATUS_SUCCESS (0)，表示相机连接状态正常;
//            否则返回 非0值，参考CameraStatus.h中错误码的定义。
/******************************************************/
MVSDK_API CameraSdkStatus __stdcall CameraGetFrameID(
	CameraHandle    hCamera,
	UINT*           id
	);

#endif
