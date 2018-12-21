#pragma once
#ifndef _CAMERA_DEFINE_H_
#define _CAMERA_DEFINE_H_

#include "CameraStatus.h"

#define MAX_CROSS_LINE 9

//Ïà»úµÄ¾ä±úÀàÐÍ¶¨Òå
typedef int CameraHandle;
typedef int INT;
typedef long LONG;
typedef unsigned int UINT;
typedef unsigned long long UINT64;
typedef int BOOL;
typedef unsigned char BYTE;
typedef unsigned int DWORD;
typedef void* PVOID;
typedef void* HWND;
typedef char* LPCTSTR;
typedef unsigned short USHORT;
typedef short SHORT;
 typedef unsigned char* LPBYTE;
typedef char CHAR;
typedef char TCHAR;
typedef  unsigned short WORD;
typedef INT HANDLE;
typedef void VOID;
typedef unsigned long ULONG;
typedef void** LPVOID;
typedef unsigned char UCHAR;
typedef void* HMODULE;

#define TRUE 1
#define FALSE 0
//Í¼Ïñ²é±í±ä»»µÄ·½Ê½
typedef enum
{
    LUTMODE_PARAM_GEN=0,//Í¨¹ýµ÷½Ú²ÎÊý¶¯Ì¬Éú³ÉLUT±í
    LUTMODE_PRESET,     //Ê¹ÓÃÔ¤ÉèµÄLUT±í
    LUTMODE_USER_DEF    //Ê¹ÓÃÓÃ»§×Ô¶¨ÒåµÄLUT±í
}emSdkLutMode;

//Ïà»úµÄÊÓÆµÁ÷¿ØÖÆ
typedef enum
{
    RUNMODE_PLAY=0,    //Õý³£Ô¤ÀÀ£¬²¶»ñµ½Í¼Ïñ¾ÍÏÔÊ¾¡££¨Èç¹ûÏà»ú´¦ÓÚ´¥·¢Ä£Ê½£¬Ôò»áµÈ´ý´¥·¢Ö¡µÄµ½À´£©
    RUNMODE_PAUSE,     //ÔÝÍ££¬»áÔÝÍ£Ïà»úµÄÍ¼ÏñÊä³ö£¬Í¬Ê±Ò²²»»áÈ¥²¶»ñÍ¼Ïñ
    RUNMODE_STOP       //Í£Ö¹Ïà»ú¹¤×÷¡£·´³õÊ¼»¯ºó£¬Ïà»ú¾Í´¦ÓÚÍ£Ö¹Ä£Ê½
}emSdkRunMode;

//SDKÄÚ²¿ÏÔÊ¾½Ó¿ÚµÄÏÔÊ¾·½Ê½
typedef enum
{
    DISPLAYMODE_SCALE=0, //Ëõ·ÅÏÔÊ¾Ä£Ê½£¬Ëõ·Åµ½ÏÔÊ¾¿Ø¼þµÄ³ß´ç
    DISPLAYMODE_REAL     //1:1ÏÔÊ¾Ä£Ê½£¬µ±Í¼Ïñ³ß´ç´óÓÚÏÔÊ¾¿Ø¼þµÄ³ß´çÊ±£¬Ö»ÏÔÊ¾¾Ö²¿
}emSdkDisplayMode;

//Â¼Ïñ×´Ì¬
typedef enum
{
  RECORD_STOP = 0,  //Í£Ö¹
  RECORD_START,     //Â¼ÏñÖÐ
  RECORD_PAUSE      //ÔÝÍ£
}emSdkRecordMode;

//Í¼ÏñµÄ¾µÏñ²Ù×÷
typedef enum
{
    MIRROR_DIRECTION_HORIZONTAL = 0,//Ë®Æ½¾µÏñ
    MIRROR_DIRECTION_VERTICAL       //´¹Ö±¾µÏñ
}emSdkMirrorDirection;

//Ïà»úÊÓÆµµÄÖ¡Â
typedef enum
{
    FRAME_SPEED_LOW = 0,  //µÍËÙÄ£Ê½
    FRAME_SPEED_NORMAL,   //ÆÕÍ¨Ä£Ê½
    FRAME_SPEED_HIGH,     //¸ßËÙÄ£Ê½(ÐèÒª½Ï¸ßµÄ´«Êä´ø¿í,¶àÉè±¸¹²Ïí´«Êä´ø¿íÊ±»á¶ÔÖ¡ÂÊµÄÎÈ¶¨ÐÔÓÐÓ°Ïì)
    FRAME_SPEED_SUPER     //³¬¸ßËÙÄ£Ê½(ÐèÒª½Ï¸ßµÄ´«Êä´ø¿í,¶àÉè±¸¹²Ïí´«Êä´ø¿íÊ±»á¶ÔÖ¡ÂÊµÄÎÈ¶¨ÐÔÓÐÓ°Ïì)
}emSdkFrameSpeed;

//±£´æÎÄ¼þµÄ¸ñÊ½ÀàÐÍ
typedef enum
{
    FILE_JPG = 1,//JPG
    FILE_BMP = 2,//BMP
    FILE_RAW = 4,//Ïà»úÊä³öµÄbayer¸ñÊ½ÎÄ¼þ,¶ÔÓÚ²»Ö§³Öbayer¸ñÊ½Êä³öÏà»ú£¬ÎÞ·¨±£´æÎª¸Ã¸ñÊ½
    FILE_PNG = 8, //PNG
    FILE_BMP_8BIT = 16,//BMP 8bit
}emSdkFileType;

//Ïà»úÖÐµÄÍ¼Ïñ´«¸ÐÆ÷µÄ¹¤×÷Ä£Ê½
typedef enum
{
    CONTINUATION = 0,//Á¬Ðø²É¼¯Ä£Ê½
    SOFT_TRIGGER,    //Èí¼þ´¥·¢Ä£Ê½£¬ÓÉÈí¼þ·¢ËÍÖ¸Áîºó£¬´«¸ÐÆ÷¿ªÊ¼²É¼¯Ö¸¶¨Ö¡ÊýµÄÍ¼Ïñ£¬²É¼¯Íê³Éºó£¬Í£Ö¹Êä³ö
    EXTERNAL_TRIGGER //Ó²¼þ´¥·¢Ä£Ê½£¬µ±½ÓÊÕµ½Íâ²¿ÐÅºÅ£¬´«¸ÐÆ÷¿ªÊ¼²É¼¯Ö¸¶¨Ö¡ÊýµÄÍ¼Ïñ£¬²É¼¯Íê³Éºó£¬Í£Ö¹Êä³ö
} emSdkSnapMode;

