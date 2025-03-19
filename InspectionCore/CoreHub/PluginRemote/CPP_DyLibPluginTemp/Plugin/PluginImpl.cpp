#include "PluginInterface.h"
#include "InspectPlugin.h"
#include "ConversionUtil.h"
#include <opencv2/opencv.hpp>

// Implementation of the plugin interface functions
extern "C" {
    PLUGIN_EXPORT void* CreateInstance() {
        // In a real plugin, you would create and return your plugin instance here
        InspectPlugin* plugin = new InspectPlugin();
        return plugin;
    }

    PLUGIN_EXPORT void DestroyInstance(void* instance) {
        // Cleanup code here
        InspectPlugin* plugin = static_cast<InspectPlugin*>(instance);
        delete plugin;
    }

    PLUGIN_EXPORT void SetupInstance(void* instance, const cJSON* setup_data) {
        // Setup code here
        InspectPlugin* plugin = static_cast<InspectPlugin*>(instance);
        plugin->setup(setup_data);
    }

    PLUGIN_EXPORT cJSON* ProcessInstanceMessage(void* instance, const cJSON* message) {
        // This would normally process messages from the host application
        // For this example, let's just return a simple response
        InspectPlugin* plugin = static_cast<InspectPlugin*>(instance);
        cJSON* response = plugin->processMessage(message);
        return response;
    }

    PLUGIN_EXPORT void ProcessInstanceImage(void* instance, ImageInfo* image_info) {
        InspectPlugin* plugin = static_cast<InspectPlugin*>(instance);
        plugin->processImage(image_info);
    }

    PLUGIN_EXPORT PluginInterface* GetPluginInterface() {
        static PluginInterface interface = {
            CreateInstance,
            DestroyInstance,
            SetupInstance,
            ProcessInstanceMessage,
            ProcessInstanceImage
        };
        return &interface;
    }
}