#ifndef INSPECT_PLUGIN_H
#define INSPECT_PLUGIN_H

#include <string>
#include <vector>
#include "cJSON.h"
#include "PluginInterface.h"

class InspectPlugin {
private:
    std::string processName;
    int pollInterval;  // in milliseconds

public:
    InspectPlugin();
    ~InspectPlugin();
    
    void setup(const cJSON* config);
    cJSON* processMessage(const cJSON* message);
    void processImage(ImageInfo* imgInfo);

private:
    cJSON* getProcessInfo();
    cJSON* setPollingInterval(int interval);
};

#endif // INSPECT_PLUGIN_H