//×Ô¶¯ÆØ¹âÊ±¿¹ÆµÉÁµÄÆµÉÁ
typedef enum
{
    LIGHT_FREQUENCY_50HZ = 0,//50HZ,Ò»°ãµÄµÆ¹â¶¼ÊÇ50HZ
    LIGHT_FREQUENCY_60HZ     //60HZ,Ö÷ÒªÊÇÖ¸ÏÔÊ¾Æ÷µÄ
}emSdkLightFrequency;

//Ïà»úµÄÅäÖÃ²ÎÊý£¬·ÖÎªA,B,C,D 4×é½øÐÐ±£´æ¡£
typedef enum
{
    PARAMETER_TEAM_DEFAULT = 0xff,
    PARAMETER_TEAM_A = 0,
    PARAMETER_TEAM_B = 1,
    PARAMETER_TEAM_C = 2,
    PARAMETER_TEAM_D = 3
}emSdkParameterTeam;


/*emSdkParameterMode Ïà»ú²ÎÊý¼ÓÔØÄ£Ê½£¬²ÎÊý¼ÓÔØ·ÖÎª´ÓÎÄ¼þºÍ´ÓÉè±¸¼ÓÔØÁ½ÖÖ·½Ê½

PARAM_MODE_BY_MODEL:ËùÓÐÍ¬ÐÍºÅµÄÏà»ú¹²ÓÃABCDËÄ×é²ÎÊýÎÄ¼þ¡£ÐÞ¸Ä
             Ò»Ì¨Ïà»úµÄ²ÎÊýÎÄ¼þ£¬»áÓ°Ïìµ½Õû¸öÍ¬ÐÍºÅµÄ
             Ïà»ú²ÎÊý¼ÓÔØ¡£

PARAM_MODE_BY_NAME:ËùÓÐÉè±¸ÃûÏàÍ¬µÄÏà»ú£¬¹²ÓÃABCDËÄ×é²ÎÊýÎÄ¼þ¡£
         Ä¬ÈÏÇé¿öÏÂ£¬µ±µçÄÔÉÏÖ»½ÓÁËÄ³ÐÍºÅÒ»Ì¨Ïà»úÊ±£¬
         Éè±¸Ãû¶¼ÊÇÒ»ÑùµÄ£¬¶øÄúÏ£ÍûÄ³Ò»Ì¨Ïà»úÄÜ¹»¼ÓÔØ
         ²»Í¬µÄ²ÎÊýÎÄ¼þ£¬Ôò¿ÉÒÔÍ¨¹ýÐÞ¸ÄÆäÉè±¸ÃûµÄ·½Ê½
         À´ÈÃÆä¼ÓÔØÖ¸¶¨µÄ²ÎÊýÎÄ¼þ¡£

PARAM_MODE_BY_SN:Ïà»ú°´ÕÕ×Ô¼ºµÄÎ¨Ò»ÐòÁÐºÅÀ´¼ÓÔØABCDËÄ×é²ÎÊýÎÄ¼þ£¬
         ÐòÁÐºÅÔÚ³ö³§Ê±ÒÑ¾­¹Ì»¯ÔÚÏà»úÄÚ£¬Ã¿Ì¨Ïà»úµÄÐòÁÐºÅ
         ¶¼²»ÏàÍ¬£¬Í¨¹ýÕâÖÖ·½Ê½£¬Ã¿Ì¨Ïà»úµÄ²ÎÊýÎÄ¼þ¶¼ÊÇ¶ÀÁ¢µÄ¡£

Äú¿ÉÒÔ¸ù¾Ý×Ô¼ºµÄÊ¹ÓÃ»·¾³£¬Áé»îÊ¹ÓÃÒÔÉÏ¼¸ÖÖ·½Ê½¼ÓÔØ²ÎÊý¡£ÀýÈç£¬ÒÔ
MV-U300ÎªÀý£¬ÄúÏ£Íû¶àÌ¨¸ÃÐÍºÅµÄÏà»úÔÚÄúµÄ µçÄÔÉÏ¶¼¹²ÓÃ4×é²ÎÊý£¬ÄÇÃ´¾Í
Ê¹ÓÃPARAM_MODE_BY_MODEL·½Ê½;Èç¹ûÄúÏ£ÍûÆäÖÐÄ³Ò»Ì¨»òÕßÄ³¼¸Ì¨MV-U300ÄÜ
Ê¹ÓÃ×Ô¼º²ÎÊýÎÄ¼þ¶øÆäÓàµÄMV-U300ÓÖÒªÊ¹ÓÃÏàÍ¬µÄ²ÎÊýÎÄ¼þ£¬ÄÇÃ´Ê¹ÓÃ
PARAM_MODE_BY_NAME·½Ê½;Èç¹ûÄúÏ£ÍûÃ¿Ì¨MV-U300¶¼Ê¹ÓÃ²»Í¬µÄ²ÎÊýÎÄ¼þ£¬ÄÇÃ´
Ê¹ÓÃPARAM_MODE_BY_SN·½Ê½¡£
²ÎÊýÎÄ¼þ´æÔÚ°²×°Ä¿Â¼µÄ \Camera\Configs Ä¿Â¼ÏÂ£¬ÒÔconfigÎªºó×ºÃûµÄÎÄ¼þ¡£
*/
typedef enum
{
  PARAM_MODE_BY_MODEL = 0,  //¸ù¾ÝÏà»úÐÍºÅÃû´ÓÎÄ¼þÖÐ¼ÓÔØ²ÎÊý£¬ÀýÈçMV-U300
  PARAM_MODE_BY_NAME,       //¸ù¾ÝÉè±¸êÇ³Æ(tSdkCameraDevInfo.acFriendlyName)´ÓÎÄ¼þÖÐ¼ÓÔØ²ÎÊý£¬ÀýÈçMV-U300,¸ÃêÇ³Æ¿É×Ô¶¨Òå
  PARAM_MODE_BY_SN,         //¸ù¾ÝÉè±¸µÄÎ¨Ò»ÐòÁÐºÅ´ÓÎÄ¼þÖÐ¼ÓÔØ²ÎÊý£¬ÐòÁÐºÅÔÚ³ö³§Ê±ÒÑ¾­Ð´ÈëÉè±¸£¬Ã¿Ì¨Ïà»úÓµÓÐ²»Í¬µÄÐòÁÐºÅ¡£
  PARAM_MODE_IN_DEVICE      //´ÓÉè±¸µÄ¹ÌÌ¬´æ´¢Æ÷ÖÐ¼ÓÔØ²ÎÊý¡£²»ÊÇËùÓÐµÄÐÍºÅ¶¼Ö§³Ö´ÓÏà»úÖÐ¶ÁÐ´²ÎÊý×é£¬ÓÉtSdkCameraCapbility.bParamInDevice¾ö¶¨
}emSdkParameterMode;


