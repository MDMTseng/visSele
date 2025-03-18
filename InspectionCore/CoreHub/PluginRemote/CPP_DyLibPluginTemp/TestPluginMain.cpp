#include <opencv2/opencv.hpp>
#include <iostream>
#include <dlfcn.h>
#include "PluginInterface.h"
#include "ConversionUtil.h"
// #include "InspectPlugin.h"

// Forward declaration for the function (now this will call the member function through the plugin)
// The standalone function is not needed anymore

class PluginClass{
    private:
        const PluginInterface* interface;
        void* handle;
    public:
        PluginClass(const PluginInterface* interface):interface(interface)
        {
            handle=interface->create();
            
        }
        ~PluginClass()
        {
            interface->destroy(handle);
            handle=nullptr;
        }
        void processImage(ImageInfo* imgInfo)
        {
            interface->processImage(handle, imgInfo);
        }
        cJSON* process(cJSON* message)
        {
            return interface->process(handle, message);
        }
};


int main() {
    std::cout << "Plugin Test Program" << std::endl;
    
    // Test direct function call
    // Create a test image (500x500 white background)
    cv::Mat testImage = cv::Mat(500, 500, CV_8UC3, cv::Scalar(255, 255, 255));
    
    // Draw a blue rectangle on the image before processing
    cv::rectangle(testImage, cv::Point(50, 50), cv::Point(200, 200), 
                 cv::Scalar(255, 0, 0), 2);
    
    std::cout << "Image created with initial blue rectangle" << std::endl;
    
   
    // Test loading and using the plugin
    const char* pluginPath = "./lib/libinspect_process_plugin.dylib";
    printf("Loading plugin: %s\n", pluginPath);
    std::cout << "Loading plugin: " << pluginPath << std::endl;
    
    void* handle = dlopen(pluginPath, RTLD_LAZY);
    if (!handle) {
        std::cerr << "Error: Failed to load plugin: " << dlerror() << std::endl;
        return 1;
    }
    
    PluginInterface* interface = nullptr;

    //load the interface
    {

        // Load the plugin interface structure instead of individual functions
        typedef PluginInterface* (*GetPluginInterfaceFunc)();
        GetPluginInterfaceFunc getInterfaceFn = (GetPluginInterfaceFunc)dlsym(handle, "GetPluginInterface");
        
        if (!getInterfaceFn) {
            std::cerr << "Error: Failed to load GetPluginInterface function: " << dlerror() << std::endl;
            dlclose(handle);
            return 1;
        }
        
        // Get the plugin interface
        interface = getInterfaceFn();
        
    }
    
    //verify the interface
    {

        if (!interface) {
            std::cerr << "Error: Plugin interface is null" << std::endl;
            dlclose(handle);
            return 1;
        }
        
    }
    

    {
        PluginClass plugin(interface);
        // Create an instance using the interface
        
        // Create a request for getInfo
        cJSON* request = cJSON_CreateObject();
        cJSON_AddStringToObject(request, "action", "getInfo");
        
        // Process the message using the interface
        cJSON* response = plugin.process(request);
        
        // Process the response
        char* responseStr = cJSON_Print(response);
        std::cout << "Plugin response: " << responseStr << std::endl;
        
        // Create a second test image
        cv::Mat testImage2 = cv::Mat(500, 500, CV_8UC3, cv::Scalar(255, 255, 255));
        cv::rectangle(testImage2, cv::Point(100, 100), cv::Point(400, 400), 
                    cv::Scalar(0, 0, 255), 2);
        ImageInfo* imgInfo2 = MatToImageInfo(testImage2, false);
        
        // Process the image using the interface
        plugin.processImage(imgInfo2);
        
        // Convert back to Mat and save
        cv::Mat processedImage2 = ImageInfoToMat(imgInfo2, false);
        cv::imwrite("processed_image2.jpg", processedImage2);
        std::cout << "Second image processed via plugin interface" << std::endl;
        
        // Try to display the second image
        try {
            cv::imshow("Processed Image 2", processedImage2);
        } catch (const cv::Exception& e) {
            std::cerr << "Warning: Could not display image: " << e.what() << std::endl;
        }
        
        // Cleanup
        free(responseStr);
        cJSON_Delete(request);
        cJSON_Delete(response);
        FreeImageInfo(imgInfo2, false);
            
        
        // Display the image if running in an environment with display
        try {
            cv::imshow("Processed Image", processedImage2);
            cv::waitKey(0);
        } catch (const cv::Exception& e) {
            std::cerr << "Warning: Could not display image: " << e.what() << std::endl;
        }
    }

    dlclose(handle);
    
    
    return 0;
} 