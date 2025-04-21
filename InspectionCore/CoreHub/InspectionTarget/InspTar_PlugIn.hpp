#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <dlfcn.h>

//#include "MLNN.hpp"
#include "cJSON.h"
#include "logctrl.h"

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

// Forward declare structs to address incomplete type issues
// Define callback functions outside the test function
int dispatch_callback(void* main_ctx, ITPIF_StageInfo_c* data);
cJSON* get_global_value_callback(void* main_ctx);
void unlock_global_value_callback(void* main_ctx);
int send_ack_callback(int pgID, int isACK, const char* json_content);
int send_callback(const char* TL, int pgID, cJSON* json_content);
int send_image_callback(int pgID, ITPIF_ImageInfo* data, const char* format, float quality);




// Class for individual plugin instances
class InspectionTarget_PlugIn :public InspectionTarget {
private:
    void* instance;
    void* handle;
    ITPIF_PluginInterface interface;
    std::string id;
    std::string plugin_type;
    InspectionTargetManager* belongMan;  // Store belongMan as member
    ITPIF_ManagerInterface manager;
    vector<cv::Mat> imgCacheList;
    vector<shared_ptr<StageInfo>> stageInfoCacheList;



    ITPIF_ImageInfo MatExtract_ITPIF_ImageInfo(const cv::Mat& mat) {
        if (mat.empty()) {
            return ITPIF_ImageInfoInit();
        }

        ITPIF_ImageInfo imgInfo = ITPIF_ImageInfoInit();
        
        imgInfo.ref_id = -1;
        // Fill in the metadata
        imgInfo.width = mat.cols;
        imgInfo.height = mat.rows;
        imgInfo.channels = mat.channels();
        imgInfo.step = static_cast<int>(mat.step);
        imgInfo.type = mat.type();
        imgInfo.elemSize = static_cast<int>(mat.elemSize());
        imgInfo.totalSize = static_cast<int>(mat.total() * mat.elemSize());
        
        imgInfo.buffer = mat.data;
        imgCacheList.push_back(mat);
        imgInfo.ref_id = imgCacheList.size() - 1;
        return imgInfo;
    }

    cv::Mat ITPIF_ImageInfo_Find_Mat(const ITPIF_ImageInfo imgInfo) {
        int ret_id=imgInfo.ref_id;
        if(ret_id<0 || ret_id>=imgCacheList.size())return cv::Mat();
        return imgCacheList[ret_id];
    }

        ITPIF_StageInfo_c StageInfo_extract_StageInfo_c(shared_ptr<StageInfo> sinfo=nullptr)
    {
      if(sinfo==nullptr)
      {
        sinfo = make_shared<StageInfo>();
      }
      StageInfo &sinfo_tmp=*(sinfo.get());
      stageInfoCacheList.push_back(sinfo);
      ITPIF_StageInfo_c stage_info_c;

      stage_info_c.jInfo=sinfo->jInfo;

      stage_info_c.ref_id=stageInfoCacheList.size()-1;
      return stage_info_c;
    }

    shared_ptr<StageInfo> StageInfo_c_find_StageInfo(const ITPIF_StageInfo_c& stage_info_c)
    {
      if(stage_info_c.ref_id<0 || stage_info_c.ref_id>=stageInfoCacheList.size())return nullptr;
      return stageInfoCacheList[stage_info_c.ref_id];
    }

    // Static callback that will be used as function pointer
    int dispatch_callback(ITPIF_StageInfo_c* data) {
      shared_ptr<StageInfo> sinfo=StageInfo_c_find_StageInfo(*data);
      if(sinfo==nullptr)
      {
        LOGE("ref_id error");
        return -1;
      }

      sinfo->img=ITPIF_ImageInfo_Find_Mat(data->img);
      sinfo->img_show=ITPIF_ImageInfo_Find_Mat(data->img_show);
      
      return belongMan->dispatch(sinfo, this);
    }
    
public:



    InspectionTarget_PlugIn(const ITPIF_PluginInterface& interface, string id, cJSON *def, 
                           InspectionTargetManager *belongMan, std::string local_env_path)
        : instance(nullptr), handle(nullptr), belongMan(belongMan)
    {
        InspectionTarget::INIT(id,def,belongMan,local_env_path);
        this->interface = interface;
        
        manager.dispatch = [](void* main_ctx, ITPIF_StageInfo_c* data)->int{

          auto self = static_cast<InspectionTarget_PlugIn*>(main_ctx);
          return self->dispatch_callback(data);
        };  // Use the static member function
        instance = interface.create(id.c_str(), def, local_env_path.c_str(), &manager, this);  // Pass "this" as context
    }

    InspectionTarget_PlugIn() : instance(nullptr), handle(nullptr) {}
    
    bool init(void* handle, const ITPIF_PluginInterface& interface, 
              const std::string& id, cJSON* def, 
              const std::string& local_env_path,
              ITPIF_ManagerInterface* manager, void* main_ctx) {
        this->handle = handle;
        this->interface = interface;
        this->id = id;
        
        if (!interface.create) {
            return false;
        }
        manager->requestImg = [](void* main_ctx, int width, int height, int channels, int type)->ITPIF_ImageInfo {
            auto self = static_cast<InspectionTarget_PlugIn*>(main_ctx);
            return self->MatExtract_ITPIF_ImageInfo(cv::Mat(height,width,type));
        };
        manager->requestStageInfo = [](void* main_ctx)->ITPIF_StageInfo_c {
            auto self = static_cast<InspectionTarget_PlugIn*>(main_ctx);
            return self->StageInfo_extract_StageInfo_c();
        };
        // Cast to void* to avoid the incomplete type issue
        instance = interface.create(id.c_str(), def, local_env_path.c_str(), manager, main_ctx);
        if (!instance) {
            return false;
        }
        
        // Plugin type will be unknown since getPluginType isn't in the interface
        plugin_type = "unknown";
        
        return true;
    }
    
    void SetEnvPath(const std::string& path) {
        if (instance && interface.setEnvPath) {
            interface.setEnvPath(instance, path.c_str());
        }
    }
    
    int SetDef(cJSON* def) {
        if (instance && interface.setDef) {
            return interface.setDef(instance, def);
        }
        return -1;
    }
    
    int ExchangeCMD(cJSON* info, int id, ITPIF_CMDActInterface& act) {
        if (instance && interface.exchangeCMD) {
            // Cast to void* to avoid the incomplete type issue
            return interface.exchangeCMD(instance, info, id, act);
        }
        return -1;
    }
    
    int Process(ITPIF_StageInfo_c* data) {
        if (instance && interface.process) {
            return interface.process(instance, data);
        }
        return -1;
    }
    
    void singleProcess(std::shared_ptr<StageInfo> sinfo)
    {
      ITPIF_StageInfo_c stage_info;
      memset(&stage_info, 0, sizeof(ITPIF_StageInfo_c));
      imgCacheList.clear();
      stageInfoCacheList.clear();

      stage_info=StageInfo_extract_StageInfo_c(sinfo);

      stage_info.img=MatExtract_ITPIF_ImageInfo(sinfo->img);
      stage_info.img_show=MatExtract_ITPIF_ImageInfo(sinfo->img_show);

      interface.process(instance, &stage_info);
      return;
    }

    void run()
    {

      // acvImage cacheImage;
      while(true)
      {
        std::shared_ptr<StageInfo> curInput;
        // LOGI("<<<<<size():%d",datTransferQueue.size());
        // std::this_thread::sleep_for(std::chrono::milliseconds(500));//SLOW load test
              
        try{
          // LOGI("TryReadNew");
          if(input_queue.pop_blocking(curInput)==false)
          {
            LOGI("TryReadTailed");
            break;
          }

          singleProcess(curInput);
        }
        catch(TS_Termination_Exception e)
        {
          LOGI("TS_Termination_Exception");
          break;
        }
        
      }


    }




    std::string GetID() const {
        return id;
    }
    
    std::string GetType() const {
        return plugin_type;
    }
    
