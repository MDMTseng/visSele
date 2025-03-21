#include "PluginInterface.h"
#include "InspectPlugin.h"
#include "ConversionUtil.h"
#include <opencv2/opencv.hpp>

// Implementation of the plugin interface functions
extern "C" {
    PLUGIN_EXPORT void* CreateInstance(const char *id, cJSON* def, const char *local_env_path, struct ManagerInterface* manager, void* main_ctx) {
        // In a real plugin, you would create and return your plugin instance here
        InspectPlugin* plugin = new InspectPlugin();
        // Initialize the plugin with the parameters
        plugin->init(id, def, local_env_path, manager, main_ctx);
        return plugin;
    }

    PLUGIN_EXPORT void DestroyInstance(void* instance) {
        // Cleanup code here
        InspectPlugin* plugin = static_cast<InspectPlugin*>(instance);
        delete plugin;
    }

    PLUGIN_EXPORT void SetEnvPath(void* instance, const char* path) {
        InspectPlugin* plugin = static_cast<InspectPlugin*>(instance);
        plugin->setEnvPath(path);
    }

    PLUGIN_EXPORT int SetDef(void* instance, cJSON* def) {
        InspectPlugin* plugin = static_cast<InspectPlugin*>(instance);
        return plugin->setDef(def);
    }

    PLUGIN_EXPORT int ExchangeCMD(void* instance, cJSON* info, int id, struct CMDActInterface act) {
        InspectPlugin* plugin = static_cast<InspectPlugin*>(instance);
        return plugin->exchangeCMD(info, id, act);
    }

    PLUGIN_EXPORT int Process(void* instance, struct StageInfo_c* data) {
        InspectPlugin* plugin = static_cast<InspectPlugin*>(instance);
        return plugin->process(data);
    }

   
    PLUGIN_EXPORT struct PluginInterface* GetPluginInterface() {
        static struct PluginInterface interface = {
            CreateInstance,
            DestroyInstance,
            SetEnvPath,
            SetDef,
            ExchangeCMD,
            Process
        };
        return &interface;
    }
}