//SDKÉú³ÉµÄÏà»úÅäÖÃÒ³ÃæÑÚÂëÖµ
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
}emSdkPropSheetMask;

//SDKÉú³ÉµÄÏà»úÅäÖÃÒ³ÃæµÄ»Øµ÷ÏûÏ¢ÀàÐÍ
typedef enum
{
  SHEET_MSG_LOAD_PARAM_DEFAULT = 0, //²ÎÊý±»»Ö¸´³ÉÄ¬ÈÏºó£¬´¥·¢¸ÃÏûÏ¢
  SHEET_MSG_LOAD_PARAM_GROUP,       //¼ÓÔØÖ¸¶¨²ÎÊý×é£¬´¥·¢¸ÃÏûÏ¢
  SHEET_MSG_LOAD_PARAM_FROMFILE,    //´ÓÖ¸¶¨ÎÄ¼þ¼ÓÔØ²ÎÊýºó£¬´¥·¢¸ÃÏûÏ¢
  SHEET_MSG_SAVE_PARAM_GROUP        //µ±Ç°²ÎÊý×é±»±£´æÊ±£¬´¥·¢¸ÃÏûÏ¢
}emSdkPropSheetMsg;

//¿ÉÊÓ»¯Ñ¡Ôñ²Î¿¼´°¿ÚµÄÀàÐÍ
typedef enum
{
  REF_WIN_AUTO_EXPOSURE = 0,
  REF_WIN_WHITE_BALANCE,
}emSdkRefWinType;

//¿ÉÊÓ»¯Ñ¡Ôñ²Î¿¼´°¿ÚµÄÀàÐÍ
typedef enum
{
  RES_MODE_PREVIEW = 0,
  RES_MODE_SNAPSHOT,
}emSdkResolutionMode;

//°×Æ½ºâÊ±É«ÎÂÄ£Ê½
typedef enum
{
  CT_MODE_AUTO = 0, //×Ô¶¯Ê¶±ðÉ«ÎÂ
  CT_MODE_PRESET,   //Ê¹ÓÃÖ¸¶¨µÄÔ¤ÉèÉ«ÎÂ
  CT_MODE_USER_DEF  //×Ô¶¨ÒåÉ«ÎÂ(ÔöÒæºÍ¾ØÕó)
}emSdkClrTmpMode;

//LUTµÄÑÕÉ«Í¨µÀ
typedef enum
{
  LUT_CHANNEL_ALL = 0,//R,B,GÈýÍ¨µÀÍ¬Ê±µ÷½Ú
  LUT_CHANNEL_RED,    //ºìÉ«Í¨µÀ
  LUT_CHANNEL_GREEN,  //ÂÌÉ«Í¨µÀ
  LUT_CHANNEL_BLUE,   //À¶É«Í¨µÀ
}emSdkLutChannel;

//ISP´¦Àíµ¥Ôª
typedef enum
{
  ISP_PROCESSSOR_PC = 0,//Ê¹ÓÃPCµÄÈí¼þISPÄ£¿é
  ISP_PROCESSSOR_DEVICE //Ê¹ÓÃÏà»ú×Ô´øµÄÓ²¼þISPÄ£¿é
}emSdkIspProcessor;

//ÉÁ¹âµÆÐÅºÅ¿ØÖÆ·½Ê½
typedef enum
{
  STROBE_SYNC_WITH_TRIG_AUTO = 0,    //ºÍ´¥·¢ÐÅºÅÍ¬²½£¬´¥·¢ºó£¬Ïà»ú½øÐÐÆØ¹âÊ±£¬×Ô¶¯Éú³ÉSTROBEÐÅºÅ¡£´ËÊ±£¬ÓÐÐ§¼«ÐÔ¿ÉÉèÖÃ(CameraSetStrobePolarity)¡£
  STROBE_SYNC_WITH_TRIG_MANUAL,      //ºÍ´¥·¢ÐÅºÅÍ¬²½£¬´¥·¢ºó£¬STROBEÑÓÊ±Ö¸¶¨µÄÊ±¼äºó(CameraSetStrobeDelayTime)£¬ÔÙ³ÖÐøÖ¸¶¨Ê±¼äµÄÂö³å(CameraSetStrobePulseWidth)£¬ÓÐÐ§¼«ÐÔ¿ÉÉèÖÃ(CameraSetStrobePolarity)¡£
  STROBE_ALWAYS_HIGH,                //Ê¼ÖÕÎª¸ß£¬ºöÂÔSTROBEÐÅºÅµÄÆäËûÉèÖÃ
  STROBE_ALWAYS_LOW                  //Ê¼ÖÕÎªµÍ£¬ºöÂÔSTROBEÐÅºÅµÄÆäËûÉèÖÃ
}emStrobeControl;

//Ó²¼þÍâ´¥·¢µÄÐÅºÅÖÖÀà
typedef enum
{
  EXT_TRIG_LEADING_EDGE = 0,     //ÉÏÉýÑØ´¥·¢£¬Ä¬ÈÏÎª¸Ã·½Ê½
  EXT_TRIG_TRAILING_EDGE,        //ÏÂ½µÑØ´¥·¢
  EXT_TRIG_HIGH_LEVEL,           //¸ßµçÆ½´¥·¢,µçÆ½¿í¶È¾ö¶¨ÆØ¹âÊ±¼ä£¬½ö²¿·ÖÐÍºÅµÄÏà»úÖ§³ÖµçÆ½´¥·¢·½Ê½¡£
  EXT_TRIG_LOW_LEVEL             //µÍµçÆ½´¥·¢,
}emExtTrigSignal;