    ~InspectionTarget_PlugIn() {
        if (instance && interface.destroy) {
            interface.destroy(instance);
            instance = nullptr;
        }
    }
};

// Main plugin manager class
class InspectionTarget_PlugIn_Manager {
private:
    std::map<std::string, void*> loaded_plugins;
    std::map<std::string, ITPIF_PluginInterface> plugin_interfaces;
    std::vector<InspectionTarget_PlugIn*> plugin_instances;
    
    // Function to load a plugin and get its interface
    bool load_plugin_functions(const std::string& path, ITPIF_PluginInterface& interface) {
        void* handle = dlopen(path.c_str(), RTLD_LAZY);
        if (!handle) {
            std::cerr << "Error: Failed to load plugin: " << dlerror() << std::endl;
            return false;
        }
        
        // Clear errors
        dlerror();
        
        // Try to load the GetPluginInterface function first
        typedef ITPIF_PluginInterface* (*GetPluginInterfaceFunc)();
        auto getInterface = (GetPluginInterfaceFunc)dlsym(handle, "ITPIF_GetPluginInterface");
        
        if (getInterface) {
            // Plugin provides the interface getter function
            ITPIF_PluginInterface* pluginInterface = getInterface();
            if (pluginInterface) {
                interface = *pluginInterface;
                return true;
            }
        }
        
        // Fallback: load individual functions
        interface.create = (ITPIF_CreatePluginInstance)dlsym(handle, "CreateInstance");
        interface.destroy = (ITPIF_DestroyPluginInstance)dlsym(handle, "DestroyInstance");
        // getPluginType isn't in the interface, skip it
        interface.setEnvPath = (ITPIF_setEnvPath)dlsym(handle, "SetEnvPath");
        interface.setDef = (ITPIF_PluginSetDef)dlsym(handle, "SetDef");
        interface.exchangeCMD = (ITPIF_PluginExchangeCMD)dlsym(handle, "ExchangeCMD");
        interface.process = (ITPIF_PluginProcess)dlsym(handle, "Process");
        
        // Check for required functions
        if (!interface.create || !interface.destroy || !interface.process) {
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
        
        ITPIF_PluginInterface interface;
        if (!load_plugin_functions(plugin_path, interface)) {
            dlclose(handle);
            return false;
        }
        
        // Store the plugin handle and interface
        loaded_plugins[plugin_path] = handle;
        plugin_interfaces[plugin_path] = interface;
        
        return true;
    }
    
    // Create an instance of a plugin
    InspectionTarget_PlugIn* create_instance(const std::string& plugin_path, 
                                            const std::string& id, 
                                            cJSON* def,
                                            const std::string& local_env_path,
                                            ITPIF_ManagerInterface* manager, 
                                            void* main_ctx) {
        // Check if plugin is loaded
        if (loaded_plugins.find(plugin_path) == loaded_plugins.end()) {
            if (!load_plugin(plugin_path)) {
                return nullptr;
            }
        }
        
        // Create a new instance
        InspectionTarget_PlugIn* plugin = new InspectionTarget_PlugIn();
        if (!plugin->init(loaded_plugins[plugin_path], plugin_interfaces[plugin_path], 
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
        plugin_interfaces.clear();
    }
};

// Implement callback functions
int dispatch_callback(void* main_ctx, ITPIF_StageInfo_c* data) {
    printf("Dispatch called\n");
    return 0;
}

cJSON* get_global_value_callback(void* main_ctx) {
    printf("GetNLockGlobalValue called\n");
    return cJSON_CreateObject();
}

void unlock_global_value_callback(void* main_ctx) {
    printf("UnLockGlobalValue called\n");
}

int send_ack_callback(int pgID, int isACK, const char* json_content) {
    printf("SendACK called: pgID=%d, isACK=%d, content=%s\n", pgID, isACK, json_content);
    return 0;
}

int send_callback(const char* TL, int pgID, cJSON* json_content) {
    char* str = cJSON_Print(json_content);
    printf("Send called: pgID=%d, content=%s\n", pgID, str);
    free(str);
    return 0;
}

int send_image_callback(int pgID, ITPIF_ImageInfo* data, const char* format, float quality) {
    printf("SendImage called: pgID=%d, format=%s\n", pgID, format);
    return 0;
}

// Simple test function for InspectionTarget_PlugIn_Manager
void test_inspection_target_plugin_manager() {
    // Create manager instance
    InspectionTarget_PlugIn_Manager manager;
    
    // Path to a test plugin
    std::string plugin_path = "./plugins/test_plugin.so";
    
    // Setup manager interface with proper function pointers
    ITPIF_ManagerInterface manager_interface;
    manager_interface.dispatch = dispatch_callback;
    manager_interface.getNLockGlobalValue = get_global_value_callback;
    manager_interface.unLockGlobalValue = unlock_global_value_callback;
    
    // Setup CMD action interface with proper function pointers
    ITPIF_CMDActInterface cmd_interface;
    cmd_interface.sendACK = send_ack_callback;
    cmd_interface.send = send_callback;
    cmd_interface.sendImage = send_image_callback;
    
    // Main context
    void* main_ctx = nullptr;
    
    // Create plugin configuration
    cJSON* plugin_config = cJSON_CreateObject();
    cJSON_AddStringToObject(plugin_config, "name", "Test Plugin");
    cJSON_AddStringToObject(plugin_config, "version", "1.0");
    
    // Try to load the plugin
    if (manager.load_plugin(plugin_path)) {
        printf("Successfully loaded plugin: %s\n", plugin_path.c_str());
        
        // Create plugin instance
        InspectionTarget_PlugIn* plugin = manager.create_instance(
            plugin_path, 
            "test_instance", 
            plugin_config, 
            "./plugin_data", 
            &manager_interface, 
            main_ctx
        );
        
        if (plugin) {
            printf("Successfully created plugin instance with ID: %s\n", plugin->GetID().c_str());
            printf("Plugin type: %s\n", plugin->GetType().c_str());
            
            // Test setting environment path
            plugin->SetEnvPath("./custom_env");
            
            // Test updating configuration
            cJSON* updated_config = cJSON_CreateObject();
            cJSON_AddStringToObject(updated_config, "name", "Updated Test Plugin");
            cJSON_AddNumberToObject(updated_config, "threshold", 0.75);
            int result = plugin->SetDef(updated_config);
            printf("SetDef result: %d\n", result);
            cJSON_Delete(updated_config);
            
            // Test exchanging commands
            cJSON* cmd = cJSON_CreateObject();
            cJSON_AddStringToObject(cmd, "command", "status");
            plugin->ExchangeCMD(cmd, 1, cmd_interface);
            cJSON_Delete(cmd);
            
            // Test processing
            ITPIF_StageInfo_c stage_info;
            memset(&stage_info, 0, sizeof(ITPIF_StageInfo_c));
            strncpy(stage_info.type, "test_stage", sizeof(stage_info.type)-1);
            strncpy(stage_info.source_id, "camera1", sizeof(stage_info.source_id)-1);
            
            result = plugin->Process(&stage_info);
            printf("Process result: %d\n", result);
            
            // Get instance by ID and verify it's the same
            InspectionTarget_PlugIn* found_plugin = manager.get_instance("test_instance");
            if (found_plugin == plugin) {
                printf("Successfully retrieved plugin by ID\n");
            } else {
                printf("Failed to retrieve plugin by ID\n");
            }
            
            // Clean up the instance
            manager.remove_instance(plugin);
            printf("Removed plugin instance\n");
        } else {
            printf("Failed to create plugin instance\n");
        }
    } else {
        printf("Failed to load plugin: %s\n", plugin_path.c_str());
    }
    
    // Clean up
    cJSON_Delete(plugin_config);
    
    // Manager destructor will clean up remaining resources
    printf("Test completed\n");
}

// If you want to run this test from a main function:
/* 
int main() {
    test_inspection_target_plugin_manager();
    return 0;
}
*/

