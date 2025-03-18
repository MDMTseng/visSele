#ifndef PLUGIN_INTERFACE_H
#define PLUGIN_INTERFACE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "cJSON.h"

// Function types for plugin operations
typedef void* (*CreatePluginInstance)();
typedef void (*DestroyPluginInstance)(void* instance);
typedef void (*SetupPluginInstance)(void* instance, const cJSON* setup_data);
typedef cJSON* (*ProcessMessage)(void* instance, const cJSON* message);

// Plugin interface structure
typedef struct {
    CreatePluginInstance create;
    DestroyPluginInstance destroy;
    SetupPluginInstance setup;
    ProcessMessage process;
} PluginInterface;

// Export these functions from your plugin
#ifdef _WIN32
    #define PLUGIN_EXPORT __declspec(dllexport)
#else
    #define PLUGIN_EXPORT __attribute__((visibility("default")))
#endif

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
};

struct InfoPack{
    cJSON* info[10];
    int info_count;
    ImageInfo *img[10];
    int img_count;
};

// Required plugin functions
PLUGIN_EXPORT void* CreateInstance();
PLUGIN_EXPORT void DestroyInstance(void* instance);
PLUGIN_EXPORT void SetupInstance(void* instance, const cJSON* setup_data);

PLUGIN_EXPORT cJSON* ProcessInstanceMessage(void* instance, const cJSON* message);

// Optional: Get plugin interface structure
PLUGIN_EXPORT PluginInterface* GetPluginInterface();

#ifdef __cplusplus
}
#endif

#endif // PLUGIN_INTERFACE_H