//Ó²¼þÍâ´¥·¢Ê±µÄ¿ìÃÅ·½Ê½
typedef enum
{
  EXT_TRIG_EXP_STANDARD = 0,     //±ê×¼·½Ê½£¬Ä¬ÈÏÎª¸Ã·½Ê½¡£
  EXT_TRIG_EXP_GRR,              //È«¾Ö¸´Î»·½Ê½£¬²¿·Ö¹ö¶¯¿ìÃÅµÄCMOSÐÍºÅµÄÏà»úÖ§³Ö¸Ã·½Ê½£¬ÅäºÏÍâ²¿»úÐµ¿ìÃÅ£¬¿ÉÒÔ´ïµ½È«¾Ö¿ìÃÅµÄÐ§¹û£¬ÊÊºÏÅÄ¸ßËÙÔË¶¯µÄÎïÌå
}emExtTrigShutterMode;

// GPIOÄ£Ê½
typedef enum
{
	IOMODE_TRIG_INPUT = 0,		//´¥·¢ÊäÈë
	IOMODE_STROBE_OUTPUT,		//ÉÁ¹âµÆÊä³ö
	IOMODE_GP_INPUT,			//Í¨ÓÃÐÍÊäÈë
	IOMODE_GP_OUTPUT,			//Í¨ÓÃÐÍÊä³ö
	IOMODE_PWM_OUTPUT,			//PWMÐÍÊä³ö
}emCameraGPIOMode;

//Ïà»úµÄÉè±¸ÐÅÏ¢
typedef struct
{
    char acProductSeries[32];   // ²úÆ·ÏµÁÐ
    char acProductName[32];     // ²úÆ·Ãû³Æ
    char acFriendlyName[32];    // ²úÆ·êÇ³Æ£¬ÓÃ»§¿É×Ô¶¨Òå¸ÄêÇ³Æ£¬±£´æÔÚÏà»úÄÚ£¬ÓÃÓÚÇø·Ö¶à¸öÏà»úÍ¬Ê±Ê¹ÓÃ,¿ÉÒÔÓÃCameraSetFriendlyName½Ó¿Ú¸Ä±ä¸ÃêÇ³Æ£¬Éè±¸ÖØÆôºóÉúÐ§¡£
    char acLinkName[32];        // ÄÚºË·ûºÅÁ¬½ÓÃû£¬ÄÚ²¿Ê¹ÓÃ
    char acDriverVersion[32];   // Çý¶¯°æ±¾
    char acSensorType[32];      // sensorÀàÐÍ
    char acPortType[32];        // ½Ó¿ÚÀàÐÍ
    char acSn[32];              // ²úÆ·Î¨Ò»ÐòÁÐºÅ
    UINT uInstance;             // ¸ÃÐÍºÅÏà»úÔÚ¸ÃµçÄÔÉÏµÄÊµÀýË÷ÒýºÅ£¬ÓÃÓÚÇø·ÖÍ¬ÐÍºÅ¶àÏà»ú
} tSdkCameraDevInfo;

//tSdkResolutionRange½á¹¹ÌåÖÐSKIP¡¢ BIN¡¢RESAMPLEÄ£Ê½µÄÑÚÂëÖµ
#define MASK_2X2_HD     (1<<0)    //Ó²¼þSKIP¡¢BIN¡¢ÖØ²ÉÑù 2X2
#define MASK_3X3_HD     (1<<1)
#define MASK_4X4_HD     (1<<2)
#define MASK_5X5_HD     (1<<3)
#define MASK_6X6_HD     (1<<4)
#define MASK_7X7_HD     (1<<5)
#define MASK_8X8_HD     (1<<6)
#define MASK_9X9_HD     (1<<7)
#define MASK_10X10_HD   (1<<8)
#define MASK_11X11_HD   (1<<9)
#define MASK_12X12_HD   (1<<10)
#define MASK_13X13_HD   (1<<11)
#define MASK_14X14_HD   (1<<12)
#define MASK_15X15_HD   (1<<13)
#define MASK_16X16_HD   (1<<14)
#define MASK_17X17_HD   (1<<15)
#define MASK_2X2_SW     (1<<16)   //Ó²¼þSKIP¡¢BIN¡¢ÖØ²ÉÑù 2X2
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

//Ïà»úµÄ·Ö±æÂÊÉè¶¨·¶Î§£¬ÓÃÓÚ¹¹¼þUI
typedef struct
{
  INT iHeightMax;             //Í¼Ïñ×î´ó¸ß¶È
  INT iHeightMin;             //Í¼Ïñ×îÐ¡¸ß¶È
  INT iWidthMax;              //Í¼Ïñ×î´ó¿í¶È
  INT iWidthMin;              //Í¼Ïñ×îÐ¡¿í¶È
  UINT uSkipModeMask;         //SKIPÄ£Ê½ÑÚÂë£¬Îª0£¬±íÊ¾²»Ö§³ÖSKIP ¡£bit0Îª1,±íÊ¾Ö§³ÖSKIP 2x2 ;bit1Îª1£¬±íÊ¾Ö§³ÖSKIP 3x3....
  UINT uBinSumModeMask;       //BIN(ÇóºÍ)Ä£Ê½ÑÚÂë£¬Îª0£¬±íÊ¾²»Ö§³ÖBIN ¡£bit0Îª1,±íÊ¾Ö§³ÖBIN 2x2 ;bit1Îª1£¬±íÊ¾Ö§³ÖBIN 3x3....
  UINT uBinAverageModeMask;   //BIN(Çó¾ùÖµ)Ä£Ê½ÑÚÂë£¬Îª0£¬±íÊ¾²»Ö§³ÖBIN ¡£bit0Îª1,±íÊ¾Ö§³ÖBIN 2x2 ;bit1Îª1£¬±íÊ¾Ö§³ÖBIN 3x3....
  UINT uResampleMask;         //Ó²¼þÖØ²ÉÑùµÄÑÚÂë
} tSdkResolutionRange;


