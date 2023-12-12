
#include <string>
#include <vector>
#include <math.h>
class CompScript
{
private:
    void* scriptImplObj;
public:
    std::vector<std::string> failInfo;
    CompScript();
    float &set_variable(std::string name, float value);
    void add_variable(std::string name, float value=NAN);

    void add_function(std::string name, float &value);

    bool compile(std::string script);

    float value();

    ~CompScript();

};