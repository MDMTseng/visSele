#include "InspectPlugin.h"
#include "ConversionUtil.h"
#include <opencv2/opencv.hpp>
#include <iostream>

InspectPlugin::InspectPlugin() 
    : processName("Default Process"), 
      id("unknown"),
      local_env_path(""),
      manager(nullptr) {
    std::cout << "InspectPlugin constructor called" << std::endl;
}

InspectPlugin::~InspectPlugin() {
    std::cout << "InspectPlugin destructor called for ID: " << id << std::endl;
}

void InspectPlugin::init(const char *id, cJSON* def, const char *local_env_path, struct ManagerInterface* manager, void* main_ctx) {
    this->id = id ? id : "unknown";
    this->local_env_path = local_env_path ? local_env_path : "";
    this->manager = manager;
    this->main_ctx = main_ctx;
    
    std::cout << "InspectPlugin initialized for ID: " << this->id << std::endl;
    
    if (def) {
        setDef(def);
    }
}

void InspectPlugin::setEnvPath(const char *path) {
    if (path) {
        local_env_path = path;
        std::cout << "Environment path set to: " << local_env_path << std::endl;
    }
}

int InspectPlugin::setDef(cJSON* def) {
    if (!def) {
        std::cout << "Definition is null" << std::endl;
        return -1;
    }

    // Extract the process name if provided
    cJSON* nameItem = cJSON_GetObjectItem(def, "processName");
    if (nameItem && cJSON_IsString(nameItem)) {
        processName = cJSON_GetStringValue(nameItem);
        std::cout << "Process name set to: " << processName << std::endl;
    }

    std::cout << "Plugin definition set successfully" << std::endl;
    return 0;
}

int InspectPlugin::exchangeCMD(cJSON* info, int id, struct CMDActInterface act) {
    if (!info) {
        std::cout << "Command info is null" << std::endl;
        return -1;
    }

    std::cout << "Received command with ID: " << id << std::endl;
    
    // Example: Send an acknowledgement
    if (act.sendACK) {
        cJSON* response = cJSON_CreateObject();
        cJSON_AddStringToObject(response, "status", "success");
        cJSON_AddStringToObject(response, "message", "Command received");
        
        char* responseStr = cJSON_Print(response);
        act.sendACK(id, 1, responseStr); // 1 = true for isACK
        free(responseStr);
        cJSON_Delete(response);
    }

    return 0;
}

int InspectPlugin::process(struct StageInfo_c* data) {
    if (!data) {
        std::cout << "Stage info is null" << std::endl;
        return -1;
    }
    
    std::cout << "Processing stage: " << data->type << " from source: " << data->source_id << std::endl;
    
    if (data->img.buffer == nullptr) {
        std::cout << "Image buffer is null" << std::endl;
        return -1;
    }


    struct StageInfo_c ret_data;
    ret_data.jInfo = cJSON_CreateObject();
    ret_data.img = data->img;
    ret_data.img_show = data->img_show;
    // Process the image if available
    {
        std::cout << "Processing image of size: " << data->img.width << "x" << data->img.height << std::endl;
        
        // Convert ImageInfo to Mat
        cv::Mat image = ImageInfoToMat(data->img, true);
        
        // Draw a green line on the image
        cv::line(image, cv::Point(50, 300), cv::Point(450, 300), 
                 cv::Scalar(0, 255, 0), 3);
        
        // Add some text with the process name
        cv::putText(image, processName, cv::Point(50, 50), 
                    cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(0, 0, 255), 2);
        
        // Convert back to ImageInfo (if needed for display)
        if (data->img_show.buffer == nullptr) {
            // Create a new ImageInfo for display
            ImageInfo displayImg = MatToImageInfo(image, true);
            data->img_show = displayImg;
            // Don't free the buffer, as it's now owned by img_show
        } else {
            // Update existing display image
            cv::Mat displayImage = ImageInfoToMat(data->img_show, false);
            image.copyTo(displayImage);
        }

        // Call the dispatch function
        manager->dispatch(main_ctx, data);
        
        std::cout << "Image processing complete" << std::endl;
    }
    
    return 0;
}
