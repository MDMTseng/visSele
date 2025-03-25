#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <dlfcn.h>

//#include "MLNN.hpp"
#include "cJSON.h"
#include "logctrl.h"

#include "MatchingCore.h"
#include "acvImage_BasicTool.hpp"
#include "mjpegLib.h"

#include <sys/stat.h>
#include <libgen.h>
#include <main.h>
#include <playground.h>
#include <stdexcept>
#include <compat_dirent.h>
#include <smem_channel.hpp>
#include <ctime>
#include <map>
#include <algorithm>
#include <iostream>
#include <vector>
#include "CameraLayerManager.hpp"

#include <opencv2/calib3d.hpp>
#include "opencv2/imgproc.hpp"
#include <opencv2/imgcodecs.hpp>
#include "InspTarPluginInterface.h"

using namespace cv;
using namespace std;

// Plugin C-style interface structures
struct ManagerInterface {
    int (*dispatch)(void* main_ctx, struct StageInfo_c* data);
    cJSON* (*getNLockGlobalValue)(void* main_ctx);
    void (*unLockGlobalValue)(void* main_ctx);
};

struct CMDActInterface {
    int (*sendACK)(int pgID, int isACK, const char* json_content);
    int (*send)(int pgID, cJSON* json_content);
    int (*sendImage)(int pgID, ITPIF_ImageInfo* data, const char* info);
};

struct StageInfo_c {
    ITPIF_ImageInfo img;
    ITPIF_ImageInfo img_show;
    const char* type;
    const char* source_id;
};

// Plugin function type definitions
typedef void* (*CreateInstanceFunc)(const char* id, cJSON* def, const char* local_env_path, 
                                    struct ManagerInterface* manager, void* main_ctx);
typedef void (*DestroyInstanceFunc)(void* instance);
typedef const char* (*GetPluginTypeFunc)(void* instance);
typedef void (*SetEnvPathFunc)(void* instance, const char* path);
typedef int (*SetDefFunc)(void* instance, cJSON* def);
typedef int (*ExchangeCMDFunc)(void* instance, cJSON* info, int id, struct CMDActInterface act);
typedef int (*ProcessFunc)(void* instance, struct StageInfo_c* data);

// Plugin interface structure
struct PluginFunctions {
    CreateInstanceFunc createInstance;
    DestroyInstanceFunc destroyInstance;
    GetPluginTypeFunc getPluginType;
    SetEnvPathFunc setEnvPath;
    SetDefFunc setDef;
    ExchangeCMDFunc exchangeCMD;
    ProcessFunc process;
};

// Class for individual plugin instances
class InspectionTarget_PlugIn {
private:
    void* instance;
    void* handle;
    PluginFunctions functions;
    std::string id;
    std::string plugin_type;

public:
    InspectionTarget_PlugIn() : instance(nullptr), handle(nullptr) {}
    
    bool init(void* handle, const PluginFunctions& functions, 
              const std::string& id, cJSON* def, 
              const std::string& local_env_path,
              ManagerInterface* manager, void* main_ctx) {
        this->handle = handle;
        this->functions = functions;
        this->id = id;
        
        if (!functions.createInstance) {
            return false;
        }
        
        instance = functions.createInstance(id.c_str(), def, local_env_path.c_str(), manager, main_ctx);
        if (!instance) {
            return false;
        }
        
        // Get the plugin type
        if (functions.getPluginType) {
            plugin_type = functions.getPluginType(instance);
        }
        
        return true;
    }
    
    void SetEnvPath(const std::string& path) {
        if (instance && functions.setEnvPath) {
            functions.setEnvPath(instance, path.c_str());
        }
    }
    
    int SetDef(cJSON* def) {
        if (instance && functions.setDef) {
            return functions.setDef(instance, def);
        }
        return -1;
    }
    
    int ExchangeCMD(cJSON* info, int id, CMDActInterface act) {
        if (instance && functions.exchangeCMD) {
            return functions.exchangeCMD(instance, info, id, act);
        }
        return -1;
    }
    
    int Process(StageInfo_c* data) {
        if (instance && functions.process) {
            return functions.process(instance, data);
        }
        return -1;
    }
    
    std::string GetID() const {
        return id;
    }
    
    std::string GetType() const {
        return plugin_type;
    }
    
    ~InspectionTarget_PlugIn() {
        if (instance && functions.destroyInstance) {
            functions.destroyInstance(instance);
            instance = nullptr;
        }
    }
};

