#ifndef INSPECT_PLUGIN_H
#define INSPECT_PLUGIN_H

#include <string>
#include <vector>
#include "cJSON.h"
#include "PluginInterface.h"

class InspectPlugin {
private:
    std::string processName;
    std::string id;
    std::string local_env_path;
    ManagerInterface* manager;
    void* main_ctx;
public:
    InspectPlugin();
    ~InspectPlugin();
    
    void init(const char *id, cJSON* def, const char *local_env_path, struct ManagerInterface* manager, void* main_ctx);
    void setEnvPath(const char *path);
    int setDef(cJSON* def);
    int exchangeCMD(cJSON* info, int id, struct CMDActInterface act);
    int process(struct StageInfo_c* data);
};

#endif // INSPECT_PLUGIN_H