//Ïà»úµÄ·Ö±æÂÊÃèÊö
typedef struct
{
  INT     iIndex;             // Ë÷ÒýºÅ£¬[0,N]±íÊ¾Ô¤ÉèµÄ·Ö±æÂÊ(N ÎªÔ¤Éè·Ö±æÂÊµÄ×î´ó¸öÊý£¬Ò»°ã²»³¬¹ý20),OXFF ±íÊ¾×Ô¶¨Òå·Ö±æÂÊ(ROI)
  char    acDescription[32];  // ¸Ã·Ö±æÂÊµÄÃèÊöÐÅÏ¢¡£½öÔ¤Éè·Ö±æÂÊÊ±¸ÃÐÅÏ¢ÓÐÐ§¡£×Ô¶¨Òå·Ö±æÂÊ¿ÉºöÂÔ¸ÃÐÅÏ¢
  UINT    uBinSumMode;        // BIN(ÇóºÍ)µÄÄ£Ê½,·¶Î§²»ÄÜ³¬¹ýtSdkResolutionRangeÖÐuBinSumModeMask
  UINT    uBinAverageMode;    // BIN(Çó¾ùÖµ)µÄÄ£Ê½,·¶Î§²»ÄÜ³¬¹ýtSdkResolutionRangeÖÐuBinAverageModeMask
  UINT    uSkipMode;          // ÊÇ·ñSKIPµÄ³ß´ç£¬Îª0±íÊ¾½ûÖ¹SKIPÄ£Ê½£¬·¶Î§²»ÄÜ³¬¹ýtSdkResolutionRangeÖÐuSkipModeMask
  UINT    uResampleMask;      // Ó²¼þÖØ²ÉÑùµÄÑÚÂë
  INT     iHOffsetFOV;        // ²É¼¯ÊÓ³¡Ïà¶ÔÓÚSensor×î´óÊÓ³¡×óÉÏ½ÇµÄ´¹Ö±Æ«ÒÆ
  INT     iVOffsetFOV;        // ²É¼¯ÊÓ³¡Ïà¶ÔÓÚSensor×î´óÊÓ³¡×óÉÏ½ÇµÄË®Æ½Æ«ÒÆ
  INT     iWidthFOV;          // ²É¼¯ÊÓ³¡µÄ¿í¶È
  INT     iHeightFOV;         // ²É¼¯ÊÓ³¡µÄ¸ß¶È
  INT     iWidth;             // Ïà»ú×îÖÕÊä³öµÄÍ¼ÏñµÄ¿í¶È
  INT     iHeight;            // Ïà»ú×îÖÕÊä³öµÄÍ¼ÏñµÄ¸ß¶È
  INT     iWidthZoomHd;       // Ó²¼þËõ·ÅµÄ¿í¶È,²»ÐèÒª½øÐÐ´Ë²Ù×÷µÄ·Ö±æÂÊ£¬´Ë±äÁ¿ÉèÖÃÎª0.
  INT     iHeightZoomHd;      // Ó²¼þËõ·ÅµÄ¸ß¶È,²»ÐèÒª½øÐÐ´Ë²Ù×÷µÄ·Ö±æÂÊ£¬´Ë±äÁ¿ÉèÖÃÎª0.
  INT     iWidthZoomSw;       // Èí¼þËõ·ÅµÄ¿í¶È,²»ÐèÒª½øÐÐ´Ë²Ù×÷µÄ·Ö±æÂÊ£¬´Ë±äÁ¿ÉèÖÃÎª0.
  INT     iHeightZoomSw;      // Èí¼þËõ·ÅµÄ¸ß¶È,²»ÐèÒª½øÐÐ´Ë²Ù×÷µÄ·Ö±æÂÊ£¬´Ë±äÁ¿ÉèÖÃÎª0.
} tSdkImageResolution;

//Ïà»ú°×Æ½ºâÉ«ÎÂÄ£Ê½ÃèÊöÐÅÏ¢
typedef struct
{
    INT  iIndex;            // Ä£Ê½Ë÷ÒýºÅ
    char acDescription[32]; // ÃèÊöÐÅÏ¢
} tSdkColorTemperatureDes;

//Ïà»úÖ¡ÂÊÃèÊöÐÅÏ¢
typedef struct
{
    INT  iIndex;             // Ö¡ÂÊË÷ÒýºÅ£¬Ò»°ã0¶ÔÓ¦ÓÚµÍËÙÄ£Ê½£¬1¶ÔÓ¦ÓÚÆÕÍ¨Ä£Ê½£¬2¶ÔÓ¦ÓÚ¸ßËÙÄ£Ê½
    char acDescription[32];  // ÃèÊöÐÅÏ¢
} tSdkFrameSpeed;

//Ïà»úÆØ¹â¹¦ÄÜ·¶Î§¶¨Òå
typedef struct
{
    UINT  uiTargetMin;      //×Ô¶¯ÆØ¹âÁÁ¶ÈÄ¿±ê×îÐ¡Öµ
    UINT  uiTargetMax;      //×Ô¶¯ÆØ¹âÁÁ¶ÈÄ¿±ê×î´óÖµ
    UINT  uiAnalogGainMin;  //Ä£ÄâÔöÒæµÄ×îÐ¡Öµ£¬µ¥Î»ÎªfAnalogGainStepÖÐ¶¨Òå
    UINT  uiAnalogGainMax;  //Ä£ÄâÔöÒæµÄ×î´óÖµ£¬µ¥Î»ÎªfAnalogGainStepÖÐ¶¨Òå
    float fAnalogGainStep;  //Ä£ÄâÔöÒæÃ¿Ôö¼Ó1£¬¶ÔÓ¦µÄÔö¼ÓµÄ·Å´ó±¶Êý¡£ÀýÈç£¬uiAnalogGainMinÒ»°ãÎª16£¬fAnalogGainStepÒ»°ãÎª0.125£¬ÄÇÃ´×îÐ¡·Å´ó±¶Êý¾ÍÊÇ16*0.125 = 2±¶
    UINT  uiExposeTimeMin;  //ÊÖ¶¯Ä£Ê½ÏÂ£¬ÆØ¹âÊ±¼äµÄ×îÐ¡Öµ£¬µ¥Î»:ÐÐ¡£¸ù¾ÝCameraGetExposureLineTime¿ÉÒÔ»ñµÃÒ»ÐÐ¶ÔÓ¦µÄÊ±¼ä(Î¢Ãë),´Ó¶øµÃµ½ÕûÖ¡µÄÆØ¹âÊ±¼ä
    UINT  uiExposeTimeMax;  //ÊÖ¶¯Ä£Ê½ÏÂ£¬ÆØ¹âÊ±¼äµÄ×î´óÖµ£¬µ¥Î»:ÐÐ
} tSdkExpose;