// Main plugin manager class
class InspectionTarget_PlugIn_Manager {
private:
    std::map<std::string, void*> loaded_plugins;
    std::map<std::string, PluginFunctions> plugin_functions;
    std::vector<InspectionTarget_PlugIn*> plugin_instances;
    
    // Function to load a plugin and get its functions
    bool load_plugin_functions(const std::string& path, PluginFunctions& functions) {
        void* handle = dlopen(path.c_str(), RTLD_LAZY);
        if (!handle) {
            std::cerr << "Error: Failed to load plugin: " << dlerror() << std::endl;
            return false;
        }
        
        // Clear errors
        dlerror();
        
        // Load function pointers
        functions.createInstance = (CreateInstanceFunc)dlsym(handle, "CreateInstance");
        functions.destroyInstance = (DestroyInstanceFunc)dlsym(handle, "DestroyInstance");
        functions.getPluginType = (GetPluginTypeFunc)dlsym(handle, "GetPluginType");
        functions.setEnvPath = (SetEnvPathFunc)dlsym(handle, "SetEnvPath");
        functions.setDef = (SetDefFunc)dlsym(handle, "SetDef");
        functions.exchangeCMD = (ExchangeCMDFunc)dlsym(handle, "ExchangeCMD");
        functions.process = (ProcessFunc)dlsym(handle, "Process");
        
        // Check for required functions
        if (!functions.createInstance || !functions.destroyInstance || !functions.process) {
            std::cerr << "Error: Plugin missing required functions: " << dlerror() << std::endl;
            dlclose(handle);
            return false;
        }
        
        return true;
    }

public:
    InspectionTarget_PlugIn_Manager() {}
    
    // Load a plugin library from path
    bool load_plugin(const std::string& plugin_path) {
        // Check if plugin already loaded
        if (loaded_plugins.find(plugin_path) != loaded_plugins.end()) {
            return true; // Already loaded
        }
        
        void* handle = dlopen(plugin_path.c_str(), RTLD_LAZY);
        if (!handle) {
            std::cerr << "Error: Failed to load plugin: " << dlerror() << std::endl;
            return false;
        }
        
        PluginFunctions functions;
        if (!load_plugin_functions(plugin_path, functions)) {
            dlclose(handle);
            return false;
        }
        
        // Store the plugin handle and functions
        loaded_plugins[plugin_path] = handle;
        plugin_functions[plugin_path] = functions;
        
        return true;
    }
    
    // Create an instance of a plugin
    InspectionTarget_PlugIn* create_instance(const std::string& plugin_path, 
                                            const std::string& id, 
                                            cJSON* def,
                                            const std::string& local_env_path,
                                            ManagerInterface* manager, 
                                            void* main_ctx) {
        // Check if plugin is loaded
        if (loaded_plugins.find(plugin_path) == loaded_plugins.end()) {
            if (!load_plugin(plugin_path)) {
                return nullptr;
            }
        }
        
        // Create a new instance
        InspectionTarget_PlugIn* plugin = new InspectionTarget_PlugIn();
        if (!plugin->init(loaded_plugins[plugin_path], plugin_functions[plugin_path], 
                         id, def, local_env_path, manager, main_ctx)) {
            delete plugin;
            return nullptr;
        }
        
        // Store the instance
        plugin_instances.push_back(plugin);
        
        return plugin;
    }
    
    // Get an instance by ID
    InspectionTarget_PlugIn* get_instance(const std::string& id) {
        for (auto plugin : plugin_instances) {
            if (plugin->GetID() == id) {
                return plugin;
            }
        }
        return nullptr;
    }
    
    // Remove and destroy an instance
    void remove_instance(InspectionTarget_PlugIn* plugin) {
        auto it = std::find(plugin_instances.begin(), plugin_instances.end(), plugin);
        if (it != plugin_instances.end()) {
            plugin_instances.erase(it);
            delete plugin;
        }
    }
    
    // Unload all plugins and clean up
    ~InspectionTarget_PlugIn_Manager() {
        // Delete all instances
        for (auto plugin : plugin_instances) {
            delete plugin;
        }
        plugin_instances.clear();
        
        // Close all plugin handles
        for (auto& handle_pair : loaded_plugins) {
            dlclose(handle_pair.second);
        }
        loaded_plugins.clear();
        plugin_functions.clear();
    }
};

