#include "InspectPlugin.h"
#include "ConversionUtil.h"
#include <opencv2/opencv.hpp>
#include <iostream>

InspectPlugin::InspectPlugin() : processName("Default Process"), pollInterval(1000) {
    std::cout << "InspectPlugin constructor called" << std::endl;
}

InspectPlugin::~InspectPlugin() {
    std::cout << "InspectPlugin destructor called" << std::endl;
}

void InspectPlugin::setup(const cJSON* config) {
    if (config == nullptr) {
        std::cout << "Config is null, using default values" << std::endl;
        return;
    }

    // Extract the process name if provided
    cJSON* nameItem = cJSON_GetObjectItem(config, "processName");
    if (nameItem && cJSON_IsString(nameItem)) {
        processName = cJSON_GetStringValue(nameItem);
        std::cout << "Process name set to: " << processName << std::endl;
    }

    // Extract the polling interval if provided
    cJSON* intervalItem = cJSON_GetObjectItem(config, "pollInterval");
    if (intervalItem && cJSON_IsNumber(intervalItem)) {
        pollInterval = intervalItem->valueint;
        std::cout << "Poll interval set to: " << pollInterval << " ms" << std::endl;
    }
}

cJSON* InspectPlugin::processMessage(const cJSON* message) {
    if (message == nullptr) {
        std::cout << "Message is null" << std::endl;
        return cJSON_CreateString("Error: Null message");
    }

    // Create a response object
    cJSON* response = cJSON_CreateObject();
    
    // Extract the action from the message
    cJSON* actionItem = cJSON_GetObjectItem(message, "action");
    if (actionItem && cJSON_IsString(actionItem)) {
        std::string action = cJSON_GetStringValue(actionItem);
        
        if (action == "getInfo") {
            // Return process information
            cJSON* info = getProcessInfo();
            cJSON_AddItemToObject(response, "info", info);
            cJSON_AddStringToObject(response, "status", "success");
        } 
        else if (action == "setInterval") {
            // Set polling interval
            cJSON* intervalItem = cJSON_GetObjectItem(message, "interval");
            if (intervalItem && cJSON_IsNumber(intervalItem)) {
                int interval = intervalItem->valueint;
                cJSON* result = setPollingInterval(interval);
                cJSON_AddItemToObject(response, "result", result);
                cJSON_AddStringToObject(response, "status", "success");
            } else {
                cJSON_AddStringToObject(response, "status", "error");
                cJSON_AddStringToObject(response, "message", "Invalid or missing interval parameter");
            }
        }
        else {
            cJSON_AddStringToObject(response, "status", "error");
            cJSON_AddStringToObject(response, "message", "Unknown action");
        }
    } else {
        cJSON_AddStringToObject(response, "status", "error");
        cJSON_AddStringToObject(response, "message", "Missing action parameter");
    }
    
    return response;
}

void InspectPlugin::processImage(ImageInfo* imgInfo) {
    if (!imgInfo || !imgInfo->buffer) {
        std::cout << "Image info is null or buffer is null" << std::endl;
        return;
    }
    
    // Convert ImageInfo to Mat without copying (shared buffer)
    cv::Mat image = ImageInfoToMat(imgInfo);
    
    std::cout << "Processing image of size: " << image.cols << "x" << image.rows << std::endl;
    
    // Draw a green line on the image
    cv::line(image, cv::Point(50, 300), cv::Point(450, 300), 
             cv::Scalar(0, 255, 0), 3);
    
    // Add some text with the process name
    cv::putText(image, processName, cv::Point(50, 50), 
                cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(0, 0, 255), 2);
    
    // No need to convert back, as we're directly modifying the shared buffer
    // Changes to the Mat are immediately reflected in the ImageInfo's buffer
    
    std::cout << "Image processing complete" << std::endl;
}

cJSON* InspectPlugin::getProcessInfo() {
    cJSON* info = cJSON_CreateObject();
    cJSON_AddStringToObject(info, "name", processName.c_str());
    cJSON_AddNumberToObject(info, "pollInterval", pollInterval);
    return info;
}

cJSON* InspectPlugin::setPollingInterval(int interval) {
    cJSON* result = cJSON_CreateObject();
    
    if (interval > 0) {
        pollInterval = interval;
        cJSON_AddBoolToObject(result, "success", true);
        cJSON_AddNumberToObject(result, "oldInterval", pollInterval);
        cJSON_AddNumberToObject(result, "newInterval", interval);
    } else {
        cJSON_AddBoolToObject(result, "success", false);
        cJSON_AddStringToObject(result, "error", "Interval must be greater than 0");
    }
    
    return result;
}