//´¥·¢Ä£Ê½ÃèÊö
typedef struct
{
  INT   iIndex;            //Ä£Ê½Ë÷ÒýºÅ
  char  acDescription[32]; //¸ÃÄ£Ê½µÄÃèÊöÐÅÏ¢
} tSdkTrigger;

//´«Êä·Ö°ü´óÐ¡ÃèÊö(Ö÷ÒªÊÇÕë¶ÔÍøÂçÏà»úÓÐÐ§)
typedef struct
{
    INT  iIndex;              //·Ö°ü´óÐ¡Ë÷ÒýºÅ
    char acDescription[32];   //¶ÔÓ¦µÄÃèÊöÐÅÏ¢
    UINT iPackSize;
} tSdkPackLength;

//Ô¤ÉèµÄLUT±íÃèÊö
typedef struct
{
    INT  iIndex;                //±àºÅ
    char acDescription[32];     //ÃèÊöÐÅÏ¢
} tSdkPresetLut;

//AEËã·¨ÃèÊö
typedef struct
{
    INT  iIndex;                //±àºÅ
    char acDescription[32];     //ÃèÊöÐÅÏ¢
} tSdkAeAlgorithm;

//RAW×ªRGBËã·¨ÃèÊö
typedef struct
{
    INT  iIndex;                //±àºÅ
    char acDescription[32];     //ÃèÊöÐÅÏ¢
} tSdkBayerDecodeAlgorithm;


//Ö¡ÂÊÍ³¼ÆÐÅÏ¢
typedef struct
{
  INT iTotal;           //µ±Ç°²É¼¯µÄ×ÜÖ¡Êý£¨°üÀ¨´íÎóÖ¡£©
    INT iCapture;       //µ±Ç°²É¼¯µÄÓÐÐ§Ö¡µÄÊýÁ¿
    INT iLost;          //µ±Ç°¶ªÖ¡µÄÊýÁ¿
} tSdkFrameStatistic;

//Ïà»úÊä³öµÄÍ¼ÏñÊý¾Ý¸ñÊ½
typedef struct
{
  INT     iIndex;             //¸ñÊ½ÖÖÀà±àºÅ
  char    acDescription[32];  //ÃèÊöÐÅÏ¢
  UINT    iMediaType;         //¶ÔÓ¦µÄÍ¼Ïñ¸ñÊ½±àÂë£¬ÈçCAMERA_MEDIA_TYPE_BAYGR8£¬ÔÚ±¾ÎÄ¼þÖÐÓÐ¶¨Òå¡£
} tSdkMediaType;

//Ù¤ÂíµÄÉè¶¨·¶Î§
typedef struct
{
  INT iMin;       //×îÐ¡Öµ
  INT iMax;       //×î´óÖµ
} tGammaRange;

//¶Ô±È¶ÈµÄÉè¶¨·¶Î§
typedef struct
{
    INT iMin;   //×îÐ¡Öµ
    INT iMax;   //×î´óÖµ
} tContrastRange;

//RGBÈýÍ¨µÀÊý×ÖÔöÒæµÄÉè¶¨·¶Î§
typedef struct
{
    INT iRGainMin;    //ºìÉ«ÔöÒæµÄ×îÐ¡Öµ
    INT iRGainMax;    //ºìÉ«ÔöÒæµÄ×î´óÖµ
    INT iGGainMin;    //ÂÌÉ«ÔöÒæµÄ×îÐ¡Öµ
    INT iGGainMax;    //ÂÌÉ«ÔöÒæµÄ×î´óÖµ
    INT iBGainMin;    //À¶É«ÔöÒæµÄ×îÐ¡Öµ
    INT iBGainMax;    //À¶É«ÔöÒæµÄ×î´óÖµ
} tRgbGainRange;

//±¥ºÍ¶ÈÉè¶¨µÄ·¶Î§
typedef struct
{
    INT iMin;   //×îÐ¡Öµ
    INT iMax;   //×î´óÖµ
} tSaturationRange;

//Èñ»¯µÄÉè¶¨·¶Î§
typedef struct
{
  INT iMin;   //×îÐ¡Öµ
  INT iMax;   //×î´óÖµ
} tSharpnessRange;

//ISPÄ£¿éµÄÊ¹ÄÜÐÅÏ¢
typedef struct
{
    BOOL bMonoSensor;       //±íÊ¾¸ÃÐÍºÅÏà»úÊÇ·ñÎªºÚ°×Ïà»ú,Èç¹ûÊÇºÚ°×Ïà»ú£¬ÔòÑÕÉ«Ïà¹ØµÄ¹¦ÄÜ¶¼ÎÞ·¨µ÷½Ú
    BOOL bWbOnce;           //±íÊ¾¸ÃÐÍºÅÏà»úÊÇ·ñÖ§³ÖÊÖ¶¯°×Æ½ºâ¹¦ÄÜ
    BOOL bAutoWb;           //±íÊ¾¸ÃÐÍºÅÏà»úÊÇ·ñÖ§³Ö×Ô¶¯°×Æ½ºâ¹¦ÄÜ
    BOOL bAutoExposure;     //±íÊ¾¸ÃÐÍºÅÏà»úÊÇ·ñÖ§³Ö×Ô¶¯ÆØ¹â¹¦ÄÜ
    BOOL bManualExposure;   //±íÊ¾¸ÃÐÍºÅÏà»úÊÇ·ñÖ§³ÖÊÖ¶¯ÆØ¹â¹¦ÄÜ
    BOOL bAntiFlick;        //±íÊ¾¸ÃÐÍºÅÏà»úÊÇ·ñÖ§³Ö¿¹ÆµÉÁ¹¦ÄÜ
    BOOL bDeviceIsp;        //±íÊ¾¸ÃÐÍºÅÏà»úÊÇ·ñÖ§³ÖÓ²¼þISP¹¦ÄÜ
    BOOL bForceUseDeviceIsp;//bDeviceIspºÍbForceUseDeviceIspÍ¬Ê±ÎªTRUEÊ±£¬±íÊ¾Ç¿ÖÆÖ»ÓÃÓ²¼þISP£¬²»¿ÉÈ¡Ïû¡£
    BOOL bZoomHD;           //Ïà»úÓ²¼þÊÇ·ñÖ§³ÖÍ¼ÏñËõ·ÅÊä³ö(Ö»ÄÜÊÇËõÐ¡)¡£
} tSdkIspCapacity;

