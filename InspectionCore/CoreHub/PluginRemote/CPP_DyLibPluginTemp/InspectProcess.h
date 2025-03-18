#ifndef INSPECT_PROCESS_H
#define INSPECT_PROCESS_H

#include <string>
#include <vector>
#include "cJSON.h"
#include "PluginInterface.h"

class InspectProcess {
private:
    std::string processName;
    int pollInterval;  // in milliseconds

public:
    InspectProcess();
    ~InspectProcess();
    
    void setup(const cJSON* config);
    cJSON* processMessage(const cJSON* message);

private:
    cJSON* getProcessInfo();
    cJSON* setPollingInterval(int interval);
};

#ifdef __cplusplus
extern "C" {
#endif

// Process an image by drawing a line on it
void processImage(ImageInfo* imgInfo);

#ifdef __cplusplus
}
#endif

#endif // INSPECT_PROCESS_H
