#ifndef PLUGIN_INTERFACE_H
#define PLUGIN_INTERFACE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "cJSON.h"

struct ITPIF_ImageInfo {
    void* buffer;       // Pointer to the image data
    int width;          // Width of the image in pixels
    int height;         // Height of the image in pixels
    int channels;       // Number of channels (1=grayscale, 3=RGB, 4=RGBA)
    int step;           // Row stride (bytes per row)
    int type;           // OpenCV type (CV_8UC1, CV_8UC3, etc.) stored as an integer
    int elemSize;       // Size of each element in bytes
    int totalSize;      // Total size of the buffer in bytes
    int refCount;       // Reference counter for memory management
    int ref_id;         // Reference ID for custom use
};

static ITPIF_ImageInfo ITPIF_ImageInfoInit(){
    ITPIF_ImageInfo imgInfo;
    imgInfo.buffer = nullptr;
    imgInfo.width = 0;
    imgInfo.height = 0;
    imgInfo.channels = 0;
    imgInfo.step = 0;
    imgInfo.type = 0;
    imgInfo.elemSize = 0;
    imgInfo.ref_id = -1;
    return imgInfo;
}

struct ITPIF_StageInfo_c{
    char type[32];
    char source_id[128];

    cJSON* jInfo;
    ITPIF_ImageInfo img_show, img;
};

// Forward declaration
struct ITPIF_StageInfo_c;

// Function types 
typedef int (*ITPIF_DispatchFunc)(void* main_ctx,struct ITPIF_StageInfo_c* data);
typedef cJSON* (*ITPIF_GetGlobalValueFunc)(void* main_ctx);
typedef void (*ITPIF_UnlockGlobalValueFunc)(void* main_ctx);

typedef struct ITPIF_ManagerInterface {
    ITPIF_DispatchFunc dispatch;
    ITPIF_GetGlobalValueFunc getNLockGlobalValue;
    ITPIF_UnlockGlobalValueFunc unLockGlobalValue;
} ITPIF_ManagerInterface;

// Function types for communicating with the plugin host
typedef int (*ITPIF_SendACKFunc)(int pgID, int isACK, const char *json_content);
typedef int (*ITPIF_SendFunc)(const char *TL, int pgID, cJSON* def);
typedef int (*ITPIF_SendImageFunc)(int pgID, ITPIF_ImageInfo* img, const char *format_lowercase, float quality);

typedef struct ITPIF_CMDActInterface {
    ITPIF_SendACKFunc sendACK;
    ITPIF_SendFunc send;
    ITPIF_SendImageFunc sendImage;
} ITPIF_CMDActInterface;

// Function types for plugin operations
typedef void* (*ITPIF_CreatePluginInstance)(const char *id, cJSON* def, const char *local_env_path, struct ManagerInterface* manager, void* main_ctx);
typedef void (*ITPIF_DestroyPluginInstance)(void* instance);
typedef void (*ITPIF_setEnvPath)(void* instance, const char *path);
typedef int (*ITPIF_PluginSetDef)(void* instance, cJSON* def);
typedef int (*ITPIF_PluginExchangeCMD)(void* instance, cJSON *info, int id, struct CMDActInterface act);
typedef int (*ITPIF_PluginProcess)(void* instance, struct ITPIF_StageInfo_c* data);

// Plugin interface structure
typedef struct ITPIF_PluginInterface {
    ITPIF_CreatePluginInstance create;
    ITPIF_DestroyPluginInstance destroy;
    ITPIF_setEnvPath setEnvPath;
    ITPIF_PluginSetDef setDef;
    ITPIF_PluginExchangeCMD exchangeCMD;
    ITPIF_PluginProcess process;
} ITPIF_PluginInterface;

// Export these functions from your plugin
#ifdef _WIN32
    #define PLUGIN_EXPORT __declspec(dllexport)
#else
    #define PLUGIN_EXPORT __attribute__((visibility("default")))
#endif

// Required plugin functions
// PLUGIN_EXPORT void* CreateInstance();
// PLUGIN_EXPORT void DestroyInstance(void* instance);
// PLUGIN_EXPORT void SetupInstance(void* instance, const cJSON* setup_data);
// PLUGIN_EXPORT cJSON* ProcessInstanceMessage(void* instance, const cJSON* message);
// PLUGIN_EXPORT void ProcessInstanceImage(void* instance, ImageInfo* image_info);

// Optional: Get plugin interface structure
PLUGIN_EXPORT struct ITPIF_PluginInterface* ITPIF_GetPluginInterface();

#ifdef __cplusplus
}
#endif

#endif // PLUGIN_INTERFACE_H