/* ¶¨ÒåÕûºÏµÄÉè±¸ÃèÊöÐÅÏ¢£¬ÕâÐ©ÐÅÏ¢¿ÉÒÔÓÃÓÚ¶¯Ì¬¹¹½¨UI */
typedef struct
{

  tSdkTrigger   *pTriggerDesc;          // ´¥·¢Ä£Ê½
  INT           iTriggerDesc;           // ´¥·¢Ä£Ê½µÄ¸öÊý£¬¼´pTriggerDescÊý×éµÄ´óÐ¡

  tSdkImageResolution   *pImageSizeDesc;// Ô¤Éè·Ö±æÂÊÑ¡Ôñ
  INT                   iImageSizeDesc; // Ô¤Éè·Ö±æÂÊµÄ¸öÊý£¬¼´pImageSizeDescÊý×éµÄ´óÐ¡

  tSdkColorTemperatureDes *pClrTempDesc;// Ô¤ÉèÉ«ÎÂÄ£Ê½£¬ÓÃÓÚ°×Æ½ºâ
  INT                     iClrTempDesc;

  tSdkMediaType     *pMediaTypeDesc;    // Ïà»úÊä³öÍ¼Ïñ¸ñÊ½
  INT               iMediaTypdeDesc;    // Ïà»úÊä³öÍ¼Ïñ¸ñÊ½µÄÖÖÀà¸öÊý£¬¼´pMediaTypeDescÊý×éµÄ´óÐ¡¡£

  tSdkFrameSpeed    *pFrameSpeedDesc;   // ¿Éµ÷½ÚÖ¡ËÙÀàÐÍ£¬¶ÔÓ¦½çÃæÉÏÆÕÍ¨ ¸ßËÙ ºÍ³¬¼¶ÈýÖÖËÙ¶ÈÉèÖÃ
  INT               iFrameSpeedDesc;    // ¿Éµ÷½ÚÖ¡ËÙÀàÐÍµÄ¸öÊý£¬¼´pFrameSpeedDescÊý×éµÄ´óÐ¡¡£

  tSdkPackLength    *pPackLenDesc;      // ´«Êä°ü³¤¶È£¬Ò»°ãÓÃÓÚÍøÂçÉè±¸
  INT               iPackLenDesc;       // ¿É¹©Ñ¡ÔñµÄ´«Êä·Ö°ü³¤¶ÈµÄ¸öÊý£¬¼´pPackLenDescÊý×éµÄ´óÐ¡¡£

  INT           iOutputIoCounts;        // ¿É±à³ÌÊä³öIOµÄ¸öÊý
  INT           iInputIoCounts;         // ¿É±à³ÌÊäÈëIOµÄ¸öÊý

  tSdkPresetLut  *pPresetLutDesc;       // Ïà»úÔ¤ÉèµÄLUT±í
  INT            iPresetLut;            // Ïà»úÔ¤ÉèµÄLUT±íµÄ¸öÊý£¬¼´pPresetLutDescÊý×éµÄ´óÐ¡

  INT           iUserDataMaxLen;        // Ö¸Ê¾¸ÃÏà»úÖÐÓÃÓÚ±£´æÓÃ»§Êý¾ÝÇøµÄ×î´ó³¤¶È¡£Îª0±íÊ¾ÎÞ¡£
  BOOL          bParamInDevice;         // Ö¸Ê¾¸ÃÉè±¸ÊÇ·ñÖ§³Ö´ÓÉè±¸ÖÐ¶ÁÐ´²ÎÊý×é¡£1ÎªÖ§³Ö£¬0²»Ö§³Ö¡£

  tSdkAeAlgorithm   *pAeAlmSwDesc;      // Èí¼þ×Ô¶¯ÆØ¹âËã·¨ÃèÊö
  int                iAeAlmSwDesc;      // Èí¼þ×Ô¶¯ÆØ¹âËã·¨¸öÊý

  tSdkAeAlgorithm    *pAeAlmHdDesc;     // Ó²¼þ×Ô¶¯ÆØ¹âËã·¨ÃèÊö£¬ÎªNULL±íÊ¾²»Ö§³ÖÓ²¼þ×Ô¶¯ÆØ¹â
  int                iAeAlmHdDesc;      // Ó²¼þ×Ô¶¯ÆØ¹âËã·¨¸öÊý£¬Îª0±íÊ¾²»Ö§³ÖÓ²¼þ×Ô¶¯ÆØ¹â

  tSdkBayerDecodeAlgorithm   *pBayerDecAlmSwDesc; // Èí¼þBayer×ª»»ÎªRGBÊý¾ÝµÄËã·¨ÃèÊö
  int                        iBayerDecAlmSwDesc;  // Èí¼þBayer×ª»»ÎªRGBÊý¾ÝµÄËã·¨¸öÊý

  tSdkBayerDecodeAlgorithm   *pBayerDecAlmHdDesc; // Ó²¼þBayer×ª»»ÎªRGBÊý¾ÝµÄËã·¨ÃèÊö£¬ÎªNULL±íÊ¾²»Ö§³Ö
  int                        iBayerDecAlmHdDesc;  // Ó²¼þBayer×ª»»ÎªRGBÊý¾ÝµÄËã·¨¸öÊý£¬Îª0±íÊ¾²»Ö§³Ö

  /* Í¼Ïñ²ÎÊýµÄµ÷½Ú·¶Î§¶¨Òå,ÓÃÓÚ¶¯Ì¬¹¹½¨UI*/
  tSdkExpose            sExposeDesc;      // ÆØ¹âµÄ·¶Î§Öµ
  tSdkResolutionRange   sResolutionRange; // ·Ö±æÂÊ·¶Î§ÃèÊö
  tRgbGainRange         sRgbGainRange;    // Í¼ÏñÊý×ÖÔöÒæ·¶Î§ÃèÊö
  tSaturationRange      sSaturationRange; // ±¥ºÍ¶È·¶Î§ÃèÊö
  tGammaRange           sGammaRange;      // Ù¤Âí·¶Î§ÃèÊö
  tContrastRange        sContrastRange;   // ¶Ô±È¶È·¶Î§ÃèÊö
  tSharpnessRange       sSharpnessRange;  // Èñ»¯·¶Î§ÃèÊö
  tSdkIspCapacity       sIspCapacity;     // ISPÄÜÁ¦ÃèÊö


} tSdkCameraCapbility;


