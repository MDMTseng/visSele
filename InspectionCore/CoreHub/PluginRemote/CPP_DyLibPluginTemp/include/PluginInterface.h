#ifndef PLUGIN_INTERFACE_H
#define PLUGIN_INTERFACE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "cJSON.h"

struct ImageInfo {
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

static ImageInfo ImageInfoInit(){
    ImageInfo imgInfo;
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

struct StageInfo_c{
    char type[32];
    char source_id[128];

    cJSON* jInfo;
    struct ImageInfo img_show, img;
};

// Forward declaration
struct StageInfo_c;

// Function types 
typedef int (*DispatchFunc)(void* main_ctx,struct StageInfo_c* data);
typedef cJSON* (*GetGlobalValueFunc)(void* main_ctx);
typedef void (*UnlockGlobalValueFunc)(void* main_ctx);

typedef struct ManagerInterface {
    DispatchFunc dispatch;
    GetGlobalValueFunc getNLockGlobalValue;
    UnlockGlobalValueFunc unLockGlobalValue;
} ManagerInterface;

// Function types for communicating with the plugin host
typedef int (*SendACKFunc)(int pgID, int isACK, const char *json_content);
typedef int (*SendFunc)(const char *TL, int pgID, cJSON* def);
typedef int (*SendImageFunc)(int pgID, struct ImageInfo* img, const char *format_lowercase, float quality);

typedef struct CMDActInterface {
    SendACKFunc sendACK;
    SendFunc send;
    SendImageFunc sendImage;
} CMDActInterface;

// Function types for plugin operations
typedef void* (*CreatePluginInstance)(const char *id, cJSON* def, const char *local_env_path, struct ManagerInterface* manager, void* main_ctx);
typedef void (*DestroyPluginInstance)(void* instance);
typedef void (*setEnvPath)(void* instance, const char *path);
typedef int (*PluginSetDef)(void* instance, cJSON* def);
typedef int (*PluginExchangeCMD)(void* instance, cJSON *info, int id, struct CMDActInterface act);
typedef int (*PluginProcess)(void* instance, struct StageInfo_c* data);

// Plugin interface structure
typedef struct PluginInterface {
    CreatePluginInstance create;
    DestroyPluginInstance destroy;
    setEnvPath setEnvPath;
    PluginSetDef setDef;
    PluginExchangeCMD exchangeCMD;
    PluginProcess process;
} PluginInterface;

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
PLUGIN_EXPORT struct PluginInterface* GetPluginInterface();

#ifdef __cplusplus
}
#endif

#endif // PLUGIN_INTERFACE_H
