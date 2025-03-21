#include <opencv2/opencv.hpp>
#include <iostream>
#include <dlfcn.h>
#include "PluginInterface.h"
#include "ConversionUtil.h"
#include <vector>
// #include "InspectPlugin.h"


// Function to send ACK
int sendAck(int pgID, int isACK, const char* json_content) {
    std::cout << "ACK received (ID: " << pgID << "): " << json_content << std::endl;
    return 0;
}

// Wrapper class for the plugin interface
class PluginWrapper {
private:
    const PluginInterface* interface;
    void* handle;
    bool valid;
    std::vector<cv::Mat> cachedImgs;
    ManagerInterface manager;
    CMDActInterface cmdAct;

public:
    PluginWrapper(const PluginInterface* interface) : interface(interface), handle(nullptr), valid(false) {
        if (!interface) {
            std::cerr << "Error: Null interface provided" << std::endl;
            return;
        }

        // Basic validation
        if (!interface->create || !interface->destroy || !interface->process) {
            std::cerr << "Error: Interface is missing required functions" << std::endl;
            return;
        }

        // Create instance with some default values
        const char* id = "test-plugin";
        cJSON* def = cJSON_CreateObject();
        cJSON_AddStringToObject(def, "processName", "TestPlugin");
        
        // Set up manager interface
        manager.dispatch = [](void* main_ctx,StageInfo_c* data) -> int {
            PluginWrapper* warpper = static_cast<PluginWrapper*>(main_ctx);
            std::cout << "dispatch " << data->type << "cachedImgs.size() : " << warpper->cachedImgs.size() << std::endl;
            
            std::cout << "data->img.ref_id : " << data->img.ref_id << std::endl;
            std::cout << "data->img_show.ref_id : " << data->img_show.ref_id << std::endl;

            return 0;
        };
        manager.getNLockGlobalValue = [](void* main_ctx) -> cJSON* {
            std::cout << "getNLockGlobalValue "  << std::endl;
            return nullptr;
        };
        manager.unLockGlobalValue = [](void* main_ctx) -> void {
            std::cout << "unLockGlobalValue " << std::endl;
        };
        
        // Create the instance
        handle = interface->create(id, def, "./", &manager,(void*)this);
        cJSON_Delete(def);
        
        if (!handle) {
            std::cerr << "Error: Failed to create plugin instance" << std::endl;
            return;
        }
        
        // Initialize CMD interface
        cmdAct.sendACK = sendAck;
        cmdAct.send = nullptr;  // Not implemented for this example
        cmdAct.sendImage = nullptr;  // Not implemented for this example
        
        valid = true;
    }

    ~PluginWrapper() {
        if (valid && interface && handle) {
            interface->destroy(handle);
            handle = nullptr;
        }
    }

    bool isValid() const { return valid && handle != nullptr; }

    // Process an image using a StageInfo structure
    bool processImage(cv::Mat testImage) {
        if (!isValid()) {
            std::cerr << "Error: Cannot process image (invalid plugin or null image)" << std::endl;
            return false;
        }

        cachedImgs.clear();
        
        // Process an image
        ImageInfo imgInfo = MatToImageInfo(testImage, true);
        cachedImgs.push_back(testImage);
        imgInfo.ref_id = cachedImgs.size()-1;

        // Set up a StageInfo structure
        StageInfo_c stageInfo = {
            .img = imgInfo,
            .img_show = imgInfo,
            .type = "test",
            .source_id = "test-source"
        };
        
        // Process the stageInfo
        int result = interface->process(handle, &stageInfo);
        
        // If we got a display image back, copy it to our input
        if (stageInfo.img_show.buffer) {
            // Copy the processed image data back if needed
            cv::Mat processedMat = ImageInfoToMat(stageInfo.img_show, true);
            cv::Mat originalMat = ImageInfoToMat(imgInfo, false);
            processedMat.copyTo(originalMat);
                
            // Clean up the img_show buffer if it's different from our input
            if (stageInfo.img_show.buffer != imgInfo.buffer) {
                free(stageInfo.img_show.buffer);
            }
        }
        
        // Save the result
        cv::imwrite("processed_image.jpg", testImage);
        std::cout << "Image processed and saved to processed_image.jpg" << std::endl;
        
        // Clean up
        FreeImageInfo(imgInfo, true);
        
        return result == 0;
    }