//Í¼ÏñÖ¡Í·ÐÅÏ¢
typedef struct
{
  UINT    uiMediaType;    // Í¼Ïñ¸ñÊ½,Image Format
  UINT    uBytes;         // Í¼ÏñÊý¾Ý×Ö½ÚÊý,Total bytes
  INT     iWidth;         // Í¼ÏñµÄ¿í¶È£¬µ÷ÓÃÍ¼Ïñ´¦Àíº¯Êýºó£¬¸Ã±äÁ¿¿ÉÄÜ±»¶¯Ì¬ÐÞ¸Ä£¬À´Ö¸Ê¾´¦ÀíºóµÄÍ¼Ïñ³ß´ç
  INT     iHeight;        // Í¼ÏñµÄ¸ß¶È£¬µ÷ÓÃÍ¼Ïñ´¦Àíº¯Êýºó£¬¸Ã±äÁ¿¿ÉÄÜ±»¶¯Ì¬ÐÞ¸Ä£¬À´Ö¸Ê¾´¦ÀíºóµÄÍ¼Ïñ³ß´ç
  INT     iWidthZoomSw;   // Èí¼þËõ·ÅµÄ¿í¶È,²»ÐèÒª½øÐÐÈí¼þ²Ã¼ôµÄÍ¼Ïñ£¬´Ë±äÁ¿ÉèÖÃÎª0.
  INT     iHeightZoomSw;  // Èí¼þËõ·ÅµÄ¸ß¶È,²»ÐèÒª½øÐÐÈí¼þ²Ã¼ôµÄÍ¼Ïñ£¬´Ë±äÁ¿ÉèÖÃÎª0.
  BOOL    bIsTrigger;     // Ö¸Ê¾ÊÇ·ñÎª´¥·¢Ö¡ is trigger
  UINT    uiTimeStamp;    // ¸ÃÖ¡µÄ²É¼¯Ê±¼ä£¬µ¥Î»0.1ºÁÃë
  UINT    uiExpTime;      // µ±Ç°Í¼ÏñµÄÆØ¹âÖµ£¬µ¥Î»ÎªÎ¢Ãëus
  float   fAnalogGain;    // µ±Ç°Í¼ÏñµÄÄ£ÄâÔöÒæ±¶Êý
  INT     iGamma;         // ¸ÃÖ¡Í¼ÏñµÄÙ¤ÂíÉè¶¨Öµ£¬½öµ±LUTÄ£Ê½Îª¶¯Ì¬²ÎÊýÉú³ÉÊ±ÓÐÐ§£¬ÆäÓàÄ£Ê½ÏÂÎª-1
  INT     iContrast;      // ¸ÃÖ¡Í¼ÏñµÄ¶Ô±È¶ÈÉè¶¨Öµ£¬½öµ±LUTÄ£Ê½Îª¶¯Ì¬²ÎÊýÉú³ÉÊ±ÓÐÐ§£¬ÆäÓàÄ£Ê½ÏÂÎª-1
  INT     iSaturation;    // ¸ÃÖ¡Í¼ÏñµÄ±¥ºÍ¶ÈÉè¶¨Öµ£¬¶ÔÓÚºÚ°×Ïà»úÎÞÒâÒå£¬Îª0
  float   fRgain;         // ¸ÃÖ¡Í¼Ïñ´¦ÀíµÄºìÉ«Êý×ÖÔöÒæ±¶Êý£¬¶ÔÓÚºÚ°×Ïà»úÎÞÒâÒå£¬Îª1
  float   fGgain;         // ¸ÃÖ¡Í¼Ïñ´¦ÀíµÄÂÌÉ«Êý×ÖÔöÒæ±¶Êý£¬¶ÔÓÚºÚ°×Ïà»úÎÞÒâÒå£¬Îª1
  float   fBgain;         // ¸ÃÖ¡Í¼Ïñ´¦ÀíµÄÀ¶É«Êý×ÖÔöÒæ±¶Êý£¬¶ÔÓÚºÚ°×Ïà»úÎÞÒâÒå£¬Îª1
}tSdkFrameHead;

//Í¼ÏñÖ¡ÃèÊö
typedef struct sCameraFrame
{
  tSdkFrameHead   head;     //Ö¡Í·
  BYTE *          pBuffer;  //Êý¾ÝÇø
}tSdkFrame;

//Í¼Ïñ²¶»ñµÄ»Øµ÷º¯Êý¶¨Òå
typedef void (*CAMERA_SNAP_PROC)(CameraHandle hCamera, BYTE *pFrameBuffer, tSdkFrameHead* pFrameHead,PVOID pContext);

//SDKÉú³ÉµÄÏà»úÅäÖÃÒ³ÃæµÄÏûÏ¢»Øµ÷º¯Êý¶¨Òå
typedef void (*CAMERA_PAGE_MSG_PROC)(CameraHandle hCamera,UINT MSG,UINT uParam,PVOID pContext);


//----------------------------IMAGE FORMAT DEFINE------------------------------------
//----------------------------Í¼Ïñ¸ñÊ½¶¨Òå-------------------------------------------
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

#define CAMERA_MEDIA_TYPE_PIXEL_SIZE(type)                 (((type) & CAMERA_MEDIA_TYPE_EFFECTIVE_PIXEL_SIZE_MASK)>>CAMERA_MEDIA_TYPE_EFFECTIVE_PIXEL_SIZE_SHIFT)

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
