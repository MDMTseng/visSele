#include <opencv2/opencv.hpp>
#include <iostream>
#include <dlfcn.h>
#include "PluginInterface.h"
#include "ConversionUtil.h"
#include "InspectProcess.h"

int main() {
    std::cout << "Plugin Test Program" << std::endl;
    
    // Test direct function call
    // Create a test image (500x500 white background)
    cv::Mat testImage = cv::Mat(500, 500, CV_8UC3, cv::Scalar(255, 255, 255));
    
    // Draw a blue rectangle on the image before processing
    cv::rectangle(testImage, cv::Point(50, 50), cv::Point(200, 200), 
                 cv::Scalar(255, 0, 0), 2);
    
    std::cout << "Image created with initial blue rectangle" << std::endl;
    
    // Convert Mat to ImageInfo without copying (shared buffer)
    ImageInfo* imgInfo = MatToImageInfo(testImage, false);
    
    // Process the image - this will add a line to it
    processImage(imgInfo);
    
    // Convert back to Mat without copying (still shared buffer)
    cv::Mat processedImage = ImageInfoToMat(imgInfo, false);
    
    std::cout << "Image processed - green line added" << std::endl;
    
    // Save the result
    cv::imwrite("processed_image.jpg", processedImage);
    std::cout << "Processed image saved to processed_image.jpg" << std::endl;
    
    // Since we shared the buffer with imgInfo but didn't make a copy,
    // we should not free the buffer when freeing the ImageInfo
    FreeImageInfo(imgInfo, false);
    
    // Test loading and using the plugin
    const char* pluginPath = "./lib/libinspect_process_plugin.dylib";
    printf("Loading plugin: %s\n", pluginPath);
    std::cout << "Loading plugin: " << pluginPath << std::endl;
    
    void* handle = dlopen(pluginPath, RTLD_LAZY);
    if (!handle) {
        std::cerr << "Error: Failed to load plugin: " << dlerror() << std::endl;
        return 1;
    }
    
    // Load plugin functions
    CreatePluginInstance createFn = (CreatePluginInstance)dlsym(handle, "CreateInstance");
    ProcessMessage processFn = (ProcessMessage)dlsym(handle, "ProcessInstanceMessage");
    
    if (!createFn || !processFn) {
        std::cerr << "Error: Failed to load plugin functions: " << dlerror() << std::endl;
        dlclose(handle);
        return 1;
    }
    
    // Create an instance and send a test message
    void* instance = createFn();
    cJSON* request = cJSON_CreateObject();
    cJSON_AddStringToObject(request, "action", "test");
    
    cJSON* response = processFn(instance, request);
    
    // Process the response
    char* responseStr = cJSON_Print(response);
    std::cout << "Plugin response: " << responseStr << std::endl;
    
    // Cleanup
    free(responseStr);
    cJSON_Delete(request);
    cJSON_Delete(response);
    dlclose(handle);
    
    // Display the image if running in an environment with display
    cv::imshow("Processed Image", processedImage);
    cv::waitKey(0);
    
    return 0;
} 