    // Send a command to the plugin
    cJSON* sendCommand(const char* action, cJSON* params = nullptr) {
        if (!isValid()) {
            std::cerr << "Error: Cannot send command (invalid plugin)" << std::endl;
            return nullptr;
        }
        
        // Create a command object
        cJSON* cmd = cJSON_CreateObject();
        cJSON_AddStringToObject(cmd, "action", action);
        
        // Add parameters if provided
        if (params) {
            cJSON_AddItemToObject(cmd, "params", params);
        }
        
        // Exchange the command
        int result = interface->exchangeCMD(handle, cmd, 1, cmdAct);
        
        // Clean up and return
        cJSON_Delete(cmd);
        
        // For now, just return a status object since we don't have a real response
        cJSON* response = cJSON_CreateObject();
        cJSON_AddBoolToObject(response, "success", result == 0);
        return response;
    }
};

int main() {
    std::cout << "Plugin Test Program" << std::endl;
    
    // Create a test image (500x500 white background)
    cv::Mat testImage = cv::Mat(500, 500, CV_8UC3, cv::Scalar(255, 255, 255));
    
    // Draw a blue rectangle on the image before processing
    cv::rectangle(testImage, cv::Point(50, 50), cv::Point(200, 200), 
                 cv::Scalar(255, 0, 0), 2);
    
    std::cout << "Image created with initial blue rectangle" << std::endl;
    
    // Test loading and using the plugin
    const char* pluginPath = "./lib/libinspect_process_plugin.dylib";
    std::cout << "Loading plugin: " << pluginPath << std::endl;
    
    void* handle = dlopen(pluginPath, RTLD_LAZY);
    if (!handle) {
        std::cerr << "Error: Failed to load plugin: " << dlerror() << std::endl;
        return 1;
    }
    
    // Load the plugin interface
    typedef PluginInterface* (*GetPluginInterfaceFunc)();
    GetPluginInterfaceFunc getInterfaceFn = (GetPluginInterfaceFunc)dlsym(handle, "GetPluginInterface");
    
    if (!getInterfaceFn) {
        std::cerr << "Error: Failed to load GetPluginInterface function: " << dlerror() << std::endl;
        dlclose(handle);
        return 1;
    }
    
    // Get the plugin interface
    PluginInterface* interface = getInterfaceFn();
    if (!interface) {
        std::cerr << "Error: Plugin interface is null" << std::endl;
        dlclose(handle);
        return 1;
    }
    
    try {
        // Create the plugin wrapper
        PluginWrapper plugin(interface);
        if (!plugin.isValid()) {
            std::cerr << "Error: Failed to initialize plugin" << std::endl;
            dlclose(handle);
            return 1;
        }
        
        // Send a command to the plugin
        cJSON* response = plugin.sendCommand("getInfo");
        if (response) {
            char* responseStr = cJSON_Print(response);
            std::cout << "Plugin response: " << responseStr << std::endl;
            free(responseStr);
            cJSON_Delete(response);
        }
        
        // Process an image
        if (plugin.processImage(testImage)) {
            // Display the image if possible
            try {
                cv::imshow("Processed Image", testImage);
                
                // Wait for keyboard input BEFORE we clean up the resources
                std::cout << "Press any key to exit..." << std::endl;
                cv::waitKey(0);
            } catch (const cv::Exception& e) {
                std::cerr << "Warning: Could not display image: " << e.what() << std::endl;
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error processing plugin: " << e.what() << std::endl;
    }
    
    // Clean up - do this after waitKey to ensure image display works
    std::cout << "Cleaning up resources..." << std::endl;
    dlclose(handle);
    
    return 0;
